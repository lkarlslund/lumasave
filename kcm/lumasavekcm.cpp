// SPDX-License-Identifier: MIT
#include "lumasavekcm.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QCheckBox>
#include <QComboBox>
#include <QDBusInterface>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(LumaSaveKcm, "kcm_lumasave.json")

LumaSaveKcm::LumaSaveKcm(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    auto *layout = new QVBoxLayout(widget());
    auto *intro = new QLabel(tr("LumaSave reduces the laptop panel backlight and uses KWin to preserve readable contrast. It can run continuously or only after inactivity."), widget());
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    m_mode = new QComboBox(widget());
    m_mode->addItem(tr("Off"), QStringLiteral("off"));
    m_mode->addItem(tr("On"), QStringLiteral("always"));
    m_mode->addItem(tr("After inactivity"), QStringLiteral("idle"));
    form->addRow(tr("Mode:"), m_mode);
    m_idleSeconds = new QSpinBox(widget());
    m_idleSeconds->setRange(3, 300);
    m_idleSeconds->setSuffix(tr(" seconds"));
    form->addRow(tr("Activate after input idle:"), m_idleSeconds);
    m_sampleInterval = new QSpinBox(widget());
    m_sampleInterval->setRange(5, 300);
    m_sampleInterval->setSuffix(tr(" seconds"));
    form->addRow(tr("Active sampling interval:"), m_sampleInterval);
    m_maxReduction = new QSpinBox(widget());
    m_maxReduction->setRange(0, 75);
    m_maxReduction->setSuffix(tr("%"));
    form->addRow(tr("Maximum backlight reduction:"), m_maxReduction);
    m_batteryOnly = new QCheckBox(tr("Only while running on battery"), widget());
    form->addRow(QString(), m_batteryOnly);
    layout->addLayout(form);

    auto *calibrationButtons = new QHBoxLayout;
    auto *calibrate = new QPushButton(tr("Calibrate Panel…"), widget());
    calibrate->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
    calibrationButtons->addWidget(calibrate);
    auto *clearCalibration = new QPushButton(tr("Clear Calibration"), widget());
    clearCalibration->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    calibrationButtons->addWidget(clearCalibration);
    calibrationButtons->addStretch();
    layout->addLayout(calibrationButtons);
    auto *note = new QLabel(tr("Calibration compares normal output with a safely reduced backlight using the same full-desktop shader as LumaSave."), widget());
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();

    connect(m_mode, &QComboBox::currentIndexChanged, this, [this] {
        const QString mode = m_mode->currentData().toString();
        m_idleSeconds->setEnabled(mode == QLatin1String("idle"));
        m_sampleInterval->setEnabled(mode == QLatin1String("always"));
        settingsChanged();
    });
    connect(m_batteryOnly, &QCheckBox::toggled, this, &LumaSaveKcm::settingsChanged);
    connect(m_idleSeconds, &QSpinBox::valueChanged, this, &LumaSaveKcm::settingsChanged);
    connect(m_maxReduction, &QSpinBox::valueChanged, this, &LumaSaveKcm::settingsChanged);
    connect(m_sampleInterval, &QSpinBox::valueChanged, this, &LumaSaveKcm::settingsChanged);
    connect(calibrate, &QPushButton::clicked, this, &LumaSaveKcm::launchCalibration);
    connect(clearCalibration, &QPushButton::clicked, this, &LumaSaveKcm::clearCalibration);
}

void LumaSaveKcm::settingsChanged() { setNeedsSave(true); }

void LumaSaveKcm::load()
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-lumasave"));
    QString mode = group.readEntry("OperatingMode", group.readEntry("Enabled", false) ? QStringLiteral("idle") : QStringLiteral("off"));
    if (mode == QLatin1String("on")) mode = QStringLiteral("idle");
    m_mode->setCurrentIndex(std::max(0, m_mode->findData(mode)));
    m_idleSeconds->setValue(group.readEntry("IdleSeconds", 15));
    m_maxReduction->setValue(group.readEntry("MaxBacklightReductionPercent", 35));
    m_batteryOnly->setChecked(group.readEntry("BatteryOnly", true));
    m_sampleInterval->setValue(group.readEntry("SampleIntervalSeconds", 15));
    setNeedsSave(false);
}

void LumaSaveKcm::save()
{
    const QString mode = m_mode->currentData().toString();
    const bool enabling = mode != QLatin1String("off");
    if (enabling && powerDevilDimmingEnabled()) offerToDisablePowerDevilDimming();
    auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup group(config, QStringLiteral("Effect-lumasave"));
    group.writeEntry("Enabled", enabling);
    group.writeEntry("OperatingMode", mode);
    group.writeEntry("IdleSeconds", m_idleSeconds->value());
    group.writeEntry("MaxBacklightReductionPercent", m_maxReduction->value());
    group.writeEntry("BatteryOnly", m_batteryOnly->isChecked());
    group.writeEntry("SampleIntervalSeconds", m_sampleInterval->value());
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    plugins.writeEntry("lumasaveEnabled", enabling);
    config->sync();
    QDBusInterface effects(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"));
    if (enabling) effects.call(QStringLiteral("loadEffect"), QStringLiteral("lumasave"));
    effects.call(QStringLiteral("reconfigureEffect"), QStringLiteral("lumasave"));
    if (!enabling) effects.call(QStringLiteral("unloadEffect"), QStringLiteral("lumasave"));
    setNeedsSave(false);
}

bool LumaSaveKcm::powerDevilDimmingEnabled() const
{
    const auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"), QStringLiteral("LowBattery")}) {
        const KConfigGroup profileGroup(config, profile);
        const KConfigGroup display(&profileGroup, QStringLiteral("Display"));
        // PowerDevil's built-in default is true when the entry is absent.
        if (display.readEntry("DimDisplayWhenIdle", true)) return true;
    }
    return false;
}

void LumaSaveKcm::offerToDisablePowerDevilDimming()
{
    const auto answer = QMessageBox::warning(widget(), tr("Conflicting Automatic Dimming"),
        tr("KDE's built-in ‘Dim screen when inactive’ is enabled. It can stack with LumaSave and produce unexpected brightness changes.\n\nDisable KDE automatic dimming for AC, Battery, and Low Battery profiles? Screen-off and suspend settings will not be changed."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) return;
    auto config = KSharedConfig::openConfig(QStringLiteral("powerdevilrc"));
    for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"), QStringLiteral("LowBattery")}) {
        KConfigGroup profileGroup(config, profile);
        KConfigGroup display(&profileGroup, QStringLiteral("Display"));
        display.writeEntry("DimDisplayWhenIdle", false);
    }
    config->sync();
    QDBusInterface powerDevil(QStringLiteral("org.kde.Solid.PowerManagement"),
                              QStringLiteral("/org/kde/Solid/PowerManagement"),
                              QStringLiteral("org.kde.Solid.PowerManagement"));
    powerDevil.call(QStringLiteral("reparseConfiguration"));
}

void LumaSaveKcm::defaults()
{
    m_mode->setCurrentIndex(0);
    m_idleSeconds->setValue(15);
    m_maxReduction->setValue(35);
    m_batteryOnly->setChecked(true);
    m_sampleInterval->setValue(15);
}

void LumaSaveKcm::launchCalibration()
{
    QStringList searchPaths{QDir::homePath() + QStringLiteral("/.local/bin")};
    searchPaths.append(QString::fromLocal8Bit(qgetenv("PATH")).split(QLatin1Char(':')));
    const QString program = QStandardPaths::findExecutable(QStringLiteral("lumasave-calibrate-preview"), searchPaths);
    if (program.isEmpty() || !QProcess::startDetached(program)) {
        QMessageBox::critical(widget(), tr("LumaSave Calibration"), tr("The calibration helper could not be started."));
    }
}

void LumaSaveKcm::clearCalibration()
{
    if (QMessageBox::question(widget(), tr("Clear LumaSave Calibration"),
            tr("Remove the 25%, 50%, and 100% panel profiles and return to the built-in correction defaults?"))
        != QMessageBox::Yes) return;
    auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup group(config, QStringLiteral("Effect-lumasave"));
    group.deleteEntry("HasCalibrationProfiles");
    constexpr int levels[] = {25, 50, 100};
    const QStringList suffixes{
        QStringLiteral("PerceivedBrightnessPercent"), QStringLiteral("ShadowDetailPercent"),
        QStringLiteral("HighlightProtectionPercent"), QStringLiteral("ColorIntensityPercent")};
    for (int level : levels) {
        for (const QString &suffix : suffixes) {
            group.deleteEntry(QStringLiteral("Profile%1%2").arg(level).arg(suffix));
        }
    }
    config->sync();
    QDBusInterface effects(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"));
    effects.call(QStringLiteral("reconfigureEffect"), QStringLiteral("lumasave"));
    QMessageBox::information(widget(), tr("LumaSave Calibration"), tr("Calibration cleared. Built-in defaults are now in use."));
}

#include "lumasavekcm.moc"
