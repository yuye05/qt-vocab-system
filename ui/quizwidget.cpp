#include "quizwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPainter>
#include <QKeyEvent>
#include <QMessageBox>
#include <QEvent>
#include <cstdlib>
#include <string>

// ================================================================
// ProgressWidget：圆角进度条（QPainter 自绘）
// ================================================================

void ProgressWidget::paintEvent(QPaintEvent*)
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
        p.setBrush(QColor("#8B9D83"));  // 鼠尾草绿
        p.drawRoundedRect(fillR, radius, radius);
    }
}

// ================================================================
// QuizWidget
// ================================================================

QuizWidget::QuizWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(0);

    QLabel* title = new QLabel("单词测验");
    title->setObjectName("pageTitle");

    m_pages = new QStackedWidget;
    m_pages->setObjectName("quizPageStack");

    buildSetupPage();
    buildQuizPage();
    buildResultPage();

    mainLayout->addWidget(title);
    mainLayout->addWidget(m_pages, 1);
}

void QuizWidget::setRoot(DictNode** rootPtr)
{
    m_dictRoot = rootPtr;
}

// ================================================================
// 设置页
// ================================================================

void QuizWidget::buildSetupPage()
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
            this, &QuizWidget::onModeBtn);

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
            this, &QuizWidget::onCountBtn);

    // ---- 开始按钮 ----
    QHBoxLayout* startRow = new QHBoxLayout;
    startRow->addStretch();
    QPushButton* startBtn = new QPushButton("开始测验");
    startBtn->setObjectName("startQuizBtn");
    startBtn->setFixedSize(180, 48);
    connect(startBtn, &QPushButton::clicked, this, &QuizWidget::onStartQuiz);
    startRow->addWidget(startBtn);
    startRow->addStretch();

    lay->addWidget(modeTitle);
    lay->addLayout(modeRow);
    lay->addWidget(countTitle);
    lay->addLayout(countRow);
    lay->addStretch();
    lay->addLayout(startRow);
    lay->addStretch();

    m_pages->addWidget(page);  // index 0
}

// ================================================================
// 答题页
// ================================================================

void QuizWidget::buildQuizPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(12);

    // ---- 顶栏：进度 + 取消 ----
    QHBoxLayout* topBar = new QHBoxLayout;

    m_progressLabel = new QLabel("题目 1/10");
    m_progressLabel->setObjectName("quizProgressLabel");

    ProgressWidget* pbar = new ProgressWidget;
    pbar->setObjectName("quizProgressBar");
    pbar->setFixedHeight(8);
    m_progressFill = pbar;

    topBar->addWidget(m_progressLabel);
    topBar->addStretch();

    QPushButton* cancelBtn = new QPushButton("取消");
    cancelBtn->setObjectName("cancelQuizBtn");
    cancelBtn->setFixedSize(72, 32);
    connect(cancelBtn, &QPushButton::clicked, this, &QuizWidget::onCancel);
    topBar->addWidget(cancelBtn);
    cancelBtn->installEventFilter(this);

    // ---- 题目区域 ----
    m_questionLabel = new QLabel;
    m_questionLabel->setObjectName("quizQuestion");
    m_questionLabel->setAlignment(Qt::AlignCenter);
    m_questionLabel->setMinimumHeight(80);

    // ---- 拼写输入 ----
    m_spellingInput = new QLineEdit;
    m_spellingInput->setObjectName("quizSpellingInput");
    m_spellingInput->setPlaceholderText("请输入英文单词...");
    m_spellingInput->setAlignment(Qt::AlignCenter);
    m_spellingInput->setFixedHeight(48);
    m_spellingInput->installEventFilter(this);

    // ---- 选择题选项（4 个） ----
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

    // ---- 反馈区域 ----
    m_feedbackLabel = new QLabel;
    m_feedbackLabel->setObjectName("quizFeedback");
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    m_feedbackLabel->setWordWrap(true);
    m_feedbackLabel->setMinimumHeight(36);

    // ---- 操作按钮 ----
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_actionBtn = new QPushButton("提交");
    m_actionBtn->setObjectName("quizActionBtn");
    m_actionBtn->setFixedSize(140, 44);
    m_actionBtn->installEventFilter(this);
    connect(m_actionBtn, &QPushButton::clicked, this, &QuizWidget::onSubmitOrNext);
    btnRow->addWidget(m_actionBtn);
    btnRow->addStretch();

    // 组装
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

    m_pages->addWidget(page);  // index 1
}

// ================================================================
// 结果页
// ================================================================

void QuizWidget::buildResultPage()
{
    QWidget* page = new QWidget;
    QVBoxLayout* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 16, 0, 0);
    lay->setSpacing(16);

    QLabel* doneTitle = new QLabel("测验完成！");
    doneTitle->setObjectName("quizDoneTitle");
    doneTitle->setAlignment(Qt::AlignCenter);

    m_scoreLabel = new QLabel;
    m_scoreLabel->setObjectName("quizScoreLabel");
    m_scoreLabel->setAlignment(Qt::AlignCenter);

    // 错题列表（可滚动）
    QScrollArea* scroll = new QScrollArea;
    scroll->setObjectName("quizWrongScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_wrongContainer = new QWidget;
    m_wrongContainer->setObjectName("quizWrongContainer");
    new QVBoxLayout(m_wrongContainer);
    scroll->setWidget(m_wrongContainer);

    // 按钮行
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(16);
    btnRow->addStretch();

    m_wrongRetryBtn = new QPushButton("错题再练");
    m_wrongRetryBtn->setObjectName("quizWrongRetryBtn");
    m_wrongRetryBtn->setFixedSize(140, 44);
    m_wrongRetryBtn->hide();
    m_wrongRetryBtn->installEventFilter(this);
    connect(m_wrongRetryBtn, &QPushButton::clicked, this, &QuizWidget::onRetryWrong);

    QPushButton* retryBtn = new QPushButton("再来一轮");
    retryBtn->setObjectName("quizRetryBtn");
    retryBtn->setFixedSize(140, 44);
    connect(retryBtn, &QPushButton::clicked, this, &QuizWidget::onCancel);

    QPushButton* backBtn = new QPushButton("返回设置");
    backBtn->setObjectName("quizBackBtn");
    backBtn->setFixedSize(140, 44);
    connect(backBtn, &QPushButton::clicked, this, &QuizWidget::onCancel);

    btnRow->addWidget(m_wrongRetryBtn);
    btnRow->addWidget(retryBtn);
    btnRow->addWidget(backBtn);
    btnRow->addStretch();

    lay->addWidget(doneTitle);
    lay->addWidget(m_scoreLabel);
    lay->addWidget(scroll, 1);
    lay->addLayout(btnRow);

    m_pages->addWidget(page);  // index 2
}

// ================================================================
// 槽函数
// ================================================================

void QuizWidget::onModeBtn(int id)
{
    m_selectedMode = id;
}

void QuizWidget::onCountBtn(int id)
{
    static const int counts[] = {5, 10, 15, 20};
    m_selectedCount = counts[id];
}

void QuizWidget::onStartQuiz()
{
    if (!m_dictRoot || !*m_dictRoot) {
        QMessageBox::information(this, "词库为空", "请先在词库管理中添加单词。");
        return;
    }
    startQuiz();
}

void QuizWidget::onRetryWrong()
{
    if (m_pages->currentIndex() != 2 || m_wrongList.empty()) return;
    startQuiz(true);
}

void QuizWidget::onSubmitOrNext()
{
    if (m_answered) {
        // 已经答过 → 下一题 / 查看结果
        m_currentQ++;
        if (m_currentQ >= static_cast<int>(m_quizIndices.size())) {
            showResult();
        } else {
            showQuestion();
        }
    } else {
        // 尚未答 → 提交答案
        if (m_selectedMode == 0) {
            checkSpellingAnswer();
        }
    }
}

void QuizWidget::onCancel()
{
    // 取消当前测验；已经提交的错题保留在生词本中。
    m_words.clear();
    m_dictTotal = 0;
    m_currentQ = 0;
    m_correctCount = 0;
    m_answered = false;
    m_quizIndices.clear();
    m_wrongList.clear();
    m_pages->setCurrentIndex(0);
}

// ================================================================
// 键盘快捷键
// ================================================================

bool QuizWidget::handleKey(QKeyEvent* event)
{
    const int key = event->key();
    const int page = m_pages->currentIndex();
    if (page == 1) {
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
    } else if (page == 2 && (key == Qt::Key_Escape || key == Qt::Key_Return ||
                            key == Qt::Key_Enter || key == Qt::Key_Space)) {
        if (!event->isAutoRepeat()) {
            if (key != Qt::Key_Escape && m_wrongRetryBtn->isVisible() && m_wrongRetryBtn->hasFocus())
                onRetryWrong();
            else onCancel();
        }
        return true;
    }
    return false;
}

bool QuizWidget::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::KeyPress && handleKey(static_cast<QKeyEvent*>(event))) return true;
    return QWidget::eventFilter(object, event);
}

void QuizWidget::keyPressEvent(QKeyEvent* event)
{
    if (!handleKey(event)) QWidget::keyPressEvent(event);
}

// ================================================================
// 测验流程
// ================================================================

void QuizWidget::startQuiz(bool wrongOnly)
{
    if (wrongOnly) {
        // 使用本轮完整快照出选项，只把错题对应的索引作为新的题目。
        m_quizIndices.clear();
        for (int i : shuffledIndices(static_cast<int>(m_wrongList.size()))) {
            const auto found = std::find_if(m_words.begin(), m_words.end(), [&](const WordEntry& word) {
                return word.word == m_wrongList[i].word;
            });
            if (found != m_words.end())
                m_quizIndices.push_back(static_cast<int>(found - m_words.begin()));
        }
    } else {
        m_words = wordSnapshot(*m_dictRoot);
        m_dictTotal = static_cast<int>(m_words.size());
        m_quizIndices = shuffledIndices(m_dictTotal);
        m_quizIndices.resize(std::min(m_selectedCount, m_dictTotal));
    }
    if (m_quizIndices.empty()) return;

    // 重置状态
    m_currentQ     = 0;
    m_correctCount = 0;
    m_answered     = false;
    m_wrongList.clear();

    // 切到答题页，显示第一题
    m_pages->setCurrentIndex(1);
    showQuestion();
}

void QuizWidget::showQuestion()
{
    m_answered = false;

    int total = static_cast<int>(m_quizIndices.size());
    int wordIdx = m_quizIndices[m_currentQ];
    const WordEntry* node = &m_words[wordIdx];

    // 进度显示
    m_progressLabel->setText(
        QString("题目 %1/%2").arg(m_currentQ + 1).arg(total));
    static_cast<ProgressWidget*>(m_progressFill)->setValue(m_currentQ, total);

    // 清空反馈
    m_feedbackLabel->setText("");
    m_feedbackLabel->setStyleSheet("");

    // 重置按钮状态
    m_spellingInput->clear();
    m_spellingInput->setEnabled(true);

    for (int i = 0; i < 4; i++) {
        m_optBtns[i]->setEnabled(true);
        m_optBtns[i]->setStyleSheet("");
    }

    m_actionBtn->setText("提交");
    m_actionBtn->setVisible(m_selectedMode == 0);  // 只有拼写模式显示

    if (m_selectedMode == 0) {
        // ---- 拼写模式：显示中文，输入英文 ----
        m_questionLabel->setText(
            QString::fromUtf8(node->pos.c_str()) + " " +
            QString::fromUtf8(node->meaning.c_str()));
        m_questionLabel->setStyleSheet("font-size: 24px; color: #2C2416; font-weight: bold;");

        m_spellingInput->setVisible(true);
        for (int i = 0; i < 4; i++) m_optBtns[i]->setVisible(false);

    } else if (m_selectedMode == 1) {
        // ---- 英→中：显示英文，选中文释义 ----
        m_questionLabel->setText(QString::fromStdString(node->word));
        m_questionLabel->setStyleSheet("font-size: 28px; color: #C66B3D; font-weight: bold;");

        m_spellingInput->setVisible(false);
        for (int i = 0; i < 4; i++) m_optBtns[i]->setVisible(true);

        setupChoiceOptions(wordIdx);

    } else {
        // ---- 中→英：显示中文，选英文单词 ----
        m_questionLabel->setText(
            QString::fromUtf8(node->pos.c_str()) + " " +
            QString::fromUtf8(node->meaning.c_str()));
        m_questionLabel->setStyleSheet("font-size: 24px; color: #2C2416; font-weight: bold;");

        m_spellingInput->setVisible(false);
        for (int i = 0; i < 4; i++) m_optBtns[i]->setVisible(true);

        setupChoiceOptions(wordIdx);
    }
    if (m_selectedMode == 0) m_spellingInput->setFocus();
    else m_optBtns[0]->setFocus();
}

void QuizWidget::setupChoiceOptions(int wordIdx)
{
    // 根据模式决定选项显示内容：英→中显示释义，中→英显示单词
    bool showMeaning = (m_selectedMode == 1);

    const auto opts = pickOptions(wordIdx, m_words);
    const char* prefix[] = {"A. ", "B. ", "C. ", "D. "};
    for (int i = 0; i < 4; ++i) {
        const bool available = i < static_cast<int>(opts.size());
        m_optBtns[i]->setVisible(available);
        m_optBtns[i]->setProperty("optIndex", available ? opts[i] : -1);
        if (!available) continue;
        const auto& option = m_words[opts[i]];
        m_optBtns[i]->setText(QString(prefix[i]) + (showMeaning
            ? QString::fromStdString(option.pos + " " + option.meaning)
            : QString::fromStdString(option.word)));
    }
}

void QuizWidget::checkSpellingAnswer()
{
    if (m_answered) return;
    int wordIdx = m_quizIndices[m_currentQ];
    const WordEntry* node = &m_words[wordIdx];

    QString userInput = m_spellingInput->text().trimmed();
    if (userInput.isEmpty()) return;

    m_answered = true;
    m_spellingInput->setEnabled(false);

    QString correct = QString::fromStdString(node->word);
    bool isCorrect = spellingMatches(m_words, *node, userInput.toStdString());

    if (isCorrect) {
        m_correctCount++;
        m_feedbackLabel->setText(QString("✓ 回答正确！本题目标词：%1").arg(QString::fromStdString(node->word)));
        m_feedbackLabel->setStyleSheet("color: #606C38; font-size: 16px; font-weight: bold;");
    } else {
        m_feedbackLabel->setText(
            QString("✗ 回答错误，正确答案是：%1").arg(correct));
        m_feedbackLabel->setStyleSheet("color: #C66B3D; font-size: 16px; font-weight: bold;");

        WrongAnsInfo info;
        info.word           = node->word;
        info.pos            = node->pos;
        info.correctMeaning = node->meaning;
        info.userAnswer     = userInput.toStdString();
        m_wrongList.push_back(info);

        if (m_dictRoot && searchWord(*m_dictRoot, node->word) && !recordWrong(node->word))
            QMessageBox::warning(this, "生词本未保存", QString::fromStdString(dataError()));
    }

    bool isLast = (m_currentQ + 1 >= static_cast<int>(m_quizIndices.size()));
    m_actionBtn->setText(isLast ? "查看结果" : "下一题");
    m_actionBtn->setVisible(true);
    m_actionBtn->setFocus();
}

void QuizWidget::checkChoiceAnswer(int clickedIdx)
{
    if (clickedIdx < 0 || clickedIdx >= static_cast<int>(m_words.size()) || m_answered) return;
    int wordIdx = m_quizIndices[m_currentQ];
    const WordEntry* node = &m_words[wordIdx];

    m_answered = true;

    bool isCorrect = (clickedIdx == wordIdx);

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
        m_feedbackLabel->setText(QString("✓ 回答正确！本题目标词：%1").arg(QString::fromStdString(node->word)));
        m_feedbackLabel->setStyleSheet("color: #606C38; font-size: 16px; font-weight: bold;");
    } else {
        m_feedbackLabel->setText(
            QString("✗ 回答错误，正确答案是：%1").arg(
                QString::fromStdString(node->word)));
        m_feedbackLabel->setStyleSheet("color: #C66B3D; font-size: 16px; font-weight: bold;");

        WrongAnsInfo info;
        info.word           = node->word;
        info.pos            = node->pos;
        info.correctMeaning = node->meaning;
        info.userAnswer     = m_selectedMode == 1 ? m_words[clickedIdx].meaning : m_words[clickedIdx].word;
        m_wrongList.push_back(info);

        if (m_dictRoot && searchWord(*m_dictRoot, node->word) && !recordWrong(node->word))
            QMessageBox::warning(this, "生词本未保存", QString::fromStdString(dataError()));
    }

    bool isLast = (m_currentQ + 1 >= static_cast<int>(m_quizIndices.size()));
    m_actionBtn->setText(isLast ? "查看结果" : "下一题");
    m_actionBtn->setVisible(true);
    m_actionBtn->setFocus();
}

void QuizWidget::showResult()
{
    int total = static_cast<int>(m_quizIndices.size());

    // 记录测验历史
    if (!saveQuizRecord(m_selectedMode, m_correctCount, total))
        QMessageBox::warning(this, "历史未保存", QString::fromStdString(dataError()));

    // 有错题时保留完整词库快照，供再练出题及生成干扰项。
    m_wrongRetryBtn->setVisible(!m_wrongList.empty());
    if (m_wrongList.empty()) {
        m_words.clear();
        m_dictTotal = 0;
    }

    int pct = (total > 0) ? (m_correctCount * 100 / total) : 0;

    m_scoreLabel->setText(
        QString("得分：%1 / %2\n正确率：%3%")
            .arg(m_correctCount).arg(total).arg(pct));

    // 清空旧的错题条目
    QVBoxLayout* wrongLay = qobject_cast<QVBoxLayout*>(m_wrongContainer->layout());
    if (wrongLay) {
        QLayoutItem* item;
        while ((item = wrongLay->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    if (m_wrongList.empty()) {
        QLabel* perfect = new QLabel("太棒了！全部答对！");
        perfect->setObjectName("quizPerfectLabel");
        perfect->setAlignment(Qt::AlignCenter);
        wrongLay->addWidget(perfect);
    } else {
        QLabel* wrongTitle = new QLabel("── 错题回顾 ──");
        wrongTitle->setObjectName("quizWrongTitle");
        wrongTitle->setAlignment(Qt::AlignCenter);
        wrongLay->addWidget(wrongTitle);

        for (const auto& w : m_wrongList) {
            QLabel* lbl = new QLabel(
                QString("✗ %1  %2%3  你的答案：%4")
                    .arg(QString::fromStdString(w.word))
                    .arg(QString::fromUtf8(w.pos.c_str()))
                    .arg(QString::fromUtf8(w.correctMeaning.c_str()))
                    .arg(QString::fromStdString(w.userAnswer)));
            lbl->setObjectName("quizWrongItem");
            lbl->setWordWrap(true);
            wrongLay->addWidget(lbl);
        }
    }

    m_pages->setCurrentIndex(2);
}
