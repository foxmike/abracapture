import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme" as Theme

Item {
    id: root

    property var settingsViewModel: null
    property var sourceListModel: null
    property var exportManager: null
    property var manualExportViewModel: null
    readonly property var vm: root.settingsViewModel
    property var currentSource: ({})
    property int queueVersion: 0
    property int settingsVersion: 0

    function refreshSource() {
        if (root.sourceListModel && root.sourceListModel.selectedSourceId !== 0) {
            root.currentSource = root.sourceListModel.getSourceById(root.sourceListModel.selectedSourceId)
        } else {
            root.currentSource = ({})
        }
    }

    function rateLabel(value) {
        if (value === "preserve") return "Preserve Source"
        var numeric = Number(value)
        return (numeric / 1000).toFixed(numeric % 1000 === 0 ? 0 : 1) + " kHz"
    }

    function sampleFormatLabel(value) {
        if (value === "s16") return "16-bit PCM"
        if (value === "s24") return "24-bit PCM"
        if (value === "s32") return "32-bit PCM"
        if (value === "f32") return "32-bit Float"
        return value
    }

    function sourceFormatText() {
        if (!root.currentSource || !root.currentSource.sampleRate) return "Native stream properties unavailable"
        var rate = Number(root.currentSource.sampleRate)
        var rateText = (rate / 1000).toFixed(rate % 1000 === 0 ? 0 : 1) + " kHz"
        var format = root.currentSource.sampleFormat || "format unavailable"
        var channels = Number(root.currentSource.channelCount || 0)
        var channelText = channels === 1 ? "Mono" : (channels === 2 ? "Stereo" : (channels > 0 ? channels + " channels" : "channels unavailable"))
        return rateText + " • " + format + " • " + channelText
    }

    function fileName(path) {
        if (!path) return ""
        var parts = String(path).split(/[\\/]/)
        return parts[parts.length - 1]
    }

    function previewSource() {
        if (root.currentSource && Object.keys(root.currentSource).length > 0) return root.currentSource
        return { displayName: "Capture", appName: "Capture", sampleRate: 0, channelCount: 0 }
    }

    Component.onCompleted: refreshSource()
    onVisibleChanged: if (visible && root.manualExportViewModel) root.manualExportViewModel.refreshCaptureSessions()

    Connections {
        target: root.sourceListModel ? root.sourceListModel : null
        ignoreUnknownSignals: true
        function onSelectionChanged() { root.refreshSource() }
        function onDataChanged() { root.refreshSource() }
    }

    Connections {
        target: root.vm ? root.vm : null
        ignoreUnknownSignals: true
        function onAutomaticExportProfileChanged() { root.settingsVersion += 1 }
        function onUseMediaMetadataForAutomatedCaptureNamesChanged() { root.settingsVersion += 1 }
    }

    Connections {
        target: root.exportManager ? root.exportManager : null
        ignoreUnknownSignals: true
        function onQueueChanged() { root.queueVersion += 1 }
    }

    FileDialog {
        id: manualFileDialog
        title: "Choose a WAV or FLAC master"
        nameFilters: ["Audio masters (*.wav *.flac)"]
        onAccepted: if (root.manualExportViewModel) root.manualExportViewModel.selectFile(selectedFile.toString())
    }

    Rectangle { anchors.fill: parent; color: Theme.AbracaptureTheme.background; z: -1 }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 18

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: "Export"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: Theme.AbracaptureTheme.text
                }
                Text {
                    text: "Configure automatic post-capture delivery files without changing the lossless master."
                    font.pixelSize: 13
                    color: Theme.AbracaptureTheme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: manualLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1

                ColumnLayout {
                    id: manualLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Text { text: "Manual Export"; font.pixelSize: 18; font.weight: Font.Bold; color: Theme.AbracaptureTheme.text }
                    Text {
                        text: "Queue files without interrupting capture. All manual work shares the same export queue as automatic exports."
                        font.pixelSize: 12; color: Theme.AbracaptureTheme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Button { text: "File"; onClicked: manualFileDialog.open() }
                        Button {
                            text: "Selected Recordings"
                            onClicked: if (root.manualExportViewModel) root.manualExportViewModel.activateSelectedRecordings()
                            ToolTip.visible: hovered
                            ToolTip.text: "Select masters on the Recordings page, then choose Export Selected."
                        }
                        Button {
                            text: "Directory"
                            onClicked: {
                                if (!root.manualExportViewModel) return
                                var directory = appController.selectOutputDirectory(root.manualExportViewModel.selectedDirectory)
                                if (directory.length > 0) root.manualExportViewModel.selectDirectory(directory)
                            }
                        }
                        Button {
                            text: "Most Recent Capture Session"
                            onClicked: if (root.manualExportViewModel) root.manualExportViewModel.selectMostRecentCaptureSession()
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        visible: root.manualExportViewModel && root.manualExportViewModel.captureSessions.length > 0
                        Layout.fillWidth: true
                        Text { text: "Capture session"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                        ComboBox {
                            id: sessionCombo
                            Layout.fillWidth: true
                            model: root.manualExportViewModel ? root.manualExportViewModel.captureSessions : []
                            textRole: "label"
                            delegate: ItemDelegate {
                                width: sessionCombo.width
                                text: modelData.timestamp + " — " + modelData.count + " masters" + (modelData.albumTitle ? " — " + modelData.albumTitle : "")
                            }
                            contentItem: Text {
                                leftPadding: 12
                                text: sessionCombo.currentIndex >= 0 && sessionCombo.model.length > 0
                                      ? sessionCombo.model[sessionCombo.currentIndex].timestamp + " — " + sessionCombo.model[sessionCombo.currentIndex].count + " masters"
                                      : "Choose a capture session"
                                color: Theme.AbracaptureTheme.text
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            onActivated: function(index) {
                                if (root.manualExportViewModel && model[index]) root.manualExportViewModel.selectCaptureSession(model[index].sessionId)
                            }
                        }
                    }
                    Text {
                        visible: root.manualExportViewModel && root.manualExportViewModel.sourceMode === "session" &&
                                 sessionCombo.currentIndex >= 0 && sessionCombo.model.length > sessionCombo.currentIndex &&
                                 sessionCombo.model[sessionCombo.currentIndex].tracks
                        text: "Tracks: " + sessionCombo.model[sessionCombo.currentIndex].tracks
                        color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        visible: root.manualExportViewModel && root.manualExportViewModel.sourceMode === "directory"
                        Layout.fillWidth: true
                        CheckBox {
                            checked: root.manualExportViewModel ? root.manualExportViewModel.includeSubdirectories : false
                            onToggled: if (root.manualExportViewModel) root.manualExportViewModel.includeSubdirectories = checked
                        }
                        Text { text: "Include subdirectories"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                        Text { text: "Off by default"; color: Theme.AbracaptureTheme.disabledText; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        text: root.manualExportViewModel ? root.manualExportViewModel.sourceCount + " source file" + (root.manualExportViewModel.sourceCount === 1 ? "" : "s") : "0 source files"
                        font.pixelSize: 13; font.weight: Font.DemiBold; color: Theme.AbracaptureTheme.primary
                    }
                    Repeater {
                        model: root.manualExportViewModel ? root.manualExportViewModel.sources.slice(0, 5) : []
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: modelData.fileName + "  •  " + modelData.codec + "  •  " +
                                      (modelData.sampleRate ? (Number(modelData.sampleRate) / 1000).toFixed(1) + " kHz" : "rate unknown") + "  •  " +
                                      (modelData.channels ? modelData.channels + " ch" : "channels unknown") +
                                      (modelData.sampleFormat ? "  •  " + modelData.sampleFormat : (modelData.bitDepth ? "  •  " + modelData.bitDepth + "-bit" : ""))
                                font.pixelSize: 11; color: Theme.AbracaptureTheme.textSecondary; elide: Text.ElideMiddle; Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.filePath
                                font.pixelSize: 10; color: Theme.AbracaptureTheme.disabledText; elide: Text.ElideMiddle; Layout.fillWidth: true
                            }
                        }
                    }
                    Text {
                        visible: root.manualExportViewModel && root.manualExportViewModel.sourceCount > 5
                        text: "+ " + (root.manualExportViewModel.sourceCount - 5) + " more"
                        font.pixelSize: 11; color: Theme.AbracaptureTheme.disabledText
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.AbracaptureTheme.divider }

                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            checked: root.manualExportViewModel ? root.manualExportViewModel.useAutomaticProfile : true
                            onToggled: if (root.manualExportViewModel) root.manualExportViewModel.useAutomaticProfile = checked
                        }
                        Text { text: "Use Automatic Export Profile"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                        Text { text: root.manualExportViewModel ? root.manualExportViewModel.profileSummary : ""; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 11; Layout.fillWidth: true }
                    }

                    ColumnLayout {
                        visible: root.manualExportViewModel && !root.manualExportViewModel.useAutomaticProfile
                        Layout.fillWidth: true; Layout.leftMargin: 32; spacing: 8
                        RowLayout {
                            Text { text: "Preset"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualPresetCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.presetOptions : []
                                readonly property var labels: ["MP3 320", "MP3 256", "FLAC 16-bit", "FLAC 24-bit", "WAV 24-bit", "Preserve Source Lossless", "Custom"]
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(root.manualExportViewModel.manualPreset)) : 0
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualPreset = model[index] }
                                contentItem: Text { leftPadding: 12; text: manualPresetCombo.labels[manualPresetCombo.currentIndex]; color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Text { text: "Format"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualFormatCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.formatOptions : []
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(root.manualExportViewModel.manualFormat)) : 0
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualFormat = model[index] }
                                contentItem: Text { leftPadding: 12; text: manualFormatCombo.currentText.toUpperCase(); color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                            }
                            Text { text: "Sample Rate"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualRateCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.sampleRateOptions : []
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(root.manualExportViewModel.manualSampleRate === 0 ? "preserve" : String(root.manualExportViewModel.manualSampleRate))) : 0
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualSampleRate = model[index] === "preserve" ? 0 : Number(model[index]) }
                                contentItem: Text { leftPadding: 12; text: root.rateLabel(manualRateCombo.currentText); color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            visible: root.manualExportViewModel && root.manualExportViewModel.manualFormat === "mp3"
                            Text { text: "Bitrate"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualBitrateCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.mp3BitrateOptions : []
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(String(root.manualExportViewModel.manualMp3Bitrate))) : 3
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualMp3Bitrate = Number(model[index]) }
                                contentItem: Text { leftPadding: 12; text: manualBitrateCombo.currentText + " kbps"; color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                        RowLayout {
                            visible: root.manualExportViewModel && root.manualExportViewModel.manualFormat === "flac"
                            Text { text: "Bit Depth"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualFlacDepthCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.flacBitDepthOptions : []
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(String(root.manualExportViewModel.manualFlacBitDepth))) : 0
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualFlacBitDepth = Number(model[index]) }
                                contentItem: Text { leftPadding: 12; text: manualFlacDepthCombo.currentText + "-bit"; color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                            }
                            Text { text: "Compression"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualFlacCompressionCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.flacCompressionOptions : []
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(String(root.manualExportViewModel.manualFlacCompressionLevel))) : 5
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualFlacCompressionLevel = Number(model[index]) }
                            }
                        }
                        RowLayout {
                            visible: root.manualExportViewModel && root.manualExportViewModel.manualFormat === "wav"
                            Text { text: "Sample Format"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                            ComboBox {
                                id: manualWavFormatCombo
                                model: root.manualExportViewModel ? root.manualExportViewModel.wavSampleFormatOptions : []
                                currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(root.manualExportViewModel.manualWavSampleFormat)) : 0
                                onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.manualWavSampleFormat = model[index] }
                                contentItem: Text { leftPadding: 12; text: root.sampleFormatLabel(manualWavFormatCombo.currentText); color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Destination"; color: Theme.AbracaptureTheme.text; font.pixelSize: 13 }
                        ComboBox {
                            id: manualDestinationCombo
                            model: ["same", "exports", "custom"]
                            readonly property var labels: ["Same directory as source", "Exports subfolder", "Custom directory"]
                            currentIndex: root.manualExportViewModel ? Math.max(0, model.indexOf(root.manualExportViewModel.destinationPolicy)) : 0
                            onActivated: function(index) { if (root.manualExportViewModel) root.manualExportViewModel.destinationPolicy = model[index] }
                            contentItem: Text { leftPadding: 12; text: manualDestinationCombo.labels[manualDestinationCombo.currentIndex]; color: Theme.AbracaptureTheme.text; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            visible: root.manualExportViewModel && root.manualExportViewModel.destinationPolicy === "custom"
                            text: "Choose…"
                            onClicked: {
                                var directory = appController.selectOutputDirectory(root.manualExportViewModel.customDestinationDirectory)
                                if (directory.length > 0) root.manualExportViewModel.customDestinationDirectory = directory
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Text { visible: root.manualExportViewModel && root.manualExportViewModel.destinationPolicy === "custom"; text: root.manualExportViewModel ? root.manualExportViewModel.customDestinationDirectory : ""; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox { checked: root.manualExportViewModel ? root.manualExportViewModel.useStoredMetadataNames : false; onToggled: if (root.manualExportViewModel) root.manualExportViewModel.useStoredMetadataNames = checked }
                        Text { text: "Use stored track metadata when available"; color: Theme.AbracaptureTheme.text; font.pixelSize: 12 }
                        CheckBox { checked: root.manualExportViewModel ? root.manualExportViewModel.organizeAlbumDirectory : false; enabled: root.manualExportViewModel ? root.manualExportViewModel.useStoredMetadataNames : false; onToggled: if (root.manualExportViewModel) root.manualExportViewModel.organizeAlbumDirectory = checked }
                        Text { text: "Organize by album"; color: Theme.AbracaptureTheme.text; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                    }
                    Text { text: "Preview: " + (root.manualExportViewModel ? root.manualExportViewModel.destinationPreview : ""); color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true }

                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: "Export " + (root.manualExportViewModel ? root.manualExportViewModel.sourceCount : 0) + " File" + (root.manualExportViewModel && root.manualExportViewModel.sourceCount === 1 ? "" : "s")
                            enabled: root.manualExportViewModel && root.manualExportViewModel.sourceCount > 0
                            onClicked: root.manualExportViewModel.enqueueExport()
                        }
                        Text { text: root.manualExportViewModel ? root.manualExportViewModel.statusMessage : ""; color: Theme.AbracaptureTheme.textSecondary; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                    }

                    ColumnLayout {
                        visible: root.manualExportViewModel && root.manualExportViewModel.jobs.length > 0
                        Layout.fillWidth: true; spacing: 4
                        Text { text: "Manual jobs"; font.pixelSize: 14; font.weight: Font.DemiBold; color: Theme.AbracaptureTheme.text }
                        Repeater {
                            model: root.manualExportViewModel ? root.manualExportViewModel.jobs : []
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true; implicitHeight: jobLayout.implicitHeight + 12; radius: 5
                                color: Theme.AbracaptureTheme.controlBackground; border.color: Theme.AbracaptureTheme.divider
                                ColumnLayout {
                                    id: jobLayout; anchors.fill: parent; anchors.margins: 6; spacing: 2
                                    Text { text: root.fileName(modelData.source) + " → " + root.fileName(modelData.output) + "  •  " + modelData.profile; color: Theme.AbracaptureTheme.text; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text { text: modelData.status + (modelData.error ? " — " + modelData.error : ""); color: modelData.status === "Failed" ? Theme.AbracaptureTheme.danger : Theme.AbracaptureTheme.textSecondary; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                                        Button { visible: modelData.canRetry; text: "Retry"; onClicked: root.manualExportViewModel.retry(modelData.jobId) }
                                        Button { visible: modelData.canCancel; text: "Cancel"; onClicked: root.manualExportViewModel.cancelQueued(modelData.jobId) }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: sourceLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1

                ColumnLayout {
                    id: sourceLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8
                    Text {
                        text: "Current source"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: Theme.AbracaptureTheme.text
                    }
                    Text {
                        text: root.currentSource && root.currentSource.displayName
                              ? root.currentSource.displayName : "No source selected"
                        font.pixelSize: 14
                        color: Theme.AbracaptureTheme.text
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.sourceFormatText()
                        font.pixelSize: 13
                        color: Theme.AbracaptureTheme.textSecondary
                    }
                    Text {
                        text: "These are current PipeWire stream properties, not an assertion about the original media bit depth."
                        font.pixelSize: 11
                        color: Theme.AbracaptureTheme.disabledText
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: automaticLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1

                ColumnLayout {
                    id: automaticLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Switch {
                            checked: root.vm ? root.vm.exportEnabled : false
                            onToggled: { if (root.vm) root.vm.exportEnabled = checked }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: "Enable automatic post-capture export"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                color: Theme.AbracaptureTheme.text
                            }
                            Text {
                                text: "One derived file is queued after each successfully finalized master."
                                font.pixelSize: 11
                                color: Theme.AbracaptureTheme.textSecondary
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 48
                        visible: root.vm ? root.vm.exportEnabled : false
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Format"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: formatCombo
                                implicitWidth: 170
                                model: root.vm ? root.vm.automaticExportFormats : []
                                currentIndex: root.vm ? Math.max(0, model.indexOf(root.vm.exportFormat)) : 0
                                onActivated: function(index) { if (root.vm) root.vm.exportFormat = model[index] }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: formatCombo.currentText.toUpperCase()
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Sample Rate"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: rateCombo
                                implicitWidth: 170
                                model: root.vm ? root.vm.automaticExportSampleRateOptions : []
                                currentIndex: {
                                    if (!root.vm) return 0
                                    var selected = root.vm.automaticExportSampleRate === 0 ? "preserve" : String(root.vm.automaticExportSampleRate)
                                    for (var i = 0; i < model.length; ++i) if (model[i] === selected) return i
                                    return 0
                                }
                                onActivated: function(index) {
                                    if (root.vm) root.vm.automaticExportSampleRate = model[index] === "preserve" ? 0 : Number(model[index])
                                }
                                delegate: ItemDelegate {
                                    width: rateCombo.width
                                    contentItem: Text {
                                        text: root.rateLabel(modelData)
                                        color: highlighted ? Theme.AbracaptureTheme.primary : Theme.AbracaptureTheme.text
                                        font.pixelSize: 13
                                    }
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: rateCombo.currentIndex >= 0 ? root.rateLabel(rateCombo.currentText) : "Preserve Source"
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            visible: root.vm && root.vm.exportFormat === "mp3"
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Bitrate"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: bitrateCombo
                                implicitWidth: 170
                                model: root.vm ? root.vm.automaticExportMp3BitrateOptions : []
                                currentIndex: root.vm ? Math.max(0, model.indexOf(String(root.vm.automaticExportMp3Bitrate))) : 3
                                onActivated: function(index) { if (root.vm) root.vm.automaticExportMp3Bitrate = Number(model[index]) }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: bitrateCombo.currentText + " kbps"
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            visible: root.vm && root.vm.exportFormat === "flac"
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Bit Depth"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: flacDepthCombo
                                implicitWidth: 170
                                model: root.vm ? root.vm.automaticExportFlacBitDepthOptions : []
                                currentIndex: root.vm ? Math.max(0, model.indexOf(String(root.vm.automaticExportFlacBitDepth))) : 0
                                onActivated: function(index) { if (root.vm) root.vm.automaticExportFlacBitDepth = Number(model[index]) }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: flacDepthCombo.currentText + "-bit"
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                            Text { text: "Compression"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: flacCompressionCombo
                                implicitWidth: 100
                                model: root.vm ? root.vm.automaticExportFlacCompressionOptions : []
                                currentIndex: root.vm ? Math.max(0, model.indexOf(String(root.vm.automaticExportFlacCompressionLevel))) : 5
                                onActivated: function(index) { if (root.vm) root.vm.automaticExportFlacCompressionLevel = Number(model[index]) }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: flacCompressionCombo.currentText
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        RowLayout {
                            visible: root.vm && root.vm.exportFormat === "wav"
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Sample Format"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: wavFormatCombo
                                implicitWidth: 190
                                model: root.vm ? root.vm.automaticExportWavSampleFormatOptions : []
                                currentIndex: root.vm ? Math.max(0, model.indexOf(root.vm.automaticExportWavSampleFormat)) : 0
                                onActivated: function(index) { if (root.vm) root.vm.automaticExportWavSampleFormat = model[index] }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: root.sampleFormatLabel(wavFormatCombo.currentText)
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Preset"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: presetCombo
                                implicitWidth: 190
                                model: ["mp3_320", "mp3_256", "flac_16", "flac_24", "wav_24", "preserve_lossless", "custom"]
                                readonly property var labels: ["MP3 320", "MP3 256", "FLAC 16-bit", "FLAC 24-bit", "WAV 24-bit", "Preserve Source Lossless", "Custom"]
                                currentIndex: Math.max(0, model.indexOf(root.vm ? root.vm.automaticExportPreset : "custom"))
                                onActivated: function(index) { if (root.vm) root.vm.automaticExportPreset = model[index] }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: presetCombo.labels[presetCombo.currentIndex]
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.AbracaptureTheme.divider }

                        Text {
                            text: "Automatic export"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Theme.AbracaptureTheme.text
                        }
                        Text {
                            text: {
                                root.settingsVersion
                                return root.vm ? root.vm.automaticExportSummary() : ""
                            }
                            font.pixelSize: 15
                            color: Theme.AbracaptureTheme.primary
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: "Destination"; font.pixelSize: 13; color: Theme.AbracaptureTheme.text }
                            ComboBox {
                                id: destinationCombo
                                implicitWidth: 190
                                model: ["same", "exports", "custom"]
                                readonly property var labels: ["Same folder as master", "Exports subfolder", "Custom folder"]
                                currentIndex: root.vm ? Math.max(0, model.indexOf(root.vm.automaticExportDestinationPolicy)) : 0
                                onActivated: function(index) { if (root.vm) root.vm.automaticExportDestinationPolicy = model[index] }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: destinationCombo.labels[destinationCombo.currentIndex]
                                    color: Theme.AbracaptureTheme.text
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 13
                                }
                            }
                            Button {
                                visible: root.vm && root.vm.automaticExportDestinationPolicy === "custom"
                                text: "Choose…"
                                onClicked: {
                                    if (!root.vm) return
                                    var selected = appController.selectOutputDirectory(root.vm.automaticExportCustomDirectory)
                                    if (selected.length > 0) {
                                        root.vm.automaticExportCustomDirectory = selected
                                        root.vm.automaticExportDestinationPolicy = "custom"
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Text {
                            visible: root.vm && root.vm.automaticExportDestinationPolicy === "custom"
                            text: root.vm ? root.vm.automaticExportCustomDirectory : ""
                            font.pixelSize: 11
                            color: Theme.AbracaptureTheme.textSecondary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Text {
                            text: {
                                root.settingsVersion
                                return "Preview: " + (root.vm ? root.vm.automaticExportPreviewPath(root.previewSource()) : "")
                            }
                            font.pixelSize: 11
                            color: Theme.AbracaptureTheme.textSecondary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            CheckBox {
                                checked: root.vm ? root.vm.useMediaMetadataForAutomatedCaptureNames : false
                                onToggled: { if (root.vm) root.vm.useMediaMetadataForAutomatedCaptureNames = checked }
                            }
                            Text {
                                text: "Organize automated captures using available media metadata"
                                font.pixelSize: 12
                                color: Theme.AbracaptureTheme.text
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                        Text {
                            text: "When album and track metadata is available, automated captures can use Album / Track.ext. Manual captures keep the normal naming behavior."
                            font.pixelSize: 11
                            color: Theme.AbracaptureTheme.disabledText
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: queueLayout.implicitHeight + 32
                radius: 12
                color: Theme.AbracaptureTheme.surfaceElevated
                border.color: Theme.AbracaptureTheme.divider
                border.width: 1
                ColumnLayout {
                    id: queueLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8
                    Text {
                        text: "Queue status"
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: Theme.AbracaptureTheme.text
                    }
                    Text {
                        text: root.exportManager && root.exportManager.busy
                              ? (root.exportManager.currentJobSummary().sourcePath
                                 ? "Exporting: " + root.fileName(root.exportManager.currentJobSummary().sourcePath) + " → " + root.fileName(root.exportManager.currentJobSummary().outputPath)
                                 : "Exporting")
                              : "No exports running"
                        font.pixelSize: 13
                        color: Theme.AbracaptureTheme.text
                    }
                    Text {
                        text: root.exportManager ? root.exportManager.queuedJobCount + " queued" : "0 queued"
                        font.pixelSize: 12
                        color: Theme.AbracaptureTheme.textSecondary
                    }
                    Repeater {
                        model: root.exportManager ? root.exportManager.recentJobSummaries(3) : []
                        delegate: Text {
                            required property var modelData
                            text: modelData.state + ": " + root.fileName(modelData.outputPath) + (modelData.error ? " — " + modelData.error : "")
                            font.pixelSize: 11
                            color: modelData.state === "failed" ? Theme.AbracaptureTheme.danger : Theme.AbracaptureTheme.textSecondary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Item { implicitHeight: 20 }
        }
    }
}
