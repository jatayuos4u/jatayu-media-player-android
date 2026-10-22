#include <QtTest>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTemporaryDir>
#include "PlaybackSession.h"
class PlaybackSessionTest:public QObject {
    Q_OBJECT
private slots:
    void focusGateAndStaleEvents(){
        const auto path=qEnvironmentVariable("JATAYU_TEST_MP3");QVERIFY(!path.isEmpty());
        QMediaPlayer player;QAudioOutput audio;player.setAudioOutput(&audio);
        QString lastAction;qint64 token=0;QVariantMap snapshot;
        PlaybackSession session(nullptr,[&](const QString &a,qint64 t,const QVariantMap &s){lastAction=a;token=t;snapshot=s;});
        session.attach(&player);player.setSource(QUrl::fromLocalFile(path));session.metadata("Song","Artist",true,true,true);
        session.play();const auto first=token;QCOMPARE(lastAction,QString("prepare"));QCOMPARE(player.playbackState(),QMediaPlayer::StoppedState);
        QCOMPARE(snapshot["title"].toString(),QString("Song"));QVERIFY(snapshot["music"].toBool());
        session.pause();session.handleAction("ready",first);QCOMPARE(player.playbackState(),QMediaPlayer::StoppedState);
        session.play();const auto accepted=token;session.handleAction("ready",accepted);
        QTRY_COMPARE(player.playbackState(),QMediaPlayer::PlayingState);
        session.handleAction("interrupt",accepted);QCOMPARE(player.playbackState(),QMediaPlayer::PausedState);
        session.handleAction("ready",accepted);QCOMPARE(player.playbackState(),QMediaPlayer::PlayingState);
        session.pause();session.handleAction("ready",accepted);QCOMPARE(player.playbackState(),QMediaPlayer::PausedState);
        session.play();const auto second=token;session.handleAction("ready",second);QCOMPARE(player.playbackState(),QMediaPlayer::PlayingState);
        session.handleAction("lost",accepted);QCOMPARE(player.playbackState(),QMediaPlayer::PlayingState);
        session.handleAction("serviceStopped",accepted);QCOMPARE(player.playbackState(),QMediaPlayer::PlayingState);
        session.handleAction("lost",second);QCOMPARE(player.playbackState(),QMediaPlayer::PausedState);
        session.handleAction("ready",second);QCOMPARE(player.playbackState(),QMediaPlayer::PausedState);
        QSignalSpy errors(&session,&PlaybackSession::errorOccurred);session.play();session.handleAction("denied",token);QCOMPARE(errors.size(),1);
    }
    void hiddenVideoPausesButMusicContinues(){
        const auto path=qEnvironmentVariable("JATAYU_TEST_VIDEO");if(path.isEmpty())QSKIP("Supply video fixture");
        QMediaPlayer player;QAudioOutput audio;player.setAudioOutput(&audio);PlaybackSession session;
        session.attach(&player);session.metadata("Video","",false,false,false);player.setSource(QUrl::fromLocalFile(path));session.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasVideo(),10000);session.handleAction("pip-enter",0);QVERIFY(session.pictureInPicture());
        QCOMPARE(player.playbackState(),QMediaPlayer::PlayingState);session.handleAction("video-hidden",0);QCOMPARE(player.playbackState(),QMediaPlayer::PausedState);
        session.handleAction("pip-exit",0);QVERIFY(!session.pictureInPicture());
        player.setSource(QUrl::fromLocalFile(qEnvironmentVariable("JATAYU_TEST_MP3")));session.metadata("Song","",true,false,false);session.play();QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio()&&!player.hasVideo()&&player.playbackState()==QMediaPlayer::PlayingState,10000);session.handleAction("video-hidden",0);QCOMPARE(player.playbackState(),QMediaPlayer::PlayingState);session.pause();
    }
    void transportActions(){
        QMediaPlayer player;qint64 token=0;PlaybackSession session(nullptr,[&](const QString &,qint64 t,const QVariantMap &){token=t;});
        session.attach(&player);QSignalSpy next(&session,&PlaybackSession::nextRequested),previous(&session,&PlaybackSession::previousRequested);
        session.metadata("Song","Artist",true,false,false);session.handleAction("next",0);QCOMPARE(next.size(),0);
        session.metadata("Song","Artist",true,true,true);session.handleAction("next",0);session.handleAction("previous",0);QCOMPARE(next.size(),1);QCOMPARE(previous.size(),1);
        const auto before=token;player.setSource(QUrl("file:///not-present.mp3"));QVERIFY(token>before);
    }
};
QTEST_GUILESS_MAIN(PlaybackSessionTest)
#include "PlaybackSessionTest.moc"
