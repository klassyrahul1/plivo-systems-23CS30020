#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>

/* Custom Wire Format (165 bytes total) */
struct WirePkt {
    uint8_t type;    // 0 for media, 1 for FEC
    uint32_t seq;    // Sequence number (or base sequence for FEC)
    uint8_t payload[160];
} __attribute__((packed));

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr));

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    uint8_t fec_buf[160] = {0}; // Caches the even-sequence payload

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, NULL, NULL);
        if (n < 164) continue;

        uint32_t seq;
        memcpy(&seq, buf, 4); // Harness sends Big-Endian seq
        uint32_t host_seq = ntohl(seq);

        // 1. Send the regular media packet
        struct WirePkt pkt;
        pkt.type = 0;
        pkt.seq = seq; // Keep in network byte order for the wire
        memcpy(pkt.payload, buf + 4, 160);
        sendto(out_fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&relay, sizeof(relay));

        // 2. FEC Logic (2:1 Ratio)
        if (host_seq % 2 == 0) {
            // Cache even frame for the next XOR
            memcpy(fec_buf, pkt.payload, 160);
        } else {
            // Odd frame: generate and send the XOR FEC packet
            struct WirePkt fec_pkt;
            fec_pkt.type = 1;
            uint32_t base_seq = htonl(host_seq - 1); 
            fec_pkt.seq = base_seq; 
            
            for (int i = 0; i < 160; i++) {
                fec_pkt.payload[i] = fec_buf[i] ^ pkt.payload[i];
            }
            sendto(out_fd, &fec_pkt, sizeof(fec_pkt), 0, (struct sockaddr *)&relay, sizeof(relay));
        }
    }
    return 0;
}