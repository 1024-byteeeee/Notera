#include "MetronomeAudio.h"

#include <QString>
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

class MetronomeAudioWin final : public MetronomeAudio
{
public:
    void setSources(const QString& accentPath, const QString& normalPath) override
    {
        m_accentPath = accentPath;
        m_normalPath = normalPath;
    }

    void setVolume(double volume) override
    {
        m_volume = volume;
    }

    void playAccent() override
    {
        if (!m_accentPath.isEmpty()) {
            PlaySoundW(reinterpret_cast<LPCWSTR>(m_accentPath.utf16()), nullptr,
                SND_FILENAME | SND_ASYNC | SND_NOSTOP);
        }
    }

    void playNormal() override
    {
        if (!m_normalPath.isEmpty()) {
            PlaySoundW(reinterpret_cast<LPCWSTR>(m_normalPath.utf16()), nullptr,
                SND_FILENAME | SND_ASYNC | SND_NOSTOP);
        }
    }

private:
    QString m_accentPath;
    QString m_normalPath;
    double m_volume {0.8};
};

std::unique_ptr<MetronomeAudio> createMetronomeAudio()
{
    return std::make_unique<MetronomeAudioWin>();
}
