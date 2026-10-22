#include <QtTest>
#include <QTemporaryDir>
#include <QAudioBuffer>
#include "PlaybackDecks.h"
#include "PlaybackSession.h"
class PlaybackDecksTest:public QObject {
 Q_OBJECT
private slots:
 void overlapAndFocusCancellation(){
    const auto first=qEnvironmentVariable("JATAYU_TEST_MP3"),second=qEnvironmentVariable("JATAYU_TEST_CROSSFADE_MP3");
    if(first.isEmpty()||second.isEmpty())QSKIP("Supply two different tone fixtures and an audio output");
    QTemporaryDir dir;PlaybackDecks decks(nullptr,{},dir.filePath("studio.ini"));
    PlaybackSession session;session.attach(decks.player());
    connect(&decks,&PlaybackDecks::activeChanged,&session,[&]{session.attach(decks.player(),decks.fading());});
    decks.studio()->setEnabled(true);decks.studio()->setMusic(true);decks.studio()->applyPreset("Bollywood");decks.setVolume(.7);decks.setCrossfade(3);
    auto outgoing=decks.player();QSignalSpy started(&decks,&PlaybackDecks::crossfadeStarted);
    QVERIFY(decks.open(outgoing,QUrl::fromLocalFile(first)));session.play();
    QTRY_VERIFY_WITH_TIMEOUT(outgoing->duration()>10000&&outgoing->position()>300,10000);
    decks.stageNext(QUrl::fromLocalFile(second));
    // Decoders load asynchronously. Move into the transition window only once
    // both have had time to read metadata.
    QTest::qWait(600);decks.studio()->prepareSeek(outgoing->duration()-4200);outgoing->setPosition(outgoing->duration()-4200);
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),1,9000);
    auto incoming=decks.player();QVERIFY(incoming!=outgoing);QVERIFY(decks.fading());
    QCOMPARE(incoming->playbackState(),QMediaPlayer::PlayingState);QCOMPARE(outgoing->playbackState(),QMediaPlayer::PlayingState);
    QCOMPARE(decks.studio()->preset(),QString("Bollywood"));QVERIFY(decks.studio()->enabled());
    QTest::qWait(550);QVERIFY(decks.incomingGain()>0&&decks.incomingGain()<1);QVERIFY(decks.outgoingGain()>0&&decks.outgoingGain()<1);
    QVERIFY(qAbs(decks.incomingGain()+decks.outgoingGain()-1)<.0001);
    session.pause();QTRY_VERIFY(!decks.fading());QCOMPARE(incoming->playbackState(),QMediaPlayer::PausedState);QCOMPARE(outgoing->playbackState(),QMediaPlayer::StoppedState);
    QCOMPARE(decks.incomingGain(),1.);QCOMPARE(decks.outgoingGain(),0.);
    // A second full transition drains the outgoing decoder without stopping
    // the incoming track or resetting its clock.
    decks.setCrossfade(2);session.play();decks.stageNext(QUrl::fromLocalFile(first));QTest::qWait(500);
    decks.studio()->prepareSeek(incoming->duration()-3300);incoming->setPosition(incoming->duration()-3300);
    QTRY_COMPARE_WITH_TIMEOUT(started.size(),2,8000);QTRY_VERIFY_WITH_TIMEOUT(!decks.fading(),5000);
    QVERIFY(decks.player()!=incoming);QCOMPARE(decks.player()->playbackState(),QMediaPlayer::PlayingState);QVERIFY(decks.player()->position()>1500);
    QCOMPARE(incoming->playbackState(),QMediaPlayer::StoppedState);
    session.pause();
 }
 void noOverlapWhenOffOrVideo(){
    QTemporaryDir dir;PlaybackDecks decks(nullptr,{},dir.filePath("studio.ini"));
    decks.setCrossfade(0);decks.stageNext(QUrl::fromLocalFile("/not-present.mp3"));QVERIFY(!decks.fading());
    decks.setCrossfade(3);decks.setVolume(.4);decks.cancelTransition();QCOMPARE(decks.incomingGain(),1.);QVERIFY(!decks.fading());
 }
};
QTEST_MAIN(PlaybackDecksTest)
#include "PlaybackDecksTest.moc"
