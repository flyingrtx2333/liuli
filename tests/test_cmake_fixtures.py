"""Custom declarations must not inherit a counter-specific GUI smoke fixture."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CMAKE = os.environ.get('CMAKE') or shutil.which('cmake')


@unittest.skipIf(not CMAKE or os.environ.get('EUI_CONSOLE_ONLY') == '1',
                 'requires CMake and graphics dependency discovery')
class CMakeFixtureTests(unittest.TestCase):
    def test_window_smoke_only_for_its_bundled_model_and_layout(self):
        variants = [([], True),
                    (['-DEUI_LAYOUT=' + str(ROOT / 'examples/window.layout.json')], False),
                    (['-DEUI_MODEL=' + str(ROOT / 'examples/enabled_controls.eui.json'),
                      '-DEUI_LAYOUT=' + str(ROOT / 'examples/enabled_controls.layout.json')], False)]
        with tempfile.TemporaryDirectory() as directory:
            for index, (arguments, expected) in enumerate(variants):
                build = Path(directory) / str(index)
                result = subprocess.run([CMAKE, '-S', str(ROOT), '-B', str(build),
                                         '-DBUILD_TESTING=ON', '-DEUI_BUILD_WINDOW=ON',
                                         '-DEUI_LINUX_ACCESSIBILITY=OFF'] + arguments,
                                        capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                registered = (build / 'CTestTestfile.cmake').read_text()
                self.assertIn('scripted_smoke', registered)
                self.assertEqual('window_synthetic' in registered, expected)


if __name__ == '__main__':
    unittest.main()
