---
name: local-build-runtime
description: 在 UOS 25（v25, Qt6+KF6）真机上运行本地构建的 KWin 的方法与坑。修改源码后要在真机验收、需要 systemd override 覆盖 dde-session@x11.service、怀疑「改了没生效」、或需要打开 kwin 的 journal 日志时必读。涵盖 ExecStart/LD_LIBRARY_PATH 双覆盖、部署自证、dconfig 覆盖日志规则、回退步骤与常见陷阱。
---

# 在 v25 真机上运行本地构建的 KWin

## 结论先行

X11 standalone 后端的代码（mirror layout、effects、backend）编译进**可执行文件 `build/bin/kwin_x11`**，而不是 `libkwin.so`。因此只设 `LD_LIBRARY_PATH` 指向 `build/bin` 时，这些改动**不会生效**——看到的现象会是「改了但行为没变」。必须同时覆盖 `ExecStart`。

## 1. 构建

按 [build-spec](../build-spec/SKILL.md) 配置构建：

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja -C build
```

产物：

| 文件 | 内容 |
|------|------|
| `build/bin/kwin_x11` | X11 standalone 后端（mirror layout、effects、backend 全在里面） |
| `build/bin/libkwin.so.5.x.y` | 核心库（窗口管理、Workspace、D-Bus） |
| `build/bin/kwin_wayland` | Wayland 会话用，X11 会话下不会运行 |

v25（Qt6 + KF6）与 v20（Qt5）的库名不同，确认 `ls build/bin/libkwin.so*` 的真实版本号再写路径。

## 2. systemd 覆盖：`dde-session@x11.service`

kwin 由用户级 unit `dde-session@x11.service` 拉起（`Type=notify`、`ExecStart=/usr/bin/kwin_x11 --replace`、`Restart=always`、`RestartSec=500ms`、`SuccessExitStatus=1`）。可用 `cat /proc/<kwin-pid>/cgroup` 确认。

写入 `~/.config/systemd/user/dde-session@x11.service.d/local-build.conf`：

```ini
[Service]
Environment=LD_LIBRARY_PATH=/home/<user>/Documents/kwin/build/bin
ExecStart=
ExecStart=/home/<user>/Documents/kwin/build/bin/kwin_x11 --replace
```

生效：

```bash
systemctl --user daemon-reload
systemctl --user restart dde-session@x11.service
```

要点：

- **空的 `ExecStart=` 必须先写一行**，否则 systemd 是「追加」而不是替换，旧的 `/usr/bin/kwin_x11` 仍在列表首位。
- 只重启这个 unit 就等于只重启 kwin（`PartOf=dde-session-pre.target`），桌面其它组件不受影响。
- 路径写错会进 `Restart=always` 循环，表现为桌面起不来 / 反复闪 —— 先 `systemctl --user status dde-session@x11.service` 看退出原因。

## 3. 部署自证（每次改完必做）

「改了没效果」先证明跑的是自己的代码，不要先怀疑逻辑：

```bash
PID=$(systemctl --user show -p MainPID --value dde-session@x11.service)
ls -l /proc/$PID/exe                      # 应为 build/bin/kwin_x11
grep -c build/bin/libkwin /proc/$PID/maps # 库也应来自 build/bin
strings build/bin/libkwin.so.* | grep <本次新增的字符串>/<日志文案>
stat -c '%y %n' build/bin/kwin_x11 build/bin/libkwin.so.*   # 对比构建时间
```

本仓库实测过的三种「假失败」：可执行文件还是系统的（布局类改动）、`libkwin` 还是系统的（Workspace/D-Bus 类改动）、两个都是新的但日志类别没注册（看不到日志）。

## 4. 日志：kwin 只进 journal，且 dconfig 覆盖环境变量

```bash
journalctl --user -t kwin_x11 -f        # 实时
journalctl --user -t kwin_x11 --since "5 min ago" | grep -i '\[MirrorMode\]'
```

`QT_LOGGING_RULES` / `KWIN_LOGGING_RULES` **会被 dconfig `org.kde.kwin.logging/log_rules` 覆盖**（last-wins），所以设环境变量常常无效。正确做法：备份后改 dconfig，验完恢复。

```bash
dde-dconfig get -a org.kde.kwin -r org.kde.kwin.logging -k log_rules
# 记下原值后追加（注意：这个版本的 dde-dconfig 没有 -t 参数，直接 -v）
dde-dconfig set -a org.kde.kwin -r org.kde.kwin.logging -k log_rules \
  -v "<原值>;kwin_mirrormode.debug=true"
# 验完恢复
dde-dconfig set -a org.kde.kwin -r org.kde.kwin.logging -k log_rules -v "<原值>"
```

默认规则通常是 `kwin*.debug=false;...`，即 `qCDebug` 一律不可见；`qCWarning` 可见。

按类别筛日志（`context.category` 会写进 journal 的 `CODE_CATEGORY` 字段）：

```bash
journalctl --user -t kwin_x11 -o json --since "30 s ago" | \
  python3 -c 'import json,sys
for l in sys.stdin:
    e=json.loads(l)
    if e.get("CODE_CATEGORY")=="kwin_mirrormode": print(e["MESSAGE"])'
```

### 新增日志类别的坑（`-fvisibility=hidden`）

`kwin` 共享库用 `-fvisibility=hidden` 编译，所以**只在 `src/` 里定义的类别符号不会被 DSO 外部看到**：`KWinX11Platform`（OBJECT 库，编进可执行文件）引用它时会直接链接失败 `undefined reference to 'KWIN_XXX()'`。

仓库现有做法（照着 `KWIN_CORE` / `main_x11.cpp:57`）：可执行文件自己也定义一份同类别。所以给 X11 后端用的类别要在 `src/backends/x11/standalone/x11_standalone_logging.cpp` 里再写一行 `Q_LOGGING_CATEGORY`，两份同名实例按名字匹配规则，行为一致。

自定义日志类别应按 [logging-spec](../logging-spec/SKILL.md) 用 `Q_DECLARE_LOGGING_CATEGORY` 声明，声明放到被两边共用的头文件里。

## 5. 回退到系统包

```bash
rm ~/.config/systemd/user/dde-session@x11.service.d/local-build.conf
systemctl --user daemon-reload
systemctl --user restart dde-session@x11.service
```

⚠️ `/usr` 是只读 ostree，不能用 `sudo cp` 覆盖，也不要猜 sudo 密码去试；本地构建只能走 drop-in 或 `~/.local` 前缀。

## 6. 常见陷阱

| 陷阱 | 现象 | 处理 |
|------|------|------|
| 只设 `LD_LIBRARY_PATH` | 布局/effects 改动不生效 | 同时覆盖 `ExecStart` |
| 手动 `./kwin_x11 --replace` 或复制 `/proc/<pid>/environ` 启动 | 起来就退，只打印 "Setting logging rules"；或和系统实例互相顶掉 | 一律走 unit 重启 |
| `ExecStart=` 空行漏写 | 仍是系统二进制 | 先清空再赋值 |
| `pkill -f "<pattern>"` | 把自己的 shell/命令也杀掉 | 用精确 pid 或 `systemctl --user stop` |
| 嵌套验证（Xephyr + `dbus-run-session`） | `org.kde.KWin` 不在私有总线上，dbus 调用失败 | 私有会话需自己起 kwin 注册服务名，或改用真机会话 |
| 单元测试直接跑 | X11 后端测试连不上显示 | `QT_QPA_PLATFORM=offscreen ctest -R <name>`，需要 RandR 的用 Xvfb/Xephyr |
| 崩溃排查 | 无输出即退出 | `journalctl --user -t kwin_x11 | grep -iE "fatal|assert"`、`coredumpctl list kwin_x11` |

## 7. 同源做法：其它 deepin 组件的本地构建运行

同一套「drop-in 覆盖 ExecStart + 自证 exe/maps + 各自日志文件」适用：

- **dde-shell**：`dde-shell@DDE.service.d/*.conf` 指向 `~/.local/bin/dde-shell-local`；日志**不进 journal**，在 `~/.cache/deepin/org.deepin.dde-shell/org.deepin.dde-shell.log`。跨版本混跑注意：新 frame 配旧 QML 会 `Create root failed`，需要对应的 `-p` 插件（如 `-p org.deepin.ds.dde-apps`）才出应用项。
- 多面板/多实例：`dde-shell` 单进程可挂多个 `-p`，第二**进程**会因 `/run/user/<uid>/dockplugin.lock` 直接 Fatal。
