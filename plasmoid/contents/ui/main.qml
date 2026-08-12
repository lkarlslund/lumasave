// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as Plasma5Support
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    property var status: ({mode: "off", state: "unavailable", reductionPercent: 0})
    property bool available: false
    property int configuredMaximum: 35
    property bool configuredBatteryOnly: true
    property bool updatingControls: false
    readonly property string helperPath: decodeURIComponent(Qt.resolvedUrl("../code/settings").toString().replace("file://", ""))
    readonly property int reduction: Number(status.reductionPercent || 0)
    readonly property string compactText: available && status.state === "saving" ? "−" + reduction + "%" : "Luma"
    readonly property string stateText: {
        if (!available) return i18n("LumaSave is unavailable")
        if (status.state === "saving") return i18n("Reducing panel backlight by %1%", reduction)
        if (status.state === "analyzing") return i18n("Analyzing screen content")
        if (status.state === "calibrating") return i18n("Calibration controls the display")
        if (status.mode === "always") return i18n("On · waiting for analysis")
        if (status.mode === "idle") return i18n("Waiting for inactivity")
        return i18n("Off")
    }
    readonly property string modeText: status.mode === "always" ? i18n("On")
        : status.mode === "idle" ? i18n("After inactivity")
        : status.mode === "calibrating" ? i18n("Calibrating") : i18n("Off")

    function duration(seconds) {
        const total = Math.max(0, Math.round(Number(seconds || 0)))
        if (total < 60) return i18np("%1 second", "%1 seconds", total)
        const minutes = Math.round(total / 60)
        if (minutes < 60) return i18np("%1 minute", "%1 minutes", minutes)
        const hours = Math.floor(minutes / 60)
        const remaining = minutes % 60
        return remaining ? i18n("%1 h %2 min", hours, remaining) : i18np("%1 hour", "%1 hours", hours)
    }

    function backlightHours(seconds) {
        return (Number(seconds || 0) / 3600).toLocaleString(Qt.locale(), "f", 2) + " h"
    }

    function consume(output) {
        try {
            status = JSON.parse(output.trim().split("\n").pop())
            available = status.serviceAvailable !== false || status.mode === "off"
        } catch (error) {
            available = false
        }
    }

    function consumeSettings(output) {
        const fields = output.trim().split("\n").pop().split("|")
        if (fields.length !== 3) return
        updatingControls = true
        configuredMaximum = Number(fields[1])
        configuredBatteryOnly = fields[2] === "true"
        updatingControls = false
    }

    function changeSetting(arguments_) {
        settingsWriter.connectSource(helperPath + " " + arguments_)
    }

    Plasmoid.title: i18n("LumaSave")
    Plasmoid.icon: "brightness-high"
    Plasmoid.backgroundHints: PlasmaCore.Types.DefaultBackground
    toolTipMainText: i18n("LumaSave")
    toolTipSubText: stateText
    preferredRepresentation: Plasmoid.formFactor === PlasmaCore.Types.Planar ? fullRepresentation : compactRepresentation

    Plasma5Support.DataSource {
        id: reader
        engine: "executable"
        connectedSources: [decodeURIComponent(Qt.resolvedUrl("../code/read-status").toString().replace("file://", ""))]
        interval: 5000
        onNewData: function(sourceName, data) {
            if (data["exit code"] === 0 && data.stdout !== undefined) root.consume(data.stdout)
            else root.available = false
        }
    }

    Plasma5Support.DataSource {
        id: settingsReader
        engine: "executable"
        connectedSources: [root.helperPath + " read"]
        interval: 5000
        onNewData: function(sourceName, data) {
            if (data["exit code"] === 0 && data.stdout !== undefined) root.consumeSettings(data.stdout)
        }
    }

    Plasma5Support.DataSource {
        id: settingsWriter
        engine: "executable"
        onNewData: function(sourceName, data) {
            disconnectSource(sourceName)
            if (data["exit code"] === 0 && data.stdout !== undefined) root.consumeSettings(data.stdout)
        }
    }

    compactRepresentation: MouseArea {
        hoverEnabled: true
        onClicked: root.expanded = !root.expanded
        Layout.minimumWidth: row.implicitWidth + Kirigami.Units.smallSpacing * 2
        Layout.preferredWidth: Layout.minimumWidth
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing
            Kirigami.Icon {
                source: "brightness-high"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: implicitWidth
                opacity: root.available && root.status.mode !== "off" ? 1 : 0.5
            }
            Text {
                text: root.compactText
                font.weight: Font.DemiBold
                color: "#f2f2f2"
                style: Text.Outline
                styleColor: "#66000000"
            }
        }
    }

    fullRepresentation: Item {
        implicitWidth: Kirigami.Units.gridUnit * 15
        implicitHeight: details.implicitHeight + Kirigami.Units.largeSpacing * 2
        ColumnLayout {
            id: details
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Kirigami.Units.largeSpacing }
            spacing: Kirigami.Units.smallSpacing
            PlasmaComponents3.Label { text: root.stateText; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Kirigami.Separator { Layout.fillWidth: true }
            PlasmaComponents3.Label { text: i18n("Quick Settings"); font.weight: Font.DemiBold }
            RowLayout {
                Layout.fillWidth: true
                spacing: 0
                PlasmaComponents3.Button {
                    text: i18n("Off")
                    checkable: true
                    checked: root.status.mode === "off"
                    Layout.fillWidth: true
                    onClicked: root.changeSetting("mode off")
                }
                PlasmaComponents3.Button {
                    text: i18n("On")
                    checkable: true
                    checked: root.status.mode === "always"
                    Layout.fillWidth: true
                    onClicked: root.changeSetting("mode always")
                }
                PlasmaComponents3.Button {
                    text: i18n("Idle")
                    checkable: true
                    checked: root.status.mode === "idle"
                    Layout.fillWidth: true
                    onClicked: root.changeSetting("mode idle")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents3.Label { text: i18n("Maximum reduction"); Layout.fillWidth: true }
                PlasmaComponents3.SpinBox {
                    id: maximumReduction
                    from: 0
                    to: 75
                    value: root.configuredMaximum
                    editable: true
                    textFromValue: function(value, locale) { return value + "%" }
                    valueFromText: function(text, locale) { return Number(text.replace("%", "")) }
                    onValueModified: root.changeSetting("maximum " + value)
                }
            }
            PlasmaComponents3.CheckBox {
                text: i18n("Only while running on battery")
                checked: root.configuredBatteryOnly
                onToggled: if (!root.updatingControls) root.changeSetting("battery " + (checked ? "true" : "false"))
            }
            Kirigami.Separator { Layout.fillWidth: true }
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: Kirigami.Units.largeSpacing
                PlasmaComponents3.Label { text: i18n("Mode"); opacity: 0.65 }
                PlasmaComponents3.Label { text: root.modeText; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                PlasmaComponents3.Label { text: i18n("User brightness"); opacity: 0.65 }
                PlasmaComponents3.Label { text: Number(root.status.userBrightnessPercent || 0) + "%"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                PlasmaComponents3.Label { text: i18n("Effective backlight"); opacity: 0.65 }
                PlasmaComponents3.Label { text: Number(root.status.effectiveBrightnessPercent || 0) + "%"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                PlasmaComponents3.Label { text: i18n("Today's average"); opacity: 0.65 }
                PlasmaComponents3.Label { text: Number(root.status.todayAverageReductionPercent || 0).toLocaleString(Qt.locale(), "f", 1) + "%"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                PlasmaComponents3.Label { text: i18n("Saved today"); opacity: 0.65 }
                PlasmaComponents3.Label { text: root.backlightHours(root.status.todayEquivalentFullReductionSeconds); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                PlasmaComponents3.Label { text: i18n("All-time average"); opacity: 0.65 }
                PlasmaComponents3.Label { text: Number(root.status.allTimeAverageReductionPercent || 0).toLocaleString(Qt.locale(), "f", 1) + "%"; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                PlasmaComponents3.Label { text: i18n("Saved all-time"); opacity: 0.65 }
                PlasmaComponents3.Label { text: root.backlightHours(root.status.allTimeEquivalentFullReductionSeconds); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
            }
            PlasmaComponents3.Button {
                text: i18n("Configure LumaSave…")
                icon.name: "configure"
                Layout.alignment: Qt.AlignRight
                onClicked: Qt.openUrlExternally("systemsettings://kcm_lumasave")
            }
            PlasmaComponents3.Label {
                Layout.fillWidth: true
                text: i18n("Saved values are full-backlight-equivalent hours (reduction × time), not estimates of watts or battery life.")
                opacity: 0.65
                font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.65)
                wrapMode: Text.WordWrap
            }
        }
    }
}
