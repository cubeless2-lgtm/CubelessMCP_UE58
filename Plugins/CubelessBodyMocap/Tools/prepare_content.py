"""Restore/verify a prepared body pack without an original Markerless install."""
import argparse
from pathlib import Path
import shutil
from model_pack import install_pack, verify_installed, preflight


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine', type=Path)
    p.add_argument('--pack', type=Path)
    p.add_argument('--verify', action='store_true')
    p.add_argument('--plugin', type=Path, default=Path(__file__).resolve().parents[1])
    a = p.parse_args(); dest = a.plugin.resolve()
    if a.pack:
        manifest, _ = preflight(a.pack, dest, a.engine)
        if manifest['component'] != 'body':
            p.error('--pack must name the prepared body-model pack')
        print(install_pack(a.pack, dest, a.engine)); return
    # Checked BEFORE any original-plugin path or descriptor lookup.
    if (dest / '.cbm-packs/body.json').is_file():
        print(verify_installed(dest, 'body')); return
    if a.verify:
        p.error('No prepared-pack receipt. Install --pack first; no original plugin was accessed.')
    if not a.engine:
        p.error('Supply a prepared --pack, or --engine for the legacy one-time source copy.')
    source = a.engine / 'Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.8'
    if not (source / 'MetaHumanBodyTracker.uplugin').is_file():
        p.error('Original source absent. Use the prepared Cubeless body-model pack instead.')
    relative = []
    for folder in ['Content/Models/Offline', 'Content/Detectron2', 'Content/SAM2', 'Resources/Licenses']:
        relative.extend(f.relative_to(source) for f in (source / folder).rglob('*') if f.is_file())
    relative.extend(Path('Content') / n for n in ['SMPLX_NEUTRAL_2020_locked_head_array_f32.uasset',
                    'gmmPriorExport.json', 'SKM_SMPL.uasset', 'SKEL_SMPL.uasset', 'IK_SMPL.uasset', 'PA_SMPL.uasset'])
    for name in relative:
        target = dest / name
        if target.exists() and target.stat().st_size != (source / name).stat().st_size:
            raise RuntimeError('Existing content differs; preserved: ' + str(target))
    for name in relative:
        target = dest / name
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(source / name, target)
    print('Original contents copied without overwriting. Seeded/optimized models still need preparation on this legacy route.')


if __name__ == '__main__':
    main()
