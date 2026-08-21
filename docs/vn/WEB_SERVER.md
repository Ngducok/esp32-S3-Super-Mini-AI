# Phân Hệ WiFi SoftAP & Máy Chủ Web

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `web/` quản lý điểm phát sóng WiFi độc lập (SoftAP) và máy chủ HTTP nhúng. Nó xử lý kết nối từ trình duyệt điện thoại/máy tính của người dùng, phục vụ giao diện chat lưu trong bộ nhớ Flash và định tuyến các yêu cầu REST API đến lõi mô hình ngôn ngữ Micro-Transformer.

---

## Bài Toán Kỹ Thuật & Giải Pháp

### Bài toán
1. Các máy chủ web nhúng truyền thống thường dựa vào hệ thống tệp tin SPIFFS hoặc LittleFS trên bộ nhớ Flash, gây phức tạp bảng phân vùng, tốn RAM đệm và làm tăng độ trễ đọc file.
2. Nhận payload HTTP POST và chạy suy luận AI cùng lúc dễ làm nghẽn socket mạng và gây rớt kết nối trình duyệt.

### Giải pháp
1. **Cơ chế nhúng Web Zero-VFS trực tiếp vào Flash**:
   Toàn bộ mã HTML5, CSS3 và JavaScript được đóng gói thành một file header C++ duy nhất (`web_ui.h`) dưới dạng chuỗi hằng số lưu trong Flash Data ROM (`DROM`). Máy chủ HTTP phục vụ trang web trực tiếp từ Flash với độ trễ phản hồi cực nhanh dưới 1 mili-giây mà không tốn SRAM.
2. **Kiến trúc REST API chuẩn mực**:
   - `GET /`: Phục vụ giao diện người dùng đơn trang Dark Mode.
   - `GET /api/status`: Trả về dữ liệu telemetry hệ thống (SRAM trống, PSRAM, thời gian uptime) ở định dạng JSON.
   - `POST /api/chat`: Tiếp nhận câu hỏi (prompt) từ người dùng, kích hoạt bộ sinh từ của Transformer và trả về kết quả kèm tốc độ tok/s và độ trễ.

---

## Lưu Đồ Giao Tiếp Client - Server (Flowchart)

```
[Trình duyệt Người dùng (Phone/PC)]               [Máy chủ HTTP ESP32-S3]
            │                                               │
            ├─────── 1. Kết nối vào WiFi SoftAP ───────────►│ (SSID: 'ESP32-Local-AI')
            │                                               │
            ├─────── 2. Gửi HTTP GET / ────────────────────►│
            │◄────── 3. Phục vụ HTML từ Flash DROM ─────────┤ (Phản hồi 200 OK tức thì)
            │                                               │
            ├─────── 4. Gửi HTTP POST /api/chat ───────────►│ (Payload: {"message": "..."})
            │           {"message": "tell me a joke"}       │
            │                                               ├── [Giải mã JSON Prompt]
            │                                               ├── [Chạy Transformer Forward]
            │                                               ├── [Gom chuỗi Token vào Buffer]
            │                                               └── [Đóng gói JSON phản hồi]
            │◄────── 5. Nhận kết quả 200 OK JSON ───────────┤
            │           {"reply": "...", "tok_sec": ..}     │
            │                                               │
            └─────── 6. Hiển thị tin nhắn lên UI ───────────┘
```

---

## Danh Sách File Mã Nguồn

- `wifi_ap.h` / `wifi_ap.cpp`: Khởi tạo NVS, giao diện mạng TCP/IP và kích hoạt trạm phát WiFi SoftAP (`192.168.4.1`).
- `web_server.h` / `web_server.cpp`: Cấu hình máy chủ HTTP của ESP-IDF, đăng ký URI handler, bóc tách JSON và gọi `LLM::Generator`.
- `web_ui.h`: Header C++ chứa toàn bộ mã nguồn giao diện HTML/CSS/JS đã được đóng gói.
