"""Install a private transfer into an existing Unreal project, with the Editor closed."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
from model_pack import digest, install_pack, preflight


def running_editors():
    if os.name != 'nt':
        raise RuntimeError('This transfer targets Windows x64')
    code = "[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); @(Get-CimInstance Win32_Process -Filter \"name LIKE 'UnrealEditor%'\" | Select-Object ProcessId,CommandLine) | ConvertTo-Json -Compress"
    result = subprocess.run(['powershell.exe', '-NoProfile', '-Command', code],
                            capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)
    value = json.loads(result.stdout) if result.stdout.strip() else []
    return value if isinstance(value, list) else [value]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--release', type=Path, default=Path(__file__).resolve().parent)
    p.add_argument('--project', type=Path, required=True)
    p.add_argument('--engine', type=Path, required=True)
    p.add_argument('--skip-plugin', action='store_true', help='Use existing source/binaries; install model/data packs only')
    p.add_argument('--keep-runtime', action='store_true', help=argparse.SUPPRESS)  # Legacy no-op: Python is always local.
    p.add_argument('--without-hands', action='store_true')
    p.add_argument('--without-manny', action='store_true')
    p.add_argument('--no-enable', action='store_true')
    p.add_argument('--plan', action='store_true', help='Read-only full conflict/hash preflight')
    a = p.parse_args()
    project = a.project.resolve()
    if project.suffix.lower() != '.uproject' or not project.is_file():
        p.error('--project must name the existing .uproject')
    catalog = json.loads((a.release / 'release.json').read_text(encoding='utf-8'))
    if catalog.get('format') != 'cubeless-transfer-release-v1':
        raise ValueError('Unknown release catalog')
    items = {x['component']: x for x in catalog['components']}
    selected = ['plugin', 'body', 'hands', 'manny']
    if a.skip_plugin: selected.remove('plugin')
    if a.without_hands: selected.remove('hands')
    if a.without_manny: selected.remove('manny')
    plugin = project.parent / 'Plugins/CubelessBodyMocap'
    if a.skip_plugin and not (plugin / 'CubelessBodyMocap.uplugin').is_file():
        raise FileNotFoundError('Existing Cubeless plugin required by --skip-plugin')
    if not a.plan:
        for editor in running_editors():
            command = str(editor.get('CommandLine') or '').replace('\\', '/').lower()
            if str(project).replace('\\', '/').lower() in command or project.name.lower() in command:
                raise RuntimeError(f"Close this project's Editor before installing: PID {editor['ProcessId']}")
    prepared = []
    for name in selected:
        item = items[name]
        if Path(item['file']).name != item['file'] or '\\' in item['file'] or ':' in item['file']:
            raise ValueError('Unsafe catalog filename')
        pack = a.release / item['file']
        if pack.stat().st_size != item['bytes'] or digest(pack) != item['sha256']:
            raise ValueError('Transfer ZIP is damaged: ' + str(pack))
        dest = project.parent if name == 'manny' else plugin
        manifest, existing = preflight(pack, dest, a.engine)
        if manifest['component'] != name or manifest['destination'] != ('project' if name == 'manny' else 'plugin'):
            raise ValueError('Catalog/pack component mismatch')
        prepared.append((pack, dest))
        print(json.dumps(dict(component=name, files=len(manifest['files']), matching_existing=existing)), flush=True)
    if a.plan:
        print('Preflight passed; no files changed.'); return
    if shutil.disk_usage(project.parent).free < max(items[x]['payload_bytes'] for x in selected) * 2 + sum(items[x]['payload_bytes'] for x in selected):
        raise OSError('Insufficient free space for payload and verified staging')
    for pack, dest in prepared:
        print(json.dumps(install_pack(pack, dest, a.engine)), flush=True)
    if not a.no_enable:
        data = json.loads(project.read_text(encoding='utf-8-sig'))
        entries = data.setdefault('Plugins', []); changed = False
        names = ['CubelessBodyMocap', 'PythonScriptPlugin']
        if 'manny' in selected:
            names += ['ControlRig', 'ControlRigSpline']
        for name in names:
            found = next((x for x in entries if x.get('Name') == name), None)
            if found is None:
                entries.append(dict(Name=name, Enabled=True)); changed = True
            elif found.get('Enabled') is not True:
                found['Enabled'] = True; changed = True
        if changed:
            backup = project.with_name(project.name + '.cbm-backup-' + uuid.uuid4().hex)
            shutil.copy2(project, backup)
            tmp = project.with_name(project.name + '.cbm-install-' + uuid.uuid4().hex)
            tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            tmp.replace(project); print('Project flags updated; exact backup:', backup)
    print('Installed. Original Markerless plugin was not accessed. Python/Torch are not installed by this transfer.')
    if not a.without_hands:
        print('Prepare HandFlow Python locally on this PC before running hands. See TRANSFER.md / README section 6.')


if __name__ == '__main__':
    main()
