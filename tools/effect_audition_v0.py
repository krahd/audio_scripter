"""Minimal offline audition renderer for structured effect-authoring experiments.

Research tooling only. This is deliberately NOT a general DSP runtime and NOT evidence of
artistic quality. It exists so that a concrete Transformation, a concretely abstracted sketch,
and structural variants can process the same recognisable WAV source during authoring studies.

Supported primitives are intentionally small/conventional: gain, saturate, lowpass, delay,
reverb, and serial/parallel structural composition. Additions require an authoring-study need;
this module must not grow into a substitute DSP library.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import argparse
import math
from typing import Iterable, Protocol

from effect_ir_v1 import Transformation
from effect_sketch_v0 import EffectSketch
from playground_sources import read_wav_mono
from playground_v0 import write_wav


class Processor(Protocol):
    def process_sample(self, value: float) -> float: ...


@dataclass
class Gain:
    amount: float

    def process_sample(self, value: float) -> float:
        return value * self.amount


@dataclass
class Saturate:
    drive: float

    def process_sample(self, value: float) -> float:
        # Keep drive=0 meaning close to bypass while allowing deliberate exaggeration.
        gain = 1.0 + max(0.0, self.drive) * 8.0
        normaliser = math.tanh(gain) or 1.0
        return math.tanh(value * gain) / normaliser


@dataclass
class Lowpass:
    cutoff: float
    sample_rate: int
    state: float = 0.0

    def __post_init__(self) -> None:
        cutoff = max(1.0, min(self.cutoff, self.sample_rate * 0.49))
        self.alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / self.sample_rate)

    def process_sample(self, value: float) -> float:
        self.state += self.alpha * (value - self.state)
        return self.state


class Delay:
    def __init__(self, distance_seconds: float, feedback: float, sample_rate: int):
        if distance_seconds < 0:
            raise ValueError("delay distance must be non-negative")
        self.feedback = float(feedback)
        self.size = max(1, int(round(distance_seconds * sample_rate)))
        self.buffer = [0.0] * self.size
        self.index = 0

    def process_sample(self, value: float) -> float:
        delayed = self.buffer[self.index]
        self.buffer[self.index] = value + delayed * self.feedback
        self.index = (self.index + 1) % self.size
        return delayed


class Reverb:
    """Small Schroeder-like audition reverb, chosen for legibility rather than fidelity."""

    def __init__(self, size: float, decay: float, sample_rate: int):
        size = max(0.05, min(float(size), 1.5))
        decay = max(0.0, min(float(decay), 0.98))
        base_ms = [29.7, 37.1, 41.1, 43.7]
        self.combs = [
            Delay((ms * size) / 1000.0, decay, sample_rate)
            for ms in base_ms
        ]
        self.wet = min(0.85, 0.2 + size * 0.35)

    def process_sample(self, value: float) -> float:
        wet = sum(comb.process_sample(value) for comb in self.combs) / len(self.combs)
        return value * (1.0 - self.wet) + wet * self.wet


class Serial:
    def __init__(self, processors: Iterable[Processor]):
        self.processors = tuple(processors)

    def process_sample(self, value: float) -> float:
        for processor in self.processors:
            value = processor.process_sample(value)
        return value


class Parallel:
    def __init__(self, processors: Iterable[Processor]):
        self.processors = tuple(processors)
        if not self.processors:
            raise ValueError("parallel processor requires at least one child")

    def process_sample(self, value: float) -> float:
        return sum(processor.process_sample(value) for processor in self.processors) / len(self.processors)


def _params(transformation: Transformation) -> dict[str, object]:
    if transformation.expr.op != "primitive":
        raise TypeError("parameter extraction requires a primitive Transformation")
    _name, params = transformation.expr.args
    return dict(params)


def compile_processor(transformation: Transformation, *, sample_rate: int) -> Processor:
    expr = transformation.expr

    if expr.op == "compose":
        return Serial(compile_processor(child, sample_rate=sample_rate) for child in expr.args)
    if expr.op == "parallel":
        return Parallel(compile_processor(child, sample_rate=sample_rate) for child in expr.args)
    if expr.op != "primitive":
        raise NotImplementedError(f"audition renderer does not support TransformExpr {expr.op!r}")

    name, _items = expr.args
    params = _params(transformation)

    if name == "gain":
        return Gain(float(params.get("amount", 1.0)))
    if name == "saturate":
        return Saturate(float(params.get("drive", 0.0)))
    if name == "lowpass":
        return Lowpass(float(params.get("cutoff", 1200.0)), sample_rate)
    if name == "delay":
        # The audition subset uses seconds. Higher-level musical-time lowering belongs elsewhere.
        distance = params.get("distance", params.get("seconds", 0.25))
        if not isinstance(distance, (int, float)):
            raise TypeError("audition delay distance must currently be numeric seconds")
        return Delay(float(distance), float(params.get("feedback", 0.35)), sample_rate)
    if name == "reverb":
        return Reverb(
            float(params.get("size", 0.7)),
            float(params.get("decay", 0.75)),
            sample_rate,
        )

    raise NotImplementedError(f"audition renderer has no primitive {name!r}")


def render_samples(
    transformation: Transformation,
    source: Iterable[float],
    *,
    sample_rate: int,
    tail_seconds: float = 0.0,
) -> list[float]:
    if tail_seconds < 0:
        raise ValueError("tail_seconds must be non-negative")
    processor = compile_processor(transformation, sample_rate=sample_rate)
    output = [processor.process_sample(float(sample)) for sample in source]
    tail_samples = int(round(tail_seconds * sample_rate))
    output.extend(processor.process_sample(0.0) for _ in range(tail_samples))
    return output


def render_sketch(
    sketch: EffectSketch,
    source: Iterable[float],
    *,
    sample_rate: int,
    bindings: dict[str, object] | None = None,
    tail_seconds: float = 0.0,
) -> list[float]:
    transformation = sketch.instantiate(bindings or {})
    return render_samples(
        transformation,
        source,
        sample_rate=sample_rate,
        tail_seconds=tail_seconds,
    )


def render_wav(
    transformation: Transformation,
    input_path: str | Path,
    output_path: str | Path,
    *,
    tail_seconds: float = 1.0,
) -> None:
    source, sample_rate = read_wav_mono(input_path)
    result = render_samples(
        transformation,
        source,
        sample_rate=sample_rate,
        tail_seconds=tail_seconds,
    )
    write_wav(output_path, result, sample_rate)


def main() -> None:
    parser = argparse.ArgumentParser(description="Audition a tiny conventional effect subset")
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--delay", type=float, default=0.24, help="delay seconds")
    parser.add_argument("--feedback", type=float, default=0.55)
    parser.add_argument("--reverb-size", type=float, default=0.65)
    parser.add_argument("--reverb-decay", type=float, default=0.72)
    parser.add_argument("--drive", type=float, default=0.15)
    parser.add_argument("--tail", type=float, default=2.0)
    args = parser.parse_args()

    effect = Transformation(
        name="audition",
        expr=__import__("effect_ir_v1").TransformExpr(
            "compose",
            (
                __import__("effect_ir_v1").primitive(
                    "delay", distance=args.delay, feedback=args.feedback
                ),
                __import__("effect_ir_v1").primitive(
                    "reverb", size=args.reverb_size, decay=args.reverb_decay
                ),
                __import__("effect_ir_v1").primitive("saturate", drive=args.drive),
            ),
        ),
    )
    render_wav(effect, args.input, args.output, tail_seconds=args.tail)


if __name__ == "__main__":
    main()
