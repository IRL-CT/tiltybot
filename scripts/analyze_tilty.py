#!/usr/bin/env python3
"""Analyze tilty log for timing, stuck values, and update rates."""
import re
import sys

logfile = sys.argv[1] if len(sys.argv) > 1 else "logs/device-monitor-260407-112000.log"

lines = []
with open(logfile) as f:
    for line in f:
        # With timestamps: TILTY t=12345 b=... g=... -> m1=... m2=...
        m = re.match(r'TILTY t=(\d+) b=([\d.-]+) g=([\d.-]+) -> m1=(\d+) m2=(\d+)', line.strip())
        if m:
            lines.append({
                't': int(m.group(1)),
                'b': float(m.group(2)),
                'g': float(m.group(3)),
                'm1': int(m.group(4)),
                'm2': int(m.group(5)),
            })
        else:
            # Without timestamps (old format)
            m = re.match(r'TILTY b=([\d.-]+) g=([\d.-]+) -> m1=(\d+) m2=(\d+)', line.strip())
            if m:
                lines.append({
                    't': None,
                    'b': float(m.group(1)),
                    'g': float(m.group(2)),
                    'm1': int(m.group(3)),
                    'm2': int(m.group(4)),
                })

has_ts = lines and lines[0]['t'] is not None
print(f"Total TILTY lines: {len(lines)}")
if has_ts and len(lines) > 1:
    total_ms = lines[-1]['t'] - lines[0]['t']
    print(f"Duration: {total_ms/1000:.1f}s")
    print(f"Average update rate: {len(lines)/(total_ms/1000):.1f} Hz")
print()

# Timing analysis
if has_ts and len(lines) > 1:
    deltas = [lines[i]['t'] - lines[i-1]['t'] for i in range(1, len(lines))]
    print(f"Inter-update timing (ms):")
    print(f"  Min: {min(deltas)}  Max: {max(deltas)}  Avg: {sum(deltas)/len(deltas):.1f}")
    print(f"  Median: {sorted(deltas)[len(deltas)//2]}")
    
    # Distribution
    buckets = {'<10ms': 0, '10-20ms': 0, '20-50ms': 0, '50-100ms': 0, '>100ms': 0}
    for d in deltas:
        if d < 10: buckets['<10ms'] += 1
        elif d < 20: buckets['10-20ms'] += 1
        elif d < 50: buckets['20-50ms'] += 1
        elif d < 100: buckets['50-100ms'] += 1
        else: buckets['>100ms'] += 1
    print(f"  Distribution:")
    for k, v in buckets.items():
        print(f"    {k}: {v} ({100*v/len(deltas):.0f}%)")
    
    # Find gaps > 100ms
    gaps = [(i, d) for i, d in enumerate(deltas) if d > 100]
    if gaps:
        print(f"  Gaps > 100ms: {len(gaps)}")
        for idx, gap in gaps[:5]:
            print(f"    Line {idx+1}: {gap}ms (m1={lines[idx]['m1']}->{lines[idx+1]['m1']}, m2={lines[idx]['m2']}->{lines[idx+1]['m2']})")
    print()

# Motor step sizes
if len(lines) > 1:
    dm1 = [abs(lines[i]['m1'] - lines[i-1]['m1']) for i in range(1, len(lines))]
    dm2 = [abs(lines[i]['m2'] - lines[i-1]['m2']) for i in range(1, len(lines))]
    print(f"Motor step sizes per update:")
    print(f"  M1: avg={sum(dm1)/len(dm1):.1f} max={max(dm1)} (>{max(dm1)*0.088:.0f} deg)")
    print(f"  M2: avg={sum(dm2)/len(dm2):.1f} max={max(dm2)} (>{max(dm2)*0.088:.0f} deg)")
    big_m2 = [(i, d) for i, d in enumerate(dm2) if d > 200]
    if big_m2:
        print(f"  M2 jumps > 200 steps: {len(big_m2)}")
        for idx, d in big_m2[:10]:
            b = lines[idx+1]['b']
            print(f"    Line {idx+1}: {d} steps ({d*0.088:.0f} deg), beta={b:.1f}, g={lines[idx]['g']:.1f}->{lines[idx+1]['g']:.1f}")
    print()

# Stuck analysis
m1_runs = []
m2_runs = []
run1 = 1
run2 = 1
for i in range(1, len(lines)):
    if lines[i]['m1'] == lines[i-1]['m1']:
        run1 += 1
    else:
        if run1 > 1: m1_runs.append((i - run1, run1, lines[i-1]['m1']))
        run1 = 1
    if lines[i]['m2'] == lines[i-1]['m2']:
        run2 += 1
    else:
        if run2 > 1: m2_runs.append((i - run2, run2, lines[i-1]['m2']))
        run2 = 1

print(f"Stuck runs (same value):")
m1_runs.sort(key=lambda x: -x[1])
m2_runs.sort(key=lambda x: -x[1])
print(f"  M1 top 5:")
for start, length, val in m1_runs[:5]:
    dur = f" ({lines[start+length-1]['t']-lines[start]['t']}ms)" if has_ts else ""
    print(f"    {length} updates at m1={val}{dur}")
print(f"  M2 top 5:")
for start, length, val in m2_runs[:5]:
    dur = f" ({lines[start+length-1]['t']-lines[start]['t']}ms)" if has_ts else ""
    print(f"    {length} updates at m2={val}{dur}")
print()

# Gimbal lock analysis
print("Gimbal lock analysis:")
beta_at_clamp = sum(1 for l in lines if l['b'] >= 69.9)
print(f"  Beta at clamp (>=69.9): {beta_at_clamp}/{len(lines)} ({100*beta_at_clamp/len(lines):.0f}%)")
g_at_clamp = [l['g'] for l in lines if l['b'] >= 69.9]
if g_at_clamp:
    jumps = [abs(g_at_clamp[i]-g_at_clamp[i-1]) for i in range(1, len(g_at_clamp))]
    if jumps:
        print(f"  Gamma range when clamped: {min(g_at_clamp):.1f} to {max(g_at_clamp):.1f}")
        print(f"  Max gamma jump: {max(jumps):.1f} deg")
        print(f"  Jumps > 10 deg: {sum(1 for j in jumps if j > 10)}")
        print(f"  Jumps > 20 deg: {sum(1 for j in jumps if j > 20)}")

# Ranges
print()
m1_vals = [l['m1'] for l in lines]
m2_vals = [l['m2'] for l in lines]
print(f"M1 range: {min(m1_vals)} - {max(m1_vals)} (span: {max(m1_vals)-min(m1_vals)} = {(max(m1_vals)-min(m1_vals))*0.088:.0f} deg)")
print(f"M2 range: {min(m2_vals)} - {max(m2_vals)} (span: {max(m2_vals)-min(m2_vals)} = {(max(m2_vals)-min(m2_vals))*0.088:.0f} deg)")
