"""Run a project-local production pipeline from any working directory."""
import importlib
import sys


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ('blueprint', 'niagara', 'pcg'):
        raise SystemExit('Usage: python <project>/cubeless/tools/run_pipeline.py {blueprint|niagara|pcg} --endpoint URL --input project-relative.json [--project name.uproject] [--output result.json] [--apply]')
    module = importlib.import_module('pipelines.production_' + sys.argv[1])
    module.main(sys.argv[2:])


if __name__ == '__main__':
    main()
