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

Item {
    width: units.iconSizes.medium
    height: width

    Plasma.Button {
        id: closeButton
        objectName: "closeButton"
        iconSource: "window-close"
        anchors.fill: parent

        Accessible.role: Accessible.Button
        Accessible.name: "highlightWin_closeButton"
        Accessible.description: "super + a/w closeButton"
        Accessible.onPressAction: pressed()
    }
}
