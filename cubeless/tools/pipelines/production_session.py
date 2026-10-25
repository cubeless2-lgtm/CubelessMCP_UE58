"""Small shared CLI/session support. Domain decisions remain in each pipeline."""
import argparse
import json
from pathlib import Path
import re
import time
import uuid
from .recorded_calls import RecordedCalls
from .result_contracts import require

NATIVE = 'CubelessMCPUE58Editor.CubelessNativeToolset'


def arguments(description, argv=None):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--endpoint', required=True)
    parser.add_argument('--project', required=True, type=Path)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--apply', action='store_true', help='Dispatch writes; otherwise validate/preview only')
    return parser.parse_args(argv)


def load(path):
    def unique(pairs):
        result={}
        for key,value in pairs:
            require(key not in result,'Duplicate input key: '+key);result[key]=value
        return result
    def finite(value):raise ValueError('Non-finite JSON is not supported: '+value)
    with Path(path).open(encoding='utf-8-sig') as f:
        return json.load(f,object_pairs_hook=unique,parse_constant=finite)


def package(value):
    require(type(value) is str and re.fullmatch(r'/Game/(?:\w+/)*\w+', value),
            'Use an exact /Game package path without object suffix or traversal')
    return value


class Session:
    def __init__(self, args, domain, toolsets):
        self.args, self.config = args, load(args.input)
        require(type(self.config) is dict, 'Input must be an object')
        self.output = args.output.resolve()
        require(not self.output.exists() and not self.output.with_suffix(self.output.suffix + '.partial').exists()
                and not self.output.with_name(self.output.name + '.evidence').exists(),
                'Fresh output required. Inspect old receipts; never blindly resume/replay')
        self.report = dict(schema='cubeless_production_pipeline_v1', domain=domain, status='running',
            operation_id=uuid.uuid4().hex,
            stage='initialize', started_ns=time.time_ns(), input=self.config, project=str(args.project.resolve()),
            apply=args.apply, stages=[], changes=[], outcome_unknown=False, automatic_retries=0,
            actual_change_scope='See per-stage native receipts; no atomic rollback',
            next_action='Inspect actual assets and operation IDs before any new mutation')
        self.calls = RecordedCalls(args.endpoint, self.output, self.report, dict(native=NATIVE, **toolsets), storage_v2=True)

    def __enter__(self):
        try:
            self.calls.discover()
            diag = self.calls.call('target_project', 'native', 'GetEditorDiagnostics')
            require(Path(diag['project_file']).resolve() == self.args.project.resolve(), 'Wrong editor project; no writes dispatched')
            require(diag['game_thread'] and not diag['pie_or_sie_active'] and diag['engine_version'].startswith('5.8.3-'),
                    'Idle UE5.8.3 Editor game thread required')
            self.report['diagnostics_before'] = diag
            return self
        except Exception as error:
            self.__exit__(type(error), error, error.__traceback__)
            raise

    def __exit__(self, kind, error, traceback):
        self.report.update(status='failed' if error else ('passed' if self.args.apply else 'previewed'),
                           failed_stage=self.report['stage'] if error else '', error=str(error) if error else '')
        self.calls.finish()
        return False

    def call(self, label, group, method, values=None, mutation=False, **options):
        require(not mutation or self.args.apply, 'Preview cannot dispatch a mutation')
        result = self.calls.call(label, group, method, values, mutation=mutation, **options)
        if isinstance(result, dict) and result.get('success') is False:
            raise RuntimeError(f'{label}: {result.get("error_code", "")} {result.get("error", "")}')
        return result

    def schema(self, group, method, values):
        from jsonschema import Draft202012Validator
        from .result_contracts import canonical
        tools=[t for t in self.calls.schemas[group]['tools'] if t['name'].rsplit('.',1)[-1]==method]
        require(len(tools)==1, 'Missing unique registered method: '+method)
        schema=tools[0]['inputSchema']; props=schema.get('properties',{})
        keys={canonical(k):k for k in props}
        require(all(canonical(k) in keys for k in values), 'Unsupported arguments for '+method)
        mapped={keys[canonical(k)]:v for k,v in values.items()}
        require(len(mapped)==len(values),'Duplicate normalized inputs for '+method)
        require(set(schema.get('required',[]))<=set(mapped), 'Missing required inputs for '+method)
        Draft202012Validator(schema).validate(mapped)
        return schema

    def checkpoint(self, **fields):
        self.report.update(fields)
        self.calls.persist()
