#pragma once
#include <JuceHeader.h>
#include <array>

class AnalyzerEngine
{
public:
    AnalyzerEngine();
    void prepare(double sampleRate, int blockSize);
    void analyzeBlock(const juce::AudioBuffer<float>& sidechainBuffer);

    // Transient data
    bool  hasOnset()         const { return onsetDetected; }
    float getOnsetStrength() const { return onsetStrength; }

    // Pitch data (Hz)
    float getCurrentPitch()  const { return currentPitchHz; }

    // Timing map — array of normalized warp offsets per block frame
    const std::array<float, 512>& getTimingMap() const { return timingMap; }

private:
    // --- Spectral flux onset detection ---
    void detectOnset(const juce::AudioBuffer<float>& buf);

    // --- Autocorrelation pitch tracking (YIN-style) ---
    void detectPitch(const juce::AudioBuffer<float>& buf);

    double sampleRate  = 44100.0;
    int    bufferSize  = 512;

    // Spectral flux state
    std::vector<float> prevMagnitude;
    float              onsetStrength   = 0.0f;
    bool               onsetDetected   = false;
    float              onsetThreshold  = 0.15f;  // adjustable
    float              runningRMS      = 0.0f;

    // Pitch state
    float              currentPitchHz  = 0.0f;
    std::vector<float> pitchFIFO;

    // Dual envelope followers for transient detection
    float envFast = 0.0f;
    float envSlow = 0.0f;

    // Timing warp map filled each block
    std::array<float, 512> timingMap{};

    juce::dsp::FFT fft;
    std::vector<float> fftBuffer;
    std::vector<float> windowData;
};
