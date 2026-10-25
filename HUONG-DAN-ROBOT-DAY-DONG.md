# Robot dây đồng Xiaozhi — Hướng dẫn làm từ A-Z
**Cho: Nam — Bản chuẩn cộng đồng Xiaozhi.vn (truongson87, 13/5/2026) + board custom đã code sẵn trong thư mục `source/`**

Robot nói chuyện AI bằng tiếng Việt, có mặt OLED, nghe bằng mic INMP441, nói bằng loa qua MAX98357, chạy 2 bánh bằng motor N20 qua DRV8833, khung uốn bằng dây đồng. Chi phí linh kiện khoảng **~1.000.000đ** (theo chia sẻ cộng đồng).

> Mình (Muse) đã làm phần mềm cho bạn: clone source `78/xiaozhi-esp32`, tạo board mới `copper-wire-robot` (không sửa board gốc — đúng quy định OTA của dự án), đăng ký đầy đủ vào Kconfig/CMake/build.py và đã kiểm tra `scripts/build.py --list-boards` nhận diện đúng board. Phần bạn phải làm tay: mua linh kiện, uốn khung, hàn, nạp firmware.

---

## 1. Danh sách linh kiện (BOM)

| # | Linh kiện | Ghi chú |
|---|---|---|
| 1 | ESP32-S3 DevKit N16R8 | 16MB Flash, 8MB PSRAM — bắt buộc đúng loại này |
| 2 | MAX98357A | Mạch amp I2S cho loa |
| 3 | INMP441 | Mic I2S omnidirectional |
| 4 | OLED 1.54" 128x64 SSD1309/CH1116, 4 chân I2C | **Màn lớn — hiển thị biểu cảm khuôn mặt**, địa chỉ 0x3D |
| 4b | OLED 0.91" 128x32 SSD1306 I2C | **Màn nhỏ — hiển thị text** (trạng thái + chat), địa chỉ 0x3C |
| 5 | Loa 4Ω 3W (hoặc 8Ω 1-2W) | Nhỏ, vừa khung |
| 6 | DRV8833 | Driver 2 motor DC |
| 7 | 2x Motor N20 giảm tốc + bánh xe | Kèm bánh răng 64T theo hướng dẫn gốc nếu làm bản xe thăng bằng |
| 8 | Pin Li-ion 14250 (bản nhỏ) hoặc 18650 + mạch sạc/bảo vệ TP4056 / module sạc có boost 5V nếu cần | Xem mục Nguồn — quan trọng |
| 9 | Công tắc nguồn, nút nhấn | BOOT có sẵn trên devkit, thêm nút touch/vol nếu muốn |
| 10 | Dây đồng trần 1.5mm | ~3 sợi để uốn 1 khung. Muốn cứng hơn dùng 2mm (khó uốn hơn) |
| 11 | Dây đồng bọc cách điện 0.7mm | Dây tín hiệu + nguồn, nhiều màu càng dễ debug |
| 12 | LED + điện trở 220Ω | Báo trạng thái (GPIO48 là LED trên board tuỳ phiên bản) |
| 13 | Tuỳ chọn: HC-SR04 hoặc VL53L0X | Cảm biến khoảng cách tránh vật cản |
| 14 | Tuỳ chọn: Module thẻ microSD | Cộng đồng có bản mở rộng riêng |

**Dụng cụ:** mỏ hàn (nên loại chỉnh nhiệt), thiếc 0.8-1mm, nhựa thông / flux, hút thiếc, kìm uốn/kìm cắt, keo nến, đồng hồ vạn năng, khuôn in 3D tạo hình khung (file khuôn được cộng đồng chia sẻ trên Xiaozhi.vn).

## 2. Sơ đồ nối dây (khớp 100% với `config.h` đã code)

### Âm thanh — INMP441 (mic)
| INMP441 | ESP32-S3 |
|---|---|
| SCK | GPIO5 |
| WS | GPIO4 |
| SD | GPIO6 |
| L/R | GND (kênh trái) |
| VDD / GND | 3V3 / GND |

### Âm thanh — MAX98357A (loa) — cập nhật 2026-10-25
| MAX98357 | ESP32-S3 |
|---|---|
| LRC | GPIO7 |
| BCLK | GPIO15 |
| DIN | GPIO16 |
| VIN / GND | 5V (VIN pin) hoặc 3V3 / GND |
| GAIN, SD | Để trống (mặc định) |

### Hai màn hình OLED — dùng chung GPIO41/42
Cả hai module đều có 4 chân `GND / VCC / SCL / SDA`, nối **song song** vào cùng 2 chân:

| Cả 2 OLED | ESP32-S3 |
|---|---|
| SDA | GPIO41 |
| SCL | GPIO42 |
| VCC / GND | 3V3 / GND |

Phân vai: màn lớn 1.54" (**0x3D**) = **animation khuôn mặt 2 mắt** như video mẫu (mắt tròn dọc khi bình thường, chớp mắt mỗi vài giây, mặt giận thì mắt dẹt ngang và xếch xuống ở đuôi trong, buồn ngủ thì mắt thành đường kẻ), màn nhỏ 0.91" (**0x3C**) = text. Hai địa chỉ đã khác nhau sẵn trên phần cứng của bạn nên không cần đổi jumper/điện trở gì cả.

### Motor — Driver → 2x N20 — cập nhật 2026-10-25
| Driver | ESP32-S3 / Đích |
|---|---|
| IN1 / IN2 | GPIO12 / GPIO13 → Motor trái (OUT1/OUT2) |
| IN3 / IN4 | GPIO14 / GPIO21 → Motor phải (OUT3/OUT4) |
| SLEEP / EN | Board DRV8833 trong ảnh đã nối sẵn qua jumper J1 (EEP) — không cần dây. Nếu tháo J1 thì nối SLEEP lên VCC/3V3, không motor sẽ không chạy |
| VM / VCC | Nguồn motor (pin, qua công tắc) / 3V3 |
| GND | GND chung — **bắt buộc nối chung GND tất cả mạch** |

### Nút & LED
BOOT = GPIO0, Touch (giữ để nói) = GPIO47, Vol+ = GPIO40, Vol- = GPIO39, LED builtin = GPIO48.

### Cảm biến khoảng cách (tuỳ chọn)
HC-SR04: Trig = GPIO17, Echo = GPIO18 **qua cầu phân áp 1kΩ/2kΩ** (Echo ra 5V, ESP32 chỉ chịu 3.3V).

## 3. Nguồn — phần dễ cháy / dễ lỗi nhất
- ESP32-S3 + MAX98357 + 2 motor N20 không được cấp chung một đường yếu: motor gây sụt áp làm ESP reset khi vừa nói vừa chạy.
- Cách làm cộng đồng hay dùng: Pin → công tắc → 1 đường cấp board (qua mạch sạc/boost ổn định 5V vào VIN), 1 đường riêng cấp VM cho DRV8833, **chung GND**.
- Không hàn trực tiếp lên pin Li-ion bằng mỏ hàn lâu — dùng đế/hộp pin hoặc dây hàn sẵn, hàn nhanh có flux. Sai cực pin có thể hỏng board / nguy hiểm.

## 4. Uốn khung dây đồng
1. In / vẽ khuôn theo kích thước mong muốn (đầu chứa OLED, thân chứa ESP32 + pin, chân đế giữ 2 motor đối xứng).
2. Uốn dây 1.5mm theo khuôn: đầu → cổ → thân → chân. Hàn cố định các mối giao nhau để khung thành "bộ xương" dẫn GND được (nhiều người dùng khung đồng làm GND chung — nếu làm vậy phải chắc chắn không chạm đường nguồn/tín hiệu).
3. Cách điện mọi điểm dây tín hiệu 0.7mm đi qua khung bằng ống co / sơn cách điện.
4. Gắn motor đối xứng hai bên, bánh xe thẳng hàng — lệch trục là robot chạy cong.

## 5. Thứ tự hàn & test (đừng lắp hết rồi mới test)
1. **Chỉ ESP32 + 2 OLED:** flash thử — màn lớn hiện mặt robot, màn nhỏ hiện "Initializing" = xong I2C. Nếu hai màn đổi vai cho nhau (mặt chạy lên màn nhỏ) là địa chỉ đang bị ngược, kiểm tra lại 0x3D = lớn, 0x3C = nhỏ.
2. **Thêm INMP441 + MAX98357 + loa:** nói thử, nghe/nói được = xong I2S.
3. **Thêm DRV8833 + motor (chưa gắn bánh, để trên bàn):** ra lệnh đi/lùi/rẽ, bánh quay đúng chiều. Ngược chiều thì đảo dây motor hoặc đảo IN1/IN2 trong code.
4. **Cuối cùng mới đóng khung + pin.**

## 6. Firmware — đã chuẩn bị sẵn

Source ở: `xiaozhi-copper-robot/source/` — board mới: `main/boards/copper-wire-robot/`
(`config.h`, `copper_wire_robot_board.cc`, `motor_controller.h`, `config.json`, `README.md`)

**Cách dễ nhất cho lần đầu (chưa cần build):**
Đừng flash firmware `bread-compact-wifi` để test loa nữa — bản đó dùng DOUT=GPIO7 / LRCK=GPIO16, ngược với cấu hình mới của bạn (LRC=GPIO7 / DIN=GPIO16) nên sẽ không ra tiếng đúng. Hãy build thẳng board `copper-wire-robot` ở dưới để test.

**Build bản custom có motor:**
```bash
# Cần ESP-IDF release/v6.1 (tối thiểu v6.0.1, KHÔNG dùng 5.x)
git clone -b release/v6.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3 && . ./export.sh

cd xiaozhi-copper-robot/source
python3 scripts/build.py copper-wire-robot --name copper-wire-robot --language vi-VN
# Ra file: build/merged-binary.bin
idf.py -p /dev/ttyUSB0 flash monitor    # Windows: COM3, macOS: /dev/tty.usbserial-*
```

**Mình đã kiểm tra:** `python3 scripts/build.py --list-boards` liệt kê đúng `copper-wire-robot` với 2 variant. **Chưa kiểm tra:** build thật bằng ESP-IDF (môi trường này không có IDF) và chạy trên phần cứng thật — bạn cần làm bước này.

## 7. Sử dụng lần đầu
1. Bật nguồn → robot phát WiFi `Xiaozhi-XXXXXX`.
2. Kết nối vào, mở http://192.168.4.1 → chọn WiFi nhà 2.4GHz.
3. Đăng ký tài khoản ở xiaozhi.me, nhập mã kích hoạt robot đọc/hiển thị.
4. Nói wake word hoặc nhấn/giữ nút touch (GPIO47) rồi ra lệnh: hỏi đáp, "đi thẳng", "rẽ trái", "dừng lại" (motor qua MCP tools `self.chassis.*`).

## 8. Lỗi thường gặp
| Triệu chứng | Nguyên nhân thường gặp |
|---|---|
| Robot im lặng hoàn toàn | Mic bị bịt lỗ / sai kênh L/R / sai chân I2S — cộng đồng từng gặp robot "câm" vì nắp mic chưa khoét lỗ |
| Reset khi motor chạy | Sụt áp nguồn motor — tách đường nguồn, thêm tụ 470-1000µF gần DRV8833 |
| Màn hình ngược / sai | Đổi `DISPLAY_MIRROR_X/Y` trong config.h, hoặc chọn nhầm 128x32 vs 128x64 |
| Chạy lệch một bên | Motor/bánh không đối xứng, hoặc tốc độ 2 bên khác nhau — giảm speed bên nhanh trong code |
| Không vào WiFi config | Giữ BOOT lúc khởi động, hoặc nhấn BOOT khi đang ở trạng thái Starting |

## Nguồn tham khảo
- Source gốc: https://github.com/78/xiaozhi-esp32
- Cộng đồng VN: Xiaozhi.vn — bài gốc robot dây đồng của truongson87 (13/5/2026), nhóm FB "Xiaozhi AI Việt Nam"
- Video hướng dẫn xe 2 bánh dây đồng (có link kit Shopee trong mô tả): YouTube "Hướng dẫn tự làm robot AI Xiaozhi 2 bánh đầy đủ chi tiết nhất"

---
Muốn mình làm tiếp bản nào? 1) Màn màu 1.54" thay OLED, 2) Thêm tránh vật cản tự động bằng HC-SR04/VL53L0X, 3) Bản chatbot để bàn không motor (rẻ, dễ nhất cho lần đầu).
