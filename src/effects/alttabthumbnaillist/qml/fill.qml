import QtQuick 2.0
import QtGraphicalEffects 1.0

Item {
    property QtObject effectFrame: null

    width: effectFrame ? effectFrame.size.width : 0
    height: effectFrame ? effectFrame.size.height : 0
    clip: true

    readonly property double maskRadius: 12 * (effectFrame ? effectFrame.scale : 1)

    Rectangle {
        id: frameBorder

        x: effectFrame ? effectFrame.clipOffset.x : 0
        y: effectFrame ? effectFrame.clipOffset.y : 0
        width: parent.width
        height: parent.height

        color: "transparent"
        radius: maskRadius
        border.width: 1
        border.color: "#19000000"
    }

    Rectangle {
        id: frame
        anchors.fill: frameBorder
        anchors.margins: 1

        color: "transparent"

        Rectangle {
            id: title_rect
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: frame.top

            width: parent.width + 2
            height: 40 * (effectFrame ? effectFrame.scale : 1)
            color: "#CCFFFFFF"
        }

        Rectangle {
            anchors.fill: title_rect
            anchors.margins: -1
            anchors.bottomMargin: 0

            color: "transparent"
            border.width: 1
            border.color: "#0D000000"
        }

        Image {
            id: icon
            anchors.verticalCenter: title_rect.verticalCenter
            anchors.left: title_rect.left
            anchors.leftMargin: 8 * (effectFrame ? effectFrame.scale : 1)

            width: effectFrame ? effectFrame.iconSize.width : 0
            height: effectFrame ? effectFrame.iconSize.height : 0
            source: effectFrame ? effectFrame.image : ""
        }

        Text {
            anchors.verticalCenter: title_rect.verticalCenter
            anchors.left: icon.right
            anchors.leftMargin: 8 * (effectFrame ? effectFrame.scale : 1)
            anchors.right: title_rect.right
            anchors.rightMargin: 8 * (effectFrame ? effectFrame.scale : 1)

            font.pointSize: 10
            text: effectFrame ? effectFrame.text : ""
            elide: Text.ElideRight
        }

        Rectangle {
            anchors.top: title_rect.bottom

            width: parent.width
            height: parent.height - title_rect.height
            color: "#CCFFFFFF"
            visible: effectFrame ? effectFrame.frameOpacity == 1 : true
        }

        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Item {
                width: frame.width
                height: frame.height
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    radius: maskRadius
                }
            }
        }
    }
}
