"""Enable transient editor animation flags only in L_RetargetPreview."""
import time
import unreal

PREVIEW_WORLDS = {'/Game/RetargetedAnimations/StylizedFantasyGirls/Maps/L_RetargetPreview.L_RetargetPreview', '/Game/RetargetedAnimations/StylizedFantasyGirls/Maps/L_AllAnimationsPreview.L_AllAnimationsPreview'}
_handle = None
_last_check = 0.0
VERSION = 4

def enable():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or world.get_path_name() not in PREVIEW_WORLDS:
        return 0
    changed = 0
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not isinstance(actor, unreal.SkeletalMeshActor):
            continue
        component = actor.skeletal_mesh_component
        animation = component.get_editor_property('animation_data').anim_to_play
        if animation and animation.get_path_name().startswith('/Game/RetargetedAnimations/StylizedFantasyGirls/'):
            if not component.get_editor_property('update_animation_in_editor') or not component.is_playing():
                component.set_update_animation_in_editor(True)
                component.play_animation(animation, True)
                changed += 1
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_viewport_realtime(True)
    return changed

def _tick(delta_time):
    global _last_check
    now = time.monotonic()
    if now - _last_check < 1.0:
        return
    _last_check = now
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        return
    enable()

def install():
    global _handle
    if '-run=' in unreal.SystemLibrary.get_command_line().lower():
        return
    if _handle is None:
        _handle = unreal.register_slate_post_tick_callback(_tick)
    enable()


