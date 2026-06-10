// PluginEditor.h
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SmartSamplerEditor : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget
{
public:
    SmartSamplerEditor(SmartSamplerProcessor&);
    ~SmartSamplerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Drag-and-drop sample loading
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int, int) override;

private:
    SmartSamplerProcessor& processor;

    juce::Slider tightnessKnob;
    juce::Slider harmonicBlendKnob;
    juce::Label  tightnessLabel;
    juce::Label  harmonicBlendLabel;
    juce::Label  dropZoneLabel;
    juce::Label  statusLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tightnessAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonicAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartSamplerEditor)
};
