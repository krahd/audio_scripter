# Roadmap (post-1.0)

## Completed in 1.0

- Lock-free script swap runtime (atomic compiled snapshots).
- Scriptable macro controls (`p1..p8`) with DAW automation.
- Script editor workflow with save/load/examples/help.
- Expanded creative primitives (`fold`, `clip`, `crush`, `lpf1`, `slew`).
- Offline-friendly JUCE path via CMake option.

## Next milestones

1. **Bytecode VM backend**
   - compile AST into lightweight opcodes
   - instruction budget per sample for stronger realtime guarantees

2. **DSP expansion**
   - delay lines
   - biquad primitives
   - envelope followers
   - random/noise functions

3. **Editor UX**
   - syntax highlighting
   - inline parse errors
   - built-in preset browser with tagging

4. **Testing and release hardening**
   - parser unit tests
   - deterministic DSP golden tests
   - CI for multi-platform builds
