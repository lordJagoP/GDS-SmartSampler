#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build/linux-release}"
GENERATOR="${CMAKE_GENERATOR:-Ninja}"

usage() {
    cat <<'USAGE'
Usage: ./build_all.sh [--install-deps] [--clean] [--no-install]

  --install-deps  Install the Kali/Debian JUCE build dependencies with apt.
  --clean         Remove the selected build directory before configuring.
  --no-install    Build without copying SmartSampler.vst3 into ~/.vst3.
USAGE
}

INSTALL_DEPS=0
CLEAN=0
INSTALL_PLUGIN=1

while (($#)); do
    case "$1" in
        --install-deps) INSTALL_DEPS=1 ;;
        --clean) CLEAN=1 ;;
        --no-install) INSTALL_PLUGIN=0 ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ "$(uname -s)" != Linux ]]; then
    printf 'This helper targets Linux. Use CMake directly on Windows or macOS.\n' >&2
    exit 1
fi

if ((INSTALL_DEPS)); then
    command -v sudo >/dev/null || { echo 'sudo is required for --install-deps.' >&2; exit 1; }
    sudo apt update
    sudo apt install -y \
        build-essential git cmake ninja-build pkg-config \
        libasound2-dev libjack-jackd2-dev ladspa-sdk \
        libfreetype6-dev libfontconfig1-dev \
        libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
        libxinerama-dev libxrandr-dev libxrender-dev libxi-dev \
        libglu1-mesa-dev mesa-common-dev libegl-dev
fi

for cmd in cmake git c++; do
    command -v "$cmd" >/dev/null || {
        printf 'Missing required command: %s\nRun: ./build_all.sh --install-deps\n' "$cmd" >&2
        exit 1
    }
done

if [[ "$GENERATOR" == Ninja ]]; then
    command -v ninja >/dev/null || {
        echo 'Ninja is missing. Install ninja-build or set CMAKE_GENERATOR.' >&2
        exit 1
    }
fi

if ((CLEAN)); then
    rm -rf -- "$BUILD_DIR"
fi

printf 'Configuring SmartSampler for %s (%s)\n' "$(uname -m)" "$GENERATOR"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --parallel \
    --target SmartSampler_VST3 SmartSampler_Standalone

VST3_SOURCE="$BUILD_DIR/SmartSampler_artefacts/Release/VST3/SmartSampler.vst3"
STANDALONE="$BUILD_DIR/SmartSampler_artefacts/Release/Standalone/SmartSampler"

[[ -d "$VST3_SOURCE" ]] || { echo "VST3 bundle not found: $VST3_SOURCE" >&2; exit 1; }
[[ -x "$STANDALONE" ]] || { echo "Standalone binary not found: $STANDALONE" >&2; exit 1; }

if ((INSTALL_PLUGIN)); then
    VST3_DEST="$HOME/.vst3"
    install -d "$VST3_DEST"
    rm -rf -- "$VST3_DEST/SmartSampler.vst3"
    cp -a -- "$VST3_SOURCE" "$VST3_DEST/"
    printf 'Installed VST3: %s/SmartSampler.vst3\n' "$VST3_DEST"
fi

printf 'Standalone: %s\n' "$STANDALONE"
printf 'Build complete. Rescan VST3 plug-ins in your DAW.\n'
