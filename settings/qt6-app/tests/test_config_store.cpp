#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QFile>
#include <QList>
#include <QString>
#include <QTemporaryDir>
#include <QVariantMap>

#include "config_store.h"

// Capture qWarning emissions during a scope
struct WarnCapture {
    QList<QString> msgs;
    WarnCapture()  { s_instance = this; qInstallMessageHandler(handler); }
    ~WarnCapture() { qInstallMessageHandler(nullptr); s_instance = nullptr; }
    static void handler(QtMsgType t, const QMessageLogContext &, const QString &m) {
        if (t == QtWarningMsg && s_instance) s_instance->msgs.append(m);
    }
    static WarnCapture *s_instance;
};
WarnCapture *WarnCapture::s_instance = nullptr;

static QString fixturesDir() {
    return QString(XTYPE_FIXTURES_DIR);
}

// ------------------------------------------------------------------ 1
TEST_CASE("load config_default.toml yields XTypeConfig defaults", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");
    REQUIRE(QFile::copy(fixturesDir() + "/config_default.toml", dst));

    ConfigStore cs(dst);
    cs.load();

    CHECK(cs.model()                   == "qwen2.5:1.5b");
    CHECK(cs.ollamaHost()              == "http://localhost:11434");
    CHECK(cs.debounceMs()              == 220);
    CHECK(cs.minContextChars()         == 10);
    CHECK(cs.contextWindow()           == 150);
    CHECK(cs.numPredict()              == 30);
    CHECK(cs.temperature()             == Catch::Approx(0.3).margin(1e-6));
    CHECK(cs.topP()                    == Catch::Approx(0.9).margin(1e-6));
    CHECK(cs.stopTokens()              == QStringList({".", "!", "?", "\n"}));
    CHECK(!cs.threads().isValid());

    CHECK(cs.engineEnabled()           == true);
    CHECK(cs.triggerMode()             == "pause");
    CHECK(cs.acceptKey()               == "tab");
    CHECK(cs.partialAccept()           == true);
    CHECK(cs.escDismisses()            == true);
    CHECK(cs.passThroughTerminals()    == true);
    CHECK(cs.blocklistApps()           == QStringList({"konsole","alacritty","keepassxc","1password","bitwarden","gnome-keyring","seahorse"}));
    CHECK(cs.blockedPhrases()          == QStringList{});

    CHECK(cs.learningEnabled()         == false);
    CHECK(cs.corpusPath()              == "~/.local/share/xtype/corpus.txt");
    CHECK(cs.flushIntervalSec()        == 60);
    CHECK(cs.maxCorpusMb()            == 50);
    CHECK(cs.minSentenceChars()        == 12);
    CHECK(cs.includeExamplesInPrompt() == true);

    CHECK(cs.userDescription()         == "");
    CHECK(cs.userTone()                == "");
    CHECK(cs.userAvoidPhrases()        == QStringList{});
    CHECK(cs.apps().isEmpty());
}

// ------------------------------------------------------------------ 2
TEST_CASE("mutate properties, saveNow, reload — values survive", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    {
        ConfigStore cs(dst);
        cs.load();   // no file → all defaults

        cs.setModel("llama3:8b");
        cs.setDebounceMs(350);
        cs.setTemperature(0.7);
        cs.setEngineEnabled(false);
        cs.setTriggerMode("manual");
        cs.setAcceptKey("right");
        cs.setPartialAccept(false);
        cs.setBlockedPhrases({"kind regards", "per my email"});
        cs.setLearningEnabled(true);
        cs.setUserDescription("terse assistant");
        cs.setUserTone("casual");
        cs.setUserAvoidPhrases({"sorry", "unfortunately"});
        cs.setThreads(QVariant(4));

        QVariantMap appMap;
        QVariantMap kateMap; kateMap["enabled"] = true; kateMap["mode"] = QString("Code");
        appMap["kate"] = kateMap;
        cs.setApps(appMap);

        cs.saveNow();
    }

    ConfigStore cs2(dst);
    cs2.load();

    CHECK(cs2.model()            == "llama3:8b");
    CHECK(cs2.debounceMs()       == 350);
    CHECK(cs2.temperature()      == Catch::Approx(0.7).margin(1e-6));
    CHECK(cs2.engineEnabled()    == false);
    CHECK(cs2.triggerMode()      == "manual");
    CHECK(cs2.acceptKey()        == "right");
    CHECK(cs2.partialAccept()    == false);
    CHECK(cs2.blockedPhrases()   == QStringList({"kind regards", "per my email"}));
    CHECK(cs2.learningEnabled()  == true);
    CHECK(cs2.userDescription()  == "terse assistant");
    CHECK(cs2.userTone()         == "casual");
    CHECK(cs2.userAvoidPhrases() == QStringList({"sorry", "unfortunately"}));
    CHECK(cs2.threads().isValid());
    CHECK(cs2.threads().toInt()  == 4);

    const QVariantMap loaded = cs2.apps();
    REQUIRE(loaded.contains("kate"));
    const QVariantMap kate = loaded["kate"].toMap();
    CHECK(kate["enabled"].toBool() == true);
    CHECK(kate["mode"].toString()  == "Code");
}

// ------------------------------------------------------------------ 3
TEST_CASE("legacy tab_accepts_word=false → partialAccept=false + one qWarning", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");
    REQUIRE(QFile::copy(fixturesDir() + "/config_legacy_tab_accepts.toml", dst));

    WarnCapture cap;
    ConfigStore cs(dst);
    cs.load();

    CHECK(cs.partialAccept() == false);
    REQUIRE(cap.msgs.size() == 1);
    CHECK(cap.msgs[0].contains("tab_accepts_word"));
}

// ------------------------------------------------------------------ 4
TEST_CASE("apps: only present keys survive, absent optionals stay absent", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    {
        ConfigStore cs(dst);
        cs.load();

        QVariantMap appMap;
        QVariantMap kateMap; kateMap["enabled"] = true;
        appMap["kate"] = kateMap;
        cs.setApps(appMap);
        cs.saveNow();
    }

    ConfigStore cs2(dst);
    cs2.load();

    const QVariantMap loaded = cs2.apps();
    REQUIRE(loaded.contains("kate"));
    const QVariantMap kate = loaded["kate"].toMap();

    CHECK(kate["enabled"].toBool() == true);
    CHECK(!kate.contains("model"));
    CHECK(!kate.contains("debounce_ms"));
    CHECK(!kate.contains("num_predict"));
    CHECK(!kate.contains("mode"));
    CHECK(!kate.contains("prompt_addendum"));
}

// ------------------------------------------------------------------ 5
TEST_CASE("temperature and top_p round-trip", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    {
        ConfigStore cs(dst);
        cs.load();
        cs.setTemperature(0.7);
        cs.setTopP(0.8);
        cs.saveNow();
    }

    ConfigStore cs2(dst);
    cs2.load();

    CHECK(cs2.temperature() == Catch::Approx(0.7).margin(1e-6));
    CHECK(cs2.topP()        == Catch::Approx(0.8).margin(1e-6));
}

// ------------------------------------------------------------------ 6
TEST_CASE("stop_tokens round-trip", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    {
        ConfigStore cs(dst);
        cs.load();
        cs.setStopTokens({".", "\n"});
        cs.saveNow();
    }

    ConfigStore cs2(dst);
    cs2.load();

    CHECK(cs2.stopTokens() == QStringList({".", "\n"}));
}

// ------------------------------------------------------------------ 7
TEST_CASE("max_corpus_mb and min_sentence_chars round-trip", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    {
        ConfigStore cs(dst);
        cs.load();
        cs.setMaxCorpusMb(25);
        cs.setMinSentenceChars(8);
        cs.saveNow();
    }

    ConfigStore cs2(dst);
    cs2.load();

    CHECK(cs2.maxCorpusMb()      == 25);
    CHECK(cs2.minSentenceChars() == 8);
}

// ------------------------------------------------------------------ 8
TEST_CASE("apps: per-app model round-trips via setApp and survives reload", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    // Write model for kate via setApp().
    {
        ConfigStore cs(dst);
        cs.load();
        cs.setApp("kate", "model", QString("gemma3:4b"));
        cs.saveNow();
    }

    // Reload: model must be present.
    {
        ConfigStore cs2(dst);
        cs2.load();
        const QVariantMap kate = cs2.apps()["kate"].toMap();
        CHECK(kate["model"].toString() == "gemma3:4b");
    }

    // Clear model via null: key must be absent after reload.
    {
        ConfigStore cs3(dst);
        cs3.load();
        cs3.setApp("kate", "model", QVariant());  // null → remove
        cs3.saveNow();
    }

    ConfigStore cs4(dst);
    cs4.load();
    const QVariantMap kate = cs4.apps()["kate"].toMap();
    CHECK(!kate.contains("model"));
}

// ------------------------------------------------------------------ 10
TEST_CASE("parse error emits parseError signal for malformed TOML", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString malformedPath = tmp.filePath("config.toml");

    {
        QFile f(malformedPath);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("model = [unclosed\n");
    }

    ConfigStore cs(malformedPath);

    int emitCount = 0;
    QString capturedMsg;
    QObject::connect(&cs, &ConfigStore::parseError, [&](const QString &msg) {
        ++emitCount;
        capturedMsg = msg;
    });

    cs.load();

    REQUIRE(emitCount == 1);
    CHECK(!capturedMsg.isEmpty());
    CHECK(!capturedMsg.contains(malformedPath));
}

// ------------------------------------------------------------------ 9
TEST_CASE("resetAll: backup file exists, main file reverts to defaults", "[config_store]") {
    QTemporaryDir tmp; REQUIRE(tmp.isValid());
    const QString dst = tmp.filePath("config.toml");

    {
        ConfigStore cs(dst);
        cs.load();
        cs.setModel("custom-model");
        cs.setDebounceMs(999);
        cs.saveNow();
    }
    REQUIRE(QFile::exists(dst));

    QString backupPath;
    {
        ConfigStore cs(dst);
        cs.load();
        QObject::connect(&cs, &ConfigStore::resetDone, [&](const QString &p){ backupPath = p; });
        cs.resetAll();
    }

    REQUIRE(!backupPath.isEmpty());
    CHECK(QFile::exists(backupPath));

    ConfigStore cs2(dst);
    cs2.load();
    CHECK(cs2.model()      == "qwen2.5:1.5b");
    CHECK(cs2.debounceMs() == 220);
}
