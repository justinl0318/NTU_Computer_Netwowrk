"""
    Test logs against the specification.

    The below tests are PROBABLY NOT used in the real tests, but if you want to check that
    you have a set of plausible logs, YOU SHOULD PASS THOSE TESTS.

    Real, autograde tests are not in here. They're in another file that is not given to students.

    When you finish an experiment, do the following to check the result: 
        $ python log_test.py <send_log.txt> <recv_log.txt> <agent_log.txt> <src_filepath> <dst_filepath>
    
    The text is colored by default. If you save the output as a file `op` and want to read, you can try:
        $ cat op        # tty deals with colors natually
        $ less -R op    # less cannot deal with colors without -R
        + Use vscode extensions, e.g. "ANSI Colors"
        + Modify some functions below to return non-colored text
    
    To remove colors / convert to pure text, use something like this
        $ python ...  | sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,2};?)?)?[mGK]//g"

    NOTE: If you send >2.5MB file with error rate 0, there might be a case where checkCoherency failed
          because e.g. the thing agent sends will not be received by sender.

          Maybe the buffer from linux's socket is itself doing drop due to buffer overflow
          It happens quite regularly, whenever the sender/receiver do ~2500 segments this will happen.
          Still, if this happens, timeout + cumulative ack should be able to recover from this and treat
          it as error rate rate > 0.
          
          TL;DR: Send big file with error rate 0 might still have loss.
"""

from log_parser import *
from config import *

import sys
import colorama
import unittest
import filecmp
import os
import dataclasses
import hashlib
from more_itertools import batched
from typing import *

def toColor(s: str, color: str, other: str = ""):
    # if not sys.stdout.isatty():
    #     # redirected, disable color
    #     return s
    if getattr(toColor, 'disable_color', False):
        return s
    return f'{getattr(colorama.Fore, color.upper())}{str(s)}{colorama.Style.RESET_ALL}'

def printLogError(sender_error, receiver_error, agent_error):
    print(f'{toColor("[x]", "red")} Error when parsing log:')
    for name, error_ls in [('sender', sender_error), ('receiver', receiver_error), ('agent', agent_error)]:
        if len(error_ls) == 0:
            continue
        print(f'{toColor(f"[*] {name}", "blue")}:')
        for i, reason in error_ls:
            print(f'    [-] Line {toColor(i, "yellow")}: {reason}')
    print(toColor("[x] Since we can't even parse your logs, we won't do any other tests. Exitting.", "red"))


Match = Tuple[Type, Dict]
def checkMatch(item_ls: List[LogDataclass], match_ls: List[Match]):
    """
        for every item in item_ls, check if the type and properties matches match_ls.
        every single item must match, can handle any item_ls

        e.g. If you want to check whether the seq_num received is the same as the sack sent out:
                Given item_ls: [
                    RecvData(is_fin=False, is_dropped=False, seq_num=1012, comment='in order'), 
                    SendAck(is_fin=False, ack=1012, sack=1012)
                ]
                Must match match_ls: [
                    (RecvData, {'seq_num': 1012}), 
                    (SendAck, {'sack': 1012})
                ]
            Note that we only check the fields specified in match_ls and ignore the others.
            The type is also matched.
    """
    if len(item_ls) != len(match_ls):
        # since every item must match, different list length must fail
        return False
    
    for item, (cls, attrs) in zip(item_ls, match_ls):
        # check type
        if not isinstance(item, cls):
            return False
        
        # check attribute
        # (if update(attrs) is the same then there aren't new values in attrs, thus they match)
        cmp_d = dataclasses.asdict(item)
        cmp_d.update(attrs)
        if cmp_d != dataclasses.asdict(item):
            return False
    return True

class TestClass:
    """
        Custom test class, since i don't think unittest is for this kind of testing
        and I can't customize a lot of things. Especially if I don't want to initilize
        the class sooo often and have custom input for every test.

        Must call clearFail() before any test.
        During test, if there's any error, should call fail().
        Might be exception in the testing, do the catch on your own.
        Call isFailed() after the test to see if the test succeeded.
        If the test failed, the fail message is in fail_ls.

        Tests are expected to have no arguments and only uses class' fields.
    """
    def __init__(self):
        self.clearFail()

    def clearFail(self):
        " should be called before any test functions "
        self.fail_ls = []

    def fail(self, msg: str):
        """
            if there's a fail, call this with fail message
            the metadata will be shown at the front
            will definitely return False
        """
        self.fail_ls.append(msg)
        return False

    def isFailed(self):
        " check if there's any fail during test functions "
        return len(self.fail_ls) != 0

    def doTest(self, test_methods: List[str] = None) -> List[Tuple[str, bool]]:
        """
            do all tests... just a simple implementation
            returns list of (method name, is_failed)

            if argument test_method is given, only test those methods instead
            else the test will be discovered and tested in a non-determined order
        """

        def getMethodNames(cls):
            return [
                method_name for method_name in dir(cls)
                if callable(getattr(cls, method_name))
            ]
        def colorLineNumber(s):
            """
                For fail msgs like "Line sender:2258: aaabbbccc",
                color "sender:2258" part
            """
            colored_group = toColor("\g<1>", "yellow")
            return re.subn(
                f'Line (.+): ', 
                f'Line {colored_group}: ', 
                s, 
                1
            )[0]
        
        if test_methods is None:
            # get the new methods of subclass, discarding internal functions
            test_methods = set(getMethodNames(self.__class__)).difference(set(getMethodNames(TestClass)))
            test_methods = [m for m in test_methods if not m.startswith('_')]
        result_ls = []
        return_ls = []
        
        for method in test_methods:
            print(f'{toColor("[*]", "blue")} Testing {toColor(method, "blue")}...')
            
            self.clearFail()
            getattr(self, method)()
            
            if self.isFailed():
                result_ls.append(toColor(f'{method}[X]', "red"))
                return_ls.append((method, True))
                print(toColor(f"[x] Test failed: total fail count {len(self.fail_ls)}", "red"))
                for f in self.fail_ls:
                    print(f"    [-] {colorLineNumber(f)}")
            else:
                result_ls.append(toColor(f'{method}[O]', "green"))
                return_ls.append((method, False))
                print(toColor(f"[o] Test succeeded!", "green"))
        
            print('\n' + '=' * 25 + '\n')

        print(f'{toColor("[*]", "blue")} Final results: [{", ".join(result_ls)}]')
        return return_ls
            
class LogTest(TestClass):
    """
        Test log against the specification.

        This class have most of the basic components for checking logs, and some very basic
        tests that should pass before even testing any further real tests.

        The below tests are PROBABLY not used in the real tests, but if you want to check that
        you have a set of plausible logs, you should pass those tests.

        Real, autograde tests are not in here. They're in another file that is not given to students.
    """

    def __init__(
            self, 
            sender_log: List[LogDataclass], 
            receiver_log: List[LogDataclass], 
            agent_log: List[LogDataclass], 
            src_filepath: Union[Path, str], 
            dst_filepath: Union[Path, str]
        ):
        """
            simply initialize but does not check any of its content
        """
        
        super().__init__()
        self.sender_log = sender_log
        self.receiver_log = receiver_log
        self.agent_log = agent_log
        self.src_filepath = Path(src_filepath)
        self.dst_filepath = Path(dst_filepath)
    
    def _checkFileExist(self, filepath: Path):
        return filepath.exists() and filepath.is_file()

    def _getSegmentCount(self, filepath: Path):
        """
            get the expected data segment count
            assumes _checkFileExist(filepath)
        """
        sz = os.stat(str(filepath)).st_size
        return (sz + MAX_SEG_SIZE - 1) // MAX_SEG_SIZE
    
    def _matchToStr(self, match: List[Match]):
        """
            e.g. [SendAck, {'is_fin': 1}] -> "SendAck(is_fin=1)"
            Don't use dataclass' __str__ because we don't want to output other fields
            in our fail messages
        """
        return f'{match[0].__name__}({", ".join([f"{key}={attr}" for key, attr in match[1].items()])})'

    def _failMatchLogStr(self, log_name: str, start_line: int, item_ls: List[LogDataclass], match_ls: List[Match]):
        return f'Line {log_name}:{start_line}~{start_line + len(item_ls) - 1}: Mismatch: ' \
               f'{item_ls} should match [{", ".join([self._matchToStr(m) for m in match_ls])}]'
    
    def checkAgentFormat(self):
        """
            check if the agent runs (somehow) correctly, i.e. has good format
            For simple checks only.

                operation sequence constraint:
                    get data -> (fwd, corrupt, drop) data
                    get ack -> fwd ack
                other constraint:
                    the last line is "fwd finack"

            also checks that the segment forwarded are with the same numbers
        """
        i = 0
        while i < len(self.agent_log):
            cur_item = self.agent_log[i]
            if isinstance(cur_item, GetData):
                item_ls = self.agent_log[i:i+2]
                all_match_ls = [
                    [(GetData, {'seq_num': cur_item.seq_num}), (FwdData, {'seq_num': cur_item.seq_num})],
                    [(GetData, {'seq_num': cur_item.seq_num}), (CorruptData, {'seq_num': cur_item.seq_num})],
                    [(GetData, {'seq_num': cur_item.seq_num}), (DropData, {'seq_num': cur_item.seq_num})]
                ]
                if all(not checkMatch(item_ls, match_ls) for match_ls in all_match_ls):
                    self.fail(
                        f'Line agent:{i+1}~{i+2}: Expected '
                        f'[GetData(seq_num={cur_item.seq_num}), Fwd|Corrupt|DropData(seq_num={cur_item.seq_num})], '
                        f'but get {item_ls} instead.')
                i += 2

            elif isinstance(cur_item, GetAck):
                item_ls = self.agent_log[i:i+2]
                match_ls = [(GetAck, dataclasses.asdict(cur_item)), (FwdAck, dataclasses.asdict(cur_item))]
                if not checkMatch(item_ls, match_ls):
                    self.fail(self._failMatchLogStr("agent", i+1, item_ls, match_ls))
                i += 2

            else:
                self.fail(f'Line agent:{i+1}: Unexpected {cur_item}')
                i += 1

        if not checkMatch(self.agent_log[-1:], [(FwdAck, {'is_fin': True})]):
            self.fail(f'Agent: Expected fwd finack as last line.')
        
    def checkReceiverFormat(self):
        """
            check if the receiver has good format:
            + operation sequence constraint:
                + (recv, drop) data -> send ack
                + recv data -> send ack -> flush -> sha256
            + other constraint:
                + the last line is finsha
            
            NOTE: Did not check seq_num and sack coherence...
                  Only check the appear sequence is correct
        """
        i = 0
        while i < len(self.receiver_log):
            cur_item = self.receiver_log[i]

            if isinstance(cur_item, Finsha):
                if i != len(self.receiver_log) - 1:
                    self.fail(f'Line receiver:{i+1}: Unexpected {cur_item}')
                i += 1

            elif isinstance(cur_item, RecvData):
                # recv data -> send ack (-> flush -> sha256)
                item_ls = self.receiver_log[i:i+4]
                match_ls = [
                    (RecvData, {'is_dropped': False}), (SendAck, {}), (Flush, {}), (Sha256, {}), 
                ]

                if checkMatch(item_ls, match_ls):
                    i += 4
                    continue

                if checkMatch(item_ls[:2], match_ls[:2]):
                    i += 2
                    continue

                match_ls = [
                    (RecvData, {'is_dropped': True}), (SendAck, {}) 
                ]
                if checkMatch(item_ls[:2], match_ls):
                    i += 2
                    continue
                

                self.fail(
                    f'Line receiver:{i+1}: Expected to match [RecvData, SendAck] or [RecvData(is_dropped=False), SendAck, Flush, Sha256], '
                    f'but get {item_ls} instead'
                )
                i += 1      # don't know how to jump now, it may output a lot of error message for some time
                            # until it found another RecvData or DropData

            else: 
                self.fail(f'Line receiver:{i+1}: Unexpected {cur_item}')
                i += 1
        
        if not checkMatch(self.receiver_log[-1:], [(Finsha, {})]):
            # (don't use [-1] because len(agent_log) might be 0 and that cause IndexError)
            self.fail(f'Receiver: Last line is not finsha')
                    
    def checkCoherency(self):
        """ 
            check if the send / receive sequence is correct.
            everything sent by sender and receiver should be got by agent, and the sequence
            of segments should be in order.

            (i.e. it also checks that no packet reordering happens, which shouldn't be a thing if
                  everything is on localhost.
                  If you didn't pass this test and is sure that packet reordering happened,
                  email to TA about this.)

            check by:
                1. every data packet sender sends should be gotten by agent
                2. every ack packet forwarded by agent should be gotten by sender
                3. every ack packet receiver sends should be gotten by agent
                4. every data packet agent forwards should be gotten by receiver
                5. every data packet agent corrupts should be gotten as corrupt by receiver
        """
        def matchDataPacket(msg, a_ls, b_ls):
            for (a_i, a), (b_i, b) in zip(a_ls, b_ls):
                if a.seq_num != b.seq_num:
                    self.fail(f'{msg}: First mismatch numbers happen between line [{a_i} | {b_i}]: {a} != {b}')
                    return
        def matchAckPacket(msg, a_ls, b_ls):
            for (a_i, a), (b_i, b) in zip(a_ls, b_ls):
                if a.ack != b.ack or a.sack != b.sack:
                    self.fail(f'{msg}: First mismatch numbers happen between line [{a_i} | {b_i}]: {a} != {b}')
                    return

        # 1. every data packet sender sends should be gotten by agent
        sender_data = [(f'sender:{i+1}', log) for i, log in enumerate(self.sender_log) if isinstance(log, SendData)]
        agent_got = [(f'agent:{i+1}', log) for i, log in enumerate(self.agent_log) if isinstance(log, GetData)]
        matchDataPacket('Sender/Agent data segment coherency', sender_data, agent_got)

        # 2. every ack packet forwarded by agent should be gotten by sender
        sender_ack = [(f'sender:{i+1}', log) for i, log in enumerate(self.sender_log) if isinstance(log, RecvAck)]
        agent_fwd = [(f'agent:{i+1}', log) for i, log in enumerate(self.agent_log) if isinstance(log, FwdAck)]
        matchAckPacket('Sender/Agent ack segment coherency', sender_ack, agent_fwd)

        # 3. every ack packet receiver sends should be gotten by agent
        receiver_ack = [(f'receiver:{i+1}', log) for i, log in enumerate(self.receiver_log) if isinstance(log, SendAck)]
        agent_got = [(f'agent:{i+1}', log) for i, log in enumerate(self.agent_log) if isinstance(log, GetAck)]
        matchAckPacket('Receiver/Agent ack segment coherency', receiver_ack, agent_got)

        # 4. every data packet agent forwards should be gotten by receiver
        # (remember that `corrupt` on agent actually means corrupt AND forward)
        receiver_data = [(f'receiver:{i+1}', log) for i, log in enumerate(self.receiver_log) if isinstance(log, RecvData)]
        agent_fwd = [(f'agent:{i+1}', log) for i, log in enumerate(self.agent_log) if isinstance(log, (FwdData, CorruptData))]
        matchDataPacket('Receiver/Agent data segment coherency', receiver_data, agent_fwd)

        # 5. every data packet agent corrupts should be gotten as corrupt by receiver
        receiver_data = [
            (f'receiver:{i+1}', log) for i, log in enumerate(self.receiver_log) 
            if isinstance(log, RecvData) and log.is_dropped == True and log.comment == 'corrupted'
        ]
        agent_fwd = [
            (f'agent:{i+1}', log) for i, log in enumerate(self.agent_log) 
            if isinstance(log, CorruptData)
        ]
        matchDataPacket('Receiver/Agent corruption coherency', receiver_data, agent_fwd)
    
    def checkSenderResnd(self):
        """
            check if send only happens once every seq_num
        """
        seq_set = {}    # seq_num: line_num
        for i, log in enumerate(self.sender_log):
            if not isinstance(log, SendData):
                continue

            # check resnd
            if log.seq_num in seq_set and not log.is_resnd:
                self.fail(
                    f'Line sender:{i+1}: seq_num {log.seq_num} appeared in line {seq_set[log.seq_num]}, '
                    f'but this line is not resnd'
                )
            if log.seq_num not in seq_set and log.is_resnd:
                self.fail(
                    f'Line sender:{i+1}: seq_num {log.seq_num} never appeared before '
                    f'but this line is resnd'
                )
            
            # update seq_set
            if log.seq_num not in seq_set:
                seq_set[log.seq_num] = i+1
                

if __name__ == '__main__':
    # argument: send_log, recv_log, agent_log, send_file, recv_file
    if len(sys.argv) != 6:
        print('argument: python log_test.py <send_log.txt> <recv_log.txt> <agent_log.txt> <src_filepath> <dst_filepath>')
        exit(1)

    sender_log, sender_error = parseSender(sys.argv[1])
    receiver_log, receiver_error = parseReceiver(sys.argv[2])
    agent_log, agent_error = parseAgent(sys.argv[3])
    src_filepath, dst_filepath = sys.argv[4:6]

    if len(sender_error) + len(receiver_error) + len(agent_error) != 0:
        printLogError(sender_error, receiver_error, agent_error)
        exit()

    test = LogTest(sender_log, receiver_log, agent_log, src_filepath, dst_filepath)

    test.doTest()