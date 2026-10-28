#pragma once
// JUCE-free: turns an AnalysisResult into concrete parameter values (rule-based, from measured features).
#include "Analyzer.h"
#include <utility>

namespace vm
{
using ParamValues = std::vector<std::pair<int, float>>;

inline ParamValues computeAutoMix (const AnalysisResult& r)
{
    ParamValues o;
    auto set = [&] (int id, float v) { const auto& d = paramDef (id); o.push_back ({ id, clampv (v, d.mn, d.mx) }); };

    set (inGain, clampv ((-20.f - r.activeRmsDb) * 0.8f, -12.f, 12.f));

    float hp = 80.f;
    if (r.loMidi > 0) hp = clampv (440.f * std::pow (2.f, (r.loMidi - 69.f) / 12.f) * 0.8f, 70.f, 130.f);
    if (r.relLow > -4.f) hp += 15.f;
    set (hpFreq, hp);
    set (gateThr, r.noiseFloorDb <= -75.f ? -90.f : clampv (std::min (r.noiseFloorDb + 7.f, r.levelP50 - 22.f), -75.f, -38.f));

    // EQ: pull measured band balance (relative to 500-2k) toward a typical vocal target curve
    set (eqLow,  clampv (-(r.relLow  - (-10.f)) * 0.5f, -6.f, 3.f));
    set (eqMud,  clampv (-(r.relMud  - 2.f)    * 0.6f, -6.f, 3.f));
    set (eqPres, clampv (-(r.relPres - (-4.f)) * 0.5f, -4.f, 5.f));
    set (eqAir,  clampv (-(r.relAir  - (-22.f)) * 0.4f, -3.f, 4.f));

    // Compression driven by measured level distribution and dynamic range
    float thr = clampv (r.levelP50 + 5.f, -40.f, -6.f);
    float ratio = r.dynamicRangeDb > 20.f ? 4.f : r.dynamicRangeDb > 14.f ? 3.2f : r.dynamicRangeDb > 9.f ? 2.5f : 2.f;
    set (compThr, thr); set (compRatio, ratio);
    set (compAtk, r.rapLike ? 5.f : 12.f); set (compRel, r.rapLike ? 90.f : 160.f);
    float avgGr = std::max (0.f, (r.levelP90 + 10.f - thr) * (1.f - 1.f / ratio) * 0.5f);
    set (compMake, clampv (avgGr * 0.7f, 0.f, 10.f));
    set (levelAmt, r.dynamicRangeDb > 14.f ? 0.6f : r.dynamicRangeDb > 9.f ? 0.45f : 0.3f);

    static const float deFreq[5] = { 5500, 6000, 6800, 7500, 6200 };
    set (deEssFreq, deFreq[clampv (r.voiceClass, 0, 4)]);
    set (deEss, clampv (0.3f + (r.relSib + 12.f) * 0.05f, 0.1f, 0.9f));

    set (satDrive, r.condition == 3 ? 0.2f : 0.12f); set (satMix, 0.35f);

    const bool sparse = r.noteRate < 2.f;
    set (revSize, r.rapLike ? 0.3f : (sparse ? 0.6f : 0.45f)); set (revMix, r.rapLike ? 0.07f : (sparse ? 0.15f : 0.1f)); set (revDamp, 0.5f);
    float beat = r.bpmConf >= 0.5f && r.bpm > 0 ? 60000.f / r.bpm : 400.f;
    while (beat > 900.f) beat *= 0.5f;
    set (dlyTime, r.rapLike ? beat * 0.5f : beat); set (dlyMix, r.rapLike ? 0.04f : (sparse ? 0.1f : 0.06f)); set (dlyFb, 0.3f);
    set (width, 1.15f); set (limCeil, -1.f); set (outGain, 0.f);

    if (r.keyValid && r.keyConf >= 0.35f)
    {
        set (pitchKey, (float) r.keyRoot); set (pitchScale, r.minor ? 2.f : 1.f);
        set (pitchOn, r.keyConf >= 0.5f ? 1.f : 0.f);
        set (pitchAmt, 0.6f); set (pitchSpeed, 0.45f);
    }
    else set (pitchOn, 0.f);
    return o;
}
}
