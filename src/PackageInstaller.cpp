#include "PackageInstaller.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

PackageInstaller::PackageInstaller(QObject *parent)
    : QObject(parent)
{
}

bool PackageInstaller::running() const
{
    return m_running;
}

QString PackageInstaller::statusText() const
{
    return m_statusText;
}

QString PackageInstaller::output() const
{
    return m_output;
}

int PackageInstaller::progress() const
{
    return m_progress;
}

QString PackageInstaller::phaseText() const
{
    return m_phaseText;
}

void PackageInstaller::installPackage(const QString &packageName)
{
    const QString cleanPackageName = packageName.trimmed();

    if (cleanPackageName.isEmpty()) {
        setStatusText("Paket adı bulunamadı. (Teknik: boş paket adı)");
        emit finished(false, "Paket adı bulunamadı. (Teknik: boş paket adı)");
        return;
    }

    runPkconTransaction(
        { "pkcon", "-y", "install", cleanPackageName },
        cleanPackageName + " yükleniyor...",
        cleanPackageName + " başarıyla yüklendi. (İşlem: yükleme, paket: " + cleanPackageName + ")",
        cleanPackageName + " yüklenemedi. (İşlem: yükleme, paket: " + cleanPackageName + ")"
    );
}

void PackageInstaller::removePackage(const QString &packageName)
{
    const QString cleanPackageName = packageName.trimmed();

    if (cleanPackageName.isEmpty()) {
        setStatusText("Paket adı bulunamadı. (Teknik: boş paket adı)");
        emit finished(false, "Paket adı bulunamadı. (Teknik: boş paket adı)");
        return;
    }

    runPkconTransaction(
        { "pkcon", "-y", "remove", cleanPackageName },
        cleanPackageName + " siliniyor...",
        cleanPackageName + " başarıyla silindi. (İşlem: kaldırma, paket: " + cleanPackageName + ")",
        cleanPackageName + " silinemedi. (İşlem: kaldırma, paket: " + cleanPackageName + ")"
    );
}

void PackageInstaller::updatePackage(const QString &packageName)
{
    const QString cleanPackageName = packageName.trimmed();

    if (cleanPackageName.isEmpty()) {
        setStatusText("Paket adı bulunamadı. (Teknik: boş paket adı)");
        emit finished(false, "Paket adı bulunamadı. (Teknik: boş paket adı)");
        return;
    }

    runPkconTransaction(
        { "pkcon", "-y", "update", cleanPackageName },
        cleanPackageName + " güncelleniyor...",
        cleanPackageName + " başarıyla güncellendi. (İşlem: güncelleme, paket: " + cleanPackageName + ")",
        cleanPackageName + " güncellenemedi. (İşlem: güncelleme, paket: " + cleanPackageName + ")"
    );
}

void PackageInstaller::runPkconTransaction(
    const QStringList &arguments,
    const QString &startMessage,
    const QString &successMessage,
    const QString &failureMessage
)
{
    if (m_running) {
        return;
    }

    const QString pkexecPath = QStandardPaths::findExecutable("pkexec");
    const QString pkconPath = QStandardPaths::findExecutable("pkcon");

    if (pkexecPath.isEmpty() || pkconPath.isEmpty()) {
        const QString message =
            failureMessage + " (Teknik: pkexec veya pkcon bu sistemde bulunamadı)";
        setPhaseText("Desteklenmiyor");
        setStatusText(message);
        emit finished(false, message);
        return;
    }

    m_output.clear();
    emit outputChanged();

    setProgress(0);
    setPhaseText("Hazırlanıyor");

    appendOutput("İşlem başlatıldı\n");
    appendOutput("Komut: " + pkexecPath + " " + arguments.join(" ") + "\n\n");

    setRunning(true);
    setStatusText(startMessage);

    QProcess *process = new QProcess(this);
    process->setProgram(pkexecPath);
    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        const QString text = QString::fromLocal8Bit(process->readAllStandardOutput());

        appendOutput(text);
        parseProgressFromOutput(text);
    });

    connect(process, &QProcess::errorOccurred, this, [this, process, failureMessage](QProcess::ProcessError error) {
        setRunning(false);
        setPhaseText("Hata");

        QString technical = "Teknik: pkexec/pkcon başlatılamadı, QProcess hata kodu: " + QString::number(static_cast<int>(error));
        QString message = failureMessage + " (" + technical + ")";

        appendOutput("\nSonuç: " + message + "\n");
        setStatusText(message);

        emit finished(false, message);
        process->deleteLater();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, successMessage, failureMessage](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;

        setRunning(false);

        if (success) {
            setProgress(100);
            setPhaseText("Tamamlandı");
            appendOutput("\nSonuç: " + successMessage + "\n");
            setStatusText(successMessage);
            emit finished(true, successMessage);
        } else {
            setPhaseText("Hata");

            QString technical = "Teknik: çıkış kodu " + QString::number(exitCode);

            if (exitStatus != QProcess::NormalExit) {
                technical += ", işlem normal tamamlanmadı";
            }

            if (m_output.contains("Failed to obtain authentication", Qt::CaseInsensitive)
                || m_output.contains("authentication", Qt::CaseInsensitive)
                || m_output.contains("Kimlik doğrulaması", Qt::CaseInsensitive)) {
                technical += ", yetkilendirme/polkit hatası";
            }

            QString message = failureMessage + " (" + technical + ")";

            appendOutput("\nSonuç: " + message + "\n");
            setStatusText(message);
            emit finished(false, message);
        }

        process->deleteLater();
    });

    process->start();
}

void PackageInstaller::setRunning(bool value)
{
    if (m_running == value) {
        return;
    }

    m_running = value;
    emit runningChanged();
}

void PackageInstaller::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }

    m_statusText = text;
    emit statusTextChanged();
}

void PackageInstaller::setProgress(int value)
{
    const int normalized = qBound(0, value, 100);

    if (m_progress == normalized) {
        return;
    }

    m_progress = normalized;
    emit progressChanged();

    if (m_running) {
        setStatusText(m_phaseText + "... %" + QString::number(m_progress));
    }
}

void PackageInstaller::setPhaseText(const QString &text)
{
    const QString cleanText = text.trimmed().isEmpty() ? "İşleniyor" : text.trimmed();

    if (m_phaseText == cleanText) {
        return;
    }

    m_phaseText = cleanText;
    emit phaseTextChanged();

    if (m_running) {
        setStatusText(m_phaseText + "... %" + QString::number(m_progress));
    }
}

void PackageInstaller::appendOutput(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    m_output += text;
    emit outputChanged();
}

QString PackageInstaller::normalizePhase(const QString &rawText) const
{
    const QString t = rawText.trimmed().toLower();

    if (t.contains("çöz") || t.contains("resolve")) {
        return "Çözümleniyor";
    }

    if (t.contains("indir") || t.contains("download")) {
        return "İndiriliyor";
    }

    if (t.contains("kurul") || t.contains("install")) {
        return "Kuruluyor";
    }

    if (t.contains("kaldır") || t.contains("sil") || t.contains("remove")) {
        return "Kaldırılıyor";
    }

    if (t.contains("güncel") || t.contains("update")) {
        return "Güncelleniyor";
    }

    if (t.contains("sorgu") || t.contains("query")) {
        return "Sorgulanıyor";
    }

    if (t.contains("bekleniyor") || t.contains("waiting")) {
        return "Bekleniyor";
    }

    if (t.contains("tamam") || t.contains("finished") || t.contains("complete")) {
        return "Tamamlanıyor";
    }

    if (t.isEmpty()) {
        return "İşleniyor";
    }

    return rawText.trimmed().simplified();
}

void PackageInstaller::parseProgressFromOutput(const QString &text)
{
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    static const QRegularExpression progressRegex(
        R"((Yüzde|Percentage)\s*:\s*(\d+))",
        QRegularExpression::CaseInsensitiveOption
    );

    static const QRegularExpression actionRegex(
        R"((İşlem|Role)\s*:\s*(.+))",
        QRegularExpression::CaseInsensitiveOption
    );

    static const QRegularExpression statusRegex(
        R"((Durum|Status)\s*:\s*(.+))",
        QRegularExpression::CaseInsensitiveOption
    );

    for (const QString &lineRaw : lines) {
        const QString line = lineRaw.trimmed();

        QRegularExpressionMatch actionMatch = actionRegex.match(line);
        if (actionMatch.hasMatch()) {
            setPhaseText(normalizePhase(actionMatch.captured(2)));
            continue;
        }

        QRegularExpressionMatch statusMatch = statusRegex.match(line);
        if (statusMatch.hasMatch()) {
            const QString normalized = normalizePhase(statusMatch.captured(2));

            if (normalized != "Bekleniyor" && normalized != "İşleniyor") {
                setPhaseText(normalized);
            }

            continue;
        }

        QRegularExpressionMatch progressMatch = progressRegex.match(line);
        if (progressMatch.hasMatch()) {
            bool ok = false;
            const int value = progressMatch.captured(2).toInt(&ok);

            if (ok) {
                setProgress(value);
            }
        }
    }
}
