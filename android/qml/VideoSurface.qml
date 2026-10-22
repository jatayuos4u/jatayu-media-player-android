pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: surface
    objectName: "videoSurface"
    required property var player
    required property var host
    property alias output: videoOutput
    property bool controlsVisible: true
    property bool locked: false
    property bool cropToFill: false
    property string gestureHint: ""
    property real seekPreview: -1
    readonly property bool fullControls: host.page === "player" && !host.pipActive
    function refreshHideTimer() {
        if (fullControls && controlsVisible && !locked && host.playing && host.store.preferences.autoHide && !seekBar.pressed) hideControls.restart()
        else hideControls.stop()
    }
    color: "#050709"
    clip: true
    onFullControlsChanged: { controlsVisible = true; locked = false }
    VideoOutput { id: videoOutput; anchors.fill: parent; fillMode: surface.cropToFill ? VideoOutput.PreserveAspectCrop : VideoOutput.PreserveAspectFit }
    Image {
        anchors.fill: parent; visible: !surface.player.hasVideo && !(surface.host.current.kind === "audio" && surface.host.store.preferences.studyVisuals && surface.host.page === "study")
        source: surface.host.hasMedia ? "image://artwork/" + encodeURIComponent(surface.player.source.toString()) : ""
        asynchronous: true; fillMode: Image.PreserveAspectFit
    }
    MusicVisualization {
        anchors.fill: parent; host: surface.host
        visible: !surface.player.hasVideo && surface.host.current.kind === "audio" && surface.host.store.preferences.studyVisuals && surface.host.page === "study"
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
        anchors.bottomMargin: surface.fullControls && surface.controlsVisible ? Math.min(surface.height * 0.4, 220) : 18
        width: Math.min(surface.width - 24, subtitleText.implicitWidth + 28)
        height: subtitleText.implicitHeight + 16; radius: 8; color: "#b8000000"
        visible: surface.host.externalCaption.length > 0
        Label { textFormat: Text.PlainText;
            id: subtitleText; anchors.centerIn: parent; width: Math.min(implicitWidth, surface.width - 52)
            text: surface.host.externalCaption;  wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter; color: "white"; font.pixelSize: surface.host.store.preferences.subtitleSize
        }
    }
    MouseArea {
        id: gestures
        objectName: "videoGestureArea"
        anchors.fill: parent
        enabled: !surface.host.pipActive
        property real startX: 0
        property real startY: 0
        property real startVolume: 1
        property real startBrightness: 0.6
        property real startPosition: 0
        property int mode: 0
        onPressed: function(mouse) {
            startX = mouse.x; startY = mouse.y; startVolume = surface.host.playerVolume
            startBrightness = surface.host.screenBrightness < 0 ? 0.6 : surface.host.screenBrightness
            startPosition = surface.player.position; mode = 0
        }
        onPositionChanged: function(mouse) {
            if (!pressed || surface.locked || !surface.host.store.preferences.gestures) return
            const dx = mouse.x - startX, dy = startY - mouse.y
            if (!mode && Math.max(Math.abs(dx), Math.abs(dy)) > 16) mode = Math.abs(dx) > Math.abs(dy) ? 1 : startX < width / 2 ? 2 : 3
            if (mode === 1 && surface.player.seekable) {
                surface.seekPreview = Math.max(0, Math.min(surface.player.duration, startPosition + dx / Math.max(1, width) * 120000))
                surface.gestureHint = surface.host.time(surface.seekPreview)
            } else if (mode === 2) {
                surface.host.screenBrightness = Math.max(0.05, Math.min(1, startBrightness + dy / Math.max(1, height)))
                surface.gestureHint = qsTr("Brightness ") + Math.round(surface.host.screenBrightness * 100) + "%"
            } else if (mode === 3) {
                surface.host.playerVolume = Math.max(0, Math.min(1, startVolume + dy / Math.max(1, height)))
                surface.gestureHint = qsTr("Volume ") + Math.round(surface.host.playerVolume * 100) + "%"
            }
        }
        onReleased: {
            if (mode === 1 && surface.seekPreview >= 0) surface.host.seekTo(surface.seekPreview)
            surface.seekPreview = -1; hintTimer.restart()
        }
        onCanceled: { surface.seekPreview = -1; surface.gestureHint = "" }
        onClicked: {
            if (!mode && !surface.locked) surface.controlsVisible = !surface.controlsVisible
            surface.refreshHideTimer()
        }
        onDoubleClicked: function(mouse) {
            if (!surface.locked) {
                const step = surface.host.store.preferences.seekStep * 1000
                surface.host.seekBy(mouse.x < width / 2 ? -step : step)
                surface.gestureHint = (mouse.x < width / 2 ? "−" : "+") + surface.host.store.preferences.seekStep + qsTr(" seconds")
                hintTimer.restart()
            }
        }
    }
    Rectangle {
        anchors.top: parent.top; width: parent.width; height: 110
        visible: surface.fullControls && surface.controlsVisible && !surface.locked
        gradient: Gradient { GradientStop { position: 0; color: "#dd070a0e" } GradientStop { position: 1; color: "transparent" } }
        RowLayout {
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            anchors.margins: 12; spacing: 6
            ActionButton { symbol: "back"; quiet: true; Accessible.name: qsTr("Back to library"); onClicked: { surface.host.saveProgress(); surface.host.page = "library"; surface.host.setFullscreen(false) } }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: surface.host.current.title || qsTr("Video"); color: "#f5f6fa"; elide: Text.ElideRight; font.pixelSize: 17; font.weight: Font.DemiBold }
            ActionButton { objectName: "lockControlsButton"; symbol: "lock"; quiet: true; Accessible.name: qsTr("Lock playback controls"); onClicked: { surface.locked = true; surface.controlsVisible = false } }
            ActionButton { symbol: "more"; quiet: true; Accessible.name: qsTr("Playback options"); onClicked: surface.host.showPlaybackOptions() }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom; width: parent.width
        height: Math.min(parent.height, transport.implicitHeight + 48)
        visible: surface.fullControls && surface.controlsVisible && !surface.locked
        gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 0.35; color: "#b0090c11" } GradientStop { position: 1; color: "#f0090c11" } }
        ColumnLayout {
            id: transport
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 12
            spacing: surface.height < 400 ? 0 : 8
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: surface.host.time(surface.player.position) + " / " + surface.host.time(surface.player.duration); color: "#e7eaf0"; font.pixelSize: 12 }
                ActionButton { text: surface.player.playbackRate.toFixed(2).replace(/0$/, "") + "×"; quiet: true; Accessible.name: qsTr("Playback speed"); onClicked: surface.host.showSpeed() }
                ActionButton { symbol: "subtitles"; quiet: true; Accessible.name: qsTr("Audio and subtitles"); onClicked: surface.host.showTracks() }
                ActionButton { symbol: "expand"; quiet: true; Accessible.name: qsTr("Fullscreen"); objectName: "fullscreenButton"; onClicked: surface.host.setFullscreen(!surface.host.immersivePlayback) }
            }
            Slider {
                id: seekBar
                Layout.fillWidth: true; from: 0; to: Math.max(1, surface.player.duration); value: surface.player.position
                enabled: surface.player.seekable; Accessible.name: qsTr("Video playback position")
                onMoved: { surface.host.seekTo(value); surface.refreshHideTimer() }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: surface.width < 400 ? 6 : 18
                ActionButton { text: "−" + surface.host.store.preferences.seekStep; quiet: true; Accessible.name: qsTr("Seek backward"); onClicked: { surface.host.seekBy(-surface.host.store.preferences.seekStep * 1000); surface.refreshHideTimer() } }
                ActionButton { symbol: "previous"; quiet: true; enabled: surface.host.queueIndex > 0; onClicked: surface.host.advance(-1) }
                ActionButton { symbol: surface.host.playing ? "pause" : "play"; primary: true; implicitWidth: 62; implicitHeight: 62; onClicked: { surface.host.togglePlayback(); surface.refreshHideTimer() } }
                ActionButton { symbol: "next"; quiet: true; enabled: surface.host.queueIndex + 1 < surface.host.queue.length; onClicked: surface.host.advance(1) }
                ActionButton { text: "+" + surface.host.store.preferences.seekStep; quiet: true; Accessible.name: qsTr("Seek forward"); onClicked: { surface.host.seekBy(surface.host.store.preferences.seekStep * 1000); surface.refreshHideTimer() } }
            }
            RowLayout {
                Layout.fillWidth: true
                ActionButton { Layout.fillWidth: true; text: qsTr("Study"); symbol: "bookmark"; quiet: true; onClicked: surface.host.page = "study" }
                ActionButton { Layout.fillWidth: true; text: qsTr("Rotate"); symbol: "rotate"; objectName: "rotateVideoButton"; quiet: true; onClicked: surface.host.rotateVideo() }
                ActionButton { Layout.fillWidth: true; text: surface.cropToFill ? qsTr("Fill") : qsTr("Fit"); symbol: "expand"; objectName: "videoFitButton"; quiet: true; Accessible.name: qsTr("Switch between fitting the whole video and filling the screen by cropping edges"); onClicked: surface.cropToFill = !surface.cropToFill }
                ActionButton { Layout.fillWidth: true; text: qsTr("Queue"); symbol: "playlist"; quiet: true; onClicked: surface.host.showQueue() }
            }
        }
    }
    ActionButton {
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 18
        objectName: "unlockControlsButton"; visible: surface.locked && !surface.host.pipActive; symbol: "unlock"; text: qsTr("Unlock"); primary: true
        onClicked: { surface.locked = false; surface.controlsVisible = true; surface.refreshHideTimer() }
    }
    Rectangle {
        anchors.centerIn: parent; visible: !surface.host.pipActive && surface.gestureHint.length > 0
        width: hint.implicitWidth + 36; height: 62; radius: 16; color: "#df202733"
        Label { textFormat: Text.PlainText; id: hint; anchors.centerIn: parent; text: surface.gestureHint; font.pixelSize: 20; font.weight: Font.DemiBold }
    }
    Timer { id: hintTimer; interval: 900; onTriggered: surface.gestureHint = "" }
    Timer {
        id: hideControls; interval: surface.host.store.preferences.hideDelay * 1000
        running: surface.fullControls && surface.controlsVisible && !surface.locked && surface.host.playing && surface.host.store.preferences.autoHide && !seekBar.pressed
        onTriggered: { if (surface.host.playing && surface.host.store.preferences.autoHide && !surface.locked) surface.controlsVisible = false }
    }
    Connections { target: surface.player; function onSourceChanged() { surface.controlsVisible = true; surface.locked = false } }
}

