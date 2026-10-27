#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;

NiveladorProcessor::NiveladorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Saida",   juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NIVELADOR", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NiveladorProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto db = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto lufs = [] (float v, int) { return juce::String (v, 1) + " LUFS"; };
    auto dbtp = [] (float v, int) { return juce::String (v, 1) + " dBTP"; };

    p.push_back (std::make_unique<APF> (juce::ParameterID { ids::target, 1 }, "Alvo",
        juce::NormalisableRange<float> (-30.0f, -8.0f, 0.5f), -14.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (lufs)));

    p.push_back (std::make_unique<APC> (juce::ParameterID { ids::mode, 1 }, "Modo",
        juce::StringArray { "Voz (microfones)", "Programa (master)" }, 0));

    p.push_back (std::make_unique<APF> (juce::ParameterID { ids::trim, 1 }, "Ganho de entrada",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    p.push_back (std::make_unique<APF> (juce::ParameterID { ids::maxBoost, 1 }, "Subida maxima",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.5f), 12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    p.push_back (std::make_unique<APF> (juce::ParameterID { ids::maxCut, 1 }, "Corte maximo",
        juce::NormalisableRange<float> (0.0f, 30.0f, 0.5f), 20.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    p.push_back (std::make_unique<APF> (juce::ParameterID { ids::gate, 1 }, "Gate (silencio)",
        juce::NormalisableRange<float> (-70.0f, -25.0f, 0.5f), -45.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (lufs)));

    p.push_back (std::make_unique<APF> (juce::ParameterID { ids::ceiling, 1 }, "Teto",
        juce::NormalisableRange<float> (-6.0f, 0.0f, 0.1f), -1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbtp)));

    p.push_back (std::make_unique<APB> (juce::ParameterID { ids::leveler, 1 }, "Nivelador ligado", true));
    p.push_back (std::make_unique<APB> (juce::ParameterID { ids::limiter, 1 }, "Limitador ligado", true));

    return { p.begin(), p.end() };
}

nivelador::Settings NiveladorProcessor::readSettings() const
{
    nivelador::Settings s;
    s.inputTrimDb = apvts.getRawParameterValue (ids::trim)->load();
    s.targetLufs  = apvts.getRawParameterValue (ids::target)->load();
    s.mode        = (int) apvts.getRawParameterValue (ids::mode)->load();
    s.maxBoostDb  = apvts.getRawParameterValue (ids::maxBoost)->load();
    s.maxCutDb    = apvts.getRawParameterValue (ids::maxCut)->load();
    s.gateLufs    = apvts.getRawParameterValue (ids::gate)->load();
    s.ceilingDbtp = apvts.getRawParameterValue (ids::ceiling)->load();
    s.levelerOn   = apvts.getRawParameterValue (ids::leveler)->load() > 0.5f;
    s.limiterOn   = apvts.getRawParameterValue (ids::limiter)->load() > 0.5f;
    return s;
}

void NiveladorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    leveler.setSettings (readSettings());
    leveler.prepare (sampleRate, getTotalNumOutputChannels(), samplesPerBlock);
    setLatencySamples (leveler.getLatency());
}

bool NiveladorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void NiveladorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    leveler.setSettings (readSettings());
    leveler.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

void NiveladorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NiveladorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* NiveladorProcessor::createEditor()
{
    return new NiveladorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NiveladorProcessor();
}
