/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
import QtQuick 2.0
import org.kde.plasma.components 2.0 as Plasma
import QtQuick.Controls 2.0

Item {
    width: childrenRect.width + buttonLayout.spacing + 32
    height: childrenRect.height + 32
    Plasma.ButtonRow {
        id: buttonLayout
        anchors.centerIn: parent
        exclusive: false
        width: childrenRect.width
        height: childrenRect.height
        spacing: 16
        Button {
            id: removeButton
            objectName: "removeButton"
            enabled: remove
            width: height
            background: Image {
                source: "icons/reduce_normal.svg"
            }
            onHoveredChanged: {
                background.source = removeButton.hovered ? "icons/reduce_hover.svg" : "icons/reduce_normal.svg"
            }
            onPressedChanged: {
                background.source = removeButton.pressed ? "icons/reduce_press.svg" : "icons/reduce_hover.svg"
            }
        }
        Button {
            id: addButton
            objectName: "addButton"
            enabled: add
            width: height
            background: Image {
                source: "icons/add_normal.svg"
            }
            onHoveredChanged: {
                background.source = addButton.hovered ? "icons/add_hover.svg" : "icons/add_normal.svg"
            }
            onPressedChanged: {
                background.source = addButton.pressed ? "icons/add_press.svg" : "icons/add_hover.svg"
            }
        }
    }
}
