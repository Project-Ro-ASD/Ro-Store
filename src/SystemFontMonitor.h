#pragma once

#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QObject>

// A single font-change notification source shared by all QML views.
//
// Qt 6 recommends handling QEvent::ApplicationFontChange instead of the
// deprecated QGuiApplication::fontChanged signal.  This also updates views
// that are temporarily inactive inside StackView.
class SystemFontMonitor final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QFont currentFont READ currentFont NOTIFY currentFontChanged)

public:
    explicit SystemFontMonitor(QObject *parent = nullptr)
        : QObject(parent)
    {
        QGuiApplication::instance()->installEventFilter(this);
    }

    QFont currentFont() const
    {
        return QGuiApplication::font();
    }

signals:
    void currentFontChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == QGuiApplication::instance()
            && event->type() == QEvent::ApplicationFontChange) {
            emit currentFontChanged();
        }

        return QObject::eventFilter(watched, event);
    }
};
