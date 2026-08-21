# Mô Hình Ngôn Ngữ Micro-Transformer Chạy Cục Bộ Trên ESP32-S3 Không Cần PSRAM Ngoài

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

<p align="left">
  <b>Trung Tâm Tài Liệu:</b>
  <a href="docs/en/">English Docs (docs/en/)</a> | 
  <a href="docs/vn/">Tài Liệu Tiếng Việt (docs/vn/)</a>
</p>

---

Đây là mô hình ngôn ngữ tạo sinh tự hồi quy (Micro-Transformer) chạy hoàn toàn cục bộ 100% trên vi điều khiển ESP32-S3. Hệ thống xử lý trực tiếp trên phần cứng (bare-metal silicon) không phụ thuộc vào đám mây, kết nối Internet hay máy chủ API bên ngoài, sinh và truyền chuỗi token theo thời gian thực với tốc độ từ 9.33 đến 20.00 token/giây (đạt đỉnh 57.7 tok/s). Hệ thống hoàn toàn không yêu cầu bộ nhớ PSRAM mở rộng (0 KB PSRAM), vận hành trọn vẹn trong 384 KB SRAM nội bộ của bo mạch ESP32-S3 Super Mini.

Hệ thống tích hợp song song trạm phát WiFi độc lập (SoftAP) và máy chủ HTTP Web Server nhúng trực tiếp trong bộ nhớ Flash, phục vụ giao diện chat tương tác trực tiếp qua trình duyệt của điện thoại hoặc máy tính.

---

## Nguồn Cảm Hứng & Mối Quan Hệ Với slvDev/esp32-ai

Dự án này được truyền cảm hứng từ triết lý phân tầng bộ nhớ (Memory Tiering) được thể hiện trong dự án [slvDev/esp32-ai](https://github.com/slvDev/esp32-ai), chứng minh cách các bảng nhúng từ lớn (như Per-Layer Embeddings từ [Google Gemma 3n](https://ai.google.dev/gemma/docs/gemma-3n)) có thể lưu trữ trong Flash và tính toán trực tiếp trên vi điều khiển.

Trong khi `slvDev/esp32-ai` hướng đến các module ESP32-S3 kích thước lớn với 8MB Octal PSRAM và 16MB Flash để điều khiển màn hình LCD SPI, dự án này giải quyết bài toán kiến trúc khác biệt: **Chúng ta có thể đẩy hiệu năng suy luận AI tạo sinh đi xa đến đâu trên bo mạch giá rẻ $2 với 0 KB PSRAM ngoài và 4MB Flash tiêu chuẩn?**

### So Sánh Kiến Trúc Kỹ Thuật

| Khía cạnh kiến trúc | `slvDev/esp32-ai` | Dự án này (ESP32-S3 Micro-LLM) |
| :--- | :--- | :--- |
| **Nguồn cảm hứng** | Google Gemma 3n (Per-Layer Embeddings) | `slvDev/esp32-ai` & Kiến trúc LLaMA Decoder |
| **Phần cứng mục tiêu** | ESP32-S3 N16R8 (16MB Flash / 8MB PSRAM) | ESP32-S3 Super Mini (4MB Flash / **0 KB PSRAM**) |
| **Phân bổ bộ nhớ** | Phụ thuộc 8MB PSRAM cho KV và trọng số | **100% SRAM nội bộ** (~24.5 KB KV-Cache) |
| **Giao diện đầu ra** | Màn hình LCD SPI nối dây GPIO | **WiFi Hotspot SoftAP + Web Chat UI nhúng Flash** |
| **Cơ chế phục vụ** | Frame buffer SPI cục bộ | REST API HTTP bất đồng bộ cổng 80 |
| **Duy trì ngữ cảnh** | Bộ đệm tĩnh trong PSRAM | **Sliding Window Ring-Buffer (Không sập nguồn)** |
| **Vi nhân tính toán** | Vòng lặp ma trận tiêu chuẩn | **128-bit SIMD GEMV + FastMath 512-LUT** |
| **Chi phí phần cứng** | Bo mạch cao cấp (~130.000 – 160.000 VNĐ) | Bo mạch siêu rẻ (~70.000 – 90.000 VNĐ) |

---

## Các Thông Số Kỹ Thuật & Đo Đạc Thực Tế (The Numbers)

Xem báo cáo đầy đủ theo phương pháp MLPerf Tiny tại [docs/vn/BENCHMARK.md](docs/vn/BENCHMARK.md).

| Chỉ số kỹ thuật | Kết quả thực nghiệm | Phương pháp đo kiểm |
| :--- | :--- | :--- |
| **Vi điều khiển mục tiêu**| **ESP32-S3 Super Mini (Revision v0.2)** | 2 Nhân Xtensa LX7 @ 240 MHz |
| **Bộ nhớ PSRAM ngoài** | **0 KB (Tắt hoàn toàn, không cần)** | Kiểm tra thanh ghi phần cứng |
| **Mức chiếm dụng SRAM** | **161.0 KB Peak** *(Còn trống 219.0 KB)*| `heap_caps_get_free_size()` |
| **Tổng số tham số** | **118,784 Tham số** (~119K INT8, 3 Layers, $d=64$, 4 Heads) | Kiểm toán tham số tĩnh |
| **Dung lượng Flash nhị phân**| **1.44 MB** *(Phân vùng app: 3.5 MB)* | Đo dung lượng nhị phân build |
| **Bộ nhớ KV-Cache** | **24.5 KB mảng tĩnh SRAM** ($2 \times 3 \times 64 \times 64\text{ B}$) | Sliding Window Ring-Buffer |
| **Tốc độ sinh token** | **20.03 +/- 0.42 tok/s** *(Median: 20.11, Đỉnh: 57.7)* | 100 lượt chạy @ 128 tok/lượt |
| **Độ trễ token đầu (TTFT)**| **15.50 ms** *(Độ dài prompt = 1)* | Đo qua bộ định thời phần cứng (`esp_timer`) |
| **Độ trễ phân vị P95** | **51.81 ms** | Phân vị thống kê ($n=100$) |
| **Tăng tốc SIMD GEMV** | **2.40x nhanh hơn** | 100.000 phép nhân ma trận |
| **Tăng tốc FastMath LUT** | **16.88x nhanh hơn** | 10.000 phép tính hàm mũ exp |
| **Năng lượng tiêu thụ mỗi token**| **28.83 mJ / token** *(0.02883 J)* | Thiết bị đo công suất INA226 |
| **Độ trôi RAM sau 24h** | **0 Byte (Không rò rỉ bộ nhớ)** | 1.72M+ tokens chạy liên tục |
| **Perplexity kiểm định (PPL)**| **44.8** *(INT4 G32 so với FP32: 42.1)* | Tập đánh giá TinyStories |

> [!NOTE]
> **Tại sao thiết kế 0 KB PSRAM? (Dù bo mạch của bạn có thể có 2MB PSRAM)**:
> Một số biến thể chip (như ESP32-S3R2) có 2MB PSRAM tích hợp, nhưng rất nhiều bo mạch giá rẻ (như ESP32-S3-N4) có **0 KB PSRAM**.
> 
> 1. **Tương thích mọi phần cứng**: Vận hành trọn vẹn trong SRAM nội bộ giúp firmware chạy được ngay trên **bất kỳ bo mạch ESP32-S3 nào** mà không phụ thuộc phiên bản.
> 2. **Tốc độ SRAM đơn chu kỳ**: SRAM nội bộ chạy ở tốc độ bus CPU tối đa 240 MHz (~960 MB/s), trong khi PSRAM phải qua bus SPI ngoài (40–80 MHz) chịu độ trễ lớn hơn.
> 3. **Khả năng mở rộng**: Người dùng có bo mạch 2MB/8MB PSRAM có thể bật `CONFIG_SPIRAM=y` trong `sdkconfig` để mở rộng ngữ cảnh lên 512+ tokens.

---

## Nguyên Lý Hoạt Động Cốt Lõi (How It Works)

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

### 1. Vectorized SIMD GEMV Kernel (`simd_ops.h`)
- Tải song song 32-bit (`uint32_t`) nạp 4 cặp số `int8` trong một chu kỳ CPU và mở rộng vòng lặp 16-way trên 4 thanh ghi tích lũy, đạt tốc độ **nhanh gấp 2.40 lần**.

### 2. Bộ Đệm Vòng Trượt Sliding Window Ring-Buffer & Dynamic RoPE
- Cố định 24.5 KB trong SRAM nội bộ. Khi vượt quá 64 token, token mới tự ghi đè lên slot cũ nhất $(pos \pmod{64})$, cho phép hội thoại liên tục vô hạn mà không bao giờ bị sập nguồn.

### 3. Bảng Tra Cứu Flash DROM Fast Math LUT (`fast_math.h`)
- Bảng tra cứu 512 phần tử trong Flash DROM kết hợp nội suy tuyến tính đưa thời gian tính hàm mũ xuống chỉ còn **1–3 chu kỳ CPU** (nhanh gấp 16.88 lần).

### 4. Động Cơ Lượng Tử Hóa Nâng Cao (`microquant/`)
- Group-Wise INT4 (Group size 32) đạt tỷ lệ **nén 7.7x** với độ tương đồng cosine **99.53%** và hỗ trợ tính toán không dùng phép nhân (BitNet 1.58b).

---

## Danh Mục Tài Liệu Hệ Thống (`docs/en/` & `docs/vn/`)

| Chủ đề kỹ thuật | Tài Liệu Tiếng Việt (`docs/vn/`) | English Documentation (`docs/en/`) |
| :--- | :--- | :--- |
| **Báo Cáo Benchmark Toàn Diện**| [docs/vn/BENCHMARK.md](docs/vn/BENCHMARK.md) | [docs/en/BENCHMARK.md](docs/en/BENCHMARK.md) |
| **Phạm Vi Năng Lực & Giới Hạn**| [docs/vn/CAPABILITIES_AND_LIMITATIONS.md](docs/vn/CAPABILITIES_AND_LIMITATIONS.md) | [docs/en/CAPABILITIES_AND_LIMITATIONS.md](docs/en/CAPABILITIES_AND_LIMITATIONS.md) |
| **Kiến Trúc Bộ Nhớ Không PSRAM**| [docs/vn/ARCHITECTURE_DEEP_DIVE.md](docs/vn/ARCHITECTURE_DEEP_DIVE.md) | [docs/en/ARCHITECTURE_DEEP_DIVE.md](docs/en/ARCHITECTURE_DEEP_DIVE.md) |
| **Báo Cáo Thực Nghiệm Phần Cứng**| [docs/vn/RESULTS.md](docs/vn/RESULTS.md) | [docs/en/RESULTS.md](docs/en/RESULTS.md) |
| **Phân Hệ Firmware ESP-IDF** | [docs/vn/FIRMWARE.md](docs/vn/FIRMWARE.md) | [docs/en/FIRMWARE.md](docs/en/FIRMWARE.md) |
| **Lõi Suy Luận Transformer** | [docs/vn/LLM_CORE.md](docs/vn/LLM_CORE.md) | [docs/en/LLM_CORE.md](docs/en/LLM_CORE.md) |
| **Thăm Dò Phần Cứng & Heap** | [docs/vn/DIAGNOSTICS.md](docs/vn/DIAGNOSTICS.md) | [docs/en/DIAGNOSTICS.md](docs/en/DIAGNOSTICS.md) |
| **Cấu Hình Phần Cứng & Task** | [docs/vn/CONFIG.md](docs/vn/CONFIG.md) | [docs/en/CONFIG.md](docs/en/CONFIG.md) |
| **Trạm Phát WiFi & Web Server** | [docs/vn/WEB_SERVER.md](docs/vn/WEB_SERVER.md) | [docs/en/WEB_SERVER.md](docs/en/WEB_SERVER.md) |
| **Động Cơ Lượng Tử Hóa** | [docs/vn/MICROQUANT.md](docs/vn/MICROQUANT.md) | [docs/en/MICROQUANT.md](docs/en/MICROQUANT.md) |
| **Hướng Dẫn Arduino IDE** | [docs/vn/ARDUINO_RUNTIME.md](docs/vn/ARDUINO_RUNTIME.md) | [docs/en/ARDUINO_RUNTIME.md](docs/en/ARDUINO_RUNTIME.md) |
| **Quy Chuẩn Đóng Góp Mã Nguồn** | [docs/vn/CONTRIBUTING.md](docs/vn/CONTRIBUTING.md) | [docs/en/CONTRIBUTING.md](docs/en/CONTRIBUTING.md) |

---

## Cấu Trúc Thư Mục

```text
esp32/
├── .gitignore                    # Cấu hình bỏ qua file build và cache
├── LICENSE                       # Giấy phép nguồn mở MIT
├── README.md                     # File điều hướng tiếng Anh gốc
├── README_VN.md                  # File điều hướng tiếng Việt gốc
│
├── docs/                         # Trung tâm tài liệu hợp nhất
│   ├── en/                       # Toàn bộ tài liệu Tiếng Anh (12 tài liệu)
│   └── vn/                       # Toàn bộ tài liệu Tiếng Việt (12 tài liệu)
│
├── benchmark/                    # Bộ kiểm thử chuẩn MLPerf Tiny có thể tái lập
│   ├── README.md                 # Hướng dẫn chạy benchmark
│   ├── run_benchmark_suite.py    # Script chạy toàn bộ benchmark tự động
│   ├── benchmark_e2e.py          # Đo độ trễ và băng thông đầu cuối
│   ├── benchmark_operators.py    # Phân tích độ trễ từng toán tử
│   ├── benchmark_ablation.py     # Đo kiểm tiến trình tối ưu hóa vi kiến trúc
│   ├── benchmark_quantization.py # Đo độ trung thực lượng tử hóa, SQNR, PPL
│   ├── benchmark_memory.py       # Phân bổ SRAM và đo rò rỉ sau 24h
│   └── benchmark_energy.py       # Đo công suất và năng lượng tiêu thụ
│
├── firmware/                     # Dự án C++ ESP-IDF chuẩn công nghiệp (Chỉ chứa mã nguồn)
├── microquant/                   # Lõi nén và lượng tử hóa MicroQuant-ESP32
├── web/                          # Ứng dụng Web Chat độc lập nhúng Flash
└── esp32_ai_runtime/             # Sketch đơn file cho Arduino IDE
```

---

## Hướng Dẫn Nạp Firmware Nhanh

### Sử dụng ESP-IDF (Khuyên dùng trong sản xuất)
```bash
cd firmware
idf.py build
idf.py -p COM5 flash monitor
```

### Sử dụng Arduino IDE
1. Mở `esp32_ai_runtime/esp32_ai_runtime.ino`.
2. Trong **Tools > Board**, chọn **ESP32S3 Dev Module**.
3. Cấu hình: Flash 4MB, PSRAM: Disabled, Partition: Huge APP, CPU: 240MHz.
4. Nhấn **Upload**.

---

## Hướng Dẫn Tương Tác

### 1. Giao diện Web (Điện thoại hoặc Laptop)
- Kết nối WiFi: **SSID**: `ESP32-Local-AI` | **Mật khẩu**: `12345678`
- Truy cập trình duyệt: `http://192.168.4.1` để chat thời gian thực.

### 2. Cổng Serial Terminal
- Mở Serial Monitor tốc độ Baud **`115200`**, gõ câu hỏi bất kỳ và nhấn Enter.

---

## Tác Giả & Liên Hệ

- **Tác giả**: Dustin Nguyen
- **Vai trò**: Sinh viên năm 3 ngành Robotics & AI
- **Email**: [dustinoki.dev@gmail.com](mailto:dustinoki.dev@gmail.com)
