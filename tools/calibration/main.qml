// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: root
    visible: true
    width: 1100
    height: 760
    minimumWidth: 850
    minimumHeight: 620
    title: "LumaSave Panel Calibration Preview"
    color: "#202124"

    property bool compensated: false
    property bool alternating: true
    property real reduction: maximumReduction.value / 100
    property real scale: 1 - reduction

    Settings {
        category: "Calibration-eDP-1"
        property alias perceivedBrightness: perceivedBrightness.value
        property alias shadowDetail: shadowDetail.value
        property alias highlightProtection: highlightProtection.value
        property alias colorIntensity: colorIntensity.value
        property alias maxReduction: maximumReduction.value
        property alias interval: interval.value
    }

    Shortcut { sequence: "Space"; onActivated: root.compensated = !root.compensated }
    Shortcut { sequence: "R"; onActivated: resetDefaults() }

    function linearToSrgb(x) {
        x = Math.max(0, Math.min(1, x))
        return x <= 0.0031308 ? 12.92 * x : 1.055 * Math.pow(x, 1 / 2.4) - 0.055
    }
    function srgbToLinear(x) {
        return x <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4)
    }
    function shownChannel(encoded, luminance) {
        if (!compensated || luminance === 0) return Math.round(encoded * 255)
        let black = 0.002 + (1 - shadowDetail.value / 100) * 0.028
        let shoulder = scale / Math.max(0.001, 1 - scale)
        let mapped = luminance * (1 + shoulder) / (luminance + shoulder)
        let t = Math.max(0, Math.min(1, (luminance - black) / Math.max(0.001, black)))
        t = t * t * (3 - 2 * t)
        let protection = highlightProtection.value / 100
        let target = luminance + (mapped - luminance) * t * (1 - 0.35 * protection)
        target *= perceivedBrightness.value / 100
        let linear = srgbToLinear(encoded) * target / Math.max(luminance, 0.000001)
        // Simulate what reaches the eye after the physical backlight reduction.
        return Math.round(linearToSrgb(linear * scale) * 255)
    }
    function patchColor(r, g, b) {
        let lr = srgbToLinear(r), lg = srgbToLinear(g), lb = srgbToLinear(b)
        let y = 0.2126 * lr + 0.7152 * lg + 0.0722 * lb
        let rr = shownChannel(r, y), gg = shownChannel(g, y), bb = shownChannel(b, y)
        if (compensated) {
            let gray = Math.round(0.2126 * rr + 0.7152 * gg + 0.0722 * bb)
            let sat = colorIntensity.value / 100
            rr = gray + (rr - gray) * sat; gg = gray + (gg - gray) * sat; bb = gray + (bb - gray) * sat
        }
        return Qt.rgba(Math.max(0, Math.min(255, rr)) / 255,
                       Math.max(0, Math.min(255, gg)) / 255,
                       Math.max(0, Math.min(255, bb)) / 255, 1)
    }
    function resetDefaults() {
        perceivedBrightness.value = 100; shadowDetail.value = 50
        highlightProtection.value = 70; colorIntensity.value = 100
        maximumReduction.value = 10; interval.value = 1500
    }

    Timer {
        running: root.alternating
        repeat: true
        interval: interval.value
        onTriggered: root.compensated = !root.compensated
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 26

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Label {
                text: root.compensated ? "B · Reduced backlight + compensation" : "A · Normal"
                color: root.compensated ? "#63d7ff" : "white"
                font.pixelSize: 22; font.bold: true
            }
            Label {
                text: "Space toggles A/B. Adjust until both modes have similar readable detail."
                color: "#c8c8c8"; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }

            GridLayout {
                columns: 4; rowSpacing: 0; columnSpacing: 0
                Layout.fillWidth: true; Layout.fillHeight: true
                Repeater {
                    model: [
                        [0,0,0], [.004,.004,.004], [.02,.02,.02], [.063,.063,.063],
                        [.25,.25,.25], [.5,.5,.5], [.75,.75,.75], [1,1,1],
                        [1,0,0], [0,1,0], [0,0,1], [1,1,0],
                        [0,1,1], [1,0,1], [.15,.08,.02], [.08,.16,.3]
                    ]
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: root.patchColor(modelData[0], modelData[1], modelData[2])
                    }
                }
            }
            RowLayout {
                Button { text: root.alternating ? "Pause alternating" : "Alternate A/B"; onClicked: root.alternating = !root.alternating }
                Button { text: "Toggle now (Space)"; onClicked: root.compensated = !root.compensated }
                Item { Layout.fillWidth: true }
                Label { text: "Simulation only — hardware brightness is untouched"; color: "#a8a8a8" }
            }
        }

        Frame {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent; spacing: 13
                Label { text: "Calibration controls"; font.pixelSize: 20; font.bold: true }

                Label { text: "Perceived brightness  " + Math.round(perceivedBrightness.value) + "%" }
                Slider { id: perceivedBrightness; from: 80; to: 120; stepSize: 1; value: 100; Layout.fillWidth: true }
                Label { text: "Shadow detail  " + Math.round(shadowDetail.value) + "%" }
                Slider { id: shadowDetail; from: 0; to: 100; stepSize: 1; value: 50; Layout.fillWidth: true }
                Label { text: "Highlight protection  " + Math.round(highlightProtection.value) + "%" }
                Slider { id: highlightProtection; from: 0; to: 100; stepSize: 1; value: 70; Layout.fillWidth: true }
                Label { text: "Color intensity  " + Math.round(colorIntensity.value) + "%" }
                Slider { id: colorIntensity; from: 80; to: 120; stepSize: 1; value: 100; Layout.fillWidth: true }
                Label { text: "Maximum reduction  " + Math.round(maximumReduction.value) + "%" }
                Slider { id: maximumReduction; from: 0; to: 60; stepSize: 1; value: 10; Layout.fillWidth: true }
                Label { text: "A/B interval  " + (interval.value / 1000).toFixed(1) + " s" }
                Slider { id: interval; from: 500; to: 3000; stepSize: 100; value: 1500; Layout.fillWidth: true }
                Item { Layout.fillHeight: true }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b8b8b8"
                    text: "Exact black is invariant. Bright highlights may not be fully matchable because lowered backlight removes physical headroom."
                }
                Button { text: "Reset safe defaults (R)"; Layout.fillWidth: true; onClicked: root.resetDefaults() }
            }
        }
    }
}
