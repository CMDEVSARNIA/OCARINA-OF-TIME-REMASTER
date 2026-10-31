"""
Rigged + animated lantern-keeper for Emberveil (original character).

Builds the character from rigid parts bound to a simple humanoid skeleton, keys a set of
procedural animation cycles, and exports:
    SK_LanternKeeper.fbx                skeletal mesh (sword baked into the right hand)
    A_Keeper_<Name>.fbx                 one animation per file

Run:  python generate_character.py --out ../../Art/Source/Characters [--preview out.png] [--verify]
Conventions: metres, Z up, character faces +X, left is +Y. Root bone at the origin.
"""

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402  (must precede bmesh/mathutils when running as the pip module)
from mathutils import Matrix, Quaternion, Vector  # noqa: E402

import generate_assets as ga  # noqa: E402

FPS = 30
X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)

# name, head, tail, parent
BONES = [
    ("root", (0, 0, 0), (0, 0, 0.15), None),
    ("pelvis", (0, 0, 0.95), (0, 0, 1.10), "root"),
    ("spine", (0, 0, 1.10), (0, 0, 1.30), "pelvis"),
    ("chest", (0, 0, 1.30), (0, 0, 1.50), "spine"),
    ("neck", (0, 0, 1.50), (0, 0, 1.58), "chest"),
    ("head", (0, 0, 1.58), (0, 0, 1.80), "neck"),
    ("lantern", (0.08, 0.36, 1.08), (0.08, 0.36, 0.94), "pelvis"),
]
for side, s in (("l", 1), ("r", -1)):
    BONES += [
        (f"upperarm_{side}", (0, 0.27 * s, 1.47), (0, 0.29 * s, 1.17), "chest"),
        (f"forearm_{side}", (0, 0.29 * s, 1.17), (0.05, 0.30 * s, 0.93), f"upperarm_{side}"),
        (f"hand_{side}", (0.05, 0.30 * s, 0.93), (0.09, 0.30 * s, 0.83), f"forearm_{side}"),
        (f"thigh_{side}", (0, 0.09 * s, 0.95), (0, 0.09 * s, 0.55), "pelvis"),
        (f"shin_{side}", (0, 0.09 * s, 0.55), (0, 0.09 * s, 0.12), f"thigh_{side}"),
        (f"foot_{side}", (0, 0.09 * s, 0.12), (0.13, 0.09 * s, 0.03), f"shin_{side}"),
    ]


# ------------------------------------------------------------------------------------------------
# Geometry
# ------------------------------------------------------------------------------------------------

def bind(objs, bone):
    """Rigidly skin whole parts to one bone."""
    for o in objs if isinstance(objs, (list, tuple)) else [objs]:
        vg = o.vertex_groups.new(name=bone)
        vg.add(list(range(len(o.data.vertices))), 1.0, "REPLACE")


def new_parts(fn):
    """Run fn() and return the parts it created."""
    start = len(ga.PARTS)
    fn()
    return ga.PARTS[start:]


def build_body(P):
    c, cyl, cone, sph, tor = ga.cube, ga.cyl, ga.cone, ga.sphere, ga.torus
    for side, s in (("l", 1), ("r", -1)):
        bind(c((0.21, 0.12, 0.12), (0.03, 0.09 * s, 0.06), P["darkleather"]), f"foot_{side}")
        bind(cyl(0.06, 0.44, (0, 0.09 * s, 0.33), P["trousers"], verts=10), f"shin_{side}")
        bind(cyl(0.068, 0.42, (0, 0.09 * s, 0.75), P["trousers"], verts=10), f"thigh_{side}")
        bind(cyl(0.06, 0.32, (0.0, 0.28 * s, 1.32), P["coat"], rot=(4 * s, 0, 0), verts=8), f"upperarm_{side}")
        bind(cyl(0.055, 0.27, (0.025, 0.295 * s, 1.05), P["coat"], rot=(2 * s, -12, 0), verts=8), f"forearm_{side}")
        bind(sph(0.062, (0.07, 0.30 * s, 0.89), P["darkleather"], segs=10, rings=6), f"hand_{side}")
        bind(sph(0.018, (0.107, 0.04 * s, 1.675), P["eye"], segs=8, rings=6), "head")

    bind([cone(0.34, 0.20, 0.62, (0, 0, 0.79), P["coat"], verts=10),
          cyl(0.215, 0.07, (0, 0, 1.09), P["leather"], verts=10),
          c((0.03, 0.07, 0.05), (0.215, 0, 1.09), P["brass"]),
          c((0.03, 0.04, 0.025), (0.27, 0, 0.95), P["brass"])], "pelvis")
    bind([cone(0.20, 0.22, 0.42, (0, 0, 1.30), P["coat"], verts=10),
          c((0.03, 0.04, 0.025), (0.205, 0, 1.2), P["brass"])], "spine")
    bind([cone(0.31, 0.14, 0.22, (0, 0, 1.47), P["mantle"], verts=10),
          c((0.03, 0.04, 0.025), (0.205, 0, 1.38), P["brass"]),
          cyl(0.09, 0.40, (-0.25, 0, 1.36), P["leather"], rot=(90, 0, 0), verts=10),
          cyl(0.095, 0.03, (-0.25, 0.12, 1.36), P["darkleather"], rot=(90, 0, 0), verts=10),
          cyl(0.095, 0.03, (-0.25, -0.12, 1.36), P["darkleather"], rot=(90, 0, 0), verts=10)], "chest")
    bind([tor(0.12, 0.045, (0, 0, 1.53), P["scarf"]),
          c((0.04, 0.09, 0.32), (-0.13, 0.07, 1.38), P["scarf"], rot=(8, 12, 0))], "neck")
    bind([sph(0.115, (0, 0, 1.66), P["skin"]),
          sph(0.15, (-0.06, 0, 1.68), P["mantle"], scale=(0.95, 1.0, 1.08))], "head")

    # Lantern (swings on its own bone).
    lx, ly, lz = 0.08, 0.36, 0.98
    lantern = [c((0.015, 0.015, 0.09), (lx, ly - 0.02, 1.06), P["darkleather"]),
               c((0.12, 0.12, 0.015), (lx, ly, lz + 0.07), P["brass"]),
               c((0.12, 0.12, 0.015), (lx, ly, lz - 0.07), P["brass"]),
               c((0.085, 0.085, 0.11), (lx, ly, lz), P["flame"]),
               cone(0.065, 0.015, 0.05, (lx, ly, lz + 0.1), P["brass"], verts=8)]
    for dx in (-0.05, 0.05):
        for dy in (-0.05, 0.05):
            lantern.append(cyl(0.008, 0.14, (lx + dx, ly + dy, lz), P["brass"], verts=6))
    bind(lantern, "lantern")

    # Sword baked into the right hand: grip at the glove, blade forward and slightly raised.
    sword = new_parts(lambda: ga.build_blade(P))
    grip = Matrix.Translation((0.07, -0.30, 0.89)) @ Matrix.Rotation(math.radians(-20), 4, "Y")
    for o in sword:
        # matrix_basis is built from loc/rot/scale directly; matrix_world may be stale until a depsgraph update.
        o.matrix_basis = grip @ o.matrix_basis
    bind(sword, "hand_r")


def build_armature():
    data = bpy.data.armatures.new("KeeperRig")
    arm = bpy.data.objects.new("Armature", data)  # Unreal drops a top node named "Armature"
    bpy.context.collection.objects.link(arm)
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, head, tail, parent in BONES:
        eb = data.edit_bones.new(name)
        eb.head, eb.tail, eb.roll = head, tail, 0.0
        if parent:
            eb.parent = data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def build_character():
    ga.reset_scene()
    P = ga.palette()
    build_body(P)
    body = ga.assemble("SK_LanternKeeper")
    arm = build_armature()
    body.parent = arm
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return arm, body


# ------------------------------------------------------------------------------------------------
# Animation
# ------------------------------------------------------------------------------------------------

def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def pose_idle(t):
    s, c = math.sin(2 * math.pi * t), math.cos(2 * math.pi * t)
    return {
        "pelvis": {"loc": (0, 0, 0.006 * s)},
        "chest": {"rot": [(Y, -1.5 * s)]},
        "head": {"rot": [(Z, 5 * math.sin(2 * math.pi * t)), (Y, 2 * c)]},
        "lantern": {"rot": [(Y, 5 * s), (X, 3 * c)]},
        "upperarm_l": {"rot": [(X, 4)]}, "upperarm_r": {"rot": [(X, -4)]},
        "forearm_l": {"rot": [(Y, -12)]}, "forearm_r": {"rot": [(Y, -18)]},
    }


def pose_walk(t):
    s = math.sin(2 * math.pi * t)
    return {
        "pelvis": {"loc": (0, 0, 0.025 * math.cos(4 * math.pi * t)), "rot": [(Z, 5 * s)]},
        "chest": {"rot": [(Z, -7 * s)]},
        "thigh_l": {"rot": [(Y, -25 * s)]}, "thigh_r": {"rot": [(Y, 25 * s)]},
        "shin_l": {"rot": [(Y, 5 + 32 * max(0.0, -s))]}, "shin_r": {"rot": [(Y, 5 + 32 * max(0.0, s))]},
        "foot_l": {"rot": [(Y, -8 * max(0.0, -s))]}, "foot_r": {"rot": [(Y, -8 * max(0.0, s))]},
        "upperarm_l": {"rot": [(Y, 20 * s), (X, 4)]}, "upperarm_r": {"rot": [(Y, -20 * s), (X, -4)]},
        "forearm_l": {"rot": [(Y, -15)]}, "forearm_r": {"rot": [(Y, -20)]},
        "lantern": {"rot": [(Y, 12 * math.sin(2 * math.pi * t + 0.6)), (X, 4 * math.sin(4 * math.pi * t))]},
        "head": {"rot": [(Z, 3 * s)]},
    }


def pose_run(t):
    s = math.sin(2 * math.pi * t)
    return {
        "pelvis": {"loc": (0, 0, 0.04 * abs(math.cos(2 * math.pi * t)) - 0.02), "rot": [(Z, 8 * s)]},
        "spine": {"rot": [(Y, 12)]},
        "chest": {"rot": [(Z, -12 * s)]},
        "head": {"rot": [(Y, -8)]},
        "thigh_l": {"rot": [(Y, -45 * s)]}, "thigh_r": {"rot": [(Y, 45 * s)]},
        "shin_l": {"rot": [(Y, 15 + 70 * max(0.0, -s))]}, "shin_r": {"rot": [(Y, 15 + 70 * max(0.0, s))]},
        "upperarm_l": {"rot": [(Y, 40 * s), (X, 8)]}, "upperarm_r": {"rot": [(Y, -40 * s), (X, -8)]},
        "forearm_l": {"rot": [(Y, -65)]}, "forearm_r": {"rot": [(Y, -60)]},
        "lantern": {"rot": [(Y, 20 * math.sin(2 * math.pi * t + 0.8))]},
    }


def pose_fall(t):
    w = math.sin(2 * math.pi * t)
    return {
        "thigh_l": {"rot": [(Y, -35)]}, "thigh_r": {"rot": [(Y, -20)]},
        "shin_l": {"rot": [(Y, 55)]}, "shin_r": {"rot": [(Y, 40)]},
        "upperarm_l": {"rot": [(X, 40 + 5 * w)]}, "upperarm_r": {"rot": [(X, -40 - 5 * w)]},
        "forearm_l": {"rot": [(Y, -30)]}, "forearm_r": {"rot": [(Y, -30)]},
        "lantern": {"rot": [(Y, -25 + 6 * w)]},
        "spine": {"rot": [(Y, 6)]},
    }


def make_slash(start, end):
    def pose(t):
        e = smooth(t)
        raise_amt = math.sin(math.pi * min(t * 1.25, 1.0)) ** 0.5
        return {
            "pelvis": {"rot": [(Z, lerp(start * 0.25, end * 0.3, e))]},
            "chest": {"rot": [(Z, lerp(start * 0.5, end * 0.6, e))]},
            "upperarm_r": {"rot": [(Z, lerp(start, end, e)), (Y, -80 * raise_amt)]},
            "forearm_r": {"rot": [(Y, -15 * raise_amt)]},
            "upperarm_l": {"rot": [(Y, 20 * raise_amt), (X, 10)]},
            "thigh_l": {"rot": [(Y, -15 * raise_amt)]}, "shin_l": {"rot": [(Y, 15 * raise_amt)]},
            "lantern": {"rot": [(X, -15 * math.sin(math.pi * t))]},
        }
    return pose


def pose_spin(t):
    e = smooth(t)
    out = math.sin(math.pi * min(t * 1.2, 1.0))
    return {
        "root": {"rot": [(Z, 360 * e)]},
        "upperarm_r": {"rot": [(X, -85 * out), (Y, -15 * out)]},
        "upperarm_l": {"rot": [(X, 40 * out)]},
        "spine": {"rot": [(Y, 8 * out)]},
        "thigh_l": {"rot": [(Y, -20 * out)]}, "thigh_r": {"rot": [(Y, 15 * out)]},
        "shin_l": {"rot": [(Y, 25 * out)]}, "shin_r": {"rot": [(Y, 20 * out)]},
        "lantern": {"rot": [(X, 50 * out)]},
    }


ANIMS = [
    # name, frames, pose function, loops
    ("Idle", 60, pose_idle, True),
    ("Walk", 32, pose_walk, True),
    ("Run", 20, pose_run, True),
    ("Fall", 20, pose_fall, True),
    ("Attack1", 12, make_slash(-60, 50), False),
    ("Attack2", 12, make_slash(50, -60), False),
    ("Attack3", 15, pose_spin, False),
]


def apply_pose(arm, pose):
    for pb in arm.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = Vector()
    for bone, spec in pose.items():
        pb = arm.pose.bones[bone]
        to_local = arm.data.bones[bone].matrix_local.to_3x3().inverted()
        q = Quaternion()
        for axis, deg in spec.get("rot", []):
            q = Quaternion((to_local @ Vector(axis)).normalized(), math.radians(deg)) @ q
        pb.rotation_quaternion = q
        if "loc" in spec:
            pb.location = to_local @ Vector(spec["loc"])


def assign_action(arm, action):
    ad = arm.animation_data or arm.animation_data_create()
    ad.action = action
    if action is not None and hasattr(ad, "action_slot") and getattr(action, "slots", None):
        if len(action.slots):
            ad.action_slot = action.slots[0]


def make_action(arm, name, frames, pose_fn):
    action = bpy.data.actions.new(f"A_Keeper_{name}")
    assign_action(arm, action)
    for f in range(0, frames + 1, 2):
        apply_pose(arm, pose_fn(f / frames))
        for pb in arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=f)
            pb.keyframe_insert("location", frame=f)
    action.use_frame_range = True
    action.frame_start, action.frame_end = 0, frames
    assign_action(arm, action)  # pick up the slot created by the first key
    return action


# ------------------------------------------------------------------------------------------------
# Export / verification / preview
# ------------------------------------------------------------------------------------------------

def select_rig(arm, body):
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = arm


def export(arm, body, path, action=None, frames=0):
    scene = bpy.context.scene
    scene.render.fps = FPS
    assign_action(arm, action)
    if action is None:
        apply_pose(arm, {})
    else:
        scene.frame_start, scene.frame_end = 0, frames
    select_rig(arm, body)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE", add_leaf_bones=False,
        use_armature_deform_only=True, armature_nodetype="NULL",
        bake_anim=action is not None, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0)


def verify(out_dir):
    """Re-import every FBX and check bones, frame ranges and that bones actually move."""
    ok = True
    for fname in sorted(os.listdir(out_dir)):
        if not fname.endswith(".fbx"):
            continue
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=os.path.join(out_dir, fname))
        arms = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if not arms:
            print(f"  FAIL {fname}: no armature"); ok = False; continue
        arm = arms[0]
        bones = len(arm.data.bones)
        info = f"{bones} bones, {len(meshes)} mesh, height {max((m.dimensions.z for m in meshes), default=0):.2f} m"
        action = arm.animation_data.action if arm.animation_data else None
        if action:
            start, end = action.frame_range
            scene = bpy.context.scene
            scene.frame_set(int(start))
            rest = [pb.matrix.copy() for pb in arm.pose.bones]
            moving = set()
            for q in (0.25, 0.5, 0.75):  # cycles return to their start pose at 0.5, so sample several
                scene.frame_set(int(start + (end - start) * q))
                for i, pb in enumerate(arm.pose.bones):
                    m0, m1 = rest[i], pb.matrix
                    if (m0.translation - m1.translation).length > 1e-3 or \
                            m0.to_quaternion().rotation_difference(m1.to_quaternion()).angle > 1e-3:
                        moving.add(pb.name)
            moved = len(moving)
            info += f", frames {start:.0f}-{end:.0f}, {moved} bones move"
            if moved == 0:
                ok = False
                info += "  <-- FAIL"
        print(f"  {fname:28s} {info}")
    return ok


def render_preview(arm, body, path):
    """Five copies of the rig, each posed from a different animation, rendered in one shot."""
    scene = bpy.context.scene
    assign_action(arm, None)
    shots = [("Idle", pose_idle(0.25)), ("Walk", pose_walk(0.25)), ("Run", pose_run(0.25)),
             ("Slash", make_slash(-60, 50)(0.55)), ("Spin", pose_spin(0.12))]
    P = ga.palette()
    for i, (label, pose) in enumerate(shots):
        a2 = arm.copy()
        a2.animation_data_clear()
        scene.collection.objects.link(a2)
        b2 = body.copy()
        b2.parent = a2
        b2.modifiers["Armature"].object = a2
        scene.collection.objects.link(b2)
        a2.location = (i * 1.3, 0, 0)
        a2.rotation_euler = (0, 0, math.radians(-60))
        apply_pose(a2, pose)
    arm.hide_render = body.hide_render = True
    ga.setup_render(scene, (2.6, -6.6, 1.5), (84, 0, 0), 35, path, samples=48)
    for i, (label, _pose) in enumerate(shots):
        ga.label(label, (i * 1.3, -0.8, 0.02), 0.16, P)
    bpy.ops.mesh.primitive_plane_add(size=30, location=(2.6, 0, 0))
    g = bpy.context.active_object
    g.name = "Ground"
    g.data.materials.append(ga.mat("PreviewGround", (0.30, 0.36, 0.26)))
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--preview")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args(argv)
    os.makedirs(args.out, exist_ok=True)

    arm, body = build_character()
    export(arm, body, os.path.join(args.out, "SK_LanternKeeper.fbx"))
    actions = []
    for name, frames, fn, _loops in ANIMS:
        action = make_action(arm, name, frames, fn)
        export(arm, body, os.path.join(args.out, f"A_Keeper_{name}.fbx"), action, frames)
        actions.append(action)
        print(f"exported A_Keeper_{name} ({frames} frames @ {FPS} fps)")

    if args.preview:
        render_preview(arm, body, args.preview)
    if args.verify:
        print("verification:")
        if not verify(args.out):
            sys.exit(1)


if __name__ == "__main__":
    main()
