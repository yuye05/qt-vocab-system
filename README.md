# 词汇学习与测验系统

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt](https://img.shields.io/badge/Qt-6.x-green)
![License](https://img.shields.io/badge/License-MIT-yellow)

一个基于 Qt 6 的桌面端英汉词汇学习与测验工具，支持词典管理、三种测验模式、生词本追踪和卡片式复习。

---

## 功能特性

| 模块 | 功能 | 说明 |
|------|------|------|
| 🏠 **首页仪表盘** | 词汇量统计 | 显示词典总词数、生词本数量、词性种类 |
| | 词性分布图 | 柱状图展示词性分布（Top 6） |
| | 测验历史 | 最近测验记录 + 累计正确率 |
| 📖 **词典管理** | 单词搜索 | 支持精确查找和前缀模糊匹配 |
| | 添加单词 | 带输入校验（语言规则检查），一键入库 |
| | 删除单词 | 选中删除，二次确认 |
| | 词典浏览 | 字母序排列，表格化展示 |
| 📝 **测验系统** | 拼写模式 | 看中文释义，手动拼写英文单词 |
| | 英→中模式 | 看英文，从 4 个选项中选择正确中文 |
| | 中→英模式 | 看中文，从 4 个选项中选择正确英文 |
| | 题量可调 | 5 / 10 / 15 / 20 题自由选择 |
| | 键盘快捷键 | 数字键选答案，回车/空格翻题 |
| ❌ **生词本** | 自动收录 | 测验答错自动加入，按错误次数排序 |
| | 专项测验 | 针对生词本进行三种模式的强化训练 |
| | 卡片复习 | 闪卡式翻面复习，掌握/未掌握标记 |
| | 扣题机制 | 专项测验答对后自动从生词本移除 |

---

## 测验流程

```mermaid
flowchart LR
    A[选择模式<br/>拼写/英中/中英] --> B[选择题量<br/>5/10/15/20]
    B --> C[答题循环]
    C --> D{答对?}
    D -->|是| E[下一题]
    D -->|否| F[记入生词本]
    F --> E
    E --> G{还有题?}
    G -->|是| C
    G -->|否| H[结果页<br/>正确率/错词列表]
    H --> I[生词本专项训练]
```

---

## 技术栈

- **语言**：C++ 17
- **框架**：Qt 6.x（Core / GUI / Widgets）
- **样式**：QSS 自主设计 Organic 暖色设计系统
- **构建**：qmake（`.pro` 工程文件）
- **词典结构**：二叉搜索树（BST），启动加载时从有序词条递归取中点，直接构建平衡形态

---

## 架构设计

- **页面栈导航**：主窗口侧边栏切换 4 个页面（`QStackedWidget`），各页面独立 Widget，解耦模块
- **BST 词典**：加载完成时树高为 O(log n)；运行期间按普通 BST 增删查改，耗时取决于树高，最坏 O(n)。前缀查询目前遍历整棵树
- **多路径资源回退**：QSS 从 `applicationDirPath()` 出发多路径搜索；词库和生词本优先从 EXE 同级的 `words/` 读取，并兼容 Qt Creator 的构建目录。数据路径在主窗口加载词典前统一设置，避免被重复配置覆盖
- **数据持久化**：生词本、测验历史以纯文本文件存储，程序启动时加载、退出时落盘

---

## 词库加载优化（2026-09）

原实现先用 Fisher-Yates 打乱词条，再逐个插入普通 BST，以降低有序输入造成树退化的风险。现在针对“启动时批量加载、运行时查询和少量修改”的使用方式，改为**有序词条直接构建平衡 BST**：

1. 读取并解析有效词条，保留原有文件格式及文本字节编码。
2. 按与查询一致的、忽略大小写的比较规则检查顺序；已有序则跳过排序，乱序时使用稳定排序。
3. 原地合并重复单词：保留首次出现的拼写，以文件中最后一条词性和释义为准。原先加载时随机洗牌会使重复条目的覆盖顺序不确定，本次将其固定下来。
4. 取中间词条为根，递归处理左右两半，直接连接节点，无需再逐项搜索插入位置。

例如，按字母序排列的 7 个单词会构建为：

```text
          dog
         /   \
       bee   fox
      /  \   /  \
    ant  cat eel hen
```

设 n 为解析出的词条数量，以下复杂度按字符串比较和复制成本视作常数计算：

| 输入或操作 | 当前实现 |
| --- | --- |
| 已有序词库，空树加载 | 顺序检查、去重、建树均为 O(n) |
| 乱序词库，空树加载 | 先排序，总体通常为 O(n log n) |
| 加载后的精确查询 | 树高为 O(log n)，沿树查找 |
| 向已有树加载文件 | 保留按文件顺序逐项合并的行为，不重新平衡、不替换已有节点 |

程序保存词典时采用中序遍历，因此保存后的词库可走有序加载流程。仓库现有的 3700 个词条按“忽略大小写”规则并非完全有序，第一次加载仍需要排序。

**这不是 AVL 或红黑树。** 平衡保证针对空树批量加载完成的时刻；后续大量连续增删仍可能使树变高。测验题序和选项的 Fisher-Yates 洗牌继续保留。此次中点建树优化、测试和文档由 Codex 辅助完成。

---

## 项目结构

```
qt-vocab-system/
├── VocabularySystem.pro   # Qt 工程文件（qmake）
├── main.cpp               # 程序入口，QSS 多路径加载 + 数据目录注册
├── core/
│   ├── dictionary.h       # 核心数据结构与函数声明
│   └── dictionary.cpp     # BST 增删查改 + 测验逻辑 + 生词本
├── ui/
│   ├── mainwindow.h/cpp   # 主窗口：侧边导航 + 页面栈
│   ├── homewidget.h/cpp   # 首页仪表盘（统计卡片 + 词性图 + 历史）
│   ├── dictwidget.h/cpp   # 词典管理页（搜索表格 + 增删词）
│   ├── quizwidget.h/cpp   # 测验页（设置 / 答题 / 结果 三页）
│   └── wrongwordswidget.h/cpp  # 生词本页（列表 / 测验 / 卡片复习）
├── style/
│   └── app.qss            # 全局 QSS 样式表
├── words/
│   ├── dictionary.txt     # 词库（约 3700 词条）
│   └── wrong_words.txt    # 生词本记录
├── tests/
│   ├── dictionary_loading_test.cpp  # 加载、查找、编辑、保存重载回归测试
│   ├── loading_benchmark.cpp        # 词库内容、树高、加载及查询耗时对比
│   ├── gui_loading_smoke.cpp        # 实际界面词库与操作回归测试
│   └── gui_loading_smoke.pro        # Qt 测试工程
├── .gitignore
└── LICENSE                # MIT
```

---

## 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows / Linux / macOS |
| Qt 版本 | Qt 6.5+ |
| 编译器 | MSVC 2019+ / MinGW 13.1+ / GCC 9+ / Clang |
| C++ 标准 | C++17 |

> ⚠️ **MinGW 版本注意**：Qt 6.11 自带 MinGW 13.1，但系统 PATH 里可能有旧的 MinGW 8.1（`C:/mingw64/bin`）。命令行手动 qmake 时旧版会报模板/链接错误，**建议直接在 Qt Creator 里构建**（自动用正确工具链）。

---

## 本地部署

1. **安装 Qt**：从 [Qt 官网](https://www.qt.io/download) 下载 Qt 6（推荐 6.5 以上），安装时勾选 MinGW 或 MSVC 组件
2. **克隆仓库**：

   ```bash
   git clone https://github.com/yuye05/qt-vocab-system.git
   ```

3. **打开工程**：启动 Qt Creator → 文件 → 打开文件或项目 → 选择 `VocabularySystem.pro`
4. **编译运行**：`Ctrl+R` 一键编译运行

---

## 使用说明

程序启动后，左侧边栏有 4 个标签页：

1. **首页** — 总览词汇量、词性分布和测验历史
2. **词典** — 浏览全部词库，支持搜索、添加和删除单词
3. **测验** — 选择测验模式（拼写 / 英→中 / 中→英）和题量，开始答题
4. **生词本** — 查看答错的词汇，可进行专项测验或卡片式复习

### 键盘操作

| 按键 | 功能 |
|------|------|
| `1` / `2` / `3` / `4` | 选择题答案 |
| `Enter` | 确认 / 下一题 |
| `Space` | 翻卡（卡片复习模式） |

---

## 数据文件格式

- **`words/dictionary.txt`**：每行一条，格式为 `单词  词性释义`（单词与释义之间用两个空格分隔）
- **`words/wrong_words.txt`**：每行一条，格式为 `单词 错误次数`，由程序自动维护

---

## 已知问题与开发注意事项

1. **资源路径**：直接运行发布版时，`dictionary.txt` 与 `wrong_words.txt` 应位于 EXE 同级的 `words/`。Qt Creator 构建仍会尝试构建根目录和源码目录；若打开后首页显示 0 词，先检查发布目录的 `words/dictionary.txt`
2. **Shadow build 复制**：`.pro` 中 `COPIES` 同时配置了 `release/` 和 `debug/`，将数据文件复制到 `$$OUT_PWD/release` 与 `$$OUT_PWD/debug`，确保两种构建模式下 EXE 旁都有数据文件和样式
3. **中文编码**：MSVC 下 `.pro` 已配置 `/source-charset:utf-8`；MinGW 下无需配置，但要保证源文件是 UTF-8 编码

---

## 优化验证与本机实测

2026-09-24，Windows、Qt 6.11.0 配套 MinGW GCC 13.1.0，使用 `-std=c++17 -O2`，对比改动前的 `6c256c6` 与本次实现。测试原始词库及仅重新排序的同一份词库，均为 3700 个词条。

每种组合先预热 1 次，再以种子 1～31 进行 31 轮加载。每轮遍历全部词条进行 20 次全量精确查找，核对返回节点。耗时取中位数；“全词库查询”是查询全部 3700 个单词一次的耗时，树高以根为第 1 层。

| 输入 | 实现 | 树高范围 | 加载中位数 | 全词库查询中位数 |
| --- | --- | --- | --- | --- |
| 原始词库（需排序） | 随机打乱后插入 | 24～32 | 1.825 ms | 0.497 ms |
| 原始词库（需排序） | 本次直接建树 | 12 | 2.205 ms | 0.422 ms |
| 已排序词库 | 随机打乱后插入 | 23～32 | 1.838 ms | 0.501 ms |
| 已排序词库 | 本次直接建树 | 12 | 1.188 ms | 0.437 ms |

四组加载结果的中序词条内容校验值一致（包括单词、词性及释义原始字节），全量查找均通过。树高稳定在 12 层，有序词库加载更快；**现有乱序词库的首次加载略慢**，所以本次优化不能理解为所有输入下都更快。毫秒耗时会随机器负载变化，以上只反映本机该次测试。

回归测试还覆盖：有序、逆序、乱序输入；大小写查询和重复覆盖；空文件、无效行和单词条；前缀结果；增删改；保存后重载内容一致；文件缺失和向已有树合并。

在配置好 MinGW 13.1 的 PowerShell 终端中，从仓库根目录执行：

```powershell
# 核心逻辑测试，不依赖 Qt GUI 库；可执行文件写入系统临时目录
g++ -std=c++17 -O2 -Wall -Wextra -I. tests/dictionary_loading_test.cpp core/dictionary.cpp -o "$env:TEMP/vocab-loading-test.exe"
if ($LASTEXITCODE -ne 0) { throw "测试编译失败" }
& "$env:TEMP/vocab-loading-test.exe"
if ($LASTEXITCODE -ne 0) { throw "回归测试失败" }

# 当前版本的原始词库基准
g++ -std=c++17 -O2 -I. tests/loading_benchmark.cpp core/dictionary.cpp -o "$env:TEMP/vocab-loading-benchmark.exe"
if ($LASTEXITCODE -ne 0) { throw "基准编译失败" }
& "$env:TEMP/vocab-loading-benchmark.exe" words/dictionary.txt
```

本次同时完成了 Qt 6.11.0 / MinGW 13.1 的 Release 完整构建。上述回归测试针对词典核心逻辑，不等同于全部界面功能的自动化测试。

针对“程序能打开但词库为空”的问题，还增加了 Qt 界面回归测试：在独立的测试目录放置词库副本，检查首页显示 3700 词，并实际操作搜索、添加、删除、测验答题和生词卡片。原版在“首页显示 3700 词”处失败；修复数据目录配置后全部通过。测试只写入测试目录内的词库副本。

使用安装了 Qt Widgets 与 Qt Test 模块的 Qt 6.11.0 MinGW Kit，从仓库根目录执行：

```powershell
New-Item -ItemType Directory -Force build/gui-smoke/words | Out-Null
Copy-Item words/dictionary.txt,words/wrong_words.txt build/gui-smoke/words/
Push-Location build/gui-smoke
qmake ../../tests/gui_loading_smoke.pro CONFIG+=release
mingw32-make -j4
$env:QT_QPA_PLATFORM = 'offscreen'
./gui_loading_smoke.exe
Pop-Location
```

应用的数据目录在主窗口初始化时统一设置，发布版优先读取 EXE 同级的 `words/`；直接运行已打包的 `build/release/VocabularySystem.exe` 即可加载词库。

---

## License

本项目采用 [MIT License](LICENSE) 开源协议。你可以自由使用、修改、分发本项目代码。
