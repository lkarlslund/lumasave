import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    Kirigami.FormLayout {
        anchors.fill: parent

        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("Enable content-adaptive backlight saving", "Enabled:")
            text: i18nc("Enable LumaSave", "Allow LumaSave to activate")
            checked: kcm.settings.enabled
            onToggled: kcm.settings.enabled = checked
            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "Enabled"
            }
        }

        Kirigami.Separator { Kirigami.FormData.isSection: true }

        QQC2.Slider {
            Kirigami.FormData.label: i18nc("Calibration gain", "Perceived brightness:")
            from: 80; to: 120; stepSize: 1
            value: kcm.settings.perceivedBrightnessPercent
            onMoved: kcm.settings.perceivedBrightnessPercent = Math.round(value)
            KCM.SettingStateBinding { configObject: kcm.settings; settingName: "PerceivedBrightnessPercent" }
        }
        QQC2.Slider {
            Kirigami.FormData.label: i18nc("Calibration shadow control", "Shadow detail:")
            from: 0; to: 100; stepSize: 1
            value: kcm.settings.shadowDetailPercent
            onMoved: kcm.settings.shadowDetailPercent = Math.round(value)
            KCM.SettingStateBinding { configObject: kcm.settings; settingName: "ShadowDetailPercent" }
        }
        QQC2.Slider {
            Kirigami.FormData.label: i18nc("Calibration highlight control", "Highlight protection:")
            from: 0; to: 100; stepSize: 1
            value: kcm.settings.highlightProtectionPercent
            onMoved: kcm.settings.highlightProtectionPercent = Math.round(value)
            KCM.SettingStateBinding { configObject: kcm.settings; settingName: "HighlightProtectionPercent" }
        }
        QQC2.Slider {
            Kirigami.FormData.label: i18nc("Calibration color control", "Color intensity:")
            from: 80; to: 120; stepSize: 1
            value: kcm.settings.colorIntensityPercent
            onMoved: kcm.settings.colorIntensityPercent = Math.round(value)
            KCM.SettingStateBinding { configObject: kcm.settings; settingName: "ColorIntensityPercent" }
        }

        Column {
            Kirigami.FormData.label: i18nc("Maximum percentage by which backlight may be reduced", "Maximum reduction:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Slider {
                id: reductionSlider
                width: Kirigami.Units.gridUnit * 16
                from: 0
                to: 75
                stepSize: 1
                value: kcm.settings.maxBacklightReductionPercent
                onMoved: kcm.settings.maxBacklightReductionPercent = Math.round(value)
                accessible.name: i18nc("Accessible name for maximum reduction slider", "Maximum backlight reduction")
                KCM.SettingStateBinding {
                    configObject: kcm.settings
                    settingName: "MaxBacklightReductionPercent"
                }
            }

            QQC2.Label {
                text: i18nc("Current maximum backlight reduction percentage", "%1%", Math.round(reductionSlider.value))
            }
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18nc("Input idle delay", "Activate after:")
            from: 3
            to: 300
            value: kcm.settings.idleSeconds
            editable: true
            textFromValue: function(value) { return i18ncp("Idle delay in seconds", "%1 second", "%1 seconds", value) }
            valueFromText: function(text) { return parseInt(text) || 3 }
            onValueModified: kcm.settings.idleSeconds = value
            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "IdleSeconds"
            }
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("Power source restriction", "Power source:")
            text: i18nc("Only adapt while running on battery", "Battery only")
            checked: kcm.settings.batteryOnly
            onToggled: kcm.settings.batteryOnly = checked
            KCM.SettingStateBinding {
                configObject: kcm.settings
                settingName: "BatteryOnly"
            }
        }

        Kirigami.InlineMessage {
            Kirigami.FormData.isSection: true
            visible: reductionSlider.value > 50
            type: Kirigami.MessageType.Warning
            text: i18nc("Warning for an aggressive backlight reduction setting", "Values above 50% can noticeably alter highlights and colors. Black pixels remain black.")
        }
    }
}
