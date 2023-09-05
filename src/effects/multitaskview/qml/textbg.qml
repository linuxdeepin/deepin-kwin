import QtQuick 2.0

Rectangle {
    id: root
    property QtObject effectFrame: null
    width: effectFrame.size.width
    height: effectFrame.size.height
    color: "#E0E0E0"
    radius: effectFrame.radius
}