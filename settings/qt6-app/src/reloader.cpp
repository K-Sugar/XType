#include "reloader.h"
#include <QDebug>

Reloader::Reloader(QObject *parent)
    : QObject(parent)
{
    connect(&_proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus) {
                emit reloadFinished(code == 0);
            });
}

void Reloader::reload() {
    if (_proc.state() != QProcess::NotRunning) {
        qWarning() << "Reloader: reload already in progress";
        return;
    }
    emit reloadStarted();
    _proc.start("fcitx5-remote", {"-r"});
    if (!_proc.waitForStarted(500)) {
        emit reloadFinished(false);
    }
}
