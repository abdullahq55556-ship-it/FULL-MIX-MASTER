#pragma once
// JUCE-free: procedural factory presets (genre x mood x voice x condition = 1200) and recommendation.
#include "AutoMixer.h"
#include <string>

namespace vm
{
struct Preset
{
    std::string name, genre, mood, voice, cond; bool user = false;
    ParamValues values;
};

inline const std::vector<int>& presetParamIds()
{
    static const std::vector<int> ids = { hpFreq, gateThr, eqLow, eqMud, eqPres, eqAir, compThr, compRatio, compAtk, compRel, compMake,
        deEss, deEssFreq, levelAmt, satDrive, satMix, dlyMix, dlyTime, dlyFb, revMix, revSize, revDamp, width, pitchAmt, pitchSpeed, limCeil };
    return ids;
}

inline std::vector<Preset> buildFactoryPresets()
{
    struct G { float thr, ratio, atk, rel, sat, revMix, revSize, dly, dlyTime, width, air, pres, lvl, de, tuneAmt, tuneSpd; };
    static const G gt[kNumGenres] = {
        { -20, 4.0f, 5, 90, .25f, .06f, .25f, .05f, 190, 1.0f, 2.0f, 2.5f, .50f, .35f, .6f, .40f },   // Hip-Hop
        { -22, 5.0f, 3, 80, .30f, .08f, .30f, .10f, 250, 1.2f, 3.0f, 2.0f, .60f, .40f, .9f, .05f },   // Trap
        { -20, 4.0f, 6, 110, .20f, .14f, .45f, .12f, 320, 1.2f, 3.0f, 1.5f, .50f, .40f, .8f, .15f },  // Melodic Rap
        { -18, 3.0f, 12, 160, .15f, .18f, .55f, .10f, 300, 1.3f, 3.5f, 1.0f, .45f, .35f, .6f, .40f }, // R&B
        { -19, 3.5f, 8, 130, .20f, .15f, .45f, .08f, 280, 1.2f, 4.0f, 2.0f, .50f, .40f, .7f, .25f },  // Pop
        { -16, 2.5f, 15, 200, .10f, .24f, .65f, .08f, 400, 1.3f, 2.5f, 0.5f, .35f, .30f, .5f, .55f }, // Ballad
        { -17, 3.0f, 15, 150, .35f, .14f, .50f, .10f, 350, 1.0f, -1.0f, -0.5f, .40f, .25f, .4f, .60f },// Lo-fi
        { -19, 3.5f, 8, 110, .20f, .12f, .40f, .12f, 266, 1.2f, 3.0f, 2.0f, .45f, .35f, .7f, .30f }   // Afrobeats
    };
    struct M { float revMul, revSizeAdd, ratioAdd, airAdd, presAdd, lowAdd, satAdd, relAdd, dlyMul, widthAdd, lvlAdd; };
    static const M mt[kNumMoods] = {
        { 1.25f, .10f, -.3f, .5f, 0, 0, 0, 10, 1.0f, .05f, -.05f },   // Emotional
        { 1.30f, .15f, -.2f, -.5f, -.5f, 0, -.03f, 30, 1.2f, 0, -.05f }, // Sad
        { 1.15f, .05f, -.3f, 1.0f, 0, 1.0f, .03f, 10, 1.0f, .10f, 0 },  // Romantic
        { 0.80f, -.05f, .8f, 0, 1.0f, 0, .08f, -20, 0.9f, .10f, .05f }, // Energetic
        { 1.00f, .10f, 0, -2.0f, -.5f, 1.5f, .10f, 0, 1.1f, 0, 0 },     // Dark
        { 1.00f, .05f, -.5f, -.5f, 0, 0, .05f, 40, 1.1f, 0, -.05f }     // Chill
    };
    struct V { float hp, mud, pres, low, air, deFreq, deAdd, ratioAdd, atkAdd; };
    static const V vt[kNumVoices] = {
        { 70, -1.5f, 1.5f, .5f, 0, 5500, -.05f, 0, 0 },     // Male Low
        { 85, -1.0f, 1.0f, 0, 0, 6000, 0, 0, 0 },           // Male High
        { 100, -1.0f, 0, -.5f, .5f, 6800, 0, 0, 0 },        // Female Low
        { 120, -.5f, -.5f, -1.0f, .5f, 7500, .05f, 0, 0 },  // Female High
        { 90, -1.5f, 1.5f, 0, 0, 6200, 0, .5f, -1 }         // Rap Spoken
    };
    struct C { float gate, hpAdd, mud, low, pres, air, deAdd, satAdd, thrAdd; };
    static const C ct[kNumConds] = {
        { -90, 0, 0, 0, 0, 0, 0, 0, 0 },          // Clean Studio
        { -52, 15, 0, 0, 0, -1.0f, 0, 0, 3 },     // Noisy Room
        { -90, 0, 0, 0, -1.5f, -3.0f, .15f, 0, 0 },// Bright Mic
        { -90, 0, 0, 0, 2.0f, 3.5f, -.05f, .05f, 0 },// Dull Mic
        { -90, 20, -3.0f, -2.0f, 0, 0, 0, 0, 0 }  // Boomy Room
    };

    std::vector<Preset> out; out.reserve (kNumGenres * kNumMoods * kNumVoices * kNumConds);
    for (int gi = 0; gi < kNumGenres; ++gi) for (int mi = 0; mi < kNumMoods; ++mi) for (int vi = 0; vi < kNumVoices; ++vi) for (int ci = 0; ci < kNumConds; ++ci)
    {
        const G& g = gt[gi]; const M& m = mt[mi]; const V& v = vt[vi]; const C& c = ct[ci];
        Preset p; p.genre = genreNames()[gi]; p.mood = moodNames()[mi]; p.voice = voiceNames()[vi]; p.cond = condNames()[ci];
        p.name = p.genre + " | " + p.mood + " | " + p.voice + " | " + p.cond;
        auto set = [&] (int id, float val) { const auto& d = paramDef (id); p.values.push_back ({ id, clampv (val, d.mn, d.mx) }); };
        float ratio = g.ratio + m.ratioAdd + v.ratioAdd;
        set (hpFreq, v.hp + c.hpAdd); set (gateThr, c.gate);
        set (eqLow, v.low + c.low + m.lowAdd); set (eqMud, v.mud + c.mud); set (eqPres, g.pres + m.presAdd + v.pres + c.pres); set (eqAir, g.air + m.airAdd + v.air + c.air);
        set (compThr, g.thr + c.thrAdd); set (compRatio, ratio); set (compAtk, std::max (0.5f, g.atk + v.atkAdd)); set (compRel, g.rel + m.relAdd);
        set (compMake, (ratio - 1.f) * 1.3f);
        set (deEss, g.de + v.deAdd + c.deAdd); set (deEssFreq, v.deFreq); set (levelAmt, g.lvl + m.lvlAdd);
        set (satDrive, g.sat + m.satAdd + c.satAdd); set (satMix, 0.4f);
        set (dlyMix, g.dly); set (dlyTime, g.dlyTime * m.dlyMul); set (dlyFb, 0.3f);
        set (revMix, g.revMix * m.revMul); set (revSize, g.revSize + m.revSizeAdd); set (revDamp, mi == 4 ? 0.65f : 0.5f);
        set (width, g.width + m.widthAdd); set (pitchAmt, g.tuneAmt); set (pitchSpeed, g.tuneSpd); set (limCeil, -1.f);
        out.push_back (std::move (p));
    }
    return out;
}

// Rank presets for an analysed vocal: voice type + recording condition (+ genre / mood if known).
inline std::vector<int> recommendPresets (const std::vector<Preset>& ps, int voiceClass, int cond, int genreIdx, int moodIdx, int maxN)
{
    std::vector<std::pair<int, int>> sc;
    for (size_t i = 0; i < ps.size(); ++i)
    {
        const auto& p = ps[i]; if (p.user) continue;
        int s = 0;
        if (p.voice == voiceNames()[clampv (voiceClass, 0, 4)]) s += 3;
        if (p.cond == condNames()[clampv (cond, 0, 4)]) s += 3;
        if (genreIdx >= 0 && p.genre == genreNames()[genreIdx]) s += 2;
        if (moodIdx >= 0 && p.mood == moodNames()[moodIdx]) s += 1;
        if (s >= 6) sc.push_back ({ -s, (int) i });
    }
    std::stable_sort (sc.begin(), sc.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    std::vector<int> r; for (auto& q : sc) { if ((int) r.size() >= maxN) break; r.push_back (q.second); }
    return r;
}
}
