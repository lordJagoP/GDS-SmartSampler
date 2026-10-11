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

juce::File makeSample(double rate)
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("smartsampler-test", ".wav");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = format.createWriterFor(stream,
        juce::AudioFormatWriterOptions().withSampleRate(rate)
            .withNumChannels(1).withBitsPerSample(16));
    check(writer != nullptr, "Create WAV fixture");
    juce::AudioBuffer<float> sample(1, 8192);
    for (int i = 0; i < sample.getNumSamples(); ++i)
        sample.setSample(0, i, 0.25f);
    if (writer != nullptr)
        check(writer->writeFromAudioSampleBuffer(sample, 0, sample.getNumSamples()), "Write WAV fixture");
    return file;
}

void testSampler(const juce::File& sample)
{
    SamplerEngine engine;
    engine.prepare(48000.0, 128);
    check(engine.loadSample(sample), "Decode WAV");
    check(engine.getTailLengthSeconds() > 0.0, "Sample tail reported to host");
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
    auto sample = makeSample(24000.0);
    SamplerEngine engine;
    engine.prepare(48000.0, 128);
    check(engine.loadSample(sample), "Decode different-rate WAV");
    juce::AudioBuffer<float> out(2, 17000);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    std::array<float, 512> timing{};
    engine.processMidi(midi, out, timing, 0.0f, 0.0f, 0.0f);
    check(near(out.getSample(0, 16383), 0.25f), "Source sample rate doubles duration correctly");
    check(near(out.getSample(0, 16384), 0.0f), "Different-rate sample ends correctly");
    sample.deleteFile();
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
