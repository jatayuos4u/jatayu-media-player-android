#include "PlaybackSession.h"
#include <QMediaPlayer>
#include <QCoreApplication>
#include <QMediaMetaData>
#include <QSize>
#include <QWindow>
#include <cmath>
#include <QJsonDocument>
#include <algorithm>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif
namespace {
QPointer<PlaybackSession> session; // Read only on the Qt application thread.
#ifdef Q_OS_ANDROID
void nativeAction(JNIEnv *,jclass,jstring action,jlong token){
    const auto text=QJniObject(action).toString();
    if(auto app=QCoreApplication::instance())QMetaObject::invokeMethod(app,[text,token]{if(session)session->handleAction(text,token);},Qt::QueuedConnection);
}
#endif
}
PlaybackSession::PlaybackSession(QObject *parent,Transport transport):QObject(parent),m_transport(std::move(transport)){
#ifdef Q_OS_ANDROID
    if(!m_transport){
        session=this;QJniEnvironment env;
        const JNINativeMethod methods[]={{const_cast<char *>("nativeAction"),const_cast<char *>("(Ljava/lang/String;J)V"),reinterpret_cast<void *>(nativeAction)}};
        const bool ready=env.registerNativeMethods("io/github/jatayuos4u/jatayumediaplayer/JatayuPlaybackService",methods,1);
        QJniObject::callStaticMethod<void>("io/github/jatayuos4u/jatayumediaplayer/JatayuPlaybackService","setNativeReady","(Z)V",jboolean(ready));
        m_transport=[](const QString &action,qint64 token,const QVariantMap &data){
            QJniObject::callStaticMethod<void>("io/github/jatayuos4u/jatayumediaplayer/JatayuPlaybackService","dispatch","(Ljava/lang/String;JLjava/lang/String;)V",QJniObject::fromString(action).object(),jlong(token),QJniObject::fromString(QString::fromUtf8(QJsonDocument::fromVariant(data).toJson(QJsonDocument::Compact))).object());
        };
    }
#endif
    m_timer.setInterval(1000);connect(&m_timer,&QTimer::timeout,this,&PlaybackSession::publish);
}
PlaybackSession::~PlaybackSession(){stop();send("shutdown");if(session==this)session=nullptr;}
void PlaybackSession::attach(QObject *object,bool preserveOutgoing){
    if(preserveOutgoing)invalidate();else stop();if(m_player)disconnect(m_player,nullptr,this,nullptr);m_player=qobject_cast<QMediaPlayer*>(object);
    if(m_player){
        connect(m_player,&QMediaPlayer::sourceChanged,this,[this]{invalidate();publish();});
        connect(m_player,&QMediaPlayer::playbackStateChanged,this,&PlaybackSession::publish);
        connect(m_player,&QMediaPlayer::hasVideoChanged,this,&PlaybackSession::publish);
        connect(m_player,&QMediaPlayer::metaDataChanged,this,&PlaybackSession::publish);
        connect(m_player,&QMediaPlayer::durationChanged,this,&PlaybackSession::publish);
        connect(m_player,&QMediaPlayer::seekableChanged,this,&PlaybackSession::publish);
        connect(m_player,&QMediaPlayer::playbackRateChanged,this,&PlaybackSession::publish);
        connect(m_player,&QMediaPlayer::errorOccurred,this,[this]{m_wantsPlay=false;send("release");publish();});
    }publish();if(preserveOutgoing)play();
}
QVariantMap PlaybackSession::snapshot()const{
    const QSize resolution=m_player?m_player->metaData().value(QMediaMetaData::Resolution).toSize():QSize();
    return {{"playLabel",tr("Play")},{"pauseLabel",tr("Pause")},{"previousLabel",tr("Previous")},{"nextLabel",tr("Next")},{"stopLabel",tr("Stop")},{"video",m_player&&m_player->hasVideo()},{"pipEnabled",m_pipEnabled},{"width",resolution.width()},{"height",resolution.height()},{"title",m_title.left(200)},{"artist",m_artist.left(200)},{"music",m_music},{"previous",m_previous},{"next",m_next},
        {"valid",m_player&&!m_player->source().isEmpty()},{"playing",m_player&&m_player->playbackState()==QMediaPlayer::PlayingState},
        {"position",m_player?m_player->position():0},{"duration",m_player?m_player->duration():0},
        {"seekable",m_player&&m_player->isSeekable()},{"speed",m_player?m_player->playbackRate():1.}};
}
void PlaybackSession::send(const QString &action){if(m_transport)m_transport(action,m_token,snapshot());}
void PlaybackSession::publish(){if(m_player&&m_player->playbackState()==QMediaPlayer::PlayingState)m_timer.start();else m_timer.stop();send("state");}
void PlaybackSession::metadata(const QString &title,const QString &artist,bool music,bool previous,bool next){m_title=title;m_artist=artist;m_music=music;m_previous=previous;m_next=next;publish();}
void PlaybackSession::invalidate(){m_wantsPlay=false;++m_token;send("cancel");}
void PlaybackSession::play(){
    if(!m_player||m_player->source().isEmpty())return;
    m_wantsPlay=true;++m_token;
    if(m_transport)send("prepare");else m_player->play();
}
void PlaybackSession::pause(){invalidate();if(m_player)m_player->pause();send("release");}
void PlaybackSession::stop(){invalidate();if(m_player)m_player->stop();send("release");}
void PlaybackSession::background(){send("background");}
bool PlaybackSession::pipSupported()const{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>("io/github/jatayuos4u/jatayumediaplayer/JatayuActivity","supportsPiP","()Z");
#else
    return false;
#endif
}
void PlaybackSession::setPiPEnabled(bool enabled){m_pipEnabled=enabled;publish();}
void PlaybackSession::enterPiP(){if(m_player&&m_player->hasVideo())send("pip");}
void PlaybackSession::restoreWindowBounds(QObject *object,int pixelWidth,int pixelHeight){
    auto window=qobject_cast<QWindow *>(object);
    if(m_pip||!window||pixelWidth<=0||pixelHeight<=0||pixelWidth>32768||pixelHeight>32768)return;
    const auto scale=window->devicePixelRatio();
    if(!std::isfinite(scale)||scale<=0)return;
    // Android's content parent, not QScreen's physical display, supplies bounds.
    // This also respects split-screen and the density of the current display.
    window->setGeometry(0,0,qRound(pixelWidth/scale),qRound(pixelHeight/scale));
    window->requestUpdate();
}
void PlaybackSession::handleAction(const QString &action,qint64 token){
    if(action=="pip-enter"||action=="pip-exit"){
        const bool value=action=="pip-enter";if(value!=m_pip){m_pip=value;emit pictureInPictureChanged();publish();}return;
    }
    if(action=="pip-restore"){
        const auto packed=quint64(token);const int width=int(packed>>32),height=int(packed&0xffffffffu);
        if(!m_pip&&width>0&&height>0&&width<=32768&&height<=32768)emit windowRestoreRequested(width,height);
        return;
    }
    if(!m_player)return;
    if(action=="video-hidden"){if(m_player->hasVideo())pause();return;}
    if(action=="ready"){if(token==m_token&&m_wantsPlay&&!m_player->source().isEmpty())m_player->play();}
    else if(action=="play")play();
    else if(action=="pause")pause();
    else if((action=="lost"||action=="serviceStopped")&&token==m_token)pause();
    else if(action=="interrupt"){if(token==m_token)m_player->pause();}
    else if(action=="stop")stop();
    else if(action=="next"&&m_next)emit nextRequested();
    else if(action=="previous"&&m_previous)emit previousRequested();
    else if(action=="seek"&&m_player->isSeekable())emit seekRequested(std::clamp<qint64>(token,0,m_player->duration()));
    else if(action=="denied"&&token==m_token){pause();emit errorOccurred(tr("Playback could not get audio focus. Close the other audio app and try again."));}
}
