import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('mods_manifest', ROOT / 'tools/ci/manifest.py')
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)


class SwitchModManifestTests(unittest.TestCase):
    def test_feature_and_host_metadata_follow_platform_and_opt_in(self):
        for platform, flag, enabled in [('switch-arm64', 'ON', True), ('switch-arm64', 'OFF', False),
                                        ('windows-x64', 'ON', False), ('macos-intel', 'ON', False)]:
            with self.subTest(platform=platform, flag=flag), tempfile.TemporaryDirectory() as tmp:
                binary = Path(tmp) / 'binary'
                binary.write_bytes(b'fixture, not a runnable NRO')
                output = Path(tmp) / 'BUILD.json'
                args = ['manifest.py', '--platform', platform, '--binary', str(binary),
                        '--compiler', 'fixture', '--output', str(output)]
                with patch.object(sys, 'argv', args), patch.dict(os.environ, {'ORACLES_SWITCH_MODS': flag}, clear=True), \
                     patch.object(manifest.subprocess, 'check_output', side_effect=['9e' * 20, '']), \
                     patch.object(manifest.platform, 'platform', return_value='fixture-host'), \
                     patch('sys.stdout', new_callable=io.StringIO):
                    manifest.main()
                result = json.loads(output.read_text())
                self.assertEqual(result['features']['switch_mods'], enabled)
                self.assertEqual(result['mod_host'], {'api': 2, 'version': 1} if enabled else None)
                self.assertFalse(result['hardware_tested'])


if __name__ == '__main__':
    unittest.main()
