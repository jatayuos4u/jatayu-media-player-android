pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: panel
    objectName: "soundStudioPanel"
    required property var host
    readonly property var studio: host.soundStudio
    readonly property var presetIds: studio.profiles.map(function(p) { return p.id })
    function syncPresetSelection() { presetBox.currentIndex = presetIds.indexOf(studio.preset) }
    Connections {
        target: panel.studio
        function onSettingsChanged() { Qt.callLater(panel.syncPresetSelection) }
    }
    Dialog {
        id: saveDialog
        objectName: "saveSoundProfileDialog"
        title: qsTr("Save personal profile")
        modal: true
        anchors.centerIn: parent
        width: Math.min(panel.width - 24, 400)
        implicitHeight: 240
        Material.theme: Material.Dark
        Material.accent: "#ff9328"
        background: Rectangle { color: "#1c2430"; radius: 18; border.color: "#343d4a" }
        Overlay.modal: Rectangle { color: "#99080d14" }
        standardButtons: Dialog.NoButton
        onAboutToShow: profileName.text = ""
        onOpened: profileName.forceActiveFocus()
        footer: DialogButtonBox {
            Button {
                objectName: "saveSoundProfileButton"
                text: qsTr("Save")
                enabled: profileName.text.trim().length > 0 && panel.studio.profiles.length < 40
                onClicked: { if (panel.studio.saveProfile(profileName.text)) saveDialog.close() }
            }
            Button { text: qsTr("Cancel"); onClicked: saveDialog.close() }
        }
        contentItem: ColumnLayout {
            TextField { id: profileName; objectName: "soundProfileName"; Layout.fillWidth: true; placeholderText: qsTr("e.g. My evening headphones"); maximumLength: 40; Accessible.name: qsTr("Personal profile name") }
            Label { Layout.fillWidth: true; text: qsTr("Save up to 32 named personal profiles."); wrapMode: Text.WordWrap; color: "#aeb7c7" }
        }
    }
    Dialog {
        id: deleteDialog
        title: qsTr("Delete saved profile?")
        modal: true
        anchors.centerIn: parent
        width: Math.min(panel.width - 24, 400)
        implicitHeight: 240
        Material.theme: Material.Dark
        Material.accent: "#ff9328"
        background: Rectangle { color: "#1c2430"; radius: 18; border.color: "#343d4a" }
        Overlay.modal: Rectangle { color: "#99080d14" }
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: panel.studio.removeProfile(panel.studio.basePreset)
        contentItem: Label { text: qsTr("The current sound stays active. Only the saved profile is removed."); wrapMode: Text.WordWrap }
    }
    title: qsTr("Jatayu Sound Studio")
    modal: true
    width: Math.min(parent.width - 24, 720)
    height: Math.min(parent.height - 28, 760)
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    Material.theme: Material.Dark
    Material.accent: "#ff9328"
    background: Rectangle { color: "#171c24"; radius: 24; border.color: "#343d4a" }
    Overlay.modal: Rectangle { color: "#b3080d14" }
    contentItem: ScrollView {
        objectName: "studioScroll"
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Switch { objectName: "studioEnable"; text: qsTr("Enable Studio"); checked: panel.studio.enabled; onToggled: panel.studio.enabled = checked }
                Item { Layout.fillWidth: true }
                Button { text: !panel.studio.routing || panel.studio.original ? qsTr("Original") : qsTr("Enhanced"); enabled: panel.studio.routing; onClicked: panel.studio.original = !panel.studio.original; Accessible.name: qsTr("Compare original and enhanced sound") }
            }
            VolumeControl { host: panel.host; controlKey: "studio"; Layout.fillWidth: true }
            Label { textFormat: Text.PlainText; text: panel.studio.status; color: "#bec7d5"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Button { text: qsTr("Retry Studio"); visible: panel.studio.enabled && panel.studio.music && !panel.studio.routing; onClicked: panel.studio.retry() }
            Label { text: qsTr("Your sound"); font.pixelSize: 18; font.weight: Font.DemiBold }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: presetBox
                    objectName: "studioPresets"
                    Layout.fillWidth: true
                    model: panel.studio.profiles
                    textRole: "name"
                    currentIndex: panel.presetIds.indexOf(panel.studio.preset)
                    displayText: {
                        const i = panel.presetIds.indexOf(panel.studio.preset)
                        return i < 0 ? qsTr("Custom") : panel.studio.profiles[i].name
                    }
                    Component.onCompleted: panel.syncPresetSelection()
                    onActivated: function(index) { panel.studio.applyPreset(panel.presetIds[index]) }
                }
                Button { text: qsTr("Save"); onClicked: saveDialog.open(); Accessible.name: qsTr("Save this personal sound") }

            }
            Label { Layout.fillWidth: true; text: panel.studio.description; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#bec7d5" }
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Preset strength"); Layout.fillWidth: true }
                Label { text: Math.round(panel.studio.strength) + "%"; color: "#ffad58" }
            }
            Slider { objectName: "studioStrength"; Layout.fillWidth: true; from: 0; to: 100; stepSize: 1; value: panel.studio.strength; onMoved: panel.studio.strength = value; Accessible.name: qsTr("Preset strength") }
            Label { Layout.fillWidth: true; text: qsTr("Profile controls at 100% strength"); color: "#aeb7c7"; font.pixelSize: 12 }
            Repeater {
                model: [{key: "bass", title: qsTr("Bass")}, {key: "vocals", title: qsTr("Vocals")}, {key: "treble", title: qsTr("Treble")}]
                delegate: ColumnLayout {
                    id: toneRow
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: toneRow.modelData.title; Layout.fillWidth: true }
                        Label { text: Number(panel.studio[toneRow.modelData.key]).toFixed(1) + " dB"; color: "#ffad58" }
                    }
                    Slider { Layout.fillWidth: true; from: -6; to: 6; stepSize: 0.1; value: panel.studio[toneRow.modelData.key]; onMoved: panel.studio[toneRow.modelData.key] = value; Accessible.name: toneRow.modelData.title }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 144
                color: "#10151d"; radius: 12
                Canvas {
                    id: curve
                    anchors.fill: parent; anchors.margins: 12
                    readonly property var response: panel.studio.response
                    onResponseChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        const limit = Math.max(12, Math.ceil(Math.max.apply(null, response.map(function(db) { return Math.abs(db) })) / 6) * 6)
                        const top = 18, bottom = height - 24, middle = (top + bottom) / 2, scale = (bottom - top) / (limit * 2)
                        ctx.font = "11px sans-serif"
                        ctx.fillStyle = "#aeb7c7"
                        ctx.fillText(qsTr("EQ response • before headroom"), 0, 11)
                        for (let db = -limit; db <= limit; db += limit / 2) {
                            const y = middle - db * scale
                            ctx.strokeStyle = db === 0 ? "#677284" : "#293240"
                            ctx.beginPath(); ctx.moveTo(28, y); ctx.lineTo(width, y); ctx.stroke()
                            ctx.fillText(String(db), 0, y + 4)
                        }
                        ctx.fillText("20 Hz", 28, height - 3)
                        ctx.fillText("1 kHz", 28 + (width - 28) * Math.log(50) / Math.log(panel.studio.responseMaxFrequency / 20) - 14, height - 3)
                        ctx.fillText((panel.studio.responseMaxFrequency / 1000).toFixed(1) + " kHz", width - 48, height - 3)
                        ctx.save(); ctx.beginPath(); ctx.rect(28, top, width - 28, bottom - top); ctx.clip()
                        ctx.strokeStyle = "#ff9328"; ctx.lineWidth = 2; ctx.beginPath()
                        for (let i = 0; i < response.length; ++i) {
                            const x = 28 + (width - 28) * i / (response.length - 1)
                            const y = middle - response[i] * scale
                            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                        }
                        ctx.stroke(); ctx.restore()
                    }
                    Accessible.role: Accessible.Graphic
                    Accessible.name: qsTr("Combined equalizer response at current preset strength, before automatic headroom")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: qsTr("Reset profile"); onClicked: panel.studio.resetPreset() }
                Button { text: qsTr("Flat"); onClicked: panel.studio.applyPreset("Flat") }
                Button { text: qsTr("Delete saved"); visible: panel.studio.basePreset.indexOf("personal:") === 0; onClicked: deleteDialog.open() }
            }
            CheckBox { objectName: "studioLevelMatch"; text: qsTr("Level-match Original comparison"); checked: panel.studio.levelMatch; onToggled: panel.studio.levelMatch = checked }
            Label { Layout.fillWidth: true; text: qsTr("Matches the reference using recent signal levels. Allow a few seconds to settle. Turn off for unmodified Original volume."); wrapMode: Text.WordWrap; font.pixelSize: 12; color: "#aeb7c7" }
            CheckBox { id: advanced; objectName: "studioAdvanced"; text: qsTr("10-band equalizer"); checked: false }
            GridLayout {
                visible: advanced.checked
                Layout.fillWidth: true
                columns: panel.width >= 620 ? 10 : 5
                rowSpacing: 12; columnSpacing: 2
                Repeater {
                    model: ["31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
                    delegate: ColumnLayout {
                        id: eqBand
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 2
                        Label { Layout.alignment: Qt.AlignHCenter; text: Number(panel.studio.bands[eqBand.index]).toFixed(1); color: "#ffad58"; font.pixelSize: 11 }
                        Slider { Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 48; Layout.preferredHeight: 126; orientation: Qt.Vertical; from: -9; to: 9; stepSize: 0.5; value: panel.studio.bands[eqBand.index]; onMoved: panel.studio.setBand(eqBand.index, value); Accessible.name: eqBand.modelData + " Hz gain" }
                        Label { Layout.alignment: Qt.AlignHCenter; text: eqBand.modelData; font.pixelSize: 12 }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: limiterText.implicitHeight + 24; color: "#222b37"; radius: 14
                Label { id: limiterText; anchors.fill: parent; anchors.margins: 12; text: qsTr("Auto headroom") + " −" + panel.studio.headroom.toFixed(1) + " dB  •  " + (panel.studio.reduction > 0.1 ? panel.studio.reduction.toFixed(1) + qsTr(" dB reduction") : qsTr("Limiter ready")); color: "#bed9d0"; wrapMode: Text.WordWrap }
            }
            Label { Layout.fillWidth: true; text: qsTr("Effects apply to music at 1.0× speed. Presets are starting points—adjust and save your own. Vocals changes voice frequencies; it does not separate a singer from the music."); wrapMode: Text.WordWrap; color: "#aeb7c7"; font.pixelSize: 12 }
        }
    }
}
