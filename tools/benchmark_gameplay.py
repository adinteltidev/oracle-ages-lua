#!/usr/bin/env python3
"""Compare C-only and C+Lua engines: separate correctness traces and timed trials.

Requires the player's local Ages ROM. Private setup states use a TemporaryDirectory.
No timing is accepted from a run that computes per-frame hashes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cache(path):
    if not path.exists():
        return {}
    wanted = {"CMAKE_BUILD_TYPE", "CMAKE_C_COMPILER", "CMAKE_C_FLAGS", "CMAKE_C_FLAGS_RELEASE", "CMAKE_OSX_SYSROOT", "CMAKE_OSX_ARCHITECTURES",
              "CMAKE_OSX_DEPLOYMENT_TARGET", "ORACLES_LUA_GAMEPLAY", "ORACLES_LUA_BLOCK_PUZZLES"}
    result = {}
    for line in path.read_text().splitlines():
        if "=" in line and ":" in line.split("=", 1)[0]:
            key, value = line.split("=", 1)
            key = key.split(":", 1)[0]
            if key in wanted:
                result[key] = value
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("control", type=Path)
    p.add_argument("lua", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--frames", type=int, default=2000)
    p.add_argument("--verify-frames", type=int, default=320)
    p.add_argument("--warmup", type=int, default=160)
    p.add_argument("--trials", type=int, default=3)
    p.add_argument("--blocks", type=int, default=4)
    p.add_argument("--verify-only", action="store_true")
    a = p.parse_args()
    if not (80 <= a.frames <= 1000000 and 80 <= a.verify_frames <= 1000000 and
            0 <= a.warmup <= 1000000 and a.warmup % 80 == 0 and a.trials >= 3 and 1 <= a.blocks <= 8):
        p.error("Use frames >= 80, warmup divisible by 80, trials >= 3 and blocks 1..8")
    binaries = {"c": a.control.resolve(), "lua": a.lua.resolve()}
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for path in binaries.values():
        if not path.is_file():
            p.error(f"Missing executable: {path}")
    configs = {mode: cache(path.parent / "CMakeCache.txt") for mode, path in binaries.items()}
    # Require actual C-only and integrated Lua builds, not a runtime toggle in one binary.
    if configs["c"].get("ORACLES_LUA_GAMEPLAY") != "OFF":
        p.error("Control must be built with ORACLES_LUA_GAMEPLAY=OFF")
    if any(configs["lua"].get(k) != "ON" for k in ("ORACLES_LUA_GAMEPLAY", "ORACLES_LUA_BLOCK_PUZZLES")):
        p.error("Lua build must enable both gameplay and block integration")
    for key in ("CMAKE_BUILD_TYPE", "CMAKE_C_COMPILER", "CMAKE_C_FLAGS", "CMAKE_C_FLAGS_RELEASE", "CMAKE_OSX_SYSROOT", "CMAKE_OSX_ARCHITECTURES", "CMAKE_OSX_DEPLOYMENT_TARGET"):
        if configs["c"].get(key) != configs["lua"].get(key):
            p.error(f"Build settings differ: {key}")
    if configs["c"].get("CMAKE_BUILD_TYPE") != "Release":
        p.error("Use Release builds for timing")
    try:
        cpu = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        cpu = platform.processor()
    report = {"status": "running", "host": {"system": platform.system(), "release": platform.release(),
              "machine": platform.machine(), "processor": cpu},
              "binaries": {m: {"path": str(b), "sha256": sha(b), "config": configs[m]} for m, b in binaries.items()},
              "verification": {}, "trials": {}, "summary": {},
              "limits": ["Headless engine CPU/wall time; excludes SDL presentation and audio device output",
                         "Blocks are injected into a naturally prepared room: a controlled stress fixture, not natural puzzle progression",
                         "No separate GC pause or total process memory measurement; heap fields describe the Lua VM",
                         "Does not validate Windows or Nintendo Switch hardware"]}
    report_file = out / "summary.json"
    source_root = Path(__file__).resolve().parents[1]
    report["source"] = {
        "base_commit": subprocess.check_output(["git", "-C", str(source_root), "rev-parse", "HEAD"], text=True).strip(),
        "tracked_diff_sha256": hashlib.sha256(subprocess.check_output(["git", "-C", str(source_root), "diff", "HEAD"])).hexdigest(),
        "benchmark_c_sha256": sha(source_root / "tools/benchmark_gameplay.c"),
        "runner_sha256": sha(Path(__file__).resolve()),
    }

    def run(mode, label, args):
        command = [str(binaries[mode]), *map(str, args)]
        with (out / f"{label}.log").open("w") as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)

    def read(path, mode, scenario, timed):
        data = json.loads(path.read_text())
        if data["mode"] != mode or data["scenario"] != scenario or data["timed"] != timed:
            raise ValueError(f"Unexpected result metadata: {path}")
        if mode == "lua" and scenario == "blocks" and not all(n > 0 for n in data["coverage"]):
            raise ValueError("Positive coverage required in all four block states")
        return data

    try:
        with tempfile.TemporaryDirectory(prefix="oracles-lua-benchmark-") as private:
            state = Path(private) / "room.state"
            run("c", "prepare", ["--prepare", state])
            common = ["--state", state, "--warmup", a.warmup, "--blocks", a.blocks]
            for label, scenario, reload_every in (("idle", "idle", 0), ("blocks", "blocks", 0), ("blocks-reload", "blocks", 13)):
                verified = {}
                for mode in ("c", "lua"):
                    prefix = f"verify-{label}-{mode}"
                    trace = out / f"{prefix}.trace"
                    result = out / f"{prefix}.json"
                    extra = ["--reload-every", reload_every, "--reload-state", Path(private)/f"reload-{mode}.state"] if reload_every else []
                    run(mode, prefix, common + ["--scenario", scenario, "--frames", a.verify_frames,
                                               "--trace", trace, "--output", result] + extra)
                    verified[mode] = read(result, mode, scenario, False)
                    if reload_every and (not verified[mode]["reloads"] or not verified[mode]["reloaded_block_states_mask"] & (1 << 3)):
                        raise ValueError("Require a state reload with active blocks waiting for their delay")
                expected = (out / f"verify-{label}-c.trace").read_text().splitlines()
                actual = (out / f"verify-{label}-lua.trace").read_text().splitlines()
                if len(expected) != a.verify_frames or actual != expected:
                    first = next((i for i, (c, l) in enumerate(zip(expected, actual)) if c != l), min(len(expected), len(actual)))
                    raise ValueError(f"Machine/sample digest mismatch: {label}, frame {first}")
                report["verification"][label] = {"passed": True, "frames": a.verify_frames, "results": verified}
                print(f"Verified {label}: {a.verify_frames} full machine/sample digests", flush=True)
            if not a.verify_only:
                for scenario in ("idle", "blocks"):
                    results = {"c": [], "lua": []}
                    for trial in range(a.trials):
                        for mode in (("c", "lua") if trial % 2 == 0 else ("lua", "c")):
                            prefix = f"timed-{scenario}-{trial+1}-{mode}"
                            result = out / f"{prefix}.json"
                            run(mode, prefix, common + ["--scenario", scenario, "--frames", a.frames, "--output", result])
                            results[mode].append(read(result, mode, scenario, True))
                        print(f"Timed {scenario}: trial {trial+1}/{a.trials}", flush=True)
                    c = statistics.median(r["cpu_ms"]["mean"] for r in results["c"])
                    lua = statistics.median(r["cpu_ms"]["mean"] for r in results["lua"])
                    overhead = (lua/c-1)*100
                    report["trials"][scenario] = results
                    report["summary"][scenario] = {"c_mean_cpu_ms_median": c, "lua_mean_cpu_ms_median": lua,
                        "overhead_percent": overhead, "within_proposed_10_percent": overhead <= 10,
                        "largest_lua_p99_cpu_ms": max(r["cpu_ms"]["p99"] for r in results["lua"])}
            report["status"] = "verified_only" if a.verify_only else "measured"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(report_file, flush=True)


if __name__ == "__main__":
    main()
