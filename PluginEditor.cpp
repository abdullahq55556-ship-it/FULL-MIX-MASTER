#include "PluginEditor.h"

//==============================================================================
void Dashboard::card (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& value, float conf)
{
    g.setColour (Col::card); g.fillRoundedRectangle (r.toFloat(), 8.f);
    auto top = r.reduced (10, 6).removeFromTop (12);
    g.setColour (Col::dim); g.setFont (10.5f); g.drawText (title, top, juce::Justification::left);
    const juce::Colour cc = conf >= 0.6f ? Col::good : conf >= 0.35f ? Col::amber : Col::warn;
    if (conf >= 0.f) { g.setColour (cc); g.drawText (juce::String ((int) (conf * 100.f)) + "%", top, juce::Justification::right); }
    g.setColour (Col::text); g.setFont (g.getCurrentFont().withHeight (17.f).boldened());
    g.drawText (value, r.reduced (10, 0).withTrimmedTop (18).withTrimmedBottom (10), juce::Justification::left);
    if (conf >= 0.f)
    {
        auto bar = juce::Rectangle<float> ((float) r.getX() + 10.f, (float) r.getBottom() - 9.f, (float) r.getWidth() - 20.f, 3.f);
        g.setColour (Col::bg); g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (cc); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.f, 1.f, conf)), 1.5f);
    }
}

void Dashboard::plotPitch (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (Col::card); g.fillRoundedRectangle (r.toFloat(), 8.f);
    g.setColour (Col::dim); g.setFont (10.5f); g.drawText ("PITCH TRACK", r.reduced (10, 6).removeFromTop (12), juce::Justification::left);
    const auto& res = proc.result();
    auto area = r.reduced (10, 22).toFloat();
    if (! res.valid || res.pitchTrack.empty() || res.hiMidi <= 0) { g.setColour (Col::dim); g.drawText ("analyze to see the sung/spoken pitch", r, juce::Justification::centred); return; }
    const float lo = std::floor (res.loMidi) - 3.f, hi = std::ceil (res.hiMidi) + 3.f;
    for (int m = (int) std::ceil (lo); m <= (int) hi; ++m)
        if (((m % 12) + 12) % 12 == 0)
        {
            float y = area.getBottom() - (m - lo) / (hi - lo) * area.getHeight();
            g.setColour (Col::dim.withAlpha (0.25f)); g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
            g.setColour (Col::dim); g.setFont (9.f); g.drawText (vm::midiName ((float) m), (int) area.getX(), (int) y - 10, 30, 10, juce::Justification::left);
        }
    const size_t n = res.pitchTrack.size(); const size_t stride = std::max<size_t> (1, n / (size_t) std::max (1.f, area.getWidth()));
    g.setColour (Col::accent2);
    for (size_t i = 0; i < n; i += stride)
    {
        float m = res.pitchTrack[i]; if (m <= 0) continue;
        float x = area.getX() + (float) i / (float) n * area.getWidth(), y = area.getBottom() - (m - lo) / (hi - lo) * area.getHeight();
        g.fillRect (x, y - 1.5f, 2.5f, 3.f);
    }
}

void Dashboard::plotSpectrum (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (Col::card); g.fillRoundedRectangle (r.toFloat(), 8.f);
    g.setColour (Col::dim); g.setFont (10.5f); g.drawText ("AVERAGE SPECTRUM  (40 Hz - 16 kHz)", r.reduced (10, 6).removeFromTop (12), juce::Justification::left);
    const auto& res = proc.result();
    auto area = r.reduced (10, 22).toFloat();
    if (! res.valid || res.spectrumDb.empty()) { g.setColour (Col::dim); g.drawText ("analyze to see the vocal spectrum", r, juce::Justification::centred); return; }
    const float mx = *std::max_element (res.spectrumDb.begin(), res.spectrumDb.end()), mn = mx - 60.f;
    juce::Path p; const int nb = (int) res.spectrumDb.size();
    for (int i = 0; i < nb; ++i)
    {
        float x = area.getX() + (float) i / (float) (nb - 1) * area.getWidth();
        float y = area.getBottom() - juce::jlimit (0.f, 1.f, (res.spectrumDb[(size_t) i] - mn) / 60.f) * area.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    juce::Path fill (p); fill.lineTo (area.getRight(), area.getBottom()); fill.lineTo (area.getX(), area.getBottom()); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Col::accent.withAlpha (0.45f), 0.f, area.getY(), Col::accent.withAlpha (0.02f), 0.f, area.getBottom(), false));
    g.fillPath (fill);
    g.setColour (Col::accent); g.strokePath (p, juce::PathStrokeType (1.5f));
}

void Dashboard::paint (juce::Graphics& g)
{
    const auto& r = proc.result();
    const bool ok = r.valid;
    const int w = (getWidth() - 4 * 6) / 5, h = 56;
    auto cell = [&] (int row, int col) { return juce::Rectangle<int> (col * (w + 6), row * (h + 4), w, h); };
    auto f1 = [] (float v, int d = 1) { return juce::String (v, d); };

    card (g, cell (0, 0), "KEY / SCALE", ok && r.keyValid ? juce::String (vm::noteNames()[r.keyRoot]) + (r.minor ? " minor" : " major") : "--", ok && r.keyValid ? r.keyConf : -1.f);
    card (g, cell (0, 1), "TEMPO (est.)", ok && r.bpm > 0 ? f1 (r.bpm, 1) + " BPM" : "--", ok && r.bpm > 0 ? r.bpmConf : -1.f);
    card (g, cell (0, 2), "VOCAL RANGE", ok && r.loMidi > 0 ? juce::String (vm::midiName (r.loMidi)) + " - " + juce::String (vm::midiName (r.hiMidi)) : "--", -1.f);
    card (g, cell (0, 3), "VOICE TYPE", ok ? juce::String (vm::voiceNames()[r.voiceClass]) : "--", -1.f);
    card (g, cell (0, 4), "LEVEL (RMS)", ok ? f1 (r.activeRmsDb) + " dB" : "--", -1.f);
    card (g, cell (1, 0), "NOISE FLOOR", ok ? (r.noiseFloorDb <= -99.f ? juce::String ("n/a") : f1 (r.noiseFloorDb) + " dB") : "--", -1.f);
    card (g, cell (1, 1), "DYNAMIC RANGE", ok ? f1 (r.dynamicRangeDb) + " dB" : "--", -1.f);
    card (g, cell (1, 2), "SIBILANCE", ok ? juce::String (r.relSib + 12.f >= 0 ? "+" : "") + f1 (r.relSib + 12.f) + " dB" : "--", -1.f);
    card (g, cell (1, 3), "PITCH CLARITY", ok ? f1 (r.clarity * 100.f, 0) + " %" : "--", ok ? r.clarity : -1.f);
    card (g, cell (1, 4), "DELIVERY PACE", ok ? f1 (r.noteRate) + " /s" : "--", -1.f);

    const int py = 154, pw = (getWidth() - 6) / 2;
    plotPitch (g, { 0, py, pw, getHeight() - py });
    plotSpectrum (g, { pw + 6, py, getWidth() - pw - 6, getHeight() - py });
}

//==============================================================================
VocalMixEditor::VocalMixEditor (VocalMixProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), dash (p)
{
    setLookAndFeel (&look);

    title.setText ("VOCALMIX AI", juce::dontSendNotification);
    title.setFont (title.getFont().withHeight (24.f).boldened());
    statusLabel.setFont (statusLabel.getFont().withHeight (13.f));
    statusLabel.setColour (juce::Label::textColourId, Col::dim);
    pitchLabel.setFont (pitchLabel.getFont().withHeight (12.5f)); pitchLabel.setColour (juce::Label::textColourId, Col::accent2);
    inLbl.setText ("IN", juce::dontSendNotification); outLbl.setText ("OUT", juce::dontSendNotification);
    manualLbl.setText ("MANUAL KEY / BPM", juce::dontSendNotification); manualLbl.setColour (juce::Label::textColourId, Col::dim);
    for (auto* l : { &inLbl, &outLbl, &manualLbl }) l->setFont (l->getFont().withHeight (10.5f));
    for (juce::Component* c : { (juce::Component*) &title, (juce::Component*) &statusLabel, (juce::Component*) &pitchLabel, (juce::Component*) &inLbl,
                                (juce::Component*) &outLbl, (juce::Component*) &manualLbl, (juce::Component*) &analyzeBtn, (juce::Component*) &autoBtn,
                                (juce::Component*) &undoBtn, (juce::Component*) &resetBtn, (juce::Component*) &bypassBtn, (juce::Component*) &inMeter,
                                (juce::Component*) &outMeter, (juce::Component*) &dash, (juce::Component*) &keyBox, (juce::Component*) &scaleBox, (juce::Component*) &bpmSlider })
        addAndMakeVisible (c);

    analyzeBtn.setColour (juce::TextButton::buttonColourId, Col::accent2.darker (0.5f));
    autoBtn.setColour (juce::TextButton::buttonColourId, Col::accent);
    analyzeBtn.onClick = [this] { if (processor.isAnalyzing()) processor.stopAnalysis(); else processor.startAnalysis(); };
    autoBtn.onClick = [this] { processor.runAutoMix(); };
    undoBtn.onClick = [this] { processor.undo(); };
    resetBtn.onClick = [this] { processor.resetAll(); };
    bypassBtn.setClickingTogglesState (true); bypassBtn.setColour (juce::TextButton::buttonOnColourId, Col::warn);
    bypassAtt = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, vm::paramDef (vm::bypass).id, bypassBtn);

    keyBox.addItemList (vm::keyNameArray(), 1); scaleBox.addItemList (vm::scaleNameArray(), 1);
    keyAtt = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, vm::paramDef (vm::pitchKey).id, keyBox);
    scaleAtt = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, vm::paramDef (vm::pitchScale).id, scaleBox);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal); bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    bpmAtt = std::make_unique<APVTS::SliderAttachment> (processor.apvts, vm::paramDef (vm::bpmManual).id, bpmSlider);

    // knobs
    addKnob (vm::inGain, " dB", 1); addKnob (vm::hpFreq, " Hz", 0); addKnob (vm::gateThr, " dB", 0);
    addKnob (vm::eqLow, " dB", 1); addKnob (vm::eqMud, " dB", 1); addKnob (vm::eqPres, " dB", 1); addKnob (vm::eqAir, " dB", 1);
    addKnob (vm::compThr, " dB", 1); addKnob (vm::compRatio, ":1", 1); addKnob (vm::compAtk, " ms", 1); addKnob (vm::compRel, " ms", 0); addKnob (vm::compMake, " dB", 1);
    addKnob (vm::deEss, "", 2); addKnob (vm::deEssFreq, " Hz", 0); addKnob (vm::levelAmt, "", 2);
    addKnob (vm::satDrive, "", 2); addKnob (vm::satMix, "", 2);
    addKnob (vm::dlyMix, "", 2); addKnob (vm::dlyTime, " ms", 0); addKnob (vm::dlyFb, "", 2);
    addKnob (vm::revMix, "", 2); addKnob (vm::revSize, "", 2); addKnob (vm::revDamp, "", 2);
    addKnob (vm::pitchAmt, "", 2); addKnob (vm::pitchSpeed, "", 2);
    addKnob (vm::width, "", 2); addKnob (vm::limCeil, " dB", 1); addKnob (vm::outGain, " dB", 1);

    for (auto* b : { &pitchOnBtn, &autoAnaBtn, &autoApplyBtn }) { styleToggle (*b); addAndMakeVisible (*b); }
    pitchOnAtt = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, vm::paramDef (vm::pitchOn).id, pitchOnBtn);
    autoAnaAtt = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, vm::paramDef (vm::autoAnalyze).id, autoAnaBtn);
    autoApplyAtt = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, vm::paramDef (vm::autoApply).id, autoApplyBtn);

    auto K = [this] (int pid) { return (juce::Component*) knobById[pid]; };
    rows = {
        { { "INPUT", { K (vm::inGain), K (vm::hpFreq), K (vm::gateThr) } }, { "EQ", { K (vm::eqLow), K (vm::eqMud), K (vm::eqPres), K (vm::eqAir) } } },
        { { "DYNAMICS", { K (vm::compThr), K (vm::compRatio), K (vm::compAtk), K (vm::compRel), K (vm::compMake) } }, { "DE-ESS / LEVELER", { K (vm::deEss), K (vm::deEssFreq), K (vm::levelAmt) } } },
        { { "SATURATION", { K (vm::satDrive), K (vm::satMix) } }, { "DELAY", { K (vm::dlyMix), K (vm::dlyTime), K (vm::dlyFb) } }, { "REVERB", { K (vm::revMix), K (vm::revSize), K (vm::revDamp) } } },
        { { "PITCH CORRECTION", { &pitchOnBtn, K (vm::pitchAmt), K (vm::pitchSpeed) } }, { "OUTPUT", { K (vm::width), K (vm::limCeil), K (vm::outGain) } }, { "AUTOMATION", { &autoAnaBtn, &autoApplyBtn } } }
    };

    // preset browser
    search.setTextToShowWhenEmpty ("search presets...", Col::dim);
    search.onTextChange = [this] { refreshPresetList(); };
    pGenre.addItem ("All genres", 1); for (int i = 0; i < vm::kNumGenres; ++i) pGenre.addItem (vm::genreNames()[i], 2 + i);
    pGenre.addItem ("My presets", 2 + vm::kNumGenres); pGenre.setSelectedId (1, juce::dontSendNotification);
    pMood.addItem ("All moods", 1); for (int i = 0; i < vm::kNumMoods; ++i) pMood.addItem (vm::moodNames()[i], 2 + i);
    pMood.setSelectedId (1, juce::dontSendNotification);
    pGenre.onChange = [this] { refreshPresetList(); }; pMood.onChange = [this] { refreshPresetList(); };
    styleToggle (recBtn); recBtn.onClick = [this] { refreshPresetList(); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    list.setModel (this); list.setRowHeight (36);
    countLabel.setFont (countLabel.getFont().withHeight (11.f)); countLabel.setColour (juce::Label::textColourId, Col::dim);
    for (juce::Component* c : { (juce::Component*) &search, (juce::Component*) &pGenre, (juce::Component*) &pMood, (juce::Component*) &recBtn,
                                (juce::Component*) &saveBtn, (juce::Component*) &list, (juce::Component*) &countLabel })
        addAndMakeVisible (c);

    // advisor
    advisorText.setMultiLine (true, true); advisorText.setReadOnly (true); advisorText.setScrollbarsShown (true);
    advisorText.setFont (advisorText.getFont().withHeight (13.f));
    advGenre.addItem ("Genre: Auto", 1); for (int i = 0; i < vm::kNumGenres; ++i) advGenre.addItem (vm::genreNames()[i], 2 + i);
    advMood.addItem ("Mood: Auto", 1); for (int i = 0; i < vm::kNumMoods; ++i) advMood.addItem (vm::moodNames()[i], 2 + i);
    advGenre.setSelectedId (1, juce::dontSendNotification); advMood.setSelectedId (1, juce::dontSendNotification);
    auto advChanged = [this] { processor.setAdvisorOverrides (advGenre.getSelectedId() - 2, advMood.getSelectedId() - 2); };
    advGenre.onChange = advChanged; advMood.onChange = advChanged;
    regenBtn.onClick = [this] { processor.regenerateAdvisor (true); };
    midiBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export beat plan as MIDI", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("BeatPlan.mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) processor.exportMidi (f.withFileExtension ("mid")); });
    };
    for (juce::Component* c : { (juce::Component*) &advisorText, (juce::Component*) &advGenre, (juce::Component*) &advMood, (juce::Component*) &regenBtn, (juce::Component*) &midiBtn })
        addAndMakeVisible (c);

    refreshPresetList();
    setSize (1240, 820);
    startTimerHz (30);
}

VocalMixEditor::~VocalMixEditor() { stopTimer(); setLookAndFeel (nullptr); }

void VocalMixEditor::styleToggle (juce::TextButton& b) { b.setClickingTogglesState (true); b.setColour (juce::TextButton::buttonOnColourId, Col::accent); }

void VocalMixEditor::addKnob (int pid, const juce::String& suffix, int decimals)
{
    auto k = std::make_unique<Knob> (processor.apvts, pid, vm::paramDef (pid).name, suffix, decimals);
    addAndMakeVisible (*k); knobById[pid] = k.get(); knobs.push_back (std::move (k));
}

void VocalMixEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::bg);
    g.setColour (Col::panel); g.fillRect (0, 0, getWidth(), 66);
    g.setColour (Col::accent); g.fillRect (0, 65, getWidth(), 1);
    auto panel = [&] (juce::Rectangle<int> r) { g.setColour (Col::panel); g.fillRoundedRectangle (r.toFloat(), 10.f); };
    panel ({ 12, 76, 250, 732 }); panel ({ 900, 76, 328, 732 });
    g.setColour (Col::dim); g.setFont (g.getCurrentFont().withHeight (11.f).boldened());
    g.drawText ("PRESET BROWSER", 24, 82, 200, 14, juce::Justification::left);
    g.drawText ("AI BEAT ADVISOR", 912, 82, 200, 14, juce::Justification::left);
    for (auto& gr : groupRects)
    {
        panel (gr.second);
        g.setColour (Col::dim); g.setFont (g.getCurrentFont().withHeight (10.5f).boldened());
        g.drawText (gr.first, gr.second.getX() + 8, gr.second.getY() + 3, gr.second.getWidth() - 12, 12, juce::Justification::left);
    }
}

void VocalMixEditor::resized()
{
    title.setBounds (16, 6, 260, 30); statusLabel.setBounds (16, 36, 600, 24);
    analyzeBtn.setBounds (626, 16, 110, 34); autoBtn.setBounds (742, 16, 100, 34);
    undoBtn.setBounds (850, 16, 56, 34); resetBtn.setBounds (910, 16, 56, 34); bypassBtn.setBounds (972, 16, 90, 34);
    inLbl.setBounds (1070, 12, 28, 14); inMeter.setBounds (1100, 15, 124, 8);
    outLbl.setBounds (1070, 34, 28, 14); outMeter.setBounds (1100, 37, 124, 8);

    // left: presets
    search.setBounds (22, 102, 230, 26);
    pGenre.setBounds (22, 134, 112, 26); pMood.setBounds (140, 134, 112, 26);
    recBtn.setBounds (22, 166, 230, 26);
    list.setBounds (22, 198, 230, 530); countLabel.setBounds (22, 732, 230, 16);
    saveBtn.setBounds (22, 760, 230, 30);

    // right: advisor
    advGenre.setBounds (910, 102, 150, 26); advMood.setBounds (1066, 102, 150, 26);
    advisorText.setBounds (910, 136, 308, 590);
    regenBtn.setBounds (910, 734, 150, 30); midiBtn.setBounds (1066, 734, 150, 30);

    // center: dashboard + manual strip + chain
    dash.setBounds (274, 76, 614, 266);
    manualLbl.setBounds (274, 202, 96, 20);
    keyBox.setBounds (372, 198, 60, 26); scaleBox.setBounds (436, 198, 92, 26);
    bpmSlider.setBounds (536, 198, 190, 26); pitchLabel.setBounds (732, 198, 156, 26);
    // NOTE: strip sits inside the dashboard's reserved gap (y 122..150 of the dashboard)

    groupRects.clear();
    const int cellW = 72, rowH = 114, x0 = 274, y0 = 350;
    for (size_t ri = 0; ri < rows.size(); ++ri)
    {
        int x = x0; const int ry = y0 + (int) ri * rowH;
        for (auto& gd : rows[ri])
        {
            const int w = (int) gd.cells.size() * cellW + 8;
            groupRects.push_back ({ gd.title, juce::Rectangle<int> (x, ry, w, rowH - 4) });
            int cx = x + 4;
            for (auto* c : gd.cells)
            {
                juce::Rectangle<int> cell (cx, ry + 18, cellW, 90);
                if (dynamic_cast<juce::Button*> (c) != nullptr) c->setBounds (cell.withTrimmedTop (28).withHeight (28).reduced (2, 0));
                else c->setBounds (cell);
                cx += cellW;
            }
            x += w + 6;
        }
    }
}

void VocalMixEditor::timerCallback()
{
    inMeter.setLevel (processor.inPeak.exchange (0.f)); outMeter.setLevel (processor.outPeak.exchange (0.f));
    statusLabel.setText (processor.status, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, processor.statusIsError ? Col::warn : Col::dim);
    analyzeBtn.setButtonText (processor.isAnalyzing() ? "STOP & FINISH" : "ANALYZE");

    const float hz = processor.pitchHz.load(), tg = processor.targetHz.load();
    juce::String s = "Live pitch: --";
    if (hz > 0.f)
    {
        s = "Live pitch: " + juce::String (vm::midiName (69.f + 12.f * std::log2 (hz / 440.f)));
        if (tg > 0.f) s += "  ->  " + juce::String (vm::midiName (69.f + 12.f * std::log2 (tg / 440.f)));
    }
    pitchLabel.setText (s, juce::dontSendNotification);

    if (processor.resultVersion() != lastRes) { lastRes = processor.resultVersion(); dash.repaint(); refreshPresetList(); }
    if (processor.advisorVersion() != lastAdv)
    {
        lastAdv = processor.advisorVersion();
        advisorText.setText (juce::String (processor.advice().text), false); advisorText.moveCaretToTop (false);
    }
}

//==============================================================================
void VocalMixEditor::refreshPresetList()
{
    const auto& ps = processor.presets; const auto& r = processor.result();
    const auto q = search.getText().trim().toLowerCase();
    const int g = pGenre.getSelectedId() - 2, m = pMood.getSelectedId() - 2;
    auto matches = [&] (int i)
    {
        const auto& p = ps[(size_t) i];
        if (g == vm::kNumGenres && ! p.user) return false;
        if (g >= 0 && g < vm::kNumGenres && p.genre != vm::genreNames()[g]) return false;
        if (m >= 0 && p.mood != vm::moodNames()[m]) return false;
        if (q.isNotEmpty() && ! juce::String (p.name).toLowerCase().contains (q)) return false;
        return true;
    };
    filtered.clear();
    if (recBtn.getToggleState())
    {
        if (r.valid) for (int i : vm::recommendPresets (ps, r.voiceClass, r.condition, processor.recommendedGenreIndex(), m, 60)) if (matches (i)) filtered.push_back (i);
    }
    else for (int i = 0; i < (int) ps.size(); ++i) if (matches (i)) filtered.push_back (i);
    list.deselectAllRows(); list.updateContent(); list.repaint();
    countLabel.setText (juce::String ((int) filtered.size()) + " of " + juce::String ((int) ps.size()) + " presets"
                        + (recBtn.getToggleState() && ! r.valid ? "  (analyze first)" : ""), juce::dontSendNotification);
}

void VocalMixEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& p = processor.presets[(size_t) filtered[(size_t) row]];
    if (selected) { g.setColour (Col::accent.withAlpha (0.55f)); g.fillRect (0, 0, w, h); }
    g.setColour (Col::text); g.setFont (12.5f);
    if (p.user) { g.drawText (juce::String (p.name), 8, 0, w - 12, h, juce::Justification::centredLeft); return; }
    g.drawText (juce::String (p.genre) + "  |  " + juce::String (p.mood), 8, 3, w - 12, 16, juce::Justification::centredLeft);
    g.setColour (Col::dim); g.setFont (11.f);
    g.drawText (juce::String (p.voice) + "  |  " + juce::String (p.cond), 8, 18, w - 12, 15, juce::Justification::centredLeft);
}

void VocalMixEditor::selectedRowsChanged (int lastRow)
{
    if (lastRow >= 0 && lastRow < (int) filtered.size()) processor.applyPreset (filtered[(size_t) lastRow]);
}

void VocalMixEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Vocal Preset");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VocalMixEditor> safe (this);
    VocalMixProcessor* proc = &processor;
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, proc, w] (int res)
    {
        if (res == 1) { proc->saveUserPreset (w->getTextEditorContents ("name")); if (safe != nullptr) safe->refreshPresetList(); }
    }), true);
}
