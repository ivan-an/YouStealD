#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QObject>
#include <QProcess>
#include "logger.h"
#include "authmethod.h"

class Downloader : public QObject
{
    Q_OBJECT

public:
    explicit Downloader(Logger *logger, QObject *parent = nullptr);
    ~Downloader();

    void startDownload(const QString &url, const QString &outputFolder, const QString &format, bool isStream, bool isVideo, bool isChannel);
    void stopDownload();
    void updateYtDlp();
    void updateYtDlpAuto();
    void handleAuthRequired(const QString &url, const QString &errorMessage);
    
    static bool isAuthError(const QString &output);
    static void openBrowser(const QString &url);
    static QString selectCookiesFile();
    void retryWithAuth();
    
    AuthMethod authMethod = AuthMethod::None;
    QString cookiesFilePath;

signals:
    void progressUpdated(int value);
    void statusUpdated(const QString &status);
    void downloadStarted();
    void downloadFinished();
    void updateFinished(bool success, const QString &message);
    void authRequired(const QString &videoUrl, const QString &errorMessage);

private:
    Logger *logger = nullptr;
    QProcess *downloadProcess = nullptr;
    QString appDir;
    QString currentUrl;
    QString currentOutputFolder;
    QString currentFormat;
    bool currentIsStream = false;
    bool currentIsVideo = false;
    bool currentIsChannel = false;
    QString lastOutput;
    
    // Store last download parameters for auth retry
    QString m_lastUrl;
    QString m_lastOutputFolder;
    QString m_lastFormat;
    bool m_lastIsStream = false;
    bool m_lastIsVideo = false;
    bool m_lastIsChannel = false;
    
    void startDownloadInternal();
    QString getAuthArgs();
    
public slots:
    void retryLastDownload();
};

#endif // DOWNLOADER_H
