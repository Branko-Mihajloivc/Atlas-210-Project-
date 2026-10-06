"""Probe solid/open intervals of an STL slice along a line. args: stl depth axis(x|y) at
   e.g. stl_probe.py plate.stl 5 x 70  -> walk along X at Y=70 (coords relative to bbox min)."""
import sys
import numpy as np

path, depth, axis, at = sys.argv[1], float(sys.argv[2]), sys.argv[3], float(sys.argv[4])
data = open(path, "rb").read()
n = int.from_bytes(data[80:84], "little")
rec = np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
tri = np.frombuffer(data, dtype=rec, count=n, offset=84)["v"].astype(np.float64)
lo = tri.reshape(-1, 3).min(0)
z = lo[2] + depth
cross = []
for t in tri:
    d = t[:, 2] - z
    if np.all(d > 0) or np.all(d < 0):
        continue
    p = []
    for i in range(3):
        a, b = t[i], t[(i + 1) % 3]
        da, db = d[i], d[(i + 1) % 3]
        if (da < 0 <= db) or (db < 0 <= da):
            s = da / (da - db)
            p.append(a + s * (b - a))
    if len(p) != 2:
        continue
    (x0, y0), (x1, y1) = (p[0][0] - lo[0], p[0][1] - lo[1]), (p[1][0] - lo[0], p[1][1] - lo[1])
    if axis == "x":          # line y = at, report x crossings
        if (y0 <= at < y1) or (y1 <= at < y0):
            cross.append(x0 + (at - y0) * (x1 - x0) / (y1 - y0))
    else:                    # line x = at, report y crossings
        if (x0 <= at < x1) or (x1 <= at < x0):
            cross.append(y0 + (at - x0) * (y1 - y0) / (x1 - x0))
cross.sort()
print(f"depth {depth} line {'y' if axis == 'x' else 'x'}={at}: solid intervals along {axis}:")
for k in range(0, len(cross) - 1, 2):
    print(f"  {cross[k]:7.2f} .. {cross[k + 1]:7.2f}  (solid {cross[k + 1] - cross[k]:5.2f})")
for k in range(1, len(cross) - 1, 2):
    print(f"     gap {cross[k]:7.2f} .. {cross[k + 1]:7.2f}  (open {cross[k + 1] - cross[k]:5.2f})")
