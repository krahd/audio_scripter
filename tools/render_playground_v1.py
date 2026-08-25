"""Artistic studies using corrected playground v1 semantics.

v1 separates process lifecycle from remembered material. There are no process-specific
history-injection events: remembered material is routed as material through explicit
relations.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import argparse

from playground_v0 import ActivityFollower, Observation, RenderContext, synthetic_source, write_trace_csv, write_wav
from playground_v1 import (
    FREEZE,
    NORMAL,
    SILENT_EVOLUTION,
    TAIL,
    FeedbackProcess,
    MaterialMemory,
    MemoryRecordPolicy,
    ProcessLifecycle,
    ResonantProcess,
    TransformVoice,
    choose_relation,
    constant,
    every,
    memory_only,
    mix_with_memory,
    present,
    section_policy,
    when_activity_above,
)


@dataclass
class MemoryTrack:
    memory: MaterialMemory
    policy: callable


@dataclass
class V1Study:
    name: str
    duration_beats: float
    bpm: float
    source: callable
    voices: list[TransformVoice]
    memories: list[MemoryTrack]
    sample_rate: int = 22050
    beats_per_bar: int = 4


@dataclass
class V1RenderResult:
    samples: list[float]
    trace: list[dict[str, float | int | str]]


def always_record(_ctx, _obs):
    return MemoryRecordPolicy.RECORD


def render_v1(study: V1Study, *, trace_every_beats: float = 0.25) -> V1RenderResult:
    total_seconds = study.duration_beats * 60.0 / study.bpm
    total_samples = int(round(total_seconds * study.sample_rate))
    follower = ActivityFollower(alpha=0.01)
    samples: list[float] = []
    trace: list[dict[str, float | int | str]] = []
    last_lifecycles: dict[str, ProcessLifecycle] = {}
    next_trace_beat = 0.0

    for i in range(total_samples):
        ctx = RenderContext(i, study.sample_rate, study.bpm, study.beats_per_bar)
        source = study.source(ctx)
        obs = Observation(follower.update(source))

        for track in study.memories:
            track.memory.record(source, track.policy(ctx, obs))

        mixed = 0.0
        for voice in study.voices:
            lifecycle = voice.lifecycle(ctx, obs)
            mixed += voice.process_sample(source, ctx, obs)
            previous = last_lifecycles.get(voice.process.identity)
            if previous != lifecycle:
                row: dict[str, float | int | str] = {
                    "event": "lifecycle",
                    "sample": i,
                    "beat": round(ctx.beat, 6),
                    "bar": ctx.bar,
                    "identity": voice.process.identity,
                    "input": lifecycle.input.name,
                    "state": lifecycle.state.name,
                    "output": lifecycle.output.name,
                    "observation": lifecycle.observation.name,
                }
                row.update({f"process_{k}": v for k, v in voice.process.trace_state().items()})
                trace.append(row)
                last_lifecycles[voice.process.identity] = lifecycle

        if ctx.beat >= next_trace_beat:
            row = {
                "event": "checkpoint",
                "sample": i,
                "beat": round(ctx.beat, 6),
                "bar": ctx.bar,
                "source_activity": obs.source_activity,
                "output": mixed,
            }
            for track in study.memories:
                row[f"memory_{track.memory.identity}_samples_seen"] = track.memory.samples_seen
            trace.append(row)
            next_trace_beat += trace_every_beats

        samples.append(mixed)

    return V1RenderResult(samples, trace)


def p1_variant(name: str, variant: str, *, sample_rate: int = 22050) -> V1Study:
    memory = MaterialMemory("source-memory", capacity_samples=sample_rate * 8)
    process = FeedbackProcess("memory-transform", decay=0.992)

    tail_section = every(4, offset_beats=2, length_beats=1, lifecycle=TAIL)
    freeze_section = every(5, offset_beats=1, length_beats=1.5, lifecycle=FREEZE)
    silent_section = every(6, offset_beats=3, length_beats=1, lifecycle=SILENT_EVOLUTION)

    if variant == "a":
        lifecycle = section_policy([
            (0, 16, constant(NORMAL)),
            (16, 32, tail_section),
            (32, 48, freeze_section),
            (48, 64, silent_section),
        ])
        reveal_amount = 0.35
        reveal_ago = 1.0
    elif variant == "b":
        lifecycle = section_policy([
            (0, 16, constant(NORMAL)),
            (16, 28, every(3, offset_beats=1, length_beats=1, lifecycle=TAIL)),
            (28, 44, every(4, offset_beats=0.5, length_beats=2, lifecycle=FREEZE)),
            (44, 64, every(7, offset_beats=2, length_beats=1.5, lifecycle=TAIL)),
        ])
        reveal_amount = 0.7
        reveal_ago = 2.0
    elif variant == "c":
        lifecycle = section_policy([
            (0, 16, constant(NORMAL)),
            (16, 32, every(4, offset_beats=1, length_beats=2, lifecycle=SILENT_EVOLUTION)),
            (32, 48, every(5, offset_beats=0, length_beats=2.5, lifecycle=FREEZE)),
            (48, 64, every(8, offset_beats=2, length_beats=3, lifecycle=SILENT_EVOLUTION)),
        ])
        reveal_amount = 0.9
        reveal_ago = 3.0
    else:
        raise ValueError(variant)

    direct = present()
    remembered = mix_with_memory(memory, beats_ago=reveal_ago, amount=reveal_amount)
    material = choose_relation(lambda ctx, _obs: ctx.beat >= 44, remembered, direct)

    return V1Study(
        name=name,
        duration_beats=64,
        bpm=120,
        source=synthetic_source,
        voices=[TransformVoice(process, lifecycle, material)],
        memories=[MemoryTrack(memory, always_record)],
        sample_rate=sample_rate,
    )


def p2(*, sample_rate: int = 22050) -> V1Study:
    memory = MaterialMemory("shared-source-memory", capacity_samples=sample_rate * 8)
    a = FeedbackProcess("A", decay=0.991)
    b = ResonantProcess("B", frequency=247.0, damping=0.998)

    a_lifecycle = section_policy([
        (0, 16, constant(NORMAL)),
        (16, 32, every(4, offset_beats=2, length_beats=1.5, lifecycle=TAIL)),
        (32, 48, every(5, offset_beats=1, length_beats=2, lifecycle=FREEZE)),
        (48, 64, constant(NORMAL)),
    ])
    b_lifecycle = section_policy([
        (0, 16, constant(SILENT_EVOLUTION)),
        (16, 32, constant(NORMAL)),
        (32, 48, every(3, offset_beats=0.5, length_beats=1.25, lifecycle=SILENT_EVOLUTION)),
        (48, 64, every(6, offset_beats=2, length_beats=1, lifecycle=FREEZE)),
    ])

    a_material = choose_relation(
        lambda ctx, _obs: ctx.beat >= 48,
        mix_with_memory(memory, beats_ago=2.0, amount=0.55),
        present(),
    )
    b_material = choose_relation(
        lambda ctx, _obs: 32 <= ctx.beat < 48,
        memory_only(memory, beats_ago=0.5, gain=0.8),
        present(),
    )

    return V1Study(
        "p2-v1",
        64,
        120,
        synthetic_source,
        [
            TransformVoice(a, a_lifecycle, a_material),
            TransformVoice(b, b_lifecycle, b_material),
        ],
        [MemoryTrack(memory, always_record)],
        sample_rate,
    )


def p3_variant(name: str, variant: str, *, sample_rate: int = 22050) -> V1Study:
    memory = MaterialMemory("responsive-memory", capacity_samples=sample_rate * 8)
    process = ResonantProcess("responsive-transform", frequency=196.0, damping=0.997)

    if variant == "a":
        lifecycle = when_activity_above(0.19, TAIL, otherwise=NORMAL)
        material = mix_with_memory(memory, beats_ago=0.5, amount=lambda _ctx, obs: min(0.8, obs.source_activity * 2.5))
    elif variant == "b":
        lifecycle = when_activity_above(0.19, SILENT_EVOLUTION, otherwise=NORMAL)
        material = mix_with_memory(memory, beats_ago=1.0, amount=0.5)
    elif variant == "c":
        responsive = when_activity_above(0.19, TAIL, otherwise=NORMAL)

        def lifecycle(ctx, obs):
            if (ctx.beat - 6) % 8 < 1.5:
                return FREEZE
            return responsive(ctx, obs)

        material = choose_relation(
            lambda ctx, _obs: int(ctx.beat // 4) % 2 == 1,
            memory_only(memory, beats_ago=1.5, gain=0.85),
            mix_with_memory(memory, beats_ago=0.25, amount=0.3),
        )
    else:
        raise ValueError(variant)

    return V1Study(
        name,
        48,
        120,
        synthetic_source,
        [TransformVoice(process, lifecycle, material)],
        [MemoryTrack(memory, always_record)],
        sample_rate,
    )


def studies(sample_rate: int = 22050) -> list[V1Study]:
    return [
        p1_variant("p1a-v1", "a", sample_rate=sample_rate),
        p1_variant("p1b-v1", "b", sample_rate=sample_rate),
        p1_variant("p1c-v1", "c", sample_rate=sample_rate),
        p2(sample_rate=sample_rate),
        p3_variant("p3a-v1", "a", sample_rate=sample_rate),
        p3_variant("p3b-v1", "b", sample_rate=sample_rate),
        p3_variant("p3c-v1", "c", sample_rate=sample_rate),
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=Path("playground_v1_renders"))
    parser.add_argument("--only", action="append", default=[])
    args = parser.parse_args()

    selected = [s for s in studies() if not args.only or s.name in set(args.only)]
    for study in selected:
        result = render_v1(study)
        write_wav(args.out / f"{study.name}.wav", result.samples, study.sample_rate)
        write_trace_csv(args.out / f"{study.name}.csv", result.trace)
        print(f"{study.name}: {len(result.samples)} samples, {len(result.trace)} trace rows")


if __name__ == "__main__":
    main()
