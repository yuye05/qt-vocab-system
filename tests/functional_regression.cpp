#include "ui/mainwindow.h"
#include "ui/dictwidget.h"
#include "ui/quizwidget.h"
#include "ui/wrongwordswidget.h"
#include <QtWidgets>
#include <QtTest>
#include <algorithm>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void put(const QString& path, const QByteArray& text)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(text) == text.size(), "fixture write failed");
}

static QByteArray get(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "fixture read failed");
    return file.readAll();
}

static QPushButton* button(QWidget* widget, const QString& text)
{
    for (auto* item : widget->findChildren<QPushButton*>())
        if (item->text() == text) return item;
    throw std::runtime_error("button missing");
}

static void invoke(QObject* widget, const char* slot)
{
    require(QMetaObject::invokeMethod(widget, slot, Qt::DirectConnection), "slot missing");
}

static void chooseMode(QObject* widget, int mode)
{
    require(QMetaObject::invokeMethod(widget, "onModeBtn", Qt::DirectConnection, Q_ARG(int, mode)), "mode slot missing");
}

static std::string answer(QWidget* widget, const std::vector<WordEntry>& snapshot)
{
    QString prompt;
    for (const auto* label : widget->findChildren<QLabel*>("quizQuestion"))
        if (label->isVisible()) prompt = label->text();
    for (const auto& entry : snapshot)
        if (prompt == QString::fromStdString(entry.pos + " " + entry.meaning)) return entry.word;
    throw std::runtime_error("question does not match snapshot");
}

static void resetFiles(const QString& directory)
{
    setDataSearchDirs({directory.toStdString()});
    put(QDir(directory).filePath(WRONG_FILE), "");
    put(QDir(directory).filePath(QUIZ_HISTORY_FILE), "");
}

static std::string gradeAndContinue(QuizWidget& widget, const std::vector<WordEntry>& snapshot,
                                    int mode, bool correct)
{
    const std::string target = mode == 1
        ? widget.findChild<QLabel*>("quizQuestion")->text().toStdString() : answer(&widget, snapshot);
    if (mode == 0) {
        auto* input = widget.findChild<QLineEdit*>("quizSpellingInput");
        input->setText(correct ? QString::fromStdString(target) : "incorrect");
        QTest::keyClick(input, Qt::Key_Return);
    } else {
        QPushButton* selected = nullptr;
        for (auto* option : widget.findChildren<QPushButton*>("quizOptionBtn")) {
            if (!option->isVisible()) continue;
            const int index = option->property("optIndex").toInt();
            if ((snapshot[index].word == target) == correct) { selected = option; break; }
        }
        require(selected, "required choice not available");
        QTest::mouseClick(selected, Qt::LeftButton);
    }
    require(widget.findChild<QLabel*>("quizFeedback")->text().contains(correct ? "回答正确" : "回答错误"),
            "answer not graded as expected");
    QTest::keyClick(widget.findChild<QPushButton*>("quizActionBtn"), Qt::Key_Space);
    return target;
}

static void addWord(DictWidget& widget, const QString& word, bool expectFailure = false)
{
    bool failed = false;
    QTimer dialogs;
    QObject::connect(&dialogs, &QTimer::timeout, [&]() {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            failed = true;
            box->accept();
        } else if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            for (auto* input : dialog->findChildren<QLineEdit*>()) {
                if (input->placeholderText().contains("abandon")) input->setText(word);
                if (input->placeholderText().contains("放弃")) input->setText("中文释义；🍐");
            }
            dialog->findChild<QComboBox*>()->setCurrentText("noun");
            button(dialog, "添加")->click();
        }
    });
    dialogs.start(5);
    invoke(&widget, "onAddWord");
    dialogs.stop();
    require(failed == expectFailure, "unexpected add/save result");
}

static void deleteSelected(DictWidget& widget, bool expectFailure = false)
{
    bool failed = false;
    QTimer dialogs;
    QObject::connect(&dialogs, &QTimer::timeout, [&]() {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            if (box->button(QMessageBox::Yes)) box->button(QMessageBox::Yes)->click();
            else { failed = true; box->accept(); }
        }
    });
    dialogs.start(5);
    invoke(&widget, "onDeleteWord");
    dialogs.stop();
    require(failed == expectFailure, "unexpected delete/save result");
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    try {
        const QString screenshotDir = argc > 1 ? app.arguments().at(1) : QString();
        if (!screenshotDir.isEmpty()) {
            QFile style(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../style/app.qss"));
            require(style.open(QFile::ReadOnly), "QA stylesheet missing");
            app.setStyleSheet(QString::fromUtf8(style.readAll()));
        }
        auto capture = [&](QWidget& widget, const char* name) {
            if (screenshotDir.isEmpty()) return;
            QTest::qWait(80);
            require(widget.grab().save(QDir(screenshotDir).filePath(name)), "screenshot save failed");
        };
        require(temporary.isValid(), "temporary data failed");
        const QString directory = temporary.path();
        const QString dictionary = QDir(directory).filePath(DICT_FILE);
        resetFiles(directory);
        DictNode* root = nullptr;
        {
            DictWidget widget;
            widget.setRoot(&root);
            widget.show();
            QTest::qWait(10);
            auto* table = widget.findChild<QTableWidget*>("wordTable");
            require(table->rowCount() == 0, "empty table is not empty");
            addWord(widget, "apple");
            require(root && root->pos == "noun" && table->rowCount() == 1, "empty dictionary cannot add");
            auto* reload = loadFromFile(nullptr, DICT_FILE);
            require(reload && reload->meaning == "中文释义；🍐", "UTF-8/POS edit did not persist");
            freeTree(reload);
            const QByteArray saved = get(dictionary);
            // 以目录阻塞目标文件，跨平台稳定复现保存失败，不依赖权限差异。
            require(QFile::remove(dictionary) && QDir().mkdir(dictionary), "save failure fixture failed");
            addWord(widget, "banana", true);
            require(countWords(root) == 1 && table->rowCount() == 1, "failed add changed memory/table");
            table->selectRow(0);
            deleteSelected(widget, true);
            require(countWords(root) == 1 && table->rowCount() == 1, "failed deletion changed memory/table");
            require(QDir().rmdir(dictionary), "blocked fixture cleanup failed");
            put(dictionary, saved);
            require(recordWrong("apple"), "wrong word fixture failed");
            table->selectRow(0);
            deleteSelected(widget);
            require(!root && table->rowCount() == 0 && countWrongWords() == 0, "last deletion or linked wrong cleanup failed");
            addWord(widget, "pear");
            require(root && countWords(root) == 1, "cannot recover after deleting last word");
        }
        freeTree(root);
        root = nullptr;
        std::cout << "PASS: empty/recoverable dictionary, UTF-8/POS edit, transactional failures, linked removal\n";

        for (int count = 1; count <= 3; ++count) {
            resetFiles(directory);
            root = nullptr;
            for (int i = 0; i < count; ++i) root = insertWord(root, "word" + std::to_string(i), "n.", "meaning" + std::to_string(i));
            for (int mode = 1; mode <= 2; ++mode) {
                QuizWidget widget;
                widget.setRoot(&root);
                widget.show();
                chooseMode(&widget, mode);
                invoke(&widget, "onStartQuiz");
                int visible = 0;
                for (auto* option : widget.findChildren<QPushButton*>("quizOptionBtn")) visible += option->isVisible();
                require(visible == count, "small dictionary option count incorrect");
                invoke(&widget, "onCancel");
            }
            freeTree(root);
        }
        std::cout << "PASS: 1/2/3-word choice modes start and cancel\n";

        for (int mode = 0; mode < 3; ++mode) {
            resetFiles(directory);
            root = nullptr;
            for (const auto* word : {"apple", "banana", "cherry", "date", "elder", "fig"})
                root = insertWord(root, word, "n.", std::string("词义 ") + word);
            const auto pool = wordSnapshot(root);
            {
                QuizWidget widget;
                widget.setRoot(&root);
                widget.resize(700, 650);
                widget.show();
                chooseMode(&widget, mode);
                QTest::mouseClick(button(&widget, "5"), Qt::LeftButton);
                invoke(&widget, "onStartQuiz");
                QSet<QString> errors;
                for (int i = 0; i < 5; ++i) {
                    const auto word = gradeAndContinue(widget, pool, mode, i >= 2);
                    if (i < 2) errors.insert(QString::fromStdString(word));
                }
                auto* pages = widget.findChild<QStackedWidget*>("quizPageStack");
                auto* retry = widget.findChild<QPushButton*>("quizWrongRetryBtn");
                require(pages->currentIndex() == 2 && retry && retry->isVisible(), "wrong retry entry missing");
                require(widget.findChild<QLabel*>("quizScoreLabel")->text().contains("3 / 5"), "initial score wrong");
                auto history = loadQuizHistory();
                require(history.size() == 1 && history[0].mode == mode && history[0].total == 5, "initial history wrong");
                if (mode == 0) capture(widget, "wrong-retry-result.png");

                // 修改实时词典后，再练仍应使用原始完整快照，而非只用两个错词出选项。
                for (const auto& entry : pool)
                    if (!errors.contains(QString::fromStdString(entry.word))) root = deleteWord(root, entry.word);
                retry->setFocus();
                QTest::keyClick(retry, Qt::Key_Return);
                require(pages->currentIndex() == 1 && widget.findChild<QLabel*>("quizProgressLabel")->text().contains("1/2"),
                        "wrong retry used original question count");
                QString failedAgain;
                QSet<QString> retried;
                for (int i = 0; i < 2; ++i) {
                    if (mode != 0) {
                        int visible = 0;
                        for (auto* option : widget.findChildren<QPushButton*>("quizOptionBtn")) visible += option->isVisible();
                        require(visible == 4, "wrong retry lost full snapshot distractors");
                    }
                    const QString target = QString::fromStdString(gradeAndContinue(widget, pool, mode, i == 0));
                    require(errors.contains(target) && !retried.contains(target), "wrong retry included correct/repeated question");
                    retried.insert(target);
                    if (i == 1) failedAgain = target;
                }
                require(retried == errors, "wrong retry did not cover all mistakes");
                require(widget.findChild<QLabel*>("quizScoreLabel")->text().contains("1 / 2"), "retry score wrong");
                history = loadQuizHistory();
                require(history.size() == 2 && history[0].mode == mode && history[0].total == 2 && history[0].correct == 1,
                        "retry did not create one new history record");
                std::vector<WrongWord> wrong;
                require(loadWrongWords(wrong) && wrong.size() == 2, "correct retry removed a wrong word");
                for (const auto& item : wrong)
                    require(item.count == (QString::fromStdString(item.word) == failedAgain ? 2 : 1), "retry error count wrong");

                retry->setFocus();
                QTest::keyClick(retry, Qt::Key_Space);
                require(widget.findChild<QLabel*>("quizProgressLabel")->text().contains("1/1"), "chained retry did not use latest errors");
                auto* focused = mode == 0 ? static_cast<QWidget*>(widget.findChild<QLineEdit*>("quizSpellingInput"))
                    : static_cast<QWidget*>(widget.findChildren<QPushButton*>("quizOptionBtn").front());
                QTest::keyClick(focused, Qt::Key_Escape);
                require(pages->currentIndex() == 0 && countWrongWords() == 2 && loadQuizHistory().size() == 2,
                        "canceled retry changed wrong words/history");
                invoke(&widget, "onStartQuiz");
                require(widget.findChild<QLabel*>("quizProgressLabel")->text().contains("1/2"), "normal start reused old snapshot");
                gradeAndContinue(widget, wordSnapshot(root), mode, true);
                gradeAndContinue(widget, wordSnapshot(root), mode, true);
                require(!retry->isVisible(), "perfect result showed wrong retry");
                invoke(&widget, "onRetryWrong");
                require(pages->currentIndex() == 2 && loadQuizHistory().size() == 3, "empty retry started or duplicated history");
            }
            freeTree(root);
        }
        std::cout << "PASS: all-mode wrong retry, full snapshot choices, retained words, counts, history and keyboard\n";

        resetFiles(directory);
        root = nullptr;
        for (const auto* word : {"adequate", "proper"}) root = insertWord(root, word, "a.", "适当的");
        root = insertWord(root, "apple", "n.", "苹果");
        root = insertWord(root, "banana", "n.", "香蕉");
        const auto words = wordSnapshot(root);
        {
            QuizWidget widget;
            widget.setRoot(&root);
            widget.resize(700, 650);
            widget.show();
            invoke(&widget, "onStartQuiz");
            const auto expected = answer(&widget, words);
            const auto* target = searchWord(root, expected);
            const std::string alternative = target->meaning == "适当的" ? "proper" : expected;
            root = deleteWord(root, expected);
            auto* input = widget.findChild<QLineEdit*>("quizSpellingInput");
            auto* progress = widget.findChild<QLabel*>("quizProgressLabel");
            input->setText(QString::fromStdString(alternative));
            QTest::keyClick(input, Qt::Key_Return);
            require(progress->text().contains("1/"), "Enter submitted and skipped feedback");
            require(widget.findChild<QLabel*>("quizFeedback")->text().contains("回答正确"), "deleted snapshot/equivalent spelling failed");
            capture(widget, "spelling-feedback.png");
            QTest::keyClick(widget.findChild<QPushButton*>("quizActionBtn"), Qt::Key_Space);
            require(progress->text().contains("2/"), "Space did not advance question");
            invoke(&widget, "onCancel");
            chooseMode(&widget, 1);
            invoke(&widget, "onStartQuiz");
            auto options = widget.findChildren<QPushButton*>("quizOptionBtn");
            QSet<QString> texts;
            for (auto* option : options) {
                if (!option->isVisible()) continue;
                const QString text = option->text().mid(3);
                require(!texts.contains(text), "duplicate displayed options");
                texts.insert(text);
            }
            options.front()->setFocus();
            QTest::keyClick(options.front(), Qt::Key_1);
            require(widget.findChild<QPushButton*>("quizActionBtn")->isVisible(), "choice key did not grade");
            QTest::keyClick(widget.findChild<QPushButton*>("quizActionBtn"), Qt::Key_Space);
            require(widget.findChild<QStackedWidget*>("quizPageStack")->currentIndex() == 1, "choice Space canceled quiz");
        }
        freeTree(root);
        std::cout << "PASS: snapshots survive deletion, spelling Enter/Space, choice shortcuts and unique options\n";

        resetFiles(directory);
        root = insertWord(nullptr, "adequate", "a.", "适当的");
        root = insertWord(root, "proper", "a.", "适当的");
        {
            QuizWidget widget;
            widget.setRoot(&root);
            widget.show();
            invoke(&widget, "onStartQuiz");
            bool sawAlternative = false;
            for (int i = 0; i < 2; ++i) {
                auto* input = widget.findChild<QLineEdit*>("quizSpellingInput");
                input->setText("PROPER");
                QTest::keyClick(input, Qt::Key_Return);
                const QString feedback = widget.findChild<QLabel*>("quizFeedback")->text();
                require(feedback.contains("回答正确"), "normal equivalent spelling rejected");
                sawAlternative |= feedback.contains("adequate");
                QTest::keyClick(widget.findChild<QPushButton*>("quizActionBtn"), Qt::Key_Space);
            }
            require(sawAlternative && widget.findChild<QLabel*>("quizScoreLabel")->text().contains("2 / 2"),
                    "normal synonym answer did not receive full score");
        }
        require(recordWrong("adequate"), "synonym special fixture failed");
        {
            WrongWordsWidget widget;
            widget.setRoot(&root);
            widget.show();
            invoke(&widget, "onStartQuiz");
            auto* input = widget.findChild<QLineEdit*>("quizSpellingInput");
            input->setText("PROPER");
            QTest::keyClick(input, Qt::Key_Return);
            auto* page = widget.findChild<QStackedWidget*>("wrongPageStack")->widget(2);
            require(page->findChild<QLabel*>("quizFeedback")->text().contains("回答正确"), "special equivalent spelling rejected");
            QTest::keyClick(page->findChild<QPushButton*>("quizActionBtn"), Qt::Key_Space);
            require(countWrongWords() == 0, "equivalent special answer did not remove target word");
        }
        freeTree(root);
        std::cout << "PASS: equivalent spelling in both real quiz pages, target feedback and full scores\n";

        resetFiles(directory);
        root = nullptr;
        for (const auto* word : {"apple", "banana", "cherry", "date", "elder", "fig"}) {
            root = insertWord(root, word, "n.", std::string("meaning ") + word);
            require(recordWrong(word), "wrong fixture failed");
        }
        const auto six = wordSnapshot(root);
        {
            WrongWordsWidget widget;
            widget.setRoot(&root);
            widget.resize(700, 650);
            widget.show();
            require(QMetaObject::invokeMethod(&widget, "onCountBtn", Qt::DirectConnection, Q_ARG(int, 0)), "count slot missing");
            invoke(&widget, "onStartQuiz");
            auto* input = widget.findChild<QLineEdit*>("quizSpellingInput");
            auto* action = widget.findChild<QStackedWidget*>("wrongPageStack")->widget(2)->findChild<QPushButton*>("quizActionBtn");
            for (int i = 0; i < 5; ++i) {
                input->setText(QString::fromStdString(answer(&widget, six)));
                QTest::keyClick(input, Qt::Key_Return);
                require(widget.findChild<QStackedWidget*>("wrongPageStack")->currentIndex() == 2, "special Enter skipped result/feedback");
                QTest::keyClick(action, Qt::Key_Space);
            }
            require(countWrongWords() == 1, "special success removed untested words");
            auto* result = widget.findChild<QLabel*>("placeholder");
            require(result->isVisible() && result->text().contains("剩余 1"), "partial quiz result is hidden or wrong");
            capture(widget, "special-result.png");
            invoke(&widget, "onStartQuiz");
            input->setText("incorrect");
            QTest::keyClick(input, Qt::Key_Return);
            std::vector<WrongWord> remaining;
            require(loadWrongWords(remaining) && remaining[0].count == 2, "special wrong count not incremented");
            invoke(&widget, "onCancel");
            chooseMode(&widget, 1);
            invoke(&widget, "onStartQuiz");
            int visible = 0;
            for (auto* option : widget.findChildren<QPushButton*>("quizOptionBtn")) visible += option->isVisible();
            require(visible == 4, "special choices did not use full dictionary");
            invoke(&widget, "onCancel");
            invoke(&widget, "onEnterCardReview");
            auto* cards = widget.findChild<QStackedWidget*>("wrongPageStack")->widget(3);
            auto* reveal = cards->findChild<QPushButton*>("quizActionBtn");
            reveal->setFocus();
            QTest::keyClick(reveal, Qt::Key_Space);
            require(cards->findChild<QLabel*>("quizFeedback")->isVisible(), "card Space did not reveal");
            capture(widget, "card-answer.png");
            QTest::keyClick(button(cards, "已掌握 ✓"), Qt::Key_Space);
            require(countWrongWords() == 0, "card Space canceled instead of marking mastered");
        }
        freeTree(root);
        std::cout << "PASS: complete special quiz, visible partial result, wrong counting, distractors, card keyboard\n";

        resetFiles(directory);
        root = insertWord(nullptr, "apple", "n.", "苹果");
        put(QDir(directory).filePath(WRONG_FILE), "ghost 1\napple 1\n");
        {
            WrongWordsWidget widget;
            widget.setRoot(&root);
            widget.show();
            invoke(&widget, "onStartQuiz");
            require(widget.findChild<QStackedWidget*>("wrongPageStack")->widget(2)
                ->findChild<QLabel*>("quizProgressLabel")->text().contains("1/1"), "orphan included in quiz");
            invoke(&widget, "onCancel");
            invoke(&widget, "onEnterCardReview");
            root = deleteWord(root, "apple");
            invoke(&widget, "onCardShowAnswer");
            auto* cards = widget.findChild<QStackedWidget*>("wrongPageStack")->widget(3);
            require(cards->findChild<QLabel*>("quizFeedback")->text().contains("苹果"), "card lost deleted snapshot");
            invoke(&widget, "onBackToList");
        }
        std::cout << "PASS: orphan filtering and card snapshot lifetime\n";

        resetFiles(directory);
        put(dictionary, "# qt-vocab UTF-8 TSV v1\n");
        require(saveQuizRecord(0, 1, 1), "startup history fixture failed");
        {
            MainWindow window(nullptr, directory);
            window.show();
            QTest::qWait(20);
            require(window.findChild<QWidget*>("quizHistoryContainer")->isVisible(), "startup history hidden");
            auto* nav = window.findChild<QListWidget*>("navList");
            nav->setCurrentRow(1);
            auto* dict = window.findChild<DictWidget*>();
            addWord(*dict, "apple");
            auto* table = window.findChild<QTableWidget*>("wordTable");
            table->selectRow(0);
            deleteSelected(*dict);
            nav->setCurrentRow(0);
            require(table->rowCount() == 0, "empty main dictionary table stale");
            for (auto* stat : window.findChildren<QLabel*>("statNumber")) require(stat->text() == "0", "empty home stats stale");
            require(window.findChild<QLabel*>("quizHistorySummary")->text().contains("最多 20 次"), "history scope not explicit");
            capture(window, "empty-home.png");
        }
        std::cout << "PASS: startup history and zero-word home refresh\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << " / " << dataError() << '\n';
        return 1;
    }
}
