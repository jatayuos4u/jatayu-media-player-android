import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string symbol: ""
    property bool primary: false
    property bool selected: false
    property bool quiet: false
    implicitHeight: 48
    implicitWidth: text.length ? buttonRow.implicitWidth + 24 : 48
    padding: 12
    leftPadding: 12; rightPadding: 12; topPadding: 12; bottomPadding: 12
    Accessible.name: text.length ? text : symbol
    background: Rectangle {
        radius: control.primary && !control.text.length ? height / 2 : 14
        color: control.primary || control.selected ? "#ff9328" : control.down ? "#3c424d" : control.quiet ? "transparent" : "#252a33"
        border.width: control.quiet || control.primary || control.selected ? 0 : 1
        border.color: "#343b46"
        opacity: control.enabled ? 1 : 0.4
    }
    contentItem: Item {
        implicitWidth: buttonRow.implicitWidth
        implicitHeight: buttonRow.implicitHeight
        Row {
            id: buttonRow
            anchors.centerIn: parent
            spacing: control.symbol.length && control.text.length ? 8 : 0
            Icon {
                visible: control.symbol.length > 0
                name: control.symbol
                color: control.primary || control.selected ? "#13161b" : "#f6f7fa"
                width: visible ? 22 : 0; height: 22
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                textFormat: Text.PlainText
                text: control.text
                font.pixelSize: 14
                font.weight: control.primary || control.selected ? Font.DemiBold : Font.Normal
                color: control.primary || control.selected ? "#13161b" : "#f6f7fa"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
