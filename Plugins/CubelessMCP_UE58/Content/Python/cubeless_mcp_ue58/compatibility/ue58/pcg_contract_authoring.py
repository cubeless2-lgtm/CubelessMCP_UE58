"""HE dedicated public EngineAPI2. Audit-based Read composition stays Python-only; no RPC/exec."""
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessPCGContractAuthoringLibrary'
BINDINGS=["audit_pcg_graph_contract","set_pcg_static_mesh_spawner_entries"]
PARAM_FIELDS={"audit_pcg_graph_contract":["graph_path","include_dependencies","forbidden_dependency_prefixes","fail_on_forbidden_dependencies"],"set_pcg_static_mesh_spawner_entries":["graph_path","node_id","mesh_entries","selector_type","attribute_name","material_override_attributes","save"]}
MAX_CARRIER_UTF16_UNITS=1048576


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS}


def _carrier(params):
    if type(params) is not dict:raise OperationError('pcg_contract_authoring_carrier_invalid','Object parameters required')
    try:
        text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,UnicodeError,OverflowError,RecursionError) as error:
        raise OperationError('pcg_contract_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:
        raise OperationError('pcg_contract_authoring_carrier_bound','Carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    function=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(function):
        error=OperationError('pcg_contract_authoring_native_api_unavailable','Exact HE Native binding is unavailable; no call dispatched')
        error.native_call_dispatched=False;raise error
    try:
        raw=function(text)
    finally:
        function=None  # Release the native bound callable before decode or exception propagation.
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('pcg_contract_authoring_native_result_invalid','Exact operation and typed changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result


def audit_pcg_graph_contract(params):return _invoke('audit_pcg_graph_contract',params)

def set_pcg_static_mesh_spawner_entries(params):return _invoke('set_pcg_static_mesh_spawner_entries',params)
