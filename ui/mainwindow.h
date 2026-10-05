#pragma once
#include <QMainWindow>
#include <vector>
#include "core/dictionary.h"

class QListWidget;
class QStackedWidget;
class HomeWidget;
class DictWidget;
class QuizWidget;
class WrongWordsWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr, const QString& dataDirectory = {});
    ~MainWindow() override;

private slots:
    void onNavChanged(int index);

private:
    void setupUi();
    void loadDictionary();
    void refreshHome();

    QListWidget*      m_navList;
    QStackedWidget*   m_pages;
    HomeWidget*       m_homePage;
    DictWidget*       m_dictPage;
    QuizWidget*       m_quizPage;
    WrongWordsWidget* m_wrongPage;
    DictNode*         m_dictRoot;
};
