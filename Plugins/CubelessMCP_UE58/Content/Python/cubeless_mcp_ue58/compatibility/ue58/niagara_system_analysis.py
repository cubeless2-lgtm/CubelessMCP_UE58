"""UE5.8 Niagara system analysis dedicated Native1. Internal metadata normalization plus direct existing compatibility observations."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraSystemAnalysisLibrary'
BINDINGS=["analyze_niagara_system"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"analyze_niagara_system": ["system_path", "include_renderers", "include_user_parameters", "include_stack", "include_graph", "include_module_inputs", "include_compile_status", "include_pins", "include_links", "include_scratch_pads", "include_resolved_stack_inputs", "max_function_calls", "max_nodes_per_graph", "max_links_per_graph", "max_modules", "max_candidates_per_module", "max_resolved_inputs_per_module", "max_top_candidates"]}


CHILD_NATIVE_BINDINGS=(
 ('CubelessNiagaraRendererLibrary','inspect_niagara_renderers'),
 ('CubelessNiagaraParameterLibrary','inspect_niagara_user_parameters'),
 ('CubelessNiagaraAuthoringInspectionLibrary','inspect_niagara_stack'),
 ('CubelessNiagaraAuthoringInspectionLibrary','inspect_niagara_graph'),
 ('CubelessNiagaraInputParameterLibrary','inspect_niagara_module_inputs'),
 ('CubelessNiagaraAuthoringInspectionLibrary','inspect_niagara_compile_status'))

def binding_status():
    functions=_child_functions()
    return {BINDINGS[0]:callable(getattr(getattr(unreal,LIBRARY,None),'inspect_niagara_system_analysis_metadata',None))
        and all(callable(fn) for fn in functions.values())
        and all(callable(getattr(getattr(unreal,library,None),method,None)) for library,method in CHILD_NATIVE_BINDINGS)}



def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_system_analysis_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_system_analysis_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_system_analysis_native_api_unavailable','Build/load the exact NiagaraSystemAnalysis Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_system_analysis_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_metadata(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('inspect_niagara_system_analysis_metadata',text)

def _child_functions():
    from . import niagara_renderers,niagara_parameters,niagara_authoring_inspection,niagara_input_parameters
    return dict(renderers=getattr(niagara_renderers,'inspect_niagara_renderers',None),
        user_parameters=getattr(niagara_parameters,'inspect_niagara_user_parameters',None),
        stack=getattr(niagara_authoring_inspection,'inspect_niagara_stack',None),
        graph=getattr(niagara_authoring_inspection,'inspect_niagara_graph',None),
        module_inputs=getattr(niagara_input_parameters,'inspect_niagara_module_inputs',None),
        compile_status=getattr(niagara_authoring_inspection,'inspect_niagara_compile_status',None))

def inspect_section(section,params):
    fn=_child_functions().get(section)
    if not callable(fn):
        error=OperationError('niagara_system_analysis_child_unavailable','Exact existing compatibility leaf is unavailable: '+section)
        error.native_call_dispatched=False;raise error
    return fn(params['system_path'] if section=='user_parameters' else params)
