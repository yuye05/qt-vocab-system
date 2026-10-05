#include "wrongwordswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPainter>
#include <QKeyEvent>
#include <QMessageBox>
#include <QEvent>
#include <algorithm>
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

}

void WrongWordsWidget::setRoot(DictNode** rootPtr)
{
    m_dictRoot = rootPtr;
    refreshList();
}

// ================================================================
// 生词读取复用核心解析；卡片与测验只包含仍在词库中的词条。
// ================================================================

std::vector<WordEntry> WrongWordsWidget::validWrongWords()
{
    std::vector<WrongWord> records;
    std::vector<WordEntry> entries;
    if (!loadWrongWords(records)) return entries;
    if (!m_dictRoot) return entries;
    for (const auto& record : records) {
        if (const auto* node = searchWord(*m_dictRoot, record.word))
            entries.push_back({node->word, node->pos, node->meaning});
    }
    return entries;
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
    loadWrongWords(arr);
    const QString readError = QString::fromStdString(dataError());

    if (arr.empty()) {
        m_emptyLabel->setText(readError.isEmpty() ? "暂无生词，去做测验吧！" : readError);
        m_emptyLabel->show();
        m_listContainer->hide();
        m_sortBtn->hide();
        return;
    }

    m_emptyLabel->setText(readError);
    m_emptyLabel->setVisible(!readError.isEmpty());
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
                posText   = QString::fromUtf8(node->pos.c_str());
                meaningText = QString::fromUtf8(node->meaning.c_str());
            }
        }

        QString displayText = wordText;
        if (posText.isEmpty()) displayText += "（词库中已删除，需重新添加或移除此记录）";
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
    cancelBtn->installEventFilter(this);

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
    m_spellingInput->installEventFilter(this);

    // 选择题选项
    QHBoxLayout* optRow = new QHBoxLayout;
    optRow->setSpacing(12);
    for (int i = 0; i < 4; i++) {
        m_optBtns[i] = new QPushButton;
        m_optBtns[i]->setObjectName("quizOptionBtn");
        m_optBtns[i]->setMinimumHeight(52);
        optRow->addWidget(m_optBtns[i], 1);
        m_optBtns[i]->installEventFilter(this);
        connect(m_optBtns[i], &QPushButton::clicked, this, [this, i]() {
            checkChoiceAnswer(m_optBtns[i]->property("optIndex").toInt());
        });
    }

    // 反馈区域
    m_feedbackLabel = new QLabel;
    m_feedbackLabel->setObjectName("quizFeedback");
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    m_feedbackLabel->setWordWrap(true);
    m_feedbackLabel->setMinimumHeight(36);

    // 操作按钮
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_actionBtn = new QPushButton("提交");
    m_actionBtn->setObjectName("quizActionBtn");
    m_actionBtn->setFixedSize(140, 44);
    m_actionBtn->installEventFilter(this);
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
    backBtn->installEventFilter(this);

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
    m_cardActionBtn->installEventFilter(this);
    m_cardActionBtn->setObjectName("quizActionBtn");
    m_cardActionBtn->setFixedSize(160, 44);
    connect(m_cardActionBtn, &QPushButton::clicked, this, &WrongWordsWidget::onCardShowAnswer);

    // "已掌握"按钮
    m_cardMasteredBtn = new QPushButton("已掌握 ✓");
    m_cardMasteredBtn->installEventFilter(this);
    m_cardMasteredBtn->setObjectName("cardMasteredBtn");
    m_cardMasteredBtn->setFixedSize(140, 44);
    m_cardMasteredBtn->hide();
    connect(m_cardMasteredBtn, &QPushButton::clicked, this, &WrongWordsWidget::onCardMastered);

    // "未掌握"按钮
    m_cardNotMasteredBtn = new QPushButton("未掌握 ✗");
    m_cardNotMasteredBtn->installEventFilter(this);
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
    if (!removeWrongWord(word))
        QMessageBox::warning(this, "生词本未更新", QString::fromStdString(dataError()));
    refreshList();
}

void WrongWordsWidget::onEnterQuizSetup()
{
    m_wrongArr = validWrongWords();
    if (m_wrongArr.empty()) {
        QMessageBox::information(this, "没有可测验生词", dataError().empty()
            ? "生词已不在词库中或生词本为空，请先添加单词。"
            : QString::fromStdString(dataError()));
        return;
    }

    m_pages->setCurrentIndex(1);
}

void WrongWordsWidget::onEnterCardReview()
{
    m_cardArr.clear();
    m_cardArr = validWrongWords();
    if (m_cardArr.empty()) {
        QMessageBox::information(this, "没有可复习生词", dataError().empty()
            ? "生词已不在词库中或生词本为空，请先添加单词。"
            : QString::fromStdString(dataError()));
        return;
    }

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

    const auto& entry = m_cardArr[m_cardIdx];
    const QString meaningText = QString::fromStdString(entry.pos + " " + entry.meaning);

    m_cardMeaningLabel->setText(meaningText);
    m_cardMeaningLabel->setStyleSheet("font-size: 20px; color: #2C2416; font-weight: bold;");
    m_cardMeaningLabel->show();

    m_cardActionBtn->hide();
    m_cardMasteredBtn->show();
    m_cardNotMasteredBtn->show();
}

void WrongWordsWidget::onCardMastered()
{
    if (!m_cardShowAnswer || m_cardIdx >= static_cast<int>(m_cardArr.size())) return;
    if (!removeWrongWord(m_cardArr[m_cardIdx].word)) {
        QMessageBox::warning(this, "生词本未更新", QString::fromStdString(dataError()));
        return;
    }
    m_cardMastered++;
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
    m_wrongArr = validWrongWords();
    if (m_wrongArr.empty()) {
        QMessageBox::information(this, "没有可测验生词", dataError().empty()
            ? "生词已不在词库中或生词本为空，请先添加单词。"
            : QString::fromStdString(dataError()));
        return;
    }
    startQuiz();
}

// ================================================================
// 测验流程
// ================================================================

void WrongWordsWidget::startQuiz()
{
    m_optionWords = m_dictRoot ? wordSnapshot(*m_dictRoot) : std::vector<WordEntry>{};
    m_correctWords.clear();
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

    for (int i = 0; i < 4; i++) {
        m_optBtns[i]->setEnabled(true);
        m_optBtns[i]->setStyleSheet("");
    }

    m_actionBtn->setText("提交");
    m_actionBtn->setVisible(m_selectedMode == 0);

    const auto& entry = m_wrongArr[idx];
    const QString posStr = QString::fromStdString(entry.pos);
    const QString meaningStr = QString::fromStdString(entry.meaning);

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
    if (m_selectedMode == 0) m_spellingInput->setFocus();
    else m_optBtns[0]->setFocus();
}

void WrongWordsWidget::setupChoiceOptions(int wordIdx)
{
    const auto found = std::find_if(m_optionWords.begin(), m_optionWords.end(), [&](const WordEntry& word) {
        return word.word == m_wrongArr[wordIdx].word;
    });
    const auto options = pickOptions(static_cast<int>(found - m_optionWords.begin()), m_optionWords);
    const char* prefix[] = {"A. ", "B. ", "C. ", "D. "};
    for (int i = 0; i < 4; ++i) {
        const bool available = i < static_cast<int>(options.size());
        m_optBtns[i]->setVisible(available);
        m_optBtns[i]->setProperty("optIndex", available ? options[i] : -1);
        if (!available) continue;
        const auto& option = m_optionWords[options[i]];
        m_optBtns[i]->setText(QString(prefix[i]) + (m_selectedMode == 1
            ? QString::fromStdString(option.pos + " " + option.meaning)
            : QString::fromStdString(option.word)));
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

bool WrongWordsWidget::handleKey(QKeyEvent* event)
{
    const int page = m_pages->currentIndex();
    const int key = event->key();
    if (page == 2) {
        if (key == Qt::Key_Escape) {
            if (!event->isAutoRepeat()) onCancel();
            return true;
        }
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) &&
            (m_selectedMode == 0 || m_answered)) {
            if (!event->isAutoRepeat()) onSubmitOrNext();
            return true;
        }
        if (m_answered && key == Qt::Key_Space) {
            if (!event->isAutoRepeat()) onSubmitOrNext();
            return true;
        }
        if (!m_answered && m_selectedMode != 0 && key >= Qt::Key_1 && key <= Qt::Key_4) {
            const int option = key - Qt::Key_1;
            if (!event->isAutoRepeat() && m_optBtns[option]->isVisible() && m_optBtns[option]->isEnabled())
                checkChoiceAnswer(m_optBtns[option]->property("optIndex").toInt());
            return true;
        }
    } else if (page == 3) {
        if (key == Qt::Key_Escape) {
            if (!event->isAutoRepeat()) onBackToList();
            return true;
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space ||
            (m_cardShowAnswer && (key == Qt::Key_M || key == Qt::Key_N))) {
            if (!event->isAutoRepeat()) {
                if (!m_cardShowAnswer) onCardShowAnswer();
                else if (key == Qt::Key_N) onCardNotMastered();
                else onCardMastered();
            }
            return true;
        }
    }
    return false;
}

bool WrongWordsWidget::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::KeyPress && handleKey(static_cast<QKeyEvent*>(event))) return true;
    return QWidget::eventFilter(object, event);
}

void WrongWordsWidget::keyPressEvent(QKeyEvent* event)
{
    if (!handleKey(event)) QWidget::keyPressEvent(event);
}

// ================================================================
// 答题校验
// ================================================================

void WrongWordsWidget::checkSpellingAnswer()
{
    if (m_answered) return;
    int idx = m_quizIndices[m_currentQ];
    std::string correctWord = m_wrongArr[idx].word;

    QString userInput = m_spellingInput->text().trimmed();
    if (userInput.isEmpty()) return;

    m_answered = true;
    m_spellingInput->setEnabled(false);

    QString correct = QString::fromStdString(correctWord);
    bool isCorrect = spellingMatches(m_optionWords, m_wrongArr[idx], userInput.toStdString());

    if (isCorrect) {
        m_correctCount++;
        m_feedbackLabel->setText(QString("✓ 回答正确！本题目标词：%1").arg(QString::fromStdString(correctWord)));
        m_feedbackLabel->setStyleSheet("color: #606C38; font-size: 16px; font-weight: bold;");
        // 收集答对单词，测验结束时统一移除
        m_correctWords.push_back(correctWord);
    } else {
        m_feedbackLabel->setText(
            QString("✗ 回答错误，正确答案是：%1").arg(correct));
        m_feedbackLabel->setStyleSheet("color: #C66B3D; font-size: 16px; font-weight: bold;");
        if (m_dictRoot && searchWord(*m_dictRoot, correctWord) && !recordWrong(correctWord))
            QMessageBox::warning(this, "生词本未保存", QString::fromStdString(dataError()));
    }

    bool isLast = (m_currentQ + 1 >= static_cast<int>(m_quizIndices.size()));
    m_actionBtn->setText(isLast ? "查看结果" : "下一题");
    m_actionBtn->setVisible(true);
    m_actionBtn->setFocus();
}

void WrongWordsWidget::checkChoiceAnswer(int clickedIdx)
{
    if (clickedIdx < 0 || clickedIdx >= static_cast<int>(m_optionWords.size()) || m_answered) return;
    int wordIdx = m_quizIndices[m_currentQ];

    m_answered = true;

    bool isCorrect = (m_optionWords[clickedIdx].word == m_wrongArr[wordIdx].word);
    std::string correctWord = m_wrongArr[wordIdx].word;

    // 禁用所有选项按钮，标记正确/错误
    for (int i = 0; i < 4; i++) {
        m_optBtns[i]->setEnabled(false);
        int optIdx = m_optBtns[i]->property("optIndex").toInt();
        if (optIdx >= 0 && m_optionWords[optIdx].word == correctWord) {
            m_optBtns[i]->setStyleSheet(
                "background-color: #606C38; color: #FFFFFF; border-radius: 10px;");
        } else if (optIdx == clickedIdx) {
            m_optBtns[i]->setStyleSheet(
                "background-color: #C66B3D; color: #FFFFFF; border-radius: 10px;");
        }
    }

    if (isCorrect) {
        m_correctCount++;
        m_feedbackLabel->setText(QString("✓ 回答正确！本题目标词：%1").arg(QString::fromStdString(correctWord)));
        m_feedbackLabel->setStyleSheet("color: #606C38; font-size: 16px; font-weight: bold;");
        // 收集答对单词，测验结束时统一移除
        m_correctWords.push_back(correctWord);
    } else {
        m_feedbackLabel->setText(
            QString("✗ 回答错误，正确答案是：%1")
                .arg(QString::fromStdString(correctWord)));
        m_feedbackLabel->setStyleSheet("color: #C66B3D; font-size: 16px; font-weight: bold;");
        if (m_dictRoot && searchWord(*m_dictRoot, correctWord) && !recordWrong(correctWord))
            QMessageBox::warning(this, "生词本未保存", QString::fromStdString(dataError()));
    }

    bool isLast = (m_currentQ + 1 >= static_cast<int>(m_quizIndices.size()));
    m_actionBtn->setText(isLast ? "查看结果" : "下一题");
    m_actionBtn->setVisible(true);
    m_actionBtn->setFocus();
}


void WrongWordsWidget::showQuizResult()
{
    const int total = static_cast<int>(m_quizIndices.size());
    QString warning;
    if (!saveQuizRecord(3, m_correctCount, total)) warning = QString::fromStdString(dataError());
    if (!removeWrongWords(m_correctWords)) warning += "\n" + QString::fromStdString(dataError());
    m_correctWords.clear();
    m_wrongArr.clear();
    m_optionWords.clear();
    m_quizIndices.clear();
    const int remaining = countWrongWords();
    refreshList();
    m_emptyLabel->setText(QString("专项测验完成！得分：%1/%2（%3%）\n生词本剩余 %4 个单词。")
        .arg(m_correctCount).arg(total).arg(total ? m_correctCount * 100 / total : 0).arg(remaining) +
        (warning.isEmpty() ? QString() : "\n未能保存全部记录：" + warning));
    m_emptyLabel->show();
    m_pages->setCurrentIndex(0);
}
