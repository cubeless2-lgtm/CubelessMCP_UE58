"""Restore pinned optional HandFlow sources/weights; never changes other runtimes.

MANO must be supplied separately under the user's agreement. See ThirdParty/HandFlow.
Existing mismatched files are refused, not overwritten. Downloads publish after SHA256.
"""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import urllib.request


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--weights', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / 'ThirdParty/HandFlow'
    manifest = json.loads((root / 'manifest.json').read_text())
    revision = manifest['revision']
    base = f"https://raw.githubusercontent.com/{revision['repo']}/{revision['commit']}/"
    items = [dict(x, url=base+x['upstream_path']) for x in manifest['source_files']]
    if args.weights:
        items += manifest['models']

    def restore(item):
        path = root / item['path']
        if path.exists():
            if digest(path) != item['sha256']:
                raise ValueError('Existing file differs; inspect before replacing: '+str(path))
            return
        if not item['url']:
            raise FileNotFoundError('Supply licensed MANO_RIGHT.pkl: '+str(path))
        path.parent.mkdir(parents=True, exist_ok=True)
        partial = path.with_suffix(path.suffix+'.partial')
        print('Downloading', item['path'], flush=True)
        with urllib.request.urlopen(item['url'], timeout=60) as response, partial.open('wb') as output:
            while chunk := response.read(4*1024*1024):
                output.write(chunk)
        if digest(partial) != item['sha256']:
            raise ValueError('Downloaded SHA256 mismatch: '+str(partial))
        partial.replace(path)
        print('Verified', item['path'], flush=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(restore, items))
    print('Pinned sources/models ready. Python environment is prepared separately.')


if __name__ == '__main__':
    main()
