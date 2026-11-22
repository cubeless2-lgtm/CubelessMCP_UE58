"""Build a private, already-prepared workstation transfer, never a public model release."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from model_pack import create_pack, digest

COMPONENTS = ('plugin', 'body', 'hands', 'manny')


def write_bootstrap(output, plugin, receipts):
    """Keep the release reproducible, including its stdlib-only installer."""
    for name in ['model_pack.py', 'install_transfer.py', 'Install-CubelessMocap.ps1']:
        shutil.copyfile(plugin / 'Tools' / name, output / name)
    for name in ['TRANSFER.md', 'README.md', 'WINDOW_API.md']:
        shutil.copyfile(plugin / name, output / name)
    archive_bytes = sum(x['bytes'] for x in receipts)
    installed_bytes = sum(x['payload_bytes'] for x in receipts)
    free_bytes = max(x['payload_bytes'] for x in receipts) * 2 + installed_bytes
    instructions = f'''Cubeless Body Mocap - 같은 이용자의 다른 PC로 이전

Release 폴더 전체를 옮기고 ZIP은 풀지 않은 상태로 보관하세요.
원본 MetaHuman Markerless 플러그인 설치는 필요 없습니다.
Python/Torch 실행 환경은 배포하지 않습니다. 손 모캡용 환경은 각 PC에서 직접 설치하세요.
Windows x64 / UE 5.8.4 CL58902052, HandFlow에는 NVIDIA/CUDA 12.8 호환 환경이 필요합니다.
원본 코드와 모델의 이용 조건은 그대로 적용됩니다. 공개 재배포용 묶음이 아닙니다.

압축 파일: {archive_bytes / 1e9:.2f} GB / 설치 데이터: {installed_bytes / 1e9:.2f} GB
모델팩 설치 시작 시 대상 드라이브 여유 공간(로컬 Python 설치 공간 별도): {free_bytes / 1e9:.2f} GB 이상
전신 ZIP이 4 GB를 넘으므로 USB는 NTFS 또는 exFAT를 사용하세요.

1. 사용할 기존 프로젝트의 Unreal Editor를 닫습니다.
2. PowerShell에서 아래 경로를 실제 경로로 바꿔 실행하세요.

& 'D:/Transfer/CubelessMocap/Install-CubelessMocap.ps1' -Project 'D:/Projects/MyProject/MyProject.uproject' -Engine 'C:/Program Files/Epic Games/UE_5.8'

3. 손 모캡을 사용하려면 대상 PC에서 Python 3.12.10과 HandFlow 의존성을 직접 설치합니다.
   TRANSFER.md의 로컬 Python 절과 플러그인 README 6절을 따르세요.

먼저 확인만 하려면 위 명령 끝에 -Plan을 추가합니다.
기존 파일과 충돌하면 덮어쓰지 않고 중단합니다. 세부 안내와 Python 직접 실행 방법은 TRANSFER.md에 있습니다.
'''
    (output / 'READ-ME-FIRST.txt').write_text(instructions, encoding='utf-8-sig')
    hashes = {x['file']: x['sha256'] for x in receipts}
    for name in ['release.json', 'READ-ME-FIRST.txt', 'TRANSFER.md', 'README.md', 'WINDOW_API.md', 'model_pack.py',
                 'install_transfer.py', 'Install-CubelessMocap.ps1',
                 *(x.name for x in output.glob('*-receipt.json'))]:
        hashes[name] = digest(output / name)
    (output / 'release-files.sha256').write_text(''.join(f'{sha}  {name}\n' for name, sha in sorted(hashes.items())), encoding='utf-8')


def tree(folder, prefix, exclude=()):
    return {prefix + p.relative_to(folder).as_posix(): p for p in folder.rglob('*')
            if p.is_file() and not any(x in p.relative_to(folder).parts for x in ('__pycache__', *exclude))
            and p.suffix not in ('.pyc', '.pyo')}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--project', type=Path, required=True)
    p.add_argument('--manny-dependencies', type=Path, required=True)
    p.add_argument('--components', nargs='+', choices=COMPONENTS, default=list(COMPONENTS))
    a = p.parse_args()
    if list(a.output.glob('*runtime*.zip')) or (a.output / 'runtime-receipt.json').exists():
        p.error('Move old Python runtime ZIP/receipt out of the release folder; this release uses local Python.')
    plugin = Path(__file__).resolve().parents[1]; project = a.project.resolve().parent
    version = json.loads((a.engine / 'Engine/Build/Build.version').read_text(encoding='utf-8-sig'))
    engine = {k: version[k] for k in ['MajorVersion', 'MinorVersion', 'PatchVersion', 'Changelist']}
    commit = subprocess.check_output(['git', '-C', str(project), 'rev-parse', 'HEAD'], text=True).strip()
    receipts = []
    for component in a.components:
        extra = dict(source_commit=commit)
        if component == 'body':
            sources = tree(plugin / 'Content', 'Content/')
            # Unpromoted CHMR experiment is not referenced by any production path.
            sources.pop('Content/Models/Offline/chmr_backbone_fused_layout.uasset', None)
            sources.update(tree(plugin / 'Resources/Licenses', 'Resources/Licenses/'))
        elif component == 'hands':
            root = plugin / 'ThirdParty/HandFlow'
            manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
            for item in manifest['models'] + manifest['source_files']:
                if digest(root / item['path']) != item['sha256']:
                    raise ValueError('Pinned HandFlow source/model differs: ' + item['path'])
            sources = {}
            for folder in ['models', 'runtime', 'handflow']:
                sources.update(tree(root / folder, 'ThirdParty/HandFlow/' + folder + '/'))
            for f in root.iterdir():
                if f.is_file():
                    sources['ThirdParty/HandFlow/' + f.name] = f
        elif component == 'manny':
            dependencies = json.loads(a.manny_dependencies.read_text(encoding='utf-8'))
            sources = {name: project / name for name in dependencies['files']}
            extra['asset_dependencies'] = dependencies
        else:
            # Start from published source, not unrelated dirty workstation files.
            rel = plugin.relative_to(project).as_posix()
            files = subprocess.check_output(['git', '-C', str(project), 'ls-files', '-z', '--', rel]).decode('utf-8').split('\0')
            sources = {name[len(rel)+1:]: subprocess.check_output(['git', '-C', str(project), 'show', 'HEAD:' + name])
                       for name in files if name}
            # Only this task's packaging/installation changes overlay the source release.
            for name in ['Tools/model_pack.py', 'Tools/build_transfer.py', 'Tools/install_transfer.py', 'Tools/Install-CubelessMocap.ps1',
                         'Tools/prepare_content.py', 'Tools/test_model_pack.py', 'Tools/run_hand_worker.py', 'TRANSFER.md', 'README.md']:
                if (plugin / name).is_file():
                    sources[name] = plugin / name
            extra['source_overlays'] = ['packaging tools/docs', 'existing one-line worker JSON byte-decoding compatibility fix']
            for f in (plugin / 'Binaries/Win64').iterdir():
                if f.suffix == '.dll' or f.name == 'UnrealEditor.modules':
                    sources['Binaries/Win64/' + f.name] = f
            sources['ThirdParty/FFmpeg/Win64/ffmpeg.exe'] = plugin / 'ThirdParty/FFmpeg/Win64/ffmpeg.exe'
            extra['binary_build_id'] = json.loads((plugin / 'Binaries/Win64/UnrealEditor.modules').read_text())['BuildId']
        output = a.output / ('CubelessMocap-' + component + '-UE5.8.4.zip')
        receipts.append(create_pack(output, component, 'project' if component == 'manny' else 'plugin', sources, engine, extra))
        (a.output / (component + '-receipt.json')).write_text(json.dumps(receipts[-1], indent=2), encoding='utf-8')
    # Keep independently built component receipts together for an installer.
    all_receipts = [json.loads(f.read_text(encoding='utf-8')) for f in sorted(a.output.glob('*-receipt.json'))]
    if any(x['component'] not in COMPONENTS for x in all_receipts):
        raise ValueError('Unsupported receipt in this local-Python release')
    (a.output / 'release.json').write_text(json.dumps(dict(format='cubeless-transfer-release-v1', engine=engine,
                                                         source_commit=commit, python_runtime='local-per-pc', components=all_receipts), indent=2), encoding='utf-8')
    write_bootstrap(a.output, plugin, all_receipts)


if __name__ == '__main__':
    main()
