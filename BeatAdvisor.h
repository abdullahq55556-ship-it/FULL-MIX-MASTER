#pragma once
// JUCE-free: rule-based beat recommendation generated from the measured vocal features.
// It describes a beat (and provides MIDI-able chord/bass/drum data). It does NOT generate audio.
#include "Analyzer.h"
#include <cstdio>
#include <random>
#include <sstream>
#include <string>

namespace vm
{
struct Chord { int root; int quality; };   // quality: 0 maj, 1 min, 2 maj7, 3 min7, 4 dom7

struct BeatPlan
{
    int bpm = 90, keyRoot = 0; bool minor = true;
    std::string genre, mood, kick, snare, hat;
    std::vector<Chord> prog;
};

struct Advice { std::string text; BeatPlan plan; bool valid = false; };

inline std::string fmt (const char* f, double a = 0, double b = 0, double c = 0)
{ char buf[256]; std::snprintf (buf, sizeof buf, f, a, b, c); return buf; }

inline std::string midiName (float m)
{
    int n = (int) std::lround (m); return std::string (noteNames()[((n % 12) + 12) % 12]) + std::to_string (n / 12 - 1);
}

inline std::string chordName (const Chord& c, int keyRoot)
{
    static const char* flat[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
    static const char* q[5] = { "", "m", "maj7", "m7", "7" };
    return std::string (flat[(keyRoot + c.root) % 12]) + q[c.quality];
}

inline std::vector<int> chordNotes (const Chord& c, int keyRoot)
{
    int root = 60 + (keyRoot + c.root) % 12; if (root >= 66) root -= 12;
    std::vector<int> n = { root };
    const bool minorTriad = c.quality == 1 || c.quality == 3;
    n.push_back (root + (minorTriad ? 3 : 4)); n.push_back (root + 7);
    if (c.quality == 2) n.push_back (root + 11); else if (c.quality == 3 || c.quality == 4) n.push_back (root + 10);
    return n;
}

inline Advice generateAdvice (const AnalysisResult& r, int keyRoot, bool minor, float manualBpm, int genreOverride, int moodOverride, unsigned seed)
{
    Advice a;
    if (! r.valid) { a.text = "Run an analysis first: press ANALYZE, play your vocal once, then press STOP (or just play it - the plugin finishes automatically after a few seconds of silence)."; return a; }
    std::mt19937 rng (seed);
    auto pick = [&] (int n) { return (int) (rng() % (unsigned) n); };
    auto norm = [] (float v, float lo, float hi) { return clampv ((v - lo) / (hi - lo), 0.f, 1.f); };

    const float energy = 0.45f * norm (r.noteRate, 1.f, 6.f) + 0.35f * norm (r.activeRmsDb, -32.f, -12.f) + 0.20f * norm (r.centroidHz, 600.f, 2200.f);
    const bool keyKnown = r.keyValid || true;
    (void) keyKnown;

    // ---- mood
    int mood = moodOverride;
    if (mood < 0) mood = minor ? (energy < 0.5f ? 0 : 4) : (energy < 0.5f ? 2 : 3);
    // ---- genre
    int genre = genreOverride;
    if (genre < 0)
    {
        if (r.rapLike) genre = minor ? (energy > 0.55f ? 1 : 2) : 0;
        else if (energy > 0.62f) genre = 4;
        else if (r.noteRate < 2.f && energy < 0.45f) genre = minor ? 5 : 3;
        else if (energy < 0.4f) genre = 6;
        else genre = 3;
    }

    // ---- genre knowledge tables
    struct GK { const char* name; int lo, hi; const char* kick; const char* snare; const char* hat; const char* drums; const char* bass; };
    static const GK gk[kNumGenres] = {
        { "Hip-Hop", 84, 96, "1000000010100000", "0000100000001000", "1010101010101010", "punchy dusty kick, snare/clap on beats 2 and 4, straight 8th hi-hats with slight swing", "round sub-bass or short 808 following the kick, 1-2 notes per bar" },
        { "Trap", 130, 150, "1000001000100000", "0000000010000000", "1111111111111111", "half-time feel: kick on 1 plus syncopated hits, clap/snare on beat 3, 16th hi-hats with velocity variation and occasional rolls", "long gliding 808 (tuned to the key) with short slides between notes" },
        { "Melodic Rap", 78, 100, "1000001000100000", "0000000010000000", "1010101010101011", "half-time kick/clap, light 8th hi-hats with small triplet fills, soft percussion", "deep but controlled 808, mostly root and fifth, clean sustain" },
        { "R&B", 68, 90, "1000000100100000", "0000100000001000", "1011101110111011", "soft kick, snap or rim on 2 and 4, swung 16th hats played quietly", "warm sub or electric-style bass, slightly behind the beat" },
        { "Pop", 100, 126, "1000100010001000", "0000100000001000", "0010001000100010", "steady four-on-the-floor or half-time kick, bright clap on 2 and 4, open hat on the off-beats", "tight synth bass or short 808 locked to the kick" },
        { "Ballad", 62, 76, "1000000000100000", "0000000010000000", "1010101010101010", "very sparse: soft kick, light rim or brushed snare on beat 3, quiet shaker", "soft sustained bass notes on the chord roots, no heavy 808" },
        { "Lo-fi", 70, 86, "1000000100100000", "0000100000001000", "1011101011101010", "dusty swung drums, soft kick, vinyl-style snare, loose 8th hats", "mellow sine/sub bass with gentle tape-style saturation" },
        { "Afrobeats", 98, 112, "1000001000100000", "0000100000001000", "1011101110111011", "syncopated kick, clap on 2 and 4, shaker and congas carrying the groove", "rounded, bouncy bass with space between notes" }
    };
    static const char* moodName[kNumMoods] = { "emotional", "sad / melancholic", "romantic and warm", "energetic and uplifting", "dark and intense", "chilled and relaxed" };
    static const char* inst[kNumMoods][4] = {
        { "soft felt piano chords", "airy atmospheric pad", "light plucked guitar or bell melody", "subtle strings in the hook" },
        { "sparse piano", "dark ambient pad", "slow reversed vocal-style textures", "distant strings" },
        { "warm Rhodes / electric piano", "silky pad", "clean muted guitar", "soft vocal-chop harmony" },
        { "bright synth stabs", "punchy plucks", "layered supersaw pad (kept low in the mids)", "claps and risers for the hook" },
        { "detuned dark pad", "minor bell or pluck melody", "low choir texture", "distorted-but-controlled 808" },
        { "mellow keys", "tape-style pad", "soft guitar loop", "vinyl crackle and light percussion" }
    };

    // ---- tempo
    BeatPlan& pl = a.plan; pl.keyRoot = keyRoot; pl.minor = minor; pl.genre = gk[genre].name; pl.mood = moodName[mood];
    pl.kick = gk[genre].kick; pl.snare = gk[genre].snare; pl.hat = gk[genre].hat;
    std::string tempoWhy;
    if (manualBpm >= 40.f) { pl.bpm = (int) std::lround (manualBpm); tempoWhy = "your manual BPM setting"; }
    else if (r.bpmConf >= 0.45f && r.bpm > 0)
    {
        double b = r.bpm; while (b < gk[genre].lo * 0.9) b *= 2; while (b > gk[genre].hi * 1.1) b *= 0.5;
        pl.bpm = (int) std::lround (b);
        tempoWhy = "the phrasing tempo measured from your vocal (" + fmt ("%.0f", r.bpm) + " BPM, confidence " + fmt ("%.0f", r.bpmConf * 100) + "%), folded into the typical range of this genre";
    }
    else
    {
        pl.bpm = (int) std::lround (gk[genre].lo + (gk[genre].hi - gk[genre].lo) * energy);
        tempoWhy = "the genre's usual range scaled by your vocal's energy, because the vocal's own tempo was not clear enough to trust (BPM confidence " + fmt ("%.0f", r.bpmConf * 100) + "%)";
    }

    // ---- harmony
    static const std::vector<std::vector<Chord>> minorProg = {
        { {0,1}, {8,0}, {3,0}, {10,0} }, { {0,1}, {5,1}, {8,0}, {10,0} }, { {8,0}, {10,0}, {0,1}, {0,1} }, { {0,1}, {10,0}, {8,0}, {7,0} } };
    static const std::vector<std::vector<Chord>> minorProgJazz = {
        { {0,3}, {8,2}, {3,2}, {10,4} }, { {0,3}, {5,3}, {8,2}, {7,4} } };
    static const std::vector<std::vector<Chord>> majorProg = {
        { {0,0}, {7,0}, {9,1}, {5,0} }, { {9,1}, {5,0}, {0,0}, {7,0} }, { {0,0}, {9,1}, {5,0}, {7,0} } };
    static const std::vector<std::vector<Chord>> majorProgJazz = {
        { {2,3}, {7,4}, {0,2}, {9,3} }, { {0,2}, {9,3}, {2,3}, {7,4} } };
    const bool rich = genre == 3 || genre == 5 || genre == 6;
    const auto& pool = minor ? (rich ? minorProgJazz : minorProg) : (rich ? majorProgJazz : majorProg);
    pl.prog = pool[(size_t) pick ((int) pool.size())];
    std::string chords; for (size_t i = 0; i < pl.prog.size(); ++i) chords += (i ? " - " : "") + chordName (pl.prog[i], keyRoot);

    // ---- text
    const std::string keyStr = std::string (noteNames()[keyRoot]) + (minor ? " minor" : " major");
    std::ostringstream t;
    t << "RECOMMENDED BEAT\n" << pl.mood << " " << pl.genre << ", " << pl.bpm << " BPM, " << keyStr << ".\n\n";
    t << "WHY THIS FITS YOUR VOCAL (measured)\n";
    t << "- Key: " << (r.keyValid ? "detected " : "using ") << keyStr << (r.keyValid ? " (confidence " + fmt ("%.0f", r.keyConf * 100) + "%)" : std::string (" (set manually)")) << ". Building the beat in the same key keeps the harmony and pitch correction consistent.\n";
    if (r.medianMidi > 0) t << "- Range: mostly " << midiName (r.loMidi) << " to " << midiName (r.hiMidi) << " (centre " << midiName (r.medianMidi) << "). "
        << (r.medianMidi < 55 ? "Low voice: keep the 808/bass tight and out of the vocal's low end, leave 150-400 Hz clear.\n" : r.medianMidi > 66 ? "High voice: you can use fuller low-mid instruments and a deeper bass without masking it.\n" : "Mid-range voice: keep pads and keys out of the 1-4 kHz zone where the vocal lives.\n");
    t << "- Delivery: about " << fmt ("%.1f", r.noteRate) << " note/syllable onsets per second, " << (r.rapLike ? "which sounds like rap/spoken flow, so the drums stay simple and the melody sparse." : r.noteRate < 2.f ? "a slow, sustained delivery, so the beat should leave space and let long notes ring." : "a medium singing pace, so a steady groove with a melodic hook works well.") << "\n";
    t << "- Energy: " << fmt ("%.0f", energy * 100) << "/100 from pace, loudness and brightness, which suggests this mood: " << moodName[mood] << ".\n";
    t << "- Tempo source: " << tempoWhy << ".\n";
    t << "- Tone: " << (r.relAir > -18.f ? "the vocal is already bright, so choose darker pads and roll off the beat's highs above ~10 kHz." : "the vocal is fairly dark, so a little air and shimmer in the beat will help it stand out.") << "\n\n";

    t << "DRUMS\n" << gk[genre].drums << ".\n";
    t << "Kick  : " << pl.kick << "\nSnare : " << pl.snare << "\nHat   : " << pl.hat << "   (16 steps per bar)\n\n";
    t << "BASS / 808\n" << gk[genre].bass << ". Root note of the key: " << noteNames()[keyRoot] << ".\n\n";
    t << "HARMONY\nSuggested progression (one chord per bar): " << chords << ".\n";
    t << "Sounds: " << inst[mood][pick (4)] << ", " << inst[mood][pick (4)] << ", " << inst[mood][pick (4)] << ".\n";
    t << "Melody: play it in " << keyStr << ", sparse, and mostly in the gaps between your vocal phrases.\n\n";

    const int bars = std::max (8, (int) std::lround (r.seconds * pl.bpm / 240.0));
    t << "ARRANGEMENT PLAN\nYour recording is about " << (int) r.seconds << " s, roughly " << bars << " bars at " << pl.bpm << " BPM.\n";
    t << "1. Intro (4 bars): chords/pad only, no drums.\n2. Verse (8-16 bars): " << (r.rapLike ? "kick + light hats, minimal melody, keep it simple under the flow" : "minimal chords, soft kick, very light hats") << ".\n";
    t << "3. Hook (8 bars): add full drums, bass/808, melody and a wider pad so the energy lifts.\n4. Verse 2: like verse 1 with one new element.\n5. Outro (4 bars): remove drums, let the chords ring out.\n\n";
    t << "PRODUCTION TECHNIQUES\n- Sidechain pads/bass lightly to the vocal or high-pass beat elements below ~150 Hz except the bass.\n- Carve 2-4 kHz in the instruments by 2-3 dB to leave room for the vocal.\n- Use the 'Export MIDI' button for the chords, bass and drum pattern above.\n\n";
    t << "NOTE: this is a written plan produced by rules applied to your measured vocal features. The plugin does not generate audio for the beat.";
    a.text = t.str(); a.valid = true;
    return a;
}
}
