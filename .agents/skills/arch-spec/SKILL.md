---
name: arch-spec
description: |
  KWin 架构设计与模块地图。定位问题归属模块前必读，说明每个目录/模块的作用、
  核心类与接口设计、模块间依赖关系，让智能体读一遍就知道某类问题应该修改哪些文件。
metadata:
  author: 张丁元
  version: "1.0"
---

# KWin 架构设计

本文件基于 `src/` 目录实际代码结构提取，描述各模块的职责、核心接口及问题路由。

## 1. 启动流程

KWin 有两个入口，对应两种运行模式：

```
src/main_wayland.cpp   →  ApplicationWayland  →  WaylandCompositor::create() → createWorkspace() → WaylandServer
src/main_x11.cpp       →  ApplicationX11      →  X11Compositor::create()    → createWorkspace()
```

启动顺序：**创建合成器 → 创建工作区 → 初始化场景 → 加载特效**。

关键全局单例：

| 单例 | 获取方式 | 职责 |
|------|---------|------|
| `Workspace` | `workspace()` | 窗口管理中枢 |
| `Compositor` | `Compositor::self()` | 合成调度 |
| `Input` | `Input::self()` | 输入事件分发 |
| `Options` | `options` | 全局配置/用户偏好 |

## 2. 核心分层

```
┌─────────────────────────────────────────────┐
│                入口层                        │
│  main_wayland / main_x11 / Application       │
├─────────────────────────────────────────────┤
│              窗口管理层                       │
│  Workspace → Window (X11Window/WaylandWindow)│
│  规则、放置、激活、活动、屏幕边缘             │
├─────────────────────────────────────────────┤
│              合成层                          │
│  Compositor → WorkspaceScene → Item 树        │
│  Scene (OpenGL/QPainter/XRender)              │
├─────────────────────────────────────────────┤
│              后端层                          │
│  OutputBackend (DRM/Wayland/X11/Virtual)      │
│  InputBackend (Libinput/FakeInput/X11)        │
│  RenderBackend → OpenGLBackend/EGL            │
├─────────────────────────────────────────────┤
│              Wayland 协议层                   │
│  WaylandServer → wayland/*_interface.h        │
│  Xwayland 桥接                               │
├─────────────────────────────────────────────┤
│              扩展层                          │
│  Effects / Decorations / TabBox / Scripting   │
│  Plugins / SplitScreen / WindowStyle          │
└─────────────────────────────────────────────┘
```

## 3. 模块地图

### 3.1 窗口管理核心

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/workspace.h/.cpp` | `Workspace` | 窗口列表管理、虚拟桌面、活动切换、屏幕布局 | 窗口找不到、桌面切换异常 |
| `src/window.h/.cpp` | `Window`（抽象基类） | 窗口几何、属性、状态、规则应用、transient 关系 | 窗口几何/焦点/属性问题 |
| `src/x11window.h/.cpp` | `X11Window : Window` | X11 窗口实现（ICCCM/EWMH） | X11 窗口行为问题 |
| `src/waylandwindow.h/.cpp` | `WaylandWindow : Window` | Wayland 窗口实现（xdg-shell） | Wayland 窗口行为问题 |
| `src/xdgshellwindow.h` 等 | `XdgToplevelWindow` 等 | xdg-shell 窗口协议实现 | 弹窗/toplevel 行为问题 |
| `src/activation.cpp` | `Workspace` 方法 | 窗口激活、焦点抢占、user timestamp | 焦点抢占/激活顺序问题 |
| `src/placement.cpp` | `Placement` | 窗口初始放置策略 | 窗口位置不对 |
| `src/rules.h/.cpp` | `Rules` | 窗口规则（类型/桌面/位置/大小强制约束） | 窗口规则不生效 |
| `src/options.h` | `Options` | 全局用户配置（焦点策略、移动阈值等） | 配置项行为问题 |
| `src/screenedge.h` | `ScreenEdges` | 屏幕边缘触发 | 边缘手势不响应 |
| `src/tabbox/` | `TabBox::TabBoxHandler` | Alt+Tab 窗口切换器 | 切换器 UI/逻辑问题 |
| `src/tiles/` | `Tile`、`TileManager`、`QuickTile`、`CustomTile` | 窗口平铺布局 | 平铺/分屏布局问题 |
| `src/splitscreen/` | `SplitManage`、`SplitBar`、`SplitMenu` | deepin 分屏功能 | 分屏拖拽/菜单问题 |

### 3.2 合成与渲染

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/composite.h/.cpp` | `Compositor`、`WaylandCompositor`、`X11Compositor` | 合成调度、帧循环、VSync | 画面撕裂/卡顿/无输出 |
| `src/scene/scene.h` | `Scene`（抽象） | 渲染目标抽象，`paint()` 虚接口 | 自定义渲染问题 |
| `src/scene/workspacescene.h` | `WorkspaceScene : Scene` | 工作区场景（Item 树根） | 场景构建问题 |
| `src/scene/workspacescene_opengl.cpp` | `WorkspaceSceneOpenGL` | OpenGL 场景实现 | OpenGL 渲染问题 |
| `src/scene/workspacescene_qpainter.cpp` | `WorkspaceSceneQPainter` | QPainter 场景实现（软件渲染） | 软件渲染问题 |
| `src/scene/workspacescene_xrender.cpp` | `WorkspaceSceneXRender` | XRender 场景实现 | XRender 渲染问题 |
| `src/scene/item.h` | `Item` | 场景图节点基类（位置/变换/子节点） | 图元不显示/层级错误 |
| `src/scene/surfaceitem.h` | `SurfaceItem` | Wayland/X11 surface 图元 | surface 内容不更新 |
| `src/scene/windowitem.h` | `WindowItem` | 窗口图元（装饰+shadow+surface 组合） | 窗口渲染内容问题 |
| `src/scene/itemrenderer.h` | `ItemRenderer` | 渲染器抽象（OpenGL/QPainter/XRender） | 渲染器选型问题 |
| `src/core/renderloop.h` | `RenderLoop` | 每输出帧循环驱动 | 帧率/VSync 问题 |
| `src/core/renderlayer.h` | `RenderLayer` | 渲染层（cursor/overlay 管理） | 光标/叠加层问题 |
| `src/core/overlaywindow.h` | `OverlayWindow` | 合成覆盖窗口（X11） | X11 合成覆盖问题 |

### 3.3 后端层

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/core/outputbackend.h` | `OutputBackend`（抽象基类） | 输出设备抽象：输出列表、创建 OpenGL/QPainter/XRender 后端、输入后端、虚拟输出 | 后端选型/初始化失败 |
| `src/core/inputbackend.h` | `InputBackend`（抽象基类） | 输入设备抽象 | 输入设备不识别 |
| `src/core/renderbackend.h` | `RenderBackend`（抽象基类） | 渲染后端抽象 | 渲染后端创建失败 |
| `src/core/output.h` | `Output` | 单个输出（显示器）抽象：几何、缩放、模式、DPMS | 显示器配置/热插拔 |
| `src/core/session.h` | `Session` | 会话管理（logind/consolekit/noop） | 会话/VT 切换问题 |
| `src/backends/drm/` | `DrmBackend : OutputBackend` | DRM/KMS 原生后端（主生产后端） | DRM 输出/显示问题 |
| `src/backends/wayland/` | `WaylandBackend : OutputBackend` | 嵌套 Wayland 后端 | 嵌套运行问题 |
| `src/backends/x11/standalone/` | `X11StandaloneBackend` | X11 原生后端 | X11 合成后端问题 |
| `src/backends/x11/windowed/` | `X11WindowedBackend` | X11 窗口化后端（嵌套） | 嵌套 X11 问题 |
| `src/backends/virtual/` | `VirtualBackend` | 虚拟后端（测试/CI） | 测试环境问题 |
| `src/backends/libinput/` | `LibinputBackend : InputBackend` | libinput 输入后端 | 输入事件处理 |
| `src/backends/fakeinput/` | `FakeInputBackend` | FakeInput（测试用） | 测试输入问题 |

### 3.4 Wayland 协议层

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/waylandserver.h` | `WaylandServer` | Wayland 显示服务器单例，管理所有协议接口 | 协议注册/客户端连接 |
| `src/wayland/*_interface.h` | `*Interface`（如 `SurfaceInterface`、`XdgShellInterface`） | Wayland 协议接口实现 | 协议行为问题 |
| `src/wayland/clientconnection.h` | `ClientConnection` | 客户端连接管理 | 客户端崩溃/识别 |
| `src/xwayland/` | `Xwl::Xwayland`、`Databridge`、`Selection` | Xwayland 桥接（剪贴板、DND、拖拽） | Xwayland 互操作问题 |

### 3.5 输入处理

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/input.h` | `Input` | 输入事件分发中枢，管理 filter 链和 spy 链 | 事件不响应/拦截异常 |
| `src/input_event.h` | `MouseEvent`、`KeyEvent`、`TouchEvent` | 输入事件封装 | 事件数据问题 |
| `src/input_event_spy.h` | `InputEventSpy` | 输入事件观察者（不拦截） | 仅需观察事件 |
| `src/input.h` | `InputEventFilter` | 输入事件过滤器（可拦截） | 全屏/特效中事件拦截 |
| `src/pointerinput.h` 等 | `PointerInputRedirection` 等 | 指针/键盘/触摸事件重定向 | 指针/触摸坐标问题 |

### 3.6 特效系统

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/libkwineffects/kwineffects.h` | `Effect`（基类）、`EffectsHandler`（API 门面） | 特效插件公共 API | 新增特效行为问题 |
| `src/effects.h/.cpp` | `EffectsHandlerImpl : EffectsHandler` | 特效管理实现，调度 prePaint/paint/postPaint | 特效调度问题 |
| `src/effectloader.cpp` | `EffectLoader` | 特效插件加载 | 特效不加载 |
| `src/effects/<name>/` | 各特效子目录 | 独立特效实现（如 `multitaskview`、`blur`、`screenshot`） | 具体特效行为问题 |
| `src/scene/` | `Scene` + Effect hook | 特效与场景交互的 paint 流水线 | 特效渲染异常 |

### 3.7 其他扩展

| 路径 | 核心类 | 职责 | 典型问题 |
|------|--------|------|---------|
| `src/decorations/` | `DecorationBridge`、`DecoratedClient` | 窗口装饰框架 | 装饰渲染/交互问题 |
| `src/scripting/` | `Scripting` | JavaScript 脚本扩展（QML 效果/KWin scripts） | 脚本加载/执行问题 |
| `src/windowstyle/` | `WindowStyleManager`、`WindowRadius`、`WindowShadow` | deepin 窗口样式（圆角/阴影） | 圆角/阴影渲染问题 |
| `src/colors/` | `ColorManager`、`ColorDevice` | 色彩管理（ICC/HDR） | 颜色/亮度问题 |
| `src/core/colorlut.h` 等 | `ColorLut`、`ColorPipelineStage` | 颜色管线阶段 | 色彩管线问题 |
| `src/plugins/` | 各插件子目录 | 可选插件（qpa、screencast、nightcolor 等） | 插件功能问题 |
| `src/utils/` | `common.h`、`c_ptr.h` 等 | 共享工具函数、日志类别、全局常量 | 工具函数问题 |
| `src/dbusinterface.h` | DBus 接口 | KWin 对外 DBus 服务 | DBus 通信问题 |

## 4. 关键接口设计

### 4.1 Window 类层次

```
Window (抽象, QObject)
├── X11Window       — X11 窗口
└── WaylandWindow   — Wayland 窗口
    └── XdgToplevelWindow / XdgPopupWindow (xdg-shell)
```

`Window` 定义了大量虚函数（`windowType()`、`isPopupWindow()`、`closeWindow()`、`killWindow()` 等），子类必须实现平台特定行为。

修改窗口行为时，优先看 `Window` 是否已声明虚函数，若是则在对应子类覆盖。

### 4.2 后端创建链

`OutputBackend` 是后端的抽象入口：

```cpp
class OutputBackend {
    virtual bool initialize() = 0;
    virtual std::unique_ptr<InputBackend> createInputBackend();
    virtual std::unique_ptr<OpenGLBackend> createOpenGLBackend();
    virtual std::unique_ptr<QPainterBackend> createQPainterBackend();
    virtual std::unique_ptr<XRenderBackend> createXRenderBackend();
    virtual QVector<CompositingType> supportedCompositors() const = 0;
    virtual Outputs outputs() const = 0;
};
```

合成器通过 `OutputBackend::supportedCompositors()` 决定可用合成类型，再调 `create*Backend()` 创建对应渲染后端。

### 4.3 Scene paint 流水线

```
Compositor::composite()
  → WorkspaceScene::paint(renderTarget, region)
    → 遍历 Item 树，调用 Effect prePaintWindow/paintWindow/postPaintWindow
    → ItemRenderer::renderItem(item, renderTarget)
```

渲染器实现：`ItemRendererOpenGL`、`ItemRendererQPainter`、`ItemRendererXRender`。

### 4.4 输入事件链

```
InputBackend 产生事件
  → InputRedirection (Pointer/Keyboard/Touch) 分发
    → InputEventFilter 链（可拦截，如全屏/特效）
    → InputEventSpy 链（仅观察）
    → 最终投递到 Workspace/Window
```

新增输入行为时：拦截用 `InputEventFilter`，观察用 `InputEventSpy`。

## 5. 问题路由速查

| 问题类型 | 首选模块 | 关键文件 |
|---------|---------|---------|
| 窗口焦点/激活异常 | 窗口管理 | `activation.cpp`、`window.h` |
| 窗口位置/大小不对 | 窗口管理 | `placement.cpp`、`window.h`、`geometry.cpp` |
| 窗口规则不生效 | 窗口管理 | `rules.cpp` |
| Alt+Tab 切换器问题 | 扩展 | `tabbox/` |
| 平铺/分屏布局 | 窗口管理 | `tiles/`、`splitscreen/` |
| 画面撕裂/卡顿 | 合成 | `composite.cpp`、`core/renderloop.cpp` |
| OpenGL 渲染异常 | 合成 | `scene/workspacescene_opengl.cpp`、`backends/*egl*` |
| 软件渲染异常 | 合成 | `scene/workspacescene_qpainter.cpp` |
| 显示器热插拔/配置 | 后端 | `core/output.cpp`、`backends/drm/` |
| 输入事件不响应 | 输入 | `input.h`、`backends/libinput/` |
| Xwayland 互操作 | Wayland | `xwayland/` |
| Wayland 协议行为 | Wayland | `wayland/*_interface.h` |
| 特效行为 | 扩展 | `effects/<name>/`、`effects.cpp` |
| 窗口装饰/圆角/阴影 | 扩展 | `decorations/`、`windowstyle/` |
| 颜色/亮度 | 其他 | `colors/`、`core/colorlut.cpp` |
| DBus 通信 | 其他 | `dbusinterface.h` |
| 全局配置项 | 窗口管理 | `options.h` |

## 6. 命名空间

| 命名空间 | 包含 |
|---------|------|
| `KWin` | 核心模块（Workspace、Window、Compositor 等） |
| `KWin::TabBox` | 窗口切换器 |
| `KWin::Decoration` | 窗口装饰 |
| `KWin::Xwl` | Xwayland 桥接 |
| `KWaylandServer` | Wayland 协议接口 |
| `KDecoration2` | 装饰框架（外部库） |
