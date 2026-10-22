#include <QtTest>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QFile>
#include <QTemporaryDir>
#include <fcntl.h>
#include <unistd.h>
#include "MediaSource.h"
class MediaSourceTest : public QObject {
    Q_OBJECT
private slots:
    void descriptorRemainsReadableAndCloses() {
        QTemporaryDir dir;QFile file(dir.filePath("sample.bin"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("0123456789");file.close();
        int fd=-1;
        MediaSource source(nullptr,[&](const QUrl &){fd=::open(QFile::encodeName(file.fileName()).constData(),O_RDONLY);return fd;});
        QMediaPlayer player;const QUrl uri("content://media/external/audio/media/7");
        QVERIFY(source.open(&player,uri));QCOMPARE(player.source(),uri);
        auto device=player.sourceDevice();QVERIFY(device);QVERIFY(device->isOpen());QVERIFY(device->isReadable());QVERIFY(!device->isSequential());
        QVERIFY(::fcntl(fd,F_GETFD)!=-1);
        player.setSource(QUrl());QVERIFY(!player.sourceDevice());QCOMPARE(::fcntl(fd,F_GETFD),-1);
    }
    void deniedAndMissingNeverStartPlayback() {
        QMediaPlayer player;
        for(int status:{-1,-2,-3}) {
            MediaSource source(nullptr,[status](const QUrl &){return status;});
            QVERIFY(!source.open(&player,QUrl("content://media/external/video/media/7")));
            QVERIFY(!source.error().isEmpty());QVERIFY(player.source().isEmpty());QVERIFY(!player.sourceDevice());
        }
        int attempts=0;MediaSource source(nullptr,[&](const QUrl &){++attempts;return -1;});
        QVERIFY(!source.open(&player,QUrl("https://example.com/video.mp4")));QCOMPARE(attempts,0);
    }
    void decodesAndSeeksRealMediaThroughProviderDescriptor_data() {
        QTest::addColumn<QString>("path");QTest::addColumn<bool>("video");
        QTest::newRow("MP3")<<qEnvironmentVariable("JATAYU_TEST_MP3")<<false;
        QTest::newRow("MP4")<<qEnvironmentVariable("JATAYU_TEST_VIDEO")<<true;
    }
    void decodesAndSeeksRealMediaThroughProviderDescriptor() {
        QFETCH(QString,path);QFETCH(bool,video);if(path.isEmpty())QSKIP("Supply generated MP3/MP4 fixtures");
        QMediaPlayer player;QVideoSink sink;player.setVideoSink(&sink);
        int fd=-1;MediaSource source(nullptr,[&](const QUrl &){fd=::open(QFile::encodeName(path).constData(),O_RDONLY);return fd;});
        const QUrl uri(video?"content://media/external/video/media/81":"content://com.android.providers.media.documents/document/audio%3A80");
        QVERIFY(source.open(&player,uri));player.play();
        QTRY_VERIFY_WITH_TIMEOUT(player.duration()>0,10000);QCOMPARE(player.error(),QMediaPlayer::NoError);
        QTRY_VERIFY_WITH_TIMEOUT(player.hasAudio(),10000);QTRY_VERIFY(player.isSeekable());
        if(video)QTRY_VERIFY_WITH_TIMEOUT(sink.videoFrame().isValid(),10000);
        player.setPosition(1200);QTRY_VERIFY_WITH_TIMEOUT(player.position()>=1100,5000);
        player.pause();QVERIFY(::fcntl(fd,F_GETFD)!=-1);player.play();
        player.setSource(QUrl());QCOMPARE(::fcntl(fd,F_GETFD),-1);
    }
};
QTEST_GUILESS_MAIN(MediaSourceTest)
#include "MediaSourceTest.moc"
