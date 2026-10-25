"""Locate this source bundle and its immediate host project, independently of cwd."""
import json
from pathlib import Path
import stat


def bundle_root():
    raw = Path(__file__).absolute().parents[2]
    for path in (raw, *raw.parents, raw / 'tools', raw / 'tools/pipelines'):
        if path.is_symlink() or getattr(path.lstat(), 'st_file_attributes', 0) & 0x400:
            raise ValueError('Linked/junction bundle layout is not supported; install real folders: ' + str(path))
    root = raw.resolve(strict=True)
    marker = root / 'distribution.json'
    if not marker.is_file():
        raise ValueError('Missing cubeless/distribution.json; use the project-local source bundle, not an installed Python package')
    identity = json.loads(marker.read_text(encoding='utf-8-sig'))
    if not isinstance(identity, dict) or identity.get('format') != 'cubeless_source_bundle_v1' or identity.get('plugin') != 'CubelessMCP_UE58':
        raise ValueError('Unrecognized Cubeless source bundle: ' + str(marker))
    return root


def project_file(value=None):
    root = bundle_root().parent
    if value is None:
        choices = sorted(p for p in root.iterdir() if p.is_file() and p.suffix.casefold() == '.uproject')
        if len(choices) != 1:
            raise ValueError(f'Expected one .uproject directly above cubeless, found {len(choices)}; place the bundle under a project or specify --project (multiple candidates)')
        selected = choices[0]
    else:
        selected = Path(value).expanduser()
        if not selected.is_absolute():
            selected = root / selected
    if selected.suffix.casefold() != '.uproject' or not selected.is_file():
        raise ValueError('Existing .uproject file required: ' + str(selected))
    if selected.is_symlink() or getattr(selected.lstat(), 'st_file_attributes', 0) & 0x400:
        raise ValueError('Linked project file is not supported: ' + str(selected))
    selected = selected.resolve(strict=True)
    if selected.parent != root:
        raise ValueError('--project must be in this cubeless parent project; remote cross-project control is not supported')
    if not isinstance(json.loads(selected.read_text(encoding='utf-8-sig')), dict):
        raise ValueError('Invalid .uproject JSON object')
    return selected


def project_path(value, project):
    path = Path(value).expanduser()
    return (path if path.is_absolute() else project.parent / path).resolve()


def verification_output(value, project):
    """Admit a fresh receipt path without following a linked verification root."""
    cubeless = bundle_root()
    allowed = cubeless / 'records/verification'
    raw = Path(value).expanduser()
    if not raw.is_absolute():
        raw = project.parent / raw
    for candidate in (allowed, raw, raw.with_suffix(raw.suffix + '.partial'),
                      raw.with_name(raw.name + '.evidence')):
        for path in (candidate, *candidate.parents):
            try:
                observed = path.lstat()
            except FileNotFoundError:
                continue  # A normal fresh output directory is permitted.
            if stat.S_ISLNK(observed.st_mode) or getattr(observed, 'st_file_attributes', 0) & 0x400:
                raise ValueError('Linked/junction verification output refused: ' + str(path))
    resolved_root = allowed.resolve()
    resolved = raw.resolve()
    if (not resolved_root.is_relative_to(cubeless)
            or not resolved_root.is_relative_to(project.parent)
            or not resolved.is_relative_to(resolved_root)):
        raise ValueError('Verification output must stay inside this project cubeless/records/verification')
    return resolved


def editor_project_matches(observed, selected):
    try:
        return Path(observed).resolve(strict=True) == selected.resolve(strict=True)
    except (OSError, ValueError):
        return False
