#ifndef MONITORINGSETTINGS_H
#define MONITORINGSETTINGS_H

#include <QDialog>

namespace Ui {
class MonitoringSettings;
}

class MonitoringSettings : public QDialog
{
    Q_OBJECT

public:
    explicit MonitoringSettings(QWidget *parent = nullptr);
    ~MonitoringSettings();

    QString getApiKey() const;
    bool getMonitorStreams() const;
    bool getMonitorVideos() const;
    QString getChannelUrl() const;
    QString getFormat() const;
    void setApiKey(const QString &key);
    void setMonitorStreams(bool monitor);
    void setMonitorVideos(bool monitor);
    void setChannelUrl(const QString &url);
    void setFormat(const QString &format);
    void setLanguage(const QString &lang) { m_language = lang; }

    void applyLanguage();

private slots:
    void on_saveButton_clicked();
    void on_cancelButton_clicked();

private:
    Ui::MonitoringSettings *ui;
    QString m_apiKey;
    bool m_monitorStreams;
    bool m_monitorVideos;
    QString m_language;
    QString m_channelUrl;
    QString m_format;
};

#endif // MONITORINGSETTINGS_H
