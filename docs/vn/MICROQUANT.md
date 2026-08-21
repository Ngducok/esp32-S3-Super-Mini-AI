# Động Cơ Lượng Tử Hóa Nén Mô Hình MicroQuant-ESP32 (microquant/)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan

`microquant/` là bộ thư viện lượng tử hóa và nén mô hình chuyên dụng cho vi điều khiển ESP32-S3. Phân hệ bao gồm bộ công cụ Python để lượng tử hóa và xuất ma trận sang C++ Flash DROM, cùng các kernel C++ tối ưu hóa vi kiến trúc (INT8, Group-wise INT4, BitNet 1.58b).

---

## 2. Bài Toán Đặt Ra & Các Nút Thắt Kỹ Thuật

1. **Giới hạn bộ nhớ Flash 4MB**: Lưu trữ mô hình ở định dạng FP32 hoặc INT8 chiếm nhiều không gian Flash khi muốn mở rộng số lượng tham số.
2. **Suy giảm dải động khi lượng tử hóa INT4 toàn cục**: Khi xuất hiện trọng số ngoại lai (outliers), lượng tử hóa toàn cục làm giảm mạnh độ chính xác (SQNR thấp).
3. **Năng lực ALU hạn chế**: Phép nhân số thực trên vi điều khiển nhúng tiêu tốn chu kỳ clock đáng kể.

---

## 3. Cách Thức Xử Lý & Giải Pháp Kiến Trúc

1. **Group-wise INT4 (Group size 32)**: Chia ma trận thành các khối 32 phần tử với hệ số scale riêng biệt, đạt tỷ lệ nén 7.7x với độ chính xác vượt trội.
2. **BitNet 1.58b Kernel**: Mã hóa trọng số ternary {-1, 0, +1}, triệt tiêu toàn bộ phép nhân trong ALU bằng các lệnh cộng/trừ thuần túy.
3. **Xuất Header C++ Zero-Copy**: Tự động sinh mảng `const` lưu trực tiếp trong Flash DROM.

---

## 4. Kết Quả Đo Đạc Toán Học (`test_quant_math.py`)

| Định dạng lượng tử | Tỷ lệ nén | Độ tương đồng Cosine | SQNR (Signal-to-Noise) | Đánh giá |
| :--- | :--- | :--- | :--- | :--- |
| **INT8 Toàn Cục** | 4.0x | **99.996%** | **40.95 dB** | Độ trung thực tuyệt đối |
| **INT4 Per-Tensor** | 8.0x | **98.720%** | **15.80 dB** | Tiết kiệm 50% Flash so với INT8 |
| **INT4 Group-wise (G32)**| **7.7x** | **99.529%** | **20.23 dB** | Tối ưu xuất sắc cho Flash 4MB |
| **BitNet 1.58b** | **16.0x** | **88.592%** | **5.77 dB** | Hoàn toàn không dùng phép nhân |
