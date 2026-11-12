#ifndef ROBI_GEMINI_LIVE_H_
#define ROBI_GEMINI_LIVE_H_

#include "protocol.h"

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

    // Live diagnostics & hardware stress-test control hooks
    static GeminiLiveProtocol* GetActiveInstance();
    uint32_t GetTurnsCompleted() const { return turns_completed_.load(); }
    uint32_t GetAudioFramesPlayed() const { return audio_frames_played_.load(); }
    size_t GetRingAvailableSamples() const { return RingAvailable(); }
    bool IsAwaitingResponse() const { return awaiting_response_.load(); }
    bool IsStreamingSpeech() const { return streaming_speech_; }
    bool InjectUserPromptTurn(const std::string& text);
    bool SimulateMicUtterance(int duration_ms);

protected:
    bool SendText(const std::string& text) override;

private:
    bool TryConnectModel(const std::string& api_key, const std::string& model_name,
                         const std::string& voice_name, const std::string& lang_code,
                         bool include_lang_code);
    std::string BuildSetupJson(const std::string& model_name, const std::string& voice_name,
                               const std::string& lang_code, bool include_lang_code) const;
    void StopSessionInternal(bool notify_closed);

    void EnqueueTxJson(std::string&& json_str);
    void EnqueueClientTextTurn(const std::string& text);
    void EnqueuePcm16k(const uint8_t* pcm_bytes, size_t byte_len);
    void EnqueueSilence16k(size_t duration_ms);
    void FinishSpeechUtterance();

    void RxWorkerLoop();
    void TxWorkerLoop();
    void PlaybackFeederLoop();

    void HandleGeminiJson(const char* data, size_t len);
    void HandleToolCalls(const cJSON* tool_call);

    void PushPcm24kToRing(const int16_t* samples, size_t count);
    size_t PopPcm24kFromRing(int16_t* out_samples, size_t max_count);
    size_t RingAvailable() const;
    void ClearRing();

    void EmitLocalJson(const std::string& json_str);
    void EmitEmotion(const std::string& emotion);
    void EmitTtsState(const char* state);

    static int ComputeVoiceLevel(const int16_t* samples, size_t count);

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
    std::atomic<int64_t> awaiting_since_us_{0};
    std::atomic<int64_t> tool_hold_until_us_{0};
    std::atomic<int64_t> last_rx_audio_us_{0};
    std::atomic<uint32_t> turn_gen_{0};
    std::atomic<uint32_t> turns_completed_{0};
    std::atomic<uint32_t> audio_frames_played_{0};

    ListeningMode listening_mode_ = kListeningModeAutoStop;

    // Uplink VAD & pre-roll state
    std::mutex uplink_mutex_;
    std::deque<std::vector<uint8_t>> pre_roll_;
    std::vector<uint8_t> tx_pcm_batch_;
    bool streaming_speech_ = false;
    int onset_frames_ = 0;
    int speech_frames_ = 0;
    int silence_frames_ = 0;
    int awaiting_nudges_ = 0;
    int64_t listen_started_us_ = 0;
    std::atomic<int64_t> last_activity_us_{0};
    bool idle_farewell_sent_ = false;

    // Raw incoming WebSocket messages queued off EspSsl's small stack
    std::mutex rx_mutex_;
    std::condition_variable rx_cv_;
    std::deque<std::string> rx_queue_;

    // Outgoing WebSocket JSON queue
    std::mutex tx_mutex_;
    std::condition_variable tx_cv_;
    std::deque<std::string> tx_queue_;

    // 24kHz PCM PSRAM jitter ring buffer (262,144 samples = 512 KB = 10.9s)
    mutable std::mutex ring_mutex_;
    std::condition_variable ring_cv_;
    int16_t* pcm_ring_ = nullptr;
    size_t ring_capacity_ = 0;
    size_t ring_head_ = 0;
    size_t ring_tail_ = 0;
    size_t ring_count_ = 0;
    std::deque<std::string> pending_emotions_;

    TaskHandle_t rx_task_ = nullptr;
    TaskHandle_t tx_task_ = nullptr;
    TaskHandle_t play_task_ = nullptr;
};

#endif  // ROBI_GEMINI_LIVE_H_
