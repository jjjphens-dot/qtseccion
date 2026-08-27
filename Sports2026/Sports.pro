QT += core gui
QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    cscore.cpp \
    csportman.cpp \
    dateeditdelegate.cpp \
    main.cpp \
    mainwindow.cpp \
    readonlydelegate.cpp \
    resultinputdialog.cpp \
    selectdialog.cpp \
    signupdialog.cpp \
    sortableheaderview.cpp \
    sportsmaninfotable.cpp \

HEADERS += \
    cscore.h \
    csportman.h \
    dateeditdelegate.h \
    mainwindow.h \
    readonlydelegate.h \
    resultinputdialog.h \
    selectdialog.h \
    signupdialog.h \
    sortableheaderview.h \
    sportsmaninfotable.h \

FORMS += \
    mainwindow.ui \
    resultinputdialog.ui \
    selectdialog.ui \
    signupdialog.ui \

RESOURCES += \
    qrc.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
