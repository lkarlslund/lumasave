// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore

ApplicationWindow {
    id: root
    visible: true
    width: Screen.availableWidth > 0 ? Math.min(1100, Screen.availableWidth - 40) : 1100
    height: Screen.availableHeight > 0 ? Math.min(760, Screen.availableHeight - 40) : 760
    minimumWidth: 900
    minimumHeight: 560
    title: "LumaSave Panel Calibration Preview"
    color: "#202124"

    property int previewMode: calibrationBackend.previewMode
    // 0 = automatic A/B, 1 = hold A, 2 = hold B.
    property int comparisonState: 1
    property bool deliberateClose: false
    property real reduction: maximumReduction.value / 100
    property real scale: 1 - reduction
    property int calibrationLevel: 100
    property bool profileInitialized: false
    property bool bypassCompensation: false

    onClosing: function(close) {
        if (!deliberateClose) {
            close.accepted = false
            deliberateClose = true
            calibrationBackend.cancelAndQuit()
        }
    }

    Settings {
        category: "Calibration-General"
        property alias maxReduction: maximumReduction.value
        property alias interval: interval.value
    }
    Settings { id: profile25; category: "Calibration-25"; property int perceivedBrightness: 100; property int shadowDetail: 50; property int highlightProtection: 70; property int colorIntensity: 100 }
    Settings { id: profile50; category: "Calibration-50"; property int perceivedBrightness: 100; property int shadowDetail: 50; property int highlightProtection: 70; property int colorIntensity: 100 }
    Settings { id: profile100; category: "Calibration-100"; property int perceivedBrightness: 100; property int shadowDetail: 50; property int highlightProtection: 70; property int colorIntensity: 100 }

    Shortcut { sequence: "Space"; onActivated: selectComparison(root.previewMode === 0 ? 2 : 1) }
    Shortcut { sequence: "R"; onActivated: resetDefaults() }

    function linearToSrgb(x) {
        x = Math.max(0, Math.min(1, x))
        return x <= 0.0031308 ? 12.92 * x : 1.055 * Math.pow(x, 1 / 2.4) - 0.055
    }
    function srgbToLinear(x) {
        return x <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4)
    }
    function shownChannel(encoded, luminance) {
        return Math.round(encoded * 255)
    }
    function patchColor(r, g, b) {
        let lr = srgbToLinear(r), lg = srgbToLinear(g), lb = srgbToLinear(b)
        let y = 0.2126 * lr + 0.7152 * lg + 0.0722 * lb
        let rr = shownChannel(r, y), gg = shownChannel(g, y), bb = shownChannel(b, y)
        return Qt.rgba(Math.max(0, Math.min(255, rr)) / 255,
                       Math.max(0, Math.min(255, gg)) / 255,
                       Math.max(0, Math.min(255, bb)) / 255, 1)
    }
    function resetDefaults() {
        perceivedBrightness.value = 100; shadowDetail.value = 50
        highlightProtection.value = 70; colorIntensity.value = 100
        maximumReduction.value = 10; interval.value = 1500
    }
    function profile(level) { return level === 25 ? profile25 : (level === 50 ? profile50 : profile100) }
    function storeProfile() {
        let p = profile(calibrationLevel)
        p.perceivedBrightness = Math.round(perceivedBrightness.value)
        p.shadowDetail = Math.round(shadowDetail.value)
        p.highlightProtection = Math.round(highlightProtection.value)
        p.colorIntensity = Math.round(colorIntensity.value)
    }
    function selectLevel(level) {
        if (profileInitialized) storeProfile()
        calibrationLevel = level
        let p = profile(level)
        perceivedBrightness.value = p.perceivedBrightness
        shadowDetail.value = p.shadowDetail
        highlightProtection.value = p.highlightProtection
        colorIntensity.value = p.colorIntensity
        profileInitialized = true
        comparisonState = 1
        calibrationBackend.setCalibrationLevel(level)
    }
    function selectComparison(state) {
        comparisonState = state
        if (state === 1) root.setMode(0)
        else if (state === 2) root.setMode(root.bypassCompensation ? 1 : 2)
        else root.setMode(0)
    }
    function setMode(mode) {
        calibrationBackend.setPreviewMode(mode, reduction,
                                          Math.round(perceivedBrightness.value),
                                          Math.round(shadowDetail.value),
                                          Math.round(highlightProtection.value),
                                          Math.round(colorIntensity.value))
    }

    Timer {
        running: root.comparisonState === 0
        repeat: true
        interval: interval.value
        onTriggered: root.setMode(root.previewMode === 0 ? (root.bypassCompensation ? 1 : 2) : 0)
    }
    Component.onCompleted: selectLevel(100)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 26

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            spacing: 12

            Label {
                text: root.previewMode === 0 ? "A · Normal" : (root.previewMode === 1 ? "B · Reduced backlight · compensation temporarily bypassed" : "B · Reduced backlight + compensation")
                color: root.previewMode === 0 ? "white" : (root.previewMode === 1 ? "#ffbf69" : "#63d7ff")
                font.pixelSize: 22; font.bold: true
            }
            Label {
                text: "Space toggles A/B. Adjust until both modes have similar readable detail."
                color: "#c8c8c8"; wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
            RowLayout {
                Label { text: "Panel brightness:"; color: "#d0d0d0" }
                Button { text: "25%"; checkable: true; checked: root.calibrationLevel === 25; onClicked: root.selectLevel(25) }
                Button { text: "50%"; checkable: true; checked: root.calibrationLevel === 50; onClicked: root.selectLevel(50) }
                Button { text: "100%"; checkable: true; checked: root.calibrationLevel === 100; onClicked: root.selectLevel(100) }
            }

            TabBar {
                id: sceneTabs
                Layout.fillWidth: true
                TabButton { text: "Photographs" }
                TabButton { text: "Technical chart" }
            }
            StackLayout {
                currentIndex: sceneTabs.currentIndex
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumWidth: 0; Layout.minimumHeight: 0
                Image {
                    source: "qrc:/calibration/assets/reference-scenes.png"
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.minimumWidth: 0; Layout.minimumHeight: 0
                }
                GridLayout {
                    columns: 4; rowSpacing: 0; columnSpacing: 0
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
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                RowLayout {
                    Layout.columnSpan: 2; Layout.fillWidth: true
                    spacing: 0
                    ButtonGroup { id: comparisonGroup; exclusive: true }
                    Button {
                        Layout.fillWidth: true; text: "Auto"; checkable: true
                        checked: root.comparisonState === 0; highlighted: checked
                        ButtonGroup.group: comparisonGroup
                        onClicked: root.selectComparison(0)
                    }
                    Button {
                        Layout.fillWidth: true; text: "A · Normal"; checkable: true
                        checked: root.comparisonState === 1; highlighted: checked
                        ButtonGroup.group: comparisonGroup
                        onClicked: root.selectComparison(1)
                    }
                    Button {
                        Layout.fillWidth: true; text: "B · Compensated"; checkable: true
                        checked: root.comparisonState === 2; highlighted: checked
                        ButtonGroup.group: comparisonGroup
                        onClicked: root.selectComparison(2)
                    }
                }
                CheckBox {
                    Layout.columnSpan: 2; Layout.fillWidth: true
                    text: "Bypass compensation (preview only)"
                    checked: root.bypassCompensation
                    onToggled: {
                        root.bypassCompensation = checked
                        if (root.previewMode !== 0 || root.comparisonState === 2)
                            root.setMode(checked ? 1 : 2)
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Keeps the same reduced backlight but removes the shader, making the compensation benefit visible. This setting is never saved."
                }
                Label { Layout.columnSpan: 2; Layout.alignment: Qt.AlignHCenter; text: calibrationBackend.available ? "Real panel A/B · silent brightness changes" : "Brightness device unavailable"; color: calibrationBackend.available ? "#8fd694" : "#ff8b8b" }
            }
        }

        Frame {
            id: controlsFrame
            Layout.preferredWidth: Math.min(320, root.width * 0.32)
            Layout.minimumWidth: Math.min(260, root.width * 0.30)
            Layout.maximumWidth: 320
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                Label { text: "Calibration controls"; font.pixelSize: 18; font.bold: true }

                Label { text: "Perceived brightness  " + Math.round(perceivedBrightness.value) + "%" }
                Slider { id: perceivedBrightness; from: 80; to: 120; stepSize: 1; value: 100; Layout.fillWidth: true; Layout.preferredHeight: 28; onMoved: if (root.previewMode === 2) root.setMode(2) }
                Label { text: "Shadow detail  " + Math.round(shadowDetail.value) + "%" }
                Slider { id: shadowDetail; from: 0; to: 100; stepSize: 1; value: 50; Layout.fillWidth: true; Layout.preferredHeight: 28; onMoved: if (root.previewMode === 2) root.setMode(2) }
                Label { text: "Highlight protection  " + Math.round(highlightProtection.value) + "%" }
                Slider { id: highlightProtection; from: 0; to: 100; stepSize: 1; value: 70; Layout.fillWidth: true; Layout.preferredHeight: 28; onMoved: if (root.previewMode === 2) root.setMode(2) }
                Label { text: "Color intensity  " + Math.round(colorIntensity.value) + "%" }
                Slider { id: colorIntensity; from: 80; to: 120; stepSize: 1; value: 100; Layout.fillWidth: true; Layout.preferredHeight: 28; onMoved: if (root.previewMode === 2) root.setMode(2) }
                Label { text: "Maximum reduction  " + Math.round(maximumReduction.value) + "%" }
                Slider {
                    id: maximumReduction; from: 0; to: 30; stepSize: 1; value: 10; Layout.fillWidth: true; Layout.preferredHeight: 28
                    onMoved: if (root.previewMode > 0) root.setMode(root.previewMode)
                }
                Label { text: "A/B interval  " + (interval.value / 1000).toFixed(1) + " s" }
                Slider { id: interval; from: 500; to: 3000; stepSize: 100; value: 1500; Layout.fillWidth: true; Layout.preferredHeight: 28 }
                Item { Layout.fillHeight: true; Layout.minimumHeight: 2 }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#b8b8b8"
                    text: "Exact black stays black. Bright highlights may not fully match because reduced backlight removes physical headroom."
                }
                Button { text: "Reset safe defaults (R)"; Layout.fillWidth: true; onClicked: root.resetDefaults() }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "Cancel"; Layout.fillWidth: true
                        onClicked: { root.deliberateClose = true; calibrationBackend.cancelAndQuit() }
                    }
                    Button {
                        text: "Save"; highlighted: true; Layout.fillWidth: true
                        onClicked: { root.storeProfile(); root.deliberateClose = true; calibrationBackend.saveAndQuit() }
                    }
                }
            }
        }
    }
}
