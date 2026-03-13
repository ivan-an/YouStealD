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

private slots:
    void on_saveButton_clicked();
    void on_cancelButton_clicked();
    void on_updateYtDlpBtn_clicked();
    void loadYtDlpVersion();

private:
    Ui::SettingsDialog *ui;
    QString m_language;
    void applyLanguage();
};

#endif // SETTINGSDIALOG_H
