pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
RowLayout {
    id: volume
    required property var host
    property string controlKey: ""
    spacing: 8
    ActionButton {
        objectName: volume.controlKey + "MuteButton"
        symbol: volume.host.playerVolume > 0 ? "volume" : "mute"
        quiet: true
        Accessible.name: volume.host.playerVolume > 0 ? qsTr("Mute playback") : qsTr("Restore playback volume")
        onClicked: volume.host.toggleMute()
    }
    Slider {
        objectName: volume.controlKey + "VolumeSlider"
        Layout.fillWidth: true
        from: 0; to: 1; stepSize: 0.01
        value: volume.host.playerVolume
        onMoved: volume.host.playerVolume = value
        Accessible.name: qsTr("Playback volume")
    }
    Label {
        text: Math.round(volume.host.playerVolume * 100) + "%"
        Layout.preferredWidth: 44
        horizontalAlignment: Text.AlignRight
        color: "#ffad58"
    }
}
