#include "apps_known.h"

#include <QDir>
#include <QSettings>
#include <QDebug>

namespace {

struct AppEntry {
    QString id;
    QString label;
    QString canonical;
    QString iconCls;
    QString defaultMode;
};

// §4 #11: display labels never reach TOML; canonical program names are the keys
static const AppEntry kApps[] = {
    {"kate",    "Kate",          "kate",                     "kate", "Default"},
    {"tb",      "Thunderbird",   "org.mozilla.thunderbird",  "tb",   "Default"},
    {"kons",    "Konsole",       "org.kde.konsole",          "kons", "Default"},
    {"firefox", "Firefox",       "firefox",                  "fox",  "Default"},
    {"discord", "Discord",       "discord",                  "disc", "Default"},
};

// Curated blocklist candidates: password managers + terminals
// canonical IDs used as Config.blocklistApps keys
static const AppEntry kBlockers[] = {
    {"keepassxc", "KeePassXC",  "keepassxc",  "kde",  "Off"},
    {"1password", "1Password",  "1password",  "kde",  "Off"},
    {"bitwarden", "Bitwarden",  "bitwarden",  "kde",  "Off"},
    {"discord",   "Discord",    "discord",    "disc", "Off"},
    {"kons",      "Konsole",    "org.kde.konsole", "kons", "Off"},
};

} // namespace

AppsKnown::AppsKnown(QObject *parent) : QObject(parent) {}

QVariantList AppsKnown::knownApps() const {
    QVariantList out;
    for (const AppEntry &e : kApps) {
        out.append(QVariantMap{
            {"id",        e.id},
            {"label",     e.label},
            {"canonical", e.canonical},
            {"iconCls",   e.iconCls},
            {"mode",      e.defaultMode},
        });
    }
    return out;
}

QVariantList AppsKnown::curatedBlockerCandidates() const {
    QVariantList out;
    for (const AppEntry &e : kBlockers) {
        out.append(QVariantMap{
            {"id",        e.canonical},   // canonical used as blocklist key
            {"label",     e.label},
            {"iconCls",   e.iconCls},
        });
    }
    return out;
}

QString AppsKnown::canonicalForId(const QString &id) const {
    for (const AppEntry &e : kApps)
        if (e.id == id) return e.canonical;
    for (const AppEntry &e : kBlockers)
        if (e.id == id) return e.canonical;
    return id;  // fallback: treat id as canonical
}

QString AppsKnown::labelForCanonical(const QString &canonical) const {
    for (const AppEntry &e : kApps)
        if (e.canonical == canonical) return e.label;
    for (const AppEntry &e : kBlockers)
        if (e.canonical == canonical) return e.label;
    return canonical;
}
