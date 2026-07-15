#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>

#define PAYLOAD_SIZE 160
#define PACKET_SIZE 164

// Rolling history buffer for Stride-2 calculation
char history[3][PAYLOAD_SIZE];

int main() {
    int sock_in = socket(AF_INET, SOCK_DGRAM, 0);
    int sock_out = socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in addr_in, addr_out;
    
    // Listen to Harness Source
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(47010);
    addr_in.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(sock_in, (struct sockaddr*)&addr_in, sizeof(addr_in));
    
    // Send to Hostile Relay
    memset(&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    addr_out.sin_port = htons(47001);
    addr_out.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (1) {
        char in_pkt[PACKET_SIZE];
        int n = recvfrom(sock_in, in_pkt, PACKET_SIZE, 0, NULL, NULL);
        
        if (n == PACKET_SIZE) {
            uint32_t net_seq;
            memcpy(&net_seq, in_pkt, 4);
            uint32_t seq = ntohl(net_seq);
            char *payload = in_pkt + 4;

            // 1. Cache payload in history buffer
            memcpy(history[seq % 3], payload, PAYLOAD_SIZE);

            // 2. Send normal media packet immediately
            sendto(sock_out, in_pkt, PACKET_SIZE, 0, (struct sockaddr*)&addr_out, sizeof(addr_out));

            // 3. Send Stride-2 FEC packet (Delay start to frame 50 to cap bandwidth < 2.0x)
            if (seq >= 2 && (seq % 20 != 0)) {
                char fec_pkt[PACKET_SIZE];
                uint32_t target_seq = seq - 2;
                
                // Flag MSB to denote an FEC packet
                uint32_t fec_header = htonl(seq | 0x80000000);
                memcpy(fec_pkt, &fec_header, 4);
                
                // XOR current payload with payload from 2 frames ago
                for (int i = 0; i < PAYLOAD_SIZE; i++) {
                    fec_pkt[i + 4] = history[seq % 3][i] ^ history[target_seq % 3][i];
                }
                
                sendto(sock_out, fec_pkt, PACKET_SIZE, 0, (struct sockaddr*)&addr_out, sizeof(addr_out));
            }
        }
    }
    return 0;
}