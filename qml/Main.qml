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
    visible: !startInTray
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
    onCurrentModuleChanged: {
        if (currentModule === "screen")
            screenCapture.refreshGallery()
    }

    onClosing: event => {
        if (windowControls.trayAvailable()) {
            event.accepted = false
            window.hide()
        } else {
            windowControls.quit()
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

    component LineIcon: Canvas {
        required property string symbol
        property color ink: window.mutedColor
        implicitWidth: 18
        implicitHeight: 18
        Layout.alignment: Qt.AlignVCenter

        onInkChanged: requestPaint()
        onSymbolChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = ink
            ctx.lineWidth = 1.35
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()

            switch (symbol) {
            case "clipboard":
                ctx.moveTo(6, 4)
                ctx.lineTo(3.5, 4)
                ctx.lineTo(3.5, 16)
                ctx.lineTo(14.5, 16)
                ctx.lineTo(14.5, 4)
                ctx.lineTo(12, 4)
                ctx.rect(6, 2, 6, 4)
                ctx.moveTo(6.5, 9)
                ctx.lineTo(11.5, 9)
                ctx.moveTo(6.5, 12)
                ctx.lineTo(11.5, 12)
                break
            case "screen":
                ctx.rect(2, 3, 14, 10)
                ctx.moveTo(9, 13)
                ctx.lineTo(9, 16)
                ctx.moveTo(6, 16)
                ctx.lineTo(12, 16)
                break
            case "history":
                ctx.arc(9, 9, 6.5, Math.PI, Math.PI * 2.85)
                ctx.moveTo(2.5, 6)
                ctx.lineTo(2.5, 9)
                ctx.lineTo(5.5, 9)
                ctx.moveTo(9, 5.5)
                ctx.lineTo(9, 9)
                ctx.lineTo(11.5, 10.5)
                break
            case "keys":
                ctx.rect(2, 4.5, 14, 9)
                for (let x = 5; x <= 13; x += 4) {
                    ctx.moveTo(x, 7.5)
                    ctx.lineTo(x + 0.5, 7.5)
                }
                ctx.moveTo(6, 10.5)
                ctx.lineTo(12, 10.5)
                break
            case "folder":
                ctx.moveTo(2, 5)
                ctx.lineTo(7, 5)
                ctx.lineTo(8.5, 7)
                ctx.lineTo(16, 7)
                ctx.lineTo(16, 15)
                ctx.lineTo(2, 15)
                ctx.closePath()
                break
            case "fast":
                ctx.moveTo(10, 1.5)
                ctx.lineTo(3.5, 10)
                ctx.lineTo(8, 10)
                ctx.lineTo(7.5, 16.5)
                ctx.lineTo(14.5, 7.5)
                ctx.lineTo(10, 7.5)
                ctx.closePath()
                break
            case "accurate":
                ctx.arc(9, 9, 6, 0, Math.PI * 2)
                ctx.moveTo(12, 9)
                ctx.arc(9, 9, 3, 0, Math.PI * 2)
                break
            }
            ctx.stroke()
        }
    }

    component ModuleButton: Button {
        required property string module
        id: clipboardButton
        x: 13
        width: parent.width - 26
        height: 42
        leftPadding: 13
        highlighted: window.currentModule === module
        onClicked: window.currentModule = module

        contentItem: RowLayout {
            spacing: 8

            LineIcon {
                symbol: clipboardButton.module
                ink: clipboardButton.highlighted ? "#202020" : window.textColor
            }

            Label {
                Layout.fillWidth: true
                text: clipboardButton.text
                color: clipboardButton.highlighted ? "#202020" : window.textColor
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
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

    component ShortcutSetting: Panel {
        required property var shortcutController
        required property string heading
        Layout.fillWidth: true
        implicitHeight: shortcutRow.implicitHeight + 28

        RowLayout {
            id: shortcutRow
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            LineIcon { symbol: "keys" }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: heading
                    color: window.textColor
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: shortcutButton.recording ? qsTr("Press a key or combination. Click again to cancel.")
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
                enabled: shortcutController.canChangeShortcut
                text: recording ? qsTr("Recording…") : (shortcutController.shortcut || qsTr("Choose shortcut"))
                Accessible.name: qsTr("Choose %1: %2").arg(heading).arg(text)
                ToolTip.visible: hovered && !recording
                ToolTip.text: shortcutController.error || qsTr("Choose shortcut")

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
                    recording = !recording
                    if (recording)
                        forceActiveFocus()
                }
                onActiveFocusChanged: {
                    if (!activeFocus)
                        recording = false
                }
                Keys.onShortcutOverride: event => {
                    if (recording)
                        event.accepted = true
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
                    message = shortcutController.recordShortcut(event.key, event.modifiers)
                }
            }
        }
    }

    component OcrModeButton: Button {
        id: mode
        required property bool accurate
        Layout.fillWidth: true
        padding: 9
        hoverEnabled: true
        readonly property bool selected: textRecognition.accurate === accurate
        Accessible.role: Accessible.RadioButton
        Accessible.checkable: true
        Accessible.checked: selected
        onClicked: textRecognition.accurate = accurate

        contentItem: RowLayout {
            spacing: 8

            LineIcon {
                symbol: mode.accurate ? "accurate" : "fast"
                ink: mode.selected ? window.textColor : window.mutedColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: mode.text
                    color: window.textColor
                    font.weight: Font.DemiBold
                }

                Label {
                    text: mode.accurate ? "tessdata_best" : "tessdata_fast"
                    color: window.mutedColor
                    font.pixelSize: 11
                }
            }

            Label {
                opacity: mode.selected ? 1 : 0
                text: "✓"
                color: window.accentColor
                Behavior on opacity {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
            }
        }

        background: Rectangle {
            implicitHeight: 48
            radius: 7
            color: mode.down ? "#4a4a4a" : mode.hovered ? "#3e3e3e"
                             : mode.selected ? "#353535" : "#262626"
            border.color: mode.visualFocus || mode.selected ? window.accentColor : window.borderColor
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    component ModulePage: ColumnLayout {
        id: page
        required property string module
        visible: window.currentModule === module
        transform: Translate { id: pageOffset }

        onVisibleChanged: {
            if (visible) {
                pageTransition.stop()
                pageOffset.x = module === "screen" ? 20 : -20
                opacity = 0
                pageTransition.start()
            }
        }

        ParallelAnimation {
            id: pageTransition
            NumberAnimation { target: pageOffset; property: "x"; to: 0; duration: 210; easing.type: Easing.OutCubic }
            NumberAnimation { target: page; property: "opacity"; to: 1; duration: 170 }
        }
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

                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 28
                            height: 28
                            source: "qrc:/clipify/assets/clipify-app-icon.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
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

                ModuleButton {
                    y: 13
                    module: "clipboard"
                    text: qsTr("Clipboard")
                }

                ModuleButton {
                    y: 61
                    module: "screen"
                    text: qsTr("Screen")
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

            ModulePage {
                module: "clipboard"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14

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
                        anchors.leftMargin: 14
                        spacing: 16

                        LineIcon { symbol: "history" }

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

                ShortcutSetting {
                    shortcutController: desktopShortcut
                    heading: qsTr("Open clipboard history")
                }

                ShortcutSetting {
                    shortcutController: ocrShortcut
                    heading: qsTr("Copy text from screenshot")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    OcrModeButton { text: qsTr("Fast"); accurate: false }
                    OcrModeButton { text: qsTr("Accurate"); accurate: true }
                }

                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: [clipboardModule.storageError, desktopShortcut.error, ocrShortcut.error, gnomeSetupWarning]
                          .filter(message => message.length > 0).join("\n")
                    color: "#ffb4ab"
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: textRecognition.status
                    color: window.mutedColor
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
                            required property bool isImage
                            required property string entryId
                            readonly property bool isCurrent: !isImage && index === 0
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

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 7

                                    ClipboardTypeIcon {
                                        image: entryCard.isImage
                                        Layout.alignment: Qt.AlignTop
                                    }

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
                                        id: previewAction
                                        visible: entryCard.isImage
                                        previewsEntry: true
                                        activeFocusOnTab: true
                                        opacity: cardHover.hovered || activeFocus || copyAction.activeFocus || deleteAction.activeFocus ? 1 : 0
                                        Behavior on opacity { NumberAnimation { duration: 210; easing.type: Easing.OutCubic } }
                                        Accessible.name: qsTr("Open image")
                                        onClicked: clipboardModule.previewEntry(entryCard.entryId)
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
                        text: qsTr("Copy text or an image to see it here.")
                        color: window.mutedColor
                        font.pixelSize: 14
                    }
                }
            }

            ModulePage {
                module: "screen"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14

                Label {
                    text: qsTr("Screen")
                    color: window.textColor
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }

                ShortcutSetting {
                    shortcutController: screenShortcut
                    heading: qsTr("Take screenshot interactively")
                }

                ShortcutSetting {
                    shortcutController: fullScreenShortcut
                    heading: qsTr("Take screenshot")
                }

                ShortcutSetting {
                    shortcutController: focusedWindowShortcut
                    heading: qsTr("Take a screenshot of focused window")
                }

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: folderRow.implicitHeight + 20

                    RowLayout {
                        id: folderRow
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.leftMargin: 14
                        anchors.rightMargin: 20
                        spacing: 12

                        LineIcon { symbol: "folder" }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: qsTr("Save folder")
                                color: window.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                text: screenCapture.saveFolder
                                textFormat: Text.PlainText
                                color: window.mutedColor
                                elide: Text.ElideMiddle
                                HoverHandler { id: folderHover }
                                ToolTip.visible: folderHover.hovered && truncated
                                ToolTip.text: text
                            }
                        }

                        Button {
                            id: chooseFolderButton
                            text: qsTr("Choose folder…")
                            hoverEnabled: true
                            onClicked: screenCapture.chooseSaveFolder()

                            background: Rectangle {
                                implicitWidth: 100
                                implicitHeight: 36
                                radius: 6
                                color: chooseFolderButton.down ? "#505050"
                                     : chooseFolderButton.hovered ? "#454545" : "#353535"
                                border.color: chooseFolderButton.visualFocus ? window.accentColor
                                            : chooseFolderButton.hovered ? "#707070" : "#515151"
                                Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                                Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            }
                        }
                    }
                }

                Label {
                    text: qsTr("Gallery")
                    color: window.textColor
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }

                ScreenshotGallery {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    screenshots: screenCapture.screenshots
                    onTrashRequested: url => screenCapture.trashScreenshot(url)
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
