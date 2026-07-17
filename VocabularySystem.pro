QT       += core gui widgets
TEMPLATE  = app
CONFIG   += c++17
TARGET    = VocabularySystem

SOURCES += \
    main.cpp \
    core/dictionary.cpp \
    ui/mainwindow.cpp \
    ui/homewidget.cpp \
    ui/dictwidget.cpp \
    ui/quizwidget.cpp \
    ui/wrongwordswidget.cpp

HEADERS += \
    core/dictionary.h \
    ui/mainwindow.h \
    ui/homewidget.h \
    ui/dictwidget.h \
    ui/quizwidget.h \
    ui/wrongwordswidget.h

# 头文件搜索路径（Qt Creator shadow build 必须）
# 源码用 #include "core/dictionary.h"，所以 INCLUDEPATH 要指向项目根目录
INCLUDEPATH += $$PWD

# MSVC: UTF-8 source files on Chinese Windows
msvc {
    QMAKE_CXXFLAGS += /source-charset:utf-8 /execution-charset:utf-8
    DEFINES       += _CRT_SECURE_NO_WARNINGS
}

# 构建后复制数据文件和样式到 EXE 同级目录
# $$OUT_PWD 在 shadow build 中指向 build 根目录
# 但 Qt Creator 的 EXE 实际在 $$OUT_PWD/release 或 $$OUT_PWD/debug
# 所以同时复制到两个子目录 + 根目录，确保 Debug/Release 都能找到
win32 {
    COPIES += vocabularyData
    vocabularyData.files = data/dictionary.txt data/wrong_words.txt
    vocabularyData.path = $$OUT_PWD/release

    COPIES += vocabularyDataDebug
    vocabularyDataDebug.files = data/dictionary.txt data/wrong_words.txt
    vocabularyDataDebug.path = $$OUT_PWD/debug

    COPIES += styleFile
    styleFile.files = style/app.qss
    styleFile.path = $$OUT_PWD/release/style

    COPIES += styleFileDebug
    styleFileDebug.files = style/app.qss
    styleFileDebug.path = $$OUT_PWD/debug/style
}
