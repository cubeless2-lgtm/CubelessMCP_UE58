"""Run in this project's Unreal Python after prepare_camera_seeded.py.

By default reads Saved/CubelessBodyMocap/WristInvestigation/camera_calib_seeded.onnx.
Refuses to overwrite an existing model asset. No MetaHuman plugin is needed.
"""
import unreal

destination = "/CubelessBodyMocap/Models/Offline/camera_calib_seeded"
if unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError("Seeded camera model already exists; preserving it")
task = unreal.AssetImportTask()
task.filename = unreal.Paths.project_saved_dir() + "CubelessBodyMocap/WristInvestigation/camera_calib_seeded.onnx"
task.destination_path = "/CubelessBodyMocap/Models/Offline"
task.destination_name = "camera_calib_seeded"
task.automated = True
task.replace_existing = False
task.save = True
task.factory = unreal.NNEModelDataFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
if not unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError("Seeded camera model import failed")
unreal.log("Imported " + destination)
