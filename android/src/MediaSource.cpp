#include "MediaSource.h"
#include "SecurityPolicy.h"
#include <QMediaPlayer>
#include <QFile>
#include <utility>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace {
#ifdef Q_OS_ANDROID
int openAndroidDescriptor(const QUrl &url) {
    const auto text = QJniObject::fromString(url.toString(QUrl::FullyEncoded));
    const int fd = QJniObject::callStaticMethod<jint>(
        "io/github/jatayuos4u/jatayumediaplayer/JatayuActivity", "openMediaDescriptor",
        "(Ljava/lang/String;)I", text.object());
    QJniEnvironment env;
    if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
    return fd;
}
#endif
}
MediaSource::MediaSource(QObject *parent, DescriptorOpener opener)
    : QObject(parent), m_opener(std::move(opener)) {
#ifdef Q_OS_ANDROID
    if (!m_opener) m_opener = openAndroidDescriptor;
#endif
}
MediaSource::~MediaSource() { detach(); }
void MediaSource::setError(const QString &error) {
    if (m_error == error) return;
    m_error = error; emit errorChanged();
}
void MediaSource::detach() {
    m_changing = true;
    if (m_player) {
        if (m_file && m_player->sourceDevice() == m_file.get()) m_player->setProperty("source", QUrl());
        disconnect(m_player, nullptr, this, nullptr);
    }
    m_file.reset(); m_player.clear(); m_changing = false;
}
bool MediaSource::open(QObject *object, const QUrl &url) {
    auto *player = qobject_cast<QMediaPlayer *>(object);
    if (!player || !SecurityPolicy::localMediaUrl(url)) {
        setError(tr("Choose a local audio or video file.")); return false;
    }
    detach(); m_player = player;
    connect(player, &QMediaPlayer::sourceChanged, this, [this] {
        // QML also clears sources when forgetting media or clearing history.
        if (!m_changing && m_file && m_player && m_player->sourceDevice() != m_file.get())
            m_file.reset();
    });
    setError({});
    if (url.scheme() == QStringLiteral("content") && m_opener) {
        const int fd = m_opener(url);
        if (fd < 0) {
            player->setProperty("source", QUrl());
            setError(fd == -2 ? tr("Android denied access. Allow media access or select the file again using +.")
                : fd == -3 ? tr("This file is unavailable. Scan your library or select it again using +.")
                : tr("Android could not open this file. Select it again using +."));
            return false;
        }
        auto file = std::make_unique<QFile>();
        if (!file->open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
#ifdef Q_OS_UNIX
            ::close(fd);
#endif
            player->setProperty("source", QUrl());
            setError(tr("Could not read the selected media file.")); return false;
        }
        m_file = std::move(file);
        m_changing = true;
        // Qt Quick MediaPlayer keeps a distinct QML-facing source property.
        // Set it through the public meta-property before supplying the stream,
        // so source bindings, history and notes retain the original URI.
        player->setProperty("source", url);
        player->setSourceDevice(m_file.get(), url);
        m_changing = false;
    } else {
        player->setProperty("source", url);
    }
    return true;
}
