#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Barra de medidor simples (horizontal).
class MeterBar : public juce::Component
{
public:
    MeterBar (juce::String label, float minV, float maxV, bool centered, juce::String unit)
        : name (std::move (label)), lo (minV), hi (maxV), fromCenter (centered), units (std::move (unit)) {}

    void setValue (float v, juce::Colour c) { value = v; colour = c; repaint(); }
    void setMarker (float v) { marker = v; hasMarker = true; }

    void paint (juce::Graphics& g) override;

private:
    juce::String name;
    float lo, hi, value = -120.0f, marker = 0.0f;
    bool fromCenter, hasMarker = false;
    juce::String units;
    juce::Colour colour { juce::Colours::limegreen };
};

class NiveladorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NiveladorEditor (NiveladorProcessor&);
    ~NiveladorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setTarget (float lufs);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& text);

    NiveladorProcessor& proc;
    juce::LookAndFeel_V4 laf;

    Knob target, trim, maxBoost, maxCut, gate, ceiling;
    juce::ComboBox mode;
    std::unique_ptr<ComboAttachment> modeAttachment;
    juce::ToggleButton levelerOn { "Nivelador" }, limiterOn { "Limitador" };
    std::unique_ptr<ButtonAttachment> levelerAttachment, limiterAttachment;
    juce::TextButton presetWeb { "Internet -14" }, presetVmix { "vMix atual -18" }, presetTv { "TV -23" };

    MeterBar meterIn  { "Entrada", -50.0f, 0.0f, false, "LUFS" };
    MeterBar meterOut { juce::String::fromUTF8 ("Saída"),   -50.0f, 0.0f, false, "LUFS" };
    MeterBar meterGain { "Ganho",  -24.0f, 24.0f, true, "dB" };
    MeterBar meterLim  { "Limitador", -12.0f, 0.0f, true, "dB" };
    juce::Label gateLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NiveladorEditor)
};
