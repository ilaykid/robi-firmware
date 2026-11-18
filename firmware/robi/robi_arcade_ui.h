    // ===================== HeyRobi v3.2 Interactive Arcade, Trivia & IMU Layer =====================
    // Included inside CustomLcdDisplay (from robi_display_ui.h).
    // Provides:
    // 1. Horizontal Swipe -> Robi Arcade ("נחש מה על המצח" Offline + AI, Solo & Head-to-Head Trivia)
    // 2. Full 466x466 circular Forehead Charades stage driven by QMI8658 IMU + touch fallback
    // 3. Real-time Gemini Live Touch Trivia overlay (`show_trivia_card`) with instant SFX feedback
    // 4. Face-down Mute/Sleep, In-Hand Shake Dizzy counter-spinning eyes, & touch Z-axis calibration

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
    lv_obj_t* robi_trivia_x_ = nullptr;

    robi::HeadsUpEngine heads_up_;
    int last_hu_cat_ = -1;
    std::string last_hu_word_rendered_;
    std::string last_hu_top_rendered_;
    robi::HeadsUpEngine::Phase last_hu_phase_ = robi::HeadsUpEngine::Phase::kInactive;

    robi::TriviaCardPayload active_trivia_;
    bool trivia_visible_ = false;
    bool trivia_answered_ = false;
    int64_t trivia_hide_at_us_ = 0;
    int64_t trivia_send_reply_at_us_ = 0;
    std::string pending_trivia_reply_;

    int32_t touch_press_x_ = 0;
    int32_t touch_press_y_ = 0;
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

    void RobiRefreshGameLocaleLocked() {
        if (robi_arcade_scr_ == nullptr) return;
        auto cfg = RobiConfig::GetInstance().Get();
        const bool en = (cfg.language == "en-US");

        lv_label_set_text(robi_arcade_title_, en ? "Robi Games" : "משחקי רובי");
        SetButtonLabelText(robi_arcade_btn_hu_, en ? "Heads Up! (Tilt Game)" : "נחש מה על המצח!");
        SetButtonLabelText(robi_arcade_btn_tr1_, en ? "Live Screen Trivia" : "טריוויה חיה — יחיד");
        if (cfg.kids.size() >= 2) {
            std::string h2h = en ? ("Head-to-Head: " + cfg.kids[0].name + " vs " + cfg.kids[1].name)
                                 : ("ראש בראש: " + cfg.kids[0].name + " מול " + cfg.kids[1].name);
            SetButtonLabelText(robi_arcade_btn_tr2_, h2h.c_str());
        } else {
            SetButtonLabelText(robi_arcade_btn_tr2_, en ? "2-Player Trivia Battle" : "טריוויה — ראש בראש");
        }
        SetButtonLabelText(robi_arcade_btn_back_, en ? "Back" : "חזרה");

        lv_label_set_text(robi_hu_menu_title_, en ? "Pick a Category" : "בחרו קטגוריה");
        for (int i = 0; i < 5; ++i) {
            SetButtonLabelText(robi_hu_cat_btns_[i],
                               en ? robi::kHeadsUpCategories[i].title_en
                                  : robi::kHeadsUpCategories[i].title_he);
        }
        SetButtonLabelText(robi_hu_cat_btns_[5], en ? "Mega Mix" : "ערבוב משפחתי");
        SetButtonLabelText(robi_hu_ai_btn_, en ? "Custom Voice Topic (AI)" : "נושא חופשי בקול (AI)");
        SetButtonLabelText(robi_hu_back_btn_, en ? "Back" : "חזרה");

        SetButtonLabelText(robi_hu_btn_skip_, en ? "Skip" : "דלג");
        SetButtonLabelText(robi_hu_btn_ok_, en ? "Correct!" : "נכון!");
        SetButtonLabelText(robi_hu_btn_again_, en ? "Play Again" : "שוב!");
        SetButtonLabelText(robi_hu_btn_exit_, en ? "Done" : "סיום");
    }

    void RobiCloseAllGameScreensLocked(bool restore_idle_widgets = true) {
        bool was_hu = heads_up_.IsActive();
        heads_up_.Stop();
#ifdef ESP_PLATFORM
        robi::RobiImuService::GetInstance().SetForeheadMode(false);
#endif
        RobiSetHidden(robi_arcade_scr_, true);
        RobiSetHidden(robi_hu_menu_, true);
        RobiSetHidden(robi_hu_stage_, true);

        bool is_idle = (Application::GetInstance().GetDeviceState() == kDeviceStateIdle);
        if (was_hu && is_idle) {
            Application::GetInstance().Schedule([]() {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    app.GetAudioService().EnableWakeWordDetection(true);
                }
            });
        }
        if (restore_idle_widgets && is_idle) {
            if (robi_profiles_ != nullptr) {
                RobiSetHidden(robi_profiles_, false);
            }
            RobiUpdateSetupHintVisibility(true);
        }
    }

    void RobiHideTriviaOverlayLocked() {
        trivia_visible_ = false;
        trivia_answered_ = false;
        trivia_hide_at_us_ = 0;
        RobiSetHidden(robi_trivia_box_, true);
        bool speaking = (Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking);
        RobiSetHidden(robi_tap_, !speaking);
    }

    void RobiOpenArcadeMenuLocked() {
        RobiStopMenuTimer();
        RobiHideAllMenus();
        RobiHideTriviaOverlayLocked();
        RobiSetHidden(robi_profiles_, true);
        RobiSetHidden(robi_setup_hint_, true);
        RobiSetHidden(robi_hu_menu_, true);
        RobiSetHidden(robi_hu_stage_, true);
        RobiRefreshGameLocaleLocked();
        RobiSetHidden(robi_arcade_scr_, false);
        lv_obj_move_foreground(robi_arcade_scr_);
    }

    void RobiOpenHeadsUpMenuLocked() {
        RobiSetHidden(robi_arcade_scr_, true);
        RobiSetHidden(robi_hu_stage_, true);
        RobiRefreshGameLocaleLocked();
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
        RobiHideTriviaOverlayLocked();
        RobiSetHidden(robi_profiles_, true);
        RobiSetHidden(robi_setup_hint_, true);
        RobiSetHidden(robi_arcade_scr_, true);
        RobiSetHidden(robi_hu_menu_, true);
        last_hu_word_rendered_.clear();
        last_hu_top_rendered_.clear();
        last_hu_phase_ = robi::HeadsUpEngine::Phase::kInactive;
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

    void RobiShowTriviaCardLocked(robi::TriviaCardPayload&& card) {
        RobiCloseAllGameScreensLocked(false);
        active_trivia_ = std::move(card);
        trivia_visible_ = true;
        trivia_answered_ = false;
        trivia_hide_at_us_ = 0;

        std::string hdr;
        if (!active_trivia_.player_turn.empty() && !active_trivia_.score_summary.empty()) {
            hdr = active_trivia_.player_turn + "  |  " + active_trivia_.score_summary;
        } else if (!active_trivia_.score_summary.empty()) {
            hdr = active_trivia_.score_summary;
        } else if (!active_trivia_.player_turn.empty()) {
            hdr = active_trivia_.player_turn;
        } else {
            hdr = (RobiConfig::GetInstance().Get().language == "en-US") ? "Trivia" : "טריוויה חיה";
        }
        lv_label_set_text(robi_trivia_hdr_, hdr.c_str());
        lv_label_set_text(robi_trivia_q_, active_trivia_.question.c_str());

        for (int i = 0; i < 4; ++i) {
            if (i < static_cast<int>(active_trivia_.options.size())) {
                SetButtonLabelText(robi_trivia_opts_[i], active_trivia_.options[i].c_str());
                lv_obj_set_style_bg_color(robi_trivia_opts_[i], lv_color_hex(0x0B172E), 0);
                lv_obj_set_style_border_color(robi_trivia_opts_[i], lv_color_hex(0x00E5FF), 0);
                RobiSetHidden(robi_trivia_opts_[i], false);
            } else {
                RobiSetHidden(robi_trivia_opts_[i], true);
            }
        }

        // Prevent full-screen `robi_tap_` from stealing touches on trivia answer buttons!
        RobiSetHidden(robi_tap_, true);
        RobiSetHidden(robi_trivia_box_, false);
        lv_obj_move_foreground(robi_trivia_box_);
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
#ifdef ESP_PLATFORM
            robi::RobiImuService::GetInstance().NotifyScreenTouched();
#endif
            return;
        }

        if (code == LV_EVENT_RELEASED) {
            int32_t dx = pt.x - self->touch_press_x_;
            int32_t dy = pt.y - self->touch_press_y_;
            if (std::abs(dx) >= 68 && std::abs(dx) * 10 > std::abs(dy) * 12) {
                self->swipe_guard_until_us_ = esp_timer_get_time() + 300000LL;
                RobiWakePowerSave();
                if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
                    return;
                }
                if (self->RobiIsArcadeOrGameOpen()) {
                    if (!self->heads_up_.IsActive()) {
                        self->RobiCloseAllGameScreensLocked(true);
#ifdef ESP_PLATFORM
                        Application::GetInstance().Schedule([]() {
                            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
                        });
#endif
                    }
                } else {
                    self->RobiOpenArcadeMenuLocked();
#ifdef ESP_PLATFORM
                    Application::GetInstance().Schedule([]() {
                        robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
                    });
#endif
                }
            }
        }
    }

    static void OnArcadeBtnHeadsUp(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiOpenHeadsUpMenuLocked();
#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([]() {
            robi::PlayRobiSfx(robi::RobiSfxId::kUiPop, false);
        });
#endif
    }

    static void OnArcadeBtnTriviaSolo(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiCloseAllGameScreensLocked(false);
        auto cfg = RobiConfig::GetInstance().Get();
        if (cfg.kids.size() == 1) {
            RobiConfig::GetInstance().ActivateKidScope(0);
        } else if (!cfg.kids.empty()) {
            RobiConfig::GetInstance().SetActiveScope("kids");
        } else {
            RobiConfig::GetInstance().ActivateDefaultWakeScope();
        }
        std::string p =
            (cfg.language == "en-US")
                ? "Let's play interactive screen trivia! Ask me the first fun question right now and call show_trivia_card with 4 options!"
                : "בוא נשחק טריוויה חיה על המסך לשחקן יחיד! הצג עכשיו את השאלה הראשונה עם הכלי show_trivia_card ושאל אותה בקול קצר ונלהב!";
        Application::GetInstance().Schedule([p]() {
            Application::GetInstance().WakeWordInvoke(p);
        });
    }

    static void OnArcadeBtnTriviaH2H(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiCloseAllGameScreensLocked(false);
        auto cfg = RobiConfig::GetInstance().Get();
        if (!cfg.kids.empty()) {
            RobiConfig::GetInstance().SetActiveScope("kids");
        } else {
            RobiConfig::GetInstance().ActivateDefaultWakeScope();
        }
        std::string p1 = (cfg.kids.size() >= 1) ? cfg.kids[0].name
                                                : ((cfg.language == "en-US") ? "Player 1" : "שחקן 1");
        std::string p2 = (cfg.kids.size() >= 2) ? cfg.kids[1].name
                                                : ((cfg.language == "en-US") ? "Player 2" : "שחקן 2");
        std::string p =
            (cfg.language == "en-US")
                ? ("Let's play a 2-player head-to-head screen trivia battle between " + p1 + " and " + p2 +
                   "! Alternate turns, track scores, and call show_trivia_card right now with Question #1 for " + p1 + "!")
                : ("בואו נשחק טריוויה חיה ראש בראש על המסך בין " + p1 + " לבין " + p2 +
                   "! החלף תורות ביניהם בכל שאלה, נהל ניקוד חי ב-score_summary, והצג עכשיו את השאלה הראשונה עם show_trivia_card לתור של " +
                   p1 + "!");
        Application::GetInstance().Schedule([p]() {
            Application::GetInstance().WakeWordInvoke(p);
        });
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

    static void OnHeadsUpPickCat(lv_event_t* e) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        int cat_idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        robi_self_->RobiStartOfflineHeadsUpLocked(cat_idx);
    }

    static void OnHeadsUpAiTopic(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiCloseAllGameScreensLocked(false);
        auto cfg = RobiConfig::GetInstance().Get();
        if (!cfg.kids.empty() && cfg.persona_mode != "assistant") {
            RobiConfig::GetInstance().SetActiveScope("kids");
        } else {
            RobiConfig::GetInstance().ActivateDefaultWakeScope();
        }
        std::string p =
            (cfg.language == "en-US")
                ? "We want to play 'Heads Up' on our forehead! Ask me in one short sentence what topic we want, and as soon as I answer, call start_heads_up_round with 15 great words!"
                : "אנחנו רוצים לשחק 'נחש מה על המצח'! שאל אותי במשפט קצר אחד על איזה נושא בא לנו לשחק, וברגע שאענה קרא מיד לכלי start_heads_up_round עם 15 מילים מעולות בעברית!";
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

    static void OnTriviaPickOption(lv_event_t* e) {
        RobiWakePowerSave();
        auto* self = robi_self_;
        if (self == nullptr || !self->trivia_visible_ || self->trivia_answered_) return;
        int picked = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        if (picked < 0 || picked >= static_cast<int>(self->active_trivia_.options.size())) return;

        self->trivia_answered_ = true;
        int corr = self->active_trivia_.correct_index;
        bool is_ok = (picked == corr);

        if (corr >= 0 && corr < 4 && self->robi_trivia_opts_[corr] != nullptr) {
            lv_obj_set_style_bg_color(self->robi_trivia_opts_[corr], lv_color_hex(0x065F46), 0);
            lv_obj_set_style_border_color(self->robi_trivia_opts_[corr], lv_color_hex(0x10B981), 0);
        }
        if (!is_ok && self->robi_trivia_opts_[picked] != nullptr) {
            lv_obj_set_style_bg_color(self->robi_trivia_opts_[picked], lv_color_hex(0x7F1D1D), 0);
            lv_obj_set_style_border_color(self->robi_trivia_opts_[picked], lv_color_hex(0xEF4444), 0);
        }

        int64_t now_us = esp_timer_get_time();
        self->trivia_hide_at_us_ = now_us + 1350000LL;
        self->trivia_send_reply_at_us_ = now_us + 420000LL;

        auto cfg = RobiConfig::GetInstance().Get();
        const std::string& chosen_txt = self->active_trivia_.options[picked];
        const std::string& right_txt = self->active_trivia_.options[corr];
        if (cfg.language == "en-US") {
            self->pending_trivia_reply_ =
                is_ok ? ("[Player tapped '" + chosen_txt + "' — CORRECT! Cheer briefly in 1 sentence and immediately present the next question with show_trivia_card!]")
                      : ("[Player tapped '" + chosen_txt + "' — wrong (correct was '" + right_txt +
                         "'). React warmly in 1 short sentence and immediately present the next question with show_trivia_card!]");
        } else {
            self->pending_trivia_reply_ =
                is_ok ? ("[השחקן לחץ על '" + chosen_txt + "' — תשובה נכונה! פרגן במשפט קצר ונלהב והצג מיד את השאלה הבאה עם show_trivia_card!]")
                      : ("[השחקן לחץ על '" + chosen_txt + "' — טעות (התשובה הנכונה הייתה '" + right_txt +
                         "'). עודד בחיוך במשפט קצר והצג מיד את השאלה הבאה עם show_trivia_card!]");
        }

#ifdef ESP_PLATFORM
        Application::GetInstance().Schedule([is_ok]() {
            robi::PlayRobiSfx(is_ok ? robi::RobiSfxId::kCorrectDing : robi::RobiSfxId::kWrongBuzz,
                              /*allow_interrupt_speaking=*/true);
        });
#endif
    }

    static void OnTriviaClose(lv_event_t*) {
        RobiWakePowerSave();
        if (robi_self_ == nullptr) return;
        robi_self_->RobiHideTriviaOverlayLocked();
    }

    void RobiInitArcadeWidgets() {
        lv_obj_t* scr = lv_screen_active();
        if (scr == nullptr) return;

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
        lv_obj_add_event_cb(robi_arcade_scr_, OnTouchGestureTrack, LV_EVENT_RELEASED, nullptr);

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

        // 2. Heads-Up Category Picker (2x3 offline categories + AI Custom Topic)
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
        lv_obj_align(robi_hu_menu_title_, LV_ALIGN_TOP_MID, 0, 30);

        const uint32_t cat_cols[6] = {0x10B981, 0xF59E0B, 0x00E5FF, 0xA855F7, 0xFF5C8A, 0x38BDF8};
        for (int i = 0; i < 6; ++i) {
            int row = i / 2;
            int col = i % 2;
            int cat_arg = (i < 5) ? i : -1;
            robi_hu_cat_btns_[i] = RobiPillBtnPx(
                robi_hu_menu_, "", cat_cols[i], 166, 50, OnHeadsUpPickCat,
                reinterpret_cast<void*>(static_cast<intptr_t>(cat_arg)));
            int x_off = (col == 0) ? -88 : 88;
            int y_off = 82 + row * 60;
            lv_obj_align(robi_hu_cat_btns_[i], LV_ALIGN_TOP_MID, x_off, y_off);
        }

        robi_hu_ai_btn_ = RobiPillBtnPx(robi_hu_menu_, "נושא חופשי בקול (AI)", 0xC084FC,
                                        320, 50, OnHeadsUpAiTopic, nullptr);
        lv_obj_align(robi_hu_ai_btn_, LV_ALIGN_TOP_MID, 0, 272);

        robi_hu_back_btn_ = RobiPillBtnPx(robi_hu_menu_, "חזרה", 0x475569,
                                          154, 44, OnHeadsUpBackToArcade, nullptr);
        lv_obj_align(robi_hu_back_btn_, LV_ALIGN_TOP_MID, 0, 342);
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

        // 4. Live Touch Trivia Overlay (shown during Gemini Live `show_trivia_card`)
        robi_trivia_box_ = lv_obj_create(scr);
        lv_obj_set_size(robi_trivia_box_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(robi_trivia_box_);
        lv_obj_set_style_bg_color(robi_trivia_box_, lv_color_hex(0x040914), 0);
        lv_obj_set_style_bg_opa(robi_trivia_box_, LV_OPA_90, 0);
        lv_obj_set_style_border_width(robi_trivia_box_, 2, 0);
        lv_obj_set_style_border_color(robi_trivia_box_, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_radius(robi_trivia_box_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(robi_trivia_box_, 0, 0);
        lv_obj_remove_flag(robi_trivia_box_, LV_OBJ_FLAG_SCROLLABLE);

        robi_trivia_hdr_ = lv_label_create(robi_trivia_box_);
        lv_label_set_long_mode(robi_trivia_hdr_, LV_LABEL_LONG_DOT);
        lv_obj_set_width(robi_trivia_hdr_, 270);
        lv_obj_set_style_text_align(robi_trivia_hdr_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(robi_trivia_hdr_, lv_color_hex(0xFBBF24), 0);
        lv_obj_align(robi_trivia_hdr_, LV_ALIGN_TOP_MID, 0, 32);

        robi_trivia_q_ = lv_label_create(robi_trivia_box_);
        lv_label_set_long_mode(robi_trivia_q_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(robi_trivia_q_, 360);
        lv_obj_set_style_text_align(robi_trivia_q_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(robi_trivia_q_, lv_color_hex(0xF8FAFC), 0);
        lv_obj_align(robi_trivia_q_, LV_ALIGN_TOP_MID, 0, 70);

        for (int i = 0; i < 4; ++i) {
            robi_trivia_opts_[i] = RobiPillBtnPx(
                robi_trivia_box_, "", 0x00E5FF, 344, 48, OnTriviaPickOption,
                reinterpret_cast<void*>(static_cast<intptr_t>(i)));
            lv_obj_align(robi_trivia_opts_[i], LV_ALIGN_TOP_MID, 0, 152 + i * 54);
        }

        robi_trivia_x_ = RobiPillBtnPx(robi_trivia_box_, "X", 0x475569, 64, 38,
                                       OnTriviaClose, nullptr);
        lv_obj_align(robi_trivia_x_, LV_ALIGN_TOP_MID, 0, 380);
        lv_obj_add_flag(robi_trivia_box_, LV_OBJ_FLAG_HIDDEN);

        RobiRefreshGameLocaleLocked();
    }

    void RobiStepArcadeAndImuLocked(int64_t now_us, float dt) {
        auto dev_state = Application::GetInstance().GetDeviceState();
        int dt_ms = std::max(1, static_cast<int>(dt * 1000.0f + 0.5f));

        // 1. Poll thread-safe GameHub commands emitted by GeminiLiveProtocol
        robi::TriviaCardPayload new_card;
        if (robi::RobiGameHub::GetInstance().PopPendingTriviaCard(new_card)) {
            RobiShowTriviaCardLocked(std::move(new_card));
        }

        if (dev_state == kDeviceStateIdle) {
            if (trivia_visible_) {
                RobiHideTriviaOverlayLocked();
            }
            std::string hu_topic;
            std::vector<std::string> hu_words;
            if (robi::RobiGameHub::GetInstance().PopPendingCustomHeadsUp(hu_topic, hu_words)) {
                auto cfg = RobiConfig::GetInstance().Get();
                heads_up_.SeedRng(static_cast<uint32_t>(now_us));
                heads_up_.StartCustomRound(hu_topic, hu_words, cfg.language == "en-US", 60);
                RobiBeginHeadsUpStageLocked();
            }
        }

        if (trivia_visible_) {
            if (trivia_send_reply_at_us_ > 0 && now_us >= trivia_send_reply_at_us_) {
                trivia_send_reply_at_us_ = 0;
                std::string msg = std::move(pending_trivia_reply_);
                if (!msg.empty()) {
                    if (auto* proto = GeminiLiveProtocol::GetActiveInstance()) {
                        proto->InjectUserPromptTurn(msg);
                    }
                }
            }
            if (trivia_hide_at_us_ > 0 && now_us >= trivia_hide_at_us_) {
                RobiHideTriviaOverlayLocked();
            }
        }

#ifdef ESP_PLATFORM
        // 2. Poll QMI8658 IMU gesture events
        auto imu_ev = robi::RobiImuService::GetInstance().ConsumePendingEvents();

        if (imu_ev.enter_face_down_sleep && !imu_sleeping_) {
            imu_sleeping_ = true;
            RobiCloseAllGameScreensLocked(false);
            RobiHideTriviaOverlayLocked();
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
            if (dev_state == kDeviceStateIdle && robi_profiles_ != nullptr && !RobiIsArcadeOrGameOpen()) {
                RobiSetHidden(robi_profiles_, false);
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

        if (imu_ev.shake_dizzy && !imu_sleeping_ && !heads_up_.IsActive()) {
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
