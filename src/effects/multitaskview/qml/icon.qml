import QtQuick 2.0
import QtQuick.Layouts 1.0

ColumnLayout {
    id: root
    property QtObject effectFrame: null
    Image {
        id: icon
        Layout.preferredWidth: root.effectFrame.size.width
        Layout.preferredHeight: root.effectFrame.size.height
        source: root.effectFrame.image
    }
}