"""Enlarge the 16x2 LCD window from 65 x 14 to 65 x 15 (0.5 mm top and bottom, centre unchanged)
so its top edge lines up with the enlarged OLED windows again (Y 77.5). Pocket and frame unchanged."""
import sys
import numpy as np
import trimesh
import manifold3d as m3d

src, dst = sys.argv[1], sys.argv[2]
mesh = trimesh.load(src, force="mesh")
lo = mesh.bounds[0]
plate = m3d.Manifold(m3d.Mesh(vert_properties=np.asarray(mesh.vertices, dtype=np.float32),
                              tri_verts=np.asarray(mesh.faces, dtype=np.uint32)))
x0, x1, y0, y1 = 156.0, 221.0, 62.5, 77.5            # relative to the part corner
win = m3d.Manifold.cube((x1 - x0, y1 - y0, 3.02)).translate((lo[0] + x0, lo[1] + y0, lo[2] - 1.0))
plate = plate - win
out = plate.to_mesh()
res = trimesh.Trimesh(vertices=out.vert_properties[:, :3], faces=out.tri_verts)
print("result:", len(res.faces), "faces, watertight:", res.is_watertight,
      "bbox unchanged:", np.allclose(res.bounds, mesh.bounds, atol=1e-3))
res.export(dst)
print("saved", dst)
