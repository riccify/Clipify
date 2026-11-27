import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window

    width: 1040
    height: Math.min(860, Screen.desktopAvailableHeight)
    minimumWidth: 680
    minimumHeight: 500
    visible: !startInTray
    title: "Clipify"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint
           | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint
    color: "#1b1b1b"

    readonly property color barColor: "#171717"
    readonly property color panelColor: "#2a2a2a"
    readonly property color borderColor: "#3a3a3a"
    readonly property color chromeDividerColor: "#303030"
    readonly property color textColor: "#f2f2f2"
    readonly property color mutedColor: "#b5b5b5"
    readonly property color accentColor: "#d6d6d6"
    readonly property int sidebarButtonRadius: 10
    readonly property bool maximized: visibility === Window.Maximized
    property string currentModule: "clipboard"
    onCurrentModuleChanged: {
        if (currentModule === "screen")
            screenCapture.refreshGallery()
    }

    MessageBox {
        id: factoryResetMessage
        parent: Overlay.overlay

        function showConfirmation() {
            showMessage(qsTr("Restore factory settings"), MessageBox.Warning, [
                { buttonLabel: qsTr("Cancel"), buttonIcon: "cancel" },
                { buttonLabel: qsTr("Restore settings"), buttonIcon: "reset" }
            ], qsTr("Reset all Clipify settings and switch to the local config? Your notes, screenshots, clipboard history, and external config files will be kept."))
        }

        onButtonChosen: index => {
            if (index === 1)
                appConfig.resetToDefaults()
        }
    }

    Connections {
        target: appConfig
        function onResetConfirmationRequested() { factoryResetMessage.showConfirmation() }
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

    component ModuleButton: Button {
        required property string module
        id: clipboardButton
        x: 13
        width: parent.width - 26
        height: 42
        leftPadding: 13
        highlighted: window.currentModule === module
        onClicked: window.currentModule = module

        CursorTooltip { text: qsTr("Open %1").arg(clipboardButton.text) }

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

            Rectangle {
                visible: clipboardButton.module === "about"
                opacity: releaseCheck.updateAvailable ? 1 : 0
                Layout.rightMargin: 7
                implicitWidth: 17
                implicitHeight: 17
                radius: 8.5
                color: "#e7c75c"
                Accessible.name: qsTr("Update available")
                Accessible.ignored: opacity === 0
                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                Label {
                    anchors.centerIn: parent
                    text: "!"
                    color: "#231c0a"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }

        background: Rectangle {
            radius: window.sidebarButtonRadius
            color: clipboardButton.down ? "#bfbfbf"
                 : clipboardButton.hovered ? (clipboardButton.highlighted ? "#e3e3e3" : "#303030")
                 : clipboardButton.highlighted ? window.accentColor : "transparent"
            border.color: clipboardButton.visualFocus ? window.accentColor : window.borderColor
            border.width: clipboardButton.highlighted || clipboardButton.visualFocus ? 1 : 0
            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    component AboutAction: Button {
        id: action
        hoverEnabled: true
        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: action.down ? "#505050" : action.hovered ? "#454545" : "#353535"
            border.color: action.visualFocus ? window.accentColor : window.borderColor
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    component ConfigAction: AboutAction {
        id: configAction
        required property string iconSymbol

        contentItem: RowLayout {
            spacing: 7

            LineIcon {
                symbol: configAction.iconSymbol
                ink: window.textColor
            }

            Label {
                text: configAction.text
                color: window.textColor
                font: configAction.font
            }
        }
    }

    component FolderSetting: Panel {
        id: folderSetting
        required property string folder
        signal chooseRequested()
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
                    text: folderSetting.folder
                    textFormat: Text.PlainText
                    color: window.mutedColor
                    elide: Text.ElideMiddle
                    CursorTooltip {
                        text: parent.text
                        available: parent.truncated
                    }
                }
            }

            Button {
                id: chooseFolderButton
                text: qsTr("Choose folder…")
                CursorTooltip { text: qsTr("Choose a folder for newly saved files.") }
                hoverEnabled: true
                onClicked: folderSetting.chooseRequested()

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

    component ShortcutSetting: Panel {
        required property var shortcutController
        required property string heading
        property bool grouped: false
        Layout.fillWidth: true
        implicitHeight: shortcutRow.implicitHeight + (grouped ? 20 : 28)
        color: grouped ? "transparent" : window.panelColor
        border.width: grouped ? 0 : 1

        RowLayout {
            id: shortcutRow
            anchors.fill: parent
            anchors.margins: 14
            anchors.topMargin: grouped ? 10 : 14
            anchors.bottomMargin: grouped ? 10 : 14
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
                CursorTooltip {
                    text: shortcutController.error || qsTr("Choose shortcut")
                    available: !parent.recording
                }

                background: Rectangle {
                    radius: 7
                    color: shortcutButton.recording || shortcutButton.hovered ? "#353535" : "transparent"
                    border.color: window.accentColor
                    border.width: shortcutButton.visualFocus ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 140 } }
                }

                contentItem: ShortcutKeys {
                    keys: shortcutButton.recording ? [shortcutButton.text] : shortcutButton.text.split("+")
                    pressed: shortcutButton.down
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

        CursorTooltip {
            text: parent.Accessible.name
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: window.barColor

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: window.chromeDividerColor
            }

            RowLayout {
                id: sidebarBrand
                anchors.top: parent.top
                anchors.topMargin: 16
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Image {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    source: "qrc:/clipify/assets/clipify-app-icon.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                Label {
                    text: "Clipify"
                    color: window.textColor
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }

            ModuleButton {
                id: clipboardNavButton
                anchors.top: sidebarBrand.bottom
                anchors.topMargin: 24
                module: "clipboard"
                text: qsTr("Clipboard")
            }

            ModuleButton {
                id: screenNavButton
                anchors.top: clipboardNavButton.bottom
                anchors.topMargin: 6
                module: "screen"
                text: qsTr("Screen")
            }

            ModuleButton {
                id: notesNavButton
                anchors.top: screenNavButton.bottom
                anchors.topMargin: 6
                module: "notes"
                text: qsTr("Quick Notes")
            }

            ModuleButton {
                id: converterNavButton
                anchors.top: notesNavButton.bottom
                anchors.topMargin: 6
                module: "converter"
                text: qsTr("Converter")
            }

            ModuleButton {
                id: commandsNavButton
                anchors.top: converterNavButton.bottom
                anchors.topMargin: 6
                module: "commands"
                text: qsTr("Commands")
            }

            ModuleButton {
                anchors.top: commandsNavButton.bottom
                anchors.topMargin: 6
                module: "whisper"
                text: qsTr("Whisper")
            }

            ModuleButton {
                anchors.bottom: aboutNavButton.top
                anchors.bottomMargin: 5
                module: "settings"
                text: qsTr("Settings")
            }

            ModuleButton {
                id: aboutNavButton
                anchors.bottom: exitButton.top
                anchors.bottomMargin: 5
                module: "about"
                text: qsTr("About")
            }

            Button {
                id: exitButton
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 13
                height: 42
                text: qsTr("Exit")
                CursorTooltip { text: qsTr("Quit Clipify and stop its background tools.") }
                    enabled: !releaseCheck.busy
                leftPadding: 13
                hoverEnabled: true
                onClicked: windowControls.quit()

                contentItem: RowLayout {
                    spacing: 8
                    LineIcon {
                        symbol: "exit"
                        ink: exitButton.hovered ? window.textColor : window.mutedColor
                            Behavior on ink { ColorAnimation { duration: 190 } }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: exitButton.text
                        color: exitButton.hovered ? window.textColor : window.mutedColor
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                            Behavior on color { ColorAnimation { duration: 190 } }
                    }
                }

                background: Rectangle {
                    radius: window.sidebarButtonRadius
                    color: exitButton.down ? "#414141" : exitButton.hovered ? "#303030" : "transparent"
                    border.color: window.accentColor
                    border.width: exitButton.visualFocus ? 1 : 0
                    Behavior on color { ColorAnimation { duration: 190; easing.type: Easing.OutCubic } }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                id: topBar
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: window.barColor

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: window.chromeDividerColor
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

                Label {
                    anchors.centerIn: parent
                    width: Math.max(0, parent.width - 2 * (windowButtons.width + 12))
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: window.currentModule === "screen" ? qsTr("Screen")
                        : window.currentModule === "notes" ? qsTr("Quick Notes")
                        : window.currentModule === "converter" ? qsTr("Converter")
                        : window.currentModule === "commands" ? qsTr("Command Shortcuts")
                        : window.currentModule === "whisper" ? qsTr("Whisper")
                        : window.currentModule === "settings" ? qsTr("Settings")
                        : window.currentModule === "about" ? qsTr("About") : qsTr("Clipboard")
                    color: window.textColor
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    onTextChanged: titleTransition.restart()

                    NumberAnimation on opacity {
                        id: titleTransition
                        running: false
                        from: 0
                        to: 1
                        duration: 160
                        easing.type: Easing.OutCubic
                    }
                }

                RowLayout {
                    id: windowButtons
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 7
                    anchors.topMargin: 6
                    anchors.bottomMargin: 5
                    spacing: 4

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

            ModulePage {
                module: "whisper"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 12

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: whisperShortcuts.implicitHeight

                    ColumnLayout {
                        id: whisperShortcuts
                        anchors.fill: parent
                        spacing: 0

                        ShortcutSetting {
                            grouped: true
                            heading: qsTr("Start or stop dictation")
                            shortcutController: whisperShortcut
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            implicitHeight: 1
                            color: window.borderColor
                        }

                        ShortcutSetting {
                            grouped: true
                            heading: qsTr("Pause or resume dictation history")
                            shortcutController: pauseDictationShortcut
                        }
                    }
                }

                WhisperView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onLlmSettingsRequested: window.currentModule = "settings"
                }
            }

            ModulePage {
                module: "commands"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22

                CommandShortcutsView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            ModulePage {
                module: "converter"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22

                ConverterView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            ModulePage {
                module: "notes"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14

                ShortcutSetting {
                    heading: qsTr("Open Quick Note")
                    shortcutController: notesShortcut
                }

                FolderSetting {
                    folder: quickNotes.defaultFolder
                    onChooseRequested: quickNotes.chooseDefaultFolder()
                }

                Label {
                    text: qsTr("Saved notes")
                    color: window.textColor
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                }

                QuickNotesView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            ModulePage {
                module: "settings"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14

                ScrollView {
                    id: settingsScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: settingsScroll.availableWidth
                        spacing: 14

                        Panel {
                            Layout.fillWidth: true
                            implicitHeight: llmSettings.implicitHeight + 32

                            LlmSettings {
                                id: llmSettings
                                anchors.fill: parent
                                anchors.margins: 16
                            }
                        }

                        Panel {
                            Layout.fillWidth: true
                            implicitHeight: autoStartRow.implicitHeight + 20

                            RowLayout {
                                id: autoStartRow
                                anchors.fill: parent
                                anchors.margins: 10
                                anchors.leftMargin: 14
                                anchors.rightMargin: 20

                                spacing: 8

                                LineIcon { symbol: "fast" }

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Auto Start")
                                    color: window.textColor
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }

                                CheckBox {
                                    Accessible.name: qsTr("Auto Start")
                                    CursorTooltip { text: qsTr("Start Clipify in the tray when you sign in.") }
                                    checked: appConfig.autoStart
                                    nextCheckState: function() {
                                        appConfig.setAutoStart(!appConfig.autoStart)
                                        return appConfig.autoStart ? Qt.Checked : Qt.Unchecked
                                    }
                                }
                            }
                        }

                        Panel {
                            Layout.fillWidth: true
                            implicitHeight: activeConfigRow.implicitHeight + 20

                            RowLayout {
                                id: activeConfigRow
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
                                        text: qsTr("Active config")
                                        color: window.textColor
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: appConfig.activePath
                                        textFormat: Text.PlainText
                                        color: window.mutedColor
                                        elide: Text.ElideMiddle
                                        CursorTooltip {
                                            text: parent.text
                                            available: parent.truncated
                                        }
                                    }
                                }

                                AboutAction {
                                    id: openConfigFolderButton
                                    Layout.preferredWidth: 36
                                    Accessible.name: qsTr("Open config folder")
                                    CursorTooltip {
                                        text: parent.Accessible.name
                                        delay: 500
                                    }
                                    onClicked: appConfig.openConfigFolder()

                                    contentItem: Item {
                                        LineIcon {
                                            anchors.centerIn: parent
                                            symbol: "folder"
                                            ink: window.textColor
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ConfigAction {
                        iconSymbol: "reset"
                        text: qsTr("Restore factory settings")
                        CursorTooltip { text: qsTr("Reset preferences and make the local config active.") }
                        onClicked: factoryResetMessage.showConfirmation()
                    }

                    Label {
                        id: configMessage
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: appConfig.message
                        textFormat: Text.PlainText
                        color: appConfig.messageError ? "#ffb4ab" : "#a6e3b2"
                        elide: Text.ElideRight
                        opacity: 0
                        Accessible.ignored: opacity === 0

                        function showMessage() {
                            opacity = text.length > 0 ? 1 : 0
                            messageTimeout.restart()
                        }

                        Behavior on opacity {
                            id: messageFade
                            NumberAnimation {
                                duration: messageFade.targetValue === 0 ? 550 : 160
                                easing.type: Easing.OutCubic
                            }
                        }

                        Timer {
                            id: messageTimeout
                            interval: 3000
                            onTriggered: configMessage.opacity = 0
                        }

                        Connections {
                            target: appConfig
                            function onMessageChanged() { configMessage.showMessage() }
                        }

                        Component.onCompleted: showMessage()
                    }

                    ConfigAction {
                        iconSymbol: "folder"
                        text: qsTr("Load Config")
                        CursorTooltip { text: qsTr("Choose a JSON configuration to use in Clipify.") }
                        onClicked: appConfig.loadConfig()
                    }

                    ConfigAction {
                        iconSymbol: "save"
                        text: qsTr("Save Config")
                        CursorTooltip { text: qsTr("Export a copy of your current settings.") }
                        onClicked: appConfig.saveConfig()
                    }
                }
            }

            ModulePage {
                module: "about"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: aboutDetails.implicitHeight + 28

                    ColumnLayout {
                        id: aboutDetails
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Label {
                            text: qsTr("Developer: riccify")
                            color: window.textColor
                        }
                        Label {
                            text: qsTr("Version %1").arg(Qt.application.version)
                            color: window.mutedColor
                        }
                        AboutAction {
                            text: qsTr("GitHub · riccify/Clipify")
                            CursorTooltip { text: qsTr("Open the Clipify repository in your browser.") }
                            onClicked: Qt.openUrlExternally("https://github.com/riccify/Clipify")
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: updateRow.implicitHeight + 28

                    RowLayout {
                        id: updateRow
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: qsTr("Updates")
                                color: window.textColor
                                font.weight: Font.DemiBold
                            }
                            Label {
                                Layout.fillWidth: true
                                text: releaseCheck.status
                                color: releaseCheck.updateAvailable ? "#e7c75c" : window.mutedColor
                                wrapMode: Text.WordWrap
                                Behavior on color { ColorAnimation { duration: 160 } }
                            }
                            RowLayout {
                                AboutAction {
                                    text: qsTr("Check again")
                                    CursorTooltip { text: qsTr("Check GitHub for a newer Clipify release.") }
                                    enabled: !releaseCheck.checking && !releaseCheck.busy
                                    onClicked: releaseCheck.check()
                                }
                                AboutAction {
                                    text: releaseCheck.canInstall ? qsTr("Download update") : qsTr("Open download")
                                    enabled: releaseCheck.updateAvailable && !releaseCheck.busy && !releaseCheck.checking
                                    opacity: releaseCheck.updateAvailable ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                                    onClicked: releaseCheck.openUpdate()
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                visible: opacity > 0
                                opacity: releaseCheck.busy ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                                Label {
                                    text: qsTr("Download")
                                    color: window.textColor
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    value: releaseCheck.downloadProgress
                                    Behavior on value { NumberAnimation { duration: 100 } }
                                }
                                Label {
                                    text: qsTr("Install")
                                    color: window.textColor
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    value: Math.max(0, releaseCheck.installProgress)
                                    indeterminate: releaseCheck.installing && releaseCheck.installProgress < 0
                                    Behavior on value { NumberAnimation { duration: 100 } }
                                }
                            }
                        }

                        Item {
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 60

                            Label {
                                anchors.centerIn: parent
                                text: "✓"
                                font.pixelSize: 42
                                color: "#ffffff"
                                opacity: releaseCheck.upToDate ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            }

                            Label {
                                anchors.centerIn: parent
                                text: "!"
                                font.pixelSize: 42
                                font.weight: Font.Bold
                                color: "#e7c75c"
                                opacity: releaseCheck.updateAvailable ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            ModulePage {
                module: "clipboard"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 22
                spacing: 14

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: historySettings.implicitHeight

                    ColumnLayout {
                        id: historySettings
                        anchors.fill: parent
                        spacing: 0

                        Panel {
                            Layout.fillWidth: true
                            implicitHeight: settingsRow.implicitHeight + 24
                            color: "transparent"
                            border.width: 0

                            RowLayout {
                                id: settingsRow
                                anchors.fill: parent
                                anchors.margins: 12
                                anchors.rightMargin: 18
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

                                HistorySizeInput {
                                    Accessible.name: qsTr("Popup history size")
                                    CursorTooltip { text: qsTr("Set how many entries appear in the clipboard popup.") }
                                    from: 1
                                    value: clipboardModule.historyNumber
                                    onValueModified: clipboardModule.historyNumber = value
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 18
                            Layout.topMargin: 5
                            Layout.bottomMargin: 5
                            spacing: 16

                            LineIcon { symbol: "history" }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("History size")
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                CursorTooltip {
                                    text: qsTr("Maximum saved entries. ∞ keeps all entries.")
                                }
                            }

                            HistorySizeInput {
                                Accessible.name: qsTr("History size")
                                CursorTooltip { text: qsTr("Limit the number of saved clipboard entries. ∞ keeps all entries.") }
                                from: 0
                                value: clipboardModule.historyLimit
                                onValueModified: clipboardModule.historyLimit = value
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 18
                            spacing: 16

                            LineIcon { symbol: "password-key" }

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Skip sensitive information")
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                CursorTooltip {
                                    text: qsTr("Skips copies marked as secret and recognizable credentials.")
                                }
                            }

                            CheckBox {
                                Accessible.name: qsTr("Skip sensitive information")
                                CursorTooltip { text: qsTr("Keep marked secrets and recognizable credentials out of the history.") }
                                checked: clipboardModule.skipSensitive
                                nextCheckState: function() {
                                    clipboardModule.skipSensitive = !clipboardModule.skipSensitive
                                    return clipboardModule.skipSensitive ? Qt.Checked : Qt.Unchecked
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            implicitHeight: 1
                            color: window.borderColor
                        }

                        ShortcutSetting {
                            grouped: true
                            shortcutController: desktopShortcut
                            heading: qsTr("Open clipboard history")
                        }

                        ShortcutSetting {
                            grouped: true
                            shortcutController: pauseHistoryShortcut
                            heading: qsTr("Pause or resume history")
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: ocrSettings.implicitHeight

                    ColumnLayout {
                        id: ocrSettings
                        anchors.fill: parent
                        spacing: 0

                        ShortcutSetting {
                            grouped: true
                            shortcutController: ocrShortcut
                            heading: qsTr("Copy text from screenshot")
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            Layout.bottomMargin: 12
                            spacing: 10
                            OcrModeButton { text: qsTr("Fast"); accurate: false }
                            OcrModeButton { text: qsTr("Accurate"); accurate: true }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: [clipboardModule.storageError, desktopShortcut.error, pauseHistoryShortcut.error, ocrShortcut.error, gnomeSetupWarning]
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

                HistoryToolbar {
                    id: clipboardHistoryToolbar
                    Layout.topMargin: 8
                    controller: clipboardModule
                    title: qsTr("History")
                    clearTitle: qsTr("Clear clipboard history?")
                    clearMessage: qsTr("All saved clipboard entries will be deleted.")
                    onSearchApplied: history.positionViewAtBeginning()
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
                                CursorTooltip { text: qsTr("Load the next page of clipboard history.") }
                                onClicked: clipboardModule.loadOlderEntries()
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: history.count === 0 && !clipboardModule.storageError
                        text: clipboardHistoryToolbar.searchText.trim().length > 0 ? qsTr("No matching entries.")
                                                                     : qsTr("Copy text or an image to see it here.")
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

                Panel {
                    Layout.fillWidth: true
                    implicitHeight: captureSettings.implicitHeight

                    ColumnLayout {
                        id: captureSettings
                        anchors.fill: parent
                        spacing: 0

                        ShortcutSetting {
                            grouped: true
                            shortcutController: screenShortcut
                            heading: qsTr("Take screenshot interactively")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            implicitHeight: 1
                            color: window.borderColor
                        }

                        ShortcutSetting {
                            grouped: true
                            shortcutController: fullScreenShortcut
                            heading: qsTr("Take screenshot")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.leftMargin: 14
                            Layout.rightMargin: 14
                            implicitHeight: 1
                            color: window.borderColor
                        }

                        ShortcutSetting {
                            grouped: true
                            shortcutController: focusedWindowShortcut
                            heading: qsTr("Take a screenshot of focused window")
                        }
                    }
                }

                FolderSetting {
                    folder: screenCapture.saveFolder
                    onChooseRequested: screenCapture.chooseSaveFolder()
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
