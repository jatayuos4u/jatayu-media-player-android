#include <QtTest>
#include "MediaStore.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

class MediaStoreTest : public QObject {
    Q_OBJECT
private slots:
    void importPersistAndForget() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto firstPath = dir.filePath("a song.wav"); QFile a(firstPath); QVERIFY(a.open(QIODevice::WriteOnly)); a.write("test"); a.close();
        const auto secondPath = dir.filePath("lesson.mp4"); QFile b(secondPath); QVERIFY(b.open(QIODevice::WriteOnly)); b.write("test"); b.close();
        const QUrl first = QUrl::fromLocalFile(firstPath), second = QUrl::fromLocalFile(secondPath);
        const auto storage = dir.filePath("state/library.json");
        QString id;
        {
            MediaStore store(nullptr, storage);
            QVERIFY(!store.importFiles({first, second, first}).isEmpty()); QCOMPARE(store.media().size(), 2);
            QCOMPARE(store.entry(first).value("kind").toString(), QString("audio"));
            QCOMPARE(store.entry(first).value("title").toString(), QString("a song.wav"));
            QVERIFY(store.importFiles({QUrl("https://example.com/video.mp4"), QUrl::fromLocalFile(dir.filePath("missing.mp4"))}).isEmpty());
            store.updateProgress(first, 5000, 3000); QCOMPARE(store.entry(first).value("position").toLongLong(), 3000LL);
            store.toggleFavorite(first); QVERIFY(store.entry(first).value("favorite").toBool());
            QVERIFY(store.createPlaylist("  ").isEmpty()); id = store.createPlaylist(" My study "); QVERIFY(!id.isEmpty());
            store.addToPlaylist(id, first); store.addToPlaylist(id, first); store.addToPlaylist(id, second); QCOMPARE(store.playlistItems(id).size(), 2);
            store.addBookmark(first, 2000, "Second"); store.addBookmark(first, 1000, "First");
            QCOMPARE(store.bookmarks(first).first().toMap().value("note").toString(), QString("First"));
            const auto exported = dir.filePath("notes.txt"); QVERIFY(store.exportNotes(first, QUrl::fromLocalFile(exported)));
            QFile notes(exported); QVERIFY(notes.open(QIODevice::ReadOnly)); QVERIFY(notes.readAll().contains("0:01  First"));
            QVERIFY(!store.exportNotes(first, QUrl("https://example.com/notes.txt")));
        }
        {
            MediaStore restored(nullptr, storage); QCOMPARE(restored.media().size(), 2);
            QCOMPARE(restored.playlistItems(id).size(), 2); QCOMPARE(restored.bookmarks(first).size(), 2);
            QVERIFY(restored.entry(first).value("favorite").toBool());
            QCOMPARE(restored.entry(first).value("position").toLongLong(), 3000LL);
            restored.removeBookmark(first, restored.bookmarks(first).first().toMap().value("id").toString()); QCOMPARE(restored.bookmarks(first).size(), 1);
            restored.forget(first); QCOMPARE(restored.media().size(), 1); QCOMPARE(restored.playlistItems(id).size(), 1); QVERIFY(restored.bookmarks(first).isEmpty());
            QVERIFY(QFile::exists(firstPath)); // Removing a library entry must never delete media.
            restored.removePlaylist(id); QVERIFY(restored.playlists().isEmpty());
        }
    }
    void deviceCatalogAndPreferences() {
        QTemporaryDir dir; const auto state = dir.filePath("state.json");
        const QUrl imported("content://com.android.providers.media.documents/document/video%3A19");
        const QVariantMap video{{"url", "content://media/external/video/media/19"}, {"kind", "video"}, {"title", "Travel.mp4"}, {"duration", 60000}, {"size", 1000000}, {"folder", "DCIM/Camera/"}, {"folderKey", "external_primary:DCIM/Camera/"}, {"dateAdded", 1234}, {"width", 1920}, {"height", 1080}};
        {
            // The host has no Android provider: inject its confirmed identity,
            // rather than assuming a bare document ID identifies a volume.
            MediaStore store(nullptr, state, [imported](const QUrl &url) -> QVariantMap {
                return url == imported ? QVariantMap{{"mediaIdentity", "video::19"}} : QVariantMap{};
            }); store.importFiles({imported});
            store.updateProgress(imported, 12000, 60000); store.toggleFavorite(imported);
            const auto list = store.createPlaylist("Travel"); store.addToPlaylist(list, imported);
            store.addBookmark(imported, 5000, "Remember");
            store.mergeCatalog({video, video}); QCOMPARE(store.media().size(), 1);
            QCOMPARE(store.entry(imported).value("title").toString(), QString("Travel.mp4"));
            QCOMPARE(store.entry(imported).value("position").toLongLong(), 12000LL);
            QVERIFY(store.entry(imported).value("favorite").toBool()); QCOMPARE(store.playlistItems(list).size(), 1);
            QCOMPARE(store.bookmarks(imported).size(), 1);
            store.acceptDeviceScan(R"({"items":[],"status":"permission","message":"Access needed"})");
            QCOMPARE(store.media().size(), 1); QVERIFY(!store.scanning()); QCOMPARE(store.scanStatus(), QString("Access needed"));
            store.mergeCatalog({QVariantMap{{"url", "https://example.com/video"}, {"kind", "video"}}, QVariantMap{{"url", "content://media/external/video/media/not-an-id"}, {"kind", "video"}}});
            QCOMPARE(store.media().size(), 1);
            store.setPreference("resume", false); store.setPreference("seekStep", 500); store.setPreference("defaultSpeed", 9.0);
            store.setPreference("sort", "name"); store.setPreference("sort", "invalid"); store.setPreference("unknown", true);
            QCOMPARE(store.preferences().value("seekStep").toInt(), 60);
            QCOMPARE(store.preferences().value("defaultSpeed").toDouble(), 2.0);
            QCOMPARE(store.preferences().value("sort").toString(), QString("name")); QVERIFY(!store.preferences().contains("unknown"));
            store.forget(imported); store.mergeCatalog({video}); QVERIFY(store.media().isEmpty());
        }
        MediaStore restored(nullptr, state);
        QVERIFY(!restored.preferences().value("resume").toBool());
        restored.mergeCatalog({video}); QVERIFY(restored.media().isEmpty());
        restored.restoreRemovedMedia(); restored.mergeCatalog({video}); QCOMPARE(restored.media().size(), 1);
    }
    void scanRemovesMissingMediaOnlyInCompleteScopes() {
        QTemporaryDir dir; const auto state = dir.filePath("state.json");
        MediaStore store(nullptr, state);
        auto row = [](int id, const QString &kind, const QString &volume) {
            return QVariantMap{{"url", "content://media/external/" + kind + "/media/" + QString::number(id)},
                {"kind", kind}, {"title", QString::number(id)}, {"volumeName", volume},
                {"folderKey", volume + ":Downloads/"}, {"duration", 60000}};
        };
        const auto keep = row(10, "video", "external_primary");
        const auto gone = row(11, "video", "external_primary");
        const auto card = row(12, "video", "ABCD-1234");
        const auto song = row(13, "audio", "external_primary");
        const auto added = row(14, "video", "external_primary");
        const QUrl keepUrl(keep.value("url").toString()), goneUrl(gone.value("url").toString());
        store.mergeCatalog({keep, gone, card, song});
        const QUrl picked("content://com.example.documents/document/selected-video");
        store.importFiles({picked});
        store.updateProgress(keepUrl, 12000, 60000); store.toggleFavorite(keepUrl);
        store.addBookmark(keepUrl, 1000, "Keep note"); store.addBookmark(goneUrl, 1000, "Old note");
        const auto playlist = store.createPlaylist("Mixed"); store.addToPlaylist(playlist, goneUrl); store.addToPlaylist(playlist, keepUrl);
        const auto scan = [&](const QVariantList &items, const QString &status, const QStringList &scopes) {
            store.acceptDeviceScan(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap({
                {"items", items}, {"status", status}, {"completeScopes", scopes}, {"message", "Refreshed"}})).toJson()));
        };
        scan({}, "limited", {}); QCOMPARE(store.media().size(), 5);
        scan({}, "permission", {"video:external_primary"}); QCOMPARE(store.media().size(), 5);
        scan({}, "error", {"video:external_primary"}); QCOMPARE(store.media().size(), 5);
        store.acceptDeviceScan("{bad json"); QCOMPARE(store.media().size(), 5);
        store.acceptDeviceScan(R"({"status":"ready","completeScopes":["video:external_primary"]})"); QCOMPARE(store.media().size(), 5);
        scan({keep, added}, "ready", {"video:external_primary"});
        QCOMPARE(store.media().size(), 5); // One gone, one added.
        QVERIFY(store.entry(goneUrl).isEmpty()); QVERIFY(!store.entry(QUrl(added.value("url").toString())).isEmpty());
        QCOMPARE(store.entry(keepUrl).value("position").toInt(), 12000); QVERIFY(store.entry(keepUrl).value("favorite").toBool());
        QCOMPARE(store.bookmarks(keepUrl).size(), 1); QVERIFY(store.bookmarks(goneUrl).isEmpty());
        QCOMPARE(store.playlistItems(playlist).size(), 1);
        QVERIFY(!store.entry(picked).isEmpty()); QVERIFY(!store.entry(QUrl(card.value("url").toString())).isEmpty());
        QVERIFY(!store.entry(QUrl(song.value("url").toString())).isEmpty());
        // Fully granted audio may reconcile even if video access is partial.
        scan({}, "limited", {"audio:external_primary"}); QCOMPARE(store.media().size(), 4);
        scan({}, "ready", {"video:external_primary"}); QCOMPARE(store.media().size(), 2);
        QVERIFY(!store.entry(picked).isEmpty()); QVERIFY(!store.entry(QUrl(card.value("url").toString())).isEmpty());
        MediaStore restored(nullptr, state); QCOMPARE(restored.media().size(), 2);
        QVERIFY(restored.playlistItems(playlist).isEmpty());
        restored.mergeCatalog({gone}); QVERIFY(!restored.entry(goneUrl).isEmpty()); // No exclusion tombstone.
    }
    void legacyScannedRowsAreCleanedButPickerOnlyRowsRemain() {
        QTemporaryDir dir; const auto state = dir.filePath("legacy.json");
        const QUrl legacy("content://com.android.providers.media.documents/document/video%3A19");
        const QUrl pickerOnly("content://com.android.providers.media.documents/document/video%3A20");
        QFile file(state); QVERIFY(file.open(QIODevice::WriteOnly));
        const QVariantList rows{
            QVariantMap{{"url", legacy.toString(QUrl::FullyEncoded)}, {"catalogId", "video:19"}, {"kind", "video"}, {"folderKey", ":DCIM/"}, {"volumeName", ""}},
            QVariantMap{{"url", pickerOnly.toString(QUrl::FullyEncoded)}, {"catalogId", "video:20"}, {"kind", "video"}}
        };
        file.write(QJsonDocument(QJsonObject::fromVariantMap({{"version", 1}, {"media", rows}})).toJson()); file.close();
        MediaStore store(nullptr, state);
        store.acceptDeviceScan(R"({"items":[],"status":"ready","completeScopes":["video:"],"message":"Refreshed"})");
        QVERIFY(store.entry(legacy).isEmpty()); QVERIFY(!store.entry(pickerOnly).isEmpty());
    }
    void editableNotesAndLocalSubtitles() {
        QTemporaryDir dir; const auto state = dir.filePath("state.json");
        const auto path = dir.filePath("lesson.mp4"); QFile media(path); QVERIFY(media.open(QIODevice::WriteOnly)); media.write("test"); media.close();
        const auto url = QUrl::fromLocalFile(path);
        MediaStore store(nullptr, state); store.importFiles({url}); store.addBookmark(url, 5000, "First idea");
        const auto id = store.bookmarks(url).first().toMap().value("id").toString();
        store.updateBookmark(url, id, "  Updated idea\nA question to revisit  ");
        QCOMPARE(store.bookmarks(url).size(), 1);
        QCOMPARE(store.bookmarks(url).first().toMap().value("position").toInt(), 5000);
        QCOMPARE(store.bookmarks(url).first().toMap().value("note").toString(), QString("Updated idea\nA question to revisit"));
        store.updateBookmark(url, "not-a-note", "Bad change"); QCOMPARE(store.bookmarks(url).size(), 1);
        store.setPreference("studySkip", 1); store.setPreference("subtitleSize", 100);
        QCOMPARE(store.preferences().value("visualMode").toString(), QString("bars"));
        store.setPreference("visualMode", "pulse"); store.setPreference("visualMode", "invalid");
        store.setPreference("studyVisuals", true);
        QCOMPARE(store.preferences().value("studySkip").toInt(), 2); QCOMPARE(store.preferences().value("subtitleSize").toInt(), 36);
        MediaStore restored(nullptr, state);
        QCOMPARE(restored.preferences().value("visualMode").toString(), QString("pulse"));
        QVERIFY(restored.preferences().value("studyVisuals").toBool());
        QCOMPARE(restored.bookmarks(url).first().toMap().value("note").toString(), QString("Updated idea\nA question to revisit"));
        const auto exported = dir.filePath("notes.txt"); QVERIFY(restored.exportNotes(url, QUrl::fromLocalFile(exported)));
        QFile noteFile(exported); QVERIFY(noteFile.open(QIODevice::ReadOnly)); QVERIFY(noteFile.readAll().contains("0:05  Updated idea\nA question to revisit"));
        QFile srt(dir.filePath("lesson.srt")); QVERIFY(srt.open(QIODevice::WriteOnly));
        srt.write("2\r\n00:00:03,250 --> 00:00:04,000\r\n<b>Second</b> line\r\n\r\n1\r\n00:00:00,500 --> 00:00:02,000\r\nFirst &amp; clear\r\nnext line\r\n\r\n3\r\n00:00:05,000 --> 00:00:04,000\r\nInvalid interval\r\n"); srt.close();
        const auto cues = store.readSubtitles(QUrl::fromLocalFile(srt.fileName())); QCOMPARE(cues.size(), 2);
        QCOMPARE(cues.first().toMap().value("start").toInt(), 500);
        QCOMPARE(cues.first().toMap().value("text").toString(), QString("First & clear\nnext line"));
        QCOMPARE(cues.last().toMap().value("text").toString(), QString("Second line"));
        QFile vtt(dir.filePath("lesson.vtt")); QVERIFY(vtt.open(QIODevice::WriteOnly));
        vtt.write("WEBVTT\n\nNOTE ignored comment\n\ncue-1\n00:01.000 --> 00:03.500 align:center\nCaption\n\ncue-2\n00:99:01.000 --> 00:99:03.000\nInvalid time\n"); vtt.close();
        const auto web = store.readSubtitles(QUrl::fromLocalFile(vtt.fileName())); QCOMPARE(web.size(), 1);
        QCOMPARE(web.first().toMap().value("end").toInt(), 3500);
        QSignalSpy errors(&store, &MediaStore::errorOccurred);
        QVERIFY(store.readSubtitles(QUrl("https://example.com/captions.srt")).isEmpty()); QCOMPARE(errors.size(), 1);
        QFile huge(dir.filePath("huge.srt")); QVERIFY(huge.open(QIODevice::WriteOnly)); huge.write(QByteArray(5 * 1024 * 1024 + 1, 'x')); huge.close();
        QVERIFY(store.readSubtitles(QUrl::fromLocalFile(huge.fileName())).isEmpty()); QCOMPARE(errors.size(), 2);
    }
    void playlistBatchImportsAndReportsResults() {
        QTemporaryDir dir; const auto state = dir.filePath("state.json");
        const auto one = dir.filePath("one.mp3"), two = dir.filePath("two.mp4");
        for (const auto &path : {one, two}) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("sample"); }
        const QUrl first = QUrl::fromLocalFile(one), second = QUrl::fromLocalFile(two), missing = QUrl::fromLocalFile(dir.filePath("missing.mp3"));
        MediaStore store(nullptr, state); const auto id = store.createPlaylist("Mixed");
        QSignalSpy changes(&store, &MediaStore::playlistsChanged);
        const auto result = store.addToPlaylistBatch(id, {first, second, first, missing, QUrl("https://example.com/music.mp3")}, true);
        QCOMPARE(result.value("status").toString(), QString("ok")); QCOMPARE(result.value("added").toInt(), 2);
        QCOMPARE(result.value("duplicates").toInt(), 1); QCOMPARE(result.value("failed").toInt(), 2);
        QCOMPARE(store.playlistItems(id).size(), 2); QCOMPARE(changes.size(), 1);
        const auto again = store.addToPlaylistBatch(id, {first, second}, true);
        QCOMPARE(again.value("added").toInt(), 0); QCOMPARE(again.value("duplicates").toInt(), 2); QCOMPARE(changes.size(), 1);
        MediaStore restored(nullptr, state); QCOMPARE(restored.playlistItems(id).size(), 2);
        QCOMPARE(restored.addToPlaylist(id, first), QString("duplicate"));
        const auto count = store.media().size();
        QCOMPARE(store.addToPlaylistBatch("deleted", {missing}, true).value("status").toString(), QString("missingPlaylist"));
        QCOMPARE(store.media().size(), count);
        // Simulate a storage failure after creation; failed membership rolls back.
        const auto blocked = dir.filePath("blocked.json"); MediaStore unwritable(nullptr, blocked);
        unwritable.importFiles({first}); const auto list = unwritable.createPlaylist("Cannot save");
        QVERIFY(QFile::remove(blocked)); QVERIFY(QDir().mkdir(blocked));
        const auto failure = unwritable.addToPlaylistBatch(list, {first});
        QCOMPARE(failure.value("status").toString(), QString("saveError")); QCOMPARE(failure.value("added").toInt(), 0);
        QVERIFY(unwritable.playlistItems(list).isEmpty());
        QVERIFY(QFile::exists(one)); QVERIFY(QFile::exists(two));
    }
    void securityLimitsAndHistory() {
        QTemporaryDir dir;
        const auto mediaPath = dir.filePath("song.mp3"), state = dir.filePath("state.json");
        QFile media(mediaPath); QVERIFY(media.open(QIODevice::WriteOnly)); media.write("original media"); media.close();
        const auto url = QUrl::fromLocalFile(mediaPath);
        MediaStore store(nullptr, state); store.importFiles({url}); store.updateProgress(url, 500, 1000);
        store.toggleFavorite(url); const auto id = store.createPlaylist("Keep me"); store.addToPlaylist(id, url); store.addBookmark(url, 100, "Keep this note");
        QVERIFY(!store.exportNotes(url, url));
        QVERIFY(media.open(QIODevice::ReadOnly)); QCOMPARE(media.readAll(), QByteArray("original media")); media.close();
        QVERIFY(store.clearPlaybackHistory());
        QCOMPARE(store.entry(url).value("position").toLongLong(), 0LL); QCOMPARE(store.entry(url).value("lastPlayed").toLongLong(), 0LL);
        QVERIFY(store.entry(url).value("favorite").toBool()); QCOMPARE(store.playlistItems(id).size(), 1); QCOMPARE(store.bookmarks(url).size(), 1);
        MediaStore restored(nullptr, state); QCOMPARE(restored.entry(url).value("position").toLongLong(), 0LL);
        QVERIFY(!(QFile::permissions(state) & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
        // Oversized persisted JSON cannot drive unbounded readAll/parsing.
        QFile large(dir.filePath("large.json")); QVERIFY(large.open(QIODevice::WriteOnly)); QVERIFY(large.resize(16 * 1024 * 1024 + 1)); large.close();
        MediaStore refused(nullptr, large.fileName()); QVERIFY(refused.media().isEmpty());
        refused.createPlaylist("Do not overwrite the refused library"); QCOMPARE(QFileInfo(large.fileName()).size(), 16 * 1024 * 1024 + 1LL);
        QFile bigSubtitles(dir.filePath("large.srt")); QVERIFY(bigSubtitles.open(QIODevice::WriteOnly)); QVERIFY(bigSubtitles.resize(5 * 1024 * 1024 + 1)); bigSubtitles.close();
        QVERIFY(store.readSubtitles(QUrl::fromLocalFile(bigSubtitles.fileName())).isEmpty());
        QFile malformedCue(dir.filePath("malformed.srt")); QVERIFY(malformedCue.open(QIODevice::WriteOnly));
        malformedCue.write("1\n00:00:01,000 --> 00:00:02,000\n"); malformedCue.write(QByteArray(100000, '<')); malformedCue.close();
        QCOMPARE(store.readSubtitles(QUrl::fromLocalFile(malformedCue.fileName())).size(), 1);
        QFile unsafe(dir.filePath("unsafe.json")); QVERIFY(unsafe.open(QIODevice::WriteOnly));
        unsafe.write("{\"version\":1,\"media\":[{\"url\":\"https://example.org/remote.mp4\"},{\"url\":\"file://server/share/video.mp4\"}]}"); unsafe.close();
        MediaStore filtered(nullptr, unsafe.fileName()); QVERIFY(filtered.media().isEmpty());
        store.updateProgress(url, 200, 1000); QVERIFY(QFile::remove(state)); QVERIFY(QDir().mkdir(state));
        QVERIFY(!store.clearPlaybackHistory()); QCOMPARE(store.entry(url).value("position").toLongLong(), 200LL);
    }
    void damagedStorageAndWriteFailure() {
        QTemporaryDir dir;
        QFile damaged(dir.filePath("broken.json")); QVERIFY(damaged.open(QIODevice::WriteOnly)); damaged.write("{bad json"); damaged.close();
        MediaStore store(nullptr, damaged.fileName()); QVERIFY(store.media().isEmpty());
        const auto blocker = dir.filePath("not-a-directory"); QFile file(blocker); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        MediaStore unwritable(nullptr, blocker + "/library.json"); QSignalSpy errors(&unwritable, &MediaStore::errorOccurred);
        unwritable.createPlaylist("Example"); QCOMPARE(errors.count(), 1);
    }
};
QTEST_GUILESS_MAIN(MediaStoreTest)
#include "MediaStoreTest.moc"
