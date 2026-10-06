"""Fix the two 0.96" SSD1306 OLED openings in the Atlas 210 front plate, keeping the look.
Measured module: PCB 27.3 wide (height assumed <= 27.8), glass 26.8 x 19.3, active area 22 x 11,
active area top edge 6.37 mm below the PCB top edge (pin header side = +Y).
Window centres stay where they are (X 26.5 / 64.0, Y 71.5 relative to the part corner).
  - window 22 x 11 -> 23 x 12 (through the 2 mm front lip, z 0..2)
  - pocket 25 x 27 -> 27.8 x 28.4 (z 2..6: through the base plate and the frame behind it),
    PCB top edge = window centre + 5.5 + 6.37, +0.25 clearance
  - shared frame ring behind (z 4..6) enlarged so its walls stay 2 mm
"""
import sys
import numpy as np
import trimesh
import manifold3d as m3d

src, dst = sys.argv[1], sys.argv[2]
mesh = trimesh.load(src, force="mesh")
lo = mesh.bounds[0]
print("loaded:", len(mesh.faces), "faces, watertight:", mesh.is_watertight)
plate = m3d.Manifold(m3d.Mesh(vert_properties=np.asarray(mesh.vertices, dtype=np.float32),
                              tri_verts=np.asarray(mesh.faces, dtype=np.uint32)))


def box(x0, y0, z0, x1, y1, z1):          # coordinates relative to the part corner
    return m3d.Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((lo[0] + x0, lo[1] + y0, lo[2] + z0))


WIN_W, WIN_H = 23.0, 12.0
PCB_W, POCKET_H = 27.3 + 0.5, 28.4
ACTIVE_H, TOP_TO_ACTIVE, CLEAR = 11.0, 6.37, 0.25
wy = 71.5                                   # window centre Y (unchanged)
pocket_top = wy + ACTIVE_H / 2 + TOP_TO_ACTIVE + CLEAR
pocket_bot = pocket_top - POCKET_H
print(f"pocket Y {pocket_bot:.2f} .. {pocket_top:.2f}")

centres = (26.5, 64.0)
pockets_x = [(cx - PCB_W / 2, cx + PCB_W / 2) for cx in centres]

# frame ring behind both modules: inner box covers both pockets, walls 2 mm
in_x0, in_x1 = pockets_x[0][0] - 0.2, pockets_x[1][1] + 0.2
in_y0, in_y1 = pocket_bot - 0.02, pocket_top + 0.02
inner = box(in_x0, in_y0, 4.0, in_x1, in_y1, 6.0 + 0.01)
ring = box(in_x0 - 2.0, in_y0 - 2.0, 4.0, in_x1 + 2.0, in_y1 + 2.0, 6.0) - box(in_x0, in_y0, 3.9, in_x1, in_y1, 6.1)
print(f"frame inner X {in_x0:.2f}..{in_x1:.2f}  Y {in_y0:.2f}..{in_y1:.2f}")

plate = plate - inner                       # remove the old (too small) frame walls
plate = plate + ring                        # new 2 mm walls around the larger opening
for cx, (px0, px1) in zip(centres, pockets_x):
    plate = plate - box(px0, pocket_bot, 2.0 - 0.01, px1, pocket_top, 6.0 + 0.01)               # PCB pocket
    plate = plate - box(cx - WIN_W / 2, wy - WIN_H / 2, -1.0, cx + WIN_W / 2, wy + WIN_H / 2, 2.0 + 0.02)  # window

out = plate.to_mesh()
res = trimesh.Trimesh(vertices=out.vert_properties[:, :3], faces=out.tri_verts)
print("result:", len(res.faces), "faces, watertight:", res.is_watertight)
print("bbox unchanged:", np.allclose(res.bounds, mesh.bounds, atol=1e-3))
res.export(dst)
print("saved", dst)
