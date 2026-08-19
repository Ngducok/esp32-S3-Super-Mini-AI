# Mô Hình Ngôn Ngữ Micro-Transformer Chạy Cục Bộ Trên ESP32-S3 Không Cần PSRAM Ngoài

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

Dự án này hiện thực hóa một mô hình ngôn ngữ tạo sinh tự hồi quy (Micro-Transformer) chạy hoàn toàn cục bộ 100% trên vi điều khiển ESP32-S3. Mô hình xử lý trực tiếp trên phần cứng không phụ thuộc vào đám mây hay máy chủ bên ngoài, sinh và stream token với tốc độ từ 9.33 đến 20.00 token/giây. Hệ thống không yêu cầu chip PSRAM mở rộng, vận hành tối ưu bên trong giới hạn 384KB SRAM nội bộ của bo mạch giá rẻ ESP32-S3 Super Mini.

Hệ thống kết hợp lõi Transformer Decoder với trạm phát WiFi độc lập (SoftAP) và máy chủ HTTP Web Server nhúng trực tiếp trong bộ nhớ Flash, phục vụ giao diện chat Dark Mode tốc độ cao mà không cần hệ thống tệp tin ngoài.

---

## Nguồn cảm hứng và Điểm khác biệt so với slvDev/esp32-ai

Dự án được truyền cảm hứng từ triết lý phân tầng bộ nhớ (Memory Tiering) được chứng minh trong dự án [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai) (dựa trên ý tưởng Per-Layer Embeddings từ Google [Gemma 3n](https://ai.google.dev/gemma/docs/gemma-3n)): đưa các bảng tra cứu trọng số lớn vào bộ nhớ Flash đọc dạng zero-copy và dành bộ nhớ RAM siêu nhanh cho các phép toán tính toán lặp lại.

Trong khi `slvDev/esp32-ai` hướng đến các module ESP32-S3 cao cấp trang bị 8MB Octal PSRAM và 16MB Flash để điều khiển màn hình vật lý SPI LCD, dự án này giải quyết bài toán kiến trúc khác: **Làm thế nào để chạy mô hình ngôn ngữ tạo sinh trên một bo mạch vi điều khiển giá rẻ (~70.000 VNĐ) chỉ có Flash 4MB và KHÔNG CÓ PSRAM ngoài (0 KB PSRAM)?**

### Bảng So Sánh Kiến Trúc

| Khía cạnh kiến trúc | `slvDev/esp32-ai` | Dự Án Này (ESP32-S3 Micro-LLM) |
| :--- | :--- | :--- |
| **Nguồn cảm hứng** | Google Gemma 3n (Per-Layer Embeddings) | `slvDev/esp32-ai` & Kiến trúc LLaMA Decoder |
| **Phần cứng mục tiêu** | ESP32-S3 N16R8 (16MB Flash / 8MB PSRAM) | ESP32-S3 Super Mini (4MB Flash / **0 KB PSRAM**) |
| **Cơ chế cấp phát RAM** | Phụ thuộc 8MB Octal PSRAM ngoài | **100% SRAM nội bộ tĩnh** (~12 KB KV-Cache) |
| **Giao diện tương tác** | Màn hình SPI LCD nối dây GPIO | **Trạm phát WiFi Hotspot + Giao diện Web ChatGPT** |
| **Cơ chế phục vụ** | Bộ đệm khung hình SPI cục bộ | REST API bất đồng bộ trên cổng HTTP 80 |
| **Chi phí phần cứng** | Bo mạch phân khúc cao (~130k - 160k VNĐ) | Bo mạch phân khúc tối thiểu (~70k - 90k VNĐ) |

---

## Các thông số thực nghiệm

| Thông số | Giá trị thực tế |
| :--- | :--- |
| Vi điều khiển | ESP32-S3 Super Mini (2 nhân Xtensa LX7 @ 240 MHz) |
| SRAM nội bộ | Tổng 512 KB (~380 KB SRAM khả dụng) |
| PSRAM ngoài | Không yêu cầu (0 KB PSRAM) |
| Kích thước Flash | Nhị phân 1.44 MB (nằm trong Flash chuẩn 4 MB) |
| Tiêu thụ RAM | ~12 KB KV-Cache trong SRAM (còn trống >220 KB SRAM) |
| Tốc độ sinh chữ | 9.33 – 20.00 token/giây |
| Độ trễ mỗi token | ~50 ms – 107 ms / token |
| Kết nối | WiFi SoftAP độc lập (`ESP32-Local-AI`) + USB Serial-JTAG |
| Lượng tử hóa | INT8 đối xứng theo tensor |

---

## Thách thức kỹ thuật và Giải pháp kiến trúc

Mô hình ngôn ngữ tiêu tốn rất nhiều băng thông và dung lượng bộ nhớ. Trên vi điều khiển, bộ nhớ RAM siêu nhanh (SRAM) chỉ có vài trăm kilobyte. Các dự án LLM nhúng thông thường luôn yêu cầu từ 8MB đến 16MB PSRAM.

Trên bo mạch ESP32-S3 Super Mini không có PSRAM, việc vận hành AI tạo sinh đòi hỏi phân tầng bộ nhớ nghiêm ngặt và tuyệt đối không cấp phát động trong quá trình suy luận.

### Phân tầng bộ nhớ (Memory Tiering)

```
  SRAM  (Nhanh, ~384 KB)   KV-Cache, activation buffers, token logits, FreeRTOS stacks
  FLASH (1.44 MB, DROM)   Ma trận trọng số INT8, bảng từ vựng, Web UI bundle
```

- **Flash DROM (Đọc trực tiếp Zero-Copy)**: Toàn bộ ma trận trọng số ($W_q, W_k, W_v, W_o, W_1, W_2, W_{te}, W_{pe}, W_{head}$) và chuỗi từ vựng được lưu dưới dạng mảng `const int8_t` trong Flash Data ROM. CPU đọc trực tiếp từng dòng ma trận qua bus SPI Flash cache trong phép nhân ma trận mà không cần tải toàn bộ lớp vào RAM.
- **SRAM nội bộ (Bộ đệm tĩnh)**: Vùng nhớ Key-Value Cache (KV-Cache) được cấp phát tĩnh trong SRAM. Với chuỗi ngữ cảnh 64 token qua 3 layer Transformer (kích thước ẩn $d=64$), toàn bộ KV-Cache chỉ tiêu thụ:

$$3 \text{ layers} \times 64 \text{ tokens} \times 64 \text{ dimensions} = 12,288 \text{ bytes} \approx 12 \text{ KB}$$

Hệ thống còn dư hơn 220 KB SRAM nội bộ cho bộ đệm TCP/IP của WiFi và các tác vụ FreeRTOS.

> [!NOTE]
> Tuyệt đối không gọi `malloc` hay `free` trong vòng lặp sinh token. Điều này loại bỏ hoàn toàn nguy cơ phân mảnh bộ nhớ và rò rỉ RAM (Zero Memory Leak).

---

## Sơ đồ khối kiến trúc

```
[Dữ liệu đầu vào từ người dùng]
               │
               ▼
   [Tra cứu & Khớp chuỗi Token]
               │
               ▼
   [Lõi Transformer Tự Hồi Quy]
  ├── Nhúng từ & Vị trí (WTE + WPE INT8)
  ├── Cơ chế Self-Attention đa đầu (L=3, H=4, d_head=16)
  ├── Quản lý KV-Cache tĩnh trong SRAM
  ├── Mạng nơ-ron truyền thẳng GELU MLP (d_ff=128)
  └── Chiếu Logits đầu ra (LM Head)
               │
               ▼
   [Bộ lấy mẫu Argmax & Nhiệt độ]
               │
       ┌───────┴───────┐
       ▼               ▼
[Stream USB Serial] [HTTP Server / Giao diện Web]
```

---

## Cấu trúc thư mục

```text
esp32/
├── .gitignore                    # Quy tắc bỏ qua file build & cache
├── LICENSE                       # Giấy phép nguồn mở MIT
├── README.md                     # Tài liệu kỹ thuật tiếng Anh
├── README_VN.md                  # Tài liệu kỹ thuật tiếng Việt
├── results.md                    # Báo cáo thực nghiệm tiếng Anh
├── results_VN.md                 # Báo cáo thực nghiệm tiếng Việt
│
├── firmware/                     # Dự án C++ ESP-IDF chuẩn công nghiệp
│   ├── CMakeLists.txt            # Cấu hình build gốc
│   ├── partitions.csv            # Phân vùng Flash ứng dụng 3.5MB
│   ├── sdkconfig                 # Cấu hình ESP32-S3 240MHz & Flash 4MB
│   └── main/
│       ├── CMakeLists.txt        # Đăng ký component
│       ├── main.cpp              # Khởi chạy FreeRTOS đa nhân
│       ├── config/               # Cấu hình chân GPIO và tham số tác vụ
│       ├── diagnostics/          # Thăm dò phần cứng, kiểm toán heap, telemetry
│       ├── llm/                  # Lõi Transformer, sampler, trọng số INT8
│       └── web/                  # Trình điều khiển WiFi SoftAP & HTTP Server
│
├── web/                          # Ứng dụng Web Chat độc lập
│   ├── index.html                # Giao diện Dark Mode
│   ├── style.css                 # Bảng định kiểu giao diện
│   ├── app.js                    # Mã JavaScript client & cập nhật telemetry
│   ├── generate_web_header.py    # Script đóng gói web vào header C++
│   └── web_ui.h                  # File header chứa web lưu trên Flash DROM
│
└── esp32_ai_runtime/             # Sketch độc lập cho Arduino IDE
    └── esp32_ai_runtime.ino      # Triển khai một file duy nhất cho Arduino
```

---

## Hướng dẫn cài đặt và nạp firmware

### Cách 1: Sử dụng ESP-IDF (Khuyên dùng)

1. Mở terminal và chuyển vào thư mục firmware:
   ```bash
   cd firmware
   ```

2. Biên dịch dự án:
   ```bash
   idf.py build
   ```

3. Nạp vào mạch và mở Serial Monitor:
   ```bash
   idf.py -p COM5 flash monitor
   ```
   *(Thay `COM5` bằng cổng COM tương ứng trên máy tính của bạn).*

---

### Cách 2: Sử dụng Arduino IDE

1. Mở file `esp32_ai_runtime/esp32_ai_runtime.ino` trong Arduino IDE.
2. Trong menu **Tools > Board**, chọn **ESP32S3 Dev Module**.
3. Thiết lập các thông số sau:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
4. Nhấn nút **Upload**.

---

## Cách tương tác với AI

### 1. Qua trình duyệt Web (Điện thoại / Laptop)

1. Kết nối vào mạng WiFi do ESP32 phát ra:
   - **Tên WiFi (SSID)**: `ESP32-Local-AI`
   - **Mật khẩu**: `12345678`
2. Mở trình duyệt web bất kỳ và truy cập địa chỉ:
   ```text
   http://192.168.4.1
   ```
3. Nhập câu hỏi vào khung chat hoặc nhấn vào các gợi ý có sẵn.

### 2. Qua cổng USB Serial Terminal

Mở Serial Monitor ở tốc độ Baud `115200`. Gõ nội dung và nhấn Enter để xem AI sinh chữ trực tiếp theo thời gian thực:

```text
====================================================================
>>> [PROMPT] : tell me a joke
<<< [STREAM] : tell me a joke : A programmer goes to the grocery store. Wife says: 'Buy a carton of milk, and if they have eggs, buy ten.' He comes back with 10 cartons of milk!
--- [METRICS]: Tokens: 48 | Speed: 18.24 tok/s | Latency: 54.82 ms | Free SRAM: 229740 B
====================================================================
```

---

## Đóng góp cho dự án (Contributing)

Mọi đóng góp, báo lỗi (bug report), và đề xuất tính năng mới đều được hoan nghênh. Nếu bạn muốn đóng góp cho dự án:

1. **Fork Kho Chứa**: Nhấn nút **Fork** ở góc trên bên phải của repository trên GitHub.
2. **Tạo Nhánh Mới (Feature Branch)**:
   ```bash
   git checkout -b feature/TenTinhNangCuaBan
   ```
3. **Commit Các Thay Đổi**:
   ```bash
   git commit -m "feat: triển khai lượng tử hóa INT4 hoặc tính năng mới"
   ```
4. **Push Lên Nhánh Của Bạn**:
   ```bash
   git push origin feature/TenTinhNangCuaBan
   ```
5. **Tạo Pull Request (PR)**: Mở một Pull Request vào nhánh `main` kèm mô tả chi tiết các thay đổi và kết quả thử nghiệm trên phần cứng.

Đối với các thay đổi lớn về kiến trúc hoặc cấu trúc mô hình, vui lòng tạo một Issue trước để thảo luận. Xem chi tiết tại [CONTRIBUTING_VN.md](CONTRIBUTING_VN.md).

---

## Giấy phép nguồn mở

Dự án được phát hành theo giấy phép [MIT License](LICENSE).
