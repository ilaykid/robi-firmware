
    // ===================== Robi UI (inserted by scripts/patch_xiaozhi.py) =====================
    // Idle screen: Robi's energy / joy bars + two child buttons. A child button opens that
    // child's activity menu; an activity starts a conversation with a fixed opening text.
    // While listening, a tap on the screen means "I finished"; while speaking, it stops Robi.

    virtual void SetStatus(const char* status) override {
        SpiLcdDisplay::SetStatus(status);
        DisplayLockGuard lock(this);
        if (robi_profiles_ == nullptr) return;
        bool idle = status != nullptr && strcmp(status, Lang::Strings::STANDBY) == 0;
        bool listening = status != nullptr && strcmp(status, Lang::Strings::LISTENING) == 0;
        bool speaking = status != nullptr && strcmp(status, Lang::Strings::SPEAKING) == 0;
        if (idle) {
            bool menu_open = (robi_menu_lavi_ && !lv_obj_has_flag(robi_menu_lavi_, LV_OBJ_FLAG_HIDDEN)) ||
                             (robi_menu_ori_ && !lv_obj_has_flag(robi_menu_ori_, LV_OBJ_FLAG_HIDDEN));
            if (!menu_open) lv_obj_remove_flag(robi_profiles_, LV_OBJ_FLAG_HIDDEN);
            if (robi_pet_ok_) lv_obj_remove_flag(robi_pet_, LV_OBJ_FLAG_HIDDEN);
            RobiRequestPet();
        } else {
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

private:
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
        lv_obj_set_style_border_width(b, 0, 0);
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
        RobiButton(menu, items[8], 0x555555, 40, 9, OnActivity, (void*)items[9]);
        lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
        return menu;
    }

    lv_obj_t* RobiPetRow(lv_obj_t* parent, const char* name, uint32_t color) {
        lv_obj_t* row = RobiBox(parent, 60, LV_FLEX_FLOW_ROW);
        lv_obj_t* bar = lv_bar_create(row);
        lv_obj_set_size(bar, LV_HOR_RES * 28 / 100, LV_VER_RES * 2 / 100 + 2);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 50, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN);
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
        // no subtitles: the text covered the face
        SetHideSubtitle(true);

        // Robi's needs, under the status line
        robi_pet_ = RobiBox(lv_screen_active(), 60, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(robi_pet_, LV_VER_RES * 1 / 100, 0);
        lv_obj_align(robi_pet_, LV_ALIGN_TOP_MID, 0, LV_VER_RES * 15 / 100);
        robi_bar_energy_ = RobiPetRow(robi_pet_, "אנרגיה", 0xF1C40F);
        robi_bar_joy_ = RobiPetRow(robi_pet_, "שמחה", 0xFF6FA8);
        lv_obj_add_flag(robi_pet_, LV_OBJ_FLAG_HIDDEN);

        // activity menus (text on the button, then what Robi hears)
        static const char* const lavi[] = {
            "חשבון", "זה לביא. בוא נתרגל חשבון",
            "אנגלית", "זה לביא. בוא נתרגל אנגלית",
            "חידות", "זה לביא. תן לי חידה",
            "סיפור", "זה לביא. בוא נמשיך את הסיפור שלנו",
            "סתם לדבר", "זה לביא",
        };
        static const char* const ori[] = {
            "חשבון", "זה אורי. בוא נתרגל חשבון",
            "מחוננים", "זה אורי. בוא נתאמן למבחן המחוננים",
            "חידות", "זה אורי. תן לי חידה",
            "סיפור", "זה אורי. בוא נמשיך את הסיפור שלנו",
            "סתם לדבר", "זה אורי",
        };
        robi_menu_lavi_ = RobiMenu(lavi, 0x2E86DE);
        robi_menu_ori_ = RobiMenu(ori, 0xF39C12);

        // who is talking
        robi_profiles_ = RobiBox(lv_screen_active(), 80, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(robi_profiles_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(robi_profiles_, LV_ALIGN_BOTTOM_MID, 0, -(LV_VER_RES * 10 / 100));
        RobiButton(robi_profiles_, "לביא", 0x2E86DE, 30, 14, OnProfile, robi_menu_lavi_);
        RobiButton(robi_profiles_, "אורי", 0xF39C12, 30, 14, OnProfile, robi_menu_ori_);
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
