#!/usr/bin/env bash
set -euo pipefail

# install.sh — copy built .vst3 plugin from this repo's build/ to macOS VST3 folder
# Usage:
#   ./install.sh                    # install to ~/Library/Audio/Plug-Ins/VST3
#   sudo ./install.sh --system     # install to /Library/Audio/Plug-Ins/VST3
#   ./install.sh --src path/to/My.vst3 --dest /some/other/dir

repo_root="$(cd "$(dirname "$0")" && pwd)"
build_dir="$repo_root/build"

print_usage() {
  cat <<EOF
Usage: $(basename "$0") [--system] [--src PATH] [--dest PATH] [SRC] [DEST]

By default installs the first *.vst3 found under $build_dir to:
  $HOME/Library/Audio/Plug-Ins/VST3
Use --system or run as root to install to /Library/Audio/Plug-Ins/VST3
EOF
  exit 1
}


SYSTEM=0
SRC_ARG=""
DEST_ARG=""
MAKE_BACKUP=0


while [[ $# -gt 0 ]]; do
  case "$1" in
    --system) SYSTEM=1; shift ;;
    --src) SRC_ARG="$2"; shift 2 ;;
    --dest) DEST_ARG="$2"; shift 2 ;;
    -b) MAKE_BACKUP=1; shift ;;
    -h|--help) print_usage ;;
    *) if [[ -z "$SRC_ARG" ]]; then SRC_ARG="$1"; else DEST_ARG="$1"; fi; shift ;;
  esac
done

# locate source plugin
if [[ -n "$SRC_ARG" ]]; then
  src="$SRC_ARG"
else
  if [[ ! -d "$build_dir" ]]; then
    echo "Build directory not found: $build_dir" >&2
    exit 1
  fi
  vst3s=()
  while IFS= read -r -d '' file; do
    vst3s+=("$file")
  done < <(find "$build_dir" -type d -name '*.vst3' -print0 2>/dev/null)
  if [[ ${#vst3s[@]} -eq 0 ]]; then
    echo "No .vst3 bundles found under $build_dir" >&2
    exit 1
  elif [[ ${#vst3s[@]} -eq 1 ]]; then
    src="${vst3s[0]}"
  else
    echo "Multiple .vst3 bundles found:"
    for i in "${!vst3s[@]}"; do
      printf "  %d) %s\n" "$((i+1))" "${vst3s[$i]}"
    done
    if [[ -t 0 ]]; then
      read -p "Choose number to install [1]: " sel
      sel=${sel:-1}
      if ! [[ "$sel" =~ ^[0-9]+$ ]] || ((sel < 1 || sel > ${#vst3s[@]})); then
        echo "Invalid selection" >&2
        exit 1
      fi
      src="${vst3s[$((sel-1))]}"
    else
      src="${vst3s[0]}"
      echo "Non-interactive shell; defaulting to first: $src"
    fi
  fi
fi

src="$(cd "$src" && pwd)"
plugin_name="$(basename "$src")"

if [[ -n "$DEST_ARG" ]]; then
  dest="$DEST_ARG"
elif [[ "$SYSTEM" -eq 1 ]] || [[ "${EUID-$(id -u)}" -eq 0 ]]; then
  dest="/Library/Audio/Plug-Ins/VST3"
else
  dest="$HOME/Library/Audio/Plug-Ins/VST3"
fi

mkdir -p "$dest"


# Only make a backup if -b was specified
if [[ $MAKE_BACKUP -eq 1 && -e "$dest/$plugin_name" ]]; then
  backup="$dest/${plugin_name}.bak.$(date +%Y%m%dT%H%M%S)"
  echo "Backing up existing plugin to $backup"
  mv "$dest/$plugin_name" "$backup"
fi

echo "Copying $src -> $dest/$plugin_name"
if command -v ditto >/dev/null 2>&1; then
  ditto "$src" "$dest/$plugin_name"
else
  cp -R "$src" "$dest/$plugin_name"
fi

echo "Installed $plugin_name to $dest"

exit 0
