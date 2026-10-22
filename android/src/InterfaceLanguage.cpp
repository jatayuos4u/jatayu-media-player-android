#include "InterfaceLanguage.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
InterfaceLanguage::InterfaceLanguage(QObject *parent):QTranslator(parent){
    QFile file(":/android/i18n/hi.json");if(file.open(QIODevice::ReadOnly)){const auto entries=QJsonDocument::fromJson(file.readAll()).object();for(auto it=entries.begin();it!=entries.end();++it)m_hindi.insert(it.key(),it.value().toString());}
}
QString InterfaceLanguage::translate(const char *,const char *source,const char *,int)const{return m_language=="hi"?m_hindi.value(QString::fromUtf8(source)):QString();}
void InterfaceLanguage::setLanguage(const QString &language){
    const QString next=language=="hi"?"hi":"en";if(next==m_language)return;
    QCoreApplication::removeTranslator(this);m_language=next;if(next=="hi")QCoreApplication::installTranslator(this);
    if(m_engine)m_engine->retranslate();emit languageChanged();
}
