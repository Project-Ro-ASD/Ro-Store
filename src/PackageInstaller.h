#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class PackageInstaller : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString phaseText READ phaseText NOTIFY phaseTextChanged)

public:
    explicit PackageInstaller(QObject *parent = nullptr);

    bool running() const;
    QString statusText() const;
    QString output() const;
    int progress() const;
    QString phaseText() const;

    Q_INVOKABLE void installPackage(const QString &packageName);
    Q_INVOKABLE void removePackage(const QString &packageName);
    Q_INVOKABLE void updatePackage(const QString &packageName);

signals:
    void runningChanged();
    void statusTextChanged();
    void outputChanged();
    void progressChanged();
    void phaseTextChanged();
    void finished(bool success, QString message);

private:
    void runPkconTransaction(
        const QStringList &arguments,
        const QString &startMessage,
        const QString &successMessage,
        const QString &failureMessage
    );

    void setRunning(bool value);
    void setStatusText(const QString &text);
    void setProgress(int value);
    void setPhaseText(const QString &text);
    void appendOutput(const QString &text);
    void parseProgressFromOutput(const QString &text);
    QString normalizePhase(const QString &rawText) const;

private:
    bool m_running = false;
    QString m_statusText = "İşlem başlatılmadı";
    QString m_output;
    int m_progress = 0;
    QString m_phaseText = "Hazır";
};
