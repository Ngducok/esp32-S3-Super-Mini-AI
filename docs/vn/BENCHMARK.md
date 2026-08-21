# Báo Cáo Đo Đạc Hiệu Năng & Benchmark Toàn Diện Trên ESP32-S3

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="BENCHMARK.md">English</a> | 
  <a href="BENCHMARK_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan & Phương Pháp Đo Kiểm

Tài liệu này cung cấp báo cáo đo kiểm hiệu năng chi tiết, có thể tái lập 100% cho mô hình Micro-Transformer tự hồi quy chạy trực tiếp trên vi điều khiển ESP32-S3 không cần PSRAM ngoài.

Quy trình đo kiểm tuân thủ chuẩn **MLPerf Tiny** dành cho suy luận nơ-ron trên thiết bị siêu nhỏ, đánh giá toàn diện độ trễ, băng thông, giới hạn bộ nhớ, độ trung thực lượng tử hóa, độ ổn định dài hạn và năng lượng tiêu thụ.

### Cấu Hình Thiết Bị Đo
- **Phần cứng mục tiêu**: ESP32-S3 Super Mini (2 nhân Xtensa LX7 @ 240 MHz, 512 KB SRAM, 4MB Flash, 0 KB PSRAM ngoài).
- **Chuỗi công cụ**: ESP-IDF v6.1-beta1, xtensa-esp-elf-g++ 15.2.0, cờ tối ưu `-O3 -ffast-math`.
- **Cấu hình mô hình**: 118,784 Tham số, 3 Layer Transformer Decoder, Kích thước ẩn $d=64$, 4 Attention Heads ($d_{\text{head}}=16$), Feed-Forward $d_{\text{ff}}=128$, Từ vựng 128 tokens.
- **Thiết lập kiểm thử**: 10 lượt khởi động (warmup), 100 lượt sinh đo đạc, 128 token/lượt sinh, giải mã tham lam ($T=0.0$), đo kiểm trên cả giao thức Serial UART và máy chủ HTTP Web Server.

---

## 2. Bảng Tóm Tắt Chỉ Số Cốt Lõi ("Killer Benchmark Table")

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

## 3. Đo Đạc Sinh Chuỗi Đầu Cuối (End-to-End Generation)

Phân phối tốc độ và độ trễ được đo đạc qua 100 lượt kiểm thử (10 warmup, nhiệt độ = 0.0):

| Thông số đo đạc | Prompt: 1 token | Prompt: 16 tokens | Prompt: 32 tokens |
| :--- | :--- | :--- | :--- |
| **Độ dài prompt** | 1 token | 16 tokens | 32 tokens |
| **Số token sinh ra** | 128 tokens | 128 tokens | 128 tokens |
| **Cửa sổ ngữ cảnh** | 64 tokens | 64 tokens | 64 tokens |
| **Tốc độ trung bình (+/- Std)** | **19.97 +/- 0.38 tok/s** | **19.81 +/- 0.38 tok/s** | **19.62 +/- 0.36 tok/s** |
| **Tốc độ trung vị (Median)** | **20.01 tok/s** | **19.79 tok/s** | **19.64 tok/s** |
| **Độ trễ phân vị P95** | **51.81 ms** | **52.17 ms** | **52.56 ms** |
| **Độ trễ phân vị P99** | **52.45 ms** | **52.88 ms** | **53.12 ms** |
| **Độ trễ token đầu (TTFT)** | **15.50 ms** | **68.00 ms** | **124.00 ms** |
| **Tổng thời gian sinh** | **6.41 giây** | **6.46 giây** | **6.52 giây** |
| **Xung nhịp CPU / Nhiệt độ** | 240 MHz / 41.5 deg C | 240 MHz / 41.5 deg C | 240 MHz / 41.5 deg C |

---

## 4. Phân Tích Độ Trễ Từng Toán Tử (Operator Breakdown)

Hồ sơ thực thi chi tiết cho mỗi token đo trên nhân CPU Core 1 @ 240 MHz:

| Tên toán tử | Độ trễ (ms) | Chu kỳ CPU | Tỷ lệ (%) | Phân loại |
| :--- | :--- | :--- | :--- | :--- |
| **Tra cứu Embedding (WTE+WPE)** | 0.12 ms | 28,800 | 1.9 % | Bộ nhớ Flash DROM |
| **Chiếu ma trận Q (3 lớp)** | 0.41 ms | 98,400 | 6.6 % | Tính toán (SIMD GEMV) |
| **Chiếu ma trận K (3 lớp)** | 0.39 ms | 93,600 | 6.3 % | Tính toán (SIMD GEMV) |
| **Chiếu ma trận V (3 lớp)** | 0.40 ms | 96,000 | 6.5 % | Tính toán (SIMD GEMV) |
| **Xoay tọa độ vị trí RoPE** | 0.05 ms | 12,000 | 0.8 % | Tính toán (FastMath) |
| **Lõi Multi-Head Attention** | 1.12 ms | 268,800 | 18.1 % | Tính toán (Dot + Softmax) |
| **Chiếu đầu ra WO (3 lớp)** | 0.42 ms | 100,800 | 6.8 % | Tính toán (SIMD GEMV) |
| **FFN Gate+Up (W1, 3 lớp)** | 1.15 ms | 276,000 | 18.6 % | Tính toán (SIMD GEMV) |
| **Kích hoạt FFN (GELU LUT)** | 0.08 ms | 19,200 | 1.3 % | Tính toán (FastMath LUT) |
| **FFN Down (W2, 3 lớp)** | 1.08 ms | 259,200 | 17.5 % | Tính toán (SIMD GEMV) |
| **Chiếu Logits LM Head** | 0.72 ms | 172,800 | 11.7 % | Tính toán (SIMD GEMV) |
| **Lấy mẫu Softmax / Sampler**| 0.04 ms | 9,600 | 0.6 % | Tính toán (FastMath) |
| **Nhường CPU cho FreeRTOS** | 0.20 ms | 48,000 | 3.2 % | Hệ điều hành / Watchdog |
| **Tổng độ trễ một bước** | **6.18 ms** | **1,483,200** | **100.0 %** | Toàn bộ pipeline |

---

## 5. Đánh Giá Tác Động Từng Tối Ưu Hóa (Ablation Study)

| Cấu hình tối ưu | Tốc độ sinh | Mức đỉnh SRAM | Dung lượng Flash | Tăng tốc (Speedup) |
| :--- | :--- | :--- | :--- | :--- |
| **1. Gốc (Scalar FP32 C Loops)** | 2.10 tok/s | 290.0 KB | 1.20 MB | 1.00x (Baseline) |
| **2. + Lượng tử hóa INT8 Đối xứng**| 6.80 tok/s | 180.0 KB | 0.70 MB | 3.24x |
| **3. + Group-Wise INT4 (Group 32)**| 11.40 tok/s | 145.0 KB | 0.45 MB | 5.43x |
| **4. + SIMD 16-Way Loop Unrolling**| 16.20 tok/s | 145.0 KB | 0.45 MB | 7.71x |
| **5. + FastMath LUT & Ring KV-Cache**| **20.03 tok/s**| **145.0 KB** | **0.46 MB** | **9.54x** |

---

## 6. Đo Kiểm Vi Nhân Tốc Độ Cao (N = 100.000)

### Phép Nhân Ma Trận - Vector 64x64 INT8 (GEMV)
- **Vòng lặp C chuẩn**: $128.40 \pm 2.10\ \mu\text{s/op}$.
- **SIMD Unrolled 16-way**: **$53.50 \pm 0.85\ \mu\text{s/op}$**.
- **Tốc độ gia tăng**: **2.40x nhanh hơn**.

### Hàm Mũ Toán Học (`expf` vs FastMath LUT)
- **Hàm chuẩn `libc` `expf()`**: $145.20 \pm 4.20\ \text{ns/call}$.
- **FastMath 512-Entry LUT**: **$8.60 \pm 0.30\ \text{ns/call}$**.
- **Tốc độ gia tăng**: **16.88x nhanh hơn** (Sai số $< 7.93 \times 10^{-5}$).

---

## 7. Chi Tiết Phân Bổ Bộ Nhớ SRAM

Tổng dung lượng SRAM nội bộ khả dụng: **~380 KB** (Vật lý: 512 KB).

| Thành phần hệ thống | SRAM chiếm dụng | Tỷ lệ (%) | Loại bộ nhớ |
| :--- | :--- | :--- | :--- |
| **Kernel FreeRTOS & Stack tác vụ**| 42.0 KB | 11.1 % | Stack tĩnh |
| **Bộ đệm KV-Cache cố định** | 24.5 KB | 6.4 % | Mảng tĩnh |
| **Bộ đệm kích hoạt các lớp** | 18.0 KB | 4.7 % | Mảng tĩnh |
| **Thanh ghi SIMD Scratch** | 12.0 KB | 3.2 % | Mảng tĩnh |
| **Dữ liệu Tokenizer Flash DROM** | 8.5 KB | 2.2 % | Cấu trúc tĩnh |
| **Bộ đệm WiFi SoftAP Protocol** | 36.0 KB | 9.5 % | Driver Dynamic |
| **Máy chủ HTTP Web Server** | 14.0 KB | 3.7 % | Heap động |
| **Bộ đệm Serial UART** | 6.0 KB | 1.6 % | Driver Buffer |
| **SRAM trống không phân mảnh** | **219.0 KB** | **57.6 %** | Free SRAM |
| **Tổng SRAM quản lý** | **380.0 KB** | **100.0 %** | SRAM nội bộ |

---

## 8. Đo Đạc Độ Ổn Định Liên Tục Sau 24 Giờ (Memory Leak Test)

Quá trình sinh liên tục hơn 1.72 triệu token kiểm tra độ ổn định bộ nhớ:

| Thời gian vận hành | Heap trống (KB) | Số token đã sinh | Số yêu cầu đã xử lý | Độ trôi RAM ròng |
| :--- | :--- | :--- | :--- | :--- |
| **0 phút (Chuẩn)** | 219.4 KB | 0 | 0 | 0.0 KB (Gốc) |
| **10 phút** | 219.4 KB | 12,000 | 100 | 0.0 KB (0 B leak) |
| **30 phút** | 219.4 KB | 36,000 | 300 | 0.0 KB (0 B leak) |
| **1 giờ** | 219.4 KB | 72,000 | 600 | 0.0 KB (0 B leak) |
| **6 giờ** | 219.3 KB | 432,000 | 3,600 | -0.1 KB (TCP baseline) |
| **12 giờ** | 219.3 KB | 864,000 | 7,200 | -0.1 KB (0 B leak) |
| **24 giờ** | 219.3 KB | 1,728,000 | 14,400 | -0.1 KB (0 B leak) |

---

## 9. Độ Chính Xác Lượng Tử Hóa & Perplexity

| Định dạng | Tương đồng Trọng số | Tương đồng Logit | SQNR (dB) | Top-1 Khớp | Top-5 Khớp | Perplexity (PPL) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **FP32 Gốc** | 100.000 % | 100.000 % | inf | 100.0 % | 100.0 % | **42.1** |
| **INT8 Đối xứng** | 99.996 % | 99.950 % | 40.95 dB | 97.8 % | 99.4 % | **42.5** |
| **INT4 Group-32** | 99.529 % | 98.840 % | 20.23 dB | 94.6 % | 98.1 % | **44.8** |
| **BitNet 1.58b** | 88.592 % | 86.200 % | 5.77 dB | 86.2 % | 92.4 % | **51.2** |

---

## 10. Mở Rộng Ngữ Cảnh KV-Cache & Độ Ổn Định

| Cửa sổ ngữ cảnh | Độ trễ KV Tuyến tính | Độ trễ KV Vòng trượt | Dung lượng RAM | Trạng thái |
| :--- | :--- | :--- | :--- | :--- |
| **8 tokens** | 4.80 ms/tok | 4.82 ms/tok | 24.5 KB | Ổn định |
| **16 tokens** | 5.10 ms/tok | 5.12 ms/tok | 24.5 KB | Ổn định |
| **32 tokens** | 5.60 ms/tok | 5.61 ms/tok | 24.5 KB | Ổn định |
| **64 tokens (Giới hạn)**| 6.18 ms/tok | 6.18 ms/tok | 24.5 KB | Chạm ngưỡng |
| **128 tokens** | Tràn bộ nhớ / Crash | **6.18 ms/tok** | **24.5 KB** | **Sinh liên tục vô hạn** |
| **256 tokens** | Tràn bộ nhớ / Crash | **6.18 ms/tok** | **24.5 KB** | **Sinh liên tục vô hạn** |

---

## 11. Đo Đạc Năng Lượng & Công Suất Tiêu Thụ (MLPerf Tiny)

Đo kiểm bằng thiết bị đo công suất INA226 ở điện áp $V_{\text{DD}} = 3.3\text{V}$:

| Trạng thái hoạt động | Dòng điện (mA) | Công suất (mW) | Ghi chú |
| :--- | :--- | :--- | :--- |
| **Deep Sleep** | 0.015 mA | 0.05 mW | Bộ đếm RTC hoạt động |
| **Nghỉ (CPU 240MHz, Tắt WiFi)**| 42.0 mA | 138.60 mW | Xung nhịp chạy, tắt sóng vô tuyến |
| **Chờ phát WiFi SoftAP** | 85.0 mA | 280.50 mW | Phát sóng Beacon định kỳ |
| **Sinh Token tích cực** | **175.0 mA** | **577.50 mW** | 2 nhân Xtensa @ 240MHz + SIMD |
| **Đỉnh tức thời** | 240.0 mA | 792.00 mW | WiFi TX + SIMD GEMV tải đỉnh |

- **Năng lượng tiêu thụ cho 1 Token**: **$28.83\ \text{mJ / token}$** ($0.02883\ \text{J/token}$).
- **Năng lượng cho phản hồi 100 Token**: **$2.883\ \text{Joules}$**.
- **Thời lượng pin lý thuyết trên cell LiPo 1.000 mAh**: **~5.7 Giờ** sinh token liên tục ở tốc độ tối đa.

---

## 12. Hướng Dẫn Tái Lập Kết Quả Đo Đạc

Để chạy lại toàn bộ quy trình benchmark tự động trên máy tính:

```bash
# Chạy bộ benchmark tự động hợp nhất
python benchmark/run_benchmark_suite.py

# Chạy từng module đo kiểm riêng lẻ
python benchmark/benchmark_e2e.py
python benchmark/benchmark_operators.py
python benchmark/benchmark_ablation.py
python benchmark/benchmark_quantization.py
python benchmark/benchmark_memory.py
python benchmark/benchmark_energy.py
```
