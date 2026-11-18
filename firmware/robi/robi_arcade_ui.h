    // ===================== HeyRobi v3.2 Interactive Arcade, Trivia & IMU Layer =====================
    // Included inside CustomLcdDisplay (from robi_display_ui.h).
    // Provides:
    // 1. Horizontal Swipe -> Robi Arcade ("נחש מה על המצח" Offline + AI, Solo & Head-to-Head Trivia)
    // 2. Shared 466x466 circular Category + AI Voice Topic picker for both Heads-Up & Trivia
    // 3. 100% deterministic C++ 8-Question Touch TriviaEngine + Live Gemini TV-Host narration
    // 4. Face-down Mute/Sleep, In-Hand Shake Dizzy counter-spinning eyes, & touch Z-axis calibration

    enum class GamePickerMode : uint8_t {
        kHeadsUp = 0,
        kTriviaSolo = 1,
        kTriviaH2H = 2,
    };

    lv_obj_t* robi_games_home_btn_ = nullptr;
    lv_obj_t* robi_arcade_scr_ = nullptr;
    lv_obj_t* robi_arcade_title_ = nullptr;
    lv_obj_t* robi_arcade_btn_hu_ = nullptr;
    lv_obj_t* robi_arcade_btn_tr1_ = nullptr;
    lv_obj_t* robi_arcade_btn_tr2_ = nullptr;
    lv_obj_t* robi_arcade_btn_back_ = nullptr;

    lv_obj_t* robi_hu_menu_ = nullptr;
    lv_obj_t* robi_hu_menu_title_ = nullptr;
    lv_obj_t* robi_hu_cat_btns_[6] = {};
    lv_obj_t* robi_hu_ai_btn_ = nullptr;
    lv_obj_t* robi_hu_back_btn_ = nullptr;

    lv_obj_t* robi_hu_stage_ = nullptr;
    lv_obj_t* robi_hu_ring_ = nullptr;
    lv_obj_t* robi_hu_top_lbl_ = nullptr;
    lv_obj_t* robi_hu_word_card_ = nullptr;
    lv_obj_t* robi_hu_word_lbl_ = nullptr;
    lv_obj_t* robi_hu_sub_lbl_ = nullptr;
    lv_obj_t* robi_hu_btn_skip_ = nullptr;
    lv_obj_t* robi_hu_btn_ok_ = nullptr;
    lv_obj_t* robi_hu_btn_again_ = nullptr;
    lv_obj_t* robi_hu_btn_exit_ = nullptr;
    lv_obj_t* robi_hu_btn_x_ = nullptr;

    lv_obj_t* robi_trivia_box_ = nullptr;
    lv_obj_t* robi_trivia_hdr_ = nullptr;
    lv_obj_t* robi_trivia_q_ = nullptr;
    lv_obj_t* robi_trivia_opts_[4] = {};
    lv_obj_t* robi_trivia_again_ = nullptr;
    lv_obj_t* robi_trivia_done_ = nullptr;
    lv_obj_t* robi_trivia_x_ = nullptr;

    GamePickerMode picker_mode_ = GamePickerMode::kHeadsUp;

    robi::HeadsUpEngine heads_up_;
    int last_hu_cat_ = -1;
    std::string last_hu_word_rendered_;
    std::string last_hu_top_rendered_;
    robi::HeadsUpEngine::Phase last_hu_phase_ = robi::HeadsUpEngine::Phase::kInactive;

    robi::TriviaEngine trivia_;
    bool trivia_visible_ = false;
    uint32_t active_trivia_req_id_ = 0;
    int last_trivia_cat_ = -1;
    bool last_trivia_h2h_ = false;
    std::string last_trivia_custom_topic_;
    int64_t trivia_summary_speak_at_us_ = 0;

    int32_t touch_press_x_ = 0;
    int32_t touch_press_y_ = 0;
    int32_t touch_last_x_ = 0;
    int32_t touch_last_y_ = 0;
    bool touch_swipe_latched_ = false;
    int64_t swipe_guard_until_us_ = 0;

    int64_t dizzy_until_us_ = 0;
    bool imu_sleeping_ = false;
    int pre_sleep_bri_ = 85;

    bool RobiIsArcadeOrGameOpen() const {
        if (robi_arcade_scr_ != nullptr && !lv_obj_has_flag(robi_arcade_scr_, LV_OBJ_FLAG_HIDDEN)) {
            return true;
        }
        if (robi_hu_menu_ != nullptr && !lv_obj_has_flag(robi_hu_menu_, LV_OBJ_FLAG_HIDDEN)) {
            return true;
        }
        if (robi_hu_stage_ != nullptr && !lv_obj_has_flag(robi_hu_stage_, LV_OBJ_FLAG_HIDDEN)) {
            return true;
        }
        if (trivia_visible_ || trivia_.IsActive()) {
            return true;
        }
        return false;
    }

    static void SetButtonLabelText(lv_obj_t* btn, const char* txt) {
        if (btn == nullptr) return;
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl != nullptr) {
            lv_label_set_text(lbl, txt);
        }
    }

    static lv_obj_t* RobiPillBtnPx(lv_obj_t* parent, const char* text, uint32_t border_hex,
                                   int w_px, int h_px, lv_event_cb_t cb, void* user_data) {
        lv_obj_t* b = lv_button_create(parent);
        lv_obj_set_size(b, w_px, h_px);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x0B172E), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_90, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(border_hex), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_60, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, LV_STATE_PRESSED);
        lv_obj_set_style_transform_width(b, 0, LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(b, 0, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(border_hex), 0);
        lv_obj_set_style_pad_hor(b, 10, 0);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, w_px - 20);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xF8FAFC), 0);
        lv_obj_center(l);
        if (cb != nullptr) {
            lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
        }
        return b;
    }

    void RobiRefreshPickerLabelsLocked() {
        if (robi_hu_menu_ == nullptr) return;
        auto cfg = RobiConfig::GetInstance().Get();
        const bool en = (cfg.language == "en-US");

        if (picker_mode_ == GamePickerMode::kHeadsUp) {
            lv_obj_set_style_border_color(robi_hu_menu_, lv_color_hex(0x10B981), 0);
            lv_obj_set_style_text_color(robi_hu_menu_title_, lv_color_hex(0x34D399), 0);
            lv_label_set_text(robi_hu_menu_title_, en ? "Heads Up — Topic" : "נחש על המצח — נושא");
            for (int i = 0; i < 5; ++i) {
                SetButtonLabelText(robi_hu_cat_btns_[i],
                                   en ? robi::kHeadsUpCategories[i].title_en
                                      : robi::kHeadsUpCategories[i].title_he);
                lv_obj_set_style_border_color(robi_hu_cat_btns_[i],
                                              lv_color_hex(robi::kHeadsUpCategories[i].color_hex), 0);
            }
        } else {
            bool h2h = (picker_mode_ == GamePickerMode::kTriviaH2H);
            uint32_t accent = h2h ? 0xF59E0B : 0x00E5FF;
            lv_obj_set_style_border_color(robi_hu_menu_, lv_color_hex(accent), 0);
            lv_obj_set_style_text_color(robi_hu_menu_title_, lv_color_hex(accent), 0);
            lv_label_set_text(robi_hu_menu_title_,
                              h2h ? (en ? "2P Battle — Topic" : "ראש בראש — בחרו נושא")
                                  : (en ? "Trivia — Pick Topic" : "טריוויה — בחרו נושא"));
            for (int i = 0; i < 5; ++i) {
                SetButtonLabelText(robi_hu_cat_btns_[i],
                                   en ? robi::kTriviaCategories[i].title_en
                                      : robi::kTriviaCategories[i].title_he);
                lv_obj_set_style_border_color(robi_hu_cat_btns_[i],
                                              lv_color_hex(robi::kTriviaCategories[i].color_hex), 0);
            }
        }
        SetButtonLabelText(robi_hu_cat_btns_[5], en ? "Mega Mix" : "ערבוב הכל");
        SetButtonLabelText(robi_hu_ai_btn_, en ? "Voice Custom Topic" : "נושא חופשי בקול");
        SetButtonLabelText(robi_hu_back_btn_, en ? "Back" : "חזרה");
    }

    void RobiRefreshGameLocaleLocked() {
        if (robi_arcade_scr_ == nullptr) return;
        auto cfg = RobiConfig::GetInstance().Get();
        const bool en = (cfg.language == "en-US");

        lv_label_set_text(robi_arcade_title_, en ? "Robi Games" : "משחקי רובי");
        SetButtonLabelText(robi_games_home_btn_, en ? "Games" : "משחקים");
        SetButtonLabelText(robi_arcade_btn_hu_, en ? "Heads Up! (Tilt Game)" : "נחש מה על המצח!");
        SetButtonLabelText(robi_arcade_btn_tr1_, en ? "Live Screen Trivia" : "טריוויה חיה — יחיד");
        if (cfg.kids.size() >= 2) {
            std::string h2h = en ? ("Battle: " + cfg.kids[0].name + " vs " + cfg.kids[1].name)
                                 : ("ראש בראש: " + cfg.kids[0].name + " מול " + cfg.kids[1].name);
            SetButtonLabelText(robi_arcade_btn_tr2_, h2h.c_str());
        } else {
            SetButtonLabelText(robi_arcade_btn_tr2_, en ? "2-Player Trivia Battle" : "טריוויה — ראש בראש");
        }
        SetButtonLabelText(robi_arcade_btn_back_, en ? "Back" : "חזרה");

        RobiRefreshPickerLabelsLocked();

        SetButtonLabelText(robi_hu_btn_skip_, en ? "Skip" : "דלג");
        SetButtonLabelText(robi_hu_btn_ok_, en ? "Correct!" : "נכון!");
        SetButtonLabelText(robi_hu_btn_again_, en ? "Again!" : "שוב!");
        SetButtonLabelText(robi_hu_btn_exit_, en ? "Done" : "סיום");

        SetButtonLabelText(robi_trivia_again_, en ? "Again!" : "שוב!");
        SetButtonLabelText(robi_trivia_done_, en ? "Done" : "סיום");
    }

    void RobiUpdateMicGameMuteLocked() {
        bool mute = heads_up_.IsActive() || trivia_.IsActive();
        robi::RobiGameHub::GetInstance().SetMicMutedForScreenGame(mute);
    }

    void RobiHideTriviaOverlayLocked(bool close_voice_session = false, bool restore_idle_widgets = false) {
        bool was_active = trivia_visible_ || trivia_.IsActive();
        trivia_.Stop();
        trivia_visible_ = false;
        active_trivia_req_id_ = 0;
        trivia_summary_speak_at_us_ = 0;
        RobiUpdateMicGameMuteLocked();
        RobiSetHidden(robi_trivia_box_, true);

        bool speaking = (Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking);
        RobiSetHidden(robi_tap_, !speaking);

        if (was_active && close_voice_session) {
            Application::GetInstance().Schedule([]() {
                auto& app = Application::GetInstance();
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    proto->CancelPendingTurnAudio();
                    if (proto->IsAudioChannelOpened()) {
                        proto->CloseAudioChannel(false);
                    }
                }
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    app.GetAudioService().EnableWakeWordDetection(true);
                }
            });
        }

        if (restore_idle_widgets && Application::GetInstance().GetDeviceState() == kDeviceStateIdle &&
            !RobiIsArcadeOrGameOpen()) {
            if (robi_profiles_ != nullptr) {
                RobiSetHidden(robi_profiles_, false);
            }
            if (!RobiIsAnyMenuOpen()) {
                RobiSetHidden(robi_games_home_btn_, false);
            }
            RobiUpdateSetupHintVisibility(true);
        }
    }

    void RobiCloseAllGameScreensLocked(bool restore_idle_widgets = true) {
        bool was_hu = heads_up_.IsActive();
        heads_up_.Stop();
        RobiUpdateMicGameMuteLocked();
#ifdef ESP_PLATFORM
        robi::RobiImuService::GetInstance().SetForeheadMode(false);
#endif
        RobiSetHidden(robi_arcade_scr_, true);
        RobiSetHidden(robi_hu_menu_, true);
        RobiSetHidden(robi_hu_stage_, true);

        bool is_idle = (Application::GetInstance().GetDeviceState() == kDeviceStateIdle);
        if (was_hu && is_idle && !trivia_.IsActive()) {
            Application::GetInstance().Schedule([]() {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    app.GetAudioService().EnableWakeWordDetection(true);
                }
            });
        }
        if (restore_idle_widgets && is_idle && !trivia_.IsActive()) {
            if (robi_profiles_ != nullptr) {
                RobiSetHidden(robi_profiles_, false);
            }
            if (!RobiIsAnyMenuOpen()) {
                RobiSetHidden(robi_games_home_btn_, false);
            }
            RobiUpdateSetupHintVisibility(true);
        }
    }

    void RobiOpenArcadeMenuLocked() {
        RobiStopMenuTimer();
        RobiHideAllMenus();
        RobiHideTriviaOverlayLocked(true, false);
        RobiSetHidden(robi_games_home_btn_, true);
        RobiSetHidden(robi_profiles_, true);
        RobiSetHidden(robi_setup_hint_, true);
        RobiSetHidden(robi_hu_menu_, true);
        RobiSetHidden(robi_hu_stage_, true);
        RobiRefreshGameLocaleLocked();
        RobiSetHidden(robi_arcade_scr_, false);
        lv_obj_move_foreground(robi_arcade_scr_);
    }

    void RobiOpenPickerMenuLocked(GamePickerMode mode) {
        picker_mode_ = mode;
        RobiSetHidden(robi_arcade_scr_, true);
        RobiSetHidden(robi_hu_stage_, true);
        RobiRefreshPickerLabelsLocked();
        RobiSetHidden(robi_hu_menu_, false);
        lv_obj_move_foreground(robi_hu_menu_);
    }

    void RobiSyncHeadsUpStageWidgetsLocked(bool force = false) {
        if (!heads_up_.IsActive() || robi_hu_stage_ == nullptr) return;
        const auto cfg = RobiConfig::GetInstance().Get();
        const bool en = (cfg.language == "en-US");
        const auto ph = heads_up_.phase();

        if (force || ph != last_hu_phase_) {
            last_hu_phase_ = ph;
            if (ph == robi::HeadsUpEngine::Phase::kFlashCorrect) {
                lv_obj_set_style_bg_color(robi_hu_stage_, lv_color_hex(0x064E3B), 0);
                lv_obj_set_style_border_color(robi_hu_ring_, lv_color_hex(0x10B981), 0);
            } else if (ph == robi::HeadsUpEngine::Phase::kFlashSkip) {
                lv_obj_set_style_bg_color(robi_hu_stage_, lv_color_hex(0x451A03), 0);
                lv_obj_set_style_border_color(robi_hu_ring_, lv_color_hex(0xF59E0B), 0);
            } else {
                lv_obj_set_style_bg_color(robi_hu_stage_, lv_color_hex(0x061022), 0);
                lv_obj_set_style_border_color(robi_hu_ring_, lv_color_hex(0x00E5FF), 0);
            }

            bool show_play_btns = (ph == robi::HeadsUpEngine::Phase::kPlaying);
            bool show_sum_btns = (ph == robi::HeadsUpEngine::Phase::kSummary);
            RobiSetHidden(robi_hu_btn_skip_, !show_play_btns);
            RobiSetHidden(robi_hu_btn_ok_, !show_play_btns);
            RobiSetHidden(robi_hu_btn_again_, !show_sum_btns);
            RobiSetHidden(robi_hu_btn_exit_, !show_sum_btns);
        }

        char top_buf[96];
        if (ph == robi::HeadsUpEngine::Phase::kCountdown) {
            std::snprintf(top_buf, sizeof(top_buf), "%s", heads_up_.topic_title().c_str());
        } else if (ph == robi::HeadsUpEngine::Phase::kSummary) {
            std::snprintf(top_buf, sizeof(top_buf),
                          en ? "Score: %d" : "ניקוד סופי: %d", heads_up_.correct_count());
        } else {
            std::snprintf(top_buf, sizeof(top_buf),
                          en ? "%ds  |  Score: %d" : "%d שנ'  |  ניקוד: %d",
                          heads_up_.remaining_sec(), heads_up_.correct_count());
        }
        if (force || last_hu_top_rendered_ != top_buf) {
            last_hu_top_rendered_ = top_buf;
            lv_label_set_text(robi_hu_top_lbl_, top_buf);
        }

        std::string main_txt;
        std::string sub_txt;
        if (ph == robi::HeadsUpEngine::Phase::kCountdown) {
            main_txt = std::to_string(heads_up_.countdown_sec());
            sub_txt = en ? "Hold on your forehead!" : "החזיקו על המצח!";
        } else if (ph == robi::HeadsUpEngine::Phase::kFlashCorrect) {
            main_txt = en ? "CORRECT! +1" : "נכון! +1";
            sub_txt = "";
        } else if (ph == robi::HeadsUpEngine::Phase::kFlashSkip) {
            main_txt = en ? "SKIP" : "דילוג";
            sub_txt = "";
        } else if (ph == robi::HeadsUpEngine::Phase::kPlaying) {
            main_txt = heads_up_.CurrentWord();
            sub_txt = en ? "Tilt Down = Correct | Up = Skip" : "למטה = נכון | למעלה = דלג";
        } else if (ph == robi::HeadsUpEngine::Phase::kSummary) {
            const auto& gw = heads_up_.guessed_words();
            if (gw.empty()) {
                main_txt = en ? "Time's Up!" : "נגמר הזמן!";
            } else {
                std::string joined;
                size_t lim = std::min<size_t>(gw.size(), 5);
                for (size_t i = 0; i < lim; ++i) {
                    if (i > 0) joined += " • ";
                    joined += gw[i];
                }
                if (gw.size() > lim) {
                    joined += " ...";
                }
                main_txt = std::move(joined);
            }
            sub_txt = en ? "Great round!" : "כל הכבוד! עוד סיבוב?";
        }

        if (force || last_hu_word_rendered_ != main_txt) {
            last_hu_word_rendered_ = main_txt;
            lv_label_set_text(robi_hu_word_lbl_, main_txt.c_str());
            lv_label_set_text(robi_hu_sub_lbl_, sub_txt.c_str());
        }
    }

    void RobiBeginHeadsUpStageLocked() {
        RobiStopMenuTimer();
        RobiHideAllMenus();
        RobiHideTriviaOverlayLocked(false, false);
        RobiSetHidden(robi_profiles_, true);
        RobiSetHidden(robi_setup_hint_, true);
        RobiSetHidden(robi_arcade_scr_, true);
        RobiSetHidden(robi_hu_menu_, true);
        last_hu_word_rendered_.clear();
        last_hu_top_rendered_.clear();
        last_hu_phase_ = robi::HeadsUpEngine::Phase::kInactive;
        RobiUpdateMicGameMuteLocked();
        RobiSetHidden(robi_hu_stage_, false);
        lv_obj_move_foreground(robi_hu_stage_);
        RobiSyncHeadsUpStageWidgetsLocked(true);

#ifdef ESP_PLATFORM
        robi::RobiImuService::GetInstance().SetForeheadMode(true);
        Application::GetInstance().Schedule([]() {
            auto& app = Application::GetInstance();
            app.GetAudioService().EnableWakeWordDetection(false);
            robi::PlayRobiSfx(robi::RobiSfxId::kCountdownTick, false);
        });
#endif
    }

    void RobiStartOfflineHeadsUpLocked(int cat_idx) {
        last_hu_cat_ = cat_idx;
        auto cfg = RobiConfig::GetInstance().Get();
        heads_up_.SeedRng(static_cast<uint32_t>(esp_timer_get_time()));
        heads_up_.StartOfflineRound(cat_idx, cfg.language == "en-US", 60);
        RobiBeginHeadsUpStageLocked();
    }

    // -------------------- Deterministic 8-Question Trivia UI & Voice Host --------------------
    static void ResolveTriviaPlayerNames(const RobiConfigData& cfg, std::string& p1_out, std::string& p2_out) {
        const bool en = (cfg.language == "en-US");
        p1_out = (cfg.kids.size() >= 1 && !cfg.kids[0].name.empty())
                     ? cfg.kids[0].name
                     : (en ? "Player 1" : "שחקן 1");
        p2_out = (cfg.kids.size() >= 2 && !cfg.kids[1].name.empty())
                     ? cfg.kids[1].name
                     : (en ? "Player 2" : "שחקן 2");
    }

    void RobiSyncTriviaWidgetsLocked() {
        if (!trivia_.IsActive() || robi_trivia_box_ == nullptr) {
            RobiHideTriviaOverlayLocked(false, false);
            return;
        }
        trivia_visible_ = true;
        RobiUpdateMicGameMuteLocked();
        RobiSetHidden(robi_profiles_, true);
        RobiSetHidden(robi_setup_hint_, true);
        RobiSetHidden(robi_tap_, true);

        const auto ph = trivia_.phase();
        const bool en = trivia_.english();

        uint32_t ring_col = 0x00E5FF;
        if (ph == robi::TriviaEngine::Phase::kSummary) {
            ring_col = 0x10B981;
        } else if (trivia_.head_to_head() && trivia_.CurrentPlayerIndex() == 1) {
            ring_col = 0xF59E0B;
        }
        lv_obj_set_style_border_color(robi_trivia_box_, lv_color_hex(ring_col), 0);

        std::string hdr = trivia_.FormatHeaderText();
        lv_label_set_text(robi_trivia_hdr_, hdr.c_str());

        if (ph == robi::TriviaEngine::Phase::kLoading) {
            lv_obj_align(robi_trivia_q_, LV_ALIGN_CENTER, 0, -10);
            lv_label_set_text(robi_trivia_q_,
                              en ? "Robi is preparing\n8 fresh trivia questions..."
                                 : "רובי מכין עכשיו\n8 שאלות טריוויה חדשות...");
            for (int i = 0; i < 4; ++i) {
                RobiSetHidden(robi_trivia_opts_[i], true);
            }
            RobiSetHidden(robi_trivia_again_, true);
            RobiSetHidden(robi_trivia_done_, true);
            RobiSetHidden(robi_trivia_x_, false);
        } else if (ph == robi::TriviaEngine::Phase::kSummary) {
            lv_obj_align(robi_trivia_q_, LV_ALIGN_TOP_MID, 0, 118);
            std::string body = trivia_.FormatSummaryBody();
            lv_label_set_text(robi_trivia_q_, body.c_str());
            for (int i = 0; i < 4; ++i) {
                RobiSetHidden(robi_trivia_opts_[i], true);
            }
            RobiSetHidden(robi_trivia_again_, false);
            RobiSetHidden(robi_trivia_done_, false);
            RobiSetHidden(robi_trivia_x_, true);
        } else {
            // kQuestion or kReveal
            lv_obj_align(robi_trivia_q_, LV_ALIGN_TOP_MID, 0, 66);
            const auto* cur = trivia_.CurrentQuestion();
            lv_label_set_text(robi_trivia_q_, cur ? cur->question.c_str() : "");

            uint32_t idle_border = (trivia_.head_to_head() && trivia_.CurrentPlayerIndex() == 1)
                                       ? 0xF59E0B
                                       : 0x00E5FF;
            for (int i = 0; i < 4; ++i) {
                if (cur != nullptr && i < static_cast<int>(cur->options.size())) {
                    SetButtonLabelText(robi_trivia_opts_[i], cur->options[static_cast<size_t>(i)].c_str());
                    uint32_t bg_hex = 0x0B172E;
                    uint32_t bd_hex = idle_border;
                    if (ph == robi::TriviaEngine::Phase::kReveal) {
                        if (i == cur->correct_index) {
                            bg_hex = 0x065F46;
                            bd_hex = 0x10B981;
                        } else if (i == trivia_.last_picked_index() && !trivia_.last_was_correct()) {
                            bg_hex = 0x7F1D1D;
                            bd_hex = 0xEF4444;
                        }
                    }
                    lv_obj_set_style_bg_color(robi_trivia_opts_[i], lv_color_hex(bg_hex), 0);
                    lv_obj_set_style_border_color(robi_trivia_opts_[i], lv_color_hex(bd_hex), 0);
                    RobiSetHidden(robi_trivia_opts_[i], false);
                } else {
                    RobiSetHidden(robi_trivia_opts_[i], true);
                }
            }
            RobiSetHidden(robi_trivia_again_, true);
            RobiSetHidden(robi_trivia_done_, true);
            RobiSetHidden(robi_trivia_x_, false);
        }

        RobiSetHidden(robi_trivia_box_, false);
        lv_obj_move_foreground(robi_trivia_box_);
    }

    void RobiSpeakCurrentTriviaQuestionLocked(bool include_prev_reaction) {
#ifdef ESP_PLATFORM
        if (!WifiManager::GetInstance().IsConnected() || !RobiConfig::GetInstance().HasApiKey()) {
            return;
        }
        const auto* cur = trivia_.CurrentQuestion();
        if (cur == nullptr || cur->question.empty()) return;

        const bool en = trivia_.english();
        const int q_num = static_cast<int>(trivia_.question_index() + 1);
        const int q_tot = static_cast<int>(trivia_.total_questions());
        const bool h2h = trivia_.head_to_head();
        const std::string cur_player = trivia_.CurrentPlayerName();

        std::string prompt;
        if (!include_prev_reaction) {
            if (en) {
                prompt = h2h ? ("😄 [Live Trivia Host — Question " + std::to_string(q_num) + "/" +
                                std::to_string(q_tot) + " for " + cur_player +
                                ". Read ONLY this exact question aloud in 1 short energetic sentence without spoiling the answer: \"" +
                                cur->question + "\"]")
                             : ("😄 [Live Trivia Host — Question " + std::to_string(q_num) + "/" +
                                std::to_string(q_tot) +
                                ". Read ONLY this exact question aloud in 1 short energetic sentence without spoiling the answer: \"" +
                                cur->question + "\"]");
            } else {
                prompt = h2h ? ("😄 [מנחה טריוויה חי — שאלה " + std::to_string(q_num) + " מתוך " +
                                std::to_string(q_tot) + ", תור " + cur_player +
                                ". הקרא עכשיו בקול חי ונלהב במשפט קצר אחד בלבד את השאלה הזו ל" + cur_player +
                                ", בלי להמציא שאלה נוספת ובלי לגלות את התשובה: \"" + cur->question + "\"]")
                             : ("😄 [מנחה טריוויה חי — שאלה " + std::to_string(q_num) + " מתוך " +
                                std::to_string(q_tot) +
                                ". הקרא עכשיו בקול חי ונלהב במשפט קצר אחד בלבד את השאלה הזו, בלי להמציא שאלה נוספת ובלי לגלות את התשובה: \"" +
                                cur->question + "\"]");
            }
        } else {
            const bool prev_ok = trivia_.last_was_correct();
            const std::string prev_p = trivia_.player_name(trivia_.last_answered_player());
            const std::string right_ans = trivia_.last_correct_text();
            if (en) {
                std::string react = prev_ok
                                        ? ("🤩 [Live Trivia Host: " + (h2h ? prev_p : std::string("Player")) +
                                           " got it RIGHT ('" + right_ans + "')! Cheer in 3-4 words, then immediately ask ")
                                        : ("😮 [Live Trivia Host: Correct answer was '" + right_ans +
                                           "'. Encourage warmly in 4 words, then immediately ask ");
                prompt = react + (h2h ? (cur_player + " ") : "") + "Question " + std::to_string(q_num) + "/" +
                         std::to_string(q_tot) +
                         " using ONLY this exact wording without revealing the answer: \"" + cur->question + "\"]";
            } else {
                std::string react = prev_ok
                                        ? ("🤩 [טריוויה: " + (h2h ? (prev_p + " צדק") : std::string("תשובה נכונה")) +
                                           " ('" + right_ans + "')! פרגן ב-3 עד 5 מילים קצרות, ומיד שאל בהתלהבות ")
                                        : ("😮 [טריוויה: התשובה הנכונה הייתה '" + right_ans +
                                           "'. עודד בחיוך ב-4 מילים קצרות, ומיד שאל בהתלהבות ");
                prompt = react + (h2h ? ("את " + cur_player + " את ") : "את ") + "שאלה " +
                         std::to_string(q_num) + " מתוך " + std::to_string(q_tot) +
                         " בדיוק בנוסח הזה (בלי לגלות את התשובה!): \"" + cur->question + "\"]";
            }
        }

        if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
            proto->InjectUserPromptTurn(prompt);
        }
#else
        (void)include_prev_reaction;
#endif
    }

    void RobiActivateTriviaPersonaScope(bool head_to_head) {
        auto cfg = RobiConfig::GetInstance().Get();
        if (cfg.persona_mode == "assistant") {
            RobiConfig::GetInstance().SetActiveScope("assistant");
        } else if (!head_to_head && cfg.kids.size() == 1) {
            RobiConfig::GetInstance().ActivateKidScope(0);
        } else if (!cfg.kids.empty()) {
            RobiConfig::GetInstance().SetActiveScope("kids");
        } else {
            RobiConfig::GetInstance().ActivateDefaultWakeScope();
        }
    }

    void RobiStartTriviaRoundLocked(int cat_idx, bool head_to_head, const std::string& custom_topic = "") {
        RobiStopMenuTimer();
        RobiHideAllMenus();
        RobiCloseAllGameScreensLocked(false);

        last_trivia_cat_ = cat_idx;
        last_trivia_h2h_ = head_to_head;
        last_trivia_custom_topic_ = custom_topic;
        trivia_summary_speak_at_us_ = 0;

        RobiActivateTriviaPersonaScope(head_to_head);
        auto cfg = RobiConfig::GetInstance().Get();
        const bool en = (cfg.language == "en-US");
        std::string p1, p2;
        ResolveTriviaPlayerNames(cfg, p1, p2);
        trivia_.SeedRng(static_cast<uint32_t>(esp_timer_get_time()));

        std::string ui_title;
        std::string ai_topic;
        if (!custom_topic.empty()) {
            ui_title = custom_topic;
            ai_topic = custom_topic;
        } else if (cat_idx >= 0 && cat_idx < 5) {
            ui_title = en ? robi::kTriviaCategories[cat_idx].title_en
                          : robi::kTriviaCategories[cat_idx].title_he;
            ai_topic = en ? robi::kTriviaCategories[cat_idx].ai_topic_en
                          : robi::kTriviaCategories[cat_idx].ai_topic_he;
        } else {
            ui_title = en ? "Mega Mix" : "ערבוב הכל";
            ai_topic = en ? "fun general knowledge, science, animals, sports, and riddles"
                          : "ידע כללי מרתק: חיות, חלל, מדע, ספורט, עולם וחידות";
        }

#ifdef ESP_PLATFORM
        bool online = WifiManager::GetInstance().IsConnected() &&
                      !WifiManager::GetInstance().GetIpAddress().empty() &&
                      RobiConfig::GetInstance().HasApiKey();
        if (online) {
            uint32_t req_id = robi::RobiGameHub::GetInstance().NextTriviaRequestId();
            active_trivia_req_id_ = req_id;
            trivia_.BeginLoading(ui_title, head_to_head, p1, p2, en);
            RobiSyncTriviaWidgetsLocked();

            Application::GetInstance().Schedule([req_id, ai_topic, head_to_head, en]() {
                auto& app = Application::GetInstance();
                app.GetAudioService().EnableWakeWordDetection(false);
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    proto->CancelPendingTurnAudio();
                    if (proto->IsAudioChannelOpened()) {
                        proto->CloseAudioChannel(false);
                    }
                }
                robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
                GeminiLiveProtocol::RequestAsyncAiTriviaDeck(req_id, ai_topic, head_to_head, en);
            });
            return;
        }
#endif
        active_trivia_req_id_ = 0;
        trivia_.StartOfflineDeck(cat_idx, head_to_head, p1, p2, en, 8);
        RobiSyncTriviaWidgetsLocked();
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            Application::GetInstance().GetAudioService().EnableWakeWordDetection(false);
            robi::PlayRobiSfx(robi::RobiSfxId::kGoStart, false);
        });
#endif
    }

    void RobiTriggerHorizontalSwipeLocked() {
        int64_t now_us = esp_timer_get_time();
        if (now_us < swipe_guard_until_us_) {
            return;
        }
        swipe_guard_until_us_ = now_us + 450000LL;
        RobiWakePowerSave();
        if (heads_up_.IsActive() || trivia_.IsActive()) {
            return;
        }
        auto dev_state = Application::GetInstance().GetDeviceState();
        if (dev_state != kDeviceStateIdle && dev_state != kDeviceStateListening) {
            return;
        }
        if (dev_state == kDeviceStateListening) {
            // User swiped while Robi was waiting in Listening -> close voice channel & open Games!
            Application::GetInstance().Schedule([]() {
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    proto->CancelPendingTurnAudio();
                    if (proto->IsAudioChannelOpened()) {
                        proto->CloseAudioChannel(false);
                    }
                }
            });
        }
        if (RobiIsArcadeOrGameOpen()) {
            RobiCloseAllGameScreensLocked(true);
        } else {
            RobiOpenArcadeMenuLocked();
        }
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
        });
#endif
    }

    static void OnScreenGesture(lv_event_t*) {
        auto* self = robi_self_;
        if (self == nullptr) return;
        lv_indev_t* indev = lv_indev_active();
        if (indev == nullptr) return;
        lv_dir_t dir = lv_indev_get_gesture_dir(indev);
        if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
            self->touch_swipe_latched_ = true;
            self->RobiTriggerHorizontalSwipeLocked();
        }
    }

    static void OnTouchGestureTrack(lv_event_t* e) {
        auto* self = robi_self_;
        if (self == nullptr) return;
        lv_event_code_t code = lv_event_get_code(e);
        lv_indev_t* indev = lv_indev_active();
        if (indev == nullptr) return;
        lv_point_t pt = {};
        lv_indev_get_point(indev, &pt);

        if (code == LV_EVENT_PRESSED) {
            self->touch_press_x_ = pt.x;
            self->touch_press_y_ = pt.y;
            self->touch_last_x_ = pt.x;
            self->touch_last_y_ = pt.y;
            self->touch_swipe_latched_ = false;
#ifdef ESP_PLATFORM
            robi::RobiImuService::GetInstance().NotifyScreenTouched();
#endif
            return;
        }

        if (code == LV_EVENT_PRESSING) {
            // Note: esp_lvgl_port zeroes data->point to (0,0) on LV_EVENT_RELEASED,
            // so real finger drag coordinates MUST be sampled during LV_EVENT_PRESSING!
            if (pt.x != 0 || pt.y != 0) {
                self->touch_last_x_ = pt.x;
                self->touch_last_y_ = pt.y;
            }
            if (!self->touch_swipe_latched_) {
                int32_t dx = self->touch_last_x_ - self->touch_press_x_;
                int32_t dy = self->touch_last_y_ - self->touch_press_y_;
                if (std::abs(dx) >= 56 && std::abs(dx) * 10 > std::abs(dy) * 12) {
                    self->touch_swipe_latched_ = true;
                    self->RobiTriggerHorizontalSwipeLocked();
                }
            }
            return;
        }

        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (!self->touch_swipe_latched_) {
                int32_t dx = self->touch_last_x_ - self->touch_press_x_;
                int32_t dy = self->touch_last_y_ - self->touch_press_y_;
                if (std::abs(dx) >= 56 && std::abs(dx) * 10 > std::abs(dy) * 12) {
                    self->touch_swipe_latched_ = true;
                    self->RobiTriggerHorizontalSwipeLocked();
                }
            }
        }
    }

    static void OnHomeOpenArcadeBtn(lv_event_t*) {
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr) return;
        self->RobiOpenArcadeMenuLocked();
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
        });
#endif
    }

    static void OnArcadeBtnHeadsUp(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiOpenPickerMenuLocked(GamePickerMode::kHeadsUp);
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
        });
#endif
    }

    static void OnArcadeBtnTriviaSolo(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiOpenPickerMenuLocked(GamePickerMode::kTriviaSolo);
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
        });
#endif
    }

    static void OnArcadeBtnTriviaH2H(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiOpenPickerMenuLocked(GamePickerMode::kTriviaH2H);
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
        });
#endif
    }

    static void OnArcadeBack(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiCloseAllGameScreensLocked(true);
    }

    static void OnHeadsUpBackToArcade(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiOpenArcadeMenuLocked();
    }

    static void OnPickerSelectCategory(lv_event_t* e) {
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr) return;
        int cat_idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        if (self->picker_mode_ == GamePickerMode::kHeadsUp) {
            self->RobiStartOfflineHeadsUpLocked(cat_idx);
        } else {
            bool h2h = (self->picker_mode_ == GamePickerMode::kTriviaH2H);
            self->RobiStartTriviaRoundLocked(cat_idx, h2h);
        }
    }

    static void OnPickerAiVoiceTopic(lv_event_t*) {
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr) return;
        const GamePickerMode mode = self->picker_mode_;
        self->RobiCloseAllGameScreensLocked(false);
        robi::RobiGameHub::GetInstance().SetMicMutedForScreenGame(false);

        auto cfg = RobiConfig::GetInstance().Get();
        if (!cfg.kids.empty() && cfg.persona_mode != "assistant") {
            RobiConfig::GetInstance().SetActiveScope("kids");
        } else {
            RobiConfig::GetInstance().ActivateDefaultWakeScope();
        }

        std::string p;
        if (mode == GamePickerMode::kHeadsUp) {
            p = (cfg.language == "en-US")
                    ? "We want to play 'Heads Up' on our forehead! Ask me in one short sentence what topic we want, and as soon as I answer, call start_heads_up_round with 15 great words!"
                    : "אנחנו רוצים לשחק 'נחש מה על המצח'! שאל אותי במשפט קצר אחד על איזה נושא בא לנו לשחק, וברגע שאענה קרא מיד לכלי start_heads_up_round עם 15 מילים מעולות בעברית!";
        } else {
            bool h2h = (mode == GamePickerMode::kTriviaH2H);
            if (cfg.language == "en-US") {
                p = h2h ? "We want a 2-player head-to-head screen trivia battle! Ask in 1 short sentence what topic we want, and as soon as I reply call start_trivia_game with head_to_head=true and 8 great questions!"
                        : "I want to play interactive screen trivia! Ask in 1 short sentence what topic I want, and as soon as I reply call start_trivia_game with head_to_head=false and 8 great questions!";
            } else {
                p = h2h ? "אנחנו רוצים לשחק טריוויה ראש בראש על המסך! שאל אותנו במשפט קצר אחד על איזה נושא בא לנו להתחרות, וברגע שנענה קרא מיד לכלי start_trivia_game עם head_to_head=true ו-8 שאלות מעולות!"
                        : "בא לי לשחק טריוויה על המסך! שאל אותי במשפט קצר אחד על איזה נושא בא לי לשחק, וברגע שאענה קרא מיד לכלי start_trivia_game עם head_to_head=false ו-8 שאלות מעולות!";
            }
        }
        Application::GetInstance().Schedule([p]() {
            Application::GetInstance().WakeWordInvoke(p);
        });
    }

    static void OnHeadsUpTapSkip(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        if (robi_self_->heads_up_.MarkSkip()) {
            bool done = (robi_self_->heads_up_.phase() == robi::HeadsUpEngine::Phase::kSummary);
            robi_self_->RobiSyncHeadsUpStageWidgetsLocked(true);
#ifdef ESP_PLATFORM
            if (done) robi::RobiImuService::GetInstance().SetForeheadMode(false);
            Application::GetInstance().Schedule([done]() {
                robi::PlayRobiSfx(done ? robi::RobiSfxId::kVictoryFanfare
                                       : robi::RobiSfxId::kSkipSwoosh,
                                  false);
            });
#endif
        }
    }

    static void OnHeadsUpTapOk(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        if (robi_self_->heads_up_.MarkCorrect()) {
            bool done = (robi_self_->heads_up_.phase() == robi::HeadsUpEngine::Phase::kSummary);
            robi_self_->RobiSyncHeadsUpStageWidgetsLocked(true);
#ifdef ESP_PLATFORM
            if (done) robi::RobiImuService::GetInstance().SetForeheadMode(false);
            Application::GetInstance().Schedule([done]() {
                robi::PlayRobiSfx(done ? robi::RobiSfxId::kVictoryFanfare
                                       : robi::RobiSfxId::kCorrectDing,
                                  false);
            });
#endif
        }
    }

    static void OnHeadsUpAgain(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiStartOfflineHeadsUpLocked(robi_self_->last_hu_cat_);
    }

    void RobiHandleTriviaPickIndexLocked(int picked) {
        if (!trivia_.SubmitAnswer(picked)) {
            return;
        }
        bool is_ok = trivia_.last_was_correct();
        RobiSyncTriviaWidgetsLocked();
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([is_ok]() {
            if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                proto->CancelPendingTurnAudio();
            }
            robi::PlayRobiSfx(is_ok ? robi::RobiSfxId::kCorrectDing : robi::RobiSfxId::kWrongBuzz,
                              /*allow_interrupt_speaking=*/true);
        });
#else
        (void)is_ok;
#endif
    }

    static void OnTriviaPickOption(lv_event_t* e) {
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr) return;
        int picked = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        self->RobiHandleTriviaPickIndexLocked(picked);
    }

    static void OnTriviaPlayAgain(lv_event_t*) {
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr) return;
        self->RobiStartTriviaRoundLocked(self->last_trivia_cat_, self->last_trivia_h2h_,
                                         self->last_trivia_custom_topic_);
    }

    static void OnTriviaClose(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiHideTriviaOverlayLocked(/*close_voice_session=*/true,
                                                /*restore_idle_widgets=*/true);
    }

    void RobiInitArcadeWidgets() {
        lv_obj_t* scr = lv_screen_active();
        if (scr == nullptr) return;
        lv_obj_add_event_cb(scr, OnScreenGesture, LV_EVENT_GESTURE, nullptr);

        // 0. Home Screen Quick "משחקים" (Games) Pill Button at top center
        robi_games_home_btn_ = RobiPillBtnPx(scr, "משחקים", 0x10B981, 132, 40,
                                             OnHomeOpenArcadeBtn, nullptr);
        lv_obj_align(robi_games_home_btn_, LV_ALIGN_TOP_MID, 0, 18);

        // 1. Robi Arcade Main Menu (466x466 circular layout)
        robi_arcade_scr_ = lv_obj_create(scr);
        lv_obj_set_size(robi_arcade_scr_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_arcade_scr_);
        lv_obj_set_style_bg_color(robi_arcade_scr_, lv_color_hex(0x050B16), 0);
        lv_obj_set_style_bg_opa(robi_arcade_scr_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(robi_arcade_scr_, 2, 0);
        lv_obj_set_style_border_color(robi_arcade_scr_, lv_color_hex(0x1E3A5F), 0);
        lv_obj_set_style_radius(robi_arcade_scr_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(robi_arcade_scr_, 0, 0);
        lv_obj_remove_flag(robi_arcade_scr_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(robi_arcade_scr_, OnTouchGestureTrack, LV_EVENT_PRESSED, nullptr);
        lv_obj_add_event_cb(robi_arcade_scr_, OnTouchGestureTrack, LV_EVENT_PRESSING, nullptr);
        lv_obj_add_event_cb(robi_arcade_scr_, OnTouchGestureTrack, LV_EVENT_RELEASED, nullptr);
        lv_obj_add_event_cb(robi_arcade_scr_, OnTouchGestureTrack, LV_EVENT_PRESS_LOST, nullptr);

        robi_arcade_title_ = lv_label_create(robi_arcade_scr_);
        lv_obj_set_style_text_color(robi_arcade_title_, lv_color_hex(0x00E5FF), 0);
        lv_obj_align(robi_arcade_title_, LV_ALIGN_TOP_MID, 0, 38);

        robi_arcade_btn_hu_ = RobiPillBtnPx(robi_arcade_scr_, "נחש מה על המצח!", 0x10B981,
                                            328, 62, OnArcadeBtnHeadsUp, nullptr);
        lv_obj_align(robi_arcade_btn_hu_, LV_ALIGN_TOP_MID, 0, 96);

        robi_arcade_btn_tr1_ = RobiPillBtnPx(robi_arcade_scr_, "טריוויה חיה — יחיד", 0x00E5FF,
                                             344, 62, OnArcadeBtnTriviaSolo, nullptr);
        lv_obj_align(robi_arcade_btn_tr1_, LV_ALIGN_TOP_MID, 0, 174);

        robi_arcade_btn_tr2_ = RobiPillBtnPx(robi_arcade_scr_, "טריוויה — ראש בראש", 0xF59E0B,
                                             344, 62, OnArcadeBtnTriviaH2H, nullptr);
        lv_obj_align(robi_arcade_btn_tr2_, LV_ALIGN_TOP_MID, 0, 252);

        robi_arcade_btn_back_ = RobiPillBtnPx(robi_arcade_scr_, "חזרה", 0x475569,
                                              168, 48, OnArcadeBack, nullptr);
        lv_obj_align(robi_arcade_btn_back_, LV_ALIGN_TOP_MID, 0, 344);
        lv_obj_add_flag(robi_arcade_scr_, LV_OBJ_FLAG_HIDDEN);

        // 2. Shared Circular Category + AI Voice Topic Picker (Heads-Up & Solo/H2H Trivia)
        robi_hu_menu_ = lv_obj_create(scr);
        lv_obj_set_size(robi_hu_menu_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_hu_menu_);
        lv_obj_set_style_bg_color(robi_hu_menu_, lv_color_hex(0x050B16), 0);
        lv_obj_set_style_bg_opa(robi_hu_menu_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(robi_hu_menu_, 2, 0);
        lv_obj_set_style_border_color(robi_hu_menu_, lv_color_hex(0x10B981), 0);
        lv_obj_set_style_radius(robi_hu_menu_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(robi_hu_menu_, 0, 0);
        lv_obj_remove_flag(robi_hu_menu_, LV_OBJ_FLAG_SCROLLABLE);

        robi_hu_menu_title_ = lv_label_create(robi_hu_menu_);
        lv_obj_set_style_text_color(robi_hu_menu_title_, lv_color_hex(0x34D399), 0);
        lv_obj_align(robi_hu_menu_title_, LV_ALIGN_TOP_MID, 0, 34);

        const uint32_t cat_cols[6] = {0x10B981, 0xF59E0B, 0x00E5FF, 0xA855F7, 0xFF5C8A, 0x38BDF8};
        for (int i = 0; i < 6; ++i) {
            int row = i / 2;
            int col = i % 2;
            int cat_arg = (i < 5) ? i : -1;
            robi_hu_cat_btns_[i] = RobiPillBtnPx(
                robi_hu_menu_, "", cat_cols[i], 170, 50, OnPickerSelectCategory,
                reinterpret_cast<void*>(static_cast<intptr_t>(cat_arg)));
            int x_off = (col == 0) ? -89 : 89;
            int y_off = 90 + row * 58;
            lv_obj_align(robi_hu_cat_btns_[i], LV_ALIGN_TOP_MID, x_off, y_off);
        }

        robi_hu_ai_btn_ = RobiPillBtnPx(robi_hu_menu_, "נושא חופשי בקול", 0xC084FC,
                                        320, 50, OnPickerAiVoiceTopic, nullptr);
        lv_obj_align(robi_hu_ai_btn_, LV_ALIGN_TOP_MID, 0, 274);

        robi_hu_back_btn_ = RobiPillBtnPx(robi_hu_menu_, "חזרה", 0x475569,
                                          154, 44, OnHeadsUpBackToArcade, nullptr);
        lv_obj_align(robi_hu_back_btn_, LV_ALIGN_TOP_MID, 0, 344);
        lv_obj_add_flag(robi_hu_menu_, LV_OBJ_FLAG_HIDDEN);

        // 3. Heads-Up Active Forehead Stage
        robi_hu_stage_ = lv_obj_create(scr);
        lv_obj_set_size(robi_hu_stage_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_hu_stage_);
        lv_obj_set_style_bg_color(robi_hu_stage_, lv_color_hex(0x061022), 0);
        lv_obj_set_style_bg_opa(robi_hu_stage_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(robi_hu_stage_, 0, 0);
        lv_obj_set_style_pad_all(robi_hu_stage_, 0, 0);
        lv_obj_remove_flag(robi_hu_stage_, LV_OBJ_FLAG_SCROLLABLE);

        robi_hu_ring_ = RobiPlainObj(robi_hu_stage_);
        lv_obj_set_size(robi_hu_ring_, 446, 446);
        lv_obj_center(robi_hu_ring_);
        lv_obj_set_style_bg_opa(robi_hu_ring_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(robi_hu_ring_, 5, 0);
        lv_obj_set_style_border_color(robi_hu_ring_, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_radius(robi_hu_ring_, LV_RADIUS_CIRCLE, 0);

        robi_hu_top_lbl_ = lv_label_create(robi_hu_stage_);
        lv_obj_set_style_text_color(robi_hu_top_lbl_, lv_color_hex(0x38BDF8), 0);
        lv_obj_align(robi_hu_top_lbl_, LV_ALIGN_TOP_MID, 0, 44);

        robi_hu_btn_x_ = RobiPillBtnPx(robi_hu_stage_, "X", 0x64748B, 52, 42, OnArcadeBack, nullptr);
        lv_obj_align(robi_hu_btn_x_, LV_ALIGN_TOP_MID, 0, 92);

        robi_hu_word_card_ = lv_obj_create(robi_hu_stage_);
        lv_obj_set_size(robi_hu_word_card_, 380, 148);
        lv_obj_align(robi_hu_word_card_, LV_ALIGN_CENTER, 0, -4);
        lv_obj_set_style_bg_color(robi_hu_word_card_, lv_color_hex(0x0B1D3A), 0);
        lv_obj_set_style_bg_opa(robi_hu_word_card_, LV_OPA_80, 0);
        lv_obj_set_style_border_width(robi_hu_word_card_, 2, 0);
        lv_obj_set_style_border_color(robi_hu_word_card_, lv_color_hex(0x38BDF8), 0);
        lv_obj_set_style_radius(robi_hu_word_card_, 28, 0);
        lv_obj_remove_flag(robi_hu_word_card_, LV_OBJ_FLAG_SCROLLABLE);

        robi_hu_word_lbl_ = lv_label_create(robi_hu_word_card_);
        lv_label_set_long_mode(robi_hu_word_lbl_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(robi_hu_word_lbl_, 348);
        lv_obj_set_style_text_align(robi_hu_word_lbl_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(robi_hu_word_lbl_, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(robi_hu_word_lbl_);

        robi_hu_sub_lbl_ = lv_label_create(robi_hu_stage_);
        lv_obj_set_style_text_color(robi_hu_sub_lbl_, lv_color_hex(0xCBD5E1), 0);
        lv_obj_align(robi_hu_sub_lbl_, LV_ALIGN_TOP_MID, 0, 306);

        robi_hu_btn_skip_ = RobiPillBtnPx(robi_hu_stage_, "דלג", 0xF59E0B, 134, 48,
                                          OnHeadsUpTapSkip, nullptr);
        lv_obj_align(robi_hu_btn_skip_, LV_ALIGN_TOP_MID, -78, 348);

        robi_hu_btn_ok_ = RobiPillBtnPx(robi_hu_stage_, "נכון!", 0x10B981, 134, 48,
                                        OnHeadsUpTapOk, nullptr);
        lv_obj_align(robi_hu_btn_ok_, LV_ALIGN_TOP_MID, 78, 348);

        robi_hu_btn_again_ = RobiPillBtnPx(robi_hu_stage_, "שוב!", 0x10B981, 134, 48,
                                           OnHeadsUpAgain, nullptr);
        lv_obj_align(robi_hu_btn_again_, LV_ALIGN_TOP_MID, 78, 348);

        robi_hu_btn_exit_ = RobiPillBtnPx(robi_hu_stage_, "סיום", 0x475569, 134, 48,
                                          OnArcadeBack, nullptr);
        lv_obj_align(robi_hu_btn_exit_, LV_ALIGN_TOP_MID, -78, 348);
        lv_obj_add_flag(robi_hu_stage_, LV_OBJ_FLAG_HIDDEN);

        // 4. Deterministic 8-Question Touch Trivia Stage (466x466 chord-fitted layout)
        robi_trivia_box_ = lv_obj_create(scr);
        lv_obj_set_size(robi_trivia_box_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_trivia_box_);
        lv_obj_set_style_bg_color(robi_trivia_box_, lv_color_hex(0x050B16), 0);
        lv_obj_set_style_bg_opa(robi_trivia_box_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(robi_trivia_box_, 3, 0);
        lv_obj_set_style_border_color(robi_trivia_box_, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_radius(robi_trivia_box_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(robi_trivia_box_, 0, 0);
        lv_obj_remove_flag(robi_trivia_box_, LV_OBJ_FLAG_SCROLLABLE);

        robi_trivia_hdr_ = lv_label_create(robi_trivia_box_);
        lv_label_set_long_mode(robi_trivia_hdr_, LV_LABEL_LONG_DOT);
        lv_obj_set_width(robi_trivia_hdr_, 256);
        lv_obj_set_style_text_align(robi_trivia_hdr_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(robi_trivia_hdr_, lv_color_hex(0xFBBF24), 0);
        lv_obj_align(robi_trivia_hdr_, LV_ALIGN_TOP_MID, 0, 26);

        robi_trivia_q_ = lv_label_create(robi_trivia_box_);
        lv_label_set_long_mode(robi_trivia_q_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(robi_trivia_q_, 336);
        lv_obj_set_style_text_align(robi_trivia_q_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(robi_trivia_q_, lv_color_hex(0xF8FAFC), 0);
        lv_obj_align(robi_trivia_q_, LV_ALIGN_TOP_MID, 0, 66);

        const int opt_widths[4] = {348, 356, 348, 316};
        for (int i = 0; i < 4; ++i) {
            robi_trivia_opts_[i] = RobiPillBtnPx(
                robi_trivia_box_, "", 0x00E5FF, opt_widths[i], 46, OnTriviaPickOption,
                reinterpret_cast<void*>(static_cast<intptr_t>(i)));
            lv_obj_align(robi_trivia_opts_[i], LV_ALIGN_TOP_MID, 0, 172 + i * 52);
        }

        robi_trivia_again_ = RobiPillBtnPx(robi_trivia_box_, "שוב!", 0x10B981, 144, 50,
                                           OnTriviaPlayAgain, nullptr);
        lv_obj_align(robi_trivia_again_, LV_ALIGN_TOP_MID, 80, 298);
        lv_obj_add_flag(robi_trivia_again_, LV_OBJ_FLAG_HIDDEN);

        robi_trivia_done_ = RobiPillBtnPx(robi_trivia_box_, "סיום", 0x475569, 144, 50,
                                          OnTriviaClose, nullptr);
        lv_obj_align(robi_trivia_done_, LV_ALIGN_TOP_MID, -80, 298);
        lv_obj_add_flag(robi_trivia_done_, LV_OBJ_FLAG_HIDDEN);

        robi_trivia_x_ = RobiPillBtnPx(robi_trivia_box_, "X", 0x475569, 68, 38,
                                       OnTriviaClose, nullptr);
        lv_obj_align(robi_trivia_x_, LV_ALIGN_TOP_MID, 0, 386);
        lv_obj_add_flag(robi_trivia_box_, LV_OBJ_FLAG_HIDDEN);

        RobiRefreshGameLocaleLocked();
    }

    void RobiStepArcadeAndImuLocked(int64_t now_us, float dt) {
        auto dev_state = Application::GetInstance().GetDeviceState();
        int dt_ms = std::max(1, static_cast<int>(dt * 1000.0f + 0.5f));

        // 1a. Poll voice-launched Trivia (`start_trivia_game` tool called during Gemini Live chat)
        robi::VoiceTriviaStartCmd voice_triv;
        if (robi::RobiGameHub::GetInstance().PopVoiceTriviaStart(voice_triv)) {
            RobiCloseAllGameScreensLocked(false);
            last_trivia_cat_ = -1;
            last_trivia_h2h_ = voice_triv.head_to_head;
            last_trivia_custom_topic_ = voice_triv.topic;
            trivia_summary_speak_at_us_ = 0;

            auto cfg = RobiConfig::GetInstance().Get();
            const bool en = (cfg.language == "en-US");
            std::string p1, p2;
            ResolveTriviaPlayerNames(cfg, p1, p2);
            trivia_.SeedRng(static_cast<uint32_t>(now_us));

            if (voice_triv.questions.size() >= 4) {
                active_trivia_req_id_ = 0;
                // Keep Question #0 first so it matches the question Gemini's toolResponse reads aloud!
                trivia_.StartCustomDeck(voice_triv.topic, voice_triv.questions, -1,
                                        voice_triv.head_to_head, p1, p2, en, 8,
                                        /*preserve_first_question_order=*/true);
                RobiSyncTriviaWidgetsLocked();
            } else {
                RobiStartTriviaRoundLocked(-1, voice_triv.head_to_head, voice_triv.topic);
            }
        }

        // 1b. Poll async HTTP AI Trivia deck completion (`RequestAsyncAiTriviaDeck`)
        if (trivia_.phase() == robi::TriviaEngine::Phase::kLoading && active_trivia_req_id_ != 0) {
            std::vector<robi::TriviaQuestionItem> ai_items;
            if (robi::RobiGameHub::GetInstance().PopAsyncTriviaDeck(active_trivia_req_id_, ai_items)) {
                active_trivia_req_id_ = 0;
                auto cfg = RobiConfig::GetInstance().Get();
                const bool en = (cfg.language == "en-US");
                std::string p1, p2;
                ResolveTriviaPlayerNames(cfg, p1, p2);
                std::string title = trivia_.topic_title();
                trivia_.StartCustomDeck(title, ai_items, last_trivia_cat_, last_trivia_h2h_,
                                        p1, p2, en, 8, false);
                RobiSyncTriviaWidgetsLocked();
                RobiSpeakCurrentTriviaQuestionLocked(/*include_prev_reaction=*/false);
            }
        }

        // 1c. Step TriviaEngine (answer reveal -> next question 2..8 -> final summary)
        if (trivia_.IsActive()) {
            auto tev = trivia_.Step(dt_ms);
            if (tev.loading_timed_out) {
                active_trivia_req_id_ = 0;
                auto cfg = RobiConfig::GetInstance().Get();
                const bool en = (cfg.language == "en-US");
                std::string p1, p2;
                ResolveTriviaPlayerNames(cfg, p1, p2);
                trivia_.StartOfflineDeck(last_trivia_cat_, last_trivia_h2h_, p1, p2, en, 8);
                RobiSyncTriviaWidgetsLocked();
                RobiSpeakCurrentTriviaQuestionLocked(false);
            } else if (tev.advanced_to_next_question) {
                RobiSyncTriviaWidgetsLocked();
                RobiSpeakCurrentTriviaQuestionLocked(/*include_prev_reaction=*/true);
            } else if (tev.finished_to_summary) {
                RobiSyncTriviaWidgetsLocked();
                trivia_summary_speak_at_us_ = now_us + 520000LL;
#ifdef ESP_PLATFORM
                Application::GetInstance().Schedule([]() {
                    robi::PlayRobiSfx(robi::RobiSfxId::kVictoryFanfare, false);
                });
#endif
            }

            if (trivia_summary_speak_at_us_ > 0 && now_us >= trivia_summary_speak_at_us_) {
                trivia_summary_speak_at_us_ = 0;
#ifdef ESP_PLATFORM
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    if (proto->IsAudioChannelOpened()) {
                        const bool en = trivia_.english();
                        std::string sum_prompt;
                        if (trivia_.head_to_head()) {
                            sum_prompt =
                                en ? ("🎉 [Trivia finished! Final score — " + trivia_.player_name(0) + ": " +
                                      std::to_string(trivia_.score(0)) + ", " + trivia_.player_name(1) + ": " +
                                      std::to_string(trivia_.score(1)) +
                                      ". Announce the result enthusiastically in 1 short celebratory sentence!]")
                                   : ("🎉 [סיום חידון הטריוויה! תוצאה סופית — " + trivia_.player_name(0) + ": " +
                                      std::to_string(trivia_.score(0)) + ", " + trivia_.player_name(1) + ": " +
                                      std::to_string(trivia_.score(1)) +
                                      ". הכרז בהתלהבות ובשמחה במשפט קצר אחד על סיום המשחק והתוצאה!]");
                        } else {
                            sum_prompt =
                                en ? ("🎉 [Trivia finished! Player got " + std::to_string(trivia_.score(0)) +
                                      " out of " + std::to_string(trivia_.total_questions()) +
                                      "! Celebrate warmly in 1 short sentence!]")
                                   : ("🎉 [סיום חידון הטריוויה! השחקן ענה נכון על " +
                                      std::to_string(trivia_.score(0)) + " מתוך " +
                                      std::to_string(trivia_.total_questions()) +
                                      " שאלות! פרגן בהתלהבות במשפט קצר אחד!]");
                        }
                        proto->InjectUserPromptTurn(sum_prompt);
                    }
                }
#endif
            }
        }

        // 1d. Poll custom AI Heads-Up deck once kickoff turn returns to Idle
        if (dev_state == kDeviceStateIdle) {
            std::string hu_topic;
            std::vector<std::string> hu_words;
            if (robi::RobiGameHub::GetInstance().PopPendingCustomHeadsUp(hu_topic, hu_words)) {
                auto cfg = RobiConfig::GetInstance().Get();
                heads_up_.SeedRng(static_cast<uint32_t>(now_us));
                heads_up_.StartCustomRound(hu_topic, hu_words, cfg.language == "en-US", 60);
                RobiBeginHeadsUpStageLocked();
            }
        }

#ifdef ESP_PLATFORM
        // 2. Poll QMI8658 IMU gesture events
        auto imu_ev = robi::RobiImuService::GetInstance().ConsumePendingEvents();

        if (imu_ev.enter_face_down_sleep && !imu_sleeping_) {
            imu_sleeping_ = true;
            RobiSetHidden(robi_games_home_btn_, true);
            RobiCloseAllGameScreensLocked(false);
            RobiHideTriviaOverlayLocked(true, false);
            RobiApplyPreset("sleepy", false);
            int cur_bri = 85;
            if (auto* bl = Board::GetInstance().GetBacklight()) {
                cur_bri = std::max(20, static_cast<int>(bl->brightness()));
            }
            pre_sleep_bri_ = cur_bri;
            Application::GetInstance().Schedule([]() {
                if (auto* bl = Board::GetInstance().GetBacklight()) {
                    bl->SetBrightness(0, false);
                }
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateSpeaking) {
                    app.AbortSpeaking(kAbortReasonNone);
                }
                if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                    if (proto->IsAudioChannelOpened()) {
                        proto->CloseAudioChannel(false);
                    }
                }
                app.GetAudioService().EnableWakeWordDetection(false);
            });
        } else if (imu_ev.exit_face_down_sleep && imu_sleeping_) {
            imu_sleeping_ = false;
            int restore_b = pre_sleep_bri_;
            RobiApplyPreset("happy", true);
            if (dev_state == kDeviceStateIdle && !RobiIsArcadeOrGameOpen()) {
                if (robi_profiles_ != nullptr) {
                    RobiSetHidden(robi_profiles_, false);
                }
                if (!RobiIsAnyMenuOpen()) {
                    RobiSetHidden(robi_games_home_btn_, false);
                }
            }
            Application::GetInstance().Schedule([restore_b]() {
                if (auto* bl = Board::GetInstance().GetBacklight()) {
                    bl->SetBrightness(static_cast<uint8_t>(restore_b), false);
                }
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    app.GetAudioService().EnableWakeWordDetection(true);
                }
            });
        }

        if (imu_ev.shake_dizzy && !imu_sleeping_ && !heads_up_.IsActive() && !trivia_.IsActive()) {
            dizzy_until_us_ = now_us + 2400000LL;
            RobiApplyPreset("dizzy", true);
            if (dev_state != kDeviceStateSpeaking) {
                Application::GetInstance().Schedule([]() {
                    robi::PlayRobiSfx(robi::RobiSfxId::kDizzyWarble, false);
                });
            }
        }

        if (dizzy_until_us_ > 0 && now_us >= dizzy_until_us_) {
            dizzy_until_us_ = 0;
            if (robi_face_mode_ == robi::ROBI_MODE_IDLE) {
                RobiApplyPreset("happy", true);
            }
        }

        if (heads_up_.IsActive()) {
            if (imu_ev.forehead_action == robi::ForeheadAction::kCorrectDown) {
                if (heads_up_.MarkCorrect()) {
                    bool done = (heads_up_.phase() == robi::HeadsUpEngine::Phase::kSummary);
                    if (done) robi::RobiImuService::GetInstance().SetForeheadMode(false);
                    Application::GetInstance().Schedule([done]() {
                        robi::PlayRobiSfx(done ? robi::RobiSfxId::kVictoryFanfare
                                               : robi::RobiSfxId::kCorrectDing,
                                          false);
                    });
                }
            } else if (imu_ev.forehead_action == robi::ForeheadAction::kSkipUp) {
                if (heads_up_.MarkSkip()) {
                    bool done = (heads_up_.phase() == robi::HeadsUpEngine::Phase::kSummary);
                    if (done) robi::RobiImuService::GetInstance().SetForeheadMode(false);
                    Application::GetInstance().Schedule([done]() {
                        robi::PlayRobiSfx(done ? robi::RobiSfxId::kVictoryFanfare
                                               : robi::RobiSfxId::kSkipSwoosh,
                                          false);
                    });
                }
            }
        }
#endif

        // 3. Step active Heads-Up Forehead Game timer
        if (heads_up_.IsActive()) {
            auto hev = heads_up_.Step(dt_ms);
            RobiSyncHeadsUpStageWidgetsLocked(false);
#ifdef ESP_PLATFORM
            if (hev.round_started) {
                Application::GetInstance().Schedule([]() {
                    robi::PlayRobiSfx(robi::RobiSfxId::kGoStart, false);
                });
            } else if (hev.countdown_tick) {
                Application::GetInstance().Schedule([]() {
                    robi::PlayRobiSfx(robi::RobiSfxId::kCountdownTick, false);
                });
            } else if (hev.round_finished) {
                robi::RobiImuService::GetInstance().SetForeheadMode(false);
                Application::GetInstance().Schedule([]() {
                    robi::PlayRobiSfx(robi::RobiSfxId::kVictoryFanfare, false);
                });
            }
#else
            (void)hev;
#endif
        }
    }
    // ===================== end HeyRobi v3.2 Arcade & IMU Layer =====================
