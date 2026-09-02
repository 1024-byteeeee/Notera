#include "MetronomeAudio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio.h"

#include <cmath>
#include <mutex>

class MetronomeAudioMini final : public MetronomeAudio
{
public:
    MetronomeAudioMini()
    {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = 1;
        config.sampleRate = 44100;
        config.dataCallback = &MetronomeAudioMini::audioCallback;
        config.pUserData = this;

        if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) {
            m_deviceInitialized = false;
            return;
        }
        m_deviceInitialized = true;
    }

    ~MetronomeAudioMini() override
    {
        if (m_deviceInitialized) {
            ma_device_stop(&m_device);
            ma_device_uninit(&m_device);
        }
    }

    void setSources(const QString&, const QString&) override {}

    void setVolume(double volume) override
    {
        m_volume = static_cast<float>(volume);
    }

    void playAccent() override
    {
        triggerClick(880.0f, 0.9f);
    }

    void playNormal() override
    {
        triggerClick(660.0f, 0.55f);
    }

private:
    void triggerClick(float frequency, float amplitude)
    {
        if (!m_deviceInitialized) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_clickFrequency = frequency;
        m_clickAmplitude = amplitude;
        m_clickSampleIndex = 0;
        m_clickActive = true;
        if (!m_deviceStarted) {
            ma_device_start(&m_device);
            m_deviceStarted = true;
        }
    }

    static void audioCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
    {
        auto* self = static_cast<MetronomeAudioMini*>(device->pUserData);
        auto* out = static_cast<float*>(output);
        const float sampleRate = static_cast<float>(device->sampleRate);
        const int totalClickSamples = static_cast<int>(sampleRate * 0.05f);

        std::lock_guard<std::mutex> lock(self->m_mutex);
        for (ma_uint32 i = 0; i < frameCount; ++i) {
            float sample = 0.0f;
            if (self->m_clickActive && self->m_clickSampleIndex < totalClickSamples) {
                const float t = static_cast<float>(self->m_clickSampleIndex) / sampleRate;
                const float envelope = std::exp(-t * 60.0f);
                sample = std::sin(2.0f * static_cast<float>(M_PI) * self->m_clickFrequency * t)
                    * envelope * self->m_clickAmplitude * self->m_volume;
                ++self->m_clickSampleIndex;
                if (self->m_clickSampleIndex >= totalClickSamples) {
                    self->m_clickActive = false;
                }
            }
            out[i] = sample;
        }
    }

    ma_device m_device {};
    bool m_deviceInitialized {false};
    bool m_deviceStarted {false};
    std::mutex m_mutex;
    float m_volume {0.8f};
    float m_clickFrequency {880.0f};
    float m_clickAmplitude {0.9f};
    int m_clickSampleIndex {0};
    bool m_clickActive {false};
};

std::unique_ptr<MetronomeAudio> createMetronomeAudio()
{
    return std::make_unique<MetronomeAudioMini>();
}
