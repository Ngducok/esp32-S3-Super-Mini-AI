# Phân Hệ Cấu Hình Hệ Thống

<p align="left">
  <b>Ngôn ngữ:</b> 
  <a href="README.md">English</a> | 
  <a href="README_VN.md">Tiếng Việt</a>
</p>

---

## Tổng Quan

Thư mục `config/` định nghĩa các khai báo chân phần cứng tĩnh lúc biên dịch, mức ưu tiên tác vụ FreeRTOS, kích thước stack và các ngưỡng an toàn vận hành.

---

## Bài Toán Kỹ Thuật & Giải Pháp

### Bài toán
Việc định nghĩa rải rác các chân GPIO và tham số tác vụ FreeRTOS trong nhiều file mã nguồn khác nhau dễ gây xung đột chân, khó bảo trì và nguy cơ tràn bộ nhớ stack.

### Giải pháp
1. **Tập Trung Hóa Trong Namespace C++ Kiểu Mạnh**:
   Gom toàn bộ định nghĩa phần cứng vào `Config::Hardware` và tham số ứng dụng vào `Config::App` bằng các hằng số `constexpr` lúc biên dịch.
2. **Tính Toán Dung Lượng Stack Chuẩn Xác**:
   - `CHAT_TASK_STACK_SIZE = 8192 bytes`: Đảm bảo đủ không gian cho vòng lặp sinh token, bộ đệm chuỗi và chuyển đổi kiểu dữ liệu.
   - `TELEMETRY_TASK_STACK_SIZE = 4096 bytes`: Đủ cho việc format chuỗi JSON và xuất qua Serial.

---

## Bảng Tra Cứu Tham Số Cấu Hình

| Tham số | Namespace | Giá trị | Ý nghĩa |
| :--- | :--- | :--- | :--- |
| `PIN_STATUS_LED` | `Config::Hardware` | `GPIO_NUM_8` | Chân LED trạng thái trên ESP32-S3 Super Mini |
| `STATUS_LED_ACTIVE_LOW` | `Config::Hardware` | `false` | Phân cực kích hoạt LED |
| `MAX_GENERATION_TOKENS` | `Config::App` | `48` | Số lượng token tối đa sinh ra cho mỗi prompt |
| `DEFAULT_TEMPERATURE` | `Config::App` | `0.0f` | Nhiệt độ lấy mẫu (0.0 = Greedy Argmax xác định) |
| `DEFAULT_TOP_P` | `Config::App` | `0.9f` | Ngưỡng xác suất tích lũy Nucleus Top-P |
| `CHAT_TASK_PRIORITY` | `Config::App` | `5` | Mức ưu tiên FreeRTOS cho tác vụ chat |
| `CHAT_TASK_STACK_SIZE` | `Config::App` | `8192 bytes` | Kích thước stack dành cho tác vụ suy luận |
| `MIN_SAFE_HEAP_BYTES` | `Config::App` | `32768 bytes` | Ngưỡng cảnh báo cạn kiệt RAM (32 KB) |

---

## Danh Sách File Mã Nguồn

- `hardware_config.h`: Khai báo chân GPIO và cấu hình ngoại vi phần cứng.
- `app_config.h`: Mức ưu tiên tác vụ, kích thước cấp phát bộ nhớ và giá trị lấy mẫu mặc định.
