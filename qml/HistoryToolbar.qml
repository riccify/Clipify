import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: toolbar
    required property var controller
    required property string title
    required property string clearTitle
    required property string clearMessage
    readonly property string searchText: historySearch.text
    signal searchApplied()

    Timer {
        id: historySearchDelay
        interval: 90
        onTriggered: {
            toolbar.controller.setHistorySearch(toolbar.searchText)
            toolbar.searchApplied()
        }
    }

    Layout.fillWidth: true
    spacing: 8

    component HistorySegmentBackground: Rectangle {
        required property Button control
        required property bool leading
        implicitHeight: 34
        radius: 7
        color: control.down ? "#505050" : control.hovered ? "#454545" : "#353535"
        Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.left: parent.leading ? undefined : parent.left
            anchors.right: parent.leading ? parent.right : undefined
            width: parent.radius
            height: parent.height
            color: parent.color
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 4
            color: "transparent"
            border.color: "#d6d6d6"
            border.width: parent.control.visualFocus ? 1 : 0
        }
    }

    MessageBox {
        id: clearHistoryMessage
        parent: Overlay.overlay
        onButtonChosen: index => {
            if (index === 1)
                toolbar.controller.clearHistory()
        }
    }

    component AboutAction: Button {
        id: control
        hoverEnabled: true
        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: control.down ? "#505050" : control.hovered ? "#454545" : "#353535"
            border.color: control.visualFocus ? "#d6d6d6" : "#3a3a3a"
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    Label {
        text: toolbar.title
        color: "#f2f2f2"
        font.pixelSize: 19
        font.weight: Font.DemiBold
    }

    Item { Layout.fillWidth: true }

    AppTextField {
        id: historySearch
        Layout.preferredWidth: 210
        Layout.minimumWidth: 92
        Accessible.name: qsTr("Search history")
        CursorTooltip { text: qsTr("Filter saved entries as you type.") }
        placeholderText: qsTr("Search history")
        leftPadding: 32
        onTextChanged: historySearchDelay.restart()

        background: Rectangle {
            radius: 7
            color: "#232323"
            border.color: historySearch.activeFocus ? "#d6d6d6" : "#3a3a3a"
            Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }

        LineIcon {
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            symbol: "search"
        }
    }

    RowLayout {
        spacing: 0
        Layout.preferredHeight: 34
        Layout.maximumHeight: 34

        AboutAction {
            id: pauseToggle
            Layout.fillHeight: true
            Layout.preferredWidth: 142
            leftPadding: 8
            rightPadding: 8
            text: toolbar.controller.paused ? qsTr("Resume history") : qsTr("Pause history")
            CursorTooltip { text: qsTr("Toggle saving new history entries.") }
            onClicked: {
                if (toolbar.controller.paused)
                    toolbar.controller.resumeHistory()
                else
                    toolbar.controller.pauseHistory(0)
            }

            background: HistorySegmentBackground {
                leading: true
                control: pauseToggle
            }

            contentItem: Item {
                implicitWidth: pauseContent.implicitWidth
                implicitHeight: pauseContent.implicitHeight

                RowLayout {
                    id: pauseContent
                    anchors.centerIn: parent
                    spacing: 5

                    LineIcon {
                        symbol: toolbar.controller.paused ? "resume" : "pause"
                        ink: "#f2f2f2"
                    }

                    Label {
                        text: pauseToggle.text
                        color: "#f2f2f2"
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: "#505050"
        }

        Dropdown {
            id: pauseDuration
            Layout.preferredWidth: 36
            Layout.fillHeight: true
            Accessible.name: qsTr("Choose pause duration")
            CursorTooltip {
                text: parent.Accessible.name
                available: !parent.popupVisible
                delay: 500
            }
            options: [
                { label: qsTr("30 seconds"), value: "30" },
                { label: qsTr("1 minute"), value: "60" },
                { label: qsTr("10 minutes"), value: "600" },
                { label: qsTr("30 minutes"), value: "1800" },
                { label: qsTr("Until resumed"), value: "0" }
            ]
            selectedValue: ""
            onValueSelected: value => toolbar.controller.pauseHistory(Number(value))

            background: HistorySegmentBackground {
                leading: false
                control: pauseDuration
            }

            contentItem: Item {
                LineIcon {
                    anchors.centerIn: parent
                    symbol: "chevron-down"
                    ink: "#f2f2f2"
                    rotation: pauseDuration.popupVisible ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                }
            }
        }
    }

    AboutAction {
        id: clearHistoryButton
        text: qsTr("Clear history")
        CursorTooltip { text: qsTr("Delete all saved history entries.") }

        contentItem: RowLayout {
            spacing: 5
            LineIcon { symbol: "trash"; ink: "#f2f2f2" }
            Label { text: clearHistoryButton.text; color: "#f2f2f2" }
        }

        onClicked: clearHistoryMessage.showMessage(toolbar.clearTitle, MessageBox.Warning, [
            { buttonLabel: qsTr("Cancel"), buttonIcon: "cancel" },
            { buttonLabel: qsTr("Clear history"), buttonIcon: "trash" }
        ], toolbar.clearMessage)
    }
}
