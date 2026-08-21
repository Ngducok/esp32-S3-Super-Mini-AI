# Phân Hệ Chẩn Đoán, Thăm Dò Phần Cứng & Giám Sát Bộ Nhớ (diagnostics/)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan

Thư mục `diagnostics/` cung cấp công cụ thăm dò phần cứng silicon, kiểm toán rò rỉ bộ nhớ (Heap Leak Audit), đo kiểm vi kiến trúc CPU và ghi nhận nhật ký telemetry thời gian thực.

---

## 2. Bài Toán Đặt Ra & Cách Xử Lý

### Bài toán
1. Việc chạy mô hình ngôn ngữ trên vi điều khiển không có hệ điều hành quản lý bộ nhớ ảo rất dễ bị rò rỉ RAM (Memory Leak), dẫn đến lỗi sập nguồn (`Guru Meditation Error`, `LoadProhibited`).
2. Tốc độ thực thi phép nhân ma trận và hàm mũ cần được đo đạc trực tiếp trên silicon để đối chứng hiệu năng vi kiến trúc.

### Cách xử lý
1. **Kiểm toán rò rỉ RAM (`MemoryTracker`)**: Chụp lại mức chiếm dụng SRAM nội bộ trước và sau mỗi lượt suy luận:
   $$\text{Drift} = \text{Free SRAM}_{\text{post}} - \text{Free SRAM}_{\text{baseline}}$$
   Xác thực độ trôi bộ nhớ bằng chính xác 0 Byte sau hàng ngàn lượt sinh từ.
2. **Benchmark Vi Kiến Trúc (`HardwareProbe`)**: Đo kiểm trực tiếp 1.000 phép nhân ma trận 64x64 INT8 (Baseline vs SIMD) và 10.000 lời gọi hàm `expf()` (libc vs Fast Math LUT).

---

## 3. Kết Quả Đo Đạc Thực Nghiệm Trên Chip

| Hạng mục kiểm tra | Thuật toán chuẩn | Tối ưu hóa Vi kiến trúc | Kết quả |
| :--- | :--- | :--- | :--- |
| **64x64 INT8 GEMV** | 128.40 us/op | **53.50 us/op** | **2.40x nhanh hơn** |
| **Hàm mũ Exp** | 145.2 ns/call | **8.6 ns/call** | **16.88x nhanh hơn** |
| **Độ trôi SRAM (Drift)** | - | **0 Byte (Zero Leak)** | **Độ ổn định tuyệt đối** |
| **Khởi động chẩn đoán** | - | **< 10 ms** | **Sẵn sàng tức thì** |
