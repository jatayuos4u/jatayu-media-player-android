#pragma once
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <functional>
#include <memory>
class QMediaPlayer;
class QFile;

// Keep the Android provider descriptor alive for the whole decoder lifetime.
class MediaSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    using DescriptorOpener = std::function<int(const QUrl &)>;
    explicit MediaSource(QObject *parent = nullptr, DescriptorOpener opener = {});
    ~MediaSource() override;
    QString error() const { return m_error; }
    Q_INVOKABLE bool open(QObject *player, const QUrl &url);
signals:
    void errorChanged();
private:
    void detach();
    void setError(const QString &error);
    DescriptorOpener m_opener;
    QPointer<QMediaPlayer> m_player;
    std::unique_ptr<QFile> m_file;
    QString m_error;
    bool m_changing = false;
};
