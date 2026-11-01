"""
Live graph for BMP180 data coming from STM32 over UART.

Expected line format (same as main.c prints):
    T=24.3 C  P=87175 Pa  Alt=1250.91 m

Install once:   pip install pyserial matplotlib
Run:            python live_plot.py

NOTE: ScriptCommunicator / any other program must be DISCONNECTED from the
COM port first, kahile ek palta ek matra program le port use garna sakxa.
"""
import re
import sys
from collections import deque

import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

PORT = "COM14"       # <-- apna port yaha rakha
BAUD = 115200
WINDOW = 120         # kati ota latest sample dekhaune

LINE_RE = re.compile(r"T=(-?[\d.]+)\s*C\s+P=(-?\d+)\s*Pa\s+Alt=(-?[\d.]+)\s*m")

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.2)
except serial.SerialException as e:
    sys.exit(f"Port {PORT} khulena: {e}\nScriptCommunicator close gara ani feri try gara.")

temps = deque(maxlen=WINDOW)
press = deque(maxlen=WINDOW)
alts  = deque(maxlen=WINDOW)
idx   = deque(maxlen=WINDOW)
count = 0

fig, (ax_t, ax_p, ax_a) = plt.subplots(3, 1, sharex=True, figsize=(9, 7))
fig.suptitle("BMP180 live data (STM32F103C8)")

(line_t,) = ax_t.plot([], [], color="tab:red")
(line_p,) = ax_p.plot([], [], color="tab:blue")
(line_a,) = ax_a.plot([], [], color="tab:green")

ax_t.set_ylabel("Temp (°C)")
ax_p.set_ylabel("Pressure (Pa)")
ax_a.set_ylabel("Altitude (m)")
ax_a.set_xlabel("Sample (1 per second)")
for ax in (ax_t, ax_p, ax_a):
    ax.grid(True, alpha=0.3)


def update(_frame):
    global count
    # buffer ma bhako sabai line padh
    while ser.in_waiting:
        raw = ser.readline().decode(errors="ignore").strip()
        m = LINE_RE.search(raw)
        if not m:
            continue
        count += 1
        idx.append(count)
        temps.append(float(m.group(1)))
        press.append(int(m.group(2)))
        alts.append(float(m.group(3)))

    if not idx:
        return line_t, line_p, line_a

    line_t.set_data(idx, temps)
    line_p.set_data(idx, press)
    line_a.set_data(idx, alts)

    for ax, data in ((ax_t, temps), (ax_p, press), (ax_a, alts)):
        lo, hi = min(data), max(data)
        pad = max((hi - lo) * 0.2, 0.5)
        ax.set_ylim(lo - pad, hi + pad)
    ax_a.set_xlim(idx[0], max(idx[-1], idx[0] + 10))

    ax_t.set_title(
        f"T = {temps[-1]:.1f} °C   P = {press[-1]} Pa   Alt = {alts[-1]:.2f} m",
        fontsize=10,
    )
    return line_t, line_p, line_a


ani = FuncAnimation(fig, update, interval=500, cache_frame_data=False)
plt.tight_layout()
try:
    plt.show()
finally:
    ser.close()