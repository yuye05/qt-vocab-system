# 词汇学习与测验系统

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt](https://img.shields.io/badge/Qt-6.x-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

用 C++17 和 Qt 6 编写的英汉词汇学习小程序，附带 3700 个词条。可以查词、管理词库，做拼写或选择题，再用生词本复习答错的单词。

## 界面预览

![英→中测验页面：题目、四个选项、答题反馈与左侧导航](docs/images/quiz-preview.png)

英→中选择题的答题界面。选定答案后显示判分结果，再进入下一题。

## 功能

| 页面 | 可以做什么 |
| --- | --- |
| 首页 | 查看词库数量、词性分布、最近 20 次测验及这些记录的正确率 |
| 词库管理 | 按字母序浏览、前缀搜索、添加或更新词条、确认后删除 |
| 单词测验 | 拼写、英→中、中→英三种模式；可选 5 / 10 / 15 / 20 题，词条不足时按实际数量出题 |
| 生词本 | 自动收录错词并累计错误次数；支持专项测验和翻卡复习，完成专项测验后移除答对的词 |

选择题最多显示 4 个选项，排除与目标词同词性、同释义的干扰项。拼写题接受词库中同词性、同释义的等价答案，并显示本题目标词。

键盘操作：`1`～`4` 选择答案，`Enter` 提交或进入下一题，作答后也可按 `Space` 继续。卡片页用 `Enter` / `Space` 显示答案或标记掌握，`N` 标记未掌握，`Esc` 返回。

## 编译运行

需要 Qt 6.5+、支持 C++17 的编译器和 qmake。推荐用 Qt Creator 选择配套 Kit 构建，避免命令行选到旧版 MinGW。

```bash
git clone https://github.com/yuye05/qt-vocab-system.git
```

在 Qt Creator 中打开 `VocabularySystem.pro`，选择 Kit 后按 `Ctrl+R` 编译运行。首次启动需要找到初始词库，推荐放在程序同级的 `words/`；工程现有的复制规则也兼容程序同级根目录。

已验证的环境是 Windows / Qt 6.11.0 / MinGW 13.1。MSVC、Linux 和 macOS 尚未进行当前版本的实机验证。

## 实现要点

- 四个页面通过 `QStackedWidget` 切换，界面使用 Qt Widgets 和 QSS。
- 词典用二叉搜索树（BST）存储，按忽略大小写的英文单词排序。启动时批量构建平衡形态，运行期间按普通 BST 增删查改。
- 测验和卡片保存独立的词条快照，词典编辑不会使正在使用的题目引用失效。
- 文件统一使用 UTF-8，借助 Qt `QSaveFile` 原子写入。词典编辑先在临时树上进行，保存成功后替换当前词库。

### 词库加载优化

早期实现先打乱词条，再逐个插入 BST，以避免有序输入使树退化。现在先检查词条顺序，必要时稳定排序并去重，再递归取中点直接建树。重复单词保留首次拼写，词性和释义采用文件中的最后一条记录。

例如，7 个有序词条构建出的树是：

```text
          dog
         /   \
       bee   fox
      /  \   /  \
    ant  cat eel hen
```

已有序输入的加载为 O(n)，乱序输入需要排序，总体为 O(n log n)。空树加载完成后，树高为 O(log n)。这些复杂度将字符串比较和复制成本视作常数。

这是普通 BST，不是 AVL 或红黑树；后续连续增删不保证平衡，最坏操作耗时为 O(n)。前缀查询目前仍遍历整棵树。向已有树加载文件时按顺序合并，不重新平衡。测验题序和选项保留 Fisher-Yates 洗牌。

<details>
<summary>历史基准：3700 词的树高与查询耗时（2026-09-24）</summary>

Windows、Qt 6.11.0 配套 MinGW GCC 13.1.0，使用 `-std=c++17 -O2`，对比旧版本 `6c256c6` 与当时的直接建树实现。原始词库和重新排序后的词库均为 3700 词。预热 1 次后，以种子 1～31 测量 31 轮，耗时取中位数；每轮重复 20 次全量精确查找，逐条核对返回节点。

“全词库查询”指查找全部 3700 词一次，树高以根为第 1 层。

| 输入 | 实现 | 树高范围 | 加载中位数 | 全词库查询中位数 |
| --- | --- | --- | --- | --- |
| 原始词库（需排序） | 随机打乱后插入 | 24～32 | 1.825 ms | 0.497 ms |
| 原始词库（需排序） | 直接建树 | 12 | 2.205 ms | 0.422 ms |
| 已排序词库 | 随机打乱后插入 | 23～32 | 1.838 ms | 0.501 ms |
| 已排序词库 | 直接建树 | 12 | 1.188 ms | 0.437 ms |

四组中序词条内容校验值一致，全量查找均通过。直接建树把树高稳定在 12 层，有序词库加载更快，但原始乱序词库的首次加载略慢。

这是当时 GBK 词库与实现的本机实验记录。当前版本已改用 Qt Core 文件处理和 UTF-8，历史耗时不代表当前性能，也不代表界面搜索耗时。

</details>

## 用户数据

Windows 数据目录为 `%LOCALAPPDATA%/QtVocabSystem/`；其他平台使用 Qt 标准用户数据位置下的 `QtVocabSystem/`。

首次运行复制找到的初始词库、旧生词本和历史，不覆盖原文件或已有用户文件。以后直接读写用户目录，重新构建不会覆盖学习记录；缺失的生词本和历史创建为空文件，不重复导入旧记录。

| 文件 | 格式与用途 |
| --- | --- |
| 初始 `words/dictionary.txt` | UTF-8，每行 `单词  词性释义`；Windows 可迁移旧发布包中的 GBK 词库 |
| 用户 `dictionary.txt` | 编辑保存后为 UTF-8 TSV，首行 `# qt-vocab UTF-8 TSV v1`，随后每行 `单词<TAB>词性<TAB>释义` |
| `wrong_words.txt` | 每行 `单词 错误次数`，无固定的 200 词上限；普通及专项测验答错都会累计次数 |
| `quiz_history.txt` | 每行 `时间戳\|模式\|正确数\|题数`，保留最近 20 次 |

TSV 将词性与释义分开保存，支持 `noun`、`n. v.` 等输入，字段不能含制表符或换行。旧版程序不能读取新 TSV；新程序不把用户数据写回旧发布包目录。

损坏记录会提示文件路径和行号，并阻止覆盖原文件。按提示修正后重试；不要将文件另存为 ANSI 编码。

## 项目结构

```text
qt-vocab-system/
├── VocabularySystem.pro   # qmake 工程
├── main.cpp               # 程序入口与样式加载
├── core/                  # BST、文件读写、出题与学习记录
├── ui/                    # 首页、词库、测验、生词本页面
├── style/app.qss          # 全局样式
├── words/                 # 初始词库与生词记录
└── tests/                 # 核心回归、界面回归与加载基准
```

## 测试与开发

在配置好 Qt MinGW Kit 的 PowerShell 中，从仓库根目录运行：

```powershell
./tests/run_tests.ps1
```

也可以用 `-QtBin` 和 `-CompilerBin` 指定 Qt 与配套 MinGW 的 `bin` 目录。脚本会隔离用户数据，使用临时目录和词库副本；失败时返回非零退出码，并保留构建日志路径。

四组回归程序覆盖：

- [词库加载](tests/dictionary_loading_test.cpp)：排序去重、树高、查询、增删、保存重载和中文路径。
- [数据安全](tests/dictionary_safety_test.cpp)：GBK 迁移、UTF-8/TSV 重载、坏记录保护、生词容量、历史校验及等价答案。
- [界面冒烟](tests/gui_loading_smoke.cpp)：3700 词的页面加载、搜索、添加删除、答题和卡片揭示。
- [功能回归](tests/functional_regression.cpp)：空词库恢复、保存回滚、测验快照、小词库选择题、快捷键、专项训练和首页刷新。

核心测试链接 Qt Core，不需要 GUI。界面测试使用 Qt Widgets 和 Qt Test，可通过 [测试工程](tests/gui_loading_smoke.pro) 构建。

<details>
<summary>单独运行当前词库加载基准</summary>

```powershell
$benchmarkSource = (Resolve-Path tests/loading_benchmark.cpp).Path.Replace('\', '/')
New-Item -ItemType Directory -Force build/benchmark | Out-Null
Push-Location build/benchmark
qmake ../../tests/core_tests.pro "TEST_SOURCE=$benchmarkSource" CONFIG+=release CONFIG-=debug_and_release DESTDIR=.
if ($LASTEXITCODE -ne 0) { throw "qmake failed" }
mingw32-make -j4
if ($LASTEXITCODE -ne 0) { throw "benchmark compilation failed" }
./core_tests.exe ../../words/dictionary.txt
Pop-Location
```

</details>

BST 加载优化、回归测试和文档由 Codex 辅助完成。

## License

[MIT](LICENSE)。
