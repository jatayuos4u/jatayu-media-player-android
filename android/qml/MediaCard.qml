import QtQuick

Item {
    id: root
    required property var entry
    signal activated()
    signal optionsRequested()
    implicitHeight: width * 0.62 + 72
    Rectangle {
        id: art
        width: parent.width; height: width * 0.62
        color: "#222b32"; radius: 14; clip: true
        Image {
            anchors.fill: parent
            source: "image://artwork/" + encodeURIComponent(root.entry.url)
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
        }
        Rectangle {
            anchors.bottom: parent.bottom; width: parent.width; height: 4
            visible: root.entry.duration > 0 && root.entry.position > 0
            color: "#383d47"
            Rectangle { height: parent.height; color: "#ff9328"; width: parent.width * Math.min(1, root.entry.position / Math.max(1, root.entry.duration)) }
        }
        Icon { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10; name: root.entry.kind === "audio" ? "music" : "video"; width: 20; height: 20 }
        Rectangle {
            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 8
            color: "#da080b10"; radius: 5; width: durationLabel.implicitWidth + 12; height: 24
            visible: root.entry.duration > 0
            Text { id: durationLabel; anchors.centerIn: parent; color: "white"; font.pixelSize: 11; text: Math.floor(root.entry.duration / 60000) + ":" + ("0" + Math.floor(root.entry.duration / 1000) % 60).slice(-2) }
        }
        TapHandler { onTapped: root.activated() }
    }
    Text {
        anchors.top: art.bottom; anchors.topMargin: 10; width: parent.width - 38
        text: root.entry.title; color: "#f6f7fa"; font.pixelSize: 14; elide: Text.ElideRight
    }
    Text {
        anchors.top: art.bottom; anchors.topMargin: 31; width: parent.width - 38
        text: root.entry.kind === "audio" ? root.entry.artist || qsTr("Local music") :
              (root.entry.height > 0 ? root.entry.height + "p • " : "") +
              (root.entry.size > 0 ? (root.entry.size / 1048576).toFixed(1) + " MB" : qsTr("Local video"))
        elide: Text.ElideRight
        color: "#9da6b6"; font.pixelSize: 12
    }
    ActionButton { anchors.right: parent.right; anchors.top: art.bottom; implicitWidth: 38; symbol: "more"; quiet: true; Accessible.name: qsTr("Media options"); onClicked: root.optionsRequested() }
}
