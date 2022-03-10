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
#include <time.h>

#define PORT 123
#define IP_ADDR "216.239.35.0"

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

int main(int argc, char const *argv[]) {

    int sock = 0, valread;
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
    
    // initialize leap indicator, version number, and packet mode
    uint8_t init_li_vn_m = 0;
    init_li_vn_m |= LI << 6;
    init_li_vn_m |= VN << 3;
    init_li_vn_m |= MODE;

    packet.li_vn_m = init_li_vn_m;

    int n;

    while(1) {
        
        printf("Writing to socket...\n");
        n = write( sock, ( char* ) &packet, sizeof(struct NTP_packet ) );


        n = read( sock, ( char* ) &packet, sizeof(struct NTP_packet ) );
        time_t rec_time = time(0);

        // T1
        uint32_t otime_s = packet.origin_timestamp_s;
        uint32_t otime_f = packet.origin_timestamp_f;

        // T2
        uint32_t r_time_s = packet.rec_timestamp_s;
        uint32_t r_time_f = packet.rec_timestamp_f;

        // T3
        uint32_t t_time_s = packet.trans_timestamp_s;
        uint32_t t_time_s = packet.trans_timestamp_f;

        printf("RESPONSE FROM SERVER:\n");
        printf("Stratum: %d\n", packet.stratum);
        printf("Poll: %d\n", packet.poll);
        printf("Precision: %d\n", packet.precision);
        printf("Root Delay: %d\n", packet.root_delay);
        printf("Reference Identifier: %d\n", packet.ref_identifier);

        time_t origin_time = (time_t) (ntohl(packet.origin_timestamp_s) - NTP_TIMESTAMP_DELTA);
        printf("Origin Time %s", ctime((const time_t*) &origin_time));
        time_t transmit_time = (time_t) (ntohl(packet.trans_timestamp_s) - NTP_TIMESTAMP_DELTA);
        printf("Transmit Time %s", ctime((const time_t*) &transmit_time));

        printf("----------------------\n");

        sleep(TIMEOUT);

        memset(&packet, 0, sizeof(struct NTP_packet));
    
        // initialize leap indicator, version number, and packet mode
        uint8_t init_li_vn_m = 0;
        init_li_vn_m |= LI << 6;
        init_li_vn_m |= VN << 3;
        init_li_vn_m |= MODE;

        packet.li_vn_m = init_li_vn_m;

        // not working...
        packet.origin_timestamp_s = ntohl(packet.trans_timestamp_s) + NTP_TIMESTAMP_DELTA;
        packet.origin_timestamp_f = ntohl(packet.trans_timestamp_f);

        packet.rec_timestamp_s = (int)rec_time;

        packet.trans_timestamp_s = (int)time(0);
    }
    
    return 0;
}