import QtQuick

Item {
    id: root
    property string name: "play"
    property color color: "#f5f5f5"
    implicitWidth: 24
    implicitHeight: 24
    readonly property var shapes: ({
        lock: 'M6 11h12v10H6zM8 11V7a4 4 0 0 1 8 0v4',
        unlock: 'M6 11h12v10H6zM8 11V7a4 4 0 0 1 8 0',
        rotate: 'M7 3h10v18H7zM2 8V3h4M22 16v5h-4',
        refresh: 'M20 9a8 8 0 1 0 0 6M20 3v6h-6',
        sort: 'M4 5h16M4 12h11M4 19h6',
        play: 'M8 5l11 7-11 7z', pause: 'M8 5v14M16 5v14',
        next: 'M5 5l11 7-11 7zM19 5v14', previous: 'M19 5l-11 7 11 7zM5 5v14',
        back: 'M15 5l-7 7 7 7M8 12h13', close: 'M6 6l12 12M6 18L18 6',
        search: 'M16 16l5 5M18 10a8 8 0 1 1-16 0 8 8 0 0 1 16 0',
        add: 'M12 4v16M4 12h16', more: 'M12 5h.01M12 12h.01M12 19h.01',
        music: 'M9 18V5l11-2v13M9 7l11-2M9 18a3 3 0 1 1-3-3h3M20 16a3 3 0 1 1-3-3h3',
        video: 'M3 5h18v14H3zM9 8l7 4-7 4z', library: 'M3 10l9-7 9 7v11h-7v-7h-4v7H3z',
        playlist: 'M4 5h16M4 10h16M4 15h9M4 20h9M18 14v7M15 18h6',
        settings: 'M9 3h6l1 3 3 1 2 5-2 5-3 1-1 3H9l-1-3-3-1-2-5 2-5 3-1zM16 12a4 4 0 1 1-8 0 4 4 0 0 1 8 0',
        bookmark: 'M6 3h12v18l-6-4-6 4z', heart: 'M12 21C-7 10 4-3 12 5c8-8 19 5 0 16z',
        expand: 'M3 9V3h6M15 3h6v6M21 15v6h-6M9 21H3v-6',
        repeat: 'M4 8h15l-4-4M20 16H5l4 4M20 8v5M4 16v-5',
        timer: 'M9 2h6M12 8v5l3 2M21 13a9 9 0 1 1-18 0 9 9 0 0 1 18 0',
        volume: 'M3 9h4l5-5v16l-5-5H3zM16 8q6 4 0 8',
        mute: 'M3 9h4l5-5v16l-5-5H3zM16 9l6 6M22 9l-6 6',
        subtitles: 'M3 5h18v14H3zM6 10h4M14 10h4M6 14h6M15 14h3',
        export: 'M12 3v12M7 8l5-5 5 5M4 14v7h16v-7',
        folder: 'M3 6h7l2 3h9v12H3z', check: 'M4 12l5 5L20 6',
        shuffle: 'M3 5h3l12 14h3M17 15l4 4-4 3M3 19h3l4-5M14 9l4-4h3M17 2l4 3-4 4',
        wing: 'M3 3l8 4v6l-8-4zM3 12l8 4v5l-8-4zM14 4l8 8-8 8z'
    })
    Image {
        anchors.fill: parent
        source: "data:image/svg+xml;utf8," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="' + (root.shapes[root.name] || root.shapes.play) + '" fill="' + (root.name === "play" || root.name === "wing" ? root.color : "none") + '" stroke="' + root.color + '" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>')
    }
}
