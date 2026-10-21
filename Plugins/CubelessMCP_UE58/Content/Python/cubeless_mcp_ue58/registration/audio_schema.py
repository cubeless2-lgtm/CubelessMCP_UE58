"""Exact four original Audio schema and finite binding gate, separate from operations."""
from cubeless_mcp_ue58.compatibility.ue58 import audio as api


def binding_status():
    methods = api.binding_status()
    return {"ready": bool(methods) and all(value is True for value in methods.values()),
        "methods": methods, "native_libraries": dict(api.BINDING_LIBRARIES),
        "behavior_verified": False,
        "verification_scope": "Required Registry metadata and three exact native method bindings only; authoring, save, no-op and graph behavior require actual evidence"}


def validate_schema(schema):
    rows = schema.get("tools", [])
    tools = {row["name"].rsplit(".", 1)[-1]: row["inputSchema"] for row in rows}
    expected = {
        "list_audio_assets": (set(), {"directory": ("string", "/Game"), "recursive": ("boolean", True),
            "max_results": ("integer", 200), "offset": ("integer", 0)}),
        "inspect_audio_asset": ({"asset_path"}, {"asset_path": ("string",)}),
        "create_sound_cue_from_wave": ({"name", "sound_wave_path"}, {"name": ("string",),
            "sound_wave_path": ("string",), "package_path": ("string", "/Game/Audio/SoundCues")}),
        "create_basic_metasound_source": ({"name"}, {"name": ("string",),
            "package_path": ("string", "/Game/Audio/MetaSounds"), "oscillator": ("string", "sine"),
            "frequency": ("number", 440.0), "output_format": ("string", "mono")}),
    }
    if len(rows) != 4 or set(tools) != set(expected):
        raise RuntimeError("Audio must retain exactly four distinct original names")
    for name, (required, definition) in expected.items():
        inputs = tools[name]
        if set(inputs.get("required", [])) != required or set(inputs.get("properties", {})) != set(definition):
            raise RuntimeError(name + " changed original required inputs or exposed extra arguments")
        for key, contract in definition.items():
            actual = inputs["properties"][key]
            if actual.get("type") != contract[0]:
                raise RuntimeError(name + "." + key + " has an incompatible type")
            if len(contract) == 2:
                default = actual.get("default")
                allowed_types = {"string": (str,), "boolean": (bool,), "integer": (int,), "number": (int, float)}[contract[0]]
                if type(default) not in allowed_types or default != contract[1]:
                    raise RuntimeError(name + "." + key + " changed the original typed default")
            if len(contract) == 1 and "default" in actual:
                raise RuntimeError(name + "." + key + " unexpectedly acquired a default")
