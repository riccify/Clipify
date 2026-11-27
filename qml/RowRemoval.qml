import QtQuick

NumberAnimation {
    id: removal
    property string rowId: ""
    property real progress: 0
    signal removed(string id)

    target: removal
    property: "progress"
    from: 0
    to: 1
    duration: 185
    easing.type: Easing.InCubic

    function remove(id) {
        if (running)
            return
        rowId = id
        start()
    }

    onFinished: {
        removed(rowId)
        rowId = ""
        progress = 0
    }
}
