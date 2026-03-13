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
        ui->saveButton->setText("Save");
        ui->cancelButton->setText("Cancel");
        ui->updateYtDlpBtn->setToolTip("Update yt-dlp");
    } else if (m_language == "zh") {
        setWindowTitle("设置");
        ui->label->setText("语言：");
        ui->versionLabel->setText("yt-dlp 版本：");
        ui->saveButton->setText("保存");
        ui->cancelButton->setText("取消");
        ui->updateYtDlpBtn->setToolTip("更新 yt-dlp");
    } else if (m_language == "hi") {
        setWindowTitle("सेटिंग्स");
        ui->label->setText("भाषा:");
        ui->versionLabel->setText("yt-dlp संस्करण:");
        ui->saveButton->setText("सहेजें");
        ui->cancelButton->setText("रद्द करें");
        ui->updateYtDlpBtn->setToolTip("yt-dlp अपडेट करें");
    } else {
        setWindowTitle("Настройки");
        ui->label->setText("Язык:");
        ui->versionLabel->setText("Версия yt-dlp:");
        ui->saveButton->setText("Сохранить");
        ui->cancelButton->setText("Отмена");
        ui->updateYtDlpBtn->setToolTip("Обновить yt-dlp");
    }
}

void SettingsDialog::on_saveButton_clicked()
{
    m_language = ui->languageComboBox->currentData().toString();
    QSettings settings(ORG_NAME, APP_NAME);
    settings.setValue("language", m_language);
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
