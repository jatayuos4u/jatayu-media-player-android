#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "MediaStore.h"

class MediaIdentityTest : public QObject {
    Q_OBJECT
    const QUrl selected{"content://com.android.providers.downloads.documents/document/msf%3A77"};
    const QUrl indexed{"content://media/external/audio/media/77"};
    QVariantMap catalog(const QUrl &url = QUrl("content://media/external/audio/media/77"), const QString &volume = "external_primary") {
        return {{"url", url.toString(QUrl::FullyEncoded)}, {"kind", "audio"}, {"title", "Same name.mp3"}, {"duration", 10000},
            {"size", 12345}, {"volumeName", volume}, {"folderKey", volume + ":Music/"}, {"dateAdded", 1000}};
    }
    MediaStore::IdentityResolver resolver() {
        return [this](const QUrl &url) -> QVariantMap {
            return url == selected ? QVariantMap{{"mediaIdentity", "audio:external_primary:77"}} : QVariantMap{};
        };
    }
private slots:
    void addThenScanPreservesReferencesAndProgress() {
        QTemporaryDir dir; const auto state = dir.filePath("state.json");
        MediaStore store(nullptr, state, resolver());
        store.importFiles({selected}); store.updateProgress(selected, 3500, 10000); store.toggleFavorite(selected);
        const auto list = store.createPlaylist("Saved"); store.addToPlaylist(list, selected); store.addBookmark(selected, 2000, "Keep note");
        store.mergeCatalog({catalog()}, {"audio:external_primary"});
        QCOMPARE(store.media().size(), 1); QCOMPARE(store.entry(selected).value("url").toString(), indexed.toString());
        QCOMPARE(store.entry(indexed).value("position").toInt(), 3500); QVERIFY(store.entry(indexed).value("favorite").toBool());
        QCOMPARE(store.playlistItems(list).size(), 1); QCOMPARE(store.bookmarks(indexed).size(), 1); QCOMPARE(store.bookmarks(selected).size(), 1);
        store.updateProgress(selected, 4000, 10000); QCOMPARE(store.entry(indexed).value("position").toInt(), 4000);
        QCOMPARE(store.addToPlaylistBatch(list, {selected, indexed}).value("duplicates").toInt(), 2);
        for (int i = 0; i < 3; ++i) { store.mergeCatalog({catalog()}, {"audio:external_primary"}); store.importFiles({selected, indexed}); }
        QCOMPARE(store.media().size(), 1); QCOMPARE(store.playlistItems(list).size(), 1);
        MediaStore reload(nullptr, state, resolver()); QCOMPARE(reload.media().size(), 1);
        QCOMPARE(reload.entry(selected).value("position").toInt(), 4000); QCOMPARE(reload.bookmarks(indexed).size(), 1);
        QVERIFY(!reload.exportNotes(indexed, selected));
        reload.forget(selected); QVERIFY(reload.media().isEmpty()); QVERIFY(reload.playlistItems(list).isEmpty()); QVERIFY(reload.bookmarks(indexed).isEmpty());
    }
    void scanThenAddIsAlsoIdempotent() {
        QTemporaryDir dir; MediaStore store(nullptr, dir.filePath("state.json"), resolver());
        store.mergeCatalog({catalog()}); store.updateProgress(indexed, 5000, 10000);
        QCOMPARE(store.importFiles({selected}), indexed.toString());
        QCOMPARE(store.media().size(), 1); QCOMPARE(store.entry(selected).value("position").toInt(), 5000);
        QCOMPARE(store.entry(selected).value("kind").toString(), QString("audio"));
    }
    void repairsLegacyLibraryUsingFreshDescriptorIdentity() {
        QTemporaryDir dir; const auto state = dir.filePath("legacy.json");
        auto picked = catalog(selected); picked.remove("volumeName"); picked.remove("folderKey"); picked["position"] = 6500; picked["lastPlayed"] = 2000; picked["favorite"] = true;
        auto scanned = catalog(); scanned["catalogId"] = "audio:77"; scanned["catalogManaged"] = true;
        scanned["position"] = 1000; scanned["lastPlayed"] = 1000;
        const auto a = selected.toString(QUrl::FullyEncoded), b = indexed.toString();
        const QVariantMap noteA{{"id", "same-id"}, {"position", 1000}, {"note", "First note"}};
        const QVariantMap noteB{{"id", "same-id"}, {"position", 2000}, {"note", "Different note"}};
        QFile file(state); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject::fromVariantMap({{"version", 1}, {"media", QVariantList{picked, scanned}},
            {"playlists", QVariantList{QVariantMap{{"id", "legacy-list"}, {"name", "Keep"}, {"urls", QVariantList{a, b}}}}},
            {"bookmarks", QVariantMap{{a, QVariantList{noteA}}, {b, QVariantList{noteB}}}}})).toJson()); file.close();
        MediaStore store(nullptr, state); QCOMPARE(store.media().size(), 2);
        auto fresh = catalog(); fresh["fileIdentity"] = "1:200:12345:900:900";
        store.mergeCatalog({fresh}, {"audio:external_primary"}, {QVariantMap{{"url", a}, {"fileIdentity", "1:200:12345:900:900"}}});
        QCOMPARE(store.media().size(), 1); QCOMPARE(store.playlistItems("legacy-list").size(), 1);
        QCOMPARE(store.entry(indexed).value("position").toInt(), 6500); QVERIFY(store.entry(indexed).value("favorite").toBool());
        const auto notes = store.bookmarks(selected); QCOMPARE(notes.size(), 2);
        QVERIFY(notes.first().toMap().value("id") != notes.last().toMap().value("id"));
        MediaStore reload(nullptr, state); QCOMPARE(reload.media().size(), 1); QCOMPARE(reload.bookmarks(indexed).size(), 2);
    }
    void namesSizesAndUnavailableIdentityNeverProveDuplicates() {
        QTemporaryDir dir; MediaStore store(nullptr, dir.filePath("state.json"));
        store.importFiles({selected});
        auto second = catalog(QUrl("content://media/external/audio/media/78"));
        store.mergeCatalog({catalog(), second}, {}, {QVariantMap{{"url", selected.toString(QUrl::FullyEncoded)}}});
        QCOMPARE(store.media().size(), 3);
        store.mergeCatalog({catalog(), second}); QCOMPARE(store.media().size(), 3);
    }
    void sameRowIdOnDifferentVolumesRemainsDistinct() {
        QTemporaryDir dir; MediaStore store(nullptr, dir.filePath("state.json"));
        const QUrl card("content://media/1234-5678/audio/media/77");
        store.mergeCatalog({catalog(), catalog(card, "1234-5678")}, {"audio:external_primary", "audio:1234-5678"});
        QCOMPARE(store.media().size(), 2);
        store.mergeCatalog({catalog()}, {"audio:external_primary", "audio:1234-5678"});
        QCOMPARE(store.media().size(), 1); QVERIFY(store.entry(card).isEmpty()); QVERIFY(!store.entry(indexed).isEmpty());
    }
    void localPathAliasesDoNotMergeSeparateCopies() {
        QTemporaryDir dir; QDir().mkpath(dir.filePath("other"));
        const auto path = dir.filePath("same.mp3"), link = dir.filePath("alias.mp3"), copy = dir.filePath("other/same.mp3");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("audio bytes"); file.close();
        QVERIFY(QFile::link(path, link)); QVERIFY(QFile::copy(path, copy));
        MediaStore store(nullptr, dir.filePath("state.json"));
        store.importFiles({QUrl::fromLocalFile(path), QUrl::fromLocalFile(link), QUrl::fromLocalFile(copy)});
        QCOMPARE(store.media().size(), 2);
        QCOMPARE(store.entry(QUrl::fromLocalFile(path)).value("url"), store.entry(QUrl::fromLocalFile(link)).value("url"));
        store.forget(QUrl::fromLocalFile(link)); QCOMPARE(store.media().size(), 1);
        QVERIFY(QFile::exists(path)); QVERIFY(QFile::exists(link)); QVERIFY(QFile::exists(copy));
    }
};
QTEST_GUILESS_MAIN(MediaIdentityTest)
#include "MediaIdentityTest.moc"
