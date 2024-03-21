/*
    SPDX-FileCopyrightText: 2023 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.0

Rectangle {
    id: root
    property QtObject effectFrame: null

    width: effectFrame ? effectFrame.size.width : 0
    height: effectFrame ? effectFrame.size.height : 0
    color: "transparent"

    Rectangle {
        id: rect2;
        width: parent.width;
        height: parent.height;
        border.color: effectFrame ? effectFrame.color : ""
        border.width: 4;
        color: "transparent";
        radius: effectFrame ? effectFrame.radius : 0
    }
}
