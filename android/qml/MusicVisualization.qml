pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Rectangle {
    id: visual
    required property var host
    property string mode: host.store.preferences.visualMode
    readonly property bool showing: visible && host.active && mode !== "artwork"
    readonly property var bandValues: showing ? host.visualizer.bands : []
    readonly property real energy: showing ? host.visualizer.level : 0
    readonly property real ringRadius: Math.min(width, height) * 0.32
    objectName: "musicVisualization"
    color: "#080d14"; radius: 20; clip: true
    border.color: "#24313d"
    Image {
        anchors.fill: parent; visible: visual.mode === "artwork"
        source: visual.host.hasMedia ? "image://artwork/" + encodeURIComponent(visual.host.playbackEngine.source.toString()) : ""
        asynchronous: true; fillMode: Image.PreserveAspectFit
    }
    Rectangle {
        anchors.centerIn: parent; width: visual.ringRadius * 2.7; height: width; radius: width / 2
        color: "#ff9328"; opacity: visual.mode === "pulse" ? visual.energy * 0.045 : 0
    }
    Item {
        id: barsArea
        anchors.fill: parent; anchors.margins: Math.max(16, visual.width * 0.045)
        visible: visual.mode === "bars"
        Image {
            anchors.horizontalCenter: parent.horizontalCenter; y: parent.height * 0.07
            width: Math.min(parent.width * 0.24, parent.height * 0.28); height: width
            source: Qt.resolvedUrl("jatayu-media-player.svg"); sourceSize: Qt.size(256, 256)
        }
        Rectangle { width: parent.width; height: 1; y: parent.height * 0.80; color: "#684321"; opacity: 0.55 }
        Repeater {
            model: 32
            delegate: Item {
                id: bar
                required property int index
                readonly property real amplitude: visual.showing && visual.mode === "bars" ? (visual.bandValues[index] || 0) : 0
                x: index * barsArea.width / 32; width: barsArea.width / 32 - 3; height: barsArea.height
                Rectangle {
                    anchors.bottom: parent.top; anchors.bottomMargin: -barsArea.height * 0.80
                    width: Math.max(2, bar.width); height: 2 + bar.amplitude * barsArea.height * 0.44; radius: width / 2
                    gradient: Gradient { GradientStop { position: 0; color: "#ffbd4a" } GradientStop { position: 1; color: "#ff8620" } }
                    Behavior on height { NumberAnimation { duration: 80 } }
                }
                Rectangle {
                    y: barsArea.height * 0.82; width: Math.max(2, bar.width); height: bar.amplitude * barsArea.height * 0.12; radius: 2; opacity: 0.12
                    gradient: Gradient { GradientStop { position: 0; color: "#ff9328" } GradientStop { position: 1; color: "transparent" } }
                }
            }
        }
    }
    Item {
        id: pulseArea; anchors.fill: parent; visible: visual.mode === "pulse"
        Rectangle {
            anchors.centerIn: parent; width: visual.ringRadius * 1.9; height: width; radius: width / 2
            color: "transparent"; border.color: "#ff9328"; border.width: 1; opacity: 0.22 + visual.energy * 0.3
        }
        Rectangle {
            anchors.centerIn: parent; width: visual.ringRadius * 2.2; height: width; radius: width / 2
            color: "transparent"; border.color: "#ff9328"; border.width: 1; opacity: 0.08 + visual.energy * 0.1
        }
        Repeater {
            model: 64
            delegate: Item {
                id: spoke
                required property int index
                readonly property real amplitude: visual.showing && visual.mode === "pulse" ? (visual.bandValues[index < 32 ? index : 63 - index] || 0) : 0
                width: pulseArea.width; height: pulseArea.height; rotation: index * 360 / 64
                Rectangle {
                    x: (spoke.width - width) / 2; y: spoke.height / 2 - visual.ringRadius - height
                    width: Math.max(3, Math.min(6, visual.ringRadius * 0.04)); height: 4 + spoke.amplitude * visual.ringRadius * 0.25; radius: width / 2
                    color: "#ffad32"; opacity: 0.45 + spoke.amplitude * 0.55
                    Behavior on height { NumberAnimation { duration: 80 } }
                }
            }
        }
        Image {
            anchors.centerIn: parent; width: visual.ringRadius * 1.05; height: width
            source: Qt.resolvedUrl("jatayu-media-player.svg"); sourceSize: Qt.size(384, 384)
            scale: 1 + visual.energy * 0.05
            Behavior on scale { NumberAnimation { duration: 120 } }
        }
    }
    Label { textFormat: Text.PlainText;
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 9
        visible: visual.mode !== "artwork"
        text: !visual.host.playing ? qsTr("Paused") : visual.host.visualizer.available ? qsTr("Live audio spectrum") : qsTr("Waiting for audio…")
        color: "#8793a4"; font.pixelSize: 10
    }
}
