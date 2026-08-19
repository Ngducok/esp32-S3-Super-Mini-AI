# Chẩn Đoán Hệ Thống, Thăm Dò Phần Cứng & Kiểm Toán Bộ Nhớ

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `diagnostics/` cung cấp các công cụ thăm dò năng lực phần cứng silicon, kiểm toán phát hiện rò rỉ bộ nhớ RAM thời gian thực, đo benchmark năng lực tính toán của CPU và xuất telemetry dạng JSON.

---

## Bài Toán Kỹ Thuật & Giải Pháp

### Bài toán
1. Khi chạy các mô hình nơ-ron bậc thấp trên vi điều khiển không có hệ điều hành quản lý bộ nhớ ảo, các lỗi rò rỉ RAM nhỏ có thể âm thầm làm cạn kiệt SRAM theo thời gian, gây sập vi điều khiển đột ngột (`Guru Meditation Errors`).
2. Thông số phần cứng (phiên bản silicon, tần số CPU, dung lượng Flash, sự tồn tại của PSRAM) có thể khác nhau giữa các lô sản xuất và cần được xác thực tự động lúc boot.

### Giải pháp
1. **Trình Kiểm Toán Rò Rỉ Bộ Nhớ (`MemoryTracker`)**:
   Chụp ảnh bộ nhớ SRAM và PSRAM trước và sau mỗi lượt suy luận. Tính toán độ trôi bộ nhớ ròng:
   $$\text{Độ trôi (Drift)} = \text{Free SRAM}_{\text{sau}} - \text{Free SRAM}_{\text{gốc}}$$
   Nếu độ trôi bị âm sau hàng trăm lượt suy luận, hệ thống sẽ phát cảnh báo ngay lập tức.
2. **Thăm Dò Phần Cứng Tự Động (`HardwareProbe`)**:
   Đọc API nhận dạng chip của ESP-IDF để phát hiện số nhân CPU, tần số hoạt động (240 MHz), dung lượng Flash SPI và dung lượng heap nội bộ.
3. **Bài Benchmark Nhân Ma Trận Cố Định**:
   Thực thi 1.000 lần phép nhân ma trận số thực $16 \times 16$ để đánh giá hiệu năng tính toán thực tế của 2 nhân Xtensa LX7.

---

## Lưu Đồ Quy Trình Chẩn Đoán (Flowchart)

```
                 [Khởi Động Hệ Thống / Boot]
                               │
                               ▼
                   [Diagnostics::HardwareProbe]
                               │
            ┌──────────────────┼──────────────────┐
            ▼                  ▼                  ▼
     [Đọc Thông Tin Chip] [Đo Flash SPI]   [Kiểm Tra Vùng Nhớ Heap]
      • Model: ESP32-S3    • Flash: 4MB      • Tổng SRAM: 512KB
      • Bản Rev: v0.2      • Tốc độ: 80MHz   • SRAM trống: ~380KB
      • Cores: 2 @ 240M                      • PSRAM: 0 KB (Không có)
                               │
                               ▼
                 [Diagnostics::MemoryTracker]
                               │
                               ├── 1. Ghi nhận mốc SRAM cơ sở lúc khởi động
                               ├── 2. Chụp trạng thái RAM trước suy luận
                               ├── 3. Chụp trạng thái RAM sau suy luận
                               └── 4. Xác nhận độ trôi ròng == 0 Byte
                               │
                               ▼
                    [Diagnostics::Telemetry]
                               │
                 ┌─────────────┴─────────────┐
                 ▼                           ▼
       [In Log Console Dễ Đọc]      [Xuất Gói JSON Telemetry]
```

---

## Danh Sách File Mã Nguồn

- `hardware_probe.h` / `hardware_probe.cpp`: Thăm dò silicon, in bảng thông số phần cứng và thực thi benchmark nhân ma trận CPU.
- `memory_tracker.h` / `memory_tracker.cpp`: Chụp ảnh heap, theo dõi đỉnh sử dụng RAM và phát hiện rò rỉ bộ nhớ.
- `telemetry.h` / `telemetry.cpp`: Định dạng số liệu hiệu năng thành log và chuỗi JSON phục vụ dashboard.
