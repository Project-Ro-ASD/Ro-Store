#include "SoftwareHistoryModel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimer>

SoftwareHistoryModel::SoftwareHistoryModel(QObject *parent) : QObject(parent) {}

SoftwareHistoryModel::~SoftwareHistoryModel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void SoftwareHistoryModel::refresh()
{
    if (m_loading) return;
    m_loading = true;
    m_step = 0;
    m_error.clear();
    m_rpmHistory.clear();
    m_flatpakHistory.clear();
    emit changed();
    runNext();
}

void SoftwareHistoryModel::runNext()
{
    if (m_step >= 3) {
        m_loading = false;
        emit changed();
        return;
    }
    const int step = m_step++;
    const QString program = step == 0 ? QStringLiteral("/usr/bin/dnf5")
                                      : QStringLiteral("/usr/bin/flatpak");
    if (!QFileInfo::exists(program)) {
        if (step == 0) m_error = QStringLiteral("DNF5 işlem geçmişi bulunamadı.");
        runNext();
        return;
    }
    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(program);
    process->setArguments(step == 0
        ? QStringList{QStringLiteral("history"), QStringLiteral("list"), QStringLiteral("--json")}
        : QStringList{step == 1 ? QStringLiteral("--user") : QStringLiteral("--system"),
                      QStringLiteral("history"), QStringLiteral("--reverse"), QStringLiteral("--json")});
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (m_process != process || error != QProcess::FailedToStart) return;
        m_process = nullptr;
        m_error = QStringLiteral("Geçmiş sorgularından biri başlatılamadı.");
        process->deleteLater();
        runNext();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, step](int code, QProcess::ExitStatus status) {
        if (m_process != process) return;
        m_process = nullptr;
        const QByteArray bytes = process->readAllStandardOutput();
        process->deleteLater();
        if (status != QProcess::NormalExit || code != 0 || bytes.size() > 8 * 1024 * 1024) {
            m_error = QStringLiteral("Geçmiş kaynaklarından biri okunamadı; liste eksik olabilir.");
            runNext();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
            m_error = QStringLiteral("Geçmiş verisinin biçimi beklenenden farklı.");
            runNext();
            return;
        }
        const QJsonArray array = document.array();
        const qsizetype limit = qMin(qsizetype(100), array.size());
        for (qsizetype i = 0; i < limit; ++i) {
            const QJsonObject item = array.at(i).toObject();
            if (step == 0) {
                const qint64 seconds = item.value(QStringLiteral("start_time")).toInteger();
                m_rpmHistory.append(QVariantMap{
                    {QStringLiteral("id"), item.value(QStringLiteral("id")).toInteger()},
                    {QStringLiteral("time"), QLocale().toString(QDateTime::fromSecsSinceEpoch(seconds), QLocale::ShortFormat)},
                    {QStringLiteral("status"), item.value(QStringLiteral("status")).toString()},
                    {QStringLiteral("command"), item.value(QStringLiteral("command_line")).toString()},
                    {QStringLiteral("alteredCount"), item.value(QStringLiteral("altered_count")).toInteger()}
                });
            } else {
                m_flatpakHistory.append(QVariantMap{
                    {QStringLiteral("time"), item.value(QStringLiteral("time")).toString()},
                    {QStringLiteral("change"), item.value(QStringLiteral("change")).toString()},
                    {QStringLiteral("application"), item.value(QStringLiteral("application")).toString()},
                    {QStringLiteral("scope"), step == 1 ? QStringLiteral("Kullanıcı") : QStringLiteral("Sistem")},
                    {QStringLiteral("remote"), item.value(QStringLiteral("remote")).toString()}
                });
            }
        }
        emit changed();
        runNext();
    });
    QTimer::singleShot(30000, process, [process]() {
        if (process->state() != QProcess::NotRunning) process->kill();
    });
    process->start();
}
