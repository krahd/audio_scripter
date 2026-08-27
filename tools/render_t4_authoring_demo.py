"""Render the first structured-authoring T4 workflow on recognisable audio.

This is a WORKFLOW DEMONSTRATION, not an artistic evaluation and not evidence that any
preselected variant is interesting. It exercises one concrete -> abstract -> compare ->
concretise -> abstract-again trajectory using the same E-piano source already stored in the
repository's Ableton test project.

The point is to verify that the structured authoring representation, textual projection,
structural diff, and audition renderer remain connected to the same semantic objects.
"""

from __future__ import annotations

from pathlib import Path
import argparse

from effect_audition_v0 import render_samples, render_sketch
from effect_ir_v1 import compose, parallel, primitive
from effect_sketch_v0 import EffectSketch
from effect_surface_v0 import (
    render_sketch as render_sketch_text,
    render_structural_diff,
    render_transformation,
    render_variants,
)
from playground_sources import read_wav_mono
from playground_v0 import write_wav


def _write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.rstrip() + "\n", encoding="utf-8")


def _render(path: Path, transformation, source, sample_rate, *, tail: float = 2.0) -> list[float]:
    audio = render_samples(
        transformation,
        source,
        sample_rate=sample_rate,
        tail_seconds=tail,
    )
    write_wav(path, audio, sample_rate)
    return audio


def render_demo(input_path: Path, out_dir: Path) -> None:
    source, sample_rate = read_wav_mono(input_path)
    if not source:
        raise ValueError("input WAV is empty")
    out_dir.mkdir(parents=True, exist_ok=True)

    write_wav(out_dir / "00-dry.wav", source, sample_rate)

    concrete = compose(
        primitive("delay", distance=0.23, feedback=0.55, mix=0.40),
        primitive("reverb", size=0.65, decay=0.72),
        primitive("saturate", drive=0.12),
        name="starting-effect",
    )
    concrete_audio = _render(out_dir / "01-concrete.wav", concrete, source, sample_rate)
    _write_text(
        out_dir / "01-concrete.txt",
        "effect starting_effect(in):\n"
        f"    in -> {render_transformation(concrete)} -> out\n",
    )

    # Concrete abstraction: the existing reverb becomes an authoring-time variation point.
    # Its exact discovered structure is retained as the default.
    sketch = EffectSketch.from_transformation(concrete).abstract_transformation((1,), "space")
    default_audio = render_sketch(
        sketch,
        source,
        sample_rate=sample_rate,
        tail_seconds=2.0,
    )
    if concrete_audio != default_audio:
        raise AssertionError("concrete abstraction changed the auditioned audio")
    write_wav(out_dir / "02-abstracted-default.wav", default_audio, sample_rate)
    _write_text(out_dir / "02-abstracted.txt", render_sketch_text(sketch, name="starting_effect"))

    alternatives = [
        (
            "tight",
            primitive("reverb", size=0.25, decay=0.45),
        ),
        (
            "memory-space",
            compose(
                primitive("delay", distance=0.07, feedback=0.72, mix=0.65),
                primitive("reverb", size=1.0, decay=0.85),
                name="memory-space",
            ),
        ),
        (
            "split-space",
            parallel(
                primitive("reverb", size=0.95, decay=0.85),
                compose(
                    primitive("lowpass", cutoff=650.0),
                    primitive("reverb", size=0.45, decay=0.75),
                    name="dark-branch",
                ),
                name="split-space",
            ),
        ),
    ]

    _write_text(
        out_dir / "03-space-variants.txt",
        render_variants(sketch, "space", alternatives),
    )

    variant_map = dict(alternatives)
    for index, (label, alternative) in enumerate(alternatives, start=1):
        variant = sketch.instantiate(space=alternative)
        _render(
            out_dir / f"03-{index}-{label}.wav",
            variant,
            source,
            sample_rate,
        )

    # This selection is only a deterministic demo path. It is NOT an artistic preference.
    chosen = sketch.instantiate(space=variant_map["memory-space"])
    _write_text(
        out_dir / "04-demo-choice.txt",
        "DEMONSTRATION CHOICE ONLY — not an artistic judgement.\n\n"
        f"chosen concrete effect:\n    {render_transformation(chosen)}\n\n"
        "structural change from the starting effect:\n"
        f"{render_structural_diff(concrete, chosen)}\n",
    )
    _render(out_dir / "04-demo-choice.wav", chosen, source, sample_rate)

    # After choosing/concretising one branch, discover another dimension and abstract it.
    # Here the original outer delay feedback is turned into an authoring variation point.
    recurrence_sketch = (
        EffectSketch.from_transformation(chosen)
        .abstract_value((0,), "feedback", "recurrence")
    )
    _write_text(
        out_dir / "05-second-abstraction.txt",
        render_sketch_text(recurrence_sketch, name="derived_effect"),
    )

    recurrence_values = [("less", 0.30), ("default", 0.55), ("more", 0.82)]
    _write_text(
        out_dir / "05-recurrence-variants.txt",
        render_variants(recurrence_sketch, "recurrence", recurrence_values),
    )
    for index, (label, value) in enumerate(recurrence_values, start=1):
        variant = recurrence_sketch.instantiate(recurrence=value)
        _render(
            out_dir / f"05-{index}-recurrence-{label}.wav",
            variant,
            source,
            sample_rate,
        )

    _write_text(
        out_dir / "README.txt",
        "T4 structured effect-authoring workflow demonstration\n"
        "===================================================\n\n"
        "This package is NOT an artistic listening study. The variants were selected in\n"
        "advance only to exercise the authoring machinery on recognisable musical audio.\n"
        "Do not infer that any variant is useful, expressive, original, or preferable.\n\n"
        "Sequence:\n"
        "00 dry source\n"
        "01 concrete conventional effect\n"
        "02 same effect after post-hoc abstraction of its space stage; audio must be identical\n"
        "03 three explicit structural alternatives for that stage\n"
        "04 deterministic demonstration choice concretised back into an effect\n"
        "05 second post-hoc abstraction and three recurrence values\n\n"
        "Text files show the provisional textual projection and structural differences.\n"
        "The useful future experiment is interactive: the author chooses what to abstract and\n"
        "what to try next based on what they hear.\n",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Render T4 effect-authoring workflow demo")
    parser.add_argument("input", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    render_demo(args.input, args.out)


if __name__ == "__main__":
    main()
