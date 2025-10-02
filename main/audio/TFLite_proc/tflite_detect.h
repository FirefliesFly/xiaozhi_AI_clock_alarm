#ifndef TFLITE_DETECT_H
#define TFLITE_DETECT_H

#include <string>
#include <vector>
#include <functional>
#include <atomic>

#include "audio_codec.h"
#include "wake_word.h"

#define TFLITE_DETECT_MODEL_SAMPLE_RATE 16000
#define TFLITE_DETECT_MODEL_FEED_DATA_SIZE 4000

class TFLiteMicroDetect : public WakeWord {
public:
    TFLiteMicroDetect();
    ~TFLiteMicroDetect();

    bool Initialize(AudioCodec* codec);
    void Feed(const std::vector<int16_t>& data);
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback);
    void Start();
    void Stop();
    size_t GetFeedSize();
    void EncodeWakeWordData();
    bool GetWakeWordOpus(std::vector<uint8_t>& opus);
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; }

private:
    // esp_wn_iface_t *wakenet_iface_ = nullptr;
    // model_iface_data_t *wakenet_data_ = nullptr;
    // srmodel_list_t *wakenet_model_ = nullptr;
    AudioCodec* codec_ = nullptr;
    std::atomic<bool> running_ = false;

    std::function<void(void)> my_model_setup_ = nullptr;
    std::function<void(int16_t *, int)> my_model_loop_ = nullptr;

    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    std::string last_detected_wake_word_;

    void RegisterSetup(std::function<void()> setup);
    void RegisterLoop(std::function<void(int16_t *, int)> loop);
    int GetSampleRate(std::function<void()>);
};

#endif
