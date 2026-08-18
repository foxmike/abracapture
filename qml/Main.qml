import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "pages" as Pages
import "theme" as Theme

ApplicationWindow {
    id: window
    visible: true
    title: appController.appName
    minimumWidth: 900
    minimumHeight: 600
    width: 1280
    height: 800
    color: Theme.AbracaptureTheme.background

    onActiveChanged: {
        if (active && appRecordingsViewModel) {
            appRecordingsViewModel.refreshEntries()
        }
    }

    header: ToolBar {
        implicitHeight: 56

        background: Rectangle {
            color: Theme.AbracaptureTheme.surface
            border.color: Theme.AbracaptureTheme.primary
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 18

            Text {
                text: appController.appName
                color: Theme.AbracaptureTheme.text
                font.pixelSize: Theme.AbracaptureTheme.fontSizeSubtitle
                font.weight: Font.Bold
            }

            TabBar {
                id: navigationTabs
                Layout.fillWidth: true

                background: Rectangle { color: "transparent" }

                TabButton {
                    text: "Capture"
                    Accessible.name: "Capture"
                    contentItem: Text { text: parent.text; color: parent.checked ? Theme.AbracaptureTheme.background : Theme.AbracaptureTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 5; color: parent.checked ? Theme.AbracaptureTheme.primary : (parent.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent") }
                }
                TabButton {
                    text: "Recordings"
                    Accessible.name: "Recordings"
                    contentItem: Text { text: parent.text; color: parent.checked ? Theme.AbracaptureTheme.background : Theme.AbracaptureTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 5; color: parent.checked ? Theme.AbracaptureTheme.primary : (parent.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent") }
                }
                TabButton {
                    text: "Settings"
                    Accessible.name: "Settings"
                    contentItem: Text { text: parent.text; color: parent.checked ? Theme.AbracaptureTheme.background : Theme.AbracaptureTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 5; color: parent.checked ? Theme.AbracaptureTheme.primary : (parent.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent") }
                }
                TabButton {
                    text: "About"
                    Accessible.name: "About Abracapture"
                    contentItem: Text { text: parent.text; color: parent.checked ? Theme.AbracaptureTheme.background : Theme.AbracaptureTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 5; color: parent.checked ? Theme.AbracaptureTheme.primary : (parent.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent") }
                }
            }

            ToolButton {
                text: Theme.AbracaptureTheme.darkMode ? "Light" : "Dark"
                Accessible.name: Theme.AbracaptureTheme.darkMode
                               ? "Switch to light theme" : "Switch to dark theme"
                onClicked: Theme.AbracaptureTheme.darkMode = !Theme.AbracaptureTheme.darkMode
                contentItem: Text { text: parent.text; color: Theme.AbracaptureTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: parent.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent" }
            }

            ToolButton {
                text: Theme.AbracaptureTheme.highContrast ? "Contrast on" : "Contrast"
                Accessible.name: Theme.AbracaptureTheme.highContrast
                               ? "Disable high contrast" : "Enable high contrast"
                onClicked: Theme.AbracaptureTheme.highContrast = !Theme.AbracaptureTheme.highContrast
                contentItem: Text { text: parent.text; color: Theme.AbracaptureTheme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { radius: 5; color: parent.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent" }
            }

            Text {
                text: appController.appVersion
                color: Theme.AbracaptureTheme.textSecondary
                font.pixelSize: Theme.AbracaptureTheme.fontSizeCaption
            }
        }
    }

    StackLayout {
        id: pageStack
        anchors.fill: parent
        currentIndex: navigationTabs.currentIndex

        Pages.CapturePage {
            sourceListModel: appSourceListModel
            waveformBridge: appWaveformBridge
            captureViewModel: appCaptureViewModel
            settingsViewModel: appSettingsViewModel
        }

        Pages.RecordingsPage {
            recordingsViewModel: appRecordingsViewModel
        }

        Pages.SettingsPage {
            settingsViewModel: appSettingsViewModel
        }

        Pages.AboutPage {
            onRequestCapture: navigationTabs.currentIndex = 0
        }
    }
}
