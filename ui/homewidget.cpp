#include "homewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>

/* ================================================================
   POSBarWidget：自定义圆角进度条
   用 QPainter 直接绘制，不受 Qt QProgressBar::chunk 宽度限制
   ================================================================ */
void POSBarWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);  // 抗锯齿偏移
    qreal radius = r.height() / 2.0;

    // 背景槽
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#D9CEBB"));
    p.drawRoundedRect(r, radius, radius);

    // 填充块（按 value/max 比例）
    if (m_max > 0 && m_val > 0) {
        qreal fillRatio = static_cast<qreal>(m_val) / m_max;
        QRectF fillR = r;
        fillR.setWidth(r.width() * fillRatio);
        // 确保填充块至少有 2px 宽，避免极小值不可见
        if (fillR.width() < 2) fillR.setWidth(2);
        p.setBrush(QColor(m_color));
        p.drawRoundedRect(fillR, radius, radius);
    }
}

/* ================================================================
   HomeWidget 主实现
   ================================================================ */

HomeWidget::HomeWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void HomeWidget::setupUi()
{
    // 主布局：单个 QScrollArea 撑满
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #D9CEBB; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #C66B3D; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");

    // 内容容器：承载所有卡片
    QWidget* content = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    // 页面标题
    QLabel* title = new QLabel("欢迎使用词汇学习系统");
    title->setObjectName("pageTitle");
    title->setAlignment(Qt::AlignLeft);

    // 统计卡片（3 列）
    QFrame* statsCard = new QFrame;
    statsCard->setObjectName("statsCard");
    QHBoxLayout* statsLayout = new QHBoxLayout(statsCard);
    statsLayout->setContentsMargins(20, 18, 20, 18);
    statsLayout->setSpacing(16);

    auto makeStatBlock = [](const QString& num, const QString& label) {
        QWidget* w = new QWidget;
        QVBoxLayout* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(4);
        QLabel* n = new QLabel(num);
        n->setObjectName("statNumber");
        n->setAlignment(Qt::AlignCenter);
        QLabel* l = new QLabel(label);
        l->setObjectName("statLabel");
        l->setAlignment(Qt::AlignCenter);
        vl->addWidget(n);
        vl->addWidget(l);
        return w;
    };

    QWidget* wordBlock = makeStatBlock("0", "词库单词");
    m_wordCountLabel = wordBlock->findChild<QLabel*>("statNumber");

    QWidget* wrongBlock = makeStatBlock("0", "生词本");
    m_wrongCountLabel = wrongBlock->findChild<QLabel*>("statNumber");

    QWidget* posBlock = makeStatBlock("0", "词性种类");
    m_posTypeLabel = posBlock->findChild<QLabel*>("statNumber");

    statsLayout->addWidget(wordBlock);
    statsLayout->addWidget(wrongBlock);
    statsLayout->addWidget(posBlock);

    // 词性分布卡片
    QFrame* posCard = new QFrame;
    posCard->setObjectName("posCard");
    QVBoxLayout* posLayout = new QVBoxLayout(posCard);
    posLayout->setContentsMargins(20, 16, 20, 16);

    QLabel* posTitle = new QLabel("词性分布");
    posTitle->setObjectName("cardTitle");
    m_posBarContainer = new QWidget;
    m_posBarContainer->setObjectName("posBarContainer");
    QVBoxLayout* barLayout = new QVBoxLayout(m_posBarContainer);
    barLayout->setContentsMargins(0, 8, 0, 0);
    barLayout->setSpacing(6);

    posLayout->addWidget(posTitle);
    posLayout->addWidget(m_posBarContainer);

    // 测验记录卡片
    QFrame* histCard = new QFrame;
    histCard->setObjectName("quizHistoryCard");
    QVBoxLayout* histLayout = new QVBoxLayout(histCard);
    histLayout->setContentsMargins(20, 16, 20, 16);
    histLayout->setSpacing(8);

    QLabel* histTitle = new QLabel("近期测验");
    histTitle->setObjectName("cardTitle");

    // 空状态提示
    m_quizEmptyLabel = new QLabel("暂无测验记录，去测验页开始你的第一次测验吧！");
    m_quizEmptyLabel->setObjectName("placeholder");
    m_quizEmptyLabel->setAlignment(Qt::AlignCenter);
    m_quizEmptyLabel->setWordWrap(true);
    m_quizEmptyLabel->hide();

    // 记录行容器
    m_quizHistoryContainer = new QWidget;
    m_quizHistoryContainer->setObjectName("quizHistoryContainer");
    m_quizHistoryContainer->hide();  // 初始隐藏，setQuizHistory 时按需显示
    QVBoxLayout* rowLay = new QVBoxLayout(m_quizHistoryContainer);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(6);

    // 分隔线
    QFrame* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { color: #D9CEBB; }");
    sep->setFixedHeight(1);

    // 汇总行
    m_quizSummaryLabel = new QLabel;
    m_quizSummaryLabel->setObjectName("quizHistorySummary");
    m_quizSummaryLabel->setAlignment(Qt::AlignCenter);
    m_quizSummaryLabel->hide();  // 初始隐藏

    histLayout->addWidget(histTitle);
    histLayout->addWidget(m_quizEmptyLabel);
    histLayout->addWidget(m_quizHistoryContainer);
    histLayout->addWidget(sep);
    histLayout->addWidget(m_quizSummaryLabel);

    // 快速入门提示卡片
    QFrame* hintCard = new QFrame;
    hintCard->setObjectName("hintCard");
    QVBoxLayout* hintLayout = new QVBoxLayout(hintCard);
    hintLayout->setContentsMargins(20, 16, 20, 16);
    hintLayout->setSpacing(10);

    QLabel* hintTitle = new QLabel("快速入门");
    hintTitle->setObjectName("cardTitle");
    QLabel* hintDesc = new QLabel(
        "📚  词库管理 — 添加、查找、管理你的单词本\n"
        "✏️  单词测验 — 拼写模式和选择模式测验\n"
        "📝  生词本 — 自动收集错题，针对性复习");
    hintDesc->setObjectName("hintDesc");
    hintDesc->setWordWrap(true);
    hintLayout->addWidget(hintTitle);
    hintLayout->addWidget(hintDesc);

    layout->addWidget(title);
    layout->addWidget(statsCard);
    layout->addWidget(posCard);
    layout->addWidget(histCard);
    layout->addWidget(hintCard);
    layout->addStretch();

    // 组装滚动区域
    scrollArea->setWidget(content);
    rootLayout->addWidget(scrollArea);
}

void HomeWidget::setStats(int wordCount, int wrongCount)
{
    m_wordCountLabel->setText(QString::number(wordCount));
    m_wrongCountLabel->setText(QString::number(wrongCount));
}

void HomeWidget::setPOSStats(const std::vector<POSStat>& stats)
{
    m_posTypeLabel->setText(QString::number(static_cast<int>(stats.size())));

    // 清空旧条目
    QLayout* barLayout = m_posBarContainer->layout();
    if (barLayout) {
        QLayoutItem* item;
        while ((item = barLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    if (stats.empty()) return;

    int maxCount = stats[0].count;

    // 给 bar 的 range 上限加 20% 余量，让占比最大的名词也不占满整行
    int barMax = static_cast<int>(maxCount * 1.2 + 1);

    int showCount = std::min(static_cast<int>(stats.size()), 6);

    // Organic 锚点：每种词性映射一种大地色调
    static const char* colors[] = {
        "#8B9D83",  // 鼠尾草绿
        "#C66B3D",  // 赤陶色
        "#C08E3A",  // 赭石色
        "#B08B6E",  // 黏土色
        "#7A8E6E",  // 苔藓绿
        "#D4956A"   // 暖沙色
    };

    for (int i = 0; i < showCount; i++) {
        QWidget* row = new QWidget;
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        QLabel* posLabel = new QLabel(QString::fromLocal8Bit(stats[i].pos.c_str()));
        posLabel->setObjectName("posNameLabel");
        posLabel->setFixedWidth(50);

        POSBarWidget* bar = new POSBarWidget;
        bar->setValue(stats[i].count, barMax, QString(colors[i % 6]));
        bar->setFixedHeight(18);
        bar->setMinimumWidth(20);

        QLabel* countLabel = new QLabel(QString::number(stats[i].count));
        countLabel->setObjectName("posCountLabel");
        countLabel->setFixedWidth(36);
        countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        rowLayout->addWidget(posLabel);
        rowLayout->addWidget(bar, 1);
        rowLayout->addWidget(countLabel);

        barLayout->addWidget(row);
    }
}

void HomeWidget::setQuizHistory(const std::vector<QuizRecord>& records)
{
    // 清空旧记录行
    QVBoxLayout* rowLay = qobject_cast<QVBoxLayout*>(m_quizHistoryContainer->layout());
    if (rowLay) {
        QLayoutItem* item;
        while ((item = rowLay->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
    }

    if (records.empty()) {
        m_quizEmptyLabel->show();
        m_quizHistoryContainer->hide();
        m_quizSummaryLabel->hide();
        return;
    }

    m_quizEmptyLabel->hide();
    m_quizHistoryContainer->show();
    m_quizSummaryLabel->show();

    // 模式名称映射
    static const char* modeNames[] = {"拼写模式", "英→中", "中→英", "生词本"};

    // 显示最近 5 条
    int showCount = std::min(static_cast<int>(records.size()), 5);

    for (int i = 0; i < showCount; i++) {
        const QuizRecord& r = records[i];

        QWidget* row = new QWidget;
        QHBoxLayout* hRow = new QHBoxLayout(row);
        hRow->setContentsMargins(0, 0, 0, 0);
        hRow->setSpacing(10);

        // 模式名称
        const char* modeStr = (r.mode >= 0 && r.mode <= 3) ? modeNames[r.mode] : "未知";
        QLabel* modeLabel = new QLabel(QString::fromUtf8(modeStr));
        modeLabel->setObjectName("quizHistoryMode");
        modeLabel->setFixedWidth(72);

        // 进度条（统一鼠尾草绿）
        POSBarWidget* bar = new POSBarWidget;
        bar->setValue(r.correct, r.total, "#8B9D83");
        bar->setFixedHeight(12);
        bar->setMinimumWidth(20);

        // 得分
        QLabel* scoreLabel = new QLabel(
            QString("%1/%2").arg(r.correct).arg(r.total));
        scoreLabel->setObjectName("quizHistoryScore");
        scoreLabel->setFixedWidth(48);
        scoreLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // 正确率
        int pct = (r.total > 0) ? (r.correct * 100 / r.total) : 0;
        QLabel* pctLabel = new QLabel(QString("%1%").arg(pct));
        pctLabel->setObjectName("quizHistoryPct");
        pctLabel->setFixedWidth(42);
        pctLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        hRow->addWidget(modeLabel);
        hRow->addWidget(bar, 1);
        hRow->addWidget(scoreLabel);
        hRow->addWidget(pctLabel);

        rowLay->addWidget(row);
    }

    // 汇总行：总次数 + 平均正确率
    int totalQuizzes = static_cast<int>(records.size());
    int sumCorrect = 0, sumTotal = 0;
    for (const auto& r : records) {
        sumCorrect += r.correct;
        sumTotal += r.total;
    }
    int avgPct = (sumTotal > 0) ? (sumCorrect * 100 / sumTotal) : 0;
    m_quizSummaryLabel->setText(
        QString("共 %1 次测验  |  平均正确率 %2%").arg(totalQuizzes).arg(avgPct));
}
