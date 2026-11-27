#pragma once

#include <QObject>

class FakeCoordinator : public QObject
{
    Q_OBJECT
public:
    bool occupied = false;
    Q_INVOKABLE bool acquireExternalOperation(QObject *)
    {
        if (occupied) return false;
        occupied = true;
        return true;
    }
    Q_INVOKABLE void releaseExternalOperation(QObject *) { occupied = false; }
};
