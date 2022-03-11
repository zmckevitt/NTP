//////////////////////////////////////////////////////
//
// NTP Server - CSCI 5673
// Author: Zack McKevitt
//
// Sources:
// - https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
// - https://www.geeksforgeeks.org/socket-programming-cc/
// - Personal repo for code from CSCI 5273 PA1
// - https://github.com/jagd/ntp/blob/master/server.c
//      > Shows how to convert tv_usec -> NTP fraction
// - https://stackoverflow.com/questions/29112071/how-to-convert-ntp-time-to-unix-epoch-time-in-c-language-linux
//
//////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define PORT 8080

// leap indicator
#define LI 0

// version number
#define VN 4

// packet mode (server)
#define MODE 4

#define NTP_TIMESTAMP_DELTA 2208988800

struct NTP_packet {
    
    uint8_t li_vn_m;
    uint8_t stratum;
    int8_t poll;
    int8_t precision;

    int32_t root_delay;
    int32_t root_dispersion;
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

struct NTP_time {
    uint32_t seconds;
    uint32_t fraction;
};

void ntpToUnix(struct NTP_time* ntp_time, struct timeval* unix_time) {
    unix_time->tv_sec = ntp_time->seconds - NTP_TIMESTAMP_DELTA;

    uint64_t tmp = 0;
    tmp = ntp_time->fraction;
    tmp *= 1000000;

    unix_time->tv_usec = tmp >> 32;
}

void unixToNTP(struct NTP_time* ntp_time, struct timeval* unix_time) {
    ntp_time->seconds = unix_time->tv_sec + NTP_TIMESTAMP_DELTA;

    uint64_t tmp = 0;
    tmp = unix_time->tv_usec;

    tmp <<= 32;
    tmp /= 1000000;

    ntp_time->fraction = tmp;
}



void initPacket(struct NTP_packet* packet) {
    
    // initialize leap indicator, version number, and packet mode
    uint8_t init_li_vn_m = 0;
    init_li_vn_m |= LI << 6;
    init_li_vn_m |= VN << 3;
    init_li_vn_m |= MODE;

    packet->li_vn_m = init_li_vn_m;
}


// open UDP socket
int open_listenfd(int port)  {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    return listenfd;
} 


int main(int argc, char **argv) {

    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in), n, len;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    struct NTP_packet packet;

    len = sizeof(clientaddr);
    listenfd = open_listenfd(PORT);

    while(1) {
        
        struct NTP_time ntp_time = {0, 0};

        n = recvfrom(listenfd, (char*) &packet, sizeof(struct NTP_packet),
                     0, (struct sockaddr*) &clientaddr, (socklen_t*) &len);

        // T4
        // record time of message receive
        struct timeval t_rec;
        gettimeofday(&t_rec, NULL);
        uint32_t rec_time_s = t_rec.tv_sec;
        uint32_t rec_time_f = t_rec.tv_usec;

        // T1
        uint32_t o_time_s = ntohl(packet.origin_timestamp_s);
        uint32_t o_time_f = ntohl(packet.origin_timestamp_f);

        // T2
        uint32_t r_time_s = ntohl(packet.rec_timestamp_s);
        uint32_t r_time_f = ntohl(packet.rec_timestamp_f);

        // T3
        uint32_t t_time_s = ntohl(packet.trans_timestamp_s);
        uint32_t t_time_f = ntohl(packet.trans_timestamp_f);

        printf("Received client message.\n");

        initPacket(&packet);

        // set reference identifier to client IP
        char* ip = inet_ntoa(clientaddr.sin_addr);

        // update origin time to client's transmit time
        packet.origin_timestamp_s = htonl(t_time_s);
        packet.origin_timestamp_f = htonl(t_time_f);

        // update receive time to server's receive timestamp
        unixToNTP(&ntp_time, &t_rec);
        packet.rec_timestamp_s = htonl(ntp_time.seconds);
        packet.rec_timestamp_f = htonl(ntp_time.fraction);

        // set transmit time to now
        struct timeval t_trans;
        gettimeofday(&t_trans, NULL);
        unixToNTP(&ntp_time, &t_trans);
        packet.trans_timestamp_s = htonl(ntp_time.seconds);
        packet.trans_timestamp_f = htonl(ntp_time.fraction);

        printf("SECONDS SINCE 1970: %u\n", packet.rec_timestamp_s);

        n = sendto(listenfd, (char*) &packet, sizeof(struct NTP_packet), 
                   0, (const struct sockaddr*)&clientaddr, len);

        if(n<0)
            perror("Write failed\n");

        printf("Wrote to client.\n");
    }
}