# Research notes (April 23, 2026)

## Why JUCE

JUCE documentation/tutorials indicate straightforward multi-format plugin generation from one codebase, including VST3 and AU, which matches the project goals.

References consulted:
- JUCE Features page (plugin format support).
- JUCE basic plugin tutorial (VST3/AU support in recent JUCE).

## Format/licensing considerations

The Steinberg VST3 Developer Portal documents licensing details and notes around VST SDK licensing changes, including MIT-licensed VST 3 SDK updates.

For commercial release and trademark/usage specifics, always review latest Steinberg terms at release time.

## Real-time constraints

JUCE community + documentation consistently emphasize that audio-thread code should avoid blocking calls, memory allocation, and locks.

For this prototype, script compilation happens outside audio processing, but script application currently uses a simple lock for safety of state mutation. A lock-free swap design is recommended in follow-up iterations.

## Chosen scripting direction

A compact language with expression assignments and audio-centric built-ins was selected because it is:
- easier to learn than embedding a full general-purpose language,
- easier to constrain for deterministic DSP,
- simpler to harden incrementally (parser, VM, instruction limits).

## References

- https://juce.com/juce/features/
- https://juce.com/tutorials/tutorial_create_projucer_basic_plugin
- https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/VST3%2BLicense.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/What%2Bare%2Bthe%2Blicensing%2Boptions.html
- https://juce.com/forum/ (real-time safety discussions)
