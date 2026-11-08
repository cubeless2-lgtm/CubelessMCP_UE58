"""Import paired compact models generated into Saved/CubelessBodyMocap/CompactSAM.
Run inside this project's Editor. Preserves any existing model assets.
"""
import unreal
names=['sam2_image_encoder_compact','sam2_mask_decoder_compact']
for name in names:
 if unreal.EditorAssetLibrary.does_asset_exist('/CubelessBodyMocap/SAM2/'+name):raise RuntimeError('Preserving existing '+name)
for name in names:
 task=unreal.AssetImportTask()
 task.filename=unreal.Paths.project_saved_dir()+'CubelessBodyMocap/CompactSAM/'+name+'.onnx'
 task.destination_path='/CubelessBodyMocap/SAM2';task.destination_name=name
 task.automated=True;task.replace_existing=False;task.save=True;task.factory=unreal.NNEModelDataFactory()
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 if not unreal.EditorAssetLibrary.does_asset_exist('/CubelessBodyMocap/SAM2/'+name):raise RuntimeError('Import failed: '+name)
 unreal.log('Imported '+name)
