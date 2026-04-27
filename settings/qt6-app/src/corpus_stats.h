#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

class CorpusStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(int    sentences READ sentences NOTIFY statsChanged)
    Q_PROPERTY(int    words     READ words     NOTIFY statsChanged)
    Q_PROPERTY(double mb        READ mb        NOTIFY statsChanged)

public:
    explicit CorpusStats(QObject *parent = nullptr);

    int    sentences() const { return _sentences; }
    int    words()     const { return _words; }
    double mb()        const { return _mb; }

    Q_INVOKABLE void refresh();

signals:
    void statsChanged();

private slots:
    void onFinished(int code, QProcess::ExitStatus status);
    void onTimeout();

private:
    int     _sentences = 0;
    int     _words     = 0;
    double  _mb        = 0.0;
    QProcess _proc;
    QTimer   _timeout;
    bool     _running  = false;
};
