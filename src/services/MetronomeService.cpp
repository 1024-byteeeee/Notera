#include "MetronomeService.h"
#include "MetronomeAudio.h"

#include <QSettings>

MetronomeService::MetronomeService(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    m_bpm = qBound(1, settings.value(QStringLiteral("metronome/bpm"), 120).toInt(), 500);
    m_beatsPerMeasure = qBound(1, settings.value(QStringLiteral("metronome/beatsPerMeasure"), 4).toInt(), 16);
    m_beatUnit = qBound(1, settings.value(QStringLiteral("metronome/beatUnit"), 4).toInt(), 32);
    static const int kValidNoteValues[] = {1, 2, 4, 8, 16, 32};
    bool isValid = false;
    for (int v : kValidNoteValues) {
        if (m_beatUnit == v) { isValid = true; break; }
    }
    if (!isValid) {
        int closest = 4;
        int minDiff = (m_beatUnit > 4) ? m_beatUnit - 4 : 4 - m_beatUnit;
        for (int v : kValidNoteValues) {
            const int diff = (m_beatUnit > v) ? m_beatUnit - v : v - m_beatUnit;
            if (diff < minDiff) { minDiff = diff; closest = v; }
        }
        m_beatUnit = closest;
    }
    m_volume = qBound(0.0, settings.value(QStringLiteral("metronome/volume"), 0.8).toDouble(), 1.0);

    m_audio = createMetronomeAudio();
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
