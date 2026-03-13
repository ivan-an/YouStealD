#include "webhookserver.h"
#include <QDebug>
#include <QUrlQuery>
#include <QProcess>
#include <QXmlStreamReader>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QThread>
#include <QHostAddress>
#include <QTimer>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

WebhookServer::WebhookServer(Logger *logger, const QString &apiKey, const QString &outputDir, const QString &format, bool monitorStreams, bool monitorVideos, QObject *parent)
    : QObject(parent)
    , logger(logger)
    , server(new QTcpServer(this))
    , networkManager(new QNetworkAccessManager(this))
    , apiKey(apiKey)
    , outputDir(outputDir)
    , format(format)
    , monitorStreams(monitorStreams)
    , monitorVideos(monitorVideos)
    , isStreamResult(false)
    , checkStatusTimer(new QTimer(this))
    , currentVideoId("")
    , recordProcess(new QProcess(this))
    , currentRecordingVideoId("")
    , requestTimer(new QTimer(this))
    , currentReply(nullptr)
    , currentRequestVideoId("")
{
    connect(server, &QTcpServer::newConnection, this, &WebhookServer::handleNewConnection);
    connect(networkManager, &QNetworkAccessManager::finished, this, &WebhookServer::handleApiReply);
    connect(this, &WebhookServer::contentDetected, this, &WebhookServer::onContentDetected);

    connect(checkStatusTimer, &QTimer::timeout, this, [this]() { checkVideoStatus(); });
    checkStatusTimer->start(60000);

    connect(recordProcess, &QProcess::readyReadStandardOutput, this, &WebhookServer::onProcessOutput);
    connect(recordProcess, &QProcess::readyReadStandardError, this, &WebhookServer::onProcessOutput);
    connect(recordProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &WebhookServer::onProcessFinished);

    // Настройка таймера для таймаута
    requestTimer->setSingleShot(true);
    connect(requestTimer, &QTimer::timeout, this, &WebhookServer::onRequestTimeout);
}

WebhookServer::~WebhookServer()
{
    if (server->isListening()) {
        server->close();
        logger->log("✅ WebhookServer закрыт, порт освобождён");
    }
    if (recordProcess) {
        recordProcess->disconnect();
        if (recordProcess->state() == QProcess::Running) {
            recordProcess->terminate();
            if (!recordProcess->waitForFinished(2000)) {
                recordProcess->kill();
                recordProcess->waitForFinished(1000);
            }
        }
        recordProcess->deleteLater();
        recordProcess = nullptr;
    }
}

void WebhookServer::stopServer()
{
    if (server->isListening()) {
        server->close();
        logger->log("✅ WebhookServer остановлен, порт освобождён");
    }
    if (checkStatusTimer) {
        checkStatusTimer->stop();
    }
    if (recordProcess && recordProcess->state() == QProcess::Running) {
        recordProcess->disconnect();
        recordProcess->terminate();
        if (!recordProcess->waitForFinished(2000)) {
            recordProcess->kill();
            recordProcess->waitForFinished(1000);
        }
        recordProcess->deleteLater();
        recordProcess = nullptr;
        logger->log("⏹ Запись остановлена");
    }
    videosToCheck.clear();
}

void WebhookServer::onContentDetected(const QString &videoId)
{
    logger->log("⏳ Начинаем обработку контента: " + videoId);
    recordContent(videoId);
}

bool WebhookServer::startServer(quint16 preferredPort, quint16 &actualPort)
{
    if (server->listen(QHostAddress::Any, preferredPort)) {
        actualPort = preferredPort;
        logger->log("✅ Webhook server started on port " + QString::number(actualPort));
        return true;
    }

    logger->log("⚠ Порт " + QString::number(preferredPort) + " занят, ищу свободный порт...");
    for (quint16 port = 8000; port <= 8100; ++port) {
        if (port == preferredPort) continue;
        if (server->listen(QHostAddress::Any, port)) {
            actualPort = port;
            logger->log("✅ Webhook server started on port " + QString::number(actualPort));
            return true;
        }
    }

    actualPort = 0;
    logger->log("❌ Не удалось запустить WebhookServer: " + server->errorString());
    return false;
}

void WebhookServer::handleNewConnection()
{
    QTcpSocket *client = server->nextPendingConnection();
    if (!client) {
        logger->log("❌ Ошибка: не удалось получить новое соединение");
        return;
    }
    connect(client, &QTcpSocket::readyRead, this, &WebhookServer::handleClientData);
    connect(client, &QTcpSocket::disconnected, client, &QTcpSocket::deleteLater);
}

void WebhookServer::handleClientData()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        logger->log("❌ Ошибка: не удалось получить клиентский сокет");
        return;
    }

    QByteArray data = client->readAll();
    QString request(data);

    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) {
        client->write("HTTP/1.1 400 Bad Request\r\n\r\n");
        client->close();
        return;
    }

    QString firstLine = lines[0];
    QStringList parts = firstLine.split(" ");
    if (parts.size() < 3) {
        client->write("HTTP/1.1 400 Bad Request\r\n\r\n");
        client->close();
        return;
    }

    QString method = parts[0];
    QString path = parts[1];

    if (method == "GET" && path.startsWith("/webhook")) {
        QUrl url(path);
        QUrlQuery query(url.query());
        QString challenge = query.queryItemValue("hub.challenge");
        if (!challenge.isEmpty()) {
            logger->log("Received challenge: " + challenge);
            client->write(QString("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n%1").arg(challenge).toUtf8());
        } else {
            client->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK");
        }
        client->close();
        return;
    }

    if (method == "POST" && path == "/webhook") {
        QByteArray body;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].isEmpty()) {
                body = data.mid(data.indexOf("\r\n\r\n") + 4);
                break;
            }
        }

        QXmlStreamReader xml(body);
        QString videoId;
        while (!xml.atEnd() && !xml.hasError()) {
            QXmlStreamReader::TokenType token = xml.readNext();
            if (token == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("id")) {
                QString idText = xml.readElementText();
                videoId = idText.split(":").last();
                break;
            }
        }

        if (!videoId.isEmpty()) {
            logger->log("Detected content with video ID: " + videoId);
            bool isStreamContent = isStream(videoId);
            if ((isStreamContent && monitorStreams) || (!isStreamContent && monitorVideos)) {
                emit contentDetected(videoId);
            }
        }

        client->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nOK");
        client->close();
        return;
    }

    client->write("HTTP/1.1 404 Not Found\r\n\r\n");
    client->close();
}

bool WebhookServer::isStream(const QString &videoId)
{
    QUrl url(QString("https://www.googleapis.com/youtube/v3/videos?part=snippet,liveStreamingDetails&id=%1&key=%2").arg(videoId, apiKey));
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        logger->log("❌ Error checking stream status: " + reply->errorString());
        reply->deleteLater();
        return false;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();
    QJsonArray items = json["items"].toArray();
    bool result = false;
    if (!items.isEmpty()) {
        QJsonObject video = items[0].toObject();
        result = video.contains("liveStreamingDetails");
    }

    reply->deleteLater();
    return result;
}

void WebhookServer::recordContent(const QString &videoId)
{
    logger->log("⏳ Проверяем статус видео: " + videoId);
    QUrl url(QString("https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=%1&key=%2").arg(videoId, apiKey));
    QNetworkRequest request(url);

    // Сохраняем контекст запроса
    currentRequestVideoId = videoId;
    currentReply = networkManager->get(request);

    // Запускаем таймер на 10 секунд
    requestTimer->start(10000);
}

void WebhookServer::handleApiReply(QNetworkReply *reply)
{
    // Останавливаем таймер
    requestTimer->stop();

    if (reply != currentReply) {
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        logger->log("❌ Error checking video status for " + currentRequestVideoId + ": " + reply->errorString());
        reply->deleteLater();
        currentReply = nullptr;
        currentRequestVideoId.clear();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();
    QJsonArray items = json["items"].toArray();

    if (items.isEmpty()) {
        logger->log("❌ Video not found: " + currentRequestVideoId);
        reply->deleteLater();
        currentReply = nullptr;
        currentRequestVideoId.clear();
        return;
    }

    QJsonObject video = items[0].toObject();
    QJsonObject liveDetails = video["liveStreamingDetails"].toObject();
    QString liveStatus = liveDetails["actualStartTime"].toString().isEmpty() ? "upcoming" : "live";

    if (liveStatus == "upcoming") {
        logger->log("⏳ Премьера " + currentRequestVideoId + " ещё не началась, добавляем в список для проверки...");
        videosToCheck.insert(currentRequestVideoId);
        checkStatusTimer->start(60000);
    } else if (liveStatus == "live") {
        logger->log("✅ Стрим " + currentRequestVideoId + " уже идёт, начинаем запись");
        recordStream(currentRequestVideoId);
    }

    reply->deleteLater();
    currentReply = nullptr;
    currentRequestVideoId.clear();
}

void WebhookServer::onRequestTimeout()
{
    if (currentReply) {
        logger->log("❌ Таймаут запроса для видео: " + currentRequestVideoId);
        currentReply->abort();
        currentReply->deleteLater();
        currentReply = nullptr;
        currentRequestVideoId.clear();
    }
}

void WebhookServer::checkOngoingContent(const QString &channelId)
{
    logger->log("⏳ Проверяем текущие стримы для канала: " + channelId);
    QUrl url(QString("https://www.googleapis.com/youtube/v3/search?part=snippet&channelId=%1&eventType=live&type=video&key=%2").arg(channelId, apiKey));
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        logger->log("❌ Error checking ongoing streams: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();
    QJsonArray items = json["items"].toArray();

    for (const QJsonValue &item : items) {
        QJsonObject video = item.toObject();
        QJsonObject snippet = video["snippet"].toObject();
        QString videoId = video["id"].toObject()["videoId"].toString();
        logger->log("Found ongoing stream: " + videoId);
        if (monitorStreams) {
            emit contentDetected(videoId);
        }
    }

    reply->deleteLater();

    logger->log("⏳ Проверяем предстоящие премьеры для канала: " + channelId);
    QUrl upcomingUrl(QString("https://www.googleapis.com/youtube/v3/search?part=snippet&channelId=%1&eventType=upcoming&type=video&key=%2").arg(channelId, apiKey));
    QNetworkRequest upcomingRequest(upcomingUrl);
    QNetworkReply *upcomingReply = networkManager->get(upcomingRequest);

    QEventLoop upcomingLoop;
    connect(upcomingReply, &QNetworkReply::finished, &upcomingLoop, &QEventLoop::quit);
    upcomingLoop.exec();

    if (upcomingReply->error() != QNetworkReply::NoError) {
        logger->log("❌ Error checking upcoming premieres: " + upcomingReply->errorString());
        upcomingReply->deleteLater();
        return;
    }

    data = upcomingReply->readAll();
    doc = QJsonDocument::fromJson(data);
    json = doc.object();
    items = json["items"].toArray();

    for (const QJsonValue &item : items) {
        QJsonObject video = item.toObject();
        QJsonObject snippet = video["snippet"].toObject();
        QString videoId = video["id"].toObject()["videoId"].toString();
        logger->log("Found upcoming premiere: " + videoId);
        if (monitorStreams) {
            emit contentDetected(videoId);
        }
    }

    upcomingReply->deleteLater();
}

void WebhookServer::checkVideoStatus(const QString &videoId)
{
    logger->log("⏳ Проверяем статус стрима: " + videoId);
    currentVideoId = videoId;

    QUrl url(QString("https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=%1&key=%2").arg(videoId, apiKey));
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, &WebhookServer::onVideoStatusChecked);
}

void WebhookServer::checkVideoStatus()
{
    if (videosToCheck.isEmpty()) {
        logger->log("ℹ️ Нет видео для проверки статуса");
        return;
    }

    for (const QString &videoId : videosToCheck) {
        checkVideoStatus(videoId);
    }
}

void WebhookServer::onVideoStatusChecked()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        logger->log("❌ Ошибка: не удалось получить ответ при проверке статуса видео");
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();
    QJsonArray items = json["items"].toArray();

    if (!items.isEmpty()) {
        QJsonObject video = items[0].toObject();
        QJsonObject liveDetails = video["liveStreamingDetails"].toObject();
        QString liveStatus = liveDetails["actualStartTime"].toString().isEmpty() ? "upcoming" : "live";

        if (liveStatus == "live") {
            logger->log("✅ Стрим начался, начинаем запись: " + currentVideoId);
            checkStatusTimer->stop();
            recordStream(currentVideoId);
            videosToCheck.remove(currentVideoId);
        } else {
            logger->log("⏳ Премьера или стрим ещё не начался: " + currentVideoId);
        }
    } else {
        logger->log("❌ Видео не найдено при проверке статуса: " + currentVideoId);
    }

    reply->deleteLater();
}

void WebhookServer::recordStream(const QString &videoId)
{
    logger->log("⏳ Подготовка к записи стрима: " + videoId);

    // Проверяем, не выполняется ли уже другая запись
    if (recordProcess->state() == QProcess::Running) {
        logger->log("⚠ Уже выполняется запись, пропускаем: " + videoId);
        return;
    }

    // Проверяем, существует ли директория и доступна ли она для записи
    QDir dir(outputDir);
    if (!dir.exists()) {
        logger->log("❌ Директория не существует: " + outputDir + ". Пытаюсь создать...");
        if (!dir.mkpath(outputDir)) {
            logger->log("❌ Не удалось создать директорию: " + outputDir);
            emit recordingFinished(videoId, false);
            return;
        }
    }

    QFileInfo dirInfo(outputDir);
    if (!dirInfo.isWritable()) {
        logger->log("❌ Директория недоступна для записи: " + outputDir);
        emit recordingFinished(videoId, false);
        return;
    }

    currentRecordingVideoId = videoId;
    emit recordingStarted(videoId);

    QString streamUrl = QString("https://www.youtube.com/watch?v=%1").arg(videoId);
    QString outputFile = QString("%1/%2_stream_%(timestamp)s.%(ext)s").arg(outputDir, videoId);
    logger->log("ℹ️ Выходной файл: " + outputFile);

    QStringList arguments;
    arguments << "--force-overwrites"
              << "--ffmpeg-location" << "ffmpeg"
              << "-o" << outputFile
              << "--progress"
              << streamUrl
              << "--live-from-start"
              << "--no-part"
              << "--buffer-size" << "256K";

    logger->log("ℹ️ Используемая команда: yt-dlp " + arguments.join(" "));

    QString ytDlpPath = QCoreApplication::applicationDirPath() + "/yt-dlp.exe";

    logger->log("ℹ️ Путь к yt-dlp: " + ytDlpPath);

    // Проверяем, существует ли файл yt-dlp.exe
    QFileInfo ytDlpFile(ytDlpPath);
    if (!ytDlpFile.exists()) {
        logger->log("❌ Файл yt-dlp.exe не найден по пути: " + ytDlpPath);
        emit recordingFinished(videoId, false);
        currentRecordingVideoId.clear();
        return;
    }

    recordProcess->start(ytDlpPath, arguments);
    if (!recordProcess->waitForStarted(5000)) {
        logger->log("❌ Не удалось запустить запись: " + recordProcess->errorString());
        emit recordingFinished(videoId, false);
        currentRecordingVideoId.clear();
        return;
    }

    logger->log("✅ Запись стрима начата: " + videoId);
}

void WebhookServer::onProcessOutput()
{
    QString output = QString::fromUtf8(recordProcess->readAllStandardOutput());
    QString errorOutput = QString::fromUtf8(recordProcess->readAllStandardError());

    if (!errorOutput.isEmpty()) {
        logger->log("⚠ Вывод ошибок yt-dlp: " + errorOutput.trimmed());
    }

    if (!output.isEmpty()) {
        logger->log("📜 Вывод yt-dlp: " + output.trimmed());
        QRegularExpression progressRx("\\[download\\]\\s+(\\d+\\.\\d+)%");
        QRegularExpressionMatch match = progressRx.match(output);
        if (match.hasMatch()) {
            QString progressStr = match.captured(1);
            bool ok;
            float progress = progressStr.toFloat(&ok);
            if (ok) {
                int progressPercent = static_cast<int>(progress);
                logger->log("ℹ️ Прогресс записи: " + QString::number(progressPercent) + "%");
                emit recordingProgress(currentRecordingVideoId, progressPercent);
            } else {
                logger->log("⚠ Не удалось преобразовать прогресс: " + progressStr);
            }
        }
    }
}

void WebhookServer::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString videoId = currentRecordingVideoId;
    currentRecordingVideoId.clear();

    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        logger->log("❌ Ошибка при записи стрима " + videoId + ": " + recordProcess->errorString());
        logger->log("ℹ️ Код завершения: " + QString::number(exitCode));
        emit recordingFinished(videoId, false);
    } else {
        logger->log("✅ Стрим " + videoId + " успешно записан");
        emit recordingFinished(videoId, true);
    }
}
