import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[2]
def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools/ci'/f'{name}.py')
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
release=module('release');manifest=module('manifest')

class ReleaseTests(unittest.TestCase):
    def test_tag_identity(self):
        self.assertFalse(release.validate('v1.0.2','1.0.2'))
        self.assertTrue(release.validate('v1.0.2-rc.1','1.0.2',True))
        for tag,candidate in [('v1.0.3',False),('v01.0.2',False),('v1.0.2;echo bad',False),('v1.0.2-rc.01',False),('v1.0.2',True)]:
            with self.assertRaises(ValueError):release.validate(tag,'1.0.2',candidate)

    def test_skips_are_not_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'tests.xml'
            p.write_text('<testsuite><testcase name="unit"/><testcase name="rom"><skipped/></testcase><testcase name="bad"><failure/></testcase></testsuite>')
            data=manifest.test_results(p)
            self.assertEqual(data['counts'],{'passed':1,'failed':1,'skipped':1})
            self.assertEqual(data['skipped'],['rom'])
            p.write_text('<testsuite/>')
            with self.assertRaises(ValueError):manifest.test_results(p)

    def assets(self,root):
        payload=b'tested native payload';digest=hashlib.sha256(payload).hexdigest()
        for name,package,binary in [('windows','Oracles-Windows-x64.zip','Oracles-Windows-x64/Oracles.exe'),('switch','Oracles-Switch.zip','switch/oracle-ages-native/oracles-switch.nro')]:
            with zipfile.ZipFile(root/package,'w') as z:z.writestr(binary,payload)
        (root/'oracles-switch.nro').write_bytes(payload)
        (root/'Oracles-macOS-Intel.dmg').write_bytes(b'tested dmg')
        (root/'oracle-ages-native-source.tar.gz').write_bytes(b'source')
        for name,files in [('windows',['Oracles-Windows-x64.zip']),('macos',['Oracles-macOS-Intel.dmg']),('switch',['Oracles-Switch.zip','oracles-switch.nro'])]:
            data={'commit':'a'*40,'source_dirty':False,'rom_supplied':False,'binary':{'sha256':digest},
                  'packages':{f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in files}}
            (root/f'{name}-BUILD.json').write_text(json.dumps(data))
        (root/'windows-SMOKE.json').write_text(json.dumps({'passed':True,'executable_sha256':digest}))

    def test_candidate_packages_and_provenance(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.assets(root)
            release.prepare(root,'a'*40)
            self.assertEqual(len((root/'SHA256SUMS.txt').read_text().splitlines()),9)
            (root/'SHA256SUMS.txt').unlink();(root/'RELEASE-NOTES.md').unlink()
            # No recompression/replacement may change an approved artifact.
            (root/'Oracles-macOS-Intel.dmg').write_bytes(b'replaced')
            with self.assertRaises(ValueError):release.prepare(root,'a'*40)

    def test_wrong_commit_and_failed_windows_gate(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.assets(root)
            with self.assertRaises(ValueError):release.prepare(root,'b'*40)
            (root/'windows-SMOKE.json').write_text(json.dumps({'passed':False,'executable_sha256':'0'*64}))
            with self.assertRaises(ValueError):release.prepare(root,'a'*40)

if __name__=='__main__':unittest.main()

class DraftCommandTests(unittest.TestCase):
    def test_draft_keeps_notes_and_never_publishes(self):
        import os,subprocess
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'release-assets').mkdir();(root/'bin').mkdir()
            (root/'release-assets/RELEASE-NOTES.md').write_text('Reviewed candidate notes')
            (root/'release-assets/package.zip').write_bytes(b'package')
            fake=root/'bin/gh'
            fake.write_text('''#!/usr/bin/env python3
import os,sys,json,pathlib
if sys.argv[1:3]==['release','view']:sys.exit(1)
args=sys.argv[1:]
assert args[:2]==['release','create']
assert '--draft' in args and '--latest=false' in args
notes=pathlib.Path(args[args.index('--notes-file')+1])
assert notes.read_text()=='Reviewed candidate notes'
assert not any(x.endswith('RELEASE-NOTES.md') for x in args)
pathlib.Path(os.environ['RECORD']).write_text(json.dumps(args))
''')
            fake.chmod(0o755)
            env=dict(os.environ,PATH=str(root/'bin')+os.pathsep+os.environ['PATH'],
                     RUNNER_TEMP=str(root),GITHUB_SHA='a'*40,RELEASE_TAG='v1.0.2-rc.1',
                     EVENT_NAME='workflow_dispatch',PRERELEASE='true',GITHUB_REPOSITORY='owner/repository',RECORD=str(root/'record.json'))
            subprocess.run(['bash',str(ROOT/'tools/ci/create_draft.sh')],cwd=root,env=env,check=True)
            args=json.loads((root/'record.json').read_text())
            self.assertIn('--prerelease',args)
            self.assertNotIn('--verify-tag',args)

class SourceArchiveTests(unittest.TestCase):
    def test_gitlinks_rom_exclusion_and_exact_committed_bytes(self):
        import subprocess,tarfile
        source=module('source_archive')
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'repo';root.mkdir()
            def git(*args):return subprocess.check_output(['git','-C',str(root),*args],stderr=subprocess.DEVNULL,text=True).strip()
            git('init');git('config','user.name','CI test');git('config','user.email','ci@example.invalid')
            (root/'native.c').write_text('committed source')
            (root/'roms').mkdir();(root/'roms/private.gbc').write_bytes(b'not for distribution')
            git('add','.');git('commit','-m','fixture')
            ref=git('rev-parse','HEAD')
            git('update-index','--add','--cacheinfo','160000,'+ref+',ref/disassembly')
            git('commit','-m','reference gitlink')
            (root/'ref/disassembly').mkdir(parents=True)
            (root/'native.c').write_text('uncommitted mutation')
            archive=Path(tmp)/'source.tar.gz';source.create(archive,root)
            with tarfile.open(archive) as tar:
                self.assertEqual(tar.extractfile('oracle-ages-native/native.c').read(),b'committed source')
                self.assertFalse(any(n.endswith('.gbc') for n in tar.getnames()))
                refs=json.load(tar.extractfile('oracle-ages-native/REFERENCE-SUBMODULES.json'))
                self.assertEqual(refs,{'ref/disassembly':ref})
