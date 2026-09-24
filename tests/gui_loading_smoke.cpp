#include "ui/mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <stdexcept>
#include <windows.h>

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static QPushButton* buttonWithText(QWidget* parent, const QString& text)
{
    for (auto* button : parent->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

int main(int argc, char* argv[])
{
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    QApplication app(argc, argv);
    const QString dictionary = QCoreApplication::applicationDirPath() + "/words/dictionary.txt";
    if (!QFileInfo::exists(dictionary)) {
        std::cerr << "FAIL: place a test copy at <test exe>/words/dictionary.txt\n";
        return 2;
    }

    try {
        MainWindow window;
        window.show();
        app.processEvents();
        auto* nav = window.findChild<QListWidget*>("navList");
        require(nav, "navigation missing");
        bool hasCount = false;
        for (auto* label : window.findChildren<QLabel*>("statNumber"))
            hasCount |= label->text() == "3700";
        require(hasCount, "home page did not load 3700 words");
        std::cerr << "stage: home loaded\n";

        nav->setCurrentRow(1);
        app.processEvents();
        auto* table = window.findChild<QTableWidget*>("wordTable");
        require(table && table->rowCount() == 3700, "dictionary table is empty");
        QLineEdit* search = nullptr;
        for (auto* edit : window.findChildren<QLineEdit*>())
            if (edit->placeholderText().contains("搜索单词")) search = edit;
        require(search, "search field missing");
        search->setText("apple");
        QTest::keyClick(search, Qt::Key_Return);
        require(table->rowCount() >= 1, "search returned no words");
        require(table->item(0, 0)->text().toLower().startsWith("apple"), "search returned wrong word");
        std::cerr << "stage: search works\n";

        QString dialogError;
        QTimer::singleShot(0, [&]() {
            auto* dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dlg) { dialogError = "add dialog did not open"; return; }
            bool wordSet = false, meaningSet = false;
            for (auto* edit : dlg->findChildren<QLineEdit*>()) {
                if (edit->placeholderText().contains("abandon")) {
                    edit->setText("vocabtestword"); wordSet = true;
                } else if (edit->placeholderText().contains("放弃")) {
                    edit->setText("测试词"); meaningSet = true;
                }
            }
            auto* pos = dlg->findChild<QComboBox*>();
            auto* accept = buttonWithText(dlg, "添加");
            if (!wordSet || !meaningSet || !pos || !accept) {
                dialogError = "add dialog controls missing";
                dlg->reject();
                return;
            }
            pos->setCurrentText("n.");
            accept->click();
        });
        auto* add = buttonWithText(window.findChild<QWidget*>("pageStack"), "＋ 添加单词");
        require(add, "add button missing");
        std::cerr << "stage: opening add dialog\n";
        QTest::mouseClick(add, Qt::LeftButton);
        require(dialogError.isEmpty(), "add dialog failed");
        search->setText("vocabtestword");
        QTest::keyClick(search, Qt::Key_Return);
        require(table->rowCount() == 1 && table->item(0, 2)->text() == "测试词",
                "new word was not saved or displayed");
        std::cerr << "stage: added word\n";

        table->selectRow(0);
        QTimer::singleShot(0, [&]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box || !box->button(QMessageBox::Yes)) {
                dialogError = "delete confirmation missing";
                if (box) box->reject();
                return;
            }
            box->button(QMessageBox::Yes)->click();
        });
        auto* remove = buttonWithText(window.findChild<QWidget*>("pageStack"), "✕ 删除选中");
        require(remove, "delete button missing");
        std::cerr << "stage: opening delete confirmation\n";
        QTest::mouseClick(remove, Qt::LeftButton);
        require(dialogError.isEmpty(), "delete confirmation failed");
        search->setText("vocabtestword");
        QTest::keyClick(search, Qt::Key_Return);
        require(table->rowCount() == 0, "deleted word remains searchable");
        std::cerr << "stage: deleted word\n";

        nav->setCurrentRow(2);
        app.processEvents();
        auto* quiz = window.findChild<QStackedWidget*>("quizPageStack");
        require(quiz, "quiz page missing");
        auto* start = quiz->findChild<QPushButton*>("startQuizBtn");
        require(start, "start quiz button missing");
        QTest::mouseClick(start, Qt::LeftButton);
        require(quiz->currentIndex() == 1, "quiz did not start");
        auto* question = quiz->findChild<QLabel*>("quizQuestion");
        require(question && !question->text().isEmpty(), "quiz question is empty");
        auto* answer = quiz->findChild<QLineEdit*>("quizSpellingInput");
        auto* submit = quiz->findChild<QPushButton*>("quizActionBtn");
        require(answer && submit, "quiz answer controls missing");
        answer->setText("notaword");
        QTest::mouseClick(submit, Qt::LeftButton);
        auto* feedback = quiz->findChild<QLabel*>("quizFeedback");
        require(feedback && feedback->text().contains("正确答案"), "quiz did not grade the answer");
        std::cerr << "stage: quiz answer graded\n";

        std::cerr << "stage: opening wrong-word page\n";
        nav->setCurrentRow(3);
        std::cerr << "stage: wrong-word page selected\n";
        app.processEvents();
        std::cerr << "stage: wrong-word page painted\n";
        auto* wrong = window.findChild<QStackedWidget*>("wrongPageStack");
        auto* cards = wrong ? buttonWithText(wrong, "卡片翻阅") : nullptr;
        require(cards, "wrong-word card button missing");
        std::cerr << "stage: opening card\n";
        QTest::mouseClick(cards, Qt::LeftButton);
        require(wrong->currentIndex() == 3, "wrong-word card did not open");
        std::cerr << "stage: card opened\n";
        auto* reveal = wrong->widget(3)->findChild<QPushButton*>("quizActionBtn");
        require(reveal, "card reveal button missing");
        std::cerr << "stage: revealing card\n";
        QTest::mouseClick(reveal, Qt::LeftButton);
        auto* meaning = wrong->widget(3)->findChild<QLabel*>("quizFeedback");
        require(meaning && !meaning->text().isEmpty(), "card meaning is empty");
        std::cerr << "stage: wrong-word card revealed\n";

        std::cout << "PASS: 3700 words, search, add/delete, quiz, wrong-word card\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
