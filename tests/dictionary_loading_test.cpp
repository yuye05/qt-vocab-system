#include "core/dictionary.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <tuple>

static void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::string lower(std::string word)
{
    for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return word;
}

static int inspect(DictNode* node, const std::string* low = nullptr,
                   const std::string* high = nullptr, bool balanced = true)
{
    if (!node) return 0;
    const auto key = lower(node->word);
    require(!low || *low < key, "BST lower bound violated");
    require(!high || key < *high, "BST upper bound violated");
    int left = inspect(node->left, low, &key, balanced);
    int right = inspect(node->right, &key, high, balanced);
    require(!balanced || std::abs(left - right) <= 1, "loaded tree is not balanced");
    return 1 + std::max(left, right);
}

using Entry = std::tuple<std::string, std::string, std::string>;
static std::vector<Entry> collected;
static void collect(const DictNode* node)
{
    collected.emplace_back(node->word, node->pos, node->meaning);
}

static std::vector<Entry> snapshot(DictNode* root)
{
    collected.clear();
    displayAll(root, collect);
    return collected;
}

static std::string word(int i)
{
    std::ostringstream s;
    s << "word" << std::setfill('0') << std::setw(4) << i;
    return s.str();
}

int main()
{
    const auto dir = std::filesystem::temp_directory_path() /
        ("vocab-loading-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    const auto input = dir / "input.txt";
    const auto saved = dir / "saved.txt";
    const auto unicodeDir = dir / std::filesystem::u8path(u8"中文目录");
    const auto unicodeInput = unicodeDir / std::filesystem::u8path(u8"词典.txt");
    const auto unicodeSaved = unicodeDir / std::filesystem::u8path(u8"保存.txt");
    auto load = [&](const std::string& text, DictNode* root = nullptr) {
        std::ofstream out(input, std::ios::binary);
        out << text;
        out.close();
        return loadFromFile(root, input.string().c_str());
    };

    int result = 0;
    try {
        // Same keys and meanings for sorted, reverse-sorted and shuffled files.
        std::vector<int> order(1023);
        for (int i = 0; i < 1023; ++i) order[i] = i;
        std::mt19937 rng(42);
        for (int mode = 0; mode < 3; ++mode) {
            if (mode == 1) std::reverse(order.begin(), order.end());
            if (mode == 2) std::shuffle(order.begin(), order.end(), rng);
            std::string text;
            for (int i : order) text += word(i) + "  n.meaning" + std::to_string(i) + "\r\n";
            DictNode* root = load(text);
            require(countWords(root) == 1023, "word count mismatch");
            require(inspect(root) == 10, "unexpected height for 1023 keys");
            for (int i = 0; i < 1023; ++i) {
                auto* node = searchWord(root, word(i));
                require(node && node->meaning == "meaning" + std::to_string(i), "lookup/data mismatch");
            }
            require(searchWord(root, "WORD0000") != nullptr, "case-insensitive lookup failed");
            require(searchWord(root, "absent") == nullptr, "missing key was found");
            collected.clear();
            prefixSearch(root, "WORD000", collect);
            require(collected.size() == 10, "prefix results changed");

            // Exercise updates, leaf/one-child/two-child deletion and save/reload.
            root = insertWord(root, "WORD0001", "v.", "updated");
            require(searchWord(root, "word0001")->meaning == "updated", "update failed");
            root = deleteWord(root, "word0000");
            root = deleteWord(root, "word0001");
            const std::string rootWord = root->word;
            root = deleteWord(root, rootWord);
            root = insertWord(root, "word9999", "n.", "added");
            root = deleteWord(root, "absent");
            require(countWords(root) == 1021, "count after editing mismatch");
            inspect(root, nullptr, nullptr, false);
            const auto expected = snapshot(root);
            require(saveToFile(root, saved.string().c_str()) == 1, "save failed");
            DictNode* reloaded = loadFromFile(nullptr, saved.string().c_str());
            inspect(reloaded);
            require(snapshot(reloaded) == expected, "save/load changed dictionary contents");
            freeTree(root);
            freeTree(reloaded);
        }
        std::cout << "PASS: input order, balanced height, every lookup, prefix, editing, round-trip\n";

        for (const auto& text : {std::string(""), std::string("broken\nword  nodot\nword  n.\n")})
            require(load(text) == nullptr, "empty/invalid file should produce an empty tree");
        DictNode* single = load("only  n.one\n");
        require(countWords(single) == 1 && inspect(single) == 1, "single-word file failed");
        freeTree(single);
        DictNode* root = load("zebra  n.last\r\nApple  n.old\r\nbanana  n.middle\r\nAPPLE  v.new\r\n");
        require(countWords(root) == 3, "case-insensitive dedup failed");
        auto* apple = searchWord(root, "apple");
        require(apple && apple->word == "Apple" && apple->pos == "v." && apple->meaning == "new",
                "duplicate policy failed");
        inspect(root);
        require(loadFromFile(root, (dir / "missing.txt").string().c_str()) == root,
                "missing file changed existing root");
        auto* originalRoot = root;
        root = load("APPLE  adj.latest\ncherry  n.new\n", root);
        require(root == originalRoot && searchWord(root, "apple") == apple,
                "merge invalidated existing node pointers");
        require(countWords(root) == 4 && apple->meaning == "latest", "merge data mismatch");
        inspect(root, nullptr, nullptr, false);
        freeTree(root);
        std::cout << "PASS: empty/invalid/single input, duplicates, missing file, existing-tree merge\n";

        std::filesystem::create_directory(unicodeDir);
        {
            std::ofstream out(unicodeInput, std::ios::binary);
            out << "apple  n.fruit\n";
        }
        {
            std::ofstream out(unicodeSaved, std::ios::binary);
        }
        setDataSearchDirs({unicodeDir.u8string()});
        DictNode* unicodeRoot = loadFromFile(nullptr, u8"词典.txt");
        require(unicodeRoot && countWords(unicodeRoot) == 1, "Unicode path load failed");
        require(saveToFile(unicodeRoot, u8"保存.txt") == 1, "Unicode path save failed");
        freeTree(unicodeRoot);
        setDataSearchDirs({});
        require(std::filesystem::file_size(unicodeSaved) > 0, "Unicode path output is empty");
        std::cout << "PASS: Unicode directory and file names\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        result = 1;
    }
    std::filesystem::remove(input);
    std::filesystem::remove(saved);
    std::filesystem::remove(unicodeInput);
    std::filesystem::remove(unicodeSaved);
    std::filesystem::remove(unicodeDir);
    std::filesystem::remove(dir);
    return result;
}
