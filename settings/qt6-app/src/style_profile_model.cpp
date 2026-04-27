#include "style_profile_model.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

StyleProfileModel::StyleProfileModel(QObject *parent) : QObject(parent) {
    refresh();
}

void StyleProfileModel::refresh() {
    const QString path = QDir::homePath() + "/.local/share/xtype/style_profile.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (!_exemplars.isEmpty() || !_openers.isEmpty()) {
            _exemplars.clear();
            _openers.clear();
            emit profileChanged();
        }
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    QStringList ex, op;

    for (const QJsonValue &v : obj.value("exemplars").toArray())
        ex.append(v.toString());
    for (const QJsonValue &v : obj.value("openers").toArray())
        op.append(v.toString());

    if (ex != _exemplars || op != _openers) {
        _exemplars = ex;
        _openers   = op;
        emit profileChanged();
    }
}
