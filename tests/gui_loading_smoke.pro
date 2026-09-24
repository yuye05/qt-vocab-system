QT += core gui widgets testlib
TEMPLATE = app
CONFIG += c++17 console
TARGET = gui_loading_smoke

INCLUDEPATH += $$PWD/..
SOURCES += \
    gui_loading_smoke.cpp \
    ../core/dictionary.cpp \
    ../ui/mainwindow.cpp \
    ../ui/homewidget.cpp \
    ../ui/dictwidget.cpp \
    ../ui/quizwidget.cpp \
    ../ui/wrongwordswidget.cpp
HEADERS += \
    ../core/dictionary.h \
    ../ui/mainwindow.h \
    ../ui/homewidget.h \
    ../ui/dictwidget.h \
    ../ui/quizwidget.h \
    ../ui/wrongwordswidget.h
