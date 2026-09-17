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

    // The C++ side sizes the overlay to this implicit size, which only changes
    // if the font environment changes. The FPS value sits in a fixed-width
    // slot sized for four digits and is left-aligned, so every text position
    // stays constant while the reported values change.
    implicitWidth: grid.implicitWidth + 2
    implicitHeight: grid.implicitHeight + 2

    color: "transparent"

    FontMetrics {
        id: valueMetrics
        font.bold: true
        font.pointSize: 20
    }

    GridLayout {
        id: grid
        anchors.right: parent.right
        anchors.top: parent.top
        columns: 3
        columnSpacing: 6
        rowSpacing: 2

        Label {
            text: "Current"
            font.bold: true
            font.pointSize: 20
        }
        Label {
            text: "FPS:"
            font.bold: true
            font.pointSize: 20
        }
        Label {
            text: root.effect ? root.effect.fps : "N/A"
            horizontalAlignment: Text.AlignLeft
            Layout.preferredWidth: valueMetrics.advanceWidth("000")
            font.bold: true
            font.pointSize: 20
        }

        Label {
            text: "Maximum"
            font.bold: true
            font.pointSize: 20
        }
        Label {
            text: "FPS:"
            font.bold: true
            font.pointSize: 20
        }
        Label {
            text: root.effect ? root.effect.maximumFps : "N/A"
            horizontalAlignment: Text.AlignLeft
            Layout.preferredWidth: valueMetrics.advanceWidth("000")
            font.bold: true
            font.pointSize: 20
        }

        Label {
            Layout.columnSpan: 3
            text: i18nc("@label", "This effect is not a benchmark")
        }
    }
}
