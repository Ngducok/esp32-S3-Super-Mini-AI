# Lõi Suy Luận Micro-Transformer & Tối Ưu Hóa Vi Kiến Trúc (llm/)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan

Thư mục `llm/` hiện thực hóa động cơ suy luận **Transformer Decoder** tự hồi quy tối ưu hóa vi kiến trúc trên vi điều khiển ESP32-S3. Phân hệ đảm nhận các phép nhân ma trận - vector song song (SIMD GEMV), bảng tra cứu hàm phi tuyến tính tốc độ cao (Fast Math LUT), quản lý bộ đệm vòng trượt (Sliding Window Ring-Buffer KV-Cache) và bộ lấy mẫu xác suất phân phối.

---

## 2. Bài Toán Đặt Ra & Các Nút Thắt Kỹ Thuật

1. **Nút thắt GEMV**: Vòng lặp C lồng nhau tiêu chuẩn tính tích vô hướng từng byte tuần tự gây nghẽn đường ống lệnh (pipeline stall) trên vi xử lý Xtensa LX7 dual-issue.
2. **Nút thắt giới hạn ngữ cảnh KV-Cache**: Cơ chế mảng tĩnh tuyến tính từ 0 đến MAX_SEQ_LEN khiến mô hình bị ngắt đột ngột hoặc tràn bộ nhớ khi chuỗi sinh vượt quá giới hạn ngữ cảnh.
3. **Nút thắt hàm phi tuyến tính**: Thư viện `math.h` tiêu tốn hơn 120 chu kỳ CPU cho mỗi lần gọi `expf()` hoặc `tanhf()`, làm chậm đáng kể lớp Softmax và hàm kích hoạt GELU/SiLU.

---

## 3. Cách Thức Xử Lý & Giải Pháp Kỹ Thuật

```
[Vector Kích Hoạt int8] ──► [SIMD GEMV: 32-bit Loads & 16-way Unroll] ──► [Fast Math LUT]
                                           │                                     │
                                           ▼                                     ▼
                     [Sliding Window Ring-Buffer KV-Cache]              [Chiếu Logits LM Head]
```

1. **Vectorized SIMD GEMV (`simd_ops.h`)**:
   - Sử dụng ép kiểu con trỏ `uint32_t*` nạp 4 cặp số `int8` trong 1 chu kỳ clock.
   - Mở rộng vòng lặp 16-way trên 4 thanh ghi tích lũy độc lập (`acc0..acc3`), triệt tiêu độ trễ dữ liệu và tăng tốc 2.40x so với vòng lặp C tiêu chuẩn.
2. **Sliding Window Ring-Buffer KV-Cache (`transformer.cpp`)**:
   - Duy trì bộ đệm cố định 24.5 KB trong SRAM. Khi $pos \ge MAX\_SEQ\_LEN$, token mới tự động ghi đè lên slot cũ nhất $(pos \pmod{MAX\_SEQ\_LEN})$ kết hợp điều chỉnh vị trí nhúng tương đối (Dynamic RoPE), hỗ trợ chat liên tục vô hạn.
3. **Bảng Tra Cứu Flash DROM Fast Math LUT (`fast_math.h`)**:
   - 512 phần tử bảng tra cứu nội suy tuyến tính cho `fast_expf`, `fast_gelu`, `fast_silu`, `fast_softmax` giảm thời gian tính toán xuống 1 - 3 chu kỳ CPU với sai số tuyệt đối $< 7.9 	imes 10^{-5}$.

---

## 4. Thông Số Thực Nghiệm Sau Khi Chạy Thử

| Chỉ số đo đạc | Baseline (Thuật toán cũ) | Tối ưu hóa Vi kiến trúc | Đánh giá |
| :--- | :--- | :--- | :--- |
| **Tốc độ GEMV 64x64** | 128.40 us/op | **53.50 us/op** | **2.40x nhanh hơn** |
| **Độ trễ hàm Exp Softmax** | 145.2 ns/call | **8.6 ns/call** | **16.88x nhanh hơn** |
| **Thời gian sinh token** | ~105 ms/token | **~50 ms - 70 ms/token** | **14 - 20 token/giây** |
| **Giới hạn ngữ cảnh** | Dừng khi chạm 64 tokens | **Vô hạn (Sliding Window)** | **0 Crash** |
| **Bộ nhớ KV-Cache SRAM** | 24,576 bytes | **24,576 bytes cố định** | **Zero Leak** |
