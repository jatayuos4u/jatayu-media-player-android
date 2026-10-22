#include "AudioVisualizer.h"
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QMediaPlayer>
#include <algorithm>
#include <cmath>
#include <complex>

namespace { constexpr double Pi = 3.14159265358979323846; }
AudioVisualizer::AudioVisualizer(QObject *parent) : QObject(parent) {
    for (int i = 0; i < Window; ++i) m_window[i] = float(0.5 - 0.5 * std::cos(2 * Pi * i / (Window - 1)));
    m_timer.setInterval(33); // Analysis and notifications capped at about 30 Hz.
    connect(&m_timer, &QTimer::timeout, this, &AudioVisualizer::updateSpectrum);
}
AudioVisualizer::~AudioVisualizer() {
    m_timer.stop(); releaseOutput();
}
QVariantList AudioVisualizer::bands() const {
    QVariantList values; values.reserve(BandCount);
    for (float value : m_bands) values.append(value);
    return values;
}
void AudioVisualizer::attach(QObject *object) {
    auto player = qobject_cast<QMediaPlayer *>(object);
    if (m_player == player && (player || !m_output)) return;
    releaseOutput();
    if (m_player) disconnect(m_player, nullptr, this, nullptr);
    m_player = player; reset();
    if (!player) return;
    connect(player, &QMediaPlayer::sourceChanged, this, &AudioVisualizer::reset);
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state != QMediaPlayer::PlayingState) reset();
        updateProcessing();
    });
    updateOutput(); updateProcessing();
}
void AudioVisualizer::setEnabled(bool enabled) {
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    updateOutput();
    if (!enabled) reset();
    updateProcessing(); emit enabledChanged();
}
void AudioVisualizer::setCaptureAudio(bool enabled) {
    if(m_captureAudio==enabled)return;
    m_captureAudio=enabled;updateOutput();
}
void AudioVisualizer::updateProcessing() {
    const bool active = m_enabled && (!m_player || m_player->playbackState() == QMediaPlayer::PlayingState);
    if (active == m_timer.isActive()) return;
    if (active) m_timer.start(); else m_timer.stop();
    emit processingChanged();
}
void AudioVisualizer::releaseOutput() {
    auto *output = m_output.data(); if (!output) return;
    m_output.clear();
    if (m_player && m_player->audioBufferOutput() == output) m_player->setAudioBufferOutput(nullptr);
    // Never reuse a detached output. Qt 6.8.3 leaves a raw player back-pointer
    // in it on detachment; dispose while that player is still alive.
    delete output;
}
void AudioVisualizer::updateOutput() {
    if ((!m_enabled && !m_captureAudio) || !m_player) { releaseOutput(); return; }
    if (m_output) return;
    auto *output = new QAudioBufferOutput(this); m_output = output;
    // Forward PCM on the decoder thread. Studio connects a bounded, thread-safe
    // ingress here; visualization still runs on the GUI thread below.
    connect(output, &QAudioBufferOutput::audioBufferReceived, this,
        &AudioVisualizer::decodedBuffer, Qt::DirectConnection);
    connect(output, &QAudioBufferOutput::audioBufferReceived, this,
        [this, guard = QPointer<QAudioBufferOutput>(output)](const QAudioBuffer &buffer) {
            if (guard && guard == m_output) { consume(buffer); }
        });
    m_player->setAudioBufferOutput(output);
}
void AudioVisualizer::reset() {
    m_samples.fill(0); m_bands.fill(0); m_write = m_count = m_sampleRate = 0;
    m_level = 0; m_available = m_dirty = false; m_lastBuffer.invalidate();
    emit spectrumChanged();
}
void AudioVisualizer::consume(const QAudioBuffer &buffer) {
    if (!m_enabled || (m_player && m_player->playbackState() != QMediaPlayer::PlayingState)) return;
    if (!buffer.isValid() || !buffer.frameCount()) { m_dirty = false; return; }
    const auto format = buffer.format();
    const int channels = format.channelCount(), bytes = format.bytesPerSample();
    if (channels < 1 || bytes < 1 || format.sampleRate() < 1 || channels > 32) return;
    if (m_sampleRate != format.sampleRate()) {
        m_samples.fill(0); m_write = m_count = 0; m_sampleRate = format.sampleRate();
    }
    const auto *data = buffer.constData<char>();
    // Keep bounded recent data and use the loudest channel so opposite-phase
    // stereo does not cancel, and a right-only recording remains visible.
    const auto start = std::max<qsizetype>(0, buffer.frameCount() - 4096);
    std::array<double, 32> energies{};
    for (qsizetype frame = start; frame < buffer.frameCount(); ++frame)
        for (int c = 0; c < channels; ++c) {
            const float sample = format.normalizedSampleValue(data + (qint64(frame) * channels + c) * bytes);
            if (std::isfinite(sample)) energies[c] += double(sample) * sample;
        }
    const int channel = int(std::max_element(energies.begin(), energies.begin() + channels) - energies.begin());
    for (qsizetype frame = start; frame < buffer.frameCount(); ++frame) {
        const float sample = format.normalizedSampleValue(data + (qint64(frame) * channels + channel) * bytes);
        m_samples[m_write] = std::isfinite(sample) ? std::clamp(sample, -1.f, 1.f) : 0.f;
        m_write = (m_write + 1) % Window; m_count = std::min(Window, m_count + 1);
    }
    const bool first = !m_available;
    m_available = m_dirty = true; m_lastBuffer.restart();
    if (first) emit spectrumChanged();
}
void AudioVisualizer::updateSpectrum() {
    if (!m_enabled) return;
    if (!m_dirty && m_lastBuffer.isValid() && m_lastBuffer.elapsed() < 200) return;
    const auto previousLevel = m_level;
    std::array<float, BandCount> target{};
    qreal level = 0;
    if (m_dirty && m_count == Window && m_sampleRate > 0 && m_lastBuffer.isValid() && m_lastBuffer.elapsed() < 200) {
        std::array<std::complex<double>, Window> fft;
        double energy = 0;
        for (int i = 0; i < Window; ++i) {
            const double sample = m_samples[(m_write + i) % Window]; energy += sample * sample;
            fft[i] = sample * m_window[i];
        }
        // Iterative radix-2 FFT; no extra dependency and no fabricated motion.
        for (int i = 1, j = 0; i < Window; ++i) {
            int bit = Window >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit; if (i < j) std::swap(fft[i], fft[j]);
        }
        for (int length = 2; length <= Window; length <<= 1) {
            const auto step = std::polar(1.0, -2 * Pi / length);
            for (int i = 0; i < Window; i += length) {
                std::complex<double> phase(1, 0);
                for (int j = 0; j < length / 2; ++j) {
                    const auto even = fft[i + j], odd = fft[i + j + length / 2] * phase;
                    fft[i + j] = even + odd; fft[i + j + length / 2] = even - odd; phase *= step;
                }
            }
        }
        const double upper = std::min(16000.0, m_sampleRate * 0.47);
        const double lower = std::min(40.0, upper / 2);
        for (int band = 0; band < BandCount; ++band) {
            const double low = lower * std::pow(upper / lower, double(band) / BandCount);
            const double high = lower * std::pow(upper / lower, double(band + 1) / BandCount);
            const int first = std::clamp(int(std::floor(low * Window / m_sampleRate)), 1, Window / 2 - 1);
            const int last = std::clamp(int(std::ceil(high * Window / m_sampleRate)), first + 1, Window / 2);
            double amplitude = 0;
            for (int bin = first; bin < last; ++bin) amplitude = std::max(amplitude, 4.0 * std::abs(fft[bin]) / Window);
            target[band] = float(std::clamp((20 * std::log10(std::max(amplitude, 1e-8)) + 72) / 72, 0.0, 1.0));
        }
        level = std::clamp(std::sqrt(energy / Window) * 1.8, 0.0, 1.0);
    }
    m_dirty = false;
    bool changed = false;
    for (int i = 0; i < BandCount; ++i) {
        const float value = m_bands[i] + (target[i] - m_bands[i]) * (target[i] > m_bands[i] ? 0.7f : 0.16f);
        const float clean = value < 0.003f ? 0.f : value;
        changed |= std::abs(clean - m_bands[i]) > 0.0001f; m_bands[i] = clean;
    }
    m_level += (level - m_level) * (level > m_level ? 0.7 : 0.16);
    if (m_level < 0.003) m_level = 0;
    // Availability notification also reaches the UI for silent audio buffers.
    if (changed || std::abs(previousLevel - m_level) > 0.0001) emit spectrumChanged();
}

