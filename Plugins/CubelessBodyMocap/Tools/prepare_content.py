"""Copy licensed model content from an existing Epic installation, without overwriting files."""
import argparse,shutil,json,hashlib
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--engine',type=Path,required=True);args=ap.parse_args()
source=args.engine/'Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.8'
dest=Path(__file__).resolve().parents[1]
assert (source/'MetaHumanBodyTracker.uplugin').is_file(), 'Install the Epic markerless plugin into this engine first.'
relative=[]
for folder in ['Content/Models/Offline','Content/Detectron2','Content/SAM2','Resources/Licenses']:
 relative += [f.relative_to(source) for f in (source/folder).rglob('*') if f.is_file()]
relative += [Path('Content')/n for n in ['SMPLX_NEUTRAL_2020_locked_head_array_f32.uasset','gmmPriorExport.json','SKM_SMPL.uasset','SKEL_SMPL.uasset','IK_SMPL.uasset','PA_SMPL.uasset']]
for rel in relative:
 target=dest/rel
 if target.exists():
  if target.stat().st_size!=(source/rel).stat().st_size:raise RuntimeError('Existing content differs: '+str(target))
  continue
 target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/rel,target)
print('Content prepared. Original MetaHuman plugins can stay disabled. Existing files were not overwritten.')
