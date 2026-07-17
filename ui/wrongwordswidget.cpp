#include "wrongwordswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPainter>
#include <QKeyEvent>
#include <QMessageBox>
#include <QCoreApplication>
#include <cstdlib>

// ================================================================
// WrongProgressWidget：圆角进度条（QPainter 自绘）
// ================================================================

void WrongProgressWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    qreal radius = r.height() / 2.0;

    // 背景轨道
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#D9CEBB"));
    p.drawRoundedRect(r, radius, radius);

    // 填充部分
    if (m_max > 0 && m_val > 0) {
        qreal ratio = static_cast<qreal>(m_val) / m_max;
        QRectF fillR = r;
        fillR.setWidth(r.width() * ratio);
        if (fillR.width() < 2) fillR.setWidth(2);
        p.setBrush(QColor("#8B9D83"));
        p.drawRoundedRect(fillR, radius, radius);
    }
}

// ================================================================
// WrongWordsWidget
// ================================================================

WrongWordsWidget::WrongWordsWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(0);

    QLabel* title = new QLabel("生词本");
    title->setObjectName("pageTitle");

    m_pages = new QStackedWidget;
    m_pages->setObjectName("wrongPageStack");

    buildListPage();
    buildQuizSetupPage();
    buildQuizPage();
    buildCardPage();

    mainLayout->addWidget(title);
    mainLayout->addWidget(m_pages, 1);

    // 初始加载列表
    refreshList();
}

WrongWordsWidget::~WrongWordsWidget()
{
}

void WrongWordsWidget::setRoot(DictNode** rootPtr)
{
    m_dictRoot = rootPtr;
}

// ================================================================
// 加载生词本数据到 vector（通过静态桥接调用 C 风格回调）
// ================================================================

static std::vector<WrongWord>* g_loadBuf = nullptr;

static void loadCallback(const WrongWord* w, int /*rank*/, int /*total*/)
{
    if (g_loadBuf) g_loadBuf->push_back(*w);
}

void WrongWordsWidget::loadWrongWordList(std::vector<WrongWord>& arr)
{
    arr.clear();
    g_loadBuf = &arr;
    showWrongWords(loadCallback);
    g_loadBuf = nullptr;
}

// ================================================================
// 排序参考函数
// ================================================================

static bool cmpByCountDesc(const WrongWord& a, const WrongWord& b)
{
    return a.count > b.count;
}

static bool cmpByAlphabet(const WrongWord& a, const WrongWord& b)
{
    return a.word < b.word;
}

// ================================================================
// 列表页
// ================================================================

void WrongWordsWidget::buildListPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(12);

    // 顶栏：排序按钮
    QHBoxLayout* topBar = new QHBoxLayout;
    m_sortBtn = new QPushButton("按错误次数 ▼");
    m_sortBtn->setObjectName("wrongSortBtn");
    m_sortBtn->setFixedHeight(36);
    connect(m_sortBtn, &QPushButton::clicked, this, &WrongWordsWidget::onSortToggle);
    topBar->addWidget(m_sortBtn);
    topBar->addStretch();

    // 空状态提示
    m_emptyLabel = new QLabel("暂无生词，去做测验吧！");
    m_emptyLabel->setObjectName("placeholder");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->hide();

    // 列表容器（可滚动）
    QScrollArea* scroll = new QScrollArea;
    scroll->setObjectName("wrongScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_listContainer = new QWidget;
    m_listContainer->setObjectName("wrongListContainer");
    new QVBoxLayout(m_listContainer);
    scroll->setWidget(m_listContainer);

    // 底部入口按钮
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(16);
    btnRow->addStretch();

    QPushButton* quizBtn = new QPushButton("专项测验");
    quizBtn->setObjectName("quizRetryBtn");
    quizBtn->setFixedSize(140, 44);
    connect(quizBtn, &QPushButton::clicked, this, &WrongWordsWidget::onEnterQuizSetup);

    QPushButton* cardBtn = new QPushButton("卡片翻阅");
    cardBtn->setObjectName("quizBackBtn");
    cardBtn->setFixedSize(140, 44);
    connect(cardBtn, &QPushButton::clicked, this, &WrongWordsWidget::onEnterCardReview);

    btnRow->addWidget(quizBtn);
    btnRow->addWidget(cardBtn);
    btnRow->addStretch();

    lay->addLayout(topBar);
    lay->addWidget(m_emptyLabel);
    lay->addWidget(scroll, 1);
    lay->addLayout(btnRow);

    m_pages->addWidget(page);  // index 0
}

void WrongWordsWidget::refreshList()
{
    // 清空旧条目
    QVBoxLayout* listLay = qobject_cast<QVBoxLayout*>(m_listContainer->layout());
    if (listLay) {
        QLayoutItem* item;
        while ((item = listLay->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    std::vector<WrongWord> arr;
    loadWrongWordList(arr);

    if (arr.empty()) {
        m_emptyLabel->setText("暂无生词，去做测验吧！");
        m_emptyLabel->show();
        m_listContainer->hide();
        m_sortBtn->hide();
        return;
    }

    m_emptyLabel->hide();
    m_listContainer->show();
    m_sortBtn->show();

    // 排序
    if (m_sortByCount) {
        std::sort(arr.begin(), arr.end(), cmpByCountDesc);
    } else {
        std::sort(arr.begin(), arr.end(), cmpByAlphabet);
    }

    // 为每个生词构建条目行
    for (const auto& w : arr) {
        QWidget* row = new QWidget;
        row->setObjectName("wrongEntryRow");
        QHBoxLayout* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(12, 8, 12, 8);
        rowLay->setSpacing(8);

        // 单词（粗体，查 BST 获取释义）
        QString wordText = QString::fromStdString(w.word);
        QString posText;
        QString meaningText;

        if (m_dictRoot && *m_dictRoot) {
            DictNode* node = searchWord(*m_dictRoot, w.word);
            if (node) {
                posText   = QString::fromLocal8Bit(node->pos.c_str());
                meaningText = QString::fromLocal8Bit(node->meaning.c_str());
            }
        }

        QString displayText = wordText;
        if (!posText.isEmpty()) {
            displayText += "  " + posText + " " + meaningText;
        }

        QLabel* wordLabel = new QLabel(displayText);
        wordLabel->setObjectName("wrongEntryWord");
        QLabel* countLabel = new QLabel(QString("错 %1 次").arg(w.count));
        countLabel->setObjectName("wrongEntryCount");

        // 删除按钮
        QPushButton* delBtn = new QPushButton("✕");
        delBtn->setObjectName("wrongDeleteBtn");
        delBtn->setFixedSize(30, 30);
        std::string wordCopy = w.word;  // 按值捕获
        connect(delBtn, &QPushButton::clicked, this, [this, wordCopy]() {
            onDeleteWord(wordCopy);
        });

        rowLay->addWidget(wordLabel, 1);
        rowLay->addWidget(countLabel);
        rowLay->addWidget(delBtn);

        listLay->addWidget(row);
    }

    listLay->addStretch();
}

// ================================================================
// 测验设置页
// ================================================================

void WrongWordsWidget::buildQuizSetupPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(20);

    // ---- 模式选择 ----
    QLabel* modeTitle = new QLabel("选择测验模式");
    modeTitle->setObjectName("quizSectionLabel");

    QHBoxLayout* modeRow = new QHBoxLayout;
    modeRow->setSpacing(12);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    const char* modeLabels[] = {"拼写模式", "英→中选择", "中→英选择"};
    for (int i = 0; i < 3; i++) {
        QPushButton* btn = new QPushButton(modeLabels[i]);
        btn->setObjectName("modeBtn");
        btn->setCheckable(true);
        btn->setFixedHeight(44);
        m_modeGroup->addButton(btn, i);
        modeRow->addWidget(btn, 1);
    }
    m_modeGroup->button(0)->setChecked(true);

    connect(m_modeGroup, &QButtonGroup::idClicked,
            this, &WrongWordsWidget::onModeBtn);

    // ---- 题数选择 ----
    QLabel* countTitle = new QLabel("选择题数");
    countTitle->setObjectName("quizSectionLabel");

    QHBoxLayout* countRow = new QHBoxLayout;
    countRow->setSpacing(12);

    m_countGroup = new QButtonGroup(this);
    m_countGroup->setExclusive(true);

    int countValues[] = {5, 10, 15, 20};
    for (int i = 0; i < 4; i++) {
        QPushButton* btn = new QPushButton(QString::number(countValues[i]));
        btn->setObjectName("countBtn");
        btn->setCheckable(true);
        btn->setFixedSize(72, 44);
        m_countGroup->addButton(btn, i);
        countRow->addWidget(btn);
        countRow->addStretch();
    }
    m_countGroup->button(1)->setChecked(true);  // 默认 10 题

    connect(m_countGroup, &QButtonGroup::idClicked,
            this, &WrongWordsWidget::onCountBtn);

    // ---- 开始 / 返回 ----
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    QPushButton* backBtn = new QPushButton("返回");
    backBtn->setObjectName("quizBackBtn");
    backBtn->setFixedSize(120, 44);
    connect(backBtn, &QPushButton::clicked, this, &WrongWordsWidget::onBackToList);

    QPushButton* startBtn = new QPushButton("开始测验");
    startBtn->setObjectName("startQuizBtn");
    startBtn->setFixedSize(180, 48);
    connect(startBtn, &QPushButton::clicked, this, &WrongWordsWidget::onStartQuiz);

    btnRow->addWidget(backBtn);
    btnRow->addSpacing(16);
    btnRow->addWidget(startBtn);
    btnRow->addStretch();

    lay->addWidget(modeTitle);
    lay->addLayout(modeRow);
    lay->addWidget(countTitle);
    lay->addLayout(countRow);
    lay->addStretch();
    lay->addLayout(btnRow);
    lay->addStretch();

    m_pages->addWidget(page);  // index 1
}

// ================================================================
// 答题页
// ================================================================

void WrongWordsWidget::buildQuizPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(12);

    // 顶栏：进度 + 取消
    QHBoxLayout* topBar = new QHBoxLayout;

    m_progressLabel = new QLabel("题目 1/10");
    m_progressLabel->setObjectName("quizProgressLabel");

    WrongProgressWidget* pbar = new WrongProgressWidget;
    pbar->setObjectName("quizProgressBar");
    pbar->setFixedHeight(8);
    m_progressFill = pbar;

    topBar->addWidget(m_progressLabel);
    topBar->addStretch();

    QPushButton* cancelBtn = new QPushButton("取消");
    cancelBtn->setObjectName("cancelQuizBtn");
    cancelBtn->setFixedSize(72, 32);
    connect(cancelBtn, &QPushButton::clicked, this, &WrongWordsWidget::onCancel);
    topBar->addWidget(cancelBtn);

    // 题目区域
    m_questionLabel = new QLabel;
    m_questionLabel->setObjectName("quizQuestion");
    m_questionLabel->setAlignment(Qt::AlignCenter);
    m_questionLabel->setMinimumHeight(80);

    // 拼写输入
    m_spellingInput = new QLineEdit;
    m_spellingInput->setObjectName("quizSpellingInput");
    m_spellingInput->setPlaceholderText("请输入英文单词...");
    m_spellingInput->setAlignment(Qt::AlignCenter);
    m_spellingInput->setFixedHeight(48);
    connect(m_spellingInput, &QLineEdit::returnPressed,
            this, &WrongWordsWidget::onSpellingReturn);

    // 选择题选项
    QHBoxLayout* optRow = new QHBoxLayout;
    optRow->setSpacing(12);
    for (int i = 0; i < 4; i++) {
        m_optBtns[i] = new QPushButton;
        m_optBtns[i]->setObjectName("quizOptionBtn");
        m_optBtns[i]->setMinimumHeight(52);
        optRow->addWidget(m_optBtns[i], 1);
    }

    // 反馈区域
    m_feedbackLabel = new QLabel;
    m_feedbackLabel->setObjectName("quizFeedback");
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    m_feedbackLabel->setMinimumHeight(36);

    // 操作按钮
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_actionBtn = new QPushButton("提交");
    m_actionBtn->setObjectName("quizActionBtn");
    m_actionBtn->setFixedSize(140, 44);
    connect(m_actionBtn, &QPushButton::clicked, this, &WrongWordsWidget::onSubmitOrNext);
    btnRow->addWidget(m_actionBtn);
    btnRow->addStretch();

    lay->addLayout(topBar);
    lay->addWidget(m_progressFill);
    lay->addStretch();
    lay->addWidget(m_questionLabel);
    lay->addWidget(m_spellingInput);
    lay->addLayout(optRow);
    lay->addWidget(m_feedbackLabel);
    lay->addStretch();
    lay->addLayout(btnRow);
    lay->addStretch();

    m_pages->addWidget(page);  // index 2
}

// ================================================================
// 卡片翻阅页
// ================================================================

void WrongWordsWidget::buildCardPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(12);

    // 顶栏：进度 + 返回
    QHBoxLayout* topBar = new QHBoxLayout;

    m_cardProgressLabel = new QLabel("1/12");
    m_cardProgressLabel->setObjectName("quizProgressLabel");

    WrongProgressWidget* pbar = new WrongProgressWidget;
    pbar->setFixedHeight(8);
    m_cardProgressFill = pbar;

    topBar->addWidget(m_cardProgressLabel);
    topBar->addStretch();

    QPushButton* backBtn = new QPushButton("返回");
    backBtn->setObjectName("cancelQuizBtn");
    backBtn->setFixedSize(72, 32);
    connect(backBtn, &QPushButton::clicked, this, &WrongWordsWidget::onBackToList);
    topBar->addWidget(backBtn);

    // 单词
    m_cardWordLabel = new QLabel;
    m_cardWordLabel->setObjectName("quizQuestion");
    m_cardWordLabel->setAlignment(Qt::AlignCenter);
    m_cardWordLabel->setMinimumHeight(100);

    // 释义（点击"显示答案"后出现）
    m_cardMeaningLabel = new QLabel;
    m_cardMeaningLabel->setObjectName("quizFeedback");
    m_cardMeaningLabel->setAlignment(Qt::AlignCenter);
    m_cardMeaningLabel->setMinimumHeight(48);
    m_cardMeaningLabel->hide();

    // 操作按钮区域
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    // "显示答案"按钮
    m_cardActionBtn = new QPushButton("显示答案");
    m_cardActionBtn->setObjectName("quizActionBtn");
    m_cardActionBtn->setFixedSize(160, 44);
    connect(m_cardActionBtn, &QPushButton::clicked, this, &WrongWordsWidget::onCardShowAnswer);

    // "已掌握"按钮
    m_cardMasteredBtn = new QPushButton("已掌握 ✓");
    m_cardMasteredBtn->setObjectName("cardMasteredBtn");
    m_cardMasteredBtn->setFixedSize(140, 44);
    m_cardMasteredBtn->hide();
    connect(m_cardMasteredBtn, &QPushButton::clicked, this, &WrongWordsWidget::onCardMastered);

    // "未掌握"按钮
    m_cardNotMasteredBtn = new QPushButton("未掌握 ✗");
    m_cardNotMasteredBtn->setObjectName("cardNotMasteredBtn");
    m_cardNotMasteredBtn->setFixedSize(140, 44);
    m_cardNotMasteredBtn->hide();
    connect(m_cardNotMasteredBtn, &QPushButton::clicked, this, &WrongWordsWidget::onCardNotMastered);

    btnRow->addWidget(m_cardActionBtn);
    btnRow->addSpacing(16);
    btnRow->addWidget(m_cardMasteredBtn);
    btnRow->addSpacing(16);
    btnRow->addWidget(m_cardNotMasteredBtn);
    btnRow->addStretch();

    lay->addLayout(topBar);
    lay->addWidget(m_cardProgressFill);
    lay->addStretch();
    lay->addWidget(m_cardWordLabel);
    lay->addWidget(m_cardMeaningLabel);
    lay->addStretch();
    lay->addLayout(btnRow);
    lay->addStretch();

    m_pages->addWidget(page);  // index 3
}

// ================================================================
// 列表页槽函数
// ================================================================

void WrongWordsWidget::onSortToggle()
{
    m_sortByCount = !m_sortByCount;
    m_sortBtn->setText(m_sortByCount ? "按错误次数 ▼" : "按字母 A-Z");
    refreshList();
}

void WrongWordsWidget::onDeleteWord(const std::string& word)
{
    removeWrongWord(word);
    refreshList();
}

void WrongWordsWidget::onEnterQuizSetup()
{
    loadWrongWordList(m_wrongArr);
    if (m_wrongArr.empty()) return;

    m_pages->setCurrentIndex(1);
}

void WrongWordsWidget::onEnterCardReview()
{
    m_cardArr.clear();
    loadWrongWordList(m_cardArr);
    if (m_cardArr.empty()) return;

    m_cardIdx = 0;
    m_cardMastered = 0;
    m_cardShowAnswer = false;

    // 显示第一张卡片
    int total = static_cast<int>(m_cardArr.size());
    m_cardProgressLabel->setText(QString("1/%1").arg(total));
    static_cast<WrongProgressWidget*>(m_cardProgressFill)->setValue(0, total);

    m_cardWordLabel->setText(QString::fromStdString(m_cardArr[0].word));
    m_cardWordLabel->setStyleSheet("font-size: 28px; color: #C66B3D; font-weight: bold;");

    m_cardMeaningLabel->hide();
    m_cardActionBtn->show();
    m_cardMasteredBtn->hide();
    m_cardNotMasteredBtn->hide();

    m_pages->setCurrentIndex(3);
}

void WrongWordsWidget::onBackToList()
{
    m_wrongArr.clear();
    m_cardArr.clear();
    refreshList();
    m_pages->setCurrentIndex(0);
}

// ================================================================
// 卡片页槽函数
// ================================================================

void WrongWordsWidget::onCardShowAnswer()
{
    m_cardShowAnswer = true;

    // 查 BST 获取释义
    QString meaningText;
    if (m_dictRoot && *m_dictRoot) {
        DictNode* node = searchWord(*m_dictRoot, m_cardArr[m_cardIdx].word);
        if (node) {
            meaningText = QString::fromLocal8Bit(node->pos.c_str()) + " " +
                          QString::fromLocal8Bit(node->meaning.c_str());
        }
    }
    if (meaningText.isEmpty()) {
        meaningText = "(释义未找到)";
    }

    m_cardMeaningLabel->setText(meaningText);
    m_cardMeaningLabel->setStyleSheet("font-size: 20px; color: #2C2416; font-weight: bold;");
    m_cardMeaningLabel->show();

    m_cardActionBtn->hide();
    m_cardMasteredBtn->show();
    m_cardNotMasteredBtn->show();
}

void WrongWordsWidget::onCardMastered()
{
    // 答对 → 移除生词
    m_cardMastered++;
    removeWrongWord(m_cardArr[m_cardIdx].word);
    advanceCard();
}

void WrongWordsWidget::onCardNotMastered()
{
    advanceCard();
}

void WrongWordsWidget::advanceCard()
{
    m_cardIdx++;
    m_cardShowAnswer = false;

    int total = static_cast<int>(m_cardArr.size());
    if (m_cardIdx >= total) {
        // 全部完成，显示掌握了多少
        refreshList();
        m_emptyLabel->setText(
            QString("翻阅完成！本次掌握 %1/%2 个单词")
                .arg(m_cardMastered).arg(total));
        m_emptyLabel->show();
        m_pages->setCurrentIndex(0);
        return;
    }

    m_cardProgressLabel->setText(QString("%1/%2").arg(m_cardIdx + 1).arg(total));
    static_cast<WrongProgressWidget*>(m_cardProgressFill)->setValue(m_cardIdx, total);

    m_cardWordLabel->setText(QString::fromStdString(m_cardArr[m_cardIdx].word));

    m_cardMeaningLabel->hide();
    m_cardActionBtn->show();
    m_cardMasteredBtn->hide();
    m_cardNotMasteredBtn->hide();
}

// ================================================================
// 测验设置槽函数
// ================================================================

void WrongWordsWidget::onModeBtn(int id)
{
    m_selectedMode = id;
}

void WrongWordsWidget::onCountBtn(int id)
{
    static const int counts[] = {5, 10, 15, 20};
    m_selectedCount = counts[id];
}

void WrongWordsWidget::onStartQuiz()
{
    loadWrongWordList(m_wrongArr);
    if (m_wrongArr.empty()) return;
    startQuiz();
}

// ================================================================
// 测验流程
// ================================================================

void WrongWordsWidget::startQuiz()
{
    m_quizTotal = static_cast<int>(m_wrongArr.size());
    if (m_quizTotal == 0) return;

    // 洗牌生成题序（直接对 m_wrongArr 索引做 shuffle）
    m_quizIndices = shuffledIndices(m_quizTotal);

    int actualCount = std::min(m_selectedCount, m_quizTotal);
    m_quizIndices.resize(actualCount);

    m_currentQ     = 0;
    m_correctCount = 0;
    m_answered     = false;

    // 切到答题页
    m_pages->setCurrentIndex(2);
    showQuestion();
}

void WrongWordsWidget::showQuestion()
{
    m_answered = false;

    int total = static_cast<int>(m_quizIndices.size());
    int idx = m_quizIndices[m_currentQ];

    // 进度显示
    m_progressLabel->setText(
        QString("题目 %1/%2").arg(m_currentQ + 1).arg(total));
    static_cast<WrongProgressWidget*>(m_progressFill)->setValue(m_currentQ, total);

    // 清空反馈
    m_feedbackLabel->setText("");
    m_feedbackLabel->setStyleSheet("");

    // 重置输入
    m_spellingInput->clear();
    m_spellingInput->setEnabled(true);

    // 焦点：拼写模式→输入框，选择模式→第一个选项按钮
    if (m_selectedMode == 0) {
        m_spellingInput->setFocus();
    } else if (m_optBtns[0]->isVisible()) {
        m_optBtns[0]->setFocus();
    }

    for (int i = 0; i < 4; i++) {
        m_optBtns[i]->setEnabled(true);
        m_optBtns[i]->setStyleSheet("");
    }

    m_actionBtn->setText("提交");
    m_actionBtn->setVisible(m_selectedMode == 0);

    // 从 BST 查找该生词的释义
    QString posStr, meaningStr;
    DictNode* node = nullptr;
    if (m_dictRoot && *m_dictRoot) {
        node = searchWord(*m_dictRoot, m_wrongArr[idx].word);
        if (node) {
            posStr     = QString::fromLocal8Bit(node->pos.c_str());
            meaningStr = QString::fromLocal8Bit(node->meaning.c_str());
        }
    }

    if (m_selectedMode == 0) {
        // 拼写模式：显示中文，输入英文
        m_questionLabel->setText(posStr + " " + meaningStr);
        m_questionLabel->setStyleSheet("font-size: 24px; color: #2C2416; font-weight: bold;");

        m_spellingInput->setVisible(true);
        for (int i = 0; i < 4; i++) m_optBtns[i]->setVisible(false);

    } else if (m_selectedMode == 1) {
        // 英→中：显示英文，选中文释义
        m_questionLabel->setText(QString::fromStdString(m_wrongArr[idx].word));
        m_questionLabel->setStyleSheet("font-size: 28px; color: #C66B3D; font-weight: bold;");

        m_spellingInput->setVisible(false);
        for (int i = 0; i < 4; i++) m_optBtns[i]->setVisible(true);

        setupChoiceOptions(idx);

    } else {
        // 中→英：显示中文，选英文
        m_questionLabel->setText(posStr + " " + meaningStr);
        m_questionLabel->setStyleSheet("font-size: 24px; color: #2C2416; font-weight: bold;");

        m_spellingInput->setVisible(false);
        for (int i = 0; i < 4; i++) m_optBtns[i]->setVisible(true);

        setupChoiceOptions(idx);
    }
}

void WrongWordsWidget::setupChoiceOptions(int wordIdx)
{
    bool showMeaning = (m_selectedMode == 1);
    int total = m_quizTotal;

    // 生词不足 4 个时，无法生成选择题（需要 4 个不同选项）
    if (total < 4) {
        // 退化为两种选项：正确 + 能显示几个算几个
        for (int i = 0; i < 4; i++) {
            m_optBtns[i]->setVisible(i < total);
        }
        const char* prefix[] = {"A. ", "B. ", "C. ", "D. "};
        for (int i = 0; i < total; i++) {
            DictNode* optNode = nullptr;
            if (m_dictRoot && *m_dictRoot) {
                optNode = searchWord(*m_dictRoot, m_wrongArr[i].word);
            }
            QString text;
            if (optNode) {
                text = QString(prefix[i]) +
                    (showMeaning
                        ? QString::fromLocal8Bit(optNode->pos.c_str()) + " " +
                          QString::fromLocal8Bit(optNode->meaning.c_str())
                        : QString::fromStdString(optNode->word));
            } else {
                text = QString(prefix[i]) + QString::fromStdString(m_wrongArr[i].word);
            }
            m_optBtns[i]->setText(text);
            m_optBtns[i]->setProperty("optIndex", i);
            disconnect(m_optBtns[i], nullptr, nullptr, nullptr);
            connect(m_optBtns[i], &QPushButton::clicked, this, [this, i]() {
                checkChoiceAnswer(i);
            });
        }
        return;
    }

    int opts[4];
    pickOptions(wordIdx, total, opts);
    const char* prefix[] = {"A. ", "B. ", "C. ", "D. "};
    for (int i = 0; i < 4; i++) {
        int optIdx = opts[i];
        // 查找 BST 节点获取显示文本
        QString text;
        DictNode* optNode = nullptr;
        if (m_dictRoot && *m_dictRoot) {
            optNode = searchWord(*m_dictRoot, m_wrongArr[optIdx].word);
        }
        if (optNode) {
            text = QString(prefix[i]) +
                (showMeaning
                    ? QString::fromLocal8Bit(optNode->pos.c_str()) + " " +
                      QString::fromLocal8Bit(optNode->meaning.c_str())
                    : QString::fromStdString(optNode->word));
        } else {
            text = QString(prefix[i]) + QString::fromStdString(m_wrongArr[optIdx].word);
        }
        m_optBtns[i]->setText(text);
        m_optBtns[i]->setProperty("optIndex", optIdx);
        disconnect(m_optBtns[i], nullptr, nullptr, nullptr);
        connect(m_optBtns[i], &QPushButton::clicked, this, [this, i]() {
            int idx = m_optBtns[i]->property("optIndex").toInt();
            checkChoiceAnswer(idx);
        });
    }
}

void WrongWordsWidget::onSpellingReturn()
{
    if (!m_answered && m_spellingInput->isVisible()) {
        onSubmitOrNext();
    }
}

void WrongWordsWidget::onSubmitOrNext()
{
    if (m_answered) {
        // 下一题
        m_currentQ++;
        if (m_currentQ >= static_cast<int>(m_quizIndices.size())) {
            showQuizResult();
        } else {
            showQuestion();
        }
    } else {
        if (m_selectedMode == 0) {
            checkSpellingAnswer();
        }
    }
}

void WrongWordsWidget::onCancel()
{
    m_wrongArr.clear();
    m_quizIndices.clear();
    m_correctWords.clear();  // 取消不删除任何生词
    m_pages->setCurrentIndex(0);
    refreshList();
}

// ================================================================
// 键盘快捷键
// ================================================================

void WrongWordsWidget::keyPressEvent(QKeyEvent* event)
{
    int page = m_pages->currentIndex();

    if (page == 2) {
        // ---- 答题页 ----
        int key = event->key();

        // Escape: 取消测验
        if (key == Qt::Key_Escape) {
            onCancel();
            return;
        }

        // 选择模式 + 未作答: 1/2/3/4 选择选项
        if (!m_answered && m_selectedMode != 0) {
            int optIdx = -1;
            if (key == Qt::Key_1) optIdx = 0;
            else if (key == Qt::Key_2) optIdx = 1;
            else if (key == Qt::Key_3) optIdx = 2;
            else if (key == Qt::Key_4) optIdx = 3;

            if (optIdx >= 0 && m_optBtns[optIdx]->isVisible()
                && m_optBtns[optIdx]->isEnabled()) {
                int realIdx = m_optBtns[optIdx]->property("optIndex").toInt();
                checkChoiceAnswer(realIdx);
                return;
            }
        }

        // 已作答: Enter/Space 下一题
        if (m_answered &&
            (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space)) {
            onSubmitOrNext();
            return;
        }
    } else if (page == 3) {
        // ---- 卡片翻阅页 ----
        int key = event->key();

        // Escape: 返回列表
        if (key == Qt::Key_Escape) {
            onBackToList();
            return;
        }

        if (!m_cardShowAnswer) {
            // 未显示答案: Enter/Space 显示答案
            if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
                onCardShowAnswer();
                return;
            }
        } else {
            // 已显示答案: Enter/Space/M 掌握, N 未掌握
            if (key == Qt::Key_Return || key == Qt::Key_Enter
                || key == Qt::Key_Space || key == Qt::Key_M) {
                onCardMastered();
                return;
            }
            if (key == Qt::Key_N) {
                onCardNotMastered();
                return;
            }
        }
    }

    QWidget::keyPressEvent(event);
}

// ================================================================
// 答题校验
// ================================================================

void WrongWordsWidget::checkSpellingAnswer()
{
    int idx = m_quizIndices[m_currentQ];
    std::string correctWord = m_wrongArr[idx].word;

    QString userInput = m_spellingInput->text().trimmed();
    if (userInput.isEmpty()) return;

    m_answered = true;
    m_spellingInput->setEnabled(false);

    QString correct = QString::fromStdString(correctWord);
    bool isCorrect = (userInput.compare(correct, Qt::CaseInsensitive) == 0);

    if (isCorrect) {
        m_correctCount++;
        m_feedbackLabel->setText("✓ 回答正确！");
        m_feedbackLabel->setStyleSheet("color: #606C38; font-size: 16px; font-weight: bold;");
        // 收集答对单词，测验结束时统一移除
        m_correctWords.push_back(correctWord);
    } else {
        m_feedbackLabel->setText(
            QString("✗ 回答错误，正确答案是：%1").arg(correct));
        m_feedbackLabel->setStyleSheet("color: #C66B3D; font-size: 16px; font-weight: bold;");
    }

    bool isLast = (m_currentQ + 1 >= static_cast<int>(m_quizIndices.size()));
    m_actionBtn->setText(isLast ? "查看结果" : "下一题");
    m_actionBtn->setVisible(true);
}

void WrongWordsWidget::checkChoiceAnswer(int clickedIdx)
{
    int wordIdx = m_quizIndices[m_currentQ];

    if (m_answered) return;
    m_answered = true;

    bool isCorrect = (clickedIdx == wordIdx);
    std::string correctWord = m_wrongArr[wordIdx].word;

    // 禁用所有选项按钮，标记正确/错误
    for (int i = 0; i < 4; i++) {
        m_optBtns[i]->setEnabled(false);
        int optIdx = m_optBtns[i]->property("optIndex").toInt();
        if (optIdx == wordIdx) {
            m_optBtns[i]->setStyleSheet(
                "background-color: #606C38; color: #FFFFFF; border-radius: 10px;");
        } else if (optIdx == clickedIdx) {
            m_optBtns[i]->setStyleSheet(
                "background-color: #C66B3D; color: #FFFFFF; border-radius: 10px;");
        }
    }

    if (isCorrect) {
        m_correctCount++;
        m_feedbackLabel->setText("✓ 回答正确！");
        m_feedbackLabel->setStyleSheet("color: #606C38; font-size: 16px; font-weight: bold;");
        // 收集答对单词，测验结束时统一移除
        m_correctWords.push_back(correctWord);
    } else {
        m_feedbackLabel->setText(
            QString("✗ 回答错误，正确答案是：%1")
                .arg(QString::fromStdString(correctWord)));
        m_feedbackLabel->setStyleSheet("color: #C66B3D; font-size: 16px; font-weight: bold;");
    }

    bool isLast = (m_currentQ + 1 >= static_cast<int>(m_quizIndices.size()));
    m_actionBtn->setText(isLast ? "查看结果" : "下一题");
    m_actionBtn->setVisible(true);
}

void WrongWordsWidget::showQuizResult()
{
    int total = static_cast<int>(m_quizIndices.size());

    // 记录测验历史（mode=3 表示生词本专项）
    saveQuizRecord(3, m_correctCount, total);

    // 测验完成：统一移除答对的生词
    for (const auto& w : m_correctWords) {
        removeWrongWord(w);
    }
    m_correctWords.clear();

    int pct = (total > 0) ? (m_correctCount * 100 / total) : 0;

    m_wrongArr.clear();
    m_quizIndices.clear();

    refreshList();

    // 全部答对：在空状态标签显示得分
    // 有错题留下：用消息框显示得分（列表仍可见）
    if (m_correctCount == total && total > 0) {
        m_emptyLabel->setText(
            QString("专项测验完成！得分：%1/%2（%3%）\n生词已全部掌握！")
                .arg(m_correctCount).arg(total).arg(pct));
    } else {
        QMessageBox::information(this, "测验结果",
            QString("专项测验完成！\n得分：%1/%2（%3%）")
                .arg(m_correctCount).arg(total).arg(pct));
    }

    m_pages->setCurrentIndex(0);
}
