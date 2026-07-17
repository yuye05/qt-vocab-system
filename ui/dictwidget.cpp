#include "dictwidget.h"
#include "core/dictionary.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>
#include <QMessageBox>

/* ================================================================
   添加单词对话框（内嵌实现，遵循 Organic 设计风格）
   ================================================================ */
static bool showAddWordDialog(QWidget* parent, std::string& word,
                               std::string& pos, std::string& meaning)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("添加单词");
    dlg.setFixedSize(420, 260);
    dlg.setObjectName("addWordDialog");
    dlg.setStyleSheet(
        "#addWordDialog { background-color: #F5F0E8; }"
        "QLabel { font-size: 14px; font-weight: bold; color: #2C2416; }"
        "QLineEdit { border: 2px solid #D9CEBB; border-radius: 8px;"
        "  padding: 8px 12px; font-size: 14px; color: #2C2416; background-color: #FFFFFF; }"
        "QLineEdit:focus { border-color: #C66B3D; }"
        "QComboBox { border: 2px solid #D9CEBB; border-radius: 8px;"
        "  padding: 8px 12px; font-size: 14px; color: #2C2416; background-color: #FFFFFF; }"
        "QComboBox:focus { border-color: #C66B3D; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background-color: #FFFFFF;"
        "  border: 1px solid #D9CEBB; selection-background-color: #C66B3D; }"
    );

    QFormLayout* form = new QFormLayout(&dlg);
    form->setContentsMargins(24, 20, 24, 16);
    form->setSpacing(12);

    QLineEdit* wordEdit = new QLineEdit;
    wordEdit->setPlaceholderText("如 abandon");
    form->addRow("英文单词：", wordEdit);

    QComboBox* posEdit = new QComboBox;
    posEdit->setEditable(true);
    posEdit->addItems({"n.", "v.", "adj.", "adv.", "prep.", "conj.",
                       "pron.", "art.", "num.", "int.", "aux.", "det."});
    posEdit->setCurrentText("");
    posEdit->lineEdit()->setPlaceholderText("如 n. v. adj.");
    form->addRow("词    性：", posEdit);

    QLineEdit* meaningEdit = new QLineEdit;
    meaningEdit->setPlaceholderText("如 放弃；遗弃");
    form->addRow("中文释义：", meaningEdit);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    QPushButton* cancelBtn = new QPushButton("取消");
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #D9CEBB; color: #2C2416; border: none;"
        "  border-radius: 8px; padding: 10px 22px; font-size: 14px; }"
        "QPushButton:hover { background-color: #C4B99E; }");
    QPushButton* okBtn = new QPushButton("添加");
    okBtn->setStyleSheet(
        "QPushButton { background-color: #C66B3D; color: #FFFFFF; border: none;"
        "  border-radius: 8px; padding: 10px 22px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #B05A32; }");
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    form->addRow(btnRow);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    std::string w = wordEdit->text().trimmed().toStdString();
    std::string p = posEdit->currentText().trimmed().toStdString();
    std::string m = meaningEdit->text().trimmed().toStdString();

    if (w.empty() || p.empty() || m.empty()) {
        QMessageBox::warning(parent, "输入不完整",
                             "英文单词、词性和中文释义都不能为空。");
        return false;
    }

    int err = inputCheck(w);
    if (err != 0) {
        QString msg;
        if (err == 1) msg = "单词必须以大写或小写字母开头。";
        else if (err == 2) msg = "单词长度必须在 1 到 45 个字符之间。";
        else if (err == 3) msg = "单词只能包含大小写字母和连字符。";
        else if (err == 4) msg = "单词不能以连字符开头或结尾。";
        else if (err == 5) msg = "单词不能包含两个连续的连字符。";
        else if (err == 6) msg = "连字符不能出现在数字旁边。";
        QMessageBox::warning(parent, "单词格式错误", msg);
        return false;
    }

    word    = w;
    pos     = p;
    meaning = m;
    return true;
}

/* ================================================================
   DictWidget 主实现
   ================================================================ */

DictWidget::DictWidget(QWidget* parent)
    : QWidget(parent), m_rootPtr(nullptr)
{
    setupUi();
}

void DictWidget::setRoot(DictNode** rootPtr)
{
    m_rootPtr = rootPtr;
    refreshTable();
}

void DictWidget::setupUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ---- 页面标题 ----
    QLabel* title = new QLabel("词库管理");
    title->setObjectName("pageTitle");

    // ---- 搜索栏 ----
    QHBoxLayout* searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("搜索单词（支持前缀匹配）…");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumHeight(36);

    QPushButton* searchBtn = new QPushButton("🔍 搜索");
    searchBtn->setFixedWidth(100);

    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(searchBtn);

    // ---- 单词表格 ----
    m_table = new QTableWidget(0, 3);
    m_table->setObjectName("wordTable");
    m_table->setHorizontalHeaderLabels({"英文单词", "词性", "中文释义"});
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setColumnWidth(0, 160);
    m_table->setColumnWidth(1, 90);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);

    // ---- 底部操作栏 ----
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);

    m_addBtn = new QPushButton("＋ 添加单词");
    m_delBtn = new QPushButton("✕ 删除选中");
    m_delBtn->setStyleSheet(
        "QPushButton { background-color: #8B7355; color: #FFFFFF; border: none;"
        "  border-radius: 8px; padding: 10px 22px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #6B5D4C; }");

    QLabel* tip = new QLabel("💡 点击列头可排序");
    tip->setStyleSheet("color: #8B7E6A; font-size: 13px;");

    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_delBtn);
    btnRow->addStretch();
    btnRow->addWidget(tip);

    // ---- 组装 ----
    layout->addWidget(title);
    layout->addLayout(searchRow);
    layout->addWidget(m_table, 1);

    auto* emptyLabel = new QLabel("没有找到匹配的单词，试试其他关键词？");
    emptyLabel->setObjectName("emptyLabel");
    emptyLabel->setAlignment(Qt::AlignCenter);
    QFont f = emptyLabel->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    emptyLabel->setFont(f);
    emptyLabel->setVisible(false);
    layout->addWidget(emptyLabel, 1);

    layout->addLayout(btnRow);

    // ---- 信号连接 ----
    connect(searchBtn, &QPushButton::clicked, this, &DictWidget::onSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &DictWidget::onSearch);
    connect(m_addBtn, &QPushButton::clicked, this, &DictWidget::onAddWord);
    connect(m_delBtn, &QPushButton::clicked, this, &DictWidget::onDeleteWord);
}

/* ---- 刷新表格 ---- */
void DictWidget::refreshTable(const std::string& filter)
{
    if (!m_rootPtr || !*m_rootPtr) return;
    DictNode* root = *m_rootPtr;

    // 收集所有单词
    constexpr int MAX_WORDS = 100000;
    DictNode** arr = new DictNode*[MAX_WORDS];
    int total = 0;
    collectAllWords(root, arr, &total);

    m_table->setSortingEnabled(false);
    m_table->setRowCount(total);

    // 构建小写版搜索词（大小写不敏感）
    std::string fLower = filter;
    for (char& c : fLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    int row = 0;
    for (int i = 0; i < total; i++) {
        const std::string& w = arr[i]->word;
        // 前缀过滤（大小写不敏感）
        if (!filter.empty()) {
            if (w.size() < filter.size())
                continue;
            bool match = true;
            for (size_t k = 0; k < filter.size(); k++) {
                char wc = static_cast<char>(std::tolower(static_cast<unsigned char>(w[k])));
                if (wc != fLower[k]) { match = false; break; }
            }
            if (!match) continue;
        }

        auto* item0 = new QTableWidgetItem(QString::fromStdString(w));
        item0->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, item0);

        auto* item1 = new QTableWidgetItem(QString::fromLocal8Bit(arr[i]->pos.c_str()));
        item1->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 1, item1);

        auto* item2 = new QTableWidgetItem(QString::fromLocal8Bit(arr[i]->meaning.c_str()));
        item2->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, item2);
        row++;
    }
    m_table->setRowCount(row);
    auto* emptyLabel = findChild<QLabel*>("emptyLabel");
    if (emptyLabel) emptyLabel->setVisible(row == 0);
    m_table->setVisible(row > 0);

    delete[] arr;
    m_table->setSortingEnabled(true);
}

/* ---- 搜索 ---- */
void DictWidget::onSearch()
{
    std::string text = m_searchEdit->text().trimmed().toStdString();
    refreshTable(text);
}

/* ---- 添加单词 ---- */
void DictWidget::onAddWord()
{
    if (!m_rootPtr || !*m_rootPtr) return;

    std::string word, pos, meaning;
    if (!showAddWordDialog(this, word, pos, meaning))
        return;

    // 检查是否已存在
    DictNode* existing = searchWord(*m_rootPtr, word);
    if (existing) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "单词已存在",
            QString::fromStdString("「" + word + "」已在词库中，是否更新词性和释义？"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    // 将用户输入的 meaning 从 UTF-8 转为 GBK，与词典文件编码一致
    QString meaningQStr = QString::fromStdString(meaning);
    std::string meaningGbk = meaningQStr.toLocal8Bit().toStdString();

    *m_rootPtr = insertWord(*m_rootPtr, word, pos, meaningGbk);
    if (!saveToFile(*m_rootPtr, DICT_FILE)) {
        QMessageBox::warning(this, "保存失败", "无法写入词典文件，请检查文件权限。");
        return;
    }

    refreshTable();
}

/* ---- 删除单词 ---- */
void DictWidget::onDeleteWord()
{
    if (!m_rootPtr || !*m_rootPtr) return;

    int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "未选中", "请先在表格中选中要删除的单词。");
        return;
    }

    QTableWidgetItem* item = m_table->item(row, 0);
    if (!item) return;

    std::string word = item->text().toStdString();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString::fromStdString("确定要删除「" + word + "」吗？\n此操作不可撤销。"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // 先修改 BST，再尝试保存
    *m_rootPtr = deleteWord(*m_rootPtr, word);
    if (!saveToFile(*m_rootPtr, DICT_FILE)) {
        QMessageBox::warning(this, "保存失败",
            "无法写入词典文件，但该单词已在内存中删除。\n请检查文件权限后重新打开程序以保持一致。");
    }

    refreshTable();
}
