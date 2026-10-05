#pragma once
#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QButtonGroup>
#include <vector>
#include "core/dictionary.h"

class QuizWidget : public QWidget {
    Q_OBJECT
public:
    explicit QuizWidget(QWidget* parent = nullptr);
    void setRoot(DictNode** rootPtr);
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onModeBtn(int id);
    void onCountBtn(int id);
    void onStartQuiz();
    void onRetryWrong();
    void onSubmitOrNext();
    void onCancel();

private:
    void buildSetupPage();
    void buildQuizPage();
    void buildResultPage();
    void startQuiz(bool wrongOnly = false);
    void showQuestion();
    void checkSpellingAnswer();
    void checkChoiceAnswer(int clickedOptIdx);
    void setupChoiceOptions(int wordIdx);
    void showResult();
    bool handleKey(QKeyEvent* event, QObject* source = nullptr);
    bool eventFilter(QObject* object, QEvent* event) override;

    // 外部数据
    DictNode**  m_dictRoot = nullptr;
    std::vector<WordEntry> m_words;
    int         m_dictTotal = 0;

    // 测验状态
    int         m_selectedMode  = 0;   // 0=拼写, 1=英→中, 2=中→英
    int         m_selectedCount = 10;
    int         m_currentQ      = 0;
    int         m_correctCount  = 0;
    bool        m_answered      = false;
    std::vector<int> m_quizIndices;

    // 错题记录
    struct WrongAnsInfo {
        std::string word, pos, correctMeaning, userAnswer;
    };
    std::vector<WrongAnsInfo> m_wrongList;

    // 内部页面栈
    QStackedWidget* m_pages = nullptr;

    // 设置页
    QButtonGroup* m_modeGroup  = nullptr;
    QButtonGroup* m_countGroup = nullptr;

    // 答题页
    QLabel*     m_progressLabel  = nullptr;
    QWidget*    m_progressFill   = nullptr;
    QLabel*     m_questionLabel  = nullptr;
    QLineEdit*  m_spellingInput  = nullptr;
    QPushButton* m_optBtns[4]    = {};
    QLabel*     m_feedbackLabel  = nullptr;
    QPushButton* m_actionBtn     = nullptr;

    // 结果页
    QLabel*     m_scoreLabel     = nullptr;
    QWidget*    m_wrongContainer = nullptr;
    QPushButton* m_wrongRetryBtn = nullptr;
};

// 进度条（QPainter 自绘，圆角，与 Organic 风格一致）
class ProgressWidget : public QWidget {
public:
    explicit ProgressWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    void setValue(int val, int maxVal) { m_val = val; m_max = maxVal; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    int m_val = 0, m_max = 1;
};
