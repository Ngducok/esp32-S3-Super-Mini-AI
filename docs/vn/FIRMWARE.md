# Hệ Thống Firmware ESP-IDF

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `firmware/` chứa mã nguồn C++ của dự án ESP-IDF chạy trên vi điều khiển ESP32-S3. Hệ thống đảm nhiệm việc điều phối đa nhiệm các tiến trình FreeRTOS, thực thi mô hình ngôn ngữ Micro-Transformer, quản lý trạm phát WiFi SoftAP độc lập, máy chủ web HTTP, và đo đạc telemetry bộ nhớ.

---

## Bài Toán Kỹ Thuật & Giải Pháp

### Bài toán
Vận hành một mô hình AI tạo sinh tự hồi quy song song với trạm phát WiFi và máy chủ HTTP trên vi điều khiển không có PSRAM ngoài đặt ra nhiều thách thức lớn về xung đột RAM và phân chia tài nguyên CPU:
1. Bộ đệm của WiFi và TCP/IP đòi hỏi lượng RAM động tương đối lớn (~100 KB - 150 KB).
2. Vòng lặp sinh token liên tục có thể chiếm dụng CPU, khiến tác vụ Idle của FreeRTOS bị nghẽn và kích hoạt ngắt bảo vệ Task Watchdog (`task_wdt`).
3. Kích thước phân vùng mặc định của ESP-IDF (1MB) không đủ chứa toàn bộ bảng trọng số và nhúng từ vựng của mô hình.

### Giải pháp
1. **Bảng phân vùng tùy chỉnh (`partitions.csv`)**:
   Mở rộng phân vùng ứng dụng `factory` lên 3.5 MB (`0x380000`), cho phép lưu trữ mô hình trực tiếp trên Flash DROM dạng zero-copy.
2. **Phân luồng đa nhân (Dual-Core Task Pinning)**:
   - **CPU Core 0**: Dành riêng cho xử lý sự kiện WiFi SoftAP, ngăn xếp TCP/IP, và lắng nghe kết nối HTTP server.
   - **CPU Core 1**: Dành riêng cho giao diện dòng lệnh Serial và vòng lặp suy luận sinh từ của Transformer (`chat_task`).
3. **Cơ chế nhường CPU (Watchdog Yielding)**:
   Chèn lệnh nhường CPU (`vTaskDelay(pdMS_TO_TICKS(1))`) giữa các lần sinh token, giúp hệ điều hành FreeRTOS reset Watchdog kịp thời mà vẫn duy trì tốc độ sinh chữ mượt mà >15 token/s.

---

## Lưu Đồ Điều Phối Tác Vụ Đa Nhân (Flowchart)

```
                 [Khởi chạy app_main() lúc Boot]
                                │
       ┌────────────────────────┴────────────────────────┐
       ▼                                                 ▼
[CPU 0: Ngăn xếp Mạng & Web]                   [CPU 1: Lõi Tính Toán AI]
  ├── Khởi tạo NVS & TCP/IP                      ├── Thăm dò phần cứng
  ├── Phát WiFi ('ESP32-Local-AI')               ├── Khởi tạo Memory Tracker
  ├── Máy chủ HTTP (Port 80)                     ├── Khởi tạo Transformer
  │     ├── GET  /       (Giao diện Web)         └── Chạy tác vụ chat_task
  │     ├── GET  /status (Telemetry RAM)               ├── Đọc Serial USB-JTAG
  │     └── POST /chat   (Inference LLM)               ├── Vòng lặp Autoregressive
  └── Tiến trình Heartbeat (Chu kỳ 10s)                └── Stream Token ra Serial
```

---

## Cấu Trúc Thư Mục Con

```text
firmware/
├── CMakeLists.txt              # Cấu hình CMake gốc của firmware
├── sdkconfig                   # Cấu hình ESP-IDF (240MHz, Flash 4MB)
├── partitions.csv              # Bảng phân vùng Flash 3.5MB tùy chỉnh
└── main/
    ├── CMakeLists.txt          # Đăng ký mã nguồn & include component
    ├── main.cpp                # Điểm khởi chạy FreeRTOS
    ├── config/                 # Định nghĩa chân GPIO và mức ưu tiên tác vụ
    ├── diagnostics/            # Thăm dò phần cứng, kiểm toán heap, telemetry
    ├── llm/                    # Lõi Transformer, sampler, và trọng số INT8
    └── web/                    # Trình quản lý WiFi SoftAP và REST API
```

---

## Hướng Dẫn Biên Dịch & Nạp Firmware

```bash
# 1. Chuyển vào thư mục firmware
cd firmware

# 2. Biên dịch dự án
idf.py build

# 3. Nạp vào mạch và mở Serial Monitor
idf.py -p COM5 flash monitor
```
