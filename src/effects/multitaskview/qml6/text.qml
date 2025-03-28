import QtQuick

Text {
   property QtObject effectFrame: null
   font.family: effectFrame ? effectFrame.font.family : ""
   font.pointSize: effectFrame ? effectFrame.font.pointSize : 10
   text: effectFrame ? effectFrame.text : ""
}

