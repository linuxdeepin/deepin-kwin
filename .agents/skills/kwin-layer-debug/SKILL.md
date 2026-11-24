# KWin 窗口层级与锁屏可见性调试

锁屏下"程序启动了但看不见窗口"这类问题的定位手段。核心是给 kwin 打 DIAG 日志，
用**层级数值 + 可见性判定结果**代替肉眼观察，避免反复让人确认屏幕。

## 适用场景

- 锁屏下按快捷键无响应，或进程起来了但窗口不可见
- 判断某个窗口是否被锁屏表面遮挡（层级问题）
- 确认 `_d_dwayland_window-type` / plasma-shell role 是否真的生效

## 一、kwin 侧要插桩的位置

两个函数覆盖了"窗口在第几层"和"窗口是否可见"：

| 位置 | 函数 | 作用 |
|------|------|------|
| `src/layers.cpp` | `computeLayer()` | 窗口最终进入哪个 Layer |
| `src/scene/windowitem.cpp` | `WindowItem::computeVisibility()` | 锁屏可见性门禁判定 |

### `src/layers.cpp` — computeLayer

```cpp
static Layer computeLayer(const Window *window)
{
    Layer diagLayer;
    if (auto x11Window = qobject_cast<const X11Window *>(window)) {
        diagLayer = layerForWindow(x11Window);
    } else {
        diagLayer = window->layer();
    }
    // [DIAG LAYER] trace which layer a window ends up in
    qWarning("DIAG LAYER resClass=\"%s\" resName=\"%s\" type=%d layer=%d",
             qUtf8Printable(window->resourceClass()), qUtf8Printable(window->resourceName()),
             int(window->windowType()), int(diagLayer));
    return diagLayer;
}
```

### `src/scene/windowitem.cpp` — computeVisibility

锁屏时只放行三类窗口（`isLockScreen` / `isInputMethod` / `isLockScreenOverlay`），
其余一律不可见。在这里打印判定输入与结果：

```cpp
    if (waylandServer() && waylandServer()->isScreenLocked()) {
        // [DIAG LOCKVIS] the lock-screen gate
        const bool diagVis = m_window->isLockScreen() || m_window->isInputMethod() || m_window->isLockScreenOverlay();
        qWarning("DIAG LOCKVIS resClass=\"%s\" lockScreen=%d inputMethod=%d lockOverlay=%d -> visible=%d",
                 qUtf8Printable(m_window->resourceClass()),
                 int(m_window->isLockScreen()), int(m_window->isInputMethod()),
                 int(m_window->isLockScreenOverlay()), int(diagVis));
        return diagVis;
    }
```

> **不要用 `window->caption()`**。在 `computeLayer()` 里（由
> `Workspace::constrainedStackingOrder()` 调用）访问 `caption()` 可能递归或碰到尚未
> ready 的 backend，会让 greeter 崩溃、lightdm 进入 `Failed with result 'exit-code'`
> 重启循环。`resourceClass()` / `resourceName()` 是普通成员，足够定位窗口。

## 二、安装：必须搞清动态链接器实际读哪个文件

kwin 的层级代码在 `libkwin.so.5.27.2` 里，**不在** `kwin_wayland` 可执行文件里。
只替换库、或只替换二进制，都可能完全没生效。

**关键陷阱**：`libkwin.so.5` 的符号链接指向哪个文件，才是真正被加载的文件。

```bash
readlink -f /lib/aarch64-linux-gnu/libkwin.so.5
# 例如输出 .../libkwin.so.5.27.2.orig  —— 那你往 .5.27.2 里装是白费
```

安装前先确认链接目标，然后把 DIAG 库覆盖到**那个文件**上：

```bash
cd ~/kwin-scale/build && ninja
TARGET=$(readlink -f /lib/aarch64-linux-gnu/libkwin.so.5)
sudo cp bin/libkwin.so.5.27.2 "$TARGET"
```

验证 DIAG 代码确实被加载：

```bash
KP=$(pgrep -x kwin_wayland | head -1)
sudo grep libkwin /proc/$KP/maps | awk '{print $6}' | sort -u
strings "$TARGET" | grep -c "DIAG LAYER"
```

`maps` 里出现的路径必须和你覆盖的文件一致。

## 三、会话准备：lightdm 自动登录

反复重启 lightdm 后停在登录界面会打断验证流程，先把自动登录配好。

`/etc/lightdm/lightdm.conf` 的 `[Seat:*]` 段（约 92-150 行）内需要这四项：

```ini
pam-autologin-service=lightdm-autologin
autologin-user=uos
autologin-user-timeout=0
autologin-session=deepin
```

一键设置：

```bash
F=/etc/lightdm/lightdm.conf
sudo sed -i \
  -e 's/^#pam-autologin-service=lightdm-autologin/pam-autologin-service=lightdm-autologin/' \
  -e 's/^#autologin-user-timeout=0/autologin-user-timeout=0/' \
  -e 's/^#autologin-session=/autologin-session=deepin/' \
  -e 's/^#autologin-user=/autologin-user=uos/' "$F"
sudo grep -nE '^autologin|^pam-autologin' "$F"
sudo systemctl restart lightdm && sleep 55
```

确认会话里的 kwin 是**目标用户**在跑（greeter 的 kwin 是 lightdm 用户，不是你要测的那个）：

```bash
pgrep -x kwin_wayland | while read p; do echo "pid=$p user=$(ps -o user= -p $p)"; done
```

## 四、复现与取证

```bash
# 1. 锁定会话（不要用 systemctl restart lightdm，那会丢失会话）
loginctl lock-session

# 2. 确认锁屏已生效
pgrep -c dde-lock          # 应 >= 1

# 3. 触发被测程序（示例：Ctrl+Alt+A 截图录屏，用 uinput 注入真实按键）
sudo python3 inj_key.py 29 56 30     # 29=Ctrl 56=Alt 30=A

# 4. 确认进程被拉起
pgrep -af "deepin-screen-record[e]r"

# 5. 取层级与可见性日志
sudo journalctl -b --since "-2 min" --no-pager | grep "DIAG LAYER"
sudo journalctl -b --since "-2 min" --no-pager | grep "DIAG LOCKVIS"
```

### 判读

| 观察到的现象 | 结论 |
|---|---|
| `DIAG LOCKVIS ... -> visible=0` | 被锁屏可见性门禁挡掉（非 lockScreen/inputMethod/lockOverlay） |
| 出现 `DIAG LAYER` 但无 `DIAG LOCKVIS` | `isScreenLocked()` 为 false，走的是普通层级逻辑，改用 layer 数值比较 |
| 两者都没有 | DIAG 库没被加载，回到第二节确认 `readlink -f` 与 `maps` |
| `visible=1` 但仍看不见 | 问题不在 kwin 侧，检查客户端是否真的映射了窗口 / 是否画了内容 |

## 五、辅助工具：绕过 X11/抓屏的按键注入

XTEST（`xdotool key`）在纯 Wayland 会话下不经过 kwin 的输入管线，会得出错误结论。
必须用 `uinput` 注入真实键事件，才能让按键走 `KeyboardInputRedirection` → 过滤器链。

```bash
# inj_key.py <keycode...>：除了最后一个键都按住，最后一个按下并松开
sudo python3 inj_key.py 29 56 30      # Ctrl+Alt+A
sudo python3 inj_key.py 56 23         # Alt+Tab
sudo python3 inj_key.py 58            # CapsLock（用于验证注入是否真的进了 kwin：看 LED）
```

**注入连通性自检**：注入 CapsLock 后读键盘 LED，能翻转说明事件确实进了 kwin 的
`processKey()`（→ Xkb → `updateLeds()`），这是最便宜的控制变量：

```bash
cat /sys/class/leds/input4::capslock/brightness   # 注入前后对比
```

## 六、坑位清单

- **`pkill -f "deepin-screen-recorder"` 会杀掉自己的 shell**（ssh 命令行里也含该字符串）。
  用 `pgrep -f "deepin-screen-record[e]r"` 取 PID 再 `kill`，或用 `"screen-""recorder"` 拼接。
- **`libkwin.so.5` 是符号链接**，覆盖 `.so.5.27.2` 不一定生效，见第二节。
- **只换库不换二进制**、或反过来，可能 ABI 不匹配导致 greeter 崩溃重启循环：
  `lightdm: instance with invalid (NULL) class pointer` + `Failed with result 'exit-code'`。
  恢复办法：`dpkg -i` 重装 `kwin-wayland` / `kwin-common` / `kwin-data` /
  `libkwineffects14` / `libkwinglutils14`。
- **会话卡在 `closing`** 会让新会话起不来：`loginctl show-session <id> -p State`，
  必要时 `loginctl terminate-session <id>`。
- **`org.kde.KWin.ScreenShot2` 抓不到锁屏画面**（锁屏时返回全屏均匀色），
  不要用它判断锁屏下窗口是否可见。
- `com.deepin.daemon.KWayland` 的 PlasmaWindow 列表**不包含** dde-lock 这类 Wayland 表面，
  不要据此判断锁屏窗口不存在。
