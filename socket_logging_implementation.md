# Socket Logging System - Implementation Guide

## Tổng Quan

Tôi đã thêm hệ thống logging chi tiết vào project chat của bạn để theo dõi **từng bước hoạt động của socket** - từ khi tạo kết nối TCP, serialize/deserialize messages, đến buffer operations.

## Những Gì Đã Thêm

### 1. **Client-Side Logging (ChatClient.cpp/h)**

#### A. Signal Mới
```cpp
// ChatClient.h
signals:
    void socketLogMessage(const QString& log);
```

#### B. Các Điểm Logging

##### 1️⃣ **TCP Connection**
```cpp
// Khi bắt đầu connect
[SOCKET] Initiating TCP connection to localhost:9000

// Khi kết nối thành công (3-way handshake hoàn tất)
[SOCKET] TCP connection established (3-way handshake completed)
[SOCKET] Local: 192.168.1.100:54321 <-> Remote: 192.168.1.1:9000
```

**Thông tin:** 
- Local/Remote IP và Port
- Xác nhận TCP handshake hoàn tất

---

##### 2️⃣ **Message Serialization (SEND)**
```cpp
// Trước khi serialize
[PROTOCOL] Preparing message: Type=LOGIN

// Sau khi serialize
[SERIALIZE] Message serialized: Total=85 bytes (Header=4 bytes, Payload=81 bytes)

// Gửi vào TCP buffer
[SOCKET-SEND] Sent 85/85 bytes to TCP buffer
```

**Chi tiết:**
- **Type**: Loại message (LOGIN, MSG_GLOBAL, PING, etc.)
- **Total bytes**: 4-byte length prefix + JSON payload
- **Actual sent**: Số bytes thực tế gửi được

---

##### 3️⃣ **Socket Receive Operations**
```cpp
// Khi có data đến
[SOCKET-RECV] Data available: 124 bytes
[SOCKET-RECV] Read 124 bytes from TCP buffer

// Buffer operations
[BUFFER] Buffer updated: 0 -> 124 bytes
[BUFFER] Complete message detected, extracting...
```

**Thông tin:**
- Số bytes available trong socket buffer
- Số bytes thực tế đọc
- Trạng thái buffer trước/sau khi thêm data

---

##### 4️⃣ **Message Deserialization (RECEIVE)**
```cpp
[DESERIALIZE] Message extracted: Type=OK
[BUFFER] Processed 1 message(s), 0 bytes remaining
```

**Chi tiết:**
- Loại message đã parse được
- Số messages đã xử lý
- Bytes còn lại trong buffer (cho message tiếp theo)

---

##### 5️⃣ **Connection Close**
```cpp
[SOCKET] TCP connection closed
[BUFFER] Clearing receive buffer (45 bytes)
```

---

### 2. **Protocol Layer Logging (Protocol.cpp)**

#### Logging có thể enable bằng `#define PROTOCOL_DEBUG_LOG`

```cpp
// Trong serialize()
[PROTOCOL] Serialize: Type=MSG_GLOBAL, Length=152 bytes
[PROTOCOL] JSON Payload: {"type":10,"sender":"user1","content":"Hello"}
[PROTOCOL] Header bytes: [00 00 00 98]  // Big-endian length

// Trong deserialize()
[PROTOCOL] Deserialize: Length=152 bytes
[PROTOCOL] JSON Payload: {"type":100,"content":"OK"}
[PROTOCOL] Parsed: Type=OK
```

**Lợi ích:**
- Thấy chính xác JSON payload
- Thấy 4-byte header (network byte order)
- Debug parsing errors

---

### 3. **GUI Log Viewer (MainWindow)**

#### A. UI Components
- **QTextEdit**: Hiển thị logs với color-coding
- **Toggle Button**: Hiện/ẩn log panel
- **Clear Button**: Xóa logs
- **Menu item**: "Hiện/Ẩn Socket Log"

#### B. Color Scheme
```
🔵 [SOCKET]      - Cyan (#4fc3f7)     - TCP connection events
🟢 [PROTOCOL]    - Green (#81c784)    - Protocol operations
🟠 [SERIALIZE]   - Orange (#ffb74d)   - Serialization
🟠 [DESERIALIZE] - Orange (#ffb74d)   - Deserialization
🟣 [BUFFER]      - Purple (#ba68c8)   - Buffer operations
🔴 [SOCKET-SEND] - Red (#e57373)      - Send operations
🔵 [SOCKET-RECV] - Blue (#64b5f6)     - Receive operations
```

#### C. Features
- **Auto-scroll**: Tự động scroll xuống log mới nhất
- **Timestamp**: Mỗi log có timestamp chính xác đến millisecond
- **Monospace font**: Dễ đọc với font Consolas/Courier
- **Dark theme**: Background đen (#1e1e1e) giống IDE

---

## Cách Sử Dụng

### Để Xem Socket Logs:

#### Trong Client GUI:
1. **Bấm nút** "▲ Hiện Socket Log" ở dưới cùng window
2. Hoặc **Menu**: Trợ giúp → Hiện/Ẩn Socket Log
3. Panel log sẽ xuất hiện với height 200px

#### Logs Tự Động Hiện:
- Mỗi khi connect/disconnect
- Mỗi khi gửi/nhận message
- Mỗi thao tác với buffer

---

## Log Flow Example

### Kịch Bản: User Login

```
[10:30:45.123] [SOCKET] Initiating TCP connection to localhost:9000
[10:30:45.156] [SOCKET] TCP connection established (3-way handshake completed)
[10:30:45.157] [SOCKET] Local: 127.0.0.1:54321 <-> Remote: 127.0.0.1:9000

[10:30:46.200] [PROTOCOL] Preparing message: Type=LOGIN
[10:30:46.201] [SERIALIZE] Message serialized: Total=85 bytes (Header=4 bytes, Payload=81 bytes)
[10:30:46.202] [SOCKET-SEND] Sent 85/85 bytes to TCP buffer

[10:30:46.250] [SOCKET-RECV] Data available: 78 bytes
[10:30:46.251] [SOCKET-RECV] Read 78 bytes from TCP buffer
[10:30:46.252] [BUFFER] Buffer updated: 0 -> 78 bytes
[10:30:46.253] [BUFFER] Complete message detected, extracting...
[10:30:46.254] [DESERIALIZE] Message extracted: Type=OK
[10:30:46.255] [BUFFER] Processed 1 message(s), 0 bytes remaining
```

### Phân Tích Timeline:
1. **T+0ms**: Bắt đầu connect
2. **T+33ms**: TCP handshake hoàn tất (RTT ≈ 33ms)
3. **T+1043ms**: User bấm Login → serialize
4. **T+1045ms**: Gửi 85 bytes
5. **T+1093ms**: Nhận response (48ms latency)
6. **T+1098ms**: Parse xong, login thành công

---

## Ứng Dụng Cho Testing

### 1. **Đo Latency**
```
# Tính từ SOCKET-SEND đến SOCKET-RECV
Latency = timestamp_recv - timestamp_send
```

### 2. **Kiểm Tra Packet Loss**
```
# So sánh:
- Số lần [SOCKET-SEND]
- Số lần [SOCKET-RECV] tương ứng

Nếu không match → investigate TCP retransmissions
```

### 3. **Phân Tích Buffer**
```
# Nếu thấy:
[BUFFER] Buffer updated: 0 -> 50 bytes
[BUFFER] Processed 0 message(s), 50 bytes remaining

→ Message chưa nhận đủ, đang chờ thêm data
→ Có thể do TCP fragmentation
```

### 4. **Debug Serialization Issues**
```
# Nếu message bị lỗi, check:
[SERIALIZE] ... Total=X bytes
[SOCKET-SEND] Sent Y/X bytes

Nếu Y < X → Send buffer full → cần retry
```

### 5. **Throughput Measurement**
```
# Count trong 1 giây:
messages_sent = số lần [SOCKET-SEND]
bytes_sent = tổng bytes từ [SOCKET-SEND]

Throughput = messages_sent / 1s
Bandwidth = bytes_sent / 1s
```

---

## Advanced: Enable Protocol Debug Log

### Để xem JSON payload chi tiết:

#### Bước 1: Mở `Protocol.cpp`
```cpp
// Thêm dòng này lên đầu file (sau #include)
#define PROTOCOL_DEBUG_LOG

#include "Protocol.h"
// ...
```

#### Bước 2: Rebuild project

#### Bước 3: Log sẽ xuất hiện trong console (stdout)
```
[PROTOCOL] Serialize: Type=LOGIN, Length=81 bytes
[PROTOCOL] JSON Payload: {"type":2,"sender":"","receiver":"","content":"{\"username\":\"user1\",\"password\":\"123\"}","timestamp":"10:30:46","extra":""}
[PROTOCOL] Header bytes: [0 0 0 51]
```

**Lưu ý**: Console logs không hiện trong GUI, chỉ trong terminal

---

## Tích Hợp Với Test Suite

### Test Script có thể parse logs:

```python
# Python test script example
import re

def parse_socket_logs(log_file):
    with open(log_file, 'r', encoding='utf-8') as f:
        logs = f.readlines()
    
    metrics = {
        'connections': 0,
        'messages_sent': 0,
        'messages_received': 0,
        'total_bytes_sent': 0,
        'total_bytes_recv': 0,
        'latencies': []
    }
    
    send_times = {}
    
    for log in logs:
        # Parse timestamp
        match = re.match(r'\[(\d{2}:\d{2}:\d{2}\.\d{3})\]', log)
        if not match:
            continue
        timestamp = match.group(1)
        
        # Count connections
        if '[SOCKET] TCP connection established' in log:
            metrics['connections'] += 1
        
        # Count sends
        if '[SOCKET-SEND]' in log:
            match = re.search(r'Sent (\d+)/(\d+) bytes', log)
            if match:
                metrics['messages_sent'] += 1
                metrics['total_bytes_sent'] += int(match.group(1))
                # Store send time for latency calculation
                # (simplified - need sequence numbers for accurate pairing)
        
        # Count receives
        if '[SOCKET-RECV] Read' in log:
            match = re.search(r'Read (\d+) bytes', log)
            if match:
                metrics['messages_received'] += 1
                metrics['total_bytes_recv'] += int(match.group(1))
    
    return metrics

# Usage
metrics = parse_socket_logs('socket_log.txt')
print(f"Connections: {metrics['connections']}")
print(f"Messages sent: {metrics['messages_sent']}")
print(f"Total bytes sent: {metrics['total_bytes_sent']}")
print(f"Throughput: {metrics['messages_sent']/60:.2f} msg/s")  # Assuming 60s test
```

---

## Tối Ưu Hóa Dựa Trên Logs

### Vấn Đề 1: Send nhiều messages nhỏ
```
[SOCKET-SEND] Sent 45 bytes
[SOCKET-SEND] Sent 52 bytes
[SOCKET-SEND] Sent 38 bytes
```

**Giải pháp:** Batch messages, enable Nagle's algorithm

---

### Vấn Đề 2: Buffer luôn còn bytes
```
[BUFFER] Processed 1 message(s), 45 bytes remaining
[BUFFER] Processed 1 message(s), 90 bytes remaining
```

**Giải pháp:** Check Protocol.cpp - có thể message length prefix bị sai

---

### Vấn Đề 3: Large latency spikes
```
[SOCKET-SEND] Sent ... (10:30:45.100)
[SOCKET-RECV] Data ... (10:30:46.500)  # 1400ms latency!
```

**Nguyên nhân có thể:**
- Network congestion
- Server busy (check server CPU)
- TCP retransmission (packet loss)

**Debug:** Dùng Wireshark để xem TCP packets

---

## Export Logs

### Để lưu logs ra file:

#### Thêm vào MainWindow.cpp:
```cpp
void MainWindow::onExportLogsClicked() {
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Export Socket Logs"),
        "socket_log.txt",
        tr("Text Files (*.txt);;All Files (*)")
    );
    
    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << socketLogViewer_->toPlainText();
            file.close();
            QMessageBox::information(this, tr("Success"), 
                tr("Logs exported to %1").arg(filename));
        }
    }
}
```

---

## Comparing với Wireshark

### Socket Logs vs Wireshark:

| Feature | Socket Logs | Wireshark |
|---------|-------------|-----------|
| **Level** | Application layer | Network layer |
| **Content** | See JSON payload | See TCP segments |
| **Timing** | Application time | Packet capture time |
| **Easy to use** | ✅ Very easy | ⚠️ Need expertise |
| **Buffer info** | ✅ Yes | ❌ No |
| **Serialize info** | ✅ Yes | ❌ No |
| **TCP details** | ❌ No | ✅ Yes (retrans, ACK, etc.) |

### Kết Hợp Cả Hai:
1. **Socket Logs**: Debug application logic, timing
2. **Wireshark**: Debug network issues, packet loss

---

## Kết Luận

### Bạn Có Thể:

✅ **Xem từng bước** message đi qua các tầng:
   - Application (prepare message)
   - Protocol (serialize)
   - Socket (send to TCP buffer)
   - Network (TCP sends) [← cần Wireshark]
   - Socket (receive from TCP buffer)
   - Protocol (deserialize)
   - Application (process message)

✅ **Đo các metrics** socket:
   - Connection time
   - Latency (per message)
   - Throughput
   - Buffer utilization

✅ **Debug issues**:
   - Message loss
   - Serialization errors
   - Buffer problems
   - Performance bottlenecks

✅ **So sánh implementations**:
   - C++ vs Python client
   - JSON vs Binary protocol
   - Different network conditions

---

## Next Steps

### 1. Thêm Vào Test Guide
Cập nhật `socket_testing_guide.md` với instructions về cách dùng socket logs.

### 2. Implement Server-Side Logging
Thêm tương tự vào Server.cpp và ServerWindow.cpp.

### 3. Add Export Function
Thêm button "Export Logs" để lưu ra file.

### 4. Add Metrics Dashboard
Tạo panel hiển thị real-time metrics:
- Messages/sec
- Bytes/sec
- Average latency
- Buffer usage

### 5. Log Filtering
Thêm dropdown để filter logs:
- All
- Socket only
- Protocol only
- Errors only

---

## Code Locations

**Modified Files:**
- ✅ `client/ChatClient.h` - Added socketLogMessage signal
- ✅ `client/ChatClient.cpp` - Added logging to all socket operations
- ✅ `common/Protocol.cpp` - Added optional debug logging
- ✅ `client/MainWindow.h` - Added log viewer components
- ✅ `client/MainWindow.cpp` - Implemented log viewer UI

**Files to Update (Optional):**
- `server/Server.cpp` - Add server-side logging
- `server/ClientSession.cpp` - Add per-client logging
- `server_gui/ServerWindow.cpp` - Add server log viewer
