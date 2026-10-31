"""Dedicated production operations; shared existing journal, no server coupling."""
from cubeless_mcp_ue58.compatibility.ue58 import animation_production as api
from . import editor_data

def run(kind,action,asset_path,arguments=None,reference_path='',execute=False,save=False,allow_dirty=False,allow_remove=False,job_id=''):
    try:
        editor_data._validate_path(asset_path)
        if not asset_path.startswith('/Game/'):raise ValueError('Explicit project asset path required')
        if any(type(v) is not bool for v in (execute,save,allow_dirty,allow_remove)):raise ValueError('Boolean options required')
        if reference_path:editor_data._validate_path(reference_path)
    except Exception as error:return editor_data._failure(action,error)
    dispatch=lambda:api.invoke(kind,asset_path,reference_path,action,arguments,execute,save,allow_dirty,allow_remove,job_id)
    return editor_data._writer(action,dict(asset_path=asset_path,arguments=arguments or {},save=save,job_id=job_id),dispatch) if execute or kind=='index' else dispatch()

def reject(action,error):return editor_data._failure(action,error)
