# TCP Socket Chat Application

Ứng dụng chat client-server sử dụng TCP Socket với giao diện Qt.

## Tính năng

### Server
- Quản lý đa kết nối (multi-threading)
- Đăng ký/Đăng nhập tài khoản (SQLite database)
- Chat nhóm (broadcast)
- Chat riêng (1-1)
- Đổi mật khẩu
- Thông báo user online/offline
- Log server events

### Client
- Giao diện Qt Widgets hiện đại
- Đăng ký/Đăng nhập
- Chat nhóm (Global room)
- Chat riêng (Tab riêng cho mỗi cuộc trò chuyện)
- Hiển thị danh sách user online
- Đổi mật khẩu
- Hiển thị trạng thái kết nối
- Tự động reconnect

## Yêu cầu hệ thống

### Linux (Ubuntu/Debian)
```bash
# Cài đặt các dependencies
sudo apt update
sudo apt install -y build-essential cmake

# SQLite3 (cho server)
sudo apt install -y libsqlite3-dev

# Qt5 (cho client)
sudo apt install -y qtbase5-dev qt5-qmake

# Hoặc Qt6 (Ubuntu 22.04+)
sudo apt install -y qt6-base-dev
```

### Windows
1. Cài đặt [CMake](https://cmake.org/download/) (3.14+)
2. Cài đặt [Qt](https://www.qt.io/download-qt-installer) (Qt5 hoặc Qt6)
3. Cài đặt Visual Studio Build Tools hoặc MinGW
4. SQLite3 đã được include trong thư mục thirdparty

### macOS
```bash
# Sử dụng Homebrew
brew install cmake sqlite qt@6
```

## Hướng dẫn Build

### Linux/macOS

```bash
# Clone và cd vào thư mục project
cd /path/to/mmt_c

# Tạo thư mục build
mkdir build && cd build

# Configure với CMake
cmake ..

# Build
make -j$(nproc)

# Executables sẽ nằm trong build/bin/
```

### Windows (với Visual Studio)

```cmd
:: Mở Developer Command Prompt hoặc PowerShell

:: Tạo thư mục build
mkdir build
cd build

:: Configure (điều chỉnh đường dẫn Qt nếu cần)
cmake -DCMAKE_PREFIX_PATH="C:/Qt/6.5.0/msvc2019_64" ..

:: Build
cmake --build . --config Release

:: Executables sẽ nằm trong build/bin/Release/
```

### Windows (với MinGW)

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.5.0/mingw_64" ..
mingw32-make -j4
```

## Hướng dẫn chạy

### 1. Khởi động Server

```bash
# Linux/macOS
./build/bin/chat_server 9000

# Windows
build\bin\Release\chat_server.exe 9000
```

Server sẽ:
- Lắng nghe trên port 9000 (mặc định)
- Tạo file database `chat_server.db`
- In log ra console

### 2. Khởi động Client

```bash
# Linux/macOS
./build/bin/chat_client

# Windows
build\bin\Release\chat_client.exe
```

Client sẽ:
- Mở giao diện đăng nhập
- Nhập địa chỉ server (mặc định: 127.0.0.1:9000)
- Kết nối và đăng ký/đăng nhập

## Ví dụ sử dụng

### Kịch bản: 2 client chat với nhau

1. **Terminal 1 - Khởi động server:**
```bash
./build/bin/chat_server 9000
```

2. **Terminal 2 - Client 1 (Alice):**
```bash
./build/bin/chat_client
```
- Nhập server: `127.0.0.1`, port: `9000`
- Click "Kết nối"
- Tab "Đăng ký": username=`alice`, password=`1234`
- Click "Đăng ký"
- Tab "Đăng nhập": đăng nhập với `alice`/`1234`

3. **Terminal 3 - Client 2 (Bob):**
```bash
./build/bin/chat_client
```
- Kết nối tương tự
- Đăng ký và đăng nhập với `bob`/`1234`

4. **Chat nhóm:**
- Alice gửi tin nhắn trong "Chat nhóm"
- Bob nhìn thấy tin nhắn ngay lập tức

5. **Chat riêng:**
- Alice double-click vào "bob" trong danh sách online
- Một tab mới mở ra cho chat riêng
- Alice gửi tin nhắn -> chỉ Bob nhận được

## Cấu trúc thư mục

```
mmt_c/
├── CMakeLists.txt          # Build configuration
├── README.md               # Documentation
├── common/                 # Shared code
│   ├── Protocol.h          # Protocol definitions
│   └── Protocol.cpp        # Protocol implementation
├── server/                 # Server code
│   ├── server_main.cpp     # Entry point
│   ├── Server.h/cpp        # Server class
│   ├── ClientSession.h/cpp # Client session handler
│   └── Database.h/cpp      # SQLite database
├── client/                 # Client code
│   ├── client_main.cpp     # Entry point
│   ├── MainWindow.h/cpp    # Main chat window
│   ├── AuthDialog.h/cpp    # Login/Register dialog
│   └── ChatClient.h/cpp    # Network client
└── thirdparty/
    └── json.hpp            # nlohmann/json library
```

## Protocol

### Message Format
```
[4 bytes: length (big-endian)][JSON payload]
```

### Message Types
| Type | Code | Description |
|------|------|-------------|
| REGISTER | 1 | Đăng ký tài khoản |
| LOGIN | 2 | Đăng nhập |
| LOGOUT | 3 | Đăng xuất |
| CHANGE_PASSWORD | 4 | Đổi mật khẩu |
| MSG_GLOBAL | 10 | Tin nhắn nhóm |
| MSG_PRIVATE | 11 | Tin nhắn riêng |
| ONLINE_LIST | 20 | Danh sách online |
| USER_STATUS | 21 | Trạng thái user |
| OK | 100 | Thành công |
| ERROR | 101 | Lỗi |

### JSON Payload Example
```json
{
  "type": 10,
  "sender": "alice",
  "receiver": "",
  "content": "Hello everyone!",
  "timestamp": "14:30:25",
  "extra": ""
}
```

## Troubleshooting

### Server không khởi động được
- Kiểm tra port có đang được sử dụng: `lsof -i :9000`
- Thử port khác: `./chat_server 9001`

### Client không kết nối được
- Kiểm tra server đang chạy
- Kiểm tra địa chỉ IP và port
- Kiểm tra firewall

### Qt không tìm thấy
```bash
# Thêm path Qt vào CMake
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64 ..
```

### SQLite3 không tìm thấy
```bash
# Ubuntu/Debian
sudo apt install libsqlite3-dev

# Fedora
sudo dnf install sqlite-devel

# macOS
brew install sqlite
```

## License

MIT License
