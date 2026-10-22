#include "ArtworkProvider.h"
#include "SecurityPolicy.h"
#include <QBuffer>
#include <QImageReader>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QStandardPaths>
#include <QUrl>
#ifdef Q_OS_ANDROID
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

ArtworkProvider::ArtworkProvider() : QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading) {
    QImageReader::setAllocationLimit(32);
}
QImage ArtworkProvider::decodeArtwork(const QByteArray &data) {
    if (data.isEmpty() || data.size() > SecurityPolicy::artworkBytes) return {};
    QBuffer buffer; buffer.setData(data); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto format = reader.format();
    if (format != "jpeg" && format != "png" && format != "webp") return {};
    const auto dimensions = reader.size();
    if (!dimensions.isValid() || qint64(dimensions.width()) * dimensions.height() > SecurityPolicy::artworkPixels) return {};
    reader.setAutoTransform(true);
    if (dimensions.width() > 480 || dimensions.height() > 480)
        reader.setScaledSize(dimensions.scaled(480, 480, Qt::KeepAspectRatio));
    return reader.read();
}
QImage ArtworkProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    const auto source = QUrl(QUrl::fromPercentEncoding(id.toUtf8()));
    const auto folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/artwork";
    QDir().mkpath(folder);
    const auto path = folder + '/' + QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".png";
    QImage image;
    QFile cached(path);
    if (cached.open(QIODevice::ReadOnly)) image = decodeArtwork(cached.read(SecurityPolicy::artworkBytes + 1));
#ifdef Q_OS_ANDROID
    if (image.isNull() && SecurityPolicy::localMediaUrl(source)) {
        QJniEnvironment env;
        QJniObject retriever("android/media/MediaMetadataRetriever");
        const auto uri = QJniObject::callStaticObjectMethod("android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;", QJniObject::fromString(source.toString()).object());
        const auto context = QNativeInterface::QAndroidApplication::context();
        retriever.callMethod<void>("setDataSource", "(Landroid/content/Context;Landroid/net/Uri;)V", context.object(), uri.object());
        bool ok = !env->ExceptionCheck(); if (!ok) env->ExceptionClear();
        if (ok) {
            auto bytes = retriever.callObjectMethod("getEmbeddedPicture", "()[B");
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (!bytes.isValid()) {
                auto bitmap = retriever.callObjectMethod("getScaledFrameAtTime", "(JIII)Landroid/graphics/Bitmap;", jlong(1000000), jint(2), jint(480), jint(300));
                if (env->ExceptionCheck()) env->ExceptionClear();
                if (bitmap.isValid()) {
                    QJniObject stream("java/io/ByteArrayOutputStream");
                    const auto format = QJniObject::getStaticObjectField("android/graphics/Bitmap$CompressFormat", "PNG", "Landroid/graphics/Bitmap$CompressFormat;");
                    bitmap.callMethod<jboolean>("compress", "(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/OutputStream;)Z", format.object(), jint(90), stream.object());
                    bytes = stream.callObjectMethod("toByteArray", "()[B");
                    bitmap.callMethod<void>("recycle", "()V");
                }
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (bytes.isValid()) {
                const auto array = bytes.object<jbyteArray>(); const auto length = env->GetArrayLength(array);
                if (length > 0 && length <= SecurityPolicy::artworkBytes) { QByteArray data(length, Qt::Uninitialized); env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte *>(data.data())); image = decodeArtwork(data); }
            }
        }
        retriever.callMethod<void>("release", "()V");
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!image.isNull()) {
            image = image.scaled(480, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            image.save(path);
            // Bound this disposable cache, without touching source media.
            QDir dir(folder); const auto files = dir.entryInfoList({"*.png"}, QDir::Files, QDir::Time);
            qint64 total = 0; for (const auto &file : files) { total += file.size(); if (total > 96 * 1024 * 1024) QFile::remove(file.absoluteFilePath()); }
        }
    }
#endif
    if (image.isNull()) {
        image = QImage(480, 300, QImage::Format_RGB32);
        QPainter painter(&image); QLinearGradient gradient(0, 0, 480, 300);
        gradient.setColorAt(0, QColor("#3f3328")); gradient.setColorAt(1, QColor("#172c30")); painter.fillRect(image.rect(), gradient);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#ff9328"), 9, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 7; ++i) { const int h = 24 + (i * 31 % 85); painter.drawLine(168 + i * 24, 150 - h / 2, 168 + i * 24, 150 + h / 2); }
    }
    if (size) *size = image.size();
    return requestedSize.isValid() ? image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image;
}
