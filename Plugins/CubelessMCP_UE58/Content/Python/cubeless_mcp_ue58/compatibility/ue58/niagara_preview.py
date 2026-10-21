"""Resolve only the running UE58 project's public Paths API context."""

from pathlib import Path

import unreal


def current_project_file():
    """Return an existing absolute .uproject; never infer a sibling or CWD."""
    paths = getattr(unreal, "Paths", None)
    names = ("is_project_file_path_set", "get_project_file_path", "convert_relative_path_to_full")
    if paths is None or not all(callable(getattr(paths, name, None)) for name in names):
        raise RuntimeError("UE58 public Paths project-file bindings are unavailable")
    if paths.is_project_file_path_set() is not True:
        raise RuntimeError("The running editor has no project file path")
    value = paths.get_project_file_path()
    if not isinstance(value, str) or not value.strip():
        raise RuntimeError("UE58 Paths returned an empty project file path")
    selected = Path(paths.convert_relative_path_to_full(value))
    if not selected.is_absolute() or selected.suffix.casefold() != ".uproject":
        raise RuntimeError("UE58 Paths did not return an absolute .uproject")
    selected = selected.resolve(strict=True)
    if not selected.is_file():
        raise RuntimeError("The running project file is not a regular file")
    return str(selected)
