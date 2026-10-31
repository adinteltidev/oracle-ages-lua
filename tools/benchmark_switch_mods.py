#!/usr/bin/env python3
"""Compare the Switch adapter on a native host; this never approves Switch hardware."""
import argparse
import json
from pathlib import Path
import shutil
import statistics
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runner', type=Path, required=True)
    p.add_argument('--rom', type=Path, required=True)
    p.add_argument('--state', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True, help='New evidence directory')
    p.add_argument('--frames', type=int, default=2400)
    p.add_argument('--trials', type=int, default=2)
    a = p.parse_args()
    if a.frames < 1 or a.trials < 1:
        p.error('frames and trials must be positive')
    a.output.mkdir(parents=True, exist_ok=False)
    root = Path(__file__).resolve().parents[1]
    packages = {'empty': [], 'overlay': ['overlay-info'], 'reader': ['room-reader'],
                'api2': ['api2-demo'], 'multiple': ['overlay-info', 'room-reader', 'api2-demo']}
    results = {key: [] for key in packages}
    order = ['multiple', 'empty', 'api2', 'overlay', 'reader']
    for trial in range(a.trials):
        for case in order if trial % 2 == 0 else reversed(order):
            data = (a.output / f'trial-{trial + 1}' / case).resolve()
            (data / 'mods').mkdir(parents=True)
            for name in packages[case]:
                shutil.copytree(root / 'examples/switch-mods' / name, data / 'mods' / name)
            if 'api2-demo' in packages[case]:
                (data / 'mods.cfg').write_text('setting api2-demo ' + b'Queue identity write'.hex() + ' 1\n')
            with (data / 'run.log').open('w') as log:
                subprocess.run([str(a.runner.resolve()), str(a.rom.resolve()), str(a.state.resolve()),
                                str(data) + '/', str(a.frames)], stdout=log, stderr=log, check=True)
            results[case].append({'cpu': json.loads((data / 'mods-process-cpu.json').read_text()),
                                  'adapter': json.loads((data / 'mods-performance.json').read_text())})
        print(f'Completed host trial {trial + 1}/{a.trials}', flush=True)
    baseline = statistics.mean(row['cpu']['mean_ms'] for row in results['empty'])
    summary = {}
    for case, rows in results.items():
        mean = statistics.mean(row['cpu']['mean_ms'] for row in rows)
        summary[case] = {'mean_cpu_ms': mean, 'overhead_percent': (mean / baseline - 1) * 100,
                         'peak_lua_bytes': max(row['adapter']['heap_peak'] for row in rows), 'trials': rows}
    report = {'hardware_tested': False, 'audio_measured': False, 'cadence_measured': False,
              'warmup_frames_per_trial': 120, 'measured_frames_per_trial': a.frames,
              'method': 'CLOCK_PROCESS_CPUTIME_ID; alternating trial order', 'cases': summary}
    (a.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print(a.output / 'summary.json')


if __name__ == '__main__':
    main()
