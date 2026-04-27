#pragma once

#include <QObject>
#include <QVariantMap>

// Scaffold: the ring is empty since §U1.2 has no event producers yet.
// Returns {} from first() until the engine exports events via a file/socket.
class RecentEventsModel : public QObject {
    Q_OBJECT

public:
    explicit RecentEventsModel(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap first() const { return {}; }
};
