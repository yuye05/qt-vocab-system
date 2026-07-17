#pragma once
#include <string>
#include <vector>

/* ===== 数据目录解析（解决 Debug/Release/shadow build 路径问题） ===== */
void setDataSearchDirs(const std::vector<std::string>& dirs);
std::string resolveDataPath(const char* filename);

/* ===== 常量定义 ===== */
constexpr int MAX_WRONG = 200;          /* 生词本最大单词数 */
constexpr int MAX_HISTORY = 20;         /* 测验历史最大条数 */
constexpr const char* DICT_FILE = "dictionary.txt";
constexpr const char* WRONG_FILE = "wrong_words.txt";
constexpr const char* QUIZ_HISTORY_FILE = "quiz_history.txt";

/* ===== BST 节点结构体 ===== */
struct DictNode {
    std::string word;        /* 英文单词（键） */
    std::string pos;         /* 词性（如 n. v. adj.） */
    std::string meaning;     /* 中文释义（值） */
    DictNode* left;          /* 左子节点（字母序较小） */
    DictNode* right;         /* 右子节点（字母序较大） */

    DictNode(const std::string& w, const std::string& p, const std::string& m)
        : word(w), pos(p), meaning(m), left(nullptr), right(nullptr) {}
};

/* ===== 生词本结构体 ===== */
struct WrongWord {
    std::string word;        /* 单词 */
    int  count;              /* 错误次数 */
};

/* ===== 词性统计结构体 ===== */
struct POSStat {
    std::string pos;         /* 词性（如 n. v. adj.） */
    int count;               /* 该词性的单词数量 */
};

/* ===== 测验历史结构体 ===== */
struct QuizRecord {
    std::string timestamp;   /* 时间戳，如 "2026-07-17T10:30" */
    int mode;                /* 0=拼写 1=英→中 2=中→英 3=生词本专项 */
    int correct;
    int total;
};

/* ===== 核心函数声明 ===== */

/* 插入单词（已存在则更新词性和释义），返回新的根节点 */
DictNode* insertWord(DictNode* root, const std::string& word,
                     const std::string& pos, const std::string& meaning);

/* 查找单词，返回节点指针；未找到返回 nullptr */
DictNode* searchWord(DictNode* root, const std::string& word);

/* 删除单词，返回新的根节点 */
DictNode* deleteWord(DictNode* root, const std::string& word);

/* 中序遍历显示全部单词（字母序），回调函数版本 */
void displayAll(DictNode* root, void (*callback)(const DictNode*));

/* 前缀匹配查询，回调函数版本 */
void prefixSearch(DictNode* root, const std::string& prefix,
                  void (*callback)(const DictNode*));

/* 保存词典到文件（中序遍历，字母序） */
int saveToFile(DictNode* root, const char* filename);

/* 从文件加载词典 */
DictNode* loadFromFile(DictNode* root, const char* filename);

/* 释放整棵 BST 的内存 */
void freeTree(DictNode* root);

/* 统计词典单词总数 */
int countWords(DictNode* root);

/* 单词有效性校验（6条语言学规则） */
int inputCheck(const std::string& word);

/* 收集全部单词指针到数组（中序遍历） */
void collectAllWords(DictNode* root, DictNode** arr, int* idx);

/* 测验模式 */
void quizMode(DictNode** arr, int total, int quizNum);            /* 拼写模式（看中文拼英文） */
void quizChoice(DictNode** arr, int total, int quizNum);         /* 选择模式（英→中） */
void quizChoiceReverse(DictNode** arr, int total, int quizNum);  /* 选择模式（中→英） */

/* 词性分布统计，回调函数版本 */
void countByPOS(DictNode* root,
                void (*callback)(const std::string& pos, int count));

/* 词性分布统计，填充 vector 版本（UI 友好） */
void getPOSStats(DictNode* root, std::vector<POSStat>& stats);

/* 生词本操作 */
void recordWrong(const std::string& word);
void removeWrongWord(const std::string& word);
void showWrongWords(void (*callback)(const WrongWord*, int rank, int total));
int countWrongWords();
void clearWrongWords();
DictNode** loadWrongWordsToArray(DictNode* root, int* count);

/* 测验辅助（供 UI 层调用） */
std::vector<int> shuffledIndices(int total);
void pickOptions(int correctIdx, int total, int options[4]);

/* 测验历史 */
void saveQuizRecord(int mode, int correct, int total);
std::vector<QuizRecord> loadQuizHistory();
