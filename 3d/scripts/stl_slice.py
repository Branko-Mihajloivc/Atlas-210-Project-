"""Slice a binary STL front plate and report the closed loops (outline, windows, pockets, holes)
at several depths along its thinnest axis."""
import sys
import numpy as np

path = sys.argv[1]
data = open(path, "rb").read()
n = int.from_bytes(data[80:84], "little")
rec = np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
tri = np.frombuffer(data, dtype=rec, count=n, offset=84)["v"].astype(np.float64)

pts = tri.reshape(-1, 3)
lo, hi = pts.min(0), pts.max(0)
size = hi - lo
print(f"triangles {n}")
print("bbox min", np.round(lo, 2), "max", np.round(hi, 2), "size", np.round(size, 2))
ax = int(np.argmin(size))                 # thickness axis
others = [i for i in range(3) if i != ax]
print(f"thickness axis = {'XYZ'[ax]} ({size[ax]:.2f} mm), plate = {size[others[0]]:.2f} x {size[others[1]]:.2f} mm")

# distinct heights of horizontal faces along the thickness axis (pocket floors, steps)
zs = np.round(tri[:, :, ax], 2)
flat = np.all(np.abs(zs - zs[:, :1]) < 1e-3, axis=1)
levels = sorted(set(zs[flat, 0]))
print("face levels along thickness axis:", [round(z - lo[ax], 2) for z in levels])


def slice_at(z):
    segs = []
    for t in tri:
        d = t[:, ax] - z
        if np.all(d > 0) or np.all(d < 0):
            continue
        p = []
        for i in range(3):
            a, b = t[i], t[(i + 1) % 3]
            da, db = d[i], d[(i + 1) % 3]
            if da == 0:
                p.append(a)
            if (da < 0 < db) or (db < 0 < da):
                s = da / (da - db)
                p.append(a + s * (b - a))
        if len(p) >= 2:
            segs.append((p[0][others], p[1][others]))
    # chain segments into loops
    key = lambda q: (round(q[0], 3), round(q[1], 3))
    adj = {}
    for a, b in segs:
        adj.setdefault(key(a), []).append(key(b))
        adj.setdefault(key(b), []).append(key(a))
    seen, loops = set(), []
    for start in adj:
        if start in seen:
            continue
        loop, cur, prev = [start], start, None
        seen.add(start)
        while True:
            nxt = [q for q in adj[cur] if q != prev and q not in seen]
            if not nxt:
                break
            prev, cur = cur, nxt[0]
            seen.add(cur)
            loop.append(cur)
        if len(loop) > 3:
            loops.append(np.array(loop))
    return loops


def describe(loop):
    x0, y0 = loop.min(0)
    x1, y1 = loop.max(0)
    w, h = x1 - x0, y1 - y0
    # area (shoelace) vs bbox area -> rectangle (~1.0) or circle (~0.785)
    x, y = loop[:, 0], loop[:, 1]
    area = 0.5 * abs(np.dot(x, np.roll(y, 1)) - np.dot(y, np.roll(x, 1)))
    fill = area / (w * h) if w * h > 0 else 0
    shape = "rect" if fill > 0.93 else ("round" if 0.70 < fill < 0.83 else f"other({fill:.2f})")
    return x0 - lo[others[0]], y0 - lo[others[1]], w, h, shape


depths = sorted(set([0.2] + [round((levels[i] + levels[i + 1]) / 2 - lo[ax], 2) for i in range(len(levels) - 1)]
                    + [round(size[ax] - 0.2, 2)]))
for dz in depths:
    z = lo[ax] + dz
    loops = slice_at(z)
    print(f"\n--- slice at depth {dz:.2f} mm from {'XYZ'[ax]}min: {len(loops)} loops ---")
    rows = sorted((describe(l) for l in loops), key=lambda r: -r[2] * r[3])
    for x0, y0, w, h, shape in rows:
        print(f"  {shape:10s} size {w:7.2f} x {h:6.2f}   at {'XYZ'[others[0]]}={x0:7.2f}..{x0 + w:7.2f}  "
              f"{'XYZ'[others[1]]}={y0:6.2f}..{y0 + h:6.2f}   center ({x0 + w / 2:6.2f},{y0 + h / 2:6.2f})")
