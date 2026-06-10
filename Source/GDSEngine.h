#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// GDS Audio Engine v1.0
// Golden Digital Services
// Foundation layer for all GDS plugins
//==============================================================================

class GDSEngine
{
public:
    enum class TriggerMode
    {
        AudioAuto,      // fires from incoming audio energy  no MIDI needed
        MidiGate,       // fires from MIDI note on
        Hybrid          // both simultaneously
    };

    enum class DecayMode
    {
        NaturalTail,    // sample plays out its full tail
        GatedSilence,   // hard cut when audio drops below threshold
        HoldDrone       // holds last note until new phrase detected
    };

    struct SoloistState
    {
        bool     isPlaying       = false;
        bool     phraseDetected  = false;
        float    detectedPitch   = 0.0f;
        float    detectedEnergy  = 0.0f;
        int      phraseLength    = 0;
        double   playheadPos     = 0.0;
        float    currentNote     = 60.0f;
        float    velocity        = 1.0f;
    };

    GDSEngine();
    void prepare(double sampleRate, int blockSize);

    // Universal format loader  WAV, AIF, MP3, ADG
    bool loadFile(const juce::File& file);
    bool isSampleLoaded()      const { return sampleLoaded; }
    juce::String getFileName() const { return loadedFileName; }

    // Core processing
    void processBlock(const juce::AudioBuffer<float>& inputBuf,
                      juce::AudioBuffer<float>& outputBuf,
                      juce::MidiBuffer& midiMessages);

    // Config
    void setTriggerMode(TriggerMode m)  { triggerMode = m; }
    void setDecayMode(DecayMode m)      { decayMode   = m; }
    void setThreshold(float t)          { threshold   = t; }
    void setTightness(float t)          { tightness   = t; }
    void setHarmonicBlend(float b)      { harmonicBlend = b; }
    void setRootNote(float n)           { rootNote    = n; }

    SoloistState getState() const { return state; }

private:
    // --- Audio analysis ---
    void   analyzeInput(const juce::AudioBuffer<float>& buf);
    float  detectEnergy(const juce::AudioBuffer<float>& buf);
    float  detectPitch(const juce::AudioBuffer<float>& buf);
    bool   detectPhrase(float energy);
    void   detectOnset(const juce::AudioBuffer<float>& buf);

    // --- Playback ---
    void   triggerVoice(float pitch, float velocity);
    void   renderOutput(juce::AudioBuffer<float>& outputBuf);

    // --- ADG parser ---
    juce::File resolveADGSample(const juce::File& adgFile);

    // State
    SoloistState state;
    TriggerMode  triggerMode  = TriggerMode::AudioAuto;
    DecayMode    decayMode    = DecayMode::NaturalTail;

    // Parameters
    float threshold     = 0.02f;   // audio energy trigger level
    float tightness     = 0.75f;
    float harmonicBlend = 0.25f;
    float rootNote      = 60.0f;

    // Sample data
    juce::AudioBuffer<float>  sampleBuffer;
    juce::AudioFormatManager  formatManager;
    bool                      sampleLoaded   = false;
    juce::String              loadedFileName;
    int                       sampleLength   = 0;

    // Envelope followers
    float envFast       = 0.0f;
    float envSlow       = 0.0f;
    float envRelease    = 0.0f;

    // Phrase detection state
    float  lastEnergy        = 0.0f;
    int    silenceCounter    = 0;
    int    phraseCounter     = 0;
    bool   inPhrase          = false;
    int    minSilenceFrames  = 20;   // frames of silence before phrase ends

    // Onset / timing map
    std::array<float, 512> timingMap{};
    float  onsetStrength   = 0.0f;
    bool   onsetDetected   = false;
    float  currentPitchHz  = 0.0f;

    // FFT for onset detection
    juce::dsp::FFT         fft;
    std::vector<float>     fftBuffer;
    std::vector<float>     windowData;
    std::vector<float>     prevMagnitude;

    double sampleRate  = 44100.0;
    int    blockSize   = 512;
};
