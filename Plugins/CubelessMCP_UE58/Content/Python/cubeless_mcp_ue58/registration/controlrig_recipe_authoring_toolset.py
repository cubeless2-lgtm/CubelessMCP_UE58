"""HH original ControlRig recipe validation, dry-run and guarded authoring contracts."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import controlrig_recipe_authoring as domain


@unreal.uclass()
class CubelessControlRigRecipeAuthoringTools(unreal.ToolsetDefinition):
    "Validate, dry-run and build ControlRig recipes with ordered authoring and explicit partial failure receipts."
    @toolset_registry.tool_call
    @staticmethod
    def validate_controlrig_recipe(recipe:str)->CubelessPythonOperationResult:
        "Validate nested ControlRig recipe policies and return original planned/skipped writes, issues and compile/save intent without asset writes."
        rejected,report,raw=_guard_admission("validate_controlrig_recipe")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"recipe":_decode_value(recipe)}
        except Exception as error:return reflected_result(_carrier_rejected("validate_controlrig_recipe",error,report,raw))
        return reflected_result(domain._named_mcp("validate_controlrig_recipe",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def dry_run_controlrig_recipe(recipe:str)->CubelessPythonOperationResult:
        "Dry-run the same ControlRig validation contract with forced validation-only mode and original structured failure report."
        rejected,report,raw=_guard_admission("dry_run_controlrig_recipe")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"recipe":_decode_value(recipe)}
        except Exception as error:return reflected_result(_carrier_rejected("dry_run_controlrig_recipe",error,report,raw))
        return reflected_result(domain._named_mcp("dry_run_controlrig_recipe",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def build_controlrig_recipe(recipe:str,options:str="null")->CubelessPythonOperationResult:
        "Build or update a sample/temp ControlRig from an ordered recipe; preserve nullable options, authored parent rules, original partial report and actual compile/save history."
        rejected,report,raw=_guard_admission("build_controlrig_recipe")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"recipe":_decode_value(recipe),"options":_decode_value(options)}
        except Exception as error:return reflected_result(_carrier_rejected("build_controlrig_recipe",error,report,raw))
        return reflected_result(domain._named_mcp("build_controlrig_recipe",arguments))

    @toolset_registry.tool_call
    @staticmethod
    def build_controlrig_recipe_with_animbp_binding(recipe:str,options:str="null")->CubelessPythonOperationResult:
        """Validate the full recipe, build a binding-disabled Rig copy, then use existing sample AnimBP and optional input-default actions. Parent journal retains each actual partial/compile/save receipt; a prior node save does not prove later defaults persistence. No final save, rollback, server RPC, runtime/cold or full pipeline claim."""
        rejected,report,raw=_guard_admission("build_controlrig_recipe_with_animbp_binding")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"recipe":_decode_value(recipe),"options":_decode_value(options)}
        except Exception as error:return reflected_result(_carrier_rejected("build_controlrig_recipe_with_animbp_binding",error,report,raw))
        return reflected_result(domain._named_mcp("build_controlrig_recipe_with_animbp_binding",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessControlRigRecipeAuthoringAttachmentLibrary', None), 'verify_controlrig_recipe_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ControlRigRecipeAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessControlRigRecipeAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_controlrig_recipe_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ControlRigRecipeAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('controlrig_recipe_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'controlrig_recipe_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('controlrig_recipe_authoring_registration_carrier_invalid',str(error)))
    result['stage']='controlrig_recipe_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
