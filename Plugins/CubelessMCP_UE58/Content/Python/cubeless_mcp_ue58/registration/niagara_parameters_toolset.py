"""Named finite User parameter tools on the official Registry; no transport coupling."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


@unreal.uclass()
class CubelessNiagaraParameterTools(unreal.ToolsetDefinition):
    """Typed saved-system User declarations and values; export scheduling remains separate."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_user_parameters(system_path: str) -> CubelessPythonOperationResult:
        """Inspect actual offset-backed User parameter values/types and bounded system/store/package/compiler state before and after. Requires the exact system already loaded; no load, edit, compile, save or execution fallback. Typed primitive/Position/UObject values are finite. Exact native ArrayPosition/Float3/Float2/Float/Int32/Mesh DI rows expose class, target ownership, reference identity and bounded public payload storage. ArrayMesh exposes ordered mesh/scale/rotation/pivot_offset/pivot_offset_space values and finite payload support; the other five array values remain null/settable_by_mcp=false with declaration emptiness/preservation observations only. Readback alone does not prove payload mutation, original-double ArrayPosition precision, weighted renderer selection, module consumption or particle export. Generic variants remain incomplete. Returns old parameter rows/counts plus precise native evidence.

        Args:
            system_path: Exact already-loaded runtime /Game Niagara System package or object path; load explicitly through a named official asset tool first.
        """
        from cubeless_mcp_ue58.operations.niagara_parameters import inspect_niagara_user_parameters
        return reflected_result(inspect_niagara_user_parameters(system_path))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_user_parameter(system_path: str, parameter_name: str, value_json: str,
            allow_source_edit: bool = False, save: bool = True,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Set one existing supported User parameter by bare or User-prefixed name. Flexible legacy value is expressed as JSON text; explicit null is preserved. Finite value types are Float/Int/Bool/Color/Vec2/3/4/Position/UObject/MaterialInterface and exact native ArrayMesh, with actual class/offset/reference and value checks. ArrayMesh accepts at most512 ordered StaticMesh path strings or objects with mesh/mesh_path/static_mesh, scale, rotation, pivot_offset and pivot_offset_space; defaults are unit scale, zero rotation/pivot and Mesh space. Explicit[] clears; null is invalid. Bounded /Game or /Engine meshes may be explicitly loaded before a write, with load/PostLoad effects reported separately. Exact editable public MeshData metadata and normal notified property permissions are required; no Internal/private DI API is used. Equal valid writes retain legacy write/compile behavior. Requests compile once after a successful set; pending compilation defers requested save with a known partial failure because Niagara PreSave would block. Inspect readiness then perform one exact official save; never resubmit the value just to save. No game-thread wait, rollback, uncertain retry, emitter creation or runtime receiver binding. The other five numeric Array-DIs reject every explicit payload including null/[]; runtime component array setters are a different operation. General payload types and renderer/GPU execution remain incomplete.

        Args:
            system_path: Exact already-loaded runtime /Game Niagara System package or object path.
            parameter_name: Existing bare or User-prefixed name, matched against actual native declarations; no declaration creation.
            value_json: Bounded JSON scalar/boolean/numeric array, null/exact already-loaded runtime asset path for admitted ordinary object types, or ordered ArrayMesh path/object rows. Present array-valued Mesh vectors require three finite numbers; omitted or non-array optional vector fields retain legacy defaults, and unknown row keys are ignored with counts. PivotOffsetSpace accepts Mesh/Simulation/World/Local or an exact enum integer. JSON object vector values are not the primitive contract. Native exact-int, class, edit-permission and finite-value checks are authoritative.
            allow_source_edit: False admits legacy /Game/_MCP_Temp/NiagaraGenerated and current /Game/CubelessMCPTests fixtures. True allows other explicitly chosen /Game assets while all runtime/reference/preservation guards remain enforced.
            save: Default true requests one exact save after setting, never a save-all. Pending compilation returns saved=false, attempted=false and a precise deferred failure. False retains dirty state; completion/cold reload are separate observations.
            operation_id: Optional durable ID; completed identical requests replay their record, interrupted/uncertain mutations are never automatically dispatched again.
        """
        from cubeless_mcp_ue58.operations.niagara_parameters import set_niagara_user_parameter
        return reflected_result(set_niagara_user_parameter(system_path, parameter_name, value_json,
            allow_source_edit, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def upsert_niagara_user_parameters(system_path: str, parameters_json: str,
            continue_on_error: bool = False, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create or update an ordered finite User parameter batch, preserving legacy row aliases, explicit-null/default omission, same-type admission, counts and stop/continue behavior. Flexible legacy parameters array is JSON text. Equal valid writes retain legacy counts/compile behavior. Admits omitted-value declarations/reuse for exactly six loaded native NiagaraDataInterfaceArray classes: Position, Float3, Float2, Float, Int32 and Mesh, using finite legacy aliases or exact /Script/Niagara class paths. Public store AddParameter initializes the new target-owned DI; data_interface_created retains its legacy manual-fallback meaning(false), while store_initialized_data_interface/observed_data_interface_creation separately record actual implicit creation. Omitted values retain exact existing DI identity/payload. ArrayMesh additionally accepts the named setter's bounded path/object payload, including[] to clear; null/invalid mesh/class/array-valued vector/space rejects before declaration; non-array optional vectors and unknown keys retain the original ignored-field behavior. Mesh loads and normal public property notifications are reported separately from store/value writes. Other five array payloads still reject explicit null/[]/values. Invalid row values/classes are prevalidated before declaration, avoiding the old orphan-on-value-failure behavior. Object/materialinterface aliases are an explicit extension to the original exporter. One final requested compile and exact save after changes; pending compilation defers save with known retained work. Inspect readiness then issue one exact official save, never rerun values to save. Generic classes, numeric System array payloads, source DI copying, module/emitter/weighted renderer/GPU/export remain incomplete. Unknown state stops all further work even with continue_on_error; no rollback or automatic retry.

        Args:
            system_path: Exact already-loaded runtime /Game Niagara System package or object path.
            parameters_json: Bounded JSON array using name/parameter_name, type/type_name and optional value/default_value. Row order/aliases remain unchanged; value presence has precedence over default_value. Omit both value fields for any admitted Array-DI to declare an observed empty interface or retain the existing same-type payload. ArrayMesh explicit[] clears and nonempty rows follow the setter's five-field contract; null is invalid. Numeric Array-DI explicit values/null/[] remain unsupported. ArrayPosition declaration is not original-double payload precision proof.
            continue_on_error: Default false stops at the first invalid row. True continues only known preflight validation failures. A failed dispatched mutation postcondition stops even with an observable current state; unknown state always stops and suppresses final compile/save.
            allow_source_edit: False admits legacy /Game/_MCP_Temp/NiagaraGenerated and current /Game/CubelessMCPTests fixtures. True allows other explicitly chosen /Game assets without permitting editor/MCP/Cubeless references.
            save: Request one exact save after a changed batch, including known partial batches. If compile is outstanding/needed, returns precise deferred failure with no save dispatch; no per-entry, unrelated or automatic retry saves.
            request_compile: Request native system compile once after a changed batch. Completion is separately observed, never inferred from the request or a sleep on the game thread.
            operation_id: Optional durable ID; completed identical requests replay recorded results, incomplete/uncertain requests do not retry automatically.
        """
        from cubeless_mcp_ue58.operations.niagara_parameters import upsert_niagara_user_parameters
        return reflected_result(upsert_niagara_user_parameters(system_path, parameters_json,
            continue_on_error, allow_source_edit, save, request_compile, operation_id))
