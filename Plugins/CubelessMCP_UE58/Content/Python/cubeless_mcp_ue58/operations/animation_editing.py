"""Dedicated animation edits sharing one guarded work implementation."""
import json
from . import editor_data
from cubeless_mcp_ue58.compatibility.ue58 import animation_editing as api

ACTIONS = frozenset((
    'add_float_curve', 'remove_float_curve', 'set_float_curve_keys',
    'add_notify_track', 'remove_notify_track', 'add_notify', 'add_notify_state',
    'remove_notify', 'set_notify_timing', 'add_sync_marker',
    'remove_sync_markers', 'rename_sync_marker', 'add_montage_section',
    'remove_montage_section', 'set_montage_section_next', 'set_montage_section_time',
    'add_montage_slot'))


def edit(action, asset_path, arguments, execute=False, save=False,
         allow_save_dirty_package=False, allow_remove=None):
    try:
        if action not in ACTIONS:
            raise ValueError('Unknown fixed animation action')
        editor_data._validate_path(asset_path)
        if not asset_path.startswith('/Game/'):
            raise ValueError('Only explicit project animation asset paths can be edited')
        if any(type(v) is not bool for v in (execute, save, allow_save_dirty_package)):
            raise ValueError('Execution and save options must be booleans')
        if allow_remove is not None and type(allow_remove) is not bool:
            raise ValueError('allow_remove must be boolean')
        if action in ('add_notify', 'add_notify_state'):
            class_path = arguments.get('notify_class')
            editor_data._validate_path(class_path)
            class_name = class_path.rsplit('.', 1)[-1]
            if class_path.count('.') != 1 or not class_name or '/' in class_name or len(class_name.encode('utf-16-le')) // 2 >= 1024 or any(c in class_path for c in (':', "'", '"', '\\')):
                raise ValueError('notify_class must be a canonical top-level /Package.Class path without subobject/export-text syntax')
        if execute and action.startswith('remove_') and allow_remove is not True:
            raise ValueError('Destructive removal requires execute=true and allow_remove=true')
        for key, value in arguments.items():
            if key != 'keys_json' and isinstance(value, str) and any(ord(c) < 32 for c in value):
                raise ValueError('Control characters are not allowed in animation identifiers')
        if 'keys_json' in arguments:
            source = arguments.pop('keys_json')
            if type(source) is not str or len(source) > 262144:
                raise ValueError('keys_json must be bounded JSON [[time,value],...]')
            arguments['keys'] = json.loads(source, parse_constant=lambda x: (_ for _ in ()).throw(ValueError('Nonfinite number')))
        encoded = json.dumps(arguments, ensure_ascii=False, allow_nan=False)
        if len(encoded) > 262144:
            raise ValueError('Animation argument budget exceeded')
    except Exception as error:
        return editor_data._failure(action, error)
    dispatch = lambda: api.edit(asset_path, action, encoded, execute, save, allow_save_dirty_package)
    return editor_data._writer('animation_' + action,
        {'asset_path': asset_path, 'arguments': arguments, 'save': save,
         'allow_save_dirty_package': allow_save_dirty_package}, dispatch) if execute else dispatch()
