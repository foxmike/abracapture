import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Theme

FocusScope {
    id: root

    signal requestCapture()

    focus: true
    activeFocusOnTab: true
    Accessible.name: "About Abracapture"

    property string applicationVersion: typeof appController !== "undefined"
                                         ? appController.appVersion : "0.1.0"

    Rectangle {
        anchors.fill: parent
        color: Theme.AbracaptureTheme.background
    }

    ScrollView {
        id: aboutScroll
        anchors.fill: parent
        anchors.margins: Theme.AbracaptureTheme.spacingXl
        contentWidth: availableWidth
        clip: true
        focus: true
        activeFocusOnTab: true
        Accessible.name: "About content"

        // ScrollView handles Tab, arrow, and page navigation while focused.
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_PageDown) {
                aboutScroll.contentItem.flick(0, -aboutScroll.height)
                event.accepted = true
            } else if (event.key === Qt.Key_PageUp) {
                aboutScroll.contentItem.flick(0, aboutScroll.height)
                event.accepted = true
            } else if (event.key === Qt.Key_Home) {
                aboutScroll.contentItem.contentY = 0
                event.accepted = true
            } else if (event.key === Qt.Key_End) {
                aboutScroll.contentItem.contentY = Math.max(0, aboutScroll.contentItem.contentHeight - aboutScroll.height)
                event.accepted = true
            }
        }

        ColumnLayout {
            width: aboutScroll.availableWidth
            spacing: Theme.AbracaptureTheme.spacingLg

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.AbracaptureTheme.spacingSm

                Text {
                    text: "Abracapture"
                    color: Theme.AbracaptureTheme.text
                    font.pixelSize: Theme.AbracaptureTheme.fontSizeTitle
                    font.weight: Font.Bold
                }

                Text {
                    text: "Find it. See it. Capture it faithfully."
                    color: Theme.AbracaptureTheme.primary
                    font.pixelSize: Theme.AbracaptureTheme.fontSizeSubtitle
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "Abracapture is a focused Linux audio-capture utility for discovering PipeWire sources and preserving the signal path you selected."
                    color: Theme.AbracaptureTheme.textSecondary
                    font.pixelSize: Theme.AbracaptureTheme.fontSizeBody
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: identityLayout.implicitHeight + Theme.AbracaptureTheme.spacingLg * 2
                color: Theme.AbracaptureTheme.surface
                border.color: Theme.AbracaptureTheme.primary
                border.width: 1
                radius: 10

                ColumnLayout {
                    id: identityLayout
                    anchors.fill: parent
                    anchors.margins: Theme.AbracaptureTheme.spacingLg
                    spacing: Theme.AbracaptureTheme.spacingSm

                    Text { text: "Product identity"; color: Theme.AbracaptureTheme.text; font.pixelSize: Theme.AbracaptureTheme.fontSizeSubtitle; font.weight: Font.Bold }
                    Text { text: "Version  " + root.applicationVersion; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: Theme.AbracaptureTheme.fontSizeBody }
                    Text { text: "License  License information is provided with the distribution."; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: Theme.AbracaptureTheme.fontSizeBody; wrapMode: Text.WordWrap }
                    Text { text: "Build  Linux desktop build · Qt 6 · C++20"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: Theme.AbracaptureTheme.fontSizeBody; wrapMode: Text.WordWrap }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.AbracaptureTheme.spacingSm

                Text {
                    text: "Third-party acknowledgements"
                    color: Theme.AbracaptureTheme.text
                    font.pixelSize: Theme.AbracaptureTheme.fontSizeSubtitle
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    text: "Abracapture is built with and acknowledges the following projects:"
                    color: Theme.AbracaptureTheme.textSecondary
                    font.pixelSize: Theme.AbracaptureTheme.fontSizeBody
                }

                Repeater {
                    model: [
                        "PipeWire — audio graph and source discovery",
                        "Qt — application framework and Qt Quick",
                        "libsndfile — audio file support",
                        "libFLAC — lossless FLAC encoding",
                        "ffmpeg — optional post-capture exports"
                    ]

                    delegate: Text {
                        required property string modelData
                        Layout.fillWidth: true
                        text: "• " + modelData
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: Theme.AbracaptureTheme.fontSizeBody
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Button {
                id: captureButton
                text: "Return to Capture"
                Accessible.name: "Return to Capture"
                activeFocusOnTab: true
                onClicked: root.requestCapture()
            }
        }
    }
}
