#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="build"
CONFIG="Release"
JUCE_PATH=""
GENERATOR=""
TARGETS=("audio_scripter_Standalone" "audio_scripter_VST3" "audio_scripter_AU")
INSTALL_PLUGINS=false
PACKAGE=false
RUN_TESTS=false

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --juce-path PATH      Path to local JUCE checkout (optional)
  --build-dir DIR       Build directory (default: build)
  --config CFG          Build config (Debug|Release) (default: Release)
  --generator NAME      CMake generator (e.g. "Xcode", "Ninja")
  --targets "t1 t2"    Space-separated targets to build (default: Standalone, VST3, AU)
  --install             Install built plugins to user plugin folders (macOS)
  --package             Create zip packages for built artifacts
  --tests               Build and run parser tests
  -h, --help            Show this help

Examples:
  bash scripts/build_release.sh --juce-path /path/to/JUCE --config Release --package --tests

EOF
  exit 1
}

if [ "$#" -eq 0 ]; then
  # no args: proceed with defaults
  :
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --juce-path)
      JUCE_PATH="$2"; shift 2;;
    --build-dir)
      BUILD_DIR="$2"; shift 2;;
    --config)
      CONFIG="$2"; shift 2;;
    --generator)
      GENERATOR="$2"; shift 2;;
    --targets)
      IFS=' ' read -r -a TARGETS <<< "$2"; shift 2;;
    --install)
      INSTALL_PLUGINS=true; shift;;
    --package)
      PACKAGE=true; shift;;
    --tests)
      RUN_TESTS=true; shift;;
    -h|--help)
      usage;;
    *)
      echo "Unknown option: $1"; usage;;
  esac
done

echo "Workspace: $WORKSPACE_ROOT"
echo "Build dir: $BUILD_DIR  Config: $CONFIG"
if [ -n "$JUCE_PATH" ]; then echo "Using JUCE at: $JUCE_PATH"; fi
if [ -n "$GENERATOR" ]; then echo "Using CMake generator: $GENERATOR"; fi

mkdir -p "$WORKSPACE_ROOT/$BUILD_DIR"

cmake_args=()
if [ -n "$JUCE_PATH" ]; then
  cmake_args+=("-DAUDIO_SCRIPTER_JUCE_PATH=$JUCE_PATH")
fi
if [ -n "$GENERATOR" ]; then
  cmake_args+=("-G" "$GENERATOR")
fi

echo "Configuring..."
# If cmake_args is non-empty (safe when script is run with set -u), pass them; otherwise call cmake without extras.
if [ "${#cmake_args[@]:-0}" -gt 0 ]; then
  cmake -S "$WORKSPACE_ROOT" -B "$WORKSPACE_ROOT/$BUILD_DIR" "${cmake_args[@]}"
else
  cmake -S "$WORKSPACE_ROOT" -B "$WORKSPACE_ROOT/$BUILD_DIR"
fi

echo "Building targets: ${TARGETS[*]}"
for t in "${TARGETS[@]}"; do
  echo "-- building target: $t"
  cmake --build "$WORKSPACE_ROOT/$BUILD_DIR" --config "$CONFIG" --target "$t"
done

if [ "$RUN_TESTS" = true ]; then
  echo "Building tests..."
  cmake --build "$WORKSPACE_ROOT/$BUILD_DIR" --config "$CONFIG" --target audio_scripter_parser_tests || true
  echo "Running tests..."
  ctest --test-dir "$WORKSPACE_ROOT/$BUILD_DIR" --output-on-failure || true
fi

ARTIFACTS_DIR="$WORKSPACE_ROOT/$BUILD_DIR/audio_scripter_artefacts"

if [ "$INSTALL_PLUGINS" = true ]; then
  echo "Installing plugins to user plugin folders (macOS)..."
  AU_SRC="$ARTIFACTS_DIR/AU"
  VST3_SRC="$ARTIFACTS_DIR/VST3"
  STANDALONE_SRC="$ARTIFACTS_DIR/Standalone"

  AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
  VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"

  if [ -d "$AU_SRC" ]; then
    mkdir -p "$AU_DEST"
    cp -R "$AU_SRC"/*.component "$AU_DEST/" || true
    echo "Copied AU components to $AU_DEST"
  else
    echo "No AU artifacts found in $AU_SRC"
  fi

  if [ -d "$VST3_SRC" ]; then
    mkdir -p "$VST3_DEST"
    cp -R "$VST3_SRC"/*.vst3 "$VST3_DEST/" || true
    echo "Copied VST3 plugins to $VST3_DEST"
  else
    echo "No VST3 artifacts found in $VST3_SRC"
  fi

  if [ -d "$STANDALONE_SRC" ]; then
    echo "Standalone app located in $STANDALONE_SRC"
    echo "You can run it with: open $STANDALONE_SRC"
  fi
fi

if [ "$PACKAGE" = true ]; then
  echo "Packaging artifacts..."
  cd "$ARTIFACTS_DIR" || exit 0
  if [ -d "Standalone" ]; then
    zip_name="$WORKSPACE_ROOT/$BUILD_DIR/audio_scripter-Standalone-$CONFIG.zip"
    zip -r "$zip_name" Standalone || true
    echo "Packaged Standalone -> $zip_name"
  fi
  if [ -d "VST3" ]; then
    zip_name="$WORKSPACE_ROOT/$BUILD_DIR/audio_scripter-VST3-$CONFIG.zip"
    zip -r "$zip_name" VST3 || true
    echo "Packaged VST3 -> $zip_name"
  fi
  if [ -d "AU" ]; then
    zip_name="$WORKSPACE_ROOT/$BUILD_DIR/audio_scripter-AU-$CONFIG.zip"
    zip -r "$zip_name" AU || true
    echo "Packaged AU -> $zip_name"
  fi
fi

echo "Done. Artifacts directory: $ARTIFACTS_DIR"
