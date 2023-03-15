// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

registerShortcut(
  "Window Absolute Maximize",
  "Window Absolute Maximize",
  "Meta+Up",
  function () {
    workspace.activeClient.setMaximize(true, true);
  }
);

registerShortcut(
  "Window Unmaximize",
  "Window Unmaximize",
  "Meta+Down",
  function () {
    workspace.activeClient.setMaximize(false, false);
  }
);
