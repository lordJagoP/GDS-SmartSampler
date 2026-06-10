// PluginEditor.cpp
#include "PluginEditor.h"

SmartSamplerEditor::SmartSamplerEditor(SmartSamplerProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 250);

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
        *processor.tightnessParam = (float)tightnessKnob.getValue();
    };
    harmonicBlendKnob.onValueChange = [this]()
    {
        *processor.harmonicBlendParam = (float)harmonicBlendKnob.getValue();
    };
}

SmartSamplerEditor::~SmartSamplerEditor() {}

void SmartSamplerEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    juce::File f(files[0]);
    processor.loadSampleFile(f);
    statusLabel.setText("Loaded: " + f.getFileName(), juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::limegreen);
}

void SmartSamplerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colours::cyan);
    g.setFont(16.0f);
    g.drawText("SmartSampler | Golden Digital Services",
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
}
