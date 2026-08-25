"""Reference spike for persistent transformation lifecycle semantics.

This module is intentionally independent of the public .ascr grammar and C++ runtime.
It exists to make lifecycle-policy semantics executable before any language syntax is
proposed.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto


class InputPolicy(Enum):
    ADMIT = auto()
    BLOCK = auto()


class StatePolicy(Enum):
    ADVANCE = auto()
    FREEZE = auto()
    RESET = auto()


class OutputPolicy(Enum):
    AUDIBLE = auto()
    SILENT = auto()


class HistoryPolicy(Enum):
    ACCUMULATE = auto()
    HOLD = auto()


class ObservationPolicy(Enum):
    ADVANCE = auto()
    HOLD = auto()


@dataclass(frozen=True)
class Lifecycle:
    input: InputPolicy = InputPolicy.ADMIT
    state: StatePolicy = StatePolicy.ADVANCE
    output: OutputPolicy = OutputPolicy.AUDIBLE
    history: HistoryPolicy = HistoryPolicy.ACCUMULATE
    observation: ObservationPolicy = ObservationPolicy.ADVANCE


NORMAL = Lifecycle()
TAIL = Lifecycle(input=InputPolicy.BLOCK)
FREEZE = Lifecycle(input=InputPolicy.BLOCK, state=StatePolicy.FREEZE)
SILENT_EVOLUTION = Lifecycle(output=OutputPolicy.SILENT)
HISTORY_ONLY = Lifecycle(
    input=InputPolicy.BLOCK,
    state=StatePolicy.FREEZE,
    output=OutputPolicy.SILENT,
    history=HistoryPolicy.ACCUMULATE,
    observation=ObservationPolicy.ADVANCE,
)
HOLD_ALL = Lifecycle(
    input=InputPolicy.BLOCK,
    state=StatePolicy.FREEZE,
    output=OutputPolicy.SILENT,
    history=HistoryPolicy.HOLD,
    observation=ObservationPolicy.HOLD,
)
OBSERVE_ONLY = Lifecycle(
    input=InputPolicy.BLOCK,
    state=StatePolicy.FREEZE,
    output=OutputPolicy.SILENT,
    history=HistoryPolicy.HOLD,
    observation=ObservationPolicy.ADVANCE,
)


class ReferenceTransformation:
    """One-state feedback process used to expose lifecycle-policy differences."""

    def __init__(
        self,
        *,
        decay: float = 0.5,
        history_size: int = 8,
        observation_alpha: float = 0.5,
    ) -> None:
        if not 0.0 <= decay <= 1.0:
            raise ValueError("decay must be in [0, 1]")
        if history_size < 1:
            raise ValueError("history_size must be positive")
        if not 0.0 < observation_alpha <= 1.0:
            raise ValueError("observation_alpha must be in (0, 1]")

        self.decay = decay
        self.observation_alpha = observation_alpha
        self.state = 0.0
        self.history = [0.0] * history_size
        self.history_pos = 0
        self.observation = 0.0
        self.samples_processed = 0

    def process_sample(self, x: float, lifecycle: Lifecycle = NORMAL) -> float:
        if lifecycle.history is HistoryPolicy.ACCUMULATE:
            self.history[self.history_pos] = x
            self.history_pos = (self.history_pos + 1) % len(self.history)

        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation += (
                abs(x) - self.observation
            ) * self.observation_alpha

        if lifecycle.state is StatePolicy.RESET:
            self.state = 0.0
        elif lifecycle.state is StatePolicy.ADVANCE:
            admitted_input = x if lifecycle.input is InputPolicy.ADMIT else 0.0
            self.state = self.decay * self.state + admitted_input
        elif lifecycle.state is StatePolicy.FREEZE:
            pass
        else:  # pragma: no cover
            raise AssertionError(f"unsupported state policy: {lifecycle.state}")

        self.samples_processed += 1
        return self.state if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def recent_history(self) -> tuple[float, ...]:
        return tuple(self.history[self.history_pos :] + self.history[: self.history_pos])


class MemoryTransformation:
    """Different stateful process whose audible result explicitly reads owned history.

    `history` stores incoming material. `read_phase` is independent processing state.
    This lets the spike test whether history may evolve while processing state is frozen
    and later become audible.
    """

    def __init__(self, *, history_size: int = 4) -> None:
        if history_size < 1:
            raise ValueError("history_size must be positive")
        self.history = [0.0] * history_size
        self.history_pos = 0
        self.read_phase = 0
        self.observation = 0.0

    def process_sample(self, x: float, lifecycle: Lifecycle = NORMAL) -> float:
        if lifecycle.history is HistoryPolicy.ACCUMULATE:
            self.history[self.history_pos] = x
            self.history_pos = (self.history_pos + 1) % len(self.history)

        if lifecycle.observation is ObservationPolicy.ADVANCE:
            self.observation = 0.5 * self.observation + 0.5 * abs(x)

        if lifecycle.state is StatePolicy.RESET:
            self.read_phase = 0
        elif lifecycle.state is StatePolicy.ADVANCE:
            self.read_phase = (self.read_phase + 1) % len(self.history)
        elif lifecycle.state is StatePolicy.FREEZE:
            pass
        else:  # pragma: no cover
            raise AssertionError(f"unsupported state policy: {lifecycle.state}")

        ordered = self.history[self.history_pos :] + self.history[: self.history_pos]
        value = ordered[self.read_phase]
        return value if lifecycle.output is OutputPolicy.AUDIBLE else 0.0


class BeatPolicy:
    """Tiny musical-time policy source independent of transformation state."""

    def __init__(self, *, beats_per_bar: int = 4) -> None:
        self.beats_per_bar = beats_per_bar

    def at(self, beat_index: int) -> Lifecycle:
        bar = beat_index // self.beats_per_bar
        beat = beat_index % self.beats_per_bar
        if bar % 4 == 3:
            return FREEZE
        if beat in (2, 3):
            return TAIL
        return NORMAL
