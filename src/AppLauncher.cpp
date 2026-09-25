#include "AppLauncher.h"

#include <QProcess>
#include <QStringList>

AppLauncher::AppLauncher(QObject *parent)
    : QObject(parent)
{
}

QString AppLauncher::statusText() const
{
    return m_statusText;
}

bool AppLauncher::launch(const QString &command)
{
    const QString cleanCommand = command.trimmed();

    if (cleanCommand.isEmpty()) {
        setStatusText("Çalıştırma komutu bulunamadı.");
        return false;
    }

    QStringList parts = QProcess::splitCommand(cleanCommand);

    if (parts.isEmpty()) {
        setStatusText("Çalıştırma komutu çözümlenemedi.");
        return false;
    }

    const QString program = parts.takeFirst();
    const bool started = QProcess::startDetached(program, parts);

    if (started) {
        setStatusText(cleanCommand + " başlatıldı.");
    } else {
        setStatusText(cleanCommand + " başlatılamadı. (Teknik: komut bulunamadı veya çalıştırılamadı)");
    }

    return started;
}

void AppLauncher::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }

    m_statusText = text;
    emit statusTextChanged();
}
