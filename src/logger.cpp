#include "logger.h"

Logger::Logger(QObject *parent)
    : QObject(parent)
{
}

void Logger::setLogOutput(QTextEdit *logOutput)
{
    this->logOutput = logOutput;
}

void Logger::log(const QString &message)
{
    if (logOutput) {
        logOutput->append(message);
    }
}
