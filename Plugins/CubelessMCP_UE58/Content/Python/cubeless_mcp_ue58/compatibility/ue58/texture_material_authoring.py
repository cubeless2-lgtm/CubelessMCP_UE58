"""HJ Editor Python material composites and LOD0 polygon UV observation.
Public context/object checks are not atomic GC/exit/save admission. No transport.
"""
import hashlib
from typing import Dict, Any
import copy
import math
import os
import re
import tempfile
import threading
from pathlib import Path
import unreal
from .functions import _decode_result
from . import texture_material_png as png
from ...operations.state import OperationError
METHODS=('get_static_mesh_uv_layout','create_material_instance_with_texture','create_material_with_texture_set','import_create_and_apply_texture_set','generate_pbr_companion_textures','import_texture_to_unreal','import_texture_set_to_unreal','apply_material_to_mesh','generate_texture_from_prompt','generate_texture_for_mesh_uv','generate_and_apply_ai_texture')
_STOPPED=False
_SHUTDOWN=None
class Stop(OperationError):pass

def _shutdown():
    global _STOPPED
    _STOPPED=True

def _valid(obj):return obj is not None and unreal.SystemLibrary.is_valid(obj) is True

def _name(value,field,allow_empty=False):
    if type(value) is not str:raise OperationError('texture_material_name_invalid',field+' must be a string')
    try:n=len(value.encode('utf-16-le'))//2
    except UnicodeError as error:raise OperationError('texture_material_name_invalid',field+' has invalid Unicode') from error
    if n>=1024 or '\x00' in value or not allow_empty and not value:raise OperationError('texture_material_name_bound',field+' must be nonempty and below NAME_SIZE without embedded NUL')
    return value

def binding_status():
    common=not _STOPPED and all(callable(getattr(unreal,n,None)) for n in ('is_editor','get_editor_subsystem','register_python_shutdown_callback'))
    common=common and callable(getattr(getattr(unreal,'SystemLibrary',None),'is_valid',None)) and callable(getattr(getattr(unreal,'UnrealEditorSubsystem',None),'get_editor_world',None))
    load=callable(getattr(getattr(unreal,'EditorAssetLibrary',None),'load_asset',None))
    mesh=all(callable(getattr(getattr(unreal,'StaticMesh',None),n,None)) for n in ('get_static_mesh_description',)) and callable(getattr(getattr(unreal,'StaticMeshEditorSubsystem',None),'get_num_uv_channels',None))
    material=common and load and all(callable(getattr(getattr(unreal,'MaterialEditingLibrary',None),n,None)) for n in ('create_material_expression','set_material_instance_parent','set_material_instance_texture_parameter_value','get_material_instance_texture_parameter_value','connect_material_property','get_material_property_input_node','get_material_property_input_node_output_name','recompile_material','update_material_instance'))
    material=material and all(callable(getattr(unreal,n,None)) for n in ('find_object','ObjectIterator')) and callable(getattr(getattr(unreal,'AssetToolsHelpers',None),'get_asset_tools',None)) and callable(getattr(getattr(unreal,'EditorAssetLibrary',None),'save_loaded_asset',None))
    return dict(get_static_mesh_uv_layout=bool(common and load and mesh),create_material_instance_with_texture=bool(material),create_material_with_texture_set=bool(material),
        import_create_and_apply_texture_set=bool(material and all(callable(getattr(getattr(unreal,'CubelessTextureMaterialAttachmentLibrary',None),name,None)) for name in ('import_texture_file','apply_texture_material'))),
        generate_pbr_companion_textures=_pbr_dependencies_available(),
        import_texture_to_unreal=bool(common and callable(getattr(getattr(unreal,'CubelessTextureMaterialAttachmentLibrary',None),'import_texture_file',None))),
        import_texture_set_to_unreal=bool(common and callable(getattr(getattr(unreal,'CubelessTextureMaterialAttachmentLibrary',None),'import_texture_file',None))),
        apply_material_to_mesh=bool(common and callable(getattr(getattr(unreal,'CubelessTextureMaterialAttachmentLibrary',None),'apply_texture_material',None))),
        generate_texture_from_prompt=not _STOPPED,
        generate_texture_for_mesh_uv=bool(common and load and mesh),
        generate_and_apply_ai_texture=bool(common and load and mesh and all(callable(getattr(getattr(unreal,'EditorActorSubsystem',None),n,None)) for n in ('get_selected_level_actors','get_all_level_actors'))))

class _Anchor:
    def __init__(self,obj,world=None,owner=None,require_world=False):
        if not _valid(obj):raise Stop('texture_material_authoring_object_unavailable','Expected live loaded public UObject')
        self.obj=obj;self.shape=self.read();self.world=world;self.owner=owner;self.require_world=require_world;self.label=None
        if require_world and obj.get_world()!=world:raise Stop('texture_material_authoring_world_changed','Borrowed object is outside the captured Editor world')
        if owner is not None and obj.get_owner()!=owner:raise Stop('texture_material_authoring_owner_changed','Component owner differs')
    def read(self):
        obj=self.obj
        return (obj.get_path_name(),obj.get_class(),obj.get_outer(),obj.get_package(),obj.get_outermost())
    def same(self):
        if not _valid(self.obj) or not _valid(self.shape[1]) or self.read()!=self.shape:return False
        if self.require_world and self.obj.get_world()!=self.world:return False
        if self.require_world and (self.owner or self.obj).is_actor_being_destroyed():return False
        if self.owner is not None and (not _valid(self.owner) or self.obj.get_owner()!=self.owner or self.obj not in self.owner.get_components_by_class(unreal.ActorComponent)):return False
        if self.label is not None and self.obj.get_actor_label(False)!=self.label:return False
        return True

class _Context:
    def __init__(self,receipt,checkpoint):
        global _SHUTDOWN
        self.r=receipt;self.checkpoint=checkpoint;self.targets=[];self.lost=False;self.level=None;self.level_shape=None
        if _STOPPED or unreal.is_editor() is not True:raise Stop('texture_material_authoring_editor_unavailable','Editor Python lifecycle is unavailable')
        if _SHUTDOWN is None:_SHUTDOWN=unreal.register_python_shutdown_callback(_shutdown)
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not _valid(self.editor):raise Stop('texture_material_authoring_editor_unavailable','Exact Editor subsystem is unavailable')
        self.editor_shape=_Anchor(self.editor)
        self.world=self.editor.get_editor_world();self.world_shape=_Anchor(self.world) if self.world is not None else None
        receipt['execution_context']=dict(editor_path=self.editor_shape.shape[0],world_path=self.world_shape.shape[0] if self.world_shape else None)
        self.admit()
    def admit(self):
        if (self.r.get('RP_parent_composite') is True or self.r.get('image_handoff_parent') is True) and self.r.get('change_state_known') is False:
            self.lost=True;raise Stop('texture_material_authoring_context_lost','RP child boundary is unknown; metadata-only stop')
        if self.lost:raise Stop('texture_material_authoring_context_lost','Context loss is sticky; no further Engine call or replay')
        try:
            if _STOPPED or unreal.is_editor() is not True:raise Stop('texture_material_authoring_shutdown','Editor shutdown; metadata-only stop')
            if (not self.editor_shape.same() or unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)!=self.editor
                or self.editor.get_editor_world()!=self.world or self.world_shape is not None and not self.world_shape.same()
                or self.level_shape is not None and (not self.level_shape.same() or unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)!=self.level or self.level.is_in_play_in_editor())
                or any(not target.same() for target in self.targets)):
                raise Stop('texture_material_authoring_context_changed','Exact Editor/world/loaded target class/outer/package/path/owner changed')
            if _STOPPED:raise Stop('texture_material_authoring_shutdown','Shutdown during public context observation')
        except Exception as error:
            self.lost=True;self.r.update(change_state_known=False,context_lost=True,post_failure_live_observation_skipped=True,do_not_retry=True)
            if isinstance(error,Stop):raise
            raise Stop('texture_material_authoring_context_changed',str(error)) from error
    def writer_admit(self):
        self.admit()
        if self.level_shape is None:
            level_type=getattr(unreal,'LevelEditorSubsystem',None)
            if level_type is None:raise Stop('texture_material_authoring_idle_context_unavailable','Public LevelEditorSubsystem is required before writer entry')
            self.level=unreal.get_editor_subsystem(level_type);self.level_shape=_Anchor(self.level)
        if self.level.is_in_play_in_editor():raise Stop('texture_material_authoring_PIE_unsupported','Stop PIE/SIE before utility writes; no writer replay')
        self.admit()
    def publish(self,stage):
        self.r['stage']=stage
        if self.checkpoint:
            try:self.checkpoint(self.r)
            except Exception as error:
                raise Stop('texture_material_authoring_checkpoint_failed','Stop before next Engine work: '+str(error)) from error
            self.admit()
    def call(self,stage,function,writer=False,save=False,on_return=None,on_enter=None,compile=False,before_dispatch=None):
        self.admit()
        if writer or save:self.writer_admit()
        if writer or save or compile:
            self.r['next_SDK_stage']=stage;self.publish('before_texture_material_authoring_checkpoint')
        self.admit()
        # Only the exact create/save admissions are repeated after the parent callback.
        if before_dispatch:before_dispatch();self.admit()
        count=self.r.get('actual_call_count',0)
        if count>=262144:raise Stop('texture_material_authoring_budget','Bounded call budget exhausted; prefix retained')
        self.r['actual_call_count']=count+1
        counts=self.r.setdefault('actual_call_counts',{});counts[stage]=counts.get(stage,0)+1
        row=dict(stage=stage,entered=True,returned=False,explicit_mutation=writer,explicit_save=save)
        prefix=self.r['actual_call_prefix']
        if len(prefix)>=128:
            prefix.pop(64);self.r['actual_call_prefix_omitted_count']=self.r.get('actual_call_prefix_omitted_count',0)+1
        prefix.append(row)  # Canonical first64+last64 diagnostic, not a renamed raw receipt.
        self.r['SDK_global_purity_unverified']=True
        self.r['mutation_attempted']|=writer;self.r['save_attempted']|=save;self.r['compile_attempted']|=compile
        if on_enter:on_enter()
        try:
            value=function();row['returned']=True
            if on_return:on_return(value)
        except Exception as error:
            row.update(raised=True,error=str(error)[:4096],error_UTF16_chars=len(str(error).encode('utf-16-le',errors='surrogatepass'))//2)
            if writer or save or compile:self.r['change_state_known']=False
            self.admit()
            if writer or save or compile:self.publish('returned_texture_material_authoring_operation')
            raise
        self.admit()
        if writer or save or compile:self.publish('returned_texture_material_authoring_operation')
        return value
    def pin(self,obj,owner=None,require_world=False):
        self.admit();target=_Anchor(obj,self.world,owner,require_world);self.targets.append(target);self.admit();return target
    def release_targets(self):
        self.admit();self.targets=[]

PARAMETER_NAMES={
 'base_color':['BaseColor','Base Color','BaseColorTexture','Base_Color','Albedo','Diffuse','Texture'],
 'normal':['Normal','NormalMap','NormalTexture','Normal_Map'],
 'roughness':['Roughness','RoughnessMap','RoughnessTexture'],
 'ambient_occlusion':['AO','AmbientOcclusion','Ambient Occlusion','Occlusion'],
 'metallic':['Metallic','Metalness','MetallicMap','MetallicTexture']}
PROPERTIES={'base_color':'MP_BASE_COLOR','normal':'MP_NORMAL','roughness':'MP_ROUGHNESS','ambient_occlusion':'MP_AMBIENT_OCCLUSION','metallic':'MP_METALLIC'}
OUTPUTS={'base_color':'','normal':'','roughness':'R','ambient_occlusion':'R','metallic':'R'}
READBACK_OUTPUTS={'base_color':'RGB','normal':'RGB','roughness':'R','ambient_occlusion':'R','metallic':'R'}
POSITIONS={'base_color':(-640,-240),'normal':(-640,-60),'roughness':(-640,120),'ambient_occlusion':(-640,300),'metallic':(-640,480)}


def _normalize_texture_set(texture_asset_paths):
    aliases={'basecolor':'base_color','base_color':'base_color','albedo':'base_color','diffuse':'base_color','normal':'normal','normal_map':'normal','roughness':'roughness','ao':'ambient_occlusion','ambient_occlusion':'ambient_occlusion','ambientocclusion':'ambient_occlusion','metallic':'metallic','metalness':'metallic'}
    normalized={}
    for key,value in texture_asset_paths.items():
        if not value:continue
        slot=aliases.get(str(key).strip().lower())
        if slot:normalized[slot]=str(value).strip()
    return normalized


def _own(obj,root):
    current=obj
    for unused in range(64):
        if current==root:return True
        if not _valid(current):return False
        current=current.get_outer()
    return False


def _package_owned(ctx,asset):
    ctx.admit();package=asset.get_package();count=0
    for obj in unreal.ObjectIterator():
        count+=1
        if count>262144:raise Stop('texture_material_object_budget','Public object scan exceeds262144; no save')
        if not _valid(obj):continue
        if obj==package:continue
        if obj.get_package()==package or obj.get_outermost()==package:
            if obj.get_package()!=package or obj.get_outermost()!=package or not _own(obj,asset):
                ctx.r['foreign_package_export']=dict(path=obj.get_path_name(),class_name=obj.get_class().get_name(),outer=obj.get_outer().get_path_name() if obj.get_outer() else None)
                raise Stop('texture_material_foreign_package_export','Only the exact newly created material subtree may be saved')
    ctx.admit()


def _fresh(ctx,package,object_path):
    # Never LoadAsset an absent target: that may create an empty package.
    exists=ctx.call('TargetAssetExists',lambda:unreal.EditorAssetLibrary.does_asset_exist(package))
    memory=ctx.call('FindTargetPackage',lambda:unreal.find_object(None,package))
    obj=ctx.call('FindTargetObject',lambda:unreal.find_object(None,object_path))
    registry=ctx.call('GetAssetRegistry',lambda:unreal.AssetRegistryHelpers.get_asset_registry())
    rows=ctx.call('TargetRegistryAllEntries',lambda:list(registry.get_assets_by_package_name(package,False)))
    if exists or memory is not None or obj is not None or rows:
        raise Stop('texture_material_target_exists','New owned material only; existing target/package is not overwritten')
    # Re-read memory/disk after the last registry callback before creation.
    if unreal.find_object(None,package) is not None or unreal.find_object(None,object_path) is not None or unreal.EditorAssetLibrary.does_asset_exist(package):
        raise Stop('texture_material_target_exists','Target appeared during admission; no overwrite or replay')
    ctx.admit()


def _load(ctx,path,expected=None):
    _name(path,'asset_path')
    # EditorAssetLibrary expands short package paths to package.object before FName lookup.
    # Export-text/quoted aliases are not parsed speculatively; refuse those bounded variants.
    if "'" in path or '"' in path or ':' in path or not path.startswith('/'):
        raise Stop('texture_material_asset_path_invalid','Canonical package or object asset path required')
    expanded=path if '.' in path.rsplit('/',1)[-1] else path+'.'+path.rsplit('/',1)[-1]
    _name(expanded,'expanded_asset_object_path')
    obj=ctx.call('LoadAsset',lambda:unreal.EditorAssetLibrary.load_asset(_name(path,'asset_path')),
        on_return=lambda obj:ctx.r['actual_call_prefix'][-1].update(nonnull_return=obj is not None))
    if obj is None:return None
    ctx.pin(obj)
    if expected is not None and not isinstance(obj,expected):raise Stop('texture_material_asset_type','Loaded asset has wrong typed class')
    return obj


def _save(ctx,asset):
    if ctx.r['save_attempted']:raise Stop('texture_material_save_not_replayed','A Save SDK dispatch already occurred; inspect its actual prefix, never resave automatically')
    _package_owned(ctx,asset)
    def returned(value):
        ctx.r['saved']|=value is True
        ctx.r['actual_call_prefix'][-1]['saved_API_true']=value is True
    try:saved=ctx.call('SaveLoadedMaterial',lambda:unreal.EditorAssetLibrary.save_loaded_asset(asset),writer=True,save=True,on_return=returned,before_dispatch=lambda:_package_owned(ctx,asset))
    except Stop:raise
    except Exception as error:
        raise Stop('texture_material_save_outcome_unknown','Save SDK raised after dispatch; actual prefix retained, no second save: '+str(error)) from error
    if saved is not True:raise Stop('texture_material_save_failed','Original save returned false; created asset and actual prefix retained')
    _package_owned(ctx,asset)
    # Public Python package dirty access is not required; do not synthesize a clean/cold result.
    ctx.r['physical_file_write_proven']=False


def _verify_material(ctx,asset,base,textures,expressions,attempted_parameters):
    ctx.admit()
    if base is not None:
        parent=ctx.call('ReadMaterialInstanceParent',lambda:asset.get_editor_property('parent'))
        if parent!=base:raise Stop('texture_material_parent_readback','Current instance parent differs; no retarget')
        observations=[]
        for slot,names in attempted_parameters.items():
            for name in names:
                actual=ctx.call('ReadTextureParameter',lambda name=name:unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(asset,name))
                observations.append(dict(slot=slot,parameter=name,matches_requested=actual==textures[slot],actual_path=actual.get_path_name() if _valid(actual) else None))
        ctx.r['parameter_readback']=observations
        # Original alias setters are best effort and append any nonexception call.
        # UE5.8's bool return is always false; neither alias list nor bool proves a match.
        ctx.r['parameter_matches_verified']=sum(row['matches_requested'] for row in observations)
    else:
        if ctx.call('ReadMaterialDomain',lambda:asset.get_editor_property('material_domain'))!=unreal.MaterialDomain.MD_SURFACE or ctx.call('ReadUseMaterialAttributes',lambda:asset.get_editor_property('use_material_attributes')) is not False:
            raise Stop('texture_material_domain_changed','Only the authored default surface-property material is admitted')
        for slot,expression in expressions.items():
            if not _valid(expression) or expression.get_outer()!=asset:raise Stop('texture_material_expression_owner','Authored expression left exact material')
            texture=ctx.call('ReadExpressionTexture',lambda:expression.get_editor_property('texture'))
            x=ctx.call('ReadExpressionX',lambda:expression.get_editor_property('material_expression_editor_x'))
            y=ctx.call('ReadExpressionY',lambda:expression.get_editor_property('material_expression_editor_y'))
            property=getattr(unreal.MaterialProperty,PROPERTIES[slot])
            source=ctx.call('ReadMaterialPropertyInput',lambda:unreal.MaterialEditingLibrary.get_material_property_input_node(asset,property))
            output=ctx.call('ReadMaterialPropertyOutput',lambda:unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(asset,property))
            if texture!=textures[slot] or (int(x),int(y))!=ctx.r['expected_positions'][slot] or source!=expression or output!=READBACK_OUTPUTS[slot]:
                raise Stop('texture_material_graph_readback','Requested texture/position/property/output is not current')
    ctx.admit()


def _material(ctx,args,single):
    r=ctx.r;warnings=[];created={};expressions={};applied={}
    paths={'base_color':args['texture_asset_path']} if single else _normalize_texture_set({
        'base_color':args['base_color_texture_asset_path'],'normal':args['normal_texture_asset_path'],
        'roughness':args['roughness_texture_asset_path'],'ambient_occlusion':args['ambient_occlusion_texture_asset_path'],'metallic':args['metallic_texture_asset_path']})
    if not single and not paths.get('base_color'):return dict(success=False,message='base_color texture asset path is required',error_code='missing_base_color_texture',texture_asset_paths=paths)
    textures={};missing=[];folder=args['unreal_folder'];name=args['material_name'];base_path=args['base_material_path'];base=None
    def folder_valid():return folder=='/Game' or folder.startswith('/Game/')
    # Preserve single-texture load-before-folder vs PBR folder-before-load order.
    if not single and not folder_valid():return dict(success=False,message='unreal_folder must be /Game or a child of /Game')
    for slot,path in paths.items():
        texture=_load(ctx,path,unreal.Texture2D)
        if texture is None:missing.append(dict(slot=slot,texture_asset_path=path))
        else:textures[slot]=texture
    if missing:
        return dict(success=False,message='Texture asset not found: '+paths['base_color']) if single else dict(success=False,message='Texture asset(s) not found',missing_textures=missing)
    if not folder_valid():return dict(success=False,message='unreal_folder must be /Game or a child of /Game')
    _name(folder,'unreal_folder');_name(name,'material_name')
    if re.search(r'[/\\.:]',name) or not name.strip() or '..' in folder.split('/') or folder.endswith('/'):
        raise Stop('texture_material_target_path_invalid','Exact simple asset name and package folder required')
    package=folder+'/'+name;object_path=package+'.'+name;_name(object_path,'expanded_object_path')
    r.update(requested_asset_path=package,requested_object_path=object_path)
    ctx.call('MakeMaterialDirectory',lambda:unreal.EditorAssetLibrary.make_directory(folder),writer=True)
    tools=ctx.call('GetAssetTools',lambda:unreal.AssetToolsHelpers.get_asset_tools())
    if base_path:
        base=_load(ctx,base_path,unreal.MaterialInterface)
        if base is None:return dict(success=False,message='Base material not found: '+base_path)
    factory_class=unreal.MaterialInstanceConstantFactoryNew if base is not None else unreal.MaterialFactoryNew
    asset_class=unreal.MaterialInstanceConstant if base is not None else unreal.Material
    factory=ctx.call('CreateTransientMaterialFactory',lambda:factory_class(),writer=True);ctx.pin(factory)
    _fresh(ctx,package,object_path)
    def created_return(asset):
        r['created_asset_returned_nonnull']=asset is not None
        r['actual_call_prefix'][-1]['nonnull_return']=asset is not None
        if asset is not None:r['observed_changed']=True
    asset=ctx.call('CreateMaterialAsset',lambda:tools.create_asset(name,folder,asset_class,factory),writer=True,on_return=created_return,before_dispatch=lambda:_fresh(ctx,package,object_path))
    if asset is None:raise Stop('texture_material_create_failed','Factory returned null; requested package/cache residual is not automatically removed')
    anchor=ctx.pin(asset)
    if not isinstance(asset,asset_class) or anchor.shape[0]!=object_path or anchor.shape[3].get_path_name()!=package or anchor.shape[4]!=anchor.shape[3] or anchor.shape[2]!=anchor.shape[3]:
        raise Stop('texture_material_created_identity','Returned material is not exact requested path/class/package/outer')
    _package_owned(ctx,asset)
    leaf=dict(success=False,asset_path=package);r['legacy_result']=leaf
    if base is not None:
        ctx.call('SetMaterialInstanceParent',lambda:unreal.MaterialEditingLibrary.set_material_instance_parent(asset,base),writer=True)
        for slot,texture in textures.items():
            applied[slot]=[]
            for parameter_name in PARAMETER_NAMES[slot]:
                try:
                    returned=ctx.call('SetTextureParameter',lambda parameter_name=parameter_name,texture=texture:unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(asset,parameter_name,texture),writer=True,
                        on_return=lambda value:r['actual_call_prefix'][-1].update(returned_bool=value,return_bool_is_success_proof=False))
                    applied[slot].append(parameter_name)
                except Stop:raise
                except Exception as error:
                    r['parameter_errors'].append(dict(slot=slot,parameter=parameter_name,message=str(error)))
            if not applied[slot]:warnings.append('Created the material instance but did not find a matching texture parameter on the base material.' if single else 'No matching texture parameter found on base material for '+slot+'.')
        ctx.call('UpdateMaterialInstance',lambda:unreal.MaterialEditingLibrary.update_material_instance(asset),writer=True)
        _verify_material(ctx,asset,base,textures,expressions,applied)
        _save(ctx,asset)
        leaf.update(success=True,object_path=object_path,class_name=asset.get_class().get_name(),base_material_path=base_path,applied_parameters=applied['base_color'] if single else applied,warnings=warnings)
    else:
        errors=[]
        try:
            for slot,texture in textures.items():
                position=(-384,0) if single else POSITIONS[slot];r['expected_positions'][slot]=position
                expression=ctx.call('CreateTextureExpression',lambda position=position:unreal.MaterialEditingLibrary.create_material_expression(asset,unreal.MaterialExpressionTextureSample,*position),writer=True,
                    on_return=lambda obj:r['actual_call_prefix'][-1].update(nonnull_return=obj is not None))
                if not _valid(expression):raise RuntimeError('Texture expression creation returned null')
                ctx.pin(expression)
                if expression.get_outer()!=asset:raise Stop('texture_material_expression_owner','New expression not owned by exact material')
                ctx.call('SetExpressionTexture',lambda:expression.set_editor_property('texture',texture),writer=True)
                expressions[slot]=expression;created[slot]=expression.get_name()
                try:
                    connected=ctx.call('ConnectMaterialProperty',lambda:unreal.MaterialEditingLibrary.connect_material_property(expression,OUTPUTS[slot],getattr(unreal.MaterialProperty,PROPERTIES[slot])),writer=True,
                        on_return=lambda value:r['actual_call_prefix'][-1].update(accepted=value))
                    if connected is not True:errors.append(dict(slot=slot,message='connect_material_property returned false'))
                except Stop:raise
                except Exception as error:errors.append(dict(slot=slot,message=str(error)))
            if errors:
                leaf.update(message='Material asset was created but one or more texture hookups failed',connection_errors=errors,created_expressions=created,mutation_performed=True,partial_mutation=True)
                if single:_save(ctx,asset)  # Single BaseColor exception path originally saves its partial asset.
                return leaf
            r['compile_requested']=True
            compile_result=ctx.call('RecompileMaterial',lambda:unreal.MaterialEditingLibrary.recompile_material(asset),writer=True,compile=True,
                on_return=lambda value:r.update(material_compile_errors=[str(e) for e in (value or [])]))
            _verify_material(ctx,asset,None,textures,expressions,applied)
            _save(ctx,asset)
            _verify_material(ctx,asset,None,textures,expressions,applied)
            if compile_result:
                raise Stop('texture_material_compile_errors','Material compiler reported errors; actual saved history retained')
            leaf.update(success=True,object_path=object_path,class_name=asset.get_class().get_name(),base_material_path='',warnings=warnings)
            if not single:leaf['created_expressions']=created
        except Stop:raise
        except Exception as error:
            leaf.update(success=False,message='Material asset was created but '+('BaseColor' if single else 'PBR texture')+' hookup failed: '+str(error),mutation_performed=True,partial_mutation=True)
            if not single:leaf['created_expressions']=created
            # Original exception path saves its partial asset, only in a still-admitted context.
            _save(ctx,asset);return leaf
    if not single:leaf['texture_asset_paths']=paths
    _verify_material(ctx,asset,base,textures,expressions,applied)
    return leaf


def _uv(ctx,args):
    mesh_path=_name(args['mesh_path'],'mesh_path');channel=int(args['uv_channel'])
    mesh=_load(ctx,mesh_path)
    if mesh is None:return dict(success=False,message='Static Mesh not found: '+mesh_path)
    if mesh.get_class().get_name()!='StaticMesh':return dict(success=False,message='Asset is not a StaticMesh: '+mesh_path,class_name=mesh.get_class().get_name())
    subsystem=ctx.call('GetStaticMeshEditorSubsystem',lambda:unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem));ctx.pin(subsystem)
    try:channels=ctx.call('GetNumUVChannels',lambda:subsystem.get_num_uv_channels(mesh,0))
    except Stop:raise
    except Exception:channels=ctx.call('GetNumUVChannelsFallback',lambda:unreal.EditorStaticMeshLibrary.get_num_uv_channels(mesh,0))
    if channel<0 or channel>=channels:return dict(success=False,message='UV channel '+str(channel)+' is out of range. Mesh has '+str(channels)+' UV channel(s).',num_uv_channels=channels)
    desc=ctx.call('GetStaticMeshDescriptionLOD0',lambda:mesh.get_static_mesh_description(0));ctx.pin(desc)
    polygons=[];min_u=min_v=999999.0;max_u=max_v=-999999.0;corners=0;skipped=0
    count=ctx.call('GetPolygonCount',lambda:desc.get_polygon_count())
    if type(count) is not int or count<0 or count>65536:raise Stop('texture_material_UV_polygon_budget','Source polygon count outside0..65536; PNG not started')
    for index in range(count):
        try:
            polygon_id=unreal.PolygonID(index)
            if not ctx.call('IsPolygonValid',lambda:desc.is_polygon_valid(polygon_id)):continue
            instances=list(ctx.call('GetPolygonVertexInstances',lambda:desc.get_polygon_vertex_instances(polygon_id)))
            corners+=len(instances)
            if corners>262144:raise Stop('texture_material_UV_corner_budget','UV corner/call budget exceeded; PNG not started')
            points=[]
            for instance in instances:
                uv=ctx.call('GetVertexInstanceUV',lambda:desc.get_vertex_instance_uv(instance,channel));u=float(uv.x);v=float(uv.y)
                if not math.isfinite(u) or not math.isfinite(v):raise Stop('texture_material_UV_nonfinite','Nonfinite UV component; PNG not started')
                min_u=min(min_u,u);min_v=min(min_v,v);max_u=max(max_u,u);max_v=max(max_v,v);points.append([u,v])
            if len(points)>=2:polygons.append(points)
        except Stop:raise
        except Exception as error:
            skipped+=1  # Original individual polygon failures are skipped, not triangulated/reindexed.
            if len(ctx.r['UV_polygon_errors'])<16:ctx.r['UV_polygon_errors'].append(dict(index=index,error=str(error)))
    ctx.r.update(UV_source_polygon_count=count,UV_polygons_skipped=skipped,UV_corner_count=corners)
    output=args['output_path']
    if not output:
        safe_mesh=Path(mesh_path.replace('/','_').replace('\\','_')).name.strip('_') or 'StaticMesh'
        output=os.path.join(tempfile.gettempdir(),safe_mesh+'_UV'+str(channel)+'.png')
    if type(output) is not str or '\x00' in output or len(output)>32767:raise Stop('texture_material_PNG_path_invalid','Bounded local PNG path required')
    ctx.admit();ctx.writer_admit();ctx.publish('before_texture_material_authoring_checkpoint');ctx.admit()
    path=Path(output).expanduser().resolve();ctx.r.update(PNG_target_existed_before=path.exists(),PNG_existing_file_overwrite_allowed=True,stage='WriteUVPNG',mutation_attempted=True,PNG_write_attempted=True,PNG_output_path=str(path))
    try:rendered=png.render_uv_layout_png(polygons,str(path),image_size=1024,transparent=True)
    except Exception:
        ctx.r.update(change_state_known=False,PNG_outcome_unknown=True);raise
    ctx.r.update(PNG_write_returned=True,observed_changed=True)
    # Bound current file read, validate bytes written without labelling a transformed value as raw.
    with path.open('rb') as handle:
        data=handle.read(8*1024*1024+1)
    if len(data)>8*1024*1024 or not data.startswith(b'\x89PNG\r\n\x1a\n') or not data.endswith(b'IEND\xaeB\x60\x82'):
        raise Stop('texture_material_PNG_readback','Current PNG failed bounded header/end readback')
    ctx.r.update(PNG_file_bytes=len(data),PNG_current_file_readback_passed=True)
    ctx.admit();ctx.publish('returned_texture_material_authoring_operation')
    return dict(success=True,mesh_path=mesh_path,mesh_name=mesh.get_name(),uv_channel=channel,num_uv_channels=channels,polygon_count=len(polygons),
        uv_bounds=dict(min_u=min_u,min_v=min_v,max_u=max_u,max_v=max_v),output_path=rendered['output_path'],image_size=rendered['image_size'],line_segment_count=rendered['line_segment_count'])


def execute(method,args,receipt,checkpoint=None):
    if method in _IMAGE_HANDOFF_METHODS:return _execute_image_handoff(method,args,receipt,checkpoint)
    if method in ('import_texture_to_unreal','import_texture_set_to_unreal','apply_material_to_mesh'):return _execute_texture_api(method,args,receipt,checkpoint)
    if method=='generate_pbr_companion_textures':return _execute_pbr_files(args,receipt,checkpoint)
    if method=='import_create_and_apply_texture_set':return _execute_import_composite(args,receipt,checkpoint)
    receipt.update(operation=method,stage='preflight',mutation_attempted=False,observed_changed=False,change_state_known=True,
        compile_requested=method!='get_static_mesh_uv_layout' and not bool(args.get('base_material_path')),compile_attempted=False,
        save_requested=method!='get_static_mesh_uv_layout',save_attempted=False,saved=False,SDK_global_purity_unverified=False,
        operation_complete=False,automatic_retry_permitted=False,actual_call_prefix=[],parameter_errors=[],parameter_readback=[],expected_positions={},requested_output_names=dict(OUTPUTS),expected_output_readback_names=dict(READBACK_OUTPUTS),UV_polygon_errors=[],
        physical_file_write_proven=False,shader_or_GPU_completion_proven=False,atomic_rollback=False,
        public_Python_GC_exit_save_atomic_admission_proven=False,automatic_cleanup_performed=False,implicit_factory_or_instance_compile_possible=True)
    try:
        for key,value in args.items():
            if type(value) is str and key!='output_path':_name(value,key,allow_empty=True)
        ctx=_Context(receipt,checkpoint)
        leaf=_uv(ctx,args) if method=='get_static_mesh_uv_layout' else _material(ctx,args,method=='create_material_instance_with_texture')
        receipt['legacy_result']=leaf;ctx.admit();receipt['operation_complete']=True
        # A caught callback exception can leave wider state unknown; never convert it into retry-safe success.
        if not receipt['change_state_known']:raise Stop('texture_material_partial_unknown','Original per-item failures retained; current scoped outcome is not fully known')
        return receipt
    except Exception as error:
        receipt.update(error_code=getattr(error,'code','texture_material_engine_call_failed'),error=str(error),do_not_retry=receipt['mutation_attempted'] or receipt['save_attempted'] or not receipt['change_state_known'])
        receipt.setdefault('legacy_result',dict(success=False,message=str(error),mutation_performed=receipt['observed_changed'],partial_mutation=receipt['mutation_attempted']))
        if receipt['mutation_attempted'] or receipt['save_attempted']:receipt['change_state_known']=False
        return receipt


def _import_name(value):
    # Original output-name rules, including dots/hyphens and the empty fallback.
    name=re.sub(r'[^A-Za-z0-9_.-]+','_',value.strip()).strip('._')
    return name or 'AI_Texture'


def _native_import_apply(ctx,method,*args):
    """Decode/adopt the fixed Native boundary before any returned-object/current getter."""
    ctx.writer_admit();ctx.publish('before_texture_material_authoring_checkpoint');ctx.admit()
    function=getattr(getattr(unreal,'CubelessTextureMaterialAttachmentLibrary',None),method,None)
    if not callable(function):raise Stop('texture_material_private_leaf_unavailable','Fixed private import/apply leaf is unavailable; no fallback or replay')
    row=dict(stage=method,entered=False,returned=False,raw_result=None,receipt_decoded=False)
    ctx.r['native_call_prefix'].append(row)
    native=None
    previous_attempted=ctx.r['mutation_attempted']
    try:
        row['entered']=True
        ctx.r['mutation_attempted']=True  # Conservative dispatch prefix until the typed receipt is decoded.
        ctx.r['SDK_global_purity_unverified']=True
        try:raw=function(*args)
        finally:function=None
        row.update(returned=True,raw_result=raw)
        native=_decode_result(raw)
        row.update(receipt_decoded=True,result=copy.deepcopy(native))
        # Preserve actual returned prefixes even when the subsequent current boundary fails.
        ctx.r['mutation_attempted'] = previous_attempted or native['mutation_attempted']
        ctx.r['observed_changed'] |= native.get('changed') is True
        for key in ('compile_requested','compile_attempted','save_requested','save_attempted','saved'):
            ctx.r[key] |= native[key]
        if not native['change_state_known']:
            ctx.r.update(change_state_known=False,do_not_retry=True,post_failure_live_observation_skipped=True)
            ctx.lost=True
        if native['success'] and (not native['change_state_known'] or not native['postcondition_passed']):
            raise Stop('texture_material_private_leaf_unknown','Private leaf success without a known postcondition; metadata-only stop')
        if native['change_state_known']:ctx.admit()
        return native
    except Exception as error:
        if native is None or native.get('success') is True:
            ctx.r.update(change_state_known=False,do_not_retry=True,post_failure_live_observation_skipped=True)
            ctx.lost=True
        row['error']=str(error)
        raise


def _native_legacy(ctx,native):
    leaf=native['data'].get('legacy_result')
    if type(leaf) is not dict:
        if native['success'] is False:
            return dict(success=False,message=native.get('error') or 'Native import/apply guard rejected the request',error_code=native.get('error_code') or 'texture_material_private_rejected')
        ctx.r.update(change_state_known=False,do_not_retry=True,post_failure_live_observation_skipped=True);ctx.lost=True
        raise Stop('texture_material_private_legacy_missing','Fixed private leaf must return the copied original utility payload; no synthetic success')
    return copy.deepcopy(leaf)


def _composite_failure(ctx,message,code,stage,completed,failed_result):
    unknown=ctx.r.get('change_state_known') is not True
    partial=bool(completed) or ctx.r['observed_changed'] or failed_result.get('mutation_performed') is True or failed_result.get('partial_mutation') is True
    fields=dict(success=False,message=message,error_code=code,stage=stage,logs=copy.deepcopy(ctx.r['composite_logs']),
        unreal_response=copy.deepcopy(failed_result),failed_stage=stage,failed_stage_effect='mutation',
        mutation_attempted=ctx.r['mutation_attempted'],mutation_performed=True if partial else None if unknown else False,
        partial_mutation=partial,mutation_state_unknown=unknown,do_not_retry=unknown or partial)
    if completed:fields.update(completed_mutation_count=len(completed),completed_mutation_stages=list(completed))
    if failed_result.get('mutation_performed') is True or failed_result.get('partial_mutation') is True:
        fields['failed_stage_mutation_performed']=True
    if unknown:fields['command_may_still_be_in_flight']=failed_result.get('command_may_still_be_in_flight') is True
    if failed_result.get('workflow_recovery_required') is True:
        fields.update(workflow_recovery_required=True,do_not_retry=True)
        for key in ('recovery_steps','recovery_guidance','restore_errors'):
            if key in failed_result:fields[key]=copy.deepcopy(failed_result[key])
    return fields


def _import_composite(ctx,args):
    logs=ctx.r['composite_logs'];completed=ctx.r['completed_mutation_stages']
    texture_name=_import_name(args['output_name'])
    material_name=('M_'+texture_name[2:] if texture_name.startswith('T_') else texture_name if texture_name.startswith(('MI_','M_')) else 'M_'+texture_name)
    folder=args['unreal_folder'].rstrip('/')
    imported=_import_texture_set(ctx,args,folder+'/Textures',texture_name)
    logs.append(dict(stage='import_texture_set',result=copy.deepcopy(imported)))
    if imported.get('success') is not True:
        return _composite_failure(ctx,imported.get('message','Texture set import failed'),imported.get('error_code','texture_set_import_failed'),'import_texture_set',imported.get('completed_mutation_stages',[]),imported)
    textures=imported['texture_asset_paths']
    completed.append('import_texture_set')
    values=dict(base_color_texture_asset_path=textures['base_color'],material_name=material_name,unreal_folder=folder+'/Materials',base_material_path=args['base_material_path'])
    values.update({slot+'_texture_asset_path':textures.get(slot,'') for slot in ('normal','roughness','ambient_occlusion','metallic')})
    # Preserve the PBR leaf's own once-only save flag, independently of imported saves.
    # Reuse execute, not the public Named wrapper: no nested journal or replay.
    material_receipt={'RP_parent_composite':True}
    material_log=dict(stage='create_material',result=None);logs.append(material_log)
    def material_checkpoint(current):
        _merge_material_receipt(ctx,current)
        if ctx.r['change_state_known']:ctx.publish(current['stage'])
        else:
            ctx.lost=True
            raise Stop('texture_material_material_unknown','PBR child boundary unknown; no parent getter or next stage')
    try:execute('create_material_with_texture_set',values,material_receipt,material_checkpoint)
    finally:_merge_material_receipt(ctx,material_receipt)
    material=copy.deepcopy(material_receipt.get('legacy_result',dict(success=False,message=material_receipt.get('error','Material creation failed'))))
    material_log['result']=material
    if material_receipt.get('error_code') or material.get('success') is not True:
        return _composite_failure(ctx,material.get('message','Material creation failed'),'create_material_failed','create_material',completed,material)
    completed.append('create_material')
    applied=None
    if args['target']:
        native=_native_import_apply(ctx,'apply_texture_material',args['target'],material['asset_path'],args['material_slot'])
        applied=_native_legacy(ctx,native);logs.append(dict(stage='apply_material',result=copy.deepcopy(applied)))
        if not native['success'] or applied.get('success') is not True:
            return _composite_failure(ctx,applied.get('message','Material application failed'),'apply_material_failed','apply_material',completed,applied)
    return dict(success=True,message='Imported texture set and created material',texture_asset_paths=textures,
        material_asset_path=material.get('asset_path'),applied=applied,logs=copy.deepcopy(logs))


def _merge_material_receipt(ctx,current):
    ctx.r['material_current_receipt']=copy.deepcopy(current)
    for key in ('mutation_attempted','observed_changed','compile_requested','compile_attempted','save_requested','save_attempted','saved','SDK_global_purity_unverified'):
        ctx.r[key] |= current.get(key) is True
    if current.get('change_state_known') is False:
        ctx.r.update(change_state_known=False,do_not_retry=True,post_failure_live_observation_skipped=True)
        ctx.lost=True


def _execute_import_composite(args,receipt,checkpoint):
    receipt.update(operation='import_create_and_apply_texture_set',stage='preflight',mutation_attempted=False,observed_changed=False,change_state_known=True,
        compile_requested=False,compile_attempted=False,save_requested=False,save_attempted=False,saved=False,
        SDK_global_purity_unverified=False,operation_complete=False,automatic_retry_permitted=False,
        actual_call_prefix=[],native_call_prefix=[],parameter_errors=[],parameter_readback=[],expected_positions={},UV_polygon_errors=[],
        requested_output_names=dict(OUTPUTS),expected_output_readback_names=dict(READBACK_OUTPUTS),
        composite_logs=[],completed_mutation_stages=[],physical_file_write_proven=False,shader_or_GPU_completion_proven=False,
        atomic_rollback=False,public_Python_GC_exit_save_atomic_admission_proven=False,automatic_cleanup_performed=False,
        private_import_apply_leaf_required=True,standalone_next_tick_required=True)
    ctx=None
    try:
        ctx=_Context(receipt,checkpoint)
        leaf=_import_composite(ctx,args)
        receipt['legacy_result']=leaf
        if receipt['change_state_known']:ctx.admit()
        receipt['operation_complete']=True
        return receipt
    except Exception as error:
        receipt.update(error_code=getattr(error,'code','texture_material_import_composite_failed'),error=str(error),do_not_retry=receipt['mutation_attempted'] or not receipt['change_state_known'])
        # Metadata only: an unknown native/SDK boundary never runs another getter or save.
        if ctx is not None:
            receipt['legacy_result']=_composite_failure(ctx,str(error),receipt['error_code'],receipt.get('next_SDK_stage','import_create_apply'),receipt['completed_mutation_stages'],receipt.get('legacy_result',{}))
        else:receipt['legacy_result']=dict(success=False,message=str(error),mutation_attempted=False)
        return receipt


def _clamp01(value: float, default: float) -> float:
    try:
        return max(0.0, min(1.0, float(value)))
    except (TypeError, ValueError):
        return default

def _nonnegative_float(value: float, default: float) -> float:
    try:
        return max(0.0, float(value))
    except (TypeError, ValueError):
        return default

def _pbr_dependencies():
    # Root owns the existing plugin PythonRequirements pins; no pip/sys.path fallback.
    import numpy as np
    from PIL import Image
    return np, Image


def _pbr_dependencies_available():
    try:
        np, Image=_pbr_dependencies()
        return all(callable(getattr(np,n,None)) for n in ('asarray','pad','ones_like','sqrt','stack','clip','full_like')) and callable(getattr(Image,'open',None)) and callable(getattr(Image,'fromarray',None))
    except ImportError:return False


def _pbr_error(message,*,error_code='error',details=None,**fields):
    result=dict(success=False,message=message,error=message,error_code=error_code)
    if details is not None:result['details']=details
    result.update(fields)
    return result


def _pbr_checkpoint(receipt,checkpoint,stage):
    receipt['stage']=stage
    if checkpoint:checkpoint(receipt)


def _pbr_mkdir(path,receipt,checkpoint):
    _pbr_checkpoint(receipt,checkpoint,'before_texture_material_authoring_checkpoint')
    existed=path.is_dir()
    row=dict(stage='mkdir_output_directory',path=str(path),entered=True,returned=False)
    receipt['file_call_prefix'].append(row);receipt['mutation_attempted']=True
    try:path.mkdir(parents=True,exist_ok=True)
    except Exception:
        receipt.update(change_state_known=False,post_failure_file_observation_skipped=True)
        raise
    row['returned']=True;receipt['observed_changed'] |= not existed
    _pbr_checkpoint(receipt,checkpoint,'returned_texture_material_authoring_operation')


def _pbr_save(image,path,slot,receipt,checkpoint):
    receipt['next_SDK_stage']='WritePBRPNG:'+slot
    _pbr_checkpoint(receipt,checkpoint,'before_texture_material_authoring_checkpoint')
    row=dict(stage='save_pbr_png',slot=slot,path=str(path),entered=True,returned=False)
    receipt['file_call_prefix'].append(row);receipt['mutation_attempted']=True
    try:image.save(path)
    except Exception:
        # A raised save may have created/truncated a file. Never inspect, delete or retry it.
        receipt.update(change_state_known=False,post_failure_file_observation_skipped=True)
        raise
    row['returned']=True;receipt['observed_changed']=True
    receipt['completed_file_stages'].append(slot)
    # The original filename/order/pixels are untouched. This checks only the returned file.
    try:
        digest=hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda:stream.read(1024*1024),b''):digest.update(chunk)
        _, Image=_pbr_dependencies()
        with Image.open(path) as actual:
            actual.load()
            if actual.format!='PNG' or actual.size!=image.size or actual.mode!=image.mode:
                raise ValueError('Returned PBR PNG dimensions/mode differ from the planned original image')
        row.update(current_sha256=digest.hexdigest(),width=image.width,height=image.height,mode=image.mode,returned_file_checked=True)
    except Exception:
        receipt.update(change_state_known=False,post_failure_file_observation_skipped=True)
        raise
    _pbr_checkpoint(receipt,checkpoint,'returned_texture_material_authoring_operation')


def _pbr_companion_files(
    base_color_image_path: str,
    output_name: str = "",
    output_dir: str = "",
    normal_strength: float = 2.0,
    roughness: float = 0.75,
    roughness_detail: float = 0.15,
    ambient_occlusion_strength: float = 0.35,
    metallic: float = 0.0,
    *, _receipt, _checkpoint=None,
) -> Dict[str, Any]:
    """Generate heuristic companion PBR texture PNGs from a BaseColor image."""

    np, Image = _pbr_dependencies()
    base_path = Path(base_color_image_path).expanduser().resolve()
    if not base_path.exists():
        return _pbr_error(
            f"BaseColor image does not exist: {base_path}",
            error_code="base_color_image_missing",
            stage="pbr_companion_generation",
            base_color_image_path=str(base_path),
        )

    local_output_dir = Path(output_dir).expanduser().resolve() if output_dir else base_path.parent
    safe_output_name = _import_name(output_name or base_path.stem)
    _pbr_mkdir(local_output_dir, _receipt, _checkpoint)

    normal_strength_value = _nonnegative_float(normal_strength, 2.0)
    roughness_value = _clamp01(roughness, 0.75)
    roughness_detail_value = _clamp01(roughness_detail, 0.15)
    ao_strength_value = _clamp01(ambient_occlusion_strength, 0.35)
    metallic_value = _clamp01(metallic, 0.0)

    with Image.open(base_path) as source_image:
        rgb_image = source_image.convert("RGB")
        width, height = rgb_image.size
        luma = np.asarray(rgb_image.convert("L"), dtype=np.float32) / 255.0

    padded_luma = np.pad(luma, 1, mode="edge")
    dx = padded_luma[1:-1, 2:] - padded_luma[1:-1, :-2]
    dy = padded_luma[2:, 1:-1] - padded_luma[:-2, 1:-1]

    nx = -dx * normal_strength_value
    ny = -dy * normal_strength_value
    nz = np.ones_like(luma)
    length = np.sqrt((nx * nx) + (ny * ny) + (nz * nz))
    normal = np.stack(
        (
            (nx / length * 0.5) + 0.5,
            (ny / length * 0.5) + 0.5,
            (nz / length * 0.5) + 0.5,
        ),
        axis=2,
    )

    roughness_map = np.clip(roughness_value + ((0.5 - luma) * roughness_detail_value), 0.0, 1.0)
    ao_map = np.clip(1.0 - ((1.0 - luma) * ao_strength_value), 0.0, 1.0)
    metallic_map = np.full_like(luma, metallic_value)

    outputs = {
        "normal": local_output_dir / f"{safe_output_name}_N.png",
        "roughness": local_output_dir / f"{safe_output_name}_R.png",
        "ambient_occlusion": local_output_dir / f"{safe_output_name}_AO.png",
        "metallic": local_output_dir / f"{safe_output_name}_M.png",
    }

    _pbr_save(Image.fromarray(np.clip(normal * 255.0, 0, 255).astype(np.uint8)), outputs["normal"], "normal", _receipt, _checkpoint)
    _pbr_save(Image.fromarray(np.clip(roughness_map * 255.0, 0, 255).astype(np.uint8)), outputs["roughness"], "roughness", _receipt, _checkpoint)
    _pbr_save(Image.fromarray(np.clip(ao_map * 255.0, 0, 255).astype(np.uint8)), outputs["ambient_occlusion"], "ambient_occlusion", _receipt, _checkpoint)
    _pbr_save(Image.fromarray(np.clip(metallic_map * 255.0, 0, 255).astype(np.uint8)), outputs["metallic"], "metallic", _receipt, _checkpoint)

    return {
        "success": True,
        "message": "Generated heuristic PBR companion textures",
        "base_color_image_path": str(base_path),
        "output_dir": str(local_output_dir),
        "output_name": safe_output_name,
        "image_size": {"width": width, "height": height},
        "map_paths": {name: str(path) for name, path in outputs.items()},
        "parameters": {
            "normal_strength": normal_strength_value,
            "roughness": roughness_value,
            "roughness_detail": roughness_detail_value,
            "ambient_occlusion_strength": ao_strength_value,
            "metallic": metallic_value,
        },
        "heuristic": True,
        "warnings": [
            "Companion maps are derived from BaseColor luminance and are suitable as a first-pass material aid, not a scanned or model-authored PBR set."
        ],
    }


def _execute_pbr_files(args,receipt,checkpoint):
    receipt.update(operation='generate_pbr_companion_textures',stage='preflight',mutation_attempted=False,observed_changed=False,change_state_known=True,
        compile_requested=False,compile_attempted=False,save_requested=False,save_attempted=False,saved=False,
        operation_complete=False,automatic_retry_permitted=False,SDK_global_purity_unverified=False,
        file_call_prefix=[],completed_file_stages=[],local_file_generation=True,Engine_asset_writes=0,
        atomic_rollback=False,physical_file_write_proven=False,shader_or_GPU_completion_proven=False)
    try:
        leaf=_pbr_companion_files(**args,_receipt=receipt,_checkpoint=checkpoint)
        receipt['legacy_result']=leaf;receipt['operation_complete']=True
        return receipt
    except Exception as error:
        # Original public wrapper's generic failure and exception type remain intact.
        receipt['legacy_result']=_pbr_error(str(error),error_code='pbr_companion_generation_failed',stage='pbr_companion_generation',details=dict(exception_type=type(error).__name__))
        receipt.update(error_code='pbr_companion_generation_failed',error=str(error),do_not_retry=receipt['mutation_attempted'] or not receipt['change_state_known'])
        return receipt


def _import_texture_set(ctx,args,unreal_folder,asset_name):
    fields=(('base_color','base_color_image_path',''),('normal','normal_image_path','_N'),('roughness','roughness_image_path','_R'),('ambient_occlusion','ambient_occlusion_image_path','_AO'),('metallic','metallic_image_path','_M'))
    paths={slot:args.get(field,'') for slot,field,suffix in fields if args.get(field,'')}
    if not paths.get('base_color'):
        return _pbr_error('base_color image path is required',error_code='missing_base_color_image',stage='import_texture_set',image_paths=paths)
    imports={};textures={};completed=[];safe_name=_import_name(asset_name)
    for slot,field,suffix in fields:
        if slot not in paths:continue
        native=_native_import_apply(ctx,'import_texture_file',paths[slot],unreal_folder,safe_name+suffix)
        leaf=_native_legacy(ctx,native);imports[slot]=leaf
        if not native['success'] or leaf.get('success') is not True:
            message='Texture import failed for '+slot
            failed=_composite_failure(ctx,message,leaf.get('error_code','texture_set_import_failed'),'import_texture_set',completed,leaf)
            failed.pop('logs',None)
            failed.update(error=message,failed_slot=slot,failed_stage='import_texture:'+slot,imports=copy.deepcopy(imports))
            return failed
        path=leaf.get('asset_path')
        if type(path) is not str or not path:
            ctx.r.update(change_state_known=False,do_not_retry=True,post_failure_live_observation_skipped=True);ctx.lost=True
            raise Stop('texture_material_import_payload_invalid','Successful import has no actual asset path; no next stage')
        textures[slot]=path;completed.append('import_texture:'+slot)
    return dict(success=True,message='Imported texture set',unreal_folder=unreal_folder,asset_name=safe_name,texture_asset_paths=textures,imports=imports)


def _execute_texture_api(method,args,receipt,checkpoint):
    receipt.update(operation=method,stage='preflight',mutation_attempted=False,observed_changed=False,change_state_known=True,
        compile_requested=False,compile_attempted=False,save_requested=False,save_attempted=False,saved=False,
        SDK_global_purity_unverified=False,operation_complete=False,automatic_retry_permitted=False,
        actual_call_prefix=[],native_call_prefix=[],composite_logs=[],completed_mutation_stages=[],
        private_import_apply_leaf_required=True,standalone_next_tick_required=method!='apply_material_to_mesh',
        physical_file_write_proven=False,atomic_rollback=False)
    ctx=None
    try:
        ctx=_Context(receipt,checkpoint)
        if method=='import_texture_set_to_unreal':leaf=_import_texture_set(ctx,args,args['unreal_folder'],args['asset_name'])
        elif method=='import_texture_to_unreal':
            leaf=_native_legacy(ctx,_native_import_apply(ctx,'import_texture_file',args['image_path'],args['unreal_folder'],args['asset_name']))
        else:
            leaf=_native_legacy(ctx,_native_import_apply(ctx,'apply_texture_material',args['target'],args['material_asset_path'],args['material_slot']))
        receipt['legacy_result']=leaf
        if receipt['change_state_known']:ctx.admit()
        receipt['operation_complete']=True
        return receipt
    except Exception as error:
        receipt.update(error_code=getattr(error,'code','texture_material_api_failed'),error=str(error),do_not_retry=receipt['mutation_attempted'] or not receipt['change_state_known'])
        # Copied metadata only; no post-failure getter, second call, cleanup or save.
        receipt.setdefault('legacy_result',_pbr_error(str(error),error_code=receipt['error_code']))
        return receipt


# Original texture_generation public4/6/6: local UV reference and disabled handoff.
_IMAGE_HANDOFF_METHODS=('generate_texture_from_prompt','generate_texture_for_mesh_uv','generate_and_apply_ai_texture')
_TEXTURE_COMPOSITE_LOCK=threading.RLock()
UV_GUIDED_PROMPT_SUFFIX=(
    'Follow the provided UV layout guide. Keep meaningful details inside UV islands. '
    'Avoid visible seams across UV island borders. Generate a BaseColor/albedo game texture only; '
    'do not claim to create a complete PBR material set.'
)


def _image_handoff(prompt,output_path,size,reference_image_path=''):
    message=('OpenAI API-key based image generation is disabled for this project. '
        'Use Codex built-in image generation to create the PNG, then call '
        'generate_pbr_companion_textures and import_create_and_apply_texture_set.')
    return _pbr_error(message,error_code='builtin_image_generation_required',stage='builtin_image_generation_required',
        api_route_disabled=True,billing_route='none',image_generation_route='codex_builtin_image_generation',
        prompt=prompt,requested_size=size,expected_output_path=output_path,reference_image_path=reference_image_path)


def _image_png_path(output_dir,output_name):
    return str(Path(output_dir).expanduser().resolve()/(_import_name(output_name)+'.png'))


def _image_ensure_png(output_dir,output_name,receipt,checkpoint):
    path=_image_png_path(output_dir,output_name)
    _pbr_mkdir(Path(path).parent,receipt,checkpoint)
    return path


def _image_read_failure(response,failed_stage):
    # These original flags describe the Unreal read stage, not prior local PNG writes.
    return dict(failed_stage=failed_stage,failed_stage_effect='read',read_unreal_response=copy.deepcopy(response),
        mutation_attempted=False,mutation_performed=False,changed=False)


def _image_uv(ctx,args):
    local_dir=args.get('output_dir') or os.path.join(tempfile.gettempdir(),'unreal_mcp_ai_textures')
    uv_path=_image_png_path(local_dir,_import_name(args['output_name'])+'_UV'+str(args['uv_channel']))
    # Merely preparing a read does not create the output directory.
    uv_result=_uv(ctx,dict(mesh_path=args['mesh_path'],uv_channel=args['uv_channel'],output_path=uv_path))
    ctx.r['image_handoff_UV_result']=copy.deepcopy(uv_result);ctx.publish('returned_texture_material_authoring_operation')
    if not uv_result.get('success'):
        return _pbr_error('UV layout export failed',error_code='uv_export_failed',stage='uv_export',
            uv_result=uv_result,**_image_read_failure(uv_result,'get_static_mesh_uv_data'))
    output=_image_ensure_png(local_dir,args['output_name'],ctx.r,ctx.checkpoint)
    ctx.admit()
    guided=args['prompt'].strip()+'\n\n'+UV_GUIDED_PROMPT_SUFFIX
    leaf=_image_handoff(guided,output,args['size'],uv_result.get('output_path',uv_path))
    leaf.update(uv_layout_path=uv_result.get('output_path'),mesh_path=args['mesh_path'],uv_channel=args['uv_channel'],uv_result=uv_result)
    return leaf


def _image_actor_label(ctx,actor):
    before=ctx.call('GetActorLabelBefore',lambda:actor.get_actor_label(False))
    label=ctx.call('GetActorLabel',lambda:actor.get_actor_label(),writer=True)
    after=ctx.call('GetActorLabelReadback',lambda:actor.get_actor_label(False))
    ctx.r['observed_changed'] |= before!=after
    rows=ctx.r.setdefault('actor_label_prefix',[])
    rows.append(dict(before=before,returned=label,after=after,changed=before!=after))
    if len(rows)>128:rows.pop(64)
    if label!=after:
        ctx.r.update(change_state_known=False,post_failure_live_observation_skipped=True);ctx.lost=True
        raise Stop('texture_material_target_label_changed','Actor label changed after the original lazy-label callback')
    for anchor in ctx.targets:
        if anchor.obj==actor:anchor.label=after
    ctx.admit()
    return label


def _image_target_mesh(ctx,target):
    # Original resolver order: actor subsystem, selected branch, otherwise asset then actors.
    actors=ctx.call('GetEditorActorSubsystem',lambda:unreal.get_editor_subsystem(unreal.EditorActorSubsystem));ctx.pin(actors)
    def first_mesh(actor,selected):
        components=ctx.call('GetStaticMeshComponents',lambda:actor.get_components_by_class(unreal.StaticMeshComponent))
        # Hold every returned wrapper before the next SDK boundary; use the original first item.
        retained=list(components)
        mesh=None
        if retained:
            component=retained[0];ctx.pin(component,owner=actor,require_world=True)
            mesh=ctx.call('GetStaticMesh',lambda:component.get_editor_property('static_mesh'),on_return=lambda value:ctx.pin(value) if value is not None else None)
        if mesh is None:
            message='Selected actor does not have a StaticMeshComponent with a mesh.' if selected else 'Actor has no StaticMeshComponent with a mesh: '+target
            return dict(success=False,message=message)
        label=_image_actor_label(ctx,actor)
        actor_path=ctx.call('GetActorPath',lambda:actor.get_path_name())
        mesh_path=ctx.call('GetStaticMeshPath',lambda:mesh.get_path_name())
        return dict(success=True,target=target,actor=label,actor_path=actor_path,mesh_path=mesh_path.split('.')[0])
    if target=='selected':
        selected=ctx.call('GetSelectedLevelActors',lambda:actors.get_selected_level_actors())
        retained=list(selected)
        if not retained:return dict(success=False,message='No selected actors. Select a Static Mesh actor or pass an actor name/path.')
        ctx.pin(retained[0],require_world=True)
        return first_mesh(retained[0],True)
    # Unlike _load, this original call accepts an actor label/name before the actor fallback.
    loaded=ctx.call('LoadTargetAsset',lambda:unreal.EditorAssetLibrary.load_asset(target),
        on_return=lambda value:ctx.pin(value) if value is not None else None)
    if loaded is not None:
        class_name=ctx.call('GetTargetAssetClass',lambda:loaded.get_class().get_name())
        if class_name=='StaticMesh':
            path=ctx.call('GetTargetMeshPath',lambda:loaded.get_path_name())
            return dict(success=True,target=target,mesh_path=path.split('.')[0])
    values=ctx.call('GetAllLevelActors',lambda:actors.get_all_level_actors())
    retained=list(values)
    matches=[]
    for actor in retained:
        anchor=ctx.pin(actor,require_world=True)
        label=_image_actor_label(ctx,actor)
        matched=target in label
        if not matched:
            path=ctx.call('GetActorPath',lambda:actor.get_path_name());matched=target in path
        if not matched:
            name=ctx.call('GetActorName',lambda:actor.get_name());matched=target==name
        if matched:matches.append(actor)
        # Retained wrappers keep returned actors alive; only the first matching actor is a later source.
        if not matched or len(matches)>1:ctx.targets.remove(anchor)
    if not matches:return dict(success=False,message='No actor or StaticMesh asset matched target: '+target)
    return first_mesh(matches[0],False)


def _image_apply_handoff(ctx,args):
    logs=ctx.r['composite_logs']
    resolved=_image_target_mesh(ctx,args['target'])
    logs.append(dict(stage='resolve_target',result=copy.deepcopy(resolved)));ctx.publish('returned_texture_material_authoring_operation')
    if not resolved.get('success'):
        return _pbr_error('Failed to resolve target Static Mesh',error_code='resolve_target_failed',stage='resolve_target',
            logs=copy.deepcopy(logs),**_image_read_failure(resolved,'resolve_target'))
    local_dir=os.path.join(tempfile.gettempdir(),'unreal_mcp_ai_textures',_import_name(args['output_name']))
    # Internal call shares the original reentrant lock and current journal; no public replay.
    with _TEXTURE_COMPOSITE_LOCK:
        generated=_image_uv(ctx,dict(mesh_path=resolved['mesh_path'],prompt=args['prompt'],uv_channel=args['uv_channel'],
            output_name=args['output_name'],output_dir=local_dir,size=args['size']))
    logs.append(dict(stage='generate_texture',result=copy.deepcopy(generated)));ctx.publish('returned_texture_material_authoring_operation')
    response=generated.get('read_unreal_response')
    if type(response) is not dict:response=generated
    # The original backend is disabled, so import/material/apply code is unreachable.
    return _pbr_error(generated.get('message','Codex built-in image generation is required before Unreal import/apply can continue'),
        error_code=generated.get('error_code','builtin_image_generation_required'),stage='builtin_image_generation_required',
        logs=copy.deepcopy(logs),next_steps=[
            'Generate the PNG with Codex built-in image generation using the returned prompt and UV layout.',
            'Save it to expected_output_path.',
            'Then call generate_pbr_companion_textures and import_create_and_apply_texture_set.'],
        **{key:generated.get(key) for key in ('prompt','expected_output_path','reference_image_path','image_generation_route','api_route_disabled','billing_route')},
        **_image_read_failure(response,str(generated.get('failed_stage') or 'generate_texture')))


def _execute_image_handoff(method,args,receipt,checkpoint):
    receipt.update(operation=method,stage='preflight',mutation_attempted=False,observed_changed=False,change_state_known=True,
        compile_requested=False,compile_attempted=False,save_requested=False,save_attempted=False,saved=False,
        SDK_global_purity_unverified=False,operation_complete=False,automatic_retry_permitted=False,
        actual_call_prefix=[],file_call_prefix=[],UV_polygon_errors=[],composite_logs=[],
        physical_file_write_proven=False,image_generation_performed=False,api_route_disabled=True,
        external_network_attempted=False,image_handoff_parent=True,source_scope='captured public Editor roles and local file prefix; not global load purity',
        legacy_read_mutation_flags_scope='Unreal read only; local UV PNG/mkdir effects remain in current_Python_receipt')
    locked=False;ctx=None
    try:
        if method!='generate_texture_from_prompt':
            locked=_TEXTURE_COMPOSITE_LOCK.acquire(blocking=False)
            if not locked:
                receipt['legacy_result']=_pbr_error('Another texture composite workflow is already running',
                    error_code='texture_composite_already_running',mutation_attempted=False,mutation_performed=False,changed=False)
                receipt['operation_complete']=True;return receipt
        if method=='generate_texture_from_prompt':
            output=_image_ensure_png(args['output_dir'],args['output_name'],receipt,checkpoint)
            leaf=_image_handoff(args['prompt'],output,args['size'])
        else:
            ctx=_Context(receipt,checkpoint)
            leaf=_image_uv(ctx,args) if method=='generate_texture_for_mesh_uv' else _image_apply_handoff(ctx,args)
            ctx.admit()
        receipt.update(legacy_result=leaf,operation_complete=True,stage=leaf.get('stage','returned_texture_material_authoring_operation'))
        if not receipt['change_state_known']:raise Stop('texture_material_partial_unknown','Current scoped outcome is unknown; no next stage')
        return receipt
    except Exception as error:
        receipt.update(error_code=getattr(error,'code','texture_material_image_handoff_failed'),error=str(error),
            do_not_retry=True,post_failure_live_observation_skipped=True)
        if receipt['mutation_attempted'] or not receipt['change_state_known']:receipt['change_state_known']=False
        receipt.setdefault('legacy_result',dict(success=False,message=str(error),error=str(error)))
        return receipt
    finally:
        if locked:_TEXTURE_COMPOSITE_LOCK.release()
