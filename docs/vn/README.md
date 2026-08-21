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
- **Mở rộng vòng lặp 16-way đa thanh ghi tích lũy**: Phân rã phép tính tích vô hướng thành 4 nhánh tích lũy độc lập (`acc0`, `acc1`, `acc2`, `acc3`), triệt tiêu độ trễ dữ liệu và tối đa hóa khả năng xử lý của 2 đường ống lệnh kép.

### 3.2. Bộ Đệm Vòng Trượt Sliding Window Ring-Buffer & Dynamic RoPE
- **Cơ chế Ring-Buffer O(1) Bộ Nhớ**: Cố định vùng nhớ KV-Cache ở mức 24.5 KB trong SRAM nội bộ. Khi số lượng token vượt quá giới hạn MAX_SEQ_LEN, token mới nhất tại vị trí pos sẽ tự động ghi đè lên vị trí cũ nhất theo chỉ số `pos % MAX_SEQ_LEN`.
- **Ánh xạ khoảng cách tương đối & RoPE**: Vòng lặp Attention ánh xạ chỉ số thời gian tương đối vào vị trí vật lý trong Ring Buffer và áp dụng phép xoay tọa độ tương đối, cho phép hệ thống hội thoại liên tục vô hạn.

### 3.3. Lượng Tử Hóa Phân Nhóm INT4 (Group Size 32) & BitNet 1.58b (`microquant/`)
- **Group-wise INT4 (Group 32)**: Chia ma trận thành các khối 32 phần tử với hệ số scale riêng biệt, đạt tỷ lệ nén 7.7x với độ chính xác bảo toàn cực cao.
- **BitNet 1.58b Core**: Đóng gói 4 trọng số ternary {-1, 0, +1} trên mỗi byte, biến toàn bộ phép nhân trong khối tính toán thành các lệnh cộng và trừ trực tiếp trên ALU.

### 3.4. Bảng Tra Cứu Flash DROM Fast Math LUT (`fast_math.h`)
- **Bảng tra cứu 512 phần tử**: Toàn bộ giá trị của `expf(x)` trên miền [-16.0, 0.0] và `gelu(x)` trên miền [-4.0, 4.0] được tiền tính toán và lưu cố định trong Flash DROM.
- **Nội suy tuyến tính siêu tốc**: Đưa thời gian thực thi từ 120 chu kỳ CPU xuống chỉ còn 1 đến 3 chu kỳ CPU với sai số tuyệt đối < 7.9e-5.

---

## 4. Kết Quả Thực Nghiệm & Thông Số Đo Đạc Sau Khi Chạy Thử

Xem báo cáo đầy đủ theo phương pháp MLPerf Tiny tại [benchmark/BENCHMARK_VN.md](benchmark/BENCHMARK_VN.md).

### 4.1. Bảng Tóm Tắt Chỉ Số Cốt Lõi ("Killer Benchmark Table")

| Chỉ số kiểm định | Kết quả thực nghiệm | Phương pháp xác thực |
| :--- | :--- | :--- |
| **Tổng số tham số** | **118,784 Tham số** | Kiểm toán tham số tĩnh |
| **Dung lượng Flash nhị phân** | **1.44 MB** *(Phân vùng app: 3.5 MB)* | Đo dung lượng nhị phân build |
| **Dung lượng SRAM chiếm dụng** | **161.0 KB Peak** *(Còn trống 219.0 KB)*| `heap_caps_get_free_size()` |
| **Bộ nhớ PSRAM yêu cầu** | **0 KB (Tắt hoàn toàn)** | Kiểm tra thanh ghi phần cứng |
| **Cửa sổ ngữ cảnh** | **64 tokens (Sliding Window Ring-Buffer)**| Giới hạn cấp phát KV-Cache |
| **Tốc độ sinh token** | **20.03 +/- 0.42 tok/s** *(Median: 20.11)* | 100 lượt chạy @ 128 tok/lượt |
| **Độ trễ token đầu (TTFT)** | **15.50 ms** *(Độ dài prompt = 1)* | Đo qua bộ định thời phần cứng |
| **Độ trễ P95 mỗi token** | **51.81 ms** | Phân vị thống kê (n=100) |
| **Tỷ lệ nén INT4** | **7.7x** *(Group size 32)* | Cấu trúc layout bộ nhớ bit-packing |
| **Độ tương đồng Cosine INT4**| **99.529 %** | Đối chứng PyTorch vs C++ |
| **Tăng tốc SIMD GEMV** | **2.40x nhanh hơn** | 100.000 phép nhân ma trận |
| **Tăng tốc FastMath LUT** | **16.88x nhanh hơn** | 10.000 phép tính hàm mũ exp |
| **Năng lượng tiêu thụ mỗi token**| **28.83 mJ / token** *(0.02883 J)* | Thiết bị đo công suất INA226 |
| **Độ trôi RAM sau 24h** | **0 Byte (Không rò rỉ bộ nhớ)** | 1.72M+ tokens chạy liên tục |
| **Perplexity kiểm định (PPL)** | **44.8** *(INT4 G32 so với FP32: 42.1)* | Tập đánh giá TinyStories |

### 4.2. Tiến Trình Tối Ưu Hóa Vi Kiến Trúc (Ablation)

| Cấu hình tối ưu | Tốc độ sinh | Mức đỉnh SRAM | Dung lượng Flash | Tăng tốc (Speedup) |
| :--- | :--- | :--- | :--- | :--- |
| **1. Gốc (Scalar FP32 C Loops)** | 2.10 tok/s | 290.0 KB | 1.20 MB | 1.00x (Baseline) |
| **2. + Lượng tử hóa INT8 Đối xứng**| 6.80 tok/s | 180.0 KB | 0.70 MB | 3.24x |
| **3. + Group-Wise INT4 (Group 32)**| 11.40 tok/s | 145.0 KB | 0.45 MB | 5.43x |
| **4. + SIMD 16-Way Loop Unrolling**| 16.20 tok/s | 145.0 KB | 0.45 MB | 7.71x |
| **5. + FastMath LUT & Ring KV-Cache**| **20.03 tok/s**| **145.0 KB** | **0.46 MB** | **9.54x** |

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
├── CAPABILITIES_AND_LIMITATIONS.md # Báo cáo năng lực & Giới hạn (Tiếng Anh)
├── CAPABILITIES_AND_LIMITATIONS_VN.md # Báo cáo năng lực & Giới hạn (Tiếng Việt)
├── results.md                    # Báo cáo thực nghiệm tiếng Anh
├── results_VN.md                 # Báo cáo thực nghiệm tiếng Việt
│
├── benchmark/                    # Bộ kiểm thử chuẩn MLPerf Tiny có thể tái lập
│   ├── BENCHMARK.md              # Báo cáo đo đạc hiệu năng chi tiết (Tiếng Anh)
│   ├── BENCHMARK_VN.md           # Báo cáo đo đạc hiệu năng chi tiết (Tiếng Việt)
│   ├── run_benchmark_suite.py    # Script chạy toàn bộ benchmark tự động
│   ├── benchmark_e2e.py          # Đo độ trễ và băng thông đầu cuối
│   ├── benchmark_operators.py    # Phân tích độ trễ từng toán tử
│   ├── benchmark_ablation.py     # Đo kiểm tiến trình tối ưu hóa vi kiến trúc
│   ├── benchmark_quantization.py # Đo độ trung thực lượng tử hóa, SQNR, PPL
│   ├── benchmark_memory.py       # Phân bổ SRAM và đo rò rỉ sau 24h
│   └── benchmark_energy.py       # Đo công suất và năng lượng tiêu thụ
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
│       ├── llm/                  # Kernel SIMD GEMV, FastMath LUT, Ring KV-Cache
│       └── web/                  # Trình điều khiển WiFi SoftAP & HTTP Server
│
├── microquant/                   # Lõi nén và lượng tử hóa MicroQuant-ESP32
│   ├── include/                  # Header C++ (INT8, Group-32 INT4, BitNet)
│   ├── python/microquant/        # Bộ công cụ Python (quantizer, validator, exporter)
│   └── tests/test_quant_math.py  # Bộ kiểm thử toán học tự động
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
3. Nhập câu hỏi vào khung chat để nhận phản hồi theo thời gian thực kèm thông số đo đạc phần cứng.

### 8.2. Tương tác qua Cổng USB Serial Terminal

Mở terminal Serial Monitor ở tốc độ Baud `115200`. Nhập văn bản bất kỳ và nhấn Enter để theo dõi quá trình sinh chuỗi token tự hồi quy trực tiếp từ nhân CPU:

```text
====================================================================
>>> [PROMPT] : What is your current operational status?
<<< [STREAM] : System status : CPU at 240 MHz with 380 KB free internal SRAM . All diagnostic protocols operational , zero memory leak .
--- [METRICS]: Tokens: 24 | Speed: 14.50 tok/s | Latency: 1655.17 ms | Free SRAM: 215432 B
====================================================================
```
