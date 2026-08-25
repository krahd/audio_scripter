# Transformation Playground

This branch contains temporary offline research playgrounds. They do **not** define the public `.ascr` language and do not modify the C++ runtime.

## Current preferred model

Use **v1**. It separates persistent transformations from remembered material.

```bash
python tools/render_playground_v1.py --out playground_v1_renders
```

This renders:

- `p1a-v1`, `p1b-v1`, `p1c-v1` — familiar memory-effect forms that diverge through transformation lifecycle + remembered material;
- `p2-v1` — two persistent transformations with divergent trajectories and a shared material memory;
- `p3a-v1`, `p3b-v1`, `p3c-v1` — activity-responsive memory/transformation studies.

Each render produces a WAV file and a CSV trace.

## Use a real recording

The same studies can process a PCM WAV source instead of the deterministic synthetic source:

```bash
python tools/render_playground_v1.py \
  --input /path/to/source.wav \
  --out playground_v1_real_source
```

The input is mixed to mono, linearly resampled if necessary, and looped for the study duration. This path is intended for formative artistic testing with guitar, voice, field recordings or musical material, not production-quality sample-rate conversion.

Render selected studies with repeated `--only`:

```bash
python tools/render_playground_v1.py \
  --input /path/to/source.wav \
  --only p1a-v1 \
  --only p1b-v1 \
  --only p1c-v1 \
  --out p1_listening
```

## Tests

```bash
python -m pytest -q \
  tests_python/test_lifecycle_spike.py \
  tests_python/test_playground_v0.py \
  tests_python/test_playground_v1.py
```

## Historical v0

`playground_v0.py`, `playground_v0_events.py`, and `render_playground_studies.py` preserve the earlier model in which history was treated as a universal lifecycle dimension. That design exposed a semantic inconsistency and is retained as research evidence rather than silently rewritten.

## Listening order

Start with:

1. `p1a-v1.wav`
2. `p1b-v1.wav`
3. `p1c-v1.wav`

Before changing code, ask whether the three forms create a desire to manipulate the transformation further and whether that desire is naturally described in terms of transformation, remembered material and musical time rather than buffers/routing/bypass machinery.

The private research workspace contains the listening/session protocol and must remain the canonical location for unpublished interpretations and novelty judgements.
