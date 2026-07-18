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

## 技术栈

- **语言**：C++17
- **框架**：Qt 6.x（Core / GUI / Widgets）
- **样式**：QSS 自制 Organic 暖色设计系统
- **构建**：qmake（`.pro` 工程文件）
- **词典结构**：二叉搜索树（BST），加载时 Fisher-Yates 打乱防退化

---

## 项目结构

```
qt-vocab-system/
├── VocabularySystem.pro   # Qt 工程文件（qmake）
├── main.cpp               # 程序入口，QSS 样式加载
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
│   ├── dictionary.txt     # 词库（~9000+ 词条）
│   └── wrong_words.txt    # 生词本记录
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

---

## 数据文件格式

- **`words/dictionary.txt`**：每行一条，格式为 `单词  词性释义`（单词与释义之间用两个空格分隔）
- **`words/wrong_words.txt`**：每行一条，格式为 `单词 错误次数`，由程序自动维护

---

## License

本项目采用 [MIT License](LICENSE) 开源协议。你可以自由使用、修改、分发本项目代码。
