#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LevelerDSP.h"

namespace ids
{
    inline constexpr const char* trim      = "trim";
    inline constexpr const char* target    = "target";
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* maxBoost  = "maxBoost";
    inline constexpr const char* maxCut    = "maxCut";
    inline constexpr const char* gate      = "gate";
    inline constexpr const char* ceiling   = "ceiling";
    inline constexpr const char* leveler   = "leveler";
    inline constexpr const char* limiter   = "limiter";
}

class NiveladorProcessor : public juce::AudioProcessor
{
public:
    NiveladorProcessor();
    ~NiveladorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nivelador vMix"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    nivelador::Leveler leveler;

private:
    nivelador::Settings readSettings() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NiveladorProcessor)
};
