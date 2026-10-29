"""Explicit editor data operations, independent of server transport/registry."""
import json
import math
import re
import unreal
from cubeless_mcp_ue58.operations import state


def _failure(operation, error, dispatched=False, mutation=False):
    return json.dumps({"success": False, "operation": operation,
        "stage": "native_dispatch" if dispatched else "preflight", "error": str(error),
        "mutation_attempted": bool(dispatched and mutation),
        "changed": None if dispatched and mutation else False,
        "change_state_known": not (dispatched and mutation),
        "compile_attempted": False, "save_attempted": False, "saved": False,
        "automatic_retry": False, "atomic_rollback": False,
        "retry_risk": "inspect_actual_target" if dispatched and mutation else "none_observed"})


def _validate_path(path):
    if not isinstance(path, str) or not path.startswith('/') or len(path) > 2048 or any(ord(c) < 32 for c in path):
        raise ValueError("Explicit bounded Unreal object/package path required")
    if any(len(part.encode('utf-16-le')) // 2 >= 1024 for part in re.split(r'[./:]', path)):
        raise ValueError('Path name segment exceeds installed FName NAME_SIZE before engine loading')


def _object(path, loaded_only=False):
    _validate_path(path)
    object_path = path
    if ":" not in path and "." not in path.rsplit("/", 1)[-1]:
        object_path = path + "." + path.rsplit("/", 1)[-1]
    if not path.startswith(("/Game/", "/Engine/", "/Script/")):
        # Plugin-mounted assets are accepted for inspection when already loaded.
        obj = unreal.find_object(None, object_path)
        if obj and unreal.SystemLibrary.is_valid(obj):
            return obj
        raise ValueError("Plugin-mounted inspection targets must already be loaded")
    # A loaded package is also findable by package path; resolve its asset, not UPackage.
    obj = unreal.find_object(None, object_path)
    if obj is None and not loaded_only:
        obj = unreal.load_object(None, path) if ":" in path or path.startswith("/Script/") else unreal.load_asset(path)
    if obj is None or not unreal.SystemLibrary.is_valid(obj):
        raise ValueError("Exact live object not found; no actor name/label/selection fallback")
    return obj


def _writer(name, arguments, dispatch):
    """Reuse the shared mutation slot/journal, including callbacks into other tools."""
    def action(record):
        record['stage'] = 'native_preflight'
        record['data']['requested_arguments'] = arguments
        state.checkpoint(record)
        record['data']['dispatch_started'] = True
        record['data']['native_change_state_known'] = False
        record['changes'].append({'stage': 'native_dispatch', 'changed': None,
                                  'change_state_known': False})
        record['retry']['duplicate_risk'] = 'inspect_actual_target'
        raw = dispatch()
        record['data']['dispatch_returned'] = True
        record['data']['raw_native_payload'] = str(raw)[:65536]
        native = json.loads(raw)
        record['data']['native_result'] = native
        if not isinstance(native, dict) or any(type(native.get(key)) is not bool for key in (
                'success', 'mutation_attempted', 'change_state_known',
                'compile_attempted', 'save_attempted', 'saved')):
            raise ValueError('Malformed native result; inspect actual target before another mutation')
        record['data']['native_result_validated'] = True
        record['data']['native_change_state_known'] = native['change_state_known']
        record['changes'].clear()
        record.update(success=native['success'], stage=native.get('stage', 'native_returned'),
                      error=native.get('error', ''),
                      error_code='' if native['success'] else native.get('error_code', 'native_operation_failed'))
        record['data']['native_expected_error_code'] = record['error_code']
        record['compile'].update(attempted=native.get('compile_attempted', False))
        record['save'].update(attempted=native.get('save_attempted', False), saved=native.get('saved', False))
        if native.get('mutation_attempted'):
            record['changes'].append({'stage': native.get('stage'), 'changed': native.get('changed'),
                                      'change_state_known': native.get('change_state_known')})
        record['retry']['duplicate_risk'] = ('inspect_actual_target' if not native.get('change_state_known', False)
            else 'mutation_attempted' if native.get('mutation_attempted') else 'none_observed')
        state.checkpoint(record)
    report = state.mutate(name, arguments, None, action)
    data = report.get('data', {})
    native = data.get('native_result')
    if not data.get('native_result_validated'):
        entered = bool(data.get('dispatch_started'))
        result = json.loads(_failure(name, report.get('error', 'Mutation admission/journal failed'),
                                     dispatched=entered, mutation=entered))
        result['error_code'] = report.get('error_code', 'mutation_admission_failed')
        if entered:
            result['raw_native_payload'] = data.get('raw_native_payload')
    else:
        result = dict(native)
        if report.get('error_code', '') != data.get('native_expected_error_code', '') or (native['success'] and not report.get('success')):
            result.update(success=False, error=report['error'], error_code=report['error_code'],
                          change_state_known=False, retry_risk='inspect_actual_target_and_journal',
                          native_result_before_journal_failure=native)
    result['operation_id'] = report['operation_id']
    result['automatic_retry'] = False
    return json.dumps(result)


def invoke(name, path=None, args=(), *, mutation=False, loaded_only=False):
    """Native compatibility layer owns engine context and post-callback validation."""
    def dispatch():
        try:
            values = (() if path is None else (_object(path, loaded_only),)) + tuple(args)
            function = getattr(unreal.CubelessEditorDataLibrary, name)
        except Exception as error:
            return _failure(name, error)
        try:
            return function(*values)
        except Exception as error:
            return _failure(name, error, dispatched=True, mutation=mutation)
    return _writer(name, {'path': path, 'args': list(args)}, dispatch) if mutation else dispatch()


def spawn_editor_actor_deferred(class_path, actor_name, transform_json, execute=False):
    name = "spawn_editor_actor_deferred"
    if execute:
        return _writer(name, {'class_path': class_path, 'actor_name': actor_name, 'transform_json': transform_json},
                       lambda: _spawn(class_path, actor_name, transform_json, execute))
    return _spawn(class_path, actor_name, transform_json, execute)


def _spawn(class_path, actor_name, transform_json, execute):
    name = "spawn_editor_actor_deferred"
    try:
        _validate_path(class_path)
        if not isinstance(class_path, str) or not class_path.startswith(("/Game/", "/Script/")):
            raise ValueError("Explicit /Game generated-class or /Script native Actor class path required")
        if not isinstance(transform_json, str) or len(transform_json) > 65536:
            raise ValueError('Transform JSON text must be bounded to65536 characters')
        def unique_keys(pairs):
            value = {}
            for key, item in pairs:
                if key in value:
                    raise ValueError('Duplicate transform JSON key: ' + key)
                value[key] = item
            return value
        data = json.loads(transform_json, object_pairs_hook=unique_keys)
        if not isinstance(data, dict) or set(data) - {"location", "rotation", "scale"}:
            raise ValueError("Transform object accepts only location[x,y,z], rotation[pitch,yaw,roll], scale[x,y,z]")
        def vector(key, default):
            value = data.get(key, default)
            if not isinstance(value, list) or len(value) != 3 or any(type(x) not in (int, float) or not math.isfinite(x) or abs(x) > 1e9 for x in value):
                raise ValueError("Finite 3-number transform arrays within magnitude1e9 required")
            return value
        position=vector("location", [0, 0, 0]);rotation=vector("rotation", [0, 0, 0]);scale=vector("scale", [1, 1, 1])
        cls=unreal.load_class(None, class_path)
        if cls is None:
            raise ValueError("Actor class could not be loaded")
        transform=unreal.Transform(location=unreal.Vector(*position), rotation=unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), scale=unreal.Vector(*scale))
        function=unreal.CubelessEditorDataLibrary.spawn_editor_actor_deferred
    except Exception as error:
        return _failure(name, error)
    try:
        return function(cls, actor_name, transform, execute)
    except Exception as error:
        return _failure(name, error, dispatched=True, mutation=bool(execute))
