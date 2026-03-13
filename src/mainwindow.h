#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "logger.h"
#include "streammonitor.h"
#include "downloader.h"
#include "webhookmanager.h"
#include "monitoringsettings.h"
#include "settingsdialog.h"
#include <QPropertyAnimation>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_selectFolderBtn_clicked();
    void on_startDownloadBtn_clicked();
    void on_stopDownloadBtn_clicked();
    void on_toggleLogBtn_clicked();
    void on_startMonitoringBtn_clicked();
    void on_stopMonitoringBtn_clicked();
    void onContentDetected(const QString &videoId);
    void onRecordingStarted(const QString &videoId);
    void onRecordingProgress(const QString &videoId, int progress);
    void onRecordingFinished(const QString &videoId, bool success);

    void on_toggleMonitoringSectionBtn_clicked();
    void on_settingsBtn_clicked();

private:
    void applyStyles();
    void applyLanguage();
    void initializeUI();
    void setupOutputFolder();
    void connectSignals();

    Ui::MainWindow *ui = nullptr;
    Logger *logger = nullptr;
    Downloader *downloader = nullptr;
    StreamMonitor *streamMonitor = nullptr;
    WebhookManager *channelIdManager = nullptr;
    QString outputFolder;
    quint16 actualPort = 0;
    bool isMonitoringSectionVisible = false;
    QPropertyAnimation *windowAnimation = nullptr;
    int monitoringSectionHeight = 0;
    int originalWindowHeight = 0;
    bool isLogSectionVisible = false;
    QPropertyAnimation *logAnimation = nullptr;
    int logSectionHeight = 0;
    QPropertyAnimation *windowSizeAnimation = nullptr;
    QString apiKey;
    QString language = "ru";
    bool monitorStreams = true;
    bool monitorVideos = true;
    QString monitorChannelUrl;
};

#endif // MAINWINDOW_H
