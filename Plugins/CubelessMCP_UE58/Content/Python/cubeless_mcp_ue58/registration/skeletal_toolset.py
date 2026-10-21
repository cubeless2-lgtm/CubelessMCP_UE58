"""Original Skeleton reads, socket lifecycle and raw-bone retargeting; editor-only."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import skeletal

import json
from cubeless_mcp_ue58.operations import skeletal_socket

def _socket_vector(carrier):
    if carrier is None:return None
    if type(carrier) not in (list,unreal.Array):raise ValueError('Exact reflected Array or list socket carrier required; no native dispatch')
    carrier=list(carrier)
    if len(carrier)>1 or any(type(v) is not str for v in carrier):
        raise ValueError('Exact internal socket whole-array carrier required')
    if not carrier:return None
    value=json.loads(carrier[0])
    if type(value) is not list:raise ValueError('Socket carrier requires a whole JSON array')
    return value
@unreal.uclass()
class CubelessSkeletalTools(unreal.ToolsetDefinition):
    """Skeleton/SkeletalMesh reads, Skeleton-owned socket Add/Set/Remove and bounded raw-bone retargeting. Native mutations report actual modify/forced-save and failure prefixes; no explicit compiler or automatic retry. Loads may have PostLoad/async/cache effects. Physics, cold/runtime and full original workflows remain incomplete."""
    @toolset_registry.tool_call
    @staticmethod
    def list_skeleton_bones(skeleton_path:str,bone_name_contains:str='',max_bones:int=500)->CubelessPythonOperationResult:
        """Original Skeleton reference hierarchy, including virtual bones, parent indices/names and translation-retargeting mode strings. Case-insensitive substring filter; limit clamped1..4096 after resolution. Finite native classes/bounds and two current point observations preserve the ordered legacy payload; not mesh-only bone topology or arbitrary-class/full-state parity."""
        return reflected_result(skeletal.list_skeleton_bones(skeleton_path,bone_name_contains,max_bones))
    @toolset_registry.tool_call
    @staticmethod
    def list_skeletal_sockets(skeleton_path:str)->CubelessPythonOperationResult:
        """Original USkeleton.Sockets array order, nulls skipped, names/bones and FVector/FRotator string transforms. SkeletalMesh input uses its Skeleton; mesh-only/active merged sockets intentionally excluded. Actual native load/read prefix and scoped disk/dirty proof retained; no mutation or compile/save."""
        return reflected_result(skeletal.list_skeletal_sockets(skeleton_path))

    @toolset_registry.tool_call
    @staticmethod
    def add_skeletal_socket(skeleton_path:str,socket_name:str,bone_name:str,location:list[str]=[],rotation:list[str]=[],scale:list[str]=[],save:bool=True)->CubelessPythonOperationResult:
        """Add one native skeleton-owned socket on an isolated copied /Game Skeleton. Public transforms are numeric array/null; omission/null uses defaults while explicit empty array is invalid. Original roll/pitch/yaw order, forced native save and actual partial prefix retained; no runtime attachment or full original class parity."""
        return reflected_result(skeletal_socket.add_skeletal_socket(skeleton_path,socket_name,bone_name,_socket_vector(location),_socket_vector(rotation),_socket_vector(scale),save))
    @toolset_registry.tool_call
    @staticmethod
    def set_skeletal_socket(skeleton_path:str,socket_name:str,bone_name:str='',new_socket_name:str='',location:list[str]=[],rotation:list[str]=[],scale:list[str]=[],save:bool=True)->CubelessPythonOperationResult:
        """Original matching/empty update still modifies and may save. Empty bone/rename retain current values; a same-name rename is an original collision failure. Public nullable numeric transforms use the exact whole-array carrier, preserving explicit empty-array refusal. Native receipts govern changes and forced-save failures."""
        return reflected_result(skeletal_socket.set_skeletal_socket(skeleton_path,socket_name,bone_name,new_socket_name,_socket_vector(location),_socket_vector(rotation),_socket_vector(scale),save))
    @toolset_registry.tool_call
    @staticmethod
    def remove_skeletal_socket(skeleton_path:str,socket_name:str,save:bool=True)->CubelessPythonOperationResult:
        """Remove the selected owned socket from Skeleton.Sockets, preserving native ordered lifecycle/save receipts. No explicit object destruction, compiler, Python second save or retry. Duplicate selected FName/custom/Engine-writer variants remain incomplete."""
        return reflected_result(skeletal_socket.remove_skeletal_socket(skeleton_path,socket_name,save))

    @toolset_registry.tool_call
    @staticmethod
    def set_bone_translation_retargeting(skeleton_path:str,bone_name:str,mode:str,recursive:bool=False,save:bool=True)->CubelessPythonOperationResult:
        """Original five modes and raw-child recursive mutation on an isolated project Skeleton. A public permission/type/count proof admits the installed unchecked setter; virtual/unbacked/custom variants remain incomplete. Matching still writes/marks dirty and native forced save is authoritative; no compiler or runtime animation evaluation claim."""
        from cubeless_mcp_ue58.operations import skeletal_retarget
        return reflected_result(skeletal_retarget.set_bone_translation_retargeting(skeleton_path,bone_name,mode,recursive,save))
