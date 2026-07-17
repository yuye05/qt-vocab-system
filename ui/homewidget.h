#pragma once
#include <QWidget>
#include <vector>
#include "core/dictionary.h"

class QLabel;
class QWidget;

/* 自定义进度条：QPainter 绘制圆角矩形，不受 chunk 宽度限制 */
class POSBarWidget : public QWidget {
public:
    explicit POSBarWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    void setValue(int val, int maxVal, const QString& color) {
        m_val = val; m_max = maxVal; m_color = color; update();
    }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    int m_val = 0, m_max = 1;
    QString m_color;
};

class HomeWidget : public QWidget {
    Q_OBJECT
public:
    explicit HomeWidget(QWidget* parent = nullptr);

    void setStats(int wordCount, int wrongCount);
    void setPOSStats(const std::vector<POSStat>& stats);
    void setQuizHistory(const std::vector<QuizRecord>& records);

private:
    void setupUi();
    QLabel* m_wordCountLabel;
    QLabel* m_wrongCountLabel;
    QLabel* m_posTypeLabel;
    QWidget* m_posBarContainer;
    QWidget* m_quizHistoryContainer;  // 测验记录卡片容器
    QLabel* m_quizEmptyLabel;         // 空状态提示
    QLabel* m_quizSummaryLabel;       // 汇总行
};
