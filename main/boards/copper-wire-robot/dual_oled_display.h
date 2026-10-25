#ifndef DUAL_OLED_DISPLAY_H
#define DUAL_OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

// Hai man hinh OLED dung chung 1 bus I2C (SDA=GPIO41, SCL=GPIO42):
//  - Man lon 1.54" 128x64 (SSD1309/CH1116, dia chi 0x3D): animation khuon mat
//    = 2 con mat ve bang LVGL (nhu video mau: mat mo tron doc, chop mat,
//      mat gian du dang ngang, mat ngu thanh duong ke...), KHONG dung emoji.
//  - Man nho 0.91" 128x32 (SSD1306, dia chi 0x3C): chi hien text
//    (trang thai, thong bao, icon mang/pin/loa va dong chat chay chu)
// Hai dia chi I2C PHAI khac nhau - xem README.md cua board.
class DualOledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t large_io_ = nullptr;
    esp_lcd_panel_handle_t large_panel_ = nullptr;
    esp_lcd_panel_io_handle_t small_io_ = nullptr;
    esp_lcd_panel_handle_t small_panel_ = nullptr;
    lv_display_t* small_display_ = nullptr;

    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    // Khuon mat hien tai: kich thuoc/goc nghieng cua tung mat
    struct FacePose {
        int w = 28;
        int h_left = 36;
        int h_right = 36;
        int y_offset = 0;     // lech doc so voi vi tri giua man
        int gaze_offset = 0;  // lech ngang (liec trai/phai)
        int rot_left = 0;     // goc xoay, don vi 0.1 do, duong = chieu kim dong ho
        int rot_right = 0;
        int radius = 9;
    };
    FacePose pose_;
    bool blinking_ = false;
    uint32_t face_tick_ms_ = 0;
    uint32_t next_blink_ms_ = 2600;
    uint32_t blink_end_ms_ = 0;
    uint32_t next_glance_ms_ = 4200;
    bool current_is_sleepy_ = false;
    lv_timer_t* face_timer_ = nullptr;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void ApplyPose(const FacePose& p);
    void ApplyBlinkClosed();
    static void FaceTimerCb(lv_timer_t* timer);
    void SetupFaceUI();
    void SetupTextUI();

public:
    DualOledDisplay(esp_lcd_panel_io_handle_t large_io, esp_lcd_panel_handle_t large_panel,
                    esp_lcd_panel_io_handle_t small_io, esp_lcd_panel_handle_t small_panel,
                    bool mirror_x, bool mirror_y, bool small_mirror_x, bool small_mirror_y);
    ~DualOledDisplay();

    virtual void SetupUI() override;
    virtual bool SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void RemoveChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void SetPowerSaveMode(bool on) override;
    virtual bool IsMonochrome() const override { return true; }
};

#endif  // DUAL_OLED_DISPLAY_H
