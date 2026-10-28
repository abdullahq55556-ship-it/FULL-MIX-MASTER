#pragma once
#include "PluginProcessor.h"

namespace Col
{
    const juce::Colour bg { 0xff0d1016 }, panel { 0xff161b26 }, card { 0xff1e2433 }, accent { 0xff8b6cff }, accent2 { 0xff2dd4bf },
                       text { 0xffe8ebf3 }, dim { 0xff8e97ad }, warn { 0xffff6b6b }, good { 0xff4ade80 }, amber { 0xfffbbf24 };
}

//==============================================================================
class VmLook : public juce::LookAndFeel_V4
{
public:
    VmLook()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Col::bg);
        setColour (juce::TextButton::buttonColourId, Col::card);
        setColour (juce::TextButton::buttonOnColourId, Col::accent);
        setColour (juce::TextButton::textColourOffId, Col::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, Col::card);
        setColour (juce::ComboBox::textColourId, Col::text);
        setColour (juce::ComboBox::outlineColourId, Col::card.brighter (0.2f));
        setColour (juce::ComboBox::arrowColourId, Col::dim);
        setColour (juce::PopupMenu::backgroundColourId, Col::card);
        setColour (juce::PopupMenu::textColourId, Col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::accent);
        setColour (juce::TextEditor::backgroundColourId, Col::card);
        setColour (juce::TextEditor::textColourId, Col::text);
        setColour (juce::TextEditor::outlineColourId, Col::card.brighter (0.2f));
        setColour (juce::TextEditor::focusedOutlineColourId, Col::accent);
        setColour (juce::ListBox::backgroundColourId, Col::card);
        setColour (juce::ListBox::outlineColourId, Col::card.brighter (0.2f));
        setColour (juce::Label::textColourId, Col::text);
        setColour (juce::Slider::textBoxTextColourId, Col::dim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, Col::accent);
        setColour (juce::Slider::trackColourId, Col::accent);
        setColour (juce::Slider::backgroundColourId, Col::card);
        setColour (juce::ScrollBar::thumbColourId, Col::accent.withAlpha (0.6f));
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int h) override { return LookAndFeel_V4::getTextButtonFont (b, h).withHeight (12.5f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float d = std::min (b.getWidth(), b.getHeight()), r = d * 0.5f, lw = 4.f;
        const auto c = b.getCentre();
        juce::Path track; track.addCentredArc (c.x, c.y, r - lw, r - lw, 0.f, a0, a1, true);
        g.setColour (Col::card.brighter (0.18f));
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ang = a0 + pos * (a1 - a0);
        juce::Path val; val.addCentredArc (c.x, c.y, r - lw, r - lw, 0.f, a0, ang, true);
        g.setGradientFill (juce::ColourGradient (Col::accent2, b.getX(), b.getBottom(), Col::accent, b.getRight(), b.getY(), false));
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float br = r - lw - 5.f;
        g.setColour (Col::panel.brighter (0.12f)); g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
        juce::Path p; p.addRoundedRectangle (-1.5f, -br, 3.f, br * 0.55f, 1.f);
        g.setColour (Col::text); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, int pid, const juce::String& name, const juce::String& suffix, int decimals)
    {
        addAndMakeVisible (slider); addAndMakeVisible (label);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (label.getFont().withHeight (11.5f));
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, vm::paramDef (pid).id, slider);
        slider.setDoubleClickReturnValue (true, vm::paramDef (pid).def);
        slider.textFromValueFunction = [decimals, suffix] (double v) { return juce::String (v, decimals) + suffix; };
        slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
        slider.updateText();
    }
    void resized() override { auto b = getLocalBounds(); label.setBounds (b.removeFromTop (14)); slider.setBounds (b); }
    juce::Slider slider; juce::Label label;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

//==============================================================================
class Meter : public juce::Component
{
public:
    void setLevel (float peak)
    {
        const float db = peak > 1.0e-4f ? 20.f * std::log10 (peak) : -80.f;
        const float t = juce::jmap (juce::jlimit (-60.f, 3.f, db), -60.f, 3.f, 0.f, 1.f);
        level = t > level ? t : level * 0.9f + t * 0.1f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (Col::card); g.fillRoundedRectangle (b, 3.f);
        auto f = b.withWidth (b.getWidth() * level);
        g.setColour (level > 0.95f ? Col::warn : level > 0.85f ? Col::amber : Col::accent2);
        g.fillRoundedRectangle (f, 3.f);
    }
private:
    float level = 0.f;
};

//==============================================================================
class Dashboard : public juce::Component
{
public:
    explicit Dashboard (VocalMixProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override;
private:
    void card (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& value, float conf);
    void plotPitch (juce::Graphics& g, juce::Rectangle<int> r);
    void plotSpectrum (juce::Graphics& g, juce::Rectangle<int> r);
    VocalMixProcessor& proc;
};

//==============================================================================
class VocalMixEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ListBoxModel
{
public:
    explicit VocalMixEditor (VocalMixProcessor&);
    ~VocalMixEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRow) override;

    void refreshPresetList();
    void savePresetDialog();
    void addKnob (int pid, const juce::String& suffix, int decimals);
    void styleToggle (juce::TextButton& b);

    VocalMixProcessor& processor;
    VmLook look;

    juce::Label title, statusLabel, pitchLabel, inLbl, outLbl, manualLbl;
    juce::TextButton analyzeBtn { "ANALYZE" }, autoBtn { "AUTO MIX" }, undoBtn { "Undo" }, resetBtn { "Reset" }, bypassBtn { "A/B Bypass" };
    Meter inMeter, outMeter;
    Dashboard dash;
    juce::ComboBox keyBox, scaleBox; juce::Slider bpmSlider;
    std::unique_ptr<APVTS::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<APVTS::SliderAttachment> bpmAtt;
    std::unique_ptr<APVTS::ButtonAttachment> bypassAtt, pitchOnAtt, autoAnaAtt, autoApplyAtt;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::map<int, Knob*> knobById;
    juce::TextButton pitchOnBtn { "Pitch On" }, autoAnaBtn { "Auto Analyze" }, autoApplyBtn { "Auto Mix" };

    // preset browser
    juce::TextEditor search; juce::ComboBox pGenre, pMood; juce::TextButton recBtn { "Recommended for my vocal" }, saveBtn { "Save current as preset..." };
    juce::ListBox list; juce::Label countLabel;
    std::vector<int> filtered;

    // advisor
    juce::TextEditor advisorText; juce::ComboBox advGenre, advMood; juce::TextButton regenBtn { "Regenerate" }, midiBtn { "Export MIDI" };
    std::unique_ptr<juce::FileChooser> chooser;

    struct GroupDef { juce::String title; std::vector<juce::Component*> cells; };
    std::vector<std::vector<GroupDef>> rows;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> groupRects;
    int lastRes = -1, lastAdv = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalMixEditor)
};
