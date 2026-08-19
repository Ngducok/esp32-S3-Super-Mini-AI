# Hướng Dẫn Triển Khai Trên Arduino IDE

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `esp32_ai_runtime/` cung cấp một sketch Arduino hoàn chỉnh trong một file duy nhất (`esp32_ai_runtime.ino`) dành cho cộng đồng Maker, học sinh, sinh viên và lập trình viên muốn thử nghiệm nhanh trên Arduino IDE mà không cần cài đặt môi trường ESP-IDF.

---

## Bài Toán Kỹ Thuật & Giải Pháp

### Bài toán
Cài đặt và thiết lập môi trường ESP-IDF đòi hỏi cấu hình Python venv, CMake và dòng lệnh, gây khó khăn cho người mới bắt đầu hoặc những người quen thuộc với hệ sinh thái Arduino.

### Giải pháp
Toàn bộ hệ thống (trình điều khiển WiFi SoftAP, WebServer, lõi Transformer Decoder INT8, KV-cache tĩnh và các endpoint REST) được hợp nhất thành một sketch `.ino` duy nhất:
1. Sử dụng thư viện `WebServer` tiêu chuẩn của Arduino.
2. Include trực tiếp các file trọng số `model_llm_weights.h` và `web_ui.h` từ firmware.
3. Vận hành vòng lặp suy luận AI bên trong hàm `loop()` quen thuộc của Arduino.

---

## Lưu Đồ Vận Hành Trên Arduino IDE (Flowchart)

```
                 [Khởi Tạo Hàm setup() Của Arduino]
                                │
                 ├── Khởi động Serial.begin(115200)
                 ├── Phát WiFi: WiFi.softAP("ESP32-Local-AI", "12345678")
                 ├── Đăng ký: server.on("/", handleRoot)
                 ├── Đăng ký: server.on("/api/status", handleStatus)
                 ├── Đăng ký: server.on("/api/chat", handleChatAPI)
                 └── Kích hoạt máy chủ: server.begin()
                                │
                                ▼
                 [Vòng Lặp loop() Thực Thi Liên Tục]
                                │
            ┌───────────────────┴───────────────────┐
            ▼                                       ▼
 [server.handleClient()]                  [Serial.available()]
   ├── Lắng nghe HTTP request               ├── Đọc prompt từ Serial
   ├── Gọi hàm forwardToken()               ├── Stream từng token ra màn hình
   └── Gửi JSON về trình duyệt              └── In kết quả đo lường hiệu năng
```

---

## Cấu Hình Arduino IDE & Hướng Dẫn Nạp

1. Cài đặt gói bo mạch **ESP32 by Espressif Systems** trong Boards Manager của Arduino IDE.
2. Mở file `esp32_ai_runtime.ino`.
3. Trong menu **Tools > Board**, chọn **ESP32S3 Dev Module**.
4. Thiết lập các mục sau trong menu Tools:
   - **Flash Size**: 4MB (32Mb)
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **USB Mode**: Hardware CDC and JTAG
   - **Upload Mode**: UART0 / Hardware CDC
   - **PSRAM**: Disabled
5. Nhấn nút **Upload** và mở Serial Monitor ở tốc độ Baud `115200`.
