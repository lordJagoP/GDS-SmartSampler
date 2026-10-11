// PluginEditor.cpp
#include "PluginEditor.h"

SmartSamplerEditor::SmartSamplerEditor(SmartSamplerProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 250);
#if !JucePlugin_IsSynth
    setSize(480, 400);
    const auto setup = [this](juce::Slider& knob, juce::Label& label,
                              juce::AudioParameterFloat* parameter)
    {
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setRange(parameter->range.start, parameter->range.end);
        knob.setValue(parameter->get());
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        knob.onValueChange = [&knob, parameter]()
        {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(
                static_cast<float>(knob.getValue())));
        };
        label.setText(parameter->name, juce::dontSendNotification);
        knob.onDragStart = [parameter]() { parameter->beginChangeGesture(); };
        knob.onDragEnd = [parameter]() { parameter->endChangeGesture(); };
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knob);
        addAndMakeVisible(label);
    };
    setup(dryKnob, dryLabel, processor.dryParam);
    setup(wetKnob, wetLabel, processor.wetParam);
    setup(triggerKnob, triggerLabel, processor.triggerDbParam);
#endif

    // Drop zone label
    dropZoneLabel.setText("Drop a sample here (.wav / .aif)", juce::dontSendNotification);
    dropZoneLabel.setJustificationType(juce::Justification::centred);
    dropZoneLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
    dropZoneLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(dropZoneLabel);

    // Status
    statusLabel.setText("No sample loaded", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(statusLabel);

    // Tightness knob
    tightnessKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    tightnessKnob.setRange(0.0, 1.0);
    tightnessKnob.setValue(processor.tightnessParam->get());
    tightnessKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible(tightnessKnob);

    tightnessLabel.setText("Tightness", juce::dontSendNotification);
    tightnessLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tightnessLabel);

    // Harmonic Blend knob
    harmonicBlendKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    harmonicBlendKnob.setRange(0.0, 1.0);
    harmonicBlendKnob.setValue(processor.harmonicBlendParam->get());
    harmonicBlendKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible(harmonicBlendKnob);

    harmonicBlendLabel.setText("Harmonic Blend", juce::dontSendNotification);
    harmonicBlendLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(harmonicBlendLabel);

    // Wire knobs directly to processor parameters
    tightnessKnob.onValueChange = [this]()
    {
        processor.tightnessParam->setValueNotifyingHost(
            processor.tightnessParam->convertTo0to1(static_cast<float>(tightnessKnob.getValue())));
    };
    harmonicBlendKnob.onValueChange = [this]()
    {
        processor.harmonicBlendParam->setValueNotifyingHost(
            processor.harmonicBlendParam->convertTo0to1(static_cast<float>(harmonicBlendKnob.getValue())));
    };
    tightnessKnob.onDragStart = [this]() { processor.tightnessParam->beginChangeGesture(); };
    tightnessKnob.onDragEnd = [this]() { processor.tightnessParam->endChangeGesture(); };
    harmonicBlendKnob.onDragStart = [this]() { processor.harmonicBlendParam->beginChangeGesture(); };
    harmonicBlendKnob.onDragEnd = [this]() { processor.harmonicBlendParam->endChangeGesture(); };
    startTimerHz(20);
}

SmartSamplerEditor::~SmartSamplerEditor() {}

void SmartSamplerEditor::timerCallback()
{
    const auto sync = [](juce::Slider& knob, juce::AudioParameterFloat* parameter)
    {
        if (!knob.isMouseButtonDown())
            knob.setValue(parameter->get(), juce::dontSendNotification);
    };
    sync(tightnessKnob, processor.tightnessParam);
    sync(harmonicBlendKnob, processor.harmonicBlendParam);
#if !JucePlugin_IsSynth
    sync(dryKnob, processor.dryParam);
    sync(wetKnob, processor.wetParam);
    sync(triggerKnob, processor.triggerDbParam);
#endif
}

void SmartSamplerEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    juce::File f(files[0]);
    if (processor.loadSampleFile(f))
    {
        statusLabel.setText("Loaded: " + f.getFileName(), juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::limegreen);
    }
    else
    {
        statusLabel.setText("Unsupported or unreadable sample", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }
}

void SmartSamplerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colours::cyan);
    g.setFont(16.0f);
    g.drawText(processor.getName() + " | Golden Digital Services",
               getLocalBounds().removeFromTop(30),
               juce::Justification::centred);
}

void SmartSamplerEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(35);  // title bar space

    dropZoneLabel.setBounds(area.removeFromTop(50));
    area.removeFromTop(5);
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(10);

    auto knobRow = area.removeFromTop(120);
    int  knobW   = knobRow.getWidth() / 2;

    auto tightArea = knobRow.removeFromLeft(knobW);
    tightnessLabel.setBounds(tightArea.removeFromBottom(20));
    tightnessKnob.setBounds(tightArea);

    harmonicBlendLabel.setBounds(knobRow.removeFromBottom(20));
    harmonicBlendKnob.setBounds(knobRow);
#if !JucePlugin_IsSynth
    auto fxRow = area.removeFromTop(120);
    const int width = fxRow.getWidth() / 3;
    auto dryArea = fxRow.removeFromLeft(width);
    dryLabel.setBounds(dryArea.removeFromBottom(20));
    dryKnob.setBounds(dryArea);
    auto wetArea = fxRow.removeFromLeft(width);
    wetLabel.setBounds(wetArea.removeFromBottom(20));
    wetKnob.setBounds(wetArea);
    triggerLabel.setBounds(fxRow.removeFromBottom(20));
    triggerKnob.setBounds(fxRow);
#endif
}
