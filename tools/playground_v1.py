"""Corrected playable semantics after the history/lifecycle audit.

v1 keeps process lifecycle separate from remembered material. A MaterialMemory records
material independently; its output can later become input material to any process.
This is still a research construction API, not proposed .ascr syntax.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Protocol
import math

from lifecycle_spike import InputPolicy, ObservationPolicy, OutputPolicy, StatePolicy
from playground_v0 import RenderContext, Observation


@dataclass(frozen=True)
class ProcessLifecycle:
    input: InputPolicy = InputPolicy.ADMIT
    state: StatePolicy = StatePolicy.ADVANCE
    output: OutputPolicy = OutputPolicy.AUDIBLE
    observation: ObservationPolicy = ObservationPolicy.ADVANCE


NORMAL = ProcessLifecycle()
TAIL = ProcessLifecycle(input=InputPolicy.BLOCK)
FREEZE = ProcessLifecycle(input=InputPolicy.BLOCK, state=StatePolicy.FREEZE)
SILENT_EVOLUTION = ProcessLifecycle(output=OutputPolicy.SILENT)
SILENT_FREEZE = ProcessLifecycle(
    input=InputPolicy.BLOCK,
    state=StatePolicy.FREEZE,
    output=OutputPolicy.SILENT,
)


class MemoryRecordPolicy(Enum):
    RECORD = auto()
    HOLD = auto()


class MaterialMemory:
    """Persistent remembered material independent of any processor's algorithmic state."""

    def __init__(self, identity: str, *, capacity_samples: int) -> None:
        if capacity_samples < 2:
            raise ValueError("capacity_samples must be >= 2")
        self.identity = identity
        self.buffer = [0.0] * capacity_samples
        self.write_pos = 0
        self.samples_seen = 0

    def record(self, x: float, policy: MemoryRecordPolicy = MemoryRecordPolicy.RECORD) -> None:
        if policy is MemoryRecordPolicy.RECORD:
            self.buffer[self.write_pos] = x
            self.write_pos = (self.write_pos + 1) % len(self.buffer)
            self.samples_seen += 1

    def read_samples_ago(self, samples_ago: float) -> float:
        samples_ago = max(0.0, min(float(len(self.buffer) - 1), samples_ago))
        position = (self.write_pos - 1) - samples_ago
        while position < 0:
            position += len(self.buffer)
        i0 = int(math.floor(position)) % len(self.buffer)
        frac = position - math.floor(position)
        i1 = (i0 + 1) % len(self.buffer)
        return self.buffer[i0] * (1.0 - frac) + self.buffer[i1] * frac

    def read_beats_ago(self, beats: float, ctx: RenderContext) -> float:
        seconds = beats * 60.0 / ctx.bpm
        return self.read_samples_ago(seconds * ctx.sample_rate)


class Process(Protocol):
    identity: str

    def process_sample(self, x: float, lifecycle: ProcessLifecycle, ctx: RenderContext) -> float: ...
    def trace_state(self) -> dict[str, float | int | str]: ...


class FeedbackProcess:
    """Recursive process with no public material-history semantics."""

    def __init__(self, identity: str, *, decay: float = 0.992) -> None:
        self.identity = identity
        self.decay = decay
        self.state = 0.0
        self.observation = 0.0

    def process_sample(self, x: float, lifecycle: ProcessLifecycle, ctx: RenderContext) -> float:
        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation = 0.995 * self.observation + 0.005 * abs(x)

        if lifecycle.state is StatePolicy.RESET:
            self.state = 0.0
        elif lifecycle.state is StatePolicy.ADVANCE:
            admitted = x if lifecycle.input is InputPolicy.ADMIT else 0.0
            self.state = math.tanh(self.decay * self.state + 0.35 * admitted)

        return self.state if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def trace_state(self) -> dict[str, float | int | str]:
        return {"state": self.state, "observation": self.observation}


class ResonantProcess:
    def __init__(self, identity: str, *, frequency: float = 247.0, damping: float = 0.998) -> None:
        self.identity = identity
        self.frequency = frequency
        self.damping = damping
        self.y1 = 0.0
        self.y2 = 0.0
        self.observation = 0.0

    def process_sample(self, x: float, lifecycle: ProcessLifecycle, ctx: RenderContext) -> float:
        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation = 0.995 * self.observation + 0.005 * abs(x)

        if lifecycle.state is StatePolicy.RESET:
            self.y1 = self.y2 = 0.0
        elif lifecycle.state is StatePolicy.ADVANCE:
            admitted = x if lifecycle.input is InputPolicy.ADMIT else 0.0
            w = 2.0 * math.pi * self.frequency / ctx.sample_rate
            y = 0.08 * admitted + 2.0 * self.damping * math.cos(w) * self.y1 - (self.damping**2) * self.y2
            self.y2, self.y1 = self.y1, math.tanh(y)

        return self.y1 if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def trace_state(self) -> dict[str, float | int | str]:
        return {"y1": self.y1, "y2": self.y2, "observation": self.observation}


LifecyclePolicy = Callable[[RenderContext, Observation], ProcessLifecycle]
MemoryPolicy = Callable[[RenderContext, Observation], MemoryRecordPolicy]
MaterialRelation = Callable[[float, RenderContext, Observation], float]


def constant(lifecycle: ProcessLifecycle) -> LifecyclePolicy:
    return lambda _ctx, _obs: lifecycle


def every(
    period_beats: float,
    *,
    offset_beats: float = 0.0,
    length_beats: float = 1.0,
    lifecycle: ProcessLifecycle,
    otherwise: ProcessLifecycle = NORMAL,
) -> LifecyclePolicy:
    def policy(ctx: RenderContext, _obs: Observation) -> ProcessLifecycle:
        local = (ctx.beat - offset_beats) % period_beats
        return lifecycle if local < length_beats else otherwise
    return policy


def when_activity_above(
    threshold: float,
    lifecycle: ProcessLifecycle,
    *,
    otherwise: ProcessLifecycle = NORMAL,
) -> LifecyclePolicy:
    return lambda _ctx, obs: lifecycle if obs.source_activity > threshold else otherwise


def section_policy(
    sections: list[tuple[float, float, LifecyclePolicy]],
    *,
    default: ProcessLifecycle = NORMAL,
) -> LifecyclePolicy:
    def policy(ctx: RenderContext, obs: Observation) -> ProcessLifecycle:
        for start, end, inner in sections:
            if start <= ctx.beat < end:
                return inner(ctx, obs)
        return default
    return policy


def present() -> MaterialRelation:
    return lambda x, _ctx, _obs: x


def mix_with_memory(
    memory: MaterialMemory,
    *,
    beats_ago: float,
    amount: Callable[[RenderContext, Observation], float] | float,
) -> MaterialRelation:
    """Mix current material with remembered material before processor admission."""

    def relation(x: float, ctx: RenderContext, obs: Observation) -> float:
        a = amount(ctx, obs) if callable(amount) else amount
        a = min(1.0, max(0.0, a))
        remembered = memory.read_beats_ago(beats_ago, ctx)
        return (1.0 - a) * x + a * remembered

    return relation


def memory_only(
    memory: MaterialMemory,
    *,
    beats_ago: float,
    gain: float = 1.0,
) -> MaterialRelation:
    return lambda _x, ctx, _obs: gain * memory.read_beats_ago(beats_ago, ctx)


def choose_relation(
    condition: Callable[[RenderContext, Observation], bool],
    when_true: MaterialRelation,
    when_false: MaterialRelation,
) -> MaterialRelation:
    return lambda x, ctx, obs: (when_true if condition(ctx, obs) else when_false)(x, ctx, obs)


@dataclass
class TransformVoice:
    process: Process
    lifecycle: LifecyclePolicy
    material: MaterialRelation

    def process_sample(self, source: float, ctx: RenderContext, obs: Observation) -> float:
        material = self.material(source, ctx, obs)
        return self.process.process_sample(material, self.lifecycle(ctx, obs), ctx)
