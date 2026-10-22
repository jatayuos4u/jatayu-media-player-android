#include <QtTest>
#include <QBuffer>
#include <QImage>
#include "ArtworkProvider.h"
#include "SecurityPolicy.h"

class SecurityInputTest : public QObject {
    Q_OBJECT
private slots:
    void localSourcesOnly() {
        QVERIFY(SecurityPolicy::localMediaUrl(QUrl("file:///storage/emulated/0/Music/a%23b.mp3")));
        QVERIFY(SecurityPolicy::localMediaUrl(QUrl("content://media/external/audio/media/42")));
        for (const auto &value : {"https://example.org/a.mp4", "data:audio/mpeg;base64,AA==", "file://server/share/a.mp4", "content:///missing-authority", "content://user:pass@media/a", "file:///tmp/a?query", "file:///tmp/a#fragment", "relative.mp4"})
            QVERIFY2(!SecurityPolicy::localMediaUrl(QUrl(QString::fromLatin1(value))), value);
    }
    void artworkIsBoundedRaster() {
        ArtworkProvider provider;
        QImage original(960, 540, QImage::Format_RGB32); original.fill(Qt::blue);
        QByteArray encoded; QBuffer out(&encoded); QVERIFY(out.open(QIODevice::WriteOnly)); QVERIFY(original.save(&out, "PNG"));
        const auto decoded = ArtworkProvider::decodeArtwork(encoded);
        QCOMPARE(decoded.size(), QSize(480, 270));
        QVERIFY(ArtworkProvider::decodeArtwork(QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\"><image href=\"file:///secret\"/></svg>")).isNull());
        QVERIFY(ArtworkProvider::decodeArtwork(encoded.left(20)).isNull());
        QVERIFY(ArtworkProvider::decodeArtwork(QByteArray(SecurityPolicy::artworkBytes + 1, 'x')).isNull());
        // A valid but excessive raster must be rejected before decoding pixels.
        QImage excessive(5000, 4000, QImage::Format_Mono); excessive.fill(0);
        encoded.clear(); out.close(); QVERIFY(out.open(QIODevice::WriteOnly)); QVERIFY(excessive.save(&out, "PNG"));
        QVERIFY(ArtworkProvider::decodeArtwork(encoded).isNull());
    }
};
QTEST_MAIN(SecurityInputTest)
#include "SecurityInputTest.moc"
