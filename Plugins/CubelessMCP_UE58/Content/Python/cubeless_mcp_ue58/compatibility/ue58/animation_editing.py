"""UE 5.8 editor API adapter. No registry or server dependency."""
import unreal
import json
import uuid
from cubeless_mcp_ue58.operations import editor_data

# Mark before acquisition so a load/result exception cannot authorize a later
# warm save. Native weak-object tracking carries this across paths/module reload.
_unobserved_load_paths = globals().get('_unobserved_load_paths', set())


def edit(asset_path, action, arguments_json, execute, save, allow_save_dirty_package):
    load_attempted = False
    load_guard_required = False
    resolved_guard_paths = ()
    resolved_is_sequence = False
    def account_load(raw):
        result = json.loads(raw)
        if result.get('montage_load_guard_recorded') is True or resolved_is_sequence:
            # Native now owns the weak-object guard (or this is a Sequence).
            # Do not leave a path-only taint on a future new UObject at this name.
            for key in resolved_guard_paths:
                _unobserved_load_paths.discard(key)
        result['asset_load_attempted'] = load_attempted
        result['native_observation_scope'] = 'after_asset_resolution'
        if load_attempted:
            # Acquisition can dispatch PostLoad/global callbacks before the native
            # writer's snapshot. Do not label that interval as observed unchanged.
            result['native_changed'] = result.get('changed')
            result['native_change_state_known'] = result.get('change_state_known')
            result['change_state_known'] = False
            result['changed'] = True if result.get('changed') is True else None
            result['retry_risk'] = 'inspect_actual_target_after_asset_load'
        return json.dumps(result)
    try:
        editor_data._validate_path(asset_path)
        object_path = asset_path if '.' in asset_path.rsplit('/', 1)[-1] else asset_path + '.' + asset_path.rsplit('/', 1)[-1]
        previous = unreal.find_object(None, object_path)
        load_attempted = previous is None or not unreal.SystemLibrary.is_valid(previous)
        if load_attempted:
            _unobserved_load_paths.add(object_path.casefold())
        asset = editor_data._object(asset_path)
        actual_path = asset.get_path_name().casefold()
        resolved_guard_paths = (object_path.casefold(), actual_path)
        resolved_is_sequence = isinstance(asset, unreal.AnimSequence)
        load_guard_required = load_attempted or object_path.casefold() in _unobserved_load_paths or actual_path in _unobserved_load_paths
        if load_guard_required:
            _unobserved_load_paths.add(actual_path)
        function = unreal.CubelessAnimationEditingLibrary.edit_animation_asset
    except Exception as error:
        return account_load(editor_data._failure(action, error))
    token = uuid.uuid4().hex
    try:
        raw = function(asset, action, arguments_json, execute, save, allow_save_dirty_package, token, load_guard_required)
        accounted = account_load(raw)
    except Exception as error:
        # Read the outcome of this exact native call. This never repeats the edit.
        try:
            receipt = json.loads(unreal.CubelessAnimationEditingLibrary.take_animation_edit_receipt(token))
            if receipt.get('request_token') != token or receipt.get('operation') != action:
                raise ValueError('Native receipt identity mismatch')
        except Exception as receipt_error:
            receipt = json.loads(editor_data._failure(action, error, dispatched=True, mutation=execute))
            receipt.update(save_attempted=None, saved=None, save_state_known=False,
                           receipt_error=str(receipt_error), change_state_known=False,
                           changed=None, retry_risk='inspect_actual_target_and_disk')
        else:
            receipt.update(native_success=receipt.get('success'), native_stage=receipt.get('stage'),
                           success=False, stage='adapter_exception_after_native_return',
                           adapter_error=str(error), receipt_recovered=True)
            if not receipt.get('error'):
                receipt['error'] = str(error)
        return account_load(json.dumps(receipt))
    else:
        # Receipts are only needed across a failed result-delivery boundary.
        try:
            unreal.CubelessAnimationEditingLibrary.take_animation_edit_receipt(token)
        except Exception:
            pass  # Bounded native receipt cache; the returned operation is authoritative.
        return accounted
