"""
chat_client.py - TCP Chat Client with Tkinter GUI

Uses C++ Winsock DLL for socket operations (via ctypes).
GUI is implemented in Python with Tkinter.
"""

import threading
import json
import time
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from datetime import datetime
from protocol import (
    Message, MessageType, serialize_to_dict, deserialize_from_dict,
    create_login_message, create_register_message, create_logout_message,
    create_global_message, create_private_message, create_ping_message
)

# Import C++ socket wrapper
try:
    from socket_wrapper import SocketClient
    USE_CPP_SOCKET = True
except ImportError as e:
    print(f"Warning: Could not load C++ socket DLL: {e}")
    print("Falling back to Python socket implementation")
    USE_CPP_SOCKET = False
    import socket


class ChatClient:
    """TCP Chat Client - Uses C++ Winsock DLL for socket operations"""

    def __init__(self):
        self.cpp_socket = None  # C++ socket wrapper
        self.py_socket = None   # Fallback Python socket
        self.connected = False
        self.authenticated = False
        self.username = ""
        self.display_name = ""
        self.is_admin = False
        self.is_muted = False
        self.receive_thread = None
        self.ping_timer = None
        self.callbacks = {}
        self.running = False

        # For Python fallback mode
        from protocol import MessageBuffer
        self.buffer = MessageBuffer()

    def connect(self, host: str, port: int) -> bool:
        try:
            if USE_CPP_SOCKET:
                # Use C++ Winsock DLL
                self.cpp_socket = SocketClient()
                if not self.cpp_socket.connect(host, port):
                    error = self.cpp_socket.get_error()
                    self._trigger_callback('error', f"Connection failed: {error}")
                    return False
                self.connected = True
            else:
                # Fallback to Python socket
                self.py_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.py_socket.connect((host, port))
                self.connected = True

            # Start receive thread
            self.running = True
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()

            # Start ping timer
            self._start_ping_timer()

            return True
        except Exception as e:
            self._trigger_callback('error', str(e))
            return False

    def disconnect(self):
        self.running = False
        self.connected = False
        self.authenticated = False

        if self.ping_timer:
            self.ping_timer.cancel()
            self.ping_timer = None

        if USE_CPP_SOCKET and self.cpp_socket:
            self.cpp_socket.disconnect()
            self.cpp_socket = None
        elif self.py_socket:
            try:
                self.py_socket.close()
            except:
                pass
            self.py_socket = None

        self._trigger_callback('disconnected')

    def login(self, username: str, password: str):
        msg = create_login_message(username, password)
        self._send_message(msg)

    def register(self, username: str, password: str):
        msg = create_register_message(username, password)
        self._send_message(msg)

    def logout(self):
        msg = create_logout_message()
        self._send_message(msg)
        self.authenticated = False
        self.username = ""

    def send_global_message(self, content: str):
        if not self.authenticated or not content:
            return
        msg = create_global_message(self.username, content)
        self._send_message(msg)

    def send_private_message(self, receiver: str, content: str):
        if not self.authenticated or not content or not receiver:
            return
        msg = create_private_message(self.username, receiver, content)
        self._send_message(msg)

    def set_callback(self, event: str, callback):
        self.callbacks[event] = callback

    def _trigger_callback(self, event: str, *args):
        if event in self.callbacks:
            self.callbacks[event](*args)

    def _send_message(self, msg: Message):
        if not self.connected:
            return

        try:
            from protocol import socket_log
            if USE_CPP_SOCKET and self.cpp_socket:
                # Use C++ DLL - send as JSON dict
                msg_dict = serialize_to_dict(msg)
                socket_log("[SOCKET]", f"Sending via C++ DLL: {msg.type.name}")
                if not self.cpp_socket.send_message(msg_dict):
                    error = self.cpp_socket.get_error()
                    self._trigger_callback('error', f"Send failed: {error}")
                    self.disconnect()
            elif self.py_socket:
                # Fallback to Python socket
                from protocol import serialize
                data = serialize(msg)
                socket_log("[SOCKET-SEND]", f"Sending {len(data)} bytes")
                self.py_socket.sendall(data)
                socket_log("[SOCKET]", "Successfully sent")
        except Exception as e:
            from protocol import socket_log
            socket_log("[SOCKET]", f"ERROR: {e}")
            self._trigger_callback('error', str(e))
            self.disconnect()

    def _receive_loop(self):
        """Receive messages from server"""
        if USE_CPP_SOCKET:
            self._receive_loop_cpp()
        else:
            self._receive_loop_python()

    def _receive_loop_cpp(self):
        """Receive loop using C++ DLL"""
        while self.running and self.connected:
            try:
                if not self.cpp_socket or not self.cpp_socket.is_connected():
                    break

                # Non-blocking receive from C++ DLL
                result = self.cpp_socket.recv_message()

                if result is None:
                    # No complete message yet, sleep briefly to avoid busy loop
                    time.sleep(0.01)
                    continue
                elif isinstance(result, dict) and result.get("error"):
                    # Error or disconnect
                    break
                else:
                    # Got a message
                    msg = deserialize_from_dict(result)
                    if msg:
                        self._handle_message(msg)

            except Exception as e:
                if self.running:
                    self._trigger_callback('error', str(e))
                break

        self.disconnect()

    def _receive_loop_python(self):
        """Receive loop using Python socket (fallback)"""
        from protocol import serialize, socket_log
        while self.running and self.connected:
            try:
                data = self.py_socket.recv(4096)
                if not data:
                    break
                    
                socket_log("[SOCKET-RECV]", f"Received {len(data)} bytes")
                self.buffer.append(data)

                while self.buffer.has_complete_message():
                    msg = self.buffer.extract_message()
                    if msg:
                        self._handle_message(msg)
            except Exception as e:
                if self.running:
                    socket_log("[SOCKET]", f"ERROR: {e}")
                    self._trigger_callback('error', str(e))
                break

        self.disconnect()

    def _handle_message(self, msg: Message):
        if msg.type == MessageType.OK:
            self._handle_ok(msg)
        elif msg.type == MessageType.ERROR:
            self._trigger_callback('error', msg.content)
        elif msg.type == MessageType.MSG_GLOBAL:
            self._trigger_callback('global_message', msg.sender, msg.content, msg.timestamp)
        elif msg.type == MessageType.MSG_PRIVATE:
            self._trigger_callback('private_message', msg.sender, msg.receiver, msg.content, msg.timestamp)
        elif msg.type == MessageType.ONLINE_LIST:
            try:
                users = json.loads(msg.extra) if msg.extra else []
                self._trigger_callback('online_list', users)
            except:
                pass
        elif msg.type == MessageType.USER_STATUS:
            try:
                data = json.loads(msg.extra) if msg.extra else {}
                self._trigger_callback('user_status', msg.sender, data.get('status', 'offline'))
            except:
                pass
        elif msg.type == MessageType.PONG:
            pass  # Heartbeat response
        elif msg.type == MessageType.KICKED:
            self._trigger_callback('kicked', msg.content)
            self.disconnect()
        elif msg.type == MessageType.BANNED:
            self._trigger_callback('banned', msg.content)
            self.disconnect()
        elif msg.type == MessageType.MUTED:
            self.is_muted = True
            self._trigger_callback('muted', msg.content)
        elif msg.type == MessageType.UNMUTED:
            self.is_muted = False
            self._trigger_callback('unmuted', msg.content)

    def _handle_ok(self, msg: Message):
        # Check if this is a login response
        if msg.extra:
            try:
                data = json.loads(msg.extra)
                if 'username' in data:
                    self.authenticated = True
                    self.username = data.get('username', '')
                    self.display_name = data.get('displayName', self.username)
                    self.is_admin = data.get('role', 0) == 1
                    self.is_muted = data.get('isMuted', False)
                    self._trigger_callback('login_success', self.username, self.display_name, self.is_admin)
                    return
            except:
                pass
        self._trigger_callback('ok', msg.content)

    def _start_ping_timer(self):
        if not self.connected:
            return
        self._send_message(create_ping_message())
        self.ping_timer = threading.Timer(30.0, self._start_ping_timer)
        self.ping_timer.daemon = True
        self.ping_timer.start()


class LoginDialog:
    """Login/Register Dialog"""

    def __init__(self, parent, on_login, on_register):
        self.dialog = tk.Toplevel(parent)
        self.dialog.title("Login")
        self.dialog.geometry("350x200")
        self.dialog.resizable(False, False)
        self.dialog.transient(parent)
        self.dialog.grab_set()

        self.on_login = on_login
        self.on_register = on_register

        self._setup_ui()
        self.dialog.protocol("WM_DELETE_WINDOW", self._on_close)

    def _setup_ui(self):
        frame = ttk.Frame(self.dialog, padding=20)
        frame.pack(fill=tk.BOTH, expand=True)

        # Username
        ttk.Label(frame, text="Username:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.username_entry = ttk.Entry(frame, width=30)
        self.username_entry.grid(row=0, column=1, pady=5)

        # Password
        ttk.Label(frame, text="Password:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.password_entry = ttk.Entry(frame, width=30, show="*")
        self.password_entry.grid(row=1, column=1, pady=5)

        # Buttons
        btn_frame = ttk.Frame(frame)
        btn_frame.grid(row=2, column=0, columnspan=2, pady=20)

        ttk.Button(btn_frame, text="Login", command=self._login, width=12).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Register", command=self._register, width=12).pack(side=tk.LEFT, padx=5)

        self.username_entry.focus()
        self.dialog.bind('<Return>', lambda e: self._login())

    def _login(self):
        username = self.username_entry.get().strip()
        password = self.password_entry.get()
        if username and password:
            self.on_login(username, password)

    def _register(self):
        username = self.username_entry.get().strip()
        password = self.password_entry.get()
        if username and password:
            self.on_register(username, password)

    def _on_close(self):
        self.dialog.destroy()

    def close(self):
        self.dialog.destroy()


class ChatWindow:
    """Main Chat Window"""

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Chat Client")
        self.root.geometry("900x600")

        self.client = ChatClient()
        self.login_dialog = None
        self.private_chats = {}  # username -> text widget

        self._setup_ui()
        self._setup_callbacks()

    def _setup_ui(self):
        # Menu
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)

        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="Connect", command=self._show_connect_dialog)
        file_menu.add_command(label="Disconnect", command=self._disconnect)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)

        # Main container
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Left panel - Chat
        left_frame = ttk.Frame(main_frame)
        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Chat tabs
        self.chat_notebook = ttk.Notebook(left_frame)
        self.chat_notebook.pack(fill=tk.BOTH, expand=True)

        # Global chat tab
        global_frame = ttk.Frame(self.chat_notebook)
        self.chat_notebook.add(global_frame, text="Global Chat")

        self.global_chat = tk.Text(global_frame, state=tk.DISABLED, wrap=tk.WORD)
        scrollbar = ttk.Scrollbar(global_frame, command=self.global_chat.yview)
        self.global_chat.config(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.global_chat.pack(fill=tk.BOTH, expand=True)
        
        # Configure text tags for formatting
        self.global_chat.tag_config('timestamp', foreground='gray')
        self.global_chat.tag_config('username', foreground='#0066CC', font=('TkDefaultFont', 9, 'bold'))
        self.global_chat.tag_config('server', foreground='#CC0000', font=('TkDefaultFont', 9, 'bold'))

        # Message input
        input_frame = ttk.Frame(left_frame)
        input_frame.pack(fill=tk.X, pady=5)

        self.message_entry = ttk.Entry(input_frame)
        self.message_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.message_entry.bind('<Return>', lambda e: self._send_message())

        self.send_button = ttk.Button(input_frame, text="Send", command=self._send_message)
        self.send_button.pack(side=tk.RIGHT, padx=5)

        # Right panel - Online users
        right_frame = ttk.LabelFrame(main_frame, text="Online Users", width=200)
        right_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
        right_frame.pack_propagate(False)

        # Instruction label
        instruction_label = ttk.Label(right_frame, text="Double-click to chat privately", 
                                      font=('', 8), foreground='gray')
        instruction_label.pack(pady=(5, 0))

        self.users_listbox = tk.Listbox(right_frame)
        self.users_listbox.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.users_listbox.bind('<Double-1>', self._on_user_double_click)
        
        # Private chat button
        self.private_chat_button = ttk.Button(right_frame, text="Private Chat", 
                                             command=self._open_private_chat_from_selection,
                                             state=tk.DISABLED)
        self.private_chat_button.pack(fill=tk.X, padx=5, pady=(0, 5))
        
        # Bind selection event to enable/disable button
        self.users_listbox.bind('<<ListboxSelect>>', self._on_user_select)

        # Status bar
        self.status_var = tk.StringVar(value="Disconnected")
        status_bar = ttk.Label(self.root, textvariable=self.status_var, relief=tk.SUNKEN)
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)

    def _setup_callbacks(self):
        self.client.set_callback('login_success', self._on_login_success)
        self.client.set_callback('error', self._on_error)
        self.client.set_callback('ok', self._on_ok)
        self.client.set_callback('disconnected', self._on_disconnected)
        self.client.set_callback('global_message', self._on_global_message)
        self.client.set_callback('private_message', self._on_private_message)
        self.client.set_callback('online_list', self._on_online_list)
        self.client.set_callback('user_status', self._on_user_status)
        self.client.set_callback('kicked', self._on_kicked)
        self.client.set_callback('banned', self._on_banned)
        self.client.set_callback('muted', self._on_muted)
        self.client.set_callback('unmuted', self._on_unmuted)

    def _show_connect_dialog(self):
        dialog = tk.Toplevel(self.root)
        dialog.title("Connect to Server")
        dialog.geometry("300x150")
        dialog.transient(self.root)
        dialog.grab_set()

        frame = ttk.Frame(dialog, padding=20)
        frame.pack(fill=tk.BOTH, expand=True)

        ttk.Label(frame, text="Host:").grid(row=0, column=0, sticky=tk.W, pady=5)
        host_entry = ttk.Entry(frame, width=25)
        host_entry.insert(0, "127.0.0.1")
        host_entry.grid(row=0, column=1, pady=5)

        ttk.Label(frame, text="Port:").grid(row=1, column=0, sticky=tk.W, pady=5)
        port_entry = ttk.Entry(frame, width=25)
        port_entry.insert(0, "9000")
        port_entry.grid(row=1, column=1, pady=5)

        def connect():
            host = host_entry.get().strip()
            try:
                port = int(port_entry.get().strip())
            except:
                messagebox.showerror("Error", "Invalid port number")
                return

            dialog.destroy()
            self._connect(host, port)

        ttk.Button(frame, text="Connect", command=connect).grid(row=2, column=0, columnspan=2, pady=15)

    def _connect(self, host: str, port: int):
        self.status_var.set(f"Connecting to {host}:{port}...")
        self.root.update()

        if self.client.connect(host, port):
            self.status_var.set(f"Connected to {host}:{port}")
            self._show_login_dialog()
        else:
            self.status_var.set("Connection failed")

    def _show_login_dialog(self):
        self.login_dialog = LoginDialog(
            self.root,
            on_login=self._do_login,
            on_register=self._do_register
        )

    def _do_login(self, username: str, password: str):
        self.client.login(username, password)

    def _do_register(self, username: str, password: str):
        self.client.register(username, password)

    def _disconnect(self):
        self.client.disconnect()

    def _send_message(self):
        content = self.message_entry.get().strip()
        if not content:
            return

        # Check which tab is selected
        current_tab = self.chat_notebook.index(self.chat_notebook.select())
        if current_tab == 0:
            # Global chat
            self.client.send_global_message(content)
        else:
            # Private chat
            tab_text = self.chat_notebook.tab(current_tab, "text")
            
            # Don't allow sending to [SERVER] tab
            if tab_text == "[SERVER]":
                messagebox.showinfo("Info", "Cannot send messages to server. Use Global Chat to broadcast.")
                return
            
            self.client.send_private_message(tab_text, content)

        self.message_entry.delete(0, tk.END)

    def _on_user_double_click(self, event):
        selection = self.users_listbox.curselection()
        if selection:
            username = self.users_listbox.get(selection[0])
            if username != self.client.username:
                self._open_private_chat(username)
    
    def _on_user_select(self, event):
        """Enable/disable Private Chat button based on selection"""
        selection = self.users_listbox.curselection()
        if selection:
            username = self.users_listbox.get(selection[0])
            # Enable button only if selected user is not self
            if username != self.client.username:
                self.private_chat_button.config(state=tk.NORMAL)
            else:
                self.private_chat_button.config(state=tk.DISABLED)
        else:
            self.private_chat_button.config(state=tk.DISABLED)
    
    def _open_private_chat_from_selection(self):
        """Open private chat with selected user from button click"""
        selection = self.users_listbox.curselection()
        if selection:
            username = self.users_listbox.get(selection[0])
            if username != self.client.username:
                self._open_private_chat(username)

    def _open_private_chat(self, username: str):
        if username in self.private_chats:
            # Switch to existing tab
            for i in range(self.chat_notebook.index("end")):
                if self.chat_notebook.tab(i, "text") == username:
                    self.chat_notebook.select(i)
                    return

        # Create new tab
        frame = ttk.Frame(self.chat_notebook)
        self.chat_notebook.add(frame, text=username)

        chat_text = tk.Text(frame, state=tk.DISABLED, wrap=tk.WORD)
        scrollbar = ttk.Scrollbar(frame, command=chat_text.yview)
        chat_text.config(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        chat_text.pack(fill=tk.BOTH, expand=True)        
        # Configure text tags for formatting
        chat_text.tag_config('timestamp', foreground='gray')
        chat_text.tag_config('username', foreground='#0066CC', font=('TkDefaultFont', 9, 'bold'))
        chat_text.tag_config('server', foreground='#CC0000', font=('TkDefaultFont', 9, 'bold'))
        self.private_chats[username] = chat_text
        self.chat_notebook.select(self.chat_notebook.index("end") - 1)

    def _append_to_chat(self, text_widget, message: str = None, timestamp: str = None, username: str = None, content: str = None, is_server: bool = False):
        """Append formatted message to chat widget"""
        def update():
            text_widget.config(state=tk.NORMAL)
            
            # If simple message string provided (legacy support)
            if message is not None:
                text_widget.insert(tk.END, message + "\n")
            else:
                # Formatted message with tags
                # Insert timestamp with gray color
                if timestamp:
                    text_widget.insert(tk.END, f"[{timestamp}] ", 'timestamp')
                
                # Insert username with blue color and bold
                if username:
                    if is_server:
                        text_widget.insert(tk.END, username, 'server')
                    else:
                        text_widget.insert(tk.END, username, 'username')
                    text_widget.insert(tk.END, ": ")
                
                # Insert message content (normal text)
                if content:
                    text_widget.insert(tk.END, content)
                
                text_widget.insert(tk.END, "\n")
            
            text_widget.config(state=tk.DISABLED)
            text_widget.see(tk.END)
        self.root.after(0, update)

    # Callbacks
    def _on_login_success(self, username: str, display_name: str, is_admin: bool):
        def update():
            if self.login_dialog:
                self.login_dialog.close()
                self.login_dialog = None
            role = "Admin" if is_admin else "Member"
            self.status_var.set(f"Logged in as {display_name} ({role})")
            self.root.title(f"Chat Client - {display_name}")
        self.root.after(0, update)

    def _on_error(self, message: str):
        def update():
            messagebox.showerror("Error", message)
        self.root.after(0, update)

    def _on_ok(self, message: str):
        def update():
            if message:
                messagebox.showinfo("Success", message)
        self.root.after(0, update)

    def _on_disconnected(self):
        def update():
            self.status_var.set("Disconnected")
            self.root.title("Chat Client")
            self.users_listbox.delete(0, tk.END)
        self.root.after(0, update)

    def _on_global_message(self, sender: str, content: str, timestamp: str):
        time_str = timestamp if timestamp else datetime.now().strftime("%H:%M:%S")
        self._append_to_chat(self.global_chat, timestamp=time_str, username=sender, content=content)

    def _on_private_message(self, sender: str, receiver: str, content: str, timestamp: str):
        time_str = timestamp if timestamp else datetime.now().strftime("%H:%M:%S")
        
        # Check if message is from server
        if sender == "[SERVER]":
            # Show server messages in Global Chat with special formatting
            self._append_to_chat(self.global_chat, timestamp=time_str, username="[SERVER → You]", content=content, is_server=True)
            return
        
        # Determine chat partner
        partner = sender if sender != self.client.username else receiver

        # Open chat if not exists
        if partner not in self.private_chats:
            self._open_private_chat(partner)

        self._append_to_chat(self.private_chats[partner], timestamp=time_str, username=sender, content=content)

    def _on_online_list(self, users: list):
        def update():
            self.users_listbox.delete(0, tk.END)
            for user in sorted(users):
                self.users_listbox.insert(tk.END, user)
        self.root.after(0, update)

    def _on_user_status(self, username: str, status: str):
        def update():
            if status == "online":
                # Add user if not in list
                users = list(self.users_listbox.get(0, tk.END))
                if username not in users:
                    self.users_listbox.insert(tk.END, username)
                self._append_to_chat(self.global_chat, f"*** {username} is now online ***")
            else:
                # Remove user from list
                users = list(self.users_listbox.get(0, tk.END))
                if username in users:
                    idx = users.index(username)
                    self.users_listbox.delete(idx)
                self._append_to_chat(self.global_chat, f"*** {username} is now offline ***")
        self.root.after(0, update)

    def _on_kicked(self, message: str):
        def update():
            messagebox.showwarning("Kicked", message)
        self.root.after(0, update)

    def _on_banned(self, message: str):
        def update():
            messagebox.showwarning("Banned", message)
        self.root.after(0, update)

    def _on_muted(self, message: str):
        def update():
            messagebox.showwarning("Muted", message)
            self.status_var.set(self.status_var.get() + " [MUTED]")
        self.root.after(0, update)

    def _on_unmuted(self, message: str):
        def update():
            messagebox.showinfo("Unmuted", message)
            status = self.status_var.get().replace(" [MUTED]", "")
            self.status_var.set(status)
        self.root.after(0, update)

    def run(self):
        self.root.mainloop()


if __name__ == "__main__":
    app = ChatWindow()
    app.run()
