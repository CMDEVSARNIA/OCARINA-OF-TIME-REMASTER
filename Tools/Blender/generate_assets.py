"""
Emberveil art kit generator (original assets).

Builds every model procedurally in Blender and exports one FBX per asset for Unreal.
Conventions: metres, Z up, front faces +X, pivot noted per asset in ASSETS below.

Run with Blender's Python module (pip install bpy) or Blender itself:
    python generate_assets.py --out ../../Art/Source/Meshes --preview-dir ./previews
    blender -b -P generate_assets.py -- --out ../../Art/Source/Meshes
"""

import argparse
import math
import os
import random
import sys

import bpy  # must come first when running as the pip 'bpy' module
import bmesh
from mathutils import Vector, noise

D2R = math.radians
PARTS = []
MATERIALS = {}


# ------------------------------------------------------------------------------------------------
# Scene / material helpers
# ------------------------------------------------------------------------------------------------

def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    MATERIALS.clear()


def mat(name, color, rough=0.8, metal=0.0, emit=0.0):
    """Shared flat-colour material. Name becomes the Unreal material asset name."""
    key = "M_Ev_" + name
    if key in MATERIALS:
        return MATERIALS[key]
    m = bpy.data.materials.new(key)
    try:
        m.use_nodes = True
    except Exception:
        pass
    m.diffuse_color = (*color, 1.0)
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = metal
        if emit > 0:
            bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emit
    MATERIALS[key] = m
    return m


def palette():
    return {
        "coat": mat("KeeperCoat", (0.10, 0.19, 0.21)),
        "mantle": mat("KeeperMantle", (0.07, 0.13, 0.15)),
        "scarf": mat("KeeperScarf", (0.78, 0.40, 0.10)),
        "skin": mat("KeeperSkin", (0.80, 0.60, 0.46), rough=0.6),
        "trousers": mat("KeeperTrousers", (0.20, 0.18, 0.16)),
        "leather": mat("Leather", (0.30, 0.18, 0.10), rough=0.7),
        "darkleather": mat("DarkLeather", (0.12, 0.08, 0.06), rough=0.7),
        "brass": mat("Brass", (0.72, 0.50, 0.20), rough=0.35, metal=0.9),
        "steel": mat("Steel", (0.75, 0.78, 0.82), rough=0.25, metal=1.0),
        "flame": mat("LanternFlame", (1.00, 0.68, 0.30), rough=0.4, emit=8.0),
        "eye": mat("Eye", (0.04, 0.04, 0.05), rough=0.3),
        "basalt": mat("Basalt", (0.09, 0.085, 0.08), rough=0.9),
        "ember": mat("Ember", (1.00, 0.33, 0.06), rough=0.5, emit=6.0),
        "stone": mat("Stone", (0.33, 0.32, 0.30)),
        "darkstone": mat("DarkStone", (0.26, 0.25, 0.24)),
        "seastone": mat("SeaStone", (0.42, 0.48, 0.52)),
        "wetstone": mat("WetStone", (0.20, 0.25, 0.25)),
        "moss": mat("Moss", (0.20, 0.32, 0.14)),
        "glass": mat("SaltGlass", (0.70, 0.90, 0.95), rough=0.12),
        "plaster": mat("Plaster", (0.78, 0.71, 0.58)),
        "timber": mat("Timber", (0.28, 0.18, 0.11)),
        "darkwood": mat("DarkWood", (0.18, 0.11, 0.07)),
        "rooftile": mat("RoofTile", (0.45, 0.20, 0.13)),
        "window": mat("WindowGlow", (0.95, 0.72, 0.38), rough=0.4, emit=3.0),
        "copper": mat("Verdigris", (0.20, 0.36, 0.31), rough=0.5, metal=0.4),
        "bark": mat("Bark", (0.22, 0.16, 0.11)),
        "leaf_a": mat("LeafDark", (0.10, 0.24, 0.14)),
        "leaf_b": mat("LeafLight", (0.16, 0.33, 0.17)),
        "tidestone": mat("TideStone", (0.30, 0.52, 0.56)),
        "water": mat("ShrineWater", (0.12, 0.42, 0.48), rough=0.1, emit=0.6),
        "gold": mat("Gold", (0.95, 0.72, 0.20), rough=0.25, metal=1.0),
        "label": mat("Label", (0.95, 0.95, 0.95), emit=1.5),
    }


# ------------------------------------------------------------------------------------------------
# Primitive helpers (all register into PARTS for the asset being built)
# ------------------------------------------------------------------------------------------------

def _finish(obj, material, smooth=False):
    obj.data.materials.clear()
    obj.data.materials.append(material)
    for poly in obj.data.polygons:
        poly.use_smooth = smooth
    PARTS.append(obj)
    return obj


def _rot(rot_deg):
    return tuple(D2R(a) for a in rot_deg)


def cube(size, loc, m, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=_rot(rot))
    o = bpy.context.active_object
    o.scale = size
    return _finish(o, m)


def cyl(r, depth, loc, m, rot=(0, 0, 0), verts=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=loc, rotation=_rot(rot))
    return _finish(bpy.context.active_object, m)


def cone(r1, r2, depth, loc, m, rot=(0, 0, 0), verts=12):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r1, radius2=r2, depth=depth, location=loc, rotation=_rot(rot))
    return _finish(bpy.context.active_object, m)


def sphere(r, loc, m, scale=(1, 1, 1), segs=16, rings=10, smooth=True):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segs, ring_count=rings, radius=r, location=loc)
    o = bpy.context.active_object
    o.scale = scale
    return _finish(o, m, smooth)


def ico(r, loc, m, scale=(1, 1, 1), subdiv=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdiv, radius=r, location=loc)
    o = bpy.context.active_object
    o.scale = scale
    return _finish(o, m)


def torus(major, minor, loc, m, rot=(0, 0, 0), seg=16, mseg=6):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, major_segments=seg, minor_segments=mseg,
                                     location=loc, rotation=_rot(rot))
    return _finish(bpy.context.active_object, m)


def mesh(name, verts, faces, m):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    return _finish(o, m)


def tube(points, radius, m, taper=0.5, resolution=4):
    """Bevelled curve through points, converted to a mesh (roots, branches)."""
    cd = bpy.data.curves.new("tube", "CURVE")
    cd.dimensions = "3D"
    cd.bevel_depth = radius
    cd.bevel_resolution = 1
    cd.resolution_u = resolution
    cd.use_fill_caps = True
    spline = cd.splines.new("BEZIER")
    spline.bezier_points.add(len(points) - 1)
    n = len(points)
    for i, (bp, p) in enumerate(zip(spline.bezier_points, points)):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
        bp.radius = 1.0 + (taper - 1.0) * (i / max(n - 1, 1))
    o = bpy.data.objects.new("tube", cd)
    bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.convert(target="MESH")
    return _finish(bpy.context.active_object, m)


def jitter(obj, amount, seed, scale=1.5, keep_bottom=None):
    """Noise-displace vertices for rocky shapes."""
    rnd = random.Random(seed)
    off = Vector((rnd.uniform(-100, 100), rnd.uniform(-100, 100), rnd.uniform(-100, 100)))
    for v in obj.data.vertices:
        co = obj.matrix_world @ v.co
        if keep_bottom is not None and co.z <= keep_bottom + 1e-4:
            continue
        n = noise.noise(co * scale + off)
        v.co += v.normal * n * amount
    obj.data.update()


def crystal(base, radius, height, tip, m, tilt=(0, 0, 0), sides=6):
    verts, faces = [], []
    for i in range(sides):
        a = 2 * math.pi * i / sides
        verts.append((math.cos(a) * radius, math.sin(a) * radius, 0))
    for i in range(sides):
        a = 2 * math.pi * i / sides
        verts.append((math.cos(a) * radius * 0.9, math.sin(a) * radius * 0.9, height))
    verts.append((0, 0, height + tip))
    top = len(verts) - 1
    faces.append(list(reversed(range(sides))))
    for i in range(sides):
        j = (i + 1) % sides
        faces.append([i, j, sides + j, sides + i])
        faces.append([sides + i, sides + j, top])
    o = mesh("crystal", verts, faces, m)
    o.rotation_euler = _rot(tilt)
    o.location = base
    return o


def assemble(name):
    """Apply transforms and join PARTS into one object whose origin is the world origin."""
    bpy.ops.object.select_all(action="DESELECT")
    for o in PARTS:
        o.select_set(True)
    bpy.context.view_layer.objects.active = PARTS[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.data.name = name
    PARTS.clear()
    return obj


# ------------------------------------------------------------------------------------------------
# Assets
# ------------------------------------------------------------------------------------------------

def build_lantern_keeper(P):
    """Player character. Pivot: between the feet. Height ~1.83 m. Faces +X."""
    for s in (1, -1):
        cube((0.21, 0.12, 0.12), (0.02, 0.09 * s, 0.06), P["darkleather"])           # boots
        cyl(0.065, 0.42, (0.0, 0.09 * s, 0.33), P["trousers"], verts=10)             # legs
    cone(0.34, 0.20, 0.64, (0, 0, 0.77), P["coat"], verts=10)                        # coat skirt
    cone(0.20, 0.22, 0.42, (0, 0, 1.28), P["coat"], verts=10)                        # torso
    cyl(0.215, 0.07, (0, 0, 1.09), P["leather"], verts=10)                           # belt
    cube((0.03, 0.07, 0.05), (0.215, 0, 1.09), P["brass"])                            # buckle
    for z in (0.95, 1.2, 1.38):                                                      # coat toggles
        cube((0.03, 0.04, 0.025), (0.205 if z > 1 else 0.27, 0, z), P["brass"])
    cone(0.31, 0.14, 0.22, (0, 0, 1.47), P["mantle"], verts=10)                     # shoulder mantle
    torus(0.12, 0.045, (0, 0, 1.53), P["scarf"])                                     # scarf
    cube((0.04, 0.09, 0.32), (-0.13, 0.07, 1.38), P["scarf"], rot=(8, 12, 0))       # scarf tail
    sphere(0.115, (0, 0, 1.66), P["skin"])                                           # head
    sphere(0.15, (-0.06, 0, 1.68), P["mantle"], scale=(0.95, 1.0, 1.08))            # hood
    for s in (1, -1):
        sphere(0.018, (0.107, 0.04 * s, 1.675), P["eye"], segs=8, rings=6)           # eyes
        cyl(0.06, 0.32, (0.0, 0.27 * s, 1.32), P["coat"], rot=(10 * s, 0, 0), verts=8)   # upper arms
        cyl(0.055, 0.30, (0.04, 0.29 * s, 1.03), P["coat"], rot=(4 * s, -12, 0), verts=8)  # forearms
        sphere(0.065, (0.08, 0.30 * s, 0.87), P["darkleather"], segs=10, rings=6)     # gloves
    # Bedroll across the back.
    cyl(0.09, 0.40, (-0.25, 0, 1.36), P["leather"], rot=(90, 0, 0), verts=10)
    for y in (0.12, -0.12):
        cyl(0.095, 0.03, (-0.25, y, 1.36), P["darkleather"], rot=(90, 0, 0), verts=10)
    # Brass lantern hanging from the left hip (+Y is the character's left).
    lx, ly, lz = 0.08, 0.36, 0.98
    cube((0.015, 0.015, 0.09), (lx, ly - 0.02, 1.06), P["darkleather"])              # strap
    cube((0.12, 0.12, 0.015), (lx, ly, lz + 0.07), P["brass"])
    cube((0.12, 0.12, 0.015), (lx, ly, lz - 0.07), P["brass"])
    for dx in (-0.05, 0.05):
        for dy in (-0.05, 0.05):
            cyl(0.008, 0.14, (lx + dx, ly + dy, lz), P["brass"], verts=6)
    cube((0.085, 0.085, 0.11), (lx, ly, lz), P["flame"])
    cone(0.065, 0.015, 0.05, (lx, ly, lz + 0.1), P["brass"], verts=8)
    torus(0.022, 0.006, (lx, ly, lz + 0.14), P["brass"], rot=(90, 0, 0), seg=10, mseg=4)


def build_blade(P):
    """Sword. Pivot: centre of the grip. Blade points +X, edge vertical. Length ~1.05 m."""
    cyl(0.018, 0.17, (0, 0, 0), P["leather"], rot=(0, 90, 0), verts=8)              # grip
    sphere(0.03, (-0.1, 0, 0), P["brass"], segs=10, rings=6)                         # pommel
    cube((0.035, 0.05, 0.19), (0.095, 0, 0), P["brass"])                             # crossguard
    t = 0.007
    verts = [(0.11, -t, -0.028), (0.11, t, -0.028), (0.11, t, 0.028), (0.11, -t, 0.028),
             (0.92, -t, -0.02), (0.92, t, -0.02), (0.92, t, 0.024), (0.92, -t, 0.024),
             (1.03, 0, 0.004)]
    faces = [[0, 3, 2, 1], [0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7],
             [4, 5, 8], [5, 6, 8], [6, 7, 8], [7, 4, 8]]
    mesh("blade", verts, faces, P["steel"])
    cube((0.7, 0.016, 0.008), (0.5, 0, 0.004), P["tidestone"])                       # tide-glass inlay


def build_sentinel(P):
    """Basalt enemy. Pivot: base centre. Height ~1.7 m. Eye socket faces +X at 1.15 m."""
    body = cone(0.5, 0.18, 1.3, (0, 0, 0.65), P["basalt"], verts=7)
    jitter(body, 0.05, 3, scale=2.5, keep_bottom=0.0)
    cone(0.22, 0.05, 0.38, (0, 0, 1.48), P["basalt"], verts=7)
    for s in (1, -1):
        cone(0.1, 0.0, 0.5, (-0.02, 0.27 * s, 1.08), P["basalt"], rot=(-38 * s, -10, 0), verts=5)
        cone(0.07, 0.0, 0.32, (-0.12, 0.2 * s, 1.3), P["basalt"], rot=(-25 * s, -30, 0), verts=5)
    # Glowing fissures (avoid the front so the eye reads clearly).
    slope = math.degrees(math.atan2(0.5 - 0.18, 1.3))  # cone wall lean
    for i, ang in enumerate((60, 120, 165, 210, 255, 300)):
        zc = 0.55 + 0.12 * (i % 3)
        r = 0.5 - (0.5 - 0.18) * (zc / 1.3) - 0.015  # half-embedded in the wall
        a = D2R(ang)
        cube((0.05, 0.035, 0.42), (r * math.cos(a), r * math.sin(a), zc), P["ember"], rot=(0, -slope, ang))
    torus(0.07, 0.022, (0.25, 0, 1.15), P["basalt"], rot=(0, 90, 0), seg=10, mseg=4)  # eye socket
    rnd = random.Random(11)
    for i in range(6):
        a = D2R(i * 60 + rnd.uniform(-20, 20))
        rock = ico(rnd.uniform(0.08, 0.14), (0.55 * math.cos(a), 0.55 * math.sin(a), 0.05), P["basalt"])
        jitter(rock, 0.03, i, scale=6)


def build_key(P):
    """Small key. Pivot: centre. Shaft vertical, ~0.33 m."""
    cyl(0.012, 0.22, (0, 0, -0.02), P["gold"], verts=8)
    torus(0.045, 0.013, (0, 0, 0.13), P["gold"], rot=(90, 0, 0), seg=14, mseg=5)
    cube((0.012, 0.045, 0.028), (0, 0.025, -0.1), P["gold"])
    cube((0.012, 0.03, 0.02), (0, 0.018, -0.06), P["gold"])
    sphere(0.02, (0, 0, 0.13), P["ember"], segs=8, rings=6)


def build_boulder(P, seed):
    """Mossy boulder. Pivot: centre. ~1 m across (drop-in for a unit sphere)."""
    o = ico(0.5, (0, 0, 0), P["stone"], subdiv=2)
    jitter(o, 0.12, seed, scale=1.6)
    for v in o.data.vertices:
        v.co.z = max(v.co.z, -0.34)
    o.data.update()
    o.data.materials.append(P["moss"])
    for poly in o.data.polygons:
        if poly.normal.z > 0.75:
            poly.material_index = 1


def build_glass_shard(P):
    """Salt-glass crystal cluster. Pivot: centre of a 1 m box (drop-in for a unit cube)."""
    crystal((0, 0, -0.5), 0.2, 0.72, 0.28, P["glass"], tilt=(4, -3, 0))
    crystal((0.12, 0.14, -0.5), 0.1, 0.42, 0.14, P["glass"], tilt=(-22, 18, 30))
    crystal((-0.14, -0.08, -0.5), 0.09, 0.34, 0.12, P["glass"], tilt=(20, -24, 10))
    crystal((0.05, -0.18, -0.5), 0.07, 0.22, 0.1, P["glass"], tilt=(28, 10, 0))


def build_basalt_column(P):
    """Hexagonal basalt column. Pivot: centre. 1 m wide, 1 m tall (drop-in for a unit cylinder)."""
    c = cyl(0.5, 1.0, (0, 0, 0), P["basalt"], verts=6)
    bpy.ops.object.select_all(action="DESELECT")
    c.select_set(True)
    bpy.context.view_layer.objects.active = c
    mod = c.modifiers.new("bevel", "BEVEL")
    mod.width = 0.035
    mod.segments = 1
    bpy.ops.object.modifier_apply(modifier=mod.name)
    cube((0.3, 0.25, 0.06), (0.1, 0.05, 0.49), P["darkstone"], rot=(0, 0, 25))


def build_spire(P):
    """Ruined lagoon spire. Pivot: base centre. ~4.4 m wide, ~48 m tall."""
    rnd = random.Random(7)
    for i in range(8):
        w = 4.4 - i * 0.28
        z0 = i * 6.0
        if i < 7:
            cube((w, w, 5.6), (0, 0, z0 + 2.8), P["wetstone"] if i < 2 else P["seastone"])
            cube((w + 0.5, w + 0.5, 0.4), (0, 0, z0 + 5.8), P["darkstone"])
            for k in range(4):
                a = D2R(90 * k)
                x, y = math.cos(a) * w / 2, math.sin(a) * w / 2
                cube((0.2, 0.7, 1.8), (x, y, z0 + 3.0), P["eye"], rot=(0, 0, 90 * k))
        else:
            for k in range(7):  # jagged broken crown
                h = rnd.uniform(1.0, 5.0)
                ox, oy = rnd.uniform(-1.2, 1.2), rnd.uniform(-1.2, 1.2)
                cube((rnd.uniform(0.7, 1.4), rnd.uniform(0.7, 1.4), h), (ox, oy, z0 + h / 2),
                     P["seastone"], rot=(rnd.uniform(-12, 12), rnd.uniform(-12, 12), rnd.uniform(0, 90)))
    for k in range(5):  # weed streaks near the waterline
        a = D2R(k * 72 + 20)
        cube((0.3, 0.9, rnd.uniform(2.0, 4.5)), (math.cos(a) * 2.25, math.sin(a) * 2.25, 2.5), P["moss"], rot=(0, 0, k * 72 + 20))


def build_standing_stone(P):
    """Waystone. Pivot: base centre. 1.6 x 0.8 x 5 m. Runes on the +X face."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 2.5))
    o = bpy.context.active_object
    o.scale = (0.8, 1.6, 5.0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=3, use_grid_fill=True)
    bm.to_mesh(o.data)
    bm.free()
    _finish(o, P["stone"])
    jitter(o, 0.09, 21, scale=1.2, keep_bottom=0.0)
    for v in o.data.vertices:  # slanted, weathered top
        if v.co.z > 2.2:
            v.co.z -= 0.25 * (v.co.y + 0.8)
    o.data.update()
    for i, z in enumerate((1.6, 2.2, 2.8, 3.4)):
        cube((0.06, 0.12 + 0.08 * (i % 2), 0.22), (0.43, 0.15 * (-1) ** i, z), P["ember"])


def build_house(P):
    """Lanternhold cottage. Pivot: base centre. Door on +X. ~6.4 x 5.4 x 6.3 m."""
    pitch = D2R(35)
    wall_top = 3.4
    rise = 2.5 * math.tan(pitch)
    cube((6.4, 5.4, 0.4), (0, 0, 0.2), P["stone"])                                      # foundation
    cube((6.0, 5.0, 3.0), (0, 0, 1.9), P["plaster"])                                    # walls
    for sx in (1, -1):
        for sy in (1, -1):
            cube((0.26, 0.26, 3.05), (2.95 * sx, 2.45 * sy, 1.9), P["timber"])         # corner posts
    for z in (2.3, 3.32):
        cube((6.06, 5.06, 0.18), (0, 0, z), P["timber"])                                # beams
    g = [(x, y, wall_top) for x in (-3.0, 3.0) for y in (-2.5, 2.5)] + [(-3.0, 0, wall_top + rise), (3.0, 0, wall_top + rise)]
    mesh("gable", g, [[0, 1, 4], [2, 5, 3], [0, 2, 3, 1], [1, 3, 5, 4], [0, 4, 5, 2]], P["plaster"])
    half = 2.85
    slope = half / math.cos(pitch)
    ridge = wall_top + rise + 0.1
    for s in (1, -1):
        cube((6.8, slope, 0.16), (0, s * half / 2, ridge - (half / 2) * math.tan(pitch)), P["rooftile"], rot=(-35 * s, 0, 0))
    cube((6.9, 0.22, 0.22), (0, 0, ridge + 0.02), P["darkwood"])                        # ridge beam
    cube((0.1, 1.35, 2.3), (3.0, 0, 1.55), P["timber"])                                 # door frame
    cube((0.12, 1.1, 2.1), (3.03, 0, 1.45), P["darkwood"])                              # door
    cube((0.8, 1.6, 0.2), (3.45, 0, 0.1), P["darkstone"])                               # step
    for y in (1.7, -1.7):                                                               # front windows
        cube((0.1, 0.7, 0.8), (3.02, y, 2.05), P["window"])
        cube((0.12, 0.08, 0.84), (3.04, y, 2.05), P["timber"])
        for sy in (1, -1):
            cube((0.06, 0.32, 0.86), (3.06, y + sy * 0.52, 2.05), P["darkwood"])         # shutters
    for x in (1.5, -1.5):                                                               # side windows
        for sy in (1, -1):
            cube((0.7, 0.1, 0.8), (x, 2.52 * sy, 2.05), P["window"])
    cube((0.1, 0.7, 0.8), (-3.02, 0, 2.05), P["window"])
    cube((0.7, 0.7, 2.4), (-1.8, -1.2, 5.0), P["stone"])                                # chimney
    cube((0.86, 0.86, 0.15), (-1.8, -1.2, 6.25), P["darkstone"])
    cube((0.12, 0.12, 0.18), (3.15, 0.85, 2.75), P["flame"])                            # door lantern
    cube((0.3, 0.04, 0.04), (3.05, 0.85, 2.9), P["darkwood"])


def build_lantern_tower(P):
    """Lanternhold's tower. Pivot: base centre. ~28 m tall. Lantern core centred at 23.8 m."""
    cyl(2.8, 1.2, (0, 0, 0.6), P["darkstone"], verts=8)
    cone(2.0, 1.5, 21.0, (0, 0, 11.7), P["stone"], verts=8)
    for z in (6.0, 12.0, 18.0):
        r = 2.0 - 0.5 * (z - 1.2) / 21.0 + 0.12
        cyl(r, 0.35, (0, 0, z), P["darkstone"], verts=8)
    cube((0.4, 1.2, 2.2), (1.95, 0, 2.3), P["darkwood"])
    for i, z in enumerate((4.5, 9.0, 13.5, 16.0, 20.0)):
        a = D2R(90 * i + 45)
        r = 2.0 - 0.5 * (z - 1.2) / 21.0
        cube((0.2, 0.25, 0.9), (r * math.cos(a), r * math.sin(a), z), P["window"], rot=(0, 0, 90 * i + 45))
    cyl(2.3, 0.4, (0, 0, 22.4), P["darkstone"], verts=8)
    for i in range(16):
        a = 2 * math.pi * i / 16
        cube((0.08, 0.08, 0.9), (2.15 * math.cos(a), 2.15 * math.sin(a), 23.05), P["darkwood"])
    torus(2.15, 0.05, (0, 0, 23.5), P["darkwood"], seg=16, mseg=4)
    for i in range(8):
        a = 2 * math.pi * i / 8
        cyl(0.1, 2.4, (1.2 * math.cos(a), 1.2 * math.sin(a), 23.8), P["darkstone"], verts=6)
    cyl(1.0, 2.2, (0, 0, 23.8), P["flame"], verts=8)
    cyl(1.4, 0.3, (0, 0, 25.15), P["darkstone"], verts=8)
    cone(1.8, 0.05, 2.6, (0, 0, 26.6), P["copper"], verts=8)
    sphere(0.2, (0, 0, 28.0), P["brass"], segs=10, rings=6)


def build_tide_shrine(P):
    """Tide shrine. Pivot: base centre. Orb cradle at ~4.5 m (the orb itself is spawned in Unreal)."""
    cyl(1.8, 0.25, (0, 0, 0.125), P["darkstone"], verts=12)
    cyl(1.5, 0.55, (0, 0, 0.525), P["seastone"], verts=12)
    cyl(1.3, 0.05, (0, 0, 0.79), P["water"], verts=12)
    cone(0.42, 0.3, 3.2, (0, 0, 2.4), P["seastone"], verts=6)
    for z, r in ((1.6, 0.43), (2.5, 0.39), (3.3, 0.35)):
        torus(r, 0.05, (0, 0, z), P["tidestone"], seg=12, mseg=4)
    torus(0.35, 0.04, (0, 0, 4.0), P["tidestone"], seg=12, mseg=4)
    for i in range(3):
        a = D2R(120 * i)
        cone(0.07, 0.02, 0.9, (0.25 * math.cos(a), 0.25 * math.sin(a), 4.35), P["seastone"],
             rot=(0, 20, 120 * i), verts=6)


def build_mangrove(P, seed):
    """Mangrove tree on arching prop roots. Pivot: base centre. ~12 m tall."""
    rnd = random.Random(seed)
    for i in range(7):
        a = D2R(i * (360 / 7) + rnd.uniform(-18, 18))
        reach = rnd.uniform(1.8, 2.5)
        c, s = math.cos(a), math.sin(a)
        tube([(0.15 * c, 0.15 * s, 1.8), (0.9 * c, 0.9 * s, 1.75), (reach * 0.75 * c, reach * 0.75 * s, 0.9), (reach * c, reach * s, -0.2)],
             0.09, P["bark"], taper=0.6)
    tube([(0, 0, 1.3), (0.15, 0.05, 4.0), (-0.12, 0.12, 7.0), (0.05, 0.0, 9.2)], 0.32, P["bark"], taper=0.5, resolution=6)
    tips = []
    for i in range(4):
        a = D2R(i * 90 + rnd.uniform(-25, 25))
        z0 = rnd.uniform(6.8, 8.4)
        tip = (2.3 * math.cos(a), 2.3 * math.sin(a), z0 + rnd.uniform(1.6, 2.4))
        tube([(0, 0, z0), (1.1 * math.cos(a), 1.1 * math.sin(a), z0 + 0.9), tip], 0.12, P["bark"], taper=0.4)
        tips.append(tip)
    tips.append((0.05, 0.0, 10.4))
    for i, t in enumerate(tips):
        r = rnd.uniform(1.6, 2.3)
        blob = ico(r, (t[0], t[1], t[2] + 0.3), P["leaf_a"] if i % 2 else P["leaf_b"], scale=(1, 1, 0.55))
        jitter(blob, 0.25, seed * 10 + i, scale=0.8)
    for i in range(3):
        a = rnd.uniform(0, 2 * math.pi)
        blob = ico(rnd.uniform(1.2, 1.6), (1.2 * math.cos(a), 1.2 * math.sin(a), 11.0), P["leaf_b"], scale=(1, 1, 0.5))
        jitter(blob, 0.2, seed * 20 + i, scale=0.8)


ASSETS = [
    # (asset name, builder, args)
    ("SM_LanternKeeper", build_lantern_keeper, ()),
    ("SM_Blade", build_blade, ()),
    ("SM_Sentinel", build_sentinel, ()),
    ("SM_Key", build_key, ()),
    ("SM_Boulder_A", build_boulder, (1,)),
    ("SM_Boulder_B", build_boulder, (2,)),
    ("SM_GlassShard", build_glass_shard, ()),
    ("SM_BasaltColumn", build_basalt_column, ()),
    ("SM_Spire", build_spire, ()),
    ("SM_StandingStone", build_standing_stone, ()),
    ("SM_House", build_house, ()),
    ("SM_LanternTower", build_lantern_tower, ()),
    ("SM_TideShrine", build_tide_shrine, ()),
    ("SM_Mangrove_A", build_mangrove, (3,)),
    ("SM_Mangrove_B", build_mangrove, (8,)),
]


# ------------------------------------------------------------------------------------------------
# Export + preview
# ------------------------------------------------------------------------------------------------

def export_fbx(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"},
                             apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE",
                             use_mesh_modifiers=True, bake_anim=False, add_leaf_bones=False)


def place(objs, name, loc, rot_z=0.0, scale=1.0):
    o = objs[name]
    o.location = loc
    o.rotation_euler = (0, 0, D2R(rot_z))
    o.scale = (scale, scale, scale)
    o.hide_render = False


def label(text, loc, size, P):
    bpy.ops.object.text_add(location=loc, rotation=(D2R(90), 0, 0))
    t = bpy.context.active_object
    t.data.body = text
    t.data.size = size
    t.data.align_x = "CENTER"
    t.data.materials.append(P["label"])
    return t


def setup_render(scene, cam_loc, cam_rot, lens, path, samples=64):
    for o in list(scene.objects):
        if o.type in {"CAMERA", "LIGHT"} or o.name.startswith(("Ground", "Text")):
            bpy.data.objects.remove(o, do_unlink=True)
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    cam.data.lens = lens
    cam.location = cam_loc
    cam.rotation_euler = tuple(D2R(a) for a in cam_rot)
    scene.collection.objects.link(cam)
    scene.camera = cam
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.data.angle = D2R(3)
    sun.rotation_euler = (D2R(50), D2R(10), D2R(-35))
    scene.collection.objects.link(sun)
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    scene.render.filepath = path
    world = bpy.data.worlds.new("Sky") if scene.world is None else scene.world
    scene.world = world
    try:
        world.use_nodes = True
    except Exception:
        pass
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.42, 0.55, 0.68, 1.0)
    bg.inputs["Strength"].default_value = 0.9


def render_previews(objs, P, out_dir):
    scene = bpy.context.scene
    for o in objs.values():
        o.hide_render = True

    def ground(size, z=0.0):
        bpy.ops.mesh.primitive_plane_add(size=size, location=(0, 0, z))
        g = bpy.context.active_object
        g.name = "Ground"
        g.data.materials.append(mat("PreviewGround", (0.30, 0.36, 0.26)))
        g.location.x = size * 0.1

    # Sheet 1: characters and props.
    setup_render(scene, (3.35, -8.2, 1.6), (84, 0, 0), 32, os.path.join(out_dir, "emberveil_props.png"))
    ground(40)
    items = [("SM_LanternKeeper", (0, 0, 0), -55, 1, "Lantern-keeper"),
             ("SM_Blade", (1.05, 0, 0.0), -90, 1, "Blade"),
             ("SM_Sentinel", (2.3, 0, 0), -125, 1, "Sentinel"),
             ("SM_Key", (3.4, 0, 0.55), -30, 3, "Small key (x3)"),
             ("SM_Boulder_A", (4.5, 0, 0.42), 0, 1, "Boulder"),
             ("SM_GlassShard", (5.6, 0, 0.5), 20, 1, "Salt-glass"),
             ("SM_BasaltColumn", (6.7, 0, 0.5), 0, 1, "Basalt")]
    for name, loc, rz, s, text in items:
        place(objs, name, loc, rz, s)
        label(text, (loc[0], -0.9, 0.02), 0.16, P)
    blade = objs["SM_Blade"]
    blade.rotation_euler = (0, D2R(-90), D2R(-30))
    blade.location = (1.05, 0, 0.12)
    bpy.ops.render.render(write_still=True)

    # Sheet 2: environment kit.
    for o in objs.values():
        o.hide_render = True
    setup_render(scene, (19, -60, 15), (84, 0, 0), 32, os.path.join(out_dir, "emberveil_environment.png"))
    ground(400)
    items = [("SM_LanternKeeper", (-6, 0, 0), -55, 1, "Keeper"),
             ("SM_StandingStone", (0.5, 0, 0), -60, 1, "Waystone"),
             ("SM_TideShrine", (6, 0, 0), 0, 1, "Tide shrine"),
             ("SM_House", (13, 0, 0), -50, 1, "Cottage"),
             ("SM_Mangrove_A", (23, 0, 0), 0, 1, "Mangrove"),
             ("SM_Mangrove_B", (31, 2, 0), 40, 1, ""),
             ("SM_LanternTower", (41, 3, 0), 0, 1, "Lantern tower"),
             ("SM_Spire", (16, 45, -4), 25, 0.85, "")]
    for name, loc, rz, s, text in items:
        place(objs, name, loc, rz, s)
        if text:
            label(text, (loc[0], -7, 0.05), 0.9, P)
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, help="Directory for FBX files")
    parser.add_argument("--preview-dir", help="Render preview sheets into this directory")
    args = parser.parse_args(argv)
    os.makedirs(args.out, exist_ok=True)

    reset_scene()
    P = palette()
    objs = {}
    for name, builder, extra in ASSETS:
        builder(P, *extra)
        obj = assemble(name)
        export_fbx(obj, os.path.join(args.out, name + ".fbx"))
        dims = obj.dimensions
        print(f"{name:20s} {dims.x:6.2f} x {dims.y:6.2f} x {dims.z:6.2f} m  ({len(obj.data.polygons)} faces)")
        obj.location.x += 1000.0  # park it away from the next build
        objs[name] = obj

    if args.preview_dir:
        os.makedirs(args.preview_dir, exist_ok=True)
        render_previews(objs, P, args.preview_dir)


if __name__ == "__main__":
    main()
