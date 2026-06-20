#pragma once

inline constexpr int kNumMacros = 8;

// Canonical version lives in CMakeLists.txt (project(... VERSION ...)), which
// passes -DAUDIO_SCRIPTER_VERSION_STRING to every target. This fallback is only
// used for non-CMake builds; keep it in sync with CMakeLists.txt.
#ifndef AUDIO_SCRIPTER_VERSION_STRING
#define AUDIO_SCRIPTER_VERSION_STRING "0.0.13"
#endif
