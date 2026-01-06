"""
chat_server.py - TCP Chat Server with Tkinter GUI
"""

import socket
import threading
import json
import tkinter as tk
from tkinter import ttk, messagebox
from datetime import datetime
from typing import Dict
from dataclasses import dataclass, field
from protocol import (
    Message, MessageType, MessageBuffer, serialize,
    create_global_message, create_private_message, set_socket_log_callback
)
from database import Database, User


@dataclass
class ClientSession:
    """Connected client session"""
    socket: socket.socket
    address: str
    username: str = ""
    display_name: str = ""
    authenticated: bool = False
    active: bool = True
    buffer: MessageBuffer = field(default_factory=MessageBuffer)
    send_lock: threading.Lock = field(default_factory=threading.Lock)


class ChatServer:
    """TCP Chat Server"""

    def __init__(self, port: int = 9000):
        self.port = port
        self.server_socket = None
        self.running = False
        self.db = Database()

        self.clients: Dict[int, ClientSession] = {}
        self.clients_lock = threading.Lock()

        self.username_to_socket: Dict[str, int] = {}
        self.users_lock = threading.Lock()

        self.log_callback = None

    def set_log_callback(self, callback):
        self.log_callback = callback
        # Also set protocol socket log callback to send to GUI
        set_socket_log_callback(callback)

    def log(self, message: str):
        timestamp = datetime.now().strftime("[%H:%M:%S]")
        log_msg = f"{timestamp} {message}"
        print(log_msg)
        if self.log_callback:
            self.log_callback(log_msg)

    def start(self) -> bool:
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(('0.0.0.0', self.port))
            self.server_socket.listen(100)
            self.running = True

            self.log(f"Server started on port {self.port}")

            # Start accept thread
            accept_thread = threading.Thread(target=self._accept_loop, daemon=True)
            accept_thread.start()

            return True
        except Exception as e:
            self.log(f"Failed to start server: {e}")
            return False

    def stop(self):
        self.running = False
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
        # Close all client connections
        with self.clients_lock:
            for session in self.clients.values():
                session.active = False
                try:
                    session.socket.close()
                except:
                    pass
            self.clients.clear()
        self.log("Server stopped")

    def _accept_loop(self):
        while self.running:
            try:
                client_socket, addr = self.server_socket.accept()
                address = f"{addr[0]}:{addr[1]}"
                self.log(f"New connection from {address}")

                fd = client_socket.fileno()
                session = ClientSession(socket=client_socket, address=address)

                with self.clients_lock:
                    self.clients[fd] = session

                # Start client handler thread
                thread = threading.Thread(target=self._handle_client, args=(fd,), daemon=True)
                thread.start()
            except Exception as e:
                if self.running:
                    self.log(f"Accept error: {e}")

    def _handle_client(self, fd: int):
        session = None
        with self.clients_lock:
            session = self.clients.get(fd)

        if not session:
            return

        try:
            while self.running and session.active:
                data = session.socket.recv(4096)
                if not data:
                    break
                
                self.log(f"[SOCKET-RECV] ← {session.address}: {len(data)} bytes")
                session.buffer.append(data)
                while session.buffer.has_complete_message():
                    msg = session.buffer.extract_message()
                    if msg:
                        self._handle_message(fd, session, msg)
        except Exception as e:
            if session.active:
                self.log(f"Error handling client {session.address}: {e}")

        # Client disconnected
        username = session.username
        self.log(f"Client disconnected: {session.address}" + (f" ({username})" if username else ""))

        if username:
            self._broadcast_user_status(username, "offline")
            with self.users_lock:
                self.username_to_socket.pop(username, None)

        with self.clients_lock:
            self.clients.pop(fd, None)

        try:
            session.socket.close()
        except:
            pass

    def _send_to_client(self, session: ClientSession, msg: Message) -> bool:
        try:
            with session.send_lock:
                data = serialize(msg)
                self.log(f"[SOCKET-SEND] → {session.address}: {len(data)} bytes")
                session.socket.sendall(data)
            return True
        except Exception as e:
            self.log(f"[SOCKET-ERROR] Failed to send to {session.address}: {e}")
            return False

    def _send_error(self, session: ClientSession, error: str):
        msg = Message(type=MessageType.ERROR, content=error)
        self._send_to_client(session, msg)

    def _send_ok(self, session: ClientSession, content: str = "", extra: str = ""):
        msg = Message(type=MessageType.OK, content=content, extra=extra)
        self._send_to_client(session, msg)

    def _broadcast(self, msg: Message, exclude_fd: int = -1):
        with self.clients_lock:
            for fd, session in self.clients.items():
                if fd != exclude_fd and session.authenticated:
                    self._send_to_client(session, msg)

    def _send_to_user(self, username: str, msg: Message) -> bool:
        with self.users_lock:
            fd = self.username_to_socket.get(username)
        if fd is None:
            return False
        with self.clients_lock:
            session = self.clients.get(fd)
        if session:
            return self._send_to_client(session, msg)
        return False

    def _broadcast_user_status(self, username: str, status: str):
        msg = Message(
            type=MessageType.USER_STATUS,
            sender=username,
            extra=json.dumps({"status": status})
        )
        self._broadcast(msg)

    def _handle_message(self, fd: int, session: ClientSession, msg: Message):
        self.log(f"[{session.address}] Received: {MessageType(msg.type).name}")

        try:
            if msg.type == MessageType.REGISTER:
                self._handle_register(session, msg)
            elif msg.type == MessageType.LOGIN:
                self._handle_login(fd, session, msg)
            elif msg.type == MessageType.LOGOUT:
                self._handle_logout(fd, session)
            elif msg.type == MessageType.MSG_GLOBAL:
                self._handle_global_message(fd, session, msg)
            elif msg.type == MessageType.MSG_PRIVATE:
                self._handle_private_message(session, msg)
            elif msg.type == MessageType.PING:
                self._send_to_client(session, Message(type=MessageType.PONG))
            elif msg.type == MessageType.KICK_USER:
                self._handle_kick(session, msg)
            elif msg.type == MessageType.BAN_USER:
                self._handle_ban(session, msg)
            elif msg.type == MessageType.UNBAN_USER:
                self._handle_unban(session, msg)
            elif msg.type == MessageType.MUTE_USER:
                self._handle_mute(session, msg)
            elif msg.type == MessageType.UNMUTE_USER:
                self._handle_unmute(session, msg)
        except Exception as e:
            self.log(f"Error handling message: {e}")
            self._send_error(session, "Internal server error")

    def _handle_register(self, session: ClientSession, msg: Message):
        try:
            data = json.loads(msg.content)
            username = data.get("username", "").strip()
            password = data.get("password", "")

            if len(username) < 3 or len(username) > 20:
                self._send_error(session, "Username must be 3-20 characters")
                return
            if len(password) < 4:
                self._send_error(session, "Password must be at least 4 characters")
                return

            if self.db.register_user(username, password):
                self.log(f"User registered: {username}")
                self._send_ok(session, "Registration successful")
            else:
                self._send_error(session, "Username already exists")
        except:
            self._send_error(session, "Invalid request format")

    def _handle_login(self, fd: int, session: ClientSession, msg: Message):
        if session.authenticated:
            self._send_error(session, "Already logged in")
            return

        try:
            data = json.loads(msg.content)
            username = data.get("username", "")
            password = data.get("password", "")

            # Check if already online
            with self.users_lock:
                if username in self.username_to_socket:
                    self._send_error(session, "User already logged in from another location")
                    return

            if self.db.is_banned(username):
                self._send_error(session, "Your account has been banned")
                return

            if self.db.authenticate(username, password):
                user = self.db.get_user(username)
                session.username = username
                session.display_name = user.display_name
                session.authenticated = True

                with self.users_lock:
                    self.username_to_socket[username] = fd

                self.log(f"User logged in: {username} from {session.address}")

                # Send success with user info
                extra = json.dumps({
                    "username": username,
                    "displayName": user.display_name,
                    "role": user.role,
                    "isMuted": user.is_muted
                })
                self._send_ok(session, "Login successful", extra)

                # Broadcast user online
                self._broadcast_user_status(username, "online")

                # Send online list
                online_users = self.get_online_users()
                online_msg = Message(type=MessageType.ONLINE_LIST, extra=json.dumps(online_users))
                self._send_to_client(session, online_msg)
            else:
                self._send_error(session, "Invalid username or password")
        except:
            self._send_error(session, "Invalid request format")

    def _handle_logout(self, fd: int, session: ClientSession):
        if not session.authenticated:
            self._send_error(session, "Not logged in")
            return

        username = session.username
        self.log(f"User logged out: {username}")

        self._broadcast_user_status(username, "offline")

        with self.users_lock:
            self.username_to_socket.pop(username, None)

        session.username = ""
        session.display_name = ""
        session.authenticated = False

        self._send_ok(session, "Logged out successfully")

    def _handle_global_message(self, fd: int, session: ClientSession, msg: Message):
        if not session.authenticated:
            self._send_error(session, "Must be logged in to send messages")
            return

        if self.db.is_muted(session.username):
            self._send_error(session, "You are muted and cannot send messages")
            return

        content = msg.content.strip()
        if not content:
            return

        self.log(f"Global message from {session.username}: {content[:50]}...")

        broadcast_msg = create_global_message(session.username, content)
        self._broadcast(broadcast_msg)

    def _handle_private_message(self, session: ClientSession, msg: Message):
        if not session.authenticated:
            self._send_error(session, "Must be logged in to send messages")
            return

        if self.db.is_muted(session.username):
            self._send_error(session, "You are muted and cannot send messages")
            return

        receiver = msg.receiver
        content = msg.content.strip()

        if not receiver or not content:
            return

        if receiver == session.username:
            self._send_error(session, "Cannot send message to yourself")
            return

        self.log(f"Private message from {session.username} to {receiver}")

        private_msg = create_private_message(session.username, receiver, content)

        if not self._send_to_user(receiver, private_msg):
            self._send_error(session, f"User not online: {receiver}")
            return

        # Also send to sender
        self._send_to_client(session, private_msg)

    def _handle_kick(self, session: ClientSession, msg: Message):
        if not session.authenticated or not self.db.is_admin(session.username):
            self._send_error(session, "Admin privileges required")
            return

        target = msg.receiver
        if not target or target == session.username:
            self._send_error(session, "Invalid target")
            return

        # Send kick notification
        kick_msg = Message(type=MessageType.KICKED, content=f"You have been kicked by {session.username}")
        self._send_to_user(target, kick_msg)

        # Disconnect user
        self._kick_user(target)
        self.log(f"User kicked: {target} by {session.username}")
        self._send_ok(session, f"User kicked: {target}")

    def _handle_ban(self, session: ClientSession, msg: Message):
        if not session.authenticated or not self.db.is_admin(session.username):
            self._send_error(session, "Admin privileges required")
            return

        target = msg.receiver
        if not target:
            self._send_error(session, "Target user not specified")
            return

        if self.db.is_admin(target):
            self._send_error(session, "Cannot ban an admin")
            return

        if self.db.ban_user(target):
            # Kick if online
            ban_msg = Message(type=MessageType.BANNED, content=f"You have been banned by {session.username}")
            self._send_to_user(target, ban_msg)
            self._kick_user(target)
            self.log(f"User banned: {target} by {session.username}")
            self._send_ok(session, f"User banned: {target}")
        else:
            self._send_error(session, "User not found")

    def _handle_unban(self, session: ClientSession, msg: Message):
        if not session.authenticated or not self.db.is_admin(session.username):
            self._send_error(session, "Admin privileges required")
            return

        target = msg.receiver
        if self.db.unban_user(target):
            self.log(f"User unbanned: {target} by {session.username}")
            self._send_ok(session, f"User unbanned: {target}")
        else:
            self._send_error(session, "User not found")

    def _handle_mute(self, session: ClientSession, msg: Message):
        if not session.authenticated or not self.db.is_admin(session.username):
            self._send_error(session, "Admin privileges required")
            return

        target = msg.receiver
        if self.db.is_admin(target):
            self._send_error(session, "Cannot mute an admin")
            return

        if self.db.mute_user(target):
            mute_msg = Message(type=MessageType.MUTED, content=f"You have been muted by {session.username}")
            self._send_to_user(target, mute_msg)
            self.log(f"User muted: {target} by {session.username}")
            self._send_ok(session, f"User muted: {target}")
        else:
            self._send_error(session, "User not found")

    def _handle_unmute(self, session: ClientSession, msg: Message):
        if not session.authenticated or not self.db.is_admin(session.username):
            self._send_error(session, "Admin privileges required")
            return

        target = msg.receiver
        if self.db.unmute_user(target):
            unmute_msg = Message(type=MessageType.UNMUTED, content=f"You have been unmuted by {session.username}")
            self._send_to_user(target, unmute_msg)
            self.log(f"User unmuted: {target} by {session.username}")
            self._send_ok(session, f"User unmuted: {target}")
        else:
            self._send_error(session, "User not found")

    def _kick_user(self, username: str):
        with self.users_lock:
            fd = self.username_to_socket.pop(username, None)
        if fd:
            with self.clients_lock:
                session = self.clients.get(fd)
                if session:
                    session.active = False
                    session.authenticated = False
            self._broadcast_user_status(username, "offline")

    def get_online_users(self) -> list:
        with self.users_lock:
            return list(self.username_to_socket.keys())

    def get_connected_clients(self) -> list:
        result = []
        with self.clients_lock:
            for fd, session in self.clients.items():
                result.append({
                    "username": session.username or "(not logged in)",
                    "display_name": session.display_name or "-",
                    "address": session.address,
                    "authenticated": session.authenticated,
                    "role": self.db.get_user(session.username).role if session.username and self.db.get_user(session.username) else 0
                })
        return result
    
    def get_all_users_with_status(self) -> list:
        """Get all users from database with their connection status"""
        result = []
        online_users = self.get_online_users()
        
        all_users = self.db.get_all_users()
        for user in all_users:
            status = "Connected" if user.username in online_users else "Offline"
            result.append({
                "username": user.username,
                "display_name": user.display_name,
                "role": user.role,
                "status": status,
                "is_banned": user.is_banned,
                "is_muted": user.is_muted
            })
        return result

    def broadcast_server_message(self, content: str):
        msg = create_global_message("[SERVER]", content)
        self._broadcast(msg)
        self.log(f"Server broadcast: {content}")

    def send_to_user_from_server(self, username: str, content: str) -> bool:
        msg = create_private_message("[SERVER]", username, content)
        result = self._send_to_user(username, msg)
        if result:
            self.log(f"Sent private message to {username}: {content}")
        else:
            self.log(f"Failed to send private message to {username} (not connected)")
        return result


class ServerWindow:
    """Server GUI Window"""

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Chat Server")
        self.root.geometry("800x600")

        self.server = None
        self.refresh_timer = None
        self.selected_username = None  # Track selected client

        self._setup_ui()

    def _setup_ui(self):
        # Control frame
        control_frame = ttk.LabelFrame(self.root, text="Server Control", padding=10)
        control_frame.pack(fill=tk.X, padx=10, pady=5)

        ttk.Label(control_frame, text="Port:").pack(side=tk.LEFT)
        self.port_var = tk.StringVar(value="9000")
        self.port_entry = ttk.Entry(control_frame, textvariable=self.port_var, width=10)
        self.port_entry.pack(side=tk.LEFT, padx=5)

        self.start_button = ttk.Button(control_frame, text="Start Server", command=self._toggle_server)
        self.start_button.pack(side=tk.LEFT, padx=10)

        self.status_label = ttk.Label(control_frame, text="Status: Stopped", foreground="red")
        self.status_label.pack(side=tk.RIGHT)

        # Main content with PanedWindow
        paned = ttk.PanedWindow(self.root, orient=tk.VERTICAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # Clients frame
        clients_frame = ttk.LabelFrame(paned, text="Clients", padding=5)
        paned.add(clients_frame, weight=1)

        # Client table
        columns = ("Username", "Display Name", "Role", "Status")
        self.client_tree = ttk.Treeview(clients_frame, columns=columns, show="headings", height=8)
        self.client_tree.heading("Username", text="Username")
        self.client_tree.heading("Display Name", text="Display Name")
        self.client_tree.heading("Role", text="Role")
        self.client_tree.heading("Status", text="Status")
        self.client_tree.column("Username", width=150)
        self.client_tree.column("Display Name", width=150)
        self.client_tree.column("Role", width=100)
        self.client_tree.column("Status", width=100)

        scrollbar = ttk.Scrollbar(clients_frame, orient=tk.VERTICAL, command=self.client_tree.yview)
        self.client_tree.configure(yscrollcommand=scrollbar.set)
        self.client_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Configure tags for status styling
        self.client_tree.tag_configure('connected', foreground='green')

        self.client_tree.bind('<<TreeviewSelect>>', self._on_client_select)

        ttk.Button(clients_frame, text="Refresh", command=self._refresh_clients).pack(side=tk.BOTTOM, pady=5)

        # Log frame
        log_frame = ttk.LabelFrame(paned, text="Server Log", padding=5)
        paned.add(log_frame, weight=1)

        self.log_text = tk.Text(log_frame, state=tk.DISABLED, height=10, wrap=tk.WORD)
        log_scroll = ttk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.config(yscrollcommand=log_scroll.set)
        log_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.log_text.pack(fill=tk.BOTH, expand=True)

        # Messaging frame
        msg_frame = ttk.LabelFrame(self.root, text="Messaging", padding=10)
        msg_frame.pack(fill=tk.X, padx=10, pady=5)

        # Broadcast row
        broadcast_row = ttk.Frame(msg_frame)
        broadcast_row.pack(fill=tk.X, pady=2)
        ttk.Label(broadcast_row, text="Broadcast:").pack(side=tk.LEFT)
        self.broadcast_entry = ttk.Entry(broadcast_row)
        self.broadcast_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.broadcast_button = ttk.Button(broadcast_row, text="Send to All", command=self._broadcast, state=tk.DISABLED)
        self.broadcast_button.pack(side=tk.RIGHT)

        # Private message row
        private_row = ttk.Frame(msg_frame)
        private_row.pack(fill=tk.X, pady=2)
        self.selected_label = ttk.Label(private_row, text="Selected: (none)", width=20)
        self.selected_label.pack(side=tk.LEFT)
        self.private_entry = ttk.Entry(private_row, state=tk.DISABLED)
        self.private_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.private_button = ttk.Button(private_row, text="Send to Selected", command=self._send_private, state=tk.DISABLED)
        self.private_button.pack(side=tk.RIGHT)

        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _toggle_server(self):
        if self.server and self.server.running:
            self._stop_server()
        else:
            self._start_server()

    def _start_server(self):
        try:
            port = int(self.port_var.get())
        except:
            messagebox.showerror("Error", "Invalid port number")
            return

        self.server = ChatServer(port)
        self.server.set_log_callback(self._log)

        if self.server.start():
            self.status_label.config(text="Status: Running", foreground="green")
            self.start_button.config(text="Stop Server")
            self.port_entry.config(state=tk.DISABLED)
            self.broadcast_button.config(state=tk.NORMAL)
            self._start_refresh_timer()
        else:
            messagebox.showerror("Error", "Failed to start server")

    def _stop_server(self):
        if self.refresh_timer:
            self.refresh_timer.cancel()
        if self.server:
            self.server.stop()
            self.server = None
        self.status_label.config(text="Status: Stopped", foreground="red")
        self.start_button.config(text="Start Server")
        self.port_entry.config(state=tk.NORMAL)
        self.broadcast_button.config(state=tk.DISABLED)
        self.private_button.config(state=tk.DISABLED)
        self.private_entry.config(state=tk.DISABLED)
        self.client_tree.delete(*self.client_tree.get_children())

    def _start_refresh_timer(self):
        self._refresh_clients()
        if self.server and self.server.running:
            self.refresh_timer = threading.Timer(2.0, self._start_refresh_timer)
            self.refresh_timer.daemon = True
            self.refresh_timer.start()

    def _refresh_clients(self):
        if not self.server:
            return

        # Temporarily unbind selection event to prevent triggering during refresh
        self.client_tree.unbind('<<TreeviewSelect>>')
        
        # Clear tree
        for item in self.client_tree.get_children():
            self.client_tree.delete(item)

        # Add all users from database
        users = self.server.get_all_users_with_status()
        selected_item = None
        
        for user in users:
            role = "Admin" if user["role"] == 1 else "Member"
            
            # Add tag for styling based on status
            tags = ()
            if user["status"] == "Connected":
                tags = ('connected',)
            
            item_id = self.client_tree.insert("", tk.END, values=(
                user["username"],
                user["display_name"],
                role,
                user["status"]
            ), tags=tags)
            
            # Track item to restore selection
            if self.selected_username == user["username"]:
                selected_item = item_id
        
        # Restore selection if user was previously selected
        if selected_item:
            self.client_tree.selection_set(selected_item)
            self.client_tree.see(selected_item)
            # Manually update UI state (since event is unbound)
            item = self.client_tree.item(selected_item)
            status = item["values"][3]
            self.selected_label.config(text=f"Selected: {self.selected_username}")
            if status == "Connected" and self.server and self.server.running:
                self.private_entry.config(state=tk.NORMAL)
                self.private_button.config(state=tk.NORMAL)
            else:
                self.private_entry.config(state=tk.DISABLED)
                self.private_button.config(state=tk.DISABLED)
                if status == "Offline":
                    self.selected_label.config(text=f"Selected: {self.selected_username} (Offline)")
        
        # Re-bind selection event
        self.client_tree.bind('<<TreeviewSelect>>', self._on_client_select)

    def _on_client_select(self, event):
        selection = self.client_tree.selection()
        if selection:
            item = self.client_tree.item(selection[0])
            username = item["values"][0]
            status = item["values"][3]  # Status column
            
            # Save selected username
            self.selected_username = username
            
            self.selected_label.config(text=f"Selected: {username}")

            # Only enable private message for Connected users
            if status == "Connected" and self.server and self.server.running:
                self.private_entry.config(state=tk.NORMAL)
                self.private_button.config(state=tk.NORMAL)
            else:
                self.private_entry.config(state=tk.DISABLED)
                self.private_button.config(state=tk.DISABLED)
                if status == "Offline":
                    self.selected_label.config(text=f"Selected: {username} (Offline)")
        else:
            self.selected_username = None
            self.selected_label.config(text="Selected: (none)")
            self.private_entry.config(state=tk.DISABLED)
            self.private_button.config(state=tk.DISABLED)

    def _broadcast(self):
        if not self.server:
            return
        content = self.broadcast_entry.get().strip()
        if content:
            self.server.broadcast_server_message(content)
            self.broadcast_entry.delete(0, tk.END)

    def _send_private(self):
        if not self.server:
            return
        selection = self.client_tree.selection()
        if not selection:
            return
        
        item = self.client_tree.item(selection[0])
        username = item["values"][0]
        status = item["values"][3]
        
        content = self.private_entry.get().strip()
        if not content:
            return
            
        if status != "Connected":
            messagebox.showwarning("Cannot Send", f"{username} is not connected")
            return
        
        if self.server.send_to_user_from_server(username, content):
            self._log(f"[PRIVATE → {username}] Server: {content}")
            self.private_entry.delete(0, tk.END)
        else:
            messagebox.showerror("Send Failed", f"Failed to send message to {username}")

    def _log(self, message: str):
        def update():
            self.log_text.config(state=tk.NORMAL)
            self.log_text.insert(tk.END, message + "\n")
            self.log_text.config(state=tk.DISABLED)
            self.log_text.see(tk.END)
        self.root.after(0, update)

    def _on_close(self):
        self._stop_server()
        self.root.destroy()

    def run(self):
        self.root.mainloop()


if __name__ == "__main__":
    app = ServerWindow()
    app.run()
