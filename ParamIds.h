#pragma once
// JUCE-free: parameter table + taxonomy shared by DSP, presets, advisor and UI.
namespace vm
{
enum Pid
{
    inGain, hpFreq, gateThr,
    eqLow, eqMud, eqPres, eqAir,
    compThr, compRatio, compAtk, compRel, compMake,
    deEss, deEssFreq, levelAmt,
    satDrive, satMix,
    dlyMix, dlyTime, dlyFb,
    revMix, revSize, revDamp,
    width, limCeil, outGain,
    pitchAmt, pitchSpeed,
    pitchOn, pitchKey, pitchScale, bpmManual,
    autoAnalyze, autoApply, bypass,
    PidCount
};

enum ParamKind { KFloat = 0, KChoice = 1, KBool = 2 };

struct ParamDef { const char* id; const char* name; int kind; float mn, mx, def; const char* unit; float skew; float step; };

inline const ParamDef& paramDef (int i)
{
    static const ParamDef d[PidCount] = {
        { "inGain",     "Input Gain",      KFloat, -18, 18, 0,    "dB", 1.0f, 0.1f },
        { "hpFreq",     "High-Pass",       KFloat, 20, 400, 80,   "Hz", 0.5f, 1.0f },
        { "gateThr",    "Noise Gate",      KFloat, -90, -20, -90, "dB", 1.0f, 0.5f },
        { "eqLow",      "Low Shelf",       KFloat, -12, 12, 0,    "dB", 1.0f, 0.1f },
        { "eqMud",      "Mud Cut",         KFloat, -12, 12, 0,    "dB", 1.0f, 0.1f },
        { "eqPres",     "Presence",        KFloat, -12, 12, 0,    "dB", 1.0f, 0.1f },
        { "eqAir",      "Air",             KFloat, -12, 12, 0,    "dB", 1.0f, 0.1f },
        { "compThr",    "Comp Thresh",     KFloat, -50, 0, -18,   "dB", 1.0f, 0.1f },
        { "compRatio",  "Comp Ratio",      KFloat, 1, 20, 3,      ":1", 0.5f, 0.1f },
        { "compAtk",    "Comp Attack",     KFloat, 0.1f, 100, 8,  "ms", 0.4f, 0.1f },
        { "compRel",    "Comp Release",    KFloat, 10, 800, 120,  "ms", 0.4f, 1.0f },
        { "compMake",   "Makeup",          KFloat, -6, 24, 0,     "dB", 1.0f, 0.1f },
        { "deEss",      "De-Ess",          KFloat, 0, 1, 0.3f,    "",   1.0f, 0.01f },
        { "deEssFreq",  "De-Ess Freq",     KFloat, 3000, 10000, 6500, "Hz", 1.0f, 10.0f },
        { "levelAmt",   "Leveler",         KFloat, 0, 1, 0.4f,    "",   1.0f, 0.01f },
        { "satDrive",   "Saturation",      KFloat, 0, 1, 0.1f,    "",   1.0f, 0.01f },
        { "satMix",     "Sat Mix",         KFloat, 0, 1, 0.5f,    "",   1.0f, 0.01f },
        { "dlyMix",     "Delay Mix",       KFloat, 0, 0.6f, 0,    "",   1.0f, 0.01f },
        { "dlyTime",    "Delay Time",      KFloat, 40, 1000, 280, "ms", 0.5f, 1.0f },
        { "dlyFb",      "Delay Feedback",  KFloat, 0, 0.9f, 0.3f, "",   1.0f, 0.01f },
        { "revMix",     "Reverb Mix",      KFloat, 0, 0.6f, 0.1f, "",   1.0f, 0.01f },
        { "revSize",    "Reverb Size",     KFloat, 0, 1, 0.4f,    "",   1.0f, 0.01f },
        { "revDamp",    "Reverb Damp",     KFloat, 0, 1, 0.5f,    "",   1.0f, 0.01f },
        { "width",      "Stereo Width",    KFloat, 0, 2, 1,       "",   1.0f, 0.01f },
        { "limCeil",    "Limiter Ceiling", KFloat, -6, 0, -1,     "dB", 1.0f, 0.1f },
        { "outGain",    "Output Gain",     KFloat, -18, 18, 0,    "dB", 1.0f, 0.1f },
        { "pitchAmt",   "Tune Amount",     KFloat, 0, 1, 0.7f,    "",   1.0f, 0.01f },
        { "pitchSpeed", "Retune Speed",    KFloat, 0, 1, 0.4f,    "",   1.0f, 0.01f },
        { "pitchOn",    "Pitch Correction",KBool,   0, 1, 0,      "",   1.0f, 1.0f },
        { "pitchKey",   "Tune Key",        KChoice, 0, 11, 0,     "",   1.0f, 1.0f },
        { "pitchScale", "Tune Scale",      KChoice, 0, 2, 0,      "",   1.0f, 1.0f },
        { "bpmManual",  "BPM Override",    KFloat, 0, 220, 0,     "",   1.0f, 1.0f },
        { "autoAnalyze","Auto Analyze",    KBool,   0, 1, 1,      "",   1.0f, 1.0f },
        { "autoApply",  "Auto Apply Mix",  KBool,   0, 1, 1,      "",   1.0f, 1.0f },
        { "bypass",     "Bypass (A/B)",    KBool,   0, 1, 0,      "",   1.0f, 1.0f },
    };
    return d[i];
}

// Taxonomy used by presets and by the Beat Advisor
inline const char* const* genreNames()  { static const char* n[] = { "Hip-Hop","Trap","Melodic Rap","R&B","Pop","Ballad","Lo-fi","Afrobeats" }; return n; }
inline const char* const* moodNames()   { static const char* n[] = { "Emotional","Sad","Romantic","Energetic","Dark","Chill" }; return n; }
inline const char* const* voiceNames()  { static const char* n[] = { "Male Low","Male High","Female Low","Female High","Rap Spoken" }; return n; }
inline const char* const* condNames()   { static const char* n[] = { "Clean Studio","Noisy Room","Bright Mic","Dull Mic","Boomy Room" }; return n; }
constexpr int kNumGenres = 8, kNumMoods = 6, kNumVoices = 5, kNumConds = 5;
inline const char* const* noteNames() { static const char* n[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; return n; }
}
