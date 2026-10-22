pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: about
    required property var host
    objectName: "aboutPage"
    contentWidth: availableWidth
    clip: true
    ColumnLayout {
        width: about.availableWidth; spacing: 18
        Image {
            objectName: "aboutBrandLogo"
            Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 104; Layout.preferredHeight: 104
            source: Qt.resolvedUrl("jatayu-media-player.svg"); sourceSize: Qt.size(256, 256)
            fillMode: Image.PreserveAspectFit; Accessible.name: qsTr("Jatayu Media Player logo")
        }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Jatayu Media Player"); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 26; font.weight: Font.Bold }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Your media. Your space."); horizontalAlignment: Text.AlignHCenter; color: "#ff9328"; font.pixelSize: 17 }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Android ") + about.host.store.appVersion + qsTr(" • A JatayuOS project"); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: "#b8c2d1" }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Made for your everyday listening, watching and learning. Offline playback, without ads or an account."); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: creator.implicitHeight + 32; radius: 18; color: "#202630"
            ColumnLayout {
                id: creator; anchors.fill: parent; anchors.margins: 16; spacing: 6
                Label { textFormat: Text.PlainText; text: qsTr("CREATED BY"); font.pixelSize: 11; color: "#9ca9bb"; font.letterSpacing: 1.2 }
                Label { textFormat: Text.PlainText; objectName: "creatorName"; Layout.fillWidth: true; text: "Anup Kumar Yadav"; wrapMode: Text.WordWrap; font.pixelSize: 22; font.weight: Font.DemiBold }
                Label { textFormat: Text.PlainText; text: qsTr("Made in India • For everyone"); color: "#b8c2d1"; font.pixelSize: 13 }
            }
        }
        ActionButton { Layout.fillWidth: true; text: "jatayuos.in"; symbol: "export"; primary: true; onClicked: Qt.openUrlExternally("https://jatayuos.in/") }
        Label { textFormat: Text.PlainText; text: qsTr("Connect & support"); font.pixelSize: 20; font.weight: Font.DemiBold }
        ActionButton { Layout.fillWidth: true; text: qsTr("Report a bug / suggest a feature"); symbol: "bookmark"; onClicked: Qt.openUrlExternally("https://github.com/jatayuos4u/jatayu-media-player-android/issues") }
        ActionButton { Layout.fillWidth: true; text: qsTr("Android source code"); symbol: "folder"; onClicked: Qt.openUrlExternally("https://github.com/jatayuos4u/jatayu-media-player-android") }
        ActionButton { Layout.fillWidth: true; text: qsTr("JatayuOS community"); symbol: "library"; onClicked: Qt.openUrlExternally("https://github.com/jatayuos4u/jatayuos/discussions") }
        ActionButton { Layout.fillWidth: true; text: qsTr("Privacy policy"); symbol: "export"; onClicked: Qt.openUrlExternally("https://github.com/jatayuos4u/jatayu-media-player-android/blob/main/docs/PRIVACY-POLICY.md") }
        Label { textFormat: Text.PlainText; text: qsTr("Privacy & permissions"); font.pixelSize: 20; font.weight: Font.DemiBold }
        Label { textFormat: Text.PlainText;
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b8c2d1"
            text: qsTr("Your library, progress, favourites, playlists and study notes stay on this device. No advertising, analytics or account integration is included. This app does not request Internet permission.\n\nMedia access is used to discover your videos and music. You can also select individual files with the Android file picker. Removing an entry from the library never deletes its original file. Exporting notes writes to the location you choose. Music uses a foreground service and wake lock for screen-off playback. Android receives now-playing metadata for media controls; titles may appear on the lock screen. Website and support links open your browser.")
        }
        Label { textFormat: Text.PlainText; text: qsTr("Licences & credits"); font.pixelSize: 20; font.weight: Font.DemiBold }
        Label { textFormat: Text.PlainText;
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b8c2d1"
            text: qsTr("Original application: GPL-3.0-or-later.\n\nBuilt with Qt %1 (Qt Quick, QML, Controls, Multimedia and SVG) and FFmpeg 7.1.5. These projects and the Android/AndroidX components retain their own copyright and licence terms. Thank you to their contributors.\n\nThe bird logo is shared with the Linux edition of Jatayu Media Player.").arg(about.host.store.qtVersion)
        }
        ActionButton { Layout.fillWidth: true; text: qsTr("Application licence"); onClicked: Qt.openUrlExternally("https://github.com/jatayuos4u/jatayu-media-player-android/blob/main/LICENSE") }
        ActionButton { Layout.fillWidth: true; text: qsTr("Third-party notices & source links"); onClicked: Qt.openUrlExternally("https://github.com/jatayuos4u/jatayu-media-player-android/blob/main/docs/ANDROID-THIRD-PARTY-NOTICES.md") }
        Label { textFormat: Text.PlainText; text: qsTr("Compatibility"); font.pixelSize: 20; font.weight: Font.DemiBold }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#9ca9bb"; text: qsTr("Smartphones and tablets • Android 9+ • ARM64\nMusic supports background playback and Android media controls. Automatic transcription is not included.") }
        Item { Layout.preferredHeight: 24 }
    }
}
