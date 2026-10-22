#pragma once
#include <QDir>
#include <QUrl>

namespace SecurityPolicy {
constexpr qint64 libraryBytes = 16 * 1024 * 1024;
constexpr qsizetype libraryEntries = 50000;
constexpr qint64 artworkBytes = 8 * 1024 * 1024;
constexpr qint64 artworkPixels = 16 * 1024 * 1024;
inline bool localMediaUrl(const QUrl &url) {
    if (!url.isValid() || url.isEmpty() || url.isRelative() || url.toEncoded().size() > 8192
        || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()) return false;
    if (url.scheme() == QStringLiteral("content")) return !url.host().isEmpty() && url.path().startsWith('/');
    return url.isLocalFile() && (url.host().isEmpty() || url.host() == QStringLiteral("localhost"))
        && QDir::isAbsolutePath(url.toLocalFile());
}
}
