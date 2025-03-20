import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property QtObject effectFrame: null
    Text {
        font.family: effectFrame ? effectFrame.font.family : ""
        font.pointSize: effectFrame ? effectFrame.font.pointSize : 10
        text: effectFrame ? effectFrame.text : ""
    }
}
