#pragma once

#include <QString>
#include <memory>

class MetronomeAudio
{
public:
    virtual ~MetronomeAudio() = default;
    virtual void setSources(const QString& accentPath, const QString& normalPath) = 0;
    virtual void setVolume(double volume) = 0;
    virtual void playAccent() = 0;
    virtual void playNormal() = 0;
};

std::unique_ptr<MetronomeAudio> createMetronomeAudio();
