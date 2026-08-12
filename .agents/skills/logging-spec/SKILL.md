---
name: logging-spec
description: |
  KWin Qt Category Logging 规范。增加日志打印前必读，涵盖日志类别清单、
  三种注册方式、日志级别使用指南、外部 C 库桥接模式及消息格式要求。
metadata:
  author: 张丁元
  version: "1.0"
---

# KWin 日志规范

KWin 使用 Qt 的 Category Logging 作为日志输出方式。本规范从 `data/org_kde_kwin.categories`、`src/utils/common.cpp`、各 `*_logging.h/cpp` 文件提取。

## 1. 核心原则

- **禁止**使用 `qDebug()`、`qWarning()`、`qCritical()` 等无类别函数
- **必须**使用 `qCDebug(Category)`、`qCWarning(Category)`、`qCCritical(Category)`、`qCInfo(Category)`
- 每个模块使用专属日志类别，不混用

```cpp
// 正确
qCDebug(KWIN_CORE) << "Window created:" << window;

// 错误 — 禁止无类别日志
qDebug() << "Window created:" << window;
```

## 2. 日志类别清单

所有日志类别的 C++ 标识符与运行时名称映射定义在：

```
data/org_kde_kwin.categories
```

每行格式：`<runtime_name> <Description> DEFAULT_SEVERITY [WARNING] IDENTIFIER [<CPP_IDENTIFIER>]`

使用前请直接读取该文件获取最新完整清单，不要凭记忆推断。

## 3. 三种注册方式

### 方式 A：集中声明 + 集中定义（核心类别）

用于跨文件共享的核心类别。在 `.h` 中声明，在 `.cpp` 中定义。

**声明**（`src/utils/common.h`）：
```cpp
#pragma once

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(KWIN_CORE)
Q_DECLARE_LOGGING_CATEGORY(KWIN_OPENGL)
```

> **跨编译单元导出性（Qt5 vs Qt6）**
>
> `Q_DECLARE_LOGGING_CATEGORY` 声明的是 `extern const QLoggingCategory &cat`，
> 跨共享库使用时需确保符号可见。KWin 同时支持 Qt5（v20）和 Qt6（v25），两者处理方式不同：
>
> **Qt6（v25，BUILD_ON_V25）**：
> Linux 默认导出 ELF 符号，`Q_DECLARE_LOGGING_CATEGORY` 可直接跨 TU 使用。
> 若启用了 `-fvisibility=hidden` 或需移植 Windows，改用
> `Q_DECLARE_EXPORTED_LOGGING_CATEGORY`（Qt 6.5+ 提供）显式导出：
>
> ```cpp
> Q_DECLARE_EXPORTED_LOGGING_CATEGORY(KWINEFFECTS, KWINEFFECTS_EXPORT)
> ```
>
> **Qt5（v20，!BUILD_ON_V25）**：
> `Q_DECLARE_EXPORTED_LOGGING_CATEGORY` **不存在**（Qt 6.5 才引入）。
> Qt5 文档明确不建议跨模块边界导出 `QLoggingCategory` 对象。两种可行方案：
>
> 1. 依赖 Linux 默认 ELF 符号导出（KWin 当前做法）— `Q_DECLARE_LOGGING_CATEGORY`
>    在 .h 声明、`Q_LOGGING_CATEGORY` 在 .cpp 定义，符号默认可见即可跨库使用。
> 2. 使用 ECM 的 `ecm_qt_declare_logging_category()` CMake 函数自动生成头文件
>    和源文件，内部使用 `Q_DECLARE_LOGGING_CATEGORY`，Qt5/Qt6 通用：
>
> ```cmake
> ecm_qt_declare_logging_category(kwin_SRCS
>     HEADER kwin_debug.h
>     IDENTIFIER KWIN_CORE
>     CATEGORY_NAME kwin_core
>     DEFAULT_SEVERITY Warning
>     EXPORT KWIN
>     DESCRIPTION "KWin Core"
> )
> ```

**定义**（`src/utils/common.cpp`）：
```cpp
#include "utils/common.h"

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtWarningMsg)
Q_LOGGING_CATEGORY(KWIN_OPENGL, "kwin_scene_opengl", QtWarningMsg)
```

### 方式 B：独立 logging 文件（模块化类别）

用于独立模块的后端类别。创建 `*_logging.h` 和 `*_logging.cpp`。

**声明**（`src/tabbox/tabbox_logging.h`）：
```cpp
#pragma once
#include <QDebug>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(KWIN_TABBOX)
```

**定义**（`src/tabbox/tabbox_logging.cpp`）：
```cpp
#include "tabbox_logging.h"

Q_LOGGING_CATEGORY(KWIN_TABBOX, "kwin_tabbox", QtWarningMsg)
```

### 方式 C：内联单文件（插件/简单模块）

用于仅单个文件使用的类别，直接在 `.cpp` 中声明并定义。

```cpp
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(KWIN_LOGGER, "kwin_logger")
```

> 如需在其他文件中使用，升级为方式 A 或 B。

## 4. 日志级别

`Q_LOGGING_CATEGORY` 第三参数指定默认严重级别：

| 常量 | 含义 | 使用场景 |
|------|------|---------|
| `QtDebugMsg` | Debug | 开发调试，发布版默认不输出 |
| `QtInfoMsg` | Info | 重要运行信息 |
| `QtWarningMsg` | Warning | 异常但不致命（**最常用**） |
| `QtCriticalMsg` | Critical | 严重错误 |
| `QtFatalMsg` | Fatal | 致命，终止进程 |

**默认严重级别选择指南**：
- 大多数类别使用 `QtWarningMsg`（运行时可见，Debug 需手动开启）
- 纯调试追踪类别可用 `QtCriticalMsg`（默认不输出，需 `QT_LOGGING_RULES` 显式开启）

### 运行时控制

```bash
# 开启某类别的 Debug 日志
QT_LOGGING_RULES="kwin_core.debug=true" kwin_wayland

# 开启所有 KWin 类别
QT_LOGGING_RULES="kwin_*.debug=true" kwin_wayland

# 关闭某类别 Warning
QT_LOGGING_RULES="kwin_core.warning=false" kwin_wayland
```

## 5. 消息格式要求

### 流式风格（推荐）

```cpp
qCDebug(KWIN_CORE) << "Window created:" << window << "on output" << output;
```

### 添加上下文前缀

KWin 常用模块前缀标识来源：

```cpp
qCDebug(KWIN_CORE) << "[Workspace]Set active window:" << window->resourceName();
qCWarning(KWIN_CORE) << "[Workspace]Take activity: not shown" << window->resourceName();
```

### printf 风格（禁止用于新代码）

```cpp
// 禁止 — 使用流式风格
qCDebug(KWIN_CORE, "Window %s created", name);
```

## 6. 外部 C 库桥接

当封装外部 C 库（libinput、xkbcommon 等）时，将库的原生日志转发到 Qt Category。

### libinput 示例（`src/backends/libinput/context.cpp`）

```cpp
static void libinputLogHandler(libinput *libinput, libinput_log_priority priority, void *data)
{
    // 获取库日志消息
    const char *buf = libinput_log_format_data(libinput, data);
    // 转发到 Qt 类别
    qCDebug(KWIN_LIBINPUT) << "Libinput:" << buf;
}

// 注册回调
libinput_log_set_handler(m_libinput, &libinputLogHandler);
```

> 不直接调用 `libinput_log_set_priority` 输出，而是通过 `qCDebug/qCWarning` 转发到 `KWIN_LIBINPUT`。

## 7. 新增日志类别流程

1. 在 `data/org_kde_kwin.categories` 添加一行：
   ```
   <category_name> <Description> DEFAULT_SEVERITY [WARNING] IDENTIFIER [<CPP_IDENTIFIER>]
   ```
2. 选择注册方式（A/B/C），在对应文件中 `Q_DECLARE_LOGGING_CATEGORY` + `Q_LOGGING_CATEGORY`
3. 使用 `qCDebug/qCWarning/qCCritical/qCInfo` 输出日志
4. 确保 CMakeLists.txt 中对应源文件被加入构建目标

## 8. 禁止事项

- ❌ 使用 `qDebug()` / `qWarning()` / `qCritical()` / `qInfo()` 无类别函数
- ❌ 使用 `printf` / `fprintf` 输出日志
- ❌ 使用 `qCDebug` 输出常驻高频日志（影响性能）
- ❌ 在头文件中 `Q_LOGGING_CATEGORY`（应在 .cpp 中定义，.h 中只声明）
- ❌ 硬编码日志字符串中的敏感信息（密码、token）
