// SPDX-License-Identifier: MIT
// Compatibility declaration for KWin's currently non-installed BrightnessDevice API.
#pragma once

#include <QByteArray>
#include <optional>

namespace KWin
{
class BrightnessDevice
{
public:
    virtual ~BrightnessDevice() = default;
    virtual void setBrightness(double brightness) = 0;
    virtual std::optional<double> observedBrightness() const = 0;
    virtual bool isInternal() const = 0;
    virtual QByteArray edidBeginning() const = 0;
    virtual bool usesDdcCi() const = 0;
    virtual int brightnessSteps() const = 0;
};
}
