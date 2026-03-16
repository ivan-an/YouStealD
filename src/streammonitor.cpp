#include "streammonitor.h"
#include "appconstants.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QDateTime>
#include <QRegularExpression>
#include <QFileDialog>
#include <QSettings>
#include <windows.h>

StreamMonitor::StreamMonitor(Logger *logger, QObject *parent)
    : QObject(parent)
    , logger(logger)
    , networkManager(new QNetworkAccessManager(this))
    , checkTimer(new QTimer(this))
    , downloadProcess(nullptr)
    , actualPort(0)
    , isMonitoring(false)
    , isDownloading(false)
    , pendingChannelUrl("")
    , pendingOutputDir("")
    , pendingFormat("")
    , pendingMonitorStreams(false)
    , pendingMonitorVideos(false)
    , lastKnownVideoId("")
    , lastKnownStreamId("")
    , stateFilePath("")
    , currentVideoId("")
    , currentIsLive(false)
    , authMethod(AuthMethod::None)
    , cookiesFilePath("")
{
    checkTimer->setSingleShot(false);
    connect(checkTimer, &QTimer::timeout, this, &StreamMonitor::checkChannels);

    stateFilePath = QCoreApplication::applicationDirPath() + "/monitor_state.txt";
    loadState();
}

StreamMonitor::~StreamMonitor()
{
    stopMonitoring();
}

void StreamMonitor::startMonitoring(const QString &channelUrl, const QString &outputDir, const QString &format, bool monitorStreams, bool monitorVideos, quint16 port)
{
    if (isMonitoring) {
        logger->log("⚠ Мониторинг уже запущен!");
        return;
    }

    actualPort = port;
    pendingChannelUrl = channelUrl;
    pendingOutputDir = outputDir;
    pendingFormat = format;
    pendingMonitorStreams = monitorStreams;
    pendingMonitorVideos = monitorVideos;

    isMonitoring = true;

    logger->log("📺 Начинаем мониторинг канала: " + channelUrl);
    logger->log("📁 Папка для сохранения: " + outputDir);
    logger->log("⚙ Формат: " + format);
    logger->log("🔖 Последний известный ID: " + lastKnownVideoId);

    if (monitorStreams) {
        logger->log("✅ Отслеживание стримов включено");
    }
    if (monitorVideos) {
        logger->log("✅ Отслеживание видео включено");
    }

    checkChannels();
    checkTimer->start(60000);

    emit monitoringStarted();
}

void StreamMonitor::stopMonitoring()
{
    if (!pendingOutputDir.isEmpty()) {
        QDir dir(pendingOutputDir);
        if (dir.exists()) {
            QStringList filters;
            filters << "*_stream.*" << "*_video.*" << "*.part" << "*.ytdl" << "*.part-Frag*" << "*.f*";
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
            
            for (const QFileInfo &file : files) {
                QFile::remove(file.absoluteFilePath());
                logger->log("🗑 Удалён файл при остановке мониторинга: " + file.fileName());
            }
        }
    }

    isMonitoring = false;
    checkTimer->stop();

    logger->log("⏹ Мониторинг остановлен");
    emit monitoringStopped();
}

void StreamMonitor::checkChannels()
{
    if (!isMonitoring) return;

    if (isDownloading) {
        logger->log("⏳ Идет загрузка, пропускаем проверку канала");
        return;
    }

    logger->log("🔍 Проверяем канал: " + pendingChannelUrl);

    if (pendingMonitorStreams) {
        checkStreams();
    }

    if (pendingMonitorVideos) {
        checkVideos();
    }
}

void StreamMonitor::checkStreams()
{
    QString streamsUrl = pendingChannelUrl;
    if (!streamsUrl.contains("/streams")) {
        if (streamsUrl.endsWith("/")) {
            streamsUrl.chop(1);
        }
        streamsUrl += "/streams";
    }

    logger->log("🔴 Проверяем стримы: " + streamsUrl);

    QProcess *process = new QProcess(this);
    QString appDir = QCoreApplication::applicationDirPath();
    QString ytDlpPath = appDir + "/yt-dlp.exe";
    QString cookiesPath = QDir::toNativeSeparators(appDir + "/cookies.txt");
    QFile cookiesFile(cookiesPath);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", appDir + ";" + env.value("PATH"));

    QStringList arguments;

    if (cookiesFile.exists()) {
        arguments << "--cookies" << cookiesPath;
        logger->log("🍪 Используем куки: " + cookiesPath);
    } else {
        logger->log("⚠️ Куки не найдены, используем API ключ");
    }

    arguments << "--print" << "%(id)s|%(live_status)s|%(title)s"
              << "--playlist-end" << "5"
              << streamsUrl;

    logger->log("Команда (стримы): " + ytDlpPath + " " + arguments.join(" "));

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process](int exitCode, QProcess::ExitStatus) {
        QString output = process->readAllStandardOutput();
        QString error = process->readAllStandardError();

        if (!error.isEmpty() && !error.contains("WARNING")) {
            logger->log("⚠ Ошибка: " + error.left(300));
        }

        if (exitCode == 0 && !output.isEmpty()) {
            QStringList lines = output.split("\n", Qt::SkipEmptyParts);
            logger->log("📋 Найдено " + QString::number(lines.size()) + " стримов/видео на /streams");

            for (const QString &line : lines) {
                QStringList parts = line.split("|");
                if (parts.size() >= 2) {
                    QString videoId = parts[0].trimmed();
                    QString liveStatus = parts[1].trimmed();
                    QString title = parts.size() >= 3 ? parts[2].trimmed() : "";

                    bool isLiveStream = (liveStatus == "is_live" || liveStatus == "is_upcoming");
                    bool wasLive = (liveStatus == "post_live" || liveStatus == "post_live_draft");

                    if (isLiveStream) {
                        logger->log("🔴 НАЙДЕН АКТИВНЫЙ СТРИМ: " + videoId + " (" + liveStatus + ") - " + title);
                        
                        bool alreadyDownloading = (downloadProcess != nullptr && currentVideoId == videoId && downloadProcess->state() == QProcess::Running);
                        
                        if (!alreadyDownloading) {
                            logger->log("📥 Начинаем запись стрима: " + videoId);
                            downloadContent(videoId, true);
                            lastKnownStreamId = videoId;
                            saveState();
                        } else {
                            logger->log("⏳ Стрим уже записывается: " + videoId);
                        }
                    } else if (wasLive) {
                        logger->log("✅ Завершённый стрим: " + videoId + " (" + liveStatus + ")");
                        if (videoId == lastKnownStreamId) {
                            lastKnownStreamId = "";
                            saveState();
                            logger->log("🔄 Стрим завершён, сбрасываем ID");
                        }
                    }
                }
            }
        } else {
            logger->log("ℹ️ Нет активных стримов");
        }

        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        logger->log("❌ Ошибка запуска yt-dlp (streams): " + process->errorString());
        process->deleteLater();
    });

    process->setProcessEnvironment(env);
    process->start(ytDlpPath, arguments);
}

void StreamMonitor::checkVideos()
{
    QString videosUrl = pendingChannelUrl;
    if (videosUrl.contains("/streams")) {
        videosUrl = videosUrl.replace("/streams", "/videos");
    } else if (!videosUrl.contains("/videos") && !videosUrl.contains("playlist?list=")) {
        if (videosUrl.endsWith("/")) {
            videosUrl.chop(1);
        }
        videosUrl += "/videos";
    }

    logger->log("📹 Проверяем видео: " + videosUrl);

    QProcess *process = new QProcess(this);
    QString appDir = QCoreApplication::applicationDirPath();
    QString ytDlpPath = appDir + "/yt-dlp.exe";
    QString cookiesPath = QDir::toNativeSeparators(appDir + "/cookies.txt");
    QFile cookiesFile(cookiesPath);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", appDir + ";" + env.value("PATH"));

    QStringList arguments;

    if (cookiesFile.exists()) {
        arguments << "--cookies" << cookiesPath;
        logger->log("🍪 Используем куки: " + cookiesPath);
    } else {
        logger->log("⚠️ Куки не найдены, используем API ключ");
    }

    arguments << "--print" << "%(id)s|%(epoch)s|%(live_status)s|%(title)s"
              << "--playlist-end" << "3"
              << videosUrl;

    logger->log("Команда (видео): " + ytDlpPath + " " + arguments.join(" "));

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process](int exitCode, QProcess::ExitStatus) {
        QString output = process->readAllStandardOutput();
        QString error = process->readAllStandardError();

        if (!error.isEmpty() && !error.contains("WARNING")) {
            logger->log("⚠ Ошибка: " + error.left(300));
        }

        if (exitCode == 0 && !output.isEmpty()) {
            QStringList lines = output.split("\n", Qt::SkipEmptyParts);
            logger->log("📋 Найдено " + QString::number(lines.size()) + " последних видео");

            for (const QString &line : lines) {
                QStringList parts = line.split("|");
                if (parts.size() >= 2) {
                    QString videoId = parts[0].trimmed();
                    QString epochStr = parts[1].trimmed();
                    QString liveStatus = parts.size() >= 3 ? parts[2].trimmed() : "";
                    QString title = parts.size() >= 4 ? parts[3].trimmed() : "";

                    if (videoId.isEmpty() || videoId.length() < 5) continue;

                    if (liveStatus == "is_live" || liveStatus == "is_upcoming") {
                        logger->log("🔴 Пропускаем стрим в видео: " + videoId);
                        continue;
                    }

                    qint64 videoEpoch = epochStr.toLongLong();
                    qint64 nowEpoch = QDateTime::currentSecsSinceEpoch();
                    qint64 diffSeconds = nowEpoch - videoEpoch;

                    QString timeAgo;
                    if (diffSeconds < 60) {
                        timeAgo = QString::number(diffSeconds) + " сек назад";
                    } else if (diffSeconds < 3600) {
                        timeAgo = QString::number(diffSeconds / 60) + " мин назад";
                    } else {
                        timeAgo = QString::number(diffSeconds / 3600) + " ч назад";
                    }

                    logger->log("📹 " + videoId + " | " + timeAgo + " | " + title);

                    if (videoId == lastKnownVideoId) {
                        logger->log("⏳ Видео уже известно: " + videoId);
                        break;
                    }

                    bool isNew = (diffSeconds < 1800);

                    if (isNew) {
                        logger->log("📥 Новое видео, скачиваем: " + videoId);
                        downloadContent(videoId, false);
                        lastKnownVideoId = videoId;
                        saveState();
                        break;
                    } else {
                        logger->log("⏳ Старое видео, пропускаем");
                        lastKnownVideoId = videoId;
                        saveState();
                        break;
                    }
                }
            }
        } else {
            logger->log("⚠ Не удалось получить список видео");
        }

        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        logger->log("❌ Ошибка запуска yt-dlp (videos): " + process->errorString());
        process->deleteLater();
    });

    process->setProcessEnvironment(env);
    process->start(ytDlpPath, arguments);
}

void StreamMonitor::downloadContent(const QString &videoId, bool isLive)
{
    if (downloadProcess) {
        logger->log("⚠ Уже идет загрузка!");
        return;
    }

    currentVideoId = videoId;
    currentIsLive = isLive;
    currentOutputDir = pendingOutputDir;

    QString url = QString("https://www.youtube.com/watch?v=%1").arg(videoId);
    QString outputFile = QString("%1/%2_%3.%(ext)s").arg(pendingOutputDir, videoId, isLive ? "stream" : "video");

    QDir outputDir(pendingOutputDir);
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(pendingOutputDir)) {
            logger->log("❌ Ошибка: Не удалось создать папку: " + pendingOutputDir);
            return;
        }
    }

    downloadProcess = new QProcess(this);
    QString appDir = QCoreApplication::applicationDirPath();
    QString ytDlpPath = appDir + "/yt-dlp.exe";
    QString cookiesPath = QDir::toNativeSeparators(appDir + "/cookies.txt");
    QFile cookiesFile(cookiesPath);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", appDir + ";" + env.value("PATH"));

    QStringList arguments;
    arguments << "--no-warnings";

    if (!m_proxy.isEmpty()) {
        arguments << "--proxy" << m_proxy;
    }

    arguments << "--no-warnings"
              << "--buffer-size" << "1M"
              << "--js-runtimes" << (appDir + "/deno.exe");

    QSettings settings(ORG_NAME, APP_NAME);
    bool useCookies = settings.value("monitorUseCookies", false).toBool();
    QString cookiesFilePath = settings.value("monitorCookiesFile", "").toString();
    
    if (useCookies && !cookiesFilePath.isEmpty()) {
        QFile cookiesFile(cookiesFilePath);
        if (cookiesFile.exists()) {
            arguments << "--cookies" << cookiesFilePath;
            logger->log("🍪 Используем куки из файла: " + QFileInfo(cookiesFilePath).fileName());
        } else {
            arguments << "--cookies-from-browser" << "chrome";
            logger->log("🍪 Файл кук не найден, используем Chrome");
        }
    } else if (cookiesFile.exists()) {
        arguments << "--cookies" << cookiesPath;
        logger->log("🍪 Используем куки из файла");
    } else {
        arguments << "--cookies-from-browser" << "chrome";
        logger->log("🍪 Используем куки из Chrome");
    }

    if (isLive) {
        arguments << "--live-from-start" << "--no-part";
        logger->log("🔴 Начинаем запись стрима: " + videoId);
    } else {
        logger->log("📥 Начинаем скачивание видео: " + videoId);
        arguments << "--download-archive" << (pendingOutputDir + "/archive.txt");
    }

    arguments << "-o" << outputFile
              << "--progress"
              << url;

    auto parseProgress = [this, videoId](const QString &output) {
        if (!output.isEmpty()) {
            logger->log("📜 Вывод: " + output.trimmed().left(200));
        }

        QRegularExpression progressRx("\\[download\\]\\s+(\\d+\\.\\d+)%");
        QRegularExpressionMatch match = progressRx.match(output);
        if (match.hasMatch()) {
            QString progressStr = match.captured(1);
            bool ok;
            float progress = progressStr.toFloat(&ok);
            if (ok) {
                int progressPercent = static_cast<int>(progress);
                logger->log("📥 Прогресс: " + QString::number(progressPercent) + "%");
                emit downloadProgress(videoId, progressPercent);
            }
        }

        QRegularExpression speedRx("\\[download\\]\\s+(\\d+\\.\\d+%)\\s+of\\s+(\\d+\\.\\d+[GM]iB)\\s+at\\s+(\\d+\\.\\d+[GM]iB/s)");
        QRegularExpressionMatch speedMatch = speedRx.match(output);
        if (speedMatch.hasMatch()) {
            QString percent = speedMatch.captured(1);
            QString size = speedMatch.captured(2);
            QString speed = speedMatch.captured(3);
            logger->log("📥 " + percent + " из " + size + " @ " + speed);
        }

        QRegularExpression fragRx("\\[download\\]\\s+(\\d+\\.\\d+)%\\s+of\\s+(\\d+\\.\\d+[GM]iB)\\s+at\\s+(\\d+\\.\\d+[GM]iB/s)\\s+\\(frag (\\d+)/(\\d+)\\)");
        QRegularExpressionMatch fragMatch = fragRx.match(output);
        if (fragMatch.hasMatch()) {
            QString percent = fragMatch.captured(1);
            QString size = fragMatch.captured(2);
            QString speed = fragMatch.captured(3);
            logger->log("📥 Фрагмент: " + percent + " из " + size + " @ " + speed);
        }
    };

    connect(downloadProcess, &QProcess::readyReadStandardOutput, this, [this, videoId, parseProgress]() {
        QString output = QString::fromUtf8(downloadProcess->readAllStandardOutput());
        parseProgress(output);
    });

    connect(downloadProcess, &QProcess::readyReadStandardError, this, [this, videoId, parseProgress]() {
        QString output = QString::fromUtf8(downloadProcess->readAllStandardError());
        parseProgress(output);
    });

    connect(downloadProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, videoId, isLive](int exitCode, QProcess::ExitStatus) {
        isDownloading = false;
        emit downloadProgress(videoId, -1);

        QString stderrOutput = QString::fromUtf8(downloadProcess->readAllStandardError());

        if (exitCode == 0) {
            logger->log(isLive ? "✅ Стрим записан: " : "✅ Видео скачано: " + videoId);
        } else {
            logger->log("❌ Ошибка скачивания (код " + QString::number(exitCode) + ")");
            if (!stderrOutput.isEmpty()) {
                QString shortOutput = stderrOutput.left(500).replace("\n", " | ");
                logger->log("Вывод: " + shortOutput);

                if (isAuthError(stderrOutput)) {
                    logger->log("🔐 Требуется авторизация!");
                    QString videoUrl = QString("https://www.youtube.com/watch?v=%1").arg(videoId);
                    emit authRequired(videoUrl, stderrOutput.left(200));
                }
            }
        }
        downloadProcess->deleteLater();
        downloadProcess = nullptr;
    });

    isDownloading = true;
    downloadProcess->setProcessEnvironment(env);
    downloadProcess->start(ytDlpPath, arguments);

    if (!downloadProcess->waitForStarted(5000)) {
        isDownloading = false;
        logger->log("❌ Не удалось запустить yt-dlp");
        downloadProcess->deleteLater();
        downloadProcess = nullptr;
        return;
    }
}

void StreamMonitor::downloadContentInternal()
{
    if (currentVideoId.isEmpty()) {
        logger->log("⚠ Нет данных для повторной загрузки");
        return;
    }

    if (downloadProcess) {
        logger->log("⚠ Уже идет загрузка!");
        return;
    }

    QString url = QString("https://www.youtube.com/watch?v=%1").arg(currentVideoId);
    QString outputFile = QString("%1/%2_%3.%(ext)s").arg(pendingOutputDir, currentVideoId, currentIsLive ? "stream" : "video");

    downloadProcess = new QProcess(this);
    QString appDir = QCoreApplication::applicationDirPath();
    QString ytDlpPath = appDir + "/yt-dlp.exe";

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", appDir + ";" + env.value("PATH"));

    QStringList arguments;
    arguments << "--no-warnings";

    if (currentIsLive) {
        arguments << "--live-from-start" << "--no-part";
        logger->log("🔴 Повторная запись стрима с авторизацией: " + currentVideoId);
    } else {
        logger->log("📥 Повторное скачивание видео с авторизацией: " + currentVideoId);
        arguments << "--download-archive" << (pendingOutputDir + "/archive.txt");
    }

    QString authArgs = getAuthArgs();
    if (!authArgs.isEmpty()) {
        QStringList authList = authArgs.split(" ", Qt::SkipEmptyParts);
        arguments << authList;
    }

    arguments << "-o" << outputFile
              << "--progress"
              << url;

    auto parseProgress = [this](const QString &output) {
        if (!output.isEmpty()) {
            logger->log("📜 Вывод: " + output.trimmed().left(200));
        }

        QRegularExpression progressRx("\\[download\\]\\s+(\\d+\\.\\d+)%");
        QRegularExpressionMatch match = progressRx.match(output);
        if (match.hasMatch()) {
            QString progressStr = match.captured(1);
            bool ok;
            float progress = progressStr.toFloat(&ok);
            if (ok) {
                int progressPercent = static_cast<int>(progress);
                logger->log("📥 Прогресс: " + QString::number(progressPercent) + "%");
                emit downloadProgress(currentVideoId, progressPercent);
            }
        }

        QRegularExpression speedRx("\\[download\\]\\s+(\\d+\\.\\d+%)\\s+of\\s+(\\d+\\.\\d+[GM]iB)\\s+at\\s+(\\d+\\.\\d+[GM]iB/s)");
        QRegularExpressionMatch speedMatch = speedRx.match(output);
        if (speedMatch.hasMatch()) {
            QString percent = speedMatch.captured(1);
            QString size = speedMatch.captured(2);
            QString speed = speedMatch.captured(3);
            logger->log("📥 " + percent + " из " + size + " @ " + speed);
        }

        QRegularExpression fragRx("\\[download\\]\\s+(\\d+\\.\\d+)%\\s+of\\s+(\\d+\\.\\d+[GM]iB)\\s+at\\s+(\\d+\\.\\d+[GM]iB/s)\\s+\\(frag (\\d+)/(\\d+)\\)");
        QRegularExpressionMatch fragMatch = fragRx.match(output);
        if (fragMatch.hasMatch()) {
            QString percent = fragMatch.captured(1);
            QString size = fragMatch.captured(2);
            QString speed = fragMatch.captured(3);
            logger->log("📥 Фрагмент: " + percent + " из " + size + " @ " + speed);
        }
    };

    connect(downloadProcess, &QProcess::readyReadStandardOutput, this, [this, parseProgress]() {
        QString output = QString::fromUtf8(downloadProcess->readAllStandardOutput());
        parseProgress(output);
    });

    connect(downloadProcess, &QProcess::readyReadStandardError, this, [this, parseProgress]() {
        QString output = QString::fromUtf8(downloadProcess->readAllStandardError());
        parseProgress(output);
    });

    connect(downloadProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus) {
        isDownloading = false;
        emit downloadProgress(currentVideoId, -1);

        QString stderrOutput = QString::fromUtf8(downloadProcess->readAllStandardError());

        if (exitCode == 0) {
            logger->log(currentIsLive ? "✅ Стрим записан (с авторизацией): " : "✅ Видео скачано (с авторизацией): " + currentVideoId);
        } else {
            logger->log("❌ Ошибка скачивания с авторизацией (код " + QString::number(exitCode) + ")");
            if (!stderrOutput.isEmpty()) {
                QString shortOutput = stderrOutput.left(500).replace("\n", " | ");
                logger->log("Вывод: " + shortOutput);
            }
        }
        downloadProcess->deleteLater();
        downloadProcess = nullptr;
    });

    isDownloading = true;
    downloadProcess->setProcessEnvironment(env);
    downloadProcess->start(ytDlpPath, arguments);

    if (!downloadProcess->waitForStarted(5000)) {
        isDownloading = false;
        logger->log("❌ Не удалось запустить yt-dlp");
        downloadProcess->deleteLater();
        downloadProcess = nullptr;
        return;
    }
}

void StreamMonitor::loadState()
{
    if (!logger) return;

    QFile file(stateFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QString content = QString::fromUtf8(file.readAll()).trimmed();
        file.close();

        QStringList parts = content.split("|");
        if (parts.size() >= 1) {
            lastKnownVideoId = parts[0];
        }
        if (parts.size() >= 2) {
            lastKnownStreamId = parts[1];
        }

        logger->log("📂 Загружено состояние: видео=" + lastKnownVideoId + " стрим=" + lastKnownStreamId);
    }
}

void StreamMonitor::saveState()
{
    QFile file(stateFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QString content = lastKnownVideoId + "|" + lastKnownStreamId;
        file.write(content.toUtf8());
        file.close();
        logger->log("💾 Сохранено состояние: видео=" + lastKnownVideoId + " стрим=" + lastKnownStreamId);
    }
}

void StreamMonitor::stopDownload()
{
    bool wasDownloading = false;

    if (downloadProcess) {
        downloadProcess->disconnect();

        if (downloadProcess->state() == QProcess::Running) {
            downloadProcess->terminate();
            if (!downloadProcess->waitForFinished(1000)) {
                downloadProcess->kill();
                downloadProcess->waitForFinished(500);
            }
        }

        downloadProcess->deleteLater();
        downloadProcess = nullptr;
        isDownloading = false;
        wasDownloading = true;
        logger->log("⏹ Загрузка остановлена!");
    }

    QProcess killYtDlp;
    killYtDlp.start("taskkill", QStringList() << "/F" << "/T" << "/IM" << "yt-dlp.exe");
    killYtDlp.waitForFinished(2000);

    QProcess killFfmpeg;
    killFfmpeg.start("taskkill", QStringList() << "/F" << "/T" << "/IM" << "ffmpeg.exe");
    killFfmpeg.waitForFinished(2000);

    if (!currentOutputDir.isEmpty() && !currentVideoId.isEmpty()) {
        QDir dir(currentOutputDir);
        QStringList filters;
        filters << QString("%1_stream.*").arg(currentVideoId)
                << QString("%1_video.*").arg(currentVideoId)
                << QString("%1_stream.*.ytdl").arg(currentVideoId)
                << QString("%1_video.*.ytdl").arg(currentVideoId)
                << QString("%1_stream.*-Frag*").arg(currentVideoId)
                << QString("%1_video.*-Frag*").arg(currentVideoId);

        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo &file : files) {
            QFile::remove(file.absoluteFilePath());
            logger->log("🗑 Удалён partial файл: " + file.fileName());
        }
    }

    if (wasDownloading && isMonitoring) {
        lastKnownStreamId = "";
        saveState();
        stopMonitoring();
    }
}

bool StreamMonitor::isAuthError(const QString &output)
{
    QString lower = output.toLower();

    QStringList authErrors = {
        "sign in to confirm",
        "sign in to confirm your age",
        "this video is available to members only",
        "this video is private",
        "this video has been removed",
        "login required",
        "please sign in",
        "authentication required",
        "hrzngl",
        "this content is not available",
        "video unavailable"
    };

    for (const QString &error : authErrors) {
        if (lower.contains(error)) {
            return true;
        }
    }

    return false;
}

void StreamMonitor::openBrowser(const QString &url)
{
    ShellExecuteA(NULL, "open", url.toUtf8().constData(), NULL, NULL, SW_SHOWNORMAL);
}

QString StreamMonitor::selectCookiesFile()
{
    return QFileDialog::getOpenFileName(
        nullptr,
        "Выберите файл cookies",
        QDir::homePath(),
        "Cookies files (*.txt);;All files (*.*)"
        );
}

void StreamMonitor::retryWithAuth()
{
    logger->log("🔄 Повторная попытка с авторизацией...");
    downloadContentInternal();
}

QString StreamMonitor::getAuthArgs()
{
    QStringList args;

    switch (authMethod) {
    case AuthMethod::BrowserChrome:
        args << "--cookies-from-browser" << "chrome";
        break;
    case AuthMethod::BrowserFirefox:
        args << "--cookies-from-browser" << "firefox";
        break;
    case AuthMethod::BrowserEdge:
        args << "--cookies-from-browser" << "edge";
        break;
    case AuthMethod::CookiesFile:
        if (!cookiesFilePath.isEmpty()) {
            args << "--cookies" << cookiesFilePath;
        }
        break;
    case AuthMethod::None:
    default:
        break;
    }

    return args.join(" ");
}
