#pragma once
// JUCE-free vocal analysis: loudness, noise floor, spectrum, pitch (YIN), key (Krumhansl-Schmuckler), tempo, pace.
#include "Dsp.h"
#include "ParamIds.h"
#include <array>
#include <numeric>
#include <string>

namespace vm
{
struct AnalysisResult
{
    bool valid = false; std::string message;
    double seconds = 0, activeSeconds = 0;
    float peakDb = -100, activeRmsDb = -100, levelP50 = -100, levelP90 = -100, dynamicRangeDb = 0, noiseFloorDb = -100;
    float centroidHz = 0, clarity = 0, voicedRatio = 0;
    float relLow = 0, relMud = 0, relPres = 0, relSib = 0, relAir = 0;      // dB relative to the 500-2k region
    float medianMidi = 0, loMidi = 0, hiMidi = 0, pitchStd = 0;
    bool keyValid = false; int keyRoot = 0; bool minor = false; float keyConf = 0;
    float bpm = 0, bpmConf = 0, noteRate = 0;
    int voiceClass = 1, condition = 0; bool rapLike = false;
    std::array<float, 12> pcHist {};
    std::vector<float> pitchTrack;      // midi per frame, 0 = unvoiced
    std::vector<float> spectrumDb;      // 96 log bands, 40 Hz .. ~16 kHz
};

struct Fft
{
    int n = 0, logn = 0; std::vector<float> cs, sn; std::vector<int> rev;
    void init (int N)
    {
        n = N; logn = 0; while ((1 << logn) < N) ++logn;
        cs.resize ((size_t) n / 2); sn.resize ((size_t) n / 2); rev.assign ((size_t) n, 0);
        for (int i = 0; i < n / 2; ++i) { double a = -2.0 * kPiD * i / n; cs[(size_t) i] = (float) std::cos (a); sn[(size_t) i] = (float) std::sin (a); }
        for (int i = 1; i < n; ++i) rev[(size_t) i] = (rev[(size_t) i >> 1] >> 1) | ((i & 1) << (logn - 1));
    }
    void forward (float* re, float* im) const
    {
        for (int i = 0; i < n; ++i) { int j = rev[(size_t) i]; if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); } }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    float wr = cs[(size_t) (j * step)], wi = sn[(size_t) (j * step)];
                    int u = i + j, v = u + half;
                    float tr = re[v] * wr - im[v] * wi, ti = re[v] * wi + im[v] * wr;
                    re[v] = re[u] - tr; im[v] = im[u] - ti; re[u] += tr; im[u] += ti;
                }
        }
    }
};

class Analyzer
{
public:
    static constexpr int kBands = 96;
    static constexpr int kBlock = 256;

    void prepare (double sampleRate)
    {
        sr = sampleRate; N = sr > 60000 ? 4096 : 2048; hop = N / 2;
        fft.init (N); win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPiD * (float) i / (float) (N - 1));
        re.assign ((size_t) N, 0.f); im.assign ((size_t) N, 0.f);
        yin.prepare ((int) (sr / 70.0) + 4);
        const double fHi = std::min (16000.0, sr * 0.45), fLo = 40.0;
        binLo.resize (kBands); binHi.resize (kBands); bandHz.resize (kBands);
        for (int b = 0; b < kBands; ++b)
        {
            double e0 = fLo * std::pow (fHi / fLo, (double) b / kBands), e1 = fLo * std::pow (fHi / fLo, (double) (b + 1) / kBands);
            int lo = (int) std::floor (e0 * N / sr), hi = (int) std::floor (e1 * N / sr) - 1;
            lo = clampv (lo, 1, N / 2 - 1); hi = clampv (std::max (hi, lo), lo, N / 2 - 1);
            binLo[(size_t) b] = lo; binHi[(size_t) b] = hi; bandHz[(size_t) b] = (float) std::sqrt (e0 * e1);
        }
        reset();
    }

    void reset()
    {
        acc.clear(); frameDb.clear(); frameF0.clear(); frameConf.clear(); bandPow.clear(); envDb.clear();
        blkSum = 0; blkCount = 0; silentBlocks = 0; peakAbs = 0; activeBlocks = 0; totalSamples = 0;
    }

    void push (const float* x, int n)
    {
        if (sr <= 0 || n <= 0) return;
        totalSamples += n;
        for (int i = 0; i < n; ++i)
        {
            float v = x[i]; peakAbs = std::max (peakAbs, std::fabs (v)); blkSum += v * v;
            if (++blkCount == kBlock)
            {
                float db = 10.f * std::log10 (blkSum / (float) kBlock + 1e-12f);
                if (envDb.size() < 200000) envDb.push_back (db);
                blkSum = 0; blkCount = 0;
                if (db < -55.f) ++silentBlocks; else { silentBlocks = 0; }
                if (db > -50.f) ++activeBlocks;
            }
        }
        acc.insert (acc.end(), x, x + n);
        while ((int) acc.size() >= N)
        {
            if (frameDb.size() < 40000) processFrame (acc.data());
            acc.erase (acc.begin(), acc.begin() + hop);
        }
    }

    double seconds() const { return sr > 0 ? (double) totalSamples / sr : 0; }
    double activeSeconds() const { return sr > 0 ? (double) activeBlocks * kBlock / sr : 0; }
    double trailingSilenceSec() const { return sr > 0 ? (double) silentBlocks * kBlock / sr : 0; }

    bool finalize (AnalysisResult& r) const
    {
        r = AnalysisResult();
        r.seconds = seconds(); r.activeSeconds = activeSeconds();
        if (frameDb.size() < 20 || r.activeSeconds < 2.0) { r.message = "Not enough vocal audio (need at least ~2 s of signal). Play the vocal while analysis is running."; return false; }

        // ---- noise floor / activity
        std::vector<float> blk; for (float d : envDb) if (d > -90.f) blk.push_back (d);
        r.noiseFloorDb = blk.empty() ? -100.f : pct (blk, 0.05f);
        std::vector<float> fd (frameDb); float p90all = pct (fd, 0.9f);
        float thr = std::min (std::max (r.noiseFloorDb + 10.f, -55.f), p90all - 12.f);
        std::vector<int> act; std::vector<float> actDb;
        for (size_t i = 0; i < frameDb.size(); ++i) if (frameDb[i] > thr) { act.push_back ((int) i); actDb.push_back (frameDb[i]); }
        if (act.size() < 15) { r.message = "Signal too quiet or too short to analyse."; return false; }

        r.peakDb = gainToDb (peakAbs);
        double pw = 0; for (float d : actDb) pw += std::pow (10.0, d / 10.0);
        r.activeRmsDb = (float) (10.0 * std::log10 (pw / (double) actDb.size() + 1e-12));
        // If the recording has no real pauses, the "noise floor" is just quiet phrases: treat it as not measurable.
        if (r.activeRmsDb - r.noiseFloorDb < 20.f) r.noiseFloorDb = -100.f;
        r.levelP50 = pct (actDb, 0.5f); r.levelP90 = pct (actDb, 0.9f);
        r.dynamicRangeDb = r.levelP90 - pct (actDb, 0.1f);

        // ---- spectrum
        std::vector<double> avg (kBands, 0.0);
        for (int f : act) for (int b = 0; b < kBands; ++b) avg[(size_t) b] += bandPow[(size_t) f * kBands + (size_t) b];
        r.spectrumDb.resize (kBands);
        for (int b = 0; b < kBands; ++b) r.spectrumDb[(size_t) b] = (float) (10.0 * std::log10 (avg[(size_t) b] / (double) act.size() + 1e-12));
        auto region = [&] (float lo, float hi) { double s = 0; int c = 0; for (int b = 0; b < kBands; ++b) if (bandHz[(size_t) b] >= lo && bandHz[(size_t) b] <= hi) { s += r.spectrumDb[(size_t) b]; ++c; } return c ? (float) (s / c) : -100.f; };
        float ref = region (500, 2000);
        r.relLow = region (60, 150) - ref; r.relMud = region (200, 500) - ref; r.relPres = region (2000, 5000) - ref;
        r.relSib = region (5000, 9000) - ref; r.relAir = region (10000, 16000) - ref;
        { double sp = 0, sf = 0; for (int b = 0; b < kBands; ++b) { double p = avg[(size_t) b]; sp += p; sf += p * bandHz[(size_t) b]; } r.centroidHz = sp > 0 ? (float) (sf / sp) : 0.f; }

        // ---- pitch
        std::vector<float> midis; double sumConf = 0; r.pitchTrack.assign (frameDb.size(), 0.f);
        for (size_t i = 0; i < frameDb.size(); ++i)
            if (frameDb[i] > thr && frameF0[i] > 0.f && frameConf[i] >= 0.8f)
            {
                float m = 69.f + 12.f * std::log2 (frameF0[i] / 440.f);
                r.pitchTrack[i] = m; midis.push_back (m); sumConf += frameConf[i];
                r.pcHist[(size_t) (((int) std::lround (m) % 12 + 12) % 12)] += frameConf[i];
            }
        r.voicedRatio = (float) midis.size() / (float) act.size();
        r.clarity = midis.empty() ? 0.f : (float) (sumConf / (double) midis.size());
        if (midis.size() >= 25)
        {
            r.medianMidi = pct (midis, 0.5f); r.loMidi = pct (midis, 0.05f); r.hiMidi = pct (midis, 0.95f);
            double mean = std::accumulate (midis.begin(), midis.end(), 0.0) / (double) midis.size(), v = 0;
            for (float m : midis) v += (m - mean) * (m - mean);
            r.pitchStd = (float) std::sqrt (v / (double) midis.size());
            detectKey (r);
        }

        // ---- onsets / tempo / pace
        analyseRhythm (r, thr);

        // ---- classification (heuristic, documented in README)
        r.rapLike = r.noteRate > 4.0f && (r.hiMidi - r.loMidi) < 14.f;
        float m = r.medianMidi;
        r.voiceClass = r.rapLike ? 4 : (m <= 0 ? 1 : (m < 52 ? 0 : (m < 60 ? 1 : (m < 67 ? 2 : 3))));
        if (r.noiseFloorDb > -50.f) r.condition = 1;
        else if (r.relMud - 2.f > 4.f) r.condition = 4;
        else if (r.relSib + 12.f > 5.f) r.condition = 2;
        else if ((r.relPres + 4.f) + (r.relAir + 22.f) < -8.f) r.condition = 3;
        else r.condition = 0;
        r.valid = true;
        return true;
    }

private:
    static float pct (std::vector<float> v, float p)
    {
        if (v.empty()) return 0.f;
        std::sort (v.begin(), v.end());
        return v[(size_t) clampv ((int) std::lround (p * (float) (v.size() - 1)), 0, (int) v.size() - 1)];
    }

    void processFrame (const float* f)
    {
        float sum = 0; for (int i = 0; i < N; ++i) sum += f[i] * f[i];
        float rms = 10.f * std::log10 (sum / (float) N + 1e-12f);
        frameDb.push_back (rms);
        for (int i = 0; i < N; ++i) { re[(size_t) i] = f[i] * win[(size_t) i]; im[(size_t) i] = 0; }
        fft.forward (re.data(), im.data());
        for (int b = 0; b < kBands; ++b)
        {
            double p = 0; for (int k = binLo[(size_t) b]; k <= binHi[(size_t) b]; ++k) p += (double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k];
            bandPow.push_back ((float) (p / (double) (binHi[(size_t) b] - binLo[(size_t) b] + 1)));
        }
        float conf = 0, f0 = 0;
        if (rms > -58.f) f0 = yin.detect (f, N, sr, conf);
        frameF0.push_back (f0); frameConf.push_back (conf);
    }

    void detectKey (AnalysisResult& r) const
    {
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        auto corr = [&] (const float* prof, int tonic)
        {
            double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += r.pcHist[(size_t) ((tonic + i) % 12)]; my += prof[i]; }
            mx /= 12; my /= 12; double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i) { double a = r.pcHist[(size_t) ((tonic + i) % 12)] - mx, b = prof[i] - my; sxy += a * b; sxx += a * a; syy += b * b; }
            return sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0.0;
        };
        double best = -2, second = -2; int bt = 0; bool bm = false;
        for (int t = 0; t < 12; ++t)
            for (int m = 0; m < 2; ++m)
            {
                double c = corr (m ? mnr : maj, t);
                if (c > best) { second = best; best = c; bt = t; bm = (m == 1); }
                else if (c > second) second = c;
            }
        r.keyValid = true; r.keyRoot = bt; r.minor = bm;
        r.keyConf = clampv ((float) ((best - 0.3) / 0.5), 0.f, 1.f) * 0.6f + clampv ((float) ((best - second) / 0.1), 0.f, 1.f) * 0.4f;
    }

    void analyseRhythm (AnalysisResult& r, float activeThr) const
    {
        const size_t nb = envDb.size();
        if (nb < 200) return;
        const double hopSec = (double) kBlock / sr;
        std::vector<float> e (nb); for (size_t i = 0; i < nb; ++i) e[i] = std::max (envDb[i], -60.f);
        std::vector<float> on (nb, 0.f);
        for (size_t i = 2; i < nb; ++i) on[i] = std::max (0.f, e[i] - e[i - 2]);

        // onset rate (syllable / note pace)
        int count = 0; const int minSep = std::max (1, (int) (0.08 / hopSec)); int last = -minSep;
        for (size_t i = 3; i + 3 < nb; ++i)
            if (on[i] > 5.f && e[i] > activeThr - 6.f && on[i] >= on[i - 1] && on[i] >= on[i + 1] && on[i] >= on[i - 2] && on[i] >= on[i + 2] && (int) i - last >= minSep)
            { ++count; last = (int) i; }
        r.noteRate = r.activeSeconds > 0 ? (float) count / (float) r.activeSeconds : 0.f;

        // tempo: autocorrelation of onset strength, 60-200 BPM, weak prior around 105 BPM
        double mean = 0; for (float v : on) mean += v; mean /= (double) nb;
        std::vector<float> o (nb); double energy = 0; for (size_t i = 0; i < nb; ++i) { o[i] = on[i] - (float) mean; energy += (double) o[i] * o[i]; }
        if (energy < 1e-6) return;
        const int lagMin = std::max (2, (int) (60.0 / 200.0 / hopSec)), lagMax = std::min ((int) nb / 3, (int) (60.0 / 60.0 / hopSec));
        if (lagMax <= lagMin + 3) return;
        std::vector<double> ac ((size_t) lagMax + 2, 0.0);
        for (int lag = lagMin - 1; lag <= lagMax + 1; ++lag)
        {
            double s = 0; for (size_t i = 0; i + (size_t) lag < nb; ++i) s += (double) o[i] * o[i + (size_t) lag];
            ac[(size_t) lag] = s / energy;
        }
        double bestScore = -1e9; int bl = lagMin;
        double m = 0, m2 = 0; int cnt = 0;
        for (int lag = lagMin; lag <= lagMax; ++lag)
        {
            double bpm = 60.0 / (lag * hopSec), pr = std::exp (-0.5 * std::pow (std::log2 (bpm / 105.0) / 0.6, 2.0));
            double sc = ac[(size_t) lag] * (0.5 + 0.5 * pr);
            if (sc > bestScore && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1]) { bestScore = sc; bl = lag; }
            m += ac[(size_t) lag]; m2 += ac[(size_t) lag] * ac[(size_t) lag]; ++cnt;
        }
        m /= cnt; double sd = std::sqrt (std::max (1e-12, m2 / cnt - m * m));
        double lagF = bl; double a = ac[(size_t) bl - 1], b = ac[(size_t) bl], c = ac[(size_t) bl + 1], den = a - 2 * b + c;
        if (std::fabs (den) > 1e-12) lagF += 0.5 * (a - c) / den;
        r.bpm = (float) (std::round (60.0 / (lagF * hopSec) * 2.0) / 2.0);
        r.bpmConf = clampv ((float) (((ac[(size_t) bl] - m) / sd - 1.5) / 3.0), 0.f, 1.f);
    }

    double sr = 0; int N = 2048, hop = 1024;
    Fft fft; std::vector<float> win, re, im; YinDetector yin;
    std::vector<int> binLo, binHi; std::vector<float> bandHz;
    std::vector<float> acc, frameDb, frameF0, frameConf, bandPow, envDb;
    float blkSum = 0, peakAbs = 0; int blkCount = 0, silentBlocks = 0, activeBlocks = 0; long long totalSamples = 0;
};
}
