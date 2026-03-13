#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "appconstants.h"

#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QPushButton>
#include "logger.h"
#include "downloader.h"
#include "streammonitor.h"
#include "webhookserver.h"
#include "webhookmanager.h"
#include "settingsdialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , logger(new Logger(this))
    , downloader(new Downloader(logger, this))
    , streamMonitor(new StreamMonitor(logger, this))
    , channelIdManager(new WebhookManager(logger, this))
    , outputFolder(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
    , actualPort(8000)
    , isMonitoringSectionVisible(false)
    , windowAnimation(nullptr)
    , monitoringSectionHeight(0)
    , originalWindowHeight(0)
    , isLogSectionVisible(false)
    , logAnimation(nullptr)
    , logSectionHeight(0)
    , windowSizeAnimation(nullptr)
    , language("ru")
    , monitorStreams(true)
    , monitorVideos(true)
{
    ui->setupUi(this);

    if (!ui->logOutput || !ui->monitoringSection) {
        return;
    }

    logAnimation = new QPropertyAnimation(ui->logOutput, "maximumHeight", this);
    windowAnimation = new QPropertyAnimation(ui->monitoringSection, "maximumHeight", this);
    windowSizeAnimation = new QPropertyAnimation(this, "size", this);

    setWindowTitle("YouStealD");
    applyStyles();

    QSettings settings(ORG_NAME, APP_NAME);
    apiKey = settings.value("apiKey", "").toString();
    monitorStreams = settings.value("monitorStreams", true).toBool();
    monitorVideos = settings.value("monitorVideos", true).toBool();
    monitorChannelUrl = settings.value("monitorChannelUrl", "").toString();
    language = settings.value("language", "ru").toString();

    channelIdManager->setApiKey(apiKey);

    initializeUI();

    applyLanguage();

    setupOutputFolder();

    connectSignals();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::applyStyles()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #2a2d3d;
            color: white;
            font-size: 10pt;
        }
        QLineEdit {
            background: #595f80;
            border-radius: 5px;
            padding: 5px;
        }
        QLineEdit#urlInput {
            background: #595f80;
            border-radius: 5px;
            padding: 4px 8px;
            min-height: 20px;
            max-height: 20px;
        }
QProgressBar#progressBar {
    border: none;
    border-radius: 6px;
    background-color: #595f80;
    text-align: center;
    color: white;
    min-height: 25px;
    max-height: 25px;
}
QProgressBar#progressBar::chunk {
    background-color: #0cad70;
    border-radius: 6px;
}
        QTextEdit {
            background-color: #222;
            border: none;
            padding: 5px;
        }
        QPushButton {
            background-color: #5E83AF;
            color: white;
            border-radius: 5px;
            padding: 5px;
            font-weight: normal;
        }
        QPushButton:hover {
            background-color: #4c698c;
        }
        QPushButton:pressed {
            background-color: #4c698c;
        }
        QComboBox {
            background-color: #222;
            color: white;
            border: none;
            padding: 5px;
            text-align: center;
        }
        QComboBox QAbstractItemView {
            background-color: #222;
            color: white;
            selection-background-color: #1E88E5;
        }
        QCheckBox {
            color: white;
            padding: 5px;
        }
        QRadioButton {
            color: white;
            padding: 5px;
        }
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
        }
        QRadioButton::indicator:unchecked {
            border: 2px solid #595f80;
            border-radius: 8px;
            background-color: transparent;
        }
        QRadioButton::indicator:checked {
            border: 2px solid #1E88E5;
            border-radius: 8px;
            background-color: #1E88E5;
        }
        QGroupBox {
            border: none;
            margin-top: 10px;
        }
        QCheckBox::indicator:unchecked {
            image: url(:/icons/icon.png);
        }
        QCheckBox::indicator:checked {
            image: url(:/icons/icon_pustoi.png);
        }
        QGroupBox:title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 3px;
            color: white;
        }
        QLabel#selectedFolderLabel {
            margin: 0px;
        }
        QPushButton#settingsBtn {
            border: none;
            background-color: transparent;
            image: url(:/icons/settings.png);
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
            border-radius: 16px;
            padding: 0px;
        }
        QPushButton#settingsBtn:hover {
            background-color: rgba(255, 255, 255, 0.08);
        }
        QPushButton#settingsBtn:pressed {
            background-color: rgba(255, 255, 255, 0.15);
        }
        QPushButton#startDownloadBtn {
            background-color: #5E83AF;
            color: #ffffff;
            font-weight: normal;
            qproperty-icon: url(:/icons/download.png);
            qproperty-iconSize: 18px 18px;
        }
        QPushButton#startDownloadBtn:hover {
            background-color: #4c698c;
        }
        QPushButton#startDownloadBtn:pressed {
            background-color: #4c698c;
        }
        QPushButton#stopDownloadBtn {
            background-color: #5E83AF;
            color: #ffffff;
            font-weight: normal;
            qproperty-icon: url(:/icons/power.png);
            qproperty-iconSize: 18px 18px;
        }
        QPushButton#stopDownloadBtn:hover {
            background-color: #4c698c;
        }
        QPushButton#stopDownloadBtn:pressed {
            background-color: #4c698c;
        }
        QPushButton#toggleMonitoringSectionBtn {
            background-color: #5E83AF;
            color: #ffffff;
            font-weight: normal;
        }
        QPushButton#toggleMonitoringSectionBtn:hover {
            background-color: #4c698c;
        }
        QPushButton#toggleMonitoringSectionBtn:pressed {
            background-color: #4c698c;
        }
    )");
}

void MainWindow::initializeUI()
{
    ui->stopDownloadBtn->setEnabled(false);
    ui->stopMonitoringBtn->setEnabled(false);

    ui->toggleMonitoringSectionBtn->setIcon(QIcon(":/icons/search_eye"));
    ui->toggleMonitoringSectionBtn->setIconSize(QSize(20, 20));

    ui->formatComboBox->setEditable(true);
    ui->formatComboBox->lineEdit()->setAlignment(Qt::AlignCenter);
    ui->formatComboBox->setEditable(false);
    ui->formatComboBox->setCurrentIndex(0);

    ui->logOutput->setMinimumHeight(0);
    ui->logOutput->setMaximumHeight(0);
    ui->logOutput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    ui->monitoringSection->setMinimumHeight(0);
    ui->monitoringSection->setMaximumHeight(0);
    ui->monitoringSection->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    ui->toggleMonitoringSectionBtn->setText(" Мониторинг");
    ui->startDownloadBtn->setText(" Начать загрузку");
    ui->stopDownloadBtn->setText(" Отменить");

    ui->logOutput->setMaximumHeight(16777215);
    logSectionHeight = ui->logOutput->sizeHint().height();
    if (logSectionHeight <= 0) logSectionHeight = 100;
    ui->logOutput->setMaximumHeight(0);

    ui->monitoringSection->setMaximumHeight(16777215);
    monitoringSectionHeight = ui->monitoringSection->sizeHint().height();
    if (monitoringSectionHeight <= 0) monitoringSectionHeight = 150;
    ui->monitoringSection->setMaximumHeight(0);

    ui->logOutput->setVisible(false);
    ui->monitoringSection->setVisible(false);
    adjustSize();
    originalWindowHeight = this->height();

    setMinimumHeight(originalWindowHeight);

    logAnimation->setDuration(500);
    logAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    windowAnimation->setDuration(500);
    windowAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    windowSizeAnimation->setDuration(500);
    windowSizeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
}

void MainWindow::setupOutputFolder()
{
    if (outputFolder.isEmpty()) {
        outputFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        ui->selectedFolderLabel->setText("❌ Не удалось определить папку загрузок! Используется: " + outputFolder);
    } else {
        ui->selectedFolderLabel->setText("Папка: " + outputFolder);
    }
}

void MainWindow::connectSignals()
{
    logger->setLogOutput(ui->logOutput);

    connect(downloader, &Downloader::progressUpdated, ui->progressBar, [this](int value) {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(value);
    });
    connect(downloader, &Downloader::statusUpdated, ui->selectedFolderLabel, &QLabel::setText);
    connect(downloader, &Downloader::downloadStarted, this, [this]() {
        ui->stopDownloadBtn->setEnabled(true);
        ui->startDownloadBtn->setEnabled(false);
        ui->progressBar->setValue(0);
        ui->optionalLabel->setText("Загрузка начата...");
    });
    connect(downloader, &Downloader::downloadFinished, this, [this]() {
        ui->stopDownloadBtn->setEnabled(false);
        ui->startDownloadBtn->setEnabled(true);
        ui->progressBar->setValue(0);
        ui->optionalLabel->setText("Загрузка завершена");
    });
    
    connect(downloader, &Downloader::authRequired, this, [this](const QString &videoUrl, const QString &errorMessage) {
        Q_UNUSED(videoUrl);
        Q_UNUSED(errorMessage);
        ui->optionalLabel->setText("🔐 Требуется авторизация!");
        
        QString title = "Требуется авторизация";
        QString message = "Выберите способ авторизации:";
        QString btnBrowser = "Использовать cookies браузера";
        QString btnSelectFile = "Выбрать cookies.txt";
        QString btnCancel = "Отмена";
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(title);
        msgBox.setText(message);
        msgBox.setIcon(QMessageBox::Warning);
        
        msgBox.addButton(btnBrowser, QMessageBox::ActionRole);
        msgBox.addButton(btnSelectFile, QMessageBox::ActionRole);
        msgBox.addButton(btnCancel, QMessageBox::RejectRole);
        
        msgBox.exec();
        
        QAbstractButton *clickedBtn = msgBox.clickedButton();
        
        if (clickedBtn) {
            QString text = clickedBtn->text();
            if (text == btnBrowser) {
                // Try to find Zen/Firefox profile using environment variable
                QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                // Convert to native separators and go up to Roaming (AppData/Local -> AppData/Roaming)
                appData = QDir::toNativeSeparators(appData);
                // Remove the last part (e.g., if appData is ".../AppData/Local", we want ".../AppData/Roaming")
                QStringList parts = appData.split("/");
                if (parts.size() >= 2) {
                    parts[parts.size()-1] = "Roaming";
                    appData = parts.join("/");
                }
                QString profilePath = appData + "/zen/Profiles";
                QDir dir(profilePath);
                QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                
                QString foundProfile;
                QString cookieFile;
                
                // Search for profile with cookies.sqlite
                for (const QString &entry : entries) {
                    QString testPath = QDir::toNativeSeparators(profilePath + "/" + entry + "/cookies.sqlite");
                    if (QFileInfo(testPath).exists()) {
                        foundProfile = entry;
                        cookieFile = testPath;
                        break;
                    }
                }
                
                if (!foundProfile.isEmpty()) {
                    // Use profile with cookies
                    downloader->authMethod = AuthMethod::BrowserFirefox;
                    logger->log("🔄 Использование Firefox cookies (Zen профиль: " + foundProfile + ")...");
                    downloader->retryLastDownload();
                } else {
                    logger->log("❌ Cookies не найдены в папке: " + profilePath);
                    ui->optionalLabel->setText("❌ Cookies браузера не найдены");
                }
            } else if (text == btnSelectFile) {
                QString cookiesPath = Downloader::selectCookiesFile();
                if (!cookiesPath.isEmpty()) {
                    downloader->authMethod = AuthMethod::CookiesFile;
                    downloader->cookiesFilePath = cookiesPath;
                    logger->log("🍪 Cookies файл: " + cookiesPath);
                    logger->log("🔄 Попытка авторизации с cookies.txt...");
                    downloader->retryLastDownload();
                } else {
                    logger->log("❌ Cookies файл не выбран");
                    ui->optionalLabel->setText("❌ Cookies файл не выбран");
                }
            }
        }
    });

    connect(streamMonitor, &StreamMonitor::statusUpdated, ui->optionalLabel, &QLabel::setText);
    connect(streamMonitor, &StreamMonitor::monitoringStarted, this, [this]() {
        ui->startMonitoringBtn->setEnabled(false);
        ui->stopMonitoringBtn->setEnabled(true);
        ui->stopDownloadBtn->setEnabled(true);
        ui->toggleMonitoringSectionBtn->setIcon(QIcon(":/icons/on.png"));
        ui->toggleMonitoringSectionBtn->setIconSize(QSize(20, 20));
        ui->optionalLabel->setText("Мониторинг запущен...");
    });
    connect(streamMonitor, &StreamMonitor::monitoringStopped, this, [this]() {
        ui->startMonitoringBtn->setEnabled(true);
        ui->stopMonitoringBtn->setEnabled(false);
        ui->stopDownloadBtn->setEnabled(false);
        ui->toggleMonitoringSectionBtn->setIcon(QIcon(":/icons/off.png"));
        ui->toggleMonitoringSectionBtn->setIconSize(QSize(20, 20));
        ui->progressBar->setValue(0);
        ui->optionalLabel->setText("Мониторинг остановлен");
    });
    
    connect(streamMonitor, &StreamMonitor::downloadProgress, this, [this](const QString &videoId, int progress) {
        if (progress < 0) {
            ui->progressBar->setValue(0);
            ui->optionalLabel->setText("Загрузка завершена");
        } else {
            ui->progressBar->setRange(0, 100);
            ui->progressBar->setValue(progress);
            ui->optionalLabel->setText("Загрузка: " + videoId + " (" + QString::number(progress) + "%)");
        }
    });
    
    connect(streamMonitor, &StreamMonitor::authRequired, this, [this](const QString &videoUrl, const QString &errorMessage) {
        Q_UNUSED(videoUrl);
        Q_UNUSED(errorMessage);
        ui->optionalLabel->setText("🔐 Требуется авторизация!");
        
        QString title = (language == "zh") ? "需要授权" : 
                       (language == "hi") ? "प्रमाणीकरण आवश्यक" : 
                       (language == "en") ? "Authorization Required" : 
                       "Требуется авторизация YouTube";
        
        QString message = (language == "zh") ? "此视频需要登录 YouTube 账户。" :
                          (language == "hi") ? "इस वीडियो के लिए YouTube खाते में लॉगिन करना आवश्यक है।" :
                          (language == "en") ? "This video requires YouTube account login." :
                          "Это видео требует входа в аккаунт YouTube.";
        
        QString btnBrowser = (language == "en") ? "Use browser cookies (Chrome)" : "Использовать cookies браузера";
        QString btnFirefox = (language == "en") ? "Firefox" : "Firefox";
        QString btnEdge = (language == "en") ? "Edge" : "Edge";
        QString btnOpenYoutube = (language == "en") ? "Open YouTube to login" : "Открыть YouTube для входа";
        QString btnSelectFile = (language == "en") ? "Select cookies.txt file" : "Выбрать cookies.txt файл";
        QString btnCancel = (language == "en") ? "Cancel" : "Отмена";
        
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(title);
        msgBox.setText(message);
        msgBox.setIcon(QMessageBox::Warning);
        
        msgBox.addButton(btnBrowser + " (Chrome)", QMessageBox::ActionRole);
        msgBox.addButton(btnFirefox, QMessageBox::ActionRole);
        msgBox.addButton(btnEdge, QMessageBox::ActionRole);
        msgBox.addButton(btnOpenYoutube, QMessageBox::ActionRole);
        msgBox.addButton(btnSelectFile, QMessageBox::ActionRole);
        msgBox.addButton(btnCancel, QMessageBox::RejectRole);
        
        msgBox.exec();
        
        QAbstractButton *clickedBtn = msgBox.clickedButton();
        
        if (clickedBtn) {
            QString btnText = clickedBtn->text();
            
            if (btnText.contains("Chrome")) {
                streamMonitor->authMethod = AuthMethod::BrowserChrome;
                streamMonitor->retryWithAuth();
            } else if (btnText.contains("Firefox")) {
                streamMonitor->authMethod = AuthMethod::BrowserFirefox;
                streamMonitor->retryWithAuth();
            } else if (btnText.contains("Edge")) {
                streamMonitor->authMethod = AuthMethod::BrowserEdge;
                streamMonitor->retryWithAuth();
            } else if (btnText.contains("YouTube") || btnText.contains("войти")) {
                StreamMonitor::openBrowser("https://www.youtube.com");
                ui->optionalLabel->setText("🌐 Откройте YouTube, войдите в аккаунт и нажмите 'Использовать cookies браузера'");
            } else if (btnText.contains("cookies.txt") || btnText.contains("файл")) {
                QString cookiesPath = StreamMonitor::selectCookiesFile();
                if (!cookiesPath.isEmpty()) {
                    streamMonitor->authMethod = AuthMethod::CookiesFile;
                    streamMonitor->cookiesFilePath = cookiesPath;
                    streamMonitor->retryWithAuth();
                } else {
                    ui->optionalLabel->setText("Загрузка отменена");
                }
            } else {
                ui->optionalLabel->setText("Загрузка отменена");
            }
        }
    });

    connect(logAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (windowSizeAnimation) windowSizeAnimation->start();
    });
    connect(windowAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (windowSizeAnimation) windowSizeAnimation->start();
    });
}

void MainWindow::on_selectFolderBtn_clicked()
{
    QString selectedFolder = QFileDialog::getExistingDirectory(this, "Выберите папку для сохранения", outputFolder);
    if (!selectedFolder.isEmpty()) {
        QFileInfo dirInfo(selectedFolder);
        if (!dirInfo.isWritable()) {
            ui->selectedFolderLabel->setText("❌ Папка недоступна для записи: " + selectedFolder);
            logger->log("❌ Папка недоступна для записи: " + selectedFolder);
            return;
        }
        outputFolder = selectedFolder;
        ui->selectedFolderLabel->setText("Папка: " + outputFolder);
        logger->log("📁 Выбрана папка: " + outputFolder);
    } else {
        ui->selectedFolderLabel->setText("Папка: " + outputFolder + " (выбор отменён)");
        logger->log("📁 Выбор папки отменён");
    }
}

void MainWindow::on_startDownloadBtn_clicked()
{
    if (!downloader || !logger) {
        QMessageBox::critical(nullptr, "Ошибка", "Компоненты не инициализированы!");
        return;
    }
    
    QString url = ui->urlInput->text().trimmed();
    if (url.isEmpty() || outputFolder.isEmpty()) {
        QString errTitle = (language == "zh") ? "错误" : (language == "hi") ? "त्रुटि" : (language == "en") ? "Error" : "Ошибка";
        QString errMsg = (language == "zh") ? "请输入 URL 并选择文件夹！" : (language == "hi") ? "URL दर्ज करें और फ़ोल्डर चुनें!" : (language == "en") ? "Enter URL and select a folder!" : "Введите URL и выберите папку!";
        ui->selectedFolderLabel->setText("❌ " + errMsg);
        logger->log("❌ " + errMsg);
        QMessageBox::warning(this, errTitle, errMsg);
        return;
    }

    bool isStream = ui->downloadStreamBtn->isChecked();
    bool isVideo = ui->downloadVideoBtn->isChecked();
    bool isChannel = ui->downloadChannelBtn->isChecked();

    if (!isStream && !isVideo && !isChannel) {
        QString errMsg = (language == "zh") ? "选择下载类型！" : (language == "hi") ? "डाउनलोड का प्रकार चुनें!" : (language == "en") ? "Select download type!" : "Выберите режим загрузки!";
        ui->selectedFolderLabel->setText("❌ " + errMsg);
        logger->log("❌ " + errMsg);
        QMessageBox::warning(this, (language == "zh") ? "错误" : (language == "hi") ? "त्रुटि" : (language == "en") ? "Error" : "Ошибка", errMsg);
        return;
    }

    logger->log("⏳ Начинаем загрузку: " + url);
    downloader->startDownload(url, outputFolder, ui->formatComboBox->currentText(), isStream, isVideo, isChannel);
}

void MainWindow::on_stopDownloadBtn_clicked()
{
    QString confirmTitle = (language == "zh") ? "确认" : (language == "hi") ? "पुष्टि" : (language == "en") ? "Confirmation" : "Подтверждение";
    QString confirmMsg = (language == "zh") ? "停止下载？" : (language == "hi") ? "डाउनलोड रद्द करें?" : (language == "en") ? "Stop download?" : "Остановить загрузку?";
    if (QMessageBox::question(this, confirmTitle, confirmMsg) == QMessageBox::Yes) {
        downloader->stopDownload();
        
        if (streamMonitor) {
            streamMonitor->stopDownload();
        }
        
        ui->optionalLabel->setText((language == "zh") ? "下载已停止" : (language == "hi") ? "डाउनलोड रद्द" : (language == "en") ? "Download stopped" : "Загрузка остановлена");
        ui->progressBar->setValue(0);
    }
}

void MainWindow::on_toggleLogBtn_clicked()
{
    int targetWindowHeight = originalWindowHeight;

    if (isLogSectionVisible) {
        logAnimation->setStartValue(ui->logOutput->maximumHeight() > 0 ? ui->logOutput->maximumHeight() : logSectionHeight);
        logAnimation->setEndValue(0);
        logAnimation->start();
        
        windowSizeAnimation->setStartValue(this->size());
        windowSizeAnimation->setEndValue(QSize(this->width(), originalWindowHeight));
        windowSizeAnimation->start();
        
        connect(logAnimation, &QPropertyAnimation::finished, this, [this]() {
            ui->logOutput->setVisible(false);
            disconnect(logAnimation, &QPropertyAnimation::finished, this, nullptr);
        }, Qt::SingleShotConnection);
    } else {
        ui->logOutput->setVisible(true);
        ui->logOutput->setMaximumHeight(0);
        
        logAnimation->setStartValue(0);
        logAnimation->setEndValue(logSectionHeight);
        logAnimation->start();
        
        targetWindowHeight += logSectionHeight;
        
        windowSizeAnimation->setStartValue(this->size());
        windowSizeAnimation->setEndValue(QSize(this->width(), targetWindowHeight));
        windowSizeAnimation->start();
    }

    QString showLog = (language == "zh") ? "显示日志" : (language == "hi") ? "लॉग दिखाएं" : (language == "en") ? "Show log" : "Показать лог";
    QString hideLog = (language == "zh") ? "隐藏日志" : (language == "hi") ? "लॉग छिपाएं" : (language == "en") ? "Hide log" : "Скрыть лог";
    ui->toggleLogBtn->setText(isLogSectionVisible ? showLog : hideLog);
    isLogSectionVisible = !isLogSectionVisible;
}

void MainWindow::on_toggleMonitoringSectionBtn_clicked()
{
    MonitoringSettings dialog(this);
    dialog.setApiKey(apiKey);
    dialog.setMonitorStreams(monitorStreams);
    dialog.setMonitorVideos(monitorVideos);
    dialog.setChannelUrl(monitorChannelUrl);
    dialog.setLanguage(language);

    if (dialog.exec() == QDialog::Accepted) {
        apiKey = dialog.getApiKey();
        monitorStreams = dialog.getMonitorStreams();
        monitorVideos = dialog.getMonitorVideos();
        monitorChannelUrl = dialog.getChannelUrl();
        
        channelIdManager->setApiKey(apiKey);
        
        logger->log("ℹ️ Настройки мониторинга сохранены");
        if (!apiKey.isEmpty()) {
            logger->log("✅ API ключ установлен");
        }
        if (!monitorChannelUrl.isEmpty()) {
            logger->log("📺 Канал для мониторинга: " + monitorChannelUrl);
            ui->urlInput->setText(monitorChannelUrl);
            
            if (monitorStreams || monitorVideos) {
                on_startMonitoringBtn_clicked();
            }
        }
    }
}

void MainWindow::on_startMonitoringBtn_clicked()
{
    // Читаем настройки из QSettings при каждом запуске
    QSettings settings(ORG_NAME, APP_NAME);
    monitorStreams = settings.value("monitorStreams", false).toBool();
    monitorVideos = settings.value("monitorVideos", false).toBool();
    monitorChannelUrl = settings.value("monitorChannelUrl", "").toString();
    
    QString channelUrl = ui->urlInput->text().trimmed();
    if (channelUrl.isEmpty()) {
        channelUrl = monitorChannelUrl;
    }
    
    QString errTitle = (language == "zh") ? QString::fromUtf8("错误") : (language == "en") ? "Error" : QString::fromUtf8("Ошибка");
    QString infoTitle = (language == "zh") ? QString::fromUtf8("监控设置") : (language == "en") ? "Monitoring Setup" : QString::fromUtf8("Настройка мониторинга");
    
    if (channelUrl.isEmpty() || outputFolder.isEmpty()) {
        QString errMsg = (language == "zh") ? QString::fromUtf8("输入频道 URL 并选择保存文件夹！") : (language == "hi") ? "चैनल URL दर्ज करें और फ़ोल्डर चुनें!" : (language == "en") ? "Enter channel URL and select save folder!" : QString::fromUtf8("Введите URL канала и выберите папку для сохранения!");
        QString labelMsg = (language == "zh") ? QString::fromUtf8("输入 URL 并选择文件夹！") : (language == "hi") ? "URL दर्ज करें और फ़ोल्डर चुनें!" : (language == "en") ? "Enter URL and select folder!" : QString::fromUtf8("Введите URL и выберите папку!");
        logger->log("❌ " + errMsg);
        ui->optionalLabel->setText("❌ " + labelMsg);
        QMessageBox::warning(this, errTitle, errMsg);
        return;
    }

    if (apiKey.isEmpty()) {
        QString infoMsg = (language == "zh") ? QString::fromUtf8("启动监控前，您需要设置 API 密钥和监控参数。\n\n点击\"用于监控\"打开设置。") : (language == "hi") ? "मॉनिटरिंग शुरू करने से पहले, आपको API कुंजी और निगरानी पैरामीटर सेट करने होंगे।\n\n\"ट्रैकिंग के लिए\" पर क्लिक करके सेटिंग्स खोलें।" : (language == "en") ? "Before starting monitoring, you need to set up API key and monitoring parameters.\n\nClick \"For monitoring\" to open settings." : QString::fromUtf8("Перед запуском мониторинга необходимо настроить API ключ и параметры отслеживания.\n\nНажмите \"Для отслеживания\" чтобы открыть настройки.");
        QString labelMsg = (language == "zh") ? QString::fromUtf8("请配置监控！") : (language == "hi") ? "निगरानी कॉन्फ़िगर करें!" : (language == "en") ? "Configure monitoring!" : QString::fromUtf8("Настройте мониторинг!");
        logger->log("⚠ API ключ не установлен. Откройте настройки мониторинга.");
        ui->optionalLabel->setText("⚠ " + labelMsg);
        QMessageBox::information(this, infoTitle, infoMsg);
        return;
    }

    if (!monitorStreams && !monitorVideos) {
        QString errMsg = (language == "zh") ? QString::fromUtf8("选择要监控的内容：直播、视频或两者！") : (language == "hi") ? "क्या मॉनिटर करना है चुनें: स्ट्रीम, वीडियो या दोनों!" : (language == "en") ? "Select what to monitor: streams, videos or both!" : QString::fromUtf8("Выберите, что отслеживать: стримы, видео или оба!");
        QString labelMsg = (language == "zh") ? QString::fromUtf8("选择内容类型！") : (language == "hi") ? "कंटेंट प्रकार चुनें!" : (language == "en") ? "Select content type!" : QString::fromUtf8("Выберите тип контента!");
        logger->log("❌ " + errMsg);
        ui->optionalLabel->setText("❌ " + labelMsg);
        QMessageBox::warning(this, errTitle, errMsg);
        return;
    }

    if (!ui->startMonitoringBtn->isEnabled()) {
        logger->log("⚠ Мониторинг уже запущен!");
        return;
    }

    streamMonitor->startMonitoring(channelUrl, outputFolder, ui->formatComboBox->currentText(), monitorStreams, monitorVideos, actualPort);
}

void MainWindow::on_stopMonitoringBtn_clicked()
{
    streamMonitor->stopMonitoring();
    
    ui->startMonitoringBtn->setEnabled(true);
    ui->stopMonitoringBtn->setEnabled(false);
    ui->optionalLabel->setText("Мониторинг остановлен");
}

void MainWindow::onContentDetected(const QString &videoId)
{
    logger->log("✅ Обнаружен контент: " + videoId);
    downloader->startDownload(
        QString("https://www.youtube.com/watch?v=%1").arg(videoId),
        outputFolder,
        ui->formatComboBox->currentText(),
        true,
        false,
        false
        );
}

void MainWindow::onRecordingStarted(const QString &videoId)
{
    ui->progressBar->setValue(0);
    ui->optionalLabel->setText("Запись стрима: " + videoId);
    logger->log("⏳ Начата запись стрима: " + videoId);
}

void MainWindow::onRecordingProgress(const QString &videoId, int progress)
{
    ui->progressBar->setValue(progress);
    ui->optionalLabel->setText(QString("Запись стрима: %1 (%2%)").arg(videoId).arg(progress));
}

void MainWindow::onRecordingFinished(const QString &videoId, bool success)
{
    if (success) {
        ui->optionalLabel->setText("Запись стрима " + videoId + " успешно завершена");
        logger->log("✅ Запись стрима " + videoId + " успешно завершена");
    } else {
        ui->optionalLabel->setText("Ошибка при записи стрима " + videoId);
        logger->log("❌ Ошибка при записи стрима " + videoId);
    }
    ui->progressBar->setValue(0);
}

void MainWindow::applyLanguage()
{
    if (language == "en") {
        ui->label->setText("Enter URL:");
        ui->groupBox->setTitle("Select download type:");
        ui->downloadStreamBtn->setText("Record stream");
        ui->downloadChannelBtn->setText("Download all videos from channel");
        ui->downloadVideoBtn->setText("Download video");
        ui->label_3->setText("Select format:");
        ui->label_2->setText("Choose save path:");
        ui->selectFolderBtn->setText("Select folder");
        ui->startDownloadBtn->setText(" Start download");
        ui->stopDownloadBtn->setText(" Cancel");
        ui->toggleLogBtn->setText("Show log");
        ui->toggleMonitoringSectionBtn->setText(" Monitoring");
        ui->settingsBtn->setText("");
    } else if (language == "zh") {
        ui->label->setText("请输入 URL：");
        ui->groupBox->setTitle("选择下载类型：");
        ui->downloadStreamBtn->setText("录制直播");
        ui->downloadChannelBtn->setText("下载频道所有视频");
        ui->downloadVideoBtn->setText("下载单个视频");
        ui->label_3->setText("选择格式：");
        ui->label_2->setText("选择保存路径：");
        ui->selectFolderBtn->setText("选择文件夹");
        ui->startDownloadBtn->setText(" 开始下载");
        ui->stopDownloadBtn->setText(" 取消");
        ui->toggleLogBtn->setText("显示日志");
        ui->toggleMonitoringSectionBtn->setText(" 监测");
        ui->settingsBtn->setText("");
    } else if (language == "hi") {
        ui->label->setText("URL दर्ज करें");
        ui->groupBox->setTitle("डाउनलोड का प्रकार चुनें:");
        ui->downloadStreamBtn->setText("स्ट्रीम रिकॉर्ड करें");
        ui->downloadChannelBtn->setText("चैनल के सभी वीडियो डाउनलोड करें");
        ui->downloadVideoBtn->setText("वीडियो डाउनलोड करें");
        ui->label_3->setText("फ़ॉर्मेट चुनें:");
        ui->label_2->setText("सहेजने का पथ चुनें:");
        ui->selectFolderBtn->setText("फोल्डर चुनें");
        ui->startDownloadBtn->setText(" डाउनलोड शुरू करें");
        ui->stopDownloadBtn->setText(" रद्द करना");
        ui->toggleLogBtn->setText("लॉग दिखाएं");
        ui->toggleMonitoringSectionBtn->setText(" निगरानी");
        ui->settingsBtn->setText("");
    } else {
        ui->label->setText("Введите url:");
        ui->groupBox->setTitle("Выберите тип загрузки:");
        ui->downloadStreamBtn->setText("Запись стрима");
        ui->downloadChannelBtn->setText("Скачать все видео с канала");
        ui->downloadVideoBtn->setText("Скачать видео");
        ui->label_3->setText("Выберите формат:");
        ui->label_2->setText("Выбрать путь для сохранения:");
        ui->selectFolderBtn->setText("Выбрать папку");
        ui->startDownloadBtn->setText(" Начать загрузку");
        ui->stopDownloadBtn->setText(" Отменить");
        ui->toggleLogBtn->setText("Показать лог");
        ui->toggleMonitoringSectionBtn->setText(" Мониторинг");
        ui->settingsBtn->setText("");
    }
}

void MainWindow::on_settingsBtn_clicked()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        language = dialog.getLanguage();
        applyLanguage();
    }
}
