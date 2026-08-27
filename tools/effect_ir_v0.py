"""Minimal effect-authoring semantic IR after prior-art bake-off round 1.

This is a dependency-free construction/reference model, not proposed .ascr syntax
and not a real-time renderer. It deliberately models only the smallest categories
needed to express the current falsification tasks.
"""

from __future__ import annotations

from dataclasses import dataclass, field, replace
from typing import Any, Iterable


@dataclass(frozen=True)
class Duration:
    value: float
    unit: str

    def __post_init__(self) -> None:
        if self.value < 0:
            raise ValueError("duration must be non-negative")
        if self.unit not in {"samples", "seconds", "beats", "bars"}:
            raise ValueError(f"unsupported duration unit: {self.unit}")


def samples(value: float) -> Duration:
    return Duration(value, "samples")


def seconds(value: float) -> Duration:
    return Duration(value, "seconds")


def beats(value: float) -> Duration:
    return Duration(value, "beats")


def bars(value: float) -> Duration:
    return Duration(value, "bars")


@dataclass(frozen=True)
class Material:
    """Symbolic audio material expression; not necessarily an allocated buffer."""

    op: str
    args: tuple[Any, ...] = ()
    label: str | None = None


def audio_input(name: str = "in") -> Material:
    return Material("input", (name,), label=name)


def combine(*materials: Material) -> Material:
    return Material("combine", tuple(materials))


@dataclass(frozen=True)
class Feature:
    name: str


@dataclass(frozen=True)
class Observation:
    feature: Feature
    source: Material
    temporal_ops: tuple[tuple[str, Any], ...] = ()

    def smooth(self, window: Duration) -> "Observation":
        return replace(self, temporal_ops=self.temporal_ops + (("smooth", window),))

    def trend(self, window: Duration) -> "Observation":
        return replace(self, temporal_ops=self.temporal_ops + (("trend", window),))

    def below(self, threshold: float) -> "Predicate":
        return Predicate(self, "below", threshold)

    def above(self, threshold: float) -> "Predicate":
        return Predicate(self, "above", threshold)


def observe(source: Material, feature: str) -> Observation:
    return Observation(Feature(feature), source)


@dataclass(frozen=True)
class Predicate:
    observation: Observation
    relation: str
    value: Any
    temporal_ops: tuple[tuple[str, Any], ...] = ()

    def for_duration(self, duration: Duration) -> "Predicate":
        return replace(self, temporal_ops=self.temporal_ops + (("dwell", duration),))

    def hysteresis(self, amount: float) -> "Predicate":
        return replace(self, temporal_ops=self.temporal_ops + (("hysteresis", amount),))


@dataclass(frozen=True)
class Chance:
    probability: float

    def __post_init__(self) -> None:
        if not 0.0 <= self.probability <= 1.0:
            raise ValueError("probability must be in [0, 1]")


@dataclass(frozen=True)
class Segmentation:
    mode: str
    source_observation: Observation | None = None
    window: Duration | None = None


@dataclass(frozen=True)
class FragmentQuery:
    memory_id: str
    segmentation: Segmentation
    predicates: tuple[Any, ...] = ()
    nearest_feature: Feature | None = None
    nearest_target: Observation | None = None
    limit: int | None = None
    item_transforms: tuple[tuple[str, Any], ...] = ()

    def where(self, predicate: Any) -> "FragmentQuery":
        return replace(self, predicates=self.predicates + (predicate,))

    def nearest(self, feature: str, target: Observation) -> "FragmentQuery":
        return replace(
            self,
            nearest_feature=Feature(feature),
            nearest_target=target,
        )

    def take(self, count: int) -> "FragmentQuery":
        if count < 0:
            raise ValueError("count must be non-negative")
        return replace(self, limit=count)

    def every(self, n: int, operation: str) -> "FragmentQuery":
        if n <= 0:
            raise ValueError("n must be positive")
        return replace(
            self,
            item_transforms=self.item_transforms + (("every", (n, operation)),),
        )

    def prefer(self, key: str, when: Any = None) -> "FragmentQuery":
        return replace(
            self,
            item_transforms=self.item_transforms + (("prefer", (key, when)),),
        )

    def as_material(self) -> Material:
        return Material("fragment_query", (self,), label=f"query:{self.memory_id}")


@dataclass(frozen=True)
class MemoryWrite:
    source: Material
    when: Any = None
    gain: float = 1.0


@dataclass
class Memory:
    identity: str
    capacity: Duration
    writes: list[MemoryWrite] = field(default_factory=list)

    def record(self, source: Material, *, when: Any = None, gain: float = 1.0) -> MemoryWrite:
        write = MemoryWrite(source=source, when=when, gain=gain)
        self.writes.append(write)
        return write

    def fragments(self, segmentation: Segmentation) -> FragmentQuery:
        return FragmentQuery(self.identity, segmentation)

    def material(self, *, age: Duration | None = None) -> Material:
        return Material("memory_read", (self.identity, age), label=self.identity)


@dataclass(frozen=True)
class Transformation:
    name: str
    processor: str
    params: tuple[tuple[str, Any], ...] = ()

    def derive(self, *, name: str | None = None, **params: Any) -> "Transformation":
        merged = dict(self.params)
        merged.update(params)
        return Transformation(name or self.name, self.processor, tuple(sorted(merged.items())))

    def instantiate(self, identity: str) -> "Instance":
        return Instance(identity=identity, behavior=self)


@dataclass
class Instance:
    identity: str
    behavior: Transformation
    generation: int = 0

    def replace_behavior(self, behavior: Transformation, *, keep_state: bool = True) -> None:
        self.behavior = behavior
        if not keep_state:
            self.generation += 1


def process(instance: Instance, material: Material) -> Material:
    return Material(
        "process",
        (instance.identity, instance.behavior, material),
        label=instance.identity,
    )


@dataclass(frozen=True)
class RotateBehaviors:
    instance_ids: tuple[str, ...]
    keep_state: bool = True


def rotate_behaviors(
    instances: Iterable[Instance],
    *,
    keep_state: bool = True,
) -> RotateBehaviors:
    return RotateBehaviors(tuple(instance.identity for instance in instances), keep_state)


@dataclass(frozen=True)
class Schedule:
    action: Any
    every: Duration | None = None
    when: Any = None


@dataclass
class Effect:
    name: str
    inputs: list[Material] = field(default_factory=list)
    memories: list[Memory] = field(default_factory=list)
    instances: list[Instance] = field(default_factory=list)
    observations: list[Observation] = field(default_factory=list)
    schedules: list[Schedule] = field(default_factory=list)
    outputs: list[Material] = field(default_factory=list)

    def add_output(self, material: Material) -> None:
        self.outputs.append(material)
