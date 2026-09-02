#include "MetronomeAudio.h"

#import <Foundation/Foundation.h>
#import <AppKit/NSSound.h>

class MetronomeAudioMac final : public MetronomeAudio
{
public:
    void setSources(const QString& accentPath, const QString& normalPath) override
    {
        m_accentSound = [[NSSound alloc] initWithContentsOfFile:accentPath.toNSString() byReference:YES];
        m_normalSound = [[NSSound alloc] initWithContentsOfFile:normalPath.toNSString() byReference:YES];
        [m_accentSound setVolume:m_volume];
        [m_normalSound setVolume:m_volume];
    }

    void setVolume(double volume) override
    {
        m_volume = static_cast<float>(volume);
        if (m_accentSound) [m_accentSound setVolume:m_volume];
        if (m_normalSound) [m_normalSound setVolume:m_volume];
    }

    void playAccent() override
    {
        if (m_accentSound) {
            [m_accentSound stop];
            [m_accentSound play];
        }
    }

    void playNormal() override
    {
        if (m_normalSound) {
            [m_normalSound stop];
            [m_normalSound play];
        }
    }

private:
    NSSound* m_accentSound {nil};
    NSSound* m_normalSound {nil};
    float m_volume {0.8f};
};

std::unique_ptr<MetronomeAudio> createMetronomeAudio()
{
    return std::make_unique<MetronomeAudioMac>();
}
