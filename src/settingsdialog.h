#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    QString getLanguage() const { return m_language; }
    bool getUseAria2c() const { return m_useAria2c; }
    QString getProxy() const { return m_proxy; }

private slots:
    void on_saveButton_clicked();
    void on_cancelButton_clicked();
    void on_updateYtDlpBtn_clicked();
    void loadYtDlpVersion();

private:
    Ui::SettingsDialog *ui;
    QString m_language;
    bool m_useAria2c = false;
    QString m_proxy;
    void applyLanguage();
};

#endif // SETTINGSDIALOG_H
