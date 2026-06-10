#!/usr/bin/env bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
JUCE_DIR="$PROJECT_DIR/JUCE"
BUILD_DIR="$PROJECT_DIR/buildV3"

echo "======================================"
echo " SmartSampler VST3 Build Script"
echo " Platform: $(uname -s)"
echo "======================================"

# --- 1. Clone JUCE if not present ---
if [ ! -d "$JUCE_DIR" ]; then
    echo "[1/4] Cloning JUCE..."
    git clone --depth 1 https://github.com/juce-framework/JUCE.git "$JUCE_DIR"
else
    echo "[1/4] JUCE already present — skipping clone."
fi

# --- 2. Patch CMakeLists.txt to use local JUCE path ---
sed -i.bak "s|add_subdirectory(/path/to/JUCE JUCE)|add_subdirectory(JUCE JUCE)|g" "$PROJECT_DIR/CMakeLists.txt"
echo "[2/4] CMakeLists.txt patched."

# --- 3. Configure ---
echo "[3/4] Configuring with CMake into buildV3..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

# --- 4. Build ---
echo "[4/4] Building VST3..."
cmake --build "$BUILD_DIR" --config Release --parallel

# --- 5. Install VST3 ---
echo ""
echo "======================================"
echo " Build complete!"
echo "======================================"

if [[ "$OSTYPE" == "darwin"* ]]; then
    VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$VST3_DEST"
    cp -r "$BUILD_DIR/SmartSampler_artefacts/Release/VST3/SmartSampler.vst3" "$VST3_DEST/"
    echo " Installed to: $VST3_DEST/SmartSampler.vst3"
else
    VST3_DEST="$HOME/.vst3"
    mkdir -p "$VST3_DEST"
    cp -r "$BUILD_DIR/SmartSampler_artefacts/Release/VST3/SmartSampler.vst3" "$VST3_DEST/"
    echo " Installed to: $VST3_DEST/SmartSampler.vst3"
fi

echo " Restart your DAW and scan for new plugins."
