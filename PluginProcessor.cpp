#include "PluginProcessor.h"
#include "PluginEditor.h"

VocalMixProcessor::VocalMixProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", vm::createLayout())
{
    for (int i = 0; i < vm::PidCount; ++i) raw.push_back (apvts.getRawParameterValue (vm::paramDef (i).id));
    presets = vm::buildFactoryPresets();
    loadUserPresets();
    fifoData.assign ((size_t) fifo.getTotalSize(), 0.f);
    currentAdvice.text = "Run an analysis first: press ANALYZE, play your vocal once, then press STOP (or just play it - the plugin finishes automatically after a few seconds of silence).";
    startTimerHz (20);
}

VocalMixProcessor::~VocalMixProcessor() { stopTimer(); }

bool VocalMixProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void VocalMixProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    chain.prepare (sr);
    setLatencySamples (chain.latency());
    dryDelay.prepare (chain.latency());
    analyzer.prepare (sr);
    fifo.reset();
    tapBuf.assign ((size_t) maxBlock + 16, 0.f);
    drainBuf.assign ((size_t) fifo.getTotalSize(), 0.f);
    wetL.assign ((size_t) maxBlock + 16, 0.f); wetR = wetL;
    reverb.setSampleRate (sr); reverb.reset();
    dryBuf.setSize (2, maxBlock + 16);
    bypassMix = getParam (vm::bypass) > 0.5f ? 1.f : 0.f;
}

void VocalMixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = std::min (buffer.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (nCh == 0 || n == 0 || n > (int) tapBuf.size() - 16) return;

    float pk = 0; for (int c = 0; c < nCh; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    inPeak.store (std::max (inPeak.load(), pk));

    // analysis tap (input, mono)
    if (analyzing.load())
    {
        for (int i = 0; i < n; ++i) { float m = 0; for (int c = 0; c < nCh; ++c) m += buffer.getReadPointer (c)[i]; tapBuf[(size_t) i] = m / (float) nCh; }
        int s1, z1, s2, z2; fifo.prepareToWrite (n, s1, z1, s2, z2);
        if (z1 > 0) std::copy (tapBuf.begin(), tapBuf.begin() + z1, fifoData.begin() + s1);
        if (z2 > 0) std::copy (tapBuf.begin() + z1, tapBuf.begin() + z1 + z2, fifoData.begin() + s2);
        fifo.finishedWrite (z1 + z2);
    }
    else if (! hasResult.load() && getParam (vm::autoAnalyze) > 0.5f && pk > 0.03f)
        requestStart.store (true);

    vm::Settings s;
    s.inGain = getParam (vm::inGain); s.hp = getParam (vm::hpFreq); s.gateThr = getParam (vm::gateThr);
    s.eqLow = getParam (vm::eqLow); s.eqMud = getParam (vm::eqMud); s.eqPres = getParam (vm::eqPres); s.eqAir = getParam (vm::eqAir);
    s.compThr = getParam (vm::compThr); s.compRatio = getParam (vm::compRatio); s.compAtk = getParam (vm::compAtk); s.compRel = getParam (vm::compRel); s.compMake = getParam (vm::compMake);
    s.deEss = getParam (vm::deEss); s.deEssFreq = getParam (vm::deEssFreq); s.levelAmt = getParam (vm::levelAmt);
    s.satDrive = getParam (vm::satDrive); s.satMix = getParam (vm::satMix);
    s.dlyMix = getParam (vm::dlyMix); s.dlyTime = getParam (vm::dlyTime); s.dlyFb = getParam (vm::dlyFb);
    s.revMix = getParam (vm::revMix); s.revSize = getParam (vm::revSize); s.revDamp = getParam (vm::revDamp);
    s.width = getParam (vm::width); s.limCeil = getParam (vm::limCeil); s.outGain = getParam (vm::outGain);
    s.pitchAmt = getParam (vm::pitchAmt); s.pitchSpeed = getParam (vm::pitchSpeed);
    s.pitchOn = getParam (vm::pitchOn) > 0.5f; s.pitchKey = (int) getParam (vm::pitchKey); s.pitchScale = (int) getParam (vm::pitchScale);

    for (int c = 0; c < nCh; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    float* ch[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    chain.processPre (ch, nCh, n, s);

    if (s.revMix > 0.001f)
    {
        juce::Reverb::Parameters rp; rp.roomSize = 0.2f + 0.78f * s.revSize; rp.damping = s.revDamp; rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        std::copy (ch[0], ch[0] + n, wetL.begin());
        if (nCh > 1) { std::copy (ch[1], ch[1] + n, wetR.begin()); reverb.processStereo (wetL.data(), wetR.data(), n); }
        else reverb.processMono (wetL.data(), n);
        for (int i = 0; i < n; ++i) { ch[0][i] += wetL[(size_t) i] * s.revMix; if (nCh > 1) ch[1][i] += wetR[(size_t) i] * s.revMix; }
    }
    chain.processPost (ch, nCh, n, s);

    // A/B bypass: crossfade to the time-aligned dry signal
    const float target = getParam (vm::bypass) > 0.5f ? 1.f : 0.f, k = 1.f - std::exp (-1.f / (0.01f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        bypassMix += (target - bypassMix) * k;
        for (int c = 0; c < nCh; ++c)
        {
            float d = dryDelay.tick (c, dryBuf.getReadPointer (c)[i]);
            ch[c][i] = ch[c][i] * (1.f - bypassMix) + d * bypassMix;
        }
        dryDelay.advance();
    }

    float op = 0; for (int c = 0; c < nCh; ++c) op = std::max (op, buffer.getMagnitude (c, 0, n));
    outPeak.store (std::max (outPeak.load(), op));
    compGr.store (chain.compGrDb.load());
    pitchHz.store (chain.pitch.detectedHz.load()); targetHz.store (chain.pitch.targetHz.load());
}

//==============================================================================
void VocalMixProcessor::drainFifo()
{
    int s1, z1, s2, z2; const int ready = fifo.getNumReady();
    if (ready <= 0) return;
    fifo.prepareToRead (ready, s1, z1, s2, z2);
    int w = 0;
    if (z1 > 0) { std::copy (fifoData.begin() + s1, fifoData.begin() + s1 + z1, drainBuf.begin()); w = z1; }
    if (z2 > 0) { std::copy (fifoData.begin() + s2, fifoData.begin() + s2 + z2, drainBuf.begin() + w); w += z2; }
    fifo.finishedRead (z1 + z2);
    analyzer.push (drainBuf.data(), w);
}

void VocalMixProcessor::timerCallback()
{
    if (requestStart.exchange (false) && ! analyzing.load() && ! hasResult.load()) startAnalysis();
    if (analyzing.load())
    {
        drainFifo();
        setStatus ("Analyzing... " + juce::String (analyzer.seconds(), 1) + " s captured. Play your full vocal, then press STOP.");
        if (analyzer.activeSeconds() > 4.0 && analyzer.trailingSilenceSec() > 2.5) stopAnalysis();
        else if (analyzer.seconds() > 300.0) stopAnalysis();
    }
}

void VocalMixProcessor::startAnalysis()
{
    analyzer.reset(); fifo.reset();
    hasResult = false; analyzing = true;
    setStatus ("Analyzing... play your vocal (analysis only hears audio that passes through this plugin).");
}

void VocalMixProcessor::stopAnalysis()
{
    if (! analyzing.exchange (false)) return;
    drainFifo();
    vm::AnalysisResult r;
    if (! analyzer.finalize (r)) { setStatus ("Analysis failed: " + juce::String (r.message), true); return; }
    lastResult = r; hasResult = true; ++resVersion;
    if (r.keyValid && r.keyConf >= 0.35f)
    {
        setParamPlain (vm::pitchKey, (float) r.keyRoot); setParamPlain (vm::pitchScale, r.minor ? 2.f : 1.f);
    }
    regenerateAdvisor (false);
    if (getParam (vm::autoApply) > 0.5f) { runAutoMix(); return; }
    setStatus ("Analysis complete. Press AUTO MIX to apply the suggested chain.");
}

void VocalMixProcessor::setParamPlain (int pid, float plain)
{
    if (auto* p = apvts.getParameter (vm::paramDef (pid).id))
    { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
}

void VocalMixProcessor::pushUndo()
{
    undoStack.push_back (apvts.copyState());
    if (undoStack.size() > 30) undoStack.erase (undoStack.begin());
}

void VocalMixProcessor::undo()
{
    if (undoStack.empty()) { setStatus ("Nothing to undo."); return; }
    apvts.replaceState (undoStack.back()); undoStack.pop_back();
    setStatus ("Undone.");
}

void VocalMixProcessor::resetAll()
{
    pushUndo();
    for (int i = 0; i < vm::PidCount; ++i) setParamPlain (i, vm::paramDef (i).def);
    setStatus ("All controls reset to default.");
}

void VocalMixProcessor::runAutoMix()
{
    if (! lastResult.valid) { setStatus ("Run ANALYZE first, then AUTO MIX.", true); return; }
    pushUndo();
    for (auto& p : vm::computeAutoMix (lastResult)) setParamPlain (p.first, p.second);
    juce::String extra;
    if (lastResult.noiseFloorDb <= -99.f) extra = " (noise floor not measurable: no pauses found, gate left off)";
    if (! (lastResult.keyValid && lastResult.keyConf >= 0.5f)) extra += " Key confidence is low, so pitch correction was left OFF: set key/scale manually.";
    setStatus ("Auto mix applied from the analysis." + extra);
}

void VocalMixProcessor::applyPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    pushUndo();
    for (auto& p : presets[(size_t) index].values) setParamPlain (p.first, p.second);
    setStatus ("Preset loaded: " + juce::String (presets[(size_t) index].name));
}

juce::File VocalMixProcessor::presetDir() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("VocalMixAI").getChildFile ("Presets");
}

bool VocalMixProcessor::saveUserPreset (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) { setStatus ("Error: preset name is empty.", true); return false; }
    auto dir = presetDir();
    if (! dir.createDirectory().wasOk()) { setStatus ("Error: could not create the preset folder.", true); return false; }
    juce::XmlElement xml ("VocalMixPreset"); xml.setAttribute ("name", name.trim());
    vm::Preset p; p.name = name.trim().toStdString(); p.genre = "User"; p.user = true;
    for (int id : vm::presetParamIds())
    {
        float v = getParam (id);
        auto* e = xml.createNewChildElement ("P"); e->setAttribute ("id", vm::paramDef (id).id); e->setAttribute ("v", (double) v);
        p.values.push_back ({ id, v });
    }
    if (! dir.getChildFile (clean + ".xml").replaceWithText (xml.toString())) { setStatus ("Error: could not write the preset file.", true); return false; }
    presets.push_back (p);
    setStatus ("Preset saved: " + name.trim());
    return true;
}

void VocalMixProcessor::loadUserPresets()
{
    for (auto& f : presetDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
    {
        auto xml = juce::parseXML (f);
        if (xml == nullptr || ! xml->hasTagName ("VocalMixPreset")) continue;
        vm::Preset p; p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension()).toStdString(); p.genre = "User"; p.user = true;
        for (auto* e : xml->getChildIterator())
            for (int i = 0; i < vm::PidCount; ++i)
                if (e->getStringAttribute ("id") == vm::paramDef (i).id) p.values.push_back ({ i, (float) e->getDoubleAttribute ("v") });
        if (! p.values.empty()) presets.push_back (p);
    }
}

int VocalMixProcessor::recommendedGenreIndex() const
{
    if (! currentAdvice.valid) return -1;
    for (int g = 0; g < vm::kNumGenres; ++g) if (currentAdvice.plan.genre == vm::genreNames()[g]) return g;
    return -1;
}

void VocalMixProcessor::regenerateAdvisor (bool newSeed)
{
    if (newSeed) ++advSeed;
    int scale = (int) getParam (vm::pitchScale);
    int key = lastResult.keyRoot; bool minor = lastResult.minor;
    if (scale != 0) { key = (int) getParam (vm::pitchKey); minor = (scale == 2); }
    currentAdvice = vm::generateAdvice (lastResult, key, minor, getParam (vm::bpmManual), advGenre, advMood, advSeed);
    ++advVersion;
}

bool VocalMixProcessor::exportMidi (const juce::File& file)
{
    if (! currentAdvice.valid) { setStatus ("Error: generate a beat recommendation first (run ANALYZE).", true); return false; }
    const auto& pl = currentAdvice.plan;
    const int tpq = 480, bar = tpq * 4, step = tpq / 4;
    juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence tempo; tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / std::max (40, pl.bpm))), 0.0); mf.addTrack (tempo);

    juce::MidiMessageSequence chords, bass, drums;
    for (int b = 0; b < 8; ++b)
    {
        const auto& cd = pl.prog[(size_t) b % pl.prog.size()];
        for (int note : vm::chordNotes (cd, pl.keyRoot))
        {
            chords.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 80), (double) (b * bar));
            chords.addEvent (juce::MidiMessage::noteOff (1, note), (double) ((b + 1) * bar - 10));
        }
        int root = 36 + (pl.keyRoot + cd.root) % 12; if (root > 42) root -= 12;
        std::vector<int> ks; for (int i = 0; i < 16; ++i) if (pl.kick[(size_t) i] == '1') ks.push_back (i);
        for (size_t k = 0; k < ks.size(); ++k)
        {
            int st = ks[k], en = k + 1 < ks.size() ? ks[k + 1] : 16;
            bass.addEvent (juce::MidiMessage::noteOn (2, root, (juce::uint8) 100), (double) (b * bar + st * step));
            bass.addEvent (juce::MidiMessage::noteOff (2, root), (double) (b * bar + en * step - 10));
        }
        for (int i = 0; i < 16; ++i)
        {
            const double t = (double) (b * bar + i * step);
            auto hit = [&] (int note, int vel) { drums.addEvent (juce::MidiMessage::noteOn (10, note, (juce::uint8) vel), t); drums.addEvent (juce::MidiMessage::noteOff (10, note), t + 100.0); };
            if (pl.kick[(size_t) i] == '1') hit (36, 110);
            if (pl.snare[(size_t) i] == '1') hit (38, 100);
            if (pl.hat[(size_t) i] == '1') hit (42, (i % 4 == 0) ? 85 : 65);
        }
    }
    chords.updateMatchedPairs(); bass.updateMatchedPairs(); drums.updateMatchedPairs();
    mf.addTrack (chords); mf.addTrack (bass); mf.addTrack (drums);

    juce::FileOutputStream os (file);
    if (! os.openedOk()) { setStatus ("Error: cannot write " + file.getFullPathName(), true); return false; }
    os.setPosition (0); os.truncate();
    if (! mf.writeTo (os)) { setStatus ("Error: MIDI write failed.", true); return false; }
    setStatus ("MIDI exported: " + file.getFileName());
    return true;
}

void VocalMixProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void VocalMixProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VocalMixProcessor::createEditor() { return new VocalMixEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalMixProcessor(); }
