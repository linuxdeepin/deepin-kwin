# Deepin KWin

deepin-kwin 是深度桌面环境的核心窗口管理器，基于 KWin 5.27 定制开发，支持 Qt6/KF6。KWin 是一个易于使用且灵活的 Linux 系统上的混成窗口管理器，支持 Wayland 和 X11 窗口系统。它主要与桌面外壳（如 KDE Plasma Desktop）配合使用。KWin 的设计理念是不引人注意；用户应该感觉不到他们正在使用窗口管理器。尽管如此，KWin 为高级功能提供了陡峭的学习曲线，这些功能在不与主要使命冲突的情况下可供使用。KWin 没有专门的目标用户群体，而是追随使用 KWin 作为窗口管理器的桌面外壳的目标用户群体。

## 构建依赖

- Qt 6.7+
- KDE Frameworks 6.0+
- XCB 相关开发库

## 编译和安装

```
git clone https://github.com/linuxdeepin/deepin-kwin.git
cd deepin-kwin
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cmake --install build
```

## 参与贡献

- [通过 GitHub 贡献代码](https://github.com/linuxdeepin/deepin-kwin/)
- [在 GitHub Issues 或 GitHub Discussions 提交 bug 或建议](https://github.com/linuxdeepin/developer-center/issues/new/choose)

## 新功能指南

新功能只有在满足以下条件时才能被添加到 KWin：

* 不违反本文档开头所述的主要使命
* 不引入不稳定性
* 得到维护，即 bug 能在及时修复（下下个小版本发布前），除非是极端情况
* 能与所有现有功能协同工作
* 支持单屏和多屏（xrandr）
* 能带来显著优势
* 功能完整，即至少支持竞争实现中所有有用的功能
* 不是针对小用户群体的特殊情况
* 不会显著增加代码复杂度
* 不影响 KWin 的许可证（GPLv2+）

所有新增功能都处于观察期，如果在接下来的两个功能版本中不满足上述任何非功能性要求，该功能将被移除。

对于任何类型的插件（效果、脚本等）也同样适用这些非功能性要求。建议使用脚本插件并单独分发。

## 许可证

**deepin-kwin** 采用 GPL-2.0-or-later 许可证。

