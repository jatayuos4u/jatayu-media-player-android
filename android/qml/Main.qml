pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: window
    required property var playbackSession
    required property var store
    required property var languages
    required property var decks
    readonly property var player: decks.player
    readonly property var mediaSource: decks
    readonly property var soundStudio: decks.studio
    readonly property var visualizer: decks.visualizer
    property bool componentReady: false
    onPlayerChanged: { if (componentReady) playbackSession.attach(player, decks.fading) }
    width: 420; height: 860
    minimumWidth: pipActive ? 1 : 320; minimumHeight: pipActive ? 1 : 300
    visible: true
    title: qsTr("Jatayu Media Player")
    color: "#12151b"
    Material.theme: Material.Dark
    Material.accent: "#ff9328"
    Material.background: "#12151b"
    Material.foreground: "#f5f6fa"

    readonly property bool pipActive: playbackSession.pictureInPicture
    readonly property bool wide: width >= 900
    readonly property bool landscape: width > height
    property string page: "library"
    property string tab: "Continue"
    property string query: ""
    property string browseSelection: ""
    property string browseTitle: ""
    property string musicBrowse: "Songs"
    readonly property bool rotationAvailable: { width; height; return store.supportsRotationLock() }
    property bool rotationLocked: false
    property int playbackOrientation: 0
    property bool immersivePlayback: false
    property real screenBrightness: -1
    property real playerVolume: store.preferences.volume
    onPlayerVolumeChanged: { decks.setVolume(playerVolume); if (playerVolume > 0) lastAudibleVolume = playerVolume; store.setPreference("volume", playerVolume) }
    property real lastAudibleVolume: store.preferences.volume > 0 ? store.preferences.volume : 0.5
    function toggleMute() { playerVolume = playerVolume > 0 ? 0 : lastAudibleVolume }
    function showVolume() { volumeDialog.open() }
    function playMedia() { playbackSession.play() }
    function pauseMedia() { playbackSession.pause() }
    function syncMediaSession() {
        playbackSession.metadata(current.title || qsTr("Selected media"), current.artist || qsTr("Local music"),
            current.kind === "audio", queueIndex > 0 || repeatQueue, queueIndex + 1 < queue.length || repeatQueue || shuffle)
    }
    onCurrentChanged: syncMediaSession()
    property bool pipSavedFullscreen: false
    property bool pipSavedRotationLocked: false
    property int pipSavedOrientation: 0
    onPipActiveChanged: {
        if (pipActive) {
            pipSavedFullscreen = immersivePlayback
            pipSavedRotationLocked = rotationLocked
            pipSavedOrientation = playbackOrientation
            page = "player"; soundStudioPanel.close()
            // Android resizes its activity for PiP. Calling showNormal here
            // stores the small geometry as a normal Qt top-level window.
            if (Qt.platform.os !== "android") visibility = Window.Windowed
        } else {
            immersivePlayback = pipSavedFullscreen
            rotationLocked = pipSavedRotationLocked
            playbackOrientation = pipSavedOrientation
            visibility = pipSavedFullscreen ? Window.FullScreen : Qt.platform.os === "android" ? Window.Maximized : Window.Windowed
        }
        syncPlayerWindow()
    }
    onQueueIndexChanged: syncMediaSession()
    onQueueChanged: { crossfadeSource = ""; syncMediaSession() }
    onRepeatQueueChanged: { crossfadeSource = ""; syncMediaSession() }
    onShuffleChanged: { crossfadeSource = ""; syncMediaSession() }
    readonly property var playbackEngine: player
    readonly property bool showGroups: !browseSelection.length && (tab === "Folders" || (tab === "Music" && musicBrowse !== "Songs"))
    readonly property var continueItems: window.store.media.filter(function(item) {
        return item.kind === "video" && item.position > 0 && item.duration > item.position + 2000
    }).sort(function(a, b) { return b.lastPlayed - a.lastPlayed })
    readonly property var browseGroups: {
        const entries = window.store.media
        const grouped = ({})
        entries.forEach(function(item) {
            if (window.tab === "Music" && item.kind !== "audio") return
            const key = window.groupKey(item)
            const title = window.tab === "Folders" ? (item.folder || qsTr("Selected files")).replace(/\/+$/, "").split("/").pop() : key
            if (!grouped[key]) grouped[key] = { key: key, title: title || qsTr("Device media"), count: 0, url: item.url, detail: window.tab === "Folders" ? (item.volumeName ? (item.volumeName === "external_primary" ? qsTr("Phone") : qsTr("Storage")) + " • " : "") + (item.folder || "") : "" }
            grouped[key].count++
        })
        return Object.keys(grouped).map(function(key) { return grouped[key] }).filter(function(group) { return group.title.toLocaleLowerCase().indexOf(window.query.toLocaleLowerCase()) >= 0 }).sort(function(a, b) { return a.title.localeCompare(b.title) })
    }
    property bool searching: false
    property var queue: []
    property int queueIndex: -1
    property var current: ({})
    property var marked: []
    property var selectedEntry: ({})
    property string selectedPlaylist: ""
    property string selectedPlaylistName: ""
    property int playlistRevision: 0
    property real pendingResume: 0
    property real lastStablePosition: 0
    property string lastStableSource: ""
    property real repeatA: -1
    property real repeatB: -1
    property bool repeatActive: false
    property bool repeatSeeking: false
    property int repeatLimit: 0
    property int repeatCompleted: 0
    property var externalCues: []
    property bool externalEnabled: false
    property string externalSubtitleName: ""
    property int subtitleDelay: 0
    readonly property string externalCaption: externalEnabled ? captionAt(player.position - subtitleDelay) : ""
    property bool shuffle: false
    property bool repeatQueue: false
    property int sleepMinutes: 0
    property string notice: ""
    readonly property bool hasMedia: player.source.toString().length > 0
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property bool playerPage: page === "player" || page === "study" || page === "visualizer"
    readonly property var visibleMedia: {
        let list = window.store.media.slice()
        if (tab === "Videos") list = list.filter(function(item) { return item.kind === "video" })
        if (tab === "Music") list = list.filter(function(item) { return item.kind === "audio" })
        if (tab === "Collections") list = list.filter(function(item) { return item.favorite })
        if (browseSelection.length) list = list.filter(function(item) { return window.groupKey(item) === window.browseSelection })
        const needle = query.toLocaleLowerCase()
        list = list.filter(function(item) { return item.title.toLocaleLowerCase().indexOf(needle) >= 0 })
        const order = tab === "Continue" ? "recent" : window.store.preferences.sort
        return list.sort(function(a, b) {
            if (order === "name") return a.title.localeCompare(b.title)
            if (order === "duration") return (b.duration || 0) - (a.duration || 0)
            if (order === "size") return (b.size || 0) - (a.size || 0)
            return (b.dateAdded || 0) - (a.dateAdded || 0)
        })
    }
    readonly property var collectionMedia: {
        playlistRevision
        window.store.media
        window.store.playlists
        return window.store.playlistItems(selectedPlaylist)
    }

    Component.onCompleted: { componentReady = true; decks.setVideoOutput(videoPane.output); decks.setVolume(playerVolume); decks.setCrossfade(store.preferences.crossfade); playbackSession.attach(player); playbackSession.setPiPEnabled(store.preferences.pipAutomatic); syncMediaSession(); if (Qt.platform.os === "android" && store.preferences.scanOnOpen) store.scanDevice(false) }
    function groupKey(item) {
        if (tab === "Music" && musicBrowse === "Albums") return item.album || qsTr("Unknown album")
        if (tab === "Music" && musicBrowse === "Artists") return item.artist || qsTr("Unknown artist")
        return item.folderKey || item.folder || qsTr("Selected files")
    }
    Binding {
        target: window.visualizer; property: "enabled"
        value: window.active && window.current.kind === "audio" && window.store.preferences.visualMode !== "artwork"
            && (window.page === "music" || window.page === "visualizer" || (window.page === "study" && window.store.preferences.studyVisuals) || soundStudioPanel.opened)
    }
    function setVisualizerFullscreen(enabled) {
        if (enabled) { page = "visualizer"; setFullscreen(true) }
        else { setFullscreen(false); page = "music" }
    }
    function showTracks() { tracksDialog.open() }
    function showSpeed() { speedDialog.open() }
    function showQueue() { queueDialog.open() }
    function showPlaybackOptions() { playbackOptions.open() }
    function exportStudyNotes() { exportPicker.notesSource = player.source; exportPicker.open() }
    function editStudyNote(note) {
        bookmarkDialog.noteSource = player.source
        bookmarkDialog.bookmarkPosition = note ? note.position : player.position
        bookmarkDialog.editingId = note ? (note.id || "") : ""
        bookmarkNote.text = note ? note.note : ""
        bookmarkDialog.open()
    }
    function showPassageEditor() {
        passageStart.value = Math.round((repeatA >= 0 ? repeatA : player.position) / 100)
        passageEnd.value = Math.round((repeatB >= 0 ? repeatB : Math.min(player.duration, passageStart.value * 100 + 10000)) / 100)
        passageDialog.open()
    }
    function clearRepeat() { repeatActive = false; repeatSeeking = false; repeatA = -1; repeatB = -1; repeatCompleted = 0 }
    function setRepeatStart() { clearRepeat(); repeatA = player.position }
    function setRepeatEnd() { beginRepeat(repeatA, player.position) }
    function beginRepeat(start, end) {
        if (!player.seekable || start < 0 || end < start + 500 || end > player.duration) {
            notice = qsTr("Choose an end at least half a second after the start."); return false
        }
        repeatA = start; repeatB = end; repeatCompleted = 0; repeatSeeking = true; repeatActive = true
        window.seekTo(start); window.playMedia(); return true
    }
    function handleRepeatBoundary(position) {
        if (!repeatActive) return
        if (repeatSeeking) { if (position < repeatB - 100) repeatSeeking = false; return }
        if (position < repeatB) return
        repeatCompleted++
        if (repeatLimit && repeatCompleted >= repeatLimit) {
            repeatActive = false; window.pauseMedia(); notice = qsTr("Practice complete • ") + repeatCompleted + qsTr(" repeats")
        } else { repeatSeeking = true; window.seekTo(repeatA) }
    }
    function captionAt(position) {
        let lo = 0, hi = externalCues.length
        while (lo < hi) { const mid = Math.floor((lo + hi) / 2); if (externalCues[mid].start <= position) lo = mid + 1; else hi = mid }
        const lines = []
        for (let i = lo - 1; i >= 0; --i) {
            const cue = externalCues[i]
            if (cue.maxEnd !== undefined && cue.maxEnd <= position) break
            if (position < cue.end) lines.unshift(cue.text)
        }
        return lines.join("\n")
    }
    function syncPlayerWindow() {
        store.configurePlayerWindow(active && playerPage && !pipActive ? screenBrightness : -1,
            active && !pipActive && playing && player.hasVideo && playerPage && store.preferences.keepAwake,
            playerPage && !pipActive && rotationAvailable ? (rotationLocked ? 3 : (playbackOrientation || 4)) : 0, active && playerPage && !pipActive && (immersivePlayback || (page === "player" && landscape)))
    }
    function setFullscreen(enabled) {
        immersivePlayback = enabled
        rotationLocked = false
        playbackOrientation = enabled ? 1 : 0
        visibility = enabled ? Window.FullScreen : Qt.platform.os === "android" ? Window.Maximized : Window.Windowed
        syncPlayerWindow()
    }
    function rotateVideo() {
        if (!rotationAvailable) { notice = qsTr("Android manages orientation on this tablet. Rotate the device to change the view."); return }
        rotationLocked = false
        playbackOrientation = landscape ? 2 : 1
        syncPlayerWindow()
    }
    onLandscapeChanged: syncPlayerWindow()
    onScreenBrightnessChanged: syncPlayerWindow()
    onRotationLockedChanged: syncPlayerWindow()
    onPlaybackOrientationChanged: syncPlayerWindow()
    onImmersivePlaybackChanged: syncPlayerWindow()
    onPlayerPageChanged: { if (!playerPage) setFullscreen(false); else syncPlayerWindow() }
    function time(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        return (seconds >= 3600 ? Math.floor(seconds / 3600) + ":" : "") +
               (seconds >= 3600 ? ("0" + Math.floor(seconds / 60) % 60).slice(-2) : Math.floor(seconds / 60)) + ":" + ("0" + seconds % 60).slice(-2)
    }
    function saveProgress() {
        if (!hasMedia || player.duration <= 0) return
        const source = player.source.toString()
        // Teardown may report position zero before the checkpoint is saved.
        const position = player.position > 0 ? player.position
            : (lastStableSource === source ? lastStablePosition : 0)
        if (position > 0 || player.mediaStatus === MediaPlayer.EndOfMedia)
            window.store.updateProgress(player.source,
                player.mediaStatus === MediaPlayer.EndOfMedia ? player.duration : position,
                player.duration)
    }
    function restorePendingPosition() {
        if (pendingResume <= 0 || !player.seekable || player.duration <= 0) return
        const resumeTo = Math.min(pendingResume, Math.max(0, player.duration - 1000))
        if (resumeTo <= 0) { pendingResume = 0; return }
        window.seekTo(resumeTo)
        lastStableSource = player.source.toString()
        lastStablePosition = resumeTo
        pendingResume = 0
    }
    function refreshBookmarks() { marked = window.store.bookmarks(player.source) }
    function openMedia(item, entries) {
        const keepVisualizer = page === "visualizer" && item.kind === "audio"
        saveProgress()
        decks.cancelTransition(); crossfadeSource = ""
        player.stop()
        queue = entries && entries.length ? entries.slice() : [item]
        queueIndex = queue.findIndex(function(value) { return value.url === item.url })
        const fresh = store.entry(item.url)
        current = fresh.url ? fresh : item
        clearRepeat()
        pendingResume = store.preferences.resume && current.position > 0
            && current.duration > current.position + 2000 ? current.position : 0
        lastStableSource = ""
        lastStablePosition = 0
        player.playbackRate = store.preferences.defaultSpeed
        if (!mediaSource.open(player, item.url)) { pendingResume = 0; notice = mediaSource.error; return }
        refreshBookmarks()
        page = item.kind === "audio" ? (keepVisualizer ? "visualizer" : "music") : "player"
        window.playMedia()
    }
    property string crossfadeSource: ""
    property int crossfadeIndex: -1
    function prepareCrossfade() {
        if (decks.fading) return
        if (!store.preferences.crossfade || !store.preferences.autoAdvance || current.kind !== "audio" || page === "study" || repeatActive || player.playbackRate !== 1) {
            decks.stageNext(""); crossfadeSource = ""; return
        }
        if (crossfadeSource === player.source.toString()) return
        crossfadeSource = player.source.toString()
        let index = queueIndex + 1
        if (shuffle && queue.length > 1) index = (queueIndex + 1 + Math.floor(Math.random() * (queue.length - 1))) % queue.length
        if (index >= queue.length && repeatQueue) index = 0
        crossfadeIndex = index
        const item = index >= 0 && index < queue.length ? store.entry(queue[index].url) : ({})
        decks.stageNext(item.kind === "audio" && item.url !== current.url ? item.url : "")
    }
    Connections {
        target: window.decks
        function onCrossfadeStarted(outgoing, incoming, position, duration) {
            window.store.updateProgress(outgoing, position, duration)
            window.pendingResume = 0
            window.current = window.store.entry(incoming)
            window.queueIndex = window.crossfadeIndex
            window.crossfadeSource = ""
            window.clearRepeat(); window.refreshBookmarks()
        }
    }
    Timer { interval: 250; running: window.playing; repeat: true; onTriggered: window.prepareCrossfade() }
    function advance(delta) {
        if (!queue.length) return
        let index = queueIndex + delta
        if (shuffle && queue.length > 1) index = (queueIndex + 1 + Math.floor(Math.random() * (queue.length - 1))) % queue.length
        if (index < 0 || index >= queue.length) {
            if (!repeatQueue) return
            index = (index + queue.length) % queue.length
        }
        // Refresh resume metadata from the persistent library, then start the
        // next item at the beginning for normal sequential queue playback.
        let item = window.store.entry(queue[index].url)
        item.position = 0
        openMedia(item, queue)
    }
    function togglePlayback() { if (playing) window.pauseMedia(); else window.playMedia() }
    onPlayingChanged: crossfadeSource = ""
    function finishPlayback() {
        if (repeatActive) { handleRepeatBoundary(player.duration); if (repeatActive) window.playMedia(); return }
        if (store.preferences.autoAdvance && (queueIndex + 1 < queue.length || repeatQueue || shuffle)) advance(1)
    }
    function showSoundStudio() { soundStudioPanel.open() }
    function seekTo(position) { crossfadeSource = ""; decks.cancelTransition(); soundStudio.prepareSeek(position); player.setPosition(position) }
    function seekBy(ms) { if (player.seekable) window.seekTo(Math.max(0, Math.min(player.duration, player.position + ms))) }
    function forgetEntry(item) {
        saveProgress()
        if (window.store.entry(player.source).url === item.url) {
            player.stop(); player.source = ""; current = ({}); page = "library"
        }
        queue = queue.filter(function(value) { return value.url !== item.url })
        queueIndex = queue.findIndex(function(value) { return value.url === player.source.toString() })
        window.store.forget(item.url)
    }
    function playlistFeedback(result) {
        if (result.status === "missingPlaylist") return qsTr("This playlist no longer exists.")
        if (result.status === "saveError") return qsTr("Playlist could not be saved. No files were added.")
        const parts = []
        if (result.added) parts.push(result.added === 1 ? qsTr("1 file added") : qsTr("%1 files added").arg(result.added))
        if (result.duplicates) parts.push(qsTr("%1 already in playlist").arg(result.duplicates))
        if (result.failed) parts.push(qsTr("%1 could not be added").arg(result.failed))
        return parts.length ? parts.join(" • ") : qsTr("No files selected")
    }
    function openMediaPicker(playlistId) { picker.targetPlaylist = playlistId || ""; picker.open() }
    function handlePickedMedia(urls, playlistId) {
        if (playlistId) {
            const result = store.addToPlaylistBatch(playlistId, urls, true)
            notice = playlistFeedback(result)
            const destination = store.playlists.find(function(item) { return item.id === playlistId })
            if (result.status === "ok" && destination) {
                selectedPlaylist = destination.id; selectedPlaylistName = destination.name; page = "collection"
            }
        } else {
            const first = store.importFiles(urls)
            if (first.length) {
                const added = urls.map(function(url) { return window.store.entry(url) }).filter(function(item, index, all) { return item.url && all.findIndex(function(other) { return other.url === item.url }) === index })
                openMedia(store.entry(first), added)
            } else notice = qsTr("No accessible local media selected")
        }
    }
    function choosePlaylistForEntry(item) {
        choosePlaylistDialog.purpose = "entry"; choosePlaylistDialog.entryUrl = item.url || ""; choosePlaylistDialog.open()
    }
    function addMediaFromHeader() {
        if (page === "collection") openMediaPicker(selectedPlaylist)
        else if (page === "playlists") {
            choosePlaylistDialog.purpose = "import"; choosePlaylistDialog.entryUrl = ""; choosePlaylistDialog.open()
        } else openMediaPicker("")
    }
    function showOptions(item) { selectedEntry = item; mediaOptions.open() }
    Shortcut {
        sequences: ["Back", "Escape"]
        enabled: window.page !== "library" || window.visibility === Window.FullScreen
        onActivated: {
            window.saveProgress()
            if (videoPane.locked) { videoPane.locked = false; videoPane.controlsVisible = true }
            else if (window.page === "visualizer") window.setVisualizerFullscreen(false)
            else if (window.immersivePlayback) window.setFullscreen(false)
            else window.page = window.page === "about" ? "settings" : window.page === "collection" ? "playlists" : "library"
        }
    }
    onClosing: function(close) {
        saveProgress(); store.configurePlayerWindow(-1, false, 0, false)
        if (Qt.platform.os === "android") { close.accepted = false; playbackSession.background() }
    }
    onActiveChanged: {
        saveProgress()
        syncPlayerWindow()
        // Music continues through the media service; hidden video stays paused.
        if (Qt.platform.os !== "android" && !active && player.hasVideo) window.pauseMedia()
    }
    Connections {
        target: window.store
        function onErrorOccurred(message) { window.notice = message }
        function onPreferencesChanged() { window.decks.setCrossfade(window.store.preferences.crossfade); window.playbackSession.setPiPEnabled(window.store.preferences.pipAutomatic); window.syncPlayerWindow() }
        function onBookmarksChanged() { window.refreshBookmarks() }
        function onPlaylistsChanged() { window.playlistRevision++ }
        function onLibraryChanged() {
            window.queue = window.queue.map(function(item) { return window.store.entry(item.url) }).filter(function(item, index, all) { return item.url && all.findIndex(function(other) { return other.url === item.url }) === index })
            const playingEntry = window.store.entry(player.source)
            window.queueIndex = window.queue.findIndex(function(item) { return item.url === playingEntry.url })
            if (window.hasMedia) {
                window.current = window.store.entry(player.source)
                if (!window.current.url) {
                    player.stop(); player.source = ""
                    if (window.playerPage || window.page === "music") window.page = "library"
                }
            }
        }
    }

    Connections {
        target: window.player
        function onSourceChanged() {
            window.clearRepeat()
            window.lastStableSource = ""
            window.lastStablePosition = 0
            window.externalCues = []; window.externalEnabled = false
            window.externalSubtitleName = ""; window.subtitleDelay = 0
        }
        function onTracksChanged() {
            if (!window.store.preferences.subtitles || window.externalEnabled) player.activeSubtitleTrack = -1
        }
        function onErrorOccurred(error, errorString) { window.notice = errorString || qsTr("Unable to play. Try selecting the file again.") }
        function onPositionChanged(position) {
            window.handleRepeatBoundary(position)
        }
        function onDurationChanged() { window.restorePendingPosition() }
        function onSeekableChanged(canSeek) { window.restorePendingPosition() }
        function onPositionChanged() {
            if (player.position > 0 && player.duration > 0) {
                window.lastStableSource = player.source.toString()
                window.lastStablePosition = player.position
            }
        }
        function onPlaybackStateChanged() { if (player.playbackState !== MediaPlayer.PlayingState) window.saveProgress(); window.syncPlayerWindow() }
        function onMediaStatusChanged() {
            window.restorePendingPosition()
            if (player.mediaStatus === MediaPlayer.EndOfMedia) {
                window.saveProgress()
                window.soundStudio.finish()
            }
        }
    }
    Connections {
        target: window.playbackSession
        function onWindowRestoreRequested(pixelWidth, pixelHeight) {
            if (!window.pipActive) window.playbackSession.restoreWindowBounds(window, pixelWidth, pixelHeight)
        }
        function onNextRequested() { window.advance(1) }
        function onPreviousRequested() { window.advance(-1) }
        function onSeekRequested(position) { window.seekTo(position) }
        function onErrorOccurred(message) { window.notice = message }
    }
    Binding { target: window.soundStudio; property: "music"; value: window.current.kind === "audio" }
    Connections { target: window.soundStudio; function onPlaybackFinished() { window.finishPlayback() } }
    Timer { interval: 10000; running: window.playing; repeat: true; onTriggered: window.saveProgress() }
    Timer {
        interval: 60000; running: window.sleepMinutes > 0; repeat: true
        onTriggered: { window.sleepMinutes--; if (!window.sleepMinutes) { window.pauseMedia(); window.notice = qsTr("Sleep timer finished") } }
    }
    FileDialog {
        id: picker; objectName: "mediaFilePicker"
        property string targetPlaylist: ""
        title: targetPlaylist.length ? qsTr("Add files to playlist") : qsTr("Add audio and video files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Media (*.mp4 *.mkv *.webm *.mov *.avi *.mp3 *.m4a *.flac *.ogg *.wav *.opus *.aac)"), qsTr("All files (*)")]
        onAccepted: { window.handlePickedMedia(selectedFiles, targetPlaylist); targetPlaylist = "" }
        onRejected: targetPlaylist = ""
    }
    FileDialog {
        id: exportPicker
        title: qsTr("Export study notes")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: [qsTr("Text files (*.txt)")]
        property url notesSource
        onAccepted: { if (window.store.exportNotes(notesSource, selectedFile)) window.notice = qsTr("Study notes exported") }
    }

    header: Item {
        height: window.pipActive || window.page === "player" || window.page === "visualizer" ? 0 : 68 + window.SafeArea.margins.top
        visible: height > 0
        RowLayout {
            anchors.fill: parent; anchors.topMargin: window.SafeArea.margins.top; anchors.leftMargin: 16 + window.SafeArea.margins.left; anchors.rightMargin: 16 + window.SafeArea.margins.right; spacing: 8
            ActionButton {
                visible: window.page !== "library"
                symbol: "back"; quiet: true; Accessible.name: qsTr("Back to library")
                onClicked: { window.saveProgress(); window.page = window.page === "about" ? "settings" : window.page === "collection" ? "playlists" : "library"; window.setFullscreen(false) }
            }
            Image { visible: window.page === "library" || window.page === "music"; source: Qt.resolvedUrl("jatayu-media-player.svg"); sourceSize: Qt.size(96, 96); Layout.preferredWidth: 36; Layout.preferredHeight: 36; fillMode: Image.PreserveAspectFit; Accessible.name: qsTr("Jatayu Media Player logo") }
            Label { textFormat: Text.PlainText;
                Layout.fillWidth: true
                text: window.page === "library" ? qsTr("Jatayu Media Player") : window.page === "music" ? qsTr("Now playing") : window.page === "study" ? qsTr("Study Mode") : window.page === "player" ? window.current.title || qsTr("Video player") : window.page === "collection" ? window.selectedPlaylistName : window.page === "playlists" ? qsTr("Playlists") : window.page === "about" ? qsTr("About") : qsTr("Settings")
                elide: Text.ElideRight; font.pixelSize: window.width < 390 ? 16 : 18; font.weight: Font.DemiBold
            }
            ActionButton { visible: window.page === "library"; symbol: "search"; quiet: true; Accessible.name: qsTr("Search library"); onClicked: { window.searching = !window.searching; if (!window.searching) searchField.clear(); else searchField.forceActiveFocus() } }
            ActionButton { visible: window.page !== "about"; symbol: "add"; quiet: true; objectName: "headerAddMedia"; Accessible.name: window.page === "collection" ? qsTr("Add files to this playlist") : qsTr("Add media files"); onClicked: window.addMediaFromHeader() }
        }
    }
    footer: ColumnLayout {
        spacing: 0
        visible: !window.pipActive && !window.playerPage && window.page !== "music"
        height: visible ? implicitHeight : 0
        Rectangle {
            visible: window.hasMedia; Layout.fillWidth: true; Layout.preferredHeight: visible ? 68 : 0
            color: "#222730"
            RowLayout {
                anchors.fill: parent; anchors.margins: 8
                Image { Layout.preferredWidth: 50; Layout.preferredHeight: 50; source: window.hasMedia ? "image://artwork/" + encodeURIComponent(player.source.toString()) : ""; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: window.current.title || qsTr("Selected media"); elide: Text.ElideRight; font.pixelSize: 14 }
                    Label { textFormat: Text.PlainText; text: window.time(player.position) + " / " + window.time(player.duration); color: "#aab2c0"; font.pixelSize: 12 }
                    TapHandler { onTapped: window.page = window.current.kind === "audio" ? "music" : "player" }
                }
                ActionButton { symbol: window.playing ? "pause" : "play"; quiet: true; onClicked: window.togglePlayback() }
                ActionButton { symbol: "playlist"; quiet: true; Accessible.name: qsTr("Open queue"); onClicked: queueDialog.open() }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 72; color: "#15191f"
            RowLayout {
                anchors.fill: parent
                Repeater {
                    model: [{ name: qsTr("Library"), icon: "library", page: "library" }, { name: qsTr("Playlists"), icon: "playlist", page: "playlists" }, { name: qsTr("Settings"), icon: "settings", page: "settings" }]
                    delegate: Item {
                        id: navItem
                        required property var modelData
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Column {
                            anchors.centerIn: parent; spacing: 6
                            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: navItem.modelData.icon; color: window.page === navItem.modelData.page ? "#ff9328" : "#aab2c0" }
                            Text { text: navItem.modelData.name; color: window.page === navItem.modelData.page ? "#ff9328" : "#aab2c0"; font.pixelSize: 12 }
                        }
                        TapHandler { onTapped: { window.saveProgress(); window.page = navItem.modelData.page } }
                        Accessible.role: Accessible.Button; Accessible.name: navItem.modelData.name
                    }
                }
            }
        }
        Item { Layout.fillWidth: true; Layout.preferredHeight: window.SafeArea.margins.bottom }

    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: window.wide ? 28 : 16; spacing: 12
        visible: window.page === "library"
        TextField {
            id: searchField
            visible: window.searching; Layout.fillWidth: true
            placeholderText: qsTr("Search your videos and music")
            onTextChanged: window.query = text
            selectByMouse: true
        }
        Flickable {
            Layout.fillWidth: true; Layout.preferredHeight: 48; contentWidth: libraryTabs.implicitWidth; clip: true
            Row {
                id: libraryTabs; spacing: 6
                Repeater {
                    model: ["Continue", "Videos", "Music", "Folders", "Collections"]
                    delegate: ActionButton {
                        required property string modelData
                        text: qsTr(modelData); selected: window.tab === modelData; quiet: !selected
                        onClicked: { window.tab = modelData; window.browseSelection = "" }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: scanPanel.implicitHeight + 24
            color: "#1e252e"; radius: 16
            ColumnLayout {
                id: scanPanel; anchors.fill: parent; anchors.margins: 12; spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 3
                        Label { textFormat: Text.PlainText; text: qsTr("Your device library"); font.weight: Font.DemiBold; font.pixelSize: 16 }
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: window.store.media.length + qsTr(" files • videos and music"); color: "#aeb8c8"; font.pixelSize: 12; elide: Text.ElideRight }
                    }
                    ActionButton { objectName: "scanDeviceButton"; symbol: "refresh"; text: window.store.scanning ? qsTr("Scanning") : qsTr("Scan"); primary: true; enabled: !window.store.scanning; onClicked: window.store.scanDevice(true) }
                }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: window.store.scanStatus.length > 0; text: qsTr(window.store.scanStatus); wrapMode: Text.WordWrap; font.pixelSize: 12; color: "#aeb8c8" }
            }
        }
        RowLayout {
            visible: window.tab === "Music"; Layout.fillWidth: true
            Repeater {
                model: ["Songs", "Albums", "Artists"]
                delegate: ActionButton { required property string modelData; text: qsTr(modelData); selected: window.musicBrowse === modelData; quiet: !selected; onClicked: { window.musicBrowse = modelData; window.browseSelection = "" } }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { visible: window.browseSelection.length > 0; symbol: "back"; quiet: true; Accessible.name: qsTr("Back to groups"); onClicked: window.browseSelection = "" }
            Label { textFormat: Text.PlainText;
                Layout.fillWidth: true; font.pixelSize: 21; font.weight: Font.DemiBold; elide: Text.ElideRight
                text: window.browseSelection.length ? window.browseTitle : window.tab === "Continue" ? qsTr("For you") : window.tab === "Collections" ? qsTr("Favorites") : window.tab === "Music" ? qsTr(window.musicBrowse) : qsTr(window.tab)
            }
            ActionButton { symbol: "sort"; quiet: true; Accessible.name: qsTr("Sort library"); onClicked: sortDialog.open() }
        }
        ListView {
            id: groupList
            visible: window.showGroups; Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 10; model: window.browseGroups; ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: groupRow
                required property var modelData
                width: groupList.width; height: 96; radius: 14; color: "#202630"
                RowLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 12
                    Image { Layout.preferredWidth: 76; Layout.preferredHeight: 72; source: "image://artwork/" + encodeURIComponent(groupRow.modelData.url); asynchronous: true; fillMode: Image.PreserveAspectCrop }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: groupRow.modelData.title; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: groupRow.modelData.count + qsTr(" files") + (groupRow.modelData.detail ? " • " + groupRow.modelData.detail : ""); color: "#aeb8c8"; font.pixelSize: 12; elide: Text.ElideRight }
                    }
                    Icon { name: window.tab === "Folders" ? "folder" : "music"; color: "#ff9328" }
                }
                TapHandler { onTapped: { window.browseSelection = groupRow.modelData.key; window.browseTitle = groupRow.modelData.title } }
            }
            Label { textFormat: Text.PlainText; anchors.centerIn: parent; width: parent.width - 24; visible: groupList.count === 0; text: qsTr("Scan your device or add files to see groups here."); color: "#aeb8c8"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        }
        GridView {
            id: libraryGrid
            visible: !window.showGroups
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            cellWidth: width / Math.max(2, Math.floor(width / 210))
            cellHeight: cellWidth * 0.62 + 78
            model: window.visibleMedia
            onCountChanged: Qt.callLater(function() { libraryGrid.positionViewAtBeginning() })
            onVisibleChanged: { if (visible) Qt.callLater(function() { libraryGrid.positionViewAtBeginning() }) }
            ScrollBar.vertical: ScrollBar {}
            header: ColumnLayout {
                width: libraryGrid.width - 12; spacing: 12
                visible: window.tab === "Continue" && !window.query.length
                height: visible ? implicitHeight : 0
                Label { textFormat: Text.PlainText; visible: window.continueItems.length > 0; text: qsTr("Continue watching"); font.pixelSize: 19; font.weight: Font.DemiBold }
                Rectangle {
                    id: continueCard
                    property var entry: window.continueItems.length ? window.continueItems[0] : ({})
                    visible: window.continueItems.length > 0
                    Layout.fillWidth: true; Layout.preferredHeight: visible ? Math.min(230, libraryGrid.width * 0.56) : 0
                    color: "#1c242c"; radius: 16; clip: true
                    Image { anchors.fill: parent; source: continueCard.entry.url ? "image://artwork/" + encodeURIComponent(continueCard.entry.url) : ""; asynchronous: true; fillMode: Image.PreserveAspectCrop }
                    Rectangle {
                        anchors.bottom: parent.bottom; width: parent.width; height: 105
                        gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "#f0080c12" } }
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 14; spacing: 6
                            Item { Layout.fillHeight: true }
                            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: continueCard.entry.title || ""; elide: Text.ElideRight; font.weight: Font.DemiBold; font.pixelSize: 17 }
                            Label { textFormat: Text.PlainText; text: window.time(continueCard.entry.position || 0) + " / " + window.time(continueCard.entry.duration || 0); font.pixelSize: 12 }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 4; color: "#5b6370"
                                Rectangle { height: 4; color: "#ff9328"; width: parent.width * Math.min(1, (continueCard.entry.position || 0) / Math.max(1, continueCard.entry.duration || 0)) }
                            }
                        }
                    }
                    TapHandler { onTapped: window.openMedia(continueCard.entry, window.continueItems) }
                }
                Label { textFormat: Text.PlainText; visible: window.visibleMedia.length > 0; text: qsTr("Recently added"); font.pixelSize: 19; font.weight: Font.DemiBold; Layout.bottomMargin: 4 }
            }
            Connections {
                target: libraryGrid.headerItem
                function onHeightChanged() { Qt.callLater(function() { libraryGrid.positionViewAtBeginning() }) }
            }
            delegate: MediaCard {
                required property var modelData
                width: libraryGrid.cellWidth - 12; entry: modelData
                onActivated: window.openMedia(modelData, window.visibleMedia)
                onOptionsRequested: window.showOptions(modelData)
            }
            Label { textFormat: Text.PlainText;
                anchors.centerIn: parent; width: Math.min(parent.width - 32, 420); visible: libraryGrid.count === 0
                text: window.query.length ? qsTr("No matching media") : window.tab === "Collections" ? qsTr("Save favorites from a file’s menu.") : qsTr("Ready for your videos and music.\n\nTap Scan to discover device media, or + to select individual files.")
                color: "#aeb7c7"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 16
            }
        }
    }

    // Music uses the same real playback engine, queue and stored resume state.
    ScrollView {
        anchors.fill: parent; visible: window.page === "music"; clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: Math.min(window.width - 40, 520); x: (window.width - width) / 2; spacing: 18
            MusicVisualization {
                host: window; objectName: "musicHero"
                Layout.fillWidth: true; Layout.preferredHeight: Math.min(window.height * 0.36, 340)
            }
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: [{key: "artwork", title: qsTr("Artwork")}, {key: "bars", title: qsTr("Bars")}, {key: "pulse", title: qsTr("Pulse")}]
                    delegate: ActionButton { required property var modelData; Layout.fillWidth: true; text: modelData.title; selected: window.store.preferences.visualMode === modelData.key; onClicked: window.store.setPreference("visualMode", modelData.key) }
                }
                ActionButton { symbol: "expand"; quiet: true; Accessible.name: qsTr("Fullscreen visualiser"); onClicked: window.setVisualizerFullscreen(true) }
            }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: window.current.title || qsTr("Choose your music"); font.pixelSize: 24; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                    Label { textFormat: Text.PlainText; text: window.current.artist || qsTr("Local music"); color: "#aeb7c7" }
                }
                ActionButton { symbol: "heart"; selected: !!window.current.favorite; quiet: !selected; Accessible.name: qsTr("Toggle favorite"); onClicked: window.store.toggleFavorite(player.source) }
            }
            VolumeControl { host: window; controlKey: "music"; Layout.fillWidth: true }
            ActionButton { text: qsTr("Sound Studio"); symbol: "settings"; Layout.fillWidth: true; primary: true; onClicked: window.showSoundStudio() }
            Slider { Layout.fillWidth: true; from: 0; to: Math.max(1, player.duration); value: player.position; enabled: player.seekable; onMoved: window.seekTo(value); Accessible.name: qsTr("Playback position") }
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; text: window.time(player.position); font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Label { textFormat: Text.PlainText; text: window.time(player.duration); font.pixelSize: 12 }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter; spacing: 12
                ActionButton { symbol: "shuffle"; selected: window.shuffle; quiet: !selected; onClicked: window.shuffle = !window.shuffle }
                ActionButton { symbol: "previous"; quiet: true; enabled: window.queueIndex > 0 || window.repeatQueue; onClicked: window.advance(-1) }
                ActionButton { symbol: window.playing ? "pause" : "play"; primary: true; implicitWidth: 68; implicitHeight: 68; onClicked: window.togglePlayback() }
                ActionButton { symbol: "next"; quiet: true; enabled: window.queueIndex + 1 < window.queue.length || window.repeatQueue; onClicked: window.advance(1) }
                ActionButton { symbol: "repeat"; selected: window.repeatQueue; quiet: !selected; onClicked: window.repeatQueue = !window.repeatQueue }
            }
            RowLayout {
                Layout.fillWidth: true
                ActionButton { Layout.fillWidth: true; symbol: "playlist"; text: qsTr("Queue"); onClicked: queueDialog.open() }
                ActionButton { Layout.fillWidth: true; symbol: "timer"; text: window.sleepMinutes ? window.sleepMinutes + qsTr(" min") : qsTr("Sleep timer"); onClicked: sleepDialog.open() }
            }
            ActionButton { Layout.fillWidth: true; symbol: "bookmark"; text: qsTr("Study this audio"); onClicked: window.page = "study" }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Up next"); font.pixelSize: 18; font.weight: Font.DemiBold }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: window.queueIndex + 1 < window.queue.length ? window.queue[window.queueIndex + 1].title : qsTr("End of queue"); color: "#aeb7c7"; elide: Text.ElideRight }
            Item { Layout.preferredHeight: 24 }
        }
    }

    // A single VideoOutput remains attached to the player across both pages.
    RowLayout {
        anchors.fill: parent; anchors.margins: window.page === "player" ? 0 : (window.landscape ? 12 : 16); spacing: 16
        visible: window.page === "player" || window.page === "study"
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: window.landscape ? 6 : 12
            VideoSurface {
                id: videoPane
                player: window.playbackEngine; host: window
                Layout.fillWidth: true
                Layout.fillHeight: window.page !== "study" || window.landscape
                Layout.preferredHeight: window.page === "study" && !window.landscape ? Math.min(window.height * 0.23, 230) : -1
                Layout.minimumHeight: 70
            }
            Label { textFormat: Text.PlainText; visible: window.page === "study"; Layout.fillWidth: true; text: window.current.title || ""; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Slider {
                visible: window.page === "study"
                Layout.fillWidth: true; from: 0; to: Math.max(1, player.duration); value: player.position
                enabled: player.seekable; onMoved: window.seekTo(value); Accessible.name: qsTr("Playback position")
            }
            RowLayout {
                visible: window.page === "study"
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; text: window.time(player.position) + " / " + window.time(player.duration); font.pixelSize: 12; color: "#b6becc" }
                Item { Layout.fillWidth: true }
                ActionButton { symbol: "timer"; quiet: true; Accessible.name: qsTr("Playback speed"); text: player.playbackRate.toFixed(2).replace(/0$/, "") + "×"; onClicked: speedDialog.open() }
                ActionButton { symbol: "expand"; quiet: true; Accessible.name: qsTr("Fullscreen"); onClicked: { if (window.current.kind === "audio") window.setVisualizerFullscreen(true); else { window.page = "player"; window.setFullscreen(true) } } }
            }
            RowLayout {
                visible: window.page === "study"
                Layout.alignment: Qt.AlignHCenter; spacing: window.landscape ? 16 : 10
                ActionButton { text: "−" + window.store.preferences.studySkip; quiet: true; Accessible.name: qsTr("Study skip backward"); onClicked: window.seekBy(-window.store.preferences.studySkip * 1000) }
                ActionButton { symbol: "previous"; quiet: true; enabled: window.queueIndex > 0; onClicked: window.advance(-1) }
                ActionButton { symbol: window.playing ? "pause" : "play"; primary: true; implicitWidth: 58; implicitHeight: 58; onClicked: window.togglePlayback() }
                ActionButton { symbol: "next"; quiet: true; enabled: window.queueIndex + 1 < window.queue.length; onClicked: window.advance(1) }
                ActionButton { text: "+" + window.store.preferences.studySkip; quiet: true; Accessible.name: qsTr("Study skip forward"); onClicked: window.seekBy(window.store.preferences.studySkip * 1000) }
            }
            RowLayout {
                visible: !window.landscape && window.page === "study"; Layout.fillWidth: true
                ActionButton { Layout.fillWidth: true; text: window.current.kind === "audio" ? qsTr("Music mode") : qsTr("Video mode"); symbol: "bookmark"; selected: window.page === "study"; onClicked: window.page = window.current.kind === "audio" ? "music" : "player" }
                ActionButton { symbol: "subtitles"; Accessible.name: qsTr("Audio and subtitles"); onClicked: tracksDialog.open() }
                ActionButton { symbol: "playlist"; Accessible.name: qsTr("Queue"); onClicked: queueDialog.open() }
            }
            StudyPanel {
                host: window; visible: window.page === "study" && !window.landscape
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 80
            }
        }
        Rectangle {
            visible: !window.pipActive && window.landscape && (window.page === "study" || (window.wide && !window.immersivePlayback)); enabled: !videoPane.locked; Layout.preferredWidth: window.page === "study" ? (window.wide ? 360 : 280) : 270; Layout.fillHeight: true
            color: "#1b2028"; radius: 16
            StudyPanel { anchors.fill: parent; anchors.margins: 12; visible: window.page === "study"; host: window }
            ScrollView {
                visible: window.page !== "study"
                anchors.fill: parent; anchors.margins: 12; contentWidth: availableWidth; clip: true
                ColumnLayout {
                    width: parent.width; spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: window.page === "study" ? qsTr("Study Mode") : qsTr("Up next"); font.pixelSize: 18; font.weight: Font.DemiBold }
                    ActionButton { symbol: "bookmark"; selected: window.page === "study"; quiet: !selected; onClicked: window.page = "study" }
                }
                ListView {
                    Layout.fillWidth: true; Layout.preferredHeight: Math.max(80, window.height - (window.page === "study" ? 390 : 230)); clip: true; spacing: 8
                    model: window.page === "study" ? window.marked : window.queue
                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width; height: 62
                        text: window.page === "study" ? window.time(modelData.position) + "  " + (modelData.note || qsTr("Bookmark")) : modelData.title
                        highlighted: window.page !== "study" && index === window.queueIndex
                        onClicked: { if (window.page === "study") window.seekTo(modelData.position); else window.openMedia(modelData, window.queue) }
                    }
                }
                ActionButton { Layout.fillWidth: true; text: qsTr("Audio / subtitles"); symbol: "subtitles"; onClicked: tracksDialog.open() }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 16
        visible: window.page === "playlists" || window.page === "collection"
        RowLayout {
            Layout.fillWidth: true
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: window.page === "collection" ? window.selectedPlaylistName : qsTr("Made by you"); font.pixelSize: 22; font.weight: Font.DemiBold }
            ActionButton { visible: window.page === "collection"; objectName: "addPlaylistFiles"; text: qsTr("Add files"); symbol: "add"; primary: true; onClicked: window.openMediaPicker(window.selectedPlaylist) }
            ActionButton { visible: window.page === "playlists"; text: qsTr("New"); symbol: "add"; primary: true; onClicked: { playlistDialog.addAfterCreate = false; playlistDialog.pendingEntry = ""; playlistName.text = ""; playlistDialog.open() } }
        }
        ListView {
            id: playlistList
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
            model: window.page === "collection" ? window.collectionMedia : window.store.playlists
            delegate: RowLayout {
                id: playlistRow
                required property var modelData
                width: ListView.view.width; height: 64
                Icon { name: window.page === "collection" ? "music" : "playlist"; color: "#ff9328"; Layout.margins: 12 }
                Label { textFormat: Text.PlainText;
                    Layout.fillWidth: true; text: playlistRow.modelData.title || playlistRow.modelData.name; elide: Text.ElideRight
                    TapHandler {
                        onTapped: {
                            if (window.page === "collection") window.openMedia(playlistRow.modelData, window.collectionMedia)
                            else { window.selectedPlaylist = playlistRow.modelData.id; window.selectedPlaylistName = playlistRow.modelData.name; window.page = "collection" }
                        }
                    }
                }
                Label { textFormat: Text.PlainText; visible: window.page === "playlists"; text: (playlistRow.modelData.urls || []).length === 1 ? qsTr("1 file") : qsTr("%1 files").arg((playlistRow.modelData.urls || []).length); color: "#aab2c0"; font.pixelSize: 12 }
                ActionButton { visible: window.page === "collection"; symbol: "more"; quiet: true; onClicked: window.showOptions(playlistRow.modelData) }
                ActionButton { visible: window.page === "playlists"; symbol: "close"; quiet: true; Accessible.name: qsTr("Remove playlist"); onClicked: { window.selectedPlaylist = playlistRow.modelData.id; removePlaylistDialog.open() } }
            }
            Label { textFormat: Text.PlainText; anchors.centerIn: parent; width: parent.width - 20; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: playlistList.count === 0; text: window.page === "collection" ? qsTr("Tap Add files to choose music or videos for this playlist.") : qsTr("Create a playlist for your music, videos or lessons"); color: "#aab2c0" }
        }
    }
    ScrollView {
        anchors.fill: parent; anchors.margins: 20; contentWidth: availableWidth
        visible: window.page === "settings"
        ColumnLayout {
            width: parent.width; spacing: 14
            Label { textFormat: Text.PlainText; text: qsTr("Playback"); font.pixelSize: 24; font.weight: Font.DemiBold }
            Switch { text: qsTr("Resume where I left off"); checked: window.store.preferences.resume; onToggled: window.store.setPreference("resume", checked) }
            Switch { text: qsTr("Play next automatically"); checked: window.store.preferences.autoAdvance; onToggled: window.store.setPreference("autoAdvance", checked) }
            Switch { text: qsTr("Keep screen on during video"); checked: window.store.preferences.keepAwake; onToggled: window.store.setPreference("keepAwake", checked) }
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Default speed") }
                ComboBox { model: ["0.5×", "0.75×", "1.0×", "1.25×", "1.5×", "2.0×"]; currentIndex: [0.5, 0.75, 1, 1.25, 1.5, 2].indexOf(window.store.preferences.defaultSpeed); onActivated: function(index) { window.store.setPreference("defaultSpeed", [0.5, 0.75, 1, 1.25, 1.5, 2][index]) } }
            }
            Switch { text: qsTr("Enable embedded subtitles"); checked: window.store.preferences.subtitles; onToggled: { window.store.setPreference("subtitles", checked); if (checked) window.externalEnabled = false; if (!checked) player.activeSubtitleTrack = -1; else if (player.subtitleTracks.length) player.activeSubtitleTrack = 0 } }
            Label { text: qsTr("Interface language"); font.bold: true }
            ComboBox { objectName: "interfaceLanguage"; model: ["English", "हिन्दी"]; currentIndex: window.store.preferences.language === "hi" ? 1 : 0; onActivated: window.store.setPreference("language", currentIndex === 1 ? "hi" : "en") }
            Label { text: qsTr("Music crossfade"); font.bold: true }
            ComboBox {
                objectName: "crossfadeDuration"; model: [qsTr("Off"), qsTr("1 second"), qsTr("2 seconds"), qsTr("3 seconds"), qsTr("5 seconds"), qsTr("8 seconds")]
                property var seconds: [0, 1, 2, 3, 5, 8]
                currentIndex: Math.max(0, seconds.indexOf(window.store.preferences.crossfade))
                onActivated: { window.store.setPreference("crossfade", seconds[currentIndex]); window.crossfadeSource = "" }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Blend consecutive songs at normal speed. Study loops and videos play without crossfade."); color: "#aab2c0" }
            Switch { text: qsTr("Picture-in-picture on leaving video"); checked: window.store.preferences.pipAutomatic; onToggled: window.store.setPreference("pipAutomatic", checked); enabled: window.playbackSession.pipSupported }
            Label { textFormat: Text.PlainText; text: qsTr("Controls"); font.pixelSize: 22; font.weight: Font.DemiBold; Layout.topMargin: 10 }
            Switch { text: qsTr("Swipe controls"); checked: window.store.preferences.gestures; onToggled: window.store.setPreference("gestures", checked) }
            Switch { text: qsTr("Auto-hide video controls"); checked: window.store.preferences.autoHide; onToggled: window.store.setPreference("autoHide", checked) }
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Hide controls after") }
                ComboBox { model: [qsTr("3 seconds"), qsTr("4 seconds"), qsTr("6 seconds"), qsTr("8 seconds")]; currentIndex: [3, 4, 6, 8].indexOf(window.store.preferences.hideDelay); onActivated: function(index) { window.store.setPreference("hideDelay", [3, 4, 6, 8][index]) } }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Skip interval") }
                ComboBox { model: [qsTr("5 seconds"), qsTr("10 seconds"), qsTr("15 seconds"), qsTr("30 seconds")]; currentIndex: [5, 10, 15, 30].indexOf(window.store.preferences.seekStep); onActivated: function(index) { window.store.setPreference("seekStep", [5, 10, 15, 30][index]) } }
            }
            Label { textFormat: Text.PlainText; text: qsTr("Volume"); font.pixelSize: 16 }
            VolumeControl { host: window; controlKey: "settings"; Layout.fillWidth: true }
            Label { textFormat: Text.PlainText; text: qsTr("Library"); font.pixelSize: 22; font.weight: Font.DemiBold; Layout.topMargin: 10 }
            Switch { text: qsTr("Refresh library on open"); checked: window.store.preferences.scanOnOpen; onToggled: window.store.setPreference("scanOnOpen", checked) }
            ActionButton { Layout.fillWidth: true; text: qsTr("Scan / change media access"); symbol: "refresh"; enabled: !window.store.scanning; onClicked: window.store.scanDevice(true, true) }
            ActionButton { Layout.fillWidth: true; text: qsTr("Restore removed device entries"); symbol: "library"; onClicked: { window.store.restoreRemovedMedia(); window.store.scanDevice(true) } }
            ActionButton { Layout.fillWidth: true; text: qsTr("Add individual files"); symbol: "folder"; onClicked: window.openMediaPicker("") }
            Label { textFormat: Text.PlainText; text: qsTr("Privacy"); font.pixelSize: 24; font.weight: Font.DemiBold }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#aab2c0"; text: qsTr("History and study notes stay in this app's local storage. They have no additional app-level encryption. Clearing playback history keeps your files, playlists, favourites and notes.") }
            ActionButton { Layout.fillWidth: true; objectName: "clearHistoryButton"; text: qsTr("Clear playback history"); onClicked: clearHistoryDialog.open() }
            ActionButton { Layout.fillWidth: true; text: qsTr("About Jatayu Media Player"); symbol: "settings"; onClicked: window.page = "about" }
            Item { Layout.preferredHeight: 16 }
        }
    }

    Dialog {
        id: clearHistoryDialog; objectName: "clearHistoryDialog"; title: qsTr("Clear playback history?"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 400); height: Math.min(window.height - 48, 260); standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { textFormat: Text.PlainText; wrapMode: Text.WordWrap; text: qsTr("This stops playback and clears remembered progress and recently played history. Your media files, playlists, favourites and study notes are kept.") }
        onAccepted: {
            player.stop(); player.source = ""; window.current = ({}); window.queue = []; window.queueIndex = -1
            window.pendingResume = 0; window.clearRepeat()
            window.notice = window.store.clearPlaybackHistory() ? qsTr("Playback history cleared") : qsTr("Could not clear playback history")
        }
    }
    Dialog {
        id: sortDialog; title: qsTr("Sort media"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 400)
        ColumnLayout {
            width: parent.width
            Repeater {
                model: [{ key: "recent", title: qsTr("Recently added") }, { key: "name", title: qsTr("Name") }, { key: "duration", title: qsTr("Duration") }, { key: "size", title: qsTr("File size") }]
                delegate: ActionButton { required property var modelData; Layout.fillWidth: true; text: modelData.title; selected: window.store.preferences.sort === modelData.key; onClicked: { window.store.setPreference("sort", modelData.key); sortDialog.close() } }
            }
        }
    }
    Dialog {
        id: playbackOptions; title: qsTr("Playback options"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 420); height: Math.min(window.height - 50, 490)
        ScrollView {
            anchors.fill: parent; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 10
                ActionButton { Layout.fillWidth: true; text: qsTr("Playback speed"); symbol: "timer"; onClicked: { playbackOptions.close(); speedDialog.open() } }
                ActionButton { Layout.fillWidth: true; text: qsTr("Audio and subtitles"); symbol: "subtitles"; onClicked: { playbackOptions.close(); tracksDialog.open() } }
                ActionButton { Layout.fillWidth: true; text: qsTr("Picture-in-picture"); enabled: player.hasVideo && window.playbackSession.pipSupported; onClicked: { playbackOptions.close(); window.playbackSession.enterPiP() } }
                ActionButton { Layout.fillWidth: true; text: qsTr("Study Mode"); symbol: "bookmark"; onClicked: { playbackOptions.close(); window.page = "study" } }
                ActionButton { Layout.fillWidth: true; text: !window.rotationAvailable ? qsTr("Rotation managed by Android") : window.rotationLocked ? qsTr("Unlock rotation") : qsTr("Lock rotation"); enabled: window.rotationAvailable; symbol: "rotate"; selected: window.rotationLocked; onClicked: window.rotationLocked = !window.rotationLocked }
                Label { textFormat: Text.PlainText;
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b8c2d1"
                    text: videoPane.output.sourceRect.width > 0
                        ? qsTr("Video: ") + Math.round(videoPane.output.sourceRect.width) + " × " + Math.round(videoPane.output.sourceRect.height) + qsTr(" • Original source")
                        : qsTr("Video resolution available during playback")
                }
                ActionButton { Layout.fillWidth: true; text: videoPane.cropToFill ? qsTr("Fit entire video") : qsTr("Fill screen (crop edges)"); onClicked: { videoPane.cropToFill = !videoPane.cropToFill; playbackOptions.close() } }
                ActionButton { Layout.fillWidth: true; text: qsTr("Automatic rotation"); symbol: "rotate"; onClicked: { window.rotationLocked = false; window.playbackOrientation = 0; playbackOptions.close() } }
                Label { textFormat: Text.PlainText; text: qsTr("Volume"); font.weight: Font.DemiBold }
                VolumeControl { host: window; controlKey: "options"; Layout.fillWidth: true }
                ActionButton { Layout.fillWidth: true; text: qsTr("Use system brightness"); onClicked: { window.screenBrightness = -1; playbackOptions.close() } }
            }
        }
    }
    Dialog {
        id: volumeDialog
        objectName: "volumeDialog"
        title: qsTr("Playback volume")
        anchors.centerIn: parent
        modal: true
        width: Math.min(window.width - 24, 420)
        standardButtons: Dialog.Close
        contentItem: VolumeControl { host: window; controlKey: "popup" }
    }
    SoundStudioPanel { id: soundStudioPanel; parent: Overlay.overlay; host: window }
    MusicFullscreen { anchors.fill: parent; host: window; visible: window.page === "visualizer" }

    AboutPage { host: window; anchors.fill: parent; anchors.margins: window.wide ? 32 : 20; visible: window.page === "about" }

    Dialog {
        id: mediaOptions; title: window.selectedEntry.title || qsTr("Media options"); modal: true
        anchors.centerIn: parent; width: Math.min(window.width - 32, 400)
        ColumnLayout {
            width: parent.width; spacing: 8
            ActionButton { Layout.fillWidth: true; text: window.selectedEntry.favorite ? qsTr("Remove favorite") : qsTr("Save to favorites"); symbol: "heart"; onClicked: { window.store.toggleFavorite(window.selectedEntry.url); mediaOptions.close() } }
            ActionButton { Layout.fillWidth: true; objectName: "addEntryToPlaylist"; text: qsTr("Add to playlist"); symbol: "playlist"; onClicked: { mediaOptions.close(); window.choosePlaylistForEntry(window.selectedEntry) } }
            ActionButton { Layout.fillWidth: true; text: qsTr("Remove from library"); symbol: "close"; onClicked: { window.forgetEntry(window.selectedEntry); mediaOptions.close(); window.notice = qsTr("Removed from library. Original file kept.") } }
        }
    }
    Dialog {
        id: choosePlaylistDialog; objectName: "playlistDestinationDialog"
        property string purpose: "entry"
        property url entryUrl
        title: qsTr("Choose playlist"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 400); height: Math.min(window.height - 100, 420)
        ColumnLayout {
            anchors.fill: parent; spacing: 12
            ListView {
                id: playlistChooser; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: window.store.playlists
                delegate: ItemDelegate {
                    required property var modelData
                    objectName: "playlistDestination_" + modelData.id
                    width: ListView.view.width; text: modelData.name
                    onClicked: {
                        const destinationId = modelData.id
                        choosePlaylistDialog.close()
                        if (choosePlaylistDialog.purpose === "import") window.openMediaPicker(destinationId)
                        else window.notice = window.playlistFeedback(window.store.addToPlaylistBatch(destinationId, [choosePlaylistDialog.entryUrl], false))
                    }
                }
                Label { textFormat: Text.PlainText; anchors.centerIn: parent; width: parent.width; wrapMode: Text.WordWrap; visible: playlistChooser.count === 0; text: qsTr("Create a playlist to start adding files.") }
            }
            ActionButton {
                Layout.fillWidth: true; text: qsTr("New playlist"); symbol: "add"; primary: true
                onClicked: {
                    playlistDialog.addAfterCreate = choosePlaylistDialog.purpose === "import"
                    playlistDialog.pendingEntry = choosePlaylistDialog.purpose === "entry" ? choosePlaylistDialog.entryUrl : ""
                    choosePlaylistDialog.close(); playlistName.text = ""; playlistDialog.open()
                }
            }
        }
    }
    Dialog {
        id: playlistDialog; objectName: "newPlaylistDialog"; title: qsTr("New playlist"); modal: true; anchors.centerIn: parent
        property bool addAfterCreate: false
        property url pendingEntry
        width: Math.min(window.width - 32, 400); standardButtons: Dialog.Ok | Dialog.Cancel
        TextField { id: playlistName; objectName: "newPlaylistName"; width: parent.width; placeholderText: qsTr("Playlist name"); maximumLength: 100 }
        onAccepted: {
            const id = window.store.createPlaylist(playlistName.text)
            if (!id) window.notice = qsTr("Enter a playlist name")
            else if (addAfterCreate) window.openMediaPicker(id)
            else if (pendingEntry.toString().length) window.notice = window.playlistFeedback(window.store.addToPlaylistBatch(id, [pendingEntry], false))
            addAfterCreate = false; pendingEntry = ""
        }
        onRejected: { addAfterCreate = false; pendingEntry = "" }
    }
    Dialog {
        id: removePlaylistDialog; title: qsTr("Remove this playlist?"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 400); height: Math.min(window.height - 24, 220); standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { textFormat: Text.PlainText; text: qsTr("Your original files will be kept."); wrapMode: Text.WordWrap }
        onAccepted: window.store.removePlaylist(window.selectedPlaylist)
    }
    Dialog {
        id: bookmarkDialog; objectName: "studyNoteDialog"
        title: (editingId.length ? qsTr("Edit note • ") : qsTr("New note • ")) + window.time(bookmarkPosition)
        modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 460); height: Math.min(window.height - 40, 340)
        standardButtons: Dialog.Save | Dialog.Cancel
        property real bookmarkPosition: 0
        property url noteSource
        property string editingId: ""
        ScrollView {
            anchors.fill: parent; contentWidth: availableWidth
            TextArea { id: bookmarkNote; objectName: "studyNoteText"; placeholderText: qsTr("Write an idea, question or word to practise…"); wrapMode: TextEdit.Wrap; selectByMouse: true; onTextChanged: { if (length > 1000) remove(1000, length) } }
        }
        onAccepted: {
            if (editingId.length) window.store.updateBookmark(noteSource, editingId, bookmarkNote.text)
            else window.store.addBookmark(noteSource, bookmarkPosition, bookmarkNote.text)
        }
    }
    Dialog {
        id: passageDialog; objectName: "passageDialog"; title: qsTr("Adjust passage"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 440); standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            width: parent.width; spacing: 12
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Start and end in seconds. Confirm to start practice."); wrapMode: Text.WordWrap; color: "#b8c2d1" }
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Start (A)") }
                SpinBox { id: passageStart; objectName: "passageStart"; from: 0; to: Math.max(0, Math.floor(player.duration / 100) - 5); stepSize: 5; editable: true; validator: DoubleValidator { bottom: 0; top: player.duration / 1000; decimals: 1; locale: "C" } textFromValue: function(value) { return (value / 10).toFixed(1) }; valueFromText: function(text) { return Math.round(Number(text) * 10) } }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("End (B)") }
                SpinBox { id: passageEnd; objectName: "passageEnd"; from: 0; to: Math.floor(player.duration / 100); stepSize: 5; editable: true; validator: DoubleValidator { bottom: 0; top: player.duration / 1000; decimals: 1; locale: "C" } textFromValue: function(value) { return (value / 10).toFixed(1) }; valueFromText: function(text) { return Math.round(Number(text) * 10) } }
            }
        }
        onAccepted: window.beginRepeat(passageStart.value * 100, passageEnd.value * 100)
    }
    FileDialog {
        id: subtitlePicker; title: qsTr("Open local subtitles"); fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Subtitles (*.srt *.vtt)"), qsTr("All files (*)")]
        property url mediaSource
        onAccepted: {
            if (mediaSource.toString() !== player.source.toString()) { window.notice = qsTr("Video changed. Select subtitles again."); return }
            const cues = window.store.readSubtitles(selectedFile)
            if (cues.length) {
                window.externalCues = cues; window.externalEnabled = true; window.subtitleDelay = 0
                window.externalSubtitleName = selectedFile.toString().split("/").pop()
                player.activeSubtitleTrack = -1; window.notice = qsTr("Local subtitles loaded")
            }
        }
    }
    Dialog {
        id: speedDialog; title: qsTr("Playback speed"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 400)
        ColumnLayout {
            width: parent.width
            Label { textFormat: Text.PlainText; text: player.playbackRate.toFixed(2) + "×"; font.pixelSize: 24; color: "#ff9328" }
            Slider { Layout.fillWidth: true; from: 0.5; to: 2; stepSize: 0.05; value: player.playbackRate; onMoved: player.playbackRate = value; Accessible.name: qsTr("Playback speed") }
            ActionButton { text: qsTr("Normal speed"); onClicked: player.playbackRate = 1 }
        }
    }
    Dialog {
        id: sleepDialog; title: qsTr("Sleep timer"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 400)
        ColumnLayout {
            width: parent.width
            Repeater { model: [0, 15, 30, 45, 60]; delegate: ActionButton { required property int modelData; Layout.fillWidth: true; text: modelData ? modelData + qsTr(" minutes") : qsTr("Off"); selected: window.sleepMinutes === modelData; onClicked: { window.sleepMinutes = modelData; sleepDialog.close() } } }
        }
    }
    Dialog {
        id: queueDialog; title: qsTr("Playback queue"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 480); height: Math.min(window.height - 100, 500)
        ListView {
            anchors.fill: parent; clip: true; model: window.queue
            delegate: ItemDelegate { required property var modelData; required property int index; width: ListView.view.width; text: modelData.title; highlighted: index === window.queueIndex; onClicked: { window.openMedia(modelData, window.queue); queueDialog.close() } }
        }
    }
    ButtonGroup { id: audioTrackGroup }
    ButtonGroup { id: subtitleTrackGroup }
    Dialog {
        id: tracksDialog; title: qsTr("Audio and subtitles"); modal: true; anchors.centerIn: parent
        width: Math.min(window.width - 32, 440); height: Math.min(window.height - 100, 450)
        ScrollView {
            anchors.fill: parent; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 8
                Label { textFormat: Text.PlainText; text: qsTr("Audio tracks"); font.weight: Font.DemiBold }
                Repeater { model: player.audioTracks; delegate: RadioButton { required property var modelData; required property int index; ButtonGroup.group: audioTrackGroup; text: modelData.stringValue(MediaMetaData.Title) || qsTr("Audio ") + (index + 1); checked: player.activeAudioTrack === index; onClicked: player.activeAudioTrack = index } }
                Label { textFormat: Text.PlainText; text: qsTr("Embedded subtitles"); font.weight: Font.DemiBold }
                RadioButton { ButtonGroup.group: subtitleTrackGroup; text: qsTr("Off"); checked: player.activeSubtitleTrack === -1 && !window.externalEnabled; onClicked: { window.externalEnabled = false; player.activeSubtitleTrack = -1 } }
                Repeater { model: player.subtitleTracks; delegate: RadioButton { required property var modelData; required property int index; ButtonGroup.group: subtitleTrackGroup; text: modelData.stringValue(MediaMetaData.Title) || qsTr("Subtitle ") + (index + 1); checked: player.activeSubtitleTrack === index && !window.externalEnabled; onClicked: { window.externalEnabled = false; player.activeSubtitleTrack = index } } }
                Label { textFormat: Text.PlainText; text: qsTr("Local subtitles • SRT / WebVTT"); font.weight: Font.DemiBold }
                ActionButton { Layout.fillWidth: true; text: qsTr("Open subtitle file"); symbol: "folder"; enabled: window.hasMedia; onClicked: { tracksDialog.close(); subtitlePicker.mediaSource = player.source; subtitlePicker.open() } }
                Switch { visible: window.externalCues.length > 0; text: qsTr("Show local subtitles"); checked: window.externalEnabled; onToggled: { window.externalEnabled = checked; if (checked) player.activeSubtitleTrack = -1 } }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: window.externalCues.length > 0; text: window.externalSubtitleName; elide: Text.ElideMiddle; color: "#aab2c0" }
                Label { textFormat: Text.PlainText; text: qsTr("Local subtitle size"); color: "#b8c2d1" }
                Slider { Layout.fillWidth: true; from: 16; to: 36; stepSize: 1; value: window.store.preferences.subtitleSize; onMoved: window.store.setPreference("subtitleSize", value); Accessible.name: qsTr("Local subtitle size") }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Local subtitle delay: ") + (window.subtitleDelay / 1000).toFixed(1) + qsTr(" s • positive = later"); wrapMode: Text.WordWrap }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton { Layout.fillWidth: true; text: "−0.5 s"; onClicked: window.subtitleDelay = Math.max(-10000, window.subtitleDelay - 500) }
                    ActionButton { Layout.fillWidth: true; text: qsTr("Reset"); onClicked: window.subtitleDelay = 0 }
                    ActionButton { Layout.fillWidth: true; text: "+0.5 s"; onClicked: window.subtitleDelay = Math.min(10000, window.subtitleDelay + 500) }
                }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Size and timing controls apply to local subtitle files. Embedded track support depends on the file and decoder."); wrapMode: Text.WordWrap; color: "#aab2c0" }
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 12
        width: Math.min(window.width - 24, 560); height: noticeText.implicitHeight + 28
        radius: 14; color: "#343d49"; visible: window.notice.length > 0; z: 50
        Label { textFormat: Text.PlainText; id: noticeText; anchors.fill: parent; anchors.margins: 14; text: window.notice; wrapMode: Text.WordWrap; color: "white" }
        TapHandler { onTapped: window.notice = "" }
        Timer { interval: 6000; running: parent.visible; onTriggered: window.notice = "" }
    }
}

