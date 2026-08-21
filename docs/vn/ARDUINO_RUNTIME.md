# Môi Trường Triển Khai Độc Lập Arduino IDE (esp32_ai_runtime/)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan

Thư mục `esp32_ai_runtime/` cung cấp bản phát hành một file duy nhất (`esp32_ai_runtime.ino`) dành cho các nhà phát triển sử dụng nền tảng Arduino IDE.

---

## 2. Bài Toán Đặt Ra & Cách Xử Lý

### Bài toán
Quá trình cài đặt ESP-IDF với chuỗi công cụ CMake và Python phức tạp đối với người dùng phổ thông hoặc môi trường giảng dạy.

### Cách xử lý
Toàn bộ hệ thống (lõi Transformer Decoder, SIMD GEMV, Fast Math LUT, Sliding Window KV-Cache, WiFi SoftAP và Web Server) được tích hợp trong file `esp32_ai_runtime.ino`:
1. Sử dụng thư viện `WebServer` chuẩn của Arduino để phục vụ giao diện chat.
2. Tích hợp trực tiếp các kernel tối ưu hóa vi kiến trúc từ `simd_ops.h` và `fast_math.h`.
3. Quản lý vòng lặp sinh tự hồi quy trong hàm `loop()` của Arduino.

---

## 3. Cấu Hình Biên Dịch Trong Arduino IDE

1. Chọn Board: **ESP32S3 Dev Module**.
2. Thiết lập cấu hình:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
   - **CPU Frequency**: 240MHz
3. Nhấn **Upload** để nạp firmware.
