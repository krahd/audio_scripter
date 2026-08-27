"""Effect-authoring semantic IR v0.1 after stress-suite corrections.

Research construction model only. Not proposed .ascr syntax, not a renderer, and not
a novelty claim. Compared with effect_ir_v0 this version:
- removes Form as a root semantic category;
- generalises Memory reads through TemporalAddress;
- adds source/channel Shape;
- separates transferable Policy from processor Transformation/state;
- resets private state on algorithm replacement unless migration is explicit;
- represents Transformation structure symbolically and keeps simple DSP composition cheap.
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
class Shape:
    """Minimal material shape. None means channel-polymorphic/unspecified."""

    channels: int | None = None

    def __post_init__(self) -> None:
        if self.channels is not None and self.channels <= 0:
            raise ValueError("channels must be positive or None")


@dataclass(frozen=True)
class TemporalAddress:
    """Symbolic mapping/view from output time to remembered-material time."""

    op: str
    args: tuple[Any, ...] = ()

    def then(self, other: "TemporalAddress") -> "TemporalAddress":
        return TemporalAddress("compose", (self, other))


def ago(duration: Duration) -> TemporalAddress:
    return TemporalAddress("ago", (duration,))


def reverse(*, window: Duration) -> TemporalAddress:
    return TemporalAddress("reverse", (window,))


def rate(factor: float, *, region: Any = None) -> TemporalAddress:
    if factor == 0:
        raise ValueError("temporal rate must be non-zero")
    return TemporalAddress("rate", (factor, region))


def jump(addresses: Iterable[TemporalAddress]) -> TemporalAddress:
    return TemporalAddress("jump", tuple(addresses))


@dataclass(frozen=True)
class Material:
    """Symbolic audio material expression; not necessarily a concrete buffer."""

    op: str
    args: tuple[Any, ...] = ()
    label: str | None = None
    shape: Shape = Shape()


def audio_input(name: str = "in", *, channels: int | None = None) -> Material:
    return Material("input", (name,), label=name, shape=Shape(channels))


def combine(*materials: Material) -> Material:
    if not materials:
        raise ValueError("combine requires at least one material")
    channels = {material.shape.channels for material in materials}
    shape = materials[0].shape if len(channels) == 1 else Shape()
    return Material("combine", tuple(materials), shape=shape)


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

    def below(self, threshold: Any) -> "Predicate":
        return Predicate(self, "below", threshold)

    def above(self, threshold: Any) -> "Predicate":
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
        return replace(self, nearest_feature=Feature(feature), nearest_target=target)

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

    def prefer(self, key: str, *, when: Any = None) -> "FragmentQuery":
        return replace(
            self,
            item_transforms=self.item_transforms + (("prefer", (key, when)),),
        )

    def as_material(self, *, shape: Shape = Shape()) -> Material:
        return Material(
            "fragment_query",
            (self,),
            label=f"query:{self.memory_id}",
            shape=shape,
        )


@dataclass(frozen=True)
class MemoryWrite:
    source: Material
    when: Any = None
    gain: float = 1.0


@dataclass
class Memory:
    identity: str
    capacity: Duration
    shape: Shape = Shape()
    writes: list[MemoryWrite] = field(default_factory=list)

    def record(self, source: Material, *, when: Any = None, gain: float = 1.0) -> MemoryWrite:
        write = MemoryWrite(source=source, when=when, gain=gain)
        self.writes.append(write)
        if self.shape.channels is None and source.shape.channels is not None:
            self.shape = source.shape
        return write

    def read(self, address: TemporalAddress) -> Material:
        return Material(
            "memory_read",
            (self.identity, address),
            label=self.identity,
            shape=self.shape,
        )

    def fragments(self, segmentation: Segmentation) -> FragmentQuery:
        return FragmentQuery(self.identity, segmentation)


@dataclass(frozen=True)
class TransformExpr:
    """Compiler-visible authored structure, independent of surface notation."""

    op: str
    args: tuple[Any, ...] = ()


@dataclass(frozen=True)
class Transformation:
    name: str
    expr: TransformExpr
    input_shape: Shape = Shape()
    output_shape: Shape = Shape()
    state_schema: tuple[tuple[str, str], ...] = ()

    def instantiate(self, identity: str, *, policy: "Policy | None" = None) -> "Instance":
        return Instance(identity, self, policy or Policy())


def primitive(
    name: str,
    *,
    input_channels: int | None = None,
    output_channels: int | None = None,
    state_schema: Iterable[tuple[str, str]] = (),
    **params: Any,
) -> Transformation:
    return Transformation(
        name=name,
        expr=TransformExpr("primitive", (name, tuple(sorted(params.items())))),
        input_shape=Shape(input_channels),
        output_shape=Shape(output_channels),
        state_schema=tuple(state_schema),
    )


def compose(*transformations: Transformation, name: str = "compose") -> Transformation:
    if not transformations:
        raise ValueError("compose requires at least one transformation")
    state_schema = tuple(
        state
        for transformation in transformations
        for state in transformation.state_schema
    )
    return Transformation(
        name=name,
        expr=TransformExpr("compose", tuple(transformations)),
        input_shape=transformations[0].input_shape,
        output_shape=transformations[-1].output_shape,
        state_schema=state_schema,
    )


def parallel(*transformations: Transformation, name: str = "parallel") -> Transformation:
    if not transformations:
        raise ValueError("parallel requires at least one transformation")
    return Transformation(
        name=name,
        expr=TransformExpr("parallel", tuple(transformations)),
        input_shape=transformations[0].input_shape,
        output_shape=Shape(),
        state_schema=tuple(
            state
            for transformation in transformations
            for state in transformation.state_schema
        ),
    )


def apply(transformation: Transformation, material: Material) -> Material:
    output_shape = transformation.output_shape
    if output_shape.channels is None:
        output_shape = material.shape
    return Material(
        "apply",
        (transformation, material),
        label=transformation.name,
        shape=output_shape,
    )


@dataclass(frozen=True)
class Policy:
    """External material/control/participation policy, separate from processor state."""

    material: Material | None = None
    controls: tuple[tuple[str, Any], ...] = ()
    lifecycle: Any = None

    def with_control(self, name: str, value: Any) -> "Policy":
        return replace(self, controls=self.controls + ((name, value),))


@dataclass(frozen=True)
class StateMigration:
    name: str
    from_schema: tuple[tuple[str, str], ...]
    to_schema: tuple[tuple[str, str], ...]


@dataclass
class Instance:
    identity: str
    transformation: Transformation
    policy: Policy = Policy()
    generation: int = 0
    last_migration: StateMigration | None = None

    def set_policy(self, policy: Policy) -> None:
        self.policy = policy

    def replace_transformation(
        self,
        transformation: Transformation,
        *,
        migration: StateMigration | None = None,
    ) -> None:
        if migration is None:
            # Private processor state is recreated by default.
            self.generation += 1
            self.last_migration = None
        else:
            if migration.from_schema != self.transformation.state_schema:
                raise ValueError("migration source schema does not match current transformation")
            if migration.to_schema != transformation.state_schema:
                raise ValueError("migration target schema does not match new transformation")
            self.last_migration = migration
        self.transformation = transformation


def process(instance: Instance, material: Material | None = None) -> Material:
    source = material or instance.policy.material
    if source is None:
        raise ValueError("instance has no material to process")
    return apply(instance.transformation, source)


def rotate_policies(instances: Iterable[Instance]) -> None:
    values = list(instances)
    if len(values) < 2:
        return
    policies = [instance.policy for instance in values]
    for index, instance in enumerate(values):
        instance.policy = policies[(index - 1) % len(values)]


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
