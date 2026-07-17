#include "dictionary.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <sys/stat.h>

/* ===== 数据目录搜索路径 ===== */
static std::vector<std::string> s_dataDirs;

void setDataSearchDirs(const std::vector<std::string>& dirs)
{
    s_dataDirs = dirs;
}

static bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

/* 在搜索路径中查找 filename，返回第一个找到的完整路径；找不到则返回原名 */
std::string resolveDataPath(const char* filename)
{
    /* 1. 先检查搜索路径列表 */
    for (const auto& dir : s_dataDirs) {
        std::string full = dir + "/" + filename;
        if (fileExists(full)) return full;
    }
    /* 2. 回退到当前工作目录（原名） */
    return std::string(filename);
}

// ================================================================
// 辅助函数（提取重复逻辑）
// ================================================================

// Fisher-Yates 洗牌，返回打乱后的索引数组
std::vector<int> shuffledIndices(int total)
{
    std::vector<int> indices(total);
    for (int i = 0; i < total; i++) indices[i] = i;
    for (int i = total - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        std::swap(indices[i], indices[j]);
    }
    return indices;
}

// 为选择题收集 4 个选项（1 正确 + 3 干扰）
void pickOptions(int correctIdx, int total, int options[4])
{
    options[0] = correctIdx;
    int optCount = 1;
    while (optCount < 4) {
        int didx = rand() % total;
        bool dup = false;
        for (int i = 0; i < optCount; i++) {
            if (options[i] == didx) { dup = true; break; }
        }
        if (!dup) options[optCount++] = didx;
    }
    // Fisher-Yates 洗牌
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = options[i];
        options[i] = options[j];
        options[j] = temp;
    }
}

// ================================================================
// BST 算法：插入、查找、删除、遍历
// ================================================================

// 大小写不敏感比较：返回 -1/0/1
static int cmpIgnoreCase(const std::string& a, const std::string& b)
{
    size_t i = 0;
    size_t minLen = std::min(a.size(), b.size());
    for (; i < minLen; i++) {
        char ca = static_cast<char>(tolower(static_cast<unsigned char>(a[i])));
        char cb = static_cast<char>(tolower(static_cast<unsigned char>(b[i])));
        if (ca < cb) return -1;
        if (ca > cb) return 1;
    }
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    return 0;
}

DictNode* insertWord(DictNode* root, const std::string& word,
                     const std::string& pos, const std::string& meaning)
{
    if (root == nullptr) {
        return new DictNode(word, pos, meaning);
    }

    int cmp = cmpIgnoreCase(word, root->word);
    if (cmp < 0) {
        root->left = insertWord(root->left, word, pos, meaning);
    } else if (cmp > 0) {
        root->right = insertWord(root->right, word, pos, meaning);
    } else {
        root->pos = pos;
        root->meaning = meaning;
    }
    return root;
}

DictNode* searchWord(DictNode* root, const std::string& word)
{
    if (root == nullptr) return nullptr;
    int cmp = cmpIgnoreCase(word, root->word);
    if (cmp == 0) return root;
    if (cmp < 0) return searchWord(root->left, word);
    return searchWord(root->right, word);
}

// 找 BST 中的最小节点（用于删除）
static DictNode* findMin(DictNode* node)
{
    if (node == nullptr) return nullptr;
    while (node->left != nullptr) node = node->left;
    return node;
}

// 删除节点（内部实现）
static DictNode* deleteNode(DictNode* root, const std::string& word, int* found)
{
    if (root == nullptr) return nullptr;

    int cmp = cmpIgnoreCase(word, root->word);
    if (cmp < 0) {
        root->left = deleteNode(root->left, word, found);
    } else if (cmp > 0) {
        root->right = deleteNode(root->right, word, found);
    } else {
        *found = 1;
        if (root->left == nullptr) {
            DictNode* temp = root->right;
            delete root;
            return temp;
        } else if (root->right == nullptr) {
            DictNode* temp = root->left;
            delete root;
            return temp;
        } else {
            DictNode* successor = findMin(root->right);
            root->word = successor->word;
            root->pos = successor->pos;
            root->meaning = successor->meaning;
            int dummy = 0;
            root->right = deleteNode(root->right, successor->word, &dummy);
        }
    }
    return root;
}

DictNode* deleteWord(DictNode* root, const std::string& word)
{
    int found = 0;
    root = deleteNode(root, word, &found);
    return root;
}

// 中序遍历：通过回调函数输出每个节点
void displayAll(DictNode* root, void (*callback)(const DictNode*))
{
    if (root == nullptr) return;
    displayAll(root->left, callback);
    callback(root);
    displayAll(root->right, callback);
}

// 前缀匹配查询：通过回调函数输出匹配节点（大小写不敏感）
void prefixSearch(DictNode* root, const std::string& prefix,
                  void (*callback)(const DictNode*))
{
    if (root == nullptr) return;
    int prefixLen = static_cast<int>(prefix.size());
    prefixSearch(root->left, prefix, callback);
    // 大小写不敏感前缀比较
    if (static_cast<int>(root->word.size()) >= prefixLen) {
        bool match = true;
        for (int k = 0; k < prefixLen; k++) {
            char wc = static_cast<char>(tolower(static_cast<unsigned char>(root->word[k])));
            char pc = static_cast<char>(tolower(static_cast<unsigned char>(prefix[k])));
            if (wc != pc) { match = false; break; }
        }
        if (match) callback(root);
    }
    prefixSearch(root->right, prefix, callback);
}

// ================================================================
// 文件 I/O
// ================================================================

// 保存节点到文件（中序遍历递归写入）
static void saveNode(std::ofstream& file, DictNode* node)
{
    if (node == nullptr) return;
    saveNode(file, node->left);
    file << node->word << "  " << node->pos << node->meaning << "\n";
    saveNode(file, node->right);
}

int saveToFile(DictNode* root, const char* filename)
{
    std::string path = resolveDataPath(filename);
    std::ofstream file(path);
    if (!file.is_open()) {
        return 0;
    }
    saveNode(file, root);
    return 1;
}

// 内部插入（不打印信息，供 loadFromFile 使用）
static DictNode* insertWordSilent(DictNode* root, const std::string& word,
                                   const std::string& pos, const std::string& meaning)
{
    if (root == nullptr) {
        return new DictNode(word, pos, meaning);
    }
    int cmp = cmpIgnoreCase(word, root->word);
    if (cmp < 0)
        root->left = insertWordSilent(root->left, word, pos, meaning);
    else if (cmp > 0)
        root->right = insertWordSilent(root->right, word, pos, meaning);
    else {
        root->pos = pos;
        root->meaning = meaning;
    }
    return root;
}

DictNode* loadFromFile(DictNode* root, const char* filename)
{
    std::string path = resolveDataPath(filename);
    std::ifstream file(path);
    if (!file.is_open()) return root;

    // 读入所有行
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty()) continue;
        lines.push_back(line);
    }
    file.close();

    // Fisher-Yates 洗牌：避免有序输入导致 BST 退化
    for (int i = static_cast<int>(lines.size()) - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        std::swap(lines[i], lines[j]);
    }

    for (const auto& ln : lines) {
        size_t delimPos = ln.find("  ");
        if (delimPos == std::string::npos) continue;

        std::string word = ln.substr(0, delimPos);
        std::string rest = ln.substr(delimPos + 2);

        size_t dotPos = rest.find('.');
        if (dotPos == std::string::npos || dotPos + 1 >= rest.size()) continue;

        std::string pos = rest.substr(0, dotPos + 1);  // 含 '.'
        std::string meaning = rest.substr(dotPos + 1);

        root = insertWordSilent(root, word, pos, meaning);
    }

    return root;
}

// ================================================================
// 树的遍历与统计
// ================================================================

void freeTree(DictNode* root)
{
    if (root == nullptr) return;
    freeTree(root->left);
    freeTree(root->right);
    delete root;
}

int countWords(DictNode* root)
{
    if (root == nullptr) return 0;
    return 1 + countWords(root->left) + countWords(root->right);
}

void collectAllWords(DictNode* root, DictNode** arr, int* idx)
{
    if (root == nullptr) return;
    collectAllWords(root->left, arr, idx);
    arr[*idx] = root;
    (*idx)++;
    collectAllWords(root->right, arr, idx);
}

// 忽略大小写的字符串比较
[[maybe_unused]] static int icompare(const std::string& a, const std::string& b)
{
    size_t i = 0;
    while (i < a.size() && i < b.size()) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + 32);
        if (ca != cb) return ca - cb;
        i++;
    }
    if (i < a.size()) return 1;
    if (i < b.size()) return -1;
    return 0;
}

// ================================================================
// 测验模式 1：拼写模式（看中文拼英文）
// ================================================================

void quizMode(DictNode** arr, int total, int quizNum)
{
    (void)arr;
    if (total == 0) return;
    if (quizNum > total) quizNum = total;

    std::vector<int> indices = shuffledIndices(total);

    for (int q = 0; q < quizNum; q++) {
        (void)indices[q];
        // 此函数在 Qt 版本中由 QuizWidget 调用，
        // 答题交互在 UI 层完成，此处保留核心逻辑骨架供后续使用
    }
}

// ================================================================
// 测验模式 2：选择模式（看英文选中文释义）
// ================================================================

void quizChoice(DictNode** arr, int total, int quizNum)
{
    (void)arr;
    if (total < 4) return;
    if (quizNum > total) quizNum = total;

    std::vector<int> indices = shuffledIndices(total);

    for (int q = 0; q < quizNum; q++) {
        int idx = indices[q];
        int options[4];
        pickOptions(idx, total, options);
        (void)options;
        // 答题交互在 UI 层完成
    }
}

// ================================================================
// 测验模式 3：选择模式（看中文选英文单词）
// ================================================================

void quizChoiceReverse(DictNode** arr, int total, int quizNum)
{
    (void)arr;
    if (total < 4) return;
    if (quizNum > total) quizNum = total;

    std::vector<int> indices = shuffledIndices(total);

    for (int q = 0; q < quizNum; q++) {
        int idx = indices[q];
        int options[4];
        pickOptions(idx, total, options);
        (void)options;
        // 答题交互在 UI 层完成
    }
}

// ================================================================
// 生词本操作
// ================================================================

void recordWrong(const std::string& word)
{
    WrongWord wrongs[MAX_WRONG];
    int count = 0;

    std::ifstream fin(resolveDataPath(WRONG_FILE));
    if (fin.is_open()) {
        std::string w;
        int c;
        while (count < MAX_WRONG && fin >> w >> c) {
            wrongs[count].word = w;
            wrongs[count].count = c;
            count++;
        }
        fin.close();
    }

    // 查找是否已存在
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (wrongs[i].word == word) {
            wrongs[i].count++;
            found = 1;
            break;
        }
    }

    // 不存在且未达上限则新增
    if (!found) {
        if (count < MAX_WRONG) {
            wrongs[count].word = word;
            wrongs[count].count = 1;
            count++;
        }
    }

    std::ofstream fout(resolveDataPath(WRONG_FILE));
    if (fout.is_open()) {
        for (int i = 0; i < count; i++) {
            fout << wrongs[i].word << " " << wrongs[i].count << "\n";
        }
    }
}

void removeWrongWord(const std::string& word)
{
    WrongWord wrongs[MAX_WRONG];
    int count = 0;

    std::ifstream fin(resolveDataPath(WRONG_FILE));
    if (fin.is_open()) {
        std::string w;
        int c;
        while (count < MAX_WRONG && fin >> w >> c) {
            wrongs[count].word = w;
            wrongs[count].count = c;
            count++;
        }
        fin.close();
    }

    // 过滤掉目标单词
    std::ofstream fout(resolveDataPath(WRONG_FILE));
    if (fout.is_open()) {
        for (int i = 0; i < count; i++) {
            if (wrongs[i].word != word) {
                fout << wrongs[i].word << " " << wrongs[i].count << "\n";
            }
        }
    }
}

void showWrongWords(void (*callback)(const WrongWord*, int rank, int total))
{
    WrongWord wrongs[MAX_WRONG];
    int count = 0;

    std::ifstream fin(resolveDataPath(WRONG_FILE));
    if (!fin.is_open()) return;
    std::string w;
    int c;
    while (count < MAX_WRONG && fin >> w >> c) {
        wrongs[count].word = w;
        wrongs[count].count = c;
        count++;
    }

    if (count == 0) return;

    // 按错误次数降序排列
    std::sort(wrongs, wrongs + count,
              [](const WrongWord& a, const WrongWord& b) { return a.count > b.count; });

    for (int i = 0; i < count; i++) {
        callback(&wrongs[i], i + 1, count);
    }
}

int countWrongWords()
{
    std::ifstream fin(resolveDataPath(WRONG_FILE));
    if (!fin.is_open()) return 0;
    int count = 0;
    std::string line;
    while (std::getline(fin, line)) {
        if (line.size() > 1) count++;
    }
    return count;
}

DictNode** loadWrongWordsToArray(DictNode* root, int* count)
{
    WrongWord wrongs[MAX_WRONG];
    int n = 0;

    std::ifstream fin(resolveDataPath(WRONG_FILE));
    if (!fin.is_open()) { *count = 0; return nullptr; }
    std::string w;
    int c;
    while (n < MAX_WRONG && fin >> w >> c) {
        wrongs[n].word = w;
        wrongs[n].count = c;
        n++;
    }

    if (n == 0) { *count = 0; return nullptr; }

    DictNode** arr = new DictNode*[n];
    int found = 0;
    for (int i = 0; i < n; i++) {
        DictNode* node = searchWord(root, wrongs[i].word);
        if (node != nullptr) {
            arr[found++] = node;
        }
    }
    *count = found;
    if (found == 0) { delete[] arr; return nullptr; }
    return arr;
}

void clearWrongWords()
{
    std::ofstream fout(resolveDataPath(WRONG_FILE), std::ios::trunc);
    fout.close();
}

// ================================================================
// 测验历史
// ================================================================

void saveQuizRecord(int mode, int correct, int total)
{
    if (total <= 0) return;
    if (correct > total) correct = total;
    if (correct < 0) correct = 0;

    // 生成时间戳
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    std::string timestamp;
    if (t != nullptr) {
        char tsBuf[32];
        strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%dT%H:%M", t);
        timestamp = tsBuf;
    } else {
        timestamp = "unknown";
    }

    // 读取已有记录
    std::vector<QuizRecord> records;
    std::string path = resolveDataPath(QUIZ_HISTORY_FILE);
    std::ifstream fin(path);
    if (fin.is_open()) {
        std::string line;
        while (std::getline(fin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            // 格式：timestamp|mode|correct|total
            size_t p1 = line.find('|');
            size_t p2 = line.find('|', p1 + 1);
            size_t p3 = line.find('|', p2 + 1);
            if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
                continue;
            QuizRecord r;
            r.timestamp = line.substr(0, p1);
            try {
                r.mode    = std::stoi(line.substr(p1 + 1, p2 - p1 - 1));
                r.correct = std::stoi(line.substr(p2 + 1, p3 - p2 - 1));
                r.total   = std::stoi(line.substr(p3 + 1));
            } catch (const std::exception&) {
                continue;
            }
            records.push_back(r);
        }
        fin.close();
    }

    // 新记录插入头部
    QuizRecord nr;
    nr.timestamp = timestamp;
    nr.mode      = mode;
    nr.correct   = correct;
    nr.total     = total;
    records.insert(records.begin(), nr);

    // 超过上限则截断
    if (static_cast<int>(records.size()) > MAX_HISTORY) {
        records.resize(MAX_HISTORY);
    }

    // 写回文件
    std::ofstream fout(path);
    if (fout.is_open()) {
        for (const auto& r : records) {
            fout << r.timestamp << "|" << r.mode << "|"
                 << r.correct << "|" << r.total << "\n";
        }
    }
}

std::vector<QuizRecord> loadQuizHistory()
{
    std::vector<QuizRecord> records;
    std::ifstream fin(resolveDataPath(QUIZ_HISTORY_FILE));
    if (!fin.is_open()) return records;

    std::string line;
    while (std::getline(fin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t p1 = line.find('|');
        size_t p2 = line.find('|', p1 + 1);
        size_t p3 = line.find('|', p2 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
            continue;
        QuizRecord r;
        r.timestamp = line.substr(0, p1);
        try {
            r.mode    = std::stoi(line.substr(p1 + 1, p2 - p1 - 1));
            r.correct = std::stoi(line.substr(p2 + 1, p3 - p2 - 1));
            r.total   = std::stoi(line.substr(p3 + 1));
        } catch (const std::exception&) {
            continue;
        }
        records.push_back(r);
    }
    return records;
}

// ================================================================
// 词性分布统计
// ================================================================

static void collectPOS(DictNode* root, std::string posTypes[], int posCounts[],
                        int* typeCount)
{
    if (root == nullptr) return;
    collectPOS(root->left, posTypes, posCounts, typeCount);

    int found = 0;
    for (int i = 0; i < *typeCount; i++) {
        if (root->pos == posTypes[i]) {
            posCounts[i]++;
            found = 1;
            break;
        }
    }
    if (!found && *typeCount < 64) {
        posTypes[*typeCount] = root->pos;
        posCounts[*typeCount] = 1;
        (*typeCount)++;
    }

    collectPOS(root->right, posTypes, posCounts, typeCount);
}

void countByPOS(DictNode* root,
                void (*callback)(const std::string& pos, int count))
{
    if (root == nullptr) return;
    std::string posTypes[64];
    int posCounts[64] = {0};
    int typeCount = 0;
    collectPOS(root, posTypes, posCounts, &typeCount);

    for (int i = 0; i < typeCount; i++) {
        callback(posTypes[i], posCounts[i]);
    }
}

void getPOSStats(DictNode* root, std::vector<POSStat>& stats)
{
    stats.clear();
    if (root == nullptr) return;
    std::string posTypes[64];
    int posCounts[64] = {0};
    int typeCount = 0;
    collectPOS(root, posTypes, posCounts, &typeCount);

    for (int i = 0; i < typeCount; i++) {
        POSStat s;
        s.pos = posTypes[i];
        s.count = posCounts[i];
        stats.push_back(s);
    }
    // 按数量降序排列
    std::sort(stats.begin(), stats.end(),
              [](const POSStat& a, const POSStat& b) { return a.count > b.count; });
}

// ================================================================
// 单词有效性校验
// 规则：
//   1. 必须以字母开头
//   2. 长度 1-45
//   3. 只能包含字母和连字符
//   4. 不能以连字符结尾
//   5. 不能有连续两个连字符
//   6. 连字符两侧必须是字母（不能挨着数字等）
// ================================================================

int inputCheck(const std::string& word)
{
    int len = static_cast<int>(word.size());

    // Rule 1: 必须以字母开头
    if (len == 0 || !isalpha(static_cast<unsigned char>(word[0])))
        return 1;

    // Rule 2: 长度 1-45
    if (len < 1 || len > 45)
        return 2;

    bool prevIsHyphen = false;
    bool prevIsAlpha  = true;   // word[0] 已确认是字母

    for (int i = 1; i < len; i++) {
        char ch = word[i];
        bool isAlpha  = isalpha(static_cast<unsigned char>(ch)) != 0;
        bool isHyphen = (ch == '-');

        // Rule 3: 只能包含字母和连字符
        if (!isAlpha && !isHyphen)
            return 3;

        // Rule 5: 不能有连续两个连字符
        if (isHyphen && prevIsHyphen)
            return 5;

        // Rule 6: 连字符两侧必须是字母
        if (isHyphen && !prevIsAlpha)
            return 6;

        prevIsHyphen = isHyphen;
        prevIsAlpha  = isAlpha;
    }

    // Rule 4: 不能以连字符结尾
    if (prevIsHyphen)
        return 4;

    return 0;
}
