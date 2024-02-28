/*
    Defines some constant and header information.
    Your code should reference those defined constants instead of
    directly writing numbers in your code.
*/

#ifndef DEF_HEADER
#define DEF_HEADER

// segment data size
#define MAX_SEG_SIZE 1000

// receiver buffer size
#define MAX_SEG_BUF_SIZE 256

// the timeout is set to `(TIMEOUT_MILLISECONDS) msec` for sender
#define TIMEOUT_MILLISECONDS 1000

// header definition
struct header {
    int length;             // number of bytes of the data. does not contain header!
    int seqNumber;          // sender: current segment's sequence number, start at 1
    int ackNumber;          // receiver: cumulative ack number, 
                            //           the seq_num of last cumulative acked packets
    int sackNumber;         // receiver: selective ack number
    int fin;                // 1 if this is the last packet else 0
                            // if this is a fin (not finack) packet, seqNumber should
                            // be the last segment's seqNumber + 1
    int syn;                // (just make it 0 for this assignment)
    int ack;                // 1 if this is an ack packet else 0
    unsigned int checksum;  // sender: crc32 checksum of data
};

struct segment {
    struct header head;
    char data[MAX_SEG_SIZE];
};

#endif // DEF_HEADER