#ifndef ROBI_CONFIG_H_
#define ROBI_CONFIG_H_

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct RobiActivity {
    std::string label;
    std::string prompt;
};

struct RobiKid {
    std::string name;
    int age = 8;
    std::string color = "#0284C7";
    uint32_t color_hex = 0x0284C7;
    std::string interests;
    std::vector<RobiActivity> activities;
};

struct RobiModelUsage {
    std::string id;
    uint32_t sessions = 0;
    uint32_t turns = 0;
    uint32_t audio_sec = 0;
    uint32_t tokens = 0;
};

struct RobiConfigData {
    std::string bot_name = "רובי";
    std::string gemini_key;
    std::string preferred_model = "auto";
    std::string voice = "Puck";
    std::string language = "he-IL";
    std::string theme = "cyber_cyan";
    std::string face_style = "cyber_visor";
    int volume = 80;
    int brightness = 85;
    std::vector<std::string> custom_topics;
    std::string custom_prompt;
    std::vector<RobiKid> kids;
};

class RobiConfig {
public:
    static constexpr uint32_t kFlashConfigAddr = 0x010000;
    static constexpr size_t kMaxConfigJsonBytes = 12288;

    static RobiConfig& GetInstance();

    // Call early at boot (before WifiBoard::TryWifiConnect) to import any new
    // WebSerial config blob from 0x010000 and load NVS configuration.
    void EnsureLoaded();

    RobiConfigData Get() const;
    bool HasApiKey() const;

    bool UpdateFromJson(const std::string& json_str, bool from_flash_sector, std::string& err_out);
    std::string ExportJson(bool mask_api_key) const;

    // Persistent short-term memory notes stored in NVS ("robi_mem")
    void AddMemoryNote(const std::string& note);
    std::vector<std::string> GetMemoryNotes() const;
    void ClearMemoryNotes();

    // Rolling in-RAM conversation history across reconnects
    void AddDialogueTurn(const std::string& role, const std::string& text);
    std::vector<std::pair<std::string, std::string>> GetDialogue() const;
    void ClearDialogue();

    // Daily per-model usage & quota tracking persisted in NVS ("robi_mem" / "usage")
    void SyncUsageDay(const std::string& client_day_iso);
    void RecordModelSession(const std::string& model_id);
    void RecordModelTurn(const std::string& model_id, uint32_t audio_sec_delta, uint32_t tokens_delta);
    void ResetDailyUsage();
    std::string GetUsageDay() const;
    std::vector<RobiModelUsage> GetModelUsage() const;

    // Builds full system instruction for Gemini Live
    std::string BuildSystemPrompt() const;

    // Live UI reload & telemetry hooks (registered by CustomLcdDisplay)
    void SetUiReloadCallback(std::function<void()> cb);
    void NotifyUiReload();
    void SetUiStatusProvider(std::function<std::string()> cb);
    std::string GetUiStatusJson() const;

    static uint32_t ParseHexColor(const std::string& s, uint32_t fallback = 0x0284C7);

private:
    RobiConfig() = default;

    bool ParseIntoLocked(const std::string& json_str, bool keep_existing_key_if_sentinel,
                         std::string& wifi_ssid_out, std::string& wifi_pass_out,
                         std::string& err_out);
    std::string SerializeLocked(bool mask_api_key) const;
    bool SaveConfigToNvsLocked(const std::string& serialized);
    void LoadMemoriesFromNvsLocked();
    void SaveMemoriesToNvsLocked();
    void LoadUsageFromNvsLocked();
    void SaveUsageToNvsLocked();
    RobiModelUsage& FindOrCreateModelUsageLocked(const std::string& model_id);
    void TryImportFlashSectorLocked();

    mutable std::mutex mutex_;
    bool loaded_ = false;
    RobiConfigData data_;
    std::vector<std::string> memories_;
    std::vector<std::pair<std::string, std::string>> dialogue_;
    std::string usage_day_ = "today";
    std::vector<RobiModelUsage> model_usage_;
    std::function<void()> ui_reload_cb_;
    std::function<std::string()> ui_status_cb_;
};

#endif  // ROBI_CONFIG_H_
