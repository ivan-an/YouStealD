#ifndef STREAMMONITOR_H
#define STREAMMONITOR_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QNetworkAccessManager>
#include "logger.h"
#include "authmethod.h"

class StreamMonitor : public QObject
{
    Q_OBJECT
public:
    explicit StreamMonitor(Logger *logger, QObject *parent = nullptr);
    ~StreamMonitor();
    void startMonitoring(const QString &channelUrl, const QString &outputDir, const QString &format, bool monitorStreams, bool monitorVideos, quint16 port);
    void stopMonitoring();
    void stopDownload();
    
    static bool isAuthError(const QString &output);
    static void openBrowser(const QString &url);
    static QString selectCookiesFile();
    void retryWithAuth();
    
signals:
    void statusUpdated(const QString &status);
    void monitoringStarted();
    void monitoringStopped();
    void downloadProgress(const QString &videoId, int progress);
    void authRequired(const QString &videoUrl, const QString &errorMessage);

private slots:
    void checkChannels();
    void checkStreams();
    void checkVideos();
    void downloadContent(const QString &videoId, bool isLive);

private:
    Logger *logger = nullptr;
    QNetworkAccessManager *networkManager = nullptr;
    QTimer *checkTimer = nullptr;
    QProcess *downloadProcess = nullptr;
    quint16 actualPort = 0;
    bool isMonitoring = false;
    bool isDownloading = false;
    QString pendingChannelUrl;
    QString pendingOutputDir;
    QString pendingFormat;
    bool pendingMonitorStreams = false;
    bool pendingMonitorVideos = false;
    QString lastKnownVideoId;
    QString lastKnownStreamId;
    QString stateFilePath;
    QString currentVideoId;
    bool currentIsLive = false;

    void loadState();
    void saveState();
    QString getAuthArgs();
    void downloadContentInternal();
    
public:
    AuthMethod authMethod = AuthMethod::None;
    QString cookiesFilePath;
};

#endif // STREAMMONITOR_H
