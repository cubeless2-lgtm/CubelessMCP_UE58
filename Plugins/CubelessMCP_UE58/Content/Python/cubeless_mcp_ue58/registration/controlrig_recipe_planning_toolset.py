"""HG original ControlRig planning3; profile Engine leaf1, pure preset C++ work1 and Python composition own actual state."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import controlrig_recipe_planning as domain


@unreal.uclass()
class CubelessControlRigRecipePlanningTools(unreal.ToolsetDefinition):
    "Read-only ControlRig skeleton profile, deterministic preset compilation and seven preset recipe generators."
    @toolset_registry.tool_call
    @staticmethod
    def profile_controlrig_skeleton(request:str)->CubelessPythonOperationResult:
        "Read mesh-specific or Skeleton reference local pose and bounded parent chains, then derive deterministic alias and preset planning evidence."
        rejected,report,raw=_guard_admission("profile_controlrig_skeleton")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"request":_decode_value(request)}
        except Exception as error:return reflected_result(_carrier_rejected("profile_controlrig_skeleton",error,report,raw))
        return reflected_result(domain._named_mcp("profile_controlrig_skeleton",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def compile_controlrig_profile_recipe(profile:str,request:str)->CubelessPythonOperationResult:
        "Compile profile alias evidence and explicit overrides into a stable preset request; optionally generate recipe data without asset build, compilation or save."
        rejected,report,raw=_guard_admission("compile_controlrig_profile_recipe")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"profile":_decode_value(profile),"request":_decode_value(request)}
        except Exception as error:return reflected_result(_carrier_rejected("compile_controlrig_profile_recipe",error,report,raw))
        return reflected_result(domain._named_mcp("compile_controlrig_profile_recipe",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def make_controlrig_preset_recipe(preset:str)->CubelessPythonOperationResult:
        "Generate recipe JSON for seven approved ControlRig presets while preserving ordered hierarchy, pins, links, policies and validation failures."
        rejected,report,raw=_guard_admission("make_controlrig_preset_recipe")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"preset":_decode_value(preset)}
        except Exception as error:return reflected_result(_carrier_rejected("make_controlrig_preset_recipe",error,report,raw))
        return reflected_result(domain._named_mcp("make_controlrig_preset_recipe",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessControlRigRecipePlanningAttachmentLibrary', None), 'verify_controlrig_recipe_planning_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ControlRigRecipePlanning adapter raw-input admission helper is unavailable')
        raw = helper(CubelessControlRigRecipePlanningTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_controlrig_recipe_planning_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ControlRigRecipePlanning adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('controlrig_recipe_planning_raw_guard_admission_missing', message))
    result['stage'] = 'controlrig_recipe_planning_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('controlrig_recipe_planning_registration_carrier_invalid',str(error)))
    result['stage']='controlrig_recipe_planning_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
