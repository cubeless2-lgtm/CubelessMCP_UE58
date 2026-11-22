"""Source-video-only contact refinement controls; original assets are preserved."""
import json,hashlib,math,uuid
from datetime import datetime,timezone
from pathlib import Path

def _editor():
    import unreal
    window = unreal.CubelessMocapWindowLibrary
    library = unreal.CubelessMocapLibrary
    state = json.loads(window.get_state())
    if state['busy']:
        raise RuntimeError('Wait for the current mocap job')
    return unreal, window, library, window.get_settings(), state

def prepare_source_contact(source_observations,directory=None,contact_segments=None,contact_hints=None):
    unreal,window,library,settings,state=_editor()
    obs_path=Path(source_observations).resolve();obs=json.loads(obs_path.read_bytes())
    if obs.get('schema')!='cbm_pose_observations_v1' or not obs.get('frames'):raise ValueError('Need original-video observations')
    video=Path(state['video']).resolve()
    if Path(obs['source']).resolve()!=video or hashlib.sha256(video.read_bytes()).hexdigest()!=obs['source_sha256']:raise ValueError('Original video changed')
    saved=Path(unreal.Paths.project_saved_dir()).resolve()/'CubelessBodyMocap'
    work=Path(directory).resolve() if directory else saved/'SourceContact'/uuid.uuid4().hex
    if saved not in work.parents or work.exists():raise ValueError('Use a new directory under Saved/CubelessBodyMocap')
    source=unreal.load_asset(state['preview_animation']);mesh=settings.get_editor_property('target_mesh')
    if not library.export_body_contact_data(source,mesh,str(work)):raise RuntimeError('Saved source export rejected')
    request=json.loads((work/'request.json').read_bytes());camera=saved/request['debug_run']/'DebugBones.json'
    meta=json.loads(camera.read_bytes())
    if Path(meta['video']).resolve()!=video or meta['fps']!=request['fps'] or meta['frames']!=request['frames'] or meta.get('start')!=state['result_start']:raise ValueError('Source camera does not match this interval')
    stamp=datetime.fromtimestamp(video.stat().st_mtime,timezone.utc).replace(microsecond=0).isoformat(timespec='milliseconds').replace('+00:00','Z')
    if meta.get('video_size')!=video.stat().st_size or meta.get('video_stamp')!=stamp:raise ValueError('Camera source video changed')
    job=dict(request=str(work/'request.json'),source_observations=str(obs_path),source_camera=str(camera),quality_contract='source_priority_contact',source_weight=30,contact_weight=20,smoothing_seconds=.065,iterations=1600,device='cuda')
    if contact_segments is not None:job['contact_segments']=contact_segments
    if contact_hints is not None:job['contact_hints']=contact_hints
    (work/'job.json').write_text(json.dumps(job,indent=2),encoding='utf-8')
    return dict(job=str(work/'job.json'),request=str(work/'request.json'),source_asset=source.get_path_name(),worker=str(Path(__file__).with_name('source_contact_refine.py')),source_unchanged=True)


def apply_source_contact(result_file, output_package):
    """Apply a reviewed worker result to a NEW saved AnimSequence, then preview.

    Reject stale source transforms, changed inputs, non-unit data, protected-track
    edits and failed numerical acceptance before creating any output package.
    """
    unreal, window, library, settings, state=_editor()
    result_file=Path(result_file).resolve();d=json.loads(result_file.read_text());report=d.get('source_contact_report',{})
    work=result_file.parent;job=json.loads((work/'job.json').read_text());original=json.loads(Path(job['request']).read_text())
    source_priority=d.get('algorithm')=='source_priority_contact_v1'
    if d.get('algorithm')!='source_priority_contact_v1' or not report.get('numerical_acceptance'):
        raise ValueError('No accepted pose refinement result')
    step=report.get('max_deforming_step_degrees_before_after',[0,float('inf')])
    if source_priority:
        temporal=report['temporal']
        if (report.get('quality_contract')!='source_priority_contact' or 
            report['contact']['l']['first_target_error_cm_before_after'][1]>=3.5 or
            report['source_after']['mean_px']>report['source_before']['mean_px']+5 or
            report['first_wrist_error_px_before_after']['l'][1]>10 or
            temporal['wrist_high_frequency_rms_cm_before_after'][1]>=temporal['wrist_high_frequency_rms_cm_before_after'][0] or
            max(x['p95_active_error_cm'] for x in report['contact'].values())>5 or
            temporal['rotation_after']['angular_acceleration_rms_degrees']>=temporal['rotation_before']['angular_acceleration_rms_degrees'] or step[1]>step[0]+3):
            raise ValueError('Source-priority contact or temporal limits were exceeded')
    for field,hash_field in [('source_observations','source_observations_sha256'),('request','request_sha256')]:
        if hashlib.sha256(Path(job[field]).read_bytes()).hexdigest()!=report[hash_field]:
            raise ValueError('Fit input changed; prepare again')
    for field in ['source_observations']:
        item=json.loads(Path(job[field]).read_text())
        if hashlib.sha256(Path(item['source']).read_bytes()).hexdigest()!=item['source_sha256']:
            raise ValueError('Original video changed')
    if Path(json.loads(Path(job['source_observations']).read_text())['source']).resolve()!=Path(state['video']).resolve():
        raise ValueError('Current video changed')
    for key in ['source_asset','source_guid','target_mesh','frames','fps','names','parents']:
        if d[key]!=original[key]:raise ValueError('Result source/schema identity changed')
    if state['preview_animation']!=d['source_asset']:
        raise ValueError('Preview changed since prepare; inspect before applying')
    if not output_package.startswith('/Game/') or unreal.EditorAssetLibrary.does_asset_exist(output_package):
        raise ValueError('Need a new /Game animation package')
    names=d['names'];n=d['frames'];b=len(names)
    allowed={'pelvis','spine_01','spine_03','spine_05','clavicle_l','clavicle_r','head'}|{
        f'{x}_{s}' for s in ['l','r'] for x in ['thigh','calf','foot','upperarm','lowerarm','hand']}|{'ik_foot_l','ik_foot_r','ik_hand_gun','ik_hand_l','ik_hand_r'}
    if source_priority:
        # This explicit mode also filters finger/deforming rotations. Original
        # bone positions/scales, hierarchy and asset stay protected below.
        allowed=set(names)
    edits=set(d['edit_names']);position_edits=set(d['position_edit_names'])
    if not edits<=allowed or not position_edits<={'root','ik_foot_l','ik_foot_r','ik_hand_gun','ik_hand_l','ik_hand_r'}:
        raise ValueError('Unexpected edited tracks')
    for key,width in [('local_q',4),('local_p',3),('local_s',3)]:
        if len(d[key])!=n*b*width or not all(math.isfinite(x) for x in d[key]):raise ValueError('Invalid transforms')
    if d['local_s']!=original['local_s']:raise ValueError('Scale edits are prohibited')
    for f in range(n):
        for j,name in enumerate(names):
            at=(f*b+j)*4;r=d['local_q'][at:at+4];before=original['local_q'][at:at+4]
            norm=sum(x*x for x in r)
            if abs(norm-1)>1e-4:raise ValueError('Non-unit quaternion')
            if name not in edits and r!=before:raise ValueError('Protected rotation changed: '+name)
            if name in edits and not name.startswith('ik_') and abs(sum(x*y for x,y in zip(r,before)))<math.cos(math.radians(90)/2):
                raise ValueError('Correction exceeds 90 degrees: '+name)
            at=(f*b+j)*3
            if name not in position_edits and d['local_p'][at:at+3]!=original['local_p'][at:at+3]:
                raise ValueError('Protected translation changed: '+name)
    source=unreal.load_asset(d['source_asset']);mesh=unreal.load_asset(d['target_mesh'])
    if settings.get_editor_property('target_mesh')!=mesh:raise ValueError('Current target mesh changed')
    check=work/('preflight-'+uuid.uuid4().hex)
    if not library.export_body_contact_data(source,mesh,str(check)):raise RuntimeError('Clean source validation failed')
    fresh=json.loads((check/'request.json').read_text())
    if any(fresh[k]!=original[k] for k in ['source_guid','local_q','local_p','local_s','names','frames','fps']):
        raise ValueError('Source was edited after preparation')
    out=unreal.EditorAssetLibrary.duplicate_asset(source.get_path_name(),output_package)
    if not out:raise RuntimeError('New animation creation failed')
    controller=out.get_editor_property('controller');controller.open_bracket('Original-video contact refinement',False)
    try:
        for name in dict.fromkeys(d['edit_names']+d['position_edit_names']):
            j=names.index(name)
            positions=[unreal.Vector(*d['local_p'][(f*b+j)*3:(f*b+j)*3+3]) for f in range(n)]
            rotations=[unreal.Quat(*d['local_q'][(f*b+j)*4:(f*b+j)*4+4]) for f in range(n)]
            scales=[unreal.Vector(*d['local_s'][(f*b+j)*3:(f*b+j)*3+3]) for f in range(n)]
            if not controller.set_bone_track_keys(name,positions,rotations,scales,False):
                raise RuntimeError('Track write failed; new partial output retained: '+name)
    finally:controller.close_bracket(False)
    unreal.EditorAssetLibrary.set_metadata_tag(out,'CubelessSourceContactSource',source.get_path_name())
    unreal.EditorAssetLibrary.set_metadata_tag(out,'CubelessSourceContactEvidence',str(result_file))
    if source_priority:unreal.EditorAssetLibrary.set_metadata_tag(out,'CubelessSourcePriorityContact','Original video contact + temporal cleanup; original-video observations only')
    unreal.EditorAssetLibrary.set_metadata_tag(out,'CubelessMocapDebugRun',d['debug_run'])
    if not unreal.EditorAssetLibrary.save_loaded_asset(out,False):raise RuntimeError('Save failed; new output retained')
    settings.set_editor_property('hand_flow_animation',source);settings.set_editor_property('corrected_hand_animation',out)
    settings.set_editor_property('preview_animation',out);settings.set_editor_property('exported_sequence',out)
    settings.set_editor_property('save_hand_flow',True);settings.set_editor_property('save_bone_correction',True)
    settings.set_editor_property('sequence_name',out.get_name());settings.set_editor_property('output_folder',output_package.rsplit('/',1)[0])
    library.load_preview()
    receipt=dict(animation=out.get_path_name(),source=source.get_path_name(),source_unchanged=True,
                 mesh=mesh.get_path_name(),result_file=str(result_file),source_priority_contact=source_priority,measured_depth=False)
    (work/'applied.json').write_text(json.dumps(receipt,indent=2))
    return receipt
