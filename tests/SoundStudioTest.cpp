#include <QtTest>
#include <QTemporaryDir>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "SoundStudio.h"
#include "AudioVisualizer.h"
#include <cmath>
class SoundStudioTest:public QObject {
    Q_OBJECT
private slots:
    void settingsPersist(){
        QTemporaryDir dir;const QString path=dir.filePath("studio.ini");QString saved;
        {SoundStudio s(nullptr,path);QVERIFY(!s.enabled());s.setEnabled(true);s.setBand(5,99);QCOMPARE(s.bands()[5].toDouble(),9.);s.setBass(-99);QCOMPARE(s.bass(),-6.);
        s.setStrength(72);QVERIFY(s.saveProfile("Evening headphones"));saved=s.preset();s.applyPreset("Ghazal");QCOMPARE(s.vocals(),1.8);
        s.applyPreset(saved);QCOMPARE(s.bands()[5].toDouble(),9.);QCOMPARE(s.bass(),-6.);QCOMPARE(s.strength(),72.);s.setOriginal(true);s.setLevelMatch(false);}
        SoundStudio s(nullptr,path);QVERIFY(s.enabled());QVERIFY(!s.original());QVERIFY(!s.levelMatch());QCOMPARE(s.preset(),saved);QCOMPARE(s.strength(),72.);
        QCOMPARE(s.bands()[5].toDouble(),9.);s.applyPreset("Flat");for(auto g:s.bands())QCOMPARE(g.toDouble(),0.);
    }
    void completeProfiles(){
        QTemporaryDir dir;SoundStudio s(nullptr,dir.filePath("studio.ini"));QSet<QString> curves;
        for(const auto &v:s.profiles()){
            const auto id=v.toMap()["id"].toString();s.applyPreset(id);QCOMPARE(s.preset(),id);
            QCOMPARE(s.strength(),100.);QCOMPARE(s.response().size(),96);
            QString curve;for(const auto &x:s.response())curve+=QString::number(x.toDouble(),'f',4)+",";
            QVERIFY(!curves.contains(curve));curves.insert(curve);
            if(id!="Flat")QVERIFY(s.bass()!=0||s.vocals()!=0||s.treble()!=0);
            const auto response=s.response();s.setStrength(0);for(const auto &x:s.response())QVERIFY(std::abs(x.toDouble())<1e-9);
            QCOMPARE(s.headroom(),0.);s.setStrength(100);QCOMPARE(s.response(),response);
            const auto bass=s.bass();s.setBass(5);QCOMPARE(s.preset(),QString("Custom"));s.resetPreset();QCOMPARE(s.preset(),id);QCOMPARE(s.bass(),bass);
        }
        QVERIFY(!s.saveProfile("   "));s.applyPreset("Dance");s.setStrength(35);QVERIFY(s.saveProfile("My profile"));const auto id=s.preset();
        s.setTreble(-3);s.resetPreset();QCOMPARE(s.strength(),35.);QCOMPARE(s.treble(),1.5);
        s.removeProfile("Dance");QCOMPARE(s.preset(),id);const auto curve=s.response();s.removeProfile(id);QCOMPARE(s.preset(),QString("Custom"));QCOMPARE(s.response(),curve);
    }
    void legacySavedMigration(){
        QTemporaryDir dir;const auto path=dir.filePath("studio.ini");
        {QSettings old(path,QSettings::IniFormat);old.setValue("preset","Saved");old.setValue("savedBands",QVariantList{0,1,2,3,4,5,6,7,8,9});old.setValue("savedBass",2);}
        SoundStudio s(nullptr,path);QCOMPARE(s.preset(),QString("personal:legacy"));s.applyPreset("Saved");QCOMPARE(s.bass(),2.);QCOMPARE(s.bands()[9].toDouble(),9.);
    }
    void upgradePreservesEditsAndRemovesLegacy(){
        QTemporaryDir dir;const auto path=dir.filePath("studio.ini");
        {QSettings old(path,QSettings::IniFormat);old.setValue("preset","Bollywood");old.setValue("savedBands",QVariantList{0,1,2,3,4,5,6,7,8,9});}
        {SoundStudio s(nullptr,path);QCOMPARE(s.bass(),2.);QCOMPARE(s.vocals(),1.2);s.removeProfile("personal:legacy");s.setBass(4);}
        {SoundStudio s(nullptr,path);QCOMPARE(s.preset(),QString("Custom"));QCOMPARE(s.bass(),4.);QCOMPARE(s.profiles().size(),8);}
    }
    void routeOwnership(){
        QTemporaryDir dir;QMediaPlayer player;QAudioOutput output;player.setAudioOutput(&output);
        AudioVisualizer v;v.attach(&player);SoundStudio s(nullptr,dir.filePath("studio.ini"));s.attach(&player,&v);
        s.setEnabled(true);QVERIFY(!s.routing());s.setMusic(true);QVERIFY(s.routing());QCOMPARE(player.audioOutput(),&output);QVERIFY(output.isMuted());QVERIFY(player.audioBufferOutput());QVERIFY(!v.enabled());
        player.setPlaybackRate(1.25);QVERIFY(!s.routing());QCOMPARE(player.audioOutput(),&output);QVERIFY(!output.isMuted());QVERIFY(!player.audioBufferOutput());
        player.setPlaybackRate(1);QVERIFY(s.routing());s.setOriginal(true);QVERIFY(s.routing());s.setMusic(false);QVERIFY(!s.routing());QCOMPARE(player.audioOutput(),&output);
        QSignalSpy finished(&s,&SoundStudio::playbackFinished);s.prepareSeek(90000);s.finish();QCOMPARE(finished.size(),1);
        s.setEnabled(false);s.setMusic(true);s.prepareSeek(90000);s.setEnabled(true);QVERIFY(s.routing());s.setEnabled(false);QCOMPARE(player.audioOutput(),&output);
    }
    void guiStallDoesNotDisableStudio(){
        if(!qEnvironmentVariableIsSet("JATAYU_TEST_AUDIO_OUTPUT"))QSKIP("Run with a real or virtual audio output");
        const auto path=qEnvironmentVariable("JATAYU_TEST_MP3");QVERIFY(!path.isEmpty());
        QTemporaryDir dir;QMediaPlayer player;QAudioOutput output;player.setAudioOutput(&output);
        AudioVisualizer visualizer;visualizer.attach(&player);SoundStudio studio(nullptr,dir.filePath("studio.ini"));
        studio.attach(&player,&visualizer);studio.setMusic(true);studio.setEnabled(true);
        player.setSource(QUrl::fromLocalFile(path));player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.position()>700,10000);
        // A popup, file scan or expensive layout must not interrupt the audio worker.
        QThread::msleep(2800);
        QTest::qWait(350);
        QVERIFY2(studio.routing(),qPrintable(studio.status()));QVERIFY(output.isMuted());
        QTRY_VERIFY_WITH_TIMEOUT(player.position()>4000,3000);
    }
    void steadyPresetStress(){
        if(!qEnvironmentVariableIsSet("JATAYU_TEST_AUDIO_OUTPUT"))QSKIP("Run with a real or virtual audio output");
        const auto path=qEnvironmentVariable("JATAYU_TEST_MP3");QVERIFY(!path.isEmpty());
        QTemporaryDir dir;QMediaPlayer player;QAudioOutput output;player.setAudioOutput(&output);
        AudioVisualizer visualizer;visualizer.attach(&player);SoundStudio studio(nullptr,dir.filePath("studio.ini"));
        studio.attach(&player,&visualizer);studio.setMusic(true);studio.setEnabled(true);player.setSource(QUrl::fromLocalFile(path));player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio()&&player.position()>300,10000);
        const QStringList presets{"Flat","Balanced","Bollywood","Classical","Ghazal","Dance","Acoustic","Speech"};
        for(int i=0;i<24;++i){
            studio.applyPreset(presets[i%presets.size()]);studio.setStrength(i%3==0?50:100);QTest::qWait(650);QThread::msleep(120);QTest::qWait(50);
            QVERIFY2(studio.routing(),qPrintable(studio.status()));
        }
        // Position notifications are asynchronous, especially on a virtual sink.
        // Keep the progress threshold, but give the final notification a bounded wait.
        QTRY_VERIFY_WITH_TIMEOUT(player.position()>18000,4000);
        QVERIFY2(studio.routing(),qPrintable(studio.status()));QVERIFY(output.isMuted());
    }
    void sustainedEffectsWithRealSink(){
        if(!qEnvironmentVariableIsSet("JATAYU_TEST_AUDIO_OUTPUT"))QSKIP("Run with a real or virtual audio output");
        const auto path=qEnvironmentVariable("JATAYU_TEST_MP3");QVERIFY(!path.isEmpty());
        QTemporaryDir dir;QMediaPlayer player;QAudioOutput output;player.setAudioOutput(&output);
        AudioVisualizer visualizer;visualizer.attach(&player);SoundStudio studio(nullptr,dir.filePath("studio.ini"));
        studio.attach(&player,&visualizer);studio.setMusic(true);studio.setEnabled(true);player.setSource(QUrl::fromLocalFile(path));player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio()&&player.position()>300,10000);
        for(const auto &preset:{"Flat","Bollywood","Classical","Ghazal","Dance","Acoustic","Flat"}){
            studio.applyPreset(preset);QTest::qWait(650);
            QThread::msleep(120); // Simulate a busy popup / layout on the GUI thread.
            QTest::qWait(100);QVERIFY2(studio.routing(),qPrintable(studio.status()));QVERIFY(output.isMuted());
        }
        studio.setBass(4);studio.setVocals(3);studio.setTreble(2);QTest::qWait(500);QVERIFY(studio.routing());
        studio.prepareSeek(15000);player.setPosition(15000);QTest::qWait(700);QVERIFY2(studio.routing(),qPrintable(studio.status()));
        player.pause();QTest::qWait(150);player.play();QTest::qWait(500);QVERIFY(studio.routing());
        studio.setOriginal(true);QTest::qWait(300);studio.setOriginal(false);QTest::qWait(300);QVERIFY(studio.routing());
        studio.setEnabled(false);QVERIFY(!output.isMuted());QCOMPARE(player.audioOutput(),&output);
    }
    void sourceFormats_data(){
        QTest::addColumn<QString>("name");
        for(const auto &name:{"mono.m4a","stereo.flac","stereo.mp3"})QTest::newRow(name)<<QString(name);
    }
    void sourceFormats(){
        if(!qEnvironmentVariableIsSet("JATAYU_TEST_AUDIO_OUTPUT"))QSKIP("Run with virtual/real audio output");
        const auto fixtures=qEnvironmentVariable("JATAYU_TEST_AUDIO_FIXTURES");if(fixtures.isEmpty())QSKIP("Supply AAC/FLAC/MP3 fixtures");
        QFETCH(QString,name);QTemporaryDir dir;QMediaPlayer player;QAudioOutput output;player.setAudioOutput(&output);
        AudioVisualizer visualizer;visualizer.attach(&player);SoundStudio studio(nullptr,dir.filePath("studio.ini"));
        studio.attach(&player,&visualizer);studio.setMusic(true);studio.setEnabled(true);player.setSource(QUrl::fromLocalFile(fixtures+"/"+name));player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio()&&player.position()>300,10000);
        for(const auto &profile:studio.profiles()){
            studio.applyPreset(profile.toMap()["id"].toString());QTest::qWait(160);QVERIFY2(studio.routing(),qPrintable(studio.status()));
        }
        QVERIFY(player.position()>1000);QCOMPARE(player.error(),QMediaPlayer::NoError);
    }
    void decoderTimeoutFallback(){
        const auto path=qEnvironmentVariable("JATAYU_TEST_MP3");if(path.isEmpty())QSKIP("Set JATAYU_TEST_MP3 for real decoder test");
        QTemporaryDir dir;QMediaPlayer player;QAudioOutput output;player.setAudioOutput(&output);AudioVisualizer v;v.attach(&player);SoundStudio s(nullptr,dir.filePath("studio.ini"));s.attach(&player,&v);s.setMusic(true);s.setEnabled(true);
        v.setCaptureAudio(false); // Simulate a decoder that stops delivering PCM.
        player.setSource(QUrl::fromLocalFile(path));player.play();QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio(),5000);
        QTRY_VERIFY_WITH_TIMEOUT(!s.routing(),7000);QCOMPARE(player.audioOutput(),&output);QVERIFY(s.status().startsWith("Original sound"));
    }
};
QTEST_GUILESS_MAIN(SoundStudioTest)
#include "SoundStudioTest.moc"
