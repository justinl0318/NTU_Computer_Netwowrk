/* NOTE: The sample code only provides packet formats. No congestion control or go-back-N checking. */
/* NOTE: UDP socket is connectionless. Thus, the target IP and port should be specified whenever sending a packet. */
/* NOTE: This agent will not exit. To exit, use Ctrl+C or kill. */
/* HINT: We suggest using "bind," "sendto," and "recvfrom" in system/socket.h. */

/*
 * Connection Rules:
 * In this homework, each "agent," "sender," and "receiver" will bind a UDP socket to their ports, respectively, for sending and receiving packets.
 * When "agent" receives a packet, it will identify whether the packet sender is "sender" or "receiver" by the packet sender's IP and port.
 * When "sender" or "receiver" receives a packet, the packet sender is always "agent."
 * When "sender" or "receiver" wants to send a packet, they should know the IP and ports for "agent" first and send a packet by socket previously bound.
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

#include "def.h"

void setIP(char *dst, const char *src){
    if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0) {
        sscanf("127.0.0.1", "%s", dst);
    }
    else {
        sscanf(src, "%s", dst);
    }
    return;
}

void corruptData(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] = ~data[i];
    }
}

int main(int argc, char* argv[]){
    int agentsocket, portNum, nBytes;
    float error_rate;
    struct segment s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, recv_size, tmp_size;
    char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
    int sendPort, agentPort, recvPort;

    setvbuf(stdin, 0, _IONBF, 0);
    setvbuf(stdout, 0, _IONBF, 0);
    
    if (argc != 7) {
        fprintf(stderr,"Usage: %s <agent port> <sender IP> <sender port> <receiver IP> <receiver port> <error_rate>\n", argv[0]);
        fprintf(stderr, "E.g., ./agent 8888 local 8887 local 8889 0.3\n");
        exit(1);
    }
    else {
        sscanf(argv[1], "%d", &agentPort);               // agent
        setIP(agentIP, "local");

        sscanf(argv[2], "%s", tmpIP);   // sender
        setIP(sendIP, tmpIP);
        sscanf(argv[3], "%d", &sendPort);   // sender

        sscanf(argv[4], "%s", tmpIP);   // receiver
        setIP(recvIP, tmpIP);
        sscanf(argv[5], "%d", &recvPort);   // receiver

        sscanf(argv[6], "%f", &error_rate);
    }

    /* Create UDP socket */
    agentsocket = socket(PF_INET, SOCK_DGRAM, 0);

    /* Configure settings in sender struct */
    sender.sin_family = AF_INET;
    sender.sin_port = htons(sendPort);
    sender.sin_addr.s_addr = inet_addr(sendIP);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));  

    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    

    /* bind socket */
    bind(agentsocket,(struct sockaddr *)&agent,sizeof(agent));

    /* Configure settings in receiver struct */
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(recvPort);
    receiver.sin_addr.s_addr = inet_addr(recvIP);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 

    /* Initialize size variable to be used later on */
    sender_size = sizeof(sender);
    recv_size = sizeof(receiver);
    tmp_size = sizeof(tmp_addr);

    fprintf(stderr, "Start!! ^Q^\n");
    fprintf(stderr, "sender info: ip = %s port = %d and receiver info: ip = %s port = %d\n",
        sendIP, sendPort, recvIP, recvPort);
    fprintf(stderr, "agent info: ip = %s port = %d\n", agentIP, agentPort);

    int total_data = 0;
    int error_data = 0;
    int segment_size, index;
    char ipfrom[1000];
    char *ptr;
    int portfrom;
    srand(time(NULL));
    while (1) {
        /* Receive message from receiver and sender */
        memset(&s_tmp, 0, sizeof(s_tmp));
        segment_size = recvfrom(agentsocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
        if (segment_size > 0) {
            inet_ntop(AF_INET, &tmp_addr.sin_addr.s_addr, ipfrom, sizeof(ipfrom));
            portfrom = ntohs(tmp_addr.sin_port);

            if (strcmp(ipfrom, sendIP) == 0 && portfrom == sendPort) {
                /* segment from sender, not ack */
                if (s_tmp.head.ack) {
                    fprintf(stderr, "Receive ack segment from \"sender\".\n");
                    exit(1);
                }
                total_data++;
                if (s_tmp.head.fin == 1) {
                    printf("get\tfin\n");
                    sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&receiver, recv_size);
                    printf("fwd\tfin\n");
                }
                else {
                    index = s_tmp.head.seqNumber;
                    printf("get\tdata\t#%d\n", index);
                    if (rand() % 10000 < 10000 * error_rate) {
                        error_data++;                        
                        if (rand() % 2 == 0) {   // drop a packet
                            printf("drop\tdata\t#%d,\terror rate = %.4f\n", index, (float)error_data/total_data);
                        }
                        else {  // corrupt a packet
                            printf("corrupt\tdata\t#%d,\terror rate = %.4f\n", index, (float)error_data/total_data);
                            corruptData(s_tmp.data, MAX_SEG_SIZE);
                            sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&receiver, recv_size);
                        }
                    } else {
                        sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&receiver, recv_size);
                        printf("fwd\tdata\t#%d,\terror rate = %.4f\n", index, (float)error_data/total_data);
                    }
                }
            } 
            else if (strcmp(ipfrom, recvIP) == 0 && portfrom == recvPort) {
                /* segment from receiver, ack */
                if (s_tmp.head.ack == 0) {
                    fprintf(stderr, "Receive non-ack segment from \"receiver\"\n");
                    exit(1);
                }
                if (s_tmp.head.fin == 1) {
                    printf("get\tfinack\n");
                    sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&sender, sender_size);
                    printf("fwd\tfinack\n");
                    break;
                } else {
                    printf("get\tack\t#%d,\tsack\t#%d\n", s_tmp.head.ackNumber, s_tmp.head.sackNumber);
                    sendto(agentsocket, &s_tmp, segment_size, 0, (struct sockaddr *)&sender, sender_size);
                    printf("fwd\tack\t#%d,\tsack\t#%d\n", s_tmp.head.ackNumber, s_tmp.head.sackNumber);
                }
            } else {
                // this should not happen, something is wrong
                fprintf(stderr, "Receive something from ip \"%s\" and port \"%d\"\n", ipfrom, portfrom);
                fprintf(stderr, "strcmp(ipfrom(\"%s\"), sendIP(\"%s\")): %d\n", ipfrom, sendIP, strcmp(ipfrom, sendIP));
                fprintf(stderr, "strcmp(ipfrom(\"%s\"), recvIP(\"%s\")): %d\n", ipfrom, recvIP, strcmp(ipfrom, recvIP));
                fprintf(stderr, "portfrom(%d) == sendPort(%d): %d\n", portfrom, sendPort, portfrom == sendPort);
                fprintf(stderr, "portfrom(%d) == recvPort(%d): %d\n", portfrom, recvPort, portfrom == recvPort);
            }
        }
    }

    return 0;
}
