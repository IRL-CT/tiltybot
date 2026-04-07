#!/usr/bin/env python3
"""Plot motor positions and sent values from tilty logs."""
import sys, re
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("Usage: plot_tilty.py <logfile>")
    sys.exit(1)

times, m1s, m2s, betas, gammas = [], [], [], [], []
t0 = None

for line in open(sys.argv[1]):
    m = re.search(r'TILTY t=(\d+).*?b=([-\d.]+).*?g=([-\d.]+).*?m1=(\d+).*?m2=(\d+)', line)
    if not m:
        continue
    t = int(m.group(1))
    if t0 is None: t0 = t
    times.append((t - t0) / 1000.0)
    betas.append(float(m.group(2)))
    gammas.append(float(m.group(3)))
    m1s.append(int(m.group(4)))
    m2s.append(int(m.group(5)))

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8), sharex=True)

ax1.plot(times, m1s, 'b-', alpha=0.7, label='M1 (tilt motor)', linewidth=0.8)
ax1.plot(times, m2s, 'r-', alpha=0.7, label='M2 (pan motor)', linewidth=0.8)
ax1.axhline(y=2048, color='gray', linestyle='--', alpha=0.3, label='Center (2048)')
ax1.axhline(y=2957, color='orange', linestyle=':', alpha=0.3, label='Limits')
ax1.axhline(y=1139, color='orange', linestyle=':', alpha=0.3)
ax1.set_ylabel('Motor Position')
ax1.legend()
ax1.set_title('Motor Positions')

ax2.plot(times, betas, 'b-', alpha=0.7, label='b (sent tilt)', linewidth=0.8)
ax2.plot(times, gammas, 'r-', alpha=0.7, label='g (sent pan)', linewidth=0.8)
ax2.axhline(y=0, color='gray', linestyle='--', alpha=0.3)
ax2.set_ylabel('Degrees')
ax2.set_xlabel('Time (s)')
ax2.legend()
ax2.set_title('Sent Values (b=tilt, g=pan)')

plt.tight_layout()
out = sys.argv[1].replace('.log', '.png')
plt.savefig(out, dpi=150)
print(f"Saved: {out}")
plt.show()
