#pragma once

#include <QObject>
#include <QStringList>

class StyleProfileModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList exemplars READ exemplars NOTIFY profileChanged)
    Q_PROPERTY(QStringList openers   READ openers   NOTIFY profileChanged)

public:
    explicit StyleProfileModel(QObject *parent = nullptr);

    QStringList exemplars() const { return _exemplars; }
    QStringList openers()   const { return _openers; }

    Q_INVOKABLE void refresh();

signals:
    void profileChanged();

private:
    QStringList _exemplars;
    QStringList _openers;
};
