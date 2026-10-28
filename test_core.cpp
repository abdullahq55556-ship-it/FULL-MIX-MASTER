#include "../Source/BeatAdvisor.h"
#include "../Source/Presets.h"
#include <cstdio>
#include <random>
using namespace vm;

static std::vector<float> synth (double sr, double bpm, const std::vector<int>& notes, int repeats, float noiseAmp)
{
    std::vector<float> out; std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 1.f);
    double beat = 60.0 / bpm; double ph = 0;
    for (int rep = 0; rep < repeats; ++rep)
        for (int n : notes)
        {
            int len = (int) (beat * sr); double f0 = 440.0 * std::pow (2.0, (n - 69) / 12.0);
            for (int i = 0; i < len; ++i)
            {
                double t = (double) i / sr, vib = 1.0 + 0.004 * std::sin (2 * kPiD * 5.5 * t);
                ph += 2 * kPiD * f0 * vib / sr;
                double env = std::min (1.0, t / 0.01) * std::exp (-t * 1.5) ;
                float s = 0; for (int h = 1; h <= 8; ++h) s += (float) (std::sin (h * ph) / h);
                out.push_back (0.25f * (float) env * s + noiseAmp * nd (rng));
            }
        }
    return out;
}

int main()
{
    const double sr = 44100;
    // C minor phrase (C4 Eb4 G4 F4 Eb4 D4 C4 Eb4), one note per beat at 120 BPM
    auto sig = synth (sr, 120, { 60, 63, 67, 65, 63, 62, 60, 63 }, 5, 0.0005f);
    { // second case: pauses + a real noise floor (~ -60 dB)
        auto sg = synth (sr, 120, { 60, 63, 67, 65 }, 6, 0.0f); std::mt19937 rg (3); std::normal_distribution<float> nd (0.f, 1.f);
        size_t bl = (size_t) (0.5 * sr); for (size_t i = 0; i < sg.size(); ++i) { size_t pos = i % bl; if (pos > bl * 6 / 10) sg[i] = 0; sg[i] += 0.001f * nd (rg); }
        Analyzer a2; a2.prepare (sr); for (size_t i = 0; i < sg.size(); i += 512) a2.push (sg.data() + i, (int) std::min<size_t> (512, sg.size() - i));
        AnalysisResult r2; a2.finalize (r2); auto m2 = computeAutoMix (r2);
        float gate = 0; for (auto& p : m2) if (p.first == gateThr) gate = p.second;
        std::printf ("CASE2 noise=%.1f (expect ~-60) cond=%s gate=%.1f key=%s %s bpm=%.1f\n", r2.noiseFloorDb, condNames()[r2.condition], gate, noteNames()[r2.keyRoot], r2.minor ? "minor" : "major", r2.bpm);
    }
    Analyzer an; an.prepare (sr);
    for (size_t i = 0; i < sig.size(); i += 512) an.push (sig.data() + i, (int) std::min<size_t> (512, sig.size() - i));
    AnalysisResult r; bool ok = an.finalize (r);
    std::printf ("finalize=%d msg=%s\n", ok, r.message.c_str());
    std::printf ("sec=%.1f active=%.1f rms=%.1f noise=%.1f DR=%.1f\n", r.seconds, r.activeSeconds, r.activeRmsDb, r.noiseFloorDb, r.dynamicRangeDb);
    std::printf ("pitch median=%s lo=%s hi=%s clarity=%.2f voiced=%.2f\n", midiName (r.medianMidi).c_str(), midiName (r.loMidi).c_str(), midiName (r.hiMidi).c_str(), r.clarity, r.voicedRatio);
    std::printf ("key=%s %s conf=%.2f (expected C minor)\n", noteNames()[r.keyRoot], r.minor ? "minor" : "major", r.keyConf);
    std::printf ("bpm=%.1f conf=%.2f noteRate=%.2f (expected ~120 BPM, 2 onsets/s)\n", r.bpm, r.bpmConf, r.noteRate);
    std::printf ("voice=%s cond=%s rap=%d\n", voiceNames()[r.voiceClass], condNames()[r.condition], r.rapLike);

    auto mix = computeAutoMix (r);
    std::printf ("automix params:"); for (auto& p : mix) std::printf (" %s=%.2f", paramDef (p.first).id, p.second); std::printf ("\n");

    auto presets = buildFactoryPresets();
    std::printf ("presets=%zu\n", presets.size());
    // uniqueness of settings
    std::vector<std::string> sigs; for (auto& p : presets) { std::string s; for (auto& v : p.values) s += std::to_string (v.second) + ","; sigs.push_back (s); }
    std::sort (sigs.begin(), sigs.end()); size_t uniq = std::unique (sigs.begin(), sigs.end()) - sigs.begin();
    std::printf ("unique setting-sets=%zu of %zu\n", uniq, sigs.size());
    auto rec = recommendPresets (presets, r.voiceClass, r.condition, 1, 0, 5);
    for (int i : rec) std::printf ("  rec: %s\n", presets[(size_t) i].name.c_str());

    auto adv = generateAdvice (r, r.keyRoot, r.minor, 0, -1, -1, 1);
    std::printf ("\n%s\n\n", adv.text.c_str());

    // chain: NaN / level sanity + pitch correction check
    VocalChain ch; ch.prepare (sr);
    Settings s; s.pitchOn = true; s.pitchKey = 9; s.pitchScale = 1; s.pitchAmt = 1.0f; s.pitchSpeed = 0.0f; s.revMix = 0; s.dlyMix = 0.1f; s.gateThr = -60;
    std::vector<float> in ((size_t) (sr * 3)), buf;
    for (size_t i = 0; i < in.size(); ++i) in[i] = 0.3f * std::sin (2 * kPiD * 452.0 * (double) i / sr);   // ~A4 + 47 cents
    buf = in; bool bad = false; float peak = 0;
    for (size_t i = 0; i < buf.size(); i += 480)
    {
        int n = (int) std::min<size_t> (480, buf.size() - i); float* p = buf.data() + i; float* chs[1] = { p };
        ch.processPre (chs, 1, n, s); ch.processPost (chs, 1, n, s);
    }
    for (float v : buf) { if (! std::isfinite (v)) bad = true; peak = std::max (peak, std::fabs (v)); }
    YinDetector y; y.prepare (700); float conf; float f = y.detect (buf.data() + (size_t) (sr * 2), 1400, sr, conf);
    std::printf ("chain: nan=%d peak=%.2f latency=%d  corrected pitch: in 452.0 Hz -> out %.1f Hz (target 440.0) conf=%.2f\n", bad, peak, ch.latency(), f, conf);

    // pitch corrector disabled => should be a clean delay
    VocalChain c2; c2.prepare (sr); Settings s2; s2.eqLow = s2.eqMud = s2.eqPres = s2.eqAir = 0; s2.hp = 20; s2.deEss = 0; s2.compRatio = 1; s2.levelAmt = 0; s2.satDrive = 0; s2.gateThr = -90; s2.revMix = 0; s2.dlyMix = 0; s2.limCeil = 0; s2.compMake = 0;
    buf = in;
    for (size_t i = 0; i < buf.size(); i += 480) { int n = (int) std::min<size_t> (480, buf.size() - i); float* p = buf.data() + i; float* chs[1] = { p }; c2.processPre (chs, 1, n, s2); c2.processPost (chs, 1, n, s2); }
    double e1 = 0, e2 = 0; for (size_t i = (size_t) sr; i < buf.size(); ++i) { e1 += in[i] * in[i]; e2 += buf[i] * buf[i]; }
    std::printf ("passthrough RMS ratio (should be ~1): %.3f\n", std::sqrt (e2 / e1));
    return 0;
}
