#include "monitoringsettings.h"
#include "ui_monitoringsettings.h"
#include "appconstants.h"

#include <QSettings>

MonitoringSettings::MonitoringSettings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MonitoringSettings)
    , m_monitorStreams(true)
    , m_monitorVideos(true)
    , m_language("ru")
    , m_channelUrl("")
{
    ui->setupUi(this);
    setModal(true);

    QSettings settings(ORG_NAME, APP_NAME);
    QString savedApiKey = settings.value("apiKey", "").toString();
    bool savedMonitorStreams = settings.value("monitorStreams", false).toBool();
    bool savedMonitorVideos = settings.value("monitorVideos", false).toBool();
    QString savedChannelUrl = settings.value("monitorChannelUrl", "").toString();
    QString savedFormat = settings.value("monitorFormat", "best").toString();
    m_language = settings.value("language", "ru").toString();

    ui->apiKeyLineEdit->setText(savedApiKey);
    ui->monitorStreamsCheckBox->setChecked(savedMonitorStreams);
    ui->monitorVideosCheckBox->setChecked(savedMonitorVideos);
    ui->channelUrlLineEdit->setText(savedChannelUrl);

    ui->formatComboBox->addItems({"best", "bestvideo+bestaudio", "worst", "mp4", "webm", "m4a", "mp3", "wav", "flac"});
    ui->formatComboBox->setEditable(true);
    ui->formatComboBox->lineEdit()->setAlignment(Qt::AlignCenter);
    ui->formatComboBox->setEditable(false);
    ui->formatComboBox->setCurrentIndex(0);

    m_apiKey = savedApiKey;
    m_monitorStreams = savedMonitorStreams;
    m_monitorVideos = savedMonitorVideos;
    m_channelUrl = savedChannelUrl;
    m_format = savedFormat;

    applyLanguage();
}

MonitoringSettings::~MonitoringSettings()
{
    delete ui;
}

void MonitoringSettings::applyLanguage()
{
    if (m_language == "en") {
        setWindowTitle("Monitoring Settings");
        ui->apiKeyLabel->setText("YouTube Data API Key:");
        ui->apiKeyLineEdit->setPlaceholderText("Enter your API key");
        ui->apiKeyLinkLabel->setText("You can get an API key at https://console.cloud.google.com/");
        ui->label_2->setText("What to monitor:");
        ui->monitorStreamsCheckBox->setText("Streams");
        ui->monitorVideosCheckBox->setText("Videos");
        ui->channelUrlLabel->setText("Channel URL to monitor:");
        ui->channelUrlLineEdit->setPlaceholderText("https://www.youtube.com/@ChannelName");
        ui->formatLabel->setText("Select format:");
        ui->saveButton->setText("Save & Start");
        ui->cancelButton->setText("Cancel");
    } else if (m_language == "zh") {
        setWindowTitle("监控设置");
        ui->apiKeyLabel->setText("YouTube Data API 密钥：");
        ui->apiKeyLineEdit->setPlaceholderText("请输入您的 API 密钥");
        ui->apiKeyLinkLabel->setText("可在 https://console.cloud.google.com/ 获取 API 密钥");
        ui->label_2->setText("要监控的内容：");
        ui->monitorStreamsCheckBox->setText("直播");
        ui->monitorVideosCheckBox->setText("视频");
        ui->channelUrlLabel->setText("要监控的频道URL：");
        ui->channelUrlLineEdit->setPlaceholderText("https://www.youtube.com/@频道名称");
        ui->formatLabel->setText("选择格式：");
        ui->saveButton->setText("保存并开始");
        ui->cancelButton->setText("取消");
    } else if (m_language == "hi") {
        setWindowTitle("निगरानी सेटिंग्स");
        ui->apiKeyLabel->setText("YouTube Data API कुंजी:");
        ui->apiKeyLineEdit->setPlaceholderText("अपनी API कुंजी दर्ज करें");
        ui->apiKeyLinkLabel->setText("आप https://console.cloud.google.com/ से API कुंजी प्राप्त कर सकते हैं");
        ui->label_2->setText("क्या मॉनिटर करना है:");
        ui->monitorStreamsCheckBox->setText("स्ट्रीम");
        ui->monitorVideosCheckBox->setText("वीडियो");
        ui->channelUrlLabel->setText("मॉनिटर करने के लिए चैनल URL:");
        ui->channelUrlLineEdit->setPlaceholderText("https://www.youtube.com/@चैनलनाम");
        ui->saveButton->setText("सहेजें और शुरू करें");
        ui->cancelButton->setText("रद्द करें");
    } else {
        setWindowTitle("Настройки мониторинга");
        ui->apiKeyLabel->setText("API Key YouTube Data API:");
        ui->apiKeyLineEdit->setPlaceholderText("Введите ваш API ключ");
        ui->apiKeyLinkLabel->setText("Получить API ключ можно на https://console.cloud.google.com/");
        ui->label_2->setText("Что отслеживать:");
        ui->monitorStreamsCheckBox->setText("Стримы");
        ui->monitorVideosCheckBox->setText("Видео");
        ui->channelUrlLabel->setText("URL канала для отслеживания:");
        ui->channelUrlLineEdit->setPlaceholderText("https://www.youtube.com/@ИмяКанала");
        ui->saveButton->setText("Сохранить и запустить");
        ui->cancelButton->setText("Отмена");
    }
}

QString MonitoringSettings::getApiKey() const
{
    return m_apiKey;
}

bool MonitoringSettings::getMonitorStreams() const
{
    return m_monitorStreams;
}

bool MonitoringSettings::getMonitorVideos() const
{
    return m_monitorVideos;
}

QString MonitoringSettings::getChannelUrl() const
{
    return m_channelUrl;
}

QString MonitoringSettings::getFormat() const
{
    return m_format;
}

void MonitoringSettings::setApiKey(const QString &key)
{
    m_apiKey = key;
    ui->apiKeyLineEdit->setText(key);
}

void MonitoringSettings::setMonitorStreams(bool monitor)
{
    m_monitorStreams = monitor;
    ui->monitorStreamsCheckBox->setChecked(monitor);
}

void MonitoringSettings::setMonitorVideos(bool monitor)
{
    m_monitorVideos = monitor;
    ui->monitorVideosCheckBox->setChecked(monitor);
}

void MonitoringSettings::setChannelUrl(const QString &url)
{
    m_channelUrl = url;
    ui->channelUrlLineEdit->setText(url);
}

void MonitoringSettings::setFormat(const QString &format)
{
    m_format = format;
    int index = ui->formatComboBox->findText(format);
    if (index >= 0) {
        ui->formatComboBox->setCurrentIndex(index);
    }
}

void MonitoringSettings::on_saveButton_clicked()
{
    m_apiKey = ui->apiKeyLineEdit->text();
    m_monitorStreams = ui->monitorStreamsCheckBox->isChecked();
    m_monitorVideos = ui->monitorVideosCheckBox->isChecked();
    m_channelUrl = ui->channelUrlLineEdit->text();
    m_format = ui->formatComboBox->currentText();

    QSettings settings(ORG_NAME, APP_NAME);
    settings.setValue("apiKey", m_apiKey);
    settings.setValue("monitorStreams", m_monitorStreams);
    settings.setValue("monitorVideos", m_monitorVideos);
    settings.setValue("monitorChannelUrl", m_channelUrl);
    settings.setValue("monitorFormat", m_format);

    accept();
}

void MonitoringSettings::on_cancelButton_clicked()
{
    reject();
}
