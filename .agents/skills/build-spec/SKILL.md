---
name: build-spec
description: |
  KWin CMake 编译指南。编译项目前必读，涵盖配置命令、构建命令、依赖列表、
  构建选项及 Qt5/Qt6 双版本支持、Ninja/Make 回退方案。
metadata:
  author: 张丁元
  version: "1.0"
---

# KWin 编译指南

本指南从 `CMakeLists.txt`、`src/CMakeLists.txt`、`cmake/modules/` 提取。

## 1. 前置依赖

### 构建工具

| 工具 | 最低版本 | 说明 |
|------|---------|------|
| CMake | 3.13 | 构建系统 |
| C++ 编译器 | C++20 | GCC 10+ |
| Ninja | 任意 | 推荐构建器（更快的增量编译） |
| pkg-config | 任意 | 查找依赖 |

### 核心库

| 依赖 | 说明 |
|------|------|
| Qt6 或 Qt5 | 自动探测：找到 Qt6 用 v25 接口，否则用 Qt5 v20 接口 |
| ECM (Extra CMake Modules) | KDE 构建模块 |
| KDE Frameworks | KF6（Qt6）或 KF5（Qt5），最低 5.54 |
| Wayland | 1.21+ |
| WaylandProtocols | 1.31+ |
| libepoxy | 1.3+（OpenGL 透明加载） |
| libinput | 输入设备处理 |
| libdrm | DRM/KMS 支持 |
| libxkbcommon | XKB 键盘布局 |
| libgbm | GBM 缓冲管理 |
| EGL | OpenGL ES 上下文 |

## 2. 配置命令

### 推荐：Ninja 构建

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_EXPORT_COMPILE_COMMANDS=YES
```

### 回退：Make 构建

当系统没有 Ninja 时，使用默认生成器（Makefiles）并手动指定构建核心数：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr
```

### 关键参数说明

| 参数 | 作用 |
|------|------|
| `-G Ninja` | 使用 Ninja 生成器（推荐） |
| `-DCMAKE_BUILD_TYPE=Debug` | 调试构建，含调试符号 |
| `-DCMAKE_INSTALL_PREFIX=/usr` | 安装前缀 |
| `-DCMAKE_EXPORT_COMPILE_COMMANDS=YES` | 导出 compile_commands.json（供 clangd/IDE 使用） |

## 3. 构建命令

配置完成后，使用以下命令编译项目。无论使用 Ninja 还是 Make，命令相同：

```bash
# 完整构建
cmake --build build

# 指定并行核心数（加速编译）
cmake --build build --parallel $(nproc)

# 增量构建指定目标
cmake --build build --target kwin

# 仅构建变更的文件（增量）
cmake --build build --target kwin_wayland
```

> `cmake --build` 会自动调用对应生成器（Ninja 或 Make），是跨生成器的通用构建命令。
> `--parallel N` 是 CMake 内置参数，等价于 `-- -jN`，无需区分底层生成器。
> Make 回退时建议指定并行数，否则默认单线程。

## 4. Qt5 / Qt6 双版本支持

CMakeLists.txt 自动探测 Qt 版本：

```
find_package(QT NAMES Qt6 QUIET)
```

- **找到 Qt6** → `BUILD_ON_V25 ON`，使用 v25 接口（`QT_MAJOR_VERSION=6`，`KF_MAJOR_VERSION=6`）
- **未找到 Qt6** → 回退 Qt5，`BUILD_ON_V25 OFF`（`QT_MAJOR_VERSION=5`，`KF_MAJOR_VERSION=5`）

源码中通过 `#ifdef BUILD_ON_V25` 区分行为，例如：

```cpp
#ifdef BUILD_ON_V25
#define KWinDBusService "org.deepin.dde.Appearance1"
#else
#define KWinDBusService "com.deepin.daemon.Appearance"
#endif
```

## 5. 构建选项

在配置时通过 `-D<OPTION>=ON/OFF` 控制：

| 选项 | 默认 | 说明 |
|------|------|------|
| `BUILD_TESTING` | OFF | 构建测试 |
| `KWIN_BUILD_DECORATIONS` | ON | 窗口装饰 |
| `KWIN_BUILD_KCMS` | OFF | 配置模块 |
| `KWIN_BUILD_NOTIFICATIONS` | ON | KNotifications 支持 |
| `KWIN_BUILD_SCREENLOCKER` | OFF | 锁屏功能 |
| `KWIN_BUILD_TABBOX` | ON | 窗口切换器 |
| `KWIN_BUILD_XRENDER_COMPOSITING` | ON | XRender 合成 |
| `KWIN_BUILD_ACTIVITIES` | ON (条件) | KActivities 支持 |
| `KWIN_BUILD_RUNNERS` | ON (条件) | KRunner 支持 |
| `ENABLE_WERROR` | Debug 下 ON | 警告视为错误（当前全局已关闭） |
| `MEMCHECK` | OFF | AddressSanitizer 调试 |

### 条件选项

`KWIN_BUILD_ACTIVITIES` 和 `KWIN_BUILD_RUNNERS` 使用 `cmake_dependent_option`，仅当对应 KF 包找到时才启用。

## 6. 最小构建示例

仅构建核心（关闭装饰、KCM、通知等）：

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=YES \
  -DKWIN_BUILD_KCMS=OFF \
  -DKWIN_BUILD_DECORATIONS=OFF \
  -DKWIN_BUILD_NOTIFICATIONS=OFF
cmake --build build
```

## 7. AddressSanitizer 调试

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMEMCHECK=ON
cmake --build build
```

这会添加 `-fsanitize=address -fno-omit-frame-pointer` 编译选项。

## 8. 安装

```bash
cmake --install build --prefix /usr
```

> 注意：安装到系统目录可能需要 root 权限，建议在沙箱或容器中测试。
