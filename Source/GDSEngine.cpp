#include "GDSEngine.h"
#include <cmath>

GDSEngine::GDSEngine() : fft(9)
{
    formatManager.registerBasicFormats();
}

void GDSEngine::prepare(double sr, int blockSz)
{
    sampleRate = sr;
    blockSize  = blockSz;

    int fftSize = 512;
    fftBuffer.resize(fftSize * 2, 0.0f);
    windowData.resize(fftSize);
    prevMagnitude.assign(fftSize / 2, 0.0f);

    for (int i = 0; i < fftSize; ++i)
        windowData[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                  * i / (fftSize - 1)));
    timingMap.fill(0.0f);
    state = SoloistState();

    // Silence threshold: ~0.5 seconds of silence ends a phrase
    minSilenceFrames = (int)(sr * 0.5 / blockSz);
}

//==============================================================================
// Universal File Loader
//==============================================================================
bool GDSEngine::loadFile(const juce::File& file)
{
    juce::File targetFile = file;

    // ADG handler  Ableton Device Group (gzipped XML)
    if (file.getFileExtension().toLowerCase() == ".adg")
    {
        targetFile = resolveADGSample(file);
        if (!targetFile.existsAsFile())
        {
            DBG("GDSEngine: Could not resolve sample from ADG file.");
            return false;
        }
    }

    auto* reader = formatManager.createReaderFor(targetFile);
    if (reader == nullptr)
    {
        DBG("GDSEngine: Unsupported format or file not found: " + targetFile.getFullPathName());
        return false;
    }

    sampleLength = (int)reader->lengthInSamples;
    sampleBuffer.setSize((int)reader->numChannels, sampleLength);
    reader->read(&sampleBuffer, 0, sampleLength, 0, true, true);
    delete reader;

    sampleLoaded   = true;
    loadedFileName = file.getFileName();
    state.playheadPos = 0.0;

    DBG("GDSEngine: Loaded [" + loadedFileName + "] "
        + juce::String(sampleLength) + " samples @ "
        + juce::String(sampleRate) + " Hz");
    return true;
}

juce::File GDSEngine::resolveADGSample(const juce::File& adgFile)
{
    // ADG = gzipped XML  decompress and parse for sample path
    juce::FileInputStream rawStream(adgFile);
    juce::GZIPDecompressorInputStream decompressor(&rawStream, false);
    juce::String xmlText = decompressor.readEntireStreamAsString();

    auto xml = juce::XmlDocument::parse(xmlText);
    if (xml == nullptr) return {};

    // Search for FileRef/Path elements in Ableton XML schema
    for (auto* child : xml->getChildIterator())
    {
        auto* fileRef = child->getChildByName("FileRef");
        if (fileRef != nullptr)
        {
            auto* pathEl = fileRef->getChildByName("Path");
            if (pathEl != nullptr)
            {
                juce::String path = pathEl->getStringAttribute("Value");
                return juce::File(path);
            }
        }
    }
    return {};
}

//==============================================================================
// Main Processing Block
//==============================================================================
void GDSEngine::processBlock(const juce::AudioBuffer<float>& inputBuf,
                              juce::AudioBuffer<float>& outputBuf,
                              juce::MidiBuffer& midiMessages)
{
    outputBuf.clear();
    if (!sampleLoaded) return;

    // --- Analyze incoming audio ---
    analyzeInput(inputBuf);

    // --- MIDI mode: check for note on/off ---
    if (triggerMode == TriggerMode::MidiGate ||
        triggerMode == TriggerMode::Hybrid)
    {
        for (const auto meta : midiMessages)
        {
            auto msg = meta.getMessage();
            if (msg.isNoteOn())
                triggerVoice((float)msg.getNoteNumber(), (float)msg.getVelocity());
            if (msg.isNoteOff() && decayMode == DecayMode::GatedSilence)
                state.isPlaying = false;
        }
    }

    // --- AudioAuto mode: trigger from audio energy ---
    if (triggerMode == TriggerMode::AudioAuto ||
        triggerMode == TriggerMode::Hybrid)
    {
        if (state.phraseDetected && !state.isPlaying)
            triggerVoice(currentPitchHz > 20.0f
                         ? (69.0f + 12.0f * std::log2(currentPitchHz / 440.0f))
                         : rootNote,
                         0.8f * 127.0f);

        // Decay handling
        if (!inPhrase && state.isPlaying)
        {
            if (decayMode == DecayMode::GatedSilence)
                state.isPlaying = false;
            // NaturalTail: let sample play out  no intervention
            // HoldDrone:   keep playing indefinitely
        }
    }

    // --- Render output ---
    renderOutput(outputBuf);
}

//==============================================================================
// Audio Analysis
//==============================================================================
void GDSEngine::analyzeInput(const juce::AudioBuffer<float>& buf)
{
    float energy = detectEnergy(buf);
    currentPitchHz = detectPitch(buf);
    detectOnset(buf);
    state.phraseDetected = detectPhrase(energy);
    state.detectedEnergy = energy;
    state.detectedPitch  = currentPitchHz;
}

float GDSEngine::detectEnergy(const juce::AudioBuffer<float>& buf)
{
    const float* data   = buf.getReadPointer(0);
    const int    nSamps = buf.getNumSamples();

    const float attackFast = std::exp(-1.0f / (0.001f * (float)sampleRate));
    const float attackSlow = std::exp(-1.0f / (0.020f * (float)sampleRate));

    float rms = 0.0f;
    for (int i = 0; i < nSamps; ++i)
    {
        float absVal = std::abs(data[i]);
        envFast = absVal + attackFast * (envFast - absVal);
        envSlow = absVal + attackSlow * (envSlow - absVal);
        rms    += data[i] * data[i];
    }
    return std::sqrt(rms / (float)nSamps);
}

bool GDSEngine::detectPhrase(float energy)
{
    bool triggered = false;

    if (energy > threshold)
    {
        if (!inPhrase)
        {
            inPhrase  = true;
            triggered = true;   // new phrase started
        }
        silenceCounter = 0;
        phraseCounter++;
    }
    else
    {
        silenceCounter++;
        if (silenceCounter > minSilenceFrames)
        {
            inPhrase      = false;
            phraseCounter = 0;
        }
    }

    state.phraseLength = phraseCounter;
    return triggered;
}

float GDSEngine::detectPitch(const juce::AudioBuffer<float>& buf)
{
    const float* data   = buf.getReadPointer(0);
    const int    nSamps = buf.getNumSamples();
    int          maxTau = nSamps / 2;

    float minVal = std::numeric_limits<float>::max();
    int   minTau = -1;

    for (int tau = 1; tau < maxTau; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < maxTau && (j + tau) < nSamps; ++j)
        {
            float diff = data[j] - data[j + tau];
            sum += diff * diff;
        }
        if (sum < minVal && tau > 4) { minVal = sum; minTau = tau; }
    }

    if (minTau > 0 && minVal < 0.1f)
        return (float)sampleRate / (float)minTau;
    return 0.0f;
}

void GDSEngine::detectOnset(const juce::AudioBuffer<float>& buf)
{
    const float* data   = buf.getReadPointer(0);
    const int    nSamps = buf.getNumSamples();

    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    for (int i = 0; i < nSamps && i < 512; ++i)
        fftBuffer[i] = data[i] * windowData[i];

    fft.performRealOnlyForwardTransform(fftBuffer.data());

    float flux = 0.0f;
    for (int bin = 0; bin < 256; ++bin)
    {
        float re  = fftBuffer[2 * bin];
        float im  = fftBuffer[2 * bin + 1];
        float mag = std::sqrt(re * re + im * im);
        float diff = mag - prevMagnitude[bin];
        if (diff > 0.0f) flux += diff;
        prevMagnitude[bin] = mag;
    }

    onsetDetected  = (flux > 2.5f);
    onsetStrength  = juce::jmin(1.0f, flux / 10.0f);

    float warpPull = onsetDetected ? onsetStrength : 0.0f;
    for (int i = 0; i < blockSize && i < 512; ++i)
        timingMap[i] = warpPull * (float(i) / float(blockSize));
}

//==============================================================================
// Voice Trigger + Render
//==============================================================================
void GDSEngine::triggerVoice(float noteOrHz, float velocityRaw)
{
    state.currentNote = noteOrHz;
    state.velocity    = (velocityRaw > 1.0f) ? velocityRaw / 127.0f : velocityRaw;
    state.playheadPos = 0.0;
    state.isPlaying   = true;
}

void GDSEngine::renderOutput(juce::AudioBuffer<float>& outputBuf)
{
    if (!state.isPlaying) return;

    float midiNote  = state.currentNote;
    float midiRatio = std::pow(2.0f, (midiNote - rootNote) / 12.0f);

    if (harmonicBlend > 0.0f && currentPitchHz > 20.0f)
    {
        float midiHz    = 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
        float blendedHz = midiHz * (1.0f - harmonicBlend) + currentPitchHz * harmonicBlend;
        float rootHz    = 440.0f * std::pow(2.0f, (rootNote - 69.0f) / 12.0f);
        midiRatio       = blendedHz / rootHz;
    }

    auto* outL       = outputBuf.getWritePointer(0);
    auto* outR       = outputBuf.getNumChannels() > 1 ? outputBuf.getWritePointer(1) : nullptr;
    const auto* srcL = sampleBuffer.getReadPointer(0);
    const auto* srcR = sampleBuffer.getNumChannels() > 1 ? sampleBuffer.getReadPointer(1) : srcL;

    for (int i = 0; i < blockSize; ++i)
    {
        if ((int)state.playheadPos >= sampleLength)
        {
            state.isPlaying = false;
            break;
        }

        double warpDelta    = (double)timingMap[i < 512 ? i : 511] * (double)tightness * 2.0;
        double playbackRate = juce::jmax(0.1, juce::jmin(4.0, (double)midiRatio + warpDelta));

        int   idx  = (int)state.playheadPos;
        float frac = (float)(state.playheadPos - idx);
        int   idxN = juce::jmin(idx + 1, sampleLength - 1);

        outL[i] = (srcL[idx] * (1.0f - frac) + srcL[idxN] * frac) * state.velocity;
        if (outR)
            outR[i] = (srcR[idx] * (1.0f - frac) + srcR[idxN] * frac) * state.velocity;

        state.playheadPos += playbackRate;
    }
}




