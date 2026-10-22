#include <QtTest>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFile>
#include <QTemporaryDir>
#include <QJSValue>
#include <QQuickStyle>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMediaPlayer>
#include <functional>
#include "MediaStore.h"
#include "ArtworkProvider.h"
#include "AudioVisualizer.h"
#include "SoundStudio.h"
#include "PlaybackDecks.h"
#include "InterfaceLanguage.h"
#include "MediaSource.h"
#include "PlaybackSession.h"
#include <fcntl.h>
#include <unistd.h>

class UiSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void providerSourcesPreserveQmlState() {
        const auto mp3=qEnvironmentVariable("JATAYU_TEST_MP3"), video=qEnvironmentVariable("JATAYU_TEST_VIDEO");
        if(mp3.isEmpty()||video.isEmpty())QSKIP("Supply generated MP3/MP4 fixtures");
        QTemporaryDir dir;MediaStore store(nullptr,dir.filePath("library.json"));
        QQuickStyle::setStyle(QStringLiteral("Material"));
        PlaybackDecks decks(nullptr,[&](const QUrl &uri){
            const auto path=uri.path().contains("/video/")?video:mp3;
            return ::open(QFile::encodeName(path).constData(),O_RDONLY);
        },dir.filePath("studio.ini"));
        PlaybackSession playbackSession;
        InterfaceLanguage languages;
        QQmlApplicationEngine engine;
        languages.setEngine(&engine);
        connect(&store,&MediaStore::preferencesChanged,&languages,[&]{languages.setLanguage(store.preferences().value("language").toString());});
        engine.setInitialProperties({{"languages",QVariant::fromValue(&languages)}, {"playbackSession",QVariant::fromValue(&playbackSession)}, {"store",QVariant::fromValue(&store)},{"decks",QVariant::fromValue(&decks)}});
        engine.addImageProvider("artwork",new ArtworkProvider);
        engine.load(QUrl::fromLocalFile(QStringLiteral(JATAYU_SOURCE_DIR "/android/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());auto window=engine.rootObjects().first();
        const QUrl audioUri("content://media/external/audio/media/700"),videoUri("content://media/external/video/media/701");
        store.mergeCatalog({QVariantMap{{"url",audioUri.toString()},{"kind","audio"},{"title","Provider song.mp3"}},
                            QVariantMap{{"url",videoUri.toString()},{"kind","video"},{"title","Provider video.mp4"}}});
        store.setPreference("autoAdvance",false);
        auto audioEntry=store.entry(audioUri);
        QVERIFY(QMetaObject::invokeMethod(window,"openMedia",Q_ARG(QVariant,QVariant(audioEntry)),Q_ARG(QVariant,QVariant(QVariantList{audioEntry}))));
        auto player=qvariant_cast<QMediaPlayer *>(window->property("playbackEngine"));QVERIFY(player);
        QTRY_VERIFY_WITH_TIMEOUT(player->hasAudio()&&player->duration()>0,10000);
        QCOMPARE(player->property("source").toUrl(),audioUri);QCOMPARE(player->error(),QMediaPlayer::NoError);
        QVERIFY(player->sourceDevice());QVERIFY(window->property("hasMedia").toBool());
        store.addBookmark(audioUri,500,"Provider note");QTRY_COMPARE(window->property("marked").toList().size(),1);
        auto videoEntry=store.entry(videoUri);
        QVERIFY(QMetaObject::invokeMethod(window,"openMedia",Q_ARG(QVariant,QVariant(videoEntry)),Q_ARG(QVariant,QVariant(QVariantList{videoEntry}))));
        QCOMPARE(player->property("source").toUrl(),videoUri);
        QTRY_VERIFY_WITH_TIMEOUT(player->hasVideo(),10000);QCOMPARE(player->error(),QMediaPlayer::NoError);
        auto surface=window->findChild<QQuickItem *>("videoSurface");QVERIFY(surface);
        auto output=qvariant_cast<QObject *>(surface->property("output"));QVERIFY(output);
        QTRY_COMPARE_WITH_TIMEOUT(output->property("sourceRect").toRectF().size(),QSizeF(1920,1080),10000);
        player->setProperty("source",QUrl());QVERIFY(!player->sourceDevice());QVERIFY(!window->property("hasMedia").toBool());
    }
    void crossfadeUpdatesQueueAndKeepsStudio(){
        const auto first=qEnvironmentVariable("JATAYU_TEST_MP3"),second=qEnvironmentVariable("JATAYU_TEST_CROSSFADE_MP3");
        if(first.isEmpty()||second.isEmpty())QSKIP("Supply crossfade fixtures");
        QTemporaryDir dir;MediaStore store(nullptr,dir.filePath("library.json"));PlaybackDecks decks(nullptr,{},dir.filePath("studio.ini"));PlaybackSession playbackSession;InterfaceLanguage languages;
        store.setPreference("crossfade",3);store.setPreference("resume",false);store.importFiles({QUrl::fromLocalFile(first),QUrl::fromLocalFile(second)});
        QQmlApplicationEngine engine;languages.setEngine(&engine);
        engine.setInitialProperties({{"languages",QVariant::fromValue(&languages)},{"decks",QVariant::fromValue(&decks)},{"playbackSession",QVariant::fromValue(&playbackSession)},{"store",QVariant::fromValue(&store)}});
        engine.addImageProvider("artwork",new ArtworkProvider);engine.load(QUrl::fromLocalFile(QStringLiteral(JATAYU_SOURCE_DIR "/android/qml/Main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());auto window=engine.rootObjects().first();
        auto a=store.entry(QUrl::fromLocalFile(first)),b=store.entry(QUrl::fromLocalFile(second));QVERIFY(!a.isEmpty()&&!b.isEmpty());
        QVERIFY(QMetaObject::invokeMethod(window,"openMedia",Q_ARG(QVariant,QVariant(a)),Q_ARG(QVariant,QVariant(QVariantList{a,b}))));
        decks.studio()->setEnabled(true);decks.studio()->applyPreset("Ghazal");auto outgoing=decks.player();
        QTRY_VERIFY_WITH_TIMEOUT(outgoing->position()>400&&outgoing->duration()>10000,10000);
        QVERIFY(QMetaObject::invokeMethod(window,"seekTo",Q_ARG(QVariant,QVariant(outgoing->duration()-4500))));
        QTRY_VERIFY_WITH_TIMEOUT(decks.player()!=outgoing,10000);
        QCOMPARE(window->property("current").toMap().value("url").toString(),b.value("url").toString());QCOMPARE(window->property("queueIndex").toInt(),1);
        QCOMPARE(decks.studio()->preset(),QString("Ghazal"));QVERIFY(decks.studio()->music());QVERIFY(decks.studio()->enabled());
        QTRY_VERIFY_WITH_TIMEOUT(!decks.fading(),5000);QVERIFY(decks.player()->position()>2000);QCOMPARE(window->property("page").toString(),QString("music"));
        playbackSession.pause();QCOMPARE(outgoing->playbackState(),QMediaPlayer::StoppedState);
    }
    void pagesRenderAndPersist() {
        QTemporaryDir dir;
        MediaStore store(nullptr, dir.filePath("library.json"));
        QQuickStyle::setStyle(QStringLiteral("Material"));
        PlaybackDecks decks(nullptr,{},dir.filePath("sound-studio.ini"));
        auto &soundStudio=*decks.studio();auto &visualizer=*decks.visualizer();
        PlaybackSession playbackSession;
        InterfaceLanguage languages;
        QQmlApplicationEngine engine;
        languages.setEngine(&engine);
        connect(&store,&MediaStore::preferencesChanged,&languages,[&]{languages.setLanguage(store.preferences().value("language").toString());});
        QSignalSpy qmlWarnings(&engine, &QQmlEngine::warnings);
        engine.setInitialProperties({{"languages",QVariant::fromValue(&languages)}, {"playbackSession",QVariant::fromValue(&playbackSession)}, {QStringLiteral("store"), QVariant::fromValue(&store)}, {QStringLiteral("decks"), QVariant::fromValue(&decks)}});
        engine.addImageProvider("artwork", new ArtworkProvider);
        engine.load(QUrl::fromLocalFile(QStringLiteral(JATAYU_SOURCE_DIR "/android/qml/Main.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not load");
        auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        // ListView delegates follow visual parenting, which can differ from QObject ownership.
        const std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual = [&](QQuickItem *root, const QString &name) -> QQuickItem * {
            if (root->objectName() == name) return root;
            for (auto child : root->childItems()) if (auto found = findVisual(child, name)) return found;
            return nullptr;
        };
        window->resize(420, 860);
        QTest::qWait(500);
        const auto capture = [&](const QString &name) {
            QTest::qWait(300);
            // The headless host has no audio output; dismiss its playback toast
            // for layout captures. QML engine warnings still fail the test.
            window->setProperty("notice", "");
            const auto image = window->grabWindow();
            QVERIFY2(!image.isNull(), "Empty rendered UI");
            const auto folder = qEnvironmentVariable("JATAYU_SCREENSHOT_DIR");
            if (!folder.isEmpty()) { QDir().mkpath(folder); QVERIFY(image.save(folder + '/' + name + ".png")); }
        };
        capture("library-empty");
        window->setProperty("page", "about"); capture("about-phone");
        auto creator = window->findChild<QObject *>("creatorName"); QVERIFY(creator);
        QCOMPARE(creator->property("text").toString(), QString("Anup Kumar Yadav"));
        auto logo = window->findChild<QObject *>("aboutBrandLogo"); QVERIFY(logo);
        QTRY_COMPARE(logo->property("status").toInt(), 1);
        window->resize(360, 720); capture("about-small-phone");
        window->resize(1180, 720); capture("about-tablet");
        window->setProperty("page", "library"); window->resize(420, 860);
        window->resize(360, 720); capture("library-small-phone");
        window->resize(420, 860);
        // Actual generated PCM file exercises the real import/playback path;
        // it is test data only and never seeds the shipped app's library.
        QFile sample(dir.filePath("Playback test.wav")); QVERIFY(sample.open(QIODevice::WriteOnly));
        QByteArray wave = QByteArray::fromHex("5249464624ac000057415645666d742010000000010001002256000044ac0000020010006461746100ac0000");
        wave.append(QByteArray(44032, '\0')); sample.write(wave); sample.close();
        store.importFiles({QUrl::fromLocalFile(sample.fileName())});
        store.mergeCatalog({
            QVariantMap{{"url", "content://media/external/video/media/300"}, {"kind", "video"}, {"title", "Device video.mp4"}, {"duration", 180000}, {"dateAdded", 2000}, {"folder", "DCIM/Camera/"}, {"folderKey", "primary:DCIM/Camera/"}, {"size", 12000000}, {"height", 1080}},
            QVariantMap{{"url", "content://media/external/audio/media/301"}, {"kind", "audio"}, {"title", "Device song.mp3"}, {"duration", 240000}, {"dateAdded", 1000}, {"folder", "Music/"}, {"folderKey", "primary:Music/"}, {"artist", "Test artist"}, {"album", "Test album"}, {"size", 3000000}}
        });
        store.updateProgress(QUrl("content://media/external/video/media/300"), 45000, 180000);
        capture("library-imported");
        window->setProperty("tab", "Folders"); QVERIFY(window->property("showGroups").toBool()); capture("folders");
        window->setProperty("tab", "Music"); window->setProperty("musicBrowse", "Artists"); capture("artists");
        window->setProperty("browseSelection", "Test artist");
        QCOMPARE(engine.newQObject(window).property("visibleMedia").property("length").toInt(), 1);
        capture("artist-songs");
        window->setProperty("browseSelection", ""); window->setProperty("tab", "Continue");
        store.setPreference("autoAdvance", false);
        const auto item = store.media().first();
        QVERIFY(QMetaObject::invokeMethod(window, "openMedia", Q_ARG(QVariant, item), Q_ARG(QVariant, QVariant(store.media()))));
        QTRY_COMPARE(window->property("page").toString(), QString("music"));
        capture("music");
        auto musicVolume=window->findChild<QObject *>("musicVolumeSlider");QVERIFY(musicVolume);
        musicVolume->setProperty("value",.37);QVERIFY(QMetaObject::invokeMethod(musicVolume,"moved"));
        QVERIFY(qAbs(window->property("playerVolume").toDouble()-.37)<.001);
        QVERIFY(qAbs(store.preferences().value("volume").toDouble()-.37)<.001);
        auto mute=window->findChild<QObject *>("musicMuteButton");QVERIFY(mute);
        QVERIFY(QMetaObject::invokeMethod(mute,"clicked"));QCOMPARE(window->property("playerVolume").toDouble(),0.);
        QVERIFY(QMetaObject::invokeMethod(mute,"clicked"));QVERIFY(qAbs(window->property("playerVolume").toDouble()-.37)<.001);
        capture("music-volume");
        QVERIFY(QMetaObject::invokeMethod(window,"showSoundStudio"));
        soundStudio.applyPreset("Bollywood");
        capture("sound-studio-phone");
        auto presets=window->findChild<QObject *>("studioPresets");QVERIFY(presets);
        QCOMPARE(presets->property("displayText").toString(),QString("Bollywood"));
        soundStudio.applyPreset("Ghazal");QCOMPARE(presets->property("displayText").toString(),QString("Ghazal"));
        soundStudio.setStrength(60);
        auto saveSound=window->findChild<QObject *>("saveSoundProfileDialog");QVERIFY(saveSound);
        QVERIFY(QMetaObject::invokeMethod(saveSound,"open"));QTRY_VERIFY(saveSound->property("opened").toBool());
        auto soundName=window->findChild<QObject *>("soundProfileName");QVERIFY(soundName);
        soundName->setProperty("text","Evening headphones");
        auto soundSave=window->findChild<QObject *>("saveSoundProfileButton");QVERIFY(soundSave);QVERIFY(soundSave->property("enabled").toBool());
        capture("sound-studio-save-profile");QVERIFY(QMetaObject::invokeMethod(soundSave,"clicked"));QTest::qWait(150);
        QTRY_VERIFY(!saveSound->property("visible").toBool());
        QCOMPARE(presets->property("displayText").toString(),QString("Evening headphones"));
        capture("sound-studio-personal");
        soundStudio.applyPreset("Bollywood");
        auto studioPanel=window->findChild<QObject *>("soundStudioPanel");QVERIFY(studioPanel);
        auto studioVolume=window->findChild<QObject *>("studioVolumeSlider");QVERIFY(studioVolume);
        QVERIFY(qAbs(studioVolume->property("value").toDouble()-.37)<.001);
        auto advanced=window->findChild<QObject *>("studioAdvanced");QVERIFY(advanced);
        advanced->setProperty("checked",true);
        window->resize(360,720);capture("sound-studio-small-phone");
        window->resize(1180,800);capture("sound-studio-tablet");
        auto studioScroll=window->findChild<QObject *>("studioScroll");QVERIFY(studioScroll);
        auto flickable=qvariant_cast<QObject *>(studioScroll->property("contentItem"));QVERIFY(flickable);
        flickable->setProperty("contentY",flickable->property("contentHeight").toReal()-flickable->property("height").toReal());
        capture("sound-studio-tablet-equalizer");
        window->resize(360,720);QTest::qWait(100);
        flickable->setProperty("contentY",flickable->property("contentHeight").toReal()-flickable->property("height").toReal());
        capture("sound-studio-phone-equalizer");

        QVERIFY(QMetaObject::invokeMethod(studioPanel,"close"));
        window->resize(420,860);

        window->setProperty("page", "study");
        store.addBookmark(QUrl::fromLocalFile(sample.fileName()), 500, "Practice this passage");
        QTRY_COMPARE(window->property("marked").toList().size(), 1);
        capture("study-phone");
        window->resize(360, 800); capture("study-small-phone");
        window->resize(740, 360); capture("study-landscape-phone");
        window->resize(1180, 720); capture("study-tablet");
        window->setProperty("page", "player"); capture("player-tablet");
        window->resize(420, 860); capture("player-phone");
        auto surface = window->findChild<QQuickItem *>("videoSurface"); QVERIFY(surface);
        auto lock = window->findChild<QObject *>("lockControlsButton"); QVERIFY(lock);
        QVERIFY(QMetaObject::invokeMethod(lock, "clicked")); QVERIFY(surface->property("locked").toBool());
        capture("player-locked");
        auto unlock = window->findChild<QObject *>("unlockControlsButton"); QVERIFY(unlock);
        QVERIFY(QMetaObject::invokeMethod(unlock, "clicked")); QVERIFY(!surface->property("locked").toBool());
        auto gestures = window->findChild<QQuickItem *>("videoGestureArea"); QVERIFY(gestures);
        window->setProperty("playerVolume", 0.4);
        const auto start = gestures->mapToScene(QPointF(gestures->width() * 0.8, gestures->height() * 0.44)).toPoint();
        const auto end = gestures->mapToScene(QPointF(gestures->width() * 0.8, gestures->height() * 0.24)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, end, 30);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(window->property("playerVolume").toReal() > 0.4);
        capture("player-volume-gesture");
        window->resize(740, 360); capture("player-landscape-phone");
        QVERIFY(qAbs(surface->width() - window->width()) < 1);
        QVERIFY(qAbs(surface->height() - window->height()) < 1);
        auto fit=window->findChild<QObject *>("videoFitButton");QVERIFY(fit);
        QVERIFY(QMetaObject::invokeMethod(fit,"clicked"));QVERIFY(surface->property("cropToFill").toBool());
        QVERIFY(QMetaObject::invokeMethod(fit,"clicked"));QVERIFY(!surface->property("cropToFill").toBool());
        auto rotate = window->findChild<QObject *>("rotateVideoButton"); QVERIFY(rotate);
        QVERIFY(QMetaObject::invokeMethod(rotate, "clicked"));
        QCOMPARE(window->property("playbackOrientation").toInt(), 2);
        window->resize(420, 860);
        QTRY_VERIFY(!window->property("landscape").toBool());
        QVERIFY(QMetaObject::invokeMethod(rotate, "clicked"));
        QCOMPARE(window->property("playbackOrientation").toInt(), 1);
        auto fullscreen = window->findChild<QObject *>("fullscreenButton"); QVERIFY(fullscreen);
        QVERIFY(QMetaObject::invokeMethod(fullscreen, "clicked"));
        QVERIFY(window->property("immersivePlayback").toBool());
        QCOMPARE(window->property("playbackOrientation").toInt(), 1);
        window->resize(1180, 720); QTest::qWait(100);
        QVERIFY(qAbs(surface->width() - window->width()) < 1);
        window->setProperty("page", "library");
        QVERIFY(!window->property("immersivePlayback").toBool());
        QCOMPARE(window->property("playbackOrientation").toInt(), 0);
        const auto videoPath = qEnvironmentVariable("JATAYU_TEST_VIDEO");
        if (!videoPath.isEmpty()) {
            QVERIFY(QFile::exists(videoPath));
            const auto videoUrl = QUrl::fromLocalFile(videoPath);
            store.importFiles({videoUrl});
            const QVariant videoEntry = store.entry(videoUrl);
            QVERIFY(QMetaObject::invokeMethod(window, "openMedia", Q_ARG(QVariant, videoEntry), Q_ARG(QVariant, QVariant(QVariantList{videoEntry}))));
            auto output = qvariant_cast<QObject *>(surface->property("output")); QVERIFY(output);
            QTRY_COMPARE_WITH_TIMEOUT(output->property("sourceRect").toRectF().size(), QSizeF(1920, 1080), 10000);
            auto sink = qvariant_cast<QVideoSink *>(output->property("videoSink")); QVERIFY(sink);
            QTRY_VERIFY_WITH_TIMEOUT(sink->videoFrame().isValid(), 10000);
            QCOMPARE(sink->videoFrame().size(), QSize(1920, 1080));
            // Exercise finite passage repetition against a real seekable source.
            QTRY_VERIFY(qvariant_cast<QObject *>(window->property("playbackEngine"))->property("seekable").toBool());
            window->setProperty("repeatLimit", 3);
            auto host = engine.newQObject(window); engine.globalObject().setProperty("testWindow", host);
            auto began = engine.evaluate("testWindow.beginRepeat(500, 2000)"); QVERIFY(!began.isError()); QVERIFY(began.toBool());
            QTRY_VERIFY_WITH_TIMEOUT(!window->property("repeatActive").toBool(), 12000);
            QCOMPARE(window->property("repeatCompleted").toInt(), 3);
            QVERIFY(!window->property("repeatActive").toBool());
            QCOMPARE(qvariant_cast<QObject *>(window->property("playbackEngine"))->property("playbackState").toInt(), 2); // PausedState
            // Lookup handles gaps/end times, and captions never become rich text.
            window->setProperty("externalCues", QVariantList{QVariantMap{{"start", 500}, {"end", 2000}, {"text", "<plain caption>"}}});
            QCOMPARE(engine.evaluate("testWindow.captionAt(1000)").toString(), QString("<plain caption>"));
            QCOMPARE(engine.evaluate("testWindow.captionAt(2000)").toString(), QString());
            window->setProperty("externalEnabled", true);
            window->setProperty("page", "study"); window->resize(420, 860); capture("study-video-notes");
            const QVariant note = QVariantMap{{"position", 750}, {"note", "Practice pronunciation"}};
            QVERIFY(QMetaObject::invokeMethod(window, "editStudyNote", Q_ARG(QVariant, note)));
            auto noteDialog = window->findChild<QObject *>("studyNoteDialog"); QVERIFY(noteDialog);
            auto noteText = window->findChild<QObject *>("studyNoteText"); QVERIFY(noteText);
            noteText->setProperty("text", "Practice pronunciation\nRepeat slowly.");
            QVERIFY(QMetaObject::invokeMethod(noteDialog, "accepted"));
            QVERIFY(QMetaObject::invokeMethod(noteDialog, "close"));
            QCOMPARE(store.bookmarks(videoUrl).size(), 1);
            auto savedNote = store.bookmarks(videoUrl).first();
            QVERIFY(QMetaObject::invokeMethod(window, "editStudyNote", Q_ARG(QVariant, savedNote)));
            noteText->setProperty("text", "Edited study note");
            QVERIFY(QMetaObject::invokeMethod(noteDialog, "accepted"));
            QVERIFY(QMetaObject::invokeMethod(noteDialog, "close"));
            QCOMPARE(store.bookmarks(videoUrl).size(), 1);
            QCOMPARE(store.bookmarks(videoUrl).first().toMap().value("note").toString(), QString("Edited study note"));
            capture("study-video-saved-note");
            window->setProperty("page", "player");
            QVERIFY(!sink->videoFrame().toImage().isNull());
            QVERIFY(QMetaObject::invokeMethod(fullscreen, "clicked"));
            window->resize(740, 360); capture("real-hd-video-fullscreen");
            QVERIFY(qAbs(surface->width() - window->width()) < 1);
            playbackSession.handleAction("pip-enter",0);QVERIFY(window->property("pipActive").toBool());
            window->resize(240,135);capture("pip-layout-simulation");QVERIFY(!surface->property("fullControls").toBool());QVERIFY(qAbs(surface->width()-240)<1);
            playbackSession.handleAction("pip-exit",0);QVERIFY(!window->property("pipActive").toBool());
            QVERIFY(window->property("immersivePlayback").toBool());QCOMPARE(window->visibility(),QWindow::FullScreen);
            QVERIFY(QMetaObject::invokeMethod(window,"setFullscreen",Q_ARG(QVariant,QVariant(false))));
            for(const QSize size : {QSize(360,740),QSize(740,360),QSize(420,860)}){
                auto player=decks.player();const auto source=player->source();
                playbackSession.handleAction("pip-enter",0);window->resize(240,135);QTest::qWait(40);
                // Stale layout work must never expand a still-visible PiP.
                playbackSession.handleAction("pip-restore",(qint64(999)<<32)|999);QCOMPARE(window->width(),240);
                playbackSession.handleAction("pip-exit",0);
                const auto dpr=window->devicePixelRatio();
                playbackSession.handleAction("pip-restore",(qint64(qRound(size.width()*dpr))<<32)|quint32(qRound(size.height()*dpr)));
                QTRY_COMPARE(window->size(),size);QTRY_COMPARE(qRound(surface->width()),size.width());
                QTRY_COMPARE(qRound(surface->height()),size.height());QVERIFY(surface->property("fullControls").toBool());QCOMPARE(player->source(),source);
            }
            capture("pip-return-phone");window->resize(740,360);
        }
        const auto lessonId = store.createPlaylist("Lessons");
        const auto secondId = store.createPlaylist("Other playlist");
        window->setProperty("selectedPlaylist", lessonId); window->setProperty("selectedPlaylistName", "Lessons");
        window->setProperty("page", "collection");
        auto addFiles = window->findChild<QObject *>("addPlaylistFiles"); QVERIFY(addFiles);
        QVERIFY(QMetaObject::invokeMethod(addFiles, "clicked"));
        auto picker = window->findChild<QObject *>("mediaFilePicker"); QVERIFY(picker);
        QCOMPARE(picker->property("targetPlaylist").toString(), lessonId);
        const auto sourceBeforeAdd = qvariant_cast<QObject *>(window->property("playbackEngine"))->property("source");
        const QVariant picked = QVariant::fromValue(QList<QUrl>{QUrl::fromLocalFile(sample.fileName())});
        QVERIFY(picker->setProperty("selectedFile", QUrl::fromLocalFile(sample.fileName())));
        QVERIFY(QMetaObject::invokeMethod(picker, "accepted"));
        QMetaObject::invokeMethod(picker, "close");
        QCOMPARE(store.playlistItems(lessonId).size(), 1); QCOMPARE(store.playlistItems(secondId).size(), 0);
        QCOMPARE(window->property("collectionMedia").toList().size(), 1);
        QCOMPARE(window->property("page").toString(), QString("collection"));
        QCOMPARE(qvariant_cast<QObject *>(window->property("playbackEngine"))->property("source"), sourceBeforeAdd);
        QVERIFY(window->property("notice").toString().contains("1 file added"));
        QVERIFY(QMetaObject::invokeMethod(window, "handlePickedMedia", Q_ARG(QVariant, picked), Q_ARG(QVariant, QVariant(lessonId))));
        QCOMPARE(store.playlistItems(lessonId).size(), 1); QVERIFY(window->property("notice").toString().contains("already in playlist"));
        window->resize(420, 860); capture("playlist-contents");
        // Existing-library menu uses its captured entry rather than a later menu item.
        QVERIFY(QMetaObject::invokeMethod(window, "choosePlaylistForEntry", Q_ARG(QVariant, item)));
        window->setProperty("selectedEntry", QVariantMap{});
        QTest::qWait(100);
        auto destination = findVisual(window->contentItem(), "playlistDestination_" + secondId); QVERIFY(destination);
        QVERIFY(QMetaObject::invokeMethod(destination, "clicked"));
        QCOMPARE(store.playlistItems(secondId).size(), 1);
        // Header + on Playlists asks for a destination before opening the picker.
        window->setProperty("page", "playlists");
        auto headerAdd = window->findChild<QObject *>("headerAddMedia"); QVERIFY(headerAdd);
        QVERIFY(QMetaObject::invokeMethod(headerAdd, "clicked"));
        auto chooser = window->findChild<QObject *>("playlistDestinationDialog"); QVERIFY(chooser);
        QCOMPARE(chooser->property("purpose").toString(), QString("import"));
        QTest::qWait(100);
        destination = findVisual(window->contentItem(), "playlistDestination_" + lessonId); QVERIFY(destination);
        QVERIFY(QMetaObject::invokeMethod(destination, "clicked"));
        QCOMPARE(picker->property("targetPlaylist").toString(), lessonId);
        QVERIFY(QMetaObject::invokeMethod(picker, "rejected")); QMetaObject::invokeMethod(picker, "close");
        QCOMPARE(picker->property("targetPlaylist").toString(), QString());
        MediaStore saved(nullptr, dir.filePath("library.json")); QCOMPARE(saved.playlistItems(lessonId).size(), 1);
        capture("playlist-imported");
        window->resize(420, 860); window->setProperty("page", "playlists"); capture("playlists");
        window->setProperty("page", "settings"); capture("settings");
        const auto visualMp3 = qEnvironmentVariable("JATAYU_TEST_VISUAL_MP3");
        if (!visualMp3.isEmpty()) {
            const auto musicUrl = QUrl::fromLocalFile(visualMp3);
            store.importFiles({musicUrl});
            const QVariant musicEntry = store.entry(musicUrl);
            auto enginePlayer = qvariant_cast<QMediaPlayer *>(window->property("playbackEngine")); QVERIFY(enginePlayer);
            enginePlayer->setAudioOutput(nullptr); // Headless host; retain the same decoded-buffer path.
            QVERIFY(QMetaObject::invokeMethod(window, "openMedia", Q_ARG(QVariant, musicEntry), Q_ARG(QVariant, QVariant(QVariantList{musicEntry}))));
            window->requestActivate();
            QTRY_VERIFY(visualizer.enabled()); QVERIFY(enginePlayer->audioBufferOutput());
            QTRY_VERIFY(visualizer.available()); QTRY_VERIFY(visualizer.level() > 0.005);
            store.setPreference("visualMode", "bars"); capture("music-bars-phone");
            store.setPreference("visualMode", "pulse"); capture("music-pulse-phone");
            window->resize(360, 720); capture("music-pulse-small-phone");
            store.setPreference("visualMode", "artwork"); QTRY_VERIFY(!visualizer.enabled());
            QCOMPARE(enginePlayer->playbackState(), QMediaPlayer::PlayingState);
            store.setPreference("visualMode", "bars"); QTRY_VERIFY(visualizer.enabled() && visualizer.level() > 0.005);
            enginePlayer->pause(); QTRY_VERIFY(!visualizer.processing()); QCOMPARE(visualizer.level(), 0.0);
            enginePlayer->play(); QTRY_VERIFY(visualizer.enabled());
            window->setProperty("page", "study"); QTRY_VERIFY(!visualizer.enabled());
            store.setPreference("studyVisuals", true); QTRY_VERIFY(visualizer.enabled()); capture("study-audio-bars");
            store.setPreference("studyVisuals", false); QTRY_VERIFY(!visualizer.enabled());
            QVERIFY(QMetaObject::invokeMethod(window, "setVisualizerFullscreen", Q_ARG(QVariant, QVariant(true))));
            QTRY_COMPARE(window->property("page").toString(), QString("visualizer"));
            QTRY_VERIFY(visualizer.enabled());
            window->resize(1180, 720); store.setPreference("visualMode", "pulse"); capture("music-pulse-fullscreen");
            QVERIFY(QMetaObject::invokeMethod(window, "setVisualizerFullscreen", Q_ARG(QVariant, QVariant(false))));
            QTRY_COMPARE(window->property("page").toString(), QString("music"));
            QVERIFY(!window->property("immersivePlayback").toBool());
            window->setProperty("page", "library"); QTRY_VERIFY(!visualizer.enabled());
        }
        // A scan can canonicalise the current file while its old URI is still
        // playing. Keep that source, collapse queue aliases and keep its index.
        const auto aliasUrl = QUrl::fromLocalFile(sample.fileName());
        const QVariant aliasEntry = store.entry(aliasUrl);
        QVERIFY(QMetaObject::invokeMethod(window, "openMedia", Q_ARG(QVariant, aliasEntry), Q_ARG(QVariant, QVariant(QVariantList{aliasEntry, aliasEntry}))));
        auto activePlayer = qvariant_cast<QMediaPlayer *>(window->property("playbackEngine")); QVERIFY(activePlayer); activePlayer->pause();
        const auto sourceBeforeReconcile = activePlayer->source();
        store.mergeCatalog({QVariantMap{{"url", "content://media/external/audio/media/900"}, {"kind", "audio"},
            {"title", "Playback test.wav"}, {"volumeName", "external_primary"}, {"fileIdentity", "ui:verified-file"}}}, {},
            {QVariantMap{{"url", aliasUrl.toString(QUrl::FullyEncoded)}, {"fileIdentity", "ui:verified-file"}}});
        QCOMPARE(activePlayer->source(), sourceBeforeReconcile);
        QCOMPARE(window->property("queue").toList().size(), 1); QCOMPARE(window->property("queueIndex").toInt(), 0);
        QCOMPARE(window->property("current").toMap().value("url").toString(), QString("content://media/external/audio/media/900"));
        window->setProperty("page", "settings");
        auto historyButton = window->findChild<QObject *>("clearHistoryButton"); QVERIFY(historyButton);
        QVERIFY(QMetaObject::invokeMethod(historyButton, "clicked"));
        auto historyDialog = window->findChild<QObject *>("clearHistoryDialog"); QVERIFY(historyDialog);
        const auto membershipBeforeClear = store.playlistItems(lessonId).size();
        QVERIFY(QMetaObject::invokeMethod(historyDialog, "accepted")); QMetaObject::invokeMethod(historyDialog, "close");
        QVERIFY(qvariant_cast<QObject *>(window->property("playbackEngine"))->property("source").toUrl().isEmpty());
        for (const auto &media : store.media()) {
            QCOMPARE(media.toMap().value("position").toLongLong(), 0LL);
            QCOMPARE(media.toMap().value("lastPlayed").toLongLong(), 0LL);
        }
        QCOMPARE(store.playlistItems(lessonId).size(), membershipBeforeClear);
        QVERIFY(QFile::exists(sample.fileName()));
        QMetaObject::invokeMethod(window, "saveProgress");
        const QUrl sampleUrl = QUrl::fromLocalFile(sample.fileName());
        const QVariant sampleEntry = store.entry(sampleUrl);
        QVERIFY(QMetaObject::invokeMethod(window, "openMedia", Q_ARG(QVariant, sampleEntry), Q_ARG(QVariant, QVariant(QVariantList{sampleEntry}))));
        store.forget(sampleUrl);
        QTRY_VERIFY(!window->property("hasMedia").toBool());
        QCOMPARE(window->property("queue").toList().size(), 0);
        QCOMPARE(window->property("page").toString(), QString("library"));
        QVERIFY(QFile::exists(sample.fileName()));
        store.setPreference("language","hi");QCOMPARE(languages.language(),QString("hi"));
        QCOMPARE(QCoreApplication::translate("Main","Settings"),QString::fromUtf8("सेटिंग"));
        window->resize(420,860);window->setProperty("page","library");capture("hindi-library-phone");
        window->setProperty("page","settings");capture("hindi-settings-phone");
        auto languageChoice=window->findChild<QObject *>("interfaceLanguage");QVERIFY(languageChoice);QCOMPARE(languageChoice->property("currentIndex").toInt(),1);
        auto fadeChoice=window->findChild<QObject *>("crossfadeDuration");QVERIFY(fadeChoice);fadeChoice->setProperty("currentIndex",3);QVERIFY(QMetaObject::invokeMethod(fadeChoice,"activated",Q_ARG(int,3)));QCOMPARE(store.preferences().value("crossfade").toInt(),3);
        window->resize(1180,720);capture("hindi-settings-tablet");
        window->setProperty("page","about");capture("hindi-about-tablet");
        { MediaStore restored(nullptr,dir.filePath("library.json"));QCOMPARE(restored.preferences().value("language").toString(),QString("hi"));QCOMPARE(restored.preferences().value("crossfade").toInt(),3); }
        store.setPreference("language","en");QCOMPARE(languages.language(),QString("en"));
        for (const auto &arguments : qmlWarnings) {
            const auto warnings = qvariant_cast<QList<QQmlError>>(arguments.first());
            for (const auto &warning : warnings) qWarning().noquote() << warning.toString();
        }
        QCOMPARE(qmlWarnings.size(), 0);
    }
};
QTEST_MAIN(UiSmokeTest)
#include "UiSmokeTest.moc"

