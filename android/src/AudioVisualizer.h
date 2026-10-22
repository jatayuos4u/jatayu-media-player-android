#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QAudioBufferOutput>
#include <QElapsedTimer>
#include <QTimer>
#include <array>

class QAudioBuffer;
class QMediaPlayer;

class AudioVisualizer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY spectrumChanged)
    Q_PROPERTY(qreal level READ level NOTIFY spectrumChanged)
    Q_PROPERTY(bool available READ available NOTIFY spectrumChanged)
public:
    explicit AudioVisualizer(QObject *parent = nullptr);
    ~AudioVisualizer() override;
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    void setCaptureAudio(bool enabled);
    bool processing() const { return m_timer.isActive(); }
    QVariantList bands() const;
    qreal level() const { return m_level; }
    bool available() const { return m_available; }
    Q_INVOKABLE void attach(QObject *player);
    // Same decoded-buffer entry point is exercised by deterministic DSP tests.
    void consume(const QAudioBuffer &buffer);
signals:
    void decodedBuffer(const QAudioBuffer &buffer);
    void enabledChanged();
    void processingChanged();
    void spectrumChanged();
private:
    void reset();
    void updateOutput();
    void updateProcessing();
    void releaseOutput();
    void updateSpectrum();
    static constexpr int Window = 2048, BandCount = 32;
    QPointer<QMediaPlayer> m_player;
    QPointer<QAudioBufferOutput> m_output;
    QTimer m_timer;
    QElapsedTimer m_lastBuffer;
    std::array<float, Window> m_samples{}, m_window{};
    std::array<float, BandCount> m_bands{};
    int m_write = 0, m_count = 0, m_sampleRate = 0;
    qreal m_level = 0;
    bool m_captureAudio = false, m_enabled = false, m_available = false, m_dirty = false;
};

