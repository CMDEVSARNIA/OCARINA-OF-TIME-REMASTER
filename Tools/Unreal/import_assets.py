"""
Imports the Emberveil art kit (Art/Source/Meshes/*.fbx) into /Game/Emberveil/Meshes.

Run inside the Unreal Editor:  Tools > Execute Python Script... > pick this file
or headless:                   UnrealEditor-Cmd.exe <abs path>/Emberveil.uproject -run=pythonscript -script="<abs path>/Tools/Unreal/import_assets.py"

Safe to re-run: existing assets are replaced. The game code picks the meshes up automatically
(see UEmberveilStatics::LoadArtMesh) and falls back to greybox shapes for anything missing.
"""

import glob
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_DIR = os.path.join(PROJECT_DIR, "Art", "Source", "Meshes")
DESTINATION = "/Game/Emberveil/Meshes"
CHARACTER_DIR = os.path.join(PROJECT_DIR, "Art", "Source", "Characters")
CHARACTER_DESTINATION = "/Game/Emberveil/Characters"

# Meshes that never need their own collision (decorative, or collision handled in code).
NO_COLLISION = {"SM_LanternKeeper", "SM_Blade", "SM_Key", "SM_Mangrove_A", "SM_Mangrove_B"}


def make_task(fbx_path):
    name = os.path.splitext(os.path.basename(fbx_path))[0]

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH

    mesh_data = options.static_mesh_import_data
    mesh_data.combine_meshes = True
    mesh_data.auto_generate_collision = name not in NO_COLLISION
    mesh_data.generate_lightmap_u_vs = True
    mesh_data.import_uniform_scale = 1.0

    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = DESTINATION
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    return task


def make_skeletal_task(fbx_path):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = True
    options.import_textures = False
    options.create_physics_asset = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH

    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = CHARACTER_DESTINATION
    task.destination_name = os.path.splitext(os.path.basename(fbx_path))[0]
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    return task


def make_animation_task(fbx_path, skeleton):
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.animation_length = unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME

    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = CHARACTER_DESTINATION
    task.destination_name = os.path.splitext(os.path.basename(fbx_path))[0]
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    return task


def import_character():
    mesh_fbx = os.path.join(CHARACTER_DIR, "SK_LanternKeeper.fbx")
    if not os.path.exists(mesh_fbx):
        unreal.log_warning(f"[Emberveil] No character FBX at {mesh_fbx}; skipping the rigged character")
        return

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tools.import_asset_tasks([make_skeletal_task(mesh_fbx)])

    skeleton = unreal.load_asset(f"{CHARACTER_DESTINATION}/SK_LanternKeeper_Skeleton")
    if not skeleton:
        unreal.log_error("[Emberveil] Skeleton SK_LanternKeeper_Skeleton not found after import")
        return

    anim_files = sorted(glob.glob(os.path.join(CHARACTER_DIR, "A_Keeper_*.fbx")))
    tasks = [make_animation_task(f, skeleton) for f in anim_files]
    tools.import_asset_tasks(tasks)

    # The game loads animations by name (A_Keeper_Walk, ...); normalise whatever the importer chose.
    for task in tasks:
        wanted = f"{CHARACTER_DESTINATION}/{task.destination_name}"
        for path in task.imported_object_paths:
            asset = unreal.load_asset(path)
            if isinstance(asset, unreal.AnimSequence) and path.split(".")[0] != wanted:
                unreal.EditorAssetLibrary.rename_asset(path.split(".")[0], wanted)
        unreal.log(f"[Emberveil] {os.path.basename(task.filename)} -> {wanted}")


def main():
    files = sorted(glob.glob(os.path.join(SOURCE_DIR, "*.fbx")))
    if not files:
        unreal.log_error(f"No FBX files found in {SOURCE_DIR}")
        return

    # Prefer the classic FBX importer so the options above (collision, combine) are honoured.
    try:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    except Exception as exc:  # older/newer engines without the flag
        unreal.log_warning(f"Could not switch FBX importer: {exc}")

    tasks = [make_task(f) for f in files]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported = sum(1 for t in tasks if t.imported_object_paths)
    for t in tasks:
        unreal.log(f"[Emberveil] {os.path.basename(t.filename)} -> {list(t.imported_object_paths)}")
    unreal.log(f"[Emberveil] Imported {imported}/{len(tasks)} meshes into {DESTINATION}")

    import_character()


main()
