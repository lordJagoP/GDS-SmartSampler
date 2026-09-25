#include "SamplerEngine.h"
#include <cmath>

SamplerEngine::SamplerEngine()
{
    formatManager.registerBasicFormats();
}

void SamplerEngine::prepare(double sr, int blockSz)
{
    sampleRate = sr;
    blockSize  = blockSz;
    playheadPos = 0.0;
    isPlaying   = false;
}

bool SamplerEngine::loadSample(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
        return false;

    juce::AudioBuffer<float> decoded(static_cast<int>(reader->numChannels),
                                     static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, true))
        return false;

    // File decoding occurs outside the lock. Only the quick buffer swap blocks
    // the audio callback, avoiding a race when a sample is dropped during play.
    const juce::ScopedLock lock(sampleLock);
    sampleBuffer = std::move(decoded);
    sampleLength = sampleBuffer.getNumSamples();
    sampleLoaded.store(true);
    playheadPos = 0.0;
    isPlaying = false;
    return true;
}

void SamplerEngine::triggerNote(int midiNote, float velocity)
{
    currentMidiNote = (float)midiNote;
    currentVelocity = velocity / 127.0f;
    playheadPos     = 0.0;
    isPlaying       = true;
}

void SamplerEngine::releaseNote(int /*midiNote*/)
{
    // Simple one-voice: stop on any note-off
    isPlaying = false;
}

void SamplerEngine::processMidi(juce::MidiBuffer& midi,
                                juce::AudioBuffer<float>& outputBuf,
                                const std::array<float, 512>& timingMap,
                                float sidechainPitchHz,
                                float tightness,       // 0.0 – 1.0
                                float harmonicBlend)   // 0.0 – 1.0
{
    outputBuf.clear();
    if (!sampleLoaded.load())
        return;

    // Process MIDI events
    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
            triggerNote(msg.getNoteNumber(), (float)msg.getVelocity());
        else if (msg.isNoteOff())
            releaseNote(msg.getNoteNumber());
    }

    if (!isPlaying)
        return;

    const juce::ScopedLock lock(sampleLock);
    if (sampleLength <= 0 || sampleBuffer.getNumChannels() == 0)
        return;

    // --- Pitch ratio: MIDI note vs root note of sample ---
    float midiRatio = std::pow(2.0f, (currentMidiNote - rootNote) / 12.0f);

    // --- Harmonic blend: shift toward sidechain pitch if > 0 ---
    if (harmonicBlend > 0.0f && sidechainPitchHz > 20.0f)
    {
        float midiHz      = 440.0f * std::pow(2.0f, (currentMidiNote - 69.0f) / 12.0f);
        float blendedHz   = midiHz * (1.0f - harmonicBlend) + sidechainPitchHz * harmonicBlend;
        float rootHz      = 440.0f * std::pow(2.0f, (rootNote - 69.0f) / 12.0f);
        midiRatio         = blendedHz / rootHz;
    }

    auto* outL = outputBuf.getWritePointer(0);
    auto* outR = outputBuf.getNumChannels() > 1 ? outputBuf.getWritePointer(1) : nullptr;

    const auto* srcL = sampleBuffer.getReadPointer(0);
    const auto* srcR = sampleBuffer.getNumChannels() > 1 ? sampleBuffer.getReadPointer(1) : srcL;

    const int numSamples = outputBuf.getNumSamples();
    const int mapSize = static_cast<int>(timingMap.size());

    for (int i = 0; i < numSamples; ++i)
    {
        if ((int)playheadPos >= sampleLength)
        {
            isPlaying = false;
            break;
        }

        // --- DTW Pocket Control: warp playhead with sidechain timing map ---
        // timingMap[i] is 0.0–1.0 onset pull strength from the analyzer
        // tightness scales how hard we snap the playhead forward on transients
        const float timingOffset = i < mapSize
            ? timingMap[static_cast<size_t>(i)]
            : 0.0f;
        double warpDelta    = static_cast<double>(timingOffset) * static_cast<double>(tightness) * 2.0;
        double playbackRate = midiRatio + warpDelta;
        playbackRate        = juce::jmax(0.1, juce::jmin(4.0, playbackRate));  // safety clamp

        // Linear interpolation for sub-sample playback
        int    idx    = (int)playheadPos;
        float  frac   = (float)(playheadPos - idx);
        int    idxN   = juce::jmin(idx + 1, sampleLength - 1);

        float sampleL = srcL[idx] * (1.0f - frac) + srcL[idxN] * frac;
        float sampleR = srcR[idx] * (1.0f - frac) + srcR[idxN] * frac;

        outL[i] = sampleL * currentVelocity;
        if (outR) outR[i] = sampleR * currentVelocity;

        playheadPos += playbackRate;
    }
}
