# Hướng Dẫn Đóng Góp Cho Dự Án ESP32-S3 Micro-LLM

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="CONTRIBUTING.md">English</a> | 
  <a href="CONTRIBUTING_VN.md">Tiếng Việt</a>
</p>

---

Cảm ơn bạn đã quan tâm và muốn đóng góp cho dự án ESP32-S3 Micro-LLM. Chúng tôi hoan nghênh mọi đóng góp về mã nguồn, báo cáo lỗi (bug report), tối ưu hóa hiệu năng tính toán và cải tiến tài liệu kỹ thuật.

---

## Bạn Có Thể Đóng Góp Những Gì?

1. **Báo cáo lỗi (Bug Reports)**: Nếu bạn gặp lỗi biên dịch hoặc lỗi trong quá trình suy luận AI, vui lòng mở một Issue trên GitHub kèm thông tin chi tiết về phần cứng (phiên bản chip, dung lượng Flash), phiên bản ESP-IDF và log backtrace trên Serial Monitor.
2. **Bổ sung tính năng mới**:
   - Nhân tính toán ma trận lượng tử hóa 4-bit (INT4).
   - Kiến trúc Attention tuyến tính hoặc State-Space Model (RWKV, Mamba-Micro).
   - Cơ chế suy đoán trước token (Speculative Decoding) trên 2 nhân CPU.
3. **Cải tiến tài liệu**: Bổ sung hướng dẫn, lưu đồ thuật toán và kết quả benchmark trên các biến thể bo mạch ESP32-S3 khác nhau.

---

## Quy Trình Đóng Góp (Contribution Workflow)

### 1. Fork Kho Chứa (Repository)
Nhấn nút **Fork** ở góc trên bên phải của repository trên GitHub để tạo một bản sao dự án trên tài khoản của bạn.

### 2. Clone Và Tạo Nhánh Mới (Branch)
```bash
git clone https://github.com/YOUR-USERNAME/esp32-S3-Super-Mini-AI.git
cd esp32-S3-Super-Mini-AI
git checkout -b feature/ten-tinh-nang-cua-ban
```

### 3. Quy Chuẩn Lập Trình (Coding Guidelines)
- **Không cấp phát động trong vòng lặp suy luận**: Tuyệt đối không gọi `malloc` hoặc `free` trong luồng sinh token để tránh phân mảnh bộ nhớ SRAM.
- **Tài liệu song ngữ**: Khi thêm tính năng hoặc thư mục mới, vui lòng cập nhật cả file tài liệu Tiếng Anh và Tiếng Việt.
- **Phong cách mã nguồn**: Sử dụng C++17/C++20 hiện đại, đóng gói trong namespace rõ ràng (`LLM`, `Web`, `Config`, `Diagnostics`). Chú thích ngắn gọn, đúng trọng tâm kỹ thuật.
- **Không lạm dụng icon/emoji**: Duy trì tài liệu chuyên nghiệp, chuẩn mực mã nguồn mở quốc tế.

### 4. Commit Và Push Lên GitHub
Sử dụng quy chuẩn commit rõ ràng:
```bash
git commit -m "feat: bổ sung nhân tính toán ma trận lượng tử hóa INT4"
git push origin feature/ten-tinh-nang-cua-ban
```

### 5. Gửi Pull Request (PR)
1. Truy cập vào kho chứa fork của bạn trên GitHub và nhấn **Compare & pull request**.
2. Điền tiêu đề và nội dung mô tả chi tiết những gì bạn đã thay đổi, lý do và kết quả đo đạc thực nghiệm trên phần cứng ESP32-S3.
3. Gửi Pull Request để được review và merge vào nhánh `main`.
