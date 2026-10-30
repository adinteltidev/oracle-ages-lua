#!/usr/bin/env python3
"""Record evidence from this run only; historical device reports are not inherited."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import xml.etree.ElementTree as ET


def test_results(path):
    root = ET.parse(path).getroot()
    cases = list(root.iter('testcase'))
    result = {'passed': [], 'failed': [], 'skipped': []}
    for case in cases:
        kind = 'skipped' if case.find('skipped') is not None or case.get('status') == 'notrun' else \
               'failed' if case.find('failure') is not None or case.find('error') is not None else 'passed'
        result[kind].append(case.get('name', 'unnamed'))
    if not cases:
        raise ValueError('Test report contains no tests')
    return dict(result, counts={key: len(value) for key, value in result.items()})


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform', required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--audit', type=Path)
    p.add_argument('--junit', type=Path)
    p.add_argument('--evidence', type=Path)
    p.add_argument('--toolchain', type=Path)
    p.add_argument('--compiler', required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    binary = a.binary.read_bytes()
    digest = hashlib.sha256(binary).hexdigest()
    audit = json.loads(a.audit.read_text()) if a.audit else None
    if audit and audit.get('nro_sha256', audit.get('sha256')) != digest:
        raise ValueError('Audit does not describe this binary')
    tests = test_results(a.junit) if a.junit else None
    evidence = json.loads(a.evidence.read_text()) if a.evidence else None
    if tests and tests['failed']:
        raise ValueError('Cannot package failed tests')
    if evidence and evidence.get('passed') is not True:
        raise ValueError('Failed evidence')
    data = {'schema_version': 1, 'platform': a.platform, 'commit': sha,
            'source_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], text=True).strip()),
            'run_id': os.getenv('GITHUB_RUN_ID'), 'run_attempt': os.getenv('GITHUB_RUN_ATTEMPT'),
            'repository': os.getenv('GITHUB_REPOSITORY'),
            'features': {key: os.getenv(env, 'OFF') == 'ON' for key, env in
                         [('lua_gameplay', 'ORACLES_LUA_GAMEPLAY'), ('lua_block_puzzles', 'ORACLES_LUA_BLOCK_PUZZLES')]},
            'compiler': a.compiler, 'host': platform.platform(),
            'binary': {'name': a.binary.name, 'bytes': len(binary), 'sha256': digest},
            'audit': audit, 'tests': tests, 'evidence': evidence,
            'rom_supplied': False, 'rom_replay_executed': False, 'hardware_tested': False,
            'toolchain': json.loads(a.toolchain.read_text()) if a.toolchain else None}
    if os.getenv('GITHUB_ACTIONS') == 'true' and data['source_dirty']:
        raise ValueError('Tracked sources changed during the CI build')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(data, indent=2) + '\n')
    if os.getenv('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as f:
            f.write(f"### {a.platform}\n\nCommit: `{sha}`\n\nSHA256: `{digest}`\n\n")
            f.write('ROM-dependent gameplay and Switch hardware tests were not executed.\n\n')
            if tests:
                f.write(f"Tests: {tests['counts']}\n\n")
                f.write('Skipped: ' + ', '.join(tests['skipped']) + '\n')
    print(a.output)


if __name__ == '__main__':
    main()
