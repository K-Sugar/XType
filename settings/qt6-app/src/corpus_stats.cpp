#include "corpus_stats.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

CorpusStats::CorpusStats(QObject *parent) : QObject(parent) {
    _timeout.setSingleShot(true);
    _timeout.setInterval(5000);
    connect(&_timeout, &QTimer::timeout, this, &CorpusStats::onTimeout);
    connect(&_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CorpusStats::onFinished);
    _proc.setProcessChannelMode(QProcess::MergedChannels);

    refresh();
}

void CorpusStats::refresh() {
    if (_running) return;
    _running = true;
    _proc.start("xtype-corpus", {"stats", "--json"});
    if (!_proc.waitForStarted(500)) {
        _running = false;
        return;  // binary not found — stats stay at 0
    }
    _timeout.start();
}

void CorpusStats::onFinished(int code, QProcess::ExitStatus) {
    _timeout.stop();
    _running = false;

    if (code != 0) return;

    const QByteArray raw = _proc.readAll();
    const QJsonObject obj = QJsonDocument::fromJson(raw).object();
    if (obj.isEmpty()) return;

    const int s = obj.value("sentences").toInt();
    const int w = obj.value("words").toInt();
    const double m = obj.value("mb").toDouble();

    if (s != _sentences || w != _words || m != _mb) {
        _sentences = s;
        _words     = w;
        _mb        = m;
        emit statsChanged();
    }
}

void CorpusStats::onTimeout() {
    _proc.kill();
    _running = false;
}
