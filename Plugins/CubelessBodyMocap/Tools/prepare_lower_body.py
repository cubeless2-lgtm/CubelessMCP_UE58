"""Prepare an isolated, CPU-only lower-body environment without changing hand runtimes.

Run with standalone Python 3.12 on Windows. No model weights are required.
"""
import json
from pathlib import Path
import subprocess
import sys
import venv


def main():
    if sys.version_info[:2] != (3, 12) or sys.prefix != sys.base_prefix:
        raise SystemExit('Use standalone Python 3.12, not a hand-model virtual environment.')
    root = Path(__file__).resolve().parents[1] / 'ThirdParty' / 'LowerBody'
    environment = root / 'venv'
    python = environment / 'Scripts' / 'python.exe'
    if not environment.exists():
        venv.EnvBuilder(with_pip=True, system_site_packages=False).create(environment)
    if not python.is_file():
        raise SystemExit('Existing LowerBody environment is incomplete; inspect it before setup.')
    subprocess.run([str(python), '-I', '-m', 'pip', 'install', '--disable-pip-version-check',
                    '-r', str(Path(__file__).with_name('requirements-lower-body.txt'))], check=True)
    check = 'import json,sys,numpy,scipy,psutil; print(json.dumps(dict(python=sys.version, prefix=sys.prefix, base_prefix=sys.base_prefix, numpy=numpy.__version__, scipy=scipy.__version__, psutil=psutil.__version__)))'
    result = json.loads(subprocess.check_output([str(python), '-I', '-c', check], text=True))
    result.update(python_executable=str(python), hand_runtime_dependency=False, models_required=False)
    (root / 'environment.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
