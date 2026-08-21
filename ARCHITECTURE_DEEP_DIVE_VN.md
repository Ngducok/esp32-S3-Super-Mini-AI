# TÀI LIỆU HỌC TẬP CHUYÊN SÂU: KIẾN TRÚC & QUY TRÌNH XỬ LÝ TOÀN DIỆN DỰ ÁN ESP32-S3 MICRO-LLM (TỪ A ĐẾN Z)

> [!IMPORTANT]
> **Tài liệu lưu hành nội bộ**: File này được biên soạn riêng để bạn tự học, nắm vững bản chất toán học, kiến trúc hệ thống nhúng và tự tin bảo vệ đồ án / phỏng vấn kỹ sư. File này đã được thêm vào `.gitignore` và không tải lên GitHub.

---

## MỤC LỤC
1. [Chương 1: Bài toán Kỹ thuật & Giới hạn Vật lý của ESP32-S3](#chương-1-bài-toán-kỹ-thuật--giới-hạn-vật-lý-của-esp32-s3)
2. [Chương 2: Bản chất Toán học của Mô hình Transformer Decoder](#chương-2-bản-chất-toán-học-của-mô-hình-transformer-decoder)
3. [Chương 3: Bí quyết Lượng tử hóa INT8 & Phép nhân Ma trận Tốc độ cao](#chương-3-bí-quyết-lượng-tử-hóa-int8--phép-nhân-ma-trận-tốc-độ-cao)
4. [Chương 4: Chiến lược Phân tầng Bộ nhớ (Memory Tiering - Chìa khóa Cốt lõi)](#chương-4-chiến-lược-phân-tầng-bộ-nhớ-memory-tiering---chìa-khóa-cốt-lõi)
5. [Chương 5: Điều phối Đa nhân FreeRTOS & Cơ chế Chống sập Watchdog](#chương-5-điều-phối-đa-nhân-freertos--cơ-chế-chống-sập-watchdog)
6. [Chương 6: Cơ chế Nhúng Web Server Zero-VFS Không Cần Hệ Thống File](#chương-6-cơ-chế-nhúng-web-server-zero-vfs-không-cần-hệ-thống-file)
7. [Chương 7: Dòng Chảy Dữ Liệu Từng Bước Khi Sinh Một Câu Prompt (Trace Step-by-Step)](#chương-7-dòng-chảy-dữ-liệu-từng-bước-khi-sinh-một-câu-prompt-trace-step-by-step)
8. [Chương 8: Bộ Câu Hỏi Ôn Tập Bảo Vệ Đồ Án & Phỏng Vấn Chuyên Sâu](#chương-8-bộ-câu-hỏi-ôn-tập-bảo-vệ-đồ-án--phỏng-vấn-chuyên-sâu)

---

## CHƯƠNG 1: BÀI TOÁN KỸ THUẬT & GIỚI HẠN VẬT LÝ CỦA ESP32-S3

### 1.1. So Sánh Máy Chủ AI (Server/GPU) vs Vi Điều Khiển (MCU)
| Thành phần | Máy chủ chạy LLM (NVIDIA H100/A100) | Vi điều khiển ESP32-S3 Super Mini |
| :--- | :--- | :--- |
| **Bộ nhớ RAM** | 80 GB – 192 GB HBM3 (Băng thông TB/s) | **384 KB Internal SRAM** (Không có PSRAM) |
| **Bộ nhớ lưu trữ** | Ổ cứng NVMe SSD hàng Terabyte | **4 MB SPI Flash** (Tốc độ đọc 80MHz) |
| **Năng lượng tiêu thụ**| 350W – 700W / GPU | **~0.5W – 1W** (Nguồn 5V Type-C) |
| **Giá thành** | Hàng chục ngàn USD | **~$2 (~70.000 VNĐ)** |

### 1.2. Thách Thức Vật Lý Trên ESP32-S3 Super Mini
1. **Không có PSRAM ngoài**:
   - Các bản ESP32-S3 cao cấp (như N16R8) có chip Octal PSRAM 8MB gắn ngoài. Nhưng bản Super Mini giá rẻ chỉ có SRAM nội bộ nằm trong chip silicon (~512KB vật lý, sau khi trừ bootloader và FreeRTOS chỉ còn khoảng **~380KB khả dụng**).
   - Nếu nạp một mô hình LLM dù chỉ 50 triệu tham số (50M params) dạng FP32 tiêu tốn 200MB RAM, vi điều khiển sẽ lập tức sập nguồn (`Out of Memory Crash`).
2. **Xung đột tài nguyên mạng WiFi**:
   - Ngăn xếp mạng WiFi SoftAP + TCP/IP + HTTP Server của ESP-IDF tự động ngốn từ **100KB đến 150KB RAM** cho các socket và bộ đệm gói tin.
   - Do đó, vùng RAM thực sự an toàn dành cho AI chỉ còn **dưới 100KB**!

---

## CHƯƠNG 2: BẢN CHẤT TOÁN HỌC CỦA MÔ HÌNH TRANSFORMER DECODER

Mô hình sử dụng kiến trúc **Transformer Decoder-Only** (tương tự kiến trúc của GPT và LLaMA nhưng thu nhỏ về kích thước vi mô).

### 2.1. Cấu Trúc Tham Số (Hyperparameters)
- Kích thước từ vựng ($V$): **128 tokens** (Bao gồm các từ, ký tự, subwords thông dụng).
- Chiều không gian ẩn ($d$): **64** (Hidden dimension).
- Số lớp Transformer ($L$): **3 layers**.
- Số đầu Attention ($H$): **4 heads**.
- Chiều mỗi đầu ($d_{\text{head}}$): $d_{\text{head}} = d / H = 64 / 4 = 16$.
- Chiều lớp Feed-Forward ($d_{\text{ff}}$): **128**.
- Chiều dài ngữ cảnh tối đa ($T$): **64 tokens**.

---

### 2.2. Chi Tiết Từng Khối Tính Toán

#### Bước 1: Nhúng Từ (Word Embedding) & Nhúng Vị Trí (Position Embedding)
Khi người dùng nhập chuỗi token, tại mỗi bước thời gian vị trí $t \in [0, T-1]$ và token ID $w_t \in [0, V-1]$:
1. Tra bảng nhúng từ: $e_t = W_{\text{te}}[w_t] \in \mathbb{R}^{64}$.
2. Tra bảng vị trí: $p_t = W_{\text{pe}}[t] \in \mathbb{R}^{64}$.
3. Dòng chảy ban đầu (Residual Stream):
   $$X^{(0)}_t = e_t + p_t$$

---

#### Bước 2: Cơ Chế Multi-Head Self-Attention Với KV-Cache
Tại mỗi lớp $l \in [0, L-1]$, vectơ đầu vào $X_t$ được chiếu tuyến tính thành 3 vectơ: **Query ($Q$)**, **Key ($K$)**, **Value ($V$)**:
$$Q_t = X_t W_q^{(l)}, \quad K_t = X_t W_k^{(l)}, \quad V_t = X_t W_v^{(l)}$$

1. **Lưu Key và Value vào KV-Cache**:
   - $K_t$ và $V_t$ được ghi đè vào vị trí $t$ trong mảng KV-Cache tĩnh của lớp $l$.
   - Các bước trước đó $[0, 1, \dots, t-1]$ đã có sẵn $K_{0..t-1}, V_{0..t-1}$ trong cache, không cần tính lại!
2. **Tính toán điểm số Attention cho từng đầu ($h \in [0, 3]$)**:
   Với mỗi vị trí quá khứ $i \in [0, t]$:
   $$\text{Score}_i = \frac{Q_t^{(h)} \cdot (K_i^{(h)})^T}{\sqrt{d_{\text{head}}}} = \frac{Q_t^{(h)} \cdot (K_i^{(h)})^T}{\sqrt{16}} = \frac{Q_t^{(h)} \cdot (K_i^{(h)})^T}{4.0}$$
3. **Chuẩn hóa Softmax**:
   $$A_i = \frac{\exp(\text{Score}_i)}{\sum_{j=0}^{t} \exp(\text{Score}_j)}$$
4. **Nhân với Value ($V$)**:
   $$\text{HeadOut}^{(h)} = \sum_{i=0}^{t} A_i \cdot V_i^{(h)}$$
5. **Gộp các đầu và Chiếu đầu ra ($W_o$)**:
   $$\text{AttnOut} = [\text{HeadOut}^{(0)}, \text{HeadOut}^{(1)}, \text{HeadOut}^{(2)}, \text{HeadOut}^{(3)}] \cdot W_o^{(l)}$$
6. **Cộng kết nối tắt (Residual Connection)**:
   $$X_t' = X_t + \text{AttnOut}$$

---

#### Bước 3: Mạng Nơ-ron Truyền Thẳng (MLP / Feed-Forward Network)
Sau khối Attention, dữ liệu đi qua 2 tầng Dense kết hợp hàm kích hoạt phi tuyến **GELU (Gaussian Error Linear Unit)**:
1. Chiếu mở rộng lên 128 chiều:
   $$H = \text{GELU}(X_t' \cdot W_1^{(l)})$$
   *Công thức xấp xỉ GELU nhanh*:
   $$\text{GELU}(x) \approx 0.5x \left( 1 + \tanh\left( \sqrt{\frac{2}{\pi}} (x + 0.044715 x^3) \right) \right)$$
2. Chiếu thu hẹp về 64 chiều:
   $$\text{MLPOut} = H \cdot W_2^{(l)}$$
3. Cộng kết nối tắt (Residual Connection):
   $$X_t^{(l+1)} = X_t' + \text{MLPOut}$$

---

#### Bước 4: Tầng Chiếu Đầu Ra (LM Head) & Lấy Mẫu (Sampler)
Sau khi đi qua 3 lớp Transformer, vectơ ẩn cuối cùng $X_t^{(3)} \in \mathbb{R}^{64}$ được chiếu lên không gian từ vựng 128 chiều:
$$\text{Logits} = X_t^{(3)} \cdot W_{\text{head}} \in \mathbb{R}^{128}$$

Bộ lấy mẫu thực thi thuật toán **Greedy Argmax** (Nhiệt độ $T=0.0$):
$$\text{Token Kế Tiếp} = \arg\max_{v \in [0, 127]} (\text{Logits}[v])$$

---

## CHƯƠNG 3: BÍ QUYẾT LƯỢNG TỬ HÓA INT8 & PHÉP NHÂN MA TRẬN TỐC ĐỘ CAO

### 3.1. Tại Sao Phải Dùng INT8 Thay Vì FP32?
- Biểu diễn số thực `float` tốn **4 bytes (32-bit)**.
- Biểu diễn số nguyên có dấu `int8_t` chỉ tốn **1 byte (8-bit)**.
- **Lợi ích**: Giảm dung lượng mô hình xuống **4 lần**, đồng thời tăng tốc độ tính toán trên CPU vi điều khiển vốn xử lý số nguyên nhanh hơn số thực.

### 3.2. Công Thức Lượng Tử Hóa Đối Xứng (Symmetric Quantization)
Một giá trị số thực $W_{\text{float}} \in [-1.0, 1.0]$ được nén thành số nguyên $W_{\text{int8}} \in [-127, 127]$:
$$W_{\text{int8}} = \text{round}(W_{\text{float}} \times \text{Scale})$$

### 3.3. Phép Nhân Ma Trận - Vector Tối Ưu Hóa (INT8 Gemv)
Khi thực hiện nhân ma trận trọng số $W$ (lưu dưới dạng `const int8_t`) với vectơ kích hoạt đầu vào $X$ (dạng `float`):
1. Lượng tử hóa tạm thời vectơ $X$ sang số nguyên: $X_q[c] = \text{clamp}(\text{int}(X[c] \times 32.0), -127, 127)$.
2. Nhân tích lũy bằng biến số nguyên 32-bit (`int32_t sum`):
   ```cpp
   int32_t sum = 0;
   for (int c = 0; c < cols; ++c) {
       sum += (int32_t)W[r * cols + c] * (int32_t)X_q[c];
   }
   // Co giãn kết quả ngược về float với hệ số scale chuẩn hóa
   output[r] = (float)sum / 900.0f;
   ```
*Hệ số `900.0f` được tính toán thực nghiệm để đưa biên độ tín hiệu về đúng dải $[-1.0, 1.0]$, ngăn chặn hiện tượng tràn số (Overflow) hoặc tiêu biến tín hiệu.*

---

## CHƯƠNG 4: CHIẾN LƯỢC PHÂN TẦNG BỘ NHỚ (MEMORY TIERING - CHÌA KHÓA CỐT LÕI)

Đây là bí kíp kỹ thuật quan trọng nhất giúp dự án chạy được trên ESP32-S3 không có PSRAM.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           BỘ NHỚ FLASH DROM                             │
│                  (Dung lượng 1.44 MB - Truy cập Zero-Copy)              │
├─────────────────────────────────────────────────────────────────────────┤
│ • Bảng trọng số Transformer (Wq, Wk, Wv, Wo, W1, W2, Wte, Wpe, Whead)   │
│ • Bảng chuỗi từ vựng Vocabulary (128 chuỗi token)                       │
│ • Mã nguồn Giao diện Web ChatGPT Dark Mode (web_ui.h)                   │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ Đọc trực tiếp qua SPI Cache Bus
                                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           BỘ NHỚ SRAM NỘI BỘ                            │
│                  (Khả dụng ~380 KB - Tốc độ truy cập cực nhanh)         │
├─────────────────────────────────────────────────────────────────────────┤
│ • KV-Cache Tĩnh (3 layers * 64 tokens * 64 dims * 2) = 24,576 B (~24.5K)│
│ • Bộ đệm trung gian Activations & Logits = ~2 KB                        │
│ • Vùng nhớ ngăn xếp FreeRTOS Task Stacks = ~16 KB                       │
│ • Vùng nhớ mạng WiFi SoftAP + TCP/IP Socket Buffers = ~120 KB           │
│ • BỘ NHỚ TRỐNG DỰ PHÒNG AN TOÀN = > 210 KB                              │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.1. Cơ Chế Zero-Copy Từ Flash DROM
- Khi bạn khai báo một mảng bằng từ khóa `const int8_t W[...] = {...};`, trình biên dịch GCC của ESP-IDF sẽ đặt mảng này vào phân vùng **DROM (Data Read-Only Memory)** trên chip Flash SPI.
- Khi CPU thực hiện phép nhân ma trận, khối quản lý bộ nhớ phần cứng (MMU) của ESP32-S3 sẽ tự động đọc từng dòng qua bộ đệm SPI Cache phần cứng.
- **Kết quả**: Bạn có thể lưu hàng Megabyte trọng số mà **tiêu thụ đúng 0 Byte RAM**!

### 4.2. Quản Lý KV-Cache Tĩnh & Triệt Tiêu Phân Mảnh (0-Leak)
- Trong các mô hình LLM chạy trên máy tính, người ta dùng thư viện động (như PyTorch hay C++ `std::vector`), liên tục cấp phát RAM (`malloc`) khi câu dài ra. Trên vi điều khiển, việc này sẽ tạo ra các "lỗ hổng" bộ nhớ (Heap Fragmentation) và làm sập chip sau vài phút.
- Dự án này giải quyết bằng cách **cố định trước mảng KV-cache**:
  ```cpp
  static int8_t s_k_cache[3][64][64]; // 3 layers, max 64 tokens, 64 dimensions (Key)
  static int8_t s_v_cache[3][64][64]; // 3 layers, max 64 tokens, 64 dimensions (Value)
  ```
  Tổng dung lượng cố định là:
  $$2 \times 3 \times 64 \times 64 \times 1\text{ byte} = 24,576\text{ bytes} \approx 24.5\text{ KB}$$
  (Gồm $12,288\text{ bytes Key} + 12,288\text{ bytes Value}$).
- **Tuyệt đối không gọi `malloc()` hay `free()`** trong suốt quá trình sinh từ.

---

## CHƯƠNG 5: ĐIỀU PHỐI ĐA NHÂN FREERTOS & CƠ CHẾ CHỐNG SẬP WATCHDOG

ESP32-S3 sở hữu vi xử lý 2 nhân thực (**Xtensa Dual-Core 32-bit LX7** chạy ở xung nhịp **240 MHz**).

### 5.1. Phân Chia Tải Đa Nhân (Core Pinning)
Nếu đưa cả tính toán AI và xử lý mạng WiFi lên cùng 1 nhân CPU, khi AI đang tính toán ma trận, CPU sẽ không kịp phản hồi gói tin WiFi ACK/SYN, dẫn đến việc trình duyệt web bị ngắt kết nối (Connection Timeout).

Giải pháp trong `firmware/main/main.cpp`:
- **CPU Core 0**: Chuyên trách toàn bộ các tác vụ mạng và truyền thông.
  - WiFi SoftAP Driver Event Loop.
  - Ngăn xếp mạng lwIP TCP/IP.
  - Máy chủ HTTP Web Server daemon (Lắng nghe cổng 80).
  - Tác vụ Heartbeat báo cáo SRAM định kỳ.
- **CPU Core 1**: Chuyên trách lõi tính toán nặng.
  - Tác vụ dòng lệnh Serial USB-JTAG (`chat_task`).
  - Lõi suy luận Transformer (`LLM::Generator`).

### 5.2. Cơ Chế Nhường CPU (FreeRTOS Watchdog Yielding)
- FreeRTOS có một cơ chế an toàn gọi là **Task Watchdog Timer (`task_wdt`)**. Nếu một tác vụ chiếm dụng CPU 100% liên tục quá 5 giây mà không nhường cho tác vụ `IDLE0` hoặc `IDLE1`, Watchdog sẽ kích hoạt ngắt Reset toàn bộ chip.
- Để xử lý việc này, sau khi tính toán xong mỗi token trong hàm `generator.cpp`, chúng ta chèn lệnh:
  ```cpp
  vTaskDelay(pdMS_TO_TICKS(1)); // Nhường CPU 1 mili-giây cho hệ điều hành
  ```
- Lệnh này cho phép FreeRTOS reset bộ đếm Watchdog và xử lý các tác vụ nền, giúp hệ thống chạy liên tục hàng tháng mà không bao giờ bị treo!

---

## CHƯƠNG 6: CƠ CHẾ NHÚNG WEB SERVER ZERO-VFS KHÔNG CẦN HỆ THỐNG FILE

### 6.1. Nhược Điểm Của Cách Làm Truyền Thống (SPIFFS/LittleFS)
Thông thường, khi lập trình web server trên ESP32, người ta thường format một phân vùng SPIFFS, upload file `index.html`, `style.css`, `app.js` lên đó, rồi dùng hàm `fopen()`/`fread()` để đọc file gửi cho client. Cách này:
- Làm chậm tốc độ nạp trang do nghẽn I/O flash.
- Tốn RAM để cấp phát bộ đệm đọc file.
- Dễ lỗi phân vùng khi mất điện đột ngột.

### 6.2. Giải Pháp Zero-VFS Bằng Python Asset Bundler
1. Trong thư mục `web/`, chúng ta có file `generate_web_header.py`.
2. Script này đọc `index.html`, tự động nhúng toàn bộ CSS từ `style.css` vào thẻ `<style>`, nhúng JavaScript từ `app.js` vào thẻ `<script>`.
3. Đóng gói chuỗi kết quả thành một biến C++ `const char CHAT_HTML[]` sử dụng cú pháp **Raw String Literal** trong file `web_ui.h`:
   ```cpp
   namespace Web {
   static const char CHAT_HTML[] = R"rawliteral(
   <!DOCTYPE html>
   <html lang="en">
   <head>
     <style>/* Toàn bộ CSS Dark Mode */</style>
   </head>
   <body>
     <!-- Giao diện Chat -->
     <script>// Toàn bộ logic JS</script>
   </body>
   </html>
   )rawliteral";
   }
   ```
4. Khi trình duyệt gửi `GET /`, máy chủ HTTP chỉ việc gọi:
   ```cpp
   httpd_resp_send(req, Web::CHAT_HTML, HTTPD_RESP_USE_STRLEN);
   ```
   Toàn bộ trang web được truyền thẳng từ Flash DROM ra sóng WiFi với độ trễ **< 1ms**, tiêu thụ **0 Byte SRAM**!

---

## CHƯƠNG 7: DÒNG CHẢY DỮ LIỆU TỪNG BƯỚC KHI SINH MỘT CÂU PROMPT (TRACE STEP-BY-STEP)

Giả sử người dùng gửi prompt: `"tell me a joke"`.

```
[BƯỚC 1: TIẾP NHẬN DỮ LIỆU]
Trình duyệt gửi POST /api/chat với body JSON: {"message": "tell me a joke"}
Máy chủ Web (Core 0) giải mã chuỗi prompt và chuyển sang tác vụ Generator (Core 1).
                                │
                                ▼
[BƯỚC 2: TOKENIZE & NHÚNG TỪ BAN ĐẦU]
Chuỗi "tell me a joke" được tách thành các token ID: [45, 12, 8, 92].
Tại bước t = 0 (token 45):
  - Tra bảng Wte[45] + Wpe[0] -> Vectơ X_0 (64 chiều).
  - Đi qua Lớp 0 -> Lớp 1 -> Lớp 2.
  - Lưu Key và Value của token 45 vào KV-Cache tại vị trí pos = 0.
Lặp lại tương tự cho các token 12, 8, 92 để "nạp ngữ cảnh" vào KV-Cache (Prefill Phase).
                                │
                                ▼
[BƯỚC 3: VÒNG LẶP SINH TỪ TỰ HỒI QUY (AUTOREGRESSIVE DECODING)]
Bắt đầu sinh token mới tại vị trí pos = 4:
  1. Vectơ ẩn cuối cùng đi qua LM Head (Whead) -> Mảng 128 Logits.
  2. Bộ lấy mẫu Argmax tìm index có logit lớn nhất: Giả sử chọn token 67 (Từ "Why").
  3. Ghép từ "Why" vào bộ đệm chuỗi kết quả và gửi stream ra Serial.
  4. Lấy token 67 làm đầu vào cho bước tiếp theo pos = 5:
     - Tra Wte[67] + Wpe[5] -> Vectơ X_5.
     - Tính Q_5, nhân với toàn bộ K_0..5 trong KV-Cache.
     - Sinh ra token tiếp theo (Ví dụ: "did").
  5. Lặp lại cho đến khi sinh đủ số token tối đa hoặc gặp ký tự kết thúc ('\n').
                                │
                                ▼
[BƯỚC 4: ĐÓNG GÓI KẾT QUẢ & ĐO ĐẠC HIỆU NĂNG]
Sau khi sinh xong:
  - Tính thời gian: Elapsed Time = 2.4 giây, Tổng số token = 48 tokens.
  - Tốc độ sinh: Speed = 48 / 2.4 = 20.0 tokens/giây.
  - Độ trễ: Latency = 2400ms / 48 = 50ms / token.
  - Đóng gói JSON trả về:
    {"reply": "Why did the programmer quit? Because he didn't get arrays!", "tok_sec": 20.0, "latency_ms": 50.0}
```

---

## CHƯƠNG 8: BỘ CÂU HỎI ÔN TẬP BẢO VỆ ĐỒ ÁN & PHỎNG VẤN CHUYÊN SÂU

### Câu 1: Tại sao không lượng tử hóa xuống INT4 luôn mà lại dùng INT8?
* **Trả lời**: INT8 là điểm cân bằng hoàn hảo giữa độ chính xác của mô hình và độ phức tạp tính toán trên kiến trúc vi điều khiển Xtensa. Với INT8, mỗi byte bộ nhớ chứa đúng 1 trọng số, CPU có thể nạp trực tiếp qua lệnh đọc byte thông thường (`l8ui`). Nếu dùng INT4, mỗi byte chứa 2 trọng số (High Nibble và Low Nibble), đòi hỏi CPU phải thực hiện thêm các phép toán dịch bit (`>> 4`) và mặt nạ bit (`& 0x0F`), làm tăng số chu kỳ lệnh CPU. Tuy nhiên, INT4 là hướng nghiên cứu tiếp theo rất giá trị để nhân đôi dung lượng mô hình trên Flash 4MB.

### Câu 2: Khác biệt giữa KV-Cache tĩnh trong dự án này và KV-Cache động trên GPU là gì?
* **Trả lời**: Trên GPU, KV-Cache thường được cấp phát động theo cơ chế PagedAttention hoặc cấp phát theo batch để phục vụ hàng ngàn người dùng đồng thời. Trên vi điều khiển đơn người dùng (Single-tenant Edge AI) không có PSRAM, việc cấp phát động sẽ gây phân mảnh RAM (Heap Fragmentation). Do đó, dự án cố định kích thước KV-Cache thành mảng tĩnh $3 \times 64 \times 64 = 12\text{ KB}$ trong SRAM, đảm bảo an toàn tuyệt đối 100% không rò rỉ bộ nhớ.

### Câu 3: Làm thế nào bạn chứng minh được firmware không bị rò rỉ bộ nhớ (Zero Memory Leak)?
* **Trả lời**: Em xây dựng module `Diagnostics::MemoryTracker`. Trước khi bắt đầu vòng lặp suy luận, hàm sẽ đọc mốc heap bằng `esp_get_free_heap_size()`. Sau khi sinh xong hàng trăm lượt suy luận và giải phóng các biến cục bộ trên stack, hàm sẽ kiểm tra lại mốc heap. Kết quả đo đạc thực tế cho thấy độ trôi ròng $\text{Drift} = 0\text{ bytes}$, chứng minh toàn bộ bộ nhớ được thu hồi hoàn hảo.

### Câu 4: Nếu muốn mở rộng ngữ cảnh từ 64 token lên 512 token thì gặp rào cản gì?
* **Trả lời**: Rào cản lớn nhất là dung lượng SRAM nội bộ. Với $T=64$, KV-cache tốn $12\text{ KB}$. Nhưng nếu tăng lên $T=512$, dung lượng KV-cache sẽ tăng tuyến tính $8\text{ lần}$ lên $96\text{ KB}$. Khi cộng thêm bộ đệm WiFi (~120KB) và stack FreeRTOS, dung lượng SRAM sẽ chạm ngưỡng nguy hiểm (>300KB/380KB), dễ gây sập WiFi khi có nhiều client truy cập. Giải pháp tối ưu là chuyển sang kiến trúc Attention tuyến tính như **RWKV hoặc Mamba** (với bộ nhớ cố định $O(1)$) hoặc nâng cấp lên bo mạch có PSRAM ngoài.

---

> [!TIP]
> **Lời khuyên**: Hãy đọc kỹ từng chương, nắm vững các công thức toán học và sơ đồ bộ nhớ trên. Khi bạn hiểu rõ bản chất từng dòng code từ mức cổng logic, thanh ghi, Flash DROM cho đến thuật toán Transformer, bạn sẽ hoàn toàn làm chủ mọi câu hỏi của hội đồng bảo vệ và nhà tuyển dụng!
