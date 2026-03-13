QT += core gui network xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

TARGET = YouStealD

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/downloader.cpp \
    src/streammonitor.cpp \
    src/logger.cpp \
    src/webhookserver.cpp \
    src/webhookmanager.cpp \
    src/monitoringsettings.cpp \
    src/settingsdialog.cpp

HEADERS += \
    src/mainwindow.h \
    src/downloader.h \
    src/streammonitor.h \
    src/logger.h \
    src/webhookmanager.h \
    src/webhookserver.h \
    src/monitoringsettings.h \
    src/settingsdialog.h \
    src/appconstants.h \
    src/authmethod.h

FORMS += \
    ui/mainwindow.ui \
    ui/monitoringsettings.ui \
    ui/settingsdialog.ui

win32:RC_FILE = filex.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res/icons.qrc
