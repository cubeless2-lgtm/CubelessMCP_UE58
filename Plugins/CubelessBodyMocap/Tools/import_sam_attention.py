"""Run inside this Editor after prepare_sam_attention.py, preserving originals."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
for name, version in [('sam2_memory_attention_rows512','sam2_query_rows_v1'),
                      ('sam2_memory_attention_fused','sam2_fused_attention_v1')]:
    source = root/'Saved/CubelessBodyMocap/AttentionModels'/(name+'.onnx')
    if not source.exists():
        continue
    provenance = json.loads(source.with_suffix('.provenance.json').read_text(encoding='utf-8'))
    if provenance.get('version') != version or provenance.get('weights_byte_identical') is not True or provenance.get('dtype') != 'float32':
        raise ValueError('Prepared FP32 attention model provenance is required')
    if name.endswith('rows512') and provenance.get('query_rows') != 512:
        raise ValueError('Expected 512 query rows')
    if hashlib.sha256(source.read_bytes()).hexdigest() != provenance['output_sha256']:
        raise ValueError('Prepared model hash differs from provenance')
    destination = '/CubelessBodyMocap/SAM2/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        unreal.log('Preserved existing model without replacement: ' + destination)
        continue
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = '/CubelessBodyMocap/SAM2'
    task.destination_name = name
    task.automated = True
    task.replace_existing = False
    task.save = True
    task.factory = unreal.NNEModelDataFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not unreal.EditorAssetLibrary.does_asset_exist(destination):
        raise RuntimeError('Model import did not produce ' + destination)
    unreal.log('Imported ' + destination)
