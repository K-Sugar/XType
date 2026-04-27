#pragma once

#include <QObject>
#include <QProcess>

class Reloader : public QObject {
    Q_OBJECT

public:
    explicit Reloader(QObject *parent = nullptr);

    Q_INVOKABLE void reload();

signals:
    void reloadStarted();
    void reloadFinished(bool ok);

private:
    QProcess _proc;
};
