"""Run in this project's Unreal Python after prepare_chmr_fused.py.

By default reads Saved/CubelessBodyMocap/CHMRModels/chmr_backbone_fused.onnx.
Refuses to overwrite an existing model asset. No MetaHuman plugin is needed.
"""
import unreal

destination = "/CubelessBodyMocap/Models/Offline/chmr_backbone_fused"
if unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError("Optimized model already exists; preserving it")
task = unreal.AssetImportTask()
task.filename = unreal.Paths.project_saved_dir() + "CubelessBodyMocap/CHMRModels/chmr_backbone_fused.onnx"
task.destination_path = "/CubelessBodyMocap/Models/Offline"
task.destination_name = "chmr_backbone_fused"
task.automated = True
task.replace_existing = False
task.save = True
task.factory = unreal.NNEModelDataFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
if not unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError("Optimized model import failed")
unreal.log("Imported " + destination)
