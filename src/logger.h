#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextEdit>

class Logger : public QObject
{
    Q_OBJECT

public:
    explicit Logger(QObject *parent = nullptr);
    void setLogOutput(QTextEdit *logOutput);
    void log(const QString &message);

private:
    QTextEdit *logOutput = nullptr;
};

#endif // LOGGER_H
