"""Build a deterministic source-only ZIP from the project-local distribution list."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile
from pipelines.layout import bundle_root


def payload_files():
    cubeless = bundle_root(); root = cubeless.parent
    manifest = json.loads((cubeless / 'distribution.json').read_text(encoding='utf-8'))
    file_list = 'cubeless/distribution-files.txt'
    if manifest.get('file_list') != file_list:
        raise ValueError('Expected explicit cubeless/distribution-files.txt product list')
    paths = (cubeless / 'distribution-files.txt').read_text(encoding='utf-8-sig').splitlines()
    files, seen = {}, set()
    for path in paths:
        relative = PurePosixPath(path)
        if (not path or relative.is_absolute() or relative.as_posix() != path
                or '..' in relative.parts or '\\' in path or ':' in path
                or not path.startswith(('Plugins/CubelessMCP_UE58/', 'cubeless/'))):
            raise ValueError('Invalid distribution file-list entry: ' + path)
        if path.casefold() in seen:
            raise ValueError('Duplicate distribution file-list entry: ' + path)
        seen.add(path.casefold())
        if (any(p in manifest['excluded_parts'] for p in relative.parts)
                or path in manifest['excluded_files']
                or any(p.startswith('.venv') for p in relative.parts)):
            raise ValueError('Generated/local/test file listed as product source: ' + path)
        source = root / path
        for parent in (source, *source.parents):
            if parent == root.parent:
                break
            if parent.is_symlink() or getattr(parent.lstat(), 'st_file_attributes', 0) & 0x400:
                raise ValueError('Reparse-point distribution source refused: ' + path)
        if not source.is_file() or not source.resolve().is_relative_to(root):
            raise ValueError('Missing/outside required distribution file: ' + path)
        files[path] = source.read_bytes()
    required = [
        'Plugins/CubelessMCP_UE58/CubelessMCP_UE58.uplugin', 'cubeless/distribution.json',
        file_list, 'cubeless/README.md', 'cubeless/tools/run_pipeline.py',
        'cubeless/tools/package_distribution.py', 'cubeless/tools/official_mcp_client.py',
        'cubeless/tools/pipeline-requirements.txt',
        *('cubeless/tools/pipelines/' + name for name in manifest['pipeline_files']),
    ]
    missing = [p for p in required if p not in files]
    if missing:
        raise ValueError('Required files absent from distribution list: ' + ', '.join(missing))
    return dict(sorted(files.items()))


def build(output):
    files = payload_files()
    hashes = {p: hashlib.sha256(data).hexdigest() for p, data in files.items()}
    files['cubeless/bundle-manifest.json'] = (json.dumps(dict(format='cubeless_source_inventory_v1', files=hashes), sort_keys=True, indent=2) + '\n').encode('utf-8')
    output = Path(output).resolve()
    if output.is_relative_to((bundle_root().parent / 'Plugins').resolve()):
        raise ValueError('Distribution output must not be written into plugin source')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as stream:
        with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path, data in sorted(files.items()):
                info = zipfile.ZipInfo(path, date_time=(2000, 1, 1, 0, 0, 0)); info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, data, compresslevel=9)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None or any(hashlib.sha256(archive.read(p)).hexdigest() != h for p, h in hashes.items()):
            raise ValueError('Distribution archive verification failed; preserve source and failed archive')
    print(json.dumps(dict(output=str(output), files=len(files), bytes=output.stat().st_size, sha256=hashlib.sha256(output.read_bytes()).hexdigest(), source_only=True)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    build(args.output if args.output and args.output.is_absolute() else bundle_root().parent / args.output if args.output else bundle_root() / 'build/distribution/CubelessMCP_UE58-source.zip')


if __name__ == '__main__':
    main()
