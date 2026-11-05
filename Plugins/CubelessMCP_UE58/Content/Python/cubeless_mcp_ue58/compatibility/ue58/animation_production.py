"""UE58 adapter: native domain work, guarded resolution, no MCP transport."""
import json,uuid
import unreal
from cubeless_mcp_ue58.operations import editor_data

def _chooser_contexts(text, reference_path, legacy_json):
    if reference_path or legacy_json:
        raise ValueError('contexts_json cannot be combined with legacy context inputs')
    if not isinstance(text, str) or len(text) > 262144:
        raise ValueError('contexts_json must be a JSON string of at most 262144 characters')
    slots = json.loads(text)
    if not isinstance(slots, list) or not 1 <= len(slots) <= 64:
        raise ValueError('contexts_json requires 1 to 64 ordered slots')
    paths, values = [], []
    for index, slot in enumerate(slots):
        if not isinstance(slot, dict) or len(slot) != 1:
            raise ValueError(f'Context {index} requires exactly one object_path or struct')
        if 'object_path' in slot:
            editor_data._validate_path(slot['object_path'])
            paths.append(slot['object_path'])
            values.append('')
        elif isinstance(slot.get('struct'), dict):
            value = json.dumps(slot['struct'], ensure_ascii=False, allow_nan=False)
            if len(value.encode('utf-16-le')) // 2 > 65536:
                raise ValueError(f'Context {index} struct exceeds 65536 UTF-16 code units')
            paths.append('')
            values.append(value)
        else:
            raise ValueError(f'Context {index} requires an object_path or a struct object')
    if sum(len(value.encode('utf-16-le')) // 2 for value in values) > 262144:
        raise ValueError('Aggregate struct JSON limit exceeded')
    return paths, values


def invoke(kind, asset_path='', reference_path='', action='', arguments=None, execute=False, save=False, allow_dirty=False, allow_remove=False, job_id=''):
    loaded=False;lookup=False;native_entered=False
    def resolve(path):
        nonlocal loaded,lookup
        editor_data._validate_path(path)
        object_path=path if '.' in path.rsplit('/',1)[-1] else path+'.'+path.rsplit('/',1)[-1]
        token=uuid.uuid4().hex;attempt=False
        try:
            begin=json.loads(unreal.CubelessAnimationEditingLibrary.begin_animation_asset_acquisition(object_path.split(':',1)[0],token))
            if not begin.get('success'):raise RuntimeError('Resolution admission rejected: '+str(begin))
            lookup=True
            value=unreal.find_object(None,object_path)
            if value is None:
                loaded=True;attempt=True;value=editor_data._object(path)
            return value
        finally:
            end=json.loads(unreal.CubelessAnimationEditingLibrary.end_animation_asset_acquisition(token,attempt))
            if not end.get('success'):raise RuntimeError('Resolution cleanup failed: '+str(end))
    try:
        args = arguments or {}
        contexts = None
        if kind == 'evaluate' and args.get('contexts_json') is not None:
            paths, values = _chooser_contexts(args['contexts_json'], reference_path, args.get('context_json'))
            contexts = ([resolve(path) if path else None for path in paths], values)
        reference=resolve(reference_path) if reference_path else None
        lib=unreal.CubelessAnimationProductionLibrary
        if kind=='create':
            native_entered=True
            raw=lib.create_animation_production_asset(action,asset_path,reference,execute,save)
        else:
            target=resolve(asset_path)
            encoded_args=json.dumps(arguments['payload'] if kind=='state_edit' else (arguments or {}),allow_nan=False)
            native_entered=True
            if kind=='inspect':raw=lib.inspect_animation_production_asset(target)
            elif kind=='index':raw=lib.animation_production_index(target,job_id,execute)
            elif kind == 'evaluate':
                if contexts is None:
                    raw = lib.evaluate_animation_production_chooser(target, reference, args.get('context_json', ''))
                else:
                    raw = lib.evaluate_animation_production_chooser_contexts(target, *contexts)
            elif kind=='state_inspect':raw=lib.inspect_animation_state_graph(target,arguments['graph'])
            elif kind=='state_edit':raw=lib.edit_animation_state_graph(target,action,arguments['graph'],encoded_args,execute,arguments.get('compile',False),save,allow_dirty,allow_remove)
            elif kind=='layer':raw=lib.create_animation_production_layer(target,arguments['name'],execute,arguments.get('compile',False),save,allow_dirty)
            elif kind=='chooser_bind':raw=lib.bind_animation_production_chooser(target,arguments['graph'],arguments['node_id'],arguments['chooser'],arguments['function'],execute,arguments.get('compile',False),save,allow_dirty,allow_remove)
            elif kind=='bind':raw=lib.bind_animation_production_node(target,action,arguments['graph'],arguments['node_id'],arguments['source'],execute,arguments.get('compile',False),save,allow_dirty,allow_remove)
            else:raw=lib.edit_animation_production_asset(target,action,encoded_args,execute,save,allow_dirty,allow_remove)
        result=json.loads(raw)
    except Exception as error:
        result=json.loads(editor_data._failure(action or kind,error,dispatched=native_entered,mutation=execute))
        if native_entered:
            result.update(change_state_known=False,changed=None,save_attempted=None,saved=None,save_state_known=False,retry_risk='inspect_actual_target_and_disk')
        elif lookup:
            result.update(change_state_known=False,changed=None,retry_risk='inspect_actual_target_after_resolution')
    result['asset_load_attempted']=loaded
    result['native_observation_scope']='after_explicit_asset_resolution'
    if loaded:
        result['native_change_state_known']=result.get('change_state_known')
        result.update(change_state_known=False,changed=True if result.get('changed') is True else None,retry_risk='inspect_actual_target_after_asset_load')
    return json.dumps(result)
