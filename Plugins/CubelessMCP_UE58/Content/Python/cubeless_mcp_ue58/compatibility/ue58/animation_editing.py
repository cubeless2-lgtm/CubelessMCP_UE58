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
    lookup_attempted = False
    acquisition_uncertain = False
    load_guard_required = False
    resolved_guard_paths = ()
    resolved_is_sequence = False
    acquisition_observation = None
    acquisition_cleanup_error = None
    def account_load(raw):
        result = json.loads(raw)
        if acquisition_observation is not None:
            result['acquisition_observation'] = acquisition_observation
            if acquisition_observation.get('requested_target_guard_recorded') is True:
                result['montage_load_guard_recorded'] = True
                result['montage_load_state_unverified'] = True
                # The native scope observed the destination even if _object never
                # returned it to Python. It now owns this identity's guard.
                _unobserved_load_paths.discard(object_path.casefold())
        if acquisition_cleanup_error is not None:
            result['acquisition_cleanup_error'] = str(acquisition_cleanup_error)
        if result.get('montage_load_guard_recorded') is True or resolved_is_sequence:
            # Native now owns the weak-object guard (or this is a Sequence).
            # Do not leave a path-only taint on a future new UObject at this name.
            for key in resolved_guard_paths:
                _unobserved_load_paths.discard(key)
        result['asset_lookup_attempted'] = lookup_attempted
        result['asset_acquisition_state_unverified'] = acquisition_uncertain
        result['asset_load_attempted'] = load_attempted
        result['native_observation_scope'] = 'after_asset_resolution'
        if load_attempted or acquisition_uncertain:
            # Acquisition can dispatch PostLoad/global callbacks before the native
            # writer's snapshot. Do not label that interval as observed unchanged.
            result['native_changed'] = result.get('changed')
            result['native_change_state_known'] = result.get('change_state_known')
            result['change_state_known'] = False
            result['changed'] = True if result.get('changed') is True else None
            result['retry_risk'] = 'inspect_actual_target_after_asset_load' if load_attempted else 'inspect_actual_target_after_asset_lookup'
        return json.dumps(result)
    try:
        editor_data._validate_path(asset_path)
        object_path = asset_path if '.' in asset_path.rsplit('/', 1)[-1] else asset_path + '.' + asset_path.rsplit('/', 1)[-1]
        scope_token = uuid.uuid4().hex
        acquisition_error = None
        try:
            # Admission and observation precede even find_object: installed
            # StaticFindObject may load during T3D import. Native rejects it here.
            begin = json.loads(unreal.CubelessAnimationEditingLibrary.begin_animation_asset_acquisition(object_path, scope_token))
            if begin.get('success') is not True or begin.get('request_token') != scope_token:
                raise RuntimeError('Acquisition observation rejected: ' + str(begin))
            lookup_attempted = True
            previous = unreal.find_object(None, object_path)
            needs_load = previous is None or not unreal.SystemLibrary.is_valid(previous)
            if needs_load:
                _unobserved_load_paths.add(object_path.casefold())
                load_attempted = True
                asset = editor_data._object(asset_path)
            else:
                asset = previous  # Reuse the checked object; no second lookup.
            actual_path = asset.get_path_name().casefold()
        except BaseException as error:
            acquisition_error = error
            if lookup_attempted:
                acquisition_uncertain = True
                _unobserved_load_paths.add(object_path.casefold())
        finally:
            # Also runs after a lost begin/lookup result. No loader/edit/save retry.
            try:
                acquisition_observation = json.loads(unreal.CubelessAnimationEditingLibrary.end_animation_asset_acquisition(scope_token, load_attempted or acquisition_uncertain))
                if acquisition_observation.get('success') is not True or acquisition_observation.get('request_token') != scope_token:
                    raise RuntimeError('Acquisition observation cleanup failed: ' + str(acquisition_observation))
            except Exception as error:
                acquisition_cleanup_error = error
                acquisition_uncertain = lookup_attempted
        if acquisition_error is not None:
            raise acquisition_error
        if acquisition_cleanup_error is not None:
            raise acquisition_cleanup_error
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
