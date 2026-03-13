#ifndef WEBHOOKMANAGER_H
#define WEBHOOKMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include "logger.h"
#include "webhookserver.h"

class WebhookManager : public QObject
{
    Q_OBJECT
public:
    explicit WebhookManager(Logger *logger, QObject *parent = nullptr);
    ~WebhookManager();
    bool startWebhook(const QString &channelUrl, const QString &webhookUrl, const QString &outputDir, const QString &format, bool monitorStreams, bool monitorVideos, quint16 &serverPort);
    void stopWebhook();
    void getChannelIdFromUrl(const QString &channelUrl);
    void setApiKey(const QString &key) { apiKey = key; }
    WebhookServer* getWebhookServer() const { return webhookServer; }

signals:
    void channelIdReceived(const QString &channelId);
    void subscriptionFinished(bool success);

private slots:
    void onChannelIdReplyFinished(QNetworkReply *reply);
    void onSubscriptionReplyFinished(QNetworkReply *reply);
    void onChannelIdReceivedForSubscription(const QString &channelId);

private:
    Logger *logger;
    WebhookServer *webhookServer;
    QNetworkAccessManager *networkManager;
    QString apiKey;
    QString channelId;
    QString pendingWebhookUrl;
    QString pendingOutputDir;
    QString pendingFormat;
    bool pendingMonitorStreams;
    bool pendingMonitorVideos;
    quint16 *pendingServerPort;
};

#endif // WEBHOOKMANAGER_H
