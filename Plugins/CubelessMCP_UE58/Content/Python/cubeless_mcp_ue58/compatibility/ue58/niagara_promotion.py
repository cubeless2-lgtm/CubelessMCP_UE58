"""UE5.8 dedicated NiagaraPromotion Native1 x2. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraPromotionLibrary'
BINDINGS=["audit_niagara_promotion_candidate", "promote_niagara_system_candidate"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"audit_niagara_promotion_candidate": ["system_path", "proposed_target_path", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_temp_target", "allow_temp_dependencies"], "promote_niagara_system_candidate": ["source_system_path", "target_system_path", "dry_run", "forbidden_dependency_prefixes", "forbidden_source_roots", "allow_non_temp_source", "allow_temp_target", "allow_temp_dependencies", "overwrite_existing", "save", "request_compile"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_promotion_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_promotion_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_promotion_native_api_unavailable','Build/load the exact NiagaraPromotion Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_promotion_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def audit_niagara_promotion_candidate(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("audit_niagara_promotion_candidate",text)

def promote_niagara_system_candidate(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("promote_niagara_system_candidate",text)
