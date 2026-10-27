#include "PluginEditor.h"

// Texto com acentos (o arquivo é UTF-8)
static juce::String u8 (const char* t) { return juce::String::fromUTF8 (t); }

namespace
{
    const juce::Colour bg       { 0xff15181d };
    const juce::Colour panel    { 0xff20252c };
    const juce::Colour textCol  { 0xffe6e9ee };
    const juce::Colour dimText  { 0xff8b939e };
    const juce::Colour accent   { 0xff2f9bff };
    const juce::Colour good     { 0xff3ccf6e };
    const juce::Colour warn     { 0xfff2b230 };
    const juce::Colour hot      { 0xffef4b4b };
}

//==============================================================================
void MeterBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromLeft (78.0f);
    auto valueArea = r.removeFromRight (86.0f);

    g.setColour (textCol);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (name, labelArea, juce::Justification::centredLeft);

    auto bar = r.reduced (4.0f, 6.0f);
    g.setColour (juce::Colour (0xff0d0f12));
    g.fillRoundedRectangle (bar, 3.0f);

    auto toX = [&] (float v) { return bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo)); };

    const bool valid = value > -119.0f;
    if (valid)
    {
        float x0 = fromCenter ? toX (hi > 0.0f ? 0.0f : hi) : bar.getX();
        float x1 = toX (value);
        if (x1 < x0) std::swap (x0, x1);
        g.setColour (colour);
        g.fillRoundedRectangle (juce::Rectangle<float> (x0, bar.getY(), juce::jmax (2.0f, x1 - x0), bar.getHeight()), 2.0f);
    }

    if (hasMarker)
    {
        g.setColour (juce::Colours::white);
        const float x = toX (marker);
        g.fillRect (juce::Rectangle<float> (x - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f));
    }

    g.setColour (valid ? textCol : dimText);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (valid ? juce::String (value, 1) + " " + units : "--", valueArea, juce::Justification::centredRight);
}

//==============================================================================
NiveladorEditor::NiveladorEditor (NiveladorProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    laf.setColour (juce::Slider::rotarySliderFillColourId, accent);
    laf.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    laf.setColour (juce::Slider::textBoxTextColourId, textCol);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::Label::textColourId, textCol);
    laf.setColour (juce::ToggleButton::textColourId, textCol);
    laf.setColour (juce::ToggleButton::tickColourId, good);
    laf.setColour (juce::TextButton::buttonColourId, panel.brighter (0.15f));
    laf.setColour (juce::TextButton::textColourOffId, textCol);
    laf.setColour (juce::ComboBox::backgroundColourId, panel.brighter (0.15f));
    laf.setColour (juce::ComboBox::textColourId, textCol);
    laf.setColour (juce::PopupMenu::backgroundColourId, panel);
    setLookAndFeel (&laf);

    setupKnob (target,   ids::target,   "Alvo");
    setupKnob (trim,     ids::trim,     "Ganho entrada");
    setupKnob (maxBoost, ids::maxBoost, u8 ("Subida máx."));
    setupKnob (maxCut,   ids::maxCut,   u8 ("Corte máx."));
    setupKnob (gate,     ids::gate,     u8 ("Gate silêncio"));
    setupKnob (ceiling,  ids::ceiling,  "Teto");

    mode.addItemList ({ "Voz (microfones)", "Programa (master)" }, 1);
    addAndMakeVisible (mode);
    modeAttachment = std::make_unique<ComboAttachment> (proc.apvts, ids::mode, mode);

    addAndMakeVisible (levelerOn);
    addAndMakeVisible (limiterOn);
    levelerAttachment = std::make_unique<ButtonAttachment> (proc.apvts, ids::leveler, levelerOn);
    limiterAttachment = std::make_unique<ButtonAttachment> (proc.apvts, ids::limiter, limiterOn);

    for (auto* b : { &presetWeb, &presetVmix, &presetTv })
        addAndMakeVisible (b);
    presetWeb.onClick  = [this] { setTarget (-14.0f); };
    presetVmix.onClick = [this] { setTarget (-18.0f); };
    presetTv.onClick   = [this] { setTarget (-23.0f); };

    for (auto* m : { &meterIn, &meterOut, &meterGain, &meterLim })
        addAndMakeVisible (m);

    gateLabel.setJustificationType (juce::Justification::centredRight);
    gateLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (gateLabel);

    setSize (640, 400);
    startTimerHz (20);
}

NiveladorEditor::~NiveladorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NiveladorEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    addAndMakeVisible (k.slider);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k.slider);
}

void NiveladorEditor::setTarget (float lufs)
{
    if (auto* param = proc.apvts.getParameter (ids::target))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (lufs));
        param->endChangeGesture();
    }
}

void NiveladorEditor::timerCallback()
{
    const float targetV = proc.apvts.getRawParameterValue (ids::target)->load();
    const float in   = proc.leveler.meterIn.load();
    const float out  = proc.leveler.meterOut.load();
    const float gain = proc.leveler.meterGain.load();
    const float lim  = proc.leveler.meterLimit.load();
    const bool gateOpen = proc.leveler.meterGate.load();

    meterIn.setValue (in, dimText.brighter (0.4f));

    const float diff = std::abs (out - targetV);
    meterOut.setMarker (targetV);
    meterOut.setValue (out, diff < 2.0f ? good : (diff < 5.0f ? warn : hot));

    meterGain.setValue (gain, gain >= 0.0f ? accent : warn);
    meterLim.setValue (lim < -0.05f ? lim : 0.0f, lim < -6.0f ? hot : warn);

    gateLabel.setText (gateOpen ? juce::String ("Sinal presente: ajustando") : u8 ("Silêncio: ganho parado"),
                       juce::dontSendNotification);
    gateLabel.setColour (juce::Label::textColourId, gateOpen ? good : dimText);
}

void NiveladorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    auto r = getLocalBounds().reduced (12);
    auto header = r.removeFromTop (34);
    g.setColour (textCol);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("Nivelador vMix", header, juce::Justification::centredLeft);

    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().reduced (12).withTrimmedTop (42).withHeight (150).toFloat(), 6.0f);
    g.fillRoundedRectangle (getLocalBounds().reduced (12).withTrimmedTop (202).toFloat(), 6.0f);
}

void NiveladorEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto header = r.removeFromTop (34);
    header.removeFromLeft (170);
    mode.setBounds (header.removeFromRight (180).reduced (0, 4));
    header.removeFromRight (8);
    limiterOn.setBounds (header.removeFromRight (100));
    levelerOn.setBounds (header.removeFromRight (100));

    r.removeFromTop (8);
    auto meters = r.removeFromTop (150).reduced (12, 8);
    auto top = meters.removeFromTop (22);
    gateLabel.setBounds (top);
    const int h = meters.getHeight() / 4;
    meterIn.setBounds (meters.removeFromTop (h));
    meterOut.setBounds (meters.removeFromTop (h));
    meterGain.setBounds (meters.removeFromTop (h));
    meterLim.setBounds (meters);

    r.removeFromTop (10);
    auto controls = r.reduced (12, 8);
    auto presets = controls.removeFromTop (28);
    const int pw = presets.getWidth() / 3;
    presetWeb.setBounds (presets.removeFromLeft (pw).reduced (4, 0));
    presetVmix.setBounds (presets.removeFromLeft (pw).reduced (4, 0));
    presetTv.setBounds (presets.reduced (4, 0));

    controls.removeFromTop (6);
    const int kw = controls.getWidth() / 6;
    for (auto* k : { &target, &trim, &maxBoost, &maxCut, &gate, &ceiling })
    {
        auto cell = controls.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell);
    }
}
