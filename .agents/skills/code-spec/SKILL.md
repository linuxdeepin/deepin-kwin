---
name: code-spec
description: |
  KWin C++ 代码规范。编写或修改 KWin 源码前必读，涵盖缩进、命名、include、
  大括号风格、Qt 宏使用、头文件结构等。所有规则基于 .clang-format 和实际源码提取。
metadata:
  author: 张丁元
  version: "1.0"
---

# KWin 代码规范

本规范从 `.clang-format`（WebKit 基风格 + 定制）和实际源码（`src/window.h`、`src/workspace.h`、`src/utils/common.h` 等）提取。

## 1. 基础格式

- **缩进**: 4 个空格，禁止 Tab
- **行宽**: 无限制（`ColumnLimit: 0`）
- **编码标准**: C++20（`CMAKE_CXX_STANDARD 20`）

## 2. 大括号风格

`BreakBeforeBraces: Custom`，规则如下：

| 元素 | 大括号位置 |
|------|-----------|
| 函数 | 另起一行 |
| 类 | 另起一行 |
| 结构体 | 另起一行 |
| 命名空间 | 另起一行 |
| 控制语句（if/for/while/switch） | 跟随同行 |
| enum | 跟随同行 |
| catch | 跟随同行 |
| else | 跟随同行 |

```cpp
// 正确
void MyClass::doSomething()
{
    if (condition) {
        foo();
    } else {
        bar();
    }
}

// 错误 — 函数大括号不能跟随
void MyClass::doSomething() {
```

## 3. 指针与对齐

- **指针对齐**: 右对齐（`PointerAlignment: Right`）— `Instruction *a;`
- **开括号后参数对齐**: 水平对齐（`AlignAfterOpenBracket: Align`）
- **不强制参数全放一行**: `AllowAllArgumentsOnNextLine: false`、`AllowAllParametersOfDeclarationOnNextLine: false`

## 4. 头文件

- **头文件保护**: 使用 `#pragma once`，不使用 `#ifndef` guard 宏
- **include 排序**: `SortIncludes: true`，组内按字母序

### include 分组顺序（参考 `src/workspace.h`）

```
1. kwin 内部头文件（"xxx.h"）
2. KDE Frameworks 头文件（<KXxx>）
3. Qt 头文件（<QXxx>）
4. 系统头文件（<sys/xxx.h>）
5. STL 头文件（<vector>、<memory> 等）
```

组与组之间用空行分隔：

```cpp
// kwin
#include "options.h"
#include "utils/common.h"
// Qt
#include <QImage>
#include <QTimer>
// std
#include <memory>
```

## 5. 文件头

每个源文件必须包含 SPDX 版权和许可证声明：

```cpp
/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Author Name <author@example.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
```

## 6. 命名规范

### 类名

PascalCase：`Window`、`Workspace`、`AbstractClient`

### 成员变量

`m_` 前缀 + camelCase：`m_output`、`m_visual`、`m_blockGeometryUpdates`

### 局部变量 / 函数参数

camelCase，无前缀：`window`、`bit_depth`（历史遗留 C 风格变量允许下划线）

### 函数名

camelCase：`doSomething()`、`setOpacity()`

### 常量 / 宏

全大写下划线分隔：`DBUS_DEEPIN_WM_SERVICE`、`KWIN_BUILD_XRENDER_COMPOSITING`

### 枚举

PascalCase 枚举名，枚举值 PascalCase：`DesktopLayer`、`NormalLayer`

## 7. Qt 宏使用（重要）

项目使用 `-DQT_NO_KEYWORDS` 编译，**禁止**使用 Qt 关键字：

| 禁止 | 必须使用 |
|------|---------|
| `signals:` | `Q_SIGNALS:` |
| `slots:` | `Q_SLOTS:` |
| `emit` | `Q_EMIT` |
| `foreach` | `Q_FOREACH` |
| `forever` | `Q_FOREVER` |

```cpp
public Q_SLOTS:
    void setActive(bool active);

protected Q_SLOTS:
    void updateState();

Q_SIGNALS:
    void geometryChanged();
```

## 8. 初始化列表

成员初始化列表中，逗号置于行首：

```cpp
Window::Window()
    : m_output(workspace()->activeOutput())
    , m_visual(XCB_NONE)
    , bit_depth(24)
    , info(nullptr)
{
}
```

## 9. 命名空间

- **不缩进**（`NamespaceIndentation: None`）
- 命名空间大括号另起一行
- 主命名空间为 `KWin`，子模块使用嵌套（如 `KWin::TabBox`、`KWin::Decoration`）

```cpp
namespace KWin
{

class Window
{
    // ...
};

} // namespace KWin
```

## 10. 短函数 / Lambda

- **不允许单行函数**（`AllowShortFunctionsOnASingleLine: None`）
- **空 Lambda 可单行**（`AllowShortLambdasOnASingleLine: Empty`）

## 11. 模板

- `template<` 无空格（`SpaceAfterTemplateKeyword: false`）
- 模板声明后总是换行（`AlwaysBreakTemplateDeclarations: true`）

## 12. 二元运算符换行

- 非赋值运算符在行首断开（`BreakBeforeBinaryOperators: NonAssignment`）
- 赋值运算符不换行

## 13. Q_PROPERTY

属性声明使用 `Q_PROPERTY`，遵循 READ/WRITE/NOTIFY 模式：

```cpp
Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
```

## 14. 代码格式化

提交前运行 clang-format：

```bash
# 格式化单个文件
clang-format -i src/path/to/file.cpp

# 格式化所有改动文件（需要 KDEClangFormat CMake 模块）
cmake --build build --target clangformat
```

## 15. 提交信息格式

```
type: 简述

type 可选值:
  feat   — 新功能
  fix    — 修复 bug
  chore  — 构建/版本/杂项
  revert — 回退
```

示例：
```
fix: 在华为主机上设置防截屏窗口后，鼠标移动延迟
chore: bump to version 4:6.1.35
```
