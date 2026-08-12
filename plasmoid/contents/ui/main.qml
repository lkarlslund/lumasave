// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.lumasave

PlasmoidItem {
    id: root

    property var status: LumaSaveController.status
    readonly property bool available: status.state !== "unavailable"
    readonly property int configuredMaximum: LumaSaveController.maximumReduction
    readonly property bool configuredBatteryOnly: LumaSaveController.batteryOnly
    readonly property int reduction: Number(status.reductionPercent || 0)
    readonly property string compactText: available && status.state === "saving" ? "−" + reduction + "%" : "Luma"
    readonly property string stateText: {
        if (!available) return i18n("LumaSave is unavailable")
        if (status.state === "saving") return i18n("Reducing panel backlight by %1%", reduction)
        if (status.state === "analyzing") return i18n("Analyzing screen content")
        if (status.state === "calibrating") return i18n("Calibration controls the display")
        if (status.state === "blocked" && status.blockedReason === "hdr") return i18n("Blocked · disable HDR to continue")
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

    Plasmoid.title: i18n("LumaSave")
    Plasmoid.icon: "brightness-high"
    Plasmoid.backgroundHints: PlasmaCore.Types.DefaultBackground
    toolTipMainText: i18n("LumaSave")
    toolTipSubText: stateText
    preferredRepresentation: Plasmoid.formFactor === PlasmaCore.Types.Planar ? fullRepresentation : compactRepresentation

    Kirigami.PromptDialog {
        id: hdrDialog
        title: i18n("HDR Is Not Supported")
        subtitle: i18n("LumaSave supports SDR displays only. Disable HDR on enabled displays and continue?")
        standardButtons: Kirigami.Dialog.Yes | Kirigami.Dialog.Cancel
        onAccepted: LumaSaveController.confirmDisableHdr()
        onRejected: LumaSaveController.cancelDisableHdr()
    }
    Kirigami.PromptDialog {
        id: dimmingDialog
        title: i18n("Conflicting Automatic Dimming")
        subtitle: i18n("KDE's built-in inactive-screen dimming can stack with LumaSave. Disable it for all power profiles? Screen-off and suspend settings are unchanged.")
        standardButtons: Kirigami.Dialog.Yes | Kirigami.Dialog.No
        onAccepted: LumaSaveController.confirmDisableDimming()
        onRejected: LumaSaveController.keepDimming()
    }
    Connections {
        target: LumaSaveController
        function onHdrConfirmationRequiredChanged() {
            if (LumaSaveController.hdrConfirmationRequired) hdrDialog.open()
        }
        function onDimmingConfirmationRequiredChanged() {
            if (LumaSaveController.dimmingConfirmationRequired) dimmingDialog.open()
        }
        function onError(message) { root.showPassiveNotification(message) }
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
                    onClicked: LumaSaveController.requestMode("off")
                }
                PlasmaComponents3.Button {
                    text: i18n("On")
                    checkable: true
                    checked: root.status.mode === "always"
                    Layout.fillWidth: true
                    onClicked: LumaSaveController.requestMode("always")
                }
                PlasmaComponents3.Button {
                    text: i18n("Idle")
                    checkable: true
                    checked: root.status.mode === "idle"
                    Layout.fillWidth: true
                    onClicked: LumaSaveController.requestMode("idle")
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
                    onValueModified: LumaSaveController.setMaximumReduction(value)
                }
            }
            PlasmaComponents3.CheckBox {
                text: i18n("Only while running on battery")
                checked: root.configuredBatteryOnly
                onToggled: LumaSaveController.setBatteryOnly(checked)
            }
            Kirigami.Separator { Layout.fillWidth: true }
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                columnSpacing: Kirigami.Units.largeSpacing
                PlasmaComponents3.Label { text: i18n("Backlight"); opacity: 0.65 }
                PlasmaComponents3.Label {
                    text: i18nc("User brightness to effective backlight", "%1% → %2%",
                                Number(root.status.userBrightnessPercent || 0),
                                Number(root.status.effectiveBrightnessPercent || 0))
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
                PlasmaComponents3.Label { text: i18n("Today"); opacity: 0.65 }
                PlasmaComponents3.Label {
                    text: i18nc("Average reduction and saved backlight hours", "%1% avg · %2 saved",
                                Number(root.status.todayAverageReductionPercent || 0).toLocaleString(Qt.locale(), "f", 1),
                                root.backlightHours(root.status.todaySavedBacklightSeconds))
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
                PlasmaComponents3.Label { text: i18n("All time"); opacity: 0.65 }
                PlasmaComponents3.Label {
                    text: i18nc("Average reduction and saved backlight hours", "%1% avg · %2 saved",
                                Number(root.status.allTimeAverageReductionPercent || 0).toLocaleString(Qt.locale(), "f", 1),
                                root.backlightHours(root.status.allTimeSavedBacklightSeconds))
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: i18n("Saved = brightness × reduction × time")
                    opacity: 0.65
                    font.pixelSize: Math.round(Kirigami.Units.gridUnit * 0.65)
                }
                PlasmaComponents3.Button {
                    text: i18n("Configure…")
                    icon.name: "configure"
                    onClicked: Qt.openUrlExternally("systemsettings://kcm_lumasave")
                }
            }
        }
    }
}
