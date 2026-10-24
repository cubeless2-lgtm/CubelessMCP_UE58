"""Function extraction public10 with private node JSON and int64 decimal carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import blueprint_function_extraction as operation

@unreal.uclass()
class CubelessBlueprintFunctionExtractionTools(unreal.ToolsetDefinition):
    'Function extraction additionally admits bounded nested stock Composite ownership closures through the existing SDK extraction writer. Each immediate SubGraphs set must correspond one-to-one to an exact owned Composite BoundGraph, with separate source Tunnel2/primitive native-pure/Branch/Sequence DAG checks and retained logical roles at every depth. SDK moves only selected top-level BoundGraphs from source to destination SubGraphs; descendant bodies retain their immediate Composite ownership. Existing partial/unknown, compile/save and no-retry contracts remain unchanged. Wider body classes/types/owners and untested deep variants remain incomplete or usage pending.\n\nThe existing guarded CustomEvent+Composite profile includes selected data fan-out, implemented / usage validation pending. SDK-disconnected links and new defaults are reported without synthetic repair; exec fan-out, external/delegate consumers and broader signature/profile variants remain incomplete. See the method description for the current detailed scope.\n\nGuarded original Blueprint function extraction public10: Native validates one complete GUID selection, owns a real derived Blueprint Editor instance, diagnostic graphs and UI state, native selection extraction, requested compiler/save and actual partial or cleanup receipts. Even dry_run may create temporary diagnostic graphs; restoring dirty/status flags is not total SDK rollback. Original temp guard and explicit allow_non_temp remain. No uncertain retry or atomic rollback is provided. Official HTTP numeric tokens may already be rounded upstream. A bounded direct-native Actor Blueprint profile additionally admits one selection set containing multiple directly owned stock Composites and other Native-admitted ordinary stock nodes. Each Composite owns a distinct owned BoundGraph with exactly two complementary stock Tunnels. Its finite reciprocal acyclic body may contain multiple exact native KismetMathLibrary by-value primitive pure calls, stock Branch and ExecutionSequence nodes. Native snapshots actual function descriptors, primitive property layouts, authored purity override, pin defaults and ordered links before public type conversions; normalized hidden self retains the already-existing authoritative native owner CDO without creating one. Only the selected source-graph node GUIDs belong in node_ids; each Composite body stays owned by its Composite and its descendants are not flattened into the selection or expected_node_count. Links within the selected set are preserved, while crossing primitive by-value data and exec boundaries follow the actual SDK ordering and unique port-name plan. Nonselected direct Composite bodies remain protected. The profile uses an own EventGraph or function graph and one captured native Scene SCS root. SDK movement of selected nodes and owned BoundGraphs, diagnostics, partial-change reporting and requested compile/save policy remain guarded by the shared operation. Existing non-Composite selections and default flags remain unchanged. Wider native owners, generated/custom/latent/ref/const/object/struct/enum/container/wildcard signatures, split pins, deeper Composite bodies outside the bounded profile, wider selected CustomEvent template combinations, exec fan-out, external consumers, wider compound boundary types, broader hosts and null-self normalization remain valid original scopes not yet migrated. This does not evaluate runtime arithmetic or branching, prove cold persistence, provide atomic rollback or complete the original full contract.'
    @toolset_registry.tool_call
    @staticmethod
    def collapse_blueprint_nodes_to_function(blueprint_name: str, graph_name: str, function_name: str, node_ids: str,
            graph_type: str="event", expected_node_count: str="-1", allow_non_temp: bool=False, dry_run: bool=True,
            compile: bool=True, save_asset: bool=False) -> CubelessPythonOperationResult:
        'Function extraction additionally admits bounded nested stock Composite ownership closures through the existing SDK extraction writer. Each immediate SubGraphs set must correspond one-to-one to an exact owned Composite BoundGraph, with separate source Tunnel2/primitive native-pure/Branch/Sequence DAG checks and retained logical roles at every depth. SDK moves only selected top-level BoundGraphs from source to destination SubGraphs; descendant bodies retain their immediate Composite ownership. Existing partial/unknown, compile/save and no-retry contracts remain unchanged. Wider body classes/types/owners and untested deep variants remain incomplete or usage pending.\n\nThe existing CustomEvent+Composite profile additionally admits multiple selected data consumers. Installed SDK Entry names Data/Data2 and collision suffixes are predicted per created consumer, separately from source parameter count; peer exclusivity, budgets and current/retired-role checks remain. SDK disconnects and fresh defaults are reported as observed, without synthetic repair. Exec fan-out, external consumers/delegates and wider types remain incomplete. Actual new data fan-out extraction is usage validation pending.\n\nA direct native BlueprintFunctionLibrary additionally admits an explicit own FunctionGraph and the existing ordinary selected-node profile. Composite/CustomEvent combinations retain the Actor-only profile and fail before toolkit creation for libraries. Existing SDK static/world-context/self transitions, requested compile/save and actual partial-change reporting remain guarded; library extraction is implemented / usage validation pending. Guarded original Blueprint function extraction public10: Native validates one complete GUID selection, owns a real derived Blueprint Editor instance, diagnostic graphs and UI state, native selection extraction, requested compiler/save and actual partial or cleanup receipts. Even dry_run may create temporary diagnostic graphs; restoring dirty/status flags is not total SDK rollback. Original temp guard and explicit allow_non_temp remain. No uncertain retry or atomic rollback is provided. Official HTTP numeric tokens may already be rounded upstream. A bounded direct-native Actor Blueprint profile additionally admits one selection set containing multiple directly owned stock Composites and other Native-admitted ordinary stock nodes. Each Composite owns a distinct owned BoundGraph with exactly two complementary stock Tunnels. Its finite reciprocal acyclic body may contain multiple exact native KismetMathLibrary by-value primitive pure calls, stock Branch and ExecutionSequence nodes. Native snapshots actual function descriptors, primitive property layouts, authored purity override, pin defaults and ordered links before public type conversions; normalized hidden self retains the already-existing authoritative native owner CDO without creating one. Only the selected source-graph node GUIDs belong in node_ids; each Composite body stays owned by its Composite and its descendants are not flattened into the selection or expected_node_count. Links within the selected set are preserved, while crossing primitive by-value data and exec boundaries follow the actual SDK ordering and unique port-name plan. Nonselected direct Composite bodies remain protected. The same bounded direct-native Actor/bounded nested-Composite profile also admits one selected stock CustomEvent template with the existing finite primitive signature and exclusive selected consumers. The real SDK removes this template, reconnects its admitted exec consumers to Entry, and disconnects its data consumers while creating template-derived Entry user pins with new SDK defaults; source event defaults are not copied. These authored transitions, ordinary node count excluding the removed event and Composite SubGraphs ownership movement are guarded and reported together. Retired event/pin roles are not inspected after SDK removal. No synthetic default/link repair or atomic rollback is added. This mixed route is implemented / usage validation pending; exec fan-out, external/delegate links, name collisions and wider signatures remain incomplete. The profile uses an own EventGraph or function graph and one captured native Scene SCS root. SDK movement of selected nodes and owned BoundGraphs, diagnostics, partial-change reporting and requested compile/save policy remain guarded by the shared operation. Existing non-Composite selections and default flags remain unchanged. Wider native owners, generated/custom/latent/ref/const/object/struct/enum/container/wildcard signatures, split pins, deeper Composite bodies outside the bounded profile, broader selected CustomEvent template combinations, wider compound boundary types, broader hosts and null-self normalization remain valid original scopes not yet migrated. This does not evaluate runtime arithmetic or branching, prove cold persistence, provide atomic rollback or complete the original full contract.'
        rejected,report,raw=_guard_admission()
        if rejected is not None:return reflected_result(rejected)
        try:
            node_ids=_decode_nodes(node_ids)
            expected_node_count=_decode_integer(expected_node_count)
        except Exception as error:
            result=operation._rejected(operation.OperationError('blueprint_function_extraction_registration_carrier_invalid',str(error)))
            result['stage']='blueprint_function_extraction_registration_carrier_admission'
            result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.collapse_blueprint_nodes_to_function(blueprint_name,graph_name,function_name,node_ids,graph_type,expected_node_count,allow_non_temp,dry_run,compile,save_asset))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessBlueprintFunctionExtractionAttachmentLibrary', None), 'verify_blueprint_function_extraction_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Blueprint function extraction adapter raw-input admission helper is unavailable')
        raw = helper(CubelessBlueprintFunctionExtractionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_blueprint_function_extraction_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'collapse_blueprint_nodes_to_function'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Blueprint function extraction adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('blueprint_function_extraction_raw_guard_admission_missing', message))
    result['stage'] = 'blueprint_function_extraction_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError('Non-finite JSON is unsupported: '+value)

def _decode_nodes(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private node array JSON text required')
    value=json.loads(text,parse_constant=_invalid_constant)
    if type(value) is not list or not 1<=len(value)<=2048 or any(type(row) is not str for row in value):raise ValueError('node_ids requires1..2048 strings; Native owns GUID parsing')
    return value

def _decode_integer(text):
    """Decode private canonical int64; Native compares a nonnegative expected selection count."""
    if type(text) is not str or not text or len(text) > 20:
        raise ValueError("Private integer limit requires a bounded canonical decimal")
    value = int(text)
    if str(value) != text or value < -(1 << 63) or value > (1 << 63) - 1:
        raise ValueError("Private integer limit must be an exact canonical int64 decimal")
    return value
