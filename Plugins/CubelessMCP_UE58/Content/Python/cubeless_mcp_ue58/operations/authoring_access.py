"""Authoring-access contracts; no MCP/server transport in the work logic."""
import json
import unreal
from . import editor_data
_preview_widgets = globals().get('_preview_widgets', {})

def create_preview_widget(owner_path, class_path, name, execute):
    def dispatch():
        dispatched = False
        try:
            if type(execute) is not bool or type(name) is not str:
                raise ValueError('Explicit boolean execute and string name required')
            if not name or name.casefold() == 'none' or len(name.encode('utf-16-le')) // 2 > 128 or any(ord(c) < 32 for c in name):
                raise ValueError('Explicit non-None widget name of 1..128 UTF-16 units without control characters required')
            if execute and len(_preview_widgets) >= 64:
                raise ValueError('Release an existing preview lease before creating more than64 widgets')
            owner=editor_data._object(owner_path,loaded_only=True)
            cls=editor_data._object(class_path,loaded_only=True)
            function = unreal.CubelessAuthoringAccessLibrary.create_editor_preview_widget
            dispatched = True
            widget, raw = function(owner,cls,name,execute)
            if widget is not None:
                _preview_widgets[widget.get_path_name()]=widget
            return raw
        except Exception as error:
            return editor_data._failure('create_editor_preview_widget',error,dispatched=dispatched,mutation=execute)
    return editor_data._writer('create_editor_preview_widget',{'owner_path':owner_path,'class_path':class_path,'name':name},dispatch) if execute else dispatch()

def release_preview_widget(path, execute):
    if type(execute) is not bool or path not in _preview_widgets:
        return editor_data._failure('release_editor_preview_widget',ValueError('Known retained preview widget path and boolean execute required'))
    if execute:
        del _preview_widgets[path]
    return json.dumps({'success':True,'retention_released':execute,'object_path':path,'scope':'Only releases this adapter reference; does not destroy a widget held elsewhere or remove it from a user panel.'})

def global_inspection(name, args=()):
    try:
        return getattr(unreal.CubelessAuthoringAccessLibrary, name)(*args)
    except Exception as error:
        return editor_data._failure(name,error)


def rigvm_unit_catalog(name_filter, include_deprecated, offset, limit):
    try:
        if type(name_filter) is not str or type(include_deprecated) is not bool or type(offset) is not int or type(limit) is not int:
            raise ValueError('Expected string filter, boolean include_deprecated and integer offset/limit')
        return unreal.CubelessAuthoringAccessLibrary.inspect_rig_vm_unit_catalog('' if name_filter == '*' else name_filter, include_deprecated, offset, limit)
    except Exception as error:
        return editor_data._failure('inspect_rigvm_unit_catalog', error)


def invoke(name, path, args=(), *, mutation=False, loaded_only=False, expected_classes=()):
    def dispatch():
        try:
            obj = editor_data._object(path, loaded_only=loaded_only)
            if expected_classes and not any(isinstance(obj, cls) for cls in (
                    getattr(unreal, key, None) for key in expected_classes) if cls is not None):
                raise ValueError('Expected asset type: ' + ', '.join(expected_classes))
            function = getattr(unreal.CubelessAuthoringAccessLibrary, name)
        except Exception as error:
            return editor_data._failure(name, error)
        try:
            return function(obj, *args)
        except Exception as error:
            failure = json.loads(editor_data._failure(name, error, dispatched=True, mutation=mutation))
            if mutation:
                failure['engine_dispatch_outcome_known'] = False
                failure['compile_state_known'] = False
                failure['save_state_known'] = False
                failure['save_attempted'] = None
                failure['saved'] = None
            return json.dumps(failure)
    return (editor_data._writer(name, {'path': path, 'args': list(args)}, dispatch)
            if mutation else dispatch())
