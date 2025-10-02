#include "tflite_detect.h"
#include <esp_log.h>

#include "TFLite_main_functions.h"

#define TAG "tflite_detect"

TFLiteMicroDetect::TFLiteMicroDetect() {
}

TFLiteMicroDetect::~TFLiteMicroDetect() {
    // destroy the model ram, TODO
    ESP_LOGI(TAG, "HELLO!!!! TFLite micro destroy");
}

void TFLiteMicroDetect::RegisterSetup(std::function<void()> setup) {
    my_model_setup_ = setup;
}

void TFLiteMicroDetect::RegisterLoop(std::function<void(int16_t *, int)> loop) {
    my_model_loop_ = loop;
}

int TFLiteMicroDetect::GetSampleRate(std::function<void()>)
{
    return TFLITE_DETECT_MODEL_SAMPLE_RATE;
}

bool TFLiteMicroDetect::Initialize(AudioCodec* codec) {
    codec_ = codec;

    RegisterSetup(setup);
    RegisterLoop(loop);

    if(my_model_setup_)
    {
        ESP_LOGI(TAG, "HELLO!!!! TFLite micro setup");
        my_model_setup_();
    }

    ESP_LOGI(TAG, "Wake word(%s),freq: %d, chunksize: %d", "TFLite TEST", TFLITE_DETECT_MODEL_SAMPLE_RATE, TFLITE_DETECT_MODEL_FEED_DATA_SIZE);
    return true;
}

void TFLiteMicroDetect::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

void TFLiteMicroDetect::Start() {
    running_ = true;
}

void TFLiteMicroDetect::Stop() {
    running_ = false;
}

void TFLiteMicroDetect::Feed(const std::vector<int16_t>& data) {
    if (!running_) {
        return;
    }

    //这个函数用来 确保捕获的音频数据样本数量达到模型的基本要求，否则可能会识别错误或者丢失zzhengqu数据
    if((!data.empty()) && (my_model_loop_))
    {
        int samples = data.size();
        ESP_LOGE(TAG, "HELLO!!!! TFLite micro feed");
        
        my_model_loop_(const_cast<int16_t*>(data.data()), samples);

        last_detected_wake_word_ = "No meanful string parameter";
        running_ = false;

        // process the application layer logic, and wakeup!!
        #if 0
        if (wake_word_detected_callback_) {
            wake_word_detected_callback_(last_detected_wake_word_);
        }
        #endif
    }
    else
    {
        ESP_LOGE(TAG, "TFLite micro model error: no data or no process procedure");
    }
}

size_t TFLiteMicroDetect::GetFeedSize() {
    //该函数返回模型预测一次需要的音频数据样本数量
    int audio_chunksize = TFLITE_DETECT_MODEL_FEED_DATA_SIZE;
    return audio_chunksize;
}

void TFLiteMicroDetect::EncodeWakeWordData() {
}

bool TFLiteMicroDetect::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    return false;
}
