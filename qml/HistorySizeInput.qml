import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SpinBox {
    id: historySize
    Layout.preferredWidth: 126
    to: 2147483647
    editable: true
    textFromValue: (value, locale) => from === 0 && value === 0 ? "∞" : String(value)
    valueFromText: (text, locale) => text === "∞" ? 0 : parseInt(text, 10)
    validator: RegularExpressionValidator { regularExpression: /^(?:[0-9]{0,10}|∞)$/ }

    background: Rectangle {
        implicitHeight: 40
        radius: height / 2
        color: "#232323"
        border.color: historySize.activeFocus ? "#d6d6d6" : "#3a3a3a"
    }

    up.indicator: Label {
        x: historySize.width - width - 4
        y: 4
        width: 32
        height: historySize.height - 8
        text: "+"
        color: "#f2f2f2"
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
        color: historySize.value > historySize.from ? "#f2f2f2" : "#b5b5b5"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        background: Rectangle {
            radius: height / 2
            color: historySize.down.pressed ? "#545454" : "#383838"
        }
    }
}
