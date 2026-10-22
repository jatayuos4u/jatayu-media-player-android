#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCoreApplication>
#include <QQmlContext>
#include <QQuickStyle>
#include "MediaStore.h"
#include "ArtworkProvider.h"
#include "AudioVisualizer.h"
#include "SoundStudio.h"
#include "MediaSource.h"
#include "PlaybackSession.h"
#include "PlaybackDecks.h"
#include "InterfaceLanguage.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("JatayuOS"));
    QCoreApplication::setApplicationName(QStringLiteral("Jatayu Media Player"));

    MediaStore mediaStore;
    QCoreApplication::setApplicationVersion(mediaStore.appVersion());
    QQuickStyle::setStyle(QStringLiteral("Material"));
    PlaybackDecks decks;
    PlaybackSession playbackSession;
    InterfaceLanguage languages;
    QQmlApplicationEngine engine;
    languages.setEngine(&engine);languages.setLanguage(mediaStore.preferences().value("language").toString());
    QObject::connect(&mediaStore,&MediaStore::preferencesChanged,&languages,[&]{languages.setLanguage(mediaStore.preferences().value("language").toString());});
    QObject::connect(&languages,&InterfaceLanguage::languageChanged,decks.studio(),&SoundStudio::settingsChanged);
    engine.setInitialProperties({{QStringLiteral("languages"),QVariant::fromValue(&languages)}, {QStringLiteral("playbackSession"), QVariant::fromValue(&playbackSession)}, {QStringLiteral("store"), QVariant::fromValue(&mediaStore)}, {QStringLiteral("decks"), QVariant::fromValue(&decks)}});
    engine.addImageProvider(QStringLiteral("artwork"), new ArtworkProvider);
    engine.loadFromModule("Jatayu.Android", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}

