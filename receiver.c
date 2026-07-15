#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#define MAX_SEQ 100000 
#define PAYLOAD_SIZE 160
#define PACKET_SIZE 164

// Jitter buffer struct
typedef struct {
    uint8_t has_media;
    uint8_t has_fec;
    char payload[PAYLOAD_SIZE];
    char fec_payload[PAYLOAD_SIZE];
} Frame;

Frame buffer[MAX_SEQ];

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main() {
    setbuf(stdout, NULL);
    int sock_in = socket(AF_INET, SOCK_DGRAM, 0);
    int sock_out = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in addr_in, addr_out;
    
    // Listen to Hostile Relay
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(47002);
    addr_in.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(sock_in, (struct sockaddr*)&addr_in, sizeof(addr_in));
    
    // Send to Harness Player
    memset(&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    addr_out.sin_port = htons(47020);
    addr_out.sin_addr.s_addr = inet_addr("127.0.0.1");

    char *t0_str = getenv("T0");
    char *delay_str = getenv("DELAY_MS");
    if (!t0_str || !delay_str) return 1;
    
    double t0 = atof(t0_str);
    double delay_s = atof(delay_str) / 1000.0;
    
    uint32_t next_play_seq = 0;
    memset(buffer, 0, sizeof(buffer));

    while (1) {
        double now = get_time();
        // Calculate hard deadline, flush 2ms early for system latency margin
        double play_time = t0 + delay_s + next_play_seq * 0.020 - 0.002;
        
        // Time to play out the next sequence
        if (now >= play_time) {
            if (buffer[next_play_seq].has_media) {
                char out_pkt[PACKET_SIZE];
                uint32_t net_seq = htonl(next_play_seq);
                memcpy(out_pkt, &net_seq, 4);
                memcpy(out_pkt + 4, buffer[next_play_seq].payload, PAYLOAD_SIZE);
                sendto(sock_out, out_pkt, PACKET_SIZE, 0, (struct sockaddr*)&addr_out, sizeof(addr_out));
            }
            next_play_seq++;
            continue;
        }

        // Wait for next packet or playout deadline
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_in, &readfds);
        
        struct timeval tv;
        double wait_time = play_time - now;
        if (wait_time < 0) wait_time = 0;
        tv.tv_sec = (long)wait_time;
        tv.tv_usec = (long)((wait_time - tv.tv_sec) * 1000000);
        
        int ret = select(sock_in + 1, &readfds, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(sock_in, &readfds)) {
            char in_pkt[PACKET_SIZE];
            int n = recvfrom(sock_in, in_pkt, PACKET_SIZE, 0, NULL, NULL);
            
            if (n == PACKET_SIZE) {
                uint32_t header_seq;
                memcpy(&header_seq, in_pkt, 4);
                header_seq = ntohl(header_seq);
                
                int is_fec = (header_seq & 0x80000000);
                uint32_t seq = header_seq & 0x7FFFFFFF;
                char *payload = in_pkt + 4;
                
                if (seq >= MAX_SEQ) continue;

                if (!is_fec) {
                    // NORMAL MEDIA PACKET
                    memcpy(buffer[seq].payload, payload, PAYLOAD_SIZE);
                    buffer[seq].has_media = 1;
                    
                    // Backward Recovery: Recover seq-2 using seq's FEC
                    if (seq >= 2 && buffer[seq].has_fec && !buffer[seq-2].has_media) {
                        for(int i=0; i<PAYLOAD_SIZE; i++) {
                            buffer[seq-2].payload[i] = buffer[seq].fec_payload[i] ^ buffer[seq].payload[i];
                        }
                        buffer[seq-2].has_media = 1;
                    }
                    // Forward Recovery: Recover seq+2 using seq+2's FEC
                    if (seq + 2 < MAX_SEQ && buffer[seq+2].has_fec && !buffer[seq+2].has_media) {
                        for(int i=0; i<PAYLOAD_SIZE; i++) {
                            buffer[seq+2].payload[i] = buffer[seq+2].fec_payload[i] ^ buffer[seq].payload[i];
                        }
                        buffer[seq+2].has_media = 1;
                    }
                } else {
                    // FEC PACKET (covers 'seq' and 'seq-2')
                    memcpy(buffer[seq].fec_payload, payload, PAYLOAD_SIZE);
                    buffer[seq].has_fec = 1;
                    
                    if (seq >= 2) {
                        // Recover older media
                        if (buffer[seq].has_media && !buffer[seq-2].has_media) {
                            for(int i=0; i<PAYLOAD_SIZE; i++) {
                                buffer[seq-2].payload[i] = buffer[seq].fec_payload[i] ^ buffer[seq].payload[i];
                            }
                            buffer[seq-2].has_media = 1;
                        } 
                        // Recover newer media
                        else if (!buffer[seq].has_media && buffer[seq-2].has_media) {
                            for(int i=0; i<PAYLOAD_SIZE; i++) {
                                buffer[seq].payload[i] = buffer[seq].fec_payload[i] ^ buffer[seq-2].payload[i];
                            }
                            buffer[seq].has_media = 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}