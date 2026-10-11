#include "PluginProcessor.h"
#include "PluginEditor.h"

SmartSamplerProcessor::SmartSamplerProcessor()
    : AudioProcessor (BusesProperties()
#if JucePlugin_IsSynth
        .withInput  ("Input", juce::AudioChannelSet::stereo(), false)
#else
        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false))
{
    // Register parameters
    addParameter(tightnessParam     = new juce::AudioParameterFloat("tightness",     "Tightness",     0.0f, 1.0f, 0.75f));
    addParameter(harmonicBlendParam = new juce::AudioParameterFloat("harmonicBlend", "Harmonic Blend", 0.0f, 1.0f, 0.25f));
#if !JucePlugin_IsSynth
    addParameter(dryParam = new juce::AudioParameterFloat("dry", "Dry", 0.0f, 1.0f, 1.0f));
    addParameter(wetParam = new juce::AudioParameterFloat("wet", "Sample Level", 0.0f, 1.0f, 0.5f));
    addParameter(triggerDbParam = new juce::AudioParameterFloat("triggerDb", "Trigger dB", -60.0f, 0.0f, -24.0f));
#endif
}

SmartSamplerProcessor::~SmartSamplerProcessor() {}

void SmartSamplerProcessor::prepareToPlay(double sr, int blockSize)
{
    analyzerEngine.prepare(sr, blockSize);
    samplerEngine.prepare(sr, blockSize);
#if !JucePlugin_IsSynth
    triggerMidi.ensureSize(16384);
    triggerArmed = true;
    retriggerSamples = 0;
    retriggerHoldSamples = juce::jmax(1, static_cast<int>(sr * 0.05));
    triggerEnvelope = 0.0f;
    triggerRelease = static_cast<float>(std::exp(-1.0 / (0.03 * sr)));
#endif
}

void SmartSamplerProcessor::releaseResources() {}

bool SmartSamplerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
#if JucePlugin_IsSynth
    if (output != juce::AudioChannelSet::stereo()
        || !layouts.getMainInputChannelSet().isDisabled())
#else
    if ((output != juce::AudioChannelSet::stereo()
         && output != juce::AudioChannelSet::mono())
        || layouts.getMainInputChannelSet() != output)
#endif
        return false;

    // Sidechain can be stereo or mono — accept both
    if (layouts.inputBuses.size() != 2 || layouts.outputBuses.size() != 1)
        return false;
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

    auto mainOutputBuffer = getBusBuffer(buffer, false, 0);
#if JucePlugin_IsSynth
    analyzerEngine.analyzeBlock(sidechainBuffer);
    samplerEngine.processMidi(
        midiMessages,
        mainOutputBuffer,
        analyzerEngine.getTimingMap(),
        analyzerEngine.getCurrentPitch(),
        tightnessParam->get(),
        harmonicBlendParam->get()
    );
#else
    auto mainInputBuffer = getBusBuffer(buffer, true, 0);
    const auto& triggerInput = sidechainBuffer.getNumChannels() > 0
        ? sidechainBuffer : mainInputBuffer;
    analyzerEngine.analyzeBlock(triggerInput);

    // A hysteretic audio gate generates sample-accurate one-shot triggers.
    // Sidechain, if enabled, replaces the main input as the trigger source.
    triggerMidi.clear();
    for (const auto event : midiMessages)
        triggerMidi.addEvent(event.getMessage(), event.samplePosition);
    const float threshold = juce::Decibels::decibelsToGain(triggerDbParam->get());
    for (int i = 0; i < triggerInput.getNumSamples(); ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < triggerInput.getNumChannels(); ++ch)
            peak = juce::jmax(peak, std::abs(triggerInput.getSample(ch, i)));
        if (retriggerSamples > 0)
            --retriggerSamples;
        // Zero crossings must not re-arm the gate during a sustained sound.
        triggerEnvelope = juce::jmax(peak, triggerEnvelope * triggerRelease);
        if (triggerEnvelope < threshold * 0.5f)
            triggerArmed = true;
        if (triggerArmed && triggerEnvelope >= threshold && retriggerSamples == 0)
        {
            triggerMidi.addEvent(juce::MidiMessage::noteOn(1, 60,
                juce::jlimit(0.0f, 1.0f, peak)), i);
            triggerArmed = false;
            retriggerSamples = retriggerHoldSamples;
        }
    }
    // Input and output main buses share the buffer for supported layouts.
    // Preserve dry audio; sampler rendering is additive instead of clearing it.
    mainOutputBuffer.applyGain(dryParam->get());
    samplerEngine.processMidi(triggerMidi, mainOutputBuffer,
        analyzerEngine.getTimingMap(), analyzerEngine.getCurrentPitch(),
        tightnessParam->get(), harmonicBlendParam->get(), wetParam->get(), false);
    midiMessages.clear(); // This effect does not advertise MIDI output.
#endif
}

void SmartSamplerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(*tightnessParam);
    stream.writeFloat(*harmonicBlendParam);
#if !JucePlugin_IsSynth
    stream.writeFloat(*dryParam);
    stream.writeFloat(*wetParam);
    stream.writeFloat(*triggerDbParam);
#endif
}

void SmartSamplerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getNumBytesRemaining() >= 8)
    {
        *tightnessParam      = stream.readFloat();
        *harmonicBlendParam  = stream.readFloat();
#if !JucePlugin_IsSynth
        if (stream.getNumBytesRemaining() >= 12)
        {
            *dryParam = juce::jlimit(0.0f, 1.0f, stream.readFloat());
            *wetParam = juce::jlimit(0.0f, 1.0f, stream.readFloat());
            *triggerDbParam = juce::jlimit(-60.0f, 0.0f, stream.readFloat());
        }
#endif
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
