import QtQuick 2.10
import QtGraphicalEffects 1.0

Rectangle {
    id: root

    property QtObject effectFrame: null

    width: effectFrame.size.width
    height: effectFrame.size.height
    radius: effectFrame.radius
    color: "transparent"

    Image {
        id: icon
        width: root.width
        height: root.height
        source: root.effectFrame.image
        sourceSize: root.effectFrame.size

        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Item {
                width: icon.width
                height: icon.height
                Rectangle {
                    anchors.centerIn: parent
                    width: icon.width - 2
                    height: icon.height - 2
                    radius: root.radius
                }
            }
        }
    }
}