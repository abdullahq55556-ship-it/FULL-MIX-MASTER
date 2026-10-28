#pragma once
// JUCE-free real-time DSP: filters, pitch detection (YIN), pitch corrector, and the vocal chain.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace vm
{
constexpr double kPiD = 3.14159265358979323846;
inline float dbToGain (float db) { return std::exp (db * 0.115129255f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coefMs (float ms, double sr) { return std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) sr)); }
template <typename T> inline T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (double a0, double nb0, double nb1, double nb2, double na1, double na2)
    { b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0); a1 = (float) (na1 / a0); a2 = (float) (na2 / a0); }

    void highPass (double sr, double f, double q)
    {
        f = std::min (f, sr * 0.45); double w = 2 * kPiD * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        set (1 + al, (1 + c) / 2, -(1 + c), (1 + c) / 2, -2 * c, 1 - al);
    }
    void peak (double sr, double f, double gDb, double q)
    {
        f = std::min (f, sr * 0.45); double A = std::pow (10.0, gDb / 40.0), w = 2 * kPiD * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        set (1 + al / A, 1 + al * A, -2 * c, 1 - al * A, -2 * c, 1 - al / A);
    }
    void lowShelf (double sr, double f, double gDb, double q = 0.7071)
    {
        f = std::min (f, sr * 0.45); double A = std::pow (10.0, gDb / 40.0), w = 2 * kPiD * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q), sA = std::sqrt (A);
        set ((A + 1) + (A - 1) * c + 2 * sA * al,
             A * ((A + 1) - (A - 1) * c + 2 * sA * al), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - 2 * sA * al),
             -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - 2 * sA * al);
    }
    void highShelf (double sr, double f, double gDb, double q = 0.7071)
    {
        f = std::min (f, sr * 0.45); double A = std::pow (10.0, gDb / 40.0), w = 2 * kPiD * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q), sA = std::sqrt (A);
        set ((A + 1) - (A - 1) * c + 2 * sA * al,
             A * ((A + 1) + (A - 1) * c + 2 * sA * al), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - 2 * sA * al),
             2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - 2 * sA * al);
    }
};

//==============================================================================
// YIN fundamental-frequency estimator. Returns 0 when unvoiced.
struct YinDetector
{
    std::vector<float> d, dn;
    void prepare (int maxTau) { d.assign ((size_t) maxTau + 2, 0.f); dn.assign ((size_t) maxTau + 2, 0.f); }

    float detect (const float* x, int n, double sr, float& conf, float minHz = 70.f, float maxHz = 900.f)
    {
        conf = 0;
        const int tauMax = std::min ((int) (sr / minHz), n / 2);
        const int tauMin = std::max (2, (int) (sr / maxHz));
        if (tauMax <= tauMin + 2) return 0;
        const int W = n - tauMax;
        if ((int) d.size() < tauMax + 2) { d.resize ((size_t) tauMax + 2); dn.resize ((size_t) tauMax + 2); }

        d[0] = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0; const float* b = x + tau;
            for (int j = 0; j < W; ++j) { float df = x[j] - b[j]; s += df * df; }
            d[(size_t) tau] = s;
        }
        dn[0] = 1; float run = 0;
        for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; dn[(size_t) tau] = run > 1e-12f ? d[(size_t) tau] * (float) tau / run : 1.f; }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (dn[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && dn[(size_t) tau + 1] < dn[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            float mn = 1e9f;
            for (int tau = tauMin; tau < tauMax; ++tau) if (dn[(size_t) tau] < mn) { mn = dn[(size_t) tau]; best = tau; }
            if (best < 0 || mn > 0.4f) return 0;
        }
        float bt = (float) best;
        if (best > 1 && best < tauMax)
        {
            float s0 = dn[(size_t) best - 1], s1 = dn[(size_t) best], s2 = dn[(size_t) best + 1];
            float den = 2.f * (2.f * s1 - s2 - s0);
            if (std::fabs (den) > 1e-9f) bt += (s2 - s0) / den;
        }
        conf = clampv (1.f - dn[(size_t) best], 0.f, 1.f);
        return (float) (sr / bt);
    }
};

inline int snapToScale (float midi, int key, int scale)
{
    static const bool maj[12] = { 1,0,1,0,1,1,0,1,0,1,0,1 };
    static const bool mnr[12] = { 1,0,1,1,0,1,0,1,1,0,1,0 };
    int base = (int) std::lround (midi), best = base; float bestD = 1e9f;
    for (int n = base - 3; n <= base + 3; ++n)
    {
        int pc = ((n - key) % 12 + 12) % 12;
        bool ok = scale == 0 || (scale == 1 ? maj[pc] : mnr[pc]);
        float dd = std::fabs ((float) n - midi);
        if (ok && dd < bestD) { bestD = dd; best = n; }
    }
    return best;
}

//==============================================================================
// Basic real-time pitch corrector: YIN detection + scale snapping + dual-tap crossfading delay shifter.
// Latency = window/2 samples (reported to the host). Quality is "basic": audible artefacts on large shifts.
class PitchCorrector
{
public:
    std::atomic<float> detectedHz { 0.f }, targetHz { 0.f };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        W = (int) (sr * 0.030) & ~1; if (W < 64) W = 64;
        int rs = 1; while (rs < W * 4) rs <<= 1;
        ringMask = rs - 1;
        for (auto& r : rings) r.assign ((size_t) rs, 0.f);
        wp = 0;
        detMask = 8191; detRing.assign (8192, 0.f); detWp = 0;
        frameLen = std::min (8000, 2 * (int) (sr / 70.0) + 64);
        hop = std::max (64, (int) (sr * 0.0116));
        tmp.assign ((size_t) frameLen, 0.f);
        yin.prepare ((int) (sr / 70.0) + 4);
        phase = 0; ratio = 1; targetRatio = 1; hopCount = 0;
    }
    int latency() const { return W / 2; }

    void process (float** x, int nCh, int n, bool enabled, int key, int scale, float amount, float speed)
    {
        nCh = std::min (nCh, 2);
        const float tauMs = 1.f + speed * speed * 250.f;
        const float smooth = 1.f - std::exp (-1.f / (tauMs * 0.001f * (float) sr));
        const int mask = ringMask;
        const float fW = (float) W;
        for (int i = 0; i < n; ++i)
        {
            float mono = 0;
            for (int c = 0; c < nCh; ++c) { mono += x[c][i]; rings[(size_t) c][(size_t) wp] = x[c][i]; }
            mono /= (float) nCh;
            detRing[(size_t) detWp] = mono; detWp = (detWp + 1) & detMask;
            if (++hopCount >= hop) { hopCount = 0; detect (enabled, key, scale, amount); }

            ratio += (targetRatio - ratio) * smooth;
            float dp;
            if (std::fabs (ratio - 1.f) < 0.0005f && targetRatio == 1.f)
            {
                float p0 = std::round (phase * 2.f) * 0.5f, step = 0.02f / fW;
                dp = clampv (p0 - phase, -step, step);
            }
            else dp = (1.f - ratio) / fW;
            phase += dp;
            if (phase >= 1.f) phase -= 1.f;
            if (phase < 0.f) phase += 1.f;
            float p2 = phase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
            const float d1 = phase * fW, d2 = p2 * fW;
            const float s1 = std::sin ((float) kPiD * phase), w1 = s1 * s1, w2 = 1.f - w1;
            for (int c = 0; c < nCh; ++c)
                x[c][i] = w1 * readTap (c, d1, mask) + w2 * readTap (c, d2, mask);
            wp = (wp + 1) & mask;
        }
    }

private:
    inline float readTap (int c, float delay, int mask) const
    {
        float pos = (float) wp - delay;
        int i0 = (int) std::floor (pos); float f = pos - (float) i0;
        const auto& r = rings[(size_t) c];
        return r[(size_t) (i0 & mask)] * (1.f - f) + r[(size_t) ((i0 + 1) & mask)] * f;
    }

    void detect (bool enabled, int key, int scale, float amount)
    {
        for (int k = 0; k < frameLen; ++k) tmp[(size_t) k] = detRing[(size_t) ((detWp - frameLen + k) & detMask)];
        float e = 0; for (int k = 0; k < frameLen; k += 4) e += tmp[(size_t) k] * tmp[(size_t) k];
        float conf = 0, f0 = 0;
        if (e / (float) (frameLen / 4) > 1.0e-6f) f0 = yin.detect (tmp.data(), frameLen, sr, conf);
        if (conf < 0.8f) f0 = 0;
        detectedHz.store (f0);
        if (! enabled || f0 <= 0.f) { targetRatio = 1.f; targetHz.store (0.f); return; }
        float midi = 69.f + 12.f * std::log2 (f0 / 440.f);
        int tgt = snapToScale (midi, key, scale);
        float corrected = midi + ((float) tgt - midi) * amount;
        targetRatio = clampv (std::pow (2.f, (corrected - midi) / 12.f), 0.75f, 1.34f);
        targetHz.store (440.f * std::pow (2.f, ((float) tgt - 69.f) / 12.f));
    }

    double sr = 44100;
    int W = 1324, ringMask = 8191, wp = 0, detMask = 8191, detWp = 0, frameLen = 1324, hop = 512, hopCount = 0;
    std::vector<float> rings[2], detRing, tmp;
    float phase = 0, ratio = 1, targetRatio = 1;
    YinDetector yin;
};

//==============================================================================
struct Settings
{
    float inGain = 0, hp = 80, gateThr = -90, eqLow = 0, eqMud = 0, eqPres = 0, eqAir = 0;
    float compThr = -18, compRatio = 3, compAtk = 8, compRel = 120, compMake = 0;
    float deEss = 0.3f, deEssFreq = 6500, levelAmt = 0.4f, satDrive = 0.1f, satMix = 0.5f;
    float dlyMix = 0, dlyTime = 280, dlyFb = 0.3f, revMix = 0.1f, revSize = 0.4f, revDamp = 0.5f;
    float width = 1, limCeil = -1, outGain = 0, pitchAmt = 0.7f, pitchSpeed = 0.4f;
    int pitchKey = 0, pitchScale = 0; bool pitchOn = false;
};

class VocalChain
{
public:
    std::atomic<float> compGrDb { 0.f };
    PitchCorrector pitch;

    void prepare (double sampleRate)
    {
        sr = sampleRate; pitch.prepare (sr);
        const size_t dl = (size_t) (sr * 1.3) + 16;
        for (int c = 0; c < 2; ++c)
        {
            hp1[c].reset(); hp2[c].reset(); eqL[c].reset(); eqM[c].reset(); eqP[c].reset(); eqA[c].reset(); deHp[c].reset();
            dBuf[c].assign (dl, 0.f); fbLp[c] = 0;
        }
        dWp = 0; dSm = 280.f * (float) sr / 1000.f;
        gEnv = 0; gGainDb = 0; deEnvHf = deEnvFull = 0; deG = 1; grS = 0; levMs = 0; levGoal = 0; levGainDb = 0; levCount = 0; limG = 1;
        lastHp = lastLow = lastMud = lastPres = lastAir = lastDeF = -12345.f;
    }
    int latency() const { return pitch.latency(); }

    // Stage 1: gain, high-pass, noise gate, pitch correction, EQ, de-esser, compressor, leveler, saturation, delay
    void processPre (float** x, int nCh, int n, const Settings& s)
    {
        nCh = std::min (nCh, 2);
        updateCoeffs (s);
        const float g = dbToGain (s.inGain);
        const bool gateOn = s.gateThr > -89.f;
        const float gAtk = coefMs (1.f, sr), gRel = coefMs (60.f, sr), gDown = 1.f - coefMs (60.f, sr), gUp = 1.f - coefMs (3.f, sr);

        for (int i = 0; i < n; ++i)
        {
            float peak = 0;
            for (int c = 0; c < nCh; ++c)
            {
                float v = hp2[c].process (hp1[c].process (x[c][i] * g));
                x[c][i] = v; peak = std::max (peak, std::fabs (v));
            }
            const float k = peak > gEnv ? gAtk : gRel; gEnv = k * gEnv + (1.f - k) * peak;
            float tgt = 0;
            if (gateOn) { float envDb = gainToDb (gEnv); if (envDb < s.gateThr) tgt = std::max (-30.f, -(s.gateThr - envDb)); }
            gGainDb += (tgt - gGainDb) * (tgt < gGainDb ? gDown : gUp);
            if (gGainDb < -0.01f) { float gg = dbToGain (gGainDb); for (int c = 0; c < nCh; ++c) x[c][i] *= gg; }
        }

        pitch.process (x, nCh, n, s.pitchOn, s.pitchKey, s.pitchScale, s.pitchAmt, s.pitchSpeed);

        const float cAtk = coefMs (s.compAtk, sr), cRel = coefMs (s.compRel, sr);
        const float ratioInv = 1.f / std::max (1.f, s.compRatio), knee = 6.f;
        const float makeup = dbToGain (s.compMake);
        const float deAtk = coefMs (0.5f, sr), deRel = coefMs (15.f, sr), deGa = 1.f - coefMs (0.5f, sr), deGr = 1.f - coefMs (20.f, sr);
        const float lvlC = coefMs (300.f, sr), lvlDown = 1.f - coefMs (60.f, sr), lvlUp = 1.f - coefMs (400.f, sr);
        const float drive = 1.f + s.satDrive * 9.f, satMix = s.satMix * (s.satDrive > 0.001f ? 1.f : 0.f);
        const float dTarget = s.dlyTime * (float) sr / 1000.f, dSmC = 0.0008f;
        const size_t dLen = dBuf[0].size();
        float grMax = 0;

        for (int i = 0; i < n; ++i)
        {
            float v[2] = { 0, 0 }, hf[2] = { 0, 0 };
            float aHf = 0, aFull = 0;
            for (int c = 0; c < nCh; ++c)
            {
                float y = x[c][i];
                y = eqL[c].process (y); y = eqM[c].process (y); y = eqP[c].process (y); y = eqA[c].process (y);
                v[c] = y; hf[c] = deHp[c].process (y);
                aHf = std::max (aHf, std::fabs (hf[c])); aFull = std::max (aFull, std::fabs (y));
            }
            // de-esser
            deEnvHf   = (aHf   > deEnvHf   ? deAtk : deRel) * deEnvHf   + (1.f - (aHf   > deEnvHf   ? deAtk : deRel)) * aHf;
            deEnvFull = (aFull > deEnvFull ? deAtk : deRel) * deEnvFull + (1.f - (aFull > deEnvFull ? deAtk : deRel)) * aFull;
            float gT = 1.f;
            if (s.deEss > 0.001f && deEnvFull > 1e-4f)
            {
                float r = deEnvHf / (deEnvFull + 1e-6f), thr = 0.45f;
                if (r > thr) gT = std::pow (thr / r, s.deEss * 1.5f);
            }
            deG += (gT - deG) * (gT < deG ? deGa : deGr);
            float peak = 0;
            for (int c = 0; c < nCh; ++c) { v[c] -= hf[c] * (1.f - deG); peak = std::max (peak, std::fabs (v[c])); }

            // compressor (peak, soft knee, dB-domain smoothing)
            float lvl = gainToDb (peak), over = lvl - s.compThr, gr = 0;
            if (2.f * over > knee) gr = over * (1.f - ratioInv);
            else if (2.f * over > -knee) { float t = over + knee * 0.5f; gr = (1.f - ratioInv) * t * t / (2.f * knee); }
            const float k = gr > grS ? cAtk : cRel; grS = k * grS + (1.f - k) * gr;
            grMax = std::max (grMax, grS);
            const float cg = dbToGain (-grS) * makeup;

            // leveler (slow gain rider)
            float pw = 0;
            for (int c = 0; c < nCh; ++c) { v[c] *= cg; pw += v[c] * v[c]; }
            levMs = lvlC * levMs + (1.f - lvlC) * (pw / (float) nCh);
            if (++levCount >= 64)
            {
                levCount = 0; float ld = 10.f * std::log10 (levMs + 1e-12f);
                if (ld > -55.f) levGoal = clampv (-20.f - ld, -6.f, 9.f) * s.levelAmt;
            }
            levGainDb += (levGoal - levGainDb) * (levGoal < levGainDb ? lvlDown : lvlUp);
            const float lg = dbToGain (levGainDb);

            // saturation + delay
            dSm += (dTarget - dSm) * dSmC;
            float rp = (float) dWp - dSm; while (rp < 0) rp += (float) dLen;
            size_t i0 = (size_t) rp % dLen, i1 = (i0 + 1) % dLen; float fr = rp - std::floor (rp);
            for (int c = 0; c < nCh; ++c)
            {
                float y = v[c] * lg;
                if (satMix > 0.f) y = y * (1.f - satMix) + satMix * std::tanh (drive * y) / drive;
                float tap = dBuf[c][i0] * (1.f - fr) + dBuf[c][i1] * fr;
                if (c == 1) { float rp2 = (float) dWp - dSm * 1.25f; while (rp2 < 0) rp2 += (float) dLen; size_t j0 = (size_t) rp2 % dLen, j1 = (j0 + 1) % dLen; float f2 = rp2 - std::floor (rp2); tap = dBuf[1][j0] * (1.f - f2) + dBuf[1][j1] * f2; }
                fbLp[c] += 0.35f * (tap - fbLp[c]);
                dBuf[c][(size_t) dWp] = y + fbLp[c] * s.dlyFb;
                x[c][i] = y + tap * s.dlyMix;
            }
            dWp = (dWp + 1) % (int) dLen;
        }
        compGrDb.store (grMax);
    }

    // Stage 2 (after reverb): stereo width, output gain, limiter
    void processPost (float** x, int nCh, int n, const Settings& s)
    {
        nCh = std::min (nCh, 2);
        const float og = dbToGain (s.outGain), ceil = dbToGain (s.limCeil), rel = coefMs (80.f, sr);
        for (int i = 0; i < n; ++i)
        {
            if (nCh == 2 && std::fabs (s.width - 1.f) > 0.001f)
            {
                float m = 0.5f * (x[0][i] + x[1][i]), sd = 0.5f * (x[0][i] - x[1][i]) * s.width;
                x[0][i] = m + sd; x[1][i] = m - sd;
            }
            float pk = 0;
            for (int c = 0; c < nCh; ++c) { x[c][i] *= og; pk = std::max (pk, std::fabs (x[c][i])); }
            float t = pk > ceil ? ceil / pk : 1.f;
            if (t < limG) limG = t; else limG = rel * limG + (1.f - rel);
            if (limG < 0.9999f) for (int c = 0; c < nCh; ++c) x[c][i] *= limG;
        }
    }

private:
    void updateCoeffs (const Settings& s)
    {
        if (s.hp != lastHp) { lastHp = s.hp; for (int c = 0; c < 2; ++c) { hp1[c].highPass (sr, s.hp, 0.5412); hp2[c].highPass (sr, s.hp, 1.3066); } }
        if (s.eqLow  != lastLow)  { lastLow = s.eqLow;   for (int c = 0; c < 2; ++c) eqL[c].lowShelf (sr, 120.0, s.eqLow); }
        if (s.eqMud  != lastMud)  { lastMud = s.eqMud;   for (int c = 0; c < 2; ++c) eqM[c].peak (sr, 320.0, s.eqMud, 1.0); }
        if (s.eqPres != lastPres) { lastPres = s.eqPres; for (int c = 0; c < 2; ++c) eqP[c].peak (sr, 3500.0, s.eqPres, 0.9); }
        if (s.eqAir  != lastAir)  { lastAir = s.eqAir;   for (int c = 0; c < 2; ++c) eqA[c].highShelf (sr, 10000.0, s.eqAir); }
        if (s.deEssFreq != lastDeF) { lastDeF = s.deEssFreq; for (int c = 0; c < 2; ++c) deHp[c].highPass (sr, s.deEssFreq, 0.7071); }
    }

    double sr = 44100;
    Biquad hp1[2], hp2[2], eqL[2], eqM[2], eqP[2], eqA[2], deHp[2];
    float lastHp = -1, lastLow = -1, lastMud = -1, lastPres = -1, lastAir = -1, lastDeF = -1;
    float gEnv = 0, gGainDb = 0, deEnvHf = 0, deEnvFull = 0, deG = 1, grS = 0, levMs = 0, levGoal = 0, levGainDb = 0, limG = 1;
    int levCount = 0, dWp = 0;
    std::vector<float> dBuf[2]; float fbLp[2] = { 0, 0 }, dSm = 12000;
};

// Fixed-length delay used to keep the bypassed (A/B) signal time-aligned with the processed one.
struct FixedDelay
{
    std::vector<float> buf[2]; int len = 1, pos = 0;
    void prepare (int d) { len = std::max (1, d); buf[0].assign ((size_t) len, 0.f); buf[1].assign ((size_t) len, 0.f); pos = 0; }
    inline float tick (int c, float in) { float o = buf[c][(size_t) pos]; buf[c][(size_t) pos] = in; return o; }
    inline void advance() { if (++pos >= len) pos = 0; }
};
}
