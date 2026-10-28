#!/usr/bin/env python3
import os,sys,json
from pathlib import Path
from manifest import test_results
p=Path(sys.argv[1])
if p.exists():
    data=test_results(p)
    text='Core test results: '+json.dumps(data,indent=2)+'\nCommercial ROM replays are skipped when no ROM is supplied.\n'
else:
    text='No test report produced: inspect the build/test failure.\n'
print(text)
if os.getenv('GITHUB_STEP_SUMMARY'):
    with open(os.environ['GITHUB_STEP_SUMMARY'],'a') as f:f.write('```text\n'+text+'```\n')
