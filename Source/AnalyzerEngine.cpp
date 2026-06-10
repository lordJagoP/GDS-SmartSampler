#include "AnalyzerEngine.h"
#include <cmath>

AnalyzerEngine::AnalyzerEngine()
    : fft(9)  // 2^9 = 512-point FFT, matches our block size exactly
{
}

void AnalyzerEngine::prepare(double sr, int blockSz)
{
    sampleRate = sr;
    bufferSize = blockSz;

    int fftSize = 512;
    prevMagnitude.assign(fftSize / 2, 0.0f);
    fftBuffer.resize(fftSize * 2, 0.0f);
    windowData.resize(fftSize);

    // Hann window
    for (int i = 0; i < fftSize; ++i)
        windowData[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

    pitchFIFO.assign(blockSz, 0.0f);
    timingMap.fill(0.0f);
}

void AnalyzerEngine::analyzeBlock(const juce::AudioBuffer<float>& buf)
{
    detectOnset(buf);
    detectPitch(buf);

    // Build timing map: per-sample warp offset based on onset strength
    // Strong onset = sharper pull toward sidechain grid position
    float warpPull = onsetDetected ? onsetStrength : 0.0f;
    for (int i = 0; i < bufferSize; ++i)
        timingMap[i] = warpPull * (float(i) / float(bufferSize));  // ramp into onset
}

void AnalyzerEngine::detectOnset(const juce::AudioBuffer<float>& buf)
{
    // --- Dual envelope follower (fast attack / slow release) ---
    const float* data   = buf.getReadPointer(0);
    const int    nSamps = buf.getNumSamples();

    const float attackFast  = std::exp(-1.0f / (0.001f * (float)sampleRate));  // ~1ms
    const float attackSlow  = std::exp(-1.0f / (0.020f * (float)sampleRate));  // ~20ms

    for (int i = 0; i < nSamps; ++i)
    {
        float absVal = std::abs(data[i]);
        envFast = absVal + attackFast  * (envFast - absVal);
        envSlow = absVal + attackSlow  * (envSlow - absVal);
    }

    // Transient = fast envelope diverging above slow envelope
    float divergence = envFast - envSlow;
    onsetStrength    = juce::jmax(0.0f, divergence);
    onsetDetected    = (divergence > onsetThreshold);

    // --- Spectral flux confirmation ---
    // Copy + window the block
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    for (int i = 0; i < nSamps && i < 512; ++i)
        fftBuffer[i] = data[i] * windowData[i];

    fft.performRealOnlyForwardTransform(fftBuffer.data());

    float flux = 0.0f;
    int halfSize = 512 / 2;
    for (int bin = 0; bin < halfSize; ++bin)
    {
        float re  = fftBuffer[2 * bin];
        float im  = fftBuffer[2 * bin + 1];
        float mag = std::sqrt(re * re + im * im);
        float diff = mag - prevMagnitude[bin];
        if (diff > 0.0f) flux += diff;  // only positive flux (HWR)
        prevMagnitude[bin] = mag;
    }

    // Combine: onset is valid if both detectors agree
    if (flux > 2.5f && onsetDetected)
        onsetStrength = juce::jmin(1.0f, onsetStrength + (flux / 10.0f));
}

void AnalyzerEngine::detectPitch(const juce::AudioBuffer<float>& buf)
{
    // YIN-style autocorrelation for fundamental frequency
    const float* data   = buf.getReadPointer(0);
    const int    nSamps = buf.getNumSamples();

    // Difference function
    float minVal   = std::numeric_limits<float>::max();
    int   minTau   = -1;
    int   maxTau   = nSamps / 2;

    std::vector<float> d(maxTau, 0.0f);
    for (int tau = 1; tau < maxTau; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < maxTau; ++j)
        {
            float diff = data[j] - data[j + tau];
            sum += diff * diff;
        }
        d[tau] = sum;

        if (sum < minVal && tau > 4)
        {
            minVal = sum;
            minTau = tau;
        }
    }

    // Cumulative mean normalized difference
    if (minTau > 0 && minVal < 0.1f)  // silence threshold
        currentPitchHz = (float)sampleRate / (float)minTau;
    else
        currentPitchHz = 0.0f;  // no confident pitch / silence
}
