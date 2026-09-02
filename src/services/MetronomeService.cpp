#include "MetronomeService.h"
#include "MetronomeAudio.h"

#include <QDir>
#include <QSettings>
#include <cmath>

namespace {

constexpr int kSampleRate = 44100;
constexpr int kBitDepth = 16;
constexpr double kClickDurationSeconds = 0.05;

QByteArray generateClickWave(double frequency, double amplitude)
{
    const int totalSamples = static_cast<int>(kSampleRate * kClickDurationSeconds);
    QByteArray samples;
    samples.resize(totalSamples * 2);
    auto* data = reinterpret_cast<qint16*>(samples.data());
    for (int i = 0; i < totalSamples; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double envelope = std::exp(-t * 60.0);
        const double value = std::sin(2.0 * M_PI * frequency * t) * envelope * amplitude;
        data[i] = static_cast<qint16>(qBound(-1.0, value, 1.0) * 32767.0);
    }

    const quint32 dataSize = static_cast<quint32>(samples.size());
    const quint32 fileSize = 36 + dataSize;
    const quint32 byteRate = kSampleRate * 1 * (kBitDepth / 8);
    const quint16 blockAlign = static_cast<quint16>(1 * (kBitDepth / 8));

    QByteArray wav;
    wav.reserve(44 + dataSize);
    wav.append("RIFF", 4);
    wav.append(reinterpret_cast<const char*>(&fileSize), 4);
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    const quint32 fmtSize = 16;
    wav.append(reinterpret_cast<const char*>(&fmtSize), 4);
    const quint16 audioFormat = 1;
    wav.append(reinterpret_cast<const char*>(&audioFormat), 2);
    const quint16 channels = 1;
    wav.append(reinterpret_cast<const char*>(&channels), 2);
    const quint32 sampleRate = kSampleRate;
    wav.append(reinterpret_cast<const char*>(&sampleRate), 4);
    wav.append(reinterpret_cast<const char*>(&byteRate), 4);
    wav.append(reinterpret_cast<const char*>(&blockAlign), 2);
    const quint16 bitsPerSample = kBitDepth;
    wav.append(reinterpret_cast<const char*>(&bitsPerSample), 2);
    wav.append("data", 4);
    wav.append(reinterpret_cast<const char*>(&dataSize), 4);
    wav.append(samples);
    return wav;
}

bool writeTempWav(QTemporaryFile& file, const QByteArray& data)
{
    file.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-metronome-XXXXXX.wav"));
    if (!file.open()) return false;
    file.write(data);
    file.flush();
    file.close();
    return true;
}

}

MetronomeService::MetronomeService(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    m_bpm = qBound(1, settings.value(QStringLiteral("metronome/bpm"), 120).toInt(), 500);
    m_beatsPerMeasure = qBound(1, settings.value(QStringLiteral("metronome/beatsPerMeasure"), 4).toInt(), 16);
    m_beatUnit = qBound(1, settings.value(QStringLiteral("metronome/beatUnit"), 4).toInt(), 32);
    m_volume = qBound(0.0, settings.value(QStringLiteral("metronome/volume"), 0.8).toDouble(), 1.0);

    generateClickSamples();
    m_audio = createMetronomeAudio();
    m_audio->setSources(m_accentFile.fileName(), m_normalFile.fileName());
    m_audio->setVolume(m_volume);

    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MetronomeService::onTick);
    updateTimerInterval();
}

MetronomeService::~MetronomeService()
{
    stop();
}

bool MetronomeService::running() const { return m_running; }
int MetronomeService::bpm() const { return m_bpm; }
int MetronomeService::beatsPerMeasure() const { return m_beatsPerMeasure; }
int MetronomeService::beatUnit() const { return m_beatUnit; }
double MetronomeService::volume() const { return m_volume; }

void MetronomeService::setBpm(int bpm)
{
    const auto clamped = qBound(1, bpm, 500);
    if (clamped == m_bpm) return;
    m_bpm = clamped;
    QSettings().setValue(QStringLiteral("metronome/bpm"), m_bpm);
    updateTimerInterval();
    emit bpmChanged();
}

void MetronomeService::setBeatsPerMeasure(int beats)
{
    const auto clamped = qBound(1, beats, 16);
    if (clamped == m_beatsPerMeasure) return;
    m_beatsPerMeasure = clamped;
    m_currentBeat = 0;
    QSettings().setValue(QStringLiteral("metronome/beatsPerMeasure"), m_beatsPerMeasure);
    emit beatsPerMeasureChanged();
}

void MetronomeService::setBeatUnit(int unit)
{
    const auto clamped = qBound(1, unit, 32);
    if (clamped == m_beatUnit) return;
    m_beatUnit = clamped;
    QSettings().setValue(QStringLiteral("metronome/beatUnit"), m_beatUnit);
    emit beatUnitChanged();
}

void MetronomeService::setVolume(double volume)
{
    const auto clamped = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(clamped, m_volume)) return;
    m_volume = clamped;
    if (m_audio) m_audio->setVolume(m_volume);
    QSettings().setValue(QStringLiteral("metronome/volume"), m_volume);
    emit volumeChanged();
}

void MetronomeService::start()
{
    if (m_running) return;
    m_currentBeat = 0;
    m_running = true;
    m_timer.start();
    onTick();
    emit runningChanged();
}

void MetronomeService::stop()
{
    if (!m_running) return;
    m_running = false;
    m_timer.stop();
    emit runningChanged();
}

void MetronomeService::toggle()
{
    if (m_running) stop();
    else start();
}

void MetronomeService::onTick()
{
    if (m_currentBeat == 0) {
        m_audio->playAccent();
    } else {
        m_audio->playNormal();
    }
    emit beat(m_currentBeat);
    ++m_currentBeat;
    if (m_currentBeat >= m_beatsPerMeasure) {
        m_currentBeat = 0;
    }
}

void MetronomeService::updateTimerInterval()
{
    const int interval = qMax(1, static_cast<int>(60000.0 / m_bpm));
    m_timer.setInterval(interval);
}

void MetronomeService::generateClickSamples()
{
    const auto accent = generateClickWave(880.0, 0.9);
    const auto normal = generateClickWave(660.0, 0.6);
    writeTempWav(m_accentFile, accent);
    writeTempWav(m_normalFile, normal);
}
