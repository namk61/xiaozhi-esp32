# Copper Wire Robot (Robot dây đồng)

Board custom cho trào lưu robot dây đồng Xiaozhi Việt Nam (xuất phát từ Xiaozhi.vn, bài của truongson87 ngày 13/5/2026).

## Hardware

- ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
- Mic I2S INMP441, Amp I2S MAX98357A + loa 4Ω 3W
- **2 màn hình OLED dùng chung bus I2C SDA=GPIO41 / SCL=GPIO42:**
  - Màn lớn 1.54" 128x64, driver SSD1309 (tương thích CH1116), địa chỉ **0x3D** — hiển thị **biểu cảm khuôn mặt** (`DualOledDisplay::SetEmotion`)
  - Màn nhỏ 0.91" 128x32, SSD1306, địa chỉ **0x3C** — hiển thị **text**: trạng thái, thông báo, icon mạng/pin và dòng chat (`SetChatMessage`/`SetStatus`/`ShowNotification`)
- Driver motor DRV8833 (board có IN1-IN4, OUT1-OUT4, chân SLEEP/EN và jumper J1/EEP) + 2x motor N20 — xe 2 bánh
- Pin 14250 / 18650 qua mạch sạc+bảo vệ, công tắc nguồn
- Khung: dây đồng 1.5mm (khung chính), dây đồng bọc cách điện 0.7mm (tín hiệu/nguồn)
- Tuỳ chọn: HC-SR04 (Trig 17 / Echo 18, Echo qua cầu phân áp 1k/2k) hoặc VL53L0X I2C

## Địa chỉ I2C (đã xác nhận với phần cứng của Nam)

- Màn nhỏ: **0x3C**
- Màn lớn: **0x3D**

Hai địa chỉ đã khác nhau sẵn nên **không cần đổi điện trở/jumper chọn địa chỉ**. Code lấy địa chỉ từ 2 define `DISPLAY_LARGE_I2C_ADDR` / `DISPLAY_SMALL_I2C_ADDR` trong `config.h` — nếu sau này thay module khác địa chỉ, chỉ sửa 2 dòng đó.

Nối dây: GND→GND, VCC→3V3, SCL→GPIO42, SDA→GPIO41 cho **cả hai** module, nối song song.

## Pins (cập nhật 2026-10-25 theo cấu hình của Nam)

- Mic INMP441: SD=GPIO6, WS=GPIO4, SC/SCK=GPIO5
- Mạch âm thanh MAX98357: LRC=GPIO7, BCLK=GPIO15, DIN=GPIO16
- Motor (DRV8833): IN1=GPIO12, IN2=GPIO13, IN3=GPIO14, IN4=GPIO21 (trái 12/13 → OUT1/OUT2, phải 14/21 → OUT3/OUT4). Chân SLEEP/EN trên board này đã nối sẵn qua jumper J1 nên code để `MOTOR_SLEEP_GPIO = NC`; nếu tháo J1 phải nối SLEEP lên VCC thủ công.
- Màn hình: SCL=GPIO42, SDA=GPIO41 (chung cho cả 2 màn)

## Kiến trúc hiển thị

`DualOledDisplay` (`dual_oled_display.h/.cc`, kế thừa `LvglDisplay`) khởi tạo LVGL **một lần** rồi đăng ký 2 `lv_display_t` trên cùng bus:
- `SetEmotion()` → chỉ màn hình lớn
- `SetChatMessage()`, `SetStatus()`, `ShowNotification()`, `UpdateStatusBar()` → chỉ màn nhỏ

## Animation khuôn mặt (theo video mẫu của Nam)

Màn lớn không dùng emoji/font nữa mà vẽ **2 con mắt bằng hình LVGL** (màu sáng trên nền đen — module xanh sẽ ra mắt xanh như video):

| Emotion (server gửi) | Khuôn mặt |
|---|---|
| `neutral` | 2 mắt tròn dọc 28x36 (mở đầu video) |
| `angry` | Mắt dẹt ngang 35x21, xoay ∓26° cho đuôi trong hạ xuống (giây 3-10 trong video) |
| `happy` / `laughing` / `funny` / `loving`... | Mắt nhíu cười 30x17 |
| `sad` / `crying` | Xoay ngược với angry, mắt hạ xuống |
| `surprised` / `shocked` | Mắt mở to 32x44 |
| `sleepy` | Mắt thành đường kẻ 4px |
| `winking` | Nháy 1 mắt, `thinking`/`confused` liếc ngang, `cool` mắt ngang 11px... | (xem `SetEmotion` trong code)

Animation nền chạy bằng `lv_timer` 100ms: **chớp mắt ~130ms mỗi 2,4-4 giây** (đúng nhịp video) và thỉnh thoảng **liếc trái/phải** khi đang neutral. Đang `sleepy` thì không chớp mắt nữa.

Đây là code mới, **chưa test trên phần cứng thật**. Các rủi ro cần kiểm tra khi có board: SSD1309 chạy bằng driver panel SSD1306 (thường tương thích; nếu bản CH1116 bị lệch cột 2px hoặc ngược màu, cần chuyển sang driver SH1106 cho màn lớn), thứ tự khởi tạo 2 display trong esp_lvgl_port, góc xoay `transform_rotation` của LVGL, và địa chỉ thực tế của module.

## Build

Yêu cầu ESP-IDF `release/v6.1` (tối thiểu v6.0.1, không dùng 5.x):

```bash
source /path/to/esp-idf/export.sh
python3 scripts/build.py copper-wire-robot --name copper-wire-robot --language vi-VN
idf.py -p /dev/ttyUSB0 flash monitor   # hoặc dùng build/merged-binary.bin với web flasher
```

Lần đầu boot: robot phát WiFi `Xiaozhi-XXXXXX`, vào http://192.168.4.1 để cấu hình WiFi nhà + tài khoản xiaozhi.me.

## MCP tools (điều khiển bằng giọng nói)

- `self.chassis.go_forward` / `go_back` / `turn_left` / `turn_right` — tham số `speed` 10-100%, `duration_ms` 100-10000
- `self.chassis.stop`
