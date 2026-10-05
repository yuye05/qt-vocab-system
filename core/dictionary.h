#pragma once
#include <string>
#include <vector>

/* ===== 数据目录解析（解决 Debug/Release/shadow build 路径问题） ===== */
void setDataSearchDirs(const std::vector<std::string>& dirs);
std::string resolveDataPath(const char* filename);
bool initializeDataDirectory(const std::string& directory,
                             const std::vector<std::string>& legacyDirectories);
const std::string& dataError();

/* ===== 常量定义 ===== */
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

// 独立于 BST 节点生命周期的词条快照，供表格与测验使用。
struct WordEntry {
    std::string word, pos, meaning;
};
std::vector<WordEntry> wordSnapshot(DictNode* root);
bool equivalentMeaning(const WordEntry& a, const WordEntry& b);
bool spellingMatches(const std::vector<WordEntry>& words,
                     const WordEntry& target, const std::string& answer);

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

/* 从文件加载：空树直接构建平衡 BST；已有树按文件顺序合并。
   忽略大小写的重复词条保留首次拼写，以最后一条词性、释义为准。 */
DictNode* loadFromFile(DictNode* root, const char* filename);

/* 释放整棵 BST 的内存 */
void freeTree(DictNode* root);

/* 统计词典单词总数 */
int countWords(DictNode* root);

/* 单词有效性校验（6条语言学规则） */
int inputCheck(const std::string& word);

/* 收集全部单词指针到数组（中序遍历） */
void collectAllWords(DictNode* root, DictNode** arr, int* idx);

/* 词性分布统计，填充 vector 版本（UI 友好） */
void getPOSStats(DictNode* root, std::vector<POSStat>& stats);

/* 生词本操作 */
bool recordWrong(const std::string& word);
bool removeWrongWord(const std::string& word);
bool removeWrongWords(const std::vector<std::string>& words);
bool loadWrongWords(std::vector<WrongWord>& words);
int countWrongWords();

/* 测验辅助（供 UI 层调用） */
std::vector<int> shuffledIndices(int total);
std::vector<int> pickOptions(int correctIdx, const std::vector<WordEntry>& words);

/* 测验历史 */
bool saveQuizRecord(int mode, int correct, int total);
std::vector<QuizRecord> loadQuizHistory();
