#ifndef __COPPER_MOTOR_CONTROLLER_H__
#define __COPPER_MOTOR_CONTROLLER_H__

#include "mcp_server.h"
#include "config.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>

// DRV8833 differential drive for 2x N20 motors, exposed as MCP tools so the
// voice assistant can move the robot: "di thang", "lui lai", "re trai"...
// PWM: LEDC timer 1, 20 kHz, 10-bit (0-1023). Speed property is 0-100 (%).
class CopperMotorController {
private:
    static constexpr ledc_timer_t kTimer = LEDC_TIMER_1;
    static constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;

    esp_timer_handle_t stop_timer_ = nullptr;

    static ledc_channel_t ChannelFor(gpio_num_t pin) {
        // Map the 4 H-bridge inputs to 4 LEDC channels (deterministic by pin)
        if (pin == MOTOR_LEFT_IN1) return LEDC_CHANNEL_0;
        if (pin == MOTOR_LEFT_IN2) return LEDC_CHANNEL_1;
        if (pin == MOTOR_RIGHT_IN1) return LEDC_CHANNEL_2;
        return LEDC_CHANNEL_3;  // MOTOR_RIGHT_IN2
    }

    void InitPin(gpio_num_t pin) {
        ledc_channel_config_t cfg = {};
        cfg.gpio_num = pin;
        cfg.speed_mode = kMode;
        cfg.channel = ChannelFor(pin);
        cfg.intr_type = LEDC_INTR_DISABLE;
        cfg.timer_sel = kTimer;
        cfg.duty = 0;
        cfg.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&cfg));
    }

    void SetPin(gpio_num_t pin, int speed_percent) {
        if (speed_percent < 0) speed_percent = 0;
        if (speed_percent > 100) speed_percent = 100;
        uint32_t duty = (1023 * speed_percent) / 100;
        auto ch = ChannelFor(pin);
        ledc_set_duty(kMode, ch, duty);
        ledc_update_duty(kMode, ch);
    }

    void Drive(int left_percent, int right_percent) {
        // left/right: -100..100, sign = direction
        SetPin(MOTOR_LEFT_IN1, left_percent > 0 ? left_percent : 0);
        SetPin(MOTOR_LEFT_IN2, left_percent < 0 ? -left_percent : 0);
        SetPin(MOTOR_RIGHT_IN1, right_percent > 0 ? right_percent : 0);
        SetPin(MOTOR_RIGHT_IN2, right_percent < 0 ? -right_percent : 0);
    }

    void ScheduleStop(int duration_ms) {
        if (stop_timer_ == nullptr) return;
        esp_timer_stop(stop_timer_);
        if (duration_ms > 0) {
            esp_timer_start_once(stop_timer_, (uint64_t)duration_ms * 1000ULL);
        }
    }

    void Move(int left, int right, int speed, int duration_ms) {
        if (MOTOR_SLEEP_GPIO != GPIO_NUM_NC) gpio_set_level(MOTOR_SLEEP_GPIO, 1);
        Drive(left * speed / 100, right * speed / 100);
        ScheduleStop(duration_ms);
    }

public:
    CopperMotorController() {
        ledc_timer_config_t timer_cfg = {};
        timer_cfg.speed_mode = kMode;
        timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
        timer_cfg.timer_num = kTimer;
        timer_cfg.freq_hz = 20000;
        timer_cfg.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

        InitPin(MOTOR_LEFT_IN1);
        InitPin(MOTOR_LEFT_IN2);
        InitPin(MOTOR_RIGHT_IN1);
        InitPin(MOTOR_RIGHT_IN2);

        if (MOTOR_SLEEP_GPIO != GPIO_NUM_NC) {
            gpio_config_t io = {};
            io.pin_bit_mask = (1ULL << MOTOR_SLEEP_GPIO);
            io.mode = GPIO_MODE_OUTPUT;
            ESP_ERROR_CHECK(gpio_config(&io));
            gpio_set_level(MOTOR_SLEEP_GPIO, 1);
        }

        esp_timer_create_args_t args = {};
        args.callback = [](void* ctx) {
            auto* self = static_cast<CopperMotorController*>(ctx);
            self->Drive(0, 0);
        };
        args.arg = this;
        args.name = "copper_stop";
        ESP_ERROR_CHECK(esp_timer_create(&args, &stop_timer_));

        auto& mcp = McpServer::GetInstance();
        auto props = [] {
            return PropertyList({
                Property("speed", kPropertyTypeInteger, 70, 10, 100),
                Property("duration_ms", kPropertyTypeInteger, 800, 100, 10000),
            });
        };

        mcp.AddTool("self.chassis.go_forward", "Robot di thang ve phia truoc", props(),
                    [this](const PropertyList& p) -> ReturnValue {
                        Move(100, 100, p["speed"].value<int>(), p["duration_ms"].value<int>());
                        return true;
                    });
        mcp.AddTool("self.chassis.go_back", "Robot di lui", props(),
                    [this](const PropertyList& p) -> ReturnValue {
                        Move(-100, -100, p["speed"].value<int>(), p["duration_ms"].value<int>());
                        return true;
                    });
        mcp.AddTool("self.chassis.turn_left", "Robot re trai (xoay tai cho)", props(),
                    [this](const PropertyList& p) -> ReturnValue {
                        Move(-100, 100, p["speed"].value<int>(), p["duration_ms"].value<int>());
                        return true;
                    });
        mcp.AddTool("self.chassis.turn_right", "Robot re phai (xoay tai cho)", props(),
                    [this](const PropertyList& p) -> ReturnValue {
                        Move(100, -100, p["speed"].value<int>(), p["duration_ms"].value<int>());
                        return true;
                    });
        mcp.AddTool("self.chassis.stop", "Robot dung lai ngay", PropertyList(),
                    [this](const PropertyList& p) -> ReturnValue {
                        if (stop_timer_) esp_timer_stop(stop_timer_);
                        Drive(0, 0);
                        return true;
                    });
    }
};

#endif
