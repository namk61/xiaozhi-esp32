#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "dual_oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "motor_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "CopperWireRobotBoard"

class CopperWireRobotBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Display* display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    void InitializeDisplayI2c() {
        // Mot bus I2C duy nhat cho ca 2 man hinh (chung SDA=41, SCL=42)
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {.enable_internal_pullup = 1},
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    // Tao 1 panel SSD1306-family tren bus chung, phan biet bang dia chi I2C + chieu cao.
    // SSD1309 (man lon 1.54") tuong thich tap lenh SSD1306 o che do 128x64.
    bool CreatePanel(uint8_t i2c_addr, uint8_t height, esp_lcd_panel_io_handle_t& out_io,
                     esp_lcd_panel_handle_t& out_panel) {
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = i2c_addr,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {.dc_low_on_data = 0, .disable_control_phase = 0},
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &out_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;
        esp_lcd_panel_ssd1306_config_t ssd1306_config = {.height = height};
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(out_io, &panel_config, &out_panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(out_panel));
        if (esp_lcd_panel_init(out_panel) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init panel at 0x%02X", i2c_addr);
            return false;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(out_panel, false));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(out_panel, true));
        return true;
    }

    void InitializeDualDisplay() {
        esp_lcd_panel_io_handle_t large_io = nullptr, small_io = nullptr;
        esp_lcd_panel_handle_t large_panel = nullptr, small_panel = nullptr;

        bool large_ok = CreatePanel(DISPLAY_LARGE_I2C_ADDR, DISPLAY_LARGE_HEIGHT, large_io, large_panel);
        bool small_ok = CreatePanel(DISPLAY_SMALL_I2C_ADDR, DISPLAY_SMALL_HEIGHT, small_io, small_panel);
        if (!large_ok || !small_ok) {
            ESP_LOGE(TAG, "Dual display init failed (large_ok=%d small_ok=%d). Kiem tra dia chi "
                          "I2C: man lon phai o 0x3D, man nho o 0x3C.",
                     large_ok, small_ok);
            display_ = new NoDisplay();
            return;
        }
        display_ = new DualOledDisplay(large_io, large_panel, small_io, small_panel,
                                       DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                       DISPLAY_SMALL_MIRROR_X, DISPLAY_SMALL_MIRROR_Y);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]() { Application::GetInstance().StopListening(); });
        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto v = codec->output_volume() + 10;
            if (v > 100) v = 100;
            codec->SetOutputVolume(v);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(v));
        });
        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto v = codec->output_volume() - 10;
            if (v < 0) v = 0;
            codec->SetOutputVolume(v);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(v));
        });
    }

    void InitializeTools() { static CopperMotorController motor; }

public:
    CopperWireRobotBoard()
        : boot_button_(BOOT_BUTTON_GPIO),
          touch_button_(TOUCH_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeDualDisplay();
        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(CopperWireRobotBoard);
