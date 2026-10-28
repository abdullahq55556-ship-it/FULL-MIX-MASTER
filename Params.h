#pragma once
#include "Common.h"
#include "ParamIds.h"

namespace vm
{
inline juce::StringArray keyNameArray()   { return juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; }
inline juce::StringArray scaleNameArray() { return juce::StringArray { "Chromatic","Major","Minor" }; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < PidCount; ++i)
    {
        const auto& d = paramDef (i);
        const juce::ParameterID pid { d.id, 1 };
        if (d.kind == KFloat)
        {
            auto attr = juce::AudioParameterFloatAttributes().withLabel (d.unit);
            if (i == bpmManual)
                attr = attr.withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? juce::String ("Auto") : juce::String ((int) v) + " BPM"; });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name,
                        juce::NormalisableRange<float> (d.mn, d.mx, d.step, d.skew), d.def, attr));
        }
        else if (d.kind == KChoice)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name,
                        i == pitchKey ? keyNameArray() : scaleNameArray(), (int) d.def));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
        }
    }
    return layout;
}
}
