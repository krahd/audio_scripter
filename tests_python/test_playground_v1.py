import importlib.util
from pathlib import Path
import sys


TOOLS = Path(__file__).resolve().parents[1] / "tools"
for module_name in (
    "lifecycle_spike",
    "playground_v0",
    "playground_v1",
    "render_playground_v1",
):
    path = TOOLS / f"{module_name}.py"
    spec = importlib.util.spec_from_file_location(module_name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)

life = sys.modules["lifecycle_spike"]
pg0 = sys.modules["playground_v0"]
pg1 = sys.modules["playground_v1"]
studies = sys.modules["render_playground_v1"]


def test_material_memory_records_source_independently_of_process_lifecycle():
    memory = pg1.MaterialMemory("m", capacity_samples=32)
    process = pg1.FeedbackProcess("p", decay=0.9)
    ctx = pg0.RenderContext(0, 1000, 120)
    obs = pg0.Observation()

    process.process_sample(1.0, pg1.NORMAL, ctx)
    state = process.state

    # Freeze the process but continue recording new source into the separate memory.
    for x in (0.2, 0.4, 0.8):
        memory.record(x)
        process.process_sample(x, pg1.FREEZE, ctx)

    assert process.state == state
    assert memory.samples_seen == 3
    assert abs(memory.read_samples_ago(0) - 0.8) < 1e-9


def test_same_material_memory_can_feed_different_process_types():
    memory = pg1.MaterialMemory("m", capacity_samples=32)
    ctx = pg0.RenderContext(0, 1000, 120)
    obs = pg0.Observation()
    for x in (0.1, 0.3, 0.6, 0.9):
        memory.record(x)

    relation = pg1.memory_only(memory, beats_ago=0.0, gain=1.0)
    f = pg1.FeedbackProcess("f")
    r = pg1.ResonantProcess("r")
    fv = pg1.TransformVoice(f, pg1.constant(pg1.NORMAL), relation)
    rv = pg1.TransformVoice(r, pg1.constant(pg1.NORMAL), relation)

    f_out = fv.process_sample(0.0, ctx, obs)
    r_out = rv.process_sample(0.0, ctx, obs)
    assert f_out != 0.0
    assert r_out != 0.0
    assert f.identity == "f" and r.identity == "r"


def test_mix_with_memory_is_material_relation_not_process_mutation():
    memory = pg1.MaterialMemory("m", capacity_samples=64)
    ctx = pg0.RenderContext(0, 1000, 120)
    obs = pg0.Observation()
    for _ in range(8):
        memory.record(1.0)
    relation = pg1.mix_with_memory(memory, beats_ago=0.0, amount=0.75)
    mixed = relation(0.0, ctx, obs)
    assert abs(mixed - 0.75) < 1e-9


def test_memory_history_has_musical_time_read_semantics():
    memory = pg1.MaterialMemory("m", capacity_samples=2000)
    # 120 bpm => half-second beat. At 1000 Hz, one beat ago = 500 samples.
    for i in range(1000):
        memory.record(i / 1000.0)
    ctx = pg0.RenderContext(1000, 1000, 120)
    value = memory.read_beats_ago(1.0, ctx)
    expected = memory.read_samples_ago(500)
    assert abs(value - expected) < 1e-12


def test_v1_studies_render_finite_audio_and_memory_trace():
    for study in studies.studies(sample_rate=2000):
        # Render enough form to cross at least one lifecycle/material section.
        fast = studies.V1Study(
            study.name,
            duration_beats=20,
            bpm=study.bpm,
            source=study.source,
            voices=study.voices,
            memories=study.memories,
            sample_rate=2000,
            beats_per_bar=study.beats_per_bar,
        )
        result = studies.render_v1(fast, trace_every_beats=2.0)
        assert result.samples
        assert result.trace
        assert all(x == x and abs(x) != float("inf") for x in result.samples)
        assert any(key.startswith("memory_") for row in result.trace for key in row)


def test_p1_v1_variants_diverge_after_shared_opening():
    rendered = []
    for variant in ("a", "b", "c"):
        study = studies.p1_variant(f"p1{variant}-v1", variant, sample_rate=2000)
        result = studies.render_v1(study, trace_every_beats=4.0)
        rendered.append(result.samples)

    # Opening section intentionally shares semantics, later sections should diverge.
    start = int((20 * 60 / 120) * 2000)
    signatures = [tuple(round(x, 4) for x in samples[start::500]) for samples in rendered]
    assert len(set(signatures)) == 3


def test_p2_v1_uses_shared_memory_as_compositional_material():
    study = studies.p2(sample_rate=2000)
    assert len(study.voices) == 2
    assert len(study.memories) == 1
    memory = study.memories[0].memory
    assert memory.identity == "shared-source-memory"
    result = studies.render_v1(study, trace_every_beats=4.0)
    assert memory.samples_seen > 0
    assert max(abs(x) for x in result.samples) > 0.0
