    // ===================== OpenRobi Dynamic Cyber-Optic UI v3.1 =====================
    // Modular LVGL Cyber-Optic Face Controller (included inside CustomLcdDisplay).
    // Uses pure procedural face math from `robi_face_math.h`, dirty-checked LVGL style setters
    // to minimize QSPI DMA & DisplayLockGuard contention, and 20Hz lock-free atomic lip-sync.

    virtual void SetTheme(Theme* theme) override {
        SpiLcdDisplay::SetTheme(theme);
        DisplayLockGuard lock(this);
        if (!robi_face_ready_) return;
        lv_obj_t* scr = lv_screen_active();
        if (scr != nullptr) {
            lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        }
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        }
        if (top_bar_ != nullptr) {
            RobiSetBgOpa(top_bar_, LV_OPA_TRANSP);
        }
        RobiSetHidden(emoji_label_, true);
        RobiSetHidden(emoji_image_, true);
    }

    virtual void SetStatus(const char* status) override {
        SpiLcdDisplay::SetStatus(status);
        DisplayLockGuard lock(this);
        if (!robi_face_ready_ || robi_tap_ == nullptr) return;

        auto dev_state = Application::GetInstance().GetDeviceState();
        bool explicit_standby = status != nullptr && std::strcmp(status, Lang::Strings::STANDBY) == 0;
        bool explicit_listening = status != nullptr && std::strcmp(status, Lang::Strings::LISTENING) == 0;
        bool explicit_speaking = status != nullptr && std::strcmp(status, Lang::Strings::SPEAKING) == 0;

        bool listening = explicit_listening ||
                         (!explicit_standby && !explicit_speaking && dev_state == kDeviceStateListening);
        bool speaking = explicit_speaking ||
                        (!explicit_standby && !explicit_listening && dev_state == kDeviceStateSpeaking);
        bool idle = explicit_standby ||
                    (!listening && !speaking && dev_state == kDeviceStateIdle);

        RobiUpdateSetupHintVisibility(idle);

        if (idle) {
            if (robi_face_mode_ != robi::ROBI_MODE_IDLE) {
                robi_face_mode_ = robi::ROBI_MODE_IDLE;
                robi_voice_env_ = 0.0f;
                RobiApplyPreset("neutral", false);
            }
            if (!RobiIsAnyMenuOpen() && !RobiIsArcadeOrGameOpen()) {
                if (robi_profiles_ != nullptr) {
                    RobiSetHidden(robi_profiles_, false);
                }
                RobiSetHidden(robi_games_home_btn_, false);
            }
        } else {
            if (listening) {
                bool awaiting_ai = false;
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    awaiting_ai = proto->IsAwaitingResponse();
                }
                if (awaiting_ai) {
                    if (robi_face_mode_ != robi::ROBI_MODE_THINKING) {
                        robi_face_mode_ = robi::ROBI_MODE_THINKING;
                        robi_voice_env_ = 0.0f;
                        RobiApplyPreset("thinking", false);
                    }
                } else if (robi_face_mode_ != robi::ROBI_MODE_LISTENING) {
                    robi_face_mode_ = robi::ROBI_MODE_LISTENING;
                    robi_voice_env_ = 0.0f;
                    RobiApplyPreset("listening", false);
                }
            } else if (speaking) {
                if (robi_face_mode_ != robi::ROBI_MODE_SPEAKING) {
                    robi_face_mode_ = robi::ROBI_MODE_SPEAKING;
                    if (std::strcmp(robi_last_emotion_, "thinking") == 0 ||
                        std::strcmp(robi_last_emotion_, "listening") == 0) {
                        RobiApplyPreset("happy", false);
                    }
                }
            } else {
                robi_face_mode_ = robi::ROBI_MODE_THINKING;
            }
            RobiSetHidden(robi_profiles_, true);
            RobiSetHidden(robi_games_home_btn_, true);
            RobiHideAllMenus();
            RobiStopMenuTimer();
        }
        RobiSetHidden(robi_tap_, !speaking || trivia_visible_);
    }

    virtual void SetEmotion(const char* emotion) override {
        if (emotion != nullptr && emotion[0] == 'v' && emotion[1] == ':') {
            int lvl = std::clamp(std::atoi(emotion + 2), 0, 100);
            robi_voice_env_ = static_cast<float>(lvl) / 100.0f;
            robi_voice_env_ts_ = esp_timer_get_time();
            return;
        }
        DisplayLockGuard lock(this);
        RobiSetHidden(emoji_label_, true);
        RobiSetHidden(emoji_image_, true);
        if (emotion == nullptr || emotion[0] == '\0' || !robi_face_ready_) {
            return;
        }
        if (std::strcmp(emotion, "arcade:open") == 0) {
            RobiOpenArcadeMenuLocked();
            return;
        }
        if (std::strcmp(emotion, "arcade:close") == 0) {
            RobiCloseAllGameScreensLocked(true);
            RobiHideTriviaOverlayLocked(true, true);
            return;
        }
        if (std::strncmp(emotion, "headsup:", 8) == 0) {
            int c_idx = std::atoi(emotion + 8);
            RobiStartOfflineHeadsUpLocked(c_idx);
            return;
        }
        if (std::strncmp(emotion, "trivia:solo:", 12) == 0) {
            int c_idx = std::atoi(emotion + 12);
            RobiStartTriviaRoundLocked(c_idx, false);
            return;
        }
        if (std::strncmp(emotion, "trivia:h2h:", 11) == 0) {
            int c_idx = std::atoi(emotion + 11);
            RobiStartTriviaRoundLocked(c_idx, true);
            return;
        }
        if (std::strncmp(emotion, "trivia:pick:", 12) == 0) {
            int p_idx = std::atoi(emotion + 12);
            RobiHandleTriviaPickIndexLocked(p_idx);
            return;
        }
        if (std::strncmp(emotion, "kid:", 4) == 0) {
            int k_idx = std::atoi(emotion + 4);
            RobiStopMenuTimer();
            RobiHideAllMenus();
            if (k_idx >= 0 && k_idx < kMaxKids && robi_menus_[k_idx] != nullptr) {
                RobiConfig::GetInstance().ActivateKidScope(static_cast<size_t>(k_idx));
                RobiSetHidden(robi_profiles_, true);
                RobiSetHidden(robi_menus_[k_idx], false);
                RobiUpdateSetupHintVisibility(true);
                robi_menu_timer_ = lv_timer_create(OnMenuTimeout, 20000, nullptr);
                lv_timer_set_repeat_count(robi_menu_timer_, 1);
            } else {
                if (robi_profiles_ != nullptr && robi_face_mode_ == robi::ROBI_MODE_IDLE) {
                    RobiSetHidden(robi_profiles_, false);
                }
                RobiUpdateSetupHintVisibility(robi_face_mode_ == robi::ROBI_MODE_IDLE);
            }
            return;
        }
        const char* mapped = emotion;
        if (std::strcmp(emotion, "neutral") == 0) {
            bool awaiting_ai = false;
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                awaiting_ai = proto->IsAwaitingResponse();
            }
            if (awaiting_ai && robi_face_mode_ != robi::ROBI_MODE_SPEAKING) {
                robi_face_mode_ = robi::ROBI_MODE_THINKING;
                mapped = "thinking";
            } else if (robi_face_mode_ == robi::ROBI_MODE_LISTENING) {
                mapped = "listening";
            }
        } else if (std::strcmp(emotion, "thinking") == 0 &&
                   robi_face_mode_ != robi::ROBI_MODE_SPEAKING) {
            robi_face_mode_ = robi::ROBI_MODE_THINKING;
        } else if (std::strcmp(emotion, "listening") == 0 &&
                   robi_face_mode_ != robi::ROBI_MODE_SPEAKING) {
            robi_face_mode_ = robi::ROBI_MODE_LISTENING;
        }
        RobiApplyPreset(mapped, true);
    }

private:
    static constexpr int kHudNodes = 16;
    static constexpr int kBrowSegs = 5;
    static constexpr int kMouthSegs = 9;
    static constexpr int kMaxKids = 3;

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
    lv_obj_t* robi_menus_[kMaxKids] = {};
    lv_obj_t* robi_setup_hint_ = nullptr;
    lv_obj_t* robi_setup_lbl_ = nullptr;
    lv_obj_t* robi_tap_ = nullptr;
    lv_timer_t* robi_menu_timer_ = nullptr;
    std::vector<std::string> robi_prompt_pool_;

    bool robi_face_ready_ = false;
    robi::RobiFaceMode robi_face_mode_ = robi::ROBI_MODE_IDLE;
    robi::RobiThemeId robi_theme_id_ = robi::ROBI_THEME_CYBER_CYAN;
    robi::RobiFaceStyle robi_face_style_ = robi::ROBI_STYLE_CYBER_VISOR;
    int last_applied_vol_ = -1;
    int last_applied_bri_ = -1;
    std::string last_hint_txt_;
    int hint_poll_div_ = 0;
    char robi_last_emotion_[24] = "neutral";
    lv_timer_t* robi_face_timer_ = nullptr;
    int64_t robi_last_tick_us_ = 0;

    lv_obj_t* hud_track_ = nullptr;
    lv_obj_t* hud_nodes_[kHudNodes] = {};

    CyberEyeWidgets eye_l_;
    CyberEyeWidgets eye_r_;
    lv_obj_t* cheek_l_ = nullptr;
    lv_obj_t* cheek_r_ = nullptr;
    lv_obj_t* visor_bar_ = nullptr;
    lv_obj_t* visor_scan_ = nullptr;

    lv_obj_t* brow_l_[kBrowSegs] = {};
    lv_obj_t* brow_r_[kBrowSegs] = {};

    lv_obj_t* mouth_glow_seg_[kMouthSegs] = {};
    lv_obj_t* mouth_seg_[kMouthSegs] = {};
    lv_obj_t* mouth_ring_ = nullptr;

    robi::FaceParams cur_ = {};
    robi::FaceParams tgt_ = {};

    float face_scale_ = 0.90f;
    float face_y_ = 208.0f;
    float talk_mix_ = 0.0f;
    float robi_voice_env_ = 0.0f;
    float robi_voice_smooth_ = 0.0f;
    int64_t robi_voice_env_ts_ = 0;

    float hud_phase_ = 0.0f;
    float phase_t_ = 0.0f;

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

    static lv_color_t RobiTintBright(float r, float g, float b, float white_mix) {
        return lv_color_hex(robi::RobiPackRgb(
            r + (255.0f - r) * white_mix,
            g + (255.0f - g) * white_mix,
            b + (255.0f - b) * white_mix));
    }

    // Dirty-checked LVGL property mutators: prevent redundant full-screen QSPI DMA invalidations
    static void RobiSetHidden(lv_obj_t* o, bool hidden) {
        if (o == nullptr) return;
        bool cur_hidden = lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
        if (hidden && !cur_hidden) {
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        } else if (!hidden && cur_hidden) {
            lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static void RobiSetRect(lv_obj_t* o, int x, int y, int w, int h) {
        if (o == nullptr) return;
        if (w < 2) w = 2;
        if (h < 2) h = 2;
        if (lv_obj_get_x(o) != x || lv_obj_get_y(o) != y) {
            lv_obj_set_pos(o, x, y);
        }
        if (lv_obj_get_width(o) != w || lv_obj_get_height(o) != h) {
            lv_obj_set_size(o, w, h);
        }
    }

    static void RobiSetRadius(lv_obj_t* o, int32_t r) {
        if (o != nullptr && lv_obj_get_style_radius(o, LV_PART_MAIN) != r) {
            lv_obj_set_style_radius(o, r, 0);
        }
    }

    static void RobiSetBorderWidth(lv_obj_t* o, int32_t w) {
        if (o != nullptr && lv_obj_get_style_border_width(o, LV_PART_MAIN) != w) {
            lv_obj_set_style_border_width(o, w, 0);
        }
    }

    static void RobiSetBgOpa(lv_obj_t* o, lv_opa_t opa) {
        if (o != nullptr && lv_obj_get_style_bg_opa(o, LV_PART_MAIN) != opa) {
            lv_obj_set_style_bg_opa(o, opa, 0);
        }
    }

    static void RobiSetBorderOpa(lv_obj_t* o, lv_opa_t opa) {
        if (o != nullptr && lv_obj_get_style_border_opa(o, LV_PART_MAIN) != opa) {
            lv_obj_set_style_border_opa(o, opa, 0);
        }
    }

    void RobiApplyPreset(const char* name, bool allow_transition_blink) {
        if (name == nullptr) return;
        bool changed = (std::strncmp(robi_last_emotion_, name, sizeof(robi_last_emotion_) - 1) != 0);
        std::strncpy(robi_last_emotion_, name, sizeof(robi_last_emotion_) - 1);
        robi_last_emotion_[sizeof(robi_last_emotion_) - 1] = '\0';

        auto preset = robi::RobiComputePresetParams(name, robi_theme_id_, robi_face_style_);
        tgt_ = preset.params;
        bounce_laugh_ = preset.bounce_laugh;
        tremor_shock_ = preset.tremor_shock;

        if (allow_transition_blink && changed && blink_phase_ < 0.0f &&
            std::strcmp(name, "surprised") != 0 && std::strcmp(name, "shocked") != 0) {
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

        e.iris = RobiPlainObj(e.core);
        lv_obj_set_style_bg_opa(e.iris, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(e.iris, 3, 0);
        lv_obj_set_style_border_opa(e.iris, LV_OPA_50, 0);
        lv_obj_set_style_radius(e.iris, LV_RADIUS_CIRCLE, 0);

        e.pupil = RobiPlainObj(e.core);
        lv_obj_set_style_bg_color(e.pupil, lv_color_hex(0x030B1A), 0);
        lv_obj_set_style_bg_opa(e.pupil, LV_OPA_90, 0);
        lv_obj_set_style_border_width(e.pupil, 0, 0);
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

    void RobiRenderEye(CyberEyeWidgets& e, float cx, float cy, float ew, float eh, float rad,
                       float pupil_sc, float gx, float gy, float sq_top, float sq_bot,
                       float scan_norm, lv_color_t c_hi, lv_color_t c_mid, bool color_dirty) {
        float open_ratio = 1.0f - robi::RobiClamp(sq_top * 0.88f + sq_bot * 0.72f, 0.0f, 0.90f);
        float eff_eh = std::max(10.0f, eh * open_ratio);
        int i_ew = static_cast<int>(ew + 0.5f);
        int i_eh = static_cast<int>(eff_eh + 0.5f);
        int i_rad = static_cast<int>(robi::RobiClamp(rad, 4.0f, std::min(ew, eff_eh) * 0.5f) + 0.5f);
        int core_x = static_cast<int>(cx - ew * 0.5f + 0.5f);
        int core_y = static_cast<int>(cy - eff_eh * 0.5f + 0.5f);
        bool is_pixel = (robi_face_style_ == robi::ROBI_STYLE_RETRO_PIXEL);
        bool is_kawaii = (robi_face_style_ == robi::ROBI_STYLE_ROUND_KAWAII);
        bool is_mech = (robi_face_style_ == robi::ROBI_STYLE_OWL_MECH);

        int go_w = i_ew + 18;
        int go_h = i_eh + 18;
        RobiSetRect(e.glow_outer, static_cast<int>(cx - go_w * 0.5f), static_cast<int>(cy - go_h * 0.5f), go_w, go_h);
        RobiSetRadius(e.glow_outer, is_pixel ? 9 : (i_rad + 10));

        int gi_w = i_ew + 8;
        int gi_h = i_eh + 8;
        RobiSetRect(e.glow_inner, static_cast<int>(cx - gi_w * 0.5f), static_cast<int>(cy - gi_h * 0.5f), gi_w, gi_h);
        RobiSetRadius(e.glow_inner, is_pixel ? 7 : (i_rad + 4));

        RobiSetRect(e.core, core_x, core_y, i_ew, i_eh);
        RobiSetRadius(e.core, i_rad);
        RobiSetBorderWidth(e.core, is_mech ? 4 : 2);

        if (color_dirty) {
            lv_obj_set_style_bg_color(e.glow_outer, c_mid, 0);
            lv_obj_set_style_bg_color(e.glow_inner, c_mid, 0);
            lv_obj_set_style_bg_color(e.core, c_mid, 0);
            lv_obj_set_style_bg_grad_color(e.core, c_mid, 0);
            lv_obj_set_style_border_color(e.core, c_hi, 0);
            lv_obj_set_style_border_color(e.iris, c_hi, 0);
            lv_obj_set_style_bg_color(e.closed_slit, c_hi, 0);
        }

        RobiSetHidden(e.lid_top, true);
        RobiSetHidden(e.lid_bot, true);
        RobiSetHidden(e.closed_slit, true);

        if (eff_eh <= 20.0f) {
            RobiSetHidden(e.iris, true);
            RobiSetHidden(e.pupil, true);
            RobiSetHidden(e.scanline, true);
            RobiSetHidden(e.spec_main, true);
            RobiSetHidden(e.spec_sub, true);
            return;
        }

        RobiSetHidden(e.iris, false);
        RobiSetHidden(e.pupil, false);
        RobiSetHidden(e.spec_main, false);
        RobiSetHidden(e.spec_sub, false);

        float iris_ratio = is_mech ? 0.72f : 0.58f;
        int iris_d = std::max(16, static_cast<int>(std::min(ew, eff_eh) * iris_ratio));
        float max_ix = std::max(2.0f, (ew - iris_d) * 0.5f - 5.0f);
        float max_iy = std::max(2.0f, (eff_eh - iris_d) * 0.5f - 4.0f);
        int iris_x = static_cast<int>((ew - iris_d) * 0.5f + robi::RobiClamp(gx * 0.25f, -max_ix, max_ix));
        int iris_y = static_cast<int>((eff_eh - iris_d) * 0.5f + robi::RobiClamp(gy * 0.25f, -max_iy, max_iy));
        RobiSetRect(e.iris, iris_x, iris_y, iris_d, iris_d);
        RobiSetRadius(e.iris, is_pixel ? 4 : LV_RADIUS_CIRCLE);
        RobiSetBorderWidth(e.iris, is_mech ? 4 : 2);
        RobiSetBorderOpa(e.iris, is_kawaii ? LV_OPA_30 : (is_mech ? LV_OPA_90 : LV_OPA_60));

        int pup_d = std::max(14, static_cast<int>(std::min(ew, eff_eh) * pupil_sc));
        float max_px = std::max(2.0f, (ew - pup_d) * 0.5f - 6.0f);
        float max_py = std::max(2.0f, (eff_eh - pup_d) * 0.5f - 5.0f);
        int pup_x = static_cast<int>((ew - pup_d) * 0.5f + robi::RobiClamp(gx * 0.35f, -max_px, max_px));
        int pup_y = static_cast<int>((eff_eh - pup_d) * 0.5f + robi::RobiClamp(gy * 0.35f, -max_py, max_py));
        RobiSetRect(e.pupil, pup_x, pup_y, pup_d, pup_d);
        RobiSetRadius(e.pupil, is_pixel ? 3 : LV_RADIUS_CIRCLE);
        RobiSetBgOpa(e.pupil, LV_OPA_90);

        if (is_kawaii) {
            RobiSetHidden(e.scanline, true);
        } else {
            RobiSetHidden(e.scanline, false);
            float sy_f = eff_eh * 0.18f + scan_norm * (eff_eh * 0.64f);
            int scan_w = static_cast<int>(ew * 0.72f);
            int scan_x = (i_ew - scan_w) / 2;
            RobiSetRect(e.scanline, scan_x, static_cast<int>(sy_f), scan_w, is_pixel ? 4 : 3);
        }

        int sp_sz = std::clamp(static_cast<int>(std::min(ew, eff_eh) * (is_kawaii ? 0.23f : 0.18f)), 8, 22);
        int sp1_x = static_cast<int>(robi::RobiClamp(ew * 0.28f - sp_sz * 0.5f + gx * 0.22f, 5.0f, ew - sp_sz - 5.0f));
        int sp1_y = static_cast<int>(robi::RobiClamp(eff_eh * 0.26f - sp_sz * 0.5f + gy * 0.22f, 4.0f, eff_eh - sp_sz - 4.0f));
        RobiSetRect(e.spec_main, sp1_x, sp1_y, sp_sz, sp_sz);
        RobiSetRadius(e.spec_main, is_pixel ? 2 : LV_RADIUS_CIRCLE);

        int sp2_sz = std::clamp(sp_sz / 2, 5, 11);
        int sp2_x = static_cast<int>(robi::RobiClamp(ew * 0.70f - sp2_sz * 0.5f + gx * 0.16f, 5.0f, ew - sp2_sz - 5.0f));
        int sp2_y = static_cast<int>(robi::RobiClamp(eff_eh * 0.72f - sp2_sz * 0.5f + gy * 0.16f, 4.0f, eff_eh - sp2_sz - 4.0f));
        RobiSetRect(e.spec_sub, sp2_x, sp2_y, sp2_sz, sp2_sz);
        RobiSetRadius(e.spec_sub, is_pixel ? 2 : LV_RADIUS_CIRCLE);
    }

    void RobiRenderBrow(lv_obj_t** segs, float cx, float cy, float half_w, float tilt_px,
                        bool is_left, lv_opa_t opa, lv_color_t c_hi, bool color_dirty) {
        if (opa < 14) {
            for (int i = 0; i < kBrowSegs; i++) {
                RobiSetHidden(segs[i], true);
            }
            return;
        }
        bool is_pixel = (robi_face_style_ == robi::ROBI_STYLE_RETRO_PIXEL);
        bool is_kawaii = (robi_face_style_ == robi::ROBI_STYLE_ROUND_KAWAII);
        bool is_mech = (robi_face_style_ == robi::ROBI_STYLE_OWL_MECH);
        int seg_w = is_mech ? 18 : 15;
        int seg_h = is_mech ? 9 : 7;
        float span_hw = std::min(half_w, 32.0f * face_scale_);
        float slope = is_left ? tilt_px : -tilt_px;

        for (int i = 0; i < kBrowSegs; i++) {
            RobiSetHidden(segs[i], false);
            float u = (static_cast<float>(i) - 2.0f) / 2.0f;
            float px = cx + u * span_hw;
            float arch = is_pixel ? 0.0f : (-std::cos(u * 1.2f) * (is_kawaii ? 9.0f : 6.0f) * face_scale_);
            float py = cy + arch + u * slope;
            RobiSetRect(segs[i], static_cast<int>(px - seg_w * 0.5f), static_cast<int>(py - seg_h * 0.5f), seg_w, seg_h);
            RobiSetBgOpa(segs[i], opa);
            if (color_dirty) {
                lv_obj_set_style_bg_color(segs[i], c_hi, 0);
            }
        }
    }

    static void OnFaceTick(lv_timer_t* t) {
        auto* self = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(t));
        if (self != nullptr && self->robi_face_ready_) {
            self->RobiStepFace();
        }
    }

    void RobiStepFace() {
        auto dev_state = Application::GetInstance().GetDeviceState();
        if (dev_state == kDeviceStateUpgrading) {
            return;
        }

        int64_t now_us = esp_timer_get_time();
        float dt = (robi_last_tick_us_ == 0)
                       ? 0.040f
                       : static_cast<float>(now_us - robi_last_tick_us_) / 1000000.0f;
        robi_last_tick_us_ = now_us;
        dt = robi::RobiClamp(dt, 0.010f, 0.090f);
        phase_t_ += dt;

        RobiStepArcadeAndImuLocked(now_us, dt);
        if (imu_sleeping_ || heads_up_.IsActive() || trivia_.IsActive()) {
            return;
        }

        if (++hint_poll_div_ >= 50) {
            hint_poll_div_ = 0;
            RobiUpdateSetupHintVisibility(robi_face_mode_ == robi::ROBI_MODE_IDLE);
        }

        bool voice_in = false;
        if (robi_face_mode_ == robi::ROBI_MODE_LISTENING) {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                voice_in = proto->IsStreamingSpeech();
            }
        }
        bool has_buttons = (robi_profiles_ != nullptr);
        bool menu_open = RobiIsAnyMenuOpen();

        float idle_scale = menu_open ? 0.84f : (has_buttons ? 0.96f : 1.06f);
        float idle_fy = menu_open ? 148.0f : (has_buttons ? 196.0f : 228.0f);
        float tgt_scale = (robi_face_mode_ == robi::ROBI_MODE_IDLE) ? idle_scale : (voice_in ? 1.05f : 1.02f);
        float tgt_fy = (robi_face_mode_ == robi::ROBI_MODE_IDLE) ? idle_fy : 228.0f;
        float tgt_talk = (robi_face_mode_ == robi::ROBI_MODE_SPEAKING) ? 1.0f : 0.0f;

        float k_fast = robi::RobiClamp(dt * 14.0f, 0.0f, 1.0f);
        float k_med = robi::RobiClamp(dt * 9.5f, 0.0f, 1.0f);
        float k_col = robi::RobiClamp(dt * 7.5f, 0.0f, 1.0f);

        face_scale_ = robi::RobiLerp(face_scale_, tgt_scale, k_med);
        face_y_ = robi::RobiLerp(face_y_, tgt_fy, k_med);
        talk_mix_ = robi::RobiLerp(talk_mix_, tgt_talk, k_fast);

        cur_.eye_w = robi::RobiLerp(cur_.eye_w, tgt_.eye_w, k_med);
        cur_.eye_h = robi::RobiLerp(cur_.eye_h, tgt_.eye_h, k_med);
        cur_.eye_radius = robi::RobiLerp(cur_.eye_radius, tgt_.eye_radius, k_med);
        cur_.pupil_scale = robi::RobiLerp(cur_.pupil_scale, tgt_.pupil_scale, k_med);
        cur_.gaze_x = robi::RobiLerp(cur_.gaze_x, tgt_.gaze_x, k_fast);
        cur_.gaze_y = robi::RobiLerp(cur_.gaze_y, tgt_.gaze_y, k_fast);
        cur_.squint_top_l = robi::RobiLerp(cur_.squint_top_l, tgt_.squint_top_l, k_fast);
        cur_.squint_top_r = robi::RobiLerp(cur_.squint_top_r, tgt_.squint_top_r, k_fast);
        cur_.squint_bot_l = robi::RobiLerp(cur_.squint_bot_l, tgt_.squint_bot_l, k_fast);
        cur_.squint_bot_r = robi::RobiLerp(cur_.squint_bot_r, tgt_.squint_bot_r, k_fast);
        cur_.brow_ang_l = robi::RobiLerp(cur_.brow_ang_l, tgt_.brow_ang_l, k_med);
        cur_.brow_ang_r = robi::RobiLerp(cur_.brow_ang_r, tgt_.brow_ang_r, k_med);
        cur_.brow_y_l = robi::RobiLerp(cur_.brow_y_l, tgt_.brow_y_l, k_med);
        cur_.brow_y_r = robi::RobiLerp(cur_.brow_y_r, tgt_.brow_y_r, k_med);
        cur_.brow_opa = robi::RobiLerp(cur_.brow_opa, tgt_.brow_opa, k_med);
        cur_.mouth_w = robi::RobiLerp(cur_.mouth_w, tgt_.mouth_w, k_fast);
        cur_.mouth_curve = robi::RobiLerp(cur_.mouth_curve, tgt_.mouth_curve, k_fast);
        cur_.mouth_tilt = robi::RobiLerp(cur_.mouth_tilt, tgt_.mouth_tilt, k_fast);
        cur_.mouth_wave = robi::RobiLerp(cur_.mouth_wave, tgt_.mouth_wave, k_med);
        cur_.mouth_o = robi::RobiLerp(cur_.mouth_o, tgt_.mouth_o, k_fast);
        cur_.cheek_opa = robi::RobiLerp(cur_.cheek_opa, tgt_.cheek_opa, k_med);
        cur_.visor_opa = robi::RobiLerp(cur_.visor_opa, tgt_.visor_opa, k_med);
        cur_.col_r = robi::RobiLerp(cur_.col_r, tgt_.col_r, k_col);
        cur_.col_g = robi::RobiLerp(cur_.col_g, tgt_.col_g, k_col);
        cur_.col_b = robi::RobiLerp(cur_.col_b, tgt_.col_b, k_col);

        float blink_amt = 0.0f;
        if (blink_phase_ >= 0.0f) {
            blink_phase_ += dt / 0.155f;
            if (blink_phase_ >= 1.0f) {
                blink_phase_ = -1.0f;
                if (pending_double_blink_) {
                    pending_double_blink_ = false;
                    next_blink_s_ = 0.11f;
                } else {
                    next_blink_s_ = 2.5f + static_cast<float>(std::rand() % 290) / 100.0f;
                    pending_double_blink_ = ((std::rand() % 100) < 18);
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

        next_saccade_s_ -= dt;
        if (next_saccade_s_ <= 0.0f) {
            if (robi_face_mode_ == robi::ROBI_MODE_LISTENING) {
                saccade_tgt_x_ = static_cast<float>((std::rand() % 7) - 3);
                saccade_tgt_y_ = static_cast<float>((std::rand() % 5) - 3);
                next_saccade_s_ = 1.6f;
            } else if (robi_face_mode_ == robi::ROBI_MODE_SPEAKING) {
                saccade_tgt_x_ = static_cast<float>((std::rand() % 17) - 8);
                saccade_tgt_y_ = static_cast<float>((std::rand() % 11) - 5);
                next_saccade_s_ = 0.9f + static_cast<float>(std::rand() % 140) / 100.0f;
            } else {
                int r = std::rand() % 100;
                if (r < 52) {
                    saccade_tgt_x_ = 0.0f;
                    saccade_tgt_y_ = 0.0f;
                } else {
                    saccade_tgt_x_ = static_cast<float>((std::rand() % 29) - 14);
                    saccade_tgt_y_ = static_cast<float>((std::rand() % 17) - 8);
                }
                next_saccade_s_ = 1.5f + static_cast<float>(std::rand() % 240) / 100.0f;
            }
        }
        saccade_x_ = robi::RobiLerp(saccade_x_, saccade_tgt_x_, robi::RobiClamp(dt * 10.0f, 0.0f, 1.0f));
        saccade_y_ = robi::RobiLerp(saccade_y_, saccade_tgt_y_, robi::RobiClamp(dt * 10.0f, 0.0f, 1.0f));

        // 20Hz lock-free real-time voice envelope from GeminiLiveProtocol (closes mouth cleanly on pauses)
        float target_voice = 0.0f;
        if (robi_face_mode_ == robi::ROBI_MODE_SPEAKING) {
            int live_lvl = -1;
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                live_lvl = proto->GetLiveVoiceLevel(now_us);
            }
            if (live_lvl >= 0) {
                target_voice = static_cast<float>(live_lvl) / 100.0f;
            } else if (now_us - robi_voice_env_ts_ < 340000) {
                target_voice = robi_voice_env_;
            } else {
                float cad = 0.42f + 0.28f * std::sin(phase_t_ * 9.8f) + 0.18f * std::sin(phase_t_ * 16.4f);
                target_voice = robi::RobiClamp(cad, 0.06f, 0.85f);
            }
        }
        robi_voice_smooth_ = robi::RobiLerp(robi_voice_smooth_, target_voice,
                                            robi::RobiClamp(dt * 20.0f, 0.0f, 1.0f));

        uint32_t cur_hex = robi::RobiPackRgb(cur_.col_r, cur_.col_g, cur_.col_b);
        bool color_dirty = (cur_hex != last_primary_hex_);
        last_primary_hex_ = cur_hex;
        lv_color_t c_mid = lv_color_hex(cur_hex);
        lv_color_t c_hi = RobiTintBright(cur_.col_r, cur_.col_g, cur_.col_b, 0.58f);

        float hud_spd = 0.85f;
        if (robi_face_mode_ == robi::ROBI_MODE_LISTENING) {
            hud_spd = voice_in ? 3.8f : 1.9f;
        } else if (robi_face_mode_ == robi::ROBI_MODE_THINKING) {
            hud_spd = 5.4f;
        } else if (robi_face_mode_ == robi::ROBI_MODE_SPEAKING) {
            hud_spd = 1.6f + robi_voice_smooth_ * 3.0f;
        }
        hud_phase_ = std::fmod(hud_phase_ + hud_spd * dt, 6.2831853f);

        if (color_dirty) {
            lv_obj_set_style_border_color(hud_track_, c_mid, 0);
        }
        for (int i = 0; i < kHudNodes; i++) {
            float ang = static_cast<float>(i) * (6.2831853f / static_cast<float>(kHudNodes));
            float wave = 0.5f + 0.5f * std::cos(2.0f * (ang - hud_phase_));
            float glow = wave * wave;
            if (robi_face_mode_ == robi::ROBI_MODE_SPEAKING) {
                glow = robi::RobiClamp(0.22f + glow * 0.45f + robi_voice_smooth_ * 0.55f, 0.0f, 1.0f);
            } else if (voice_in) {
                glow = robi::RobiClamp(0.35f + glow * 0.65f, 0.0f, 1.0f);
            }
            int nd = 7 + static_cast<int>(glow * 6.0f + 0.5f);
            float r_ring = 219.0f - glow * 2.5f;
            int nx = static_cast<int>(233.0f + std::cos(ang) * r_ring - nd * 0.5f + 0.5f);
            int ny = static_cast<int>(233.0f + std::sin(ang) * r_ring - nd * 0.5f + 0.5f);
            RobiSetRect(hud_nodes_[i], nx, ny, nd, nd);
            lv_opa_t n_opa = static_cast<lv_opa_t>(robi::RobiClamp(38.0f + glow * 217.0f, 30.0f, 255.0f));
            RobiSetBgOpa(hud_nodes_[i], n_opa);
            if (color_dirty) {
                lv_obj_set_style_bg_color(hud_nodes_[i], (i % 2 == 0) ? c_hi : c_mid, 0);
            }
        }

        float breathe_y = std::sin(phase_t_ * 2.2f) * 2.8f;
        if (bounce_laugh_) {
            breathe_y += std::sin(phase_t_ * 18.0f) * 3.2f;
        }
        float jitter_x = tremor_shock_ ? std::sin(phase_t_ * 42.0f) * 2.2f : 0.0f;

        float cx_mid = LV_HOR_RES * 0.5f + (cur_.gaze_x + saccade_x_) * 0.32f + jitter_x;
        float cy_eyes = face_y_ - 22.0f * face_scale_ + breathe_y + (cur_.gaze_y + saccade_y_) * 0.24f;
        bool full_visor = (robi_face_style_ == robi::ROBI_STYLE_VISOR_CYCLOPS);
        float sep = (full_visor ? 80.0f : 78.0f) * face_scale_;
        float ew = cur_.eye_w * face_scale_;
        float eh = cur_.eye_h * face_scale_ * (1.0f + robi_voice_smooth_ * 0.05f);
        float rad = cur_.eye_radius * face_scale_;
        float gx = (cur_.gaze_x + saccade_x_) * face_scale_;
        float gy = (cur_.gaze_y + saccade_y_) * face_scale_;

        float sq_top_l = std::max(cur_.squint_top_l, blink_amt);
        float sq_top_r = std::max(cur_.squint_top_r, blink_amt);
        float scan_norm = 0.5f + 0.46f * std::sin(phase_t_ * 1.65f);

        float gx_l = gx;
        float gy_l = gy;
        float gx_r = gx;
        float gy_r = gy;
        if (dizzy_until_us_ > now_us) {
            float spin = phase_t_ * 11.5f;
            gx_l += std::cos(spin) * 18.0f * face_scale_;
            gy_l += std::sin(spin) * 18.0f * face_scale_;
            gx_r -= std::cos(spin) * 18.0f * face_scale_;
            gy_r += std::sin(spin) * 18.0f * face_scale_;
        }

        RobiRenderEye(eye_l_, cx_mid - sep, cy_eyes, ew, eh, rad, cur_.pupil_scale,
                      gx_l, gy_l, sq_top_l, cur_.squint_bot_l, scan_norm, c_hi, c_mid, color_dirty);
        RobiRenderEye(eye_r_, cx_mid + sep, cy_eyes, ew, eh, rad, cur_.pupil_scale,
                      gx_r, gy_r, sq_top_r, cur_.squint_bot_r, scan_norm, c_hi, c_mid, color_dirty);

        float brow_offset = full_visor ? (48.0f * face_scale_) : (eh * 0.5f + 18.0f * face_scale_);
        float brow_base_y = cy_eyes - brow_offset - robi_voice_smooth_ * 4.0f;
        lv_opa_t b_opa = static_cast<lv_opa_t>(robi::RobiClamp(cur_.brow_opa * 255.0f, 0.0f, 255.0f));
        RobiRenderBrow(brow_l_, cx_mid - sep, brow_base_y + cur_.brow_y_l * face_scale_,
                       ew * 0.42f, cur_.brow_ang_l * face_scale_, true, b_opa, c_hi, color_dirty);
        RobiRenderBrow(brow_r_, cx_mid + sep, brow_base_y + cur_.brow_y_r * face_scale_,
                       ew * 0.42f, cur_.brow_ang_r * face_scale_, false, b_opa, c_hi, color_dirty);

        if (cur_.cheek_opa > 0.04f) {
            RobiSetHidden(cheek_l_, false);
            RobiSetHidden(cheek_r_, false);
            lv_opa_t copa = static_cast<lv_opa_t>(robi::RobiClamp(cur_.cheek_opa * 135.0f, 0.0f, 180.0f));
            int cw = static_cast<int>(38.0f * face_scale_);
            int ch = static_cast<int>(16.0f * face_scale_);
            int cy_ch = static_cast<int>(cy_eyes + eh * 0.54f);
            RobiSetRect(cheek_l_, static_cast<int>(cx_mid - sep - ew * 0.36f - cw * 0.5f), cy_ch, cw, ch);
            RobiSetBgOpa(cheek_l_, copa);
            RobiSetRect(cheek_r_, static_cast<int>(cx_mid + sep + ew * 0.36f - cw * 0.5f), cy_ch, cw, ch);
            RobiSetBgOpa(cheek_r_, copa);
        } else {
            RobiSetHidden(cheek_l_, true);
            RobiSetHidden(cheek_r_, true);
        }

        if (cur_.visor_opa > 0.04f) {
            RobiSetHidden(visor_bar_, false);
            int vw = static_cast<int>(sep * 2.0f + ew + (full_visor ? 44.0f * face_scale_ : 30.0f));
            int vh = static_cast<int>(full_visor ? (eh + 26.0f * face_scale_) : (eh * 0.56f));
            RobiSetRect(visor_bar_, static_cast<int>(cx_mid - vw * 0.5f),
                        static_cast<int>(cy_eyes - (full_visor ? vh * 0.50f : vh * 0.56f)), vw, vh);
            RobiSetBgOpa(visor_bar_, static_cast<lv_opa_t>(cur_.visor_opa * (full_visor ? 210.0f : 185.0f)));
            RobiSetBorderOpa(visor_bar_, static_cast<lv_opa_t>(cur_.visor_opa * 245.0f));
            if (color_dirty) {
                lv_obj_set_style_border_color(visor_bar_, c_mid, 0);
                lv_obj_set_style_bg_color(visor_bar_, lv_color_hex(0x061022), 0);
                lv_obj_set_style_bg_grad_color(visor_bar_, lv_color_hex(0x0A1931), 0);
                if (visor_scan_ != nullptr) {
                    lv_obj_set_style_bg_color(visor_scan_, c_hi, 0);
                }
            }
            if (visor_scan_ != nullptr) {
                if (full_visor && vw > 72 && vh > 16) {
                    RobiSetHidden(visor_scan_, false);
                    float sweep = 0.5f + 0.5f * std::sin(phase_t_ * 2.8f);
                    int sx = 14 + static_cast<int>(sweep * static_cast<float>(vw - 64));
                    RobiSetRect(visor_scan_, sx, 6, 36, vh - 12);
                } else {
                    RobiSetHidden(visor_scan_, true);
                }
            }
        } else {
            RobiSetHidden(visor_bar_, true);
            RobiSetHidden(visor_scan_, true);
        }

        float mouth_cx = LV_HOR_RES * 0.5f + (cur_.gaze_x + saccade_x_) * 0.16f;
        float mouth_cy = cy_eyes + 94.0f * face_scale_ + breathe_y * 0.7f;

        float o_vis = cur_.mouth_o * (1.0f - talk_mix_ * 0.85f);
        float seg_vis = 1.0f - o_vis * 0.92f;

        if (o_vis > 0.06f) {
            RobiSetHidden(mouth_ring_, false);
            int rw = static_cast<int>(44.0f * face_scale_);
            int rh = static_cast<int>(42.0f * face_scale_);
            RobiSetRect(mouth_ring_, static_cast<int>(mouth_cx - rw * 0.5f), static_cast<int>(mouth_cy - rh * 0.5f), rw, rh);
            RobiSetBorderOpa(mouth_ring_, static_cast<lv_opa_t>(robi::RobiClamp(o_vis * 255.0f, 0.0f, 255.0f)));
            if (color_dirty) {
                lv_obj_set_style_border_color(mouth_ring_, c_hi, 0);
            }
        } else {
            RobiSetHidden(mouth_ring_, true);
        }

        if (seg_vis > 0.06f) {
            static const float kBarWeight[kMouthSegs] = {0.28f, 0.50f, 0.74f, 0.92f, 1.00f, 0.92f, 0.74f, 0.50f, 0.28f};
            float rest_hw = cur_.mouth_w * face_scale_;
            float talk_hw = 64.0f * face_scale_;
            float hw = robi::RobiLerp(rest_hw, talk_hw, talk_mix_);
            float crv = cur_.mouth_curve * face_scale_ * robi::RobiLerp(1.0f, 0.55f, talk_mix_);
            float tlt = cur_.mouth_tilt * face_scale_ * (1.0f - talk_mix_ * 0.6f);
            float wav = cur_.mouth_wave * face_scale_ * (1.0f - talk_mix_ * 0.7f);

            int seg_w = (robi_face_style_ == robi::ROBI_STYLE_RETRO_PIXEL) ? 10 : 9;
            lv_opa_t core_opa = static_cast<lv_opa_t>(robi::RobiClamp(seg_vis * 255.0f, 0.0f, 255.0f));
            lv_opa_t glow_opa = static_cast<lv_opa_t>(robi::RobiClamp(seg_vis * 55.0f, 0.0f, 80.0f));

            for (int i = 0; i < kMouthSegs; i++) {
                RobiSetHidden(mouth_seg_[i], false);
                RobiSetHidden(mouth_glow_seg_[i], false);

                float u = (static_cast<float>(i) / static_cast<float>(kMouthSegs - 1)) * 2.0f - 1.0f;
                float px = mouth_cx + u * hw;
                float py = mouth_cy + crv * 0.42f * (1.0f - u * u) - u * tlt * 0.5f;
                if (wav > 0.2f) {
                    py += std::sin(u * 4.8f + phase_t_ * 7.0f) * wav;
                }

                float ripple = 0.72f + 0.28f * std::sin(phase_t_ * 15.0f + static_cast<float>(i) * 0.88f);
                float voice_h = (8.0f + robi_voice_smooth_ * kBarWeight[i] * ripple * 48.0f);
                float taper_rest = 9.0f + (1.0f - std::fabs(u)) * 3.0f;
                int seg_h = static_cast<int>(robi::RobiLerp(taper_rest, voice_h, talk_mix_) + 0.5f);

                RobiSetRect(mouth_glow_seg_[i],
                            static_cast<int>(px - (seg_w + 4) * 0.5f),
                            static_cast<int>(py - (seg_h + 4) * 0.5f),
                            seg_w + 4, seg_h + 4);
                RobiSetRect(mouth_seg_[i],
                            static_cast<int>(px - seg_w * 0.5f),
                            static_cast<int>(py - seg_h * 0.5f),
                            seg_w, seg_h);

                RobiSetBgOpa(mouth_glow_seg_[i], glow_opa);
                RobiSetBgOpa(mouth_seg_[i], core_opa);
                if (color_dirty) {
                    lv_obj_set_style_bg_color(mouth_glow_seg_[i], c_mid, 0);
                    lv_obj_set_style_bg_color(mouth_seg_[i], c_mid, 0);
                    lv_obj_set_style_bg_grad_color(mouth_seg_[i], c_hi, 0);
                }
            }
        } else {
            for (int i = 0; i < kMouthSegs; i++) {
                RobiSetHidden(mouth_seg_[i], true);
                RobiSetHidden(mouth_glow_seg_[i], true);
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
            lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_CLICKABLE);
        }
        if (status_bar_ != nullptr) {
            lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_CLICKABLE);
        }
        RobiSetHidden(emoji_label_, true);
        RobiSetHidden(emoji_image_, true);
        if (emoji_box_ == nullptr) {
            return;
        }

        lv_obj_set_size(emoji_box_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(emoji_box_);
        lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(emoji_box_, 0, 0);
        lv_obj_set_style_border_width(emoji_box_, 0, 0);
        lv_obj_set_scrollbar_mode(emoji_box_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(emoji_box_, OnTouchGestureTrack, LV_EVENT_PRESSED, nullptr);
        lv_obj_add_event_cb(emoji_box_, OnTouchGestureTrack, LV_EVENT_PRESSING, nullptr);
        lv_obj_add_event_cb(emoji_box_, OnTouchGestureTrack, LV_EVENT_RELEASED, nullptr);
        lv_obj_add_event_cb(emoji_box_, OnTouchGestureTrack, LV_EVENT_PRESS_LOST, nullptr);
        lv_obj_add_event_cb(emoji_box_, OnFaceTap, LV_EVENT_CLICKED, nullptr);

        hud_track_ = RobiPlainObj(emoji_box_);
        lv_obj_set_size(hud_track_, 446, 446);
        lv_obj_center(hud_track_);
        lv_obj_set_style_bg_opa(hud_track_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(hud_track_, 2, 0);
        lv_obj_set_style_border_opa(hud_track_, LV_OPA_20, 0);
        lv_obj_set_style_radius(hud_track_, LV_RADIUS_CIRCLE, 0);

        for (int i = 0; i < kHudNodes; i++) {
            hud_nodes_[i] = RobiPlainObj(emoji_box_);
            lv_obj_set_style_radius(hud_nodes_[i], LV_RADIUS_CIRCLE, 0);
        }

        cheek_l_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(cheek_l_, lv_color_hex(0xFF4D94), 0);
        lv_obj_set_style_radius(cheek_l_, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(cheek_l_, LV_OBJ_FLAG_HIDDEN);

        cheek_r_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(cheek_r_, lv_color_hex(0xFF4D94), 0);
        lv_obj_set_style_radius(cheek_r_, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(cheek_r_, LV_OBJ_FLAG_HIDDEN);

        visor_bar_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(visor_bar_, lv_color_hex(0x061022), 0);
        lv_obj_set_style_bg_grad_color(visor_bar_, lv_color_hex(0x0A1931), 0);
        lv_obj_set_style_bg_grad_dir(visor_bar_, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_width(visor_bar_, 3, 0);
        lv_obj_set_style_border_color(visor_bar_, lv_color_hex(robi::RobiThemeBaseHex(robi_theme_id_)), 0);
        lv_obj_set_style_radius(visor_bar_, 22, 0);
        lv_obj_add_flag(visor_bar_, LV_OBJ_FLAG_HIDDEN);

        // Sweeping horizontal scanner beam inside visor_cyclops (matching Wizard face-preview.js 1:1)
        visor_scan_ = RobiPlainObj(visor_bar_);
        lv_obj_set_style_bg_opa(visor_scan_, LV_OPA_40, 0);
        lv_obj_set_style_radius(visor_scan_, 10, 0);
        lv_obj_add_flag(visor_scan_, LV_OBJ_FLAG_HIDDEN);

        RobiCreateEye(emoji_box_, eye_l_);
        RobiCreateEye(emoji_box_, eye_r_);

        for (int i = 0; i < kBrowSegs; i++) {
            brow_l_[i] = RobiPlainObj(emoji_box_);
            lv_obj_set_style_radius(brow_l_[i], LV_RADIUS_CIRCLE, 0);
            brow_r_[i] = RobiPlainObj(emoji_box_);
            lv_obj_set_style_radius(brow_r_[i], LV_RADIUS_CIRCLE, 0);
        }

        for (int i = 0; i < kMouthSegs; i++) {
            mouth_glow_seg_[i] = RobiPlainObj(emoji_box_);
            lv_obj_set_style_radius(mouth_glow_seg_[i], LV_RADIUS_CIRCLE, 0);
        }
        for (int i = 0; i < kMouthSegs; i++) {
            mouth_seg_[i] = RobiPlainObj(emoji_box_);
            lv_obj_set_style_bg_grad_dir(mouth_seg_[i], LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_radius(mouth_seg_[i], LV_RADIUS_CIRCLE, 0);
        }

        mouth_ring_ = RobiPlainObj(emoji_box_);
        lv_obj_set_style_bg_color(mouth_ring_, lv_color_hex(0x031828), 0);
        lv_obj_set_style_bg_opa(mouth_ring_, LV_OPA_60, 0);
        lv_obj_set_style_border_width(mouth_ring_, 5, 0);
        lv_obj_set_style_radius(mouth_ring_, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(mouth_ring_, LV_OBJ_FLAG_HIDDEN);

        cur_ = robi::RobiDefaultParams(robi_theme_id_, robi_face_style_);
        tgt_ = cur_;
        robi_face_ready_ = true;
        RobiStepFace();
        robi_face_timer_ = lv_timer_create(OnFaceTick, 40, this);
    }

    bool RobiIsAnyMenuOpen() const {
        for (int i = 0; i < kMaxKids; ++i) {
            if (robi_menus_[i] != nullptr && !lv_obj_has_flag(robi_menus_[i], LV_OBJ_FLAG_HIDDEN)) {
                return true;
            }
        }
        return false;
    }

    void RobiHideAllMenus() {
        for (int i = 0; i < kMaxKids; ++i) {
            RobiSetHidden(robi_menus_[i], true);
        }
    }

    void RobiUpdateSetupHintVisibility(bool idle) {
        if (robi_setup_hint_ == nullptr || robi_setup_lbl_ == nullptr) return;
        bool has_kids = (RobiConfig::GetInstance().Get().persona_mode != "assistant") &&
                        RobiConfig::GetInstance().HasKids();
        if (!idle || RobiIsAnyMenuOpen() || RobiIsArcadeOrGameOpen()) {
            RobiSetHidden(robi_setup_hint_, true);
            return;
        }
        std::string ip = WifiManager::GetInstance().GetIpAddress();
        if (has_kids && !ip.empty() && RobiConfig::GetInstance().HasApiKey()) {
            RobiSetHidden(robi_setup_hint_, true);
            return;
        }
        std::string txt = ip.empty() ? "robi.local" : ip;
        if (txt != last_hint_txt_) {
            last_hint_txt_ = txt;
            lv_obj_set_style_base_dir(robi_setup_lbl_, LV_BASE_DIR_LTR, 0);
            lv_label_set_text(robi_setup_lbl_, txt.c_str());
            lv_obj_align(robi_setup_hint_, LV_ALIGN_BOTTOM_MID, 0, -10);
        }
        RobiSetHidden(robi_setup_hint_, false);
    }

    static void RobiWakePowerSave() {
        Application::GetInstance().Schedule([]() {
            Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
        });
    }

    static void OnProfile(lv_event_t* e) {
        RobiWakePowerSave();
        lv_obj_t* menu = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
        auto* self = robi_self_;
        if (self == nullptr || menu == nullptr) return;
        for (int i = 0; i < kMaxKids; ++i) {
            if (self->robi_menus_[i] == menu) {
                RobiConfig::GetInstance().ActivateKidScope(static_cast<size_t>(i));
                break;
            }
        }
        RobiSetHidden(self->robi_profiles_, true);
        RobiSetHidden(self->robi_games_home_btn_, true);
        RobiSetHidden(menu, false);
        self->RobiUpdateSetupHintVisibility(true);
        self->RobiStopMenuTimer();
        self->robi_menu_timer_ = lv_timer_create(OnMenuTimeout, 20000, nullptr);
        lv_timer_set_repeat_count(self->robi_menu_timer_, 1);
    }

    static void OnMenuTimeout(lv_timer_t* t) {
        (void)t;
        auto* self = robi_self_;
        if (self == nullptr) return;
        self->robi_menu_timer_ = nullptr;
        self->RobiHideAllMenus();
        bool is_idle = (Application::GetInstance().GetDeviceState() == kDeviceStateIdle);
        if (is_idle) {
            if (self->robi_profiles_ != nullptr) {
                RobiSetHidden(self->robi_profiles_, false);
            }
            if (!self->RobiIsArcadeOrGameOpen()) {
                RobiSetHidden(self->robi_games_home_btn_, false);
            }
        }
        self->RobiUpdateSetupHintVisibility(is_idle);
    }

    void RobiStopMenuTimer() {
        if (robi_menu_timer_ != nullptr) {
            lv_timer_delete(robi_menu_timer_);
            robi_menu_timer_ = nullptr;
        }
    }

    static void OnActivity(lv_event_t* e) {
        RobiWakePowerSave();
        const char* says = static_cast<const char*>(lv_event_get_user_data(e));
        auto* self = robi_self_;
        if (self == nullptr || says == nullptr) return;
        self->RobiStopMenuTimer();
        self->RobiHideAllMenus();
        ESP_LOGI(TAG, "Robi activity: %s", says);
        Application::GetInstance().WakeWordInvoke(says);
    }

    static void OnFaceTap(lv_event_t* e) {
        (void)e;
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr) return;
        if (esp_timer_get_time() < self->swipe_guard_until_us_) {
            return;
        }
        auto& app = Application::GetInstance();
        auto dev_state = app.GetDeviceState();
        if (dev_state == kDeviceStateListening) {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                if (proto->IsStreamingSpeech()) {
                    ESP_LOGI(TAG, "Robi face tap in listening: committing active speech turn");
                    proto->CommitListeningUtterance();
                    return;
                }
                if (proto->CanExitListeningByTap()) {
                    ESP_LOGI(TAG, "Robi face tap in listening: return to standby");
                    app.ToggleChatState();
                }
            } else {
                app.ToggleChatState();
            }
            return;
        }
        if (dev_state != kDeviceStateIdle) return;

        if (self->RobiIsAnyMenuOpen()) {
            self->RobiStopMenuTimer();
            self->RobiHideAllMenus();
            RobiSetHidden(self->robi_profiles_, false);
            RobiSetHidden(self->robi_games_home_btn_, false);
            self->RobiUpdateSetupHintVisibility(true);
            return;
        }
        RobiConfig::GetInstance().ActivateDefaultWakeScope();
        ESP_LOGI(TAG, "Robi face tap: start listening (scope=%s)",
                 RobiConfig::GetInstance().GetActiveScope().c_str());
        app.ToggleChatState();
    }

    static void OnTap(lv_event_t* e) {
        (void)e;
        auto& app = Application::GetInstance();
        app.Schedule([]() {
            Board::GetInstance().SetPowerSaveLevel(PowerSaveLevel::PERFORMANCE);
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "Robi tap: stop speaking");
                app.AbortSpeaking(kAbortReasonNone);
            }
        });
    }

    static lv_obj_t* RobiBox(lv_obj_t* parent, int w_pct, lv_flex_flow_t flow) {
        lv_obj_t* o = lv_obj_create(parent);
        lv_obj_set_size(o, LV_HOR_RES * w_pct / 100, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_pad_all(o, 0, 0);
        lv_obj_set_style_pad_row(o, LV_VER_RES * 2 / 100, 0);
        lv_obj_set_style_pad_column(o, LV_HOR_RES * 2 / 100, 0);
        lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(o, flow);
        lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        return o;
    }

    static lv_obj_t* RobiButton(lv_obj_t* parent, const char* text, uint32_t color, int w_pct, int h_pct,
                                lv_event_cb_t cb, void* data) {
        lv_obj_t* b = lv_button_create(parent);
        lv_obj_set_size(b, LV_HOR_RES * w_pct / 100, LV_VER_RES * h_pct / 100);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x0B1528), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_90, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_50, LV_STATE_PRESSED);
        lv_obj_set_style_transform_width(b, 0, LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(b, 0, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(color), 0);
        lv_obj_set_style_border_opa(b, LV_OPA_COVER, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_color(l, lv_color_hex(0xF8FAFC), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
        return b;
    }

    void RobiRebuildDynamicConfigLocked() {
        RobiStopMenuTimer();
        for (int i = 0; i < kMaxKids; ++i) {
            if (robi_menus_[i] != nullptr) {
                lv_obj_delete(robi_menus_[i]);
                robi_menus_[i] = nullptr;
            }
        }
        if (robi_profiles_ != nullptr) {
            lv_obj_delete(robi_profiles_);
            robi_profiles_ = nullptr;
        }
        robi_prompt_pool_.clear();

        auto cfg = RobiConfig::GetInstance().Get();
        robi_theme_id_ = robi::RobiParseThemeId(cfg.theme);
        robi_face_style_ = robi::RobiParseFaceStyle(cfg.face_style);
        last_primary_hex_ = 0xFFFFFFFFu;

        int seg_rad = (robi_face_style_ == robi::ROBI_STYLE_RETRO_PIXEL) ? 2 : LV_RADIUS_CIRCLE;
        for (int i = 0; i < kHudNodes; ++i) {
            RobiSetRadius(hud_nodes_[i], seg_rad);
        }
        for (int i = 0; i < kBrowSegs; ++i) {
            RobiSetRadius(brow_l_[i], seg_rad);
            RobiSetRadius(brow_r_[i], seg_rad);
        }
        for (int i = 0; i < kMouthSegs; ++i) {
            RobiSetRadius(mouth_seg_[i], seg_rad);
            RobiSetRadius(mouth_glow_seg_[i], seg_rad);
        }

        if (cfg.volume != last_applied_vol_) {
            last_applied_vol_ = cfg.volume;
            auto* codec = Board::GetInstance().GetAudioCodec();
            if (codec != nullptr) codec->SetOutputVolume(cfg.volume);
        }
        if (cfg.brightness != last_applied_bri_) {
            last_applied_bri_ = cfg.brightness;
            auto* bl = Board::GetInstance().GetBacklight();
            if (bl != nullptr) bl->SetBrightness(cfg.brightness);
        }

        if (robi_face_ready_) {
            RobiApplyPreset(robi_last_emotion_, false);
            cur_ = tgt_;
        }

        size_t n_kids = (cfg.persona_mode == "assistant")
                            ? 0
                            : std::min<size_t>(cfg.kids.size(), kMaxKids);
        robi_prompt_pool_.reserve(n_kids * 5 + 2);
        RobiRefreshGameLocaleLocked();

        if (n_kids > 0) {
            int btn_w_px = (n_kids == 1) ? 156 : ((n_kids == 2) ? 132 : 98);
            int btn_h_px = 46;

            robi_profiles_ = RobiBox(lv_screen_active(), 82, LV_FLEX_FLOW_ROW);
            lv_obj_set_height(robi_profiles_, 54);
            lv_obj_set_flex_align(robi_profiles_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(robi_profiles_, (n_kids == 3) ? 8 : 16, 0);
            lv_obj_align(robi_profiles_, LV_ALIGN_BOTTOM_MID, 0, -62);

            for (size_t i = 0; i < n_kids; ++i) {
                const auto& kid = cfg.kids[i];
                lv_obj_t* menu = RobiBox(lv_screen_active(), 76, LV_FLEX_FLOW_ROW_WRAP);
                lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, -44);

                for (size_t a = 0; a < 4 && a < kid.activities.size(); ++a) {
                    std::string full_p =
                        (cfg.language == "en-US")
                            ? ("Hi, it's " + kid.name + ". " + kid.activities[a].prompt)
                            : ("זה " + kid.name + ". " + kid.activities[a].prompt);
                    robi_prompt_pool_.push_back(std::move(full_p));
                    const char* p_ptr = robi_prompt_pool_.back().c_str();
                    lv_obj_t* ab = RobiButton(menu, kid.activities[a].label.c_str(), kid.color_hex, 34, 11, OnActivity,
                                              const_cast<char*>(p_ptr));
                    lv_obj_set_size(ab, 156, 48);
                }

                std::string talk_lbl = (cfg.language == "en-US") ? "Just Chat" : "סתם לדבר";
                std::string talk_prm =
                    (cfg.language == "en-US") ? ("Hi, it's " + kid.name + "!") : ("זה " + kid.name);
                robi_prompt_pool_.push_back(std::move(talk_prm));
                const char* talk_ptr = robi_prompt_pool_.back().c_str();
                lv_obj_t* tb = RobiButton(menu, talk_lbl.c_str(), 0x374151, 42, 9, OnActivity, const_cast<char*>(talk_ptr));
                lv_obj_set_size(tb, 196, 42);
                lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
                robi_menus_[i] = menu;

                lv_obj_t* kb = RobiButton(robi_profiles_, kid.name.c_str(), kid.color_hex, 28, 10, OnProfile, menu);
                lv_obj_set_size(kb, btn_w_px, btn_h_px);
            }

            if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
                RobiSetHidden(robi_profiles_, true);
            }
        }

        if (robi_games_home_btn_ != nullptr) {
            lv_obj_move_foreground(robi_games_home_btn_);
            bool show_games_btn = (Application::GetInstance().GetDeviceState() == kDeviceStateIdle) &&
                                  !RobiIsAnyMenuOpen() && !RobiIsArcadeOrGameOpen() && !imu_sleeping_;
            RobiSetHidden(robi_games_home_btn_, !show_games_btn);
        }
        if (robi_setup_hint_ != nullptr) {
            lv_obj_move_foreground(robi_setup_hint_);
        }
        if (robi_tap_ != nullptr) {
            lv_obj_move_foreground(robi_tap_);
        }

        bool is_idle = (Application::GetInstance().GetDeviceState() == kDeviceStateIdle);
        RobiUpdateSetupHintVisibility(is_idle);
        if (robi_face_ready_) {
            RobiStepFace();
        }
    }

    void RobiSetupUI() {
        robi_self_ = this;
        RobiConfig::GetInstance().EnsureLoaded();
        SetHideSubtitle(true);

        robi_setup_hint_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(robi_setup_hint_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(robi_setup_hint_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(robi_setup_hint_, lv_color_hex(0x0B1528), 0);
        lv_obj_set_style_bg_opa(robi_setup_hint_, LV_OPA_80, 0);
        lv_obj_set_style_border_width(robi_setup_hint_, 1, 0);
        lv_obj_set_style_border_color(robi_setup_hint_, lv_color_hex(0x1E3A5F), 0);
        lv_obj_set_style_pad_hor(robi_setup_hint_, 12, 0);
        lv_obj_set_style_pad_ver(robi_setup_hint_, 3, 0);
        lv_obj_set_style_base_dir(robi_setup_hint_, LV_BASE_DIR_LTR, 0);
        lv_obj_remove_flag(robi_setup_hint_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(robi_setup_hint_, LV_OBJ_FLAG_SCROLLABLE);
        robi_setup_lbl_ = lv_label_create(robi_setup_hint_);
        lv_obj_set_style_base_dir(robi_setup_lbl_, LV_BASE_DIR_LTR, 0);
        lv_label_set_text(robi_setup_lbl_, "robi.local");
        lv_obj_set_style_text_color(robi_setup_lbl_, lv_color_hex(0x94A3B8), 0);
        lv_obj_align(robi_setup_hint_, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_add_flag(robi_setup_hint_, LV_OBJ_FLAG_HIDDEN);

        robi_tap_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(robi_tap_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_tap_);
        lv_obj_set_style_bg_opa(robi_tap_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(robi_tap_, 0, 0);
        lv_obj_remove_flag(robi_tap_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(robi_tap_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(robi_tap_, OnTap, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_flag(robi_tap_, LV_OBJ_FLAG_HIDDEN);

        RobiInitCyberFace();
        RobiInitArcadeWidgets();
        RobiRebuildDynamicConfigLocked();

        RobiConfig::GetInstance().SetUiReloadCallback([]() {
            Application::GetInstance().Schedule([]() {
                if (robi_self_ != nullptr) {
                    DisplayLockGuard lock(robi_self_);
                    robi_self_->RobiRebuildDynamicConfigLocked();
                }
            });
        });

        RobiConfig::GetInstance().SetUiStatusProvider([]() -> std::string {
            if (robi_self_ == nullptr) return "{}";
            DisplayLockGuard lock(robi_self_);
            return robi_self_->RobiBuildUiStatusJsonLocked();
        });
    }

    std::string RobiBuildUiStatusJsonLocked() {
        auto rect_str = [](lv_obj_t* o) -> std::string {
            if (o == nullptr) return "null";
            lv_obj_update_layout(o);
            lv_area_t a;
            lv_obj_get_coords(o, &a);
            bool vis = !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
            char buf[160];
            std::snprintf(buf, sizeof(buf), "{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"y2\":%d,\"vis\":%s}",
                          static_cast<int>(a.x1), static_cast<int>(a.y1),
                          static_cast<int>(a.x2 - a.x1 + 1), static_cast<int>(a.y2 - a.y1 + 1),
                          static_cast<int>(a.y2), vis ? "true" : "false");
            return std::string(buf);
        };

        int open_menu = -1;
        for (int i = 0; i < kMaxKids; ++i) {
            if (robi_menus_[i] != nullptr && !lv_obj_has_flag(robi_menus_[i], LV_OBJ_FLAG_HIDDEN)) {
                open_menu = i;
                break;
            }
        }

        char head[384];
        std::snprintf(
            head, sizeof(head),
            "{\"emotion\":\"%s\",\"face_mode\":%d,\"theme_id\":%d,\"style_id\":%d,"
            "\"face_y\":%.1f,\"face_scale\":%.2f,\"hint_text\":\"%s\",\"hint_dir\":%d,\"open_menu\":%d",
            robi_last_emotion_, static_cast<int>(robi_face_mode_),
            static_cast<int>(robi_theme_id_), static_cast<int>(robi_face_style_),
            face_y_, face_scale_, last_hint_txt_.c_str(),
            robi_setup_lbl_ ? static_cast<int>(lv_obj_get_style_base_dir(robi_setup_lbl_, LV_PART_MAIN)) : -1,
            open_menu);

        std::string out(head);
        out += ",\"eye_l\":" + rect_str(eye_l_.core);
        out += ",\"eye_r\":" + rect_str(eye_r_.core);
        out += ",\"visor\":" + rect_str(visor_bar_);
        out += ",\"mouth_mid\":" + rect_str(mouth_seg_[4]);
        out += ",\"profiles_box\":" + rect_str(robi_profiles_);
        out += ",\"hint_pill\":" + rect_str(robi_setup_hint_);
        if (open_menu >= 0) {
            out += ",\"active_menu\":" + rect_str(robi_menus_[open_menu]);
        }
        out += ",\"arcade_open\":" + std::string(RobiIsArcadeOrGameOpen() ? "true" : "false");
        out += ",\"headsup_phase\":" + std::to_string(static_cast<int>(heads_up_.phase()));
        out += ",\"trivia_visible\":" + std::string(trivia_visible_ ? "true" : "false");
        out += ",\"trivia_phase\":" + std::to_string(static_cast<int>(trivia_.phase()));
        out += ",\"trivia_q_idx\":" + std::to_string(static_cast<int>(trivia_.question_index()));
        out += ",\"trivia_tot\":" + std::to_string(static_cast<int>(trivia_.total_questions()));
        out += ",\"trivia_s0\":" + std::to_string(trivia_.score(0));
        out += ",\"trivia_s1\":" + std::to_string(trivia_.score(1));
        out += "}";
        return out;
    }

#include "robi/robi_arcade_ui.h"
    // ===================== end OpenRobi UI =====================
