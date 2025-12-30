"""
socket_wrapper.py - Python ctypes wrapper for C++ socket DLL

This module loads socket_client.dll and provides a Pythonic interface
to the C++ Winsock socket operations.
"""

import ctypes
import os
import json
from typing import Optional, Dict, Any

class SocketClient:
    """Wrapper class for C++ socket DLL"""

    def __init__(self):
        self._dll = None
        self._initialized = False
        self._load_dll()

    def _load_dll(self):
        """Load the C++ socket DLL"""
        # Try to find DLL in same directory as this script
        script_dir = os.path.dirname(os.path.abspath(__file__))
        dll_path = os.path.join(script_dir, "socket_client.dll")

        # Also check cpp_socket subdirectory
        if not os.path.exists(dll_path):
            dll_path = os.path.join(script_dir, "cpp_socket", "socket_client.dll")

        if not os.path.exists(dll_path):
            raise FileNotFoundError(
                f"socket_client.dll not found. Please build it first.\n"
                f"Looked in:\n"
                f"  - {os.path.join(script_dir, 'socket_client.dll')}\n"
                f"  - {os.path.join(script_dir, 'cpp_socket', 'socket_client.dll')}"
            )

        try:
            self._dll = ctypes.CDLL(dll_path)
        except OSError as e:
            raise RuntimeError(f"Failed to load DLL: {e}")

        # Setup function signatures
        self._setup_functions()

    def _setup_functions(self):
        """Setup ctypes function signatures for type safety"""

        # int socket_init(void)
        self._dll.socket_init.argtypes = []
        self._dll.socket_init.restype = ctypes.c_int

        # void socket_cleanup(void)
        self._dll.socket_cleanup.argtypes = []
        self._dll.socket_cleanup.restype = None

        # int socket_connect(const char* host, int port)
        self._dll.socket_connect.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self._dll.socket_connect.restype = ctypes.c_int

        # void socket_disconnect(void)
        self._dll.socket_disconnect.argtypes = []
        self._dll.socket_disconnect.restype = None

        # int socket_is_connected(void)
        self._dll.socket_is_connected.argtypes = []
        self._dll.socket_is_connected.restype = ctypes.c_int

        # int socket_send_raw(const char* data, int length)
        self._dll.socket_send_raw.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self._dll.socket_send_raw.restype = ctypes.c_int

        # int socket_recv_raw(char* buffer, int max_length)
        self._dll.socket_recv_raw.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self._dll.socket_recv_raw.restype = ctypes.c_int

        # int socket_send_message(const char* json_data)
        self._dll.socket_send_message.argtypes = [ctypes.c_char_p]
        self._dll.socket_send_message.restype = ctypes.c_int

        # int socket_recv_message(char* buffer, int max_length)
        self._dll.socket_recv_message.argtypes = [ctypes.c_char_p, ctypes.c_int]
        self._dll.socket_recv_message.restype = ctypes.c_int

        # const char* socket_get_error(void)
        self._dll.socket_get_error.argtypes = []
        self._dll.socket_get_error.restype = ctypes.c_char_p

        # int socket_set_nonblocking(int non_blocking)
        self._dll.socket_set_nonblocking.argtypes = [ctypes.c_int]
        self._dll.socket_set_nonblocking.restype = ctypes.c_int

    def init(self) -> bool:
        """Initialize Winsock library"""
        if self._initialized:
            return True
        result = self._dll.socket_init()
        if result == 0:
            self._initialized = True
            return True
        return False

    def cleanup(self):
        """Cleanup Winsock library"""
        if self._initialized:
            self._dll.socket_cleanup()
            self._initialized = False

    def connect(self, host: str, port: int) -> bool:
        """Connect to server"""
        if not self._initialized:
            if not self.init():
                return False

        host_bytes = host.encode('utf-8')
        result = self._dll.socket_connect(host_bytes, port)
        return result == 0

    def disconnect(self):
        """Disconnect from server"""
        self._dll.socket_disconnect()

    def is_connected(self) -> bool:
        """Check if connected to server"""
        return self._dll.socket_is_connected() == 1

    def send_raw(self, data: bytes) -> int:
        """Send raw bytes to server"""
        return self._dll.socket_send_raw(data, len(data))

    def recv_raw(self, max_length: int = 4096) -> Optional[bytes]:
        """
        Receive raw bytes from server (non-blocking)
        Returns None on error, empty bytes if no data, bytes otherwise
        """
        buffer = ctypes.create_string_buffer(max_length)
        result = self._dll.socket_recv_raw(buffer, max_length)

        if result < 0:
            return None  # Error or disconnect
        elif result == 0:
            return b''  # No data available
        else:
            return buffer.raw[:result]

    def send_message(self, data: Dict[str, Any]) -> bool:
        """Send a JSON message with length prefix"""
        json_str = json.dumps(data, ensure_ascii=False)
        json_bytes = json_str.encode('utf-8')
        result = self._dll.socket_send_message(json_bytes)
        return result == 0

    def recv_message(self, max_length: int = 65536) -> Optional[Dict[str, Any]]:
        """
        Receive a complete JSON message
        Returns None if no complete message or error, dict otherwise
        """
        buffer = ctypes.create_string_buffer(max_length)
        result = self._dll.socket_recv_message(buffer, max_length)

        if result <= 0:
            if result < 0:
                return {"error": True}  # Error/disconnect marker
            return None  # No complete message yet

        try:
            json_str = buffer.value.decode('utf-8')
            return json.loads(json_str)
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            print(f"Error decoding message: {e}")
            return None

    def get_error(self) -> str:
        """Get last error message"""
        error = self._dll.socket_get_error()
        if error:
            return error.decode('utf-8')
        return ""

    def set_nonblocking(self, non_blocking: bool = True) -> bool:
        """Set socket to non-blocking mode"""
        result = self._dll.socket_set_nonblocking(1 if non_blocking else 0)
        return result == 0

    def __del__(self):
        """Cleanup on destruction"""
        try:
            self.disconnect()
            self.cleanup()
        except:
            pass


# Singleton instance for convenience
_default_client: Optional[SocketClient] = None

def get_socket_client() -> SocketClient:
    """Get or create the default socket client instance"""
    global _default_client
    if _default_client is None:
        _default_client = SocketClient()
    return _default_client


# Convenience functions using default client
def socket_init() -> bool:
    return get_socket_client().init()

def socket_cleanup():
    get_socket_client().cleanup()

def socket_connect(host: str, port: int) -> bool:
    return get_socket_client().connect(host, port)

def socket_disconnect():
    get_socket_client().disconnect()

def socket_is_connected() -> bool:
    return get_socket_client().is_connected()

def socket_send_message(data: Dict[str, Any]) -> bool:
    return get_socket_client().send_message(data)

def socket_recv_message() -> Optional[Dict[str, Any]]:
    return get_socket_client().recv_message()

def socket_get_error() -> str:
    return get_socket_client().get_error()


if __name__ == "__main__":
    # Simple test
    print("Testing socket_wrapper.py")

    try:
        client = SocketClient()
        print("DLL loaded successfully!")

        if client.init():
            print("Winsock initialized!")
        else:
            print(f"Init failed: {client.get_error()}")

        # Try connecting to localhost
        print("Attempting to connect to localhost:5000...")
        if client.connect("127.0.0.1", 5000):
            print("Connected!")
            client.disconnect()
            print("Disconnected!")
        else:
            print(f"Connect failed (expected if no server): {client.get_error()}")

        client.cleanup()
        print("Cleanup complete!")

    except Exception as e:
        print(f"Error: {e}")
