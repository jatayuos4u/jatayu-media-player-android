#pragma once
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
#include <QAudioFormat>
#include <QByteArray>
#include <QSettings>
#include <memory>
#include <algorithm>
#include <QThread>
#include "StudioOutput.h"
#include "SoundDsp.h"
class QMediaPlayer;class AudioVisualizer;class QAudioOutput;class QAudioSink;class QIODevice;class QAudioBuffer;
class SoundStudio : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool original READ original WRITE setOriginal NOTIFY settingsChanged)
    Q_PROPERTY(bool music READ music WRITE setMusic NOTIFY stateChanged)
    Q_PROPERTY(bool routing READ routing NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString preset READ preset NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY settingsChanged)
    Q_PROPERTY(double bass READ bass WRITE setBass NOTIFY settingsChanged)
    Q_PROPERTY(double vocals READ vocals WRITE setVocals NOTIFY settingsChanged)
    Q_PROPERTY(double treble READ treble WRITE setTreble NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY settingsChanged)
    Q_PROPERTY(QString basePreset READ basePreset NOTIFY settingsChanged)
    Q_PROPERTY(QString description READ description NOTIFY settingsChanged)
    Q_PROPERTY(double strength READ strength WRITE setStrength NOTIFY settingsChanged)
    Q_PROPERTY(bool levelMatch READ levelMatch WRITE setLevelMatch NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList response READ response NOTIFY settingsChanged)
    Q_PROPERTY(double responseMaxFrequency READ responseMaxFrequency NOTIFY settingsChanged)
    Q_PROPERTY(double headroom READ headroom NOTIFY settingsChanged)
    Q_PROPERTY(double reduction READ reduction NOTIFY meterChanged)
public:
    explicit SoundStudio(QObject *parent=nullptr,const QString &settingsFile={},bool persistent=true);
    void copyConfigurationFrom(const SoundStudio &other);
    ~SoundStudio() override;
    bool enabled() const{return m_enabled;}bool original() const{return m_values.original;}
    bool music() const{return m_music;}bool routing() const{return m_routing;}
    QString status() const;QString preset() const{return m_preset;}QVariantList bands() const;
    double bass() const{return m_values.bass;}double vocals() const{return m_values.vocals;}double treble() const{return m_values.treble;}
    QVariantList profiles() const;
    QString basePreset() const{return m_basePreset;}
    QString description() const;
    double strength() const{return m_strength;}
    bool levelMatch() const{return m_values.levelMatch;}
    QVariantList response() const;
    double headroom() const;
    double responseMaxFrequency() const{return std::min(20000.,m_sampleRate*.49); }
    void setStrength(double);void setLevelMatch(bool);
    double reduction() const{return m_reduction;}
    void setEnabled(bool);void setOriginal(bool);void setMusic(bool);
    void setBass(double);void setVocals(double);void setTreble(double);
    Q_INVOKABLE void attach(QObject *player,QObject *visualizer);
    Q_INVOKABLE void setBand(int index,double value);
    Q_INVOKABLE void applyPreset(const QString &name);
    Q_INVOKABLE void saveCustom();
    Q_INVOKABLE bool saveProfile(const QString &name);
    Q_INVOKABLE void removeProfile(const QString &id);
    Q_INVOKABLE void resetPreset();
    Q_INVOKABLE void prepareSeek(qint64 position);
    Q_INVOKABLE void finish();
    Q_INVOKABLE void retry();
signals:
    void settingsChanged();void stateChanged();void meterChanged();void playbackFinished();
private:
    SoundDsp::Settings effective() const;
    void changed();void save();void updateRouting();void stopSink(bool reconnect=true);void connectCapture();
    void fail(const QString &reason);void syncVolume();
    QPointer<QMediaPlayer> m_player;QPointer<AudioVisualizer> m_visualizer;QPointer<QAudioOutput> m_originalOutput;
    std::unique_ptr<QSettings> m_settings;
    QThread m_audioThread;StudioOutput *m_worker=nullptr;
    std::shared_ptr<StudioBudget> m_budget=std::make_shared<StudioBudget>();
    quint64 m_token=0;
    QTimer m_watch,m_save;
    QMetaObject::Connection m_captureConnection;
    SoundDsp::Settings m_values;
    bool m_persistent=true;
    bool m_enabled=false,m_music=false,m_routing=false,m_finishing=false;
    bool m_userMuted=false,m_internalMute=false;
    double m_reduction=0;
    QString m_preset="Flat",m_basePreset="Flat",m_failure;
    double m_strength=100;
    int m_sampleRate=48000;
    QVariantList m_profiles;
};
