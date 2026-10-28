#!/usr/bin/env python3
"""Linux-only observational probe; not a battery, window latency, or benchmark claim."""
import argparse
import json
import os
from pathlib import Path
import selectors
import statistics
import subprocess
import time


def proc_sample(pid):
    tail = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    status = {}
    for line in Path(f'/proc/{pid}/status').read_text().splitlines():
        if ':' in line:
            key, value = line.split(':', 1)
            status[key] = value.strip()
    return {
        'cpu_ticks': int(tail[11]) + int(tail[12]),
        'rss_kib': int(status['VmRSS'].split()[0]),
        'peak_rss_kib': int(status['VmHWM'].split()[0]),
        'voluntary_switches': int(status['voluntary_ctxt_switches']),
        'nonvoluntary_switches': int(status['nonvoluntary_ctxt_switches']),
    }


def probe(binary, render_dir, samples, idle_seconds):
    args = [str(binary)]
    if render_dir:
        render_dir.mkdir(parents=True, exist_ok=True)
        args.append(str(render_dir))
    p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True, bufsize=1)
    sel = selectors.DefaultSelector()
    sel.register(p.stdout, selectors.EVENT_READ)

    def receive():
        if not sel.select(30):
            raise RuntimeError('No response in 30 seconds')
        line = p.stdout.readline()
        if not line:
            raise RuntimeError('Host exited before a response')
        return json.loads(line)

    def send(command):
        start = time.perf_counter_ns()
        p.stdin.write(command + '\n')
        p.stdin.flush()
        value = receive()
        elapsed = (time.perf_counter_ns() - start) / 1e6
        if 'error' in value:
            raise RuntimeError(value)
        return value, elapsed

    try:
        initial = receive()
        if 'views' not in initial:
            raise RuntimeError('Expected initial snapshot')
        before = proc_sample(p.pid)
        t0 = time.monotonic()
        time.sleep(idle_seconds)
        after = proc_sample(p.pid)
        idle_elapsed = time.monotonic() - t0
        unchanged, _ = send('set-int count 0')
        if unchanged['patches'] != initial['patches'] or unchanged['renders'] != initial['renders']:
            raise RuntimeError('Same-value update changed views')
        elapsed_values = []
        for i in range(1, samples + 1):
            final, elapsed = send(f'set-int count {i}')
            if final['views']['counter'] != f'Count: {i}':
                raise RuntimeError('Incorrect counter output')
            elapsed_values.append(elapsed)
        end = proc_sample(p.pid)
        renderer_stats, _ = send('renderer-stats')
        if render_dir and (renderer_stats['cache_builds'] != 1 or
                           renderer_stats['render_frames'] != samples + 1):
            raise RuntimeError('Unexpected native render/cache counts')
        stopped, _ = send('quit')
        p.wait(timeout=5)
        stderr = p.stderr.read()
        if p.returncode:
            raise RuntimeError(f'Host exited {p.returncode}: {stderr}')
        return {
            'binary': binary.name,
            'mode': 'native PNG export' if render_dir else 'console state only',
            'samples': samples,
            'idle_observation_seconds': idle_elapsed,
            'clock_ticks_per_second': os.sysconf('SC_CLK_TCK'),
            'idle_cpu_ticks_delta': after['cpu_ticks'] - before['cpu_ticks'],
            'idle_voluntary_switches_delta': after['voluntary_switches'] - before['voluntary_switches'],
            'idle_nonvoluntary_switches_delta': after['nonvoluntary_switches'] - before['nonvoluntary_switches'],
            'initial_rss_kib': before['rss_kib'],
            'final_rss_kib': end['rss_kib'],
            'peak_rss_kib': end['peak_rss_kib'],
            'command_response_ms_median': statistics.median(elapsed_values),
            'command_response_ms_max': max(elapsed_values),
            'command_response_ms_samples': elapsed_values,
            'initial_patches': initial['patches'],
            'final_patches': final['patches'],
            'renderer_stats': renderer_stats,
            'quit_response': stopped,
            'notes': [
                'One Linux cloud process observation, not a cross-framework comparison.',
                'CPU ticks have finite resolution; zero delta does not prove zero energy.',
                'RSS includes this process resident mappings; it is not total system or GPU memory.',
                'Timing includes stdin/stdout protocol, state work, JSON, and PNG export if enabled.',
                'This is not input-to-display latency, FPS, battery life, or desktop-window testing.'
            ],
        }
    finally:
        sel.close()
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(timeout=3)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--render-dir', type=Path)
    parser.add_argument('--samples', type=int, default=20)
    parser.add_argument('--idle-seconds', type=float, default=2)
    args = parser.parse_args()
    if not 1 <= args.samples <= 1000 or not 0.1 <= args.idle_seconds <= 60:
        parser.error('samples must be 1..1000 and idle-seconds 0.1..60')
    print(json.dumps(probe(args.binary.resolve(), args.render_dir, args.samples,
                           args.idle_seconds), indent=2))
