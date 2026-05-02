#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

// TOML key names mirror XTypeConfig struct fields exactly (snake_case).
// Enum strings: TriggerMode → "pause"|"manual"; AcceptKey → "tab"|"enter"|"right".
// std::optional round-trip: absent key ↔ invalid QVariant / empty optional.

class ConfigStore : public QObject {
    Q_OBJECT

    // Behaviour
    Q_PROPERTY(bool        engineEnabled    READ engineEnabled    WRITE setEngineEnabled    NOTIFY engineEnabledChanged)
    Q_PROPERTY(QString     triggerMode      READ triggerMode      WRITE setTriggerMode      NOTIFY triggerModeChanged)
    Q_PROPERTY(QString     triggerKey       READ triggerKey       WRITE setTriggerKey       NOTIFY triggerKeyChanged)
    Q_PROPERTY(QString     acceptKey        READ acceptKey        WRITE setAcceptKey        NOTIFY acceptKeyChanged)
    Q_PROPERTY(bool        partialAccept    READ partialAccept    WRITE setPartialAccept    NOTIFY partialAcceptChanged)
    Q_PROPERTY(QStringList blocklistApps   READ blocklistApps    WRITE setBlocklistApps    NOTIFY blocklistAppsChanged)
    Q_PROPERTY(QStringList blockedPhrases  READ blockedPhrases   WRITE setBlockedPhrases   NOTIFY blockedPhrasesChanged)

    // Inference
    Q_PROPERTY(QString  model           READ model           WRITE setModel           NOTIFY modelChanged)
    Q_PROPERTY(QString  ollamaHost      READ ollamaHost      WRITE setOllamaHost      NOTIFY ollamaHostChanged)
    Q_PROPERTY(int      debounceMs      READ debounceMs      WRITE setDebounceMs      NOTIFY debounceMsChanged)
    Q_PROPERTY(int      minContextChars READ minContextChars WRITE setMinContextChars NOTIFY minContextCharsChanged)
    Q_PROPERTY(int      contextWindow   READ contextWindow   WRITE setContextWindow   NOTIFY contextWindowChanged)
    Q_PROPERTY(int      numPredict      READ numPredict      WRITE setNumPredict      NOTIFY numPredictChanged)
    Q_PROPERTY(double   temperature     READ temperature     WRITE setTemperature     NOTIFY temperatureChanged)
    Q_PROPERTY(double   topP            READ topP            WRITE setTopP            NOTIFY topPChanged)
    Q_PROPERTY(QStringList stopTokens  READ stopTokens      WRITE setStopTokens      NOTIFY stopTokensChanged)
    Q_PROPERTY(QVariant threads        READ threads         WRITE setThreads         NOTIFY threadsChanged)  // invalid = unset

    // Learning
    Q_PROPERTY(bool    learningEnabled          READ learningEnabled          WRITE setLearningEnabled          NOTIFY learningEnabledChanged)
    Q_PROPERTY(QString corpusPath               READ corpusPath               WRITE setCorpusPath               NOTIFY corpusPathChanged)
    Q_PROPERTY(int     flushIntervalSec         READ flushIntervalSec         WRITE setFlushIntervalSec         NOTIFY flushIntervalSecChanged)
    Q_PROPERTY(int     maxCorpusMb              READ maxCorpusMb              WRITE setMaxCorpusMb              NOTIFY maxCorpusMbChanged)
    Q_PROPERTY(int     minSentenceChars         READ minSentenceChars         WRITE setMinSentenceChars         NOTIFY minSentenceCharsChanged)
    Q_PROPERTY(bool    includeExamplesInPrompt  READ includeExamplesInPrompt  WRITE setIncludeExamplesInPrompt  NOTIFY includeExamplesInPromptChanged)
    Q_PROPERTY(int     voiceStrength            READ voiceStrength            WRITE setVoiceStrength            NOTIFY voiceStrengthChanged)
    Q_PROPERTY(int     forgetAfterDays          READ forgetAfterDays          WRITE setForgetAfterDays          NOTIFY forgetAfterDaysChanged)

    // UserPrompt
    Q_PROPERTY(QString     userDescription  READ userDescription  WRITE setUserDescription  NOTIFY userDescriptionChanged)
    Q_PROPERTY(QString     userTone         READ userTone         WRITE setUserTone         NOTIFY userToneChanged)
    Q_PROPERTY(QStringList userAvoidPhrases READ userAvoidPhrases WRITE setUserAvoidPhrases NOTIFY userAvoidPhrasesChanged)

    // Apps map: { "program": { "enabled": bool, "model": str, "debounce_ms": int,
    //                          "num_predict": int, "mode": str, "prompt_addendum": str } }
    // Missing keys in sub-map → absent optional in AppOverride.
    Q_PROPERTY(QVariantMap apps READ apps WRITE setApps NOTIFY appsChanged)

public:
    // configPath: defaults to ~/.config/xtype/config.toml; override for tests.
    explicit ConfigStore(const QString &configPath = QString(), QObject *parent = nullptr);

    // Loads config from disk. Call once from main.cpp before QML engine starts.
    void load();

    // --- Getters ---
    bool        engineEnabled()          const { return _engineEnabled; }
    QString     triggerMode()            const { return _triggerMode; }
    QString     triggerKey()             const { return _triggerKey; }
    QString     acceptKey()              const { return _acceptKey; }
    bool        partialAccept()          const { return _partialAccept; }
    QStringList blocklistApps()          const { return _blocklistApps; }
    QStringList blockedPhrases()         const { return _blockedPhrases; }

    QString     model()                  const { return _model; }
    QString     ollamaHost()             const { return _ollamaHost; }
    int         debounceMs()             const { return _debounceMs; }
    int         minContextChars()        const { return _minContextChars; }
    int         contextWindow()          const { return _contextWindow; }
    int         numPredict()             const { return _numPredict; }
    double      temperature()            const { return _temperature; }
    double      topP()                   const { return _topP; }
    QStringList stopTokens()             const { return _stopTokens; }
    QVariant    threads()                const { return _threads; }

    bool    learningEnabled()            const { return _learningEnabled; }
    QString corpusPath()                 const { return _corpusPath; }
    int     flushIntervalSec()           const { return _flushIntervalSec; }
    int     maxCorpusMb()               const { return _maxCorpusMb; }
    int     minSentenceChars()           const { return _minSentenceChars; }
    bool    includeExamplesInPrompt()    const { return _includeExamplesInPrompt; }
    int     voiceStrength()              const { return _voiceStrength; }
    int     forgetAfterDays()            const { return _forgetAfterDays; }

    QString     userDescription()        const { return _userDescription; }
    QString     userTone()               const { return _userTone; }
    QStringList userAvoidPhrases()       const { return _userAvoidPhrases; }

    QVariantMap apps()                   const { return _apps; }

    // --- Setters (each emits its own xChanged + schedules debounce save) ---
    void setEngineEnabled(bool v);
    void setTriggerMode(const QString &v);
    void setTriggerKey(const QString &v);
    void setAcceptKey(const QString &v);
    void setPartialAccept(bool v);
    void setBlocklistApps(const QStringList &v);
    void setBlockedPhrases(const QStringList &v);

    void setModel(const QString &v);
    void setOllamaHost(const QString &v);
    void setDebounceMs(int v);
    void setMinContextChars(int v);
    void setContextWindow(int v);
    void setNumPredict(int v);
    void setTemperature(double v);
    void setTopP(double v);
    void setStopTokens(const QStringList &v);
    void setThreads(const QVariant &v);

    void setLearningEnabled(bool v);
    void setCorpusPath(const QString &v);
    void setFlushIntervalSec(int v);
    void setMaxCorpusMb(int v);
    void setMinSentenceChars(int v);
    void setIncludeExamplesInPrompt(bool v);
    void setVoiceStrength(int v);
    void setForgetAfterDays(int v);

    Q_INVOKABLE void setApp(const QString &id, const QString &key, const QVariant &value);

    void setUserDescription(const QString &v);
    void setUserTone(const QString &v);
    void setUserAvoidPhrases(const QStringList &v);

    void setApps(const QVariantMap &v);

public slots:
    void resetAll();   // backup current file, write defaults, mark reload
    void saveNow();    // bypass debounce — used in tests and resetAll

signals:
    void changed();            // any field changed → debounce timer restarts
    void saved();              // atomic TOML write succeeded
    void saveFailed(QString reason);
    void resetDone(QString backupPath);  // emitted after resetAll completes
    void parseError(const QString& message);

    // Per-property signals
    void engineEnabledChanged();
    void triggerModeChanged();
    void triggerKeyChanged();
    void acceptKeyChanged();
    void partialAcceptChanged();
    void blocklistAppsChanged();
    void blockedPhrasesChanged();

    void modelChanged();
    void ollamaHostChanged();
    void debounceMsChanged();
    void minContextCharsChanged();
    void contextWindowChanged();
    void numPredictChanged();
    void temperatureChanged();
    void topPChanged();
    void stopTokensChanged();
    void threadsChanged();

    void learningEnabledChanged();
    void corpusPathChanged();
    void flushIntervalSecChanged();
    void maxCorpusMbChanged();
    void minSentenceCharsChanged();
    void includeExamplesInPromptChanged();
    void voiceStrengthChanged();
    void forgetAfterDaysChanged();

    void userDescriptionChanged();
    void userToneChanged();
    void userAvoidPhrasesChanged();

    void appsChanged();

private:
    void scheduleSave();
    void writeToml(const QString &path);   // throws on I/O error (caught in saveNow/resetAll)

    QString _configPath;
    QTimer  _debounce;

    // Behaviour
    bool        _engineEnabled   = true;
    QString     _triggerMode     = "pause";
    QString     _triggerKey      = "ctrl+space";
    QString     _acceptKey       = "tab";
    bool        _partialAccept   = true;
    QStringList _blocklistApps;
    QStringList _blockedPhrases;

    // Inference
    QString     _model            = "qwen2.5:1.5b";
    QString     _ollamaHost       = "http://localhost:11434";
    int         _debounceMs       = 220;
    int         _minContextChars  = 10;
    int         _contextWindow    = 150;
    int         _numPredict       = 30;
    double      _temperature      = 0.3;
    double      _topP             = 0.9;
    QStringList _stopTokens       = {".", "!", "?", "\n"};
    QVariant    _threads;   // invalid = unset

    // Learning
    bool    _learningEnabled         = false;
    QString _corpusPath              = "~/.local/share/xtype/corpus.txt";
    int     _flushIntervalSec        = 60;
    int     _maxCorpusMb             = 50;
    int     _minSentenceChars        = 12;
    bool    _includeExamplesInPrompt = true;
    int     _voiceStrength           = 50;
    int     _forgetAfterDays         = 0;

    // UserPrompt
    QString     _userDescription;
    QString     _userTone;
    QStringList _userAvoidPhrases;

    // Apps
    QVariantMap _apps;
};
