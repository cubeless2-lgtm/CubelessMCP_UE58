# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Modified by Epic Games, Inc. (2026): correct_motion now calls the native UE C++ correction.
# The request round-trips through files in KIMODO_CORRECTION_DIR that the editor services each
# tick (UKimodoPoseGeneratorEditor::TickInference).
#
# Modified by Epic Games, Inc. (2026): the request manifest is written with allow_nan=False, since
# Python emits bare NaN and Infinity that the editor's JSON parser rejects, and the wait loop also
# watches for kimodo_correction_error.json so a rejected request fails at once instead of blocking
# for the full timeout.
#
# Modified by Epic Games, Inc. (2026): the manifest files are read and written with an explicit
# UTF-8 encoding, matching the editor side, instead of the locale default (cp1252 on Windows).

import json
import math
import os
import time
import uuid

import numpy as np
import torch


def _as_float32_array(values):
    # Convert a torch tensor, numpy array, or sequence into a flat float32 numpy array.
    if isinstance(values, torch.Tensor):
        return values.detach().cpu().flatten().to(torch.float32).numpy()
    return np.asarray(values, dtype=np.float32).ravel()


# The float arrays travel in one binary blob, concatenated in this fixed order. The manifest
# carries the per-array counts so the C++ side can slice the blob back apart.
_REQUEST_ARRAY_ORDER = (
    "root_translations",
    "joint_rotations",
    "root_translations_target",
    "joint_rotations_target",
    "full_body_mask",
    "left_hand_mask",
    "right_hand_mask",
    "left_foot_mask",
    "right_foot_mask",
    "root_mask",
    "contacts",
    "joint_ref_translations",
    "joint_ref_rotations",
)

# Generous because the wait is passive polling: right after a first-run model load the editor's
# game thread can stall for a long time under memory pressure, and a modal dialog blocks it fully.
_CORRECTION_RESULT_TIMEOUT_SECONDS = 300.0


def _correct_motion_via_editor(arrays, joint_parents, joint_indices, contact_threshold,
                               root_margin, has_double_ankle_joints):
    """Send one correction request to the editor process through files and block on the result.
    Returns (corrected_root_translations, corrected_joint_rotations) as flat float32 arrays.
    """
    exchange_dir = os.environ.get("KIMODO_CORRECTION_DIR", "")
    if not exchange_dir or not os.path.isdir(exchange_dir):
        raise RuntimeError(
            "correct_motion: KIMODO_CORRECTION_DIR is not set or does not exist; the editor "
            "correction service is unavailable outside the Kimodo inference worker.")

    request_bin_path = os.path.join(exchange_dir, "kimodo_correction_request.bin")
    request_json_path = os.path.join(exchange_dir, "kimodo_correction_request.json")
    result_bin_path = os.path.join(exchange_dir, "kimodo_correction_result.bin")
    result_json_path = os.path.join(exchange_dir, "kimodo_correction_result.json")
    error_json_path = os.path.join(exchange_dir, "kimodo_correction_error.json")

    # Drop any stale result or rejection so we never mistake a previous request's output for ours.
    for stale_path in (result_json_path, result_bin_path, error_json_path):
        try:
            os.remove(stale_path)
        except OSError:
            pass

    token = uuid.uuid4().hex

    blob = np.concatenate([arrays[name] for name in _REQUEST_ARRAY_ORDER])
    with open(request_bin_path, "wb") as request_bin_file:
        request_bin_file.write(blob.astype(np.float32).tobytes())

    manifest = {
        "token": token,
        "array_counts": {name: int(arrays[name].size) for name in _REQUEST_ARRAY_ORDER},
        "joint_parents": [int(parent) for parent in joint_parents],
        "left_hand_idx": int(joint_indices["left_hand"]),
        "right_hand_idx": int(joint_indices["right_hand"]),
        "left_foot_idx": int(joint_indices["left_foot"]),
        "right_foot_idx": int(joint_indices["right_foot"]),
        "contact_threshold": float(contact_threshold),
        "root_margin": float(root_margin),
        "has_double_ankle_joints": bool(has_double_ankle_joints),
    }

    # Publish the manifest atomically after the blob so the editor never observes a half request.
    # allow_nan=False because Python writes bare NaN and Infinity, which are not valid JSON and the
    # editor's parser rejects outright. Failing here names the bad value instead of stalling on a
    # manifest the editor can never read.
    manifest_temp_path = request_json_path + ".tmp"
    with open(manifest_temp_path, "w", encoding="utf-8") as manifest_file:
        try:
            json.dump(manifest, manifest_file, allow_nan=False)
        except ValueError as error:
            non_finite = {key: value for key, value in manifest.items()
                          if isinstance(value, float) and not math.isfinite(value)}
            raise RuntimeError(
                f"correct_motion: Refusing to publish a correction request with non-finite "
                f"values {non_finite}; the editor cannot parse it.") from error
    os.replace(manifest_temp_path, request_json_path)

    deadline = time.monotonic() + _CORRECTION_RESULT_TIMEOUT_SECONDS
    while True:
        # The editor publishes this when it cannot use the request at all. Without it a rejected
        # request is indistinguishable from a slow one and costs the full timeout.
        try:
            with open(error_json_path, encoding="utf-8") as error_json_file:
                service_error = json.load(error_json_file)
            if service_error.get("token") in (token, None, ""):
                raise RuntimeError(
                    "correct_motion: The editor rejected the motion correction request: "
                    f"{service_error.get('reason', 'no reason given')}")
        except (OSError, ValueError):
            pass

        if time.monotonic() > deadline:
            raise RuntimeError(
                "correct_motion: Timed out waiting for the editor to service the motion "
                "correction request.")
        try:
            with open(result_json_path, encoding="utf-8") as result_json_file:
                result = json.load(result_json_file)
            if result.get("token") == token:
                break
        except (OSError, ValueError):
            pass
        time.sleep(0.005)

    root_count = int(result["root_translations_num"])
    rotations_count = int(result["joint_rotations_num"])
    result_blob = np.fromfile(result_bin_path, dtype=np.float32,
                              count=root_count + rotations_count)
    if result_blob.size != root_count + rotations_count:
        raise RuntimeError("correct_motion: Correction result blob is truncated.")

    for result_path in (result_json_path, result_bin_path):
        try:
            os.remove(result_path)
        except OSError:
            pass

    return result_blob[:root_count], result_blob[root_count:]


def correct_motion(
    hipTranslations,
    jointRotations,
    contacts,
    hipTranslationsInput,
    rotationsInput,
    constraint_masks,
    contact_threshold,
    root_margin,
    working_rig,
    has_double_ankle_joints=False,
):
    joint_names = [x.name for x in working_rig]
    joint_parents = [
        joint_names.index(working_rig[i].parent) if working_rig[i].parent in joint_names else -1
        for i in range(len(working_rig))
    ]
    joint_ref_translations = [list(x.t_pose_translation) for x in working_rig]
    joint_ref_rotations = [list(x.t_pose_rotation) for x in working_rig]

    left_hand_idx = [i for i in range(len(joint_names)) if working_rig[i].retarget_tag == "LeftHand"]
    if len(left_hand_idx) != 1:
        raise RuntimeError(f"correct_motion: Expected exactly one joint with LeftHand tag")
    left_hand_idx = left_hand_idx[0]

    right_hand_idx = [i for i in range(len(joint_names)) if working_rig[i].retarget_tag == "RightHand"]
    if len(right_hand_idx) != 1:
        raise RuntimeError(f"correct_motion: Expected exactly one joint with RightHand tag")
    right_hand_idx = right_hand_idx[0]

    left_foot_idx = [i for i in range(len(joint_names)) if working_rig[i].retarget_tag == "LeftFoot"]
    if len(left_foot_idx) != 1:
        raise RuntimeError(f"correct_motion: Expected exactly one joint with LeftFoot tag")
    left_foot_idx = left_foot_idx[0]

    right_foot_idx = [i for i in range(len(joint_names)) if working_rig[i].retarget_tag == "RightFoot"]
    if len(right_foot_idx) != 1:
        raise RuntimeError(f"correct_motion: Expected exactly one joint with RightFoot tag")
    right_foot_idx = right_foot_idx[0]

    end_frame = hipTranslations.shape[1]

    default_mask = torch.zeros(hipTranslations.shape[1], dtype=torch.float32)
    root_mask = constraint_masks.get("Root", default_mask)
    full_body_mask = constraint_masks.get("FullBody", default_mask)
    left_hand_mask = constraint_masks.get("LeftHand", default_mask)
    right_hand_mask = constraint_masks.get("RightHand", default_mask)
    left_foot_mask = constraint_masks.get("LeftFoot", default_mask)
    right_foot_mask = constraint_masks.get("RightFoot", default_mask)

    # Skeleton metadata is the same for every batch item, so flatten it once. Ref
    # translations are [J*3] (xyz) and ref rotations are [J*4] (xyzw), matching the
    # CorrectMotion contract on the C++ side.
    joint_parents_flat = [int(p) for p in joint_parents]
    joint_ref_translations_flat = [float(c) for row in joint_ref_translations for c in row]
    joint_ref_rotations_flat = [float(c) for row in joint_ref_rotations for c in row]

    joint_indices = {
        "left_hand": left_hand_idx,
        "right_hand": right_hand_idx,
        "left_foot": left_foot_idx,
        "right_foot": right_foot_idx,
    }

    batch_size = hipTranslations.shape[0]

    for b in range(batch_size):
        hipTranslationsCorrected = hipTranslations[b, :end_frame].detach().cpu().flatten().numpy().astype(np.float32)
        rotationsCorrected = jointRotations[b, :end_frame].detach().cpu().flatten().numpy().astype(np.float32)

        hipTranslationsInput_flat = hipTranslationsInput.detach().cpu().flatten().numpy().astype(np.float32)
        rotationsInput_flat = rotationsInput.detach().cpu().flatten().numpy().astype(np.float32)
        ctcs = contacts[b].detach().cpu().flatten().numpy().astype(np.float32)

        arrays = {
            "root_translations": hipTranslationsCorrected,
            "joint_rotations": rotationsCorrected,
            "root_translations_target": hipTranslationsInput_flat,
            "joint_rotations_target": rotationsInput_flat,
            "full_body_mask": _as_float32_array(full_body_mask),
            "left_hand_mask": _as_float32_array(left_hand_mask),
            "right_hand_mask": _as_float32_array(right_hand_mask),
            "left_foot_mask": _as_float32_array(left_foot_mask),
            "right_foot_mask": _as_float32_array(right_foot_mask),
            "root_mask": _as_float32_array(root_mask),
            "contacts": ctcs,
            "joint_ref_translations": np.asarray(joint_ref_translations_flat, dtype=np.float32),
            "joint_ref_rotations": np.asarray(joint_ref_rotations_flat, dtype=np.float32),
        }
        hipTranslationsCorrected, rotationsCorrected = _correct_motion_via_editor(
            arrays, joint_parents_flat, joint_indices, contact_threshold, root_margin,
            has_double_ankle_joints)

        hipTranslations[b, :end_frame] = torch.from_numpy(
            hipTranslationsCorrected.reshape(*hipTranslations[b, :end_frame].shape)
        )
        jointRotations[b, :end_frame] = torch.from_numpy(
            rotationsCorrected.reshape(*jointRotations[b, :end_frame].shape)
        )
