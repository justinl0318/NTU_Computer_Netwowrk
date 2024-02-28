from dataclasses import dataclass
import numpy as np


class LogDataclass:
    # for typing
    pass

""" Sender """
@dataclass
class SendData(LogDataclass):
    is_fin: bool = False
    is_resnd: bool = False
    seq_num: int = 0
    cwnd: int = 0

@dataclass
class RecvAck(LogDataclass):
    is_fin: bool = False
    ack: int = 0
    sack: int = 0
    
@dataclass
class Timeout(LogDataclass):
    threshold: int
    cwnd: int

""" Receiver """
@dataclass
class RecvData(LogDataclass):
    is_fin: bool = False
    is_dropped: bool = False
    seq_num: int = 0
    comment: str = ""

@dataclass
class SendAck(LogDataclass):
    is_fin: bool = False
    ack: int = 0
    sack: int = 0

@dataclass
class Flush(LogDataclass):
    pass

@dataclass
class Sha256(LogDataclass):
    n_bytes: int = 0
    hexdigest: str = ""

@dataclass
class Finsha(LogDataclass):
    hexdigest: str = ""

""" Agent """
@dataclass
class GetData(LogDataclass):
    is_fin: bool = False
    seq_num: int = 0

@dataclass
class FwdData(LogDataclass):
    is_fin: bool = False
    seq_num: int = 0
    error_rate: float = 0.0

@dataclass
class DropData(LogDataclass):
    seq_num: int = 0
    error_rate: float = 0.0

@dataclass
class CorruptData(LogDataclass):
    seq_num: int = 0
    error_rate: float = 0.0

@dataclass
class GetAck(LogDataclass):
    is_fin: bool = False
    ack: int = 0
    sack: int = 0

@dataclass
class FwdAck(LogDataclass):
    is_fin: bool = False
    ack: int = 0
    sack: int = 0

