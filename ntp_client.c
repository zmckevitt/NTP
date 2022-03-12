//////////////////////////////////////////////////////
//
// NTP Client - CSCI 5673
// Author: Zack McKevitt
//
// Sources:
// - https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
// - https://www.geeksforgeeks.org/socket-programming-cc/
// - https://github.com/jagd/ntp/blob/master/server.c
//      > Shows how to convert tv_usec -> NTP fraction
// - https://stackoverflow.com/questions/29112071/how-to-convert-ntp-time-to-unix-epoch-time-in-c-language-linux
//
//////////////////////////////////////////////////////

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define LOCAL       // tells program to use local NTP server over public server
#define LOGGING     // enables logging for experiments

#define LOGGING_DIR  "./experiments/"
#define LOG_SCENARIO "LAN"  // can be "LAN", "CLOUD", or "PUBLIC"

// localhost configuration
#ifdef LOCAL
    #define PORT 8080
    // #define IP_ADDR "127.0.0.1"      // localhost server
    // #define IP_ADDR "10.0.0.29"      // LAN server 
    #define IP_ADDR "34.106.71.102"  // GCP server
#endif

// google NTP server configuration
#ifndef LOCAL
    #define PORT 123
    #define IP_ADDR "216.239.35.0"
#endif

// send BURST requests every TIMEOUT seconds
#define BURST 8
#define TIMEOUT 240

// leap indicator
#define LI 0

// version number
#define VN 4

// packet mode (client)
#define MODE 3

#define NTP_TIMESTAMP_DELTA 2208988800

// socket timeout threshold
#define S_TIMEOUT 10
   
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

void printPacket(struct NTP_packet packet) {

    printf("--------NTP Packet--------\n");
    printf("Leap Indicator:         %d\n", ((packet.li_vn_m >> 6) & 3));
    printf("Version Number:         %d\n", ((packet.li_vn_m >> 3) & 7));
    printf("Mode:                   %d\n", (packet.li_vn_m & 7));
    printf("Stratum:                %d\n", (packet.stratum));
    printf("Poll:                   %d\n", (packet.poll));
    printf("Precision:              %d\n", (packet.precision));
    printf("Root Delay:             %d\n", (packet.root_delay));
    printf("Root Dispersion:        %d\n", (packet.root_dispersion));
    printf("Ref Identifier:         %u\n", (packet.ref_identifier));
    printf("Ref timestamp (s):      %u\n", ntohl(packet.ref_timestamp_s));
    printf("Ref timestamp (f):      %u\n", ntohl(packet.ref_timestamp_f));
    printf("Org timestamp (s):      %u\n", ntohl(packet.origin_timestamp_s));
    printf("Org timestamp (f):      %u\n", ntohl(packet.origin_timestamp_f));
    printf("Rec timestamp (s):      %u\n", ntohl(packet.rec_timestamp_s));
    printf("Rec timestamp (f):      %u\n", ntohl(packet.rec_timestamp_f));
    printf("Trans timestamp (s):    %u\n", ntohl(packet.trans_timestamp_s));
    printf("Trans timestamp (f):    %u\n", ntohl(packet.trans_timestamp_f));
}

// calculate individual delay in NTP TIME
struct NTP_time calcDelay(struct NTP_time T1, struct NTP_time  T2, struct NTP_time  T3, struct NTP_time  T4) {
    
    // delay = (T4 – T1) - (T3 – T2) 

    struct NTP_time left;
    left.seconds = T4.seconds - T1.seconds;
    left.fraction = T4.fraction - T1.fraction;

    struct NTP_time right;
    right.seconds = T3.seconds - T2.seconds;
    right.fraction = T3.fraction - T2.fraction;

    struct NTP_time ret_t;
    ret_t.seconds = left.seconds - right.seconds;
    ret_t.fraction = left.fraction - right.fraction;

    return ret_t;
}

// calculate individual offset in NTP TIME
struct NTP_time calcOffset(struct NTP_time  T1, struct NTP_time  T2, struct NTP_time  T3, struct NTP_time  T4) {
    
    // offset = 0.5[(T2 – T1) + (T3 – T4)]

    struct NTP_time left;
    left.seconds = T2.seconds - T1.seconds;
    left.fraction = T2.fraction - T1.fraction;

    printf("LEFT F: %u\n", left.fraction);

    struct NTP_time right;
    right.seconds = T3.seconds - T4.seconds;
    right.fraction = T3.fraction - T4.fraction;

    printf("RIGHT F: %u\n", right.fraction);

    struct NTP_time ret_t;
    ret_t.seconds = (left.seconds + right.seconds)/2;
    ret_t.fraction = (left.fraction + right.fraction)/2;

    return ret_t;
}

struct NTP_time arrMin(struct NTP_time* arr) {

    struct NTP_time min;
    min.seconds = 4294967295; // max uint32
    min.fraction = 4294967295;

    for(int i=0; i<BURST; ++i) {
        struct NTP_time tmp = arr[i];
        if(tmp.seconds < min.seconds && tmp.fraction < min.fraction) {
            min = tmp;
        }
    }

    return min;
}

// at each burst, record d_i and o_i
void logDelOff(int message_pair, int burst, struct NTP_time delay, struct NTP_time offset) {
    char filename[100];
    sprintf(filename, "%s%s.csv", LOGGING_DIR, LOG_SCENARIO);

    // open logging file to append
    FILE* fp = fopen(filename, "a");

    fprintf(fp, "%d %d %u %u %u %u\n", message_pair, burst, 
            delay.seconds, delay.fraction, offset.seconds, offset.fraction);

    fclose(fp);
}

// after each burst, record update values delt_0 and thet_0
void logUpdates(int message_pair, struct NTP_time delay, struct NTP_time offset) {
    char filename[100];
    sprintf(filename, "%s%s.csv", LOGGING_DIR, LOG_SCENARIO);

    // open logging file to append
    FILE* fp = fopen(filename, "a");

    fprintf(fp, "%d %u %u %u %u\n", message_pair, 
            delay.seconds, delay.fraction, offset.seconds, offset.fraction);

    fclose(fp);

}

// after each burst, record update values delt_0 and thet_0
void logData(int message_pair, int burst, struct NTP_time  T1, struct NTP_time  T2, struct NTP_time  T3, struct NTP_time  T4) {
    char filename[100];
    sprintf(filename, "%s%s_raw.csv", LOGGING_DIR, LOG_SCENARIO);

    // open logging file to append
    FILE* fp = fopen(filename, "a");

    fprintf(fp, "%d %d %u %u %u %u %u %u %u %u\n", message_pair, burst, 
            T1.seconds, T1.fraction, T2.seconds, T2.fraction, 
            T3.seconds, T3.fraction, T4.seconds, T4.fraction);

    fclose(fp);

}

int main(int argc, char const *argv[]) {

    int sock = 0, valread, n;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP )) < 0) {
        printf("Socket creation error \n");
        return -1;
    }

    // set socket to timeout after TIMEOUT seconds
    struct timeval timeout;      
    timeout.tv_sec = S_TIMEOUT;
    timeout.tv_usec = 0;
    
    if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                sizeof timeout) < 0)
        printf("setsockopt failed\n");
   
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
    initPacket(&packet);
    
    int first_burst = 0;

    // define arrays for delay and offset to be calculated after each burst
    // upon sedning BURST bursts, we calculate min value and reset arrays
    struct NTP_time delay_arr[BURST];
    struct NTP_time offset_arr[BURST];

    // initialize delay and offset arrays to 0
    memset(delay_arr, 0, sizeof(delay_arr));
    memset(offset_arr, 0, sizeof(offset_arr));

    // increment after each burst set
    int message_pair = 0;

    while(1) {

        
        // initialize arrays to 0 before each burst round
        memset(delay_arr, 0, sizeof(delay_arr));
        memset(offset_arr, 0, sizeof(offset_arr));

        int i;
        for(i=0; i<BURST; ++i) {

            struct NTP_time ntp_time = {0, 0};

            // update transmit time for first packet of burst
            if(first_burst) {
                // set transmit time to now
                struct timeval t_trans;
                gettimeofday(&t_trans, NULL);
                unixToNTP(&ntp_time, &t_trans);
                packet.trans_timestamp_s = htonl(ntp_time.seconds);
                packet.trans_timestamp_f = htonl(ntp_time.fraction);
                
                // someone suggested to set tx time for first burst to be 0
                // packet.trans_timestamp_s = 0;
                // packet.trans_timestamp_f = 0;

                first_burst = 0;

                printf("Transmitting at %u NTP seconds\n", ntp_time.seconds);
                printf("Transmitting at %u NTP fraction\n", ntp_time.fraction);
            }

            printf("Writing to socket...\n");
            n = write( sock, ( char* ) &packet, sizeof(struct NTP_packet ) );
            printf("Reading from socket...\n");
            n = read( sock, ( char* ) &packet, sizeof(struct NTP_packet ) );

            // timeout
            // recalculate transmit time and reset burst
            if(n<0) {
                first_burst = 1;
                i = 0;
                continue;
            }
            
            // T4
            // record time of message receive
            struct timeval r_rec;
            struct NTP_time r_NTP;
            gettimeofday(&r_rec, NULL);
            uint32_t rec_time_s = r_rec.tv_sec;
            uint32_t rec_time_f = r_rec.tv_usec;
            unixToNTP(&r_NTP, &r_rec);

            // T1
            uint32_t o_time_s = ntohl(packet.origin_timestamp_s);
            uint32_t o_time_f = ntohl(packet.origin_timestamp_f);

            // convert T1 NTP time to unix time
            struct NTP_time org_NTP = {o_time_s, o_time_f};
            struct timeval org_unix;
            ntpToUnix(&org_NTP, &org_unix);

            // T2
            uint32_t r_time_s = ntohl(packet.rec_timestamp_s);
            uint32_t r_time_f = ntohl(packet.rec_timestamp_f);

            // convert T2 NTP time to unix time
            struct NTP_time rec_NTP = {r_time_s, r_time_f};
            struct timeval rec_unix;
            ntpToUnix(&rec_NTP, &rec_unix);

            // T3
            uint32_t t_time_s = ntohl(packet.trans_timestamp_s);
            uint32_t t_time_f = ntohl(packet.trans_timestamp_f);

            // convert T2 NTP time to unix time
            struct NTP_time tx_NTP = {t_time_s, t_time_f};
            struct timeval tx_unix;
            ntpToUnix(&tx_NTP, &tx_unix);

            // calculate delay and offset values and store them in the array
            delay_arr[i] = calcDelay(org_NTP, rec_NTP, tx_NTP, r_NTP);
            offset_arr[i] = calcOffset(org_NTP, rec_NTP, tx_NTP, r_NTP);

            // log delay and offset
            #ifdef LOGGING
            logDelOff(message_pair, i, delay_arr[i], offset_arr[i]);
            logData(message_pair, i, org_NTP, rec_NTP, tx_NTP, r_NTP);
            #endif

            printf("RESPONSE FROM SERVER:\n");
            printPacket(packet);

            time_t origin_time = (time_t) (ntohl(packet.trans_timestamp_s) - NTP_TIMESTAMP_DELTA);
            printf("\nOrigin Time %s", ctime((const time_t*) &origin_time));

            time_t transmit_time = (time_t) (ntohl(packet.trans_timestamp_s) - NTP_TIMESTAMP_DELTA);
            printf("Transmit Time %s", ctime((const time_t*) &transmit_time));

            time_t recv_time = (time_t) (ntohl(packet.rec_timestamp_s) - NTP_TIMESTAMP_DELTA);
            printf("Receive Time %s", ctime((const time_t*) &recv_time));
            printf("----------------------\n");

            initPacket(&packet);

            packet.origin_timestamp_s = htonl(t_time_s);
            packet.origin_timestamp_f = htonl(t_time_f);

            unixToNTP(&ntp_time, &r_rec);
            packet.rec_timestamp_s = htonl(ntp_time.seconds);
            packet.rec_timestamp_f = htonl(ntp_time.fraction);
            

            // set transmit time to now
            struct timeval t_trans;
            gettimeofday(&t_trans, NULL);
            unixToNTP(&ntp_time, &t_trans);
            packet.trans_timestamp_s = htonl(ntp_time.seconds);
            packet.trans_timestamp_f = htonl(ntp_time.fraction);

            if(!first_burst) {
                printf("Transmitting at %u NTP seconds\n", ntp_time.seconds);
                printf("Transmitting at %u NTP fraction\n", ntp_time.fraction);
            }
        }

        struct NTP_time delay_update = arrMin(delay_arr);
        struct NTP_time offset_update = arrMin(offset_arr);

        printf("Delay update value (s):   %u\n", delay_update.seconds);
        printf("Delay update value (us):  %u\n", delay_update.fraction);
        printf("Offset update value (s):  %u\n", offset_update.seconds);
        printf("Offset update value (us): %u\n", offset_update.fraction);

        #ifdef LOGGING
        logUpdates(message_pair, delay_update, offset_update);
        #endif

        message_pair += 1;
        first_burst = 1;
        sleep(TIMEOUT);
    }
    
    return 0;
}