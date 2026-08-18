import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Theme

ComboBox {
    id: control

    property string displayLabel: ""
    property var popupLabels: []

    function optionLabel(index) {
        return popupLabels && popupLabels.length > index ? popupLabels[index] : control.textAt(index)
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: control.indicator.width + 12
        text: control.displayLabel !== "" ? control.displayLabel : control.displayText
        color: control.enabled ? Theme.AbracaptureTheme.text : Theme.AbracaptureTheme.disabledText
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "▾"
        color: control.enabled ? Theme.AbracaptureTheme.text : Theme.AbracaptureTheme.disabledText
        font.pixelSize: 14
    }

    background: Rectangle {
        radius: 5
        color: control.enabled
                   ? (control.hovered ? Theme.AbracaptureTheme.controlSurfaceHover
                                      : Theme.AbracaptureTheme.controlSurface)
                   : Theme.AbracaptureTheme.controlSurfaceDisabled
        border.color: control.popup.visible || control.activeFocus
                         ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
        border.width: control.activeFocus ? 2 : 1
    }

    delegate: ItemDelegate {
        required property int index
        width: control.popup.width
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            leftPadding: 10
            rightPadding: 10
            text: control.optionLabel(index)
            color: highlighted ? Theme.AbracaptureTheme.background : Theme.AbracaptureTheme.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: highlighted ? Theme.AbracaptureTheme.primary
                                : (hovered ? Theme.AbracaptureTheme.controlSurfaceHover
                                            : Theme.AbracaptureTheme.controlSurface)
        }
    }

    popup: Popup {
        y: control.height - 1
        width: control.width
        padding: 2
        implicitHeight: Math.min(contentItem.implicitHeight + padding * 2, 320)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color: Theme.AbracaptureTheme.controlSurface
            border.color: Theme.AbracaptureTheme.border
            radius: 5
        }
    }
}
