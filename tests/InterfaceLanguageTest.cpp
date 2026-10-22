#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include "InterfaceLanguage.h"
class InterfaceLanguageTest:public QObject {
 Q_OBJECT
private slots:
 void translateAndRestore(){
    InterfaceLanguage language;
    QCOMPARE(QCoreApplication::translate("Main","Settings"),QString("Settings"));
    language.setLanguage("hi");QCOMPARE(QCoreApplication::translate("Main","Settings"),QString::fromUtf8("सेटिंग"));
    QCOMPARE(QCoreApplication::translate("Main","%1 files added").arg(4),QString::fromUtf8("4 फ़ाइलें जोड़ी गईं"));
    QCOMPARE(QCoreApplication::translate("Main","Artist's original filename.mp3"),QString("Artist's original filename.mp3"));
    language.setLanguage("en");QCOMPARE(QCoreApplication::translate("Main","Settings"),QString("Settings"));
    language.setLanguage("unknown");QCOMPARE(language.language(),QString("en"));
 }
 void placeholdersArePreserved(){
    QFile f(":/android/i18n/hi.json");QVERIFY(f.open(QIODevice::ReadOnly));const auto map=QJsonDocument::fromJson(f.readAll()).object();QVERIFY(map.size()>320);
    QRegularExpression placeholder("%[1-9n]");
    for(auto it=map.begin();it!=map.end();++it){QVERIFY(!it.value().toString().isEmpty());QStringList a,b;
        auto matches=placeholder.globalMatch(it.key());while(matches.hasNext())a<<matches.next().captured();
        matches=placeholder.globalMatch(it.value().toString());while(matches.hasNext())b<<matches.next().captured();a.sort();b.sort();QCOMPARE(a,b);
    }
 }
};
QTEST_GUILESS_MAIN(InterfaceLanguageTest)
#include "InterfaceLanguageTest.moc"
