# MicroQuant-ESP32 (Tiếng Việt)

<p align="center">
  <b>Khung Làm Việc Nén Dữ Liệu Toán Học & Động Cơ Suy Luận Sub-Byte Bare-Metal Cho ESP32 & Vi Điều Khiển</b><br>
  <i>INT8 (4x) • INT4 (8x) • BitNet 1.58-bit (16x) • Đọc Zero-Copy Flash DROM • Không Rò Rỉ RAM</i>
</p>

<p align="center">
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 🌟 Tổng Quan

**MicroQuant-ESP32** là một bộ công cụ nén mô hình toán học (Python Toolkit) kết hợp cùng động cơ suy luận C++ Bare-Metal hiệu năng cao, được thiết kế để triển khai các mô hình nơ-ron lớn lên các vi điều khiển bị giới hạn bộ nhớ (như **ESP32-S3 với 4MB Flash và 0 KB PSRAM ngoài**) mà **không cần cắt tỉa (Non-Pruning) và không làm mất bất kỳ tham số nào của mô hình**.

---

## 📐 3 Công Thức Lượng Tử Hóa Toán Học & Đóng Gói Bit

### 1. Lượng Tử Hóa INT8 Đối Xứng (Thu nhỏ 4 lần so với FP32)
Với ma trận trọng số $W \in \mathbb{R}^{M \times N}$:

$$S_8 = \frac{\max(|W|)}{127.0}, \quad W_{\text{int8}} = \text{clamp}\left(\left\lfloor \frac{W}{S_8} + 0.5 \right\rfloor, -128, 127\right)$$

- **Mật độ lưu trữ**: 1 byte / trọng số
- **Cơ chế tính toán**: Tận dụng biến tích lũy SIMD của CPU Xtensa LX7

---

### 2. Lượng Tử Hóa INT4 Đóng Gói Bit (Thu nhỏ 8 lần so với FP32)
Đóng gói 2 số nguyên 4-bit $[-8, 7]$ vào chung đúng 1 byte `uint8_t`:

$$S_4 = \frac{\max(|W|)}{7.0}, \quad W_{\text{int4}} = \text{clamp}\left(\left\lfloor \frac{W}{S_4} + 0.5 \right\rfloor, -8, 7\right)$$

$$\text{PackedByte}[m] = \left(W_{\text{int4}}[2m] \ \& \ \text{0x0F}\right) \ | \ \left(\left(W_{\text{int4}}[2m+1] \ \& \ \text{0x0F}\right) \ll 4\right)$$

- **Mật độ lưu trữ**: 0.5 byte / trọng số (1 byte chứa 2 trọng số)
- **Cơ chế tính toán**: Giải nén bit trực tiếp trên thanh ghi CPU (Register-level unpacking) trong vòng lặp nhân ma trận

---

### 3. Lượng Tử Hóa BitNet 1.58-Bit Ternary (Thu nhỏ 16 lần so với FP32)
Mã hóa trọng số sang tập 3 ngôi $\{-1, 0, +1\}$, triệt tiêu hoàn toàn phép nhân số học của CPU:

$$\gamma = \frac{1}{M \times N} \sum_{i=1}^M \sum_{j=1}^N |W_{ij}|, \quad \widetilde{W}_{ij} = \text{clamp}\left(\text{round}\left(\frac{W_{ij}}{\gamma}\right), -1, 1\right)$$

- **Mã hóa 2-bit**: $00_2 \to 0, \quad 01_2 \to +1, \quad 10_2 \to -1$
- **Mật độ lưu trữ**: 0.25 byte / trọng số (**1 byte chứa 4 trọng số**)
- **Phép nhân ma trận không cần nhân (Multiplication-Free)**:
  $$Y[r] = \gamma \times \left( \sum_{w_{r, c} = +1} X[c] - \sum_{w_{r, k} = -1} X[k] \right)$$
  *(CPU chỉ thực hiện phép cộng và trừ số nguyên, bỏ qua phép nhân)*

---

## 📊 Bảng So Sánh Dung Lượng & Khả Năng Nén

| Định dạng | Dung lượng / 1 Trọng số | Mô hình 1 Triệu tham số (1M) | Vừa Flash 4MB? | Chế độ tính toán |
| :--- | :--- | :--- | :--- | :--- |
| **Float32 (Gốc)** | 4.0 Bytes | 4.0 MB | ❌ Không vừa | Nhân số thực chuẩn |
| **INT8 (Đối xứng)** | 1.0 Byte | 1.0 MB | ✅ Vừa | Nhân số nguyên SIMD |
| **INT4 (Packed Nibbles)** | 0.5 Bytes | 500 KB | ✅ Vừa (**nhỏ hơn 8x**) | Giải nén thanh ghi |
| **BitNet (1.58-bit)** | 0.25 Bytes | **250 KB** | ✅ Vừa (**nhỏ hơn 16x**)| **Cộng/Trừ (Bỏ phép nhân)** |

---

## 🚀 Hướng Dẫn Sử Dụng Trong C++ (Arduino & ESP-IDF)

```cpp
#include "MicroQuant.h"
#include "model_int4_weights.h" // File header sinh ra từ tool Python

void run_inference(const float* input_activations, float* output_logits) {
    int8_t x_q[MicroQuantModel::WEIGHT_DIM_1];
    MicroQuant::float_to_int8_vector(input_activations, x_q, MicroQuantModel::WEIGHT_DIM_1);

    // Tính tích ma trận - vector với cơ chế giải nén trực tiếp trên thanh ghi CPU:
    MicroQuant::matvec_int4(
        MicroQuantModel::WEIGHT_DATA,
        x_q,
        output_logits,
        MicroQuantModel::WEIGHT_DIM_0,
        MicroQuantModel::WEIGHT_DIM_1,
        MicroQuantModel::WEIGHT_SCALE * (1.0f / 30.0f)
    );
}
```

---

## 📜 Giấy Phép Bản Quyền

Phát hành theo giấy phép mã nguồn mở **MIT License**.
