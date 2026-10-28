# VocalMix AI (VST3 for FL Studio, Windows)

Offline vocal analysis + auto-mix + key/BPM detection + written Beat Advisor.

## Build on GitHub (no Visual Studio needed)
1. Create a new GitHub repository and upload everything in this folder (keep the `.github` folder).
2. Open the **Actions** tab -> "Build VST3 (Windows)" runs automatically on every push (about 10-15 min first time).
3. Open the finished run -> **Artifacts** -> download `VocalMixAI-VST3-Windows`.
4. Copy the `VocalMix AI.vst3` folder to `C:\Program Files\Common Files\VST3\`.
5. FL Studio: Options -> Manage plugins -> **Find installed plugins**, then add "VocalMix AI" on a mixer insert.
6. If the build fails, open the failed step and send me the red error lines. I will fix them.

## How to use
1. Put the plugin on the vocal mixer track.
2. Press **ANALYZE** (or just play: auto-analyze starts on first audio) and play the whole vocal once.
   Press **STOP & FINISH**, or stay silent for ~2.5 s and it finishes by itself.
3. With "Auto Mix" on, the chain is set automatically; otherwise press **AUTO MIX**.
4. Check KEY/BPM confidence. Correct them in the MANUAL strip if confidence is low.
5. Read the **AI Beat Advisor** panel; change genre/mood, **Regenerate**, or **Export MIDI** (chords, bass, drums).
6. **A/B Bypass** compares before/after (time-aligned). **Undo** / **Reset** restore settings.

## What is real
Real DSP: high-pass, noise gate/expander, pitch correction, 4-band EQ, de-esser, compressor, leveler,
saturation, delay, reverb, stereo width, limiter. Real analysis: loudness, noise floor, spectrum, pitch (YIN),
key (Krumhansl-Schmuckler), tempo (onset autocorrelation), delivery pace.
1200 procedurally generated presets (genre x mood x voice x condition), each with different settings, plus user presets.

## Honest limitations
- About 14 processing stages, not "20+ effects / 100+ tools".
- Pitch correction is basic (delay-line shifter): good for small corrections, audible artefacts on big shifts.
- BPM from a solo vocal is a guess (see the confidence bar). Key detection is heuristic.
- The plugin can only analyse audio that flows through it while playing; it cannot read the audio file itself.
- Beat Advisor is rule-based text from measured features. It does not generate audio.
- Cannot load or control your other VST plugins; everything uses the built-in engine.
- Core logic (analysis, DSP, presets, advisor) was tested on synthetic signals. The JUCE UI/plugin layer has NOT been
  compiled or tested in FL Studio yet.
