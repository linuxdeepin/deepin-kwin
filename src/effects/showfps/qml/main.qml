/*
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3

Rectangle {
    id: root

    property QtObject effect

    color: "transparent"
    ColumnLayout {
        anchors.fill: parent

        Label {
            Layout.fillWidth: true

            Text {
                text: root.effect ? "Current FPS: " + root.effect.fps : "Current FPS: N/A"
                font.bold: true
                font.pointSize: 20
            }
        }

        Label {
            Layout.fillWidth: true

            Text {
                text: root.effect ? "Maximum FPS: " + root.effect.maximumFps : "Maximum FPS: N/A"
                font.bold: true
                font.pointSize: 20
            }
        }

        Label {
            Layout.fillWidth: true
            text: i18nc("@label", "This effect is not a benchmark")
        }
    }
}
