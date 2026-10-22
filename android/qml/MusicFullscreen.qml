pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: fullscreen
    required property var host
    property bool controlsVisible: true
    onVisibleChanged: controlsVisible = true
    MusicVisualization { anchors.fill: parent; host: fullscreen.host; radius: 0 }
    MouseArea { anchors.fill: parent; onClicked: { fullscreen.controlsVisible = !fullscreen.controlsVisible; if (fullscreen.controlsVisible) hideControls.restart() } }
    Rectangle {
        anchors.top: parent.top; width: parent.width; height: 88
        visible: fullscreen.controlsVisible
        gradient: Gradient { GradientStop { position: 0; color: "#e6080d14" } GradientStop { position: 1; color: "transparent" } }
        RowLayout {
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 12
            ActionButton { symbol: "back"; quiet: true; Accessible.name: qsTr("Exit visualiser"); onClicked: fullscreen.host.setVisualizerFullscreen(false) }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: fullscreen.host.current.title || qsTr("Music visualiser"); font.pixelSize: 17; elide: Text.ElideRight }
            ComboBox { model: [qsTr("Bars"), qsTr("Pulse"), qsTr("Artwork")]; currentIndex: ["bars", "pulse", "artwork"].indexOf(fullscreen.host.store.preferences.visualMode); onActivated: function(index) { fullscreen.host.store.setPreference("visualMode", ["bars", "pulse", "artwork"][index]) } }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom; width: parent.width; height: 150
        visible: fullscreen.controlsVisible
        gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 0.3; color: "#ce080d14" } GradientStop { position: 1; color: "#f5080d14" } }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 12; spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; text: fullscreen.host.time(fullscreen.host.playbackEngine.position); font.pixelSize: 12 }
                Slider { Layout.fillWidth: true; from: 0; to: Math.max(1, fullscreen.host.playbackEngine.duration); value: fullscreen.host.playbackEngine.position; enabled: fullscreen.host.playbackEngine.seekable; onMoved: { fullscreen.host.seekTo(value); hideControls.restart() } }
                Label { textFormat: Text.PlainText; text: fullscreen.host.time(fullscreen.host.playbackEngine.duration); font.pixelSize: 12 }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 6
                ActionButton { symbol: "previous"; quiet: true; enabled: fullscreen.host.queueIndex > 0; onClicked: fullscreen.host.advance(-1) }
                ActionButton { symbol: fullscreen.host.playing ? "pause" : "play"; primary: true; implicitWidth: 62; implicitHeight: 62; onClicked: { fullscreen.host.togglePlayback(); hideControls.restart() } }
                ActionButton { symbol: "next"; quiet: true; enabled: fullscreen.host.queueIndex + 1 < fullscreen.host.queue.length; onClicked: fullscreen.host.advance(1) }
                ActionButton { symbol: "volume"; quiet: true; Accessible.name: qsTr("Playback volume"); onClicked: fullscreen.host.showVolume() }
                ActionButton { symbol: "settings"; quiet: true; Accessible.name: qsTr("Sound Studio"); onClicked: fullscreen.host.showSoundStudio() }
                ActionButton { symbol: "playlist"; quiet: true; Accessible.name: qsTr("Queue"); onClicked: fullscreen.host.showQueue() }
            }
        }
    }
    Timer { id: hideControls; interval: 4000; running: fullscreen.visible && fullscreen.controlsVisible && fullscreen.host.playing; onTriggered: fullscreen.controlsVisible = false }
}

