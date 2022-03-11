//////////////////////////////////////////////////////
//
// NTP Client - CSCI 5673
//
// Sources:
// - https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
// - https://www.geeksforgeeks.org/socket-programming-cc/
//
//////////////////////////////////////////////////////

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define PORT 8080
#define IP_ADDR "127.0.0.1"
// #define IP_ADDR "216.239.35.0"

// leap indicator
#define LI 0

// version number
#define VN 4

// packet mode
#define MODE 3

#define NTP_TIMESTAMP_DELTA 2208988800

#define TIMEOUT 4
   
struct NTP_packet {
    
    uint8_t li_vn_m;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;

    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_identifier;

    // seconds and fractional seconds
    uint32_t ref_timestamp_s;
    uint32_t ref_timestamp_f;

    uint32_t origin_timestamp_s;
    uint32_t origin_timestamp_f;

    uint32_t rec_timestamp_s;
    uint32_t rec_timestamp_f;

    uint32_t trans_timestamp_s;
    uint32_t trans_timestamp_f;
};

void initPacket(struct NTP_packet* packet) {
    
    // initialize leap indicator, version number, and packet mode
    uint8_t init_li_vn_m = 0;
    init_li_vn_m |= LI << 6;
    init_li_vn_m |= VN << 3;
    init_li_vn_m |= MODE;

    packet->li_vn_m = init_li_vn_m;
}

void printPacket(struct NTP_packet packet) {

    printf("--------NTP Packet--------\n");
    printf("Leap Indicator:      %u\n", (packet.li_vn_m >> 6) & 3);
    printf("Version Number:      %u\n", (packet.li_vn_m >> 3) & 7);
    printf("Mode:                %u\n", (packet.li_vn_m & 7));
    printf("Stratum:             %u\n", packet.stratum);
    printf("Poll:                %u\n", packet.poll);
    printf("Precision:           %u\n", packet.precision);
    printf("Root Delay:          %u\n", packet.root_delay);
    printf("Root Dispersion:     %u\n", packet.root_dispersion);
    printf("Ref Identifier:      %u\n", (packet.ref_identifier));
    printf("Ref timestamp (s):   %u\n", (packet.ref_timestamp_s));
    printf("Ref timestamp (f):   %u\n", (packet.ref_timestamp_f));
    printf("Org timestamp (s):   %u\n", (packet.origin_timestamp_s));
    printf("Org timestamp (f):   %u\n", (packet.origin_timestamp_f));
    printf("Rec timestamp (s):   %u\n", (packet.rec_timestamp_s));
    printf("Rec timestamp (f):   %u\n", (packet.rec_timestamp_f));
    printf("Trans timestamp (s): %u\n", (packet.trans_timestamp_s));
    printf("Trans timestamp (f): %u\n", (packet.trans_timestamp_f));
}

int main(int argc, char const *argv[]) {

    int sock = 0, valread, n;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP )) < 0) {
        printf("Socket creation error \n");
        return -1;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
       
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, IP_ADDR, &serv_addr.sin_addr)<=0) {
        printf("Invalid address/ Address not supported \n");
        return -1;
    }
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed \n");
        return -1;
    }

    struct NTP_packet packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memset(&packet, 0, sizeof(struct NTP_packet));

    initPacket(&packet);
    
    while(1) {
        
        printf("Writing to socket...\n");
        n = write( sock, ( char* ) &packet, sizeof(struct NTP_packet ) );
        printf("Reading from socket...\n");
        n = read( sock, ( char* ) &packet, sizeof(struct NTP_packet ) );
        
        // T4
        // record time of message receive
        struct timeval r_rec;
        gettimeofday(&r_rec, NULL);
        uint32_t rec_time_s = r_rec.tv_sec;
        uint32_t rec_time_f = r_rec.tv_usec;

        // T1
        uint32_t o_time_s = ntohl(packet.origin_timestamp_s);
        uint32_t o_time_f = ntohl(packet.origin_timestamp_f);

        // T2
        uint32_t r_time_s = ntohl(packet.rec_timestamp_s);
        uint32_t r_time_f = ntohl(packet.rec_timestamp_f);

        // T3
        uint32_t t_time_s = ntohl(packet.trans_timestamp_s);
        uint32_t t_time_f = ntohl(packet.trans_timestamp_f);

        printf("RESPONSE FROM SERVER:\n");
        printPacket(packet);

        time_t origin_time = (time_t) (ntohl(packet.trans_timestamp_s) - NTP_TIMESTAMP_DELTA);
        printf("Origin Time %s", ctime((const time_t*) &origin_time));

        time_t transmit_time = (time_t) (ntohl(packet.trans_timestamp_s) - NTP_TIMESTAMP_DELTA);
        printf("Transmit Time %s", ctime((const time_t*) &transmit_time));

        printf("----------------------\n");

        sleep(TIMEOUT);

        // initPacket(&packet);

        initPacket(&packet);

        packet.origin_timestamp_s = t_time_s;
        packet.origin_timestamp_f = t_time_f;

        packet.rec_timestamp_s = rec_time_s;
        packet.rec_timestamp_f = rec_time_f;
        

        // set transmit time to now
        struct timeval t_trans;
        gettimeofday(&t_trans, NULL);
        packet.trans_timestamp_s = t_trans.tv_sec;
        packet.trans_timestamp_f = t_trans.tv_usec;

        printf("Transmitting at %u seconds\n", (uint32_t)(t_trans.tv_sec));
        printf("Transmitting at %u useconds\n", (uint32_t)(t_trans.tv_usec));
    }
    
    return 0;
}