#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class OllamaClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)

public:
    explicit OllamaClient(QObject *parent = nullptr);

    QStringList availableModels() const { return _models; }

    Q_INVOKABLE QVariantMap activeMeta(const QString &name) const;
    Q_INVOKABLE void refresh();

signals:
    void availableModelsChanged();

private slots:
    void onFinished(int code, QProcess::ExitStatus status);
    void onTimeout();

private:
    QStringList _models;
    QProcess    _proc;
    QTimer      _timeout;
    bool        _running = false;
};
