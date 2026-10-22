#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QMediaPlayer>

class MediaDecoderSafetyTest : public QObject {
    Q_OBJECT
private slots:
    void malformedLocalMediaFailsCleanly() {
        QTemporaryDir dir;
        const QList<QPair<QString, QByteArray>> cases{
            {"truncated.mp4", QByteArray::fromHex("000000186674797069736f6d0000")},
            {"truncated.wav", QByteArray("RIFF\xff\xff\xff\xffWAVEfmt ", 16)},
            {"invalid.mp3", QByteArray(4096, '\x7f')},
            {"empty.mkv", QByteArray{}}
        };
        for (const auto &item : cases) {
            QFile file(dir.filePath(item.first)); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(item.second); file.close();
            QMediaPlayer player; player.setSource(QUrl::fromLocalFile(file.fileName())); player.play();
            QTRY_VERIFY_WITH_TIMEOUT(player.error() != QMediaPlayer::NoError, 5000);
            player.stop();
        }
    }
};
QTEST_GUILESS_MAIN(MediaDecoderSafetyTest)
#include "MediaDecoderSafetyTest.moc"
