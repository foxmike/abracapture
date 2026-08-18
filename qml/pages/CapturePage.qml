import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components
import "../theme" as Theme

Item {
    id: root

    // SourceListModel, WaveformBridge, and CaptureViewModel are context
    // properties supplied by main.cpp; QML never constructs these C++ types.
    property var sourceListModel: null
    property var waveformBridge: null
    property var captureViewModel: null
    property var settingsViewModel: null

    readonly property var sources: root.sourceListModel
    readonly property var bridge: root.waveformBridge
    readonly property var capture: root.captureViewModel

    function durationLabel(seconds) {
        var safeSeconds = Math.max(0, Math.floor(Number(seconds) || 0))
        var hours = Math.floor(safeSeconds / 3600)
        var minutes = Math.floor((safeSeconds % 3600) / 60)
        var remainingSeconds = safeSeconds % 60
        function pad(value) { return value < 10 ? "0" + value : value.toString() }
        return pad(hours) + ":" + pad(minutes) + ":" + pad(remainingSeconds)
    }

    function captureKindLabel() {
        if (!root.capture) {
            return "Awaiting capture engine"
        }
        if (root.capture.isNativeCapture) {
            return "Native signal path"
        }
        if (root.capture.isQuantizedCapture) {
            return "Quantized capture"
        }
        if (root.capture.isResampledCapture) {
            return "Resampled capture"
        }
        return "Converted capture"
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.AbracaptureTheme.background
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 24
        columns: width >= 1060 ? 2 : 1
        columnSpacing: 20
        rowSpacing: 20

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 370
            spacing: 12

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Capture sources"
                        color: Theme.AbracaptureTheme.text
                        font.pixelSize: 23
                        font.weight: Font.Bold
                    }

                    Text {
                        text: "Choose an application, system monitor, or physical input."
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                ToolButton {
                    text: "↻"
                    font.pixelSize: 20
                    Accessible.name: "Refresh sources"
                    onClicked: if (root.sources) root.sources.refresh()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: [
                        { "label": "APP STREAM", "color": "#60a5fa" },
                        { "label": "SYSTEM MONITOR", "color": "#c084fc" },
                        { "label": "HARDWARE INPUT", "color": "#34d399" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 27
                        radius: 5
                        color: Theme.AbracaptureTheme.surfaceElevated
                        border.color: modelData.color
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: parent.modelData.label
                            color: parent.modelData.color
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: Theme.AbracaptureTheme.surface
                border.color: Theme.AbracaptureTheme.border
                border.width: 1

                ListView {
                    id: sourceList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    spacing: 8
                    model: root.sources
                    delegate: Components.SourceCard {
                        width: sourceList.width
                        selectedSourceId: root.sources ? root.sources.selectedSourceId : 0
                        onSourceSelected: function(sourceId) {
                            if (root.sources) {
                                root.sources.selectSourceById(sourceId)
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar { }

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        visible: !root.sources || root.sources.count === 0
                        text: "No capture sources discovered yet. Check that PipeWire is running, then refresh."
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 13
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420
            spacing: 16

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Live capture"
                        color: Theme.AbracaptureTheme.text
                        font.pixelSize: 23
                        font.weight: Font.Bold
                    }

                    Text {
                        text: root.capture && root.capture.selectedSourceId !== 0
                              ? "Source #" + root.capture.selectedSourceId
                              : "Select a source to begin"
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 13
                    }
                }

                Rectangle {
                    implicitWidth: stateLabel.implicitWidth + 20
                    implicitHeight: 28
                    radius: 14
                    color: root.capture && root.capture.captureState === "capturing" ? "#065f46"
                          : root.capture && root.capture.captureState === "connecting" ? "#713f12"
                          : Theme.AbracaptureTheme.controlBackgroundHover

                    Text {
                        id: stateLabel
                        anchors.centerIn: parent
                        text: root.capture ? root.capture.captureState.toUpperCase() : "IDLE"
                        color: Theme.AbracaptureTheme.text
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                }
            }

            Components.WaveformView {
                Layout.fillWidth: true
                Layout.preferredHeight: 225
                waveformBridge: root.bridge
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 78
                    radius: 8
                    color: Theme.AbracaptureTheme.surface
                    border.color: Theme.AbracaptureTheme.border

                    Column {
                        anchors.centerIn: parent
                        spacing: 5
                        Text { text: "DURATION"; color: Theme.AbracaptureTheme.disabledText; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text { text: root.durationLabel(root.capture ? root.capture.durationSeconds : 0); color: Theme.AbracaptureTheme.text; font.pixelSize: 20; font.family: "monospace" }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 78
                    radius: 8
                    color: Theme.AbracaptureTheme.surface
                    border.color: Theme.AbracaptureTheme.border

                    Column {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        spacing: 5
                        Text { text: "FORMAT"; color: Theme.AbracaptureTheme.disabledText; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: root.capture ? root.capture.formatDescription : "—"
                            color: Theme.AbracaptureTheme.text
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 78
                    radius: 8
                    color: Theme.AbracaptureTheme.surface
                    border.color: Theme.AbracaptureTheme.border

                    Column {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        spacing: 5
                        Text { text: "SIGNAL PATH"; color: Theme.AbracaptureTheme.disabledText; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: root.captureKindLabel()
                            color: root.capture && root.capture.isNativeCapture ? Theme.AbracaptureTheme.success : Theme.AbracaptureTheme.warning
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: integrityText.implicitHeight + 20
                radius: 7
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: root.capture && root.capture.integrityStatus.indexOf("error") >= 0 ? Theme.AbracaptureTheme.error : Theme.AbracaptureTheme.border

                Text {
                    id: integrityText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: root.capture ? root.capture.integrityStatus : "Preparing capture engine"
                    color: Theme.AbracaptureTheme.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Output destination"
                    color: Theme.AbracaptureTheme.textSecondary
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: destinationField
                    Layout.fillWidth: true
                    text: root.capture ? root.capture.outputPath : ""
                    placeholderText: "/path/to/recordings"
                    color: Theme.AbracaptureTheme.text
                    selectByMouse: true
                    onEditingFinished: {
                        if (root.capture) root.capture.outputPath = text
                        if (root.settingsViewModel) root.settingsViewModel.outputDirectory = text
                    }
                    background: Rectangle {
                        radius: 6
                        color: Theme.AbracaptureTheme.controlBackground
                        border.width: 1
                        border.color: destinationField.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Pre-roll: " + (root.capture ? root.capture.preRollSeconds : 0) + " s"
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 11
                    }
                    Text {
                        text: "Post-roll: " + (root.capture ? root.capture.postRollSeconds : 0) + " s"
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 11
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: root.capture && root.capture.autoStartEnabled ? "AUTO START" : ""
                        color: "#fbbf24"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: root.capture && root.capture.autoStopEnabled ? "AUTO STOP" : ""
                        color: "#fbbf24"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 10

                Button {
                    id: armButton
                    Layout.fillWidth: true
                    text: root.capture && root.capture.armed ? "Disarm" : "Arm"
                    onClicked: {
                        if (!root.capture) return
                        if (root.capture.armed) root.capture.disarmCapture()
                        else root.capture.armCapture()
                    }
                }

                Button {
                    id: startButton
                    Layout.fillWidth: true
                    enabled: root.capture && root.capture.selectedSourceId !== 0
                             && root.capture.captureState !== "capturing"
                             && root.capture.captureState !== "connecting"
                    text: "Start capture"
                    onClicked: if (root.capture) root.capture.startCapture()
                    contentItem: Text {
                        text: startButton.text
                        color: Theme.AbracaptureTheme.darkMode ? "#052e16" : "#ffffff"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: startButton.enabled ? Theme.AbracaptureTheme.captureActive : Theme.AbracaptureTheme.captureIdleBackground
                    }
                }

                Button {
                    id: stopButton
                    Layout.fillWidth: true
                    enabled: root.capture && (root.capture.captureState === "capturing"
                                               || root.capture.captureState === "connecting")
                    text: "Stop"
                    onClicked: if (root.capture) root.capture.stopCapture()
                    contentItem: Text {
                        text: stopButton.text
                        color: Theme.AbracaptureTheme.text
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: stopButton.enabled ? Theme.AbracaptureTheme.error : Theme.AbracaptureTheme.captureIdleBackground
                    }
                }
            }
        }
    }
}
