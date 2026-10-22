#include "MediaStore.h"
#include "SecurityPolicy.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <QMimeDatabase>
#include <QHash>
#include <QPointer>
#include <QCoreApplication>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QCoreApplication>
#include <QGuiApplication>
#endif

namespace {
QString key(const QUrl &url) { return url.toString(QUrl::FullyEncoded); }
QVariantMap resolveIdentity(const QUrl &url) {
#ifdef Q_OS_ANDROID
    const auto payload = QJniObject::callStaticObjectMethod("io/github/jatayuos4u/jatayumediaplayer/JatayuActivity", "identifyMedia", "(Ljava/lang/String;)Ljava/lang/String;", QJniObject::fromString(key(url)).object());
    QJniEnvironment env;
    if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
    return QJsonDocument::fromJson(payload.toString().toUtf8()).object().toVariantMap();
#else
    const auto path = url.isLocalFile() ? QFileInfo(url.toLocalFile()).canonicalFilePath() : QString{};
    return path.isEmpty() ? QVariantMap{} : QVariantMap{{"fileIdentity", "path:" + path}};
#endif
}
QStringList entryUrls(const QVariantMap &item) {
    QStringList result{item.value("url").toString()};
    for (const auto &value : item.value("aliases").toList()) {
        const auto url = value.toString();
        if (SecurityPolicy::localMediaUrl(QUrl(url)) && !result.contains(url)) result.append(url);
    }
    return result;
}
QString mediaIdentity(const QVariantMap &item) {
    if (!item.value("mediaIdentity").toString().isEmpty()) return item.value("mediaIdentity").toString();
    // Legacy indexed rows have a proven collection/volume. Do not guess a
    // document provider's volume or equate bare row IDs across storage devices.
    const QUrl url(item.value("url").toString());
    if (url.host() == "media" && item.contains("volumeName") && !item.value("catalogId").toString().isEmpty()) {
        const auto id = item.value("catalogId").toString();
        return id.section(':', 0, 0) + ':' + item.value("volumeName").toString() + ':' + id.section(':', -1);
    }
    return {};
}
QStringList identityTokens(const QVariantMap &item) {
    QStringList result;
    for (const auto &url : entryUrls(item)) if (!url.isEmpty()) result.append("url:" + url);
    const auto media = mediaIdentity(item), file = item.value("fileIdentity").toString();
    if (!media.isEmpty()) result.append("media:" + media);
    if (!file.isEmpty()) result.append("file:" + file);
    return result;
}
QString catalogId(const QUrl &url) {
    const auto path = url.path(QUrl::FullyDecoded);
    if (url.host() == "com.android.providers.media.documents") {
        const auto document = path.section("/document/", -1);
        if (document.startsWith("video:") || document.startsWith("audio:")) return document;
    }
    if (url.host() == "media") {
        if (path.contains("/video/media/")) return "video:" + path.section('/', -1);
        if (path.contains("/audio/media/")) return "audio:" + path.section('/', -1);
    }
    return {};
}
#ifdef Q_OS_ANDROID
QPointer<MediaStore> scanStore;
void scanFinished(JNIEnv *, jclass, jstring payload) {
    const QString json = QJniObject(payload).toString();
    QMetaObject::invokeMethod(QCoreApplication::instance(), [json] { if (scanStore) scanStore->acceptDeviceScan(json); }, Qt::QueuedConnection);
}
#endif
QString clockText(qint64 ms) {
    const auto seconds = ms / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
QVariantMap describe(const QUrl &url) {
    QString title = url.fileName();
    QString mime = QMimeDatabase().mimeTypeForUrl(url).name();
    qint64 size = url.isLocalFile() ? QFileInfo(url.toLocalFile()).size() : 0;
#ifdef Q_OS_ANDROID
    if (url.scheme() == QStringLiteral("content")) {
        QJniEnvironment env;
        const QJniObject context(QNativeInterface::QAndroidApplication::context().object());
        const auto resolver = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
        const auto uri = QJniObject::callStaticObjectMethod("android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;", QJniObject::fromString(key(url)).object());
        mime = resolver.callObjectMethod("getType", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object()).toString();
        const auto cursor = resolver.callObjectMethod("query", "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;", uri.object(), nullptr, nullptr, nullptr, nullptr);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (cursor.isValid()) {
            if (cursor.callMethod<jboolean>("moveToFirst", "()Z")) {
                int column = cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I", QJniObject::fromString(QStringLiteral("_display_name")).object());
                if (column >= 0) title = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", column).toString();
                int sizeColumn = cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I", QJniObject::fromString(QStringLiteral("_size")).object());
                if (sizeColumn >= 0) size = cursor.callMethod<jlong>("getLong", "(I)J", sizeColumn);
            }
            cursor.callMethod<void>("close", "()V");
        }
        // Providers may decline persistent access. Keep playback available and let
        // the user reselect the document if access expires after a restart.
        resolver.callMethod<void>("takePersistableUriPermission", "(Landroid/net/Uri;I)V", uri.object(), 1);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
#endif
    if (title.isEmpty()) title = QObject::tr("Selected media");
    if (!mime.startsWith(QStringLiteral("audio/")) && !mime.startsWith(QStringLiteral("video/")))
        mime = QMimeDatabase().mimeTypeForFile(title, QMimeDatabase::MatchExtension).name();
    return {{"url", key(url)}, {"title", title}, {"kind", mime.startsWith(QStringLiteral("audio/")) ? "audio" : "video"},
            {"catalogId", catalogId(url)}, {"size", size}, {"dateAdded", QDateTime::currentMSecsSinceEpoch()},
            {"folder", url.isLocalFile() ? QFileInfo(url.toLocalFile()).absolutePath() : QObject::tr("Selected files")},
            {"favorite", false}, {"position", 0}, {"duration", 0}, {"lastPlayed", 0}};
}
}

MediaStore::MediaStore(QObject *parent, const QString &storagePath, IdentityResolver identityResolver) : QObject(parent), m_storagePath(storagePath), m_identityResolver(identityResolver ? std::move(identityResolver) : resolveIdentity) {
    if (m_storagePath.isEmpty()) m_storagePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/library-v1.json";
    load();
}
int MediaStore::indexOf(const QUrl &url) const {
    for (qsizetype i = 0; i < m_media.size(); ++i)
        if (entryUrls(m_media.at(i).toMap()).contains(key(url))) return int(i);
    return -1;
}
QString MediaStore::storageKey(const QUrl &url) const {
    const int index = indexOf(url);
    return index < 0 ? key(url) : m_media.at(index).toMap().value("url").toString();
}
bool MediaStore::coalesceLibrary() {
    // Union all proven aliases, including a row bridging two earlier groups.
    QList<int> parent;
    for (int i = 0; i < m_media.size(); ++i) parent.append(i);
    const auto root = [&parent](int i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
    QHash<QString, int> owners;
    for (int i = 0; i < m_media.size(); ++i) {
        for (const auto &token : identityTokens(m_media.at(i).toMap())) {
            if (owners.contains(token)) {
                const int a = root(i), b = root(owners.value(token));
                if (a != b) parent[std::max(a, b)] = std::min(a, b);
            } else owners.insert(token, i);
        }
    }
    QVariantList merged;
    QHash<int, int> groups;
    QHash<QString, QString> remap;
    for (int i = 0; i < m_media.size(); ++i) {
        const auto source = m_media.at(i).toMap(); const int group = root(i);
        if (!groups.contains(group)) { groups[group] = int(merged.size()); merged.append(source); }
        else {
            auto target = merged.at(groups.value(group)).toMap();
            const bool newer = source.value("lastPlayed").toLongLong() > target.value("lastPlayed").toLongLong()
                || (source.value("lastPlayed") == target.value("lastPlayed") && source.value("position").toLongLong() > target.value("position").toLongLong());
            if (newer) { target["position"] = source.value("position"); target["lastPlayed"] = source.value("lastPlayed"); }
            target["favorite"] = target.value("favorite").toBool() || source.value("favorite").toBool();
            for (const auto &field : {"title", "kind", "size", "duration", "folder", "folderKey", "volumeName", "catalogId", "catalogUrl", "width", "height", "artist", "album", "mediaIdentity", "fileIdentity"}) {
                const auto value = source.value(field);
                if (value.isValid() && !value.toString().isEmpty() && value.toString() != "0"
                    && (source.value("catalogManaged").toBool() || !target.contains(field) || target.value(field).toString().isEmpty() || target.value(field).toString() == "0")) target[field] = value;
            }
            target["catalogManaged"] = target.value("catalogManaged").toBool() || source.value("catalogManaged").toBool();
            const auto sourceDate = source.value("dateAdded").toLongLong(), targetDate = target.value("dateAdded").toLongLong();
            if (sourceDate > 0 && (targetDate <= 0 || sourceDate < targetDate)) target["dateAdded"] = sourceDate;
            auto aliases = entryUrls(target);
            for (const auto &url : entryUrls(source)) if (!aliases.contains(url)) aliases.append(url);
            QVariantList savedAliases; for (const auto &url : aliases) if (url != target.value("url").toString()) savedAliases.append(url);
            target["aliases"] = savedAliases;
            merged[groups.value(group)] = target;
        }
    }
    for (auto &value : merged) {
        auto item = value.toMap(); const auto urls = entryUrls(item);
        const QUrl catalog(item.value("catalogUrl").toString());
        if (catalog.host() == "media" && urls.contains(key(catalog))) item["url"] = key(catalog);
        const auto primary = item.value("url").toString();
        QVariantList aliases;
        for (const auto &url : urls) { remap[url] = primary; if (url != primary) aliases.append(url); }
        if (!aliases.isEmpty() || item.contains("aliases")) item["aliases"] = aliases;
        value = item;
    }
    auto playlists = m_playlists;
    for (auto &value : playlists) {
        auto playlist = value.toMap(); QVariantList urls;
        for (const auto &old : playlist.value("urls").toList()) {
            const auto canonical = remap.value(old.toString(), old.toString());
            if (!urls.contains(canonical)) urls.append(canonical);
        }
        playlist["urls"] = urls; value = playlist;
    }
    QVariantMap bookmarks;
    for (auto it = m_bookmarks.begin(); it != m_bookmarks.end(); ++it) {
        const auto canonical = remap.value(it.key(), it.key()); auto notes = bookmarks.value(canonical).toList();
        for (const auto &value : it.value().toList()) {
            if (notes.contains(value)) continue;
            auto note = value.toMap();
            for (const auto &existing : notes) if (existing.toMap().value("id") == note.value("id")) { note["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces); break; }
            notes.append(note);
        }
        std::stable_sort(notes.begin(), notes.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("position").toLongLong() < b.toMap().value("position").toLongLong(); });
        bookmarks[canonical] = notes;
    }
    const bool changed = merged != m_media || playlists != m_playlists || bookmarks != m_bookmarks;
    m_media = merged; m_playlists = playlists; m_bookmarks = bookmarks;
    return changed;
}
QVariantMap MediaStore::entry(const QUrl &url) const {
    const int i = indexOf(url);
    return i >= 0 ? m_media.at(i).toMap() : QVariantMap{};
}
QString MediaStore::importFiles(const QList<QUrl> &urls) {
    QString first;
    bool changed = false;
    for (const auto &url : urls) {
        if (!SecurityPolicy::localMediaUrl(url)) continue;
        if (url.isLocalFile() && !QFileInfo(url.toLocalFile()).isFile()) continue;
        if (first.isEmpty()) first = key(url);
        if (indexOf(url) < 0) {
            auto item = describe(url);
            const auto identity = m_identityResolver(url);
            for (auto it = identity.begin(); it != identity.end(); ++it)
                if (it.key() == "fileIdentity" || it.key() == "mediaIdentity") item[it.key()] = it.value();
            m_removedMedia.removeAll(item.value("catalogId").toString()); m_removedMedia.removeAll(key(url));
            m_media.append(item); changed = true;
        }
    }
    if (changed) {
        const bool reconciled = coalesceLibrary(); save(); emit libraryChanged();
        if (reconciled) { emit playlistsChanged(); emit bookmarksChanged(); }
    }
    return first.isEmpty() ? first : storageKey(QUrl(first));
}
void MediaStore::updateProgress(const QUrl &url, qint64 position, qint64 duration) {
    const int i = indexOf(url); if (i < 0) return;
    auto item = m_media.at(i).toMap();
    item["position"] = std::clamp(position, qint64(0), std::max(duration, qint64(0)));
    item["duration"] = std::max(duration, qint64(0));
    item["lastPlayed"] = QDateTime::currentMSecsSinceEpoch();
    m_media[i] = item; save(); emit libraryChanged();
}
void MediaStore::toggleFavorite(const QUrl &url) {
    const int i = indexOf(url); if (i < 0) return;
    auto item = m_media.at(i).toMap(); item["favorite"] = !item.value("favorite").toBool(); m_media[i] = item;
    save(); emit libraryChanged();
}
QString MediaStore::createPlaylist(const QString &name) {
    const auto clean = name.trimmed().left(100); if (clean.isEmpty()) return {};
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_playlists.append(QVariantMap{{"id", id}, {"name", clean}, {"urls", QVariantList{}}});
    save(); emit playlistsChanged(); return id;
}
QString MediaStore::addToPlaylist(const QString &id, const QUrl &url) {
    const auto result = addToPlaylistBatch(id, {url});
    if (result.value("status").toString() != "ok") return result.value("status").toString();
    return result.value("added").toInt() ? QStringLiteral("added") : result.value("duplicates").toInt() ? QStringLiteral("duplicate") : QStringLiteral("missingMedia");
}
QVariantMap MediaStore::addToPlaylistBatch(const QString &id, const QList<QUrl> &urls, bool importMissing) {
    QVariantMap result{{"status", "missingPlaylist"}, {"added", 0}, {"duplicates", 0}, {"failed", urls.size()}};
    qsizetype index = -1;
    for (qsizetype i = 0; i < m_playlists.size(); ++i)
        if (m_playlists.at(i).toMap().value("id").toString() == id) { index = i; break; }
    if (index < 0) return result; // Never import into a deleted/invalid destination.
    if (importMissing) importFiles(urls);
    const auto original = m_playlists.at(index).toMap();
    auto playlist = original;
    auto members = playlist.value("urls").toList();
    int added = 0, duplicates = 0, failed = 0;
    for (const auto &url : urls) {
        if (!SecurityPolicy::localMediaUrl(url) || indexOf(url) < 0
            || (importMissing && url.isLocalFile() && !QFileInfo(url.toLocalFile()).isFile())) { ++failed; continue; }
        const auto identity = storageKey(url);
        if (members.contains(identity)) { ++duplicates; continue; }
        members.append(identity); ++added;
    }
    if (added) {
        playlist["urls"] = members; m_playlists[index] = playlist;
        if (!save()) {
            m_playlists[index] = original;
            return {{"status", "saveError"}, {"added", 0}, {"duplicates", duplicates}, {"failed", failed + added}};
        }
        emit playlistsChanged();
    }
    return {{"status", "ok"}, {"added", added}, {"duplicates", duplicates}, {"failed", failed}};
}
void MediaStore::removePlaylist(const QString &id) {
    m_playlists.removeIf([&](const QVariant &v) { return v.toMap().value("id").toString() == id; });
    save(); emit playlistsChanged();
}
QVariantList MediaStore::playlistItems(const QString &id) const {
    QVariantList result;
    for (const auto &playlist : m_playlists) {
        const auto item = playlist.toMap(); if (item.value("id").toString() != id) continue;
        for (const auto &url : item.value("urls").toList()) { const auto value = entry(QUrl(url.toString())); if (!value.isEmpty()) result.append(value); }
    }
    return result;
}
QVariantList MediaStore::bookmarks(const QUrl &url) const { return m_bookmarks.value(storageKey(url)).toList(); }
void MediaStore::addBookmark(const QUrl &url, qint64 position, const QString &note) {
    if (indexOf(url) < 0) return;
    auto list = bookmarks(url);
    const qint64 time = std::max(qint64(0), position);
    list.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"position", time}, {"note", note.trimmed().left(1000)}});
    std::stable_sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("position").toLongLong() < b.toMap().value("position").toLongLong(); });
    m_bookmarks[storageKey(url)] = list; save(); emit bookmarksChanged();
}
void MediaStore::updateBookmark(const QUrl &url, const QString &id, const QString &note) {
    if (indexOf(url) < 0) return;
    auto list = bookmarks(url);
    for (qsizetype i = 0; i < list.size(); ++i) {
        auto item = list.at(i).toMap();
        if (item.value("id").toString() != id) continue;
        item["note"] = note.trimmed().left(1000); list[i] = item;
        m_bookmarks[storageKey(url)] = list; save(); emit bookmarksChanged(); return;
    }
}
QVariantList MediaStore::readSubtitles(const QUrl &url) {
    if (!SecurityPolicy::localMediaUrl(url)) {
        emit errorOccurred(tr("Choose a local SRT or WebVTT file.")); return {};
    }
    QFile file(url.isLocalFile() ? url.toLocalFile() : key(url));
    if (!file.open(QIODevice::ReadOnly)) { emit errorOccurred(tr("Could not open subtitles. Select the file again.")); return {}; }
    constexpr qint64 limit = 5 * 1024 * 1024;
    const auto bytes = file.read(limit + 1);
    if (bytes.size() > limit) { emit errorOccurred(tr("Choose a subtitle file smaller than 5 MB.")); return {}; }
    auto text = QString::fromUtf8(bytes); text.replace("\r\n", "\n"); text.replace('\r', '\n'); text.remove(QChar(0xfeff));
    static const QRegularExpression timing(QStringLiteral(R"(^\s*(?:(\d{1,3}):)?(\d{2}):(\d{2})[,.](\d{3})\s*-->\s*(?:(\d{1,3}):)?(\d{2}):(\d{2})[,.](\d{3})(?:\s.*)?$)"));
    static const QRegularExpression blank(QStringLiteral("\n[ \t]*\n"));
    // Excluding nested '<' avoids repeatedly scanning the rest of a malformed cue.
    static const QRegularExpression tags(QStringLiteral("<[^<>]*>"));
    QVariantList result;
    for (const auto &block : text.split(blank, Qt::SkipEmptyParts)) {
        const auto lines = block.split('\n');
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const auto match = timing.match(lines.at(i)); if (!match.hasMatch()) continue;
            if (match.captured(2).toInt() > 59 || match.captured(3).toInt() > 59 || match.captured(6).toInt() > 59 || match.captured(7).toInt() > 59) break;
            const qint64 start = ((match.captured(1).toLongLong() * 60 + match.captured(2).toInt()) * 60 + match.captured(3).toInt()) * 1000 + match.captured(4).toInt();
            const qint64 end = ((match.captured(5).toLongLong() * 60 + match.captured(6).toInt()) * 60 + match.captured(7).toInt()) * 1000 + match.captured(8).toInt();
            auto caption = lines.mid(i + 1).join('\n').trimmed(); caption.remove(tags);
            caption.replace("&lt;", "<").replace("&gt;", ">").replace("&nbsp;", " ").replace("&amp;", "&");
            if (end > start && !caption.isEmpty()) result.append(QVariantMap{{"start", start}, {"end", end}, {"text", caption.left(4000)}});
            break;
        }
        if (result.size() >= 20000) break;
    }
    std::stable_sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("start").toLongLong() < b.toMap().value("start").toLongLong(); });
    qint64 maxEnd = 0;
    for (auto &value : result) { auto cue = value.toMap(); maxEnd = std::max(maxEnd, cue.value("end").toLongLong()); cue["maxEnd"] = maxEnd; value = cue; }
    if (result.isEmpty()) emit errorOccurred(tr("No valid captions found. Use a UTF-8 SRT or WebVTT file."));
    return result;
}
void MediaStore::removeBookmark(const QUrl &url, const QString &id) {
    auto list = bookmarks(url); list.removeIf([&](const QVariant &v) { return v.toMap().value("id").toString() == id; });
    m_bookmarks[storageKey(url)] = list; save(); emit bookmarksChanged();
}
bool MediaStore::exportNotes(const QUrl &url, const QUrl &destination) {
    if (!SecurityPolicy::localMediaUrl(destination) || indexOf(url) < 0) return false;
    if (indexOf(destination) >= 0) { emit errorOccurred(tr("Choose a notes destination different from your media files.")); return false; }
    // Never overwrite media when a mistaken export destination is selected.
    for (const auto &value : m_media) {
        const QUrl media(value.toMap().value("url").toString());
        if (key(media) == key(destination) || (media.isLocalFile() && destination.isLocalFile()
            && QFileInfo(media.toLocalFile()).canonicalFilePath() == QFileInfo(destination.toLocalFile()).canonicalFilePath()
            && QFileInfo(destination.toLocalFile()).exists())) {
            emit errorOccurred(tr("Choose a notes destination different from your media files.")); return false;
        }
    }
    QFile file(destination.isLocalFile() ? destination.toLocalFile() : key(destination));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { emit errorOccurred(tr("Could not save notes. Choose another location.")); return false; }
    QString text = entry(url).value("title").toString() + "\nJatayu Media Player — study notes\n\n";
    for (const auto &value : bookmarks(url)) { const auto b = value.toMap(); text += clockText(b.value("position").toLongLong()) + "  " + b.value("note").toString() + '\n'; }
    const auto bytes = text.toUtf8(); const bool ok = file.write(bytes) == bytes.size(); file.close();
    if (!ok) emit errorOccurred(tr("The notes could not be written completely."));
    return ok;
}
void MediaStore::forget(const QUrl &url) {
    const int i = indexOf(url); if (i < 0) return;
    const auto removed = m_media.at(i).toMap();
    const auto identity = removed.value("catalogId").toString();
    m_removedMedia.append(identity.isEmpty() ? key(url) : identity);
    const auto canonical = removed.value("url").toString();
    m_media.removeAt(i); m_bookmarks.remove(canonical);
    for (qsizetype j = 0; j < m_playlists.size(); ++j) { auto p = m_playlists.at(j).toMap(); auto urls = p.value("urls").toList(); urls.removeAll(canonical); p["urls"] = urls; m_playlists[j] = p; }
    save(); emit libraryChanged(); emit playlistsChanged(); emit bookmarksChanged();
}
bool MediaStore::save() {
    if (m_storageBlocked) { emit errorOccurred(tr("The stored library could not be loaded safely. Its file has been kept unchanged.")); return false; }
    if (!QDir().mkpath(QFileInfo(m_storagePath).absolutePath())) { emit errorOccurred(tr("Could not create the library folder.")); return false; }
    QSaveFile file(m_storagePath);
    const auto data = QJsonDocument(QJsonObject::fromVariantMap({{"version", 1}, {"media", m_media}, {"playlists", m_playlists}, {"bookmarks", m_bookmarks}, {"preferences", m_preferences}, {"removedMedia", m_removedMedia}})).toJson(QJsonDocument::Compact);
    if (data.size() > SecurityPolicy::libraryBytes) { emit errorOccurred(tr("Library storage limit reached. Remove unused entries or notes before adding more.")); return false; }
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) { emit errorOccurred(tr("Could not save library changes.")); return false; }
    QFile::setPermissions(m_storagePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}
void MediaStore::load() {
    QFile file(m_storagePath); if (!file.open(QIODevice::ReadOnly)) return;
    const auto bytes = file.read(SecurityPolicy::libraryBytes + 1);
    if (bytes.size() > SecurityPolicy::libraryBytes) { m_storageBlocked = true; emit errorOccurred(tr("Library data is too large to load safely.")); return; }
    QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) { m_storageBlocked = true; return; }
    const auto root = document.object().toVariantMap(); if (root.value("version").toInt() != 1) { m_storageBlocked = true; return; }
    m_media = root.value("media").toList(); m_playlists = root.value("playlists").toList(); m_bookmarks = root.value("bookmarks").toMap();
    if (m_media.size() > SecurityPolicy::libraryEntries || m_playlists.size() > 2048 || m_bookmarks.size() > SecurityPolicy::libraryEntries) {
        m_storageBlocked = true; m_media.clear(); m_playlists.clear(); m_bookmarks.clear(); return;
    }
    m_media.removeIf([](const QVariant &value) { return !SecurityPolicy::localMediaUrl(QUrl(value.toMap().value("url").toString())); });
    for (auto &value : m_media) {
        auto item = value.toMap();
        for (const auto &field : {"title", "artist", "album"}) item[field] = item.value(field).toString().left(1024);
        value = item;
    }
    const auto preferences = root.value("preferences").toMap();
    for (auto it = preferences.begin(); it != preferences.end(); ++it)
        if (m_preferences.contains(it.key())) m_preferences[it.key()] = it.value();
    m_removedMedia = root.value("removedMedia").toStringList();
    if (coalesceLibrary()) save();
}

void MediaStore::setPreference(const QString &name, const QVariant &value) {
    if (!m_preferences.contains(name)) return;
    QVariant clean = value;
    if (name == "crossfade") clean=std::clamp(value.toInt(),0,8);
    else if (name == "language") { if(!QStringList{"en","hi"}.contains(value.toString()))return; }
    else if (name == "volume") { const double level=value.toDouble(); if (!std::isfinite(level)) return; clean=std::clamp(level,0.,1.); }
    else if (name == "defaultSpeed") clean = std::clamp(value.toDouble(), 0.5, 2.0);
    else if (name == "hideDelay") clean = std::clamp(value.toInt(), 2, 10);
    else if (name == "studySkip") clean = std::clamp(value.toInt(), 2, 30);
    else if (name == "subtitleSize") clean = std::clamp(value.toInt(), 16, 36);
    else if (name == "seekStep") clean = std::clamp(value.toInt(), 5, 60);
    else if (name == "visualMode") {
        if (!QStringList{"bars", "pulse", "artwork"}.contains(value.toString())) return;
    }
    else if (name == "sort") {
        if (!QStringList{"recent", "name", "duration", "size"}.contains(value.toString())) return;
    } else clean = value.toBool();
    if (m_preferences.value(name) == clean) return;
    m_preferences[name] = clean; save(); emit preferencesChanged();
}
void MediaStore::restoreRemovedMedia() { m_removedMedia.clear(); save(); }
bool MediaStore::clearPlaybackHistory() {
    const auto original = m_media;
    for (auto &value : m_media) {
        auto item = value.toMap(); item["position"] = 0; item["lastPlayed"] = 0; value = item;
    }
    if (!save()) { m_media = original; return false; }
    emit libraryChanged(); return true;
}
bool MediaStore::supportsRotationLock() const {
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>("io/github/jatayuos4u/jatayumediaplayer/JatayuActivity", "supportsRotationLock", "()Z");
#else
    return true;
#endif
}
void MediaStore::configurePlayerWindow(qreal brightness, bool keepAwake, int orientation, bool immersive) {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("io/github/jatayuos4u/jatayumediaplayer/JatayuActivity", "setPlayerWindow", "(FZIZ)V", jfloat(brightness), jboolean(keepAwake), jint(orientation), jboolean(immersive));
#else
    Q_UNUSED(brightness); Q_UNUSED(keepAwake); Q_UNUSED(orientation); Q_UNUSED(immersive);
#endif
}
void MediaStore::scanDevice(bool askPermission, bool changeAccess) {
    if (m_scanning) return;
#ifdef Q_OS_ANDROID
    QJniEnvironment env;
    scanStore = this;
    const JNINativeMethod methods[] = {{const_cast<char *>("nativeScanFinished"), const_cast<char *>("(Ljava/lang/String;)V"), reinterpret_cast<void *>(scanFinished)}};
    if (!env.registerNativeMethods("io/github/jatayuos4u/jatayumediaplayer/JatayuActivity", methods, 1)) {
        m_scanStatus = tr("Device scan could not start. You can still add files."); emit scanChanged(); return;
    }
    m_scanning = true; m_scanStatus = tr("Finding your videos and music…"); emit scanChanged();
    QJsonArray known;
    for (const auto &value : m_media) { const auto item = value.toMap(); if (!item.value("catalogManaged").toBool()) known.append(item.value("url").toString()); }
    const auto knownJson = QJniObject::fromString(QString::fromUtf8(QJsonDocument(known).toJson(QJsonDocument::Compact)));
    QJniObject::callStaticMethod<void>("io/github/jatayuos4u/jatayumediaplayer/JatayuActivity", "requestMediaScan", "(ZZLjava/lang/String;)V", jboolean(askPermission), jboolean(changeAccess), knownJson.object());
#else
    Q_UNUSED(askPermission); Q_UNUSED(changeAccess);
    m_scanStatus = tr("Device discovery is available on Android. Add files to test the library here."); emit scanChanged();
#endif
}
void MediaStore::acceptDeviceScan(const QString &payload) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    m_scanning = false;
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        m_scanStatus = tr("Scan could not finish. Your existing library has been kept."); emit scanChanged(); return;
    }
    const auto result = document.object().toVariantMap();
    if (!document.object().value("items").isArray()) {
        m_scanStatus = tr("Scan could not finish. Your existing library has been kept."); emit scanChanged(); return;
    }
    const auto status = result.value("status").toString();
    const auto scopes = (status == "ready" || status == "limited") ? result.value("completeScopes").toStringList() : QStringList{};
    mergeCatalog(result.value("items").toList(), scopes, result.value("identities").toList());
    m_scanStatus = result.value("message").toString();
    emit scanChanged();
}
void MediaStore::mergeCatalog(const QVariantList &items, const QStringList &completeScopes, const QVariantList &resolvedIdentities) {
    bool changed = false;
    QHash<QString, int> knownUrls;
    for (int i = 0; i < m_media.size(); ++i)
        for (const auto &url : entryUrls(m_media.at(i).toMap())) knownUrls[url] = i;
    // Legacy manually selected entries are resolved on Android's scan worker,
    // so reconciliation does not open descriptors on the QML/UI thread.
    for (const auto &value : resolvedIdentities) {
        const auto identity = value.toMap(); const int index = knownUrls.value(identity.value("url").toString(), -1);
        if (index < 0) continue;
        auto item = m_media.at(index).toMap();
        for (const auto &field : {"mediaIdentity", "fileIdentity"}) {
            if (!identity.value(field).toString().isEmpty()) item[field] = identity.value(field);
            else item.remove(field);
        }
        if (item != m_media.at(index).toMap()) { m_media[index] = item; changed = true; }
    }
    QSet<QString> seen;
    for (const auto &value : items) {
        const auto incoming = value.toMap(); const QUrl url(incoming.value("url").toString());
        const auto id = catalogId(url), kind = incoming.value("kind").toString();
        bool numericId = false; id.section(':', -1).toULongLong(&numericId);
        if (!SecurityPolicy::localMediaUrl(url) || url.host() != "media" || id.isEmpty() || !numericId || !id.startsWith(kind + ":") ||
            (kind != "audio" && kind != "video") || m_removedMedia.contains(id) || m_removedMedia.contains(key(url))) continue;
        const int index = knownUrls.value(key(url), -1);
        QVariantMap item = index < 0 ? QVariantMap{{"url", key(url)}, {"position", 0}, {"lastPlayed", 0}, {"favorite", false}} : m_media.at(index).toMap();
        const auto scope = kind + ":" + incoming.value("volumeName").toString();
        seen.insert(scope + "|" + id);
        item["catalogId"] = id; item["catalogManaged"] = true; item["catalogUrl"] = key(url);
        item["mediaIdentity"] = kind + ":" + incoming.value("volumeName").toString() + ":" + id.section(':', -1);
        item["fileIdentity"] = incoming.value("fileIdentity");
        for (const auto &field : {"title", "kind", "size", "dateAdded", "folder", "folderKey", "volumeName", "width", "height", "artist", "album"}) item[field] = incoming.value(field);
        if (incoming.value("duration").toLongLong() > 0) item["duration"] = incoming.value("duration");
        if (item.value("title").toString().isEmpty()) item["title"] = tr("Untitled media");
        if (index < 0) { knownUrls[key(url)] = int(m_media.size()); m_media.append(item); } else m_media[index] = item;
        changed = true;
    }
    const bool reconciled = coalesceLibrary(); changed |= reconciled;
    bool pruned = false;
    for (qsizetype i = m_media.size(); i-- > 0;) {
        const auto item = m_media.at(i).toMap(); const auto id = item.value("catalogId").toString();
        const auto scope = item.value("kind").toString() + ":" + item.value("volumeName").toString();
        if (id.isEmpty() || seen.contains(scope + "|" + id) || !completeScopes.contains(scope)
            || (!item.value("catalogManaged").toBool() && !item.contains("folderKey"))) continue;
        const auto url = item.value("url").toString();
        m_media.removeAt(i); m_bookmarks.remove(url);
        for (auto &value : m_playlists) {
            auto playlist = value.toMap(); auto members = playlist.value("urls").toList(); members.removeAll(url);
            playlist["urls"] = members; value = playlist;
        }
        changed = true; pruned = true;
    }
    if (changed) { save(); emit libraryChanged(); }
    if (reconciled || pruned) { emit playlistsChanged(); emit bookmarksChanged(); }
}
