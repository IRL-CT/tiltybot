#!/usr/bin/env python3
"""Correlate SSL errors with TILTY update gaps."""
import re
import sys

logfile = sys.argv[1] if len(sys.argv) > 1 else "logs/device-monitor-260407-145327.log"

ssl_errors = []
tilty_lines = []

with open(logfile) as f:
    for line in f:
        line = line.strip()
        # ESP-IDF log timestamps: E (12345) ...
        m = re.match(r'E \((\d+)\) esp-tls', line)
        if m:
            ssl_errors.append(int(m.group(1)))
        
        m = re.match(r'.*httpd_accept_conn.*failed', line)
        if m:
            pass  # these follow the ssl errors
        
        m = re.match(r'TILTY t=(\d+) b=([\d.-]+) g=([\d.-]+) -> m1=(\d+) m2=(\d+)', line)
        if m:
            tilty_lines.append({
                't': int(m.group(1)),
                'b': float(m.group(2)),
                'g': float(m.group(3)),
                'm1': int(m.group(4)),
                'm2': int(m.group(5)),
            })

print(f"SSL errors: {len(ssl_errors)}")
print(f"TILTY updates: {len(tilty_lines)}")
if not tilty_lines:
    sys.exit(0)

print(f"\nTILTY time range: {tilty_lines[0]['t']} - {tilty_lines[-1]['t']} ms")
print()

# Find all TILTY gaps > 50ms
gaps = []
for i in range(1, len(tilty_lines)):
    dt = tilty_lines[i]['t'] - tilty_lines[i-1]['t']
    if dt > 50:
        gaps.append((tilty_lines[i-1]['t'], tilty_lines[i]['t'], dt, i))

print(f"TILTY gaps > 50ms: {len(gaps)}")
print()

# For each gap, check if SSL errors occurred during or just before it
print(f"{'Gap#':>4} {'Start':>8} {'End':>8} {'Duration':>8}  SSL errors during gap")
print("-" * 65)
for idx, (start, end, dt, line_idx) in enumerate(gaps):
    # Look for SSL errors within [start-100, end+100] window
    nearby_ssl = [s for s in ssl_errors if start - 100 <= s <= end + 100]
    marker = f" <-- {len(nearby_ssl)} SSL error(s)" if nearby_ssl else ""
    print(f"{idx+1:4d} {start:8d} {end:8d} {dt:7d}ms{marker}")

# Summary
print()
gaps_with_ssl = sum(1 for start, end, dt, _ in gaps 
                    if any(start - 100 <= s <= end + 100 for s in ssl_errors))
gaps_without_ssl = len(gaps) - gaps_with_ssl
print(f"Gaps with nearby SSL errors: {gaps_with_ssl}/{len(gaps)}")
print(f"Gaps WITHOUT SSL errors: {gaps_without_ssl}/{len(gaps)}")

# SSL errors outside of TILTY session
if tilty_lines:
    ssl_during = [s for s in ssl_errors if tilty_lines[0]['t'] <= s <= tilty_lines[-1]['t']]
    ssl_before = [s for s in ssl_errors if s < tilty_lines[0]['t']]
    print(f"\nSSL errors before TILTY session: {len(ssl_before)}")
    print(f"SSL errors during TILTY session: {len(ssl_during)}")
    if ssl_during:
        print(f"SSL error times during session: {ssl_during}")
