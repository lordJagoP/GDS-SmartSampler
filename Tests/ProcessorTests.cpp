#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <thread>
#include <atomic>

namespace
{
int failures = 0;
void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }

juce::File makeSample(double rate, int channels = 1, bool rightOnly = false, int length = 8192)
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("smartsampler-test", ".wav");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = format.createWriterFor(stream,
        juce::AudioFormatWriterOptions().withSampleRate(rate)
            .withNumChannels(channels).withBitsPerSample(16));
    check(writer != nullptr, "Create WAV fixture");
    juce::AudioBuffer<float> sample(channels, length);
    sample.clear();
    for (int i = 0; i < sample.getNumSamples(); ++i)
        for (int ch = 0; ch < channels; ++ch)
            if (!rightOnly || ch == channels - 1)
                sample.setSample(ch, i, 0.25f);
    if (writer != nullptr)
        check(writer->writeFromAudioSampleBuffer(sample, 0, sample.getNumSamples()), "Write WAV fixture");
    return file;
}

void testSampler(const juce::File& sample)
{
    SamplerEngine engine;
    engine.prepare(48000.0, 128);
    check(engine.loadSample(sample), "Decode WAV");
    check(std::abs(engine.getTailLengthSeconds() - 8192.0 / (48000.0 * 0.1)) < 0.000001,
        "Conservative tail matches minimum supported pitch duration");
    std::array<float, 512> timing{};
    juce::AudioBuffer<float> out(2, 1024);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 27);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 150);
    engine.processMidi(midi, out, timing, 0.0f, 0.0f, 0.0f);
    check(near(out.getSample(0, 26), 0.0f), "Note-on respects sample offset");
    check(near(out.getSample(0, 27), 0.25f), "Normalized full velocity");
    check(near(out.getSample(0, 149), 0.25f), "Render until note-off");
    check(near(out.getSample(0, 150), 0.0f), "Note-off respects sample offset");
    for (const int size : {0, 1, 16, 127, 512, 1024, 2048})
    {
        out.setSize(2, size);
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
        engine.processMidi(midi, out, timing, 0.0f, 1.0f, 0.0f);
        if (size > 0)
            check(std::isfinite(out.getSample(0, size - 1)), "Variable block remains finite");
    }
    // Exercise sample replacement while playback-state transitions occur.
    std::atomic_bool loaded{true};
    std::thread loader([&]()
    {
        for (int i = 0; i < 30; ++i)
            if (!engine.loadSample(sample))
                loaded.store(false);
    });
    out.setSize(2, 128);
    for (int i = 0; i < 300; ++i)
        engine.processMidi(midi, out, timing, 0.0f, 0.0f, 0.0f);
    loader.join();
    check(loaded.load(), "Concurrent replacement succeeds");
}

void testSampleRate()
{
    auto sample = makeSample(48000.0, 2, true);
    SamplerEngine engine;
    engine.prepare(48000.0, 128);
    check(engine.loadSample(sample), "Decode right-only stereo WAV");
    juce::AudioBuffer<float> out(1, 16);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    std::array<float, 512> timing{};
    engine.processMidi(midi, out, timing, 0.0f, 0.0f, 0.0f);
    check(near(out.getSample(0, 0), 0.125f), "Stereo right channel survives mono downmix");
    sample.deleteFile();
    for (const double rate : {24000.0, 384000.0, 2400.0})
    {
        sample = makeSample(rate, 1, false, 512);
        check(engine.loadSample(sample), "Decode different-rate WAV");
        const int duration = static_cast<int>(512.0 * 48000.0 / rate);
        out.setSize(2, duration + 4);
        engine.processMidi(midi, out, timing, 0.0f, 0.0f, 0.0f);
        check(near(out.getSample(0, duration - 2), 0.25f), "Source/host ratio preserves duration");
        check(near(out.getSample(0, duration + 2), 0.0f), "Different-rate sample ends correctly");
        check(std::abs(engine.getTailLengthSeconds() - 512.0 / (rate * 0.1)) < 0.000001,
            "Tail bound uses source sample rate");
        sample.deleteFile();
    }
    AnalyzerEngine left, right;
    left.prepare(48000.0, 512);
    right.prepare(48000.0, 512);
    juce::AudioBuffer<float> leftInput(2, 512), rightInput(2, 512);
    leftInput.clear();
    rightInput.clear();
    for (int i = 0; i < 512; ++i)
    {
        const float value = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * i / 48.0f);
        leftInput.setSample(0, i, value);
        rightInput.setSample(1, i, value);
    }
    left.analyzeBlock(leftInput);
    right.analyzeBlock(rightInput);
    check(left.getCurrentPitch() > 0.0f && near(left.getCurrentPitch(), right.getCurrentPitch()),
        "Right-only pitch analysis matches left");
    check(near(left.getOnsetStrength(), right.getOnsetStrength()),
        "Right-only onset analysis matches left");
}

void testProcessor(const juce::File& sample)
{
    SmartSamplerProcessor processor;
    auto layout = processor.getBusesLayout();
    check(layout.inputBuses.size() == 2, "Main input and sidechain have separate bus indices");
    check(processor.isBusesLayoutSupported(layout), "Default layout supported");
    processor.prepareToPlay(48000.0, 128);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(2, 128);
#if JucePlugin_IsSynth
    buffer.clear();
    processor.processBlock(buffer, midi);
    check(near(buffer.getMagnitude(0, 128), 0.0f), "Empty instrument outputs silence");
    check(processor.loadSampleFile(sample), "Instrument sample load");
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 23);
    *processor.tightnessParam = 0.0f;
    *processor.harmonicBlendParam = 0.0f;
    processor.processBlock(buffer, midi);
    check(near(buffer.getSample(0, 22), 0.0f), "Instrument trigger offset");
    check(near(buffer.getSample(0, 23), 0.25f), "Instrument plays without sidechain");
#else
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 128; ++i)
            buffer.setSample(ch, i, 0.02f);
    processor.processBlock(buffer, midi);
    check(near(buffer.getSample(0, 40), 0.02f), "FX without sample passes audio");
    check(processor.loadSampleFile(sample), "FX sample load");
    processor.prepareToPlay(48000.0, 128);
    *processor.tightnessParam = 0.0f;
    *processor.harmonicBlendParam = 0.0f;
    buffer.clear();
    buffer.setSample(0, 37, 0.5f);
    buffer.setSample(1, 37, 0.5f);
    processor.processBlock(buffer, midi);
    const float wetExpected = 0.25f * 0.5f
        * juce::MidiMessage::noteOn(1, 60, 0.5f).getFloatVelocity();
    check(near(buffer.getSample(0, 36), 0.0f), "No wet signal before audio trigger");
    check(near(buffer.getSample(0, 37), 0.5f + wetExpected), "FX mixes dry plus velocity-scaled sample at trigger");
    check(near(buffer.getSample(0, 38), wetExpected), "Audio trigger plays sample tail without MIDI");
    *processor.dryParam = 0.3f;
    *processor.wetParam = 0.7f;
    *processor.triggerDbParam = -18.0f;
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    SmartSamplerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(near(restored.dryParam->get(), 0.3f)
        && near(restored.wetParam->get(), 0.7f)
        && near(restored.triggerDbParam->get(), -18.0f), "FX parameter state round-trip");
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    check(processor.setBusesLayout(layout), "FX mono layout");
    layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    check(processor.setBusesLayout(layout), "FX optional stereo sidechain");
    processor.prepareToPlay(48000.0, 128);
    *processor.dryParam = 1.0f;
    *processor.wetParam = 0.5f;
    buffer.setSize(3, 128);
    buffer.clear();
    buffer.setSample(0, 0, 0.02f);
    buffer.setSample(1, 41, 0.5f);
    processor.processBlock(buffer, midi);
    check(near(buffer.getSample(0, 0), 0.02f), "Sidechain never overwrites dry main input");
    check(near(buffer.getSample(0, 40), 0.0f), "Sidechain trigger offset");
    check(near(buffer.getSample(0, 41), wetExpected), "Sidechain triggers wet sample");
    SmartSamplerProcessor sustained;
    sustained.prepareToPlay(48000.0, 128);
    check(sustained.loadSampleFile(sample), "Sustained-input fixture load");
    *sustained.dryParam = 0.0f;
    *sustained.wetParam = 1.0f;
    *sustained.tightnessParam = 0.0f;
    *sustained.harmonicBlendParam = 0.0f;
    buffer.setSize(2, 128);
    for (int block = 0; block < 80; ++block)
    {
        for (int i = 0; i < 128; ++i)
        {
            const float value = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi
                * static_cast<float>(block * 128 + i) / 192.0f);
            buffer.setSample(0, i, value);
            buffer.setSample(1, i, value);
        }
        sustained.processBlock(buffer, midi);
        if (block == 1)
            check(buffer.getMagnitude(0, 128) > 0.0f, "Sustained input triggers initial sample");
    }
    check(near(buffer.getMagnitude(0, 128), 0.0f),
        "Sustained waveform zero crossings do not restart completed sample");
#endif
    processor.releaseResources();
}
}

int main()
{
    auto sample = makeSample(48000.0);
    testSampler(sample);
    testSampleRate();
    testProcessor(sample);
    sample.deleteFile();
    std::cout << JucePlugin_Name << ": " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
