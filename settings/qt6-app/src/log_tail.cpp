#include "log_tail.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

static QString findLogPath() {
    const QString base = QDir::homePath() + "/.local/share/xtype/";
    static const QLatin1StringView kNames[] = {QLatin1StringView("fcitx5.log"), QLatin1StringView("engine.log"), QLatin1StringView("debug.log")};
    for (const auto &name : kNames) {
        const QString p = base + name;
        if (QFile::exists(p)) return p;
    }
    return base + "fcitx5.log";  // doesn't exist yet — watcher will pick it up when created
}

LogTail::LogTail(QObject *parent) : QObject(parent) {
    connect(&_watcher, &QFileSystemWatcher::fileChanged,
            this, &LogTail::onFileChanged);
    tryOpen();
}

void LogTail::tryOpen() {
    const QString path = findLogPath();
    if (path == _watchedPath) return;

    if (!_watchedPath.isEmpty())
        _watcher.removePath(_watchedPath);

    _watchedPath = path;
    _offset = 0;

    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        // seek to end so we only tail new lines
        _offset = f.size();
        f.close();
    }

    _watcher.addPath(path);
}

QVariantList LogTail::bufferedLines() const {
    QVariantList out;
    for (const QVariantMap &m : _buffer)
        out.append(m);
    return out;
}

void LogTail::onFileChanged(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        // File may have been rotated; re-find
        tryOpen();
        return;
    }

    if (f.size() < _offset) {
        // Rotation — restart from beginning
        _offset = 0;
    }

    f.seek(_offset);
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (line.isEmpty()) continue;
        const QString kind = classify(line);
        QVariantMap entry{{"text", line}, {"kind", kind}};
        if (_buffer.size() >= 200)
            _buffer.removeFirst();
        _buffer.append(entry);
        emit lineAdded(line, kind);
    }
    _offset = f.pos();
}

QString LogTail::classify(const QString &line) const {
    const QString lower = line.toLower();
    if (lower.contains("suggest accepted") || lower.contains("accepted"))
        return "accent";
    if (lower.contains("error") || lower.contains("fail") || lower.contains("warn"))
        return "err";
    return "normal";
}
