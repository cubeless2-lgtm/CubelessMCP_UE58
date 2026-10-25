"""UE5.8 public API readback for actual actor-owned mesh instances."""
import unreal

def pcg_placement(actor_path, max_instances):
    if not unreal.SystemLibrary.get_engine_version().startswith('5.8.3-'):
        raise ValueError('Only UE5.8.3 is implemented')
    if type(max_instances) is not int or not 1<=max_instances<=10000:
        raise ValueError('max_instances must be 1..10000; truncation is not a complete observation')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor=unreal.find_object(None,actor_path)
    if not unreal.SystemLibrary.is_valid(actor) or not isinstance(actor,unreal.Actor) or actor.get_world()!=world:
        raise ValueError('Exact loaded actor in current Editor world required; no actor load or label fallback')
    def current(component=None):
        if not unreal.SystemLibrary.is_valid(world) or not unreal.SystemLibrary.is_valid(actor) or actor.get_path_name()!=actor_path or actor.get_world()!=world:
            raise RuntimeError('Actor/world lifetime changed during observation')
        if component is not None and (not unreal.SystemLibrary.is_valid(component) or component.get_owner()!=actor or component.get_world()!=world):
            raise RuntimeError('Component ownership changed during observation')
    components=actor.get_components_by_class(unreal.InstancedStaticMeshComponent);current()
    if len(components)>512:raise ValueError('Actor component observation bound exceeded')
    rows=[];total=0
    for component in components:
        current(component);count=component.get_instance_count();current(component)
        total+=count
        if total>max_instances:raise ValueError('Complete instance count exceeds bound; no partial sample pass')
        mesh=component.get_editor_property('static_mesh');current(component)
        transforms=[]
        for i in range(count):
            transform=component.get_instance_transform(i,world_space=True);current(component)
            if transform is None:raise RuntimeError('Instance transform read failed')
            p=transform.translation;s=transform.scale3d;r=transform.rotation
            transforms.append(dict(position=[p.x,p.y,p.z],scale=[s.x,s.y,s.z],rotation=[r.x,r.y,r.z,r.w]))
        if component.get_instance_count()!=count:raise RuntimeError('Instance count changed during observation')
        current(component)
        rows.append(dict(path=component.get_path_name(),class_path=component.get_class().get_path_name(),
            mesh=mesh.get_path_name() if mesh else '',instance_count=count,instances=transforms))
    current()
    return dict(actor_path=actor_path,world_path=world.get_path_name(),components=rows,
                component_count=len(rows),total_instances=total,complete=True,
                scope='All actor-owned ISM/HISM instances; PCG resource ownership/completion is checked separately. No asset mutation/compile/save/generation.')
