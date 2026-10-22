#pragma once
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <functional>

class MediaStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QVariantList media READ media NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY preferencesChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanChanged)
public:
    using IdentityResolver = std::function<QVariantMap(const QUrl &)>;
    explicit MediaStore(QObject *parent = nullptr, const QString &storagePath = {}, IdentityResolver identityResolver = {});
    QString appVersion() const { return QStringLiteral(JATAYU_APP_VERSION); }
    QString qtVersion() const { return QString::fromLatin1(qVersion()); }
    QVariantList media() const { return m_media; }
    QVariantList playlists() const { return m_playlists; }
    QVariantMap preferences() const { return m_preferences; }
    bool scanning() const { return m_scanning; }
    QString scanStatus() const { return m_scanStatus; }
    Q_INVOKABLE void scanDevice(bool askPermission = true, bool changeAccess = false);
    Q_INVOKABLE void setPreference(const QString &name, const QVariant &value);
    Q_INVOKABLE bool supportsRotationLock() const;
    Q_INVOKABLE void configurePlayerWindow(qreal brightness, bool keepAwake, int orientation, bool immersive);
    Q_INVOKABLE void restoreRemovedMedia();
    Q_INVOKABLE bool clearPlaybackHistory();
    void acceptDeviceScan(const QString &payload);
    void mergeCatalog(const QVariantList &items, const QStringList &completeScopes = {}, const QVariantList &resolvedIdentities = {});
    Q_INVOKABLE QString importFiles(const QList<QUrl> &urls);
    Q_INVOKABLE QVariantMap entry(const QUrl &url) const;
    Q_INVOKABLE void updateProgress(const QUrl &url, qint64 position, qint64 duration);
    Q_INVOKABLE void toggleFavorite(const QUrl &url);
    Q_INVOKABLE QString createPlaylist(const QString &name);
    Q_INVOKABLE QString addToPlaylist(const QString &id, const QUrl &url);
    Q_INVOKABLE QVariantMap addToPlaylistBatch(const QString &id, const QList<QUrl> &urls, bool importMissing = false);
    Q_INVOKABLE void removePlaylist(const QString &id);
    Q_INVOKABLE QVariantList playlistItems(const QString &id) const;
    Q_INVOKABLE QVariantList bookmarks(const QUrl &url) const;
    Q_INVOKABLE void addBookmark(const QUrl &url, qint64 position, const QString &note);
    Q_INVOKABLE void updateBookmark(const QUrl &url, const QString &id, const QString &note);
    Q_INVOKABLE QVariantList readSubtitles(const QUrl &url);
    Q_INVOKABLE void removeBookmark(const QUrl &url, const QString &id);
    Q_INVOKABLE bool exportNotes(const QUrl &url, const QUrl &destination);
    Q_INVOKABLE void forget(const QUrl &url);
signals:
    void preferencesChanged();
    void scanChanged();
    void libraryChanged();
    void playlistsChanged();
    void bookmarksChanged();
    void errorOccurred(const QString &message);
private:
    int indexOf(const QUrl &url) const;
    QString storageKey(const QUrl &url) const;
    bool coalesceLibrary();
    bool save();
    void load();
    QString m_storagePath;
    IdentityResolver m_identityResolver;
    bool m_storageBlocked = false;
    QVariantList m_media;
    QVariantList m_playlists;
    QVariantMap m_bookmarks;
    QVariantMap m_preferences{{"pipAutomatic", true}, {"crossfade", 0}, {"language", "en"}, {"volume", 1.0}, {"resume", true}, {"autoAdvance", true}, {"gestures", true},
        {"subtitles", true}, {"autoHide", true}, {"keepAwake", true}, {"scanOnOpen", true},
        {"visualMode", "bars"}, {"studyVisuals", false}, {"studySkip", 5}, {"subtitleSize", 22}, {"defaultSpeed", 1.0}, {"hideDelay", 4}, {"seekStep", 10}, {"sort", "recent"}};
    QStringList m_removedMedia;
    bool m_scanning = false;
    QString m_scanStatus;
};
