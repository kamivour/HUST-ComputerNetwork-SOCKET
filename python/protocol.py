"""
protocol.py - Shared protocol for TCP Chat Application
Compatible with C++ server protocol (length-prefixed JSON)
"""

import json
import struct
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional
from datetime import datetime


class MessageType(IntEnum):
    """Message types matching C++ Protocol.h"""
    REGISTER = 1
    LOGIN = 2
    LOGOUT = 3
    CHANGE_PASSWORD = 4

    MSG_GLOBAL = 10
    MSG_PRIVATE = 11

    ONLINE_LIST = 20
    USER_STATUS = 21
    USER_INFO = 22

    KICK_USER = 30
    BAN_USER = 31
    UNBAN_USER = 32
    MUTE_USER = 33
    UNMUTE_USER = 34
    PROMOTE_USER = 35
    DEMOTE_USER = 36

    GET_ALL_USERS = 40
    GET_BANNED_LIST = 41
    GET_MUTED_LIST = 42

    KICKED = 50
    BANNED = 51
    MUTED = 52
    UNMUTED = 53

    OK = 100
    ERROR = 101

    PING = 200
    PONG = 201


@dataclass
class Message:
    """Message structure matching C++ Protocol::Message"""
    type: MessageType
    sender: str = ""
    receiver: str = ""
    content: str = ""
    timestamp: str = ""
    extra: str = ""

    def to_dict(self) -> dict:
        return {
            "type": int(self.type),
            "sender": self.sender,
            "receiver": self.receiver,
            "content": self.content,
            "timestamp": self.timestamp,
            "extra": self.extra
        }

    @classmethod
    def from_dict(cls, data: dict) -> 'Message':
        return cls(
            type=MessageType(data.get("type", 0)),
            sender=data.get("sender", ""),
            receiver=data.get("receiver", ""),
            content=data.get("content", ""),
            timestamp=data.get("timestamp", ""),
            extra=data.get("extra", "")
        )


def serialize(msg: Message) -> bytes:
    """Serialize message to bytes with 4-byte length prefix (big-endian)"""
    payload = json.dumps(msg.to_dict()).encode('utf-8')
    length = len(payload)
    header = struct.pack('>I', length)  # Big-endian unsigned int
    return header + payload


def deserialize(data: bytes) -> Optional[Message]:
    """Deserialize bytes to Message"""
    try:
        payload = json.loads(data.decode('utf-8'))
        return Message.from_dict(payload)
    except (json.JSONDecodeError, UnicodeDecodeError):
        return None


def serialize_to_dict(msg: Message) -> dict:
    """Serialize message to dict (for C++ DLL which handles JSON conversion)"""
    return msg.to_dict()


def deserialize_from_dict(data: dict) -> Optional[Message]:
    """Deserialize dict to Message (for C++ DLL which returns parsed JSON)"""
    try:
        return Message.from_dict(data)
    except Exception:
        return None


class MessageBuffer:
    """Buffer for handling TCP stream fragmentation"""

    def __init__(self):
        self.buffer = b""

    def append(self, data: bytes):
        self.buffer += data

    def has_complete_message(self) -> bool:
        if len(self.buffer) < 4:
            return False
        length = struct.unpack('>I', self.buffer[:4])[0]
        return len(self.buffer) >= 4 + length

    def extract_message(self) -> Optional[Message]:
        if not self.has_complete_message():
            return None

        length = struct.unpack('>I', self.buffer[:4])[0]
        payload = self.buffer[4:4+length]
        self.buffer = self.buffer[4+length:]

        return deserialize(payload)

    def clear(self):
        self.buffer = b""


# Helper functions to create common messages
def create_login_message(username: str, password: str) -> Message:
    content = json.dumps({"username": username, "password": password})
    return Message(type=MessageType.LOGIN, content=content)


def create_register_message(username: str, password: str) -> Message:
    content = json.dumps({"username": username, "password": password})
    return Message(type=MessageType.REGISTER, content=content)


def create_logout_message() -> Message:
    return Message(type=MessageType.LOGOUT)


def create_global_message(sender: str, content: str) -> Message:
    return Message(
        type=MessageType.MSG_GLOBAL,
        sender=sender,
        content=content,
        timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    )


def create_private_message(sender: str, receiver: str, content: str) -> Message:
    return Message(
        type=MessageType.MSG_PRIVATE,
        sender=sender,
        receiver=receiver,
        content=content,
        timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    )


def create_ping_message() -> Message:
    return Message(type=MessageType.PING)


def create_change_password_message(old_password: str, new_password: str) -> Message:
    content = json.dumps({"oldPassword": old_password, "newPassword": new_password})
    return Message(type=MessageType.CHANGE_PASSWORD, content=content)
