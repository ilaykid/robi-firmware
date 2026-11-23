#ifndef ROBI_IMU_H_
#define ROBI_IMU_H_

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#ifdef ESP_PLATFORM
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace robi {

struct ImuSample {
    float ax = 0.0f;  // g
    float ay = 0.0f;  // g
    float az = 0.0f;  // g
    float gx = 0.0f;  // deg/s
    float gy = 0.0f;  // deg/s
    float gz = 0.0f;  // deg/s
};

enum class ForeheadAction : uint8_t {
    kNone = 0,
    kCorrectDown = 1,
    kSkipUp = 2,
};

struct ImuGestureEvents {
    bool enter_face_down_sleep = false;
    bool exit_face_down_sleep = false;
    bool shake_dizzy = false;
    ForeheadAction forehead_action = ForeheadAction::kNone;
};

// Pure, deterministic 6-axis gesture tracker (host-testable with ASan+UBSan, zero RTOS deps).
// Supports:
// 1. Sustained face-down detection on a flat surface -> instant Mute & Sleep, auto-wake on pickup.
// 2. Deliberate in-hand shake -> dizzy reaction (requires BOTH linear g-bursts AND gyro rotation
//    reversals so speaker audio vibrations or desk bumps never false-trigger).
// 3. 360-degree-roll-agnostic Forehead Tilt ("Heads Up!" / "נחש מה על המצח"): uses the screen
//    glass normal axis (`az * z_front_sign_`) so holding the round body at any rotation works!
class ImuGestureTracker {
public:
    void Reset() {
        face_down_ = false;
        face_down_hold_ms_ = 0;
        face_up_hold_ms_ = 0;
        shake_window_ms_ = 0;
        shake_peaks_ = 0;
        last_shake_dir_ = 0;
        shake_cooldown_ms_ = 0;
        forehead_mode_ = false;
        forehead_armed_ = false;
        forehead_neutral_ms_ = 0;
        forehead_down_ms_ = 0;
        forehead_up_ms_ = 0;
        forehead_refractory_ms_ = 0;
        boot_seeded_ = false;
    }

    void SetZFrontSign(float sign) {
        z_front_sign_ = (sign >= 0.0f) ? 1.0f : -1.0f;
        boot_seeded_ = true;
    }

    float z_front_sign() const { return z_front_sign_; }
    bool is_face_down() const { return face_down_; }
    bool is_forehead_armed() const { return forehead_armed_; }

    void SetForeheadMode(bool active) {
        if (forehead_mode_ == active) return;
        forehead_mode_ = active;
        forehead_armed_ = false;
        forehead_neutral_ms_ = 0;
        forehead_down_ms_ = 0;
        forehead_up_ms_ = 0;
        forehead_refractory_ms_ = 300;
    }

    // Touching the front AMOLED glass is physically impossible while lying flat face-down on a
    // table: if the screen is angled >18 deg from vertical while steady, lock `z_front_sign_`.
    void NotifyScreenTouched() {
        float mag = std::sqrt(last_sample_.ax * last_sample_.ax +
                              last_sample_.ay * last_sample_.ay +
                              last_sample_.az * last_sample_.az);
        float gyro = std::sqrt(last_sample_.gx * last_sample_.gx +
                               last_sample_.gy * last_sample_.gy +
                               last_sample_.gz * last_sample_.gz);
        if (mag > 0.75f && mag < 1.25f && gyro < 65.0f && std::fabs(last_sample_.az) > 0.32f) {
            z_front_sign_ = (last_sample_.az >= 0.0f) ? 1.0f : -1.0f;
            boot_seeded_ = true;
        }
    }

    ImuGestureEvents Step(const ImuSample& s, int dt_ms) {
        ImuGestureEvents ev = {};
        if (dt_ms <= 0) return ev;
        dt_ms = std::min(dt_ms, 200);
        last_sample_ = s;

        const float planar_g = std::sqrt(s.ax * s.ax + s.ay * s.ay);
        const float accel_mag = std::sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
        const float gyro_mag = std::sqrt(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);

        if (!boot_seeded_ && accel_mag > 0.80f && accel_mag < 1.20f && gyro_mag < 25.0f &&
            std::fabs(s.az) > 0.35f) {
            z_front_sign_ = (s.az >= 0.0f) ? 1.0f : -1.0f;
            boot_seeded_ = true;
        }

        // `screen_z`: +1.0g when front AMOLED glass faces ceiling, 0.0g when vertical,
        //             -1.0g when front AMOLED glass faces floor/desk.
        const float screen_z = s.az * z_front_sign_;

        if (shake_cooldown_ms_ > 0) {
            shake_cooldown_ms_ = std::max(0, shake_cooldown_ms_ - dt_ms);
        }

        // 1. Forehead Game Mode ("נחש מה על המצח"):
        //    Upright neutral (`|screen_z| < 0.42g`) arms the detector;
        //    Nodding glass toward floor (`screen_z < -0.62g`) = Correct;
        //    Tilting glass toward ceiling (`screen_z > +0.62g`) = Skip.
        if (forehead_mode_) {
            if (forehead_refractory_ms_ > 0) {
                forehead_refractory_ms_ = std::max(0, forehead_refractory_ms_ - dt_ms);
                return ev;
            }

            if (!forehead_armed_) {
                if (std::fabs(screen_z) < 0.42f && planar_g > 0.65f && accel_mag < 1.45f) {
                    forehead_neutral_ms_ += dt_ms;
                    if (forehead_neutral_ms_ >= 180) {
                        forehead_armed_ = true;
                        forehead_down_ms_ = 0;
                        forehead_up_ms_ = 0;
                    }
                } else {
                    forehead_neutral_ms_ = 0;
                }
                return ev;
            }

            if (screen_z < -0.60f && accel_mag > 0.65f && accel_mag < 1.65f) {
                forehead_down_ms_ += dt_ms;
                forehead_up_ms_ = 0;
                if (forehead_down_ms_ >= 110) {
                    ev.forehead_action = ForeheadAction::kCorrectDown;
                    forehead_armed_ = false;
                    forehead_neutral_ms_ = 0;
                    forehead_down_ms_ = 0;
                    forehead_refractory_ms_ = 650;
                }
            } else if (screen_z > 0.60f && accel_mag > 0.65f && accel_mag < 1.65f) {
                forehead_up_ms_ += dt_ms;
                forehead_down_ms_ = 0;
                if (forehead_up_ms_ >= 110) {
                    ev.forehead_action = ForeheadAction::kSkipUp;
                    forehead_armed_ = false;
                    forehead_neutral_ms_ = 0;
                    forehead_up_ms_ = 0;
                    forehead_refractory_ms_ = 650;
                }
            } else {
                forehead_down_ms_ = std::max(0, forehead_down_ms_ - dt_ms);
                forehead_up_ms_ = std::max(0, forehead_up_ms_ - dt_ms);
            }
            return ev;
        }

        // 2. Face-Down Mute & Sleep (resting steadily face-down on a flat surface)
        if (!face_down_) {
            if (screen_z < -0.78f && planar_g < 0.45f && gyro_mag < 40.0f &&
                accel_mag > 0.80f && accel_mag < 1.22f) {
                face_down_hold_ms_ += dt_ms;
                if (face_down_hold_ms_ >= 650) {
                    face_down_ = true;
                    face_down_hold_ms_ = 0;
                    face_up_hold_ms_ = 0;
                    ev.enter_face_down_sleep = true;
                    return ev;
                }
            } else {
                face_down_hold_ms_ = 0;
            }
        } else {
            if (screen_z > -0.45f || planar_g > 0.65f) {
                face_up_hold_ms_ += dt_ms;
                if (face_up_hold_ms_ >= 160) {
                    face_down_ = false;
                    face_up_hold_ms_ = 0;
                    face_down_hold_ms_ = 0;
                    ev.exit_face_down_sleep = true;
                    return ev;
                }
            } else {
                face_up_hold_ms_ = 0;
            }
            return ev;
        }

        // 3. Deliberate In-Hand Shake (requires strong linear g-force + strong rotational velocity
        //    with alternating sign reversals within 900ms; ignores desk bumps & speaker audio!)
        const float lin_dyn = std::fabs(accel_mag - 1.0f);
        if (shake_cooldown_ms_ == 0 && lin_dyn > 0.75f && gyro_mag > 165.0f) {
            float dom_axis = (std::fabs(s.ax) >= std::fabs(s.ay)) ? s.ax : s.ay;
            int dir = (dom_axis >= 0.0f) ? 1 : -1;
            if (shake_peaks_ == 0 || dir != last_shake_dir_) {
                ++shake_peaks_;
                last_shake_dir_ = dir;
            }
            shake_window_ms_ = 0;
            if (shake_peaks_ >= 4) {
                ev.shake_dizzy = true;
                shake_peaks_ = 0;
                last_shake_dir_ = 0;
                shake_cooldown_ms_ = 6500;
            }
        } else if (shake_peaks_ > 0) {
            shake_window_ms_ += dt_ms;
            if (shake_window_ms_ > 550) {
                shake_peaks_ = 0;
                last_shake_dir_ = 0;
                shake_window_ms_ = 0;
            }
        }

        return ev;
    }

private:
    ImuSample last_sample_ = {};
    float z_front_sign_ = 1.0f;
    bool boot_seeded_ = false;
    bool face_down_ = false;
    int face_down_hold_ms_ = 0;
    int face_up_hold_ms_ = 0;

    int shake_window_ms_ = 0;
    int shake_peaks_ = 0;
    int last_shake_dir_ = 0;
    int shake_cooldown_ms_ = 0;

    bool forehead_mode_ = false;
    bool forehead_armed_ = false;
    int forehead_neutral_ms_ = 0;
    int forehead_down_ms_ = 0;
    int forehead_up_ms_ = 0;
    int forehead_refractory_ms_ = 0;
};

#ifdef ESP_PLATFORM

// Non-panicking QMI8658 6-axis IMU service sharing the board's existing I2C master bus.
class RobiImuService {
public:
    static RobiImuService& GetInstance() {
        static RobiImuService instance;
        return instance;
    }

    bool Initialize(i2c_master_bus_handle_t bus) {
        if (initialized_.load(std::memory_order_relaxed) || bus == nullptr) {
            return available_.load(std::memory_order_relaxed);
        }
        initialized_.store(true, std::memory_order_relaxed);

        const uint8_t candidates[2] = {0x6B, 0x6A};
        for (uint8_t addr : candidates) {
            if (i2c_master_probe(bus, addr, 30) != ESP_OK) {
                continue;
            }
            i2c_device_config_t dev_cfg = {};
            dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            dev_cfg.device_address = addr;
            dev_cfg.scl_speed_hz = 100 * 1000;  // Safe 100kHz matching CST9217 touch controller
            i2c_master_dev_handle_t dev = nullptr;
            if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) != ESP_OK || dev == nullptr) {
                continue;
            }

            uint8_t who = 0;
            if (!ReadRegs(dev, 0x00, &who, 1) || who != 0x05) {
                i2c_master_bus_rm_device(dev);
                continue;
            }

            // Configure QMI8658:
            // CTRL1 (0x02) = 0x60 (address auto-increment + little-endian serial)
            // CTRL2 (0x03) = 0x15 (+-4g full scale = 8192 LSB/g, 250 Hz ODR)
            // CTRL3 (0x04) = 0x55 (+-512 dps full scale = 64 LSB/(deg/s), 250 Hz ODR)
            // CTRL5 (0x06) = 0x11 (enable low-pass filters on Accel & Gyro)
            // CTRL7 (0x08) = 0x03 (enable Accelerometer + Gyroscope)
            WriteReg(dev, 0x02, 0x60);
            WriteReg(dev, 0x03, 0x15);
            WriteReg(dev, 0x04, 0x55);
            WriteReg(dev, 0x06, 0x11);
            WriteReg(dev, 0x08, 0x03);

            dev_ = dev;
            addr_ = addr;
            available_.store(true, std::memory_order_release);
            ESP_LOGI("RobiImu", "QMI8658 6-axis IMU active at I2C 0x%02X (low-prio task)", addr);

            // Run synchronous 100kHz I2C polling on a low-priority task (prio 2) instead of
            // Priority-22 ESP_TIMER_TASK so IMU I2C reads never delay timers or audio DMA!
            xTaskCreate(
                [](void* arg) { static_cast<RobiImuService*>(arg)->PollTaskLoop(); },
                "robi_imu", 3072, this, 2, &poll_task_);
            return true;
        }

        ESP_LOGW("RobiImu", "QMI8658 IMU not detected on I2C bus (motion features inactive)");
        return false;
    }

    bool IsAvailable() const { return available_.load(std::memory_order_acquire); }
    uint8_t GetAddress() const { return addr_; }
    bool IsFaceDown() const { return face_down_.load(std::memory_order_relaxed); }
    float GetZFrontSign() const { return z_sign_cached_.load(std::memory_order_relaxed); }

    void SetForeheadMode(bool active) {
        forehead_mode_req_.store(active, std::memory_order_release);
    }

    void NotifyScreenTouched() {
        touch_cal_req_.store(true, std::memory_order_release);
    }

    void OverrideZFrontSign(float sign) {
        override_z_sign_.store((sign >= 0.0f) ? 1 : -1, std::memory_order_release);
    }

    ImuSample GetLastSample() const {
        ImuSample s;
        s.ax = ax_mg_.load(std::memory_order_relaxed) / 1000.0f;
        s.ay = ay_mg_.load(std::memory_order_relaxed) / 1000.0f;
        s.az = az_mg_.load(std::memory_order_relaxed) / 1000.0f;
        s.gx = gx_dps10_.load(std::memory_order_relaxed) / 10.0f;
        s.gy = gy_dps10_.load(std::memory_order_relaxed) / 10.0f;
        s.gz = gz_dps10_.load(std::memory_order_relaxed) / 10.0f;
        return s;
    }

    ImuGestureEvents ConsumePendingEvents() {
        ImuGestureEvents ev;
        ev.enter_face_down_sleep = pending_enter_sleep_.exchange(false, std::memory_order_acq_rel);
        ev.exit_face_down_sleep = pending_exit_sleep_.exchange(false, std::memory_order_acq_rel);
        ev.shake_dizzy = pending_shake_.exchange(false, std::memory_order_acq_rel);
        ev.forehead_action = static_cast<ForeheadAction>(
            pending_forehead_.exchange(0, std::memory_order_acq_rel));
        return ev;
    }

private:
    RobiImuService() = default;

    static bool WriteReg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
        uint8_t buf[2] = {reg, val};
        return i2c_master_transmit(dev, buf, sizeof(buf), 25) == ESP_OK;
    }

    static bool ReadRegs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* out, size_t len) {
        return i2c_master_transmit_receive(dev, &reg, 1, out, len, 25) == ESP_OK;
    }

    void PollTick() {
        if (dev_ == nullptr) return;
        uint8_t raw[12] = {};
        if (!ReadRegs(dev_, 0x35, raw, sizeof(raw))) {
            return;
        }
        auto rd16 = [&](int idx) -> int16_t {
            uint16_t u = static_cast<uint16_t>(raw[idx]) |
                         (static_cast<uint16_t>(raw[idx + 1]) << 8);
            return static_cast<int16_t>(u);
        };

        ImuSample s;
        s.ax = static_cast<float>(rd16(0)) / 8192.0f;
        s.ay = static_cast<float>(rd16(2)) / 8192.0f;
        s.az = static_cast<float>(rd16(4)) / 8192.0f;
        s.gx = static_cast<float>(rd16(6)) / 64.0f;
        s.gy = static_cast<float>(rd16(8)) / 64.0f;
        s.gz = static_cast<float>(rd16(10)) / 64.0f;

        ax_mg_.store(static_cast<int>(s.ax * 1000.0f), std::memory_order_relaxed);
        ay_mg_.store(static_cast<int>(s.ay * 1000.0f), std::memory_order_relaxed);
        az_mg_.store(static_cast<int>(s.az * 1000.0f), std::memory_order_relaxed);
        gx_dps10_.store(static_cast<int>(s.gx * 10.0f), std::memory_order_relaxed);
        gy_dps10_.store(static_cast<int>(s.gy * 10.0f), std::memory_order_relaxed);
        gz_dps10_.store(static_cast<int>(s.gz * 10.0f), std::memory_order_relaxed);

        int ov = override_z_sign_.exchange(0, std::memory_order_acq_rel);
        if (ov != 0) {
            tracker_.SetZFrontSign(static_cast<float>(ov));
        }
        if (touch_cal_req_.exchange(false, std::memory_order_acq_rel)) {
            tracker_.NotifyScreenTouched();
        }
        tracker_.SetForeheadMode(forehead_mode_req_.load(std::memory_order_acquire));

        int64_t now_us = esp_timer_get_time();
        int dt_ms = (last_poll_us_ == 0)
                        ? 40
                        : static_cast<int>((now_us - last_poll_us_) / 1000LL);
        last_poll_us_ = now_us;

        auto ev = tracker_.Step(s, dt_ms);
        face_down_.store(tracker_.is_face_down(), std::memory_order_relaxed);
        z_sign_cached_.store(tracker_.z_front_sign(), std::memory_order_relaxed);

        if (ev.enter_face_down_sleep) {
            pending_enter_sleep_.store(true, std::memory_order_release);
        }
        if (ev.exit_face_down_sleep) {
            pending_exit_sleep_.store(true, std::memory_order_release);
        }
        if (ev.shake_dizzy) {
            pending_shake_.store(true, std::memory_order_release);
        }
        if (ev.forehead_action != ForeheadAction::kNone) {
            pending_forehead_.store(static_cast<uint8_t>(ev.forehead_action),
                                    std::memory_order_release);
        }
    }

    void PollTaskLoop() {
        while (available_.load(std::memory_order_acquire)) {
            PollTick();
            const uint32_t delay_ms = forehead_mode_req_.load(std::memory_order_relaxed) ? 40u : 60u;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        vTaskDelete(nullptr);
    }

    i2c_master_dev_handle_t dev_ = nullptr;
    TaskHandle_t poll_task_ = nullptr;
    uint8_t addr_ = 0;
    int64_t last_poll_us_ = 0;
    ImuGestureTracker tracker_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> available_{false};
    std::atomic<bool> face_down_{false};
    std::atomic<bool> forehead_mode_req_{false};
    std::atomic<bool> touch_cal_req_{false};
    std::atomic<int> override_z_sign_{0};
    std::atomic<float> z_sign_cached_{1.0f};

    std::atomic<int> ax_mg_{0};
    std::atomic<int> ay_mg_{0};
    std::atomic<int> az_mg_{0};
    std::atomic<int> gx_dps10_{0};
    std::atomic<int> gy_dps10_{0};
    std::atomic<int> gz_dps10_{0};

    std::atomic<bool> pending_enter_sleep_{false};
    std::atomic<bool> pending_exit_sleep_{false};
    std::atomic<bool> pending_shake_{false};
    std::atomic<uint8_t> pending_forehead_{0};
};

#endif  // ESP_PLATFORM

}  // namespace robi

#endif  // ROBI_IMU_H_
