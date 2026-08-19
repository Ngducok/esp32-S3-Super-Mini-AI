# Ứng Dụng Web Chat Độc Lập & Script Đóng Gói Tự Động

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `web/` chứa toàn bộ mã nguồn của giao diện Web Chat Dark Mode phong cách ChatGPT và script Python tự động đóng gói HTML, CSS, JavaScript thành file header C++ lưu trong bộ nhớ Flash.

---

## Bài Toán Kỹ Thuật & Giải Pháp

### Bài toán
1. Việc tải từng file HTML, CSS, JS qua nhiều request HTTP riêng biệt trên vi điều khiển làm tăng số lượt bắt tay mạng, chiếm dụng slot socket và gây chậm giao diện.
2. Lưu file trên phân vùng SPIFFS/LittleFS vừa tốn dung lượng Flash vừa làm chậm tốc độ nạp trang do phải đọc I/O từ hệ thống file.

### Giải pháp
1. **Đóng Gói Giao Diện Một File Duy Nhất (`generate_web_header.py`)**:
   Script Python tự động đọc `index.html`, `style.css`, và `app.js`, nhúng CSS vào thẻ `<style>`, nhúng JS vào thẻ `<script>`, và xuất ra file `web_ui.h`.
2. **Lưu Trữ Trong Flash DROM**:
   Trang web hoàn chỉnh được lưu dưới dạng chuỗi C++ raw string literal:
   ```cpp
   namespace Web {
   static const char CHAT_HTML[] = R"rawliteral(<!DOCTYPE html>...)rawliteral";
   }
   ```
   Khi người dùng truy cập `http://192.168.4.1`, toàn bộ trang web được trả về trong đúng 1 gói tin HTTP duy nhất với tốc độ tức thì và tiêu thụ 0 Byte SRAM.

---

## Lưu Đồ Quy Trình Đóng Gói Web (Flowchart)

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  index.html  │     │  style.css   │     │    app.js    │
└──────┬───────┘     └──────┬───────┘     └──────┬───────┘
       │                    │                    │
       └──────────────┐     │     ┌──────────────┘
                      ▼     ▼     ▼
         [Script generate_web_header.py]
                      │
                      ├── Nhúng nội dung <style>...</style>
                      ├── Nhúng nội dung <script>...</script>
                      └── Đóng gói thành C++ R"rawliteral(...)rawliteral"
                      │
                      ▼
               [web/web_ui.h]
                      │
                      ▼
    [Được include trực tiếp vào firmware/main/web/web_server.cpp]
```

---

## Cấu Trúc Ứng Dụng Phía Client

- **`index.html`**: Giao diện ngữ nghĩa với thanh tiêu đề, luồng tin nhắn chat cuộn mượt mà, các chip gợi ý câu hỏi nhanh và khung nhập liệu.
- **`style.css`**: Bảng màu Dark Mode hiện đại (nền `#131314`, thẻ `#1e1f20`, điểm nhấn `#388bfd`) tối ưu hoàn hảo cho cả màn hình điện thoại và máy tính mà không cần thư viện CSS nặng nề bên ngoài.
- **`app.js`**: Mã xử lý logic bất đồng bộ phía client:
  - Gửi câu hỏi đến endpoint `POST /api/chat`.
  - Hiển thị bong bóng chat kèm tốc độ sinh từ tok/s và độ trễ latency.
  - Tự động gọi `GET /api/status` mỗi 5 giây để cập nhật dung lượng SRAM và thời gian uptime.

---

## Lệnh Tái Đóng Gói Giao Diện

Nếu bạn chỉnh sửa `index.html`, `style.css` hoặc `app.js`, hãy chạy lệnh sau để cập nhật lại `web_ui.h`:

```bash
python web/generate_web_header.py
```
