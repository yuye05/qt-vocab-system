# 贡献指南

欢迎通过 [Issues](https://github.com/yuye05/qt-vocab-system/issues) 报告问题、提出功能建议，或提交 Pull Request。

## 开发环境

需要 Qt 6.5+ 的 Core、GUI、Widgets 模块、qmake 和支持 C++17 的编译器；界面测试另需 Qt Test。推荐使用 Qt Creator 配套的 Kit。

```bash
git clone https://github.com/yuye05/qt-vocab-system.git
cd qt-vocab-system
```

在 Qt Creator 打开 `VocabularySystem.pro` 并编译运行。源代码及词库采用 UTF-8，MSVC 工程已配置相应字符集。

目前完成实机验证的环境为 Windows / Qt 6.11.0 / MinGW 13.1。其他平台的修复或验证结果，请同时注明 Qt、编译器和系统版本。

## 报告问题

请描述操作步骤、预期行为、实际结果，以及系统和 Qt Kit 信息。词库或文件解析问题可提供最小复现词条；公开截图与日志时请隐去个人路径和私人学习记录。

功能建议请说明使用场景，以及现有功能为什么不能满足需求。

## 修改与验证

从 `main` 创建自己的分支，每个 Pull Request 尽量集中处理一个问题。沿用现有代码风格，行为变化应补充能复现问题的回归检查。

配置好 Qt MinGW Kit 后，在仓库根目录运行：

```powershell
./tests/run_tests.ps1
```

也可以明确指定工具链：

```powershell
./tests/run_tests.ps1 -QtBin 'D:/Qt/Download/6.11.0/mingw_64/bin' -CompilerBin 'D:/Qt/Download/Tools/mingw1310_64/bin'
```

脚本使用隔离的临时目录和词库副本，运行核心加载、数据安全、界面冒烟及功能回归四组程序。失败时保留构建日志，成功条件是所有程序返回零退出码。该 PowerShell 脚本针对 Windows MinGW；其他平台可使用 `tests/core_tests.pro` 和 `tests/gui_loading_smoke.pro` 构建测试。

界面变动请附实际运行的截图，并检查文字、布局和键盘操作。涉及用户文件的修改，还应检查保存失败时原文件能否保留，以及旧数据能否正常读取。

## 提交说明

Pull Request 请说明修改原因、最终行为、验证结果和仍存在的限制。常见 commit 类型包括 `fix:`、`feat:`、`docs:` 和 `test:`；标题可使用中文。

不要提交编译产物、Qt Creator 个人配置、临时日志或自己的学习数据。项目中的 `words/` 是初始资源；运行时用户数据位于 README 说明的独立目录。

## 许可与 AI 辅助贡献

主动提交并纳入项目的原创贡献按本项目 [MIT License](LICENSE) 提供，除非事先另有书面约定。请确保有权提交相关代码、文本和图片，并保留第三方资源的来源及许可说明。

允许 AI 辅助贡献。请审阅生成代码并验证实际行为；有较多 AI 辅助时，在 Pull Request 中说明使用的工具和人工审阅范围。第三方代码或数据仍须遵守其原有许可。
