# Báo Cáo Kỹ Thuật: Mô Hình Micro-Transformer Chạy Cục Bộ Trên ESP32-S3

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="results.md">English</a> | 
  <a href="results_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tóm Tắt Dự Án

Dự án này đã hiện thực hóa thành công một **Mô hình Ngôn ngữ Tạo sinh (Micro-Transformer)** chạy hoàn toàn cục bộ 100% trên vi điều khiển **ESP32-S3 Super Mini** mà không cần bộ nhớ PSRAM ngoài, không cần kết nối mạng Internet hay máy chủ API bên ngoài.

Hệ thống tích hợp mô hình **Transformer Decoder lượng tử hóa INT8**, trạm phát **WiFi SoftAP độc lập**, **Giao diện Web ChatGPT Dark Mode** nhúng sẵn trong bộ nhớ Flash của chip đơn.

---

## 2. Thống Kê Đo Đạc Thực Nghiệm

Bảng dưới đây trình bày các chỉ số đo đạc thực tế từ quá trình vận hành trực tiếp trên chip silicon ESP32-S3:

| Chỉ số đo đạc | Giá trị thực nghiệm | Ý nghĩa kỹ thuật |
| :--- | :--- | :--- |
| **Vi điều khiển mục tiêu** | **ESP32-S3 Super Mini (Silicon Rev v0.2)** | 2 Cores Xtensa LX7 @ 240 MHz |
| **Kích thước file nhị phân Flash** | **1.44 MB** *(Phân vùng ứng dụng: 3.5 MB)* | Nằm hoàn hảo trong Flash chuẩn 4MB |
| **Bộ nhớ PSRAM ngoài yêu cầu** | **0 KB (Không cần PSRAM ngoài)** | Tối ưu hóa chi phí phần cứng (~$2 / bo mạch) |
| **Tiêu thụ SRAM (KV-Cache + Buffer)**| **~24.5 KB** | Còn trống **> 210 KB SRAM nội bộ** cho mạng |
| **Tốc độ sinh token** | **9.33 – 20.00 token/giây** | Ngang ngửa và vượt trội các dự án nhúng quốc tế |
| **Độ trễ mỗi token** | **~50 ms – 107 ms / token** | Phản hồi mượt mà theo thời gian thực |
| **Thời gian khởi động hoàn chỉnh** | **< 1.5 giây** | Khởi động cả SoftAP WiFi, Web Server & Model |
| **Độ trôi bộ nhớ (Rò rỉ RAM)** | **0 Byte (Zero Leak)** | Chạy ổn định lâu dài trên hệ điều hành FreeRTOS |

---

## 3. Kiến Trúc Hệ Thống

```
                     ┌─────────────────────────────────────────────────────────┐
                     │          ESP32-S3 SUPER MINI HARDWARE (240MHz)          │
                     └────────────────────────────┬────────────────────────────┘
                                                  │
                   ┌──────────────────────────────┴──────────────────────────────┐
                   ▼                                                             ▼
┌──────────────────────────────────────┐                      ┌──────────────────────────────────────┐
│       NEURAL COMPUTATION CORE        │                      │        COMMUNICATION & UI CORE       │
├──────────────────────────────────────┤                      ├──────────────────────────────────────┤
│ • Micro-Transformer Decoder (INT8)   │                      │ • SoftAP WiFi: 'ESP32-Local-AI'      │
│ • 118,784 Tham số (~119K INT8)       │                      │ • Flash-Resident Web UI (Port 80)    │
│ • Static SRAM KV-Cache (~24.5 KB)    │                      │ • Dual-Core Live Streaming Engine    │
│ • Zero-Copy Flash DROM Weights       │                      │ • Quy trình tự hồi quy thực thụ      │
│ • Bộ lấy mẫu Argmax & Temperature    │                      │ • Real-Time Telemetry & Status API   │
└──────────────────────────────────────┘                      └──────────────────────────────────────┘
```

---

## 4. So Sánh Với Các Dự Án Mã Nguồn Mở Quốc Tế

| Tiêu chí | Dự Án Này (ESP32-S3 Micro-LLM) | `slvDev/esp32-ai` | `karpathy/llama2.c` |
| :--- | :--- | :--- | :--- |
| **Bộ nhớ PSRAM ngoài** | **0 KB (Chạy tốt trên Super Mini)** | **Bắt buộc 8 MB Octal PSRAM** | Thường cần PSRAM trên MCU |
| **Dung lượng Flash yêu cầu** | **4 MB Flash** | **Bắt buộc 16 MB Flash** | Phụ thuộc kích thước model |
| **Chi phí phần cứng** | **Siêu rẻ (~70.000 – 90.000 VNĐ)** | Cao hơn (~130.000 – 160.000 VNĐ) | Tùy bo mạch |
| **Giao diện người dùng** | **Web Hotspot ChatGPT UI + Serial** | Màn hình LCD SPI nhỏ | Console Terminal |
| **Cơ chế mô hình** | **Micro-LLM tự hồi quy (JARVIS)** | Chỉ sinh truyện cố định | Sinh một lần |
| **Kích thước KV-Cache** | **24.5 KB tĩnh trong SRAM (0-Leak)** | Động trong PSRAM | Động / Tùy bo mạch |
| **Giao thức kết nối** | **Kênh đôi (REST API & Serial)** | Chỉ Serial/SPI | Chỉ Serial |

---

## 5. Điểm Đổi Mới & Đóng Góp Kỹ Thuật

1. **Bộ đệm KV-Cache tĩnh trong SRAM nội bộ**:
   - Loại bỏ hoàn toàn việc cấp phát động (`malloc`/`free`) trong quá trình suy luận. KV-Cache nằm cố định trong vùng nhớ 24.5 KB ($2 \times 3 \times 64 \times 64$ bytes) trong SRAM nội bộ, ngăn ngừa hiện tượng phân mảnh bộ nhớ.
2. **Cơ chế nhúng Web Zero-VFS vào Flash**:
   - Tích hợp toàn bộ mã nguồn HTML5, CSS3 và JavaScript thành chuỗi hằng số C++ lưu trên Flash DROM. Máy chủ HTTP phục vụ trang web tức thì mà không cần phân vùng hệ thống tệp tin (SPIFFS/LittleFS).
3. **Quy trình suy luận tự hồi quy thực thụ**:
   - Tính toán trực tiếp tích ma trận INT8 qua từng bước, truyền logits qua bộ Sampler để giải mã token từ vựng và truyền trực tiếp về cho người dùng trong thời gian thực.

---

## 6. Ưu Điểm & Hạn Chế

### Ưu điểm:
- **Độc lập và bảo mật 100%**: Dữ liệu không rời khỏi con chip; không cần tài khoản hay khóa API trả phí.
- **Chi phí cực thấp**: Hoạt động hoàn hảo trên các bo mạch vi điều khiển giá rẻ dưới $3.
- **Độ trễ truyền mạng bằng 0**: Phản hồi tức thì ngay trên thiết bị.
- **Hỗ trợ đa nền tảng**: Tương thích cả ESP-IDF và Arduino IDE.

### Hạn chế:
- **Cửa sổ ngữ cảnh (Context Window)**: Giới hạn trong khoảng 64–128 token do kích thước SRAM nội bộ không có PSRAM.
- **Phạm vi kiến thức**: Tập trung vào các chủ đề hội thoại, câu chuyện và chẩn đoán hệ thống được đóng gói tối ưu cho Edge AI.

---

## 7. Hướng Nghiên Cứu Tiếp Theo

1. **Đóng gói trọng số 4-bit (INT4 / 2-Bit Weight Packing)**: Nén nhiều tham số vào 1 byte để tăng dung lượng mô hình trên Flash 4MB.
2. **Mô hình Attention tuyến tính (RWKV / Mamba-Micro)**: Chuyển sang mô hình trạng thái ẩn $O(1)$ RAM để đạt độ dài ngữ cảnh vô hạn với $< 2\text{ KB}$ SRAM.
3. **Speculative Decoding trên 2 nhân CPU**: Thực hiện kiểm chứng token song song giữa 2 nhân Xtensa Core 0 và Core 1 để đạt tốc độ $> 35\text{ token/giây}$.
4. **Tích Hợp Ngoại Vi Phần Cứng & Âm Thanh (Hiện Thực Hóa "JARVIS" Ngoài Đời Thực)**:
   - **Xử lý âm thanh & Giọng nói**: Kết nối Microphone kỹ thuật số I2S (INMP441) để nhận diện từ khóa đánh thức (Wake-Word) và chip khuếch đại âm thanh I2S DAC (MAX98357A) để phát giọng nói trực tiếp từ chip.
   - **Giao tiếp Robot & Cơ cấu chấp hành**: Kết nối với driver động cơ, servo và các bus cảm biến (I2C/SPI/CAN) để biến vi điều khiển Micro-LLM thành một module "Bộ não Robot JARVIS" chạy 100% offline cho các hệ thống Robotics và AIoT.
