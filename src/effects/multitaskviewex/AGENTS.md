# multitaskviewex — agent guide

多任务视图效果（`multitaskviewex`，自包含插件，不依赖其它特效）。领域术语见同目录 `CONTEXT.md`；本文件只收录实现约定与必要流程。

## 分层与依赖（单向）

| 目录 | 职责 |
|------|------|
| `model/` | 业务事实 + 稳态目标（纯逻辑，零 GL / 零 KWin，可纯单测） |
| `animation/` | 确定性时间线 / 插值节点（纯数学，零依赖） |
| `data/` | 工作区数据模型、壁纸 DBus、条度量基准（内联，不依赖外部插件） |
| `view/` | GLControl 控件树：几何 + 绘制 + 命中（GL 边界） |
| `controller/` | Effect 生命周期、绘制管线、输入分发；每类一文件 |

`controller/` 的协作类（一文件一类）：

| 文件 | 类 | 职责 |
|------|----|------|
| `multitaskviewex.h/.cpp` | `MultitaskViewExEffect` | Effect 接口、绘制管线、输入分发 |
| `session.cpp` | 同左（`MultitaskViewExEffect` 的会话/Wiring TU） | 屏幕视图集合、会话进出、桌面滑动、窗口开关 |
| `bar.cpp` | 同左（条 TU） | 工作区条模型同步、拖拽落定 |
| `keynav.cpp` | 同左（导航 TU） | 悬停/循环/同类窗口导航的胶水 |
| `wallpaper.cpp` | 同左（壁纸 TU） | 壁纸预取与背景重应用胶水 |
| `effectconfig.h/.cpp` | `EffectConfig` | DConfig 读取与变更通知 |
| `wallpapermanager.h/.cpp` | `WallpaperManager` | 壁纸缓存、DBus 刷新、URI 解析、壁纸迁移 |
| `workspacemanager.h/.cpp` | `WorkspaceManager` | `WorkspaceModel` 与 VirtualDesktopManager 同步、增删/重排/切换 |
| `windownavigator.h/.cpp` | `WindowNavigator` | 视图内窗口查询（无状态） |
| `windowroles.h/.cpp` | `WindowRoles` | 窗口圆角/裁剪角色的应用与还原 |
| `dockrelay.h` | `DockRelay` | 退出后补发 Dock 点击 |
| `helpers.h` | — | 日志类别、X11 探测（非类） |

- 数据单向流动：输入事件 → controller → `view->hitTest` → 动作/动画状态；动画状态 → view 属性 → `paintScreen` 逐帧绘制。view 不回写 controller，controller 不持有绘制细节。
- `model/` 不 include 任何 `view/` 头，也不 include `kwineffects.h`（`EffectWindow` 前向声明即可）。
- view 通过注入 `WindowPainter` 与 KWin 解耦，Scene/控件因此可在无 KWin 的单元测试中驱动。

## 多屏不变量

每个 `EffectScreen` 恰好一份完整视图（`ScreenView`，容器 `ScreenViewSet`）：视图集合恒等于 `effects->screens()`，插入序 = 屏列表顺序；单屏 = 列表长度 1 的平凡情况。**不存在**「当前屏」访问器，也不存在按后端/屏数分支的第二条场景。新增多屏逻辑时保持该不变量。

## 动画语义（ADR 0001）

- Model 只产出稳态目标（纯函数，可单测）；插值状态归 Scene。
- 插值节点持有 `cur/from/to + 时间线`：`from=cur` 实现可打断过渡；`sameCore` 短路（目标未变不重启时间线——否则每帧 apply 会反复 start，永不收敛）。
- 帧增量由每屏一份的 `FrameDeltaClock` 换算：首帧 16ms 起步；时间戳倒退 → 0；间隔 > 100ms 钳到 100。
- 删除当前工作区必须显式播一次切换滑动：`MultitaskViewExEffect::removeDesktop()` 在删除前抓取被删工作区的壁纸，删除后以新当前编号调用 `slideTo()`；`WorkspaceManager::removeDesktop()` 在自有变更区间内完成 current 切换并返回新编号，使 `currentChanged` 不再触发第二条滑动。方向由 `removalSlideDirection()` 决定——删首位的 `newCurrent` 与被删编号相等时仍向前滑（新内容从左入）。

## 全局快捷键

- 唯一全局快捷键是 `Meta+S`，注册在 action `MultitaskViewExToggle`（`multitaskviewex.cpp` 构造）。
- `Meta+W` / `Meta+A` 不可用：已被 `Expose` / `ExposeAll`（`/etc/xdg/kglobalshortcutsrc`，dde-kwin 提供）占用；注册它们只会被 KGlobalAccel 丢弃。
- `KGlobalAccel::setShortcut()` 默认 `Autoloading`：会沿用 `~/.config/kglobalshortcutsrc` 里已保存的值（含空值）。因此**不要注册会被丢弃的键**——一旦 active 列被写成 `none`，之后重启也不会自己恢复，需在系统设置里重设。
- 依赖系统默认键位时需要同步 dde-kwin 的 `/etc/xdg/kglobalshortcutsrc`；本仓库不提供该文件。

## 会话锁屏

- 视图运行时同时持有整屏 effect、鼠标拦截和键盘抓取。锁屏窗口出现后视图必须立即 `setRunning(false)`（不播退场动画），否则锁屏拿不到键鼠，用户无法解锁。
- 两个方向都要守：`setRunning(true)` 在 `sessionLocked()` 为真时拒绝进入；`onWindowAdded` 收到锁屏窗口时退出。`m_running` 只在 `setRunning()` 赋值，守卫放这里可覆盖 Meta+S、D-Bus `activate()` 与后续新增入口。
- 识别顺序：先认 `EffectWindow::isLockScreen()`；X11 的 dde-lock 是 override-redirect 的 `Unmanaged`，`Window::isLockScreen()` 恒为 false，只能按身份字符串精确匹配——`captionNormal() == "org.deepin.dde.lock"` 或 `windowClass() == "dde-lock org.deepin.dde.lock"`。禁止子串匹配：普通窗口标题可能包含该路径，会误判为锁屏并拒绝进入。
- DDE 的 dde-lock 不实现 `org.freedesktop.ScreenSaver` 的 `/ScreenSaver`（GetActive 返回 `UnknownInterface`），`effects->isScreenLocked()` 在 DDE 下不可依赖，不能作为唯一判据。
- 手工验证矩阵：自动锁屏到点、合盖再开盖、锁屏期间按 Meta+S / 点任务栏多任务图标。三项都要确认锁屏界面可正常输入，解锁后视图已退出。

## 资源

- 素材与 shader 统一编译进二进制（`multitaskviewex.qrc`），路径见 `view/assetpaths.h`。
- 静态库中 qrc 资源必须显式初始化：`Q_INIT_RESOURCE(multitaskviewex)`，且必须放在**文件作用域 `static`** 函数里（不能放匿名 namespace —— `Q_INIT_RESOURCE` 的 extern 声明会被限定作用域）。
- 壁纸只走 DBus（`deepinwmrc` 已废弃）：端点是与旧插件 `src/effects/multitaskview` 一致的编译期常量（`model/wallpaperdata.h`）——v25 用 `org.deepin.dde.Appearance1`，v20 用 `com.deepin.daemon.Appearance`；外观服务空回复时回退 `file:///usr/share/wallpapers/deepin/desktop.jpg`。解析与缓存归 `WallpaperManager`。
- 壁纸只有一处来源：`GetWorkspaceBackgroundForMonitor`（appearance，session bus）。缩略图/预览用 `WallpaperManager::clearPath`，全屏底/滑动过渡用 `WallpaperManager::backgroundFor`，两者都是清晰图；`BackgroundControl` 先按 `frame.enterProgress` 画不透明壁纸，再在其上叠一层黑色色罩（`kBackgroundMaskAlpha = 1 - kBackgroundBrightness`，0.4），把底图压到 `kBackgroundBrightness`（0.6）亮度。色罩只画在 `SceneLayer::Background`，网格（Cells/Windows）与工作区条（Bar）在更上层绘制，对比度不受影响。
- 不依赖 ImageBlur（`ImageBlur1.Get`，system bus）：该 DBus 接口已不再使用。随机壁纸直接遍历 `/usr/share/wallpapers/deepin`。

## 代码约定

- **注释**：只允许公共接口的 Doxygen 块注释（`/** … */`，英文），写在头文件的声明处；禁止其它说明性注释（含单行 `///`）与 `.cpp` 内的实现注释——必要说明写进本文件。公共接口 = 头文件中 `public:` 段的函数、结构体的公共方法，以及命名空间作用域下的自由函数。
- **语言**：代码内字符串与日志一律英文（i18n 字段除外）。
- **include**：不使用 `../`。插件根目录已在 `CMakeLists.txt` 加入 include 路径，跨目录一律写插件相对完整路径（`model/…`、`view/…`、`data/…`、`animation/…`、`controller/…`）；同目录头文件用裸名。分组顺序（组间空行）：插件自身 → kwin 内部 → KDE Frameworks → Qt → 系统 → STL。
- **license 头**：每个 `.h` / `.cpp` 文件使用
  ```
  /*
      SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

      SPDX-License-Identifier: GPL-2.0-or-later
  */
  ```
  `.frag` / `.vert` / `.svg` / `.qrc` / `.json` 由仓库根 `REUSE.toml` 统一声明，不加文件头；`CMakeLists.txt` 沿用仓库既有 `#` 注释头写法。
- **格式**：遵循仓库 `.clang-format`（WebKit 基，4 空格、函数/类大括号另起一行、无单行函数）；相邻的函数定义之间、相邻的结构体/类定义之间保留且仅保留 1 个空行；提交前对本插件跑 `clang-format -i`。
- **文件粒度**：一个类/结构体一个头文件（`view/screenviewset.h`、`view/windownode.h`、`model/bardraglayout.h` 等）；`bardragmodel.h` 只保留 `WorkspaceBarDragModel`。

## 构建与测试

- 编译见仓库 `.agents/skills/build-spec/SKILL.md`。
- 单测入口 `tests/`（`ctest` 或直接运行 `tst_*`）：
  - 纯逻辑目标 `tst_animation` / `tst_bardragmodel` / `tst_windowgrid` / `tst_wallpaperdata` / `tst_scenemodel` / `tst_session` / `tst_baritem` 只链 Qt Core(/Gui)，不链 kwinglutils/kwineffects —— 证明 model/anim 可脱离合成器纯单测。
  - `tst_scene` / `tst_multiscreen` 覆盖 Scene + 控件（链 Gui / epoxy / kwinglutils）。
  - `tests/scene_model_state.h` 是模型域测试契约层（快照合并/差异断言）；`tests/scene_state.h` 在其上补控件输入瞬态键。
