"""
    Parser for all logs.
    If you get a stroke after seeing those if-elses I'm not responsible for anything.

    It is meant for you to check the format, don't try to bypass the regex with weird input.
    Subject to change.
    
    if you don't have any one of those files, just put an empty file or /dev/null instead
"""

from ops import *
from pathlib import Path
import re
import sys

def parseSender(filepath: Path):
    op_ls = []
    error_line_num = []     # (line_i, reason)
    with open(filepath, 'r') as fin:
        for line_i, line in enumerate(fin, 1):
            if (m := re.match("send\tdata\t#(\\d+),\twinSize = (\\d+)\n", line)):
                op_ls.append(SendData(seq_num=int(m.group(1)), cwnd=int(m.group(2))))
            elif (m := re.match("resnd\tdata\t#(\\d+),\twinSize = (\\d+)\n", line)):
                op_ls.append(SendData(is_resnd=True, seq_num=int(m.group(1)), cwnd=int(m.group(2))))
            elif (m := re.match("recv\tack\t#(\\d+),\tsack\t#(\\d+)\n", line)):
                op_ls.append(RecvAck(ack=int(m.group(1)), sack=int(m.group(2))))
            elif (m := re.match("time\tout,\tthreshold = (\\d+),\twinSize = (\\d+)\n", line)):
                op_ls.append(Timeout(threshold=int(m.group(1)), cwnd=int(m.group(2))))
            elif (m := re.match("send\tfin\n", line)):
                op_ls.append(SendData(is_fin=True))
            elif (m := re.match("recv\tfinack\n", line)):
                op_ls.append(RecvAck(is_fin=True))
            else:
                error_line_num.append((line_i, f'Cannot parse {repr(line.encode())[1:]}'))
    return op_ls, error_line_num
            
def parseReceiver(filepath: Path):
    op_ls = []
    error_line_num = []     # (line_i, reason)
    with open(filepath, 'r') as fin:
        for line_i, line in enumerate(fin, 1):
            if (m := re.match("recv\tdata\t#(\\d+)\t\(([^)]+)\)\n", line)):
                if m.group(2) not in ['in order', 'out of order, sack-ed']:
                    error_line_num.append((line_i, 'RecvData: wrong comment'))
                    continue
                op_ls.append(RecvData(seq_num=int(m.group(1)), comment=m.group(2)))
            elif (m := re.match("drop\tdata\t#(\\d+)\t\(([^)]+)\)\n", line)):
                if m.group(2) not in ['buffer overflow', 'corrupted']:
                    error_line_num.append((line_i, 'RecvData: wrong comment'))
                op_ls.append(RecvData(is_dropped=True, seq_num=int(m.group(1)), comment=m.group(2)))
            elif (m := re.match("send\tack\t#(\\d+),\tsack\t#(\\d+)\n", line)):
                op_ls.append(SendAck(ack=int(m.group(1)), sack=int(m.group(2))))
            elif (m := re.match("flush\n", line)):
                op_ls.append(Flush())
            elif (m := re.match("recv\tfin\n", line)):
                op_ls.append(RecvData(is_fin=True))
            elif (m := re.match("send\tfinack\n", line)):
                op_ls.append(SendAck(is_fin=True))
            elif (m := re.match("sha256\t(\\d+)\t(.+)\n", line)):
                op_ls.append(Sha256(n_bytes=int(m.group(1)), hexdigest=m.group(2)))
            elif (m := re.match("finsha\t(.+)\n", line)):
                op_ls.append(Finsha(hexdigest=m.group(1)))
            else:
                error_line_num.append((line_i, f'Cannot parse {repr(line.encode())[1:]}'))
    return op_ls, error_line_num

def parseAgent(filepath: Path):
    op_ls = []
    error_line_num = []     # (line_i, reason)
    with open(filepath, 'r') as fin:
        for line_i, line in enumerate(fin, 1):
            if (m := re.match("get\tdata\t#(\\d+)\n", line)):
                op_ls.append(GetData(seq_num=int(m.group(1))))
            elif (m := re.match("get\tack\t#(\\d+),\tsack\t#(\\d+)\n", line)):
                op_ls.append(GetAck(ack=int(m.group(1)), sack=int(m.group(2))))
            elif (m := re.match("fwd\tdata\t#(\\d+),\terror rate = ([01].\\d{4})\n", line)):
                op_ls.append(FwdData(seq_num=int(m.group(1)), error_rate=float(m.group(2))))
            elif (m := re.match("fwd\tack\t#(\\d+),\tsack\t#(\\d+)\n", line)):
                op_ls.append(FwdAck(ack=int(m.group(1)), sack=int(m.group(2))))
            elif (m := re.match("drop\tdata\t#(\\d+),\terror rate = ([01].\\d{4})\n", line)):
                op_ls.append(DropData(seq_num=int(m.group(1)), error_rate=float(m.group(2))))
            elif (m := re.match("corrupt\tdata\t#(\\d+),\terror rate = ([01].\\d{4})\n", line)):
                op_ls.append(CorruptData(seq_num=int(m.group(1)), error_rate=float(m.group(2))))
            elif (m := re.match("get\tfin\n", line)):
                op_ls.append(GetData(is_fin=True))
            elif (m := re.match("fwd\tfin\n", line)):
                op_ls.append(FwdData(is_fin=True))
            elif (m := re.match("get\tfinack\n", line)):
                op_ls.append(GetAck(is_fin=True))
            elif (m := re.match("fwd\tfinack\n", line)):
                op_ls.append(FwdAck(is_fin=True))
            else:
                error_line_num.append((line_i, f'Cannot parse {repr(line.encode())[1:]}'))
    return op_ls, error_line_num

if __name__ == '__main__':
    sender_log, sender_error = parseSender(sys.argv[1])
    receiver_log, receiver_error = parseReceiver(sys.argv[2])
    agent_log, agent_error = parseAgent(sys.argv[3])