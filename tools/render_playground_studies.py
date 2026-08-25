"""Render deterministic artistic-viability studies for playground v0.

Generated WAV/CSV files are intentionally not committed by this script.
"""

from __future__ import annotations

from pathlib import Path
import argparse

from lifecycle_spike import FREEZE, NORMAL, SILENT_EVOLUTION, TAIL, Lifecycle, InputPolicy, StatePolicy, OutputPolicy, HistoryPolicy, ObservationPolicy
from playground_v0 import (
    Chain,
    DelayMemory,
    FeedbackMemory,
    ResonantMemory,
    Study,
    TransformNode,
    constant,
    every,
    render,
    section_policy,
    synthetic_source,
    when_activity_above,
    write_trace_csv,
    write_wav,
)
from playground_v0_events import ScheduledIntervention, render_with_interventions


REMEMBER_SILENT = Lifecycle(
    input=InputPolicy.BLOCK,
    state=StatePolicy.FREEZE,
    output=OutputPolicy.SILENT,
    history=HistoryPolicy.ACCUMULATE,
    observation=ObservationPolicy.ADVANCE,
)



def p1_variant(name: str, variant: str) -> Study:
    process = FeedbackMemory("memory", decay=0.992, history_size=12000)

    if variant == "a":
        policy = section_policy([
            (0, 16, constant(NORMAL)),
            (16, 32, every(4, offset_beats=2, length_beats=1, lifecycle=TAIL)),
            (32, 48, every(5, offset_beats=1, length_beats=1.5, lifecycle=FREEZE)),
            (48, 64, every(6, offset_beats=3, length_beats=1, lifecycle=SILENT_EVOLUTION)),
        ])
    elif variant == "b":
        policy = section_policy([
            (0, 16, constant(NORMAL)),
            (16, 28, every(3, offset_beats=1, length_beats=1, lifecycle=TAIL)),
            (28, 44, every(4, offset_beats=0.5, length_beats=2, lifecycle=REMEMBER_SILENT)),
            (44, 64, every(7, offset_beats=2, length_beats=1.5, lifecycle=TAIL)),
        ])
    elif variant == "c":
        policy = section_policy([
            (0, 16, constant(NORMAL)),
            (16, 32, every(4, offset_beats=1, length_beats=2, lifecycle=SILENT_EVOLUTION)),
            (32, 48, every(5, offset_beats=0, length_beats=2.5, lifecycle=REMEMBER_SILENT)),
            (48, 64, every(8, offset_beats=2, length_beats=3, lifecycle=SILENT_EVOLUTION)),
        ])
    else:
        raise ValueError(variant)

    return Study(name, duration_beats=64, bpm=120, source=synthetic_source, chain=Chain(TransformNode(process, policy)))


def p1_interventions(variant: str) -> list[ScheduledIntervention]:
    if variant == "a":
        beats = (49.0, 57.0)
        amount = 0.22
    elif variant == "b":
        beats = (44.0, 50.0, 58.0)
        amount = 0.5
    elif variant == "c":
        beats = (48.0, 52.0, 60.0)
        amount = 0.7
    else:
        raise ValueError(variant)

    return [
        ScheduledIntervention(
            beat=beat,
            identity="memory",
            action=lambda process, a=amount: process.inject_recent_history(fraction_ago=0.0, amount=a),
            label=f"reveal-recent-history-{index + 1}",
        )
        for index, beat in enumerate(beats)
    ]


def p2() -> Study:
    a = FeedbackMemory("A", decay=0.991, history_size=9000)
    b = ResonantMemory("B", frequency=247.0, damping=0.998)

    a_policy = section_policy([
        (0, 16, constant(NORMAL)),
        (16, 32, every(4, offset_beats=2, length_beats=1.5, lifecycle=TAIL)),
        (32, 48, every(5, offset_beats=1, length_beats=2, lifecycle=REMEMBER_SILENT)),
        (48, 64, constant(NORMAL)),
    ])
    b_policy = section_policy([
        (0, 16, constant(SILENT_EVOLUTION)),
        (16, 32, constant(NORMAL)),
        (32, 48, every(3, offset_beats=0.5, length_beats=1.25, lifecycle=SILENT_EVOLUTION)),
        (48, 64, every(6, offset_beats=2, length_beats=1, lifecycle=FREEZE)),
    ])

    return Study("p2", 64, 120, synthetic_source, Chain(TransformNode(a, a_policy), TransformNode(b, b_policy)))


def p2_interventions() -> list[ScheduledIntervention]:
    return [
        ScheduledIntervention(beat=48.0, identity="A", action=lambda process: process.inject_recent_history(0.0, 0.45), label="A-reveal-history"),
        ScheduledIntervention(beat=56.0, identity="A", action=lambda process: process.inject_recent_history(0.25, 0.4), label="A-reveal-older-history"),
    ]


def p3_variant(name: str, variant: str) -> Study:
    process = DelayMemory("responsive-memory", delay_samples=3200, feedback=0.65)

    if variant == "a":
        policy = when_activity_above(0.19, TAIL, otherwise=NORMAL)
    elif variant == "b":
        policy = when_activity_above(0.19, SILENT_EVOLUTION, otherwise=NORMAL)
    elif variant == "c":
        responsive = when_activity_above(0.19, TAIL, otherwise=NORMAL)
        timed = every(8, offset_beats=6, length_beats=1.5, lifecycle=FREEZE)

        def policy(ctx, obs):
            if (ctx.beat - 6) % 8 < 1.5:
                return timed(ctx, obs)
            return responsive(ctx, obs)
    else:
        raise ValueError(variant)

    return Study(name, 48, 120, synthetic_source, Chain(TransformNode(process, policy)))


def studies() -> list[Study]:
    return [
        p1_variant("p1a", "a"),
        p1_variant("p1b", "b"),
        p1_variant("p1c", "c"),
        p2(),
        p3_variant("p3a", "a"),
        p3_variant("p3b", "b"),
        p3_variant("p3c", "c"),
    ]


def interventions_for(study: Study) -> list[ScheduledIntervention]:
    if study.name.startswith("p1"):
        return p1_interventions(study.name[-1])
    if study.name == "p2":
        return p2_interventions()
    return []


def render_study(study: Study):
    interventions = interventions_for(study)
    return render_with_interventions(study, interventions) if interventions else render(study)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=Path("playground_renders"))
    parser.add_argument("--only", action="append", default=[])
    args = parser.parse_args()

    selected = [s for s in studies() if not args.only or s.name in set(args.only)]
    for study in selected:
        result = render_study(study)
        write_wav(args.out / f"{study.name}.wav", result.samples, study.sample_rate)
        write_trace_csv(args.out / f"{study.name}.csv", result.trace)
        print(f"{study.name}: {len(result.samples)} samples, {len(result.trace)} trace rows")


if __name__ == "__main__":
    main()
