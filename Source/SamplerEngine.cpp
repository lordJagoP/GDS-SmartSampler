#include "SamplerEngine.h"
#include <cmath>

SamplerEngine::SamplerEngine()
{
    formatManager.registerBasicFormats();
}

void SamplerEngine::prepare(double sr, int blockSz)
{
    const juce::ScopedLock lock(sampleLock);
    sampleRate = sr;
    tailSeconds.store(static_cast<double>(sampleLength) / (sampleRate * 0.1));
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
    sourceSampleRate = reader->sampleRate;
    tailSeconds.store(static_cast<double>(sampleLength) / (sampleRate * 0.1));
    sampleLoaded.store(true);
    playheadPos = 0.0;
    isPlaying = false;
    return true;
}

void SamplerEngine::triggerNote(int midiNote, float velocity)
{
    currentMidiNote = (float)midiNote;
    currentVelocity = juce::jlimit(0.0f, 1.0f, velocity);
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
                                float harmonicBlend,
                                float outputGain,
                                bool clearOutput)
{
    if (clearOutput)
        outputBuf.clear();
    if (outputBuf.getNumChannels() == 0 || outputBuf.getNumSamples() == 0)
        return;

    // All playback-state reads/writes share the sample-swap lock.
    const juce::ScopedLock lock(sampleLock);
    if (!sampleLoaded.load() || sampleLength <= 0 || sampleBuffer.getNumChannels() == 0)
        return;

    auto* outL = outputBuf.getWritePointer(0);
    auto* outR = outputBuf.getNumChannels() > 1 ? outputBuf.getWritePointer(1) : nullptr;

    const auto* srcL = sampleBuffer.getReadPointer(0);
    const auto* srcR = sampleBuffer.getNumChannels() > 1 ? sampleBuffer.getReadPointer(1) : srcL;

    const int numSamples = outputBuf.getNumSamples();
    const int mapSize = static_cast<int>(timingMap.size());

    const auto render = [&](int start, int end)
    {
        if (!isPlaying)
            return;
        float midiRatio = std::pow(2.0f, (currentMidiNote - rootNote) / 12.0f);
        if (harmonicBlend > 0.0f && sidechainPitchHz > 20.0f)
        {
            const float midiHz = 440.0f * std::pow(2.0f, (currentMidiNote - 69.0f) / 12.0f);
            const float blendedHz = midiHz * (1.0f - harmonicBlend) + sidechainPitchHz * harmonicBlend;
            const float rootHz = 440.0f * std::pow(2.0f, (rootNote - 69.0f) / 12.0f);
            midiRatio = blendedHz / rootHz;
        }
        for (int i = start; i < end; ++i)
        {
            if ((int)playheadPos >= sampleLength)
            {
                isPlaying = false;
                break;
            }
            const float timingOffset = i < mapSize
                ? timingMap[static_cast<size_t>(i)] : 0.0f;
            const double warpDelta = static_cast<double>(timingOffset) * tightness * 2.0;
            const double playbackRate = juce::jlimit(0.1, 4.0,
                (midiRatio + warpDelta) * sourceSampleRate / sampleRate);
            const int idx = static_cast<int>(playheadPos);
            const float frac = static_cast<float>(playheadPos - idx);
            const int idxN = juce::jmin(idx + 1, sampleLength - 1);
            const float sampleL = srcL[idx] * (1.0f - frac) + srcL[idxN] * frac;
            const float sampleR = srcR[idx] * (1.0f - frac) + srcR[idxN] * frac;
            outL[i] += sampleL * currentVelocity * outputGain;
            if (outR)
                outR[i] += sampleR * currentVelocity * outputGain;
            playheadPos += playbackRate;
        }
    };
    int cursor = 0;
    for (const auto meta : midi)
    {
        const int position = juce::jlimit(cursor, numSamples, meta.samplePosition);
        render(cursor, position);
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            triggerNote(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            releaseNote(msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            releaseNote(0);
        cursor = position;
    }
    render(cursor, numSamples);
}
