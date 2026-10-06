"""Fix the MBL600 encoder mount in the Atlas 210 front plate STL, keeping everything else.
MBL600 datasheet: flange D61, body D43, panel cutout D44, 3x M3 studs on PCD 50.8 at 120 deg
(one at 12 o'clock), panel holes D3.5.
Changes (all centred on the existing encoder centre):
  - plug the old 3 mounting holes (two D4.1 slightly off position, top slot only 3 mm wide)
  - through hole D41 -> D44.4 (cutout D44 + print allowance)
  - front recess D61 -> D61.8, same depth (1 mm)
  - 3 new holes D3.8 exactly on PCD 50.8 at 90 / 210 / 330 deg
"""
import sys
import numpy as np
import trimesh
import manifold3d as m3d

src, dst = sys.argv[1], sys.argv[2]
mesh = trimesh.load(src, force="mesh")
print("loaded:", len(mesh.faces), "faces, watertight:", mesh.is_watertight, "volume:", round(mesh.volume, 1))

lo = mesh.bounds[0]
cx, cy = lo[0] + 117.0, lo[1] + 36.5          # encoder centre (measured from the slices)
print(f"encoder centre at X={cx:.2f} Y={cy:.2f}")

plate = m3d.Manifold(m3d.Mesh(vert_properties=np.asarray(mesh.vertices, dtype=np.float32),
                              tri_verts=np.asarray(mesh.faces, dtype=np.uint32)))
SEG = 128


def cyl(x, y, r, z0, z1):
    return m3d.Manifold.cylinder(z1 - z0, r, r, SEG).translate((x, y, z0))


def box(x0, y0, z0, x1, y1, z1):
    return m3d.Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


# 1) plug the old holes (they only exist between z=1 and z=4, inside solid plate)
plugs = (cyl(lo[0] + 94.5, lo[1] + 24.5, 2.6, 1.0, 4.0)
         + cyl(lo[0] + 139.5, lo[1] + 24.5, 2.6, 1.0, 4.0)
         + box(lo[0] + 114.0, lo[1] + 60.5, 1.0, lo[0] + 120.0, lo[1] + 64.5, 4.0))
plate = plate + plugs

# 2) through hole D44.4, 3) front recess D61.8 x 1 mm, 4) three D3.8 holes on PCD 50.8
cut = cyl(cx, cy, 44.4 / 2, -1.0, 10.0) + cyl(cx, cy, 61.8 / 2, -1.0, 1.0)
r = 50.8 / 2
for ang in (90, 210, 330):
    a = np.radians(ang)
    cut = cut + cyl(cx + r * np.cos(a), cy + r * np.sin(a), 3.8 / 2, -1.0, 10.0)
plate = plate - cut

out = plate.to_mesh()
res = trimesh.Trimesh(vertices=out.vert_properties[:, :3], faces=out.tri_verts)
print("result:", len(res.faces), "faces, watertight:", res.is_watertight, "volume:", round(res.volume, 1))
print("bbox unchanged:", np.allclose(res.bounds, mesh.bounds, atol=1e-3))
res.export(dst)
print("saved", dst)
