//////////////////////////////////////////////////////
//
// NTP Server - CSCI 5673
//
// Sources:
// - https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
// - https://www.geeksforgeeks.org/socket-programming-cc/
// - Personal repo for code from CSCI 5273 PA1
//
//////////////////////////////////////////////////////

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define PORT 8080

// leap indicator
#define LI 0

// version number
#define VN 4

// packet mode
#define MODE 3

#define NTP_TIMESTAMP_DELTA 2208988800

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
        
        n = recvfrom(listenfd, (char*) &packet, sizeof(struct NTP_packet),
                     0, (struct sockaddr*) &clientaddr, &len);

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
        uint32_t t_time_s = packet.trans_timestamp_s;
        uint32_t t_time_f = packet.trans_timestamp_f;

        printf("Received client message.\n");

        initPacket(&packet);

        // update origin time to client's transmit time
        packet.origin_timestamp_s = t_time_s;
        packet.origin_timestamp_f = t_time_f;

        // update receive time to server's receive timestamp
        packet.rec_timestamp_s = rec_time_s + NTP_TIMESTAMP_DELTA;
        packet.rec_timestamp_f = (4294*(rec_time_f)) + ((1981*(rec_time_f))>>11);//rec_time_f;

        // set transmit time to now
        struct timeval t_trans;
        gettimeofday(&t_trans, NULL);
        packet.trans_timestamp_s = t_trans.tv_sec + NTP_TIMESTAMP_DELTA;
        packet.trans_timestamp_f = (4294*(t_trans.tv_usec)) + ((1981*(t_trans.tv_usec))>>11);//t_trans.tv_usec;

        printf("SECONDS SINCE 1970: %u\n", packet.rec_timestamp_s);

        n = sendto(listenfd, (char*) &packet, sizeof(struct NTP_packet), 
                   0, (const struct sockaddr*)&clientaddr, len);

        if(n<0)
            perror("Write failed\n");

        printf("Wrote to client.\n");
    }
}