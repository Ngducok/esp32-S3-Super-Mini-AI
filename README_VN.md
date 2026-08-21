# Mô Hình Ngôn Ngữ Micro-Transformer Chạy Cục Bộ Trên ESP32-S3 Không Cần PSRAM Ngoài

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan Dự Án

Dự án này hiện thực hóa một mô hình ngôn ngữ tạo sinh tự hồi quy (Micro-Transformer) chạy hoàn toàn cục bộ 100% trên vi điều khiển ESP32-S3. Hệ thống xử lý trực tiếp trên phần cứng (bare-metal silicon) không phụ thuộc vào đám mây, kết nối Internet hay máy chủ API bên ngoài, sinh và truyền chuỗi token theo thời gian thực với tốc độ từ 9.33 đến 20.00 token/giây.

Toàn bộ hệ thống vận hành bên trong giới hạn 384 KB SRAM nội bộ của bo mạch ESP32-S3 Super Mini, hoàn toàn không yêu cầu bộ nhớ PSRAM mở rộng (0 KB PSRAM). Hệ thống tích hợp song song trạm phát WiFi độc lập (SoftAP) và máy chủ HTTP Web Server nhúng trực tiếp trong bộ nhớ Flash, phục vụ giao diện chat tương tác trực tiếp qua trình duyệt của điện thoại hoặc máy tính.

---

## 2. Bài Toán Đề Ra & Các Nút Thắt Vi Kiến Trúc Phần Cứng

Mô hình Transformer truyền thống là tác vụ bị giới hạn nghiêm ngặt bởi băng thông bộ nhớ và năng lực tính toán dấu phẩy động (Memory & Compute Bound). Trên các vi điều khiển nhúng giá rẻ như ESP32-S3 không có PSRAM, việc triển khai suy luận LLM gặp phải 4 nút thắt cổ chai kỹ thuật cốt lõi:

### 2.1. Phép Nhân Ma Trận - Vector (GEMV) Thô Sơ
- **Vấn đề**: Các thuật toán nhân ma trận thông thường sử dụng các vòng lặp `for` lồng nhau của C để tính tích vô hướng từng byte tuần tự. Trên kiến trúc vi xử lý Xtensa LX7 dual-issue, cách tiếp cận này gây ra hiện tượng tắc nghẽn đường ống lệnh (instruction pipeline stalls), nạp dữ liệu đơn lẻ từng byte từ bộ nhớ Flash DROM và không tận dụng được tập lệnh xử lý vector của phần cứng.

### 2.2. Quản Lý KV-Cache Tuyến Tính Khiến Hệ Thống Ngắt Quãng
- **Vấn đề**: Cấp phát bộ nhớ đệm Key-Value tĩnh từ vị trí 0 đến MAX_SEQ_LEN (ví dụ 64 hoặc 256 token). Khi quá trình hội thoại vượt quá dung lượng ngữ cảnh tối đa, hệ thống bị tràn bộ nhớ hoặc dừng hoàn toàn quá trình sinh token, buộc người dùng phải thiết lập lại (reset) đoạn chat từ đầu.

### 2.3. Lượng Tử Hóa Toàn Cục Làm Suy Giảm Dải Động
- **Vấn đề**: Phương pháp lượng tử hóa đối xứng INT8 toàn cục (Per-Tensor Symmetric INT8) tính toán một hệ số tỷ lệ duy nhất cho toàn bộ tensor. Khi ma trận xuất hiện các giá trị ngoại lai (outliers), dải biểu diễn của các trọng số còn lại bị thu hẹp đáng kể, đồng thời dung lượng trọng số 8-bit vẫn chiếm dụng đáng kể không gian Flash 4MB khi muốn mở rộng quy mô mô hình.

### 2.4. Hàm Phi Tuyến Tính (Softmax, GELU, SiLU, Exp) Tiêu Tốn Chu Kỳ CPU
- **Vấn đề**: Việc gọi trực tiếp các hàm toán học thư viện chuẩn C (`math.h`) như `expf()` và `tanhf()` tiêu tốn từ 100 đến 140 chu kỳ CPU cho mỗi lần gọi. Trong cơ chế Attention đa đầu và mạng nơ-ron truyền thẳng (FFN), các hàm này được triệu gọi hàng ngàn lần cho mỗi token, trở thành điểm nghẽn độ trễ lớn.

---

## 3. Cách Thức Xử Lý & Giải Pháp Kiến Trúc Phần Cứng

Dự án áp dụng các kỹ thuật tối ưu hóa phần cứng ở cấp độ vi kiến trúc (micro-architecture) để giải quyết triệt để 4 nút thắt trên:

```
[Dữ liệu đầu vào] ──► [Khớp chuỗi Token] ──► [Sliding Window Ring-Buffer KV-Cache]
                                                        │
         ┌──────────────────────────────────────────────┴──────────────────────────────┐
         ▼                                                                             ▼
  [SIMD 128-bit GEMV Kernel]                                              [Fast Math 512-Entry LUT]
  • 32-bit chunked word loads                                             • fast_expf() [1-3 cycles]
  • 16-way unrolling (4 accumulators)                                     • fast_gelu() & fast_silu()
  • Group-32 INT4 & BitNet 1.58b                                          • fast_softmax() single-pass
         │                                                                             │
         └──────────────────────────────────────┬──────────────────────────────────────┘
                                                ▼
                             [Bộ Lấy Mẫu Argmax / Temperature]
                                                │
                               ┌────────────────┴────────────────┐
                               ▼                                 ▼
                     [USB Serial Streaming]             [HTTP Server / Web UI]
```

### 3.1. Vectorized SIMD GEMV Kernel (`simd_ops.h`)
- **Tải song song 32-bit (Chunked Loads)**: Sử dụng con trỏ kiểu word 32-bit (`uint32_t`) để nạp đồng thời 4 cặp số `int8` trong một chu kỳ CPU duy nhất.
- **Mở rộng vòng lặp 16-way đa thanh ghi tích lũy**: Phân rã phép tính tích vô hướng thành 4 nhánh tích lũy độc lập (`acc0`, `acc1`, `acc2`, `acc3`). Thiết kế này triệt tiêu hoàn toàn độ trễ phụ thuộc dữ liệu (data dependency latency) giữa các lệnh nhân-cộng kế tiếp, giúp khai thác tối đa năng lực song song của 2 đường ống lệnh (dual-issue pipeline) trên lõi Xtensa LX7.

### 3.2. Bộ Đệm Vòng Trượt Sliding Window Ring-Buffer & Dynamic RoPE
- **Cơ chế Ring-Buffer O(1) Bộ Nhớ**: Cố định vùng nhớ KV-Cache ở mức 24.5 KB trong SRAM nội bộ. Khi số lượng token vượt quá giới hạn MAX_SEQ_LEN, token mới nhất tại vị trí pos sẽ tự động ghi đè lên vị trí cũ nhất theo chỉ số `pos % MAX_SEQ_LEN`.
- **Ánh xạ khoảng cách tương đối & RoPE**: Vòng lặp Attention ánh xạ chỉ số thời gian tương đối vào vị trí vật lý trong Ring Buffer và áp dụng phép xoay tọa độ tương đối (Rotary Position Embedding). Cơ chế này cho phép hệ thống hội thoại liên tục vô hạn mà không bao giờ bị tràn bộ nhớ hay crash.

### 3.3. Lượng Tử Hóa Phân Nhóm INT4 (Group Size 32) & BitNet 1.58b (`microquant/`)
- **Group-wise INT4 (Group 32)**: Chia ma trận thành các khối 32 phần tử, mỗi khối sở hữu một hệ số tỷ lệ cục bộ `scale_group`. Giải pháp này triệt tiêu ảnh hưởng của giá trị ngoại lai, đạt tỷ lệ nén 7.7x (giảm 50% dung lượng Flash so với INT8) với độ chính xác bảo toàn cực cao.
- **BitNet 1.58b Core**: Đóng gói 4 trọng số ternary {-1, 0, +1} trên mỗi byte, biến toàn bộ phép nhân trong khối tính toán thành các lệnh cộng và trừ trực tiếp trên ALU.

### 3.4. Bảng Tra Cứu Flash DROM Fast Math LUT (`fast_math.h`)
- **Bảng tra cứu 512 phần tử**: Toàn bộ giá trị của `expf(x)` trên miền [-16.0, 0.0] và `gelu(x)` trên miền [-4.0, 4.0] được tiền tính toán và lưu cố định trong Flash DROM (zero SRAM consumption).
- **Nội suy tuyến tính siêu tốc**: Thay thế phép tính chuỗi phức tạp bằng một phép tính chỉ số mảng và nội suy tuyến tính, đưa thời gian thực thi từ 120 chu kỳ CPU xuống chỉ còn 1 đến 3 chu kỳ CPU với sai số tuyệt đối < 7.9e-5.

---

## 4. Kết Quả Thực Nghiệm & Thông Số Đo Đạc Sau Khi Chạy Thử

Các số liệu dưới đây được đo đạc trực tiếp từ quá trình thực thi trên bo mạch phần cứng ESP32-S3:

### 4.1. Thông Số Tổng Thể Hệ Thống

| Thông số kỹ thuật | Giá trị thực nghiệm đo đạc | Ghi chú kỹ thuật |
| :--- | :--- | :--- |
| **Phần cứng mục tiêu** | **ESP32-S3 Super Mini (Silicon Rev v0.2)** | 2 nhân Xtensa LX7 @ 240 MHz |
| **Yêu cầu PSRAM ngoài** | **0 KB (Không dùng PSRAM ngoài)** | Tương thích 100% mọi bo mạch ESP32-S3 |
| **Kích thước mô hình** | **118,784 Tham số (3 Layers, d=64, 4 Heads)** | Transformer Decoder tự hồi quy |
| **Kích thước file nhị phân Flash**| **1.44 MB** *(Phân vùng app: 3.5 MB)* | Nằm trọn vẹn trong chip Flash 4MB |
| **Dung lượng KV-Cache** | **24.5 KB mảng tĩnh trong SRAM** | 2 x 3 x 64 x 64 bytes |
| **SRAM trống khi vận hành** | **> 210 KB SRAM nội bộ** | Dành cho mạng WiFi SoftAP và TCP/IP |
| **Độ trôi bộ nhớ (Memory Leak)** | **0 Byte (Zero Leak sau > 24h chạy liên tục)** | Tuyệt đối không gọi malloc/free trong loop |
| **Tốc độ sinh token** | **9.33 – 20.00 token/giây** | Đạt được nhờ SIMD GEMV và Fast Math |
| **Độ trễ mỗi token** | **~50 ms – 107 ms / token** | Phản hồi mượt mà trong thời gian thực |
| **Thời gian khởi động toàn bộ** | **< 1.5 giây** | Khởi động SoftAP, Web Server & Model |

### 4.2. Benchmark Tối Ưu Hóa Vi Kiến Trúc Phần Cứng

Số liệu thu được từ module đo lường `HardwareProbe::runCPUBenchmark()` trực tiếp trên CPU 240 MHz:

| Tác vụ kiểm thử | Thuật toán chuẩn (Baseline) | Tối ưu hóa Vi kiến trúc (dev) | Tăng tốc (Speedup) |
| :--- | :--- | :--- | :--- |
| **64x64 INT8 GEMV** | 128.40 us/op (Vòng lặp C chuẩn) | **53.50 us/op (SIMD 16-way Unrolled)** | **2.40x nhanh hơn** |
| **Hàm mũ Exp (Softmax)** | 145.2 ns/call (libc expf()) | **8.6 ns/call (Fast Math LUT)** | **16.88x nhanh hơn** |
| **Context Handling** | Crash / Ngắt khi pos >= 64 | **Vòng trượt vô hạn (0 Crash)** | **Vô hạn token** |

### 4.3. Kiểm Định Độ Chính Xác Lượng Tử Hóa Toán Học (`MicroQuant`)

Kết quả thu được từ bộ kiểm thử toán học `test_quant_math.py`:

| Định dạng lượng tử | Tỷ lệ nén | Độ tương đồng Cosine | SQNR (Tỷ số tín hiệu trên nhiễu) | Đánh giá |
| :--- | :--- | :--- | :--- | :--- |
| **INT8 Toàn Cục** | 4.0x | **99.996%** | **40.95 dB** | Độ trung thực tuyệt đối |
| **INT4 Per-Tensor** | 8.0x | **98.720%** | **15.80 dB** | Giảm 50% RAM so với INT8 |
| **INT4 Group-wise (G32)** | **7.7x** | **99.529%** | **20.23 dB** | Tối ưu xuất sắc cho Flash 4MB |
| **BitNet 1.58b** | **16.0x** | **88.592%** | **5.77 dB** | Hoàn toàn không dùng phép nhân |
| **Fast Exp LUT** | - | - | Sai số tuyệt đối tối đa: 7.93e-5 | Chính xác cao |
| **Fast Softmax** | - | - | Sai số tuyệt đối tối đa: 2.55e-5 | Chính xác cao |

---

## 5. Bảng So Sánh Với Các Dự Án Mã Nguồn Mở Khác

| Khía cạnh kỹ thuật | Dự Án Này (ESP32-S3 Micro-LLM) | slvDev/esp32-ai | karpathy/llama2.c |
| :--- | :--- | :--- | :--- |
| **Bộ nhớ PSRAM yêu cầu** | **0 KB (Không cần PSRAM ngoài)** | **Bắt buộc 8 MB Octal PSRAM** | Thường cần PSRAM ngoài |
| **Dung lượng Flash yêu cầu** | **4 MB Flash** | **Bắt buộc 16 MB Flash** | Phụ thuộc kích thước file model |
| **Chi phí phần cứng** | **~70.000 – 90.000 VNĐ** | ~130.000 – 160.000 VNĐ | Tùy bo mạch |
| **Giao diện tương tác** | **WiFi Hotspot Web UI + Serial** | Màn hình LCD SPI | Console Terminal |
| **Cơ chế suy luận** | **Tự hồi quy thời gian thực** | Chỉ sinh chuỗi văn bản mẫu | Tự hồi quy |
| **Quản lý KV-Cache** | **Sliding Window Ring-Buffer 24.5 KB**| Cấp phát động trong PSRAM | Cấp phát động |
| **Tối ưu hóa vi kiến trúc** | **SIMD GEMV + Fast Math LUT** | Phép nhân chuẩn | Tùy trình biên dịch |

---

## 6. Cấu Trúc Thư Mục

```text
esp32/
├── .gitignore                    # Cấu hình bỏ qua file build và cache
├── LICENSE                       # Giấy phép nguồn mở MIT
├── README.md                     # Tài liệu kỹ thuật tiếng Anh (Chuẩn kỹ thuật)
├── README_VN.md                  # Tài liệu kỹ thuật tiếng Việt (Chuẩn kỹ thuật)
├── results.md                    # Báo cáo thực nghiệm tiếng Anh
├── results_VN.md                 # Báo cáo thực nghiệm tiếng Việt
│
├── firmware/                     # Dự án C++ ESP-IDF chuẩn công nghiệp
│   ├── CMakeLists.txt            # Cấu hình build gốc
│   ├── partitions.csv            # Bảng phân vùng Flash ứng dụng 3.5MB
│   ├── sdkconfig                 # Cấu hình ESP32-S3 240MHz & Flash 4MB
│   └── main/
│       ├── CMakeLists.txt        # Đăng ký component
│       ├── main.cpp              # Khởi chạy đa nhân FreeRTOS & Chat Task
│       ├── config/               # Cấu hình phần cứng và tham số tác vụ
│       ├── diagnostics/          # Thăm dò phần cứng, kiểm toán heap, micro-benchmark
│       ├── llm/
│       │   ├── fast_math.h       # Bảng tra cứu LUT (Exp, GELU, SiLU, Softmax)
│       │   ├── simd_ops.h        # Kernel SIMD GEMV 32-bit chunking & RoPE
│       │   ├── transformer.h     # Lớp Transformer Decoder
│       │   ├── transformer.cpp   # Sliding Window Ring-Buffer KV-Cache logic
│       │   ├── generator.cpp     # Vòng lặp sinh token tự hồi quy liên tục
│       │   ├── sampler.cpp       # Lấy mẫu phân phối xác suất
│       │   └── model_llm_weights.h # Trọng số INT8 lưu trong Flash DROM
│       └── web/                  # Trình điều khiển WiFi SoftAP & HTTP Server
│
├── microquant/                   # Lõi nén và lượng tử hóa MicroQuant-ESP32
│   ├── include/
│   │   ├── MicroQuant.h          # Header chính của thư viện MicroQuant
│   │   └── kernels/              # Kernel SIMD INT8, Group-32 INT4, BitNet
│   ├── python/microquant/        # Bộ công cụ Python (quantizer, validator, exporter)
│   └── tests/
│       └── test_quant_math.py    # Bộ kiểm thử toán học tự động
│
├── web/                          # Ứng dụng Web Chat độc lập nhúng Flash
│   ├── index.html                # Giao diện Dark Mode
│   ├── style.css                 # Bảng định kiểu CSS3
│   ├── app.js                    # Mã JavaScript client và telemetry
│   └── web_ui.h                  # Header C++ chứa web bundle lưu Flash DROM
│
└── esp32_ai_runtime/             # Sketch đơn file cho Arduino IDE
    └── esp32_ai_runtime.ino      # Triển khai độc lập cho Arduino
```

---

## 7. Hướng Dẫn Biên Dịch & Nạp Firmware

### 7.1. Sử dụng ESP-IDF (Khuyên dùng trong môi trường sản xuất)

1. Mở terminal và chuyển vào thư mục `firmware`:
   ```bash
   cd firmware
   ```

2. Biên dịch firmware:
   ```bash
   idf.py build
   ```

3. Nạp firmware vào vi điều khiển và mở cổng Serial Monitor:
   ```bash
   idf.py -p COM5 flash monitor
   ```
   *(Thay `COM5` bằng cổng COM thực tế của thiết bị).*

---

### 7.2. Sử dụng Arduino IDE

1. Mở file `esp32_ai_runtime/esp32_ai_runtime.ino` trong Arduino IDE.
2. Trong menu **Tools > Board**, chọn **ESP32S3 Dev Module**.
3. Cấu hình các thông số:
   - **Flash Size**: 4MB
   - **Partition Scheme**: Huge APP (3MB No OTA / 1MB SPIFFS)
   - **PSRAM**: Disabled
   - **CPU Frequency**: 240MHz
4. Nhấn nút **Upload**.

---

## 8. Hướng Dẫn Tương Tác & Sử Dụng

### 8.1. Tương tác qua Giao diện Web (Điện thoại hoặc Máy tính)

1. Dùng điện thoại hoặc máy tính quét và kết nối vào mạng WiFi do ESP32 phát ra:
   - **Tên WiFi (SSID)**: `ESP32-Local-AI`
   - **Mật khẩu**: `12345678`
2. Mở trình duyệt web và truy cập địa chỉ IP:
   ```text
   http://192.168.4.1
   ```
3. Nhập câu hỏi vào khung chat để nhận phản hồi theo thời gian thực kèm thông số đo đạc phần cứng (độ trễ, tốc độ sinh token, dung lượng SRAM khả dụng).

### 8.2. Tương tác qua Cổng USB Serial Terminal

Mở terminal Serial Monitor ở tốc độ Baud `115200`. Nhập văn bản bất kỳ và nhấn Enter để theo dõi quá trình sinh chuỗi token tự hồi quy trực tiếp từ nhân CPU:

```text
====================================================================
>>> [PROMPT] : What is your current operational status?
<<< [STREAM] : System status : CPU at 240 MHz with 380 KB free internal SRAM . All diagnostic protocols operational , zero memory leak .
--- [METRICS]: Tokens: 24 | Speed: 14.50 tok/s | Latency: 1655.17 ms | Free SRAM: 215432 B
====================================================================
```
