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
