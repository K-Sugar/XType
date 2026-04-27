#include "ollama_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// Static model metadata table — UI-PLAN §U1.3
static const QMap<QString, QVariantMap> kModelMeta = {
    {"qwen2.5:1.5b",  {{"name","qwen2.5:1.5b"},  {"family","Qwen"},   {"size_mb",974},  {"quant","Q4_K_M"}, {"throughput_estimate_tps",42}}},
    {"qwen2.5:3b",    {{"name","qwen2.5:3b"},    {"family","Qwen"},   {"size_mb",1900}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",28}}},
    {"qwen2.5:7b",    {{"name","qwen2.5:7b"},    {"family","Qwen"},   {"size_mb",4700}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",14}}},
    {"qwen3:1.7b",    {{"name","qwen3:1.7b"},    {"family","Qwen3"},  {"size_mb",1100}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",38}}},
    {"qwen3:4b",      {{"name","qwen3:4b"},      {"family","Qwen3"},  {"size_mb",2500}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",22}}},
    {"gemma3:1b",     {{"name","gemma3:1b"},     {"family","Gemma3"}, {"size_mb",815},  {"quant","Q8"},     {"throughput_estimate_tps",60}}},
    {"gemma3:4b",     {{"name","gemma3:4b"},     {"family","Gemma3"}, {"size_mb",3300}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",18}}},
    {"llama3.2:1b",   {{"name","llama3.2:1b"},   {"family","Llama"},  {"size_mb",1300}, {"quant","Q8"},     {"throughput_estimate_tps",55}}},
    {"llama3.2:3b",   {{"name","llama3.2:3b"},   {"family","Llama"},  {"size_mb",2000}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",32}}},
    {"phi3.5:mini",   {{"name","phi3.5:mini"},   {"family","Phi"},    {"size_mb",2200}, {"quant","Q4_K_M"}, {"throughput_estimate_tps",30}}},
};

OllamaClient::OllamaClient(QObject *parent) : QObject(parent) {
    _timeout.setSingleShot(true);
    _timeout.setInterval(2000);
    connect(&_timeout, &QTimer::timeout, this, &OllamaClient::onTimeout);
    connect(&_proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OllamaClient::onFinished);
    _proc.setProcessChannelMode(QProcess::MergedChannels);

    refresh();
}

QVariantMap OllamaClient::activeMeta(const QString &name) const {
    auto it = kModelMeta.find(name);
    if (it != kModelMeta.end()) return it.value();
    // Unknown model: return minimal entry
    return {{"name", name}, {"family", "Unknown"}, {"size_mb", 0},
            {"quant", "?"}, {"throughput_estimate_tps", 0}};
}

void OllamaClient::refresh() {
    if (_running) return;
    _running = true;
    _proc.start("ollama", {"list"});
    _timeout.start();
}

void OllamaClient::onFinished(int code, QProcess::ExitStatus) {
    _timeout.stop();
    _running = false;

    if (code != 0) {
        _models.clear();
        emit availableModelsChanged();
        return;
    }

    const QString out = QString::fromUtf8(_proc.readAll());
    QStringList result;
    for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
        // ollama list output: NAME  ID  SIZE  MODIFIED
        const QString name = line.section(QChar(' '), 0, 0).trimmed();
        if (!name.isEmpty() && name != "NAME")
            result.append(name);
    }
    if (result != _models) {
        _models = result;
        emit availableModelsChanged();
    }
}

void OllamaClient::onTimeout() {
    _proc.kill();
    _running = false;
    if (!_models.isEmpty()) {
        _models.clear();
        emit availableModelsChanged();
    }
}
