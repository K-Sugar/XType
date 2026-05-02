#include "config_store.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QVariant>
#include <QDebug>

#define TOML_EXCEPTIONS 0
#include "toml/toml.hpp"

static QString defaultConfigPath() {
    return QDir::homePath() + "/.config/xtype/config.toml";
}

// -------------------------------------------------------------------------
// helpers: Qt ↔ toml++ conversions
// -------------------------------------------------------------------------
static QStringList tomlArrayToStringList(const toml::array *arr) {
    QStringList out;
    if (!arr) return out;
    for (auto &elem : *arr)
        if (auto *s = elem.as_string())
            out.append(QString::fromStdString(s->get()));
    return out;
}

static void writeStringArray(toml::table &tbl, std::string_view key, const QStringList &list) {
    toml::array arr;
    for (const QString &s : list)
        arr.push_back(s.toStdString());
    tbl.insert_or_assign(key, std::move(arr));
}

// -------------------------------------------------------------------------
ConfigStore::ConfigStore(const QString &configPath, QObject *parent)
    : QObject(parent)
    , _configPath(configPath.isEmpty() ? defaultConfigPath() : configPath)
{
    _blocklistApps = {"konsole","alacritty","keepassxc","1password","bitwarden","gnome-keyring","seahorse"};
    _stopTokens    = {".", "!", "?", "\n"};

    _debounce.setSingleShot(true);
    _debounce.setInterval(500);
    connect(&_debounce, &QTimer::timeout, this, &ConfigStore::saveNow);
}

// -------------------------------------------------------------------------
void ConfigStore::load() {
    if (!QFile::exists(_configPath)) {
        return;  // use struct defaults already set in member initialisation
    }

    auto result = toml::parse_file(_configPath.toStdString());
    if (!result) {
        QString msg = QString::fromStdString(std::string(result.error().description()));
        qWarning() << "ConfigStore: failed to parse" << _configPath << msg;
        emit parseError(msg);
        return;
    }
    const toml::table &root = result.table();

    // --- [inference] ---
    if (auto *inf = root["inference"].as_table()) {
        if (auto v = (*inf)["model"].value<std::string>())
            _model = QString::fromStdString(*v);
        if (auto v = (*inf)["ollama_host"].value<std::string>())
            _ollamaHost = QString::fromStdString(*v);
        if (auto v = (*inf)["debounce_ms"].value<int64_t>())
            _debounceMs = static_cast<int>(*v);
        if (auto v = (*inf)["min_context_chars"].value<int64_t>())
            _minContextChars = static_cast<int>(*v);
        if (auto v = (*inf)["context_window"].value<int64_t>())
            _contextWindow = static_cast<int>(*v);
        if (auto v = (*inf)["num_predict"].value<int64_t>())
            _numPredict = static_cast<int>(*v);
        if (auto v = (*inf)["temperature"].value<double>())
            _temperature = *v;
        if (auto v = (*inf)["top_p"].value<double>())
            _topP = *v;
        if (auto *arr = (*inf)["stop_tokens"].as_array())
            _stopTokens = tomlArrayToStringList(arr);
        if (auto v = (*inf)["threads"].value<int64_t>())
            _threads = QVariant(static_cast<int>(*v));
        // else _threads stays invalid (unset)
    }

    // --- [behaviour] ---
    if (auto *beh = root["behaviour"].as_table()) {
        if (auto v = (*beh)["engine_enabled"].value<bool>())        _engineEnabled = *v;
        if (auto v = (*beh)["trigger_mode"].value<std::string>())   _triggerMode = QString::fromStdString(*v);
        if (auto v = (*beh)["trigger_key"].value<std::string>())    _triggerKey  = QString::fromStdString(*v);
        if (auto v = (*beh)["accept_full_key"].value<std::string>()) _acceptKey  = QString::fromStdString(*v);
        if (auto v = (*beh)["partial_accept"].value<bool>())         _partialAccept = *v;
        if (auto *arr = (*beh)["blocklist_apps"].as_array())         _blocklistApps = tomlArrayToStringList(arr);
        if (auto *arr = (*beh)["blocked_phrases"].as_array())      _blockedPhrases = tomlArrayToStringList(arr);

        // Legacy: tab_accepts_word → partialAccept
        if (auto v = (*beh)["tab_accepts_word"].value<bool>()) {
            qWarning() << "ConfigStore: 'tab_accepts_word' is deprecated; use 'partial_accept' instead";
            _partialAccept = *v;
        }
    }

    // --- [learning] ---
    if (auto *lrn = root["learning"].as_table()) {
        if (auto v = (*lrn)["enabled"].value<bool>())                    _learningEnabled = *v;
        if (auto v = (*lrn)["corpus_path"].value<std::string>())         _corpusPath = QString::fromStdString(*v);
        if (auto v = (*lrn)["flush_interval_sec"].value<int64_t>())      _flushIntervalSec = static_cast<int>(*v);
        if (auto v = (*lrn)["max_corpus_mb"].value<int64_t>())           _maxCorpusMb = static_cast<int>(*v);
        if (auto v = (*lrn)["min_sentence_chars"].value<int64_t>())      _minSentenceChars = static_cast<int>(*v);
        if (auto v = (*lrn)["include_examples_in_prompt"].value<bool>()) _includeExamplesInPrompt = *v;
        if (auto v = (*lrn)["voice_strength"].value<int64_t>())           _voiceStrength = static_cast<int>(*v);
        if (auto v = (*lrn)["forget_after_days"].value<int64_t>())        _forgetAfterDays = static_cast<int>(*v);
    }

    // --- [user_prompt] ---
    if (auto *up = root["user_prompt"].as_table()) {
        if (auto v = (*up)["description"].value<std::string>()) _userDescription = QString::fromStdString(*v);
        if (auto v = (*up)["tone"].value<std::string>())        _userTone = QString::fromStdString(*v);
        if (auto *arr = (*up)["avoid_phrases"].as_array())      _userAvoidPhrases = tomlArrayToStringList(arr);
    }

    // --- [apps] ---
    _apps.clear();
    if (auto *appsTable = root["apps"].as_table()) {
        for (auto &&[prog, val] : *appsTable) {
            auto *overTbl = val.as_table();
            if (!overTbl) continue;
            QVariantMap entry;
            if (auto v = (*overTbl)["enabled"].value<bool>())          entry["enabled"] = *v;
            if (auto v = (*overTbl)["model"].value<std::string>())      entry["model"] = QString::fromStdString(*v);
            if (auto v = (*overTbl)["debounce_ms"].value<int64_t>())   entry["debounce_ms"] = static_cast<int>(*v);
            if (auto v = (*overTbl)["num_predict"].value<int64_t>())   entry["num_predict"] = static_cast<int>(*v);
            if (auto v = (*overTbl)["mode"].value<std::string>())       entry["mode"] = QString::fromStdString(*v);
            if (auto v = (*overTbl)["prompt_addendum"].value<std::string>()) entry["prompt_addendum"] = QString::fromStdString(*v);
            _apps[QString::fromStdString(std::string(prog))] = entry;
        }
    }
}

// -------------------------------------------------------------------------
void ConfigStore::scheduleSave() {
    _debounce.start();
}

void ConfigStore::saveNow() {
    _debounce.stop();
    try {
        writeToml(_configPath);
        emit saved();
    } catch (const std::exception &e) {
        emit saveFailed(QString::fromLocal8Bit(e.what()));
    }
}

void ConfigStore::writeToml(const QString &path) {
    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    toml::table root;

    // [inference]
    toml::table inf;
    inf.insert_or_assign("model",             _model.toStdString());
    inf.insert_or_assign("ollama_host",       _ollamaHost.toStdString());
    inf.insert_or_assign("debounce_ms",       static_cast<int64_t>(_debounceMs));
    inf.insert_or_assign("min_context_chars", static_cast<int64_t>(_minContextChars));
    inf.insert_or_assign("context_window",    static_cast<int64_t>(_contextWindow));
    inf.insert_or_assign("num_predict",       static_cast<int64_t>(_numPredict));
    inf.insert_or_assign("temperature",       _temperature);
    inf.insert_or_assign("top_p",             _topP);
    writeStringArray(inf, "stop_tokens", _stopTokens);
    if (_threads.isValid())
        inf.insert_or_assign("threads", static_cast<int64_t>(_threads.toInt()));
    root.insert_or_assign("inference", std::move(inf));

    // [behaviour]
    toml::table beh;
    beh.insert_or_assign("engine_enabled", _engineEnabled);
    beh.insert_or_assign("trigger_mode",   _triggerMode.toStdString());
    beh.insert_or_assign("trigger_key",    _triggerKey.toStdString());
    beh.insert_or_assign("accept_full_key", _acceptKey.toStdString());
    beh.insert_or_assign("partial_accept", _partialAccept);
    writeStringArray(beh, "blocklist_apps",  _blocklistApps);
    writeStringArray(beh, "blocked_phrases", _blockedPhrases);
    root.insert_or_assign("behaviour", std::move(beh));

    // [learning]
    toml::table lrn;
    lrn.insert_or_assign("enabled",                    _learningEnabled);
    lrn.insert_or_assign("corpus_path",                _corpusPath.toStdString());
    lrn.insert_or_assign("flush_interval_sec",         static_cast<int64_t>(_flushIntervalSec));
    lrn.insert_or_assign("max_corpus_mb",              static_cast<int64_t>(_maxCorpusMb));
    lrn.insert_or_assign("min_sentence_chars",         static_cast<int64_t>(_minSentenceChars));
    lrn.insert_or_assign("include_examples_in_prompt", _includeExamplesInPrompt);
    lrn.insert_or_assign("voice_strength",             static_cast<int64_t>(_voiceStrength));
    lrn.insert_or_assign("forget_after_days",          static_cast<int64_t>(_forgetAfterDays));
    root.insert_or_assign("learning", std::move(lrn));

    // [user_prompt]
    toml::table up;
    up.insert_or_assign("description",    _userDescription.toStdString());
    up.insert_or_assign("tone",           _userTone.toStdString());
    writeStringArray(up, "avoid_phrases", _userAvoidPhrases);
    root.insert_or_assign("user_prompt", std::move(up));

    // [apps]
    if (!_apps.isEmpty()) {
        toml::table appsTbl;
        for (auto it = _apps.cbegin(); it != _apps.cend(); ++it) {
            toml::table entry;
            const QVariantMap sub = it.value().toMap();
            if (sub.contains("enabled"))          entry.insert_or_assign("enabled",          sub["enabled"].toBool());
            if (sub.contains("model"))             entry.insert_or_assign("model",             sub["model"].toString().toStdString());
            if (sub.contains("debounce_ms"))       entry.insert_or_assign("debounce_ms",       static_cast<int64_t>(sub["debounce_ms"].toInt()));
            if (sub.contains("num_predict"))       entry.insert_or_assign("num_predict",       static_cast<int64_t>(sub["num_predict"].toInt()));
            if (sub.contains("mode"))              entry.insert_or_assign("mode",              sub["mode"].toString().toStdString());
            if (sub.contains("prompt_addendum"))   entry.insert_or_assign("prompt_addendum",   sub["prompt_addendum"].toString().toStdString());
            appsTbl.insert_or_assign(it.key().toStdString(), std::move(entry));
        }
        root.insert_or_assign("apps", std::move(appsTbl));
    }

    // Serialise to string — disable literal/multi-line string formats so that
    // control characters like '\n' are written as TOML escape sequences ("\n")
    // rather than as raw newlines inside '''...''' or """...""" (which strips
    // the first newline per TOML spec, breaking round-trip).
    using ff = toml::format_flags;
    const auto fmtFlags = toml::toml_formatter::default_flags
                        & ~ff::allow_literal_strings
                        & ~ff::allow_multi_line_strings;
    std::ostringstream oss;
    oss << "# Generated by xtype-settings — manual edits are preserved across sessions but inline comments are not.\n\n"
        << toml::toml_formatter(root, fmtFlags);
    const std::string content = oss.str();

    // Atomic write via QSaveFile
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        throw std::runtime_error(f.errorString().toStdString());
    f.write(content.data(), static_cast<qint64>(content.size()));
    if (!f.commit())
        throw std::runtime_error(f.errorString().toStdString());
}

// -------------------------------------------------------------------------
void ConfigStore::resetAll() {
    _debounce.stop();

    // 1. Back up current file
    QString backupPath;
    if (QFile::exists(_configPath)) {
        backupPath = _configPath + ".bak." +
                     QString::number(QDateTime::currentSecsSinceEpoch());
        QFile::copy(_configPath, backupPath);
    }

    // 2. Reset all fields to XTypeConfig defaults
    _engineEnabled = true;
    _triggerMode   = "pause";
    _triggerKey    = "ctrl+space";
    _acceptKey     = "tab";
    _partialAccept = true;
    _blocklistApps = {"konsole","alacritty","keepassxc","1password","bitwarden","gnome-keyring","seahorse"};
    _blockedPhrases       = {};

    _model            = "qwen2.5:1.5b";
    _ollamaHost       = "http://localhost:11434";
    _debounceMs       = 220;
    _minContextChars  = 10;
    _contextWindow    = 150;
    _numPredict       = 30;
    _temperature      = 0.3;
    _topP             = 0.9;
    _stopTokens       = {".", "!", "?", "\n"};
    _threads          = QVariant();

    _learningEnabled         = false;
    _corpusPath              = "~/.local/share/xtype/corpus.txt";
    _flushIntervalSec        = 60;
    _maxCorpusMb             = 50;
    _minSentenceChars        = 12;
    _includeExamplesInPrompt = true;
    _voiceStrength           = 50;
    _forgetAfterDays         = 0;

    _userDescription  = {};
    _userTone         = {};
    _userAvoidPhrases = {};
    _apps             = {};

    // 3. Write defaults synchronously
    try {
        writeToml(_configPath);
        emit saved();
    } catch (const std::exception &e) {
        emit saveFailed(QString::fromLocal8Bit(e.what()));
    }

    // 4. Notify QML bindings (emit all per-property signals)
    emit engineEnabledChanged();
    emit triggerModeChanged();
    emit triggerKeyChanged();
    emit acceptKeyChanged();
    emit partialAcceptChanged();
    emit blocklistAppsChanged();
    emit blockedPhrasesChanged();
    emit modelChanged();
    emit ollamaHostChanged();
    emit debounceMsChanged();
    emit minContextCharsChanged();
    emit contextWindowChanged();
    emit numPredictChanged();
    emit temperatureChanged();
    emit topPChanged();
    emit stopTokensChanged();
    emit threadsChanged();
    emit learningEnabledChanged();
    emit corpusPathChanged();
    emit flushIntervalSecChanged();
    emit maxCorpusMbChanged();
    emit minSentenceCharsChanged();
    emit includeExamplesInPromptChanged();
    emit voiceStrengthChanged();
    emit forgetAfterDaysChanged();
    emit userDescriptionChanged();
    emit userToneChanged();
    emit userAvoidPhrasesChanged();
    emit appsChanged();

    emit resetDone(backupPath);
}

// -------------------------------------------------------------------------
// Setters
// -------------------------------------------------------------------------
#define CS_SET(field, signal) \
    do { if (_##field == v) return; _##field = v; emit signal(); emit changed(); scheduleSave(); } while(0)

void ConfigStore::setEngineEnabled(bool v)              { CS_SET(engineEnabled,  engineEnabledChanged); }
void ConfigStore::setPartialAccept(bool v)              { CS_SET(partialAccept,  partialAcceptChanged); }
void ConfigStore::setTriggerMode(const QString &v)      { CS_SET(triggerMode,    triggerModeChanged); }
void ConfigStore::setTriggerKey(const QString &v)       { CS_SET(triggerKey,     triggerKeyChanged); }
void ConfigStore::setAcceptKey(const QString &v)        { CS_SET(acceptKey,      acceptKeyChanged); }
void ConfigStore::setBlocklistApps(const QStringList &v){ CS_SET(blocklistApps,         blocklistAppsChanged); }
void ConfigStore::setBlockedPhrases(const QStringList &v){ CS_SET(blockedPhrases,       blockedPhrasesChanged); }

void ConfigStore::setModel(const QString &v)            { CS_SET(model,                 modelChanged); }
void ConfigStore::setOllamaHost(const QString &v)       { CS_SET(ollamaHost,            ollamaHostChanged); }
void ConfigStore::setDebounceMs(int v)                  { CS_SET(debounceMs,            debounceMsChanged); }
void ConfigStore::setMinContextChars(int v)             { CS_SET(minContextChars,       minContextCharsChanged); }
void ConfigStore::setContextWindow(int v)               { CS_SET(contextWindow,         contextWindowChanged); }
void ConfigStore::setNumPredict(int v)                  { CS_SET(numPredict,            numPredictChanged); }
void ConfigStore::setTemperature(double v)              { CS_SET(temperature,           temperatureChanged); }
void ConfigStore::setTopP(double v)                     { CS_SET(topP,                  topPChanged); }
void ConfigStore::setStopTokens(const QStringList &v)  { CS_SET(stopTokens,            stopTokensChanged); }
void ConfigStore::setThreads(const QVariant &v)        { CS_SET(threads,               threadsChanged); }

void ConfigStore::setLearningEnabled(bool v)            { CS_SET(learningEnabled,       learningEnabledChanged); }
void ConfigStore::setCorpusPath(const QString &v)       { CS_SET(corpusPath,            corpusPathChanged); }
void ConfigStore::setFlushIntervalSec(int v)            { CS_SET(flushIntervalSec,      flushIntervalSecChanged); }
void ConfigStore::setMaxCorpusMb(int v)                 { CS_SET(maxCorpusMb,          maxCorpusMbChanged); }
void ConfigStore::setMinSentenceChars(int v)            { CS_SET(minSentenceChars,      minSentenceCharsChanged); }
void ConfigStore::setIncludeExamplesInPrompt(bool v)    { CS_SET(includeExamplesInPrompt, includeExamplesInPromptChanged); }
void ConfigStore::setVoiceStrength(int v)               { CS_SET(voiceStrength,           voiceStrengthChanged); }
void ConfigStore::setForgetAfterDays(int v)             { CS_SET(forgetAfterDays,         forgetAfterDaysChanged); }

void ConfigStore::setUserDescription(const QString &v)  { CS_SET(userDescription,       userDescriptionChanged); }
void ConfigStore::setUserTone(const QString &v)         { CS_SET(userTone,              userToneChanged); }
void ConfigStore::setUserAvoidPhrases(const QStringList &v){ CS_SET(userAvoidPhrases,   userAvoidPhrasesChanged); }

void ConfigStore::setApps(const QVariantMap &v)        { CS_SET(apps,                  appsChanged); }

void ConfigStore::setApp(const QString &id, const QString &key, const QVariant &value) {
    QVariantMap appMap = _apps.value(id).toMap();
    if (!value.isValid() || value.isNull())
        appMap.remove(key);
    else
        appMap.insert(key, value);
    _apps.insert(id, appMap);
    emit appsChanged();
    emit changed();
    scheduleSave();
}
