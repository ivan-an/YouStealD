#include "downloader.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QFileDialog>
#include <QUrl>
#include <QDir>
#include <windows.h>
#include <winreg.h>

Downloader::Downloader(Logger *logger, QObject *parent)
    : QObject(parent),
    logger(logger),
    downloadProcess(nullptr),
    currentIsStream(false),
    currentIsVideo(false),
    currentIsChannel(false),
    m_lastIsStream(false),
    m_lastIsVideo(false),
    m_lastIsChannel(false)
{
    appDir = QCoreApplication::applicationDirPath();
}

Downloader::~Downloader()
{
    stopDownload();
}

void Downloader::startDownload(const QString &url,
                               const QString &outputFolder,
                               const QString &format,
                               bool isStream,
                               bool isVideo,
                               bool isChannel)
{
    currentUrl = url;
    currentOutputFolder = outputFolder;
    currentFormat = format;
    currentIsStream = isStream;
    currentIsVideo = isVideo;
    currentIsChannel = isChannel;
    
    // Save parameters for retry
    m_lastUrl = url;
    m_lastOutputFolder = outputFolder;
    m_lastFormat = format;
    m_lastIsStream = isStream;
    m_lastIsVideo = isVideo;
    m_lastIsChannel = isChannel;
    
    authMethod = AuthMethod::None;
    cookiesFilePath = "";
    
    startDownloadInternal();
}

void Downloader::startDownloadInternal()
{
    try {
    lastOutput.clear();
    
    if (currentUrl.isEmpty() || currentOutputFolder.isEmpty()) {
        if (logger) logger->log("❌ Ошибка: URL или папка не указаны!");
        emit statusUpdated("❌ Ошибка: Укажите URL и папку!");
        return;
    }
    
    if (downloadProcess) {
        downloadProcess->terminate();
        downloadProcess->waitForFinished(3000);
        downloadProcess->deleteLater();
    }

    downloadProcess = new QProcess(this);

    QString ytDlpPath = appDir + "/yt-dlp.exe";
    QString ffmpegPath = appDir + "/ffmpeg.exe";

    QFile ytDlpFile(ytDlpPath);
    if (!ytDlpFile.exists()) {
        if (logger) logger->log("❌ Ошибка: yt-dlp.exe не найден: " + ytDlpPath);
        emit statusUpdated("❌ yt-dlp.exe не найден!");
        return;
    }

    QFile ffmpegFile(ffmpegPath);
    if (!ffmpegFile.exists()) {
        if (logger) logger->log("❌ Ошибка: ffmpeg.exe не найден: " + ffmpegPath);
        emit statusUpdated("❌ ffmpeg.exe не найден!");
        return;
    }

    QDir outputDir(currentOutputFolder);
    if (!outputDir.exists()) {
        if (!outputDir.mkpath(currentOutputFolder)) {
            if (logger) logger->log("❌ Ошибка: Не удалось создать папку: " + currentOutputFolder);
            emit statusUpdated("❌ Ошибка создания папки!");
            return;
        }
    }

    QStringList arguments;
    QString formatOption;
    QString mergeFormat;
    QString recodeFormat;

    if (!m_proxy.isEmpty()) {
        arguments << "--proxy" << m_proxy;
    }

    bool useAria2c = m_useAria2c && m_proxy.isEmpty();

    if (currentFormat == "MP4 (best)") {
        formatOption = "bestvideo+bestaudio/best";
        mergeFormat = "mp4";
        recodeFormat = "mp4";
    } else if (currentFormat == "MP4 (1080p)") {
        formatOption = "bestvideo[height<=1080]+bestaudio/best";
        mergeFormat = "mp4";
        recodeFormat = "mp4";
    } else if (currentFormat == "MP4 (720p)") {
        formatOption = "bestvideo[height<=720]+bestaudio/best";
        mergeFormat = "mp4";
        recodeFormat = "mp4";
    } else if (currentFormat == "WebM (best)") {
        formatOption = "bestvideo[ext=webm]+bestaudio[ext=webm]/best";
        mergeFormat = "webm";
        recodeFormat = "webm";
    } else if (currentFormat == "Audio only (MP3)") {
        formatOption = "bestaudio";
        arguments << "--extract-audio"
                  << "--audio-format"
                  << "mp3";
        mergeFormat = "";
        recodeFormat = "";
    }

    arguments << "-f" << formatOption;

    if (authMethod != AuthMethod::None) {
        QString authArgs = getAuthArgs();
        if (!authArgs.isEmpty()) {
            QStringList authParts = authArgs.split(" ", Qt::SkipEmptyParts);
            arguments << authParts;
        }
    }

    if (currentIsStream) {
        arguments << "--live-from-start";
        arguments << "--wait-for-video" << "5-60";
        arguments << "--no-part";
        arguments << "--no-warnings"
                  << "--buffer-size" << "1M"
                  << "--js-runtimes" << (appDir + "/deno.exe");
    } else {
        arguments << "--no-part"
                  << "--no-warnings"
                  << "--buffer-size" << "1M"
                  << "--js-runtimes" << (appDir + "/deno.exe");

        arguments << "--retries" << "10"
                  << "--fragment-retries" << "10"
                  << "--retry-sleep" << "fragment:3";

        QString aria2cPath = appDir + "/aria2c.exe";
        QFile aria2cFile(aria2cPath);
        bool aria2cAvailable = useAria2c && aria2cFile.exists();
        
        if (aria2cAvailable) {
            arguments << "--downloader" << "aria2c";
            arguments << "--downloader-args" << "aria2c:-x16";
            arguments << "--concurrent-fragments" << "16";
            if (logger) logger->log("ℹ️ Используется aria2c для загрузки (16 соединений)");
        } else {
            arguments << "--concurrent-fragments" << "4";
            if (useAria2c && !aria2cFile.exists()) {
                if (logger) logger->log("⚠️ aria2c.exe не найден! Используется стандартная загрузка.");
            }
            if (useAria2c && aria2cFile.exists() && !m_proxy.isEmpty()) {
                if (logger) logger->log("⚠️ aria2c отключен при использовании прокси");
            }
        }
    }

    if (!mergeFormat.isEmpty()) {
        arguments << "--merge-output-format" << mergeFormat
                  << "--recode-video" << recodeFormat;
    }

    arguments << "--force-overwrites";
    if (currentIsChannel) {
        arguments << "--ignore-errors";
    }
    arguments << "--ffmpeg-location" << ffmpegPath
              << "-o" << currentOutputFolder + "/%(title)s_%(timestamp)s.%(ext)s"
              << currentUrl;

    if (logger) logger->log("Команда: " + ytDlpPath + " " + arguments.join(" "));
    emit statusUpdated("Статус: Загрузка началась...");

    downloadProcess->start(ytDlpPath, arguments);

    if (!downloadProcess->waitForStarted(3000)) {
        if (logger) logger->log("❌ Ошибка: Не удалось запустить процесс: " + downloadProcess->errorString());
        emit statusUpdated("❌ Ошибка запуска!");
        downloadProcess->deleteLater();
        downloadProcess = nullptr;
        return;
    }

    
    connect(downloadProcess,
            &QProcess::readyReadStandardOutput,
            this,
            [this]() {
                if (!logger) return;

                QByteArray output = downloadProcess->readAllStandardOutput();
                QString outputStr = QString(output);
                lastOutput += outputStr;

                logger->log(outputStr);

                QRegularExpression re("\\[download\\]\\s+(\\d+\\.\\d+)%");
                QRegularExpressionMatch match = re.match(outputStr);

                if (match.hasMatch()) {
                    float progress = match.captured(1).toFloat();
                    emit progressUpdated(static_cast<int>(progress));
                } else {
                    QRegularExpression re2("\\[#\\w+\\s+[\\d.]+[KMG]?i?B/([\\d.]+)[KMG]?i?B\\s*\\((\\d+)%\\)");
                    QRegularExpressionMatch match2 = re2.match(outputStr);
                    if (match2.hasMatch()) {
                        float percent = match2.captured(2).toFloat();
                        emit progressUpdated(static_cast<int>(percent));
                    }
                }
            });

    connect(downloadProcess,
            &QProcess::readyReadStandardError,
            this,
            [this]() {
                if (!logger) return;

                QByteArray error = downloadProcess->readAllStandardError();
                QString errorStr = QString(error);
                lastOutput += errorStr;
                logger->log("Ошибка: " + errorStr);
            });

    connect(downloadProcess,
            &QProcess::finished,
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (!logger) return;

            bool isPlaylistSuccess = false;
            if (exitCode == 1 && exitStatus == QProcess::NormalExit) {
                // Check if this is a playlist download that partially succeeded
                if (lastOutput.contains("Finished downloading playlist") || 
                    lastOutput.contains("Finished downloading")) {
                    isPlaylistSuccess = true;
                }
            }

            if ((exitCode == 0 && exitStatus == QProcess::NormalExit) || isPlaylistSuccess) {
                logger->log("✅ Загрузка завершена!");
                emit statusUpdated("✅ Загрузка завершена!");
                authMethod = AuthMethod::None;

            } else {
                logger->log("❌ Ошибка при загрузке: Код " + QString::number(exitCode));
                
                bool needsAuth = isAuthError(lastOutput);
                
                if (needsAuth && authMethod == AuthMethod::None) {
                    logger->log("🔐 Требуется авторизация!");
                    emit authRequired(currentUrl, lastOutput);
                } else {
                    emit statusUpdated("❌ Ошибка при загрузке!");
                    
                    if (authMethod != AuthMethod::None) {
                        logger->log("⚠️ Авторизация не помогла, пробуем обновить yt-dlp...");
                        updateYtDlpAuto();
                    } else {
                        logger->log("🔄 Попытка обновить yt-dlp...");
                        updateYtDlpAuto();
                    }
                }
            }

                downloadProcess->deleteLater();
                downloadProcess = nullptr;

                emit downloadFinished();
            });

    emit downloadStarted();
    } catch (std::exception& e) {
        if (logger) logger->log(QString("❌ Исключение: ") + e.what());
    } catch (...) {
        if (logger) logger->log("❌ Неизвестное исключение!");
    }
}

void Downloader::stopDownload()
{
    QString outputFolder = currentOutputFolder;
    
    if (downloadProcess) {
        downloadProcess->terminate();
        if (!downloadProcess->waitForFinished(3000)) {
            downloadProcess->kill();
        }
        delete downloadProcess;
        downloadProcess = nullptr;
    }
    
    if (!outputFolder.isEmpty()) {
        QDir dir(outputFolder);
        if (dir.exists()) {
            QStringList filters;
            filters << "*.part" << "*.ytdl" << "*.part-Frag*" << "*.f*";
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
            
            for (const QFileInfo &file : files) {
                QFile::remove(file.absoluteFilePath());
                if (logger) logger->log("🧹 Удален частичный файл: " + file.fileName());
            }
        }
    }
}

void Downloader::updateYtDlpAuto()
{
    QString ytDlpPath = appDir + "/yt-dlp.exe";
    QFile ytDlpFile(ytDlpPath);

    if (!ytDlpFile.exists()) {
        return;
    }

    if (logger) logger->log("🔄 Автообновление yt-dlp...");

    QProcess *updateProcess = new QProcess(this);

    connect(updateProcess,
            &QProcess::finished,
            this,
            [this, updateProcess](int exitCode, QProcess::ExitStatus exitStatus) {
                if (!logger) return;

                QString output = updateProcess->readAllStandardOutput();
                QString error  = updateProcess->readAllStandardError();

                if (exitCode == 0 && exitStatus == QProcess::NormalExit) {

                    if (output.contains("yt-dlp is up to date") ||
                        output.contains("уже最新版本")) {

                        logger->log("ℹ️ yt-dlp уже последней версии");

                    } else {
                        logger->log("✅ yt-dlp обновлён! Попробуйте загрузить снова.");
                        emit statusUpdated("✅ yt-dlp обновлён! Попробуйте загрузить снова.");
                    }

                } else {
                    logger->log("⚠️ Не удалось обновить yt-dlp");
                }

                updateProcess->deleteLater();
            });

    updateProcess->start(ytDlpPath, QStringList() << "--update");
}

bool Downloader::isAuthError(const QString &output)
{
    QString lower = output.toLower();
    
    QStringList authErrors = {
        "sign in to confirm",
        "sign in to confirm your age",
        "this video is available to members only",
        "this video is private",
        "this video has been removed",
        "this video is not available",
        "login required",
        "please sign in",
        "authentication required",
        "hrzngl",
        "this content is not available",
        "video unavailable",
        "unavailable",
        "member",
        "membership"
    };
    
    for (const QString &error : authErrors) {
        if (lower.contains(error)) {
            return true;
        }
    }
    
    return false;
}

void Downloader::openBrowser(const QString &url)
{
    ShellExecuteA(NULL, "open", url.toUtf8().constData(), NULL, NULL, SW_SHOWNORMAL);
}

QString Downloader::selectCookiesFile()
{
    return QFileDialog::getOpenFileName(
        nullptr,
        "Выберите файл cookies",
        QDir::homePath(),
        "Cookies files (*.txt);;All files (*.*)"
    );
}

void Downloader::retryWithAuth()
{
    if (logger) logger->log("🔄 Повторная попытка с авторизацией...");
    startDownloadInternal();
}

void Downloader::retryLastDownload()
{
    // Restore the last download parameters
    if (m_lastUrl.isEmpty() || m_lastOutputFolder.isEmpty()) {
        if (logger) logger->log("❌ Ошибка: Нет параметров для повтора!");
        emit statusUpdated("❌ Ошибка: Нет параметров для повтора!");
        return;
    }
    
    currentUrl = m_lastUrl;
    currentOutputFolder = m_lastOutputFolder;
    currentFormat = m_lastFormat;
    currentIsStream = m_lastIsStream;
    currentIsVideo = m_lastIsVideo;
    currentIsChannel = m_lastIsChannel;
    startDownloadInternal();
}

QString Downloader::getAuthArgs()
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
