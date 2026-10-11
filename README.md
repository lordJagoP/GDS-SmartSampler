# GDS SmartSampler

A JUCE-based VST3 and standalone sampler from Golden Digital Services. The same
C++ source builds on Windows and Linux; the Linux helper is designed for
Kali/Debian-based systems.

## Kali Linux build

```bash
git clone https://github.com/lordJagoP/GDS-SmartSampler.git
cd GDS-SmartSampler/Source
chmod +x build_all.sh
./build_all.sh --install-deps
```

The script builds both formats and installs the VST3 bundle to:

```text
~/.vst3/SmartSampler.vst3
```

The standalone executable is written to:

```text
Source/build/linux-release/SmartSampler_artefacts/Release/Standalone/SmartSampler
```

To rebuild cleanly without installing the plug-in:

```bash
./build_all.sh --clean --no-install
```

## Manual CMake build

```bash
cmake -S Source -B Source/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build Source/build/release --parallel \
  --target SmartSampler_VST3 SmartSampler_Standalone
```

CMake uses a local `Source/JUCE` checkout when one exists. Otherwise it fetches
the pinned JUCE release declared in `Source/CMakeLists.txt`.

## Windows build

Install Visual Studio with the Desktop development with C++ workload, Git, and
CMake, then run from a Developer PowerShell:

```powershell
cmake -S Source -B Source/build/windows -A x64
cmake --build Source/build/windows --config Release `
  --target SmartSampler_VST3 SmartSampler_Standalone
```

## DAW setup

Rescan VST3 plug-ins after installation. SmartSampler is an instrument with MIDI
input, stereo output, and an optional mono/stereo sidechain input. Drop a WAV or
AIFF file onto its editor, route MIDI to it, and optionally route a sidechain
signal for timing and pitch analysis.

## Audio-effect build for Windows 11

The additional **SmartSampler FX** target is an audio effect, not an instrument.
It has a separate plug-in ID, matching mono/stereo main input and output, an
optional mono/stereo sidechain, and optional MIDI input. The original
SmartSampler instrument remains available with its existing identity.

From Developer PowerShell with Visual Studio C++ tools and CMake installed:

```powershell
.\tools\Build-Windows.ps1
```

The script builds x64 VST3 and standalone versions of both products and runs
processor regression tests. To install the FX bundle, close your DAWs and run
the script with `-Install` from an administrator Developer PowerShell. It
refuses to overwrite an existing FX installation.

The Windows FX bundle is:

```text
Source\build\windows-fx\SmartSamplerFX_artefacts\Release\VST3\SmartSampler FX.vst3
```

Copy the entire `.vst3` bundle to `C:\Program Files\Common Files\VST3`, not just
its internal binary. Windows VST3 builds cannot be replaced by Linux `.so`
files or the standalone `.exe`; changing an extension does not convert formats.
Ableton supports 64-bit VST3 on Windows and documents this installation path:
[Ableton Windows VST setup](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows).
Fender Studio Pro supports VST3 as well:
[Fender plug-in management](https://fenderstudiopromanual.fender.com/en/Content/Setup_Topics/Managing_Your_Content.htm).

### Ableton Live

Enable **Use VST3 Plug-In System Folder** under Preferences/Settings > Plug-Ins,
rescan, then insert **SmartSampler FX** on an audio track or after an instrument.
These VST3 settings are documented in
[Ableton's setup guide](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows).
Drop a WAV/AIFF file onto the editor and play the audio track. The FX version
does not require a MIDI track for audio triggering.

### Fender Studio Pro

Restart/rescan after installing the FX bundle. Fender documents plug-in
locations under **Options > Locations > VST Plug-ins** and notes that VST3 uses
its OS preset path:
[Fender content management](https://fenderstudiopromanual.fender.com/en/Content/Setup_Topics/Managing_Your_Content.htm).
Insert **SmartSampler FX** in the track's audio-effect insert chain, not its
instrument slot. Drop a sample into its editor and play the source audio.

### FX controls and current limitations

- **Dry**: original track level, default 1.0.
- **Sample Level**: added sampler level, default 0.5. Start low to avoid clipping.
- **Trigger dB**: audio gate threshold, default -24 dBFS. Raise it to reduce
  unwanted triggers; lower it for quiet source audio.
- **Tightness / Harmonic Blend**: timing warp and pitch-following controls.
  Set both to zero when first troubleshooting.
- **Sidechain**: when enabled, it supplies analysis and triggers instead of the
  main input. Without a sidechain, the main track supplies them.
- **Trigger behavior**: fixed-root (MIDI note 60), single-voice one-shot playback,
  with hysteresis and a minimum 50 ms interval between automatic triggers.
  This is audio-triggered sample layering/replacement, not a general-purpose
  continuous pitch/time processor for the original audio.
- **Session recall**: parameters are saved, but the loaded sample is not yet
  embedded or restored from a path. Reload the sample when reopening a session.
- **Host validation**: builds and processor tests do not substitute for testing
  scanning, playback, automation, sidechain routing and project recall in both DAWs.
