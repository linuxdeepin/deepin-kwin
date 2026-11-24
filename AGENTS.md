# KWin — AI Agent Guide

KWin 是 KDE 窗口管理器，deepin 版本维护在 `develop/snipe` 分支（Gerrit: http://gerrit.uniontech.com/deepin-kde/kwin）。

本文件是 AI 智能体在本仓库工作时的入口指南。开始任何编码、构建、打包或日志相关工作前，请先阅读对应的 skill。

## Skills

| Skill | 用途 | 触发时机 |
|-------|------|----------|
| [arch-spec](.agents/skills/arch-spec/SKILL.md) | 架构设计与模块地图 | 定位问题归属模块前 |
| [code-spec](.agents/skills/code-spec/SKILL.md) | C++ 代码规范 | 编写或修改源码前 |
| [build-spec](.agents/skills/build-spec/SKILL.md) | CMake 编译指南 | 编译项目前 |
| [pkg-spec](.agents/skills/pkg-spec/SKILL.md) | Debian 打包规范 | 打包构建 .deb 前 |
| [logging-spec](.agents/skills/logging-spec/SKILL.md) | Qt Category Logging 规范 | 增加日志打印前 |
| [kwin-layer-debug](.agents/skills/kwin-layer-debug/SKILL.md) | 窗口层级与锁屏可见性调试 | 排查锁屏下窗口不可见 / 快捷键失效 |

## 仓库概况

- **语言**: C++20（`CMAKE_CXX_STANDARD 20`）
- **构建系统**: CMake 3.13+，推荐 Ninja
- **Qt 版本**: 同时支持 Qt5（v20 接口）和 Qt6（v25 接口），通过 `BUILD_ON_V25` 宏区分
- **分支**: `develop/snipe` 为开发主分支
- **代码风格**: 基于 WebKit 风格，详见 `.clang-format`
- **许可证**: GPL-2.0-or-later（SPDX 标注）

## 工作流程

1. 定位问题前阅读 `arch-spec`，确认应修改哪个模块
2. 修改代码前阅读 `code-spec`，确保符合 `.clang-format` 规范
3. 编译前阅读 `build-spec`，使用正确的配置和构建命令
4. 打包前阅读 `pkg-spec`，注意切换 control 符号链接
5. 如需增加日志，阅读 `logging-spec`，使用 Qt Category Logging
6. 提交信息格式：`type: 简述`（如 `fix: 修复窗口切换焦点问题`）
7. 推送到 Gerrit review：`git push origin HEAD:refs/for/develop/snipe`

## 注意事项

- 代码使用 `-DQT_NO_KEYWORDS` 编译，必须使用 `Q_SIGNALS`/`Q_SLOTS`/`Q_EMIT` 而非 `signals`/`slots`/`emit`
- 头文件使用 `#pragma once`，不使用 include guard 宏
- 成员变量使用 `m_` 前缀
- include 分组排序：kwin 内部 → KDE Frameworks → Qt → 系统 → STL
- 不要修改 `.clang-format` 除非有明确需求
