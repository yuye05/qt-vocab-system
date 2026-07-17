#include <QApplication>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <cstdlib>
#include <ctime>
#include "ui/mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("词汇学习与测验系统");

    // 随机种子（测验洗牌需要）
    srand(static_cast<unsigned>(time(nullptr)));

    // 加载全局 QSS 样式表（多路径回退，覆盖 Debug/Release/shadow build）
    QString exeDir = QCoreApplication::applicationDirPath();
    QStringList candidates;

    // 1. EXE 同级（最终部署：build/release/style/ 或 standalone）
    candidates << QDir(exeDir).filePath("style/app.qss");

    // 2. build/ 根目录下的 style/（qmake COPIES 到 $$OUT_PWD 的结果）
    //    EXE 在 build/release/ 或 build/debug/，style 在 build/style/
    candidates << QDir(exeDir).filePath("../style/app.qss");

    // 3. 显式 Debug/Release 交叉回退
    candidates << QDir(exeDir).filePath("../release/style/app.qss");
    candidates << QDir(exeDir).filePath("../debug/style/app.qss");

    // 4. 源码目录（Qt Creator 直接运行或 shadow build 回退）
    candidates << QDir(exeDir + "/../..").filePath("style/app.qss");
    candidates << QDir(exeDir + "/../../VocabSystem").filePath("style/app.qss");
    candidates << QDir(exeDir + "/../../英汉词汇学习与测验系统").filePath("style/app.qss");

    for (const QString& path : candidates) {
        QFile styleFile(path);
        if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
            QString style = QString::fromUtf8(styleFile.readAll());
            app.setStyleSheet(style);
            styleFile.close();
            break;
        }
    }

    MainWindow window;
    window.show();

    return app.exec();
}
