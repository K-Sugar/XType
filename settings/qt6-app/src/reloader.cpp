#include "reloader.h"
#include <QDebug>

Reloader::Reloader(QObject *parent)
    : QObject(parent)
{
    // Step 1 complete: fire the xtype-specific addon reload.
    connect(&_proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int, QProcess::ExitStatus) {
                _dbusProc.start("dbus-send", {
                    "--session",
                    "--type=method_call",
                    "--dest=org.fcitx.Fcitx5",
                    "/controller",
                    "org.fcitx.Fcitx.Controller1.ReloadAddonConfig",
                    "string:xtype"
                });
                if (!_dbusProc.waitForStarted(500))
                    emit reloadFinished(false);
            });

    // Step 2 complete: report success/failure.
    connect(&_dbusProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus) {
                emit reloadFinished(code == 0);
            });
}

void Reloader::reload() {
    if (_proc.state() != QProcess::NotRunning ||
        _dbusProc.state() != QProcess::NotRunning) {
        qWarning() << "Reloader: reload already in progress";
        return;
    }
    emit reloadStarted();
    _proc.start("fcitx5-remote", {"-r"});
    if (!_proc.waitForStarted(500)) {
        emit reloadFinished(false);
    }
}
