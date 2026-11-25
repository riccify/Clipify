import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window

    width: 1040
    height: 620
    minimumWidth: 680
    minimumHeight: 460
    visible: true
    title: "Clipify"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint
           | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    color: "#212121"

    readonly property color barColor: "#171717"
    readonly property color panelColor: "#2a2a2a"
    readonly property color borderColor: "#3a3a3a"
    readonly property color textColor: "#f2f2f2"
    readonly property color mutedColor: "#b5b5b5"
    readonly property color accentColor: "#d6d6d6"
    readonly property bool maximized: visibility === Window.Maximized
    property string currentModule: "clipboard"

    onClosing: event => {
        if (windowControls.trayAvailable()) {
            event.accepted = false
            window.hide()
        }
    }

    palette.window: color
    palette.windowText: textColor
    palette.text: textColor
    palette.buttonText: textColor
    palette.base: "#242424"
    palette.button: "#373737"
    palette.highlight: "#595959"
    palette.highlightedText: "#ffffff"

    function toggleMaximized() {
        if (maximized)
            showNormal()
        else
            showMaximized()
    }

    component Panel: Rectangle {
        radius: 8
        antialiasing: true
        color: window.panelColor
        border.color: window.borderColor
    }

    component WindowButton: Button {
        id: control
        property bool closesWindow: false
        Layout.preferredWidth: 36
        Layout.fillHeight: true
        hoverEnabled: true

        contentItem: Label {
            text: control.text
            color: window.textColor
            font.pixelSize: 17
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: height / 2
            color: control.down ? (control.closesWindow ? "#a43447" : "#484848")
                 : control.hovered ? (control.closesWindow ? "#782d3b" : "#303030")
                 : "transparent"
            border.color: window.accentColor
            border.width: control.visualFocus ? 1 : 0
            Behavior on color { ColorAnimation { duration: 100 } }
        }

        ToolTip.visible: hovered
        ToolTip.delay: 550
        ToolTip.text: Accessible.name
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: window.barColor

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: window.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 17
                anchors.rightMargin: 7
                anchors.topMargin: 6
                anchors.bottomMargin: 5
                spacing: 4

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        Rectangle {
                            width: 23
                            height: 23
                            radius: 7
                            color: window.accentColor

                            Rectangle {
                                x: 6
                                y: 7
                                width: 8
                                height: 9
                                radius: 2
                                color: "transparent"
                                border.color: "#232323"
                            }

                            Rectangle {
                                x: 9
                                y: 5
                                width: 8
                                height: 9
                                radius: 2
                                color: window.accentColor
                                border.color: "#232323"
                            }
                        }

                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Clipify"
                            color: window.textColor
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        property point pressPoint

                        onPressed: mouse => pressPoint = Qt.point(mouse.x, mouse.y)
                        onPositionChanged: mouse => {
                            if (pressed && Math.abs(mouse.x - pressPoint.x)
                                           + Math.abs(mouse.y - pressPoint.y) >= 6)
                                windowControls.move(window)
                        }
                        onDoubleClicked: window.toggleMaximized()
                    }
                }

                WindowButton {
                    text: "−"
                    Accessible.name: qsTr("Minimize")
                    onClicked: window.showMinimized()
                }
                WindowButton {
                    text: window.maximized ? "❐" : "□"
                    Accessible.name: window.maximized ? qsTr("Restore") : qsTr("Maximize")
                    onClicked: window.toggleMaximized()
                }
                WindowButton {
                    text: "×"
                    closesWindow: true
                    Accessible.name: qsTr("Close")
                    onClicked: window.close()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                color: window.barColor

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: window.borderColor
                }

                Button {
                    id: clipboardButton
                    x: 13
                    y: 13
                    width: parent.width - 26
                    height: 42
                    text: qsTr("Clipboard")
                    highlighted: window.currentModule === "clipboard"
                    onClicked: window.currentModule = "clipboard"

                    contentItem: Label {
                        text: clipboardButton.text
                        leftPadding: 13
                        verticalAlignment: Text.AlignVCenter
                        color: clipboardButton.highlighted ? "#202020" : window.textColor
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    background: Rectangle {
                        radius: height / 2
                        color: clipboardButton.down ? "#bfbfbf"
                             : clipboardButton.hovered ? (clipboardButton.highlighted ? "#e3e3e3" : "#303030")
                             : clipboardButton.highlighted ? window.accentColor : "transparent"
                        border.color: clipboardButton.visualFocus ? window.accentColor : window.borderColor
                        border.width: clipboardButton.highlighted || clipboardButton.visualFocus ? 1 : 0
                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                }

                Button {
                    id: exitButton
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 13
                    height: 42
                    text: qsTr("Exit")
                    onClicked: windowControls.quit()

                    contentItem: Label {
                        text: exitButton.text
                        leftPadding: 13
                        verticalAlignment: Text.AlignVCenter
                        color: exitButton.hovered ? window.textColor : window.mutedColor
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    background: Rectangle {
                        radius: height / 2
                        color: exitButton.down ? "#414141" : exitButton.hovered ? "#303030" : "transparent"
                        border.color: window.accentColor
                        border.width: exitButton.visualFocus ? 1 : 0
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14
                visible: window.currentModule === "clipboard"

                Label {
                    text: qsTr("Clipboard")
                    color: window.textColor
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: settingsRow.implicitHeight + 36

                    RowLayout {
                        id: settingsRow
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Popup history size")
                                color: window.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Sets the number of entries in the shortcut popup. Text copies are stored locally.")
                                color: window.mutedColor
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                        }

                        SpinBox {
                            id: historySize
                            Layout.preferredWidth: 126
                            Accessible.name: qsTr("Popup history size")
                            from: 1
                            to: 2147483647
                            editable: true
                            value: clipboardModule.historyNumber
                            onValueModified: clipboardModule.historyNumber = value

                            background: Rectangle {
                                implicitHeight: 40
                                radius: height / 2
                                color: "#232323"
                                border.color: historySize.activeFocus ? window.accentColor : window.borderColor
                            }

                            up.indicator: Label {
                                x: historySize.width - width - 4
                                y: 4
                                width: 32
                                height: historySize.height - 8
                                text: "+"
                                color: window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                background: Rectangle {
                                    radius: height / 2
                                    color: historySize.up.pressed ? "#545454" : "#383838"
                                }
                            }

                            down.indicator: Label {
                                x: 4
                                y: 4
                                width: 32
                                height: historySize.height - 8
                                text: "−"
                                color: historySize.value > historySize.from ? window.textColor : window.mutedColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                background: Rectangle {
                                    radius: height / 2
                                    color: historySize.down.pressed ? "#545454" : "#383838"
                                }
                            }
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: shortcutRow.implicitHeight + 28

                    RowLayout {
                        id: shortcutRow
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Clipboard history shortcut")
                                color: window.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: shortcutButton.recording ? qsTr("Press a key combination. Esc cancels.")
                                      : shortcutButton.message
                                color: shortcutButton.recording ? window.mutedColor : "#ffb4ab"
                                wrapMode: Text.WordWrap
                            }
                        }

                        Button {
                            id: shortcutButton
                            property bool recording: false
                            property string message: ""
                            Layout.rightMargin: 8
                            padding: 3
                            hoverEnabled: true
                            enabled: desktopShortcut.canChangeShortcut
                            text: recording ? qsTr("Recording…") : desktopShortcut.shortcut
                            Accessible.name: qsTr("Choose clipboard history shortcut: %1").arg(text)
                            ToolTip.visible: hovered && !recording
                            ToolTip.text: qsTr("Choose shortcut")

                            background: Rectangle {
                                radius: 7
                                color: shortcutButton.recording || shortcutButton.hovered ? "#353535" : "transparent"
                                border.color: window.accentColor
                                border.width: shortcutButton.visualFocus ? 1 : 0
                                Behavior on color { ColorAnimation { duration: 140 } }
                            }

                            contentItem: Row {
                                spacing: 6

                                Repeater {
                                    model: shortcutButton.recording ? [shortcutButton.text]
                                                                   : shortcutButton.text.split("+")

                                    Row {
                                        required property int index
                                        required property string modelData
                                        spacing: 6

                                        Label {
                                            visible: index > 0
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: "+"
                                            color: window.mutedColor
                                            font.pixelSize: 12
                                        }

                                        Rectangle {
                                            width: Math.max(28, keyLabel.implicitWidth + 16)
                                            height: 28
                                            radius: 5
                                            color: shortcutButton.down ? "#444444" : "#303030"
                                            border.color: "#5d5d5d"

                                            Label {
                                                id: keyLabel
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: window.textColor
                                                font.pixelSize: 12
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }
                                }
                            }

                            onClicked: {
                                message = ""
                                recording = true
                                forceActiveFocus()
                            }
                            onActiveFocusChanged: {
                                if (!activeFocus)
                                    recording = false
                            }
                            Keys.onPressed: event => {
                                if (!recording)
                                    return
                                event.accepted = true
                                if (event.isAutoRepeat || event.key === Qt.Key_Control || event.key === Qt.Key_Alt
                                        || event.key === Qt.Key_Shift || event.key === Qt.Key_Meta
                                        || event.key === Qt.Key_Super_L || event.key === Qt.Key_Super_R)
                                    return
                                recording = false
                                if (event.key !== Qt.Key_Escape)
                                    message = desktopShortcut.recordShortcut(event.key, event.modifiers)
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: clipboardModule.storageError || desktopShortcut.error
                    color: "#ffb4ab"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.topMargin: 8
                    text: qsTr("History")
                    color: window.textColor
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    GridView {
                        id: history
                        anchors.fill: parent
                        readonly property int columns: 3
                        readonly property int cardGap: 8
                        cellWidth: Math.floor(width / columns)
                        cellHeight: 108
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: clipboardModule

                        add: Transition {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
                            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 200; easing.type: Easing.OutCubic }
                        }

                        remove: Transition {
                            NumberAnimation { property: "opacity"; to: 0; duration: 160 }
                            NumberAnimation { property: "scale"; to: 0.96; duration: 160; easing.type: Easing.InCubic }
                        }

                        displaced: Transition {
                            NumberAnimation { properties: "x,y"; duration: 220; easing.type: Easing.OutCubic }
                        }

                        ScrollBar.vertical: ScrollBar {}

                        delegate: Panel {
                            id: entryCard
                            required property int index
                            required property string clipboardText
                            required property date copiedAt
                            readonly property bool isCurrent: index === 0
                                                              && clipboardText === clipboardModule.currentText

                            readonly property bool lastColumn: index % history.columns === history.columns - 1

                            width: lastColumn ? history.width - history.cellWidth * (history.columns - 1)
                                              : history.cellWidth - history.cardGap
                            height: GridView.view.cellHeight - 8
                            color: isCurrent ? "#343434" : window.panelColor
                            border.color: isCurrent || cardHover.hovered ? "#828282" : window.borderColor
                            Behavior on border.color { ColorAnimation { duration: 210; easing.type: Easing.OutCubic } }

                            HoverHandler { id: cardHover }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: clipboardText
                                    textFormat: Text.PlainText
                                    color: window.textColor
                                    font.pixelSize: 14
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                }

                                Item { Layout.fillHeight: true }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Label {
                                        Layout.fillWidth: true
                                        text: entryCard.isCurrent ? qsTr("Current clipboard")
                                                                 : copiedAt.toLocaleString(Qt.locale(), Locale.ShortFormat)
                                        color: entryCard.isCurrent ? window.textColor : window.mutedColor
                                        font.pixelSize: 11
                                        font.weight: entryCard.isCurrent ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideRight
                                    }

                                    HistoryAction {
                                        id: copyAction
                                        activeFocusOnTab: true
                                        opacity: cardHover.hovered || activeFocus || deleteAction.activeFocus ? 1 : 0
                                        Behavior on opacity { NumberAnimation { duration: 210; easing.type: Easing.OutCubic } }
                                        Accessible.name: qsTr("Copy to clipboard")
                                        onClicked: clipboardModule.copyEntry(entryCard.index)
                                    }

                                    HistoryAction {
                                        id: deleteAction
                                        activeFocusOnTab: true
                                        opacity: cardHover.hovered || activeFocus || copyAction.activeFocus ? 1 : 0
                                        Behavior on opacity { NumberAnimation { duration: 210; easing.type: Easing.OutCubic } }
                                        removesEntry: true
                                        Accessible.name: qsTr("Delete entry")
                                        onClicked: clipboardModule.deleteEntry(entryCard.index)
                                    }
                                }
                            }
                        }

                        footer: Item {
                            width: history.width
                            height: visible ? 49 : 0
                            visible: clipboardModule.hasOlderEntries

                            Button {
                                anchors.centerIn: parent
                                text: qsTr("Load older entries")
                                onClicked: clipboardModule.loadOlderEntries()
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: history.count === 0 && !clipboardModule.storageError
                        text: qsTr("Copy some text to see it here.")
                        color: window.mutedColor
                        font.pixelSize: 14
                    }
                }
            }
        }
    }

    Repeater {
        model: [
            { left: true, cursor: Qt.SizeHorCursor, edges: Qt.LeftEdge },
            { right: true, cursor: Qt.SizeHorCursor, edges: Qt.RightEdge },
            { top: true, cursor: Qt.SizeVerCursor, edges: Qt.TopEdge },
            { bottom: true, cursor: Qt.SizeVerCursor, edges: Qt.BottomEdge },
            { top: true, left: true, cursor: Qt.SizeFDiagCursor, edges: Qt.TopEdge | Qt.LeftEdge },
            { top: true, right: true, cursor: Qt.SizeBDiagCursor, edges: Qt.TopEdge | Qt.RightEdge },
            { bottom: true, left: true, cursor: Qt.SizeBDiagCursor, edges: Qt.BottomEdge | Qt.LeftEdge },
            { bottom: true, right: true, cursor: Qt.SizeFDiagCursor, edges: Qt.BottomEdge | Qt.RightEdge }
        ]

        MouseArea {
            required property var modelData
            readonly property int grip: 5

            visible: !window.maximized
            x: modelData.right ? window.width - grip : 0
            y: modelData.bottom ? window.height - grip : 0
            width: modelData.left || modelData.right ? grip : window.width
            height: modelData.top || modelData.bottom ? grip : window.height
            cursorShape: modelData.cursor
            onPressed: windowControls.resize(window, modelData.edges)
        }
    }
}
