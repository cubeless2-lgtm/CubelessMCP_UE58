"""Authoring-access contracts; no MCP/server transport in the work logic."""
import json
import unreal
from . import editor_data


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
