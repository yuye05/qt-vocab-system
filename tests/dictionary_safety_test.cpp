#include "core/dictionary.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <set>

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static void put(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "fixture open failed");
    require(file.write(bytes) == bytes.size(), "fixture write failed");
}

static QByteArray get(const QString& path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "fixture read failed");
    return file.readAll();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    try {
        require(temporary.isValid(), "temporary directory failed");
        const QDir base(temporary.path());
        const QString legacy = base.filePath("旧数据");
        const QString user = base.filePath("用户数据");
        require(QDir().mkpath(legacy), "legacy directory failed");
        const QByteArray gbk("apple  n.\xc6\xbb\xb9\xfb\n");
        put(QDir(legacy).filePath(DICT_FILE), gbk);
        put(QDir(legacy).filePath(WRONG_FILE), "apple 2\n");
        put(QDir(legacy).filePath(QUIZ_HISTORY_FILE), "2026-10-05T10:00|0|1|1\n");
        require(initializeDataDirectory(user.toStdString(), {legacy.toStdString()}), "legacy migration failed");
        require(get(QDir(legacy).filePath(DICT_FILE)) == gbk, "migration changed original");
        require(get(QDir(user).filePath(DICT_FILE)).contains(QString("苹果").toUtf8()), "migration is not UTF-8");
        auto* root = loadFromFile(nullptr, DICT_FILE);
        require(root && root->meaning == "苹果", "GBK migration changed meaning");
        root = insertWord(root, "pear", "noun", "梨；🍐");
        root = insertWord(root, "act", "n. v.", "动作；行动");
        require(saveToFile(root, DICT_FILE), "UTF-8 / POS save failed");
        auto* loaded = loadFromFile(nullptr, DICT_FILE);
        require(searchWord(loaded, "pear")->meaning == "梨；🍐", "UTF-8 round trip failed");
        require(searchWord(loaded, "pear")->pos == "noun", "POS without dot lost");
        require(searchWord(loaded, "act")->pos == "n. v.", "multiple POS lost");
        require(initializeDataDirectory(user.toStdString(), {legacy.toStdString()}), "second initialization failed");
        auto* initialized = loadFromFile(nullptr, DICT_FILE);
        require(searchWord(initialized, "pear"), "initialization overwrote user edits");
        freeTree(initialized);
        freeTree(root);
        freeTree(loaded);
        require(QFile::remove(QDir(user).filePath(WRONG_FILE)), "missing wrong fixture failed");
        require(initializeDataDirectory(user.toStdString(), {legacy.toStdString()}) && countWrongWords() == 0,
                "deleted user wrong list resurrected legacy records");
        const auto validDictionary = get(QDir(user).filePath(DICT_FILE));
        const QByteArray invalidUtf8("# qt-vocab UTF-8 TSV v1\npear\tnoun\t\xb9\xfb\n");
        put(QDir(user).filePath(DICT_FILE), invalidUtf8);
        require(!loadFromFile(nullptr, DICT_FILE) && !dataError().empty(), "declared UTF-8 corruption decoded as GBK");
        require(!saveToFile(nullptr, DICT_FILE) && get(QDir(user).filePath(DICT_FILE)) == invalidUtf8,
                "corrupt UTF-8 overwritten");
        put(QDir(user).filePath(DICT_FILE), validDictionary);
        std::cout << "PASS: isolated directory, GBK migration, UTF-8/POS round trip, no overwrite\n";

        put(QDir(user).filePath(WRONG_FILE), "apple 1\nbroken nope\npear 3\n");
        const auto damaged = get(QDir(user).filePath(WRONG_FILE));
        std::vector<WrongWord> wrong;
        require(!loadWrongWords(wrong) && wrong.size() == 2, "malformed row hid later valid records");
        require(!recordWrong("act"), "damaged file should block writes");
        require(get(QDir(user).filePath(WRONG_FILE)) == damaged, "damaged file overwritten");
        require(!removeWrongWord("apple"), "damaged file should block removal");
        put(QDir(user).filePath(WRONG_FILE), "");
        for (int i = 0; i < 250; ++i) require(recordWrong("word" + std::to_string(i)), "wrong word insertion failed");
        require(countWrongWords() == 250, "200-word limit still exists");
        require(recordWrong("WORD0"), "case-insensitive wrong count failed");
        require(loadWrongWords(wrong) && wrong.size() == 250 && wrong[0].count == 2, "wrong word duplicated by case");
        require(removeWrongWords({"WORD0", "word1"}) && countWrongWords() == 248, "batch removal failed");
        require(!recordWrong("two words"), "invalid wrong word accepted");
        std::cout << "PASS: damaged-file preservation, dynamic wrong list, counting and batch removal\n";

        const QString history = QDir(user).filePath(QUIZ_HISTORY_FILE);
        put(history, "2026-10-05T10:00|1junk|8|2\n2026-10-05T10:00|0|1|1\n");
        const auto badHistory = get(history);
        require(loadQuizHistory().size() == 1 && !dataError().empty(), "invalid history accepted");
        require(!saveQuizRecord(0, 1, 1) && get(history) == badHistory, "damaged history overwritten");
        put(history, "");
        for (int i = 0; i < 25; ++i) require(saveQuizRecord(i % 4, 1, 2), "history write failed");
        require(loadQuizHistory().size() == 20, "history limit mismatch");
        require(!saveQuizRecord(5, 1, 2) && !saveQuizRecord(0, 3, 2), "invalid score accepted");
        std::cout << "PASS: strict history fields, preserved failures, recent-20 limit\n";

        const std::vector<WordEntry> words = {
            {"adequate", "a.", "适当的"}, {"proper", "a.", "适当的"},
            {"apple", "n.", "苹果"}, {"banana", "n.", "香蕉"}};
        require(spellingMatches(words, words[0], "PROPER"), "equivalent spelling not accepted");
        require(!spellingMatches(words, words[0], "apple"), "unrelated spelling accepted");
        for (size_t count = 1; count <= words.size(); ++count) {
            const std::vector<WordEntry> candidates(words.begin(), words.begin() + count);
            const auto options = pickOptions(0, candidates);
            require(!options.empty() && options.size() <= 4, "small pool failed");
            require(std::find(options.begin(), options.end(), 0) != options.end(), "correct option missing");
            std::set<std::pair<std::string, std::string>> meanings;
            for (int option : options) require(meanings.emplace(candidates[option].pos, candidates[option].meaning).second,
                                                "ambiguous distractor accepted");
        }
        require(pickOptions(-1, words).empty() && pickOptions(0, {}).empty(), "invalid option boundary failed");
        std::cout << "PASS: finite small-pool choices, unique meanings, equivalent spelling\n";

        root = insertWord(nullptr, "apple", "n.", "苹果");
        const auto snapshot = wordSnapshot(root);
        root = deleteWord(root, "apple");
        require(!root && snapshot[0].meaning == "苹果", "snapshot depended on deleted node");
        require(QDir().mkdir(base.filePath("blocked")), "blocked path fixture failed");
        require(!saveToFile(nullptr, base.filePath("blocked").toStdString().c_str()), "save failure reported success");
        put(base.filePath("not-a-directory"), "blocked");
        require(!initializeDataDirectory(base.filePath("not-a-directory").toStdString(), {}), "invalid directory accepted");
        require(resolveDataPath(DICT_FILE) != DICT_FILE, "failed initialization fell back to working directory");
        setDataSearchDirs({user.toStdString()});
        put(QDir(legacy).filePath(WRONG_FILE), QByteArray(1, '\x81'));
        const QString retry = base.filePath("retry");
        require(!initializeDataDirectory(retry.toStdString(), {legacy.toStdString()}), "broken migration source accepted");
        require(!QFile::exists(QDir(retry).filePath(DICT_FILE)), "failed migration partially copied dictionary");
        put(QDir(legacy).filePath(WRONG_FILE), "apple 2\n");
        require(initializeDataDirectory(retry.toStdString(), {legacy.toStdString()}) && countWrongWords() == 1,
                "failed migration could not safely retry");
        std::cout << "PASS: snapshot lifetime and atomic-write error reporting\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << " / " << dataError() << '\n';
        return 1;
    }
}
