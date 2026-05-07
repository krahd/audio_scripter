#!/usr/bin/env bash
set -euo pipefail

# Helper: build release, install VST3, quit Ableton Live, open test.als
repo_root="$(cd "$(dirname "$0")/.." && pwd)"

echo "Running Release build..."
bash "$repo_root/scripts/build_release.sh" --config Release

echo "Installing plugin..."
bash "$repo_root/install.sh"

echo "Attempting to quit Ableton Live (if running)..."
osascript -e 'tell application "Ableton Live 11 Suite" to quit' >/dev/null 2>&1 || true
osascript -e 'tell application "Ableton Live" to quit' >/dev/null 2>&1 || true

als_path="$repo_root/test Project/test.als"
if [[ -f "$als_path" ]]; then
  echo "Opening $als_path"
  open "$als_path"
else
  echo "Warning: $als_path not found" >&2
fi

exit 0
