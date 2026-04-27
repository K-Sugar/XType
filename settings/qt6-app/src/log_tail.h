#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QVariantList>

class LogTail : public QObject {
    Q_OBJECT

public:
    explicit LogTail(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList bufferedLines() const;

signals:
    void lineAdded(const QString &text, const QString &kind);

private slots:
    void onFileChanged(const QString &path);

private:
    void tryOpen();
    QString classify(const QString &line) const;

    QFileSystemWatcher _watcher;
    QString            _watchedPath;
    qint64             _offset = 0;

    // Ring buffer of last 200 lines: { "text": ..., "kind": ... }
    QList<QVariantMap> _buffer;
};
