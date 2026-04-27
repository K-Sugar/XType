#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class RecentEventsModel : public QObject {
    Q_OBJECT

public:
    explicit RecentEventsModel(QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap first() const;
    Q_INVOKABLE void refresh();

signals:
    void eventsChanged();

private:
    void loadFile();

    QVariantList       _events;
    QFileSystemWatcher _watcher;
    QString            _path;
};
