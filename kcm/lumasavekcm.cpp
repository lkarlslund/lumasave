// SPDX-License-Identifier: MIT
#include "lumasavekcm.h"

#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QCheckBox>
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
    auto *intro = new QLabel(tr("LumaSave reduces the laptop panel backlight after the desktop becomes idle, then uses KWin to preserve readable contrast."), widget());
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_enabled = new QCheckBox(tr("Enable content-adaptive display power saving"), widget());
    layout->addWidget(m_enabled);
    auto *form = new QFormLayout;
    m_idleSeconds = new QSpinBox(widget());
    m_idleSeconds->setRange(3, 300);
    m_idleSeconds->setSuffix(tr(" seconds"));
    form->addRow(tr("Activate after input idle:"), m_idleSeconds);
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

    connect(m_enabled, &QCheckBox::toggled, this, &LumaSaveKcm::settingsChanged);
    connect(m_batteryOnly, &QCheckBox::toggled, this, &LumaSaveKcm::settingsChanged);
    connect(m_idleSeconds, &QSpinBox::valueChanged, this, &LumaSaveKcm::settingsChanged);
    connect(m_maxReduction, &QSpinBox::valueChanged, this, &LumaSaveKcm::settingsChanged);
    connect(calibrate, &QPushButton::clicked, this, &LumaSaveKcm::launchCalibration);
    connect(clearCalibration, &QPushButton::clicked, this, &LumaSaveKcm::clearCalibration);
}

void LumaSaveKcm::settingsChanged() { setNeedsSave(true); }

void LumaSaveKcm::load()
{
    const KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-lumasave"));
    m_enabled->setChecked(group.readEntry("Enabled", false));
    m_idleSeconds->setValue(group.readEntry("IdleSeconds", 15));
    m_maxReduction->setValue(group.readEntry("MaxBacklightReductionPercent", 35));
    m_batteryOnly->setChecked(group.readEntry("BatteryOnly", true));
    setNeedsSave(false);
}

void LumaSaveKcm::save()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup group(config, QStringLiteral("Effect-lumasave"));
    group.writeEntry("Enabled", m_enabled->isChecked());
    group.writeEntry("OperatingMode", m_enabled->isChecked() ? QStringLiteral("on") : QStringLiteral("off"));
    group.writeEntry("IdleSeconds", m_idleSeconds->value());
    group.writeEntry("MaxBacklightReductionPercent", m_maxReduction->value());
    group.writeEntry("BatteryOnly", m_batteryOnly->isChecked());
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    plugins.writeEntry("lumasaveEnabled", m_enabled->isChecked());
    config->sync();
    QDBusInterface effects(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"));
    if (m_enabled->isChecked()) effects.call(QStringLiteral("loadEffect"), QStringLiteral("lumasave"));
    effects.call(QStringLiteral("reconfigureEffect"), QStringLiteral("lumasave"));
    if (!m_enabled->isChecked()) effects.call(QStringLiteral("unloadEffect"), QStringLiteral("lumasave"));
    setNeedsSave(false);
}

void LumaSaveKcm::defaults()
{
    m_enabled->setChecked(false);
    m_idleSeconds->setValue(15);
    m_maxReduction->setValue(35);
    m_batteryOnly->setChecked(true);
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
