"""Explicit instance-color operations; transport/registry independent."""
import hashlib
import unreal
from . import editor_data


def invoke(name, component_path, lod_index=0, *, colors_json="", execute=False,
           allow_replace=False, save=False, allow_save_dirty_package=False,
           start_index=0, count=256, include_positions=False):
    summary = {"component_path": component_path, "lod_index": lod_index,
               "execute": execute, "allow_replace_or_remove": allow_replace,
               "save": save, "allow_save_dirty_package": allow_save_dirty_package}
    try:
        editor_data._validate_path(component_path)
        if type(lod_index) is not int or not 0 <= lod_index < 64:
            raise ValueError("lod_index must be an integer0..63")
        if any(type(v) is not bool for v in (execute, allow_replace, save,
                    allow_save_dirty_package, include_positions)):
            raise ValueError("Options must be booleans")
        if name == "set_instance_vertex_colors":
            if not isinstance(colors_json, str) or len(colors_json) > 32 * 1024 * 1024:
                raise ValueError("colors_json must be RGBA JSON text bounded to32MiB")
            summary.update(colors_json_length=len(colors_json),
                colors_sha256=hashlib.sha256(colors_json.encode("utf-8")).hexdigest())
        if name == "inspect_instance_vertex_colors":
            if type(start_index) is not int or not 0 <= start_index <= 1048576 or type(count) is not int or not 0 <= count <= 4096:
                raise ValueError("start_index0..1048576 and count0..4096 required")
        elif name not in ("set_instance_vertex_colors", "clear_instance_vertex_colors"):
            raise ValueError("Unknown instance color operation")
    except Exception as error:
        return editor_data._failure(name, error)

    def dispatch():
        try:
            component = editor_data._object(component_path, loaded_only=True)
            function = getattr(unreal.CubelessInstanceVertexColorLibrary, name)
        except Exception as error:
            return editor_data._failure(name, error)
        try:
            if name == "inspect_instance_vertex_colors":
                return function(component, lod_index, start_index, count, include_positions)
            if name == "set_instance_vertex_colors":
                return function(component, colors_json, lod_index, execute,
                                allow_replace, save, allow_save_dirty_package)
            return function(component, lod_index, execute, allow_replace, save,
                            allow_save_dirty_package)
        except Exception as error:
            return editor_data._failure(name, error, dispatched=True, mutation=execute)

    return editor_data._writer(name, summary, dispatch) if execute else dispatch()
