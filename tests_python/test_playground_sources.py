import pathlib
import struct
import sys
import tempfile
import unittest

TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from playground_sources import read_wav_mono  # noqa: E402


def make_wav(*, format_tag: int, channels: int, sample_rate: int, bits: int, payload: bytes) -> bytes:
    width = bits // 8
    block_align = channels * width
    byte_rate = sample_rate * block_align
    fmt = struct.pack("<HHIIHH", format_tag, channels, sample_rate, byte_rate, block_align, bits)

    def chunk(kind: bytes, data: bytes) -> bytes:
        pad = b"\x00" if len(data) & 1 else b""
        return kind + struct.pack("<I", len(data)) + data + pad

    body = b"WAVE" + chunk(b"fmt ", fmt) + chunk(b"data", payload)
    return b"RIFF" + struct.pack("<I", len(body)) + body


class TestPlaygroundSources(unittest.TestCase):
    def write_temp(self, content: bytes) -> pathlib.Path:
        handle = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
        handle.write(content)
        handle.close()
        self.addCleanup(lambda: pathlib.Path(handle.name).unlink(missing_ok=True))
        return pathlib.Path(handle.name)

    def test_reads_pcm16_mono(self):
        payload = struct.pack("<hhh", 0, 16384, -16384)
        path = self.write_temp(
            make_wav(
                format_tag=1,
                channels=1,
                sample_rate=48000,
                bits=16,
                payload=payload,
            )
        )

        samples, rate = read_wav_mono(path)

        self.assertEqual(rate, 48000)
        self.assertEqual(len(samples), 3)
        self.assertAlmostEqual(samples[1], 0.5)
        self.assertAlmostEqual(samples[2], -0.5)

    def test_reads_ieee_float32_stereo_and_averages_channels(self):
        payload = struct.pack("<ffff", 0.25, 0.75, -0.5, 0.0)
        path = self.write_temp(
            make_wav(
                format_tag=3,
                channels=2,
                sample_rate=44100,
                bits=32,
                payload=payload,
            )
        )

        samples, rate = read_wav_mono(path)

        self.assertEqual(rate, 44100)
        self.assertEqual(len(samples), 2)
        self.assertAlmostEqual(samples[0], 0.5)
        self.assertAlmostEqual(samples[1], -0.25)

    def test_rejects_compressed_format(self):
        path = self.write_temp(
            make_wav(
                format_tag=6,
                channels=1,
                sample_rate=8000,
                bits=8,
                payload=b"\x00\x01",
            )
        )

        with self.assertRaisesRegex(ValueError, "unsupported WAV format tag"):
            read_wav_mono(path)


if __name__ == "__main__":
    unittest.main()
