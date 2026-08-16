import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

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
        color: "#0b1120"
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
                        color: "#f8fafc"
                        font.pixelSize: 23
                        font.weight: Font.Bold
                    }

                    Text {
                        text: "Choose an application, system monitor, or physical input."
                        color: "#94a3b8"
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
                        color: "#161e2d"
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
                color: "#111827"
                border.color: "#273449"
                border.width: 1

                ListView {
                    id: sourceList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    spacing: 8
                    model: root.sources
                    currentIndex: root.sources ? root.sources.selectedIndex : -1

                    delegate: Components.SourceCard {
                        width: sourceList.width
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
                        color: "#94a3b8"
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
                        color: "#f8fafc"
                        font.pixelSize: 23
                        font.weight: Font.Bold
                    }

                    Text {
                        text: root.capture && root.capture.selectedSourceId !== 0
                              ? "Source #" + root.capture.selectedSourceId
                              : "Select a source to begin"
                        color: "#94a3b8"
                        font.pixelSize: 13
                    }
                }

                Rectangle {
                    implicitWidth: stateLabel.implicitWidth + 20
                    implicitHeight: 28
                    radius: 14
                    color: root.capture && root.capture.captureState === "capturing" ? "#065f46"
                          : root.capture && root.capture.captureState === "connecting" ? "#713f12"
                          : "#1e293b"

                    Text {
                        id: stateLabel
                        anchors.centerIn: parent
                        text: root.capture ? root.capture.captureState.toUpperCase() : "IDLE"
                        color: "#e2e8f0"
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
                    color: "#111827"
                    border.color: "#273449"

                    Column {
                        anchors.centerIn: parent
                        spacing: 5
                        Text { text: "DURATION"; color: "#64748b"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text { text: root.durationLabel(root.capture ? root.capture.durationSeconds : 0); color: "#f8fafc"; font.pixelSize: 20; font.family: "monospace" }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 78
                    radius: 8
                    color: "#111827"
                    border.color: "#273449"

                    Column {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        spacing: 5
                        Text { text: "FORMAT"; color: "#64748b"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: root.capture ? root.capture.formatDescription : "—"
                            color: "#e2e8f0"
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
                    color: "#111827"
                    border.color: "#273449"

                    Column {
                        anchors.centerIn: parent
                        width: parent.width - 16
                        spacing: 5
                        Text { text: "SIGNAL PATH"; color: "#64748b"; font.pixelSize: 10; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: root.captureKindLabel()
                            color: root.capture && root.capture.isNativeCapture ? "#34d399" : "#fbbf24"
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
                color: "#101c2c"
                border.color: root.capture && root.capture.integrityStatus.indexOf("error") >= 0 ? "#ef4444" : "#235472"

                Text {
                    id: integrityText
                    anchors.fill: parent
                    anchors.margins: 10
                    text: root.capture ? root.capture.integrityStatus : "Preparing capture engine"
                    color: "#bae6fd"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Output destination"
                    color: "#cbd5e1"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: destinationField
                    Layout.fillWidth: true
                    text: root.capture ? root.capture.outputPath : ""
                    placeholderText: "/path/to/recordings"
                    color: "#f8fafc"
                    selectByMouse: true
                    onEditingFinished: {
                        if (root.capture) root.capture.outputPath = text
                        if (root.settingsViewModel) root.settingsViewModel.outputDirectory = text
                    }
                    background: Rectangle {
                        radius: 6
                        color: "#0f172a"
                        border.width: 1
                        border.color: destinationField.activeFocus ? "#38bdf8" : "#334155"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Pre-roll: " + (root.capture ? root.capture.preRollSeconds : 0) + " s"
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                    Text {
                        text: "Post-roll: " + (root.capture ? root.capture.postRollSeconds : 0) + " s"
                        color: "#94a3b8"
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
                        color: "#052e16"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: startButton.enabled ? "#34d399" : "#475569"
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
                        color: "#fee2e2"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: stopButton.enabled ? "#b91c1c" : "#475569"
                    }
                }
            }
        }
    }
}
