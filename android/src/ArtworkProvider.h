#pragma once
#include <QQuickImageProvider>
class ArtworkProvider : public QQuickImageProvider {
public:
    ArtworkProvider();
    static QImage decodeArtwork(const QByteArray &data);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
