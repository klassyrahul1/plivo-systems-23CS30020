#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/select.h>

struct WirePkt {
    uint8_t type;
    uint32_t seq;
    uint8_t payload[160];
} __attribute__((packed));

struct Frame {
    bool present;
    uint8_t payload[160];
};

// Increased boundaries to 3500 to safely buffer a full 30-second run (1500+ frames)
struct Frame frames[3500];
struct Frame fecs[3500]; 

double get_time_s() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(void) {
    double t0 = atof(getenv("T0"));
    double delay_s = atof(getenv("DELAY_MS")) / 1000.0;

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr));

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    int next_play_seq = 0;

    for (;;) {
        double now = get_time_s();
        
        // FLUSH 2ms EARLY: Ensures the packet crosses the UDP socket before the hard deadline
        double play_time = t0 + delay_s + next_play_seq * 0.020 - 0.002;
        double wait_time = play_time - now;

        struct timeval tv;
        if (wait_time > 0) {
            tv.tv_sec = (time_t)wait_time;
            tv.tv_usec = (wait_time - tv.tv_sec) * 1000000;
        } else {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(in_fd, &readfds);

        int ret = select(in_fd + 1, &readfds, NULL, NULL, &tv);

        if (ret > 0 && FD_ISSET(in_fd, &readfds)) {
            struct WirePkt pkt;
            ssize_t n = recvfrom(in_fd, &pkt, sizeof(pkt), 0, NULL, NULL);
            if (n == sizeof(pkt)) {
                uint32_t host_seq = ntohl(pkt.seq);
                if (host_seq < 3400) {
                    if (pkt.type == 0) { // Media Packet
                        frames[host_seq].present = true;
                        memcpy(frames[host_seq].payload, pkt.payload, 160);
                        
                        int base = (host_seq % 2 == 0) ? host_seq : host_seq - 1;
                        int other = (host_seq % 2 == 0) ? host_seq + 1 : host_seq - 1;
                        
                        if (!frames[other].present && fecs[base].present) {
                            frames[other].present = true;
                            for(int i = 0; i < 160; i++) {
                                frames[other].payload[i] = pkt.payload[i] ^ fecs[base].payload[i];
                            }
                        }
                    } else if (pkt.type == 1) { // FEC Packet
                        fecs[host_seq].present = true;
                        memcpy(fecs[host_seq].payload, pkt.payload, 160);
                        
                        int base = host_seq;
                        int next = host_seq + 1;
                        if (frames[base].present && !frames[next].present) {
                            frames[next].present = true;
                            for(int i = 0; i < 160; i++) 
                                frames[next].payload[i] = frames[base].payload[i] ^ pkt.payload[i];
                        } else if (!frames[base].present && frames[next].present) {
                            frames[base].present = true;
                            for(int i = 0; i < 160; i++) 
                                frames[base].payload[i] = frames[next].payload[i] ^ pkt.payload[i];
                        }
                    }
                }
            }
        }

        now = get_time_s();
        
        // Loop and flush any frames that are within 2ms of their deadline
        while (now >= t0 + delay_s + next_play_seq * 0.020 - 0.002) {
            if (next_play_seq >= 3400) break;
            
            if (frames[next_play_seq].present) {
                uint8_t out_buf[164];
                uint32_t net_seq = htonl(next_play_seq);
                memcpy(out_buf, &net_seq, 4);
                memcpy(out_buf + 4, frames[next_play_seq].payload, 160);
                sendto(out_fd, out_buf, 164, 0, (struct sockaddr *)&player, sizeof(player));
            }
            next_play_seq++;
            now = get_time_s();
        }
    }
    return 0;
}