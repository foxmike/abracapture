import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme" as Theme

Item {
    id: root

    property var recordingsViewModel: null
    signal requestExportSelected(var entries)

    readonly property var vm: root.recordingsViewModel
    property int selectedIndex: root.vm ? root.vm.selectedIndex : -1

    function durationLabel(seconds) {
        var safeSeconds = Math.max(0, Math.floor(Number(seconds) || 0))
        var hours = Math.floor(safeSeconds / 3600)
        var minutes = Math.floor((safeSeconds % 3600) / 60)
        var remainingSeconds = safeSeconds % 60
        function pad(value) { return value < 10 ? "0" + value : value.toString() }
        if (hours > 0) {
            return pad(hours) + ":" + pad(minutes) + ":" + pad(remainingSeconds)
        }
        return pad(minutes) + ":" + pad(remainingSeconds)
    }

    function timestampLabel(dt) {
        if (!dt) return "—"
        if (typeof dt === "string") {
            if (dt.length === 0) return "—"
            return dt.replace("T", " ").substring(0, 19)
        }
        try {
            return Qt.formatDateTime(dt, "yyyy-MM-dd hh:mm:ss")
        } catch (e) {
            return dt.toString()
        }
    }

    function sampleRateLabel(sr) {
        var num = Number(sr) || 0
        if (num <= 0) return "—"
        return (num / 1000.0).toFixed(1) + " kHz"
    }

    function bitDepthLabel(bd) {
        var num = Number(bd) || 0
        if (num === 32) return "32-bit float"
        if (num > 0) return num + "-bit"
        return "—"
    }

    function channelsLabel(ch) {
        var num = Number(ch) || 0
        if (num === 1) return "Mono"
        if (num === 2) return "Stereo"
        if (num > 0) return num + " ch"
        return "—"
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.AbracaptureTheme.background
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: "Recordings"
                    color: Theme.AbracaptureTheme.text
                    font.pixelSize: 23
                    font.weight: Font.Bold
                }

                Text {
                    text: "Browse captured audio sessions, inspect master metadata, and manage history."
                    color: Theme.AbracaptureTheme.textSecondary
                    font.pixelSize: 13
                }
            }

            Rectangle {
                implicitWidth: countText.implicitWidth + 20
                implicitHeight: 28
                radius: 14
                    color: Theme.AbracaptureTheme.surface
                    border.color: Theme.AbracaptureTheme.border
                border.width: 1

                Text {
                    id: countText
                    anchors.centerIn: parent
                    text: (root.vm ? root.vm.count : 0) + " items"
                    color: Theme.AbracaptureTheme.textSecondary
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }

            Button {
                id: refreshButton
                text: "Refresh"
                font.pixelSize: 12
                implicitHeight: 32
                contentItem: Text {
                    text: refreshButton.text
                    color: Theme.AbracaptureTheme.text
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: refreshButton.down ? Theme.AbracaptureTheme.controlBackgroundHover : (refreshButton.hovered ? Theme.AbracaptureTheme.surface : Theme.AbracaptureTheme.controlBackgroundHover)
                    radius: 6
                    border.color: Theme.AbracaptureTheme.border
                    border.width: 1
                }
                onClicked: {
                    if (root.vm) {
                        root.vm.refreshEntries()
                    }
                }
            }

            Button {
                text: "Export Selected" + (root.vm && root.vm.exportSelectionCount > 0 ? " (" + root.vm.exportSelectionCount + ")" : "")
                enabled: root.vm && root.vm.exportSelectionCount > 0
                onClicked: root.requestExportSelected(root.vm.exportSelectedEntries())
            }
        }

        // Main content area
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width >= 1060 ? 2 : 1
            columnSpacing: 20
            rowSpacing: 20

            // Left side: Recordings list
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: root.width >= 1060 ? 460 : -1
                radius: 10
                color: Theme.AbracaptureTheme.surface
                border.color: Theme.AbracaptureTheme.border
                border.width: 1

                ListView {
                    id: recordingsListView
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    spacing: 8
                    model: root.vm
                    currentIndex: root.selectedIndex

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    // Empty state
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - 40
                        spacing: 12
                        visible: !root.vm || root.vm.count === 0

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 48
                            height: 48
                            radius: 24
                            color: Theme.AbracaptureTheme.controlBackgroundHover
                            border.color: Theme.AbracaptureTheme.border
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "♫"
                    color: Theme.AbracaptureTheme.disabledText
                                font.pixelSize: 20
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "No recordings in index"
                    color: Theme.AbracaptureTheme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Captured audio sessions will appear here automatically with full sample and format metadata."
                    color: Theme.AbracaptureTheme.textSecondary
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    delegate: Rectangle {
                        id: itemCard
                        width: recordingsListView.width - (recordingsListView.ScrollBar.vertical.visible ? 12 : 0)
                        implicitHeight: itemLayout.implicitHeight + 20
                        radius: 8
                        color: isSelected ? Theme.AbracaptureTheme.surface : (cardMouseArea.containsMouse ? Theme.AbracaptureTheme.controlBackgroundHover : Theme.AbracaptureTheme.controlBackground)
                        border.color: isSelected ? Theme.AbracaptureTheme.primary : (cardMouseArea.containsMouse ? Theme.AbracaptureTheme.border : Theme.AbracaptureTheme.divider)
                        border.width: isSelected ? 2 : 1

                        readonly property bool isSelected: root.selectedIndex === index
                        readonly property string itemFilePath: model.filePath || ""
                        readonly property string itemFileName: model.fileName || ""
                        readonly property string itemFormat: model.format || ""
                        readonly property var itemSampleRate: model.sampleRate || 0
                        readonly property var itemBitDepth: model.bitDepth || 0
                        readonly property var itemChannels: model.channels || 0
                        readonly property var itemDuration: model.duration || 0.0
                        readonly property var itemTimestamp: model.timestamp || ""
                        readonly property bool itemIsNative: model.isNative !== undefined ? model.isNative : true
                        readonly property string itemSidecarPath: model.sidecarPath || ""

                        CheckBox {
                            id: exportSelectionCheck
                            z: 2
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 8
                            checked: root.vm ? root.vm.isSelectedForExport(index) : false
                            onToggled: if (root.vm) root.vm.toggleExportSelection(index)
                            ToolTip.visible: hovered
                            ToolTip.text: "Include in manual export"
                        }

                        MouseArea {
                            id: cardMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                root.selectedIndex = index
                                if (root.vm) {
                                    root.vm.selectedIndex = index
                                }
                            }
                        }

                        ColumnLayout {
                            id: itemLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            // Card Top Row: Badges & Timestamp
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                // Format badge
                                Rectangle {
                                    implicitWidth: formatBadgeText.implicitWidth + 12
                                    implicitHeight: 20
                                    radius: 4
                                    color: "#1e3a5f"
                                    border.color: "#2563eb"
                                    border.width: 1

                                    Text {
                                        id: formatBadgeText
                                        anchors.centerIn: parent
                                        text: itemCard.itemFormat ? itemCard.itemFormat.toUpperCase() : "AUDIO"
                                        color: "#60a5fa"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                }

                                // Native vs Converted badge
                                Rectangle {
                                    implicitWidth: nativeBadgeText.implicitWidth + 12
                                    implicitHeight: 20
                                    radius: 4
                                    color: itemCard.itemIsNative ? "#064e3b" : "#78350f"
                                    border.color: itemCard.itemIsNative ? "#059669" : "#d97706"
                                    border.width: 1

                                    Text {
                                        id: nativeBadgeText
                                        anchors.centerIn: parent
                                        text: itemCard.itemIsNative ? "NATIVE" : "CONVERTED"
                                        color: itemCard.itemIsNative ? "#34d399" : "#fbbf24"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: root.timestampLabel(itemCard.itemTimestamp)
                    color: Theme.AbracaptureTheme.disabledText
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                }
                            }

                            // Card Middle: Filename
                            Text {
                                Layout.fillWidth: true
                                text: itemCard.itemFileName.length > 0 ? itemCard.itemFileName : itemCard.itemFilePath
                    color: Theme.AbracaptureTheme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideMiddle
                            }

                            // Card Bottom Row: Specs & Quick Action Buttons
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                RowLayout {
                                    spacing: 12

                                    Text {
                                        text: root.durationLabel(itemCard.itemDuration)
                                        color: "#38bdf8"
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                        font.weight: Font.Bold
                                    }

                                    Text {
                                        text: "·"
                                        color: "#475569"
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        text: root.sampleRateLabel(itemCard.itemSampleRate)
                    color: Theme.AbracaptureTheme.textSecondary
                                        font.pixelSize: 11
                                    }

                                    Text {
                                        text: "·"
                                        color: "#475569"
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        text: root.bitDepthLabel(itemCard.itemBitDepth)
                    color: Theme.AbracaptureTheme.textSecondary
                                        font.pixelSize: 11
                                    }

                                    Text {
                                        text: "·"
                                        color: "#475569"
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        text: root.channelsLabel(itemCard.itemChannels)
                    color: Theme.AbracaptureTheme.textSecondary
                                        font.pixelSize: 11
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                RowLayout {
                                    spacing: 6

                                    // Reveal Button
                                    Button {
                                        id: playBtn
                                        implicitWidth: 26
                                        implicitHeight: 26
                                        hoverEnabled: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Play recording"
                                        contentItem: Text { text: "▶"; color: Theme.AbracaptureTheme.primary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: playBtn.hovered ? Theme.AbracaptureTheme.surface : "transparent"; radius: 4; border.color: Theme.AbracaptureTheme.primary }
                                        onClicked: if (root.vm) root.vm.playEntry(index)
                                    }

                                    Button {
                                        id: revealBtn
                                        implicitWidth: 26
                                        implicitHeight: 26
                                        hoverEnabled: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Reveal in File Manager"
                                        contentItem: Text {
                                            text: "📂"
                                            font.pixelSize: 12
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: revealBtn.down ? Theme.AbracaptureTheme.controlBackgroundHover : (revealBtn.hovered ? Theme.AbracaptureTheme.surface : Theme.AbracaptureTheme.controlBackgroundHover)
                                            radius: 4
                                            border.color: Theme.AbracaptureTheme.border
                                            border.width: 1
                                        }
                                        onClicked: {
                                            if (root.vm) {
                                                root.vm.revealInFileManager(index)
                                            }
                                        }
                                    }

                                    // Delete Button
                                    Button {
                                        id: deleteBtn
                                        implicitWidth: 26
                                        implicitHeight: 26
                                        hoverEnabled: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Remove from index"
                                        contentItem: Text {
                                            text: "🗑"
                                            font.pixelSize: 11
                                            color: "#f87171"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: deleteBtn.down ? Theme.AbracaptureTheme.error : (deleteBtn.hovered ? Theme.AbracaptureTheme.error : Theme.AbracaptureTheme.controlBackgroundHover)
                                            radius: 4
                                            border.color: deleteBtn.hovered ? Theme.AbracaptureTheme.error : Theme.AbracaptureTheme.border
                                            border.width: 1
                                        }
                                        onClicked: {
                                            if (root.vm) {
                                                root.vm.removeEntry(index)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Right side: Details Panel
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: Theme.AbracaptureTheme.surface
                border.color: Theme.AbracaptureTheme.border
                border.width: 1

                readonly property var currentEntry: {
                    if (!root.vm || root.selectedIndex < 0) return null
                    return root.vm.get(root.selectedIndex)
                }

                // Placeholder when no item selected
                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 12
                    visible: !parent.currentEntry

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48
                        height: 48
                        radius: 24
                        color: Theme.AbracaptureTheme.controlBackgroundHover
                        border.color: Theme.AbracaptureTheme.border
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "🔍"
                            font.pixelSize: 18
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Recording details"
                    color: Theme.AbracaptureTheme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Select a recording from the list to inspect audio format specifications, source file paths, and sidecar metadata."
                    color: Theme.AbracaptureTheme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // Details Content
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 18
                    clip: true
                    visible: parent.currentEntry !== null && parent.currentEntry !== undefined

                    ColumnLayout {
                        width: parent.width
                        spacing: 16

                        // Header & Title
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    implicitWidth: detailFormatText.implicitWidth + 12
                                    implicitHeight: 22
                                    radius: 4
                                    color: "#1e3a5f"
                                    border.color: "#2563eb"
                                    border.width: 1

                                    Text {
                                        id: detailFormatText
                                        anchors.centerIn: parent
                                        text: parent.parent.parent.parent.parent.parent.currentEntry ? (parent.parent.parent.parent.parent.parent.currentEntry.format || "AUDIO").toUpperCase() : ""
                                        color: "#60a5fa"
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                    }
                                }

                                Rectangle {
                                    implicitWidth: detailNativeText.implicitWidth + 12
                                    implicitHeight: 22
                                    radius: 4
                                    color: (parent.parent.parent.parent.parent.parent.currentEntry && parent.parent.parent.parent.parent.parent.currentEntry.isNative) ? "#064e3b" : "#78350f"
                                    border.color: (parent.parent.parent.parent.parent.parent.currentEntry && parent.parent.parent.parent.parent.parent.currentEntry.isNative) ? "#059669" : "#d97706"
                                    border.width: 1

                                    Text {
                                        id: detailNativeText
                                        anchors.centerIn: parent
                                        text: (parent.parent.parent.parent.parent.parent.currentEntry && parent.parent.parent.parent.parent.parent.currentEntry.isNative) ? "NATIVE SIGNAL PATH" : "CONVERTED SIGNAL PATH"
                                        color: (parent.parent.parent.parent.parent.parent.currentEntry && parent.parent.parent.parent.parent.parent.currentEntry.isNative) ? "#34d399" : "#fbbf24"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.currentEntry.fileName : ""
                    color: Theme.AbracaptureTheme.text
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }
                        }

                        // Specs Grid
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10

                            // Duration Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 64
                                radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                                border.color: Theme.AbracaptureTheme.divider
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 2

                                    Text {
                                        text: "DURATION"
                    color: Theme.AbracaptureTheme.disabledText
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: {
                                            var d = parent.parent.parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.parent.parent.currentEntry.duration : 0
                                            return root.durationLabel(d) + " (" + (Number(d) || 0).toFixed(2) + "s)"
                                        }
                                        color: "#38bdf8"
                                        font.pixelSize: 13
                                        font.family: "monospace"
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            // Sample Rate Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 64
                                radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                                border.color: Theme.AbracaptureTheme.divider
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 2

                                    Text {
                                        text: "SAMPLE RATE"
                    color: Theme.AbracaptureTheme.disabledText
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: {
                                            var sr = parent.parent.parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.parent.parent.currentEntry.sampleRate : 0
                                            return root.sampleRateLabel(sr) + " (" + sr + " Hz)"
                                        }
                    color: Theme.AbracaptureTheme.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            // Bit Depth Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 64
                                radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                                border.color: Theme.AbracaptureTheme.divider
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 2

                                    Text {
                                        text: "BIT DEPTH"
                    color: Theme.AbracaptureTheme.disabledText
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: root.bitDepthLabel(parent.parent.parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.parent.parent.currentEntry.bitDepth : 0)
                    color: Theme.AbracaptureTheme.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            // Channels Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 64
                                radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                                border.color: Theme.AbracaptureTheme.divider
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 2

                                    Text {
                                        text: "CHANNELS"
                    color: Theme.AbracaptureTheme.disabledText
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: root.channelsLabel(parent.parent.parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.parent.parent.currentEntry.channels : 0)
                    color: Theme.AbracaptureTheme.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }

                        // Timestamp Information
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: timestampLayout.implicitHeight + 16
                            radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                            border.color: Theme.AbracaptureTheme.divider
                            border.width: 1

                            ColumnLayout {
                                id: timestampLayout
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 2

                                Text {
                                    text: "RECORDED AT"
                    color: Theme.AbracaptureTheme.disabledText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    text: root.timestampLabel(parent.parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.parent.currentEntry.timestamp : "")
                                    color: Theme.AbracaptureTheme.text
                                    font.pixelSize: 12
                                    font.family: "monospace"
                                }
                            }
                        }

                        // File Path Section
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: filePathLayout.implicitHeight + 16
                            radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                            border.color: Theme.AbracaptureTheme.divider
                            border.width: 1

                            ColumnLayout {
                                id: filePathLayout
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4

                                Text {
                                    text: "FILE PATH"
                    color: Theme.AbracaptureTheme.disabledText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: parent.parent.parent.parent.parent.currentEntry ? (parent.parent.parent.parent.parent.currentEntry.filePath || "—") : "—"
                                    color: Theme.AbracaptureTheme.text
                                    font.pixelSize: 12
                                    font.family: "monospace"
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        // Sidecar Section (if present)
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: sidecarLayout.implicitHeight + 16
                            radius: 6
                color: Theme.AbracaptureTheme.controlBackground
                            border.color: Theme.AbracaptureTheme.divider
                            border.width: 1
                            visible: !!(parent.parent.parent.parent.currentEntry && parent.parent.parent.parent.currentEntry.sidecarPath && parent.parent.parent.parent.currentEntry.sidecarPath.length > 0)

                            ColumnLayout {
                                id: sidecarLayout
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4

                                Text {
                                    text: "SIDECAR METADATA"
                    color: Theme.AbracaptureTheme.disabledText
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: parent.parent.parent.parent.parent.currentEntry ? parent.parent.parent.parent.parent.currentEntry.sidecarPath : ""
                    color: Theme.AbracaptureTheme.textSecondary
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        // Export uses the shared manual-export workflow.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Button {
                                text: "Export This Recording"
                                onClicked: {
                                    if (root.vm && root.selectedIndex >= 0) {
                                        root.requestExportSelected([root.vm.get(root.selectedIndex)])
                                    }
                                }
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Button {
                                id: detailPlayBtn
                                Layout.fillWidth: true
                                implicitHeight: 38
                                text: "Play"
                                onClicked: if (root.vm && root.selectedIndex >= 0) root.vm.playEntry(root.selectedIndex)
                            }

                            Button {
                                id: detailRevealBtn
                                Layout.fillWidth: true
                                implicitHeight: 38
                                contentItem: Text {
                                    text: "Reveal in File Manager"
                    color: Theme.AbracaptureTheme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: detailRevealBtn.down ? "#1d4ed8" : (detailRevealBtn.hovered ? "#2563eb" : "#1e40af")
                                    radius: 6
                                }
                                onClicked: {
                                    if (root.vm && root.selectedIndex >= 0) {
                                        root.vm.revealInFileManager(root.selectedIndex)
                                    }
                                }
                            }

                            Button {
                                id: detailDeleteBtn
                                Layout.fillWidth: true
                                implicitHeight: 38
                                contentItem: Text {
                                    text: "Remove from Index"
                                    color: "#fca5a5"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: detailDeleteBtn.down ? Theme.AbracaptureTheme.error : (detailDeleteBtn.hovered ? Theme.AbracaptureTheme.error : Theme.AbracaptureTheme.controlBackgroundHover)
                                    radius: 6
                                    border.color: detailDeleteBtn.hovered ? Theme.AbracaptureTheme.error : Theme.AbracaptureTheme.border
                                    border.width: 1
                                }
                                onClicked: {
                                    if (root.vm && root.selectedIndex >= 0) {
                                        root.vm.removeEntry(root.selectedIndex)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
