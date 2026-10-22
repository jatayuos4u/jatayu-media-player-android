#pragma once
#include <QTranslator>
#include <QHash>
#include <QQmlEngine>
#include <QPointer>
class InterfaceLanguage : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
public:
    explicit InterfaceLanguage(QObject *parent=nullptr);
    QString language()const{return m_language;}
    void setLanguage(const QString &);
    void setEngine(QQmlEngine *engine){m_engine=engine;}
    bool isEmpty()const override{return false;}
    QString translate(const char *,const char *source,const char *,int n)const override;
signals:
    void languageChanged();
private:
    QString m_language="en";QHash<QString,QString> m_hindi;QPointer<QQmlEngine> m_engine;
};
