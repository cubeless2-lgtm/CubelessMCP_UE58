"""Checksummed private workstation-transfer packs; standard library only.

This module never searches an Epic/Markerless installation. Model bytes are
prepared once on the source workstation and restored without model conversion.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import uuid
import zipfile

FORMAT = 'cubeless-transfer-v1'
CHUNK = 4 * 1024 * 1024
RESERVED = {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}


def extended(path):
    """Use long Windows paths without changing any OS setting."""
    path = Path(path).absolute()
    value = str(path)
    if os.name == 'nt' and not value.startswith('\\\\?\\'):
        value = '\\\\?\\UNC\\' + value[2:] if value.startswith('\\\\') else '\\\\?\\' + value
    return Path(value)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_name(name):
    if not isinstance(name, str) or not name or '\\' in name or ':' in name:
        raise ValueError(f'Unsafe pack path: {name!r}')
    p = PurePosixPath(name)
    if p.is_absolute() or '/'.join(p.parts) != name or any(x in ('', '.', '..') or x.endswith(('.', ' ')) or x.split('.')[0].upper() in RESERVED for x in p.parts):
        raise ValueError(f'Unsafe pack path: {name!r}')
    return p


def target(root, name):
    root = extended(root)
    p = root.joinpath(*safe_name(name).parts)
    current = root
    for part in safe_name(name).parts:
        current = current / part
        if current.exists() or current.is_symlink():
            info = current.lstat()
            if current.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
                raise ValueError(f'Reparse/symlink destination refused: {current}')
    if not p.resolve().is_relative_to(Path(root).resolve()):
        raise ValueError('Destination escapes selected root')
    return p


def create_pack(output, component, destination, sources, engine, extra=None):
    """sources maps relative destinations to source Path or literal bytes."""
    output = Path(output)
    if output.exists():
        raise FileExistsError(output)
    names = sorted(sources)
    if len({x.casefold() for x in names}) != len(names):
        raise ValueError('Case-insensitive path collision')
    for name in names:
        safe_name(name)
    output.parent.mkdir(parents=True, exist_ok=True)
    partial = output.with_name(output.name + '.partial')
    if partial.exists():
        raise FileExistsError(partial)
    manifest = dict(format=FORMAT, component=component, destination=destination,
                    engine=engine, scope='private transfer between the license holder workstations; original terms retained',
                    requires_original_markerless=False, files=[], **(extra or {}))
    with zipfile.ZipFile(partial, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as archive:
        for index, name in enumerate(names):
            value = sources[name]
            h, size = hashlib.sha256(), 0
            with archive.open('payload/' + name, 'w', force_zip64=True) as out:
                if isinstance(value, bytes):
                    out.write(value); h.update(value); size = len(value)
                else:
                    with Path(value).open('rb') as src:
                        while chunk := src.read(CHUNK):
                            h.update(chunk); size += len(chunk); out.write(chunk)
            manifest['files'].append(dict(path=name, bytes=size, sha256=h.hexdigest()))
            if index % 100 == 0 or size > 100_000_000:
                print(f'{component}: {index+1}/{len(names)} {name} ({size} bytes)', flush=True)
        manifest['bytes'] = sum(x['bytes'] for x in manifest['files'])
        archive.writestr('manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2).encode('utf-8'))
    partial.rename(output)
    receipt = dict(file=output.name, component=component, bytes=output.stat().st_size,
                   sha256=digest(output), payload_bytes=manifest['bytes'], files=len(names))
    print(json.dumps(receipt), flush=True)
    return receipt


def read_manifest(archive):
    infos = archive.infolist()
    if len(infos) > 60000 or len({x.filename.casefold() for x in infos}) != len(infos):
        raise ValueError('Duplicate or excessive archive entries')
    info = archive.getinfo('manifest.json')
    if info.file_size > 16 * 1024 * 1024:
        raise ValueError('Manifest too large')
    m = json.loads(archive.read(info))
    if m.get('format') != FORMAT or m.get('destination') not in ('plugin', 'project'):
        raise ValueError('Unsupported pack format/destination')
    names = []
    for f in m['files']:
        safe_name(f['path']); names.append(f['path'])
        if type(f['bytes']) is not int or not 0 <= f['bytes'] <= 1024**4 or not re.fullmatch('[0-9a-f]{64}', f['sha256']):
            raise ValueError('Invalid file metadata')
        z = archive.getinfo('payload/' + f['path'])
        if z.file_size != f['bytes'] or z.is_dir() or stat.S_ISLNK(z.external_attr >> 16):
            raise ValueError('Archive file metadata mismatch')
    if len(set(x.casefold() for x in names)) != len(names):
        raise ValueError('Duplicate output paths')
    if set(x.filename for x in infos) != {'manifest.json', *('payload/' + x for x in names)}:
        raise ValueError('Unlisted archive entries')
    if sum(f['bytes'] for f in m['files']) != m['bytes']:
        raise ValueError('Total size mismatch')
    return m


def check_engine(manifest, engine):
    expected = manifest.get('engine', {})
    binary_id = manifest.get('binary_build_id')
    if not expected and not binary_id:
        return
    if not engine:
        raise ValueError('Supply --engine for the prepared UE assets/binaries')
    folder = Path(engine)
    actual = json.loads((folder / 'Engine/Build/Build.version').read_text(encoding='utf-8-sig'))
    for key, value in expected.items():
        if actual.get(key) != value:
            raise ValueError(f'Engine mismatch: {key}: expected {value}, found {actual.get(key)}')
    if binary_id:
        modules = json.loads((folder / 'Engine/Binaries/Win64/UnrealEditor.modules').read_text(encoding='utf-8-sig'))
        if modules.get('BuildId') != binary_id:
            raise ValueError('Editor binary BuildId differs; rebuild this plugin from source for that engine')


def preflight(archive_path, root, engine=None):
    root = extended(Path(root).resolve())
    with zipfile.ZipFile(archive_path) as z:
        m = read_manifest(z); check_engine(m, engine)
    existing = 0
    for f in m['files']:
        p = target(root, f['path'])
        if p.exists():
            if not p.is_file() or p.stat().st_size != f['bytes'] or digest(p) != f['sha256']:
                raise FileExistsError(f'Existing file differs; preserved: {p}')
            existing += 1
    return m, existing


def verify_pack(archive_path, root=None, engine=None):
    with zipfile.ZipFile(archive_path) as z:
        m = read_manifest(z); check_engine(m, engine)
        for f in m['files']:
            h = hashlib.sha256(); size = 0
            with z.open('payload/' + f['path']) as stream:
                while chunk := stream.read(CHUNK):
                    h.update(chunk); size += len(chunk)
            if size != f['bytes'] or h.hexdigest() != f['sha256']:
                raise ValueError('Payload hash mismatch: ' + f['path'])
            if root:
                p = target(Path(root).resolve(), f['path'])
                if not p.is_file() or p.stat().st_size != size or digest(p) != f['sha256']:
                    raise ValueError('Installed file mismatch: ' + str(p))
    return dict(status='verified', component=m['component'], files=len(m['files']), bytes=m['bytes'])


def install_pack(archive_path, root, engine=None):
    root = extended(Path(root).resolve())
    m, existing = preflight(archive_path, root, engine)
    def receipt():
        records = target(root, '.cbm-packs/' + m['component'] + '.json')
        records.parent.mkdir(parents=True, exist_ok=True)
        if records.exists():
            prior = json.loads(records.read_text(encoding='utf-8'))
            if prior.get('format') != FORMAT or prior.get('component') != m['component']:
                raise ValueError('Unrecognized existing install receipt; preserved')
        temporary = records.with_name(records.name + '.' + uuid.uuid4().hex + '.tmp')
        with temporary.open('x', encoding='utf-8') as out:
            json.dump(m, out, ensure_ascii=False, indent=2)
        temporary.replace(records)
    if existing == len(m['files']):
        receipt()
        return dict(status='already_installed', component=m['component'], installed=0, unchanged=existing)
    root.mkdir(parents=True, exist_ok=True)
    stage = root / ('.cbm-install-' + uuid.uuid4().hex)
    stage.mkdir()
    published = []
    try:
        # Validate the complete payload before publishing any destination file.
        with zipfile.ZipFile(archive_path) as z:
            for f in m['files']:
                p = target(stage, f['path']); p.parent.mkdir(parents=True, exist_ok=True)
                h, size = hashlib.sha256(), 0
                with z.open('payload/' + f['path']) as src, p.open('xb') as out:
                    while chunk := src.read(CHUNK):
                        h.update(chunk); size += len(chunk)
                        if size > f['bytes']:
                            raise ValueError('Payload size exceeded')
                        out.write(chunk)
                if size != f['bytes'] or h.hexdigest() != f['sha256']:
                    raise ValueError('Payload hash mismatch: ' + f['path'])
        for f in m['files']:
            p = target(root, f['path'])
            if p.exists():
                if not p.is_file() or digest(p) != f['sha256']:
                    raise FileExistsError('Destination changed during install: ' + str(p))
                continue
            p.parent.mkdir(parents=True, exist_ok=True)
            # Windows rename refuses an existing destination; never replace it.
            if os.name == 'nt':
                (stage / f['path']).rename(p)
            else:
                os.link(stage / f['path'], p)
            published.append(f['path'])
        receipt()
        return dict(status='installed', component=m['component'], installed=len(published), unchanged=existing)
    except Exception as exc:
        raise RuntimeError(f'{exc}; published_new_files={published!r}') from exc
    finally:
        if stage.parent.resolve() != root or not stage.name.startswith('.cbm-install-'):
            raise RuntimeError('Unsafe staging cleanup')
        shutil.rmtree(stage)


def verify_installed(root, component='body'):
    root = extended(Path(root).resolve())
    manifest = json.loads(target(root, '.cbm-packs/' + component + '.json').read_text(encoding='utf-8'))
    if manifest.get('format') != FORMAT:
        raise ValueError('Invalid installation receipt')
    for f in manifest['files']:
        p = target(root, f['path'])
        if not p.is_file() or p.stat().st_size != f['bytes'] or digest(p) != f['sha256']:
            raise ValueError('Installed file mismatch: ' + str(p))
    return dict(status='verified', component=component, files=len(manifest['files']))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['install', 'verify', 'verify-installed'])
    p.add_argument('--pack', type=Path)
    p.add_argument('--root', type=Path)
    p.add_argument('--engine', type=Path)
    p.add_argument('--component', default='body', choices=['body', 'hands', 'runtime', 'plugin', 'manny'])
    a = p.parse_args()
    if a.action == 'verify-installed':
        result = verify_installed(a.root, a.component)
    elif a.action == 'install':
        if not a.root or not a.pack:
            p.error('--pack and --root are required')
        result = install_pack(a.pack, a.root, a.engine)
    else:
        result = verify_pack(a.pack, a.root, a.engine)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
