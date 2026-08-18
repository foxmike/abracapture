import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Theme

Item {
    id: root

    property var settingsViewModel

    readonly property var vm: root.settingsViewModel

    property bool showSaveNotification: false

    Rectangle { anchors.fill: parent; color: Theme.AbracaptureTheme.background; z: -1 }

    Timer {
        id: saveNotificationTimer
        interval: 3000
        repeat: false
        onTriggered: root.showSaveNotification = false
    }

    Connections {
        target: root.vm ? root.vm : null
        ignoreUnknownSignals: true

        function onSaved() {
            root.showSaveNotification = true
            saveNotificationTimer.restart()
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 20

            // Page Header
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "Settings"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: Theme.AbracaptureTheme.text
                }

                Text {
                    text: "Configure audio capture destinations, file naming, master formats, and automated recording thresholds."
                    font.pixelSize: 13
                    color: Theme.AbracaptureTheme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            // Inline Notification / Error Banners
            Rectangle {
                id: errorBanner
                Layout.fillWidth: true
                implicitHeight: errorLayout.implicitHeight + 20
                radius: 8
                color: "#450a0a"
                border.color: "#ef4444"
                border.width: 1
                visible: root.vm ? (!root.vm.isValid && root.vm.errorMessage.length > 0) : false

                RowLayout {
                    id: errorLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#ef4444"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        text: root.vm ? root.vm.errorMessage : ""
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: "#fca5a5"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                id: saveSuccessBanner
                Layout.fillWidth: true
                implicitHeight: saveSuccessLayout.implicitHeight + 20
                radius: 8
                color: "#064e3b"
                border.color: "#10b981"
                border.width: 1
                visible: root.showSaveNotification

                RowLayout {
                    id: saveSuccessLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#34d399"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        text: "Settings saved successfully."
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: "#a7f3d0"
                        Layout.fillWidth: true
                    }
                }
            }

            // SECTION 1: Output & Formats
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: outputSectionLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1

                ColumnLayout {
                    id: outputSectionLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 3
                            height: 16
                            radius: 1.5
                            color: "#38bdf8"
                        }
                        Text {
                            text: "Output & Storage"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                                    color: Theme.AbracaptureTheme.text
                        }
                    }

                    // Output Directory
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Output Directory"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                id: outputDirField
                                Layout.fillWidth: true
                                text: root.vm ? root.vm.outputDirectory : ""
                                placeholderText: "/path/to/recordings"
                                    color: Theme.AbracaptureTheme.text
                                background: Rectangle {
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: outputDirField.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                                onTextChanged: {
                                    if (root.vm && root.vm.outputDirectory !== text) {
                                        root.vm.outputDirectory = text
                                    }
                                }
                            }

                            Button {
                                id: browseButton
                                text: "Browse..."
                                implicitHeight: outputDirField.implicitHeight
                                onClicked: {
                                    if (!root.vm) {
                                        return
                                    }
                                    var selectedDirectory = appController.selectOutputDirectory(root.vm.outputDirectory)
                                    if (selectedDirectory.length > 0) {
                                        root.vm.outputDirectory = selectedDirectory
                                        root.vm.save()
                                    }
                                }
                                contentItem: Text {
                                    text: browseButton.text
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: Theme.AbracaptureTheme.text
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: browseButton.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : Theme.AbracaptureTheme.surface
                                    border.color: "#475569"
                                    border.width: 1
                                }
                            }
                        }

                        Text {
                            text: "Location where completed masters and post-capture exports will be stored."
                            font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.disabledText
                        }
                    }

                    // File Naming Pattern
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "File Naming Pattern"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                        }

                        TextField {
                            id: namingPatternField
                            Layout.fillWidth: true
                            text: root.vm ? root.vm.namingPattern : "{source}_{date}_{time}"
                            placeholderText: "{source}_{date}_{time}"
                                    color: Theme.AbracaptureTheme.text
                            background: Rectangle {
                                radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: namingPatternField.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                border.width: 1
                            }
                            onTextChanged: {
                                if (root.vm && root.vm.namingPattern !== text) {
                                    root.vm.namingPattern = text
                                }
                            }
                        }

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "Available tokens:"
                                font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.disabledText
                            }
                            Rectangle {
                                radius: 4
                                    color: Theme.AbracaptureTheme.controlBackgroundHover
                                implicitWidth: t1.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t1; anchors.centerIn: parent; text: "{source}"; font.pixelSize: 10; color: Theme.AbracaptureTheme.textSecondary }
                            }
                            Rectangle {
                                radius: 4
                                    color: Theme.AbracaptureTheme.controlBackgroundHover
                                implicitWidth: t2.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t2; anchors.centerIn: parent; text: "{date}"; font.pixelSize: 10; color: Theme.AbracaptureTheme.textSecondary }
                            }
                            Rectangle {
                                radius: 4
                                    color: Theme.AbracaptureTheme.controlBackgroundHover
                                implicitWidth: t3.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t3; anchors.centerIn: parent; text: "{time}"; font.pixelSize: 10; color: Theme.AbracaptureTheme.textSecondary }
                            }
                            Rectangle {
                                radius: 4
                                    color: Theme.AbracaptureTheme.controlBackgroundHover
                                implicitWidth: t4.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t4; anchors.centerIn: parent; text: "{counter}"; font.pixelSize: 10; color: Theme.AbracaptureTheme.textSecondary }
                            }
                        }
                    }

                    // Master Format
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Master Recording Format"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                        }

                        RowLayout {
                            spacing: 12

                            ComboBox {
                                id: masterFormatCombo
                                implicitWidth: 160
                                model: ["WAV (Uncompressed PCM)", "FLAC (Lossless Compressed)"]
                                currentIndex: (root.vm && root.vm.masterFormat.toLowerCase() === "flac") ? 1 : 0
                                onActivated: function(index) {
                                    if (root.vm) {
                                        root.vm.masterFormat = (index === 1 ? "flac" : "wav")
                                    }
                                }
                                indicator: Text {
                                    x: masterFormatCombo.width - width - 10
                                    y: (masterFormatCombo.height - height) / 2
                                    text: "▼"
                                    color: Theme.AbracaptureTheme.textSecondary
                                    font.pixelSize: 10
                                }
                                delegate: ItemDelegate {
                                    width: masterFormatCombo.width
                                    contentItem: Text {
                                        text: modelData
                                        color: highlighted ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.text
                                        font.pixelSize: 13
                                    }
                                    background: Rectangle {
                                        color: highlighted ? Theme.AbracaptureTheme.controlBackgroundHover : Theme.AbracaptureTheme.controlBackground
                                    }
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    rightPadding: 24
                                    text: masterFormatCombo.displayText
                                    font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: masterFormatCombo.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                            }

                            Text {
                                text: masterFormatCombo.currentIndex === 0
                                      ? "32-bit float / 24-bit PCM without encoding latency."
                                      : "Bit-perfect FLAC container with compressed storage footprint."
                                font.pixelSize: 12
                                    color: Theme.AbracaptureTheme.textSecondary
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: audioFormatLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surface
                border.color: Theme.AbracaptureTheme.textSecondary
                border.width: 1

                ColumnLayout {
                    id: audioFormatLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: "Capture Resolution"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: Theme.AbracaptureTheme.text
                    }
                    Text {
                        text: "Auto (source) preserves the format PipeWire negotiates. Manual values intentionally resample or quantize the capture."
                        font.pixelSize: 12
                        color: Theme.AbracaptureTheme.textSecondary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        spacing: 16
                        Text { text: "Sample rate"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 13 }
                        ComboBox {
                            id: sampleRateCombo
                            model: root.vm ? root.vm.availableSampleRates : ["Auto (source)", "48000", "96000", "128000", "192000"]
                            currentIndex: root.vm && root.vm.captureSampleRate > 0 ? model.indexOf(String(root.vm.captureSampleRate)) : 0
                            onActivated: if (root.vm) root.vm.captureSampleRate = index === 0 ? 0 : Number(currentText)
                            indicator: Text {
                                x: sampleRateCombo.width - width - 8
                                y: (sampleRateCombo.height - height) / 2
                                text: "▼"
                                color: Theme.AbracaptureTheme.textSecondary
                                font.pixelSize: 10
                            }
                        }
                        Text { text: "Bit depth"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 13 }
                        ComboBox {
                            id: bitDepthCombo
                            model: root.vm ? root.vm.availableBitDepths : ["Auto (source)", "16", "24", "32"]
                            currentIndex: root.vm && root.vm.captureBitDepth > 0 ? model.indexOf(String(root.vm.captureBitDepth)) : 0
                            onActivated: if (root.vm) root.vm.captureBitDepth = index === 0 ? 0 : Number(currentText)
                            indicator: Text {
                                x: bitDepthCombo.width - width - 8
                                y: (bitDepthCombo.height - height) / 2
                                text: "▼"
                                color: Theme.AbracaptureTheme.textSecondary
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }

            // SECTION 2: Capture Automation & Buffering
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: captureSectionLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1

                ColumnLayout {
                    id: captureSectionLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 3
                            height: 16
                            radius: 1.5
                            color: "#10b981"
                        }
                        Text {
                            text: "Capture Automation & Pre/Post Roll"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                                    color: Theme.AbracaptureTheme.text
                        }
                    }

                    // Pre-Roll & Post-Roll
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 24

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: "Pre-Roll Buffer (seconds)"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                            }

                            Row {
                                spacing: 8
                                SpinBox {
                                id: preRollSpinBox
                                from: 0
                                to: 60
                                stepSize: 1
                                value: root.vm ? root.vm.preRollSeconds : 0
                                editable: false
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.preRollSeconds = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 140
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: preRollSpinBox.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: preRollSpinBox.textFromValue(preRollSpinBox.value, preRollSpinBox.locale)
                                    font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: true
                                }
                                up.indicator: Rectangle {
                                    x: preRollSpinBox.width - width
                                    width: 22
                                    height: preRollSpinBox.height / 2
                                    color: preRollSpinBox.up.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▲"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                down.indicator: Rectangle {
                                    x: preRollSpinBox.width - width
                                    y: preRollSpinBox.height / 2
                                    width: 22
                                    height: preRollSpinBox.height / 2
                                    color: preRollSpinBox.down.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▼"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                }
                                Text { text: "s"; color: "#cbd5e1"; font.pixelSize: 13 }
                            }

                            Text {
                                text: "Continuously captures audio prior to recording trigger."
                                font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.disabledText
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: "Post-Roll Tail (seconds)"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                            }

                            Row {
                                spacing: 8
                                SpinBox {
                                id: postRollSpinBox
                                from: 0
                                to: 60
                                stepSize: 1
                                value: root.vm ? root.vm.postRollSeconds : 0
                                editable: false
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.postRollSeconds = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 140
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: postRollSpinBox.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: postRollSpinBox.textFromValue(postRollSpinBox.value, postRollSpinBox.locale)
                                    font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: true
                                }
                                up.indicator: Rectangle {
                                    x: postRollSpinBox.width - width
                                    width: 22
                                    height: postRollSpinBox.height / 2
                                    color: postRollSpinBox.up.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▲"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                down.indicator: Rectangle {
                                    x: postRollSpinBox.width - width
                                    y: postRollSpinBox.height / 2
                                    width: 22
                                    height: postRollSpinBox.height / 2
                                    color: postRollSpinBox.down.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▼"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                }
                                Text { text: "s"; color: "#cbd5e1"; font.pixelSize: 13 }
                            }

                            Text {
                                text: "Extends capture after stop signal to preserve reverberation."
                                font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.disabledText
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.AbracaptureTheme.divider
                    }

                    // Auto-Start Switch & Threshold
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Switch {
                                id: autoStartSwitch
                                checked: root.vm ? root.vm.autoStartEnabled : false
                                onToggled: {
                                    if (root.vm) {
                                        root.vm.autoStartEnabled = checked
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Auto-Start Capture on Signal"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                                }

                                Text {
                                    text: "Arm capture and automatically begin recording when input audio exceeds threshold."
                                    font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.textSecondary
                                }
                            }
                        }

                        // Auto-start threshold control
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 48
                            visible: autoStartSwitch.checked
                            spacing: 16

                            Text {
                                text: "Start Threshold:"
                                font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                            }

                            Row {
                                spacing: 8
                                SpinBox {
                                id: autoStartSpinBox
                                from: -90
                                to: 0
                                stepSize: 1
                                value: root.vm ? Math.round(root.vm.autoStartThreshold) : -30
                                editable: false
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.autoStartThreshold = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 120
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: autoStartSpinBox.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: autoStartSpinBox.value
                                    font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: true
                                }
                                up.indicator: Rectangle {
                                    x: autoStartSpinBox.width - width
                                    width: 22
                                    height: autoStartSpinBox.height / 2
                                    color: autoStartSpinBox.up.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▲"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                down.indicator: Rectangle {
                                    x: autoStartSpinBox.width - width
                                    y: autoStartSpinBox.height / 2
                                    width: 22
                                    height: autoStartSpinBox.height / 2
                                    color: autoStartSpinBox.down.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▼"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                }
                                Text { text: "dBFS"; color: "#cbd5e1"; font.pixelSize: 13 }
                            }

                            Text {
                                text: "Default is -30 dBFS."
                                font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.disabledText
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.AbracaptureTheme.divider
                    }

                    // Auto-Stop Switch & Silence Duration
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Switch {
                                id: autoStopSwitch
                                checked: root.vm ? root.vm.autoStopEnabled : false
                                onToggled: {
                                    if (root.vm) {
                                        root.vm.autoStopEnabled = checked
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Auto-Stop Capture on Silence"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                                }

                                Text {
                                    text: "Automatically finalize recording when input drops below silence threshold."
                                    font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.textSecondary
                                }
                            }
                        }

                        // Auto-stop duration control
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 48
                            visible: autoStopSwitch.checked
                            spacing: 16

                            Text {
                                text: "Silence Duration:"
                                font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                            }

                            Row {
                                spacing: 8
                                SpinBox {
                                id: autoStopDurationSpinBox
                                from: 1
                                to: 60
                                stepSize: 1
                                value: root.vm ? root.vm.autoStopSilenceDuration : 3
                                editable: false
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.autoStopSilenceDuration = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 120
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: autoStopDurationSpinBox.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: autoStopDurationSpinBox.value
                                    font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: true
                                }
                                up.indicator: Rectangle {
                                    x: autoStopDurationSpinBox.width - width
                                    width: 22
                                    height: autoStopDurationSpinBox.height / 2
                                    color: autoStopDurationSpinBox.up.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▲"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                down.indicator: Rectangle {
                                    x: autoStopDurationSpinBox.width - width
                                    y: autoStopDurationSpinBox.height / 2
                                    width: 22
                                    height: autoStopDurationSpinBox.height / 2
                                    color: autoStopDurationSpinBox.down.pressed ? Theme.AbracaptureTheme.controlBackgroundHover : "transparent"
                                    Text { anchors.centerIn: parent; text: "▼"; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 9 }
                                }
                                }
                                Text { text: "s"; color: "#cbd5e1"; font.pixelSize: 13 }
                            }

                            Text {
                                text: "Continuous silence before trigger."
                                font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.disabledText
                            }
                        }
                    }
                }
            }

            // SECTION 3: Post-Capture Export
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: exportSectionLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1

                ColumnLayout {
                    id: exportSectionLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 3
                            height: 16
                            radius: 1.5
                            color: "#c084fc"
                        }
                        Text {
                            text: "Post-Capture Export"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                                    color: Theme.AbracaptureTheme.text
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Switch {
                            id: exportSwitch
                            checked: root.vm ? root.vm.exportEnabled : false
                            onToggled: {
                                if (root.vm) {
                                    root.vm.exportEnabled = checked
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "Generate Additional Export File"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                            }

                            Text {
                                text: "Automatically transcode a secondary delivery format via ffmpeg when capture finishes."
                                font.pixelSize: 11
                                    color: Theme.AbracaptureTheme.textSecondary
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 48
                        visible: exportSwitch.checked
                        spacing: 8

                        Text {
                            text: "Export Format"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                                    color: Theme.AbracaptureTheme.text
                        }

                        RowLayout {
                            spacing: 12

                            ComboBox {
                                id: exportFormatCombo
                                implicitWidth: 160
                                model: ["MP3", "FLAC", "OGG Vorbis", "WAV"]
                                readonly property var formatCodes: ["mp3", "flac", "ogg", "wav"]
                                currentIndex: {
                                    if (!root.vm) return 0
                                    var idx = formatCodes.indexOf(root.vm.exportFormat.toLowerCase())
                                    return idx >= 0 ? idx : 0
                                }
                                onActivated: function(index) {
                                    if (root.vm) {
                                        root.vm.exportFormat = formatCodes[index]
                                    }
                                }
                                indicator: Text {
                                    x: exportFormatCombo.width - width - 10
                                    y: (exportFormatCombo.height - height) / 2
                                    text: "▼"
                                    color: Theme.AbracaptureTheme.textSecondary
                                    font.pixelSize: 10
                                }
                                delegate: ItemDelegate {
                                    width: exportFormatCombo.width
                                    contentItem: Text {
                                        text: modelData
                                        color: highlighted ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.text
                                        font.pixelSize: 13
                                    }
                                    background: Rectangle {
                                        color: highlighted ? Theme.AbracaptureTheme.controlBackgroundHover : Theme.AbracaptureTheme.controlBackground
                                    }
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    rightPadding: 24
                                    text: exportFormatCombo.displayText
                                    font.pixelSize: 13
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: Theme.AbracaptureTheme.controlBackground
                                    border.color: exportFormatCombo.activeFocus ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                            }

                            Text {
                                text: "Master file remains pristine in original lossless format."
                                font.pixelSize: 12
                                    color: Theme.AbracaptureTheme.textSecondary
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            // Bottom Actions Bar
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 12

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: resetButton
                    text: "Reset to Defaults"
                    implicitHeight: 38
                    onClicked: {
                        if (root.vm) {
                            root.vm.resetToDefaults()
                        }
                    }
                    contentItem: Text {
                        text: resetButton.text
                        font.pixelSize: 13
                        font.weight: Font.Medium
                                    color: Theme.AbracaptureTheme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: resetButton.hovered ? Theme.AbracaptureTheme.controlBackgroundHover : Theme.AbracaptureTheme.surfaceElevated
                        border.color: Theme.AbracaptureTheme.border
                        border.width: 1
                    }
                }

                Button {
                    id: saveButton
                    text: "Save Settings"
                    implicitHeight: 38
                    implicitWidth: 130
                    enabled: root.vm ? root.vm.isValid : true
                    onClicked: {
                        if (root.vm) {
                            root.vm.save()
                        }
                    }
                    contentItem: Text {
                        text: saveButton.text
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: saveButton.enabled ? Theme.AbracaptureTheme.text : Theme.AbracaptureTheme.disabledText
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: saveButton.enabled ? (saveButton.hovered ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.primary) : Theme.AbracaptureTheme.controlBackgroundHover
                        border.color: saveButton.enabled ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.border
                        border.width: 1
                    }
                }
            }

            Item {
                implicitHeight: 24
            }
        }
    }
}
