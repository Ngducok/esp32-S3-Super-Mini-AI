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

## 1. Tổng Quan Dự Án

Dự án này hiện thực hóa một mô hình ngôn ngữ tạo sinh tự hồi quy (Micro-Transformer) chạy hoàn toàn cục bộ 100% trên vi điều khiển ESP32-S3. Hệ thống xử lý trực tiếp trên phần cứng (bare-metal silicon) không phụ thuộc vào đám mây, kết nối Internet hay máy chủ API bên ngoài, sinh và truyền chuỗi token theo thời gian thực với tốc độ từ 9.33 đến 20.00 token/giây (đạt đỉnh 57.7 tok/s).

Toàn bộ hệ thống vận hành bên trong giới hạn 384 KB SRAM nội bộ của bo mạch ESP32-S3 Super Mini, hoàn toàn không yêu cầu bộ nhớ PSRAM mở rộng (0 KB PSRAM). Hệ thống tích hợp song song trạm phát WiFi độc lập (SoftAP) và máy chủ HTTP Web Server nhúng trực tiếp trong bộ nhớ Flash, phục vụ giao diện chat tương tác trực tiếp qua trình duyệt của điện thoại hoặc máy tính.

---

## 2. Danh Mục Tài Liệu Hệ Thống (`docs/en/` & `docs/vn/`)

Toàn bộ tài liệu kỹ thuật chuyên sâu được quy hoạch gọn gàng trong 2 thư mục ngôn ngữ riêng biệt:

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

## 3. Bài Toán Đề Ra & Các Nút Thắt Vi Kiến Trúc Phần Cứng

1. **Phép Nhân Ma Trận (GEMV) Thô Sơ**: Vòng lặp C tuần tự gây tắc nghẽn đường ống lệnh CPU.
2. **Quản Lý KV-Cache Tuyến Tính Gây Sập Nguồn**: Cấp phát mảng tĩnh khiến mô hình bị ngắt khi chạm mốc ngữ cảnh.
3. **Lượng Tử Hóa Toàn Cục Làm Suy Giảm Dải Động**: Lượng tử hóa INT8 toàn tensor làm giảm độ chính xác khi có trọng số ngoại lai.
4. **Hàm Phi Tuyến Tính Tiêu Tốn Chu Kỳ CPU**: `libc` `expf()` và `tanhf()` tiêu tốn hơn 120 chu kỳ CPU cho mỗi lần gọi.

---

## 4. Cách Thức Xử Lý & Giải Pháp Kiến Trúc Phần Cứng

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

- **Vectorized SIMD GEMV Kernel (`simd_ops.h`)**: Tải song song 32-bit (`uint32_t`) và mở rộng vòng lặp 16-way trên 4 thanh ghi tích lũy, đạt tốc độ **nhanh gấp 2.40 lần**.
- **Bộ Đệm Vòng Trượt Sliding Window Ring-Buffer & Dynamic RoPE**: Cố định 24.5 KB trong SRAM nội bộ, cho phép hội thoại liên tục vô hạn mà không bao giờ bị tràn bộ nhớ hay crash.
- **Lượng Tử Hóa Phân Nhóm INT4 (Group Size 32) & BitNet 1.58b (`microquant/`)**: Nén 7.7x với độ tương đồng cosine **99.53%** và hỗ trợ tính toán không dùng phép nhân.
- **Bảng Tra Cứu Flash DROM Fast Math LUT (`fast_math.h`)**: 512 phần tử trong Flash DROM đưa thời gian tính hàm mũ xuống chỉ còn **1–3 chu kỳ CPU** (nhanh gấp 16.88 lần).

---

## 5. Bảng Tóm Tắt Chỉ Số Cốt Lõi ("Killer Benchmark Table")

Xem báo cáo đầy đủ theo phương pháp MLPerf Tiny tại [docs/vn/BENCHMARK.md](docs/vn/BENCHMARK.md).

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

---

## 6. Cấu Trúc Thư Mục

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

## 7. Hướng Dẫn Nạp Firmware Nhanh

### Sử dụng ESP-IDF
```bash
cd firmware
idf.py build
idf.py -p COM5 flash monitor
```

### Sử dụng Arduino IDE
1. Mở `esp32_ai_runtime/esp32_ai_runtime.ino`.
2. Chọn Board: **ESP32S3 Dev Module**, Flash: 4MB, PSRAM: Disabled, Partition: Huge APP.
3. Nhấn **Upload**.
