"SACK"

"""
# Sender

Define:
    - Transmit queue: the whole sequence of segments waiting to be sent, in order of seqNumber. 
                      When a segment is sacked, it is removed from the queue 
                      (i.e. only unsacked segments is in the transmit queue.)
    - Window: The first `cwnd` segments in transmit queue. 
    - Base: The first segment after all cumulative acked segments, 
            (i.e. the first segment in transmit queue)
            (i.e. the expected in-order ack number)
"""

# *** function ***
def resetTimer():
    return "Reset timer to a specific time (TIMEOUT_MILLISECONDS msec)."
def transmitNew():
    return """
        After you remove some segments in the window or the cwnd increases, there will be more segments
        that is contained inside the window.
        (Re)transmit those segments, no matter whether this segment has ever been sent before.
    """
def transmitMissing():
    return "(Re)transmit the first segment in the window."
def setState(state):
    return "Go to a specific state."
def isAtState(state):
    return "True if the state is at `state` else False"
def markSACK(seq_num):
    return "Remove the segment with sequence number seqNumber from the transmit queue"
def updateBase(ack_num): 
    return """
        Update base, transmit queue and window s.t. base > ackNumber
    """

# *** state ***
SlowStart = "slow start state"
CongestionAvoidance = "congestion avoidance state"


# *** variables ***
cwnd, thresh, dup_ack = 1, 16, 0
# `cwnd` is a double. We define the window as `int(cwnd)` (i.e. `floor(cwnd)`)
# `thresh` is int
# there is also other variables that you might need to keep, e.g. timer, base, etc.

# *** event callback ***
def init():
    """ initialize """
    cwnd = 1
    thresh = 16
    dup_ack = 0
    transmitNew()   # that is, transmit the first segment
    resetTimer()
    setState(SlowStart)

def timeout():
    """ timer alerted """
    thresh = max(1, int(cwnd / 2))
    cwnd = 1
    dup_ack = 0
    transmitMissing()                                                                                                                                                                                                                                                                                                                        
    resetTimer()
    setState(SlowStart)

def dupACK(pkt):
    """ 
        get duplicate ack (i.e. ackNumber < base)
        note that even if we sacked this segment, this is counted as dupACK
        if the cumulative ack is not updated
    """
    dup_ack += 1
    markSACK(pkt.sack_num)
    transmitNew()
    if dup_ack == 3:
        transmitMissing()   # no matter whether transmitNew() had transmitted anything,
                            # this unconditionally transmit the first segment in the queue

def newACK(pkt):
    """ 
        get new ack, i.e. ackNumber > base
    """
    dup_ack = 0
    markSACK(pkt.sack_num)
    updateBase(pkt.ack_num)
    if isAtState(SlowStart):
        cwnd += 1           # exponential increasing
        if (cwnd >= thresh):
            setState(CongestionAvoidance)
    elif isAtState(CongestionAvoidance):
        cwnd += double(1) / int(cwnd)    # heuristic linear increase
    transmitNew()
    resetTimer()

"""
after everything in the transmit queue is sent, just sent a FIN and receive a FINACK.
"""

# --------------------------------------------------------------------------------

"""
# receiver
"""

"""
Define:
    - Base: The first segment after all cumulative acked segments, 
            (i.e. cumulative ack number = base - 1)

When we drop a packet, we still need to send a sack packet back.
We define that in this case, set `sackNumber = ackNumber`.

Note that FIN is sent at the very last stage
"""

# function
def flush():
    return "Flush buffer and deliver to application (i.e. hash and store)"
def isAllReceived():
    return """
        True if every packet (i.e. packet before AND INCLUDING fin) is received.
        This actually should happen when you receive FIN, no matter what.
    """
def endReceive():
    return "Indicate that this connection is finished"
def isBufferFull():
    return "True if the buffer is full else False"
def isCorrupt():
    return "True if the packet is corrupted else False"
def sendSACK(ack_seq_num, sack_seq_num, is_fin=False): 
    return "Send a SACK packet"
def markSACK(seq_num): 
    return """
        Mark and put segment with sequence number seq_num in buffer
        (only if it is in current buffer range, if it is over buffer range then 
        you should've dropped this packet.)
    """
def updateBase(ack_num): 
    return """
        Update base and buffer s.t. base is the first unsacked packet
    """
def isOverBuffer(seq_num): 
    return """
        True if the sequence number is above buffer range
        e.g. if the buffer stores sequence number in range [1, 257) and receives
            a segment with seqNumber 257 (or above 257), return True
    """

# variable
base = 1
# there may be other variables you may need to maintain

def receiveDataPacket(pkt):
    if isCorrupt():
        """ Corrupt: drop """
        # (still send sack, but effectively only cumulative ack)
        sendSACK(ack_seq_num=base-1, sack_seq_num=base-1, is_fin=False)
    elif pkt.seq_num == base:
        """ In-order """
        markSACK(pkt.seq_num)
        updateBase()
        sendSACK(ack_seq_num=base-1, sack_seq_num=pkt.seq_num, is_fin=pkt.fin)
        if isAllReceived():
            flush()
            endReceive()
        elif isBufferFull():
            flush()
    else:
        """ Out-of-order """
        if isOverBuffer(pkt.seq_num):
            # out of buffer range (buffer_end), drop
            # (still send sack, but effectively only cumulative ack)
            sendSACK(ack_seq_num=base-1, sack_seq_num=base-1, is_fin=False)
        else:
            # out of order sack or under buffer range
            # just do sack the normal way
            markSACK(pkt.seq_num)
            sendSACK(ack_seq_num=base-1, sack_seq_num=pkt.seq_num, is_fin=pkt.fin)