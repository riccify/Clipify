import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: formatButton
    required property var options
    required property string selectedValue
    property bool searchable: false
    property string searchPlaceholder: qsTr("Search…")
    readonly property bool popupVisible: formatMenu.visible
    signal valueSelected(string value)

    hoverEnabled: true
    leftPadding: 12
    rightPadding: 10
    text: formatButton.options.find(format => format.value === formatButton.selectedValue)?.label ?? ""
    onClicked: {
        if (formatMenu.visible)
            formatMenu.close()
        else {
            formatSearch.clear()
            formatMenu.open()
        }
    }

    background: Rectangle {
        implicitHeight: 34
        radius: 6
        color: formatButton.down ? "#505050" : formatButton.hovered ? "#454545" : "#353535"
        border.color: formatButton.visualFocus ? "#d6d6d6" : "#3a3a3a"
        opacity: formatButton.enabled ? 1 : 0.5
        Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
    }

    contentItem: RowLayout {
        Label {
            Layout.fillWidth: true
            text: formatButton.text
            color: "#f2f2f2"
            font.weight: Font.DemiBold
        }

        Label {
            text: "⌄"
            color: "#c4c4c4"
            rotation: formatMenu.visible ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        }
    }

    Popup {
        id: formatMenu
        y: formatButton.height + 6
        width: 228
        padding: 8
        margins: 6
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        readonly property var matches: {
            const query = formatButton.searchable ? formatSearch.text.trim().toLowerCase() : ""
            return formatButton.options.filter(format => format.label.toLowerCase().includes(query)
                                                      || format.value.includes(query))
        }

        function selectFormat(index) {
            if (index < 0 || index >= matches.length)
                return
            formatButton.valueSelected(matches[index].value)
            close()
            formatButton.forceActiveFocus()
        }

        onOpened: focusInput()
        function focusInput() {
            if (formatButton.searchable)
                formatSearch.forceActiveFocus()
            else
                formatList.forceActiveFocus()
        }

        onAboutToShow: {
            formatList.currentIndex = Math.max(0, matches.findIndex(format => format.value === formatButton.selectedValue))
            focusInput()
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }

        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 140; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; to: 0.96; duration: 140; easing.type: Easing.InCubic }
        }

        transformOrigin: Popup.TopLeft
        background: Rectangle {
            radius: 9
            color: "#262626"
            border.color: "#484848"
        }

        contentItem: ColumnLayout {
            spacing: 6

            TextField {
                id: formatSearch
                Layout.fillWidth: true
                visible: formatButton.searchable
                placeholderText: formatButton.searchPlaceholder
                Accessible.name: placeholderText
                placeholderTextColor: "#b5b5b5"
                color: "#f2f2f2"
                selectByMouse: true
                leftPadding: 31
                onTextChanged: formatList.currentIndex = formatMenu.matches.length > 0 ? 0 : -1
                onAccepted: formatMenu.selectFormat(formatList.currentIndex)
                Keys.onDownPressed: formatList.incrementCurrentIndex()
                Keys.onUpPressed: formatList.decrementCurrentIndex()

                background: Rectangle {
                    implicitHeight: 33
                    radius: 5
                    color: "#1c1c1c"
                    border.color: "#4b4b4b"
                }

                LineIcon {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    symbol: "search"
                }
            }

            ListView {
                id: formatList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(count, 5) * 34
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: formatMenu.matches
                keyNavigationEnabled: !formatButton.searchable
                Keys.onReturnPressed: formatMenu.selectFormat(currentIndex)
                Keys.onEnterPressed: formatMenu.selectFormat(currentIndex)
                Keys.onSpacePressed: formatMenu.selectFormat(currentIndex)

                delegate: ItemDelegate {
                    id: option
                    required property int index
                    required property var modelData
                    width: formatList.width
                    height: 34
                    text: modelData.label
                    hoverEnabled: true
                    highlighted: ListView.isCurrentItem
                    onClicked: formatMenu.selectFormat(index)

                    contentItem: RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: option.text
                            color: "#f2f2f2"
                        }

                        Label {
                            text: "✓"
                            opacity: option.modelData.value === formatButton.selectedValue ? 1 : 0
                            color: "#d6d6d6"
                        }
                    }

                    background: Rectangle {
                        radius: 5
                        color: option.hovered || option.highlighted ? "#3c3c3c" : "transparent"
                        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: formatList.count === 0
                color: "#b5b5b5"
                padding: 6
                text: qsTr("No matching formats.")
            }
        }
    }
}
