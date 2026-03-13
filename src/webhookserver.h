#ifndef WEBHOOKSERVER_H
#define WEBHOOKSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include "logger.h"
#include <QTimer>
#include <QSet>
#include <QProcess>

class WebhookServer : public QObject
{
    Q_OBJECT
public:
    explicit WebhookServer(Logger *logger, const QString &apiKey, const QString &outputDir, const QString &format, bool monitorStreams, bool monitorVideos, QObject *parent = nullptr);
    ~WebhookServer();
    bool startServer(quint16 preferredPort, quint16 &actualPort);
    void checkOngoingContent(const QString &channelId);
    void onVideoStatusChecked();
    void checkVideoStatus();
    void checkVideoStatus(const QString &videoId);
    void recordStream(const QString &videoId);
    void setOutputDir(const QString &newOutputDir) { outputDir = newOutputDir; }
    void close() { if (server->isListening()) server->close(); }
    void stopServer();

signals:
    void contentDetected(const QString &videoId);
    void recordingProgress(const QString &videoId, int progress);
    void recordingStarted(const QString &videoId);
    void recordingFinished(const QString &videoId, bool success);

private slots:
    void handleNewConnection();
    void handleClientData();
    void handleApiReply(QNetworkReply *reply);
    void onContentDetected(const QString &videoId);
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onRequestTimeout(); // Новый слот для таймаута

private:
    bool isStream(const QString &videoId);
    void recordContent(const QString &videoId);

    Logger *logger;
    QTcpServer *server;
    QNetworkAccessManager *networkManager;
    QString apiKey;
    QString outputDir;
    QString format;
    bool monitorStreams;
    bool monitorVideos;
    bool isStreamResult;
    QTimer* checkStatusTimer;
    QString currentVideoId;
    QSet<QString> videosToCheck;
    QProcess *recordProcess;
    QString currentRecordingVideoId;
    QTimer *requestTimer; // Таймер для таймаута запросов
    QNetworkReply *currentReply; // Текущий запрос для отслеживания
    QString currentRequestVideoId; // Контекст для текущего запроса
};

#endif // WEBHOOKSERVER_H
