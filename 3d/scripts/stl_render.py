"""Render filled cross-sections of an STL plate (thin axis = Z) at several depths into one PNG.
Solid = black, open = white. Uses even-odd scanline fill of the sliced segments."""
import sys
import numpy as np
from PIL import Image, ImageDraw

path, out = sys.argv[1], sys.argv[2]
depths = [float(d) for d in sys.argv[3].split(",")]
S = 4  # pixels per mm

data = open(path, "rb").read()
n = int.from_bytes(data[80:84], "little")
rec = np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
tri = np.frombuffer(data, dtype=rec, count=n, offset=84)["v"].astype(np.float64)
lo = tri.reshape(-1, 3).min(0)
hi = tri.reshape(-1, 3).max(0)
W, H = int((hi[0] - lo[0]) * S) + 1, int((hi[1] - lo[1]) * S) + 1


def segments(z):
    segs = []
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
        if len(p) == 2:
            segs.append(((p[0][0] - lo[0]) * S, (p[0][1] - lo[1]) * S, (p[1][0] - lo[0]) * S, (p[1][1] - lo[1]) * S))
    return segs


panels = []
for dz in depths:
    z = lo[2] + dz
    segs = segments(z)
    img = np.zeros((H, W), dtype=bool)
    for row in range(H):
        y = row + 0.5
        xs = []
        for x0, y0, x1, y1 in segs:
            if (y0 <= y < y1) or (y1 <= y < y0):
                xs.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
        xs.sort()
        for k in range(0, len(xs) - 1, 2):
            a, b = int(round(xs[k])), int(round(xs[k + 1]))
            img[row, max(a, 0):min(b, W)] = True
    im = Image.fromarray(np.where(img, 40, 255).astype(np.uint8)).transpose(Image.FLIP_TOP_BOTTOM).convert("RGB")
    d = ImageDraw.Draw(im)
    d.text((6, 4), f"depth {dz:.1f} mm from front (Z={z:.1f})", fill=(220, 0, 0))
    for mm in range(0, int(hi[0] - lo[0]) + 1, 10):      # 10 mm ticks on the top edge
        d.line([(mm * S, 0), (mm * S, 6 if mm % 50 else 14)], fill=(0, 120, 255))
    panels.append(im)

sheet = Image.new("RGB", (W, (H + 8) * len(panels)), "white")
for i, p in enumerate(panels):
    sheet.paste(p, (0, i * (H + 8)))
sheet.save(out)
print("saved", out, sheet.size)
