#pragma once
#include <QObject>
#include <QAudioBuffer>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>
#include <deque>
#include <memory>
#include <chrono>
#include <mutex>
#include "SoundDsp.h"
class QAudioSink;class QIODevice;
inline qint64 studioClockMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
// Shared with the decoder-thread ingress. No QObject or player state is read
// across threads; old source/seek generations keep their own accounting.
struct StudioBudget {
    std::mutex ingress;
    std::atomic<qint64> queuedUs{0},lastFeedMs{0},seekTarget{-1},seekStartedMs{0};
    std::atomic<bool> accepting{false},paused{true};
};

// All sink writes and DSP work run on the audio worker, never the QML thread.
class StudioOutput : public QObject {
    Q_OBJECT
public:
    explicit StudioOutput(QObject *parent=nullptr);
    ~StudioOutput() override;
    void reset(quint64 token);
    void enqueue(const QAudioBuffer &,const QAudioDevice &,std::shared_ptr<StudioBudget>,quint64 token);
    void settings(const SoundDsp::Settings &s){m_dsp.setSettings(s);}
    void volume(float v);
    void pause(bool paused);
    void finish(quint64 token);
    void reject(quint64 token,const QString &reason){if(token==m_token)fail(reason);}
signals:
    void failed(quint64 token,const QString &reason);
    void meter(quint64 token,double reduction);
    void drained(quint64 token);
    void sourceFormat(quint64 token,int sampleRate);
private:
    struct Block {QByteArray pcm;qsizetype offset=0;qint64 duration=0;std::shared_ptr<StudioBudget> budget;};
    void pump();void fail(const QString &);void clear();
    std::deque<Block> m_blocks;
    QAudioSink *m_sink=nullptr;QIODevice *m_io=nullptr;
    QTimer *m_timer;QAudioFormat m_format;SoundDsp m_dsp;
    QElapsedTimer m_finishClock,m_meterClock;
    quint64 m_token=0;qint64 m_pendingUs=0;
    float m_volume=1;bool m_paused=false,m_finishing=false;
};
