"""Read-only domain receipt; no engine work in the registration layer."""
from . import state
from ..compatibility.ue58 import production_inspection as api

def inspect_pcg_placement(actor_path,max_instances=10000):
    result=state.new_result()
    try:
        result['data']=api.pcg_placement(actor_path,max_instances)
        result.update(success=True,stage='complete')
        result['validation'].update(status='complete_actor_owned_mesh_readback',generation_completion='not_assessed')
    except Exception as error:
        result.update(success=False,stage='pcg_placement_observation',error_code='pcg_placement_observation_failed',error=str(error))
        result['data'].update(observation_complete=False,explicit_asset_mutation=False)
    result['retry'].update(automatic_retry=False,atomic_rollback=False)
    return result
