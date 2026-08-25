"""Playable offline playground for transformation-composition research.

This is a temporary construction API over experimental semantics. It is not the
public .ascr language, and names here are not proposed language syntax.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Protocol
import csv
import math
import struct
import wave

from lifecycle_spike import (
    FREEZE,
    HISTORY_ONLY,
    NORMAL,
    SILENT_EVOLUTION,
    TAIL,
    HistoryPolicy,
    InputPolicy,
    Lifecycle,
    ObservationPolicy,
    OutputPolicy,
    StatePolicy,
)


@dataclass(frozen=True)
class RenderContext:
    sample_index: int
    sample_rate: int
    bpm: float
    beats_per_bar: int = 4

    @property
    def seconds(self) -> float:
        return self.sample_index / self.sample_rate

    @property
    def beat(self) -> float:
        return self.seconds * self.bpm / 60.0

    @property
    def beat_index(self) -> int:
        return int(math.floor(self.beat))

    @property
    def beat_phase(self) -> float:
        return self.beat - self.beat_index

    @property
    def bar(self) -> int:
        return self.beat_index // self.beats_per_bar

    @property
    def beat_in_bar(self) -> int:
        return self.beat_index % self.beats_per_bar


@dataclass(frozen=True)
class Observation:
    source_activity: float = 0.0


Policy = Callable[[RenderContext, Observation], Lifecycle]


def constant(lifecycle: Lifecycle) -> Policy:
    return lambda _ctx, _obs: lifecycle


def during(
    start_beat: float,
    length_beats: float,
    lifecycle: Lifecycle,
    *,
    otherwise: Lifecycle = NORMAL,
) -> Policy:
    end = start_beat + length_beats

    def policy(ctx: RenderContext, _obs: Observation) -> Lifecycle:
        return lifecycle if start_beat <= ctx.beat < end else otherwise

    return policy


def every(
    period_beats: float,
    *,
    offset_beats: float = 0.0,
    length_beats: float = 1.0,
    lifecycle: Lifecycle,
    otherwise: Lifecycle = NORMAL,
) -> Policy:
    if period_beats <= 0 or length_beats < 0:
        raise ValueError("period_beats must be positive and length_beats non-negative")

    def policy(ctx: RenderContext, _obs: Observation) -> Lifecycle:
        local = (ctx.beat - offset_beats) % period_beats
        return lifecycle if local < length_beats else otherwise

    return policy


def when_activity_above(
    threshold: float,
    lifecycle: Lifecycle,
    *,
    otherwise: Lifecycle = NORMAL,
) -> Policy:
    def policy(_ctx: RenderContext, obs: Observation) -> Lifecycle:
        return lifecycle if obs.source_activity > threshold else otherwise

    return policy


def section_policy(sections: list[tuple[float, float, Policy]], *, default: Lifecycle = NORMAL) -> Policy:
    """Select an explicit policy by beat interval [start, end)."""

    def policy(ctx: RenderContext, obs: Observation) -> Lifecycle:
        for start, end, inner in sections:
            if start <= ctx.beat < end:
                return inner(ctx, obs)
        return default

    return policy


def override(base: Policy, condition: Callable[[RenderContext, Observation], bool], replacement: Policy) -> Policy:
    """Explicit, ordered policy override. No hidden merge semantics."""

    def policy(ctx: RenderContext, obs: Observation) -> Lifecycle:
        return replacement(ctx, obs) if condition(ctx, obs) else base(ctx, obs)

    return policy


class ActivityFollower:
    def __init__(self, alpha: float = 0.01) -> None:
        if not 0.0 < alpha <= 1.0:
            raise ValueError("alpha must be in (0, 1]")
        self.alpha = alpha
        self.value = 0.0

    def update(self, x: float) -> float:
        self.value += (abs(x) - self.value) * self.alpha
        return self.value


class Transformation(Protocol):
    identity: str

    def process_sample(self, x: float, lifecycle: Lifecycle, ctx: RenderContext) -> float: ...
    def trace_state(self) -> dict[str, float | int | str]: ...


class FeedbackMemory:
    """Simple recursive memory with an independent source-history ring."""

    def __init__(self, identity: str, *, decay: float = 0.985, history_size: int = 8000) -> None:
        self.identity = identity
        self.decay = decay
        self.state = 0.0
        self.history = [0.0] * max(1, history_size)
        self.history_pos = 0
        self.observation = 0.0

    def process_sample(self, x: float, lifecycle: Lifecycle, ctx: RenderContext) -> float:
        if lifecycle.history is HistoryPolicy.ACCUMULATE:
            self.history[self.history_pos] = x
            self.history_pos = (self.history_pos + 1) % len(self.history)

        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation = 0.995 * self.observation + 0.005 * abs(x)

        if lifecycle.state is StatePolicy.RESET:
            self.state = 0.0
        elif lifecycle.state is StatePolicy.ADVANCE:
            admitted = x if lifecycle.input is InputPolicy.ADMIT else 0.0
            # soft bounded recursive memory
            self.state = math.tanh(self.decay * self.state + 0.35 * admitted)

        out = self.state
        return out if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def inject_recent_history(self, fraction_ago: float = 0.5, amount: float = 0.25) -> None:
        fraction_ago = min(1.0, max(0.0, fraction_ago))
        offset = int(fraction_ago * (len(self.history) - 1))
        idx = (self.history_pos - 1 - offset) % len(self.history)
        self.state = math.tanh(self.state + amount * self.history[idx])

    def trace_state(self) -> dict[str, float | int | str]:
        return {"state": self.state, "history_pos": self.history_pos, "observation": self.observation}


class DelayMemory:
    """Owned temporal memory with independently advancing read position."""

    def __init__(self, identity: str, *, delay_samples: int = 4000, feedback: float = 0.55) -> None:
        self.identity = identity
        self.buffer = [0.0] * max(2, delay_samples)
        self.write_pos = 0
        self.read_phase = 0
        self.feedback = feedback
        self.last = 0.0
        self.observation = 0.0

    def process_sample(self, x: float, lifecycle: Lifecycle, ctx: RenderContext) -> float:
        read_pos = (self.write_pos - len(self.buffer) + self.read_phase) % len(self.buffer)
        remembered = self.buffer[read_pos]

        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation = 0.995 * self.observation + 0.005 * abs(x)

        if lifecycle.history is HistoryPolicy.ACCUMULATE:
            admitted = x if lifecycle.input is InputPolicy.ADMIT else 0.0
            self.buffer[self.write_pos] = math.tanh(admitted + self.feedback * self.last)
            self.write_pos = (self.write_pos + 1) % len(self.buffer)

        if lifecycle.state is StatePolicy.RESET:
            self.read_phase = 0
            self.last = 0.0
        elif lifecycle.state is StatePolicy.ADVANCE:
            self.read_phase = (self.read_phase + 1) % len(self.buffer)
            self.last = remembered

        return remembered if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def trace_state(self) -> dict[str, float | int | str]:
        return {"read_phase": self.read_phase, "write_pos": self.write_pos, "last": self.last}


class ResonantMemory:
    """Two-state resonant recursion to test lifecycle semantics on different state."""

    def __init__(self, identity: str, *, frequency: float = 330.0, damping: float = 0.997) -> None:
        self.identity = identity
        self.frequency = frequency
        self.damping = damping
        self.y1 = 0.0
        self.y2 = 0.0
        self.observation = 0.0

    def process_sample(self, x: float, lifecycle: Lifecycle, ctx: RenderContext) -> float:
        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation = 0.995 * self.observation + 0.005 * abs(x)

        if lifecycle.state is StatePolicy.RESET:
            self.y1 = self.y2 = 0.0
        elif lifecycle.state is StatePolicy.ADVANCE:
            admitted = x if lifecycle.input is InputPolicy.ADMIT else 0.0
            w = 2.0 * math.pi * self.frequency / ctx.sample_rate
            a = 2.0 * self.damping * math.cos(w)
            b = -(self.damping * self.damping)
            y = 0.08 * admitted + a * self.y1 + b * self.y2
            self.y2, self.y1 = self.y1, math.tanh(y)

        return self.y1 if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def trace_state(self) -> dict[str, float | int | str]:
        return {"y1": self.y1, "y2": self.y2, "observation": self.observation}


@dataclass
class TransformNode:
    process: Transformation
    policy: Policy = constant(NORMAL)


class Chain:
    def __init__(self, *nodes: TransformNode) -> None:
        self.nodes = list(nodes)

    def process_sample(self, x: float, ctx: RenderContext, obs: Observation) -> float:
        value = x
        for node in self.nodes:
            value = node.process.process_sample(value, node.policy(ctx, obs), ctx)
        return value


@dataclass(frozen=True)
class Study:
    name: str
    duration_beats: float
    bpm: float
    source: Callable[[RenderContext], float]
    chain: Chain
    sample_rate: int = 22050
    beats_per_bar: int = 4


@dataclass
class RenderResult:
    samples: list[float]
    trace: list[dict[str, float | int | str]]


def synthetic_source(ctx: RenderContext) -> float:
    """Deterministic source alternating sparse and dense regions."""
    t = ctx.seconds
    bar = ctx.bar
    tone = 0.22 * math.sin(2.0 * math.pi * 110.0 * t)
    pulse_phase = ctx.beat_phase
    pulse = math.exp(-35.0 * pulse_phase) if pulse_phase < 0.3 else 0.0
    if bar % 4 in (1, 2):
        dense = 0.16 * math.sin(2.0 * math.pi * 220.0 * t) + 0.1 * math.sin(2.0 * math.pi * 330.0 * t)
        eighth_phase = (ctx.beat * 2.0) % 1.0
        dense += 0.16 * math.exp(-50.0 * eighth_phase)
    else:
        dense = 0.0
    return tone + 0.28 * pulse + dense


def render(study: Study, *, trace_every_beats: float = 0.25) -> RenderResult:
    total_seconds = study.duration_beats * 60.0 / study.bpm
    total_samples = int(round(total_seconds * study.sample_rate))
    follower = ActivityFollower(alpha=0.01)
    samples: list[float] = []
    trace: list[dict[str, float | int | str]] = []
    next_trace_beat = 0.0
    last_lifecycles: dict[str, Lifecycle] = {}

    for i in range(total_samples):
        ctx = RenderContext(i, study.sample_rate, study.bpm, study.beats_per_bar)
        source = study.source(ctx)
        obs = Observation(follower.update(source))
        value = source

        for node in study.chain.nodes:
            lifecycle = node.policy(ctx, obs)
            value = node.process.process_sample(value, lifecycle, ctx)
            previous = last_lifecycles.get(node.process.identity)
            if previous != lifecycle:
                row: dict[str, float | int | str] = {
                    "event": "lifecycle",
                    "sample": i,
                    "beat": round(ctx.beat, 6),
                    "bar": ctx.bar,
                    "identity": node.process.identity,
                    "input": lifecycle.input.name,
                    "state": lifecycle.state.name,
                    "output": lifecycle.output.name,
                    "history": lifecycle.history.name,
                    "observation": lifecycle.observation.name,
                }
                row.update({f"process_{k}": v for k, v in node.process.trace_state().items()})
                trace.append(row)
                last_lifecycles[node.process.identity] = lifecycle

        if ctx.beat >= next_trace_beat:
            trace.append({
                "event": "checkpoint",
                "sample": i,
                "beat": round(ctx.beat, 6),
                "bar": ctx.bar,
                "source_activity": obs.source_activity,
                "output": value,
            })
            next_trace_beat += trace_every_beats

        samples.append(value)

    return RenderResult(samples=samples, trace=trace)


def write_wav(path: str | Path, samples: Iterable[float], sample_rate: int) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = list(samples)
    peak = max((abs(x) for x in data), default=1.0)
    scale = 0.92 / peak if peak > 0.92 else 1.0
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for x in data:
            q = int(max(-1.0, min(1.0, x * scale)) * 32767.0)
            frames.extend(struct.pack("<h", q))
        wf.writeframes(bytes(frames))


def write_trace_csv(path: str | Path, trace: list[dict[str, float | int | str]]) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fields: list[str] = []
    seen: set[str] = set()
    for row in trace:
        for key in row:
            if key not in seen:
                seen.add(key)
                fields.append(key)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(trace)
