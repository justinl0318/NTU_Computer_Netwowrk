# Log Specification

Those are all the log lines you need to write to stdout. 
Make sure that you follow those formats closely, since we may use an autograding script to parse your logs.

Don't send anything other than those in stdout. If you want to debug / have your own log, make sure that you use stderr for those instead.

## Sender

Define:
- `cwnd, threshold`: The window size and threshold AFTER you update them.
  - if you follow sack_fsm.py, you can output those during transmitNew and transmitMissing.
- `seqNumber, ackNumber, sackNumber`: The numbers defined in packet header.

```cpp
// data and ack segment's send and receive
// should use `resnd` if this segment has ever been sent before
printf("send\tdata\t#%d,\twinSize = %d\n", seqNumber, int(cwnd));
printf("resnd\tdata\t#%d,\twinSize = %d\n", seqNumber, int(cwnd));
printf("recv\tack\t#%d,\tsack\t#%d\n", ackNumber, sackNumber);

// timeout
printf("time\tout,\tthreshold = %d,\twinSize = %d\n", threshold, int(cwnd));

// fin-related send and receive
printf("send\tfin\n");
printf("recv\tfinack\n");
```

## Receiver

Define:
- `seqNumber, ackNumber, sackNumber`: The numbers defined in packet header.
- `n_bytes`: The number of bytes of data that is digested in the sha256 hash.
- `hexdigest`: The sha256 hash. This should be a string of length 64.
  - You can see how to output this in `sha256.c`.

```cpp
// receive-related log
// in order receive (i.e., this segment increases cumulative ACK number)
printf("recv\tdata\t#%d\t(in order)\n", seqNumber);
// a segment can be out-of-order sacked multiple time                                                   
// (also send this if a packet is under buffer range!)
printf("recv\tdata\t#%d\t(out of order, sack-ed)\n", seqNumber); 
// when the packet is above buffer range or corrupted
printf("drop\tdata\t#%d\t(buffer overflow)\n", seqNumber); 
printf("drop\tdata\t#%d\t(corrupted)\n", seqNumber);

// send ack (there's no resnd ack)
printf("send\tack\t#%d,\tsack\t#%d\n", ackNumber, sackNumber);

// flush buffer
printf("flush\n");

// fin-related send and receive
printf("recv\tfin\n");
printf("send\tfinack\n");

// hash-related log
// should output this after you flush, INCLUDING LAST TIME
printf("sha256\t%d\t%s\n", n_bytes, hexDigest(hash)); 
// output the hash of whole file 
// (so the whole file hash will be printed 2 times: sha256 & finsha)
printf("finsha\t%s\n", hexDigest(hash));

// Note the sequence of recv, send, flush, sha256, and finsha:
// You should do the log in this sequence:
//    recv a data or FIN segment
// -> send out corresponding ACK segmnt
// -> flush the buffer (if the buffer needs to be flushed)
// -> output sha256 hash (if you flushed the buffer)
// -> output finsha (if the file stream is complete)
```

## Agent

> You don't really need to do anything to the given agent code. Those are for your reference.

Define:
- `seqNumber, ackNumber, sackNumber`: The numbers defined in packet header.
- `error_rate`: Defined as the total number of error data packets divided by the total number of data packets.

```cpp
// get-related log
printf("get\tdata\t#%d\n", seqNumber);
printf("get\tack\t#%d,\tsack\t#%d\n", ackNumber, sackNumber);

// operation on the packet
printf("fwd\tdata\t#%d,\terror rate = %.4f\n", seqNumber, error_rate);
printf("fwd\tack\t#%d,\tsack\t#%d\n", ackNumber, sackNumber);

// drop: simply not send the packet to receiver
printf("drop\tdata\t#%d,\terror rate = %.4f\n", seqNumber, error_rate);
// corrupt: corrupt hash AND forward the packet to receiver
printf("corrupt\tdata\t#%d,\terror rate = %.4f\n", seqNumber, error_rate);

// fin-related log
printf("get\tfin\n");
printf("fwd\tfin\n");
printf("get\tfinack\n");
printf("fwd\tfinack\n");    // should exit after this
```