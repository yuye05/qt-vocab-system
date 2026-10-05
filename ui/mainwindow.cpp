#include "mainwindow.h"
#include "homewidget.h"
#include "dictwidget.h"
#include "quizwidget.h"
#include "wrongwordswidget.h"
#include "core/dictionary.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QMessageBox>
#include <vector>

MainWindow::MainWindow(QWidget* parent, const QString& dataDirectory)
    : QMainWindow(parent), m_dictRoot(nullptr)
{
    // 页面构造时可能读取生词本，先统一设置数据目录。
    QString exeDir = QCoreApplication::applicationDirPath();
    std::vector<std::string> dataDirs;
    dataDirs.push_back(QDir(exeDir).filePath("words").toStdString());
    dataDirs.push_back(exeDir.toStdString());                          // EXE 同级
    dataDirs.push_back(QDir(exeDir + "/../words").absolutePath().toStdString()); // build 根的 words/
    dataDirs.push_back(QDir(exeDir + "/..").absolutePath().toStdString()); // build 根
    dataDirs.push_back(QDir(exeDir + "/../release/words").absolutePath().toStdString());
    dataDirs.push_back(QDir(exeDir + "/../release").absolutePath().toStdString()); // release 交叉
    dataDirs.push_back(QDir(exeDir + "/../../words").absolutePath().toStdString());
    dataDirs.push_back(QDir(exeDir + "/../../").absolutePath().toStdString());   // 项目根
    dataDirs.push_back(QDir::currentPath().toStdString()); // 兼容旧版写在工作目录的历史
    // 测试可注入独立目录；实际程序使用 Windows LOCALAPPDATA / 其他平台用户数据目录。
    const std::string userDir = (dataDirectory.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("QtVocabSystem")
        : dataDirectory).toStdString();
    if (!initializeDataDirectory(userDir, dataDirs))
        QMessageBox::warning(this, "数据目录初始化失败", QString::fromStdString(dataError()));

    setupUi();
    loadDictionary();
}

MainWindow::~MainWindow()
{
    freeTree(m_dictRoot);
}

void MainWindow::setupUi()
{
    // 窗口基本属性
    setWindowTitle("词汇学习与测验系统");
    setFixedSize(900, 650);
    setObjectName("mainWindow");

    // ---- 中央容器 ----
    QWidget* central = new QWidget(this);
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- 左侧导航栏 ----
    QFrame* sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(200);

    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // Logo 区域
    QLabel* logo = new QLabel("📖  词汇学习");
    logo->setObjectName("logo");
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedHeight(70);

    // 导航列表
    m_navList = new QListWidget;
    m_navList->setObjectName("navList");
    m_navList->addItem("🏠  首页");
    m_navList->addItem("📚  词库管理");
    m_navList->addItem("✏️  单词测验");
    m_navList->addItem("📝  生词本");
    m_navList->setCurrentRow(0);

    // 底部版本信息
    QLabel* version = new QLabel("v1.0.0");
    version->setObjectName("versionLabel");
    version->setAlignment(Qt::AlignCenter);
    version->setFixedHeight(40);

    sidebarLayout->addWidget(logo);
    sidebarLayout->addWidget(m_navList, 1);
    sidebarLayout->addWidget(version);

    // ---- 右侧页面区域 ----
    m_pages = new QStackedWidget;
    m_pages->setObjectName("pageStack");

    m_homePage  = new HomeWidget;
    m_dictPage  = new DictWidget;
    m_quizPage  = new QuizWidget;
    m_wrongPage = new WrongWordsWidget;

    m_pages->addWidget(m_homePage);   // index 0
    m_pages->addWidget(m_dictPage);   // index 1
    m_pages->addWidget(m_quizPage);   // index 2
    m_pages->addWidget(m_wrongPage);  // index 3

    // ---- 组装布局 ----
    mainLayout->addWidget(sidebar);
    mainLayout->addWidget(m_pages, 1);

    // ---- 信号连接 ----
    connect(m_navList, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavChanged);
}

void MainWindow::loadDictionary()
{
    // 加载词典数据到持久 BST（不再释放）
    m_dictRoot = loadFromFile(nullptr, DICT_FILE);
    if (!dataError().empty())
        QMessageBox::warning(this, "词库读取提示", QString::fromStdString(dataError()));
    refreshHome();

    // 将 BST 根指针的指针传给词库管理页面（DictWidget 可修改根节点）
    m_dictPage->setRoot(&m_dictRoot);

    // 将 BST 根指针传给测验页面（QuizWidget 只读访问）
    m_quizPage->setRoot(&m_dictRoot);

    // 将 BST 根指针传给生词本页面
    m_wrongPage->setRoot(&m_dictRoot);
}

void MainWindow::refreshHome()
{
    const int wrongCount = countWrongWords();
    QString readError = QString::fromStdString(dataError());
    m_homePage->setStats(countWords(m_dictRoot), wrongCount);
    std::vector<POSStat> stats;
    getPOSStats(m_dictRoot, stats);
    m_homePage->setPOSStats(stats);
    m_homePage->setQuizHistory(loadQuizHistory());
    if (!dataError().empty()) readError += "\n" + QString::fromStdString(dataError());
    if (!readError.isEmpty()) QMessageBox::warning(this, "学习记录读取提示", readError);
}

void MainWindow::onNavChanged(int index)
{
    m_pages->setCurrentIndex(index);

    // 切换到首页时刷新统计（词库可能有增删）
    if (index == 0) refreshHome();

    // 切换到生词本时刷新列表
    if (index == 3) {
        m_wrongPage->refreshList();
    }
}
