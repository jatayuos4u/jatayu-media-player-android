pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: panel
    required property var host
    objectName: "studyTools"
    contentWidth: availableWidth; clip: true
    ColumnLayout {
        width: panel.availableWidth; spacing: 12
        TabBar {
            id: toolsTab; objectName: "studyToolTabs"; Layout.fillWidth: true
            TabButton { text: qsTr("Notes") }
            TabButton { text: qsTr("Practice") }
        }
        Switch {
            visible: panel.host.current.kind === "audio"; text: qsTr("Music visualiser")
            checked: panel.host.store.preferences.studyVisuals
            onToggled: { panel.host.store.setPreference("studyVisuals", checked); if (checked && panel.host.store.preferences.visualMode === "artwork") panel.host.store.setPreference("visualMode", "bars") }
        }
        ColumnLayout {
            visible: toolsTab.currentIndex === 1; Layout.fillWidth: true; spacing: 12
        Label { textFormat: Text.PlainText; text: qsTr("Practice a passage"); font.pixelSize: 19; font.weight: Font.DemiBold }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Mark the start and end, then practise at your own pace."); color: "#aeb9c9"; wrapMode: Text.WordWrap; font.pixelSize: 13 }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { Layout.fillWidth: true; objectName: "setPassageStart"; text: qsTr("Set A"); symbol: "bookmark"; enabled: panel.host.playbackEngine.seekable; onClicked: panel.host.setRepeatStart() }
            ActionButton { Layout.fillWidth: true; objectName: "setPassageEnd"; text: qsTr("Set B"); symbol: "bookmark"; enabled: panel.host.playbackEngine.seekable && panel.host.repeatA >= 0; onClicked: panel.host.setRepeatEnd() }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: rangeLabel.implicitHeight + 24; radius: 12; color: "#202630"
            Label { textFormat: Text.PlainText;
                id: rangeLabel; anchors.fill: parent; anchors.margins: 12; wrapMode: Text.WordWrap; color: "#ffad55"
                text: panel.host.repeatA < 0 ? qsTr("No passage selected") : panel.host.time(panel.host.repeatA) + " → " + (panel.host.repeatB < 0 ? qsTr("Set an end") : panel.host.time(panel.host.repeatB)) + (panel.host.repeatActive ? qsTr("\nRepeating • completed ") + panel.host.repeatCompleted + (panel.host.repeatLimit ? " / " + panel.host.repeatLimit : "") : "")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { Layout.fillWidth: true; text: qsTr("Adjust passage"); enabled: panel.host.playbackEngine.seekable; onClicked: panel.host.showPassageEditor() }
            ActionButton { symbol: "close"; Accessible.name: qsTr("Clear passage"); enabled: panel.host.repeatA >= 0; onClicked: panel.host.clearRepeat() }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Repeat count") }
            ComboBox { Layout.preferredWidth: 150; model: [qsTr("Continuous"), "3", "5", "10"]; currentIndex: [0, 3, 5, 10].indexOf(panel.host.repeatLimit); onActivated: function(index) { panel.host.repeatLimit = [0, 3, 5, 10][index] } }
        }
        ActionButton {
            Layout.fillWidth: true; primary: !panel.host.repeatActive; symbol: "repeat"
            text: panel.host.repeatActive ? qsTr("Stop repeating") : qsTr("Start practice")
            enabled: panel.host.playbackEngine.seekable && panel.host.repeatB > panel.host.repeatA && panel.host.repeatA >= 0
            onClicked: { if (panel.host.repeatActive) panel.host.repeatActive = false; else panel.host.beginRepeat(panel.host.repeatA, panel.host.repeatB) }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Study skip") }
            ComboBox { model: ["2 s", "5 s", "10 s", "15 s"]; currentIndex: [2, 5, 10, 15].indexOf(panel.host.store.preferences.studySkip); onActivated: function(index) { panel.host.store.setPreference("studySkip", [2, 5, 10, 15][index]) } }
        }
        ActionButton { Layout.fillWidth: true; text: qsTr("Subtitles & timing"); symbol: "subtitles"; onClicked: panel.host.showTracks() }
        }
        ColumnLayout {
            visible: toolsTab.currentIndex === 0; Layout.fillWidth: true; spacing: 12
        Label { textFormat: Text.PlainText; text: qsTr("Your study notes"); font.pixelSize: 19; font.weight: Font.DemiBold; Layout.topMargin: 6 }
        ActionButton { objectName: "addStudyNote"; Layout.fillWidth: true; text: qsTr("Add note at ") + panel.host.time(panel.host.playbackEngine.position); symbol: "add"; primary: true; enabled: panel.host.hasMedia; onClicked: panel.host.editStudyNote(null) }
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: !panel.host.marked.length; text: qsTr("Save a key idea, a question or a word to practise. Tap a timestamp to return to it."); color: "#9ca9bb"; wrapMode: Text.WordWrap }
        Repeater {
            model: panel.host.marked
            delegate: Rectangle {
                id: noteCard
                required property var modelData
                Layout.fillWidth: true; implicitHeight: noteContent.implicitHeight + 24; color: "#202630"; radius: 14
                ColumnLayout {
                    id: noteContent; anchors.fill: parent; anchors.margins: 12; spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        ActionButton { text: panel.host.time(noteCard.modelData.position); symbol: "play"; quiet: true; onClicked: panel.host.seekTo(noteCard.modelData.position) }
                        Item { Layout.fillWidth: true }
                        ActionButton { text: qsTr("Edit"); quiet: true; onClicked: panel.host.editStudyNote(noteCard.modelData) }
                        ActionButton { symbol: "close"; quiet: true; Accessible.name: qsTr("Delete note"); onClicked: panel.host.store.removeBookmark(panel.host.playbackEngine.source, noteCard.modelData.id) }
                    }
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: noteCard.modelData.note || qsTr("Bookmark");  wrapMode: Text.WordWrap; color: "#e7eaf0" }
                }
            }
        }
        ActionButton { Layout.fillWidth: true; text: qsTr("Export timestamped notes"); symbol: "export"; enabled: panel.host.marked.length > 0; onClicked: panel.host.exportStudyNotes() }
        }
        Item { Layout.preferredHeight: 16 }
    }
}

