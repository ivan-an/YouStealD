#include "webhookmanager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QUrl>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QCoreApplication>

WebhookManager::WebhookManager(Logger *logger, QObject *parent)
    : QObject(parent)
    , logger(logger)
    , webhookServer(nullptr)
    , networkManager(new QNetworkAccessManager(this))
    , apiKey("")
    , pendingServerPort(nullptr)
{
}

WebhookManager::~WebhookManager()
{
    stopWebhook();
}

void WebhookManager::getChannelIdFromUrl(const QString &channelUrl)
{
    if (apiKey.isEmpty()) {
        logger->log("❌ API ключ не установлен!");
        emit channelIdReceived("");
        return;
    }
    
    if (channelUrl.contains("channel/")) {
        QString channelId = channelUrl.split("channel/").last().split("/").first();
        emit channelIdReceived(channelId);
        return;
    }

    if (channelUrl.contains("@") || channelUrl.contains("youtube.com/")) {
        logger->log("Получаем ID канала через yt-dlp...");
        
        QProcess *process = new QProcess();
        QString appDir = QCoreApplication::applicationDirPath();
        QString ytDlpPath = appDir + "/yt-dlp.exe";
        
        // Используем --playlist-end 1 для ограничения до 1 видео
        process->setProgram(ytDlpPath);
        process->setArguments(QStringList() << "--print" << "%(channel_id)s" << "--no-playlist" << "--playlist-end" << "1" << channelUrl);
        
        logger->log("Команда: " + ytDlpPath + " --print %(channel_id)s --no-playlist --playlist-end 1 " + channelUrl);
        
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process](int exitCode, QProcess::ExitStatus) {
            QString output = process->readAllStandardOutput().trimmed();
            QString error = process->readAllStandardError().trimmed();
            
            logger->log("yt-dlp stdout: " + output);
            logger->log("yt-dlp stderr: " + error);
            
            if (exitCode == 0 && !output.isEmpty()) {
                // Берем первую строку как channel ID
                QStringList lines = output.split("\n", Qt::SkipEmptyParts);
                QString channelId = lines.first().trimmed();
                if (!channelId.isEmpty() && channelId.startsWith("UC")) {
                    logger->log("✅ Получен channel ID: " + channelId);
                    emit channelIdReceived(channelId);
                    process->deleteLater();
                    return;
                }
            }
            logger->log("❌ Не удалось получить channel ID из yt-dlp");
            emit channelIdReceived("");
            process->deleteLater();
        });
        
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            logger->log("❌ Ошибка запуска yt-dlp: " + process->errorString());
            emit channelIdReceived("");
            process->deleteLater();
        });
        
        process->start();
        return;
    }

    logger->log("❌ Не удалось извлечь ID канала из URL: " + channelUrl);
    emit channelIdReceived("");
}

void WebhookManager::onChannelIdReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        logger->log("❌ Ошибка запроса: " + reply->errorString());
        emit channelIdReceived("");
        return;
    }

    QByteArray data = reply->readAll();
    logger->log("📦 Ответ от API: " + data);

    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();
    QJsonArray items = json["items"].toArray();

    if (!items.isEmpty()) {
        // Search API возвращает ID в snippet.channelId
        QString channelId = items[0].toObject()["snippet"].toObject()["channelId"].toString();
        if (channelId.isEmpty()) {
            // Пробуем получить из ID напрямую (для search API)
            channelId = items[0].toObject()["id"].toObject()["channelId"].toString();
        }
        if (!channelId.isEmpty()) {
            logger->log("✅ Найден ID канала: " + channelId);
            emit channelIdReceived(channelId);
            return;
        }
    }
    logger->log("❌ Канал не найден");
    emit channelIdReceived("");
}

bool WebhookManager::startWebhook(const QString &channelUrl, const QString &webhookUrl, const QString &outputDir, const QString &format, bool monitorStreams, bool monitorVideos, quint16 &serverPort)
{
    pendingWebhookUrl = webhookUrl;
    pendingOutputDir = outputDir;
    pendingFormat = format;
    pendingMonitorStreams = monitorStreams;
    pendingMonitorVideos = monitorVideos;
    pendingServerPort = &serverPort;

    disconnect(this, &WebhookManager::channelIdReceived, nullptr, nullptr);
    connect(this, &WebhookManager::channelIdReceived, this, &WebhookManager::onChannelIdReceivedForSubscription);

    getChannelIdFromUrl(channelUrl);

    return true;
}

void WebhookManager::onChannelIdReceivedForSubscription(const QString &channelId)
{
    if (channelId.isEmpty()) {
        logger->log("❌ Не удалось получить ID канала.");
        return;
    }

    this->channelId = channelId;

    if (webhookServer) {
        logger->log("⚠ WebhookServer уже запущен!");
        return;
    }

    webhookServer = new WebhookServer(logger, apiKey, pendingOutputDir, pendingFormat, pendingMonitorStreams, pendingMonitorVideos, this);
    quint16 actualPort;
    if (!webhookServer->startServer(*pendingServerPort, actualPort)) {
        logger->log("❌ Не удалось запустить WebhookServer");
        delete webhookServer;
        webhookServer = nullptr;
        return;
    }
    *pendingServerPort = actualPort;

    QUrl url("https://pubsubhubbub.appspot.com/subscribe");
    QUrlQuery query;
    query.addQueryItem("hub.callback", pendingWebhookUrl);
    query.addQueryItem("hub.topic", QString("https://www.youtube.com/xml/feeds/videos.xml?channel_id=%1").arg(channelId));
    query.addQueryItem("hub.mode", "subscribe");
    query.addQueryItem("hub.verify", "sync");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply *reply = networkManager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onSubscriptionReplyFinished(reply);
    });
}

void WebhookManager::onSubscriptionReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorResponse = reply->readAll();
        logger->log("❌ Ошибка подписки на уведомления: " + reply->errorString() + " | Server replied: " + errorResponse);
        emit subscriptionFinished(false);
        return;
    }

    logger->log("✅ Подписка на уведомления для канала " + channelId + " успешно выполнена");
    emit subscriptionFinished(true);

    if (webhookServer) {
        webhookServer->checkOngoingContent(channelId);
    }
}

void WebhookManager::stopWebhook()
{
    if (webhookServer) {
        webhookServer->close();
        webhookServer->deleteLater();
        webhookServer = nullptr;
    }
    logger->log("✅ WebhookServer остановлен");
}
