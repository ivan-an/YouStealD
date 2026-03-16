#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "appconstants.h"

#include <QSettings>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_language("ru")
{
    ui->setupUi(this);
    setModal(true);

    QSettings settings(ORG_NAME, APP_NAME);
    m_language = settings.value("language", "ru").toString();

    ui->languageComboBox->addItem("Русский", "ru");
    ui->languageComboBox->addItem("English", "en");
    ui->languageComboBox->addItem("中文", "zh");
    ui->languageComboBox->addItem("हिंदी", "hi");

    int index = ui->languageComboBox->findData(m_language);
    if (index >= 0) {
        ui->languageComboBox->setCurrentIndex(index);
    }

    m_useAria2c = settings.value("useAria2c", false).toBool();
    ui->aria2cCheckBox->setChecked(m_useAria2c);

    m_speedUnlimited = settings.value("speedUnlimited", false).toBool();
    ui->speedUnlimitedCheckBox->setChecked(m_speedUnlimited);

    m_proxy = settings.value("proxy", "").toString();
    ui->proxyLineEdit->setText(m_proxy);

    ui->ytDlpVersionLabel->setText("...");
    QTimer::singleShot(100, this, &SettingsDialog::loadYtDlpVersion);

    applyLanguage();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadYtDlpVersion()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString ytDlpPath = appDir + "/yt-dlp.exe";
    QProcess process;
    process.start(ytDlpPath, QStringList() << "--version");
    if (process.waitForFinished(3000)) {
        QString version = process.readAllStandardOutput().trimmed();
        ui->ytDlpVersionLabel->setText(version.isEmpty() ? "Не найден" : version);
    } else {
        ui->ytDlpVersionLabel->setText("Не найден");
    }
}

void SettingsDialog::applyLanguage()
{
    if (m_language == "en") {
        setWindowTitle("Settings");
        ui->label->setText("Language:");
        ui->versionLabel->setText("yt-dlp version:");
        ui->aria2cCheckBox->setText("Use aria2c");
        ui->aria2cCheckBox->setToolTip("Use aria2c for faster downloads");
        ui->speedUnlimitedCheckBox->setText("Unlimited speed");
        ui->speedUnlimitedCheckBox->setToolTip("Remove speed limit (may cause block)");
        ui->proxyLabel->setText("Proxy:");
        ui->proxyLineEdit->setPlaceholderText("socks5://127.0.0.1:1080 or http://127.0.0.1:8080");
        ui->saveButton->setText("Save");
        ui->cancelButton->setText("Cancel");
        ui->updateYtDlpBtn->setToolTip("Update yt-dlp");
    } else if (m_language == "zh") {
        setWindowTitle("设置");
        ui->label->setText("语言：");
        ui->versionLabel->setText("yt-dlp 版本：");
        ui->aria2cCheckBox->setText("使用 aria2c");
        ui->aria2cCheckBox->setToolTip("使用 aria2c 加速下载");
        ui->speedUnlimitedCheckBox->setText("无限速度");
        ui->speedUnlimitedCheckBox->setToolTip("取消速度限制（可能导致封锁）");
        ui->proxyLabel->setText("代理：");
        ui->proxyLineEdit->setPlaceholderText("socks5://127.0.0.1:1080 或 http://127.0.0.1:8080");
        ui->saveButton->setText("保存");
        ui->cancelButton->setText("取消");
        ui->updateYtDlpBtn->setToolTip("更新 yt-dlp");
    } else if (m_language == "hi") {
        setWindowTitle("सेटिंग्स");
        ui->label->setText("भाषा:");
        ui->versionLabel->setText("yt-dlp संस्करण:");
        ui->aria2cCheckBox->setText("aria2c का उपयोग करें");
        ui->aria2cCheckBox->setToolTip("तेज डाउनलोड के लिए aria2c का उपयोग करें");
        ui->speedUnlimitedCheckBox->setText("असीमित गति");
        ui->speedUnlimitedCheckBox->setToolTip("गति सीमा हटाएं (अवरोध हो सकता है)");
        ui->proxyLabel->setText("प्रॉक्सी:");
        ui->proxyLineEdit->setPlaceholderText("socks5://127.0.0.1:1080 या http://127.0.0.1:8080");
        ui->saveButton->setText("सहेजें");
        ui->cancelButton->setText("रद्द करें");
        ui->updateYtDlpBtn->setToolTip("yt-dlp अपडेट करें");
    } else {
        setWindowTitle("Настройки");
        ui->label->setText("Язык:");
        ui->versionLabel->setText("Версия yt-dlp:");
        ui->aria2cCheckBox->setText("Использовать aria2c");
        ui->aria2cCheckBox->setToolTip("Использовать aria2c для ускорения загрузки");
        ui->speedUnlimitedCheckBox->setText("Без ограничения скорости");
        ui->speedUnlimitedCheckBox->setToolTip("Снять ограничение скорости (может привести к блокировке)");
        ui->proxyLabel->setText("Прокси:");
        ui->proxyLineEdit->setPlaceholderText("socks5://127.0.0.1:1080 или http://127.0.0.1:8080");
        ui->saveButton->setText("Сохранить");
        ui->cancelButton->setText("Отмена");
        ui->updateYtDlpBtn->setToolTip("Обновить yt-dlp");
    }
}

void SettingsDialog::on_saveButton_clicked()
{
    m_language = ui->languageComboBox->currentData().toString();
    m_useAria2c = ui->aria2cCheckBox->isChecked();
    m_speedUnlimited = ui->speedUnlimitedCheckBox->isChecked();
    m_proxy = ui->proxyLineEdit->text();
    QSettings settings(ORG_NAME, APP_NAME);
    settings.setValue("language", m_language);
    settings.setValue("useAria2c", m_useAria2c);
    settings.setValue("speedUnlimited", m_speedUnlimited);
    settings.setValue("proxy", m_proxy);
    accept();
}

void SettingsDialog::on_cancelButton_clicked()
{
    reject();
}

void SettingsDialog::on_updateYtDlpBtn_clicked()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString ytDlpPath = appDir + "/yt-dlp.exe";
    
    ui->ytDlpVersionLabel->setText("Обновление...");
    ui->updateYtDlpBtn->setEnabled(false);
    
    QProcess *process = new QProcess(this);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process, ytDlpPath](int, QProcess::ExitStatus) {
        QString output = process->readAllStandardOutput();
        
        if (output.contains("yt-dlp is up to date") || output.contains("уже最新版本") || output.contains("is up to date")) {
            ui->ytDlpVersionLabel->setText("Уже последняя версия");
        } else if (!output.isEmpty()) {
            ui->ytDlpVersionLabel->setText(output.trimmed());
        } else {
            QProcess *verProcess = new QProcess(this);
            verProcess->start(ytDlpPath, QStringList() << "--version");
            if (verProcess->waitForFinished(3000)) {
                ui->ytDlpVersionLabel->setText(verProcess->readAllStandardOutput().trimmed());
            }
            verProcess->deleteLater();
        }
        
        ui->updateYtDlpBtn->setEnabled(true);
        process->deleteLater();
    });
    
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        ui->ytDlpVersionLabel->setText("Ошибка запуска");
        ui->updateYtDlpBtn->setEnabled(true);
        process->deleteLater();
    });
    
    process->start(ytDlpPath, QStringList() << "--update");
}
