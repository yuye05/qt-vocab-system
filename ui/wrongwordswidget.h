#pragma once
#include <QWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QButtonGroup>
#include <QScrollArea>
#include <vector>
#include "core/dictionary.h"

class WrongWordsWidget : public QWidget {
    Q_OBJECT
public:
    explicit WrongWordsWidget(QWidget* parent = nullptr);
    ~WrongWordsWidget() override;
    void setRoot(DictNode** rootPtr);
    void refreshList();
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSortToggle();
    void onDeleteWord(const std::string& word);
    void onEnterQuizSetup();
    void onEnterCardReview();
    void onBackToList();
    void onModeBtn(int id);
    void onCountBtn(int id);
    void onStartQuiz();
    void onSubmitOrNext();
    void onCancel();
    void onSpellingReturn();
    void onCardShowAnswer();
    void onCardMastered();
    void onCardNotMastered();

    // 辅助
    void advanceCard();

private:
    void buildListPage();
    void buildQuizSetupPage();
    void buildQuizPage();
    void buildCardPage();
    void loadWrongWordList(std::vector<WrongWord>& arr);

    // 测验相关
    void startQuiz();
    void showQuestion();
    void checkSpellingAnswer();
    void checkChoiceAnswer(int clickedOptIdx);
    void setupChoiceOptions(int wordIdx);
    void showQuizResult();

    // 外部数据
    DictNode**  m_dictRoot = nullptr;

    // 内部页面栈
    QStackedWidget* m_pages = nullptr;

    // 列表页
    QWidget*     m_listContainer = nullptr;
    QPushButton* m_sortBtn = nullptr;
    QLabel*      m_emptyLabel = nullptr;
    bool         m_sortByCount = true;   // true=按次数, false=按字母

    // 测验设置页
    QButtonGroup* m_modeGroup  = nullptr;
    QButtonGroup* m_countGroup = nullptr;

    // 测验状态
    int         m_selectedMode  = 0;
    int         m_selectedCount = 10;
    int         m_currentQ      = 0;
    int         m_correctCount  = 0;
    int         m_quizTotal     = 0;
    bool        m_answered      = false;
    std::vector<WrongWord> m_wrongArr;     // 当前生词本快照（测验用）
    std::vector<int>       m_quizIndices;
    std::vector<std::string> m_correctWords;  // 本轮测验答对（测验结束时统一移除）

    // 答题页
    QLabel*     m_progressLabel  = nullptr;
    QWidget*    m_progressFill   = nullptr;
    QLabel*     m_questionLabel  = nullptr;
    QLineEdit*  m_spellingInput  = nullptr;
    QPushButton* m_optBtns[4]    = {};
    QLabel*     m_feedbackLabel  = nullptr;
    QPushButton* m_actionBtn     = nullptr;

    // 卡片页
    int         m_cardIdx = 0;
    int         m_cardMastered = 0;      // 本次翻阅掌握的单词数
    bool        m_cardShowAnswer = false;
    std::vector<WrongWord> m_cardArr;
    QLabel*     m_cardProgressLabel = nullptr;
    QWidget*    m_cardProgressFill  = nullptr;
    QLabel*     m_cardWordLabel     = nullptr;
    QLabel*     m_cardMeaningLabel  = nullptr;
    QPushButton* m_cardActionBtn    = nullptr;
    QPushButton* m_cardMasteredBtn  = nullptr;
    QPushButton* m_cardNotMasteredBtn = nullptr;
};

// 进度条（QPainter 自绘，与 QuizWidget 一致）
class WrongProgressWidget : public QWidget {
public:
    explicit WrongProgressWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    void setValue(int val, int maxVal) { m_val = val; m_max = maxVal; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    int m_val = 0, m_max = 1;
};
