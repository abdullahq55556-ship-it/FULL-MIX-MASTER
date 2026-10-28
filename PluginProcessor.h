#pragma once
#include "Common.h"
#include "Params.h"
#include "Dsp.h"
#include "Analyzer.h"
#include "AutoMixer.h"
#include "Presets.h"
#include "BeatAdvisor.h"

class VocalMixProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VocalMixProcessor();
    ~VocalMixProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VocalMix AI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-thread API used by the editor
    void startAnalysis();
    void stopAnalysis();
    void runAutoMix();
    void undo();
    void resetAll();
    void applyPreset (int index);
    bool saveUserPreset (const juce::String& name);
    void regenerateAdvisor (bool newSeed);
    void setAdvisorOverrides (int genreIdx, int moodIdx) { advGenre = genreIdx; advMood = moodIdx; regenerateAdvisor (true); }
    bool exportMidi (const juce::File& file);
    int recommendedGenreIndex() const;

    const vm::AnalysisResult& result() const { return lastResult; }
    const vm::Advice& advice() const { return currentAdvice; }
    int advisorVersion() const { return advVersion; }
    int resultVersion() const { return resVersion; }
    bool isAnalyzing() const { return analyzing.load(); }
    double analysisSeconds() const { return analyzer.seconds(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::String status { "Ready. Press ANALYZE and play your vocal." };
    bool statusIsError = false;
    std::vector<vm::Preset> presets;

    std::atomic<float> inPeak { 0.f }, outPeak { 0.f }, compGr { 0.f }, pitchHz { 0.f }, targetHz { 0.f };

private:
    void timerCallback() override;
    void setParamPlain (int pid, float plain);
    float getParam (int pid) const { return raw[(size_t) pid]->load(); }
    void pushUndo();
    void loadUserPresets();
    juce::File presetDir() const;
    void drainFifo();
    void setStatus (const juce::String& s, bool err = false) { status = s; statusIsError = err; }

    std::vector<std::atomic<float>*> raw;
    vm::VocalChain chain;
    vm::Analyzer analyzer;
    vm::AnalysisResult lastResult;
    vm::Advice currentAdvice;
    int advGenre = -1, advMood = -1, advVersion = 0, resVersion = 0; unsigned advSeed = 1;
    std::atomic<bool> analyzing { false }, hasResult { false }, requestStart { false };
    std::vector<juce::ValueTree> undoStack;

    juce::AbstractFifo fifo { 1 << 17 };
    std::vector<float> fifoData, tapBuf, drainBuf, wetL, wetR;
    vm::FixedDelay dryDelay;
    juce::Reverb reverb;
    double sr = 44100; float bypassMix = 0.f;
    juce::AudioBuffer<float> dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalMixProcessor)
};
