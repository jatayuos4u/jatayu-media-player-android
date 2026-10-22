#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <functional>
class QMediaPlayer;
// Owns transport intent, not decoding. Android must grant focus before play().
class PlaybackSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool pictureInPicture READ pictureInPicture NOTIFY pictureInPictureChanged)
    Q_PROPERTY(bool pipSupported READ pipSupported CONSTANT)
public:
    using Transport=std::function<void(const QString &,qint64,const QVariantMap &)>;
    explicit PlaybackSession(QObject *parent=nullptr,Transport transport={});
    ~PlaybackSession() override;
    Q_INVOKABLE void attach(QObject *player,bool preserveOutgoing=false);
    Q_INVOKABLE void metadata(const QString &title,const QString &artist,bool music,bool previous,bool next);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void background();
    bool pictureInPicture()const{return m_pip;}
    bool pipSupported()const;
    Q_INVOKABLE void setPiPEnabled(bool enabled);
    Q_INVOKABLE void enterPiP();
    Q_INVOKABLE void restoreWindowBounds(QObject *window,int pixelWidth,int pixelHeight);
    void handleAction(const QString &action,qint64 token);
signals:
    void pictureInPictureChanged();
    void windowRestoreRequested(int pixelWidth,int pixelHeight);
    void nextRequested();void previousRequested();void seekRequested(qint64 position);
    void errorOccurred(const QString &message);
private:
    void publish();void send(const QString &action);void invalidate();
    QVariantMap snapshot() const;
    QPointer<QMediaPlayer> m_player;
    Transport m_transport;
    QTimer m_timer;
    QString m_title,m_artist;
    bool m_pip=false,m_pipEnabled=true;
    bool m_music=false,m_previous=false,m_next=false,m_wantsPlay=false;
    qint64 m_token=0;
};
