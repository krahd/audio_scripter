"""Explicit scheduled interventions for playground v0.

This module exists because lifecycle policy alone cannot express all artistic actions.
Interventions operate on persistent transformation identities at musical times without
recreating them. The mechanism is intentionally small and explicit.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from playground_v0 import ActivityFollower, Observation, RenderContext, RenderResult, Study


@dataclass(frozen=True)
class ScheduledIntervention:
    beat: float
    identity: str
    action: Callable[[object], None]
    label: str


def render_with_interventions(
    study: Study,
    interventions: list[ScheduledIntervention],
    *,
    trace_every_beats: float = 0.25,
) -> RenderResult:
    total_seconds = study.duration_beats * 60.0 / study.bpm
    total_samples = int(round(total_seconds * study.sample_rate))
    follower = ActivityFollower(alpha=0.01)
    samples: list[float] = []
    trace: list[dict[str, float | int | str]] = []
    next_trace_beat = 0.0
    last_lifecycles = {}
    ordered = sorted(interventions, key=lambda event: event.beat)
    event_index = 0
    by_identity = {node.process.identity: node.process for node in study.chain.nodes}

    for i in range(total_samples):
        ctx = RenderContext(i, study.sample_rate, study.bpm, study.beats_per_bar)

        while event_index < len(ordered) and ctx.beat >= ordered[event_index].beat:
            event = ordered[event_index]
            process = by_identity.get(event.identity)
            if process is None:
                raise KeyError(f"unknown transformation identity: {event.identity}")
            event.action(process)
            trace.append({
                "event": "intervention",
                "sample": i,
                "beat": round(ctx.beat, 6),
                "bar": ctx.bar,
                "identity": event.identity,
                "label": event.label,
            })
            event_index += 1

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
