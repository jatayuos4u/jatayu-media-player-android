#pragma once
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>
#include <QElapsedTimer>
#include "MediaSource.h"
#include "AudioVisualizer.h"
#include "SoundStudio.h"
// Two independent decoders and Studio chains. Complementary gain ramps never
// exceed the selected volume, including correlated recordings.
class PlaybackDecks : public QObject {
    Q_OBJECT
    Q_PROPERTY(QMediaPlayer* player READ player NOTIFY activeChanged)
    Q_PROPERTY(SoundStudio* studio READ studio NOTIFY activeChanged)
    Q_PROPERTY(AudioVisualizer* visualizer READ visualizer NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool fading READ fading NOTIFY fadingChanged)
public:
    explicit PlaybackDecks(QObject *parent=nullptr,MediaSource::DescriptorOpener opener={},const QString &settingsFile={});
    QMediaPlayer *player(){return &m_decks[m_active]->player;}
    SoundStudio *studio(){return &m_decks[m_active]->studio;}
    AudioVisualizer *visualizer(){return &m_decks[m_active]->visualizer;}
    QString error()const{return m_error;}bool fading()const{return m_fading;}
    Q_INVOKABLE bool open(QObject *,const QUrl &);
    Q_INVOKABLE void setVideoOutput(QObject *);
    Q_INVOKABLE void setVolume(double);
    Q_INVOKABLE void setCrossfade(int seconds);
    Q_INVOKABLE void stageNext(const QUrl &);
    Q_INVOKABLE void cancelTransition();
    double outgoingGain()const{return m_outGain;}
    double incomingGain()const{return m_inGain;}
signals:
    void activeChanged();void errorChanged();void fadingChanged();
    void crossfadeStarted(const QUrl &outgoing,const QUrl &incoming,qint64 position,qint64 duration);
private:
    struct Deck {
        QAudioOutput audio;QMediaPlayer player;MediaSource source;AudioVisualizer visualizer;SoundStudio studio;
        Deck(MediaSource::DescriptorOpener opener,const QString &file,bool persist):source(nullptr,opener),studio(nullptr,file,persist){player.setAudioOutput(&audio);visualizer.attach(&player);studio.attach(&player,&visualizer);}
    };
    void updateTimer();void tick();void gains(double incoming);void handoff();
    std::unique_ptr<Deck> m_decks[2];QPointer<QObject> m_video;
    int m_active=0,m_seconds=0,m_fadeDuration=0;double m_volume=1,m_outGain=0,m_inGain=1;
    bool m_fading=false,m_starting=false,m_mirroring=false,m_mutating=false;
    QString m_error;QUrl m_staged;QTimer m_timer;QElapsedTimer m_clock;
};
