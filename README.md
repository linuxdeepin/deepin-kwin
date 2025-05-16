# Deepin KWin

deepin-kwin is deepin desktop environment's core window manager, customized based on KWin 5.27 with Qt6/KF6 support. KWin is an easy to use, but flexible, composited Window Manager for Xorg windowing systems (Wayland, X11) on Linux. Its primary usage is in conjunction with a Desktop Shell (e.g. KDE Plasma Desktop). KWin is designed to go out of the way; users should not notice that they use a window manager at all. Nevertheless KWin provides a steep learning curve for advanced features, which are available, if they do not conflict with the primary mission. KWin does not have a dedicated targeted user group, but follows the targeted user group of the Desktop Shell using KWin as it's window manager.

## Build Requirements

- Qt 6.7+
- KDE Frameworks 6.0+
- XCB related development libraries

## Compilation & Installation

```
git clone https://github.com/linuxdeepin/deepin-kwin.git
cd deepin-kwin
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cmake --install build
```

## Getting Involved

- [Code contribution via GitHub](https://github.com/linuxdeepin/deepin-kwin/)
- [Submit bug or suggestions to GitHub Issues or GitHub Discussions](https://github.com/linuxdeepin/developer-center/issues/new/choose)

## Guidelines for new features

A new Feature can only be added to KWin if:

 * it does not violate the primary missions as stated at the start of this document
 * it does not introduce instabilities
 * it is maintained, that is bugs are fixed in a timely manner (second next minor release) if it is not a corner case.
 * it works together with all existing features
 * it supports both single and multi screen (xrandr)
 * it adds a significant advantage
 * it is feature complete, that is supports at least all useful features from competitive implementations
 * it is not a special case for a small user group
 * it does not increase code complexity significantly
 * it does not affect KWin's license (GPLv2+)

All new added features are under probation, that is if any of the non-functional requirements as listed above do not hold true in the next two feature releases, the added feature will be removed again.

The same non functional requirements hold true for any kind of plugins (effects, scripts, etc.). It is suggested to use scripted plugins and distribute them separately.

## License

**deepin-kwin** is licensed under GPL-2.0-or-later.