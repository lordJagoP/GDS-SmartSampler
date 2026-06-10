#pragma once
#include <JuceHeader.h>
#include "AnalyzerEngine.h"

class SamplerEngine
{
public:
    SamplerEngine();
    void prepare(double sampleRate, int blockSize);
    void loadSample(const juce::File& file);

    void processMidi(juce::MidiBuffer& midi,
                     juce::AudioBuffer<float>& outputBuf,
                     const std::array<float, 512>& timingMap,
                     float sidechainPitchHz,
                     float tightness,
                     float harmonicBlend);

    bool isSampleLoaded() const { return sampleLoaded; }

private:
    void triggerNote(int midiNote, float velocity);
    void releaseNote(int midiNote);

    double sampleRate  = 44100.0;
    int    blockSize   = 512;
    bool   sampleLoaded = false;

    juce::AudioBuffer<float> sampleBuffer;
    int    sampleLength    = 0;
    double playheadPos     = 0.0;
    bool   isPlaying       = false;
    float  currentMidiNote = 60.0f;   // C3 = root
    float  rootNote        = 60.0f;   // pitch of the loaded sample
    float  currentVelocity = 1.0f;

    juce::AudioFormatManager formatManager;
};
