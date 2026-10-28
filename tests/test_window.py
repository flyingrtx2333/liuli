"""SDL integration tests. Dummy video is synthetic, never proof of a desktop window."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(os.environ.get("EUI_CONSOLE_ONLY") == "1", "console-only build excludes SDL/Cairo window tests")
class WindowHostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["sh", "scripts/build-window.sh"], cwd=ROOT, check=True,
                       capture_output=True, timeout=120)
        cls.binary = ROOT / "build/window/window-host"
        cls.env = dict(os.environ, SDL_VIDEODRIVER="dummy", XDG_CACHE_HOME="/tmp/explicit-ui-font-cache")

    def test_synthetic_events_through_sdl_queue(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "window.png"
            result = subprocess.run([str(self.binary), "--smoke", str(output)],
                                    env=self.env, text=True, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
            self.assertEqual(records[-1], {"smoke": "passed", "synthetic_events": True,
                                          "errors": 2, "video_driver": "dummy"})
            self.assertEqual(records[-2]["views"]["counter"], "Count: 4")
            self.assertEqual(records[-2]["views"]["list"], ["Item 00007"])
            self.assertEqual(output.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")

    def test_opt_in_state_load_save_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            state = path / "app.state"
            state.write_bytes(b"EUISTATE 1\nI 5 1\ncount5\nI 6 1\noffset0\nT 5 5\nquery00007\nEND\n")
            result = subprocess.run([str(self.binary), "--state-file", str(state), "--smoke", str(path / "out.png")], env=self.env, text=True, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
            self.assertEqual(records[0]["views"]["counter"], "Count: 5")
            self.assertEqual(records[0]["views"]["list"], ["Item 00007"])
            self.assertEqual(records[-2]["views"]["counter"], "Count: 9")
            self.assertIn("state_saved", result.stdout)
            self.assertEqual(result.stdout.count("state_loaded"), 2)
            self.assertIn(state.read_bytes().splitlines()[0], [b"EUISTATE 1", b"EUISTATE 2"])
            self.assertFalse(list(path.glob("*.tmp.*")))

    def test_missing_opt_in_state_recovers_without_implicit_save(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            state = path / "missing-dir" / "app.state"
            result = subprocess.run([str(self.binary), "--state-file", str(state), "--smoke", str(path / "out.png")], env=self.env, text=True, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("state load rejected", result.stderr)
            self.assertIn("state save rejected", result.stderr)
            self.assertFalse(state.exists())
            records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
            self.assertEqual(records[-2]["views"]["counter"], "Count: 4")

    def test_bad_arguments_fail_without_starting_sdl(self):
        result = subprocess.run([str(self.binary), "--unknown"], env=self.env,
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn("Usage:", result.stderr)

    def test_missing_video_driver_is_explicit_error(self):
        env = dict(self.env, SDL_VIDEODRIVER="explicit-ui-invalid-driver")
        result = subprocess.run([str(self.binary)], env=env, capture_output=True,
                                text=True, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertIn("window host:", result.stderr)
        self.assertNotIn("window-ready", result.stdout)


if __name__ == "__main__":
    unittest.main()
