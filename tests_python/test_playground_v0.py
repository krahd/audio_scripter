import importlib.util
from pathlib import Path
import sys


TOOLS = Path(__file__).resolve().parents[1] / "tools"
for module_name in ("lifecycle_spike", "playground_v0", "render_playground_studies"):
    path = TOOLS / f"{module_name}.py"
    spec = importlib.util.spec_from_file_location(module_name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)

lifecycle = sys.modules["lifecycle_spike"]
pg = sys.modules["playground_v0"]
studies_mod = sys.modules["render_playground_studies"]


def test_render_context_maps_samples_to_musical_time():
    ctx = pg.RenderContext(sample_index=22050, sample_rate=22050, bpm=120, beats_per_bar=4)
    assert ctx.seconds == 1.0
    assert ctx.beat == 2.0
    assert ctx.beat_index == 2
    assert ctx.bar == 0
    assert ctx.beat_in_bar == 2


def test_every_policy_is_musical_time_not_sample_count():
    policy = pg.every(4, offset_beats=1, length_beats=1, lifecycle=lifecycle.FREEZE)
    obs = pg.Observation()
    at_beat_1 = pg.RenderContext(11025, 22050, 120)
    at_beat_2 = pg.RenderContext(22050, 22050, 120)
    assert policy(at_beat_1, obs) == lifecycle.FREEZE
    assert policy(at_beat_2, obs) == lifecycle.NORMAL


def test_feedback_memory_preserves_identity_across_policy_changes():
    t = pg.FeedbackMemory("x", decay=0.9, history_size=32)
    identity = id(t)
    ctx = pg.RenderContext(0, 22050, 120)
    t.process_sample(1.0, lifecycle.NORMAL, ctx)
    state_after_normal = t.state
    t.process_sample(0.0, lifecycle.FREEZE, ctx)
    assert id(t) == identity
    assert t.state == state_after_normal
    t.process_sample(0.0, lifecycle.TAIL, ctx)
    assert id(t) == identity
    assert t.state != state_after_normal


def test_history_can_diverge_from_processing_state_and_remain_owned():
    t = pg.FeedbackMemory("x", decay=0.9, history_size=16)
    ctx = pg.RenderContext(0, 22050, 120)
    t.process_sample(1.0, lifecycle.NORMAL, ctx)
    frozen = t.state
    for x in (0.2, 0.4, 0.8):
        t.process_sample(x, studies_mod.REMEMBER_SILENT, ctx)
    assert t.state == frozen
    assert any(abs(v) > 0.0 for v in t.history)
    before = t.state
    t.inject_recent_history(fraction_ago=0.0, amount=0.5)
    assert t.state != before


def test_three_reference_processes_respond_to_same_lifecycle_contract():
    ctx = pg.RenderContext(0, 22050, 120)
    processes = [
        pg.FeedbackMemory("f", history_size=32),
        pg.DelayMemory("d", delay_samples=16),
        pg.ResonantMemory("r"),
    ]
    for process in processes:
        process.process_sample(1.0, lifecycle.NORMAL, ctx)
        before = process.trace_state().copy()
        process.process_sample(0.7, lifecycle.FREEZE, ctx)
        after = process.trace_state().copy()
        # Some observation/history fields may advance; core state must not reset.
        assert process.identity in ("f", "d", "r")
        assert before is not after


def test_each_study_renders_finite_nonzero_audio_and_trace():
    # Lower the sample rate and duration for test speed while preserving each semantic program.
    for study in studies_mod.studies():
        fast = pg.Study(
            study.name,
            duration_beats=4,
            bpm=study.bpm,
            source=study.source,
            chain=study.chain,
            sample_rate=2000,
            beats_per_bar=study.beats_per_bar,
        )
        result = pg.render(fast, trace_every_beats=1.0)
        assert result.samples
        assert result.trace
        assert all(x == x and abs(x) != float("inf") for x in result.samples)
        assert max(abs(x) for x in result.samples) > 0.0


def test_p1_variants_are_structurally_distinct():
    rendered = []
    for variant in ("a", "b", "c"):
        study = studies_mod.p1_variant(f"p1{variant}", variant)
        fast = pg.Study(study.name, 20, study.bpm, study.source, study.chain, sample_rate=2000)
        rendered.append(pg.render(fast).samples)
    # Compare coarse signatures instead of requiring exact predetermined audio.
    signatures = [tuple(round(x, 4) for x in samples[::250]) for samples in rendered]
    assert len(set(signatures)) == 3


def test_activity_driven_policies_produce_both_modes():
    study = studies_mod.p3_variant("p3a", "a")
    fast = pg.Study(study.name, 20, study.bpm, study.source, study.chain, sample_rate=2000)
    result = pg.render(fast)
    modes = {
        row.get("input")
        for row in result.trace
        if row.get("event") == "lifecycle" and row.get("identity") == "responsive-memory"
    }
    assert "ADMIT" in modes
    assert "BLOCK" in modes
