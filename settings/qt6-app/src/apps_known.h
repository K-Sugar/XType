#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class AppsKnown : public QObject {
    Q_OBJECT

public:
    explicit AppsKnown(QObject *parent = nullptr);

    // Returns list of { id, label, canonical, iconCls, defaultMode }
    Q_INVOKABLE QVariantList knownApps() const;

    // Returns list of curated blocklist candidates (password managers + terminals)
    Q_INVOKABLE QVariantList curatedBlockerCandidates() const;

    // Maps short id → canonical program name for TOML keys
    Q_INVOKABLE QString canonicalForId(const QString &id) const;

    // Maps canonical program name → display label
    Q_INVOKABLE QString labelForCanonical(const QString &canonical) const;
};
