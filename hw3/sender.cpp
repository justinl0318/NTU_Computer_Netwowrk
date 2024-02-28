#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include "def.h"
#include <time.h>
#include <signal.h>

using namespace std;
#define MAX_SEGMENTS 1000000
#define FILESIZE 10240000 //10 MB
#define MAX(x, y) (x > y ? x : y)
#define SLOWSTART 0
#define CONGESTIONAVOID 1

timer_t timerid;
segment *transmit_queue[MAX_SEGMENTS];
char file_arr[FILESIZE];
int total_segments;
int max_send_seq_num = 0; // current max send sequence number, so we can tell if it is resend or not
int successfully_sent = 0; // number of segments successfully sent, to check if all are sent or not

double cwnd;
int thresh, dup_ack;
int state;
int base;

void setIP(char *dst, char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

long get_remaining_timeout(const struct itimerspec *remaining_time) {
    long milliseconds = remaining_time->it_value.tv_sec * 1000 +
                        remaining_time->it_value.tv_nsec / 1000000;
    // printf("Remaining Timeout: %ld milliseconds\n", milliseconds);
    return milliseconds;
}

void resetTimer(){
    //printf("reset Timer\n");
    struct sigevent sev;
    struct itimerspec its;
    // Initialize the sigevent structure
    sev.sigev_notify = SIGEV_NONE;
    timer_create(CLOCK_REALTIME, &sev, &timerid);
    
    its.it_value.tv_sec = TIMEOUT_MILLISECONDS / 1000;
    its.it_value.tv_nsec = (TIMEOUT_MILLISECONDS % 1000) * 1000000;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    timer_settime(timerid, 0, &its, NULL);
}

void transmitNew(int num, int sock_fd, struct sockaddr_in recv_addr){
    if (num == 0) return;
    //construct window array
    int window[(int)cwnd];
    int index = 0;
    int count = 0;
    int k = base - 1;
    while (k < total_segments){
        k++;
        // segments already acked, that means not in window
        if (transmit_queue[k-1]->head.ack == 1){
            continue;
        }

        count++;
        //send the last num number of segments in window
        if (count > (int)cwnd - num){
            segment *send_segment = transmit_queue[k-1];
            sendto(sock_fd, send_segment, sizeof(*send_segment), 0, (struct sockaddr *)&recv_addr, sizeof(sockaddr));
            
            if (k > max_send_seq_num){
                printf("send\tdata\t#%d,\twinSize = %d\n", k, (int)cwnd);
            }
            else if (k <= max_send_seq_num){
                printf("resnd\tdata\t#%d,\twinSize = %d\n", k, (int)cwnd);
            }
            if (k > max_send_seq_num) max_send_seq_num = k;

        }
        if (count == (int)cwnd) break;
    }

    // //send the last num number of segments in window
    // for (int i = (int)cwnd - num - 1; i < (int)cwnd; i++){
    //     segment *sgmt = transmit_queue[window[i]];
    //     sendto(sock_fd, sgmt, sizeof(*sgmt), 0, (struct sockaddr *)&recv_addr, sizeof(sockaddr));
    // }
}

void transmitMissing(int sock_fd, struct sockaddr_in recv_addr){
    segment *sgmt = transmit_queue[base-1];
    sendto(sock_fd, sgmt, sizeof(*sgmt), 0, (struct sockaddr *)&recv_addr, sizeof(sockaddr));
    printf("resnd\tdata\t#%d,\twinSize = %d\n", sgmt->head.seqNumber, (int)cwnd);
}

void setState(int curr_state){
    state = curr_state;
}

bool isAtState(int curr_state){
    if (state == curr_state) return true;
    return false;
}

void markSACK(int seq_num){
    // if first packet is corrupted, then seq_num is 0, so need to check border
    if (seq_num >= 1 && seq_num <= total_segments){
        if (transmit_queue[seq_num-1]->head.ack == 0) successfully_sent++;
        
        transmit_queue[seq_num-1]->head.ack = 1;
    }
}

void updateBase(int ack_num){
    base = ack_num + 1;
}

void init(int sock_fd, struct sockaddr_in recv_addr){
    cwnd = 1, thresh = 16, dup_ack = 0, base = 1;
    transmitNew(1, sock_fd, recv_addr);
    resetTimer();
    setState(SLOWSTART);
}

void timeout(int sock_fd, struct sockaddr_in recv_addr){
    thresh = MAX(1, int(cwnd / 2));
    cwnd = 1;
    dup_ack = 0;
    printf("time\tout,\tthreshold = %d,\twinSize = %d\n", thresh, (int)cwnd);
    transmitMissing(sock_fd, recv_addr);
    resetTimer();
    setState(SLOWSTART);
}

void dupACK(segment *segment, int sock_fd, struct sockaddr_in recv_addr){
    //dupACK: cumulative ACK < first segment in the transmit queue
    dup_ack++;
    markSACK(segment->head.sackNumber);
    
    //if all packets are in current window, then there are no more new packets to send
    if (base + (int)cwnd - 1 >= total_segments) transmitNew(0, sock_fd, recv_addr);
    //if packets is corrupted or dropped bcuz of out-of-buffer-range, then ack = sack, so don't need to send new packets
    //ex: when first packets is corrupted, then ack = sack = 0, since no segments in window are removed
    //and window also didn't increase, so transmit 0 new segments
    else if (segment->head.ackNumber == segment->head.sackNumber) transmitNew(0, sock_fd, recv_addr);

    else transmitNew(1, sock_fd, recv_addr);

    if (dup_ack == 3){
        transmitMissing(sock_fd, recv_addr);
    }
}

void newACK(segment *segment, int sock_fd, struct sockaddr_in recv_addr){
    //newACK: cumulative ACK >= first segment in the transmit queue
    dup_ack = 0;
    markSACK(segment->head.sackNumber);
    updateBase(segment->head.ackNumber);
    int increase;
    double prev_cwnd = cwnd;
    if (isAtState(SLOWSTART)){
        increase = 1;
        cwnd += 1;
        if (cwnd >= thresh){
            setState(CONGESTIONAVOID);
        }
    }
    else if (isAtState(CONGESTIONAVOID)){
        cwnd += (double)(1) / (int)(cwnd);
        increase = (int)cwnd - (int)prev_cwnd;
    }
    
    //transmit new segments in window
    transmitNew(1 + increase, sock_fd, recv_addr);
    resetTimer();
}

// ./sender <send_ip> <send_port> <agent_ip> <agent_port> <src_filepath>
int main(int argc, char *argv[]) {
    // parse arguments
    if (argc != 6) {
        cerr << "Usage: " << argv[0] << " <send_ip> <send_port> <agent_ip> <agent_port> <src_filepath>" << endl;
        exit(1);
    }

    int send_port, agent_port;
    char send_ip[50], agent_ip[50];

    // read argument
    setIP(send_ip, argv[1]);
    sscanf(argv[2], "%d", &send_port);

    setIP(agent_ip, argv[3]);
    sscanf(argv[4], "%d", &agent_port);

    char *filepath = argv[5];

    // make socket related stuff
    int sock_fd = socket(PF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in recv_addr;
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(agent_port);
    recv_addr.sin_addr.s_addr = inet_addr(agent_ip);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(send_port);
    addr.sin_addr.s_addr = inet_addr(send_ip);
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));    
    bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));
    
    // make a segment (do file IO stuff on your own)
    int fd = open(filepath, O_RDONLY);

    memset(file_arr, 0, sizeof(file_arr));
    int read_bytes = read(fd, file_arr, sizeof(file_arr));
    file_arr[read_bytes] = '\0';
    
    //allocate a transmit_queue
    total_segments = read_bytes / MAX_SEG_SIZE;
    if (read_bytes % MAX_SEG_SIZE != 0) total_segments++;
    //printf("%d\n", total_segments);
    for (int i = 0; i < total_segments; i++){
        segment *sgmt = (segment *) malloc(sizeof(segment));
        int curr_segment_size = MAX_SEG_SIZE;
        // last segment and not aligned
        if ((read_bytes % MAX_SEG_SIZE != 0) && (i == total_segments - 1)){
            curr_segment_size = read_bytes % MAX_SEG_SIZE;
        }
        memset(sgmt->data, 0, sizeof(char) * MAX_SEG_SIZE);
        memcpy(sgmt->data, file_arr + (i * MAX_SEG_SIZE), curr_segment_size);
        
        //sgmt->data[curr_segment_size] = '\0';
        sgmt->head.length = curr_segment_size;
        sgmt->head.seqNumber = i+1;
        sgmt->head.ackNumber = 0;
        sgmt->head.sackNumber = 0;
        sgmt->head.fin = 0;
        sgmt->head.syn = 0;
        sgmt->head.ack = 0;
        sgmt->head.checksum = crc32(0L, (const Bytef *)sgmt->data, MAX_SEG_SIZE);
        transmit_queue[i] = sgmt;
    }

    // for (int i = 0; i < total_segments; i++){
    //     printf("data = %s\n", transmit_queue[i]->data);
    // }

    //implement select()
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock_fd, &read_fds);
    struct timeval select_timeout;

    //start
    init(sock_fd, recv_addr);
    while (true){
        //if all packets are successfully sent, break
        if (successfully_sent == total_segments) break;

        // get remaining time from timer
        struct itimerspec remaining_time;
        timer_gettime(timerid, &remaining_time);

        // TODO: may need to check here first if remaining time is 0 (i think done)
        // handle timeout ASAP
        long curr_remaining_time = get_remaining_timeout(&remaining_time);
        if (curr_remaining_time == 0){
            timeout(sock_fd, recv_addr);
            // Reset the set for the next iteration
            FD_ZERO(&read_fds);
            FD_SET(sock_fd, &read_fds);
            continue;
        }

        select_timeout.tv_sec = MAX(0, remaining_time.it_value.tv_sec);
        //millisecond to microsecond
        select_timeout.tv_usec = MAX(0, remaining_time.it_value.tv_nsec / 1000); 

        int select_result = select(sock_fd + 1, &read_fds, NULL, NULL, &select_timeout);
        //print_remaining_timeout(&remaining_time);
        if (select_result == -1){
            perror("Error in select");
            exit(EXIT_FAILURE);
        }
        else if (select_result == 0){
            timeout(sock_fd, recv_addr);
        }
        else if (select_result > 0 && FD_ISSET(sock_fd, &read_fds)){
            // There is incoming data from receiver
            segment *recv_segment = (segment *) malloc(sizeof(segment));
            socklen_t recv_addr_sz;

            recvfrom(sock_fd, recv_segment, sizeof(*recv_segment), 0, (struct sockaddr *)&recv_addr, &recv_addr_sz);
            printf("recv\tack\t#%d,\tsack\t#%d\n", recv_segment->head.ackNumber, recv_segment->head.sackNumber);
            
            if (recv_segment->head.ackNumber < transmit_queue[base-1]->head.seqNumber){
                dupACK(recv_segment, sock_fd, recv_addr);
            }
            else if (recv_segment->head.ackNumber >= transmit_queue[base-1]->head.seqNumber){
                newACK(recv_segment, sock_fd, recv_addr);
            }
        }
        // Reset the set for the next iteration
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
    }

    segment *fin_segment = (segment *) malloc(sizeof(segment));
    fin_segment->head.fin = 1;
    fin_segment->head.seqNumber = total_segments + 1;
    sendto(sock_fd, fin_segment, sizeof(*fin_segment), 0, (struct sockaddr *)&recv_addr, sizeof(sockaddr));
    printf("send\tfin\n");

    // keep receiving until it is finack
    while (true){
        segment *finack_segment = (segment *) malloc(sizeof(segment));
        socklen_t finack_addr_sz;
        recvfrom(sock_fd, finack_segment, sizeof(*finack_segment), 0, (struct sockaddr *)&recv_addr, &finack_addr_sz);
        if (finack_segment->head.fin == 1 && finack_segment->head.ack == 1){
            printf("recv\tfinack\n");
            break;
        }
    }
    return 0;
}