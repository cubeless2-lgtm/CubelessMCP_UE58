"""Nine canonical AI names plus two original fixed, typed asynchronous live smokes."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import ai_behavior_inspection
from cubeless_mcp_ue58.operations import ai_discovery
from cubeless_mcp_ue58.operations import ai_blackboard
from cubeless_mcp_ue58.operations import ai_live
from cubeless_mcp_ue58.compatibility.ue58 import ai_live as ai_live_adapter

try:
    _LiveResult = ai_live_adapter.result_class()
except Exception:
    _LiveResult = None


@unreal.uclass()
class CubelessAITools(unreal.ToolsetDefinition):
    """Native Blackboard declarations/removal, Behavior Tree creation/assignment, blank EQS, one finite standard patrol/chase subtree builder, specialized local AIController Sight configuration and shared structure readback. Two original fixed live smokes are added only with the exact typed async result dependency. Broader BT/EQS authoring, actual perception, navigation, live parity and cooked/client behavior remain incomplete. No generic executor or server."""

    @toolset_registry.tool_call
    @staticmethod
    def create_ai_blackboard_asset(asset_name: str, asset_root: str = "/Game/AI",
            save_asset: bool = True, timeout_seconds: int = 60,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one standard BlackboardData without replacing an existing asset. Original four behavior inputs/defaults are preserved; native validates the exact project target, rejects PIE/SIE, publishes the asset and performs cache/readback/save work. The initial payload may include Engine SelfActor according to project configuration, so no caller-defined keys does not mean an assumed total count zero. Returned legacy keys/counts and actual requested/attempted/saved flags are retained on failure. No explicit compilation or runtime AI execution is claimed.

        Args:
            asset_name: Original required simple asset name; native validates Unreal names and existing targets before allocation.
            asset_root: Original /Game/AI default, normalized and restricted to a project content package root by native.
            save_asset: Original True default. Force one exact requested asset save after successful creation; False retains the unsaved asset. Failed save/readback may leave a known partial asset and is not retried.
            timeout_seconds: Original 60 default, retained in caller context and the journal fingerprint. A positive signed-int32 integer; this synchronous Engine leaf has no game-thread timeout/wait or automatic timeout replay.
            operation_id: Optional durable request ID; identical completed requests return the recorded result and interrupted requests block reexecution.
        """
        return reflected_result(ai_blackboard.create_ai_blackboard_asset(asset_name,
            asset_root, save_asset, timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_ai_blackboard_key(blackboard_path: str, key_name: str, key_type: str,
            base_class: str | None = None, enum_path: str | None = None,
            instance_synced: bool = False, save_asset: bool = True,
            timeout_seconds: int = 60, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Add one native typed local Blackboard declaration using original eight behavior keys/defaults. A compatible own duplicate is retained and skips save; incompatible type/reference/synced settings and inherited/Engine keys reject. Native refreshes caches before duplicate/conflict checks and may invoke Engine callbacks; a no-change declaration is not proof that callbacks or hidden caches were untouched. All returned state/counts/actual save work survive errors. Object/Class BaseClass also admits exact saved-clean current Actor BlueprintGeneratedClass references through stock class loading when needed, followed by target/disk/dirty and readonly source/current generated-skeleton-CDO/SCS/dependency fences. Loader PostLoad/implicit compile effects are not measured zero; no explicit source compile/save or CDO creation is requested. Unappended allocation, selected key order and nonselected/source state stay guarded across callbacks; actual partial/save prefixes remain authoritative. Other generated/custom/plugin bases, arbitrary live callback effects and full original AI behavior remain incomplete.

        Args:
            blackboard_path: Original required Blackboard path; native normalizes/resolves its admitted exact project asset and reports actual load/cache scope.
            key_name: Required simple FName; Engine/inherited keys cannot be shadowed. Native preserves case-insensitive FName identity.
            key_type: Original bool, class, enum, float, int, name, object, rotator, string or vector. Do not infer aliases. Fresh native creation trims/lowercases, while original duplicate comparison keeps the requested whitespace behavior.
            base_class: Original empty-string default represented as nullable FString at reflection. Omission/null maps to empty; domain default remains empty. Object defaults to Actor, Class to UObject. An exact /Game Actor Blueprint generated-class object path may resolve with one original stock LoadObject; it must finish current, saved, clean and idle with the admitted parent/default/dependency proof. Unsupported input still rejects before target Modify/allocation here, while the original could fail after Modify/NewObject; no atomic restoration is implied. Native reports source/type and actual load/partial outcomes.
            enum_path: Original empty-string default represented as nullable FString. Omission/null maps to empty; new enum declaration requires a native-admitted exact enum with visible values in 0..255. Original existing enum duplicate with empty reference does not reconcile its reference.
            instance_synced: Original False default; existing declarations must match this value. This declares the flag and does not prove live-instance synchronization.
            save_asset: Original True default; a successful new declaration requests an exact save, a duplicate does not save. User intent and actual attempt/saved observations are separate.
            timeout_seconds: Original 60 default, positive signed-int32 caller context only; no native game-thread deadline or retry.
            operation_id: One durable standalone journal, or the existing batch parent journal through the shared action core.
        """
        return reflected_result(ai_blackboard.add_ai_blackboard_key(blackboard_path,
            key_name, key_type, base_class, enum_path, instance_synced, save_asset,
            timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_ai_asset_structure(asset_path: str, timeout_seconds: int = 60) -> CubelessPythonOperationResult:
        """Inspect the original named tool's finite Blackboard, Behavior Tree or EQS branch. Native EQS dispatch delegates existing BT/Blackboard results unchanged; Python does not load/type-sniff an asset or try a fallback. Blackboard reports ordered declarations, parent, references/synced flags and resolved IDs, including the same admitted readonly current Actor Blueprint BaseClass sources used by Object/Class keys. This does not instantiate the Actor or run an AI controller. BT reports actual runtime root/ordered children, Blackboard/editor-graph pointers and bounded dependency observations. A blank tree's legacy tree_shape fallback is not proof of authored patrol/chase tasks. EQS reports the actual query name, option count and editor-graph presence/count; the blank contract has no generator/test correctness or execution claim. Native EQS inspection reports persistence/audit issues separately while retaining a successful finite summary of an inspectable unsaved query. Custom/generated query/tree variants and incomplete inventories remain unsupported. No explicit cache refresh, graph repair, compile or save is requested by this read; resolving/loading may run Engine PostLoad work. Hidden callbacks, cold persistence and runtime AI execution are not independently proved. The existing non-composite route is unchanged; within the Composite opt-in, bounded normalized Sequence/Selector trees may also contain exact native Wait, MoveDirectlyToward and RotateToFaceBBEntry tasks, multiple closed Composite graphs and plain native Blackboard decorators. Closed Sink/AND/OR/NOT expression trees are collected in actual input-pin order, and host decorator order plus the SDK X/Y child ordering determine each runtime index/depth/parent/child and Composite execution range. The original Bool-only Composite scope remains supported; mixed trees admit 1..32 own exact Bool, Object (Actor base and null default), Vector or Name keys without a parent; an optional existing reserved SelfActor key retains its exact owned native Object/Actor-base/null-default/sync/ID role. Selected key names, ordered filters, graph authored values and links remain guarded; actual typed ID/type resolution is separate from serialized text. Sources must already have matching Tree-owned initialized runtime nodes, ordered child/decorator/logic arrays and supported flow-abort modes. Existing non-composite admission is unchanged. Custom/generated nodes, services, injected or root-level decorators, parent Blackboards, key types outside this mixed scope, shared-leaf/cyclic/disconnected expressions and sources requiring SDK rename or graph/runtime normalization remain valid legacy scopes not yet migrated. Named missing keys follow SDK Invalid/null cache resolution. Save-time PreSave may rewrite authored selector names for missing/filter-incompatible keys or key-bound config wrappers; those valid SDK transitions remain incomplete, and strict current checks preserve an actual partial save/unknown prefix rather than certify success. No live AI execution, atomic rollback or full private-cache preservation is claimed.

        Args:
            asset_path: Original required asset path; native dispatch admits its finite BlackboardData/BehaviorTree/EnvQuery branch and reports exact asset/load scope.
            timeout_seconds: Original 60 default, positive signed-int32 caller context; no synchronous game-thread timeout or wait.
        """
        return reflected_result(ai_blackboard.inspect_ai_asset_structure(asset_path, timeout_seconds))

    @toolset_registry.tool_call
    @staticmethod
    def create_ai_behavior_tree_asset(asset_name: str, asset_root: str = "/Game/AI",
            blackboard_path: str | None = None, save_asset: bool = True,
            timeout_seconds: int = 60, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one standard project Behavior Tree and its native editor graph/root without replacing an existing asset. Original five behavior keys/defaults are preserved by the domain, with optional Blackboard reflection adaptation described below. Native validates references before allocation and applies the original public graph lifecycle and both root/runtime Blackboard pointers. Root placement, graph updates and unlock may invoke Engine callbacks; null final pointers do not prove no temporary Blackboard selection/load. Returned legacy fields, raw before/after/after_save and ordered save_calls remain available on failure. Full original cleanup/failure parity, task/hierarchy authoring, cold reload and runtime AI execution remain incomplete.

        Args:
            asset_name: Original required simple asset name; the native project target must be absent before allocation.
            asset_root: Original /Game/AI default; native normalizes and restricts creation to the admitted project package root.
            blackboard_path: Original empty-string domain default represented as nullable FString. Omission/null at reflection maps to empty; this is an adaptation, since original explicit JSON null rejected. Empty requests a blank tree; a nonempty path must resolve to the native-admitted Blackboard and must be saved when this BT is saved. No Blackboard save is implied.
            save_asset: Original True default. False retains the unsaved tree. Native locks graph updates for the first forced exact save, unlocks with actual UpdateAsset work, then permits one second save only after first success leaves the package dirty. A false save is not retried; ordered attempts and actual results are retained rather than inferred from one saved boolean.
            timeout_seconds: Original 60 default, positive signed-int32 caller/journal context only; no native GT wait/deadline or request replay.
            operation_id: One durable standalone request journal; existing batch children use the shared action checkpoint without nested mutation journals.
        """
        return reflected_result(ai_blackboard.create_ai_behavior_tree_asset(asset_name,
            asset_root, blackboard_path, save_asset, timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_ai_behavior_tree_blackboard(behavior_tree_path: str, blackboard_path: str,
            save_asset: bool = True, timeout_seconds: int = 60,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Assign a required Blackboard to both the runtime Behavior Tree and its native editor root using original four behavior keys/defaults. There is no clear-to-null mode. Both pointers must already match for already_assigned; that branch skips graph updates and exact saves even when save_asset=True, and does not persist an unsaved matching tree. Changed assignment performs the actual graph/Blackboard update, dependency audit and requested bounded exact saves. Preserve returned attempt/change state, old/current pointers, graph/cache observations and save-call prefix on error; interrupted or unobservable outcomes cannot be replayed. Task execution and full legacy AI parity are not claimed. The existing non-composite route is unchanged; within the Composite opt-in, bounded normalized Sequence/Selector trees may also contain exact native Wait, MoveDirectlyToward and RotateToFaceBBEntry tasks, multiple closed Composite graphs and plain native Blackboard decorators. Closed Sink/AND/OR/NOT expression trees are collected in actual input-pin order, and host decorator order plus the SDK X/Y child ordering determine each runtime index/depth/parent/child and Composite execution range. The original Bool-only Composite scope remains supported; mixed trees admit 1..32 own exact Bool, Object (Actor base and null default), Vector or Name keys without a parent; an optional existing reserved SelfActor key retains its exact owned native Object/Actor-base/null-default/sync/ID role. Selected key names, ordered filters, graph authored values and links remain guarded; actual typed ID/type resolution is separate from serialized text. Sources must already have matching Tree-owned initialized runtime nodes, ordered child/decorator/logic arrays and supported flow-abort modes. Existing non-composite admission is unchanged. Custom/generated nodes, services, injected or root-level decorators, parent Blackboards, key types outside this mixed scope, shared-leaf/cyclic/disconnected expressions and sources requiring SDK rename or graph/runtime normalization remain valid legacy scopes not yet migrated. Named missing keys follow SDK Invalid/null cache resolution. Save-time PreSave may rewrite authored selector names for missing/filter-incompatible keys or key-bound config wrappers; those valid SDK transitions remain incomplete, and strict current checks preserve an actual partial save/unknown prefix rather than certify success. No live AI execution, atomic rollback or full private-cache preservation is claimed. Changed assignment retains the original UpdateBlackboardChange, UpdateAsset(KeepRebuildCounter), then Root.InitializeFromAsset sequence. KeepRebuildCounter does not suppress runtime rebuild or the internal second Blackboard update. Selected key names, filters, graph links and nonselected values remain guarded while resolved key ID/type are planned from the requested Blackboard. This adds no API, explicit compile, fixture authoring or live BT execution. Callback/current failures preserve the actual attempted/returned prefix and stop later work; a returned save is not proof of successful preservation.

        Args:
            behavior_tree_path: Original required mutable project BT path; native resolves its admitted exact class/editor graph and reports load scope.
            blackboard_path: Original required nonempty Blackboard path; native checks the finite class/reference boundary and persistence when saving the BT.
            save_asset: Original True default; changed assignment follows forced save plus conditional successful-first/dirty-after-unlock second save. Already-assigned skips save regardless of intent. Native actual call count/return/dirty observations are authoritative.
            timeout_seconds: Original 60 default, positive signed-int32 caller context only; no native game-thread timeout or automatic retry.
            operation_id: Existing durable operation ID and shared batch checkpoint policy.
        """
        return reflected_result(ai_blackboard.set_ai_behavior_tree_blackboard(behavior_tree_path,
            blackboard_path, save_asset, timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def create_ai_eqs_query(asset_name: str, asset_root: str = "/Game/AI/EQS",
            save_asset: bool = True, timeout_seconds: int = 60,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create the original factory-equivalent blank native EnvQuery using its four behavior inputs/defaults. Native public NewObject/lifecycle sets the query name; this route allocates no EQS editor graph, options, generators or tests. Existing destinations reject before allocation. The shared native inspector handles EQS and delegates the existing BT/Blackboard branches without Python loading/type guesses. Returned legacy summary fields, actual raw snapshots, load scope and ordered zero-or-one save calls are retained before any failure. Full original cleanup/error parity, graph authoring, cold reload, runtime query execution and cooked/client behavior are not claimed. A failure can leave an allocation/publication/save prefix; no rollback, cleanup or replay is automatic.

        Args:
            asset_name: Original required simple asset name. Native validates the absent ordinary project destination and Editor state before allocation.
            asset_root: Original /Game/AI/EQS default. Native restricts the creation root to project content without silently replacing an existing asset.
            save_asset: Original True default; after successful creation request one forced exact asset save, or retain the dirty unsaved asset when False. Actual attempt/result/disk state survives errors; a false save is not retried.
            timeout_seconds: Original 60 default, positive signed-int32 caller/journal context only. This synchronous native leaf does not enforce a game-thread deadline or wait.
            operation_id: Existing durable standalone operation ID; a supported batch action shares its one parent journal/checkpoint without nested mutator calls.
        """
        return reflected_result(ai_blackboard.create_ai_eqs_query(asset_name,
            asset_root, save_asset, timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def create_ai_patrol_chase_behavior_assets(asset_root: str = "/Game/Cubeless/AI/Smoke",
            behavior_tree_name: str = "BT_AIDummyPatrolChase", blackboard_name: str = "BB_AIDummyPatrolChase",
            target_actor_key: str = "TargetActor", patrol_location_key: str = "PatrolLocation",
            tree_shape: str = "sequence_patrol_chase", acceptable_radius: float = 55.0,
            alert_wait_seconds: float = 0.5, face_precision_degrees: float = 12.0,
            replace_existing: bool = False, save_assets: bool = False,
            timeout_seconds: int = 60, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create the original standard patrol/chase Blackboard and Behavior Tree using one finite native EngineAPI leaf. Original eleven behavior keys and timeout default are preserved. Native captures the actual optional Engine SelfActor factory baseline, resets keys as the original builder did, declares the requested keys and builds the selected standard subtree/editor graph in original order. Allocation, cache, graph, publication and ordered per-asset save prefixes are returned before errors. This is asset authoring; AI task execution, navigation, detection, perception and live SIE are not proved. With replace_existing=True, saved clean exact native isolated targets use original ordered Tree deletion, Blackboard deletion, then this same builder. External saved/loaded subtree references or incomplete observations refuse the next delete. Actual delete/create/save prefixes are retained on partial failure; original identities are not restored. SDK force deletion may reset undo and run editor/GC/global callbacks; its internal atomicity and global purity are unverified. No automatic writer replay.

        existing_target_replacement_verified reports only this invocation's scoped
        Native replacement postcondition after actual ordered deletion and rebuild.
        It does not establish full legacy parity, runtime or global callback safety.

        Args:
            asset_root: Original /Game/Cubeless/AI/Smoke default; native validates an exact project content root and both destinations before allocation.
            behavior_tree_name: Original BT_AIDummyPatrolChase default; distinct simple name for the tree.
            blackboard_name: Original BB_AIDummyPatrolChase default; distinct simple name for the Blackboard.
            target_actor_key: Original TargetActor default; native Object/Actor declaration and chase/face/IsSet selector binding.
            patrol_location_key: Original PatrolLocation default; native Vector declaration and patrol task selector. Both names must remain distinct from LastKnownTargetLocation, AIState and Engine SelfActor.
            tree_shape: Original sequence_patrol_chase default. Native admits sequence_patrol_chase, selector_chase_patrol or selector_alert_chase_patrol with their ordered task/decorator branches; arbitrary subtrees are incomplete.
            acceptable_radius: Original 55.0 default. Finite number excluding bool; native enforces float representability and the original lower clamp to zero.
            alert_wait_seconds: Original 0.5 default. Finite number; original native lower clamp to zero, retained requested value in journal and returned effective value separately.
            face_precision_degrees: Original 12.0 default. Finite number; original native clamp to [0,180].
            replace_existing: Original False default rejects existing targets before deletion/allocation/save. True with fresh targets performs zero deletions. Existing targets must be clean saved registered writable exact native assets with no open asset editor or external saved/loaded subtree referencers; Tree is deleted before Blackboard and both are recreated at the original paths. Unobserved/load/unsaved/custom/referenced variants remain qualified. No atomic rollback or original-object identity restoration is provided.
            save_assets: Original False default leaves both created assets dirty/in-memory. True saves the Blackboard first once, then the tree once or the original conditional second save after successful-first/unlock-dirty. Returned ordered save_calls/counts are authoritative, not a boolean-derived count; actual any-save True survives later failure while data.asset_saved and per-asset flags retain both-assets/verified-save meaning. Failed saves are never retried.
            timeout_seconds: Original 60 default, positive signed-int32 caller/journal context; no synchronous Engine timeout or wait.
            operation_id: Optional durable standalone ID. Batch uses one parent journal and the shared action, including a returned-native checkpoint that retains the two-resource prefix before failure.
        """
        return reflected_result(ai_blackboard.create_ai_patrol_chase_behavior_assets(asset_root,
            behavior_tree_name, blackboard_name, target_actor_key, patrol_location_key, tree_shape,
            acceptable_radius, alert_wait_seconds, face_precision_degrees, replace_existing,
            save_assets, timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def configure_ai_controller_perception(controller_blueprint: str = "/Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller",
            component_name: str = "AIPerception", sight_radius: float = 900.0, lose_sight_radius: float = 1150.0,
            peripheral_vision_angle_degrees: float = 90.0, max_age: float = 3.0, detect_enemies: bool = True,
            detect_neutrals: bool = True, detect_friendlies: bool = True, replace_existing_config: bool = True,
            save_blueprint: bool = False, timeout_seconds: int = 60,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Configure the original standard AIController local AIPerception Sight template through one specialized native EngineAPI leaf. Original eleven behavior keys and independent lose-radius default are preserved. Native owns exact class/SCS/config admission, raw versus effective age-zero readback, original clamps, configuration callbacks, structural skeleton refresh and one explicit full compile on every successful call. All returned allocation/configuration/compile/save counts, snapshots and known/unknown failure prefixes survive before domain failure. No generic component whitelist, Python load, fallback, server executor, rollback or automatic Engine retry is used. Template configuration does not prove actual actor registration, sight callbacks, known-perceived actors, target expiration, player-only filtering, navigation or live AI.

        Args:
            controller_blueprint: Original /Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller default. Loaded current clean AIController Blueprint parent chains are supported with read-only parent preservation. Only the selected child's own exact native local SCS Perception/Sight template is configured; inherited templates, custom classes, live selected instances and loaded child-derived classes remain incomplete.
            component_name: Original AIPerception default. Native preserves named local selection then finite exact-native fallback/create semantics and reports actual SCS variable name/GUID/template ownership; wrong-class or ambiguous/inherited variants reject before mutation.
            sight_radius: Original 900.0 default. Finite number excluding bool; native float representability and original lower clamp to zero.
            lose_sight_radius: Original independent 1150.0 public default, not an inferred radius+250. Native clamps it to at least clamped sight_radius.
            peripheral_vision_angle_degrees: Original 90.0 default. Native clamp to [0,180].
            max_age: Original 3.0 default. Native clamp to at least zero. Stored zero means never expire and the effective getter returns the Engine sentinel; returned stored_max_age/effective_max_age/max_age_means_never preserve that distinction and explicitly correct the legacy zero-versus-sentinel readback defect. Legacy max_age remains the effective value.
            detect_enemies: Original True default; Hostile team-affiliation bit, not a player identity filter.
            detect_neutrals: Original True default; Neutral affiliation bit.
            detect_friendlies: Original True default; Friendly affiliation bit.
            replace_existing_config: Original True default updates the existing exact native Sight config in place. False rejects existing Sight config before Modify/configuration/compile/save as an explicit safety adaptation from the legacy Modify-before-refusal order; no replacement deletion is implied.
            save_blueprint: Original False default. True forces one exact selected Blueprint save after successful compile/readback; actual ordered save_calls and saved prefix stay authoritative even when later observation fails. Structural/full compile can replace generated templates/classes and reinstance actors; implicit effects are not fully measured.
            timeout_seconds: Original 60 default, positive signed-int32 caller/journal context; excluded from synchronous native behavior arguments and does not install a game-thread timeout.
            operation_id: Optional durable standalone ID. Named batch uses the shared action and one parent journal; exact returned-native checkpoint preserves actual attempted/known/compile/save state, including known-zero rejection.
        """
        return reflected_result(ai_blackboard.configure_ai_controller_perception(controller_blueprint,
            component_name, sight_radius, lose_sight_radius, peripheral_vision_angle_degrees, max_age,
            detect_enemies, detect_neutrals, detect_friendlies, replace_existing_config,
            save_blueprint, timeout_seconds, operation_id))

    if _LiveResult is not None:
        @toolset_registry.tool_call
        @staticmethod
        def run_ai_dummy_patrol_chase_smoke(pawn_asset_path: str = "/Game/Cubeless/Reactive/BP_Piper_ReactivePrototype",
                skeletal_mesh_path: str = "/Game/Cubeless/Character/Piper/SK_Piper",
                anim_blueprint_path: str = "/Game/Cubeless/Character/Animation/ABP_RPGChracter",
                label_prefix: str = "MCP_AIDummySmoke", report_dir: str = "Docs/Analysis/AIDummySmoke",
                cleanup_existing: bool = True, cleanup_after: bool = True,
                setup_wait_seconds: float = 1.5, patrol_wait_seconds: float = 2.0,
                chase_wait_seconds: float = 2.0, sight_radius: float = 900.0,
                acceptance_radius: float = 55.0) -> _LiveResult:
            """Run the original fixed dummy patrol, distance/LOS detection and chase criteria in an owned SIE session using tick-driven public Engine calls. Return a typed pending result immediately; the official registry delivers its raw legacy report object after observed end/cleanup or an explicit quarantined failure. Distance plus AIController line_of_sight_to is distinct from AIPerception. Move request status alone is insufficient: actual ticking displacement must meet the original criteria. The initial bounded scope requires an already-open disposable Temp, legacy CubelessMCPTests or current _MCP_Temp/<feature>/CubelessTests/<run> level and finite persisted source dependency graph. No compile/save, navigation build, gameplay C++ helper, diagnostic probe, public job protocol or automatic replay is added. Disconnect does not cancel Engine work; unknown mutation/end/cleanup/journal outcomes quarantine the live workflow.

            Args:
                pawn_asset_path: Original project Blueprint default. Empty/missing generated classes select native Character before actor allocation; loaded generated classes must derive from Pawn. A failed or uncertain spawn retains its prefix and never dispatches fallback retry.
                skeletal_mesh_path: Original optional visual asset default; empty omits mesh assignment. Missing/wrong-type visuals are warnings without writes. Actual setter properties are read back; setter exceptions retain unknown effects and stop without property fallback. Rendering remains unverified.
                anim_blueprint_path: Original ABP_RPGChracter spelling/default; empty omits animation assignment.
                label_prefix: Original MCP_AIDummySmoke default; bounded dedicated MCP_ label namespace for exactly four fixed roles.
                report_dir: Original relative Docs/Analysis/AIDummySmoke default; migration writes beneath project Saved/CubelessMCP_UE58/AILiveReports, retaining this relative suffix. Absolute/outside-root directories are unsupported.
                cleanup_existing: Original True; destroy only exact four matching fixture labels before fresh setup. Existing labels with False reject before fixture mutation.
                cleanup_after: Original True; destroy exact owned Editor actors only after both session flag and game-world absence are observed. False retains them. Original global garbage collection/map-dirty restoration equivalence is incomplete.
                setup_wait_seconds: Original 1.5; negative clamps to zero, initial bound at most 30. Waits require actual world-time progress and never block the Editor thread with sleep.
                patrol_wait_seconds: Original 2.0; same finite tick wait policy.
                chase_wait_seconds: Original 2.0; same finite tick wait policy.
                sight_radius: Original 900.0 distance threshold combined with actual LOS, not a sensing-component radius.
                acceptance_radius: Original 55.0 passed to the actual native move calls. Abstract navigation availability and actual movement remain runtime evidence.
            """
            return ai_live.run_ai_dummy_patrol_chase_smoke(pawn_asset_path, skeletal_mesh_path,
                anim_blueprint_path, label_prefix, report_dir, cleanup_existing, cleanup_after,
                setup_wait_seconds, patrol_wait_seconds, chase_wait_seconds, sight_radius, acceptance_radius)

        @toolset_registry.tool_call
        @staticmethod
        def run_ai_behavior_tree_execution_smoke(pawn_asset_path: str = "/Game/Cubeless/Reactive/BP_Piper_ReactivePrototype",
                skeletal_mesh_path: str = "/Game/Cubeless/Character/Piper/SK_Piper",
                anim_blueprint_path: str = "/Game/Cubeless/Character/Animation/ABP_RPGChracter",
                label_prefix: str = "MCP_AIBTExecutionSmoke", report_dir: str = "Docs/Analysis/AIDummySmoke",
                behavior_tree_path: str = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
                blackboard_path: str = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase",
                target_actor_key: str = "TargetActor", patrol_location_key: str = "PatrolLocation",
                setup_wait_seconds: float = 1.5, bt_wait_seconds: float = 5.0) -> _LiveResult:
            """Run the original separate saved Behavior Tree smoke with actual UseBlackboard, key write/readback and RunBehaviorTree calls, then measure task-driven movement in an owned ticking SIE world. It does not substitute dummy LOS requests for task execution. The supplied native tree must have a real runtime root and the exact supplied Blackboard; blank asset authoring is not a live task proof. Typed completion contains the original report shape plus checkpoint/lifecycle scope. Cleanup-existing/after are the original private True policy. Initial fixture/source/wait/report-directory bounds and disconnect/quarantine policy match the dummy workflow. No asset authoring, explicit compile/save, NavMesh construction, perception-event or cooked/client claim is implied.

            Args:
                pawn_asset_path: Original pawn Blueprint default; empty/missing generated classes select native Character before allocation, and loaded generated classes must derive from Pawn. Failed/uncertain spawns retain their prefix without automatic fallback retry.
                skeletal_mesh_path: Original optional visual default; empty omits it. Missing/wrong-type assets are warnings without writes; setter exceptions retain unknown effects and stop without property fallback.
                anim_blueprint_path: Original optional ABP_RPGChracter default; empty omits it.
                label_prefix: Original MCP_AIBTExecutionSmoke default; exact dedicated four-role fixture namespace.
                report_dir: Original relative default, stored under the bounded project Saved/CubelessMCP_UE58/AILiveReports migration root.
                behavior_tree_path: Original saved tree default. Real native runtime subtree required; use the saved successful BD lineage for the representative rather than a failed/blank tree.
                blackboard_path: Original saved Blackboard default; actual runtime tree reference and returned component must agree with this asset.
                target_actor_key: Original TargetActor default; actual object write/readback must be the owned runtime player.
                patrol_location_key: Original PatrolLocation default; actual vector readback must match the fixed patrol target within one centimeter.
                setup_wait_seconds: Original 1.5; negative clamps to zero, admitted at most 30 with observed world progress.
                bt_wait_seconds: Original 5.0; same tick-driven bound. Run result/key checks plus actual movement threshold determine PASS, not brain status alone.
            """
            return ai_live.run_ai_behavior_tree_execution_smoke(pawn_asset_path, skeletal_mesh_path,
                anim_blueprint_path, label_prefix, report_dir, behavior_tree_path, blackboard_path,
                target_actor_key, patrol_location_key, setup_wait_seconds, bt_wait_seconds)

    @toolset_registry.tool_call
    @staticmethod
    def remove_ai_blackboard_key(blackboard_path: str, key_name: str,
            save_asset: bool = True, timeout_seconds: int = 60,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Remove one locally owned native Blackboard key; inherited/engine keys are refused. Object/Class keys can carry the admitted saved-clean Actor Blueprint BaseClass reference. Removing the last such reference changes only the selected row/IDs: the captured source remains held and readonly through final save/observation, and actual deleted/save/error prefixes remain retained.

        Args:
            blackboard_path: Existing mutable /Game Blackboard package or exact object path; native resolution/load scope is reported.
            key_name: One nonempty simple local key name. Public caches refresh before absence/inheritance/engine-key decisions; attempted cache work can be observed even without deletion.
            save_asset: Default true; only a verified own-key deletion can force-save exactly this asset once. Absent/refused requests save zero. Actual save receipt/count and failed prefix remain retained.
            timeout_seconds: Positive signed-int32 caller context; no native game-thread timeout is applied.
            operation_id: Optional durable standalone ID; batch uses the shared action and its one parent journal. Interrupted/unknown requests are never replayed automatically.
        """
        return reflected_result(ai_blackboard.remove_ai_blackboard_key(
            blackboard_path, key_name, save_asset, timeout_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_ai_authoring_support() -> CubelessPythonOperationResult:
        """Report the original eleven class exposure flags and six selected AIController members from actual current Unreal Python. Missing classes are False, not preflight failure. This performs no explicit asset load, authoring, compilation, save or world lookup. Implicit type wrapping is not independently observed. No input, timeout or operation_id was added."""
        return reflected_result(ai_discovery.inspect_ai_authoring_support())

    @toolset_registry.tool_call
    @staticmethod
    def list_ai_assets(asset_root: str = "/Game", asset_type: str = "all",
            name_filter: str = "", max_results: int = 500,
            timeout_seconds: int = 60) -> CubelessPythonOperationResult:
        """List original BehaviorTree, BlackboardData and EnvQuery/EnvironmentQuery metadata using the public recursive AssetRegistry path query. Result rows retain asset_type, asset_name, package_name, object_path and class_name; sorting is type then case-insensitive object path. No asset load, synchronous registry scan or inventory dependency audit is requested. Registry may include loaded unsaved assets; it is not a disk-only or runtime inventory.

        Args:
            asset_root: Original /Game default. Normalize slash/trailing slash; empty becomes /Game. Admit /Game or its subpaths without any dot/traversal token. Exact strings only at this reflected boundary.
            asset_type: Original all default; trim/lowercase all, behavior_tree, blackboard or eqs.
            name_filter: Original empty default; trim/lowercase substring matched against asset name or constructed object path. Original MCP-only input tokens remain rejected.
            max_results: Original 500 default, signed-int32 value clamped to 1..2000. Full total_matched is calculated before slicing; strict bool/noninteger rejection replaces historical arbitrary int coercion.
            timeout_seconds: Original 60 default. Positive signed-int32 caller context; the synchronous Engine read has no GT timeout/wait or automatic retry.
        """
        return reflected_result(ai_discovery.list_ai_assets(asset_root, asset_type,
            name_filter, max_results, timeout_seconds))

    @toolset_registry.tool_call
    @staticmethod
    def search_ai_behavior_tree_node_classes(category: str = "all", query: str = "",
            include_abstract: bool = False, max_results: int = 200,
            timeout_seconds: int = 60) -> CubelessPythonOperationResult:
        """Search actual loaded native and generated BT classes with the original finite native flags/display-name filters. The hidden EngineAPI observer returns the original class rows and preserves unsupported-category errors; Python does not guess UClass flags or load missing classes. Base Composite/Task/Decorator/Service classes and deprecated/newer-version/transient naming variants are excluded. The native observer requests no explicit CDO creation or asset load. Python static dispatch may obtain its library CDO before the observer begins; that wrapping scope and metadata effects are not independently instrumented.

        Args:
            category: Original all default; native lowercases without trimming, then admits all, composite, task, decorator or service.
            query: Original empty default; native lowercases without trimming and matches name/path/display name substrings. Original MCP-only input tokens remain rejected.
            include_abstract: Original False default, exact boolean; True retains matching abstract descendants.
            max_results: Original 200 default, signed-int32 value clamped to 1..500. Legacy category/name sort and bounded rows remain native-authoritative.
            timeout_seconds: Original 60 default, positive signed-int32 caller context only; not forwarded into the four-argument native observer.
        """
        return reflected_result(ai_discovery.search_ai_behavior_tree_node_classes(
            category, query, include_abstract, max_results, timeout_seconds))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_ai_behavior_assets(
            behavior_tree_path: str = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
            blackboard_path: str = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase",
            timeout_seconds: int = 60) -> CubelessPythonOperationResult:
        """Inspect one explicitly supplied Behavior Tree/Blackboard pair with the original path defaults and twelve checks. The native paired adapter reuses existing bounded readers and resolves both inputs in original BT-then-BB order before failed-load decisions. Supplied Blackboard is independently projected and audited even when it differs from the tree's assigned Blackboard; mismatch or False structure/key/audit checks do not by themselves fail an otherwise complete inspection. Typed failures preserve actual load/reader/observation prefix. No repair, cache refresh, compile, save or AI execution is requested. Native admission remains finite exact stock assets/nodes outside PIE/SIE; arbitrary custom/generated/corrupted variants remain incomplete. Wrapper dispatch can obtain its library CDO before native observation, and load/PostLoad effects are separately scoped.

        Args:
            behavior_tree_path: Original default saved Smoke Behavior Tree object path. Native normalizes/validates both requested paths before loading; explicit empty/null or invalid control/surrogate strings are rejected, not replaced with defaults.
            blackboard_path: Original default saved Smoke Blackboard object path. This requested resource is distinct from any Blackboard assigned to the tree or editor root. Original checks use fixed TargetActor and PatrolLocation names, not inferred custom builder keys.
            timeout_seconds: Original 60 default, positive signed-int32 caller context only; excluded from the two-argument EngineAPI and not a game-thread timeout/retry.
        """
        return reflected_result(ai_behavior_inspection.inspect_ai_behavior_assets(
            behavior_tree_path, blackboard_path, timeout_seconds))
