import QtQuick 2.0
import QtQuick.Layouts 1.0

ColumnLayout {
    id: root
    property QtObject effectFrame: null
    Text {
        font.family: effectFrame.font.family
        font.pointSize: effectFrame.font.pointSize
        text: effectFrame.text
    }
}