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
#include <zlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <string.h>

#include "def.h"

using namespace std;
#define FILESIZE 10240000 //10 MB

segment *buffer[MAX_SEG_BUF_SIZE];
char file_arr[FILESIZE];
int file_copy_offset = 0;
int base;
int flush_count; //count how many times the buffer has been flushed
int file_size = 0;
bool endflag;
int fd;

void setIP(char *dst, char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

char *hexDigest(const void *buf, int len) {
    const unsigned char *cbuf = (const unsigned char *)buf;
    char *hx = (char *) malloc(len * 2 + 1); // Each byte requires 2 characters, plus 1 for null terminator

    for (int i = 0; i < len; ++i)
        sprintf(hx + i * 2, "%02x", cbuf[i]);

    return hx;
}

char *printSHA256(){
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_MD_CTX *sha256 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha256, EVP_sha256(), NULL);

    // Process the entire string
    EVP_DigestUpdate(sha256, file_arr, file_copy_offset); // Exclude null terminator

    // Calculate the final hash
    EVP_DigestFinal_ex(sha256, hash, &hash_len);
    return hexDigest(hash, hash_len);
}

// Flush buffer and deliver to application (i.e. hash and store)
void flush(){
    for (int i = 0; i < MAX_SEG_BUF_SIZE; i++){
        if (buffer[i] != NULL){
            memcpy(file_arr + file_copy_offset, buffer[i]->data, buffer[i]->head.length);
            file_copy_offset += buffer[i]->head.length;
        }
        buffer[i] = NULL;
    }
    flush_count++;
    base = 1;
    printf("flush\n");
    cout << "sha256\t" << file_copy_offset << "\t" << printSHA256() << endl;
}

// True if every packet (i.e. packet before AND INCLUDING fin) is received.
// This actually should happen when you receive FIN, no matter what.
int isAllReceived(segment *segment, int sock_fd, struct sockaddr_in recv_addr){
    if (segment->head.fin == 1){
        printf("recv\tfin\n");
        segment->head.ack = 1;
        sendto(sock_fd, segment, sizeof(*segment), 0, (struct sockaddr *)&recv_addr, sizeof(sockaddr));
        printf("send\tfinack\n");
        endflag = true;
        return true;
    }
    return false;
}

void endReceive(int sock_fd, struct sockaddr_in recv_addr){
    cout << "finsha\t" << printSHA256() << endl;
    //printf("content = %s", file_arr);
    write(fd, file_arr, file_copy_offset);
}

bool isBufferFull(){
    for (int i = 0; i < MAX_SEG_BUF_SIZE; i++){
        if (buffer[i] == NULL) return false;
    }
    return true;
}

bool isCorrupt(segment *recv_segment){
    if (recv_segment->head.fin == 1) return false;

    unsigned long checksum = crc32(0L, (const Bytef *)recv_segment->data, MAX_SEG_SIZE);
    if (checksum != recv_segment->head.checksum) return true;
    return false;
}

void sendSACK(int ack_seq_num, int sack_seq_num, bool is_fin, int sock_fd, struct sockaddr_in recv_addr){
    segment *ack_segment = (segment *) malloc(sizeof(segment));
    ack_segment->head.ackNumber = ack_seq_num;
    ack_segment->head.sackNumber = sack_seq_num;
    ack_segment->head.fin = false;
    ack_segment->head.ack = 1;
    sendto(sock_fd, ack_segment, sizeof(*ack_segment), 0, (struct sockaddr *)&recv_addr, sizeof(sockaddr));
    printf("send\tack\t#%d,\tsack\t#%d\n", ack_seq_num, sack_seq_num);
}

// Mark and put segment with sequence number seq_num in buffer
void markSACK(segment *segment){
    buffer[(segment->head.seqNumber - 1) % MAX_SEG_BUF_SIZE] = segment;
}

// Update base and buffer s.t. base is the first unsacked packet
void updateBase(){
    // base means cumulative ACK here
    for (int i = base + 1; i <= MAX_SEG_BUF_SIZE; i++){
        if (buffer[i-1] == NULL){
            base = i;
            return;
        }
    }
    
    // if did not return above, that means after receiving this segment, buffer is full
    base = MAX_SEG_BUF_SIZE + 1;
    return;
}

// True if the sequence number is above buffer range
// e.g. if the buffer stores sequence number in range [1, 257) and receives
// a segment with seqNumber 257 (or above 257), return True
bool isOverBuffer(int seq_num){
    if (seq_num > MAX_SEG_BUF_SIZE * (flush_count + 1)) return true;
    return false;
}

void receiveDataPacket(segment *segment, int sock_fd, struct sockaddr_in recv_addr){
    if (isCorrupt(segment)){
        //corrupted segments
        printf("drop\tdata\t#%d\t(corrupted)\n", segment->head.seqNumber);
        sendSACK(MAX_SEG_BUF_SIZE * flush_count + base - 1, MAX_SEG_BUF_SIZE * flush_count + base - 1, false, sock_fd, recv_addr);
    }
    else if (segment->head.seqNumber == (MAX_SEG_BUF_SIZE * flush_count + base)){
        //in order segments
        updateBase(); // important: update base first
        //not fin segments
        if (segment->head.fin == 0){
            printf("recv\tdata\t#%d\t(in order)\n", segment->head.seqNumber);
            markSACK(segment);
            // base - 1 because update base first
            sendSACK(MAX_SEG_BUF_SIZE * flush_count + base - 1, segment->head.seqNumber, segment->head.fin, sock_fd, recv_addr);
        }
        
        if (isAllReceived(segment, sock_fd, recv_addr)){
            flush();
            endReceive(sock_fd, recv_addr);
        }
        else if (isBufferFull()){
            flush();
        }
    }
    else{
        //Out of order
        if (isOverBuffer(segment->head.seqNumber)){
            // out of buffer range (buffer_end), drop
            // (still send sack, but effectively only cumulative ack)
            printf("drop\tdata\t#%d\t(buffer overflow)\n", segment->head.seqNumber); 
            sendSACK(MAX_SEG_BUF_SIZE * flush_count + base - 1, MAX_SEG_BUF_SIZE * flush_count + base - 1, false, sock_fd, recv_addr);
        }
        else{
            // out of order sack or under buffer range
            // just do sack the normal way
            printf("recv\tdata\t#%d\t(out of order, sack-ed)\n", segment->head.seqNumber); 
            markSACK(segment);
            sendSACK(MAX_SEG_BUF_SIZE * flush_count + base - 1, segment->head.seqNumber, segment->head.fin, sock_fd, recv_addr);
        }
    }
}

// ./receiver <recv_ip> <recv_port> <agent_ip> <agent_port> <dst_filepath>
int main(int argc, char *argv[]) {
    // parse arguments
    if (argc != 6) {
        cerr << "Usage: " << argv[0] << " <recv_ip> <recv_port> <agent_ip> <agent_port> <dst_filepath>" << endl;
        exit(1);
    }

    int recv_port, agent_port;
    char recv_ip[50], agent_ip[50];

    // read argument
    setIP(recv_ip, argv[1]);
    sscanf(argv[2], "%d", &recv_port);

    setIP(agent_ip, argv[3]);
    sscanf(argv[4], "%d", &agent_port);

    char *filepath = argv[5];
    unlink(filepath); // delete file first if it exists
    fd = open(filepath, O_CREAT | O_WRONLY | O_TRUNC, 0777);

    // make socket related stuff
    int sock_fd = socket(PF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in recv_addr;
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(agent_port);
    recv_addr.sin_addr.s_addr = inet_addr(agent_ip);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(recv_port);
    addr.sin_addr.s_addr = inet_addr(recv_ip);
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));    
    bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr));

    memset(file_arr, 0, sizeof(file_arr));

    base = 1;
    flush_count = 0; //at the beginning, buffer has range [1, MAX_SEG_BUF_SIZE]
    endflag = false;
    socklen_t recv_addr_sz;
    while (endflag == false){
        segment *recv_segment = (segment *) malloc(sizeof(segment));
        recvfrom(sock_fd, recv_segment, sizeof(*recv_segment), 0, (struct sockaddr *)&recv_addr, &recv_addr_sz);
        receiveDataPacket(recv_segment, sock_fd, recv_addr);
    }
}