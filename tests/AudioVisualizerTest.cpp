#include <QtTest>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QMediaPlayer>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include "AudioVisualizer.h"

class AudioVisualizerTest : public QObject {
    Q_OBJECT
    static QAudioBuffer tone(double hz, bool opposite = false, bool rightOnly = false) {
        QAudioFormat format; format.setSampleRate(48000); format.setChannelCount(2); format.setSampleFormat(QAudioFormat::Int16);
        QByteArray pcm(4096 * 2 * sizeof(qint16), '\0');
        auto samples = reinterpret_cast<qint16 *>(pcm.data());
        for (int i = 0; i < 4096; ++i) {
            const auto sample = qint16(std::sin(2 * 3.141592653589793 * hz * i / 48000) * 24000);
            samples[i * 2] = rightOnly ? 0 : sample; samples[i * 2 + 1] = opposite ? -sample : sample;
        }
        return QAudioBuffer(pcm, format);
    }
    static int peak(const AudioVisualizer &visualizer) {
        const auto bands = visualizer.bands();
        return int(std::max_element(bands.begin(), bands.end(), [](const QVariant &a, const QVariant &b) { return a.toFloat() < b.toFloat(); }) - bands.begin());
    }
private slots:
    void frequencyAndSilence() {
        AudioVisualizer visualizer;
        visualizer.consume(tone(1000)); QVERIFY(!visualizer.available());
        visualizer.setEnabled(true);
        visualizer.consume(tone(1000));
        QTRY_VERIFY(visualizer.level() > 0.1);
        const int middle = peak(visualizer);
        const double low = 40 * std::pow(400.0, double(middle) / 32), high = 40 * std::pow(400.0, double(middle + 1) / 32);
        QVERIFY(low < 1030 && high > 970);
        visualizer.setEnabled(false); QCOMPARE(visualizer.level(), 0.0); QVERIFY(!visualizer.available());
        for (const auto &band : visualizer.bands()) QCOMPARE(band.toDouble(), 0.0);
        visualizer.setEnabled(true); visualizer.consume(tone(8000, true));
        QTRY_VERIFY(visualizer.level() > 0.1); QVERIFY(peak(visualizer) > middle + 7); // Opposite-phase stereo must not cancel.
        visualizer.setEnabled(false); visualizer.setEnabled(true); visualizer.consume(tone(1000, false, true));
        QTRY_VERIFY(visualizer.level() > 0.1); QCOMPARE(peak(visualizer), middle);
        visualizer.setEnabled(false); visualizer.setEnabled(true);
        visualizer.consume(tone(0)); QTest::qWait(80);
        QVERIFY(visualizer.available()); QCOMPARE(visualizer.level(), 0.0);
        for (const auto &band : visualizer.bands()) QCOMPARE(band.toDouble(), 0.0);
    }
    void playerAndOutputLifetimes() {
        AudioVisualizer visualizer;
        for (int round = 0; round < 4; ++round) {
            auto *player = new QMediaPlayer;
            visualizer.attach(player);
            for (int toggle = 0; toggle < 4; ++toggle) {
                visualizer.setEnabled(true); QVERIFY(player->audioBufferOutput());
                visualizer.setEnabled(false); QVERIFY(!player->audioBufferOutput());
            }
            visualizer.setEnabled(true); delete player;
            visualizer.attach(nullptr); visualizer.setEnabled(false);
        }
    }
    void realDecodedMp3AndDetach() {
        const auto path = qEnvironmentVariable("JATAYU_TEST_MP3");
        if (path.isEmpty()) QSKIP("Set JATAYU_TEST_MP3 to a 1 kHz MP3 test tone for decoder integration.");
        QVERIFY(QFile::exists(path));
        AudioVisualizer visualizer; QMediaPlayer player;
        visualizer.attach(&player); visualizer.setEnabled(true);
        QVERIFY(player.audioBufferOutput());
        player.setSource(QUrl::fromLocalFile(path)); player.play();
        QTRY_VERIFY_WITH_TIMEOUT(visualizer.available() && visualizer.level() > 0.1, 10000);
        QVERIFY(peak(visualizer) >= 14 && peak(visualizer) <= 18);
        player.pause(); QTRY_COMPARE(visualizer.level(), 0.0);
        for (const auto &band : visualizer.bands()) QCOMPARE(band.toDouble(), 0.0);
        player.play(); QTRY_VERIFY(visualizer.level() > 0.1);
        visualizer.setEnabled(false); QVERIFY(!player.audioBufferOutput()); QCOMPARE(visualizer.level(), 0.0);
        QCOMPARE(player.playbackState(), QMediaPlayer::PlayingState);
        visualizer.setEnabled(true); QTRY_VERIFY(visualizer.level() > 0.1);
        player.stop(); QTRY_COMPARE(visualizer.level(), 0.0);
    }
};
QTEST_GUILESS_MAIN(AudioVisualizerTest)
#include "AudioVisualizerTest.moc"
