QT += core gui widgets testlib
TEMPLATE = app
CONFIG += c++17 console
TARGET = gui_loading_smoke
msvc: QMAKE_CXXFLAGS += /utf-8

INCLUDEPATH += $$PWD/..
isEmpty(TEST_SOURCE): TEST_SOURCE = $$PWD/gui_loading_smoke.cpp
SOURCES += \
    $$TEST_SOURCE \
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
