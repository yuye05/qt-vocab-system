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

    // 数据目录多路径回退（与 QSS 同思路，style/ 换成 words/）
    // resolveDataPath 会在这些目录里找 dictionary.txt / wrong_words.txt
    std::vector<std::string> dataDirs;
    dataDirs.push_back(QDir(exeDir).filePath("words").toStdString());            // EXE 同级 words/
    dataDirs.push_back(QDir(exeDir).filePath("../words").toStdString());         // build/ 下
    dataDirs.push_back(QDir(exeDir).filePath("../release/words").toStdString()); // release
    dataDirs.push_back(QDir(exeDir).filePath("../debug/words").toStdString());   // debug
    dataDirs.push_back(QDir(exeDir + "/../..").filePath("words").toStdString()); // 源码目录
    setDataSearchDirs(dataDirs);

    MainWindow window;
    window.show();

    return app.exec();
}
