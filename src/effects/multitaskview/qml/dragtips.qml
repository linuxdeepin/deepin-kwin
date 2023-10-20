import QtQuick 2.0
import QtQuick.Layouts 1.0

RowLayout {
    id: root
    property QtObject effectFrame: null

    spacing: 13

    Image {
        id: icon
        Layout.preferredWidth: root.effectFrame.iconSize.width
        Layout.preferredHeight: root.effectFrame.iconSize.height
        source: root.effectFrame.image
    }

    Text {
        id: label
        font.family: root.effectFrame.font.family
        font.pointSize: root.effectFrame.font.pointSize
        text: root.effectFrame.text
        color: root.effectFrame.color
    }
}