
    // ===================== Robi Cyber-Optic UI v2.8 (inserted by scripts/patch_xiaozhi.py) =====================
    // Live 30-FPS procedural sci-fi cyber-optic face + dual-ring holographic HUD + syllable-synced voice waveform,
    // plus Robi's idle pet bars, Lavi & Ori profile/activity menus, and tap-to-finish / tap-to-stop overlay.

    virtual void SetTheme(Theme* theme) override {
        SpiLcdDisplay::SetTheme(theme);
        DisplayLockGuard lock(this);
        lv_obj_t* scr = lv_screen_active();
        if (scr != nullptr) {
            lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        }
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        }
        if (top_bar_ != nullptr) {
            lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
        }
        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    virtual void SetStatus(const char* status) override {
        SpiLcdDisplay::SetStatus(status);
        DisplayLockGuard lock(this);
        if (robi_profiles_ == nullptr) return;
        bool idle = status != nullptr && strcmp(status, Lang::Strings::STANDBY) == 0;
        bool listening = status != nullptr && strcmp(status, Lang::Strings::LISTENING) == 0;
        bool speaking = status != nullptr && strcmp(status, Lang::Strings::SPEAKING) == 0;

        if (idle) {
            robi_face_mode_ = ROBI_MODE_IDLE;
            robi_voice_env_ = 0.0f;
            RobiApplyPreset("neutral", false);
            bool menu_open = (robi_menu_lavi_ && !lv_obj_has_flag(robi_menu_lavi_, LV_OBJ_FLAG_HIDDEN)) ||
                             (robi_menu_ori_ && !lv_obj_has_flag(robi_menu_ori_, LV_OBJ_FLAG_HIDDEN));
            if (!menu_open) lv_obj_remove_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);
            if (robi_pet_ok_) lv_obj_remove_flag(robi_pet_, LV_OBJ_FLAG_HIDDEN);
            RobiRequestPet();
        } else {
            if (listening) {
                robi_face_mode_ = ROBI_MODE_LISTENING;
                robi_voice_env_ = 0.0f;
                RobiApplyPreset("listening", false);
            } else if (speaking) {
                robi_face_mode_ = ROBI_MODE_SPEAKING;
            } else {
                robi_face_mode_ = ROBI_MODE_THINKING;
            }
            lv_obj_add_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(robi_menu_lavi_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(robi_menu_ori_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(robi_pet_, LV_OBJ_FLAG_HIDDEN);
            RobiStopMenuTimer();
        }
        if (listening || speaking) {
            lv_obj_remove_flag(robi_tap_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(robi_tap_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    virtual void SetEmotion(const char* emotion) override {
        DisplayLockGuard lock(this);
        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emotion == nullptr || emotion[0] == '\0' || !robi_face_ready_) {
            return;
        }
        // Real-time syllable voice envelope ("v:0" .. "v:100") synced with Opus playback
        if (emotion[0] == 'v' && emotion[1] == ':') {
            int lvl = atoi(emotion + 2);
            if (lvl < 0) lvl = 0;
            if (lvl > 100) lvl = 100;
            robi_voice_env_ = static_cast<float>(lvl) / 100.0f;
            robi_voice_env_ts_ = esp_timer_get_time();
            return;
        }
        const char* mapped = emotion;
        if (robi_face_mode_ == ROBI_MODE_LISTENING && strcmp(emotion, "neutral") == 0) {
            mapped = "listening";
        } else if (strcmp(emotion, "thinking") == 0 && robi_face_mode_ != ROBI_MODE_SPEAKING) {
            robi_face_mode_ = ROBI_MODE_THINKING;
        }
        RobiApplyPreset(mapped, true);
    }

private:
    enum RobiFaceMode {
        ROBI_MODE_IDLE = 0,
        ROBI_MODE_LISTENING = 1,
        ROBI_MODE_THINKING = 2,
        ROBI_MODE_SPEAKING = 3,
    };

    struct FaceParams {
        float eye_w;
        float eye_h;
        float eye_radius;
        float pupil_scale;
        float gaze_x;
        float gaze_y;
        float squint_top_l;
        float squint_top_r;
        float squint_bot_l;
        float squint_bot_r;
        float brow_ang_l;
        float brow_ang_r;
        float brow_y_l;
        float brow_y_r;
        float brow_opa;
        float mouth_w;
        float mouth_curve;
        float mouth_tilt;
        float mouth_wave;
        float mouth_o;
        float cheek_opa;
        float visor_opa;
        float col_r;
        float col_g;
        float col_b;
    };

    struct CyberEyeWidgets {
        lv_obj_t* glow_outer = nullptr;
        lv_obj_t* glow_inner = nullptr;
        lv_obj_t* core = nullptr;
        lv_obj_t* iris = nullptr;
        lv_obj_t* pupil = nullptr;
        lv_obj_t* scanline = nullptr;
        lv_obj_t* spec_main = nullptr;
        lv_obj_t* spec_sub = nullptr;
        lv_obj_t* lid_top = nullptr;
        lv_obj_t* lid_bot = nullptr;
        lv_obj_t* closed_slit = nullptr;
    };

    inline static CustomLcdDisplay* robi_self_ = nullptr;
    lv_obj_t* robi_profiles_ = nullptr;
    lv_obj_t* robi_menu_lavi_ = nullptr;
    lv_obj_t* robi_menu_ori_ = nullptr;
    lv_obj_t* robi_tap_ = nullptr;
    lv_obj_t* robi_pet_ = nullptr;
    lv_obj_t* robi_bar_energy_ = nullptr;
    lv_obj_t* robi_bar_joy_ = nullptr;
    lv_timer_t* robi_menu_timer_ = nullptr;
    volatile bool robi_pet_busy_ = false;
    bool robi_pet_ok_ = false;
    int64_t robi_pet_last_ = 0;

    // Cyber-Optic Face widgets & state
    bool robi_face_ready_ = false;
    RobiFaceMode robi_face_mode_ = ROBI_MODE_IDLE;
    char robi_last_emotion_[24] = "neutral";
    lv_timer_t* robi_face_timer_ = nullptr;
    int64_t robi_last_tick_us_ = 0;

    lv_obj_t* hud_track_ = nullptr;
    lv_obj_t* hud_outer_a_ = nullptr;
    lv_obj_t* hud_outer_b_ = nullptr;
    lv_obj_t* hud_inner_a_ = nullptr;
    lv_obj_t* hud_inner_b_ = nullptr;

    CyberEyeWidgets eye_l_;
    CyberEyeWidgets eye_r_;
    lv_obj_t* cheek_l_ = nullptr;
    lv_obj_t* cheek_r_ = nullptr;
    lv_obj_t* visor_bar_ = nullptr;

    lv_obj_t* brow_glow_l_ = nullptr;
    lv_obj_t* brow_core_l_ = nullptr;
    lv_obj_t* brow_glow_r_ = nullptr;
    lv_obj_t* brow_core_r_ = nullptr;
    lv_point_precise_t brow_pts_l_[3] = {};
    lv_point_precise_t brow_pts_r_[3] = {};

    lv_obj_t* mouth_glow_ = nullptr;
    lv_obj_t* mouth_core_ = nullptr;
    lv_obj_t* mouth_ring_ = nullptr;
    lv_point_precise_t mouth_pts_[9] = {};
    lv_obj_t* voice_bars_[7] = {};

    FaceParams cur_ = {};
    FaceParams tgt_ = {};

    float face_scale_ = 0.86f;
    float face_y_ = 220.0f;
    float talk_mix_ = 0.0f;
    float robi_voice_env_ = 0.0f;
    float robi_voice_smooth_ = 0.0f;
    int64_t robi_voice_env_ts_ = 0;

    float hud_angle_ = 0.0f;
    float hud_span_ = 44.0f;
    float phase_t_ = 0.0f;

    // Natural blink & micro-saccade state
    float blink_phase_ = -1.0f;
    float next_blink_s_ = 2.4f;
    bool pending_double_blink_ = false;
    float saccade_x_ = 0.0f;
    float saccade_y_ = 0.0f;
    float saccade_tgt_x_ = 0.0f;
    float saccade_tgt_y_ = 0.0f;
    float next_saccade_s_ = 1.8f;
    uint32_t last_primary_hex_ = 0xFFFFFFFFu;
    bool bounce_laugh_ = false;
    bool tremor_shock_ = false;

    static float RobiClamp(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    static float RobiLerp(float a, float b, float k) {
        return a + (b - a) * k;
    }

    static uint32_t RobiPackRgb(float r, float g, float b) {
        int ri = static_cast<int>(RobiClamp(r, 0.0f, 255.0f));
        int gi = static_cast<int>(RobiClamp(g, 0.0f, 255.0f));
        int bi = static_cast<int>(RobiClamp(b, 0.0f, 255.0f));
        return (static_cast<uint32_t>(ri) << 16) | (static_cast<uint32_t>(gi) << 8) | static_cast<uint32_t>(bi);
    }

    static lv_color_t RobiTintBright(float r, float g, float b, float white_mix) {
        return lv_color_hex(RobiPackRgb(
            r + (255.0f - r) * white_mix,
            g + (255.0f - g) * white_mix,
            b + (255.0f - b) * white_mix));
    }

    static FaceParams RobiDefaultParams() {
        FaceParams p = {};
        p.eye_w = 106.0f;
        p.eye_h = 96.0f;
        p.eye_radius = 34.0f;
        p.pupil_scale = 0.25f;
        p.gaze_x = 0.0f;
        p.gaze_y = 0.0f;
        p.squint_top_l = 0.0f;
        p.squint_top_r = 0.0f;
        p.squint_bot_l = 0.0f;
        p.squint_bot_r = 0.0f;
        p.brow_ang_l = 0.0f;
        p.brow_ang_r = 0.0f;
        p.brow_y_l = 0.0f;
        p.brow_y_r = 0.0f;
        p.brow_opa = 0.45f;
        p.mouth_w = 38.0f;
        p.mouth_curve = 10.0f;
        p.mouth_tilt = 0.0f;
        p.mouth_wave = 0.0f;
        p.mouth_o = 0.0f;
        p.cheek_opa = 0.0f;
        p.visor_opa = 0.0f;
        p.col_r = 0.0f;
        p.col_g = 229.0f;
        p.col_b = 255.0f;
        return p;
    }

    void RobiSetTargetRgb(uint32_t hex) {
        tgt_.col_r = static_cast<float>((hex >> 16) & 0xFF);
        tgt_.col_g = static_cast<float>((hex >> 8) & 0xFF);
        tgt_.col_b = static_cast<float>(hex & 0xFF);
    }

    void RobiApplyPreset(const char* name, bool allow_transition_blink) {
        if (name == nullptr) return;
        bool changed = (strncmp(robi_last_emotion_, name, sizeof(robi_last_emotion_) - 1) != 0);
        strncpy(robi_last_emotion_, name, sizeof(robi_last_emotion_) - 1);
        robi_last_emotion_[sizeof(robi_last_emotion_) - 1] = '\0';

        tgt_ = RobiDefaultParams();
        bounce_laugh_ = false;
        tremor_shock_ = false;

        if (strcmp(name, "listening") == 0) {
            RobiSetTargetRgb(0x00FFBF);
            tgt_.eye_w = 112.0f;
            tgt_.eye_h = 104.0f;
            tgt_.pupil_scale = 0.30f;
            tgt_.gaze_y = -2.0f;
            tgt_.brow_y_l = -5.0f;
            tgt_.brow_y_r = -5.0f;
            tgt_.brow_opa = 0.80f;
            tgt_.mouth_w = 34.0f;
            tgt_.mouth_curve = 9.0f;
        } else if (strcmp(name, "thinking") == 0) {
            RobiSetTargetRgb(0x8B5CF6);
            tgt_.eye_w = 104.0f;
            tgt_.eye_h = 92.0f;
            tgt_.gaze_x = 15.0f;
            tgt_.gaze_y = -11.0f;
            tgt_.squint_bot_l = 0.20f;
            tgt_.squint_top_r = 0.08f;
            tgt_.brow_ang_l = 7.0f;
            tgt_.brow_ang_r = 12.0f;
            tgt_.brow_y_l = 2.0f;
            tgt_.brow_y_r = -9.0f;
            tgt_.brow_opa = 0.95f;
            tgt_.mouth_w = 30.0f;
            tgt_.mouth_curve = -2.0f;
            tgt_.mouth_tilt = 6.0f;
        } else if (strcmp(name, "happy") == 0) {
            RobiSetTargetRgb(0x00F2FE);
            tgt_.eye_w = 110.0f;
            tgt_.eye_h = 98.0f;
            tgt_.squint_bot_l = 0.50f;
            tgt_.squint_bot_r = 0.50f;
            tgt_.brow_y_l = -5.0f;
            tgt_.brow_y_r = -5.0f;
            tgt_.brow_opa = 0.70f;
            tgt_.mouth_w = 50.0f;
            tgt_.mouth_curve = 24.0f;
            tgt_.cheek_opa = 0.32f;
        } else if (strcmp(name, "laughing") == 0 || strcmp(name, "funny") == 0) {
            RobiSetTargetRgb(0x38F9D7);
            bounce_laugh_ = true;
            tgt_.eye_w = 114.0f;
            tgt_.eye_h = 96.0f;
            tgt_.squint_bot_l = 0.68f;
            tgt_.squint_bot_r = 0.68f;
            tgt_.brow_y_l = -7.0f;
            tgt_.brow_y_r = -7.0f;
            tgt_.brow_opa = 0.85f;
            tgt_.mouth_w = 58.0f;
            tgt_.mouth_curve = 32.0f;
            tgt_.cheek_opa = 0.55f;
        } else if (strcmp(name, "loving") == 0) {
            RobiSetTargetRgb(0xFF4DA6);
            tgt_.eye_w = 112.0f;
            tgt_.eye_h = 102.0f;
            tgt_.pupil_scale = 0.34f;
            tgt_.squint_bot_l = 0.46f;
            tgt_.squint_bot_r = 0.46f;
            tgt_.brow_ang_l = -6.0f;
            tgt_.brow_ang_r = 6.0f;
            tgt_.brow_y_l = -4.0f;
            tgt_.brow_y_r = -4.0f;
            tgt_.brow_opa = 0.75f;
            tgt_.mouth_w = 48.0f;
            tgt_.mouth_curve = 25.0f;
            tgt_.cheek_opa = 0.88f;
        } else if (strcmp(name, "kissy") == 0) {
            RobiSetTargetRgb(0xFF6B9D);
            tgt_.squint_bot_l = 0.20f;
            tgt_.squint_bot_r = 0.74f;
            tgt_.squint_top_r = 0.18f;
            tgt_.brow_y_l = -6.0f;
            tgt_.brow_opa = 0.75f;
            tgt_.mouth_w = 18.0f;
            tgt_.mouth_o = 0.88f;
            tgt_.cheek_opa = 0.80f;
        } else if (strcmp(name, "surprised") == 0) {
            RobiSetTargetRgb(0x5CE1E6);
            tgt_.eye_w = 116.0f;
            tgt_.eye_h = 116.0f;
            tgt_.eye_radius = 58.0f;
            tgt_.pupil_scale = 0.20f;
            tgt_.brow_y_l = -14.0f;
            tgt_.brow_y_r = -14.0f;
            tgt_.brow_opa = 1.0f;
            tgt_.mouth_w = 26.0f;
            tgt_.mouth_o = 0.92f;
        } else if (strcmp(name, "shocked") == 0) {
            RobiSetTargetRgb(0xFFB703);
            tremor_shock_ = true;
            tgt_.eye_w = 120.0f;
            tgt_.eye_h = 120.0f;
            tgt_.eye_radius = 60.0f;
            tgt_.pupil_scale = 0.16f;
            tgt_.brow_y_l = -18.0f;
            tgt_.brow_y_r = -18.0f;
            tgt_.brow_opa = 1.0f;
            tgt_.mouth_w = 34.0f;
            tgt_.mouth_o = 1.0f;
        } else if (strcmp(name, "winking") == 0) {
            RobiSetTargetRgb(0x00F5D4);
            tgt_.eye_w = 108.0f;
            tgt_.eye_h = 100.0f;
            tgt_.squint_bot_r = 0.76f;
            tgt_.squint_top_r = 0.20f;
            tgt_.brow_y_l = -8.0f;
            tgt_.brow_y_r = 3.0f;
            tgt_.brow_opa = 0.90f;
            tgt_.mouth_w = 46.0f;
            tgt_.mouth_curve = 18.0f;
            tgt_.mouth_tilt = 11.0f;
            tgt_.cheek_opa = 0.35f;
        } else if (strcmp(name, "cool") == 0) {
            RobiSetTargetRgb(0x00E5FF);
            tgt_.squint_top_l = 0.22f;
            tgt_.squint_top_r = 0.22f;
            tgt_.brow_ang_l = 6.0f;
            tgt_.brow_ang_r = -6.0f;
            tgt_.brow_opa = 0.85f;
            tgt_.visor_opa = 0.92f;
            tgt_.mouth_w = 46.0f;
            tgt_.mouth_curve = 14.0f;
            tgt_.mouth_tilt = 10.0f;
        } else if (strcmp(name, "confident") == 0) {
            RobiSetTargetRgb(0x00BBF9);
            tgt_.squint_top_l = 0.20f;
            tgt_.squint_top_r = 0.20f;
            tgt_.squint_bot_l = 0.18f;
            tgt_.squint_bot_r = 0.18f;
            tgt_.brow_ang_l = 9.0f;
            tgt_.brow_ang_r = -4.0f;
            tgt_.brow_y_r = -6.0f;
            tgt_.brow_opa = 0.95f;
            tgt_.mouth_w = 48.0f;
            tgt_.mouth_curve = 16.0f;
            tgt_.mouth_tilt = 10.0f;
        } else if (strcmp(name, "confused") == 0) {
            RobiSetTargetRgb(0xFBBF24);
            tgt_.gaze_x = -9.0f;
            tgt_.gaze_y = -4.0f;
            tgt_.squint_top_l = 0.32f;
            tgt_.brow_ang_l = 12.0f;
            tgt_.brow_ang_r = 10.0f;
            tgt_.brow_y_l = 4.0f;
            tgt_.brow_y_r = -10.0f;
            tgt_.brow_opa = 1.0f;
            tgt_.mouth_w = 42.0f;
            tgt_.mouth_curve = -3.0f;
            tgt_.mouth_wave = 6.0f;
        } else if (strcmp(name, "silly") == 0) {
            RobiSetTargetRgb(0x80ED99);
            bounce_laugh_ = true;
            tgt_.gaze_x = 8.0f;
            tgt_.squint_bot_l = 0.56f;
            tgt_.squint_bot_r = 0.12f;
            tgt_.brow_y_l = 2.0f;
            tgt_.brow_y_r = -10.0f;
            tgt_.brow_opa = 0.90f;
            tgt_.mouth_w = 52.0f;
            tgt_.mouth_curve = 22.0f;
            tgt_.mouth_wave = 4.5f;
            tgt_.cheek_opa = 0.40f;
        } else if (strcmp(name, "embarrassed") == 0) {
            RobiSetTargetRgb(0xFB7185);
            tgt_.gaze_x = -12.0f;
            tgt_.gaze_y = 9.0f;
            tgt_.squint_bot_l = 0.28f;
            tgt_.squint_bot_r = 0.28f;
            tgt_.brow_ang_l = -10.0f;
            tgt_.brow_ang_r = 10.0f;
            tgt_.brow_opa = 0.90f;
            tgt_.mouth_w = 28.0f;
            tgt_.mouth_wave = 4.0f;
            tgt_.cheek_opa = 0.92f;
        } else if (strcmp(name, "relaxed") == 0 || strcmp(name, "delicious") == 0) {
            RobiSetTargetRgb(0x2EC4B6);
            tgt_.squint_bot_l = 0.60f;
            tgt_.squint_bot_r = 0.60f;
            tgt_.squint_top_l = 0.14f;
            tgt_.squint_top_r = 0.14f;
            tgt_.brow_opa = 0.55f;
            tgt_.mouth_w = 44.0f;
            tgt_.mouth_curve = 20.0f;
            tgt_.cheek_opa = 0.35f;
        } else if (strcmp(name, "sad") == 0) {
            RobiSetTargetRgb(0x3A86FF);
            tgt_.gaze_y = 11.0f;
            tgt_.squint_top_l = 0.34f;
            tgt_.squint_top_r = 0.34f;
            tgt_.brow_ang_l = -14.0f;
            tgt_.brow_ang_r = 14.0f;
            tgt_.brow_y_l = -2.0f;
            tgt_.brow_y_r = -2.0f;
            tgt_.brow_opa = 1.0f;
            tgt_.mouth_w = 36.0f;
            tgt_.mouth_curve = -15.0f;
        } else if (strcmp(name, "crying") == 0) {
            RobiSetTargetRgb(0x4361EE);
            tgt_.gaze_y = 10.0f;
            tgt_.squint_top_l = 0.28f;
            tgt_.squint_top_r = 0.28f;
            tgt_.squint_bot_l = 0.42f;
            tgt_.squint_bot_r = 0.42f;
            tgt_.brow_ang_l = -16.0f;
            tgt_.brow_ang_r = 16.0f;
            tgt_.brow_opa = 1.0f;
            tgt_.mouth_w = 40.0f;
            tgt_.mouth_curve = -20.0f;
            tgt_.mouth_wave = 2.5f;
        } else if (strcmp(name, "angry") == 0) {
            RobiSetTargetRgb(0xFF3366);
            tgt_.squint_top_l = 0.34f;
            tgt_.squint_top_r = 0.34f;
            tgt_.squint_bot_l = 0.16f;
            tgt_.squint_bot_r = 0.16f;
            tgt_.brow_ang_l = 18.0f;
            tgt_.brow_ang_r = -18.0f;
            tgt_.brow_y_l = 5.0f;
            tgt_.brow_y_r = 5.0f;
            tgt_.brow_opa = 1.0f;
            tgt_.mouth_w = 40.0f;
            tgt_.mouth_curve = -12.0f;
        } else if (strcmp(name, "sleepy") == 0) {
            RobiSetTargetRgb(0x6366F1);
            tgt_.gaze_y = 6.0f;
            tgt_.squint_top_l = 0.72f;
            tgt_.squint_top_r = 0.72f;
            tgt_.brow_opa = 0.40f;
            tgt_.mouth_w = 16.0f;
            tgt_.mouth_o = 0.55f;
        }

        // Natural Pixar/Vector-style transition micro-blink on major expression shift
        if (allow_transition_blink && changed && blink_phase_ < 0.0f &&
            strcmp(name, "surprised") != 0 && strcmp(name, "shocked") != 0) {
            blink_phase_ = 0.0f;
        }
    }

    static lv_obj_t* RobiPlainObj(lv_obj_t* parent) {
        lv_obj_t* o = lv_obj_create(parent);
        lv_obj_set_style_pad_all(o, 0, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_shadow_width(o, 0, 0);
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        return o;
    }

    static lv_obj_t* RobiCreateHudArc(lv_obj_t* parent, int diameter, int width, lv_opa_t opa) {
        lv_obj_t* arc = lv_arc_create(parent);
        lv_obj_set_size(arc, diameter, diameter);
        lv_obj_center(arc);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_angles(arc, 0, 45);
        lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc, opa, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        return arc;
    }

    void RobiCreateEye(lv_obj_t* parent, CyberEyeWidgets& e) {
        e.glow_outer = RobiPlainObj(parent);
        lv_obj_set_style_bg_opa(e.glow_outer, LV_OPA_20, 0);

        e.glow_inner = RobiPlainObj(parent);
        lv_obj_set_style_bg_opa(e.glow_inner, LV_OPA_40, 0);

        e.core = RobiPlainObj(parent);
        lv_obj_set_style_bg_opa(e.core, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_grad_dir(e.core, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(e.core, 2, 0);
        lv_obj_set_style_border_opa(e.core, LV_OPA_80, 0);
        lv_obj_set_style_clip_corner(e.core, true, 0);

        e.iris = RobiPlainObj(e.core);
        lv_obj_set_style_bg_opa(e.iris, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(e.iris, 3, 0);
        lv_obj_set_style_border_opa(e.iris, LV_OPA_50, 0);
        lv_obj_set_style_radius(e.iris, LV_RADIUS_CIRCLE, 0);

        e.pupil = RobiPlainObj(e.core);
        lv_obj_set_style_bg_color(e.pupil, lv_color_hex(0x021324), 0);
        lv_obj_set_style_bg_opa(e.pupil, LV_OPA_50, 0);
        lv_obj_set_style_border_width(e.pupil, 2, 0);
        lv_obj_set_style_border_color(e.pupil, lv_color_white(), 0);
        lv_obj_set_style_border_opa(e.pupil, LV_OPA_40, 0);
        lv_obj_set_style_radius(e.pupil, LV_RADIUS_CIRCLE, 0);

        e.scanline = RobiPlainObj(e.core);
        lv_obj_set_style_bg_color(e.scanline, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(e.scanline, LV_OPA_30, 0);
        lv_obj_set_style_radius(e.scanline, 2, 0);

        e.spec_main = RobiPlainObj(e.core);
        lv_obj_set_size(e.spec_main, 20, 20);
        lv_obj_set_style_bg_color(e.spec_main, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(e.spec_main, LV_OPA_90, 0);
        lv_obj_set_style_radius(e.spec_main, LV_RADIUS_CIRCLE, 0);

        e.spec_sub = RobiPlainObj(e.core);
        lv_obj_set_size(e.spec_sub, 9, 9);
        lv_obj_set_style_bg_color(e.spec_sub, lv_color_hex(0xE0FFFF), 0);
        lv_obj_set_style_bg_opa(e.spec_sub, LV_OPA_70, 0);
        lv_obj_set_style_radius(e.spec_sub, LV_RADIUS_CIRCLE, 0);

        // Pitch-black AMOLED shutters to sculpt blinks and happy crescent arches seamlessly
        e.lid_top = RobiPlainObj(parent);
        lv_obj_set_style_bg_color(e.lid_top, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(e.lid_top, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(e.lid_top, 16, 0);
        lv_obj_add_flag(e.lid_top, LV_OBJ_FLAG_HIDDEN);

        e.lid_bot = RobiPlainObj(parent);
        lv_obj_set_style_bg_color(e.lid_bot, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(e.lid_bot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(e.lid_bot, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(e.lid_bot, LV_OBJ_FLAG_HIDDEN);

        e.closed_slit = RobiPlainObj(parent);
        lv_obj_set_style_bg_opa(e.closed_slit, LV_OPA_90, 0);
        lv_obj_set_style_radius(e.closed_slit, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(e.closed_slit, LV_OBJ_FLAG_HIDDEN);
    }

    static lv_obj_t* RobiCreateLine(lv_obj_t* parent, int w, int h, int line_w, lv_opa_t opa) {
        lv_obj_t* ln = lv_line_create(parent);
        lv_obj_set_size(ln, w, h);
        lv_obj_set_style_line_width(ln, line_w, 0);
        lv_obj_set_style_line_opa(ln, opa, 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
        lv_obj_remove_flag(ln, LV_OBJ_FLAG_CLICKABLE);
        return ln;
    }

    void RobiRenderEye(CyberEyeWidgets& e, float cx, float cy, float ew, float eh, float rad,
                       float pupil_sc, float gx, float gy, float sq_top, float sq_bot,
                       float scan_norm, lv_color_t c_hi, lv_color_t c_mid, bool color_dirty) {
        float blink_atten = 1.0f - RobiClamp(sq_top, 0.0f, 1.0f) * 0.75f;
        int i_ew = static_cast<int>(ew + 0.5f);
        int i_eh = static_cast<int>(eh + 0.5f);
        int i_rad = static_cast<int>(rad + 0.5f);
        int core_x = static_cast<int>(cx - ew * 0.5f + 0.5f);
        int core_y = static_cast<int>(cy - eh * 0.5f + 0.5f);

        int go_w = i_ew + 26;
        int go_h = static_cast<int>((eh + 26.0f) * blink_atten + 0.5f);
        lv_obj_set_pos(e.glow_outer, static_cast<int>(cx - go_w * 0.5f), static_cast<int>(cy - go_h * 0.5f));
        lv_obj_set_size(e.glow_outer, go_w, go_h);
        lv_obj_set_style_radius(e.glow_outer, i_rad + 13, 0);

        int gi_w = i_ew + 12;
        int gi_h = static_cast<int>((eh + 12.0f) * blink_atten + 0.5f);
        lv_obj_set_pos(e.glow_inner, static_cast<int>(cx - gi_w * 0.5f), static_cast<int>(cy - gi_h * 0.5f));
        lv_obj_set_size(e.glow_inner, gi_w, gi_h);
        lv_obj_set_style_radius(e.glow_inner, i_rad + 6, 0);

        lv_obj_set_pos(e.core, core_x, core_y);
        lv_obj_set_size(e.core, i_ew, i_eh);
        lv_obj_set_style_radius(e.core, i_rad, 0);

        if (color_dirty) {
            lv_obj_set_style_bg_color(e.glow_outer, c_mid, 0);
            lv_obj_set_style_bg_color(e.glow_inner, c_mid, 0);
            lv_obj_set_style_bg_color(e.core, c_hi, 0);
            lv_obj_set_style_bg_grad_color(e.core, c_mid, 0);
            lv_obj_set_style_border_color(e.core, c_hi, 0);
            lv_obj_set_style_border_color(e.iris, c_hi, 0);
            lv_obj_set_style_bg_color(e.closed_slit, c_hi, 0);
        }

        // Internal 3D parallax lens layers (relative to e.core)
        int iris_d = static_cast<int>(ew * 0.56f);
        int iris_x = static_cast<int>((ew - iris_d) * 0.5f + gx * 0.28f);
        int iris_y = static_cast<int>((eh - iris_d) * 0.5f + gy * 0.28f);
        lv_obj_set_pos(e.iris, iris_x, iris_y);
        lv_obj_set_size(e.iris, iris_d, iris_d);

        int pup_d = static_cast<int>(ew * pupil_sc);
        if (pup_d < 12) pup_d = 12;
        int pup_x = static_cast<int>((ew - pup_d) * 0.5f + gx * 0.40f);
        int pup_y = static_cast<int>((eh - pup_d) * 0.5f + gy * 0.40f);
        lv_obj_set_pos(e.pupil, pup_x, pup_y);
        lv_obj_set_size(e.pupil, pup_d, pup_d);

        int scan_y = static_cast<int>(scan_norm * (eh - 4.0f));
        lv_obj_set_pos(e.scanline, 6, scan_y);
        lv_obj_set_size(e.scanline, i_ew - 12, 3);

        int sp1_x = static_cast<int>(ew * 0.20f + gx * 0.46f);
        int sp1_y = static_cast<int>(eh * 0.16f + gy * 0.46f);
        lv_obj_set_pos(e.spec_main, sp1_x, sp1_y);

        int sp2_x = static_cast<int>(ew * 0.66f + gx * 0.25f);
        int sp2_y = static_cast<int>(eh * 0.66f + gy * 0.25f);
        lv_obj_set_pos(e.spec_sub, sp2_x, sp2_y);

        // Upper shutter (blinks + angry/sleepy/cool top lid)
        int top_cov = static_cast<int>(RobiClamp(sq_top, 0.0f, 1.0f) * (eh + 26.0f));
        if (top_cov >= 3) {
            lv_obj_remove_flag(e.lid_top, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(e.lid_top, core_x - 15, core_y - 14);
            lv_obj_set_size(e.lid_top, i_ew + 30, top_cov);
        } else {
            lv_obj_add_flag(e.lid_top, LV_OBJ_FLAG_HIDDEN);
        }

        // Lower curved shutter (sculpts smooth EVE/EMO joyful crescent eyes)
        int bot_cov = static_cast<int>(RobiClamp(sq_bot, 0.0f, 1.0f) * (eh + 10.0f));
        if (bot_cov >= 3) {
            int dome_w = i_ew + 44;
            int dome_h = i_eh + 36;
            lv_obj_remove_flag(e.lid_bot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(e.lid_bot, static_cast<int>(cx - dome_w * 0.5f), core_y + i_eh + 8 - bot_cov);
            lv_obj_set_size(e.lid_bot, dome_w, dome_h);
        } else {
            lv_obj_add_flag(e.lid_bot, LV_OBJ_FLAG_HIDDEN);
        }

        // Sleek neon slit when eye is fully blinked shut
        if (sq_top > 0.88f) {
            lv_obj_remove_flag(e.closed_slit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(e.closed_slit, core_x + 4, static_cast<int>(cy + eh * 0.12f));
            lv_obj_set_size(e.closed_slit, i_ew - 8, 5);
        } else {
            lv_obj_add_flag(e.closed_slit, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void RobiRenderBrow(lv_obj_t* glow, lv_obj_t* core, lv_point_precise_t* pts,
                        float cx, float cy, float half_w, float tilt_px, bool is_left,
                        lv_opa_t opa, lv_color_t c_hi, lv_color_t c_mid, bool color_dirty) {
        if (opa < 12) {
            lv_obj_add_flag(glow, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(core, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_remove_flag(glow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(core, LV_OBJ_FLAG_HIDDEN);

        const int box_w = 140;
        const int box_h = 52;
        int bx = static_cast<int>(cx - box_w * 0.5f);
        int by = static_cast<int>(cy - box_h * 0.5f);
        lv_obj_set_pos(glow, bx, by);
        lv_obj_set_pos(core, bx, by);

        // Positive tilt_px pushes inner corner down (determined/angry); negative lifts inner corner (gentle/sad)
        float left_dy = is_left ? -tilt_px * 0.6f : tilt_px;
        float right_dy = is_left ? tilt_px : -tilt_px * 0.6f;
        float arch_dy = -5.0f - std::fabs(tilt_px) * 0.15f;

        pts[0].x = static_cast<lv_value_precise_t>(box_w * 0.5f - half_w);
        pts[0].y = static_cast<lv_value_precise_t>(box_h * 0.5f + left_dy);
        pts[1].x = static_cast<lv_value_precise_t>(box_w * 0.5f);
        pts[1].y = static_cast<lv_value_precise_t>(box_h * 0.5f + (left_dy + right_dy) * 0.35f + arch_dy);
        pts[2].x = static_cast<lv_value_precise_t>(box_w * 0.5f + half_w);
        pts[2].y = static_cast<lv_value_precise_t>(box_h * 0.5f + right_dy);

        lv_line_set_points(glow, pts, 3);
        lv_line_set_points(core, pts, 3);
        lv_obj_set_style_line_opa(glow, static_cast<lv_opa_t>(opa * 0.32f), 0);
        lv_obj_set_style_line_opa(core, opa, 0);
        if (color_dirty) {
            lv_obj_set_style_line_color(glow, c_mid, 0);
            lv_obj_set_style_line_color(core, c_hi, 0);
        }
    }

    static void OnFaceTick(lv_timer_t* t) {
        auto* self = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(t));
        if (self != nullptr && self->robi_face_ready_) {
            self->RobiStepFace();
        }
    }

    void RobiStepFace() {
        int64_t now_us = esp_timer_get_time();
        float dt = (robi_last_tick_us_ == 0) ? 0.033f : static_cast<float>(now_us - robi_last_tick_us_) / 1000000.0f;
        robi_last_tick_us_ = now_us;
        dt = RobiClamp(dt, 0.008f, 0.080f);
        phase_t_ += dt;

        bool voice_in = (robi_face_mode_ == ROBI_MODE_LISTENING) && Application::GetInstance().IsVoiceDetected();

        // 1. Whole-face posture & speaking blend
        float tgt_scale = (robi_face_mode_ == ROBI_MODE_IDLE) ? 0.85f : (voice_in ? 1.07f : 1.03f);
        float tgt_fy = (robi_face_mode_ == ROBI_MODE_IDLE) ? 220.0f : 226.0f;
        float tgt_talk = (robi_face_mode_ == ROBI_MODE_SPEAKING) ? 1.0f : 0.0f;

        float k_fast = RobiClamp(dt * 14.0f, 0.0f, 1.0f);
        float k_med = RobiClamp(dt * 9.5f, 0.0f, 1.0f);
        float k_col = RobiClamp(dt * 7.5f, 0.0f, 1.0f);

        face_scale_ = RobiLerp(face_scale_, tgt_scale, k_med);
        face_y_ = RobiLerp(face_y_, tgt_fy, k_med);
        talk_mix_ = RobiLerp(talk_mix_, tgt_talk, k_fast);

        // 2. Smoothly interpolate all 25 expression parameters
        cur_.eye_w = RobiLerp(cur_.eye_w, tgt_.eye_w, k_med);
        cur_.eye_h = RobiLerp(cur_.eye_h, tgt_.eye_h, k_med);
        cur_.eye_radius = RobiLerp(cur_.eye_radius, tgt_.eye_radius, k_med);
        cur_.pupil_scale = RobiLerp(cur_.pupil_scale, tgt_.pupil_scale, k_med);
        cur_.gaze_x = RobiLerp(cur_.gaze_x, tgt_.gaze_x, k_fast);
        cur_.gaze_y = RobiLerp(cur_.gaze_y, tgt_.gaze_y, k_fast);
        cur_.squint_top_l = RobiLerp(cur_.squint_top_l, tgt_.squint_top_l, k_fast);
        cur_.squint_top_r = RobiLerp(cur_.squint_top_r, tgt_.squint_top_r, k_fast);
        cur_.squint_bot_l = RobiLerp(cur_.squint_bot_l, tgt_.squint_bot_l, k_fast);
        cur_.squint_bot_r = RobiLerp(cur_.squint_bot_r, tgt_.squint_bot_r, k_fast);
        cur_.brow_ang_l = RobiLerp(cur_.brow_ang_l, tgt_.brow_ang_l, k_med);
        cur_.brow_ang_r = RobiLerp(cur_.brow_ang_r, tgt_.brow_ang_r, k_med);
        cur_.brow_y_l = RobiLerp(cur_.brow_y_l, tgt_.brow_y_l, k_med);
        cur_.brow_y_r = RobiLerp(cur_.brow_y_r, tgt_.brow_y_r, k_med);
        cur_.brow_opa = RobiLerp(cur_.brow_opa, tgt_.brow_opa, k_med);
        cur_.mouth_w = RobiLerp(cur_.mouth_w, tgt_.mouth_w, k_fast);
        cur_.mouth_curve = RobiLerp(cur_.mouth_curve, tgt_.mouth_curve, k_fast);
        cur_.mouth_tilt = RobiLerp(cur_.mouth_tilt, tgt_.mouth_tilt, k_fast);
        cur_.mouth_wave = RobiLerp(cur_.mouth_wave, tgt_.mouth_wave, k_med);
        cur_.mouth_o = RobiLerp(cur_.mouth_o, tgt_.mouth_o, k_fast);
        cur_.cheek_opa = RobiLerp(cur_.cheek_opa, tgt_.cheek_opa, k_med);
        cur_.visor_opa = RobiLerp(cur_.visor_opa, tgt_.visor_opa, k_med);
        cur_.col_r = RobiLerp(cur_.col_r, tgt_.col_r, k_col);
        cur_.col_g = RobiLerp(cur_.col_g, tgt_.col_g, k_col);
        cur_.col_b = RobiLerp(cur_.col_b, tgt_.col_b, k_col);

        // 3. Natural randomized blinking (with occasional lifelike double-blink)
        float blink_amt = 0.0f;
        if (blink_phase_ >= 0.0f) {
            blink_phase_ += dt / 0.155f;
            if (blink_phase_ >= 1.0f) {
                blink_phase_ = -1.0f;
                if (pending_double_blink_) {
                    pending_double_blink_ = false;
                    next_blink_s_ = 0.11f;
                } else {
                    next_blink_s_ = 2.5f + static_cast<float>(rand() % 290) / 100.0f;
                    pending_double_blink_ = ((rand() % 100) < 18);
                }
            } else {
                blink_amt = std::sin(blink_phase_ * 3.14159265f);
            }
        } else {
            next_blink_s_ -= dt;
            if (next_blink_s_ <= 0.0f) {
                blink_phase_ = 0.0f;
            }
        }

        // 4. Lifelike micro-saccade gaze curiosity
        next_saccade_s_ -= dt;
        if (next_saccade_s_ <= 0.0f) {
            if (robi_face_mode_ == ROBI_MODE_LISTENING) {
                saccade_tgt_x_ = static_cast<float>((rand() % 7) - 3);
                saccade_tgt_y_ = static_cast<float>((rand() % 5) - 3);
                next_saccade_s_ = 1.6f;
            } else if (robi_face_mode_ == ROBI_MODE_SPEAKING) {
                saccade_tgt_x_ = static_cast<float>((rand() % 17) - 8);
                saccade_tgt_y_ = static_cast<float>((rand() % 11) - 5);
                next_saccade_s_ = 0.9f + static_cast<float>(rand() % 140) / 100.0f;
            } else {
                int r = rand() % 100;
                if (r < 52) {
                    saccade_tgt_x_ = 0.0f;
                    saccade_tgt_y_ = 0.0f;
                } else {
                    saccade_tgt_x_ = static_cast<float>((rand() % 29) - 14);
                    saccade_tgt_y_ = static_cast<float>((rand() % 17) - 8);
                }
                next_saccade_s_ = 1.5f + static_cast<float>(rand() % 240) / 100.0f;
            }
        }
        saccade_x_ = RobiLerp(saccade_x_, saccade_tgt_x_, RobiClamp(dt * 10.0f, 0.0f, 1.0f));
        saccade_y_ = RobiLerp(saccade_y_, saccade_tgt_y_, RobiClamp(dt * 10.0f, 0.0f, 1.0f));

        // 5. Real-time voice envelope (server Opus-synced level, with harmonic fallback)
        float target_voice = 0.0f;
        if (robi_face_mode_ == ROBI_MODE_SPEAKING) {
            if (now_us - robi_voice_env_ts_ < 340000) {
                target_voice = robi_voice_env_;
            } else {
                float cad = 0.50f + 0.32f * std::sin(phase_t_ * 9.8f) + 0.22f * std::sin(phase_t_ * 16.4f);
                target_voice = RobiClamp(cad, 0.08f, 0.92f);
            }
        }
        robi_voice_smooth_ = RobiLerp(robi_voice_smooth_, target_voice, RobiClamp(dt * 18.0f, 0.0f, 1.0f));

        // 6. Color computation (only touch LVGL color properties when hex changed)
        uint32_t cur_hex = RobiPackRgb(cur_.col_r, cur_.col_g, cur_.col_b);
        bool color_dirty = (cur_hex != last_primary_hex_);
        last_primary_hex_ = cur_hex;
        lv_color_t c_mid = lv_color_hex(cur_hex);
        lv_color_t c_hi = RobiTintBright(cur_.col_r, cur_.col_g, cur_.col_b, 0.58f);

        // 7. Outer Dual-Ring Holographic HUD (inv_arc_area via lv_arc_set_angles = minimal dirty rects!)
        float hud_spd = 18.0f;
        float tgt_span = 42.0f + 8.0f * std::sin(phase_t_ * 1.9f);
        if (robi_face_mode_ == ROBI_MODE_LISTENING) {
            hud_spd = voice_in ? 115.0f : 52.0f;
            tgt_span = voice_in ? 112.0f : 72.0f;
        } else if (robi_face_mode_ == ROBI_MODE_THINKING) {
            hud_spd = 185.0f;
            tgt_span = 84.0f;
        } else if (robi_face_mode_ == ROBI_MODE_SPEAKING) {
            hud_spd = 42.0f + robi_voice_smooth_ * 95.0f;
            tgt_span = 54.0f + robi_voice_smooth_ * 68.0f;
        }
        hud_span_ = RobiLerp(hud_span_, tgt_span, k_med);
        hud_angle_ = std::fmod(hud_angle_ + hud_spd * dt, 360.0f);
        if (hud_angle_ < 0.0f) hud_angle_ += 360.0f;

        float a1_s = hud_angle_;
        float a1_e = std::fmod(a1_s + hud_span_, 360.0f);
        float a2_s = std::fmod(hud_angle_ + 180.0f, 360.0f);
        float a2_e = std::fmod(a2_s + hud_span_, 360.0f);
        lv_arc_set_angles(hud_outer_a_, static_cast<lv_value_precise_t>(a1_s), static_cast<lv_value_precise_t>(a1_e));
        lv_arc_set_angles(hud_outer_b_, static_cast<lv_value_precise_t>(a2_s), static_cast<lv_value_precise_t>(a2_e));

        float in_ang = std::fmod(360.0f - std::fmod(hud_angle_ * 1.35f, 360.0f), 360.0f);
        float in_span = hud_span_ * 0.62f;
        float b1_s = in_ang;
        float b1_e = std::fmod(b1_s + in_span, 360.0f);
        float b2_s = std::fmod(in_ang + 180.0f, 360.0f);
        float b2_e = std::fmod(b2_s + in_span, 360.0f);
        lv_arc_set_angles(hud_inner_a_, static_cast<lv_value_precise_t>(b1_s), static_cast<lv_value_precise_t>(b1_e));
        lv_arc_set_angles(hud_inner_b_, static_cast<lv_value_precise_t>(b2_s), static_cast<lv_value_precise_t>(b2_e));

        if (color_dirty) {
            lv_obj_set_style_border_color(hud_track_, c_mid, 0);
            lv_obj_set_style_arc_color(hud_outer_a_, c_hi, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(hud_outer_b_, c_hi, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(hud_inner_a_, c_mid, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(hud_inner_b_, c_mid, LV_PART_INDICATOR);
        }

        // 8. Compute live eye positions & micro-motion
        float breathe_y = std::sin(phase_t_ * 2.2f) * 2.8f;
        if (bounce_laugh_) {
            breathe_y += std::sin(phase_t_ * 18.0f) * 3.2f;
        }
        float jitter_x = tremor_shock_ ? std::sin(phase_t_ * 42.0f) * 2.2f : 0.0f;

        float cx_mid = LV_HOR_RES * 0.5f + (cur_.gaze_x + saccade_x_) * 0.32f + jitter_x;
        float cy_eyes = face_y_ - 26.0f * face_scale_ + breathe_y + (cur_.gaze_y + saccade_y_) * 0.24f;
        float sep = 74.0f * face_scale_;
        float ew = cur_.eye_w * face_scale_;
        float eh = cur_.eye_h * face_scale_ * (1.0f + robi_voice_smooth_ * 0.05f);
        float rad = cur_.eye_radius * face_scale_;
        float gx = (cur_.gaze_x + saccade_x_) * face_scale_;
        float gy = (cur_.gaze_y + saccade_y_) * face_scale_;

        float sq_top_l = std::max(cur_.squint_top_l, blink_amt);
        float sq_top_r = std::max(cur_.squint_top_r, blink_amt);
        float scan_norm = 0.5f + 0.46f * std::sin(phase_t_ * 1.65f);

        RobiRenderEye(eye_l_, cx_mid - sep, cy_eyes, ew, eh, rad, cur_.pupil_scale,
                      gx, gy, sq_top_l, cur_.squint_bot_l, scan_norm, c_hi, c_mid, color_dirty);
        RobiRenderEye(eye_r_, cx_mid + sep, cy_eyes, ew, eh, rad, cur_.pupil_scale,
                      gx, gy, sq_top_r, cur_.squint_bot_r, scan_norm, c_hi, c_mid, color_dirty);

        // 9. Holographic Cyber-Brows
        float brow_base_y = cy_eyes - eh * 0.5f - 16.0f * face_scale_ - robi_voice_smooth_ * 4.0f;
        lv_opa_t b_opa = static_cast<lv_opa_t>(RobiClamp(cur_.brow_opa * 255.0f, 0.0f, 255.0f));
        RobiRenderBrow(brow_glow_l_, brow_core_l_, brow_pts_l_,
                       cx_mid - sep, brow_base_y + cur_.brow_y_l * face_scale_,
                       ew * 0.44f, cur_.brow_ang_l * face_scale_, true, b_opa, c_hi, c_mid, color_dirty);
        RobiRenderBrow(brow_glow_r_, brow_core_r_, brow_pts_r_,
                       cx_mid + sep, brow_base_y + cur_.brow_y_r * face_scale_,
                       ew * 0.44f, cur_.brow_ang_r * face_scale_, false, b_opa, c_hi, c_mid, color_dirty);

        // 10. Warm Holographic Cheeks & Cool Cyber-Visor
        if (cur_.cheek_opa > 0.04f) {
            lv_obj_remove_flag(cheek_l_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(cheek_r_, LV_OBJ_FLAG_HIDDEN);
            lv_opa_t copa = static_cast<lv_opa_t>(RobiClamp(cur_.cheek_opa * 135.0f, 0.0f, 180.0f));
            int cw = static_cast<int>(38.0f * face_scale_);
            int ch = static_cast<int>(16.0f * face_scale_);
            int cy_ch = static_cast<int>(cy_eyes + eh * 0.54f);
            lv_obj_set_pos(cheek_l_, static_cast<int>(cx_mid - sep - ew * 0.36f - cw * 0.5f), cy_ch);
            lv_obj_set_size(cheek_l_, cw, ch);
            lv_obj_set_style_bg_opa(cheek_l_, copa, 0);
            lv_obj_set_pos(cheek_r_, static_cast<int>(cx_mid + sep + ew * 0.36f - cw * 0.5f), cy_ch);
            lv_obj_set_size(cheek_r_, cw, ch);
            lv_obj_set_style_bg_opa(cheek_r_, copa, 0);
        } else {
            lv_obj_add_flag(cheek_l_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(cheek_r_, LV_OBJ_FLAG_HIDDEN);
        }

        if (cur_.visor_opa > 0.04f) {
            lv_obj_remove_flag(visor_bar_, LV_OBJ_FLAG_HIDDEN);
            int vw = static_cast<int>((sep * 2.0f + ew + 30.0f));
            int vh = static_cast<int>(eh * 0.56f);
            lv_obj_set_pos(visor_bar_, static_cast<int>(cx_mid - vw * 0.5f), static_cast<int>(cy_eyes - vh * 0.56f));
            lv_obj_set_size(visor_bar_, vw, vh);
            lv_obj_set_style_bg_opa(visor_bar_, static_cast<lv_opa_t>(cur_.visor_opa * 185.0f), 0);
            lv_obj_set_style_border_opa(visor_bar_, static_cast<lv_opa_t>(cur_.visor_opa * 245.0f), 0);
        } else {
            lv_obj_add_flag(visor_bar_, LV_OBJ_FLAG_HIDDEN);
        }

        // 11. Expressive Holographic Mouth + Real-Time 7-Bar Syllable Voice Synthesizer
        float mouth_cx = LV_HOR_RES * 0.5f + (cur_.gaze_x + saccade_x_) * 0.16f;
        float mouth_cy = face_y_ + 66.0f * face_scale_ + breathe_y * 0.7f;

        float o_vis = cur_.mouth_o * (1.0f - talk_mix_ * 0.85f);
        float line_vis = (1.0f - cur_.mouth_o) * (1.0f - talk_mix_ * 0.88f);

        if (o_vis > 0.06f) {
            lv_obj_remove_flag(mouth_ring_, LV_OBJ_FLAG_HIDDEN);
            int rw = static_cast<int>(cur_.mouth_w * 1.45f * face_scale_);
            int rh = static_cast<int>((cur_.mouth_w * 1.35f + 6.0f) * face_scale_);
            lv_obj_set_pos(mouth_ring_, static_cast<int>(mouth_cx - rw * 0.5f), static_cast<int>(mouth_cy - rh * 0.5f));
            lv_obj_set_size(mouth_ring_, rw, rh);
            lv_obj_set_style_border_opa(mouth_ring_, static_cast<lv_opa_t>(RobiClamp(o_vis * 255.0f, 0.0f, 255.0f)), 0);
            if (color_dirty) {
                lv_obj_set_style_border_color(mouth_ring_, c_hi, 0);
            }
        } else {
            lv_obj_add_flag(mouth_ring_, LV_OBJ_FLAG_HIDDEN);
        }

        if (line_vis > 0.06f) {
            lv_obj_remove_flag(mouth_glow_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(mouth_core_, LV_OBJ_FLAG_HIDDEN);
            const int mw_box = 168;
            const int mh_box = 84;
            int mbx = static_cast<int>(mouth_cx - mw_box * 0.5f);
            int mby = static_cast<int>(mouth_cy - mh_box * 0.5f);
            lv_obj_set_pos(mouth_glow_, mbx, mby);
            lv_obj_set_pos(mouth_core_, mbx, mby);

            float hw = cur_.mouth_w * face_scale_;
            float crv = cur_.mouth_curve * face_scale_;
            float tlt = cur_.mouth_tilt * face_scale_;
            float wav = cur_.mouth_wave * face_scale_;
            for (int i = 0; i < 9; i++) {
                float u = (static_cast<float>(i) / 4.0f) - 1.0f;  // -1.0 .. +1.0
                float px = mw_box * 0.5f + u * hw;
                // Parabolic smile: center dips down (+crv) while corners curve up
                float py = mh_box * 0.5f + crv * 0.42f * (1.0f - u * u) - u * tlt * 0.5f;
                if (wav > 0.2f) {
                    py += std::sin(u * 4.8f + phase_t_ * 7.0f) * wav;
                }
                mouth_pts_[i].x = static_cast<lv_value_precise_t>(px);
                mouth_pts_[i].y = static_cast<lv_value_precise_t>(py);
            }
            lv_line_set_points(mouth_glow_, mouth_pts_, 9);
            lv_line_set_points(mouth_core_, mouth_pts_, 9);
            lv_obj_set_style_line_opa(mouth_glow_, static_cast<lv_opa_t>(line_vis * 78.0f), 0);
            lv_obj_set_style_line_opa(mouth_core_, static_cast<lv_opa_t>(line_vis * 255.0f), 0);
            if (color_dirty) {
                lv_obj_set_style_line_color(mouth_glow_, c_mid, 0);
                lv_obj_set_style_line_color(mouth_core_, c_hi, 0);
            }
        } else {
            lv_obj_add_flag(mouth_glow_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mouth_core_, LV_OBJ_FLAG_HIDDEN);
        }

        // 7-bar holographic voice equalizer (curves with Robi's active emotion while speaking!)
        if (talk_mix_ > 0.05f) {
            static const float kBarWeight[7] = {0.38f, 0.66f, 0.90f, 1.00f, 0.90f, 0.66f, 0.38f};
            int bar_w = 10;
            int step_x = 18;
            lv_opa_t bar_opa = static_cast<lv_opa_t>(RobiClamp(talk_mix_ * 255.0f, 0.0f, 255.0f));
            for (int i = 0; i < 7; i++) {
                lv_obj_remove_flag(voice_bars_[i], LV_OBJ_FLAG_HIDDEN);
                float u = static_cast<float>(i - 3) / 3.0f;  // -1..+1
                float ripple = 0.72f + 0.28f * std::sin(phase_t_ * 15.0f + static_cast<float>(i) * 0.95f);
                float amp = robi_voice_smooth_ * kBarWeight[i] * ripple;
                int bh = static_cast<int>((8.0f + amp * 58.0f) * talk_mix_);
                if (bh < 6) bh = 6;
                float emo_arc_y = cur_.mouth_curve * 0.28f * (1.0f - u * u);
                int bx = static_cast<int>(mouth_cx + (i - 3) * step_x - bar_w * 0.5f);
                int by = static_cast<int>(mouth_cy + emo_arc_y - bh * 0.5f);
                lv_obj_set_pos(voice_bars_[i], bx, by);
                lv_obj_set_size(voice_bars_[i], bar_w, bh);
                lv_obj_set_style_bg_opa(voice_bars_[i], bar_opa, 0);
                if (color_dirty) {
                    lv_obj_set_style_bg_color(voice_bars_[i], c_hi, 0);
                    lv_obj_set_style_bg_grad_color(voice_bars_[i], c_mid, 0);
                }
            }
        } else {
            for (int i = 0; i < 7; i++) {
                lv_obj_add_flag(voice_bars_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    void RobiInitCyberFace() {
        lv_obj_t* scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        }
        if (top_bar_ != nullptr) {
            lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
        }
        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_box_ == nullptr) {
            return;
        }

        // Expand emoji_box_ into the full 466x466 procedural sci-fi face stage (under status/menus/tap)
        lv_obj_set_size(emoji_box_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(emoji_box_);
        lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(emoji_box_, 0, 0);
        lv_obj_set_style_border_width(emoji_box_, 0, 0);
        lv_obj_set_scrollbar_mode(emoji_box_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_CLICKABLE);

        // Outer subtle instrument track + dual counter-rotating energy HUD rings
        hud_track_ = RobiPlainObj(emoji_box_);
        lv_obj_set_size(hud_track_, 450, 450);
        lv_obj_center(hud_track_);
        lv_obj_set_style_bg_opa(hud_track_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(hud_track_, 2, 0);
        lv_obj_set_style_border_opa(hud_track_, LV_OPA_20, 0);
        lv_obj_set_style_radius(hud_track_, LV_RADIUS_CIRCLE, 0);

        hud_outer_a_ = RobiCreateHudArc(emoji_box_, 448, 5, LV_OPA_90);
        hud_outer_b_ = RobiCreateHudArc(emoji_box_, 448, 5, LV_OPA_90);
        hud_inner_a_ = RobiCreateHudArc(emoji_box_, 428, 3, LV_OPA_50);
        hud_inner_b_ = RobiCreateHudArc(emoji_box_, 428, 3, LV_OPA_50);

        // Warm holographic blush cheeks
        cheek_l_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(cheek_l_, lv_color_hex(0xFF4D94), 0);
        lv_obj_set_style_radius(cheek_l_, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(cheek_l_, LV_OBJ_FLAG_HIDDEN);

        cheek_r_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(cheek_r_, lv_color_hex(0xFF4D94), 0);
        lv_obj_set_style_radius(cheek_r_, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(cheek_r_, LV_OBJ_FLAG_HIDDEN);

        // Multi-layer 3D cyber-optic eyes
        RobiCreateEye(emoji_box_, eye_l_);
        RobiCreateEye(emoji_box_, eye_r_);

        // Sleek holographic visor for "cool" emotion
        visor_bar_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(visor_bar_, lv_color_hex(0x061A2E), 0);
        lv_obj_set_style_bg_grad_color(visor_bar_, lv_color_hex(0x00B4D8), 0);
        lv_obj_set_style_bg_grad_dir(visor_bar_, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(visor_bar_, 3, 0);
        lv_obj_set_style_border_color(visor_bar_, lv_color_hex(0xFFD166), 0);
        lv_obj_set_style_radius(visor_bar_, 22, 0);
        lv_obj_add_flag(visor_bar_, LV_OBJ_FLAG_HIDDEN);

        // Dual-layer glowing holographic brows
        brow_glow_l_ = RobiCreateLine(emoji_box_, 140, 52, 11, LV_OPA_30);
        brow_core_l_ = RobiCreateLine(emoji_box_, 140, 52, 5, LV_OPA_COVER);
        brow_glow_r_ = RobiCreateLine(emoji_box_, 140, 52, 11, LV_OPA_30);
        brow_core_r_ = RobiCreateLine(emoji_box_, 140, 52, 5, LV_OPA_COVER);

        // Expressive holographic spline mouth + surprised "O" ring + 7-bar voice equalizer
        mouth_glow_ = RobiCreateLine(emoji_box_, 168, 84, 13, LV_OPA_30);
        mouth_core_ = RobiCreateLine(emoji_box_, 168, 84, 5, LV_OPA_COVER);

        mouth_ring_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(mouth_ring_, lv_color_hex(0x031828), 0);
        lv_obj_set_style_bg_opa(mouth_ring_, LV_OPA_60, 0);
        lv_obj_set_style_border_width(mouth_ring_, 5, 0);
        lv_obj_set_style_radius(mouth_ring_, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(mouth_ring_, LV_OBJ_FLAG_HIDDEN);

        for (int i = 0; i < 7; i++) {
            voice_bars_[i] = RobiPlainObj(emoji_box_);
            lv_obj_set_style_bg_grad_dir(voice_bars_[i], LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_radius(voice_bars_[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_add_flag(voice_bars_[i], LV_OBJ_FLAG_HIDDEN);
        }

        cur_ = RobiDefaultParams();
        tgt_ = cur_;
        robi_face_ready_ = true;
        RobiStepFace();
        robi_face_timer_ = lv_timer_create(OnFaceTick, 33, this);
    }

    // ---- callbacks (run inside the LVGL task, which already holds the display lock) ----
    static void OnProfile(lv_event_t* e) {
        lv_obj_t* menu = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
        auto* self = robi_self_;
        lv_obj_add_flag(self->robi_profiles_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(menu, LV_OBJ_FLAG_HIDDEN);
        self->RobiStopMenuTimer();
        self->robi_menu_timer_ = lv_timer_create(OnMenuTimeout, 20000, nullptr);
        lv_timer_set_repeat_count(self->robi_menu_timer_, 1);
    }

    static void OnMenuTimeout(lv_timer_t* t) {
        (void)t;
        auto* self = robi_self_;
        self->robi_menu_timer_ = nullptr;  // a one-shot timer deletes itself
        lv_obj_add_flag(self->robi_menu_lavi_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(self->robi_menu_ori_, LV_OBJ_FLAG_HIDDEN);
        if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle) {
            lv_obj_remove_flag(self->robi_profiles_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void RobiStopMenuTimer() {
        if (robi_menu_timer_ != nullptr) {
            lv_timer_delete(robi_menu_timer_);
            robi_menu_timer_ = nullptr;
        }
    }

    static void OnActivity(lv_event_t* e) {
        const char* says = static_cast<const char*>(lv_event_get_user_data(e));
        auto* self = robi_self_;
        self->RobiStopMenuTimer();
        lv_obj_add_flag(self->robi_menu_lavi_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(self->robi_menu_ori_, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Robi activity: %s", says);
        Application::GetInstance().WakeWordInvoke(says);
    }

    static void OnTap(lv_event_t* e) {
        (void)e;
        auto& app = Application::GetInstance();
        app.Schedule([]() {
            auto& app = Application::GetInstance();
            auto state = app.GetDeviceState();
            if (state == kDeviceStateListening) {
                ESP_LOGI(TAG, "Robi tap: done talking");
                app.StopListening();
            } else if (state == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Robi tap: stop speaking");
                app.AbortSpeaking(kAbortReasonNone);
            }
        });
    }

    // ---- building blocks ----
    static lv_obj_t* RobiBox(lv_obj_t* parent, int w_pct, lv_flex_flow_t flow) {
        lv_obj_t* o = lv_obj_create(parent);
        lv_obj_set_size(o, LV_HOR_RES * w_pct / 100, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_pad_all(o, 0, 0);
        lv_obj_set_style_pad_row(o, LV_VER_RES * 2 / 100, 0);
        lv_obj_set_style_pad_column(o, LV_HOR_RES * 3 / 100, 0);
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(o, flow);
        lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        return o;
    }

    static lv_obj_t* RobiButton(lv_obj_t* parent, const char* text, uint32_t color, int w_pct, int h_pct,
                                lv_event_cb_t cb, void* data) {
        lv_obj_t* b = lv_button_create(parent);
        lv_obj_set_size(b, LV_HOR_RES * w_pct / 100, LV_VER_RES * h_pct / 100);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, lv_color_white(), 0);
        lv_obj_set_style_border_opa(b, LV_OPA_30, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
        return b;
    }

    // one child's activity menu: 2 x 2 activities + "just talk"
    lv_obj_t* RobiMenu(const char* const* items, uint32_t color) {
        lv_obj_t* menu = RobiBox(lv_screen_active(), 70, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, -(LV_VER_RES * 9 / 100));
        for (int i = 0; i < 4; i++) {
            RobiButton(menu, items[i * 2], color, 31, 11, OnActivity, (void*)items[i * 2 + 1]);
        }
        RobiButton(menu, items[8], 0x374151, 40, 9, OnActivity, (void*)items[9]);
        lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
        return menu;
    }

    lv_obj_t* RobiPetRow(lv_obj_t* parent, const char* name, uint32_t color) {
        lv_obj_t* row = RobiBox(parent, 60, LV_FLEX_FLOW_ROW);
        lv_obj_t* bar = lv_bar_create(row);
        lv_obj_set_size(bar, LV_HOR_RES * 28 / 100, LV_VER_RES * 2 / 100 + 2);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 50, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x1E293B), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_t* l = lv_label_create(row);
        lv_label_set_text(l, name);
        lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
        return bar;
    }

    // ---- Robi's energy / joy from the home server: <ota host>/robi/pet ----
    void RobiRequestPet() {
        int64_t now = esp_timer_get_time() / 1000000;
        if (robi_pet_busy_ || (robi_pet_last_ != 0 && now - robi_pet_last_ < 30)) return;
        robi_pet_busy_ = true;
        robi_pet_last_ = now;
        if (xTaskCreate([](void* arg) {
                static_cast<CustomLcdDisplay*>(arg)->RobiFetchPet();
                static_cast<CustomLcdDisplay*>(arg)->robi_pet_busy_ = false;
                vTaskDelete(NULL);
            }, "robi_pet", 8192, this, 2, nullptr) != pdPASS) {
            robi_pet_busy_ = false;
        }
    }

    void RobiFetchPet() {
        std::string url = Settings("wifi", false).GetString("ota_url");
        if (url.empty()) url = CONFIG_OTA_URL;
        auto pos = url.find("/xiaozhi/ota");
        if (pos == std::string::npos) return;
        url = url.substr(0, pos) + "/robi/pet";
        auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
        if (!http) return;
        http->SetTimeout(5000);
        if (!http->Open("GET", url)) return;
        auto code = http->GetStatusCode();
        if (!code || *code != 200) {
            http->Close();
            return;
        }
        std::string body = http->ReadAll();
        http->Close();
        cJSON* j = cJSON_Parse(body.c_str());
        if (j == nullptr) return;
        cJSON* energy = cJSON_GetObjectItem(j, "energy");
        cJSON* joy = cJSON_GetObjectItem(j, "joy");
        if (cJSON_IsNumber(energy) && cJSON_IsNumber(joy)) {
            DisplayLockGuard lock(this);
            lv_bar_set_value(robi_bar_energy_, energy->valueint, LV_ANIM_ON);
            lv_bar_set_value(robi_bar_joy_, joy->valueint, LV_ANIM_ON);
            robi_pet_ok_ = true;
            if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle) {
                lv_obj_remove_flag(robi_pet_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        cJSON_Delete(j);
    }

    void RobiSetupUI() {
        robi_self_ = this;
        // no subtitles: the live cyber-face + voice waveform fills the circular display
        SetHideSubtitle(true);
        RobiInitCyberFace();

        // Robi's needs, under the status line
        robi_pet_ = RobiBox(lv_screen_active(), 60, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(robi_pet_, LV_VER_RES * 1 / 100, 0);
        lv_obj_align(robi_pet_, LV_ALIGN_TOP_MID, 0, LV_VER_RES * 14 / 100);
        robi_bar_energy_ = RobiPetRow(robi_pet_, "אנרגיה", 0x00F5D4);
        robi_bar_joy_ = RobiPetRow(robi_pet_, "שמחה", 0xFF5DA2);
        lv_obj_add_flag(robi_pet_, LV_OBJ_FLAG_HIDDEN);

        // activity menus (text on the button, then what Robi hears)
        static const char* const lavi[] = {
            "חשבון", "זה לביא. בוא נתרגל חשבון",
            "אנגלית", "זה לביא. בוא נתרגל אנגלית",
            "ידע כללי", "זה לביא. ספר לי משהו מרתק ומעמיק בידע כללי, מדע, חלל, היסטוריה או טכנולוגיה!",
            "סיפור", "זה לביא. בוא נמשיך את הסיפור שלנו",
            "סתם לדבר", "זה לביא",
        };
        static const char* const ori[] = {
            "חשבון", "זה אורי. בוא נתרגל חשבון",
            "ידע כללי", "זה אורי. ספר לי משהו מדהים ומסקרן בידע כללי על העולם, מדע, חלל או חיות!",
            "חידות", "זה אורי. תן לי חידה",
            "סיפור", "זה אורי. בוא נמשיך את הסיפור שלנו",
            "סתם לדבר", "זה אורי",
        };
        robi_menu_lavi_ = RobiMenu(lavi, 0x0284C7);
        robi_menu_ori_ = RobiMenu(ori, 0xEA580C);

        // who is talking
        robi_profiles_ = RobiBox(lv_screen_active(), 80, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(robi_profiles_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(robi_profiles_, LV_ALIGN_BOTTOM_MID, 0, -(LV_VER_RES * 10 / 100));
        RobiButton(robi_profiles_, "לביא", 0x0284C7, 30, 14, OnProfile, robi_menu_lavi_);
        RobiButton(robi_profiles_, "אורי", 0xEA580C, 30, 14, OnProfile, robi_menu_ori_);
        lv_obj_add_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);

        // invisible full-screen layer: tap = "I finished" (listening) / "stop" (speaking)
        robi_tap_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(robi_tap_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_tap_);
        lv_obj_set_style_bg_opa(robi_tap_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(robi_tap_, 0, 0);
        lv_obj_remove_flag(robi_tap_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(robi_tap_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(robi_tap_, OnTap, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_flag(robi_tap_, LV_OBJ_FLAG_HIDDEN);
    }
    // ===================== end Robi UI =====================
