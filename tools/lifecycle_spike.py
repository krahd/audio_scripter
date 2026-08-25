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


class ReferenceTransformation:
    """A deliberately simple stateful process used to expose policy differences.

    The processor is a one-pole feedback accumulator:

        state[n] = decay * state[n-1] + admitted_input[n]

    It is not intended as an audio effect design. It gives state evolution a clear,
    deterministic audible consequence while keeping the lifecycle semantics visible.
    """

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
        """Advance one reference sample under the supplied lifecycle policy."""

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
        else:  # pragma: no cover - defensive against future enum extension
            raise AssertionError(f"unsupported state policy: {lifecycle.state}")

        self.samples_processed += 1
        return self.state if lifecycle.output is OutputPolicy.AUDIBLE else 0.0

    def recent_history(self) -> tuple[float, ...]:
        """Return history in chronological order, oldest to newest."""

        return tuple(self.history[self.history_pos :] + self.history[: self.history_pos])
