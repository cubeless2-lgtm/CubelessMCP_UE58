"""HF finite public Editor Python utilities. No Native leaf, executor or transport.
Exact public object/context checks bound callback continuations; they are not an
atomic public Python proof of all Engine GC/exit/save globals.
"""
import copy
import unreal
from ...operations.state import OperationError

METHODS=('list_pcg_assets','list_pcg_components','set_pcg_debug_enabled','resave_pcg_assets')
DEBUG_PROPERTIES=('debug','bDebug','b_debug','show_debug','bShowDebug','b_show_debug','debug_enabled','bDebugEnabled','b_debug_enabled')
_STOPPED=False
_SHUTDOWN=None

class Stop(OperationError):pass

def _shutdown():
    global _STOPPED
    _STOPPED=True  # Metadata only; no borrowed UObject or late SDK cleanup.

def binding_status():
    common=not _STOPPED and all(callable(getattr(unreal,name,None)) for name in ('is_editor','get_editor_subsystem','register_python_shutdown_callback')) and callable(getattr(getattr(unreal,'SystemLibrary',None),'is_valid',None))
    registry=callable(getattr(getattr(unreal,'AssetRegistryHelpers',None),'get_asset_registry',None))
    actors=all(callable(getattr(getattr(unreal,'EditorActorSubsystem',None),name,None)) for name in ('get_all_level_actors','get_selected_level_actors'))
    object_type=getattr(unreal,'Object',None)
    debug=all(callable(getattr(object_type,name,None)) for name in ('get_editor_property','set_editor_property'))
    save=all(callable(getattr(getattr(unreal,'EditorAssetLibrary',None),name,None)) for name in ('load_asset','save_loaded_asset'))
    world=callable(getattr(getattr(unreal,'UnrealEditorSubsystem',None),'get_editor_world',None))
    idle=callable(getattr(getattr(unreal,'LevelEditorSubsystem',None),'is_in_play_in_editor',None))
    return dict(list_pcg_assets=bool(common and world and registry),list_pcg_components=bool(common and world and actors and idle),
        set_pcg_debug_enabled=bool(common and world and actors and debug and idle),resave_pcg_assets=bool(common and world and registry and save and idle))

def _valid(obj):return obj is not None and unreal.SystemLibrary.is_valid(obj) is True

def _name(value,field):
    if type(value) is not str:raise OperationError('pcg_utilities_name_invalid',field+' must be a string')
    try:units=len(value.encode('utf-16-le'))//2
    except UnicodeError as error:raise OperationError('pcg_utilities_name_invalid',field+' must be valid Unicode') from error
    if units>=1024 or '\x00' in value:raise OperationError('pcg_utilities_name_bound',field+' exceeds FName bound or contains embedded NUL')
    return value

class _Anchor:
    def __init__(self,obj,world=None,owner=None,require_world=False):
        if not _valid(obj):raise Stop('pcg_utilities_object_unavailable','Expected live loaded public UObject')
        self.obj=obj;self.shape=self.read();self.world=world;self.owner=owner;self.require_world=require_world;self.label=None
        if require_world and obj.get_world()!=world:raise Stop('pcg_utilities_world_changed','Borrowed object is outside the captured Editor world')
        if owner is not None and obj.get_owner()!=owner:raise Stop('pcg_utilities_owner_changed','Component owner differs')
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
        if _STOPPED or unreal.is_editor() is not True:raise Stop('pcg_utilities_editor_unavailable','Editor Python lifecycle is unavailable')
        if _SHUTDOWN is None:_SHUTDOWN=unreal.register_python_shutdown_callback(_shutdown)
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        if not _valid(self.editor):raise Stop('pcg_utilities_editor_unavailable','Exact Editor subsystem is unavailable')
        self.editor_shape=_Anchor(self.editor)
        self.world=self.editor.get_editor_world();self.world_shape=_Anchor(self.world) if self.world is not None else None
        receipt['execution_context']=dict(editor_path=self.editor_shape.shape[0],world_path=self.world_shape.shape[0] if self.world_shape else None)
        self.admit()
    def admit(self):
        if self.lost:raise Stop('pcg_utilities_context_lost','Context loss is sticky; no further Engine call or replay')
        try:
            if _STOPPED or unreal.is_editor() is not True:raise Stop('pcg_utilities_shutdown','Editor shutdown; metadata-only stop')
            if (not self.editor_shape.same() or unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)!=self.editor
                or self.editor.get_editor_world()!=self.world or self.world_shape is not None and not self.world_shape.same()
                or self.level_shape is not None and (not self.level_shape.same() or unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)!=self.level or self.level.is_in_play_in_editor())
                or any(not target.same() for target in self.targets)):
                raise Stop('pcg_utilities_context_changed','Exact Editor/world/loaded target class/outer/package/path/owner changed')
            if _STOPPED:raise Stop('pcg_utilities_shutdown','Shutdown during public context observation')
        except Exception as error:
            self.lost=True;self.r.update(change_state_known=False,context_lost=True,post_failure_live_observation_skipped=True,do_not_retry=True)
            if isinstance(error,Stop):raise
            raise Stop('pcg_utilities_context_changed',str(error)) from error
    def writer_admit(self):
        self.admit()
        if self.level_shape is None:
            level_type=getattr(unreal,'LevelEditorSubsystem',None)
            if level_type is None:raise Stop('pcg_utilities_idle_context_unavailable','Public LevelEditorSubsystem is required before writer entry')
            self.level=unreal.get_editor_subsystem(level_type);self.level_shape=_Anchor(self.level)
        if self.level.is_in_play_in_editor():raise Stop('pcg_utilities_PIE_unsupported','Stop PIE/SIE before utility writes; no writer replay')
        self.admit()
    def publish(self,stage):
        self.r['stage']=stage
        if self.checkpoint:
            try:self.checkpoint(self.r)
            except Exception as error:
                raise Stop('pcg_utilities_checkpoint_failed','Stop before next Engine work: '+str(error)) from error
    def call(self,stage,function,writer=False,save=False,on_return=None,on_enter=None):
        self.admit()
        if writer or save:self.writer_admit()
        if len(self.r['actual_call_prefix'])>=262144:raise Stop('pcg_utilities_budget','Bounded call/row budget exhausted; prefix retained')
        row=dict(stage=stage,entered=False,returned=False,explicit_mutation=writer,explicit_save=save)
        self.r['actual_call_prefix'].append(row)
        if writer or save:self.publish('before_pcg_utilities_checkpoint')
        row['entered']=True;self.r['SDK_global_purity_unverified']=True
        self.r['mutation_attempted']|=writer;self.r['save_attempted']|=save
        if on_enter:on_enter()
        try:
            value=function();row['returned']=True
            if on_return:on_return(value)
        except Exception as error:
            row.update(raised=True,error=str(error))
            self.admit()
            if writer or save:self.publish('returned_pcg_utilities_operation')
            raise
        self.admit()
        if writer or save:self.publish('returned_pcg_utilities_operation')
        return value
    def pin(self,obj,owner=None,require_world=False):
        self.admit();target=_Anchor(obj,self.world,owner,require_world);self.targets.append(target);self.admit();return target
    def release_targets(self):
        self.admit();self.targets=[]
    def actor_label(self,actor,anchor):
        previous=self.call('GetActorLabel_NoCreate',lambda:actor.get_actor_label(False))
        def returned(label):
            if not previous and label:
                self.r['observed_changed']=True;self.r['lazy_actor_labels_created']+=1
            anchor.label=label if not previous else previous
        return self.call('GetActorLabel',lambda:actor.get_actor_label(),writer=not bool(previous),on_return=returned)


def _asset_rows(ctx,root_path):
    root=_name(root_path,'root_path')
    registry=ctx.call('GetAssetRegistry',lambda:unreal.AssetRegistryHelpers.get_asset_registry())
    ctx.pin(registry)
    data=ctx.call('GetAssetsByPath',lambda:registry.get_assets_by_path(root,recursive=True))
    if len(data)>262144:raise Stop('pcg_utilities_budget','Asset query rows exceed262144; public query itself is a synchronous SDK call')
    rows=[]
    for item in data:
        class_name=str(item.asset_class_path.asset_name);package_name=str(item.package_name);asset_name=str(item.asset_name)
        if 'PCG' in class_name or 'PCG' in asset_name:
            rows.append(dict(asset_name=asset_name,package_name=package_name,class_name=class_name,object_path=package_name+'.'+asset_name))
    ctx.admit();ctx.release_targets();return rows


def _actors(ctx,selected_only):
    if ctx.world is None:raise Stop('pcg_utilities_world_unavailable','Current Editor world is required for actor utilities')
    subsystem=ctx.call('GetEditorActorSubsystem',lambda:unreal.get_editor_subsystem(unreal.EditorActorSubsystem))
    ctx.pin(subsystem)
    actors=ctx.call('GetSelectedLevelActors' if selected_only else 'GetAllLevelActors',lambda:subsystem.get_selected_level_actors() if selected_only else subsystem.get_all_level_actors())
    if len(actors)>262144:raise Stop('pcg_utilities_budget','Actor query rows exceed262144')
    ctx.release_targets();return list(actors)


def _property_snapshot(ctx,component):
    values={}
    for name in DEBUG_PROPERTIES:
        try:
            value=ctx.call('GetDebugProperty',lambda n=name:component.get_editor_property(n))
            values[name]=dict(available=True,value=value) if type(value) is bool else dict(available=True,unsupported_type=type(value).__name__)
        except Stop:raise
        except Exception:values[name]=dict(available=False)
    return values


def _debug(ctx,args):
    r=ctx.r;enabled=bool(args['enabled']);actor_filter=args['actor_name'];changed=[]
    r['legacy_result']=dict(success=True,count=0,changed=changed,errors=[])
    for actor in _actors(ctx,bool(args['selected_only'])):
        ctx.release_targets();a=ctx.pin(actor,require_world=True);label=ctx.actor_label(actor,a);path=a.shape[0]
        if actor_filter and actor_filter not in label and actor_filter not in path:continue
        components=list(ctx.call('GetActorComponents',lambda:actor.get_components_by_class(unreal.ActorComponent)))
        for component in components:
            ctx.targets=[a];c=ctx.pin(component,actor,True);class_name=c.shape[1].get_name()
            if 'PCG' not in class_name:continue
            component_name=component.get_name();before=_property_snapshot(ctx,component);accepted=[];errors=[]
            prefix=dict(actor=label,component_path=c.shape[0],before=before,accepted_properties=accepted,property_errors=errors)
            r['debug_components'].append(prefix)
            for name in DEBUG_PROPERTIES:
                try:
                    ctx.call('SetDebugProperty:'+name,lambda n=name:component.set_editor_property(n,enabled),writer=True)
                    accepted.append(name)
                except Stop:raise
                except Exception as error:errors.append(dict(property=name,error=str(error)))  # Original public errors stays[].
            after=_property_snapshot(ctx,component);prefix['after']=after
            same_shape=all(before[n].get('available')==after[n].get('available') and before[n].get('unsupported_type')==after[n].get('unsupported_type') for n in DEBUG_PROPERTIES)
            known=same_shape and all(not after[n].get('available') or 'value' in after[n] for n in DEBUG_PROPERTIES)
            known=known and all(after[n].get('value') is enabled for n in accepted)
            r['change_state_known']&=known
            r['observed_changed']|=any(before[n]!=after[n] for n in DEBUG_PROPERTIES)
            if accepted:changed.append(dict(actor=label,component=component_name,component_class=class_name,properties=list(accepted)))
            r['legacy_result']['count']=len(changed);ctx.publish('returned_pcg_utilities_operation')
    ctx.release_targets();return r['legacy_result']


def _resave(ctx,args):
    r=ctx.r;saved=[];errors=[];r['legacy_result']=dict(success=True,count=0,saved=saved,errors=errors)
    rows=_asset_rows(ctx,args['root_path'])
    for row in rows:
        ctx.release_targets();package=row['package_name'];prefix=dict(package=package,load_returned=False,save_attempted=False)
        r['asset_save_prefix'].append(prefix)
        try:
            asset=ctx.call('LoadAsset',lambda:unreal.EditorAssetLibrary.load_asset(package),on_return=lambda a:prefix.update(load_returned=True,load_returned_nonnull=a is not None))
            if not asset:
                prefix['load_null_silent']=True;continue
            anchor=ctx.pin(asset)
            if anchor.shape[0]!=row['object_path'] or anchor.shape[1].get_name()!=row['class_name'] or anchor.shape[3].get_path_name()!=package:
                raise Stop('pcg_utilities_loaded_asset_replaced','Loaded asset differs from exact registry path/class/package; no save or retarget')
            def returned(success):
                prefix.update(save_returned=True,save_success=bool(success))
                if success:
                    r['saved']=True;saved.append(package);r['legacy_result']['count']=len(saved)
                else:r['save_false_silent_paths'].append(package)
            ctx.call('SaveLoadedAsset',lambda:unreal.EditorAssetLibrary.save_loaded_asset(asset),save=True,on_return=returned,on_enter=lambda:prefix.update(save_attempted=True))
        except Stop:raise
        except Exception as error:
            errors.append(dict(asset=package,error=str(error)));prefix['error']=str(error)
            if prefix['save_attempted'] and not prefix.get('save_returned'):
                prefix['disk_state_unknown']=True;r['disk_state_unknown']=True;r['change_state_known']=False
    ctx.release_targets();return r['legacy_result']


def execute(method,args,receipt,checkpoint=None):
    receipt.update(operation=method,stage='preflight',mutation_attempted=False,change_state_known=True,observed_changed=False,
        save_requested=method=='resave_pcg_assets',save_attempted=False,saved=False,SDK_global_purity_unverified=False,
        actual_call_prefix=[],asset_save_prefix=[],debug_components=[],lazy_actor_labels_created=0,save_false_silent_paths=[],
        explicit_compile_requested=False,explicit_compile_attempted=False,physical_file_write_proven=False,
        public_Python_GC_exit_save_atomic_admission_proven=False,automatic_retry_permitted=False,operation_complete=False)
    try:
        ctx=_Context(receipt,checkpoint)
        if method=='list_pcg_assets':
            rows=_asset_rows(ctx,args['root_path']);leaf=dict(success=True,count=len(rows),assets=rows)
        elif method=='list_pcg_components':
            items=[];leaf=dict(success=True,count=0,components=items);receipt['legacy_result']=leaf
            for actor in _actors(ctx,False):
                ctx.release_targets();a=ctx.pin(actor,require_world=True)
                for component in list(ctx.call('GetActorComponents',lambda:actor.get_components_by_class(unreal.ActorComponent))):
                    ctx.targets=[a];c=ctx.pin(component,actor,True);class_name=c.shape[1].get_name()
                    if 'PCG' in class_name:
                        label=ctx.actor_label(actor,a);items.append(dict(actor=label,actor_path=a.shape[0],component=component.get_name(),component_class=class_name));leaf['count']=len(items)
            ctx.release_targets()
        elif method=='set_pcg_debug_enabled':leaf=_debug(ctx,args)
        elif method=='resave_pcg_assets':leaf=_resave(ctx,args)
        else:raise OperationError('pcg_utilities_method_invalid','Unknown finite public utility')
        ctx.admit();receipt.update(legacy_result=leaf,operation_complete=True)
        if not receipt['change_state_known']:
            raise OperationError('pcg_utilities_save_outcome_unknown' if method=='resave_pcg_assets' else 'pcg_utilities_debug_readback_unverified',
                'Original per-item results retained; actual save outcome or readable target debug projection is not proven')
        return receipt
    except Exception as error:
        receipt.update(error_code=getattr(error,'code','pcg_utilities_engine_call_failed'),error=str(error),do_not_retry=receipt['mutation_attempted'] or receipt['save_attempted'] or not receipt['change_state_known'])
        receipt.setdefault('legacy_result',dict(success=False,error=str(error),error_code=receipt['error_code']))
        return receipt
