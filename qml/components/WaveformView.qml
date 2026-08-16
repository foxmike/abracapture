import QtQuick

Item {
    id: root

    property var waveformBridge: null
    property color backgroundColor: "#161920"
    property color borderColor: "#242933"
    property color waveformColor: "#38bdf8"
    property color centerLineColor: "#334155"
    property color meterTrackColor: "#0f172a"
    property color meterColor: "#10b981"
    property color warningColor: "#f59e0b"
    property color clipColor: "#ef4444"
    property color rmsColor: "#e2e8f0"

    readonly property var waveformSamples: waveformBridge ? waveformBridge.waveformSamples : []
    readonly property real peakLevel: boundedLevel(waveformBridge ? waveformBridge.peakLevel : 0)
    readonly property real rmsLevel: boundedLevel(waveformBridge ? waveformBridge.rmsLevel : 0)

    function boundedLevel(value) {
        var numericValue = Number(value)
        return isFinite(numericValue) ? Math.max(0, Math.min(1, numericValue)) : 0
    }

    implicitWidth: 480
    implicitHeight: 160

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root.backgroundColor
        border.color: root.borderColor
        border.width: 1
    }

    Canvas {
        id: waveformCanvas

        anchors.left: parent.left
        anchors.right: meterPanel.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 12
        renderStrategy: Canvas.Cooperative

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var context = getContext("2d")
            context.reset()
            context.clearRect(0, 0, width, height)

            var centerY = height / 2
            context.beginPath()
            context.moveTo(0, centerY)
            context.lineTo(width, centerY)
            context.lineWidth = 1
            context.strokeStyle = root.centerLineColor
            context.stroke()

            var samples = root.waveformSamples
            if (!samples || samples.length === 0 || width <= 0 || height <= 0) {
                return
            }

            var amplitude = Math.max(0, centerY - 2)
            var denominator = Math.max(1, samples.length - 1)
            context.beginPath()
            for (var index = 0; index < samples.length; ++index) {
                var sample = Number(samples[index])
                if (!isFinite(sample)) {
                    sample = 0
                }
                sample = Math.max(-1, Math.min(1, sample))
                var x = index * width / denominator
                var y = centerY - sample * amplitude
                if (index === 0) {
                    context.moveTo(x, y)
                } else {
                    context.lineTo(x, y)
                }
            }
            context.lineWidth = 1.5
            context.lineJoin = "round"
            context.strokeStyle = root.waveformColor
            context.stroke()
        }
    }

    Item {
        id: meterPanel

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 24

        Rectangle {
            id: meterTrack

            anchors.fill: parent
            radius: 4
            color: root.meterTrackColor
            border.color: root.borderColor
            border.width: 1
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 2
                height: Math.max(0, (parent.height - 4) * root.peakLevel)
                radius: 2
                color: root.peakLevel >= 0.98 ? root.clipColor
                                               : (root.peakLevel >= 0.8
                                                  ? root.warningColor : root.meterColor)

                Behavior on height {
                    NumberAnimation { duration: 50 }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 1
                y: Math.max(1, parent.height - 2 - (parent.height - 4) * root.rmsLevel)
                height: 2
                color: root.rmsColor
            }
        }
    }

    Connections {
        target: root.waveformBridge
        ignoreUnknownSignals: true

        function onWaveformSamplesChanged() {
            waveformCanvas.requestPaint()
        }
    }

    onWaveformBridgeChanged: waveformCanvas.requestPaint()
    Component.onCompleted: waveformCanvas.requestPaint()
}
