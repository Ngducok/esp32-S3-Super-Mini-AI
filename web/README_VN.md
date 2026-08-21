# Giao Diện Người Dùng Web Nhúng Flash DROM (web/)

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## 1. Tổng Quan

Thư mục `web/` chứa toàn bộ mã nguồn giao diện người dùng web (HTML5, CSS3, JavaScript) được nhúng trực tiếp vào bộ nhớ Flash DROM dưới dạng mảng C++ (`web_ui.h`).

---

## 2. Bài Toán Đặt Ra & Cách Xử Lý

### Bài toán
Việc sử dụng hệ thống tệp tin ngoài (như SPIFFS hoặc LittleFS) tiêu tốn thêm RAM đệm tệp tin và đòi hỏi các bước phân vùng Flash riêng biệt.

### Cách xử lý
Toàn bộ mã nguồn web được biên dịch và đóng gói trực tiếp thành chuỗi hằng số C++ trong Flash DROM:
1. `generate_web_header.py` tự động đọc `index.html`, `style.css`, `app.js` và sinh ra `web_ui.h`.
2. Máy chủ HTTP phục vụ trang web với độ trễ phản hồi tức thì (< 5 ms) mà không tiêu tốn SRAM đệm tệp tin.
