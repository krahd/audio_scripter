from pathlib import Path
import tempfile

from tools.validate_scripts import audit_signal_safety


def _temp_script(content: str) -> Path:
    with tempfile.NamedTemporaryFile("w", suffix=".ascr", delete=False, encoding="utf-8") as handle:
        handle.write(content)
        return Path(handle.name)


def test_audit_signal_safety_flags_unstable_feedback_literal():
    script = _temp_script(
        """
#@p1: Wet
# p1 = 0.5
fb = 1.1;
d = delay(inL + state_fb * fb, 1200, 0);
outL = mix(inL, d, p1);
outR = mix(inR, d, p1);
"""
    )
    warnings = audit_signal_safety(script)
    assert any("unstable gain >= 1.0" in w for w in warnings)


def test_audit_signal_safety_accepts_macro_scaled_feedback_and_explicit_stereo_out():
    script = _temp_script(
        """
#@p1: Feedback
#@p2: Wet
# p1 = 0.3
# p2 = 0.5
fb = p1 * 0.75;
dL = delay(inL + state_fbL * fb, 800, 0);
dR = delay(inR + state_fbR * fb, 800, 1);
state_fbL = dL;
state_fbR = dR;
outL = mix(inL, dL, p2);
outR = mix(inR, dR, p2);
"""
    )
    warnings = audit_signal_safety(script)
    assert warnings == []
