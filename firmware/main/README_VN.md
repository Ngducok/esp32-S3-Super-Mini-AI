# Thành Phần Ứng Dụng Chính (ESP-IDF Main Component)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `firmware/main/` chứa điểm khởi chạy ứng dụng chính và các phân hệ chức năng trên vi điều khiển ESP32-S3. Nó kết nối quá trình khởi tạo phần cứng, điều phối tác vụ đa nhân FreeRTOS, tính toán mạng nơ-ron Transformer và phục vụ máy chủ web.

---

## Cấu Trúc Khối Các Phân Hệ

```
                             [firmware/main/main.cpp]
                                        │
             ┌──────────────────────────┼──────────────────────────┐
             ▼                          ▼                          ▼
       [config/]                  [diagnostics/]                 [llm/]
 • Định nghĩa chân GPIO cứng  • Thăm dò phần cứng silicon   • Lõi Transformer (INT8)
 • Mức ưu tiên tác vụ & RAM   • Kiểm toán rò rỉ RAM (0-leak)• KV-cache tĩnh SRAM (~12KB)
 • Các ngưỡng an toàn bộ nhớ  • Xuất telemetry JSON         • Bộ lấy mẫu Argmax token
                                                                   │
                                                                   ▼
                                                                [web/]
                                                      • WiFi SoftAP ('ESP32-Local-AI')
                                                      • Máy chủ Web nhúng (Port 80)
                                                      • REST API (/api/chat, /api/status)
```

---

## Lưu Đồ Thực Thi Đa Nhân FreeRTOS

```
                 [Khởi chạy hàm app_main()]
                              │
             ┌────────────────┴────────────────┐
             ▼                                 ▼
      [CPU Core 0]                       [CPU Core 1]
 ├── Khởi tạo NVS Flash             ├── Chạy thăm dò phần cứng
 ├── Khởi tạo TCP/IP                ├── Khởi tạo Memory Tracker
 ├── Phát WiFi SoftAP               ├── Khởi tạo lõi Transformer
 ├── Khởi động HTTP Server          └── Chạy tác vụ chat_task
 └── Chạy tiến trình Heartbeat            ├── Đọc Serial USB-JTAG
      (Báo cáo RAM mỗi 10s)               ├── Token hóa prompt đầu vào
                                          ├── Vòng lặp Autoregressive
                                          └── Stream Token qua Serial
```

---

## Giải Quyết Bài Toán Kiến Trúc

### 1. Cân Bằng Tải Đa Nhân
- **Bài toán**: Chạy đồng thời phép nhân ma trận Transformer nặng và xử lý gói tin mạng WiFi trên 1 nhân CPU sẽ gây giật lag mạng và kích hoạt watchdog.
- **Giải pháp**: Xử lý mạng WiFi và HTTP Server được ghim cố định trên **CPU Core 0**, trong khi vòng lặp sinh token AI chạy độc lập trên **CPU Core 1**.

### 2. Loại Bỏ Cấp Phát Bộ Nhớ Động
- **Bài toán**: Gọi `malloc`/`free` liên tục trong vòng lặp sinh token làm phân mảnh bộ nhớ SRAM, dẫn đến sập nguồn sau thời gian dài.
- **Giải pháp**: Toàn bộ KV-cache ($3 \times 64 \times 64 = 12,288\text{ bytes}$), bộ đệm prompt và mảng kết quả đều được cấp phát tĩnh ngay từ lúc biên dịch.

---

## Các Thư Mục Con

- [`config/`](config/README_VN.md): Khai báo chân phần cứng, mức ưu tiên tác vụ và cấu hình lấy mẫu.
- [`diagnostics/`](diagnostics/README_VN.md): Thăm dò silicon, phát hiện rò rỉ RAM và benchmark nhân ma trận CPU.
- [`llm/`](llm/README_VN.md): Lõi Transformer INT8, quản lý KV-cache tĩnh và bộ sinh token tự hồi quy.
- [`web/`](web/README_VN.md): Trình quản lý WiFi SoftAP, HTTP daemon và nhúng web Zero-VFS.
