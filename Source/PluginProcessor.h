#pragma once
#include <JuceHeader.h>
#include "AnalyzerEngine.h"
#include "SamplerEngine.h"

class SmartSamplerProcessor : public juce::AudioProcessor
{
public:
    SmartSamplerProcessor();
    ~SmartSamplerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SmartSampler"; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public access for editor
    void loadSampleFile(const juce::File& f) { samplerEngine.loadSample(f); }
    bool isSampleLoaded() const { return samplerEngine.isSampleLoaded(); }

    // Parameters exposed to the UI
    juce::AudioParameterFloat* tightnessParam    = nullptr;
    juce::AudioParameterFloat* harmonicBlendParam = nullptr;

private:
    AnalyzerEngine analyzerEngine;
    SamplerEngine  samplerEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartSamplerProcessor)
};
