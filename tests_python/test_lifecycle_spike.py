import importlib.util
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "lifecycle_spike.py"
SPEC = importlib.util.spec_from_file_location("lifecycle_spike", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
lifecycle_spike = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lifecycle_spike)


ReferenceTransformation = lifecycle_spike.ReferenceTransformation
NORMAL = lifecycle_spike.NORMAL
TAIL = lifecycle_spike.TAIL
FREEZE = lifecycle_spike.FREEZE
SILENT_EVOLUTION = lifecycle_spike.SILENT_EVOLUTION
HISTORY_ONLY = lifecycle_spike.HISTORY_ONLY


def test_tail_blocks_new_input_but_state_decays_and_remains_audible():
    t = ReferenceTransformation(decay=0.5)
    assert t.process_sample(1.0, NORMAL) == 1.0

    outputs = [t.process_sample(1.0, TAIL) for _ in range(3)]

    assert outputs == [0.5, 0.25, 0.125]
    assert t.state == 0.125


def test_freeze_preserves_state_exactly():
    t = ReferenceTransformation(decay=0.5)
    assert t.process_sample(1.0, NORMAL) == 1.0

    outputs = [t.process_sample(1.0, FREEZE) for _ in range(3)]

    assert outputs == [1.0, 1.0, 1.0]
    assert t.state == 1.0


def test_silent_evolution_advances_state_while_suppressing_output():
    t = ReferenceTransformation(decay=0.5)
    assert t.process_sample(1.0, NORMAL) == 1.0

    assert t.process_sample(1.0, SILENT_EVOLUTION) == 0.0
    assert t.state == 1.5

    # Resume without recreating the transformation. State continues from 1.5.
    assert t.process_sample(0.0, NORMAL) == 0.75


def test_history_only_accumulates_source_without_advancing_processing_state():
    t = ReferenceTransformation(decay=0.5, history_size=4)
    assert t.process_sample(1.0, NORMAL) == 1.0
    frozen_state = t.state

    assert t.process_sample(2.0, HISTORY_ONLY) == 0.0
    assert t.process_sample(3.0, HISTORY_ONLY) == 0.0

    assert t.state == frozen_state
    assert t.recent_history()[-2:] == (2.0, 3.0)
    assert t.observation > 0.0


def test_resume_preserves_identity_and_distinguishes_tail_from_freeze():
    tail = ReferenceTransformation(decay=0.5)
    frozen = ReferenceTransformation(decay=0.5)

    tail.process_sample(1.0, NORMAL)
    frozen.process_sample(1.0, NORMAL)

    tail.process_sample(0.0, TAIL)
    frozen.process_sample(0.0, FREEZE)

    assert tail.state == 0.5
    assert frozen.state == 1.0

    # Same next input, different audible result because lifecycle policy changed state.
    assert tail.process_sample(0.0, NORMAL) == 0.25
    assert frozen.process_sample(0.0, NORMAL) == 0.5
