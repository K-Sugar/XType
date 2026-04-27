#include "recent_events_model.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

RecentEventsModel::RecentEventsModel(QObject* parent)
    : QObject(parent),
      _path(QDir::homePath() + "/.local/share/xtype/recent_events.json")
{
    if (QFile::exists(_path))
        _watcher.addPath(_path);
    const QString dir = QDir::homePath() + "/.local/share/xtype";
    if (QDir(dir).exists())
        _watcher.addPath(dir);
    connect(&_watcher, &QFileSystemWatcher::fileChanged,
            this, &RecentEventsModel::loadFile);
    connect(&_watcher, &QFileSystemWatcher::directoryChanged,
            this, &RecentEventsModel::loadFile);
    loadFile();
}

QVariantMap RecentEventsModel::first() const {
    return _events.isEmpty() ? QVariantMap{} : _events.first().toMap();
}

void RecentEventsModel::refresh() { loadFile(); }

void RecentEventsModel::loadFile() {
    QFile f(_path);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) return;
    _events.clear();
    for (const auto& val : doc.array())
        if (val.isObject())
            _events.append(val.toObject().toVariantMap());
    if (!_watcher.files().contains(_path) && QFile::exists(_path))
        _watcher.addPath(_path);
    emit eventsChanged();
}
