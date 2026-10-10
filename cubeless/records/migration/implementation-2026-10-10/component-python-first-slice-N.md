# Local SCS public Python slice N

2026-10-10. Source and offline tests only. Official registration, real calls, independent property-value proof and cold reload remain pending until root records them. Two named functions do not complete the legacy component domain.

## Public registration and native boundary

`CubelessComponentTools` adds two functions through the existing official Registry bootstrap. No external MCP server, transport bridge, additional C++ module, gameplay reference, or engine modification is introduced.

| Public function | Required inputs | Defaults | Hidden native call |
| --- | --- | --- | --- |
| `list_blueprint_components` | `blueprint_name: str` | `component_name: str\|None=None`, `max_components: int=10000` | `CubelessComponentLibrary.list_blueprint_components(path, filter, max_components)` |
| `get_component_property` | `blueprint_name, component_name, property_name: str` | `max_components: int=10000`, `max_value_chars: int=65536` | `CubelessComponentLibrary.get_component_property(path, component, property, max_components, max_value_chars)` |

Registry-only code is in `registration/component_toolset.py`; shared operation/result policy is in `operations/component.py`; the two exact reflected calls are in `compatibility/ue58/component.py`. `registration/__init__.py` changes only by importing/appending the N class. M/L operations are unchanged. The existing registration test's class inventory gains N; its product behavior is unchanged.

The filter's omitted/null value becomes the old empty string. This preserves all-components behavior while avoiding the observed official Registry empty-string-default omission. Real generated schema must still verify the optional filter and integer defaults. No reflected array input is introduced.

## Legacy connections and equivalence boundary

- `Python/tools/blueprint_tools.py:175`: public `list_blueprint_components(ctx, blueprint_name, component_name="")` → command of the same name → `UnrealMCPBlueprintCommands.cpp:1434` → `FindBlueprint`, `SimpleConstructionScript->GetAllNodes`, component template/scene/static-mesh reads. Runtime/source inventory IDs: `python:list_blueprint_components` and `native:list_blueprint_components`.
- `blueprint_tools.py:550`: public `get_component_property(ctx, blueprint_name, component_name, property_name)` → same command → `UnrealMCPBlueprintCommands.cpp:2450` → `FindBlueprint`, `GetAllNodes`, `FindFProperty`, `PropertyValueToJsonValue`. IDs: `python:get_component_property`, `native:get_component_property`.
- `Commands/UnrealMCPCommonUtils.cpp:1276–1351`: old FindBlueprint first normalizes/loads exact references and LevelBlueprint aliases, tries `/Game/Blueprints/<name>.<name>`, then registry-wide name candidates and refuses ambiguity. Those additional alias/search branches are **not** silently replaced with equal functionality.

N pure-text adaptation maps a simple name to `/Game/Blueprints/<name>` or passes an absolute package/object path unchanged. It never preloads, looks up engine objects, resolves generated-class/export-text/LevelBlueprint aliases, or searches the registry. Registry-wide unique-name and richer aliases remain incomplete; callers can provide an exact path. Exact `/Engine` and plugin-mounted Blueprint reads are accepted by the native path validator. Reading a tool/editor asset creates no serialized dependency and therefore does not reuse mutation-only dependency rejection rules.

List data retains requested `blueprint_name`, `has_simple_construction_script`, `component_count`, and `components`; per-component variable/GUID/class/template/parent/relative-transform/static-mesh fields retain native legacy shapes. A missing exact filter succeeds with an empty array. Native rows are sorted by local variable name instead of old AllNodes storage order. Get data retains component name/class, requested property name, C++ property type, property value and success; source path, encoding, limits and observations are additive.

The envelope is now `CubelessPythonOperationResult`, shared with the other migrated named tools. Legacy payload is in `data_json` / decoded `data`; callers must use this official structured-result adapter rather than expect the old TCP response envelope. Missing Blueprint/component/property and unsupported encoding return `success=false`, stable `error_code`, stage and retained native diagnostics. Error text/wire shape are intentionally not byte-identical to old connection failures. External-connection/no-response errors disappear with the bridge.

## Limits that remain incomplete

Only reachable nodes under this Blueprint's **local** SCS roots are traversed. Inherited effective overrides, native CDO components and orphan AllNodes entries are not inspected. No claim of full component enumeration is made. Nonactor/no-SCS list is an empty success; property lookup fails when its local component is absent. A malformed tree, duplicate name, traversal budget or unsupported value fails, never a partial-success list.

Supported property values: bool; finite JSON-exact numeric values within ±(2^53−1); string/name/text; byte enums as legacy numbers and FEnum names; FVector/FRotator arrays; object paths; and at most 256 plain scalar/name/string/enum array entries returned in legacy Unreal text encoding. Other structs, maps, sets, delegates, fixed native arrays, nonfinite/inexact numbers and over-budget text remain explicit errors. Scalar-array estimates are conservative and may reject valid long data. This is a read-export bound, independent of Material write-import limits.

Selectors are at most 1023 UTF-16 code units with no controls/unpaired surrogates. Local node budget is 1..10000; property-text budget is 64..65536 UTF-16 units. Booleans/floats are not accepted as integer budgets by the Python operation core; official schema conversion behavior still requires actual boundary probes. Exact component names are case-sensitive; property comparison is case-insensitive like legacy FName lookup.

## Failure, loading and verification semantics

No explicit mutation, compile, save, undo transaction or mutation journal is requested. The returned operation ID identifies this observation, not a replayable durable read journal. Repeated reads are not automatically retried or served from mutation replay history.

Native `loaded_during_read`, implicit PostLoad warning, dirty/status before/after, `change_state_known`, `changed`, verification scope, limits and native timing/node count are retained in `data.native_outcome`. Baseline starts **after Blueprint loading**. Stable dirty/status proves neither absence of earlier PostLoad work, arbitrary UObject immutability, independent value correctness, disk persistence nor runtime independence. `loading_side_effects_verified=false` is explicit even on success.

Observed dirty/status changes remain in `changes`. Failed loading, native exceptions or malformed native responses preserve an unknown read outcome; they are not reported as a clean no-change failure. Unavailable native binding is explicitly not dispatched. Unexpected write/compile/save flags cause failure and retain contradictory native observations. Successful-native flags without stable known dirty/status observations are rejected. None of these cases triggers automatic retries.

## Offline proof and remaining execution

`cubeless/tools/verification/test_component_python_contracts.py` contains 18 counterexamples/positive contracts: exact positional calls, no preload/API dependency, omitted/empty filter, preserved legacy array text, native errors/partial data, dirty/status changes, failed-load uncertainty, successful-load caveat, false-success rejection, unexpected write flags, malformed JSON/native exception, unavailable library, strict budgets/selectors/UTF-16 boundaries, unsupported aliases, copied response isolation and public AST defaults. They use a synthetic native library and make **no engine calls**. Existing 46 plugin tests also pass after adding N to the fake registration inventory.

Required actual proof: expected next registry count is eight Toolsets / 37 public tools, but that is an expectation, not an observed registration. Validate real schema/defaults and calls through official MCP; compare prepared local SCS fixtures and supported property encodings to independent editor values; probe unsupported/missing/budget cases; check dirty/status/template counts; explicitly save fixtures, restart, and verify cold observations. Tests must remain in isolated assets, with M/L evidence preserved unchanged.
