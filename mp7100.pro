QT       += core gui serialport svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


# application version
VERSION = 1.0.0.1
QMAKE_TARGET_COMPANY = t2ft
QMAKE_TARGET_PRODUCT = MP7100
QMAKE_TARGET_DESCRIPTION = t2ft MP7100 control tool
QMAKE_TARGET_COPYRIGHT = Copyright (C) 2023 by t2ft - Thomas Thanner

# Define some preprocessor macros to get the infos in our application.
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_ORGANIZATION=\\\"$$QMAKE_TARGET_COMPANY\\\"
DEFINES += APP_NAME=\\\"$$QMAKE_TARGET_PRODUCT\\\"
DEFINES += APP_DOMAIN=\\\"t2ft.de\\\"

SOURCES += \
    mp7100.cpp \
    main.cpp \
    mainwidget.cpp \
    tmainwidget.cpp \
    tmessagehandler.cpp \
    tapp.cpp \
    serdev.cpp \
    tpowereventfilter.cpp

HEADERS += \
    mp7100.h \
    mainwidget.h \
    tmainwidget.h \
    tmessagehandler.h \
    tmsghandler_main.h \
    tapp.h \
    silentcall.h \
    serdev.h \
    tpowereventfilter.h

FORMS += \
    mainwidget.ui

RC_ICONS = res/mp7100_02.ico

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    mp7100.qrc
