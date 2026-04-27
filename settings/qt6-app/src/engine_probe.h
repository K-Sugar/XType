#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <cstdint>

class EngineProbe : public QObject {
    Q_OBJECT
    Q_PROPERTY(int     cpuPct       READ cpuPct       NOTIFY cpuPctChanged)
    Q_PROPERTY(double  ramGb        READ ramGb        NOTIFY ramGbChanged)
    Q_PROPERTY(QString state        READ state        NOTIFY stateChanged)
    Q_PROPERTY(QString stateMessage READ stateMessage NOTIFY stateMessageChanged)
    Q_PROPERTY(int     latencyP50   READ latencyP50   CONSTANT)

public:
    explicit EngineProbe(QObject *parent = nullptr);

    int     cpuPct()       const { return _cpuPct; }
    double  ramGb()        const { return _ramGb; }
    QString state()        const { return _state; }
    QString stateMessage() const { return _stateMessage; }
    int     latencyP50()   const { return 0; }  // scaffold; engine doesn't yet export this via IPC

signals:
    void cpuPctChanged();
    void ramGbChanged();
    void stateChanged();
    void stateMessageChanged();

private slots:
    void poll();

private:
    int   findOllamaPid();
    void  sampleCpu(int pid);
    void  sampleRam(int pid);
    void  checkFcitx5State();

    QTimer   _timer;
    QProcess _stateProc;  // reused for fcitx5-remote -s

    int     _cpuPct        = 0;
    double  _ramGb         = 0.0;
    QString _state         = "paused";
    QString _stateMessage;

    // CPU delta tracking
    int         _lastPid      = -1;
    uint64_t    _lastCpuTicks = 0;
    uint64_t    _lastWallNs   = 0;
    long        _clkTck       = 100;  // sysconf(_SC_CLK_TCK), set in ctor
    bool        _demoMetrics  = false;
};
