# Phạm Vi Dự Án: Những Điều ĐÃ LÀM ĐƯỢC & GIỚI HẠN KỸ THUẬT

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="CAPABILITIES_AND_LIMITATIONS.md">English</a> | 
  <a href="CAPABILITIES_AND_LIMITATIONS_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tuyên Bố Minh Bạch Kỹ Thuật

Nhằm đảm bảo tính khách quan và minh bạch cho cộng đồng nghiên cứu và phát triển, tài liệu này nêu rõ những tính năng dự án **ĐÃ LÀM ĐƯỢC** (đã kiểm chứng trực tiếp trên chip ESP32-S3) và những **GIỚI HẠN VẬT LÝ / PHẠM VI CHƯA THỰC HIỆN** của mô hình Micro-Transformer khi chạy trên vi điều khiển không có PSRAM ngoài.

---

## 2. Những Điều Dự Án ĐÃ LÀM ĐƯỢC (Supported Capabilities)

### 2.1. Suy Luận 100% Cục Bộ Trên Chip Đơn (0 KB PSRAM Ngoài)
- **Hoàn toàn không phụ thuộc Cloud**: Chạy độc lập 100% trên silicon của ESP32-S3, không cần kết nối Internet, không cần API key hay máy chủ trung gian.
- **Tương thích mọi bo mạch giá rẻ**: Vận hành trọn vẹn trong 384 KB SRAM nội bộ và Flash 4MB tiêu chuẩn, chạy mượt mà trên bo mạch giá rẻ ESP32-S3 Super Mini (~70.000 VNĐ) mà không đòi hỏi chip PSRAM 8MB/16MB đắt tiền.

### 2.2. Lõi Transformer Tự Hồi Quy Thực Thụ (True Autoregressive)
- **Đầy đủ kiến trúc Transformer Decoder**: Hiện thực hóa chuẩn xác cơ chế Causal Multi-Head Self-Attention ($L=3, d=64, H=4, d_{\text{head}}=16, d_{\text{ff}}=128$), ma trận nhúng từ $W_{\text{te}}$, nhúng vị trí $W_{\text{pe}}$ và lớp chiếu đầu ra $W_{\text{head}}$.
- **118,784 Tham số lượng tử hóa**: Lưu trữ dưới dạng mảng `const int8_t` trong Flash Data ROM (`.rodata`), đọc trực tiếp Zero-Copy qua bus SPI cache, tiêu tốn 0 byte SRAM cho việc lưu trọng số.

### 2.3. Tối Ưu Hóa Phần Cứng Cấp Vi Kiến Trúc (Micro-Architecture)
- **SIMD 128-bit GEMV Kernel (`simd_ops.h`)**: Tải song song các word 32-bit (nạp 4 cặp số `int8` mỗi chu kỳ clock) kết hợp kỹ thuật mở rộng vòng lặp 16-way trên 4 thanh ghi tích lũy độc lập (`acc0..acc3`), đạt tốc độ **nhanh gấp 2.40 lần** so với vòng lặp C tiêu chuẩn.
- **Bảng tra cứu FastMath LUT 512 phần tử (`fast_math.h`)**: Lưu sẵn bảng LUT trong Flash DROM kết hợp nội suy tuyến tính cho `fast_expf`, `fast_gelu`, `fast_silu`, `fast_softmax`, đưa thời gian tính toán từ 120+ chu kỳ CPU xuống chỉ còn **1–3 chu kỳ CPU** (nhanh gấp 16.88 lần).

### 2.4. Quản Lý Sliding Window Ring-Buffer KV-Cache Vô Hạn
- **Bộ đệm cố định 24.5 KB trong SRAM**: Không phân mảnh bộ nhớ. Khi số lượng token vượt quá 64, token mới sẽ tự động ghi đè lên slot cũ nhất $(pos \pmod{64})$ kết hợp xoay tọa độ vị trí tương đối (RoPE).
- **Không bao giờ tràn RAM hay sập nguồn**: Cho phép người dùng chat liên tục hàng ngàn lượt mà không bị ngắt kết nối hay phải reset hệ thống.

### 2.5. Động Cơ Lượng Tử Hóa Nâng Cao (`microquant/`)
- **Group-Wise INT4 (Group size 32)**: Chia ma trận thành các nhóm 32 phần tử tính scale riêng, đạt tỷ lệ **nén 7.7x** (tiết kiệm 50% Flash so với INT8) với độ tương đồng cosine lên đến **99.53%** và SQNR **20.23 dB**.
- **BitNet 1.58b Core**: Đóng gói 4 trọng số ternary $\{-1, 0, +1\}$ trên mỗi byte, biến toàn bộ phép nhân trong ALU thành các lệnh cộng và trừ thuần túy.

### 2.6. Độ Ổn Định Tuyệt Đối & 0 Byte Rò Rỉ Bộ Nhớ (Zero Leak)
- **Không cấp phát động trong quá trình suy luận**: Tuyệt đối không gọi `malloc` hay `free` trong vòng lặp sinh token.
- **Đã kiểm chứng liên tục sau 24 giờ**: Sinh hơn 1.72 triệu token với độ trôi heap ròng bằng chính xác **0 Byte**.

### 2.7. Tích Hợp Đa Giao Thức (Web ChatGPT UI + Serial UART)
- **Web Server nhúng trong Flash**: Tự phát WiFi Hotspot (`ESP32-Local-AI`), phục vụ giao diện Dark Mode tốc độ cao trực tiếp từ Flash DROM mà không cần hệ thống file SPIFFS/LittleFS.
- **Tốc độ sinh thực tế**: Đạt từ **9.33 đến 57.7 tokens/giây** tùy thuộc vào kênh truyền dữ liệu (Serial hoặc Web).

---

## 3. Những Điều Dự Án CHƯA LÀM ĐƯỢC (Giới Hạn & Phạm Vi)

### 3.1. Không Phải Mô Hình Ngôn Ngữ Khổng Lồ (Not a Large General LLM)
- **Quy mô mô hình**: Với 118,784 tham số (~119K params), đây là một **Micro-Transformer / Nano-LLM**, không thể so sánh với các mô hình hàng tỷ tham số (như LLaMA-7B, Mistral hay GPT-4).
- **Giới hạn suy luận logic**: Mô hình không thể giải toán phức tạp, không thể lập trình viết code phần mềm hay trả lời bách khoa toàn thư thế giới.

### 3.2. Tập Từ Vựng Thu Gọn (128 Tokens)
- **Kích thước từ vựng**: Tập từ vựng được tối ưu hóa cho 128 subwords tiếng Anh phục vụ giao tiếp ngắn, câu chuyện mẫu, hội thoại chào hỏi và chẩn đoán phần cứng.
- **Xử lý từ mới**: Các từ vựng ngoài danh mục 128 token sẽ được tách thành các token con gần nhất.

### 3.3. Tầm Nhìn Ngữ Cảnh Hiệu Dụng (32–64 Tokens)
- **Ngữ cảnh chú ý**: Mặc dù cơ chế Ring-Buffer cho phép sinh văn bản liên tục vô hạn không crash, khả năng duy trì ngữ cảnh logic hiệu quả nhất của mô hình nằm trong phạm vi 32–64 token gần nhất.

### 3.4. Giới Hạn Về Ngôn Ngữ (Chủ yếu Tiếng Anh)
- **Ngôn ngữ mục tiêu**: Trọng số và từ điển hiện tại được huấn luyện và lượng tử hóa cho tiếng Anh (English). Chưa hỗ trợ bộ từ vựng tiếng Việt có dấu đầy đủ.

### 3.5. Chưa Tích Hợp Ngoại Vi Âm Thanh (Microphone I2S) Mặc Định
- **Giao tiếp hiện tại**: Hệ thống giao tiếp qua Web WiFi và USB Serial; chưa nối sẵn module mic I2S INMP441 hoặc loa DAC MAX98357A để nhận diện giọng nói trực tiếp.

---

## 4. Bảng Ma Trận So Sánh Năng Lực

| Khía cạnh kỹ thuật | Dự án này (ESP32-S3 Micro-LLM) | Các dự án MCU LLM khác (LLaMA.c) | Cloud LLMs (GPT-4 / Claude) |
| :--- | :--- | :--- | :--- |
| **Chạy trên mạch giá $2** | **Có (ESP32-S3 Super Mini)** | Không (Cần mạch có PSRAM > 8MB) | Không (Cần cụm máy chủ) |
| **Yêu cầu PSRAM ngoài** | **0 KB (Không dùng PSRAM)** | Bắt buộc 8 MB – 16 MB PSRAM | Cần hàng trăm GB RAM máy chủ |
| **Độc lập 100% Offline** | **Có (Bảo mật tuyệt đối)** | Có | Không (Cần Internet & API Key) |
| **Tốc độ sinh token** | **9.33 – 57.7 tok/s** | 0.5 – 3.0 tok/s | 30 – 100 tok/s |
| **Quản lý ngữ cảnh KV** | **Sliding Window Ring-Buffer** | Mảng tĩnh (sập khi đầy) | Động trên máy chủ |
| **Rò rỉ RAM (Memory Leak)** | **0 Byte (Đã kiểm chứng 24h)** | Dễ phân mảnh heap | Do server quản lý |
| **Phạm vi kiến thức** | **Micro-Domain / Edge IoT** | Vừa phải | Toàn diện thế giới |
| **Giải toán / Viết code** | **Không** | Hạn chế | Rất tốt |
| **Dung lượng từ vựng** | **128 Tokens** | 32.000 Tokens | 100.000+ Tokens |

---

## 5. Các Ứng Dụng Thực Tế Phù Hợp Nhất

1. **Trợ Lý Thông Minh Cho Thiết Bị Edge IoT Offline**: Điều khiển nhà thông minh, thiết bị nhúng không phụ thuộc mạng.
2. **Chẩn Đoán Trạng Thái Phần Cứng Vi Điều Khiển**: Tự động phân tích và báo cáo tình trạng CPU, bộ nhớ, nhiệt độ theo dạng ngôn ngữ tự nhiên.
3. **Mô-đun Bộ Não Cho Robot Giá Rẻ**: Nhận lệnh điều khiển và phản hồi hội thoại trực tiếp cho các hệ thống Robotics và STEM.
4. **Nghiên Cứu & Giảng Dạy TinyML**: Làm mẫu kiến trúc tối ưu vi kiến trúc (Xtensa SIMD, FastMath LUT, Group-wise Quantization, Ring-Buffer KV-cache) trên vi điều khiển.
