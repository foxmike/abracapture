import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var settingsViewModel

    readonly property var vm: root.settingsViewModel

    property bool showSaveNotification: false

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
                    color: "#f8fafc"
                }

                Text {
                    text: "Configure audio capture destinations, file naming, master formats, and automated recording thresholds."
                    font.pixelSize: 13
                    color: "#94a3b8"
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
                color: "#161920"
                border.color: "#242933"
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
                            color: "#f8fafc"
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
                            color: "#e2e8f0"
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                id: outputDirField
                                Layout.fillWidth: true
                                text: root.vm ? root.vm.outputDirectory : ""
                                placeholderText: "/path/to/recordings"
                                color: "#f8fafc"
                                background: Rectangle {
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: outputDirField.activeFocus ? "#38bdf8" : "#334155"
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
                                    color: "#f8fafc"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: browseButton.hovered ? "#334155" : "#1e293b"
                                    border.color: "#475569"
                                    border.width: 1
                                }
                            }
                        }

                        Text {
                            text: "Location where completed masters and post-capture exports will be stored."
                            font.pixelSize: 11
                            color: "#64748b"
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
                            color: "#e2e8f0"
                        }

                        TextField {
                            id: namingPatternField
                            Layout.fillWidth: true
                            text: root.vm ? root.vm.namingPattern : "{source}_{date}_{time}"
                            placeholderText: "{source}_{date}_{time}"
                            color: "#f8fafc"
                            background: Rectangle {
                                radius: 6
                                color: "#0f172a"
                                border.color: namingPatternField.activeFocus ? "#38bdf8" : "#334155"
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
                                color: "#64748b"
                            }
                            Rectangle {
                                radius: 4
                                color: "#1e293b"
                                implicitWidth: t1.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t1; anchors.centerIn: parent; text: "{source}"; font.pixelSize: 10; color: "#94a3b8" }
                            }
                            Rectangle {
                                radius: 4
                                color: "#1e293b"
                                implicitWidth: t2.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t2; anchors.centerIn: parent; text: "{date}"; font.pixelSize: 10; color: "#94a3b8" }
                            }
                            Rectangle {
                                radius: 4
                                color: "#1e293b"
                                implicitWidth: t3.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t3; anchors.centerIn: parent; text: "{time}"; font.pixelSize: 10; color: "#94a3b8" }
                            }
                            Rectangle {
                                radius: 4
                                color: "#1e293b"
                                implicitWidth: t4.implicitWidth + 8
                                implicitHeight: 18
                                Text { id: t4; anchors.centerIn: parent; text: "{counter}"; font.pixelSize: 10; color: "#94a3b8" }
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
                            color: "#e2e8f0"
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
                                delegate: ItemDelegate {
                                    width: masterFormatCombo.width
                                    contentItem: Text {
                                        text: modelData
                                        color: highlighted ? "#38bdf8" : "#f8fafc"
                                        font.pixelSize: 13
                                    }
                                    background: Rectangle {
                                        color: highlighted ? "#1e293b" : "#0f172a"
                                    }
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    rightPadding: 24
                                    text: masterFormatCombo.displayText
                                    font.pixelSize: 13
                                    color: "#f8fafc"
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: masterFormatCombo.activeFocus ? "#38bdf8" : "#334155"
                                    border.width: 1
                                }
                            }

                            Text {
                                text: masterFormatCombo.currentIndex === 0
                                      ? "32-bit float / 24-bit PCM without encoding latency."
                                      : "Bit-perfect FLAC container with compressed storage footprint."
                                font.pixelSize: 12
                                color: "#94a3b8"
                                Layout.fillWidth: true
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
                color: "#161920"
                border.color: "#242933"
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
                            color: "#f8fafc"
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
                                color: "#e2e8f0"
                            }

                            SpinBox {
                                id: preRollSpinBox
                                from: 0
                                to: 60
                                stepSize: 1
                                value: root.vm ? root.vm.preRollSeconds : 0
                                editable: true
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.preRollSeconds = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 140
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: preRollSpinBox.activeFocus ? "#38bdf8" : "#334155"
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: preRollSpinBox.textFromValue(preRollSpinBox.value, preRollSpinBox.locale) + " s"
                                    font.pixelSize: 13
                                    color: "#f8fafc"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: !preRollSpinBox.editable
                                    validator: preRollSpinBox.validator
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                }
                            }

                            Text {
                                text: "Continuously captures audio prior to recording trigger."
                                font.pixelSize: 11
                                color: "#64748b"
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: "Post-Roll Tail (seconds)"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: "#e2e8f0"
                            }

                            SpinBox {
                                id: postRollSpinBox
                                from: 0
                                to: 60
                                stepSize: 1
                                value: root.vm ? root.vm.postRollSeconds : 0
                                editable: true
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.postRollSeconds = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 140
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: postRollSpinBox.activeFocus ? "#38bdf8" : "#334155"
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: postRollSpinBox.textFromValue(postRollSpinBox.value, postRollSpinBox.locale) + " s"
                                    font.pixelSize: 13
                                    color: "#f8fafc"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: !postRollSpinBox.editable
                                    validator: postRollSpinBox.validator
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                }
                            }

                            Text {
                                text: "Extends capture after stop signal to preserve reverberation."
                                font.pixelSize: 11
                                color: "#64748b"
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#242933"
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
                                    color: "#f8fafc"
                                }

                                Text {
                                    text: "Arm capture and automatically begin recording when input audio exceeds threshold."
                                    font.pixelSize: 11
                                    color: "#94a3b8"
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
                                color: "#e2e8f0"
                            }

                            SpinBox {
                                id: autoStartSpinBox
                                from: -90
                                to: 0
                                stepSize: 1
                                value: root.vm ? Math.round(root.vm.autoStartThreshold) : -30
                                editable: true
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.autoStartThreshold = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 120
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: autoStartSpinBox.activeFocus ? "#38bdf8" : "#334155"
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: autoStartSpinBox.value + " dBFS"
                                    font.pixelSize: 13
                                    color: "#f8fafc"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                }
                            }

                            Text {
                                text: "Default is -30 dBFS."
                                font.pixelSize: 11
                                color: "#64748b"
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#242933"
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
                                    color: "#f8fafc"
                                }

                                Text {
                                    text: "Automatically finalize recording when input drops below silence threshold."
                                    font.pixelSize: 11
                                    color: "#94a3b8"
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
                                color: "#e2e8f0"
                            }

                            SpinBox {
                                id: autoStopDurationSpinBox
                                from: 1
                                to: 60
                                stepSize: 1
                                value: root.vm ? root.vm.autoStopSilenceDuration : 3
                                editable: true
                                onValueModified: {
                                    if (root.vm) {
                                        root.vm.autoStopSilenceDuration = value
                                    }
                                }
                                background: Rectangle {
                                    implicitWidth: 120
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: autoStopDurationSpinBox.activeFocus ? "#38bdf8" : "#334155"
                                    border.width: 1
                                }
                                contentItem: TextInput {
                                    text: autoStopDurationSpinBox.value + " s"
                                    font.pixelSize: 13
                                    color: "#f8fafc"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                }
                            }

                            Text {
                                text: "Continuous silence before trigger."
                                font.pixelSize: 11
                                color: "#64748b"
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
                color: "#161920"
                border.color: "#242933"
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
                            color: "#f8fafc"
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
                                color: "#f8fafc"
                            }

                            Text {
                                text: "Automatically transcode a secondary delivery format via ffmpeg when capture finishes."
                                font.pixelSize: 11
                                color: "#94a3b8"
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
                            color: "#e2e8f0"
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
                                delegate: ItemDelegate {
                                    width: exportFormatCombo.width
                                    contentItem: Text {
                                        text: modelData
                                        color: highlighted ? "#38bdf8" : "#f8fafc"
                                        font.pixelSize: 13
                                    }
                                    background: Rectangle {
                                        color: highlighted ? "#1e293b" : "#0f172a"
                                    }
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    rightPadding: 24
                                    text: exportFormatCombo.displayText
                                    font.pixelSize: 13
                                    color: "#f8fafc"
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: exportFormatCombo.activeFocus ? "#38bdf8" : "#334155"
                                    border.width: 1
                                }
                            }

                            Text {
                                text: "Master file remains pristine in original lossless format."
                                font.pixelSize: 12
                                color: "#94a3b8"
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
                        color: "#94a3b8"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: resetButton.hovered ? "#1f242d" : "#161920"
                        border.color: "#334155"
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
                        color: saveButton.enabled ? "#0f172a" : "#64748b"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: saveButton.enabled ? (saveButton.hovered ? "#7dd3fc" : "#38bdf8") : "#1e293b"
                        border.color: saveButton.enabled ? "#38bdf8" : "#334155"
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
