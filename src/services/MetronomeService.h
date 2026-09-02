#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

class MetronomeAudio;

class MetronomeService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    Q_PROPERTY(int beatsPerMeasure READ beatsPerMeasure WRITE setBeatsPerMeasure NOTIFY beatsPerMeasureChanged)
    Q_PROPERTY(int beatUnit READ beatUnit WRITE setBeatUnit NOTIFY beatUnitChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit MetronomeService(QObject* parent = nullptr);
    ~MetronomeService() override;

    [[nodiscard]] bool running() const;
    [[nodiscard]] int bpm() const;
    void setBpm(int bpm);
    [[nodiscard]] int beatsPerMeasure() const;
    void setBeatsPerMeasure(int beats);
    [[nodiscard]] int beatUnit() const;
    void setBeatUnit(int unit);
    [[nodiscard]] double volume() const;
    void setVolume(double volume);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();

signals:
    void runningChanged();
    void bpmChanged();
    void beatsPerMeasureChanged();
    void beatUnitChanged();
    void volumeChanged();
    void beat(int beatIndex);

private slots:
    void onTick();

private:
    void updateTimerInterval();

    QTimer m_timer;
    std::unique_ptr<MetronomeAudio> m_audio;

    bool m_running {false};
    int m_bpm {120};
    int m_beatsPerMeasure {4};
    int m_beatUnit {4};
    double m_volume {0.8};
    int m_currentBeat {0};
};
