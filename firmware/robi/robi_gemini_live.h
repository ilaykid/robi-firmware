#ifndef ROBI_GEMINI_LIVE_H_
#define ROBI_GEMINI_LIVE_H_

#include "protocol.h"
#include "robi_audio_sync.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <web_socket.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class GeminiLiveProtocol : public Protocol {
public:
    GeminiLiveProtocol();
    ~GeminiLiveProtocol() override;

    bool Start() override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;

    void SendWakeWordDetected(const std::string& wake_word) override;
    void SendStartListening(ListeningMode mode) override;
    void SendStopListening() override;
    void SendAbortSpeaking(AbortReason reason) override;

    // Called by Application::HandleStateChangedEvent right after ResetDecoder() completes
    // when entering kDeviceStateSpeaking, guaranteeing 0 dropped opening PCM frames.
    void NotifySpeakingAudioReady();

    // Lock-free 20Hz lip-sync voice envelope (0..100, or -1 if inactive/stale)
    int GetLiveVoiceLevel(int64_t now_us) const;

    // Live diagnostics & hardware stress-test control hooks
    static GeminiLiveProtocol* GetActiveInstance();
    std::string GetActiveModel() const;
    std::string GetOpenedScope() const;
    static std::string GetLastConnectedModel();
    static int GetModelCooldownSec(int idx);
    static void ResetModelCooldowns();
    uint32_t GetTurnsCompleted() const { return turns_completed_.load(); }
    uint32_t GetAudioFramesPlayed() const { return audio_frames_played_.load(); }
    uint32_t GetTxAudioPackets() const { return tx_audio_packets_.load(); }
    uint32_t GetRxWsMessages() const { return rx_ws_msgs_.load(); }
    int GetLastMicRms() const { return last_mic_rms_.load(std::memory_order_relaxed); }
    int GetPeakMicRms() const { return peak_mic_rms_.load(std::memory_order_relaxed); }
    int GetNoiseFloorRms() const { return last_noise_floor_rms_.load(std::memory_order_relaxed); }
    size_t GetRingAvailableSamples() const { return jitter_ring_.Available(); }
    bool IsAwaitingResponse() const { return awaiting_response_.load(); }
    bool IsStreamingSpeech() const { return streaming_speech_.load(); }
    bool CanExitListeningByTap() const;
    bool CommitListeningUtterance();
    bool InjectUserPromptTurn(const std::string& text);
    void CancelPendingTurnAudio();
    bool SimulateMicUtterance(int duration_ms);
    static void RequestAsyncAiTriviaDeck(uint32_t req_id, const std::string& topic_desc,
                                         bool head_to_head, bool english);

protected:
    bool SendText(const std::string& text) override;

private:
    enum class ConnectResult {
        kConnected,
        kNetworkUnreachable,
        kSetupRejected,
    };

    ConnectResult TryConnectModel(const std::string& api_key, const std::string& model_name,
                                  const std::string& voice_name, const std::string& lang_code,
                                  bool include_lang_code, bool include_google_search = true);
    std::string BuildSetupJson(const std::string& model_name, const std::string& voice_name,
                               const std::string& lang_code, bool include_lang_code,
                               bool include_google_search = true) const;
    static std::string BuildClientTurnJson(const std::string& text);
    static void CooldownModelByName(const std::string& model_name, int64_t cooldown_sec);
    void StopSessionInternal(bool notify_closed);
    void HandleFatalSocketError(const char* reason);
    void ResetTurnRuntimeFlags();

    void EnqueueTxJson(std::string&& json_str, bool is_audio = false);
    void PurgeQueuedClientContent();
    void EnqueueClientTextTurn(const std::string& text);
    void EnqueueSilentTurnNudge(const std::string& user_text);
    void EnqueuePcm16k(const uint8_t* pcm_bytes, size_t byte_len);
    void EnqueueSilence16k(size_t duration_ms);
    void EnqueueAudioStreamEnd();
    void FinishSpeechUtterance();

    void RxWorkerLoop();
    void TxWorkerLoop();
    void PlaybackFeederLoop();

    void HandleToolCalls(const cJSON* tool_call);
    void FlushPendingTranscripts();

    void ClearLiveVoiceLevel();
    void EmitLocalJson(const std::string& json_str);
    void EmitEmotion(const std::string& emotion);
    void EmitTtsState(const char* state);

    EventGroupHandle_t event_group_ = nullptr;
    EventGroupHandle_t worker_done_group_ = nullptr;

    std::unique_ptr<WebSocket> websocket_;
    std::atomic<bool> ws_connected_{false};
    std::atomic<bool> channel_opened_{false};
    std::atomic<bool> stopping_{false};
    std::atomic<bool> drop_turn_{false};
    std::atomic<bool> turn_complete_{false};
    std::atomic<bool> close_after_turn_{false};
    std::atomic<bool> awaiting_response_{false};
    std::atomic<bool> receiving_model_audio_{false};
    std::atomic<bool> speaking_active_{false};
    std::atomic<bool> speaking_audio_ready_{false};
    std::atomic<bool> streaming_speech_{false};
    std::atomic<bool> user_turn_had_speech_{false};
    std::atomic<bool> server_ack_received_{false};
    std::atomic<bool> silent_nudge_used_{false};
    std::atomic<int> consecutive_timeouts_{0};
    std::atomic<int> awaiting_nudges_{0};
    std::atomic<int> live_voice_level_{0};
    std::atomic<int64_t> live_voice_updated_us_{0};
    std::atomic<int64_t> awaiting_since_us_{0};
    std::atomic<int64_t> listen_started_us_{0};
    std::atomic<int64_t> post_playback_guard_until_us_{0};
    std::atomic<int64_t> last_input_tr_us_{0};
    std::atomic<int64_t> tool_hold_until_us_{0};
    std::atomic<int64_t> tool_sent_at_us_{0};
    std::atomic<int64_t> last_rx_audio_us_{0};
    std::atomic<uint32_t> turn_gen_{0};
    std::atomic<uint32_t> turns_completed_{0};
    std::atomic<uint32_t> audio_frames_played_{0};
    std::atomic<uint32_t> tx_audio_packets_{0};
    std::atomic<uint32_t> rx_ws_msgs_{0};
    std::atomic<int> last_mic_rms_{0};
    std::atomic<int> peak_mic_rms_{0};
    std::atomic<int> last_noise_floor_rms_{95};
    std::atomic<uint32_t> turn_uplink_pcm_bytes_{0};
    std::atomic<uint32_t> turn_downlink_samples_{0};
    std::atomic<uint32_t> turn_server_tokens_{0};
    mutable std::mutex model_mutex_;
    std::string active_model_;
    std::string opened_scope_;

    ListeningMode listening_mode_ = kListeningModeAutoStop;

    // Uplink turn-framed stream state
    std::mutex uplink_mutex_;
    std::deque<std::vector<uint8_t>> pre_roll_;
    std::vector<uint8_t> tx_pcm_batch_;
    robi::AdaptiveSpeechTracker speech_tracker_;
    float speech_pre_gain_ = 1.0f;
    std::mutex client_turn_mutex_;
    std::string last_client_turn_text_;
    std::atomic<int64_t> last_activity_us_{0};
    bool idle_farewell_sent_ = false;

    // Raw incoming WebSocket messages queued off EspSsl's small stack
    std::mutex rx_mutex_;
    std::condition_variable rx_cv_;
    std::deque<std::string> rx_queue_;

    // Accumulated transcripts flushed on turnComplete or synthesized turn end
    std::mutex transcript_mutex_;
    std::string pending_user_transcript_;
    std::string pending_bot_transcript_;

    // Outgoing WebSocket JSON queue
    std::mutex tx_mutex_;
    std::condition_variable tx_cv_;
    std::deque<std::string> tx_queue_;

    // Sample-accurate 24kHz PCM PSRAM jitter ring buffer (262,144 samples = 512 KB = 10.92s)
    int16_t* pcm_ring_mem_ = nullptr;
    robi::SampleSyncedJitterRing jitter_ring_;

    TaskHandle_t rx_task_ = nullptr;
    TaskHandle_t tx_task_ = nullptr;
    TaskHandle_t play_task_ = nullptr;
    StackType_t* rx_stack_ = nullptr;
    StaticTask_t* rx_tcb_ = nullptr;
    StackType_t* tx_stack_ = nullptr;
    StaticTask_t* tx_tcb_ = nullptr;
    StackType_t* play_stack_ = nullptr;
    StaticTask_t* play_tcb_ = nullptr;
};

#endif  // ROBI_GEMINI_LIVE_H_
