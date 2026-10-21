"""Ten named Sequencer contracts on the official reflected registry."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import sequencer


@unreal.uclass()
class CubelessSequencerTools(unreal.ToolsetDefinition):
    """Public Engine factory/binding/observation and seven finite native MovieScene mutators with exact asset save accounting, plus one explicit Editor UI leaf. The existing nine sequence operations (Info, ActorBinding, Transform, Float, Playback, Remove, Subsequence, CameraCut and Open) use guarded primary resolution: an exact already-loaded sequence takes zero LoadAsset calls, while an absent primary canonical Game asset may invoke the original LoadAsset once. The actual returned sequence is retained and current Editor/world, identity/class/package and owned MovieScene are checked before continuation. Subsequence resolves Parent first and then the selected Child, so each primary path may separately be loaded once; sibling/nested-source and internal postchecks remain find-only with zero additional loads. Native load/failure/callback receipts are preserved; unknown stops later getters, writers and saves without retry. Loader boundary proof is not global PostLoad/cache/file purity, cold persistence, playback/render or full original pipeline parity. Existing finite track/binding/composition scope and valid wider incomplete cases remain unchanged. Create keeps its separate fresh-target factory contract."""

    @toolset_registry.tool_call
    @staticmethod
    def create_level_sequence(sequence_path: str, frame_rate: float = 30.0,
            playback_start_frame: float = 0.0, playback_end_frame: float = 150.0,
            save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one fresh native LevelSequence through public AssetTools/default LevelSequenceFactoryNew. Existing registry, loaded or disk targets are rejected. The factory initializes MovieScene; no unreflected Initialize call is attempted. Exact save request/attempt/result are separate. Source control must already be disabled; no editor settings are changed. Fractional playback uses the finite native public SetPlaybackRange path and original display-to-tick floor conversion; actual returned native bounds are retained. Implicit callback work remains unmeasured.

        Args:
            sequence_path: Fresh /Game package path or matching object path. No simple-name search, overwrite, deletion or existing-target fallback.
            frame_rate: Original 30 default. Integral fps in (0,480]; 29.97 fails before factory/allocation. Original near-integer tolerance is retained.
            playback_start_frame: Original 0 default in display frames. Finite signed-int32 display bounds, including fractional values; native FromDecimal/TransformTime/FloorToFrame preserves original tick conversion. Scaled tick overflow fails without native range mutation; an earlier created asset remains retained.
            playback_end_frame: Original 150 default; must exceed start and fit signed-int32 display frames and native scaled ticks.
            save: Original True default; one exact forced public save_loaded_asset(asset, False) after configuration. Implicit engine saves/callbacks are not counted as explicit calls.
            operation_id: Optional durable ID. One parent journal; interrupted work is never retried automatically.
        """
        return reflected_result(sequencer.create_level_sequence(sequence_path, frame_rate,
            playback_start_frame, playback_end_frame, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def get_level_sequence_info(sequence_path: str) -> CubelessPythonOperationResult:
        """After guarded loaded0/primary-cold1 resolution, observe an exact native sequence's rates, integer display-frame playback projection, binding names/GUIDs/tracks and transform channel payload. Public scripting observation creates transient channel/key wrappers; it is an asset payload observation without a claim of zero allocation or callback effects. Native localized AsPrettyText rate strings and precise fractional playback bounds are not reproduced; additive rational rate fields retain actual numeric authority. No explicit sequence mutation/compile/save/playback/render. A cold load can invoke Engine callbacks; Info reports its actual load prefix and does not label that path a loaded-only read.

        Args:
            sequence_path: Canonical /Game package or matching object path. Guarded primary resolution uses the existing asset or one original LoadAsset call; legacy simple-name and broader class loading remain incomplete.
        """
        return reflected_result(sequencer.get_level_sequence_info(sequence_path))

    @toolset_registry.tool_call
    @staticmethod
    def add_level_sequence_actor_binding(sequence_path: str, actor_label: str,
            save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Bind the first current-editor-world Actor label/object-name match. Reuse the first binding named with that Actor's actual label, preserving the original zero-write/no-save reuse branch. Otherwise public AddPossessable creates a real native binding and the returned GUID/count/name/prior binding projection are verified. The public resolver may create transient evaluation state. An already-bound same Actor under a different name is rejected before mutation because public CreatePossessable would reuse it instead of the original always-new behavior. No actor spawn or natural playback.

        Args:
            sequence_path: Exact native /Game sequence path; guarded primary resolution reports loaded0 or cold LoadAsset1 before binding work.
            actor_label: Current level Actor label or object name; first world Actor iteration match is retained. No synthetic Actor or ambiguous class fallback.
            save: Original True default; force exact sequence save after a new binding. Existing same-label reuse reports save_requested=False/saved=False even when this argument is True, matching the original branch.
            operation_id: Optional durable named mutation ID; no uncertain request replay.
        """
        return reflected_result(sequencer.add_level_sequence_actor_binding(sequence_path, actor_label, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_level_sequence_transform_keys(sequence_path: str, keys: str,
            binding_id: str | None = None, actor_label: str | None = None,
            save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Add Cubic/Auto transform keys through the finite native public AddCubicKey path, retaining same-tick duplicate insertion and original display-to-tick floor conversion. Preserve original ordered rows/optional location, rotation, scale fields and keys_added row count, with scalar_keys_added for actual axis calls. Exactly nine Double channels are observed in Location.X/Y/Z, Rotation.X/Y/Z (roll,pitch,yaw), Scale.X/Y/Z order; separate Float Weight is preserved. Native full time/value/interpolation/tangent payload and actual channel-copy postconditions are retained before any failure or requested Python save. Whole request validation precedes allocation; original late-invalid-row partial mutation is deliberately not reproduced. Wrong-type first sections and overridden channels fail before writes. Native known partial/unknown dispatch states stop without retry or save. Public scripting observers create temporary wrappers, but Scripting AddKey and its explicit channel broadcast are not used. Native Modify/signature/cache callbacks and runtime/render effects remain unmeasured.

        Args:
            sequence_path: Exact native /Game sequence package/object path; guarded primary resolution reports loaded0 or cold LoadAsset1.
            keys: Original key represented as required JSON array text, e.g. [{"frame":0,"location":[1,2,3],"rotation":[0,0,0],"scale":[1,1,1]}]. The standalone domain keeps the original list of dictionaries. 1..512 rows; finite numeric frame and present three-number arrays; bool excluded. Missing fields skip silently, frame-only rows still count, unknown fields are ignored. Whole malformed/oversized JSON rejects before Engine dispatch. Native selected channels are bounded to 8192 keys each; narrower public observation bounds also apply. This reflected representation is not full original public I/O parity.
            binding_id: Original empty default represented by nullable text; supplied valid nonzero GUID takes precedence, and invalid/missing supplied GUID does not fall back to actor_label.
            actor_label: Original empty default represented by nullable text; first matching binding name only when GUID is absent. This selects an existing binding, not a new Actor.
            save: Original True default; exact forced sequence save once after completed keys. save=False leaves owned changes dirty for a later explicit boundary.
            operation_id: Optional durable ID. Failed/partial work is never resubmitted automatically; batches call the shared action without a nested journal.
        """
        return reflected_result(sequencer.add_level_sequence_transform_keys(sequence_path, keys,
            binding_id, actor_label, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_level_sequence_float_property_keys(sequence_path: str, property_path: str,
            keys: list[unreal.JsonObjectWrapper], binding_id: str = "", actor_label: str = "",
            save: bool = True) -> CubelessPythonOperationResult:
        """Add original ordered Cubic/Auto keys to one Float property track on an existing binding. Public keys remains array<object> through stock Array(JsonObjectWrapper) and the domain list of dictionaries; only the agreed hidden5 receives JSON text. Native source mutation preserves exact display-frame floor, float32 values and same-tick AddCubicKey insertion; Scripting AddKey replacement/rounding is never substituted. Track selection retains first exact property-path match and original first-section behavior within the final native finite admission. Whole malformed request validation occurs before native writes; original late-invalid-row partial failure remains an explicit gap. Raw native track/section/channel/tangent/time-value/default and partial outcomes are retained before error or the requested single forced public save. Public post-save observers compare their precise Float projection while full private native source/save and cold proofs remain scoped separately. No new actor, binding, property evaluation, compiler, playback, render, transport, automatic retry or rollback. The original literal property path is not proof that a bound Actor field resolves or animates.

        Args:
            sequence_path: Original required path; guarded preparation resolves an exact native Game LevelSequence with actual loaded0/cold1 receipt. Its retained asset is handed to native insertion; later internal resolution remains load0.
            property_path: Original required nonempty literal path, e.g. CameraComponent.FieldOfView; last-dot suffix is the native property name. Actual actor-property resolution is unverified.
            keys: Original required nonempty JSON array of objects with finite frame/value. Adapter1..512 rows and bounded JSON; unknown fields retained for native ignored-field semantics. Signed-int32 display floors and finite float32 values required; scaled tick bounds remain native. Duplicate ticks retain insertion and caller order.
            binding_id: Original empty-string default, non-null. Valid nonzero GUID preferred; invalid/missing supplied GUID never falls back to actor_label.
            actor_label: Original empty-string default, non-null; first matching existing binding name only when GUID absent. It is not an Actor spawn/lookup request.
            save: Original True default; one exact forced public sequence save after known native insertion, with raw actual attempts/receipts preserved. No public timeout/operation_id input.
        """
        import json
        from cubeless_mcp_ue58.operations.sequencer_float import add_level_sequence_float_property_keys
        from cubeless_mcp_ue58.operations import state
        try:
            rows = [json.loads(row.export_text()) for row in keys]
        except Exception as error:
            result = state.fail(state.new_result(), state.OperationError("sequencer_invalid_keys", "Stock JsonObjectWrapper key conversion failed: " + str(error)))
            result["data"].update(operation="add_level_sequence_float_property_keys", admission_rejected=True,
                native_dispatched=False, native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
            return reflected_result(result)
        return reflected_result(add_level_sequence_float_property_keys(sequence_path, property_path, rows, binding_id, actor_label, save))


    @toolset_registry.tool_call
    @staticmethod
    def set_level_sequence_playback_range(sequence_path: str, playback_start_frame: float,
            playback_end_frame: float, save: bool = True) -> CubelessPythonOperationResult:
        """Set existing sequence playback tick bounds from original display-frame inputs. Every successful apply still performs the original native Modify/dirty lifecycle and requested single forced exact save, including an unchanged requested range; semantic changed=False is not zero work. Native retains original decimal tick-to-display range checks separately from exact floor conversion: a fractional requested frame can mutate/save and then fail the strict round-trip postcondition. Source-computed mutation_result_v1 metadata and raw complete/partial observations, actual save receipts and typed known/changed prefix remain separate. No second Python save, compiler, playback, UI opening, rollback or automatic retry. Guarded primary loaded0/cold1 resolution of the exact native Game sequence and other finite native guards apply; name-only loads, broader classes and complete external pipeline proof remain incomplete.

        Args:
            sequence_path: Original required exact native Game LevelSequence path; primary resolution uses loaded0 or one cold LoadAsset call before playback mutation.
            playback_start_frame: Original required finite numeric display frame; native floor conversion and scaled int32 bounds are authoritative.
            playback_end_frame: Original required finite numeric end greater than start; strict requested decimal readback may reject a quantized result after actual work/save.
            save: Original True default; native owns actual0/1 forced save and terminal save-gate verification. No extra public operation_id/timeout.
        """
        from cubeless_mcp_ue58.operations.sequencer_maintenance import set_level_sequence_playback_range
        return reflected_result(set_level_sequence_playback_range(sequence_path, playback_start_frame, playback_end_frame, save))


    @toolset_registry.tool_call
    @staticmethod
    def remove_level_sequence_binding(sequence_path: str, binding_id: str = "", actor_label: str = "",
            save: bool = True) -> CubelessPythonOperationResult:
        """Remove one current native possessable binding, its tracks and sequence binding references. Preferred GUID never falls back to actor_label; caller selector spelling is preserved for the native parser/literal name match. The finite prewrite guard rejects missing/spawnable/custom and unsupported hierarchy/reference shapes rather than reproducing the original Modify-before-RemovePossessable-failure variant. A separate finite removal profile admits a selected native SceneComponent possessable (including admitted native subclasses) whose declared parent is a parentless native Actor possessable and which has no possessable children. Leaf refers to the sequence binding hierarchy, not the world component attachment tree. An additive mixed-scene profile permits a selected ordinary parentless possessable while preserving nonselected native modern SpawnableActorBinding/plain Actor templates, stock SpawnTrack with exact native BoolSection or SpawnSection, native leaf folders and fixed root binding tags. Collateral legacy spawnable declarations are observed separately; this does not add selected spawnable/custom deletion, nested template/components/folders or hierarchy rewrites. Only the selected binding, its admitted tracks and its binding references are retired; the parent and other captured binding/track/reference state remain protected. Locators are not resolved and no bound Actor/component is destroyed, detached or edited. A separate finite hierarchy profile removes one parentless native Actor possessable with native SceneComponent descendants in an admitted closed acyclic inventory. The direct SDK removal leaves each surviving child ParentGuid literal unchanged; child bindings, tracks and locators remain protected without reparenting or recursive deletion. Broader hierarchy, custom bindings and live callback/runtime behavior remain incomplete. An exact bounded native nonblend auto-managed CameraCut track may coexist: its section order, fields, ranges and full binding literals are preserved, including a literal that names the removed GUID. No cut cleanup, GUID rewrite or runtime camera resolution is added. Actual removal/unbind/dirty/save calls, retired declarations and complete/partial before/after/after-save scopes remain native evidence; removal does not destroy the world Actor. Native owns requested forced exact save and specific GUID absence/count decrement/save-gate strict mutation metadata. No Python second save, Actor cleanup, compiler, evaluation/player, transport fallback, rollback or uncertain retransmission. Broader binding/reference/lifecycle variants and whole original pipeline remain incomplete.

        Args:
            sequence_path: Original required exact sequence path; guarded primary loaded0/cold1 resolution precedes removal admission.
            binding_id: Original empty-string non-null default; supplied GUID is preferred and parser/admission remain native-authoritative.
            actor_label: Original empty-string non-null default; first literal binding-name match only when GUID is absent. Empty selectors are an error; no Actor lookup/spawn.
            save: Original True default; actual native save flags/counts/ordered receipts are preserved before postcondition errors.
        """
        from cubeless_mcp_ue58.operations.sequencer_maintenance import remove_level_sequence_binding
        return reflected_result(remove_level_sequence_binding(sequence_path, binding_id, actor_label, save))

    @toolset_registry.tool_call
    @staticmethod
    def add_level_sequence_subsequence(sequence_path: str, subsequence_path: str,
            start_frame: float, duration_frames: float, save: bool = True) -> CubelessPythonOperationResult:
        """Add one child LevelSequence section to the parent root SubTrack through finite public native MovieScene APIs. The original five behavior inputs and save=True remain; framework ctx is excluded. Both assets must be exact native Game sequences with ordinary finite parent/child shapes. Primary Parent resolution precedes primary selected Child resolution; each uses its existing asset or one original LoadAsset call. Both returned source roles are retained, and later sibling/nested-source and internal postchecks perform no additional loads. Preserve original display-frame floor conversion including positive requested duration whose floored tick duration is zero. Native owns the optional single forced parent save; raw actual allocation/Modify/section/range/row/timecode/child/source/save observations and ordered receipts survive failed or unknown results before the caller journal returns. Requested-save requires a clean persisted selected child and every admitted collateral child. An existing root SubTrack may reference up to four distinct exact already-loaded native Game leaf siblings in separate packages. Their current source/section/fields/signatures/dirty/disk state is checked read-only at explicit SDK, save and transaction-publication boundaries; only the parent dirty row follows actual parent dirty. Existing subsection identities, assignments, ranges and rows remain exact; no sibling is loaded, resolved to a world, compiled or saved. A separate finite nested-child route admits a fixed exact already-loaded Parent-to-Child-to-Leaf chain in distinct native Game assets. The selected Child has one root exact SubTrack with one exact SubSection referencing one Leaf; Leaf has no SubTrack. In this nested route, existing Parent subsections may reference only that selected Child. Child and Leaf remain read-only, including other admitted Float/Transform and finite CameraCut tracks, while the requested section is added only to Parent. Native checks their current identity, source, captured fields, existing section membership/order, child assignments, ranges, rows, frame/timecode data and source files at the applicable SDK/save/publication boundaries. Requested-save requires clean persisted Child and Leaf; save=False instead preserves their initially observed dirty state. This does not generalize the separate flat collateral-child route. Fixed finite playrate is admitted; cycles, aliased assets, deeper chains, additional nested collateral, Director/custom tracks and timewarp variants remain incomplete. Exact bounded native nonblend auto-managed CameraCut tracks on parent or child are preserved by current-role and literal readback; their presence alone is no longer a refusal. No Python second save, new Actor, compiler, arbitrary nested/custom/director composition fallback, playback, UI, rollback or automatic retry. The original Subsequence response had no mutation_result_v1; stronger native typed proof does not manufacture that contract. The guarded primary loading handoff does not establish full original lifecycle, arbitrary cold nested sources or external mixed-pipeline parity.

        Args:
            sequence_path: Original required parent sequence path; canonical package or matching object suffix of the finite exact native Game asset, resolved first with loaded0/cold1 receipt.
            subsequence_path: Original required selected Child path, resolved after Parent with its own loaded0/cold1 receipt and exact returned identity. Collateral siblings and nested Leaf sources must still already be loaded. Self-reference and unsupported recursive/composition variants fail before writes.
            start_frame: Original required finite numeric start in the parent's display frames; native floor, rate-product and signed-int32 bounds remain authoritative.
            duration_frames: Original required positive numeric display duration; native accepts zero floored ticks when requested duration is positive, preserving the original edge.
            save: Original True default; native actual0/1 forced exact parent save, strong parent/selected-child/admitted-leaf readback and ordered returned prefix; neither Child nor Leaf is saved by this operation. No added operation_id/timeout/selectors.
        """
        from cubeless_mcp_ue58.operations.sequencer_subsequence import add_level_sequence_subsequence
        return reflected_result(add_level_sequence_subsequence(sequence_path, subsequence_path, start_frame, duration_frames, save))



    @toolset_registry.tool_call
    @staticmethod
    def add_level_sequence_camera_cut(sequence_path: str, start_frame: float, end_frame: float,
            binding_id: str = "", actor_label: str = "", save: bool = True) -> CubelessPythonOperationResult:
        """Preserve original CameraCut6 with literal non-null selectors, one journal and native forced-save authority. Both GUID and label are passed literally; a preferred missing GUID never falls back to a valid label. The guarded primary resolved native sequence must fit the bounded writable exact native nonblend auto-managed camera-track scope. Existing sections are unique exact native row-zero sections with finite nonempty inclusive-start/exclusive-end ranges. The common native section-array plan admits ordinary existing arrangements including overlapping ranges and equal start times; it no longer requires an empty/single or exactly three-cut layout, initially sorted nonoverlapping cuts, or a distinct insertion start. Native predicts the SDK temporary end, exact reuse lookup, section sorting and permitted neighbor range changes before the original requested SetRange. The original handler did not verify an actual camera Actor class or resolve a runtime camera, and this wrapper does not add such a claim.

        Native current section/local GUID/floor range and bounded parent/source fields are observed before optional forced save0/1. Public success retains the original strict section-name/range/countAfter==countBefore+1/save predicates. Typed current proof records actual allocation or reuse and preservation; it does not override an original count failure. Genuine exact reuse keeps the count and may complete the requested save before returning the original verification failure. The original requested created-count literal is not an actual allocation claim. Past strict-failure receipts are not rewritten. All raw partial/complete snapshots, observed membership/retirement counts and actual save receipts survive later failure. No Python second save, compile, playback/render, rollback or uncertain retry. Primary Native asset resolution separately records its actual loaded0/cold1 prefix; internal postchecks do not load again.

        Existing section membership and SDK sorting are checked against the copied plan rather than assuming the original array order. Selected range/binding and permitted neighbor range/signature/dirty transitions follow the bounded SDK plan; unrelated authored data remains protected. Unpredicted lifecycle/current drift stops further calls and retains the actual prefix. Blended, nonzero-row, non-auto-managed, custom/nested/director/reference, start-at-or-after-playback-end fallback and wider bound/source/lifecycle variants remain incomplete. SDK-internal callbacks and global purity, camera playback/rendering and whole original pipelines are not proved.

        Args:
            sequence_path: Exact /Game package or matching object path. String adaptation stays pure; the subsequent guarded Native primary resolver records loaded0/cold1 and has no retry/fallback.
            start_frame: Finite original display-frame start; native floor conversion and integer safety budgets remain authoritative.
            end_frame: Finite original display-frame end greater than start; zero floored duration remains valid quantization.
            binding_id: Original non-null empty default. Native GUID parser sees literal spelling and has priority over label.
            actor_label: Original non-null empty default. Exact current modern binding name, no whitespace stripping.
            save: Original True default. One native forced exact SaveLoadedAsset(False) only after current state proof; strict terminal failure can retain a successful save.
        """
        from cubeless_mcp_ue58.operations import sequencer_camera
        return reflected_result(sequencer_camera.add_level_sequence_camera_cut(sequence_path,
            start_frame, end_frame, binding_id, actor_label, save))

    @toolset_registry.tool_call
    @staticmethod
    def open_level_sequence_in_editor(sequence_path: str) -> CubelessPythonOperationResult:
        """Resolve the exact native sequence through the guarded primary loaded0/cold1 path, then open it through the public AssetEditorSubsystem, preserving original one-input behavior and actual opened bool. This is an Editor UI operation: it can focus an existing toolkit, invoke notifications/delegates, load Editor modules, update MRU/cache and flush Editor config. It is not a pure read or a proof of visible UI, rendered output or playback.

        One original outer transaction and actual Open/Find calls are reported separately from bounded current target/config/dirty/disk/editor-presence observations. The raw returned bool and partial observations survive errors; an incomplete after observation remains unknown without a retry or automatic editor closure. Beyond the separately recorded primary asset resolution, no explicit Modify/dirty/compile/save is added by our wrapper; implicit loader/toolkit work is not exhaustively measured. Original result had no mutation_result_v1. Broader custom toolkit/loading and whole original mixed pipeline scope remain incomplete.

        Args:
            sequence_path: Original required canonical /Game package or matching object path of an exact native LevelSequence. Primary resolution uses its existing loaded object or one LoadAsset call before opening; no operation_id/timeout/save parameters added.
        """
        from cubeless_mcp_ue58.operations.sequencer_editor import open_level_sequence_in_editor
        return reflected_result(open_level_sequence_in_editor(sequence_path))
