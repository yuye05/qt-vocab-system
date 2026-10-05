#include "dictionary.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>
#include <algorithm>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <cctype>
#include <sstream>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static std::vector<std::string> s_dataDirs;
static std::string s_error;

const std::string& dataError() { return s_error; }

void setDataSearchDirs(const std::vector<std::string>& dirs)
{
    s_dataDirs = dirs;
}

std::string resolveDataPath(const char* filename)
{
    const QString name = QString::fromUtf8(filename);
    if (QFileInfo(name).isAbsolute()) return filename;
    for (const auto& dir : s_dataDirs) {
        const QString path = QDir(QString::fromStdString(dir)).filePath(name);
        if (QFileInfo::exists(path)) return path.toStdString();
    }
    return s_dataDirs.empty() ? std::string(filename)
        : QDir(QString::fromStdString(s_dataDirs.front())).filePath(name).toStdString();
}

static bool readText(const std::string& path, std::string& text)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        s_error = path + ": " + file.errorString().toStdString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        s_error = path + ": " + file.errorString().toStdString();
        return false;
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString decoded = utf8(bytes);
    if (utf8.hasError()) {
        if (bytes.startsWith("# qt-vocab UTF-8 TSV v1") || bytes.startsWith("\xef\xbb\xbf")) {
            s_error = path + ": UTF-8 文件编码损坏，原文件未改动。";
            return false;
        }
        // 旧 Windows 发布包使用 GBK；迁移时显式解码，不依赖本机 ACP。
#ifdef _WIN32
        int size = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), nullptr, 0);
        if (size > 0) {
            std::wstring wide(size, L'\0');
            MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), wide.data(), size);
            decoded = QString::fromStdWString(wide);
        } else
#else
        QStringDecoder gbk("GB18030", QStringConverter::Flag::Stateless);
        if (gbk.isValid()) {
            decoded = gbk(bytes);
            if (gbk.hasError()) decoded.clear();
        } else decoded.clear();
        if (decoded.isEmpty())
#endif
        {
            s_error = path + ": 无法识别 UTF-8 / 旧 GBK 编码，原文件未改动。";
            return false;
        }
    }
    if (decoded.startsWith(QChar(0xfeff))) decoded.remove(0, 1);
    text = decoded.toStdString();
    return true;
}

static bool writeText(const std::string& path, const std::string& text)
{
    QSaveFile file(QString::fromStdString(path));
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size()) ||
        !file.commit()) {
        s_error = path + ": " + file.errorString().toStdString();
        return false;
    }
    return true;
}

bool initializeDataDirectory(const std::string& directory,
                             const std::vector<std::string>& legacyDirectories)
{
    s_error.clear();
    setDataSearchDirs({directory});
    if (!QDir().mkpath(QString::fromStdString(directory))) {
        s_error = "无法创建用户数据目录：" + directory;
        return false;
    }
    const std::string marker = QDir(QString::fromStdString(directory)).filePath(".initialized").toStdString();
    const bool firstUse = !QFileInfo::exists(QString::fromStdString(marker));
    std::vector<std::pair<std::string, std::string>> pending;
    for (const char* name : {DICT_FILE, WRONG_FILE, QUIZ_HISTORY_FILE}) {
        const std::string destination = resolveDataPath(name);
        if (QFileInfo::exists(QString::fromStdString(destination))) continue;
        std::string text;
        bool found = false;
        for (const auto& legacy : firstUse || std::string(name) == DICT_FILE
                ? legacyDirectories : std::vector<std::string>{}) {
            const std::string source = QDir(QString::fromStdString(legacy)).filePath(name).toStdString();
            if (!QFileInfo::exists(QString::fromStdString(source))) continue;
            if (!readText(source, text)) return false;
            found = true;
            break;
        }
        if (!found && std::string(name) == DICT_FILE) {
            s_error = "未找到初始词库，请在程序旁提供 words/dictionary.txt。";
            return false;
        }
        pending.emplace_back(destination, text);
    }
    // 先读取全部待迁移文件；初始化标记仅在所有文件成功落盘后写入，失败可安全重试。
    for (const auto& file : pending)
        if (!writeText(file.first, file.second)) return false;
    return !firstUse || writeText(marker, "initialized\n");
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

// 候选有限遍历：不足四项也会结束，并排除同释义干扰项。
std::vector<int> pickOptions(int correctIdx, const std::vector<WordEntry>& words)
{
    if (correctIdx < 0 || correctIdx >= static_cast<int>(words.size())) return {};
    std::vector<int> options{correctIdx};
    for (int candidate : shuffledIndices(static_cast<int>(words.size()))) {
        bool duplicate = std::any_of(options.begin(), options.end(), [&](int chosen) {
            return words[candidate].word == words[chosen].word ||
                   equivalentMeaning(words[candidate], words[chosen]);
        });
        if (!duplicate) options.push_back(candidate);
        if (options.size() == 4) break;
    }
    const auto order = shuffledIndices(static_cast<int>(options.size()));
    std::vector<int> shuffled;
    for (int i : order) shuffled.push_back(options[i]);
    return shuffled;
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
static DictNode* deleteNode(DictNode* root, const std::string& word)
{
    if (root == nullptr) return nullptr;

    int cmp = cmpIgnoreCase(word, root->word);
    if (cmp < 0) {
        root->left = deleteNode(root->left, word);
    } else if (cmp > 0) {
        root->right = deleteNode(root->right, word);
    } else {
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
            root->right = deleteNode(root->right, successor->word);
        }
    }
    return root;
}

DictNode* deleteWord(DictNode* root, const std::string& word)
{
    return deleteNode(root, word);
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

static bool parseDictionary(const std::string& text, std::vector<WordEntry>& entries)
{
    std::istringstream input(text);
    std::string line;
    int number = 0;
    bool valid = true;
    while (std::getline(input, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line == "# qt-vocab UTF-8 TSV v1") continue;
        WordEntry entry;
        size_t first = line.find('\t');
        size_t second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
        if (first != std::string::npos && second != std::string::npos &&
            line.find('\t', second + 1) == std::string::npos) {
            entry = {line.substr(0, first), line.substr(first + 1, second - first - 1), line.substr(second + 1)};
        } else if (first == std::string::npos) {
            size_t delimiter = line.find("  ");
            if (delimiter != std::string::npos) {
                const std::string rest = line.substr(delimiter + 2);
                size_t dot = rest.find('.');
                if (dot != std::string::npos)
                    entry = {line.substr(0, delimiter), rest.substr(0, dot + 1), rest.substr(dot + 1)};
            }
        }
        if (entry.word.empty() || entry.word.find_first_of(" \t\r\n") != std::string::npos ||
            entry.pos.empty() || entry.meaning.empty()) {
            if (valid) s_error = "词库第 " + std::to_string(number) + " 行格式错误，原文件未改动。";
            valid = false;
            continue;
        }
        entries.push_back(std::move(entry));
    }
    return valid;
}

int saveToFile(DictNode* root, const char* filename)
{
    s_error.clear();
    const std::string path = resolveDataPath(filename);
    if (QFileInfo::exists(QString::fromStdString(path))) {
        std::string existing;
        std::vector<WordEntry> entries;
        if (!readText(path, existing)) return 0;
        if (!parseDictionary(existing, entries)) {
            s_error = path + ": " + s_error;
            return 0;
        }
    }
    std::string text = "# qt-vocab UTF-8 TSV v1\n";
    for (const auto& entry : wordSnapshot(root)) {
        if (entry.word.empty() || entry.pos.empty() || entry.meaning.empty() ||
            entry.word.find_first_of(" \t\r\n") != std::string::npos ||
            entry.pos.find_first_of("\t\r\n") != std::string::npos ||
            entry.meaning.find_first_of("\t\r\n") != std::string::npos) {
            s_error = "单词、词性、释义必须完整，且不能包含制表符或换行。";
            return 0;
        }
        text += entry.word + "\t" + entry.pos + "\t" + entry.meaning + "\n";
    }
    return writeText(path, text) ? 1 : 0;
}

// 从有序、去重的词条区间 [begin, end) 直接建树，左右子树规模最多相差 1。
static DictNode* buildBalancedTree(const std::vector<WordEntry>& entries,
                                 size_t begin, size_t end)
{
    if (begin == end) return nullptr;
    size_t mid = begin + (end - begin) / 2;
    const auto& entry = entries[mid];
    DictNode* node = new DictNode(entry.word, entry.pos, entry.meaning);
    node->left = buildBalancedTree(entries, begin, mid);
    node->right = buildBalancedTree(entries, mid + 1, end);
    return node;
}

DictNode* loadFromFile(DictNode* root, const char* filename)
{
    s_error.clear();
    const std::string path = resolveDataPath(filename);
    std::string text;
    if (!readText(path, text)) return root;
    std::vector<WordEntry> entries;
    if (!parseDictionary(text, entries)) s_error = path + ": " + s_error;

    // 保留加载到已有树时的合并行为，以及已有节点指针的有效性。
    if (root != nullptr) {
        for (const auto& entry : entries)
            root = insertWord(root, entry.word, entry.pos, entry.meaning);
        return root;
    }

    auto less = [](const WordEntry& a, const WordEntry& b) {
        return cmpIgnoreCase(a.word, b.word) < 0;
    };
    // 程序保存的词库已经有序；外部乱序词库才需要排序。
    if (!std::is_sorted(entries.begin(), entries.end(), less))
        std::stable_sort(entries.begin(), entries.end(), less);

    // 稳定排序保留同名单词的文件顺序：沿用首次拼写，最后一条释义生效。
    size_t uniqueCount = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (uniqueCount > 0 &&
            cmpIgnoreCase(entries[uniqueCount - 1].word, entries[i].word) == 0) {
            entries[uniqueCount - 1].pos = std::move(entries[i].pos);
            entries[uniqueCount - 1].meaning = std::move(entries[i].meaning);
        } else {
            if (uniqueCount != i) entries[uniqueCount] = std::move(entries[i]);
            ++uniqueCount;
        }
    }
    return buildBalancedTree(entries, 0, uniqueCount);
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

static void snapshotNodes(DictNode* root, std::vector<WordEntry>& entries)
{
    if (!root) return;
    snapshotNodes(root->left, entries);
    entries.push_back({root->word, root->pos, root->meaning});
    snapshotNodes(root->right, entries);
}

std::vector<WordEntry> wordSnapshot(DictNode* root)
{
    std::vector<WordEntry> entries;
    snapshotNodes(root, entries);
    return entries;
}

bool equivalentMeaning(const WordEntry& a, const WordEntry& b)
{
    return a.pos == b.pos && a.meaning == b.meaning;
}

bool spellingMatches(const std::vector<WordEntry>& words,
                     const WordEntry& target, const std::string& answer)
{
    return std::any_of(words.begin(), words.end(), [&](const WordEntry& entry) {
        return equivalentMeaning(entry, target) && cmpIgnoreCase(entry.word, answer) == 0;
    });
}

// ================================================================
// 生词本操作
// ================================================================

static bool positiveInteger(const std::string& text, int& value)
{
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc() && result.ptr == text.data() + text.size() && value > 0;
}

bool loadWrongWords(std::vector<WrongWord>& words)
{
    s_error.clear();
    words.clear();
    const std::string path = resolveDataPath(WRONG_FILE);
    if (!QFileInfo::exists(QString::fromStdString(path))) return true;
    std::string text;
    if (!readText(path, text)) return false;
    std::istringstream input(text);
    std::string line;
    int number = 0;
    bool valid = true;
    while (std::getline(input, line)) {
        ++number;
        if (line.empty() || line == "\r") continue;
        std::istringstream fields(line);
        std::string word, countText, extra;
        int count = 0;
        if (!(fields >> word >> countText) || (fields >> extra) || !positiveInteger(countText, count)) {
            if (valid) s_error = path + ": 第 " + std::to_string(number) + " 行格式错误，请修复该行后重试。";
            valid = false;
            continue;
        }
        auto found = std::find_if(words.begin(), words.end(), [&](const WrongWord& w) {
            return cmpIgnoreCase(w.word, word) == 0;
        });
        if (found == words.end()) words.push_back({word, count});
        else if (found->count <= INT_MAX - count) found->count += count;
        else {
            s_error = "生词错误次数超出可保存范围。";
            valid = false;
        }
    }
    return valid;
}

static bool saveWrongWords(const std::vector<WrongWord>& words)
{
    std::string text;
    for (const auto& w : words) text += w.word + " " + std::to_string(w.count) + "\n";
    return writeText(resolveDataPath(WRONG_FILE), text);
}

bool recordWrong(const std::string& word)
{
    std::vector<WrongWord> words;
    if (!loadWrongWords(words)) return false;
    if (word.empty() || word.find_first_of(" \t\r\n") != std::string::npos) {
        s_error = "生词格式不正确。";
        return false;
    }
    auto found = std::find_if(words.begin(), words.end(), [&](const WrongWord& w) {
        return cmpIgnoreCase(w.word, word) == 0;
    });
    if (found == words.end()) words.push_back({word, 1});
    else if (found->count < INT_MAX) ++found->count;
    else {
        s_error = "生词错误次数超出可保存范围。";
        return false;
    }
    return saveWrongWords(words);
}

bool removeWrongWords(const std::vector<std::string>& removed)
{
    std::vector<WrongWord> words;
    if (!loadWrongWords(words)) return false;
    words.erase(std::remove_if(words.begin(), words.end(), [&](const WrongWord& w) {
        return std::any_of(removed.begin(), removed.end(), [&](const std::string& word) {
            return cmpIgnoreCase(w.word, word) == 0;
        });
    }), words.end());
    return saveWrongWords(words);
}

bool removeWrongWord(const std::string& word)
{
    return removeWrongWords({word});
}

int countWrongWords()
{
    std::vector<WrongWord> words;
    loadWrongWords(words);
    return static_cast<int>(words.size());
}

// ================================================================
// 测验历史（最近 MAX_HISTORY 次，不表示终身累计统计）
// ================================================================

std::vector<QuizRecord> loadQuizHistory()
{
    s_error.clear();
    std::vector<QuizRecord> records;
    const std::string path = resolveDataPath(QUIZ_HISTORY_FILE);
    if (!QFileInfo::exists(QString::fromStdString(path))) return records;
    std::string text;
    if (!readText(path, text)) return records;
    std::istringstream input(text);
    std::string line;
    int number = 0;
    while (std::getline(input, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> fields;
        std::istringstream row(line);
        std::string field;
        while (std::getline(row, field, '|')) fields.push_back(field);
        QuizRecord record{};
        bool valid = fields.size() == 4 && !line.empty() && line.back() != '|';
        if (valid) {
            record.timestamp = fields[0];
            for (int i = 1; i < 4; ++i) {
                int& value = i == 1 ? record.mode : i == 2 ? record.correct : record.total;
                auto parsed = std::from_chars(fields[i].data(), fields[i].data() + fields[i].size(), value);
                valid = valid && parsed.ec == std::errc() && parsed.ptr == fields[i].data() + fields[i].size();
            }
            valid = valid && record.mode >= 0 && record.mode <= 3 &&
                record.total > 0 && record.correct >= 0 && record.correct <= record.total &&
                QDateTime::fromString(QString::fromStdString(record.timestamp), "yyyy-MM-dd'T'HH:mm").isValid();
        }
        if (!valid) {
            if (s_error.empty()) s_error = path + ": 第 " + std::to_string(number) + " 行格式错误，请修复该行后重试。";
            continue;
        }
        if (records.size() < MAX_HISTORY) records.push_back(record);
    }
    return records;
}

bool saveQuizRecord(int mode, int correct, int total)
{
    auto records = loadQuizHistory();
    if (!s_error.empty()) return false;
    if (mode < 0 || mode > 3 || total <= 0 || correct < 0 || correct > total) {
        s_error = "测验成绩不在有效范围内。";
        return false;
    }
    records.insert(records.begin(), {
        QDateTime::currentDateTime().toString("yyyy-MM-dd'T'HH:mm").toStdString(), mode, correct, total});
    if (records.size() > MAX_HISTORY) records.resize(MAX_HISTORY);
    std::string text;
    for (const auto& r : records)
        text += r.timestamp + "|" + std::to_string(r.mode) + "|" +
            std::to_string(r.correct) + "|" + std::to_string(r.total) + "\n";
    return writeText(resolveDataPath(QUIZ_HISTORY_FILE), text);
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
