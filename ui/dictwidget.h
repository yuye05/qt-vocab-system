#pragma once
#include <QWidget>
#include "core/dictionary.h"

class QLineEdit;
class QTableWidget;
class QPushButton;

class DictWidget : public QWidget {
    Q_OBJECT
public:
    explicit DictWidget(QWidget* parent = nullptr);
    void setRoot(DictNode** rootPtr);

private slots:
    void onSearch();
    void onAddWord();
    void onDeleteWord();

private:
    void setupUi();
    void refreshTable(const std::string& filter = "");

    DictNode**    m_rootPtr;
    QLineEdit*    m_searchEdit;
    QTableWidget* m_table;
    QPushButton*  m_addBtn;
    QPushButton*  m_delBtn;
};
