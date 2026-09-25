#include "PluginProcessor.h"
#include "PluginEditor.h"

SmartSamplerProcessor::SmartSamplerProcessor()
    : AudioProcessor (BusesProperties()
        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false))
{
    // Register parameters
    addParameter(tightnessParam     = new juce::AudioParameterFloat("tightness",     "Tightness",     0.0f, 1.0f, 0.75f));
    addParameter(harmonicBlendParam = new juce::AudioParameterFloat("harmonicBlend", "Harmonic Blend", 0.0f, 1.0f, 0.25f));
}

SmartSamplerProcessor::~SmartSamplerProcessor() {}

void SmartSamplerProcessor::prepareToPlay(double sr, int blockSize)
{
    analyzerEngine.prepare(sr, blockSize);
    samplerEngine.prepare(sr, blockSize);
}

void SmartSamplerProcessor::releaseResources() {}

bool SmartSamplerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Must have stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain can be stereo or mono — accept both
    auto sc = layouts.getChannelSet(true, 1);
    if (!sc.isDisabled() &&
        sc != juce::AudioChannelSet::stereo() &&
        sc != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void SmartSamplerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // 1. Grab the sidechain bus (Bus index 1, input side)
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);

    // 2. Analyze: build onset + pitch data from the sidechain. Calling this
    // for a disabled bus also clears stale analysis from the previous block.
    analyzerEngine.analyzeBlock(sidechainBuffer);

    // 3. Prepare the main output buffer (clear it first)
    auto mainOutputBuffer = getBusBuffer(buffer, false, 0);
    mainOutputBuffer.clear();

    // 4. Run the sampler engine — pass timing map + current pitch from analyzer
    samplerEngine.processMidi(
        midiMessages,
        mainOutputBuffer,
        analyzerEngine.getTimingMap(),
        analyzerEngine.getCurrentPitch(),
        tightnessParam->get(),
        harmonicBlendParam->get()
    );
}

void SmartSamplerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(*tightnessParam);
    stream.writeFloat(*harmonicBlendParam);
}

void SmartSamplerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getNumBytesRemaining() >= 8)
    {
        *tightnessParam      = stream.readFloat();
        *harmonicBlendParam  = stream.readFloat();
    }
}


juce::AudioProcessorEditor* SmartSamplerProcessor::createEditor()
{
    return new SmartSamplerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmartSamplerProcessor();
}
