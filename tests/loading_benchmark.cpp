#include "core/dictionary.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

static int height(DictNode* node)
{
    return node ? 1 + std::max(height(node->left), height(node->right)) : 0;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: loading_benchmark <dictionary.txt>\n";
        return 2;
    }
    using Clock = std::chrono::steady_clock;
    std::vector<double> loads, lookups;
    int minHeight = std::numeric_limits<int>::max(), maxHeight = 0, count = 0;
    std::uint64_t expectedHash = 0;
    for (int run = 0; run <= 31; ++run) {
        std::srand(run); // Run 0 warms the file cache; seeds 1..31 are measured.
        auto start = Clock::now();
        DictNode* root = loadFromFile(nullptr, argv[1]);
        auto loaded = Clock::now();
        count = countWords(root);
        if (!count) { std::cerr << "Empty dictionary\n"; return 1; }
        std::vector<DictNode*> nodes(count);
        int index = 0;
        collectAllWords(root, nodes.data(), &index);
        // Compare the full ordered content, including POS and raw meaning bytes.
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto* node : nodes) {
            for (const auto* field : {&node->word, &node->pos, &node->meaning}) {
                for (unsigned char c : *field) { hash ^= c; hash *= 1099511628211ULL; }
                hash ^= 0; hash *= 1099511628211ULL;
            }
        }
        if (run == 0) expectedHash = hash;
        if (hash != expectedHash) { std::cerr << "Contents changed between runs\n"; return 1; }
        auto searching = Clock::now();
        for (int repeat = 0; repeat < 20; ++repeat)
            for (const auto* node : nodes)
                if (searchWord(root, node->word) != node) {
                    std::cerr << "Lookup mismatch\n";
                    return 1;
                }
        auto searched = Clock::now();
        if (run > 0) {
            int h = height(root);
            minHeight = std::min(minHeight, h);
            maxHeight = std::max(maxHeight, h);
            loads.push_back(std::chrono::duration<double, std::milli>(loaded - start).count());
            lookups.push_back(std::chrono::duration<double, std::milli>(searched - searching).count() / 20);
        }
        freeTree(root);
    }
    std::sort(loads.begin(), loads.end());
    std::sort(lookups.begin(), lookups.end());
    std::cout << "words=" << count << " content_hash=" << expectedHash
              << " height=" << minHeight << ".." << maxHeight
              << " load_median_ms=" << loads[loads.size() / 2]
              << " full_lookup_median_ms=" << lookups[lookups.size() / 2] << '\n';
}
