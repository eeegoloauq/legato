#include "crossfade.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace legato::crossfade {

bool albumContinues(const Song &from, const Song &to)
{
    if (from.albumId.isEmpty() || from.albumId != to.albumId)
        return false;
    if (from.track <= 0 || to.track <= 0)
        return true;
    const int fromDisc = std::max(from.disc, 1);
    const int toDisc = std::max(to.disc, 1);
    return (toDisc == fromDisc && to.track == from.track + 1) || (toDisc == fromDisc + 1 && to.track == 1);
}

double length(double seconds, double fromDuration, double toDuration)
{
    double out = std::max(seconds, 0.0);
    if (fromDuration > 0)
        out = std::min(out, fromDuration / 3);
    if (toDuration > 0)
        out = std::min(out, toDuration / 3);
    return out;
}

std::pair<double, double> gains(double progress)
{
    const double angle = std::clamp(progress, 0.0, 1.0) * std::numbers::pi / 2;
    return {std::cos(angle), std::sin(angle)};
}

Plan fixedPlan(double seconds)
{
    Plan plan;
    plan.start = std::max(seconds, 0.0);
    plan.inRamp = plan.start;
    return plan;
}

std::pair<double, double> gains(const Plan &plan, double seconds)
{
    const double overlap = plan.overlap();
    const double outLength = overlap - plan.outFadeFrom;
    const double outGain = seconds <= plan.outFadeFrom ? 1.0
        : outLength <= 0                               ? 0.0
                                                       : gains((seconds - plan.outFadeFrom) / outLength).first;
    const double inGain = plan.inRamp <= 0 ? 1.0 : gains(seconds / plan.inRamp).second;
    return {outGain, inGain};
}

std::pair<double, double> bassCuts(const Plan &plan, double seconds)
{
    if (plan.bassSwap < 0)
        return {0.0, 0.0};
    const double swapped = std::clamp((seconds - plan.bassSwap) / BassSwapSeconds, 0.0, 1.0);
    return {swapped, 1.0 - swapped};
}

double mpvVolume(int volume, double gain)
{
    return volume * std::cbrt(std::clamp(gain, 0.0, 1.0));
}

} // namespace legato::crossfade
