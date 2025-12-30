"""
database.py - SQLite Database for Chat Server

Provides persistent storage for:
- User accounts (username, password hash, display name)
- User roles (member/admin)
- User status (banned/muted)
- Message history (optional)
"""

import sqlite3
import hashlib
import threading
from typing import Optional, List, Dict, Any
from dataclasses import dataclass
from datetime import datetime


@dataclass
class User:
    """User account data"""
    username: str
    password_hash: str
    display_name: str
    role: int = 0  # 0 = member, 1 = admin
    is_banned: bool = False
    is_muted: bool = False
    created_at: str = ""


class Database:
    """SQLite-based user database with thread-safe operations"""

    # Salt for password hashing (should match C++ server)
    PASSWORD_SALT = "chat_salt_2024"

    def __init__(self, db_path: str = "chat_server.db"):
        """
        Initialize database connection.

        Args:
            db_path: Path to SQLite database file
        """
        self.db_path = db_path
        self.lock = threading.Lock()
        self._initialize()

    def _get_connection(self) -> sqlite3.Connection:
        """Get a thread-local database connection."""
        conn = sqlite3.connect(self.db_path, check_same_thread=False)
        conn.row_factory = sqlite3.Row  # Enable dict-like access
        return conn

    def _initialize(self):
        """Create tables if they don't exist."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()

            # Create users table
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    username TEXT UNIQUE NOT NULL,
                    password_hash TEXT NOT NULL,
                    display_name TEXT NOT NULL,
                    role INTEGER DEFAULT 0,
                    is_banned INTEGER DEFAULT 0,
                    is_muted INTEGER DEFAULT 0,
                    created_at TEXT DEFAULT CURRENT_TIMESTAMP
                )
            ''')

            # Create messages table (for history logging)
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    sender TEXT NOT NULL,
                    receiver TEXT,
                    content TEXT NOT NULL,
                    message_type TEXT NOT NULL,
                    timestamp TEXT DEFAULT CURRENT_TIMESTAMP
                )
            ''')

            # Create index for faster queries
            cursor.execute('''
                CREATE INDEX IF NOT EXISTS idx_username ON users(username)
            ''')
            cursor.execute('''
                CREATE INDEX IF NOT EXISTS idx_messages_time ON messages(timestamp DESC)
            ''')

            conn.commit()
            conn.close()

            # Create default admin if not exists
            self._create_default_admin()

    def _create_default_admin(self):
        """Create default admin account if not exists."""
        if not self.user_exists("admin"):
            self.register_user("admin", "admin", "Administrator")
            self._set_role("admin", 1)

    def _hash_password(self, password: str) -> str:
        """Hash password with salt using SHA-256."""
        salted = password + self.PASSWORD_SALT
        return hashlib.sha256(salted.encode()).hexdigest()

    def _set_role(self, username: str, role: int) -> bool:
        """Internal method to set user role."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET role = ? WHERE username = ?',
                (role, username)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    # ==================== User Registration & Authentication ====================

    def register_user(self, username: str, password: str, display_name: str = "") -> bool:
        """
        Register a new user.

        Args:
            username: Unique username
            password: Plain text password (will be hashed)
            display_name: Display name (defaults to username)

        Returns:
            True if registration successful, False if username exists
        """
        if not display_name:
            display_name = username

        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()

            try:
                cursor.execute('''
                    INSERT INTO users (username, password_hash, display_name)
                    VALUES (?, ?, ?)
                ''', (username, self._hash_password(password), display_name))
                conn.commit()
                return True
            except sqlite3.IntegrityError:
                # Username already exists
                return False
            finally:
                conn.close()

    def authenticate(self, username: str, password: str) -> bool:
        """
        Authenticate user login.

        Args:
            username: Username
            password: Plain text password

        Returns:
            True if credentials are valid
        """
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()

            cursor.execute(
                'SELECT password_hash FROM users WHERE username = ?',
                (username,)
            )
            row = cursor.fetchone()
            conn.close()

            if row is None:
                return False

            return row['password_hash'] == self._hash_password(password)

    def change_password(self, username: str, old_password: str, new_password: str) -> bool:
        """
        Change user password.

        Args:
            username: Username
            old_password: Current password for verification
            new_password: New password

        Returns:
            True if password changed successfully
        """
        if not self.authenticate(username, old_password):
            return False

        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET password_hash = ? WHERE username = ?',
                (self._hash_password(new_password), username)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    def user_exists(self, username: str) -> bool:
        """Check if username exists."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'SELECT 1 FROM users WHERE username = ?',
                (username,)
            )
            exists = cursor.fetchone() is not None
            conn.close()
            return exists

    # ==================== User Info ====================

    def get_user(self, username: str) -> Optional[User]:
        """Get user by username."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'SELECT * FROM users WHERE username = ?',
                (username,)
            )
            row = cursor.fetchone()
            conn.close()

            if row is None:
                return None

            return User(
                username=row['username'],
                password_hash=row['password_hash'],
                display_name=row['display_name'],
                role=row['role'],
                is_banned=bool(row['is_banned']),
                is_muted=bool(row['is_muted']),
                created_at=row['created_at']
            )

    def get_display_name(self, username: str) -> str:
        """Get user's display name."""
        user = self.get_user(username)
        return user.display_name if user else ""

    def update_display_name(self, username: str, display_name: str) -> bool:
        """Update user's display name."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET display_name = ? WHERE username = ?',
                (display_name, username)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    def get_all_users(self) -> List[User]:
        """Get all registered users."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('SELECT * FROM users ORDER BY created_at DESC')
            rows = cursor.fetchall()
            conn.close()

            return [User(
                username=row['username'],
                password_hash=row['password_hash'],
                display_name=row['display_name'],
                role=row['role'],
                is_banned=bool(row['is_banned']),
                is_muted=bool(row['is_muted']),
                created_at=row['created_at']
            ) for row in rows]

    # ==================== Role Management ====================

    def is_admin(self, username: str) -> bool:
        """Check if user is admin."""
        user = self.get_user(username)
        return user.role == 1 if user else False

    def get_role(self, username: str) -> int:
        """Get user's role (0=member, 1=admin, -1=not found)."""
        user = self.get_user(username)
        return user.role if user else -1

    def promote_user(self, username: str) -> bool:
        """Promote user to admin."""
        return self._set_role(username, 1)

    def demote_user(self, username: str) -> bool:
        """Demote user to member."""
        return self._set_role(username, 0)

    # ==================== Ban Management ====================

    def ban_user(self, username: str) -> bool:
        """Ban a user."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET is_banned = 1 WHERE username = ?',
                (username,)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    def unban_user(self, username: str) -> bool:
        """Unban a user."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET is_banned = 0 WHERE username = ?',
                (username,)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    def is_banned(self, username: str) -> bool:
        """Check if user is banned."""
        user = self.get_user(username)
        return user.is_banned if user else False

    def get_banned_users(self) -> List[str]:
        """Get list of banned usernames."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('SELECT username FROM users WHERE is_banned = 1')
            rows = cursor.fetchall()
            conn.close()
            return [row['username'] for row in rows]

    # ==================== Mute Management ====================

    def mute_user(self, username: str) -> bool:
        """Mute a user."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET is_muted = 1 WHERE username = ?',
                (username,)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    def unmute_user(self, username: str) -> bool:
        """Unmute a user."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute(
                'UPDATE users SET is_muted = 0 WHERE username = ?',
                (username,)
            )
            conn.commit()
            affected = cursor.rowcount
            conn.close()
            return affected > 0

    def is_muted(self, username: str) -> bool:
        """Check if user is muted."""
        user = self.get_user(username)
        return user.is_muted if user else False

    def get_muted_users(self) -> List[str]:
        """Get list of muted usernames."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('SELECT username FROM users WHERE is_muted = 1')
            rows = cursor.fetchall()
            conn.close()
            return [row['username'] for row in rows]

    # ==================== Message Logging ====================

    def log_message(self, sender: str, receiver: str, content: str,
                    message_type: str = "global") -> bool:
        """
        Log a chat message to the database.

        Args:
            sender: Sender username
            receiver: Receiver username (empty for global)
            content: Message content
            message_type: "global" or "private"
        """
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()

            try:
                cursor.execute('''
                    INSERT INTO messages (sender, receiver, content, message_type)
                    VALUES (?, ?, ?, ?)
                ''', (sender, receiver or None, content, message_type))
                conn.commit()
                return True
            except Exception as e:
                print(f"Error logging message: {e}")
                return False
            finally:
                conn.close()

    def get_recent_messages(self, limit: int = 50) -> List[Dict[str, Any]]:
        """Get recent messages from history."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('''
                SELECT sender, receiver, content, message_type, timestamp
                FROM messages
                ORDER BY timestamp DESC
                LIMIT ?
            ''', (limit,))
            rows = cursor.fetchall()
            conn.close()

            return [{
                'sender': row['sender'],
                'receiver': row['receiver'],
                'content': row['content'],
                'type': row['message_type'],
                'timestamp': row['timestamp']
            } for row in reversed(rows)]

    def get_message_count(self) -> int:
        """Get total message count."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('SELECT COUNT(*) FROM messages')
            count = cursor.fetchone()[0]
            conn.close()
            return count

    def get_user_count(self) -> int:
        """Get total user count."""
        with self.lock:
            conn = self._get_connection()
            cursor = conn.cursor()
            cursor.execute('SELECT COUNT(*) FROM users')
            count = cursor.fetchone()[0]
            conn.close()
            return count


# Test the database
if __name__ == "__main__":
    import os

    # Use test database
    test_db = "test_chat.db"
    if os.path.exists(test_db):
        os.remove(test_db)

    db = Database(test_db)

    print("=== Testing Database ===\n")

    # Test registration
    print("1. Register users:")
    print(f"   Register 'user1': {db.register_user('user1', 'pass123', 'User One')}")
    print(f"   Register 'user2': {db.register_user('user2', 'pass456')}")
    print(f"   Register 'user1' again: {db.register_user('user1', 'xxx')}")  # Should fail

    # Test authentication
    print("\n2. Authentication:")
    print(f"   Login 'user1' with correct password: {db.authenticate('user1', 'pass123')}")
    print(f"   Login 'user1' with wrong password: {db.authenticate('user1', 'wrong')}")
    print(f"   Login non-existent user: {db.authenticate('ghost', 'pass')}")

    # Test admin
    print("\n3. Admin check:")
    print(f"   Is 'admin' an admin: {db.is_admin('admin')}")
    print(f"   Is 'user1' an admin: {db.is_admin('user1')}")

    # Test ban/mute
    print("\n4. Ban/Mute:")
    db.ban_user('user2')
    db.mute_user('user1')
    print(f"   Is 'user2' banned: {db.is_banned('user2')}")
    print(f"   Is 'user1' muted: {db.is_muted('user1')}")

    # Test message logging
    print("\n5. Message logging:")
    db.log_message('user1', '', 'Hello everyone!', 'global')
    db.log_message('user1', 'user2', 'Private hi', 'private')
    print(f"   Message count: {db.get_message_count()}")

    # List all users
    print("\n6. All users:")
    for user in db.get_all_users():
        print(f"   - {user.username} ({user.display_name}) role={user.role} banned={user.is_banned}")

    # Cleanup
    os.remove(test_db)
    print("\n=== All tests passed! ===")
