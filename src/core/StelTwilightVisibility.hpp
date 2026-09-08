/* Stellarium — experimental twilight visibility calibration.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STELTWILIGHTVISIBILITY_HPP
#define STELTWILIGHTVISIBILITY_HPP
#include <algorithm>
#include <cmath>

namespace StelTwilightVisibility
{
// Representative natural dark sky, cd/m². This is a contrast reference,
// not additional light painted into the atmosphere.
constexpr float darkSkyLuminance = 0.0002f;

// Zenith V-band fit from Patat et al. (2006), A&A 455, 385, Table 1:
// https://doi.org/10.1051/0004-6361:20064992
// Valid solar depression: 5–15 degrees. Return only the twilight excess.
// Outside that interval the continuation is a display calibration, not
// a measurement: tangent towards daylight, smooth decay to zero at 18°.
inline float luminance(float geometricSunAltitudeDegrees)
{
    const float depression = -geometricSunAltitudeDegrees;
    if (!std::isfinite(depression) || depression >= 18.f)
        return 0.f;
    const float x = std::max(-5.f, std::min(10.f, depression - 5.f));
    const float surfaceBrightness = 11.84f + 1.518f*x - 0.057f*std::max(0.f, x)*x;
    const float total = 108000.f * std::pow(10.f, -0.4f*surfaceBrightness);
    const float excess = std::max(0.f, total - darkSkyLuminance);
    if (depression <= 15.f)
        return excess;
    const float t = (depression - 15.f)/3.f;
    const float slope = 0.4f*std::log(10.f)*(1.518f - 2.f*0.057f*10.f)*total/excess;
    return excess * std::exp(-slope*(depression-15.f)) * (1.f-t*t*(3.f-2.f*t));
}

// Point-source limit, including rod and cone vision. Cinzano, Falchi &
// Elvidge (2001), MNRAS 323, 34, eqs. 19–21, with angular size theta=0:
// https://arxiv.org/abs/astro-ph/0011310
// Use a relative threshold, leaving Stellarium's FOV, extinction, and
// observer/display calibration intact. No extra extinction is included.
inline float pointSourceThreshold(float background)
{
    const float rootNanolamberts = std::sqrt(std::max(0.f, background)*314159.26536f);
    const float rod = 1.f + 0.109f*rootNanolamberts;
    const float cone = 1.f + 0.00151f*rootNanolamberts;
    const float i1 = 3.451e-9f*rod*rod;
    const float i2 = 4.276e-8f*cone*cone;
    return i1*i2/(i1+i2);
}

inline float magnitudeLoss(float twilight, float night)
{
    const float background = std::max(darkSkyLuminance, night);
    return 2.5f*std::log10(pointSourceThreshold(background + std::max(0.f, twilight)) /
                         pointSourceThreshold(background));
}

inline float diffuseVisibility(float twilight, float night)
{
    const float background = std::max(darkSkyLuminance, night);
    return background/(background + std::max(0.f, twilight));
}
}
#endif
