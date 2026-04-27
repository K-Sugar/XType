#include "engine_probe.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <ctime>
#include <unistd.h>

static uint64_t nowNs() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

EngineProbe::EngineProbe(QObject *parent)
    : QObject(parent)
{
    _clkTck      = sysconf(_SC_CLK_TCK);
    _demoMetrics = qEnvironmentVariableIsSet("XTYPE_DEMO_METRICS");

    _timer.setInterval(1500);
    _timer.setSingleShot(false);
    connect(&_timer, &QTimer::timeout, this, &EngineProbe::poll);
    _timer.start();
    poll();  // first tick immediately
}

// Find the PID of `ollama serve` (or ollama in any invocation) by scanning /proc.
// Returns -1 if not found.
int EngineProbe::findOllamaPid() {
    if (_demoMetrics) return static_cast<int>(getpid());

    const QDir procDir("/proc");
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        int pid = entry.toInt(&ok);
        if (!ok || pid <= 0) continue;

        QFile comm("/proc/" + entry + "/comm");
        if (!comm.open(QIODevice::ReadOnly)) continue;
        const QByteArray name = comm.readAll().trimmed();
        if (name == "ollama") return pid;
    }
    return -1;
}

void EngineProbe::sampleCpu(int pid) {
    if (pid < 0) {
        if (_cpuPct != 0) { _cpuPct = 0; emit cpuPctChanged(); }
        _lastPid = -1;
        return;
    }

    const QString statPath = QString("/proc/%1/stat").arg(pid);
    QFile f(statPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (_cpuPct != 0) { _cpuPct = 0; emit cpuPctChanged(); }
        return;
    }
    const QByteArray raw = f.readAll();
    // Find the last ')' to skip the comm field which may contain spaces
    const int rparen = raw.lastIndexOf(')');
    if (rparen < 0 || rparen + 2 >= raw.size()) return;

    const QByteArray rest = raw.mid(rparen + 2);  // skip ") "
    const QList<QByteArray> fields = rest.split(' ');
    // fields[11] = utime, fields[12] = stime (0-indexed from state)
    if (fields.size() < 13) return;

    bool ok1, ok2;
    const uint64_t utime = fields[11].toULongLong(&ok1);
    const uint64_t stime = fields[12].toULongLong(&ok2);
    if (!ok1 || !ok2) return;

    const uint64_t totalTicks = utime + stime;
    const uint64_t wallNs     = nowNs();

    if (_lastPid == pid && _lastWallNs > 0) {
        const double deltaTicks = static_cast<double>(totalTicks - _lastCpuTicks);
        const double deltaSecs  = static_cast<double>(wallNs - _lastWallNs) / 1e9;
        if (deltaSecs > 0.0) {
            const int pct = static_cast<int>(
                (deltaTicks / static_cast<double>(_clkTck)) / deltaSecs * 100.0
            );
            const int clamped = qBound(0, pct, 100);
            if (_cpuPct != clamped) { _cpuPct = clamped; emit cpuPctChanged(); }
        }
    }

    _lastPid       = pid;
    _lastCpuTicks  = totalTicks;
    _lastWallNs    = wallNs;
}

void EngineProbe::sampleRam(int pid) {
    if (pid < 0) {
        if (_ramGb != 0.0) { _ramGb = 0.0; emit ramGbChanged(); }
        return;
    }

    QFile f(QString("/proc/%1/status").arg(pid));
    if (!f.open(QIODevice::ReadOnly)) {
        if (_ramGb != 0.0) { _ramGb = 0.0; emit ramGbChanged(); }
        return;
    }

    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (line.startsWith("VmRSS:")) {
            // Format: "VmRSS:    123456 kB"
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2) {
                bool ok;
                const double kb = parts[1].toDouble(&ok);
                if (ok) {
                    const double gb = kb / (1024.0 * 1024.0);
                    if (qAbs(_ramGb - gb) > 0.005) { _ramGb = gb; emit ramGbChanged(); }
                }
            }
            break;
        }
    }
}

void EngineProbe::checkFcitx5State() {
    // Kill any stale process from a previous tick that didn't finish
    if (_stateProc.state() != QProcess::NotRunning) {
        _stateProc.kill();
        _stateProc.waitForFinished(200);
    }

    _stateProc.start("fcitx5-remote", {"--check"});
    if (!_stateProc.waitForStarted(300)) {
        // Binary missing or not executable
        const QString msg = "fcitx5 not found";
        if (_state != "error" || _stateMessage != msg) {
            _state = "error"; _stateMessage = msg;
            emit stateChanged(); emit stateMessageChanged();
        }
        return;
    }

    const bool finished = _stateProc.waitForFinished(800);
    if (!finished) {
        _stateProc.kill();
        _stateProc.waitForFinished(200);
        // Don't change state — fcitx5 is probably starting up
        return;
    }

    const int code = _stateProc.exitCode();
    const QString newState = (code == 0) ? "ready" : "paused";
    if (_state != newState || !_stateMessage.isEmpty()) {
        _state = newState;
        _stateMessage = {};
        emit stateChanged();
        emit stateMessageChanged();
    }
}

void EngineProbe::poll() {
    const int pid = findOllamaPid();
    sampleCpu(pid);
    sampleRam(pid);
    checkFcitx5State();

    QFile mf(QDir::homePath() + "/.local/share/xtype/metrics.json");
    if (mf.open(QIODevice::ReadOnly)) {
        QByteArray data = mf.readAll();
        mf.close();
        QRegularExpression re(R"("latency_p50_ms"\s*:\s*(\d+))");
        auto m = re.match(QString::fromUtf8(data));
        if (m.hasMatch()) {
            int p50 = m.captured(1).toInt();
            if (p50 != _latencyP50) {
                _latencyP50 = p50;
                emit latencyP50Changed();
            }
        }
    }
}
