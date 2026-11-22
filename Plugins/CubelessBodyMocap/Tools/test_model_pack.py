"""Installer integrity and non-overwrite tests; tiny files, no Unreal host project."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
import stat
from model_pack import create_pack, install_pack, verify_pack, verify_installed, safe_name, extended
import shutil


class PackTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        self.pack = self.root / 'test.zip'; self.dest = self.root / 'plugin'

    def tearDown(self):
        shutil.rmtree(extended(self.root))
        self.tmp.cleanup()

    def build(self):
        create_pack(self.pack, 'body', 'plugin', {'Content/A.bin': b'first', 'Content/B.bin': b'second'}, {})

    def rewrite(self, change):
        with zipfile.ZipFile(self.pack) as z:
            entries = {x.filename: z.read(x) for x in z.infolist()}
        change(entries)
        altered = self.root / 'altered.zip'
        with zipfile.ZipFile(altered, 'w') as z:
            for name, data in entries.items(): z.writestr(name, data)
        return altered

    def test_roundtrip_and_idempotent_restore(self):
        self.build(); self.assertEqual(install_pack(self.pack, self.dest)['installed'], 2)
        self.assertEqual(verify_pack(self.pack, self.dest)['files'], 2)
        self.assertEqual(verify_installed(self.dest)['files'], 2)
        self.assertEqual(install_pack(self.pack, self.dest)['status'], 'already_installed')

    def test_conflict_preflight_preserves_all_existing_files(self):
        self.build(); (self.dest / 'Content').mkdir(parents=True)
        (self.dest / 'Content/B.bin').write_bytes(b'user-change')
        with self.assertRaises(FileExistsError): install_pack(self.pack, self.dest)
        self.assertFalse((self.dest / 'Content/A.bin').exists())
        self.assertEqual((self.dest / 'Content/B.bin').read_bytes(), b'user-change')

    def test_late_corruption_publishes_no_destination_files(self):
        self.build()
        bad = self.rewrite(lambda x: x.update({'payload/Content/B.bin': b'wrong!'}))
        with self.assertRaises(RuntimeError): install_pack(bad, self.dest)
        self.assertFalse((self.dest / 'Content/A.bin').exists())
        self.assertFalse((self.dest / 'Content/B.bin').exists())
        self.assertFalse(list(self.dest.glob('.cbm-install-*')))

    def test_paths_reject_traversal_drive_unc_and_reserved_names(self):
        for name in ['../outside', '/outside', 'C:/outside', '\\\\host\\share', 'Content/../A', 'Content/CON.txt', 'Content/A ', 'Content//A']:
            with self.subTest(name=name), self.assertRaises(ValueError): safe_name(name)

    def test_unlisted_and_case_duplicate_members_refused(self):
        self.build()
        for extra in ['payload/extra', 'payload/content/a.bin']:
            bad = self.rewrite(lambda x: x.update({extra: b'extra'}))
            with self.assertRaises(ValueError): install_pack(bad, self.dest)
            bad.unlink()

    def test_unsafe_manifest_name_refused_before_publication(self):
        self.build()
        def bad_name(x):
            m = json.loads(x['manifest.json']); m['files'][0]['path'] = '../outside'
            x['manifest.json'] = json.dumps(m).encode()
        bad = self.rewrite(bad_name)
        with self.assertRaises(ValueError): install_pack(bad, self.dest)
        self.assertFalse((self.root / 'outside').exists())

    def test_zip_symlink_refused(self):
        self.build(); bad = self.root / 'symlink.zip'
        with zipfile.ZipFile(self.pack) as src, zipfile.ZipFile(bad, 'w') as dst:
            for entry in src.infolist():
                data = src.read(entry)
                if entry.filename == 'payload/Content/A.bin':
                    entry.create_system = 3; entry.external_attr = (stat.S_IFLNK | 0o777) << 16
                dst.writestr(entry, data)
        with self.assertRaises(ValueError): install_pack(bad, self.dest)

    def test_long_windows_staging_path(self):
        name = 'Content/' + '/'.join(['long_directory_name_12345'] * 10) + '/value.bin'
        create_pack(self.pack, 'body', 'plugin', {name: b'long-path'}, {})
        install_pack(self.pack, self.dest)
        self.assertEqual(extended(self.dest / name).read_bytes(), b'long-path')

    def test_engine_mismatch_is_preflight_failure(self):
        create_pack(self.pack, 'body', 'plugin', {'Content/A': b'A'}, {'PatchVersion': 4})
        engine = self.root / 'engine'; (engine / 'Engine/Build').mkdir(parents=True)
        (engine / 'Engine/Build/Build.version').write_text('{"PatchVersion":3}')
        with self.assertRaises(ValueError): install_pack(self.pack, self.dest, engine)
        self.assertFalse(self.dest.exists())

    def test_prepare_uses_installed_receipt_without_original_plugin(self):
        self.build(); install_pack(self.pack, self.dest)
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('prepare_content.py')),
                                 '--plugin', str(self.dest), '--verify'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('verified', result.stdout)
        (self.dest / 'Content/A.bin').write_bytes(b'changed')
        result = subprocess.run([sys.executable, str(Path(__file__).with_name('prepare_content.py')),
                                 '--plugin', str(self.dest), '--verify'], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__': unittest.main()
