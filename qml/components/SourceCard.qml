import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ItemDelegate {
    id: root

    property int cardSourceId: model.sourceId !== undefined ? model.sourceId : 0
    property string cardDisplayName: model.displayName !== undefined ? model.displayName : ""
    property string cardSourceKind: model.sourceKind !== undefined ? model.sourceKind : ""
    property int cardChannelCount: model.channelCount !== undefined ? model.channelCount : 2
    property int cardSampleRate: model.sampleRate !== undefined ? model.sampleRate : 48000
    property bool cardIsActive: model.isActive !== undefined ? model.isActive : false
    property bool cardIsSelected: model.isSelected !== undefined ? model.isSelected : (ListView.isCurrentItem || false)

    signal sourceSelected(int sourceId)

    function formatSampleRate(rate) {
        if (!rate || rate <= 0) {
            return "48.0 kHz"
        }
        if (rate >= 1000) {
            var khz = (rate / 1000).toFixed(1)
            return khz + " kHz"
        }
        return rate + " Hz"
    }

    function formatChannels(channels) {
        if (channels === 1) {
            return "Mono"
        }
        if (channels === 2) {
            return "Stereo"
        }
        if (channels > 2) {
            return channels + " ch"
        }
        return "2 ch"
    }

    function kindLabel(kind) {
        if (!kind) {
            return "Source"
        }
        var lower = kind.toLowerCase()
        if (lower.indexOf("app") !== -1 || lower.indexOf("playback") !== -1) {
            return "App Stream"
        }
        if (lower.indexOf("monitor") !== -1 || lower.indexOf("output") !== -1) {
            return "Monitor"
        }
        if (lower.indexOf("hardware") !== -1 || lower.indexOf("input") !== -1) {
            return "Hardware"
        }
        return kind
    }

    function kindBadgeColor(kind) {
        var lower = (kind || "").toLowerCase()
        if (lower.indexOf("app") !== -1 || lower.indexOf("playback") !== -1) {
            return "#1e3a5f"
        }
        if (lower.indexOf("monitor") !== -1 || lower.indexOf("output") !== -1) {
            return "#3b3054"
        }
        if (lower.indexOf("hardware") !== -1 || lower.indexOf("input") !== -1) {
            return "#2a3a30"
        }
        return "#2d333b"
    }

    function kindTextColor(kind) {
        var lower = (kind || "").toLowerCase()
        if (lower.indexOf("app") !== -1 || lower.indexOf("playback") !== -1) {
            return "#60a5fa"
        }
        if (lower.indexOf("monitor") !== -1 || lower.indexOf("output") !== -1) {
            return "#c084fc"
        }
        if (lower.indexOf("hardware") !== -1 || lower.indexOf("input") !== -1) {
            return "#34d399"
        }
        return "#94a3b8"
    }

    implicitWidth: ListView.view ? ListView.view.width : 340
    implicitHeight: 88
    padding: 0

    background: Rectangle {
        radius: 8
        color: root.cardIsSelected ? "#1e293b" : (root.hovered ? "#1f242d" : "#161920")
        border.color: root.cardIsSelected ? "#38bdf8" : (root.hovered ? "#374151" : "#242933")
        border.width: root.cardIsSelected ? 2 : 1

        Rectangle {
            id: activeStripe
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: 2
            visible: root.cardIsActive || root.cardIsSelected
            color: root.cardIsActive ? "#10b981" : (root.cardIsSelected ? "#38bdf8" : "transparent")
        }
    }

    contentItem: RowLayout {
        spacing: 12
        anchors.fill: parent
        anchors.margins: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    implicitWidth: kindText.implicitWidth + 12
                    implicitHeight: 22
                    radius: 4
                    color: root.kindBadgeColor(root.cardSourceKind)

                    Text {
                        id: kindText
                        anchors.centerIn: parent
                        text: root.kindLabel(root.cardSourceKind)
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: root.kindTextColor(root.cardSourceKind)
                    }
                }

                Text {
                    id: nameLabel
                    Layout.fillWidth: true
                    text: root.cardDisplayName ? root.cardDisplayName : ("Source #" + root.cardSourceId)
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: root.cardIsSelected ? "#f8fafc" : "#e2e8f0"
                    elide: Text.ElideRight
                }

                Rectangle {
                    id: activeBadge
                    visible: root.cardIsActive
                    implicitWidth: activeRow.implicitWidth + 10
                    implicitHeight: 22
                    radius: 11
                    color: "#064e3b"
                    border.color: "#059669"
                    border.width: 1

                    RowLayout {
                        id: activeRow
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: "#34d399"
                        }

                        Text {
                            text: "LIVE"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: "#a7f3d0"
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "ID: " + root.cardSourceId
                    font.pixelSize: 12
                    color: "#64748b"
                }

                Rectangle {
                    width: 3
                    height: 3
                    radius: 1.5
                    color: "#475569"
                }

                Text {
                    text: root.formatChannels(root.cardChannelCount)
                    font.pixelSize: 12
                    color: "#94a3b8"
                }

                Rectangle {
                    width: 3
                    height: 3
                    radius: 1.5
                    color: "#475569"
                }

                Text {
                    text: root.formatSampleRate(root.cardSampleRate)
                    font.pixelSize: 12
                    color: "#94a3b8"
                }
            }
        }
    }

    onClicked: {
        root.sourceSelected(root.cardSourceId)
        if (ListView.view && ListView.view.model && typeof ListView.view.model.selectSourceById === "function") {
            ListView.view.model.selectSourceById(root.cardSourceId)
        }
    }
}
