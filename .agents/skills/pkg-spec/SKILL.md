---
name: pkg-spec
description: |
  KWin Debian 打包规范。使用 dpkg-buildpackage 构建本地 .deb 包前必读，
  涵盖 control.1/control.2 双版本机制、符号链接切换、打包命令、产出包清单及 debian/rules 要点。
metadata:
  author: 张丁元
  version: "1.0"
---

# KWin Debian 打包规范

本规范从 `debian/` 目录实际文件扫描提取。

## 1. 概述

KWin 使用 Debian 原生打包格式（`debian/source/format` = `3.0 (native)`）。打包文件位于 `debian/` 目录。

## 2. control 文件与 v20/v25 双版本

仓库维护两份 `control` 文件，通过符号链接切换：

| 文件 | 目标版本 | Qt/KF 版本 | dh 序列 | 依赖前缀 |
|------|---------|-----------|---------|---------|
| `debian/control.1` | v20 | Qt5 / KF5 | `dh-sequence-kf5` | `libkf5*-dev` |
| `debian/control.2` | v25 | Qt6 / KF6 | `dh-sequence-kf6` | `libkf6*-dev` |

`debian/control` 是一个符号链接，仓库中默认指向 `control.2`（v25）：

```
debian/control -> control.2
```

两份 control 文件声明的产出包相同（7 个），区别仅在构建依赖的 Qt/KF 版本和 dh 序列。

## 3. 本地打包前的符号链接切换（重要）

使用 `dpkg-buildpackage` 本地打包时，**必须先将 `debian/control` 重新符号链接到目标版本对应的 control 文件**，否则 `dpkg-buildpackage` 会读取错误的依赖声明，导致构建依赖安装不正确。

```bash
# 打包 v25（Qt6/KF6）— 仓库默认
ln -sf control.2 debian/control

# 打包 v20（Qt5/KF5）
ln -sf control.1 debian/control
```

切换后验证：

```bash
readlink -f debian/control
# v25 应显示: .../debian/control.2
# v20 应显示: .../debian/control.1
```

## 4. 本地打包命令

```bash
# 确保已切换 control 符号链接（见第 3 节）

# 完整构建，跳过签名（生成 .deb 包，本地测试常用）
dpkg-buildpackage --no-sign -b

# 指定并行编译核心数
DEB_BUILD_OPTIONS="parallel=$(nproc)" dpkg-buildpackage --no-sign -b
```

> `--no-sign` 等价于同时跳过源码包签名（`-us`）和 .changes 签名（`-uc`），
> 省去多个参数。-b 表示仅构建二进制包。

## 5. 打包产出

`dpkg-buildpackage` 在上级目录生成以下 `.deb` 包（共 7 个）：

| 包名 | 说明 |
|------|------|
| `kwin-common` | KWin 公共文件 |
| `kwin-data` | 数据文件（QML、配置等） |
| `kwin-dev` | 开发文件 |
| `kwin-wayland` | Wayland 窗口管理器 |
| `kwin-x11` | X11 窗口管理器 |
| `libkwineffects14` | 特效库 |
| `libkwinglutils14` | OpenGL 工具库 |

## 6. debian/rules 要点

`debian/rules` 使用 `dh` + KDE 团队工具链：

- `dh-sequence-kf5` 或 `dh-sequence-kf6`（由 control 文件决定）
- `override_dh_auto_configure` 传递 CMake 选项：`-DENABLE_WERROR=ON -DBUILD_TESTING=OFF -DKWIN_BUILD_KCMS=OFF`
- `override_dh_auto_test` 禁用构建时测试
- `DEB_VERSION` 从 `debian/changelog` 提取，传给 `-DEVENTLOG_VERSION`

## 7. 版本号

版本号记录在 `debian/changelog`，格式为 `kwin (4:6.1.x) unstable; urgency=medium`。当前版本可通过 `dpkg-parsechangelog -S version` 查看。

## 8. 注意事项

- `debian/control` 的符号链接方向不随 `cmake` 的 Qt 版本探测自动切换 — 它是独立的打包层配置
- 切换 control 文件后，确保构建环境的已安装依赖与目标版本匹配（v20 装 KF5，v25 装 KF6）
- `debian/.gitignore` 忽略 `files` 目录（构建产物）
- `debian/.gitattributes` 对 `changelog` 启用 `dpkg-mergechangelogs` 合并策略
