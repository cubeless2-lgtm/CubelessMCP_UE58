"""Comment collection operations, reusing the shared mutation journal."""
from . import editor_data


def run(asset_path, action, text="", x=0, y=0, width=400, height=100, execute=False, save=False):
    if type(execute) is not bool or type(save) is not bool:
        return editor_data._failure("material_comments", "execute/save must be booleans")
    if not isinstance(text, str) or len(text.encode("utf-16-le")) // 2 > 16384:
        return editor_data._failure("material_comments", "Comment text must be at most16384 UTF-16 units")
    if any(type(v) is not int or not -(2**31) <= v < 2**31 for v in (x, y, width, height)):
        return editor_data._failure("material_comments", "Coordinates and dimensions must be int32 values")
    return editor_data.invoke("material_comments", asset_path,
                              (action, text, x, y, width, height, execute, save),
                              mutation=execute, loaded_only=False)
