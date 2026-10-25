#include "dual_oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_font.h"
#include "lvgl_theme.h"
#include "settings.h"

#include <algorithm>
#include <string>

#include <esp_lvgl_port.h>
#include <esp_log.h>

#define TAG "DualOledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_material_symbols_30_1);
LV_FONT_DECLARE(font_noto_emoji_30_1);

DualOledDisplay::DualOledDisplay(esp_lcd_panel_io_handle_t large_io,
                                 esp_lcd_panel_handle_t large_panel,
                                 esp_lcd_panel_io_handle_t small_io,
                                 esp_lcd_panel_handle_t small_panel, bool mirror_x,
                                 bool mirror_y, bool small_mirror_x, bool small_mirror_y)
    : large_io_(large_io), large_panel_(large_panel), small_io_(small_io),
      small_panel_(small_panel) {
    width_ = 128;
    height_ = 64;  // Bao cao kich thuoc man chinh (man lon) cho Application

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_material_symbols_30_1);
    auto emoji_font = std::make_shared<LvglBuiltInFont>(&font_noto_emoji_30_1);

    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);
    dark_theme->set_emoji_font(emoji_font);
    LvglThemeManager::GetInstance().RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL (once, for both displays)");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    auto add_disp = [](esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, int w, int h,
                       bool mx, bool my) -> lv_display_t* {
        const lvgl_port_display_cfg_t cfg = {
            .io_handle = io,
            .panel_handle = panel,
            .control_handle = nullptr,
            .buffer_size = static_cast<uint32_t>(w * h),
            .double_buffer = false,
            .trans_size = 0,
            .hres = static_cast<uint32_t>(w),
            .vres = static_cast<uint32_t>(h),
            .monochrome = true,
            .rotation = {.swap_xy = false, .mirror_x = mx, .mirror_y = my},
            .flags = {.buff_dma = 1, .buff_spiram = 0, .sw_rotate = 0, .full_refresh = 0,
                      .direct_mode = 0},
        };
        return lvgl_port_add_disp(&cfg);
    };

    display_ = add_disp(large_io_, large_panel_, 128, 64, mirror_x, mirror_y);
    if (display_ == nullptr) ESP_LOGE(TAG, "Failed to add large (face) display");
    small_display_ = add_disp(small_io_, small_panel_, 128, 32, small_mirror_x, small_mirror_y);
    if (small_display_ == nullptr) ESP_LOGE(TAG, "Failed to add small (text) display");
    // Note: SetupUI() is called by Application::Initialize()
}

DualOledDisplay::~DualOledDisplay() {
    if (face_timer_ != nullptr) lv_timer_del(face_timer_);
    if (left_eye_ != nullptr) lv_obj_del(left_eye_);
    if (right_eye_ != nullptr) lv_obj_del(right_eye_);
    if (chat_message_label_ != nullptr) lv_obj_del(chat_message_label_);
    if (large_panel_ != nullptr) esp_lcd_panel_del(large_panel_);
    if (large_io_ != nullptr) esp_lcd_panel_io_del(large_io_);
    if (small_panel_ != nullptr) esp_lcd_panel_del(small_panel_);
    if (small_io_ != nullptr) esp_lcd_panel_io_del(small_io_);
    lvgl_port_deinit();
}

bool DualOledDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }
void DualOledDisplay::Unlock() { lvgl_port_unlock(); }

void DualOledDisplay::ApplyPose(const FacePose& p) {
    // Mat trai: tam o x=38, mat phai: tam o x=90 (man 128px, giua la 64)
    // y: giua man 64px la 32, cong y_offset va tru nua chieu cao mat
    auto apply_one = [this](lv_obj_t* eye, int cx, int h, int rot, const FacePose& pose) {
        if (eye == nullptr) return;
        lv_obj_set_size(eye, pose.w, h);
        lv_obj_set_pos(eye, cx - pose.w / 2 + pose.gaze_offset, 32 - h / 2 + pose.y_offset);
        lv_obj_set_style_radius(eye, pose.radius, 0);
        lv_obj_set_style_transform_pivot_x(eye, pose.w / 2, 0);
        lv_obj_set_style_transform_pivot_y(eye, h / 2, 0);
        lv_obj_set_style_transform_rotation(eye, rot, 0);
    };
    pose_ = p;
    apply_one(left_eye_, 38, p.h_left, p.rot_left, p);
    apply_one(right_eye_, 90, p.h_right, p.rot_right, p);
}

void DualOledDisplay::ApplyBlinkClosed() {
    // Chop mat: giu nguyen vi tri/goc, chi ep chieu cao xuong 3px nhu video mau
    auto close_one = [this](lv_obj_t* eye, int cx, int rot) {
        if (eye == nullptr) return;
        lv_obj_set_size(eye, pose_.w, 3);
        lv_obj_set_pos(eye, cx - pose_.w / 2 + pose_.gaze_offset, 32 - 1 + pose_.y_offset);
        lv_obj_set_style_radius(eye, 1, 0);
        lv_obj_set_style_transform_rotation(eye, rot, 0);
    };
    close_one(left_eye_, 38, pose_.rot_left);
    close_one(right_eye_, 90, pose_.rot_right);
}

void DualOledDisplay::FaceTimerCb(lv_timer_t* timer) {
    // Chay trong LVGL task (lv_timer), thao tac truc tiep len object - khong Lock
    auto* self = static_cast<DualOledDisplay*>(lv_timer_get_user_data(timer));
    if (self == nullptr || self->left_eye_ == nullptr) return;
    self->face_tick_ms_ += 100;

    if (self->blinking_) {
        if (self->face_tick_ms_ >= self->blink_end_ms_) {
            self->blinking_ = false;
            self->ApplyPose(self->pose_);
            // Lan chop tiep theo sau 2.4-4.0s (thay doi gia de tu nhien)
            self->next_blink_ms_ =
                self->face_tick_ms_ + 2400 + (self->face_tick_ms_ % 3) * 500;
        }
        return;
    }
    if (!self->current_is_sleepy_ && self->face_tick_ms_ >= self->next_blink_ms_) {
        self->blinking_ = true;
        self->blink_end_ms_ = self->face_tick_ms_ + 130;  // nhay mat ~130ms nhu video
        self->ApplyBlinkClosed();
        return;
    }
    // Luc ranh (neutral) thi thoang liec trai/phai cho co hon
    if (self->face_tick_ms_ >= self->next_glance_ms_) {
        self->next_glance_ms_ = self->face_tick_ms_ + 4200;
        static const int kGlance[] = {-6, 0, 6, 0};
        static int idx = 0;
        if (self->pose_.rot_left == 0 && self->pose_.rot_right == 0 &&
            self->pose_.h_left >= 30) {
            auto p = self->pose_;
            p.gaze_offset = kGlance[idx++ % 4];
            self->ApplyPose(p);
        }
    }
}

void DualOledDisplay::SetupFaceUI() {
    // LVGL tao object len screen cua display dang la default
    lv_display_set_default(display_);
    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    auto make_eye = [](lv_obj_t* parent) -> lv_obj_t* {
        auto* eye = lv_obj_create(parent);
        lv_obj_set_style_bg_color(eye, lv_color_white(), 0);  // OLED don sac: trang = mau cua module (xanh/trang)
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_pad_all(eye, 0, 0);
        lv_obj_set_scrollbar_mode(eye, LV_SCROLLBAR_MODE_OFF);
        return eye;
    };
    left_eye_ = make_eye(screen);
    right_eye_ = make_eye(screen);
    ApplyPose(FacePose{});  // neutral: 2 mat tron doc nhu dau video

    face_timer_ = lv_timer_create(FaceTimerCb, 100, this);
}

void DualOledDisplay::SetupTextUI() {
    lv_display_set_default(small_display_);
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    auto container = lv_obj_create(screen);
    lv_obj_set_size(container, 128, 32);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_row(container, 0, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

    // Dong 1 (16px): trang thai / thong bao + icon mang, am thanh, pin
    auto top = lv_obj_create(container);
    lv_obj_set_size(top, 128, 16);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(top, 0, 0);
    lv_obj_set_scrollbar_mode(top, LV_SCROLLBAR_MODE_OFF);

    status_label_ = lv_label_create(top);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(top);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(top);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(top);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(top);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    // Dong 2 (16px): noi dung chat, chay chu vong tron
    chat_message_label_ = lv_label_create(container);
    lv_obj_set_size(chat_message_label_, 128, 16);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_,
                                   lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}

void DualOledDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping");
        return;
    }
    Display::SetupUI();  // danh dau da SetupUI
    DisplayLockGuard lock(this);
    SetupFaceUI();
    SetupTextUI();
    lv_display_set_default(display_);  // dua default ve man chinh
}

bool DualOledDisplay::SetEmotion(const char* emotion) {
    // Chi man lon: doi POSE cua 2 con mat theo emotion server gui xuong.
    // Ten emotion la tap chuan cua Xiaozhi (neutral, happy, angry, sleepy...).
    if (emotion == nullptr) return false;
    std::string e(emotion);
    FacePose p;  // mac dinh neutral
    bool known = true;
    current_is_sleepy_ = false;

    if (e == "neutral") {
        // giu mac dinh: mat tron doc 28x36
    } else if (e == "happy" || e == "laughing" || e == "funny" || e == "delicious" ||
               e == "loving" || e == "kissy" || e == "confident") {
        p.h_left = p.h_right = 17;  // nhiu mat cuoi
        p.y_offset = -3;
        p.radius = 8;
    } else if (e == "angry") {
        // Nhu video (giay 3-10): mat dang ngang, dui trong ha xuong, xoay cheo vao trong
        p.w = 35;
        p.h_left = p.h_right = 21;
        p.rot_left = 260;   // +26 do: phia trong (phai) cua mat trai ha xuong
        p.rot_right = -260; // -26 do: phia trong (trai) cua mat phai ha xuong
        p.radius = 6;
    } else if (e == "sad" || e == "crying") {
        p.h_left = p.h_right = 27;
        p.rot_left = -220;  // dui trong nang len = mat buon nguoc voi angry
        p.rot_right = 220;
        p.y_offset = 4;
    } else if (e == "surprised" || e == "shocked") {
        p.w = 32;
        p.h_left = p.h_right = 44;
        p.y_offset = -2;
        p.radius = 12;
    } else if (e == "sleepy") {
        p.h_left = p.h_right = 4;  // mat nham thanh duong ke nhu cuoi video
        p.radius = 2;
        current_is_sleepy_ = true;  // dang ngu thi khong chop mat nua
    } else if (e == "relaxed") {
        p.h_left = p.h_right = 13;
        p.radius = 6;
    } else if (e == "winking") {
        p.h_left = 4;  // nhay mat trai
        p.radius = 8;
    } else if (e == "thinking" || e == "confused" || e == "silly") {
        p.gaze_offset = 8;  // liec sang phai
        if (e == "confused") p.h_right = 22;
    } else if (e == "embarrassed") {
        p.h_left = p.h_right = 24;
        p.y_offset = 7;     // nhin xuong
        p.gaze_offset = -5;
    } else if (e == "cool") {
        p.h_left = p.h_right = 11;  // mat ngang "ngau"
        p.radius = 5;
    } else {
        known = false;  // khong biet -> ve neutral, bao false nhu OledDisplay goc
    }

    DisplayLockGuard lock(this);
    if (left_eye_ == nullptr) return false;
    blinking_ = false;
    ApplyPose(p);
    return known;
}

void DualOledDisplay::SetChatMessage(const char* role, const char* content) {
    // Chi man nho hien text chat
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr || content == nullptr) return;
    std::string s(content);
    std::replace(s.begin(), s.end(), '\n', ' ');
    lv_label_set_text(chat_message_label_, s.c_str());
}

void DualOledDisplay::RemoveChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr || content == nullptr) return;
    std::string s(content);
    std::replace(s.begin(), s.end(), '\n', ' ');
    if (s != lv_label_get_text(chat_message_label_)) return;
    lv_label_set_text(chat_message_label_, "");
}

void DualOledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);
    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();
    for (auto* disp : {display_, small_display_}) {
        if (disp == nullptr) continue;
        lv_display_set_default(disp);
        lv_obj_set_style_text_font(lv_screen_active(), text_font, 0);
    }
    lv_display_set_default(display_);
}

void DualOledDisplay::SetPowerSaveMode(bool on) {
    Settings settings("wifi", false);
    if (settings.GetBool("power_save_display_off", false)) {
        if (large_panel_) esp_lcd_panel_disp_on_off(large_panel_, !on);
        if (small_panel_) esp_lcd_panel_disp_on_off(small_panel_, !on);
    }
    LvglDisplay::SetPowerSaveMode(on);  // se goi SetChatMessage (man nho) + SetEmotion (man lon)
}
