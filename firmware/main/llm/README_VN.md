# Lõi Suy Luận Micro-Transformer (INT8 LLM Core)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `llm/` hiện thực hóa động cơ **Transformer Decoder** chạy trên chip. Nó đảm nhận các phép nhân ma trận - vector số nguyên lượng tử hóa INT8, quản lý bộ nhớ đệm Key-Value tĩnh trong SRAM, cơ chế Multi-Head Self-Attention và lấy mẫu token xác thực.

---

## Bài Toán Kỹ Thuật & Thuật Toán Xử Lý

### Bài toán
1. Suy luận mô hình Transformer ở định dạng số thực 32-bit (FP32) tiêu tốn rất nhiều băng thông và vượt quá dung lượng SRAM của chip.
2. Việc cấp phát động bộ nhớ cho KV-cache trong quá trình sinh từ dễ gây phân mảnh RAM và sập hệ thống trên vi điều khiển không có PSRAM ngoài.
3. Lấy mẫu ngẫu nhiên xác suất cao trên mô hình lượng tử hóa nhỏ dễ gây nhiễu và sinh ra các ký tự vô nghĩa.

### Giải pháp
1. **Lượng tử hóa INT8 Đối xứng**:
   Toàn bộ các ma trận trọng số ($W_q, W_k, W_v, W_o, W_1, W_2, W_{te}, W_{pe}, W_{head}$) được lượng tử hóa sang số nguyên có dấu 8-bit (`int8_t`). Phép nhân ma trận - vector được tính bằng biến tích lũy số nguyên 32-bit và co giãn theo tỷ lệ $1/900.0f$:

$$\text{Output}[r] = \left( \sum_{c=0}^{\text{cols}-1} W[r, c] \cdot X_q[c] \right) \times \frac{1}{900.0}$$

2. **KV-Cache Tĩnh Trong SRAM Nội Bộ**:
   Bộ nhớ đệm Key và Value được cấp phát tĩnh cố định trong SRAM nội bộ:
   ```cpp
   static int8_t s_k_cache[LAYERS][MAX_SEQ_LEN][DIM];
   static int8_t s_v_cache[LAYERS][MAX_SEQ_LEN][DIM];
   ```
   Với cấu hình $L=3, T=64, d=64$, tổng bộ đệm Key và Value tiêu thụ đúng $2 \times 3 \times 64 \times 64 = 24,576\text{ bytes}$ (~24.5 KB).

3. **Bộ Lấy Mẫu Tham Lam Argmax (Greedy Sampler)**:
   Khi thiết lập nhiệt độ về $0.0$, bộ lấy mẫu sẽ luôn chọn token có logit lớn nhất $\arg\max(\text{logits})$, đảm bảo câu sinh ra luôn chính xác, mạch lạc và đúng ngữ pháp.

---

## Lưu Đồ Thuật Toán Sinh Token Tự Hồi Quy (Autoregressive Flowchart)

```
                      [Chuỗi Prompt Đầu Vào]
                                │
                                ▼
                       [Bộ Tokenizer Khớp Từ]
                                │
                 ┌──────────────┴──────────────┐
                 ▼                             ▼
       [Bảng Nhúng Từ WTE]           [Bảng Nhúng Vị Trí WPE]
                 └──────────────┬──────────────┘
                                ▼
                   [Dòng Residual: X = WTE + WPE]
                                │
          ┌─────────────────────┴─────────────────────┐
          ▼                                           ▼
[Lớp 0, 1, 2: Transformer Block]              [Cập Nhật KV-Cache]
  ├── Chiếu ma trận Q, K, V (INT8)              └── Lưu K, V tại vị trí pos
  ├── Multi-Head Attention (H=4, d_head=16)
  ├── Chiếu đầu ra Attention (W_o)
  ├── Cộng Residual (X = X + Attn_Out)
  ├── Mạng nơ-ron GELU MLP (d_ff=128)
  └── Cộng Residual (X = X + MLP_Out)
                                │
                                ▼
                   [Lớp Chiếu Đầu Ra LM Head]
                                │
                                ▼
                    [Vectơ Logits: 128 chiều]
                                │
                                ▼
                  [Bộ Lấy Mẫu Greedy / Softmax]
                                │
                                ▼
                   [Phát Token Kế Tiếp Ra Stream]
                                │
                                ├───────────► (Lặp lại cho token tiếp theo)
                                ▼
                 [Dừng khi gặp Ký tự xuống dòng hoặc Hết độ dài]
```

---

## Bảng Thông Số Siêu Tham Số (Hyperparameters)

| Tham số | Ký hiệu | Giá trị |
| :--- | :--- | :--- |
| Kích thước từ vựng | $V$ | 128 tokens |
| Kích thước ẩn | $d$ | 64 |
| Số lớp Transformer | $L$ | 3 |
| Số đầu Attention | $H$ | 4 |
| Kích thước mỗi đầu | $d_{\text{head}}$ | 16 |
| Kích thước lớp Feed-Forward | $d_{\text{ff}}$ | 128 |
| Độ dài ngữ cảnh tối đa | $T$ | 64 |
| Bộ nhớ KV-Cache tiêu thụ | - | 24,576 bytes (~24.5 KB tổng cộng K + V) |
| Dung lượng Flash lưu trọng số | - | 118,784 tham số (~119 KB INT8 trong Flash DROM) |

---

## Danh Sách File Mã Nguồn

- `transformer.h` / `transformer.cpp`: Cơ chế Multi-Head Attention, hàm kích hoạt GELU và quản lý KV-cache tĩnh.
- `sampler.h` / `sampler.cpp`: Bộ lấy mẫu Softmax nhiệt độ thấp và Argmax xác định.
- `generator.h` / `generator.cpp`: Vòng lặp sinh token tự hồi quy với cơ chế nhường CPU cho FreeRTOS Watchdog.
- `model_llm_weights.h`: Header chứa toàn bộ ma trận trọng số INT8 và bảng từ vựng lưu trong Flash DROM.
