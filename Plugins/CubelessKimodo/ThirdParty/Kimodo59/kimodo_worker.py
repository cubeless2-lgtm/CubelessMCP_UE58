# -*- coding: utf-8 -*-
"""
Persistent out-of-process inference worker for Kimodo.
Copyright Epic Games, Inc. All Rights Reserved.

Runs in a normal Python process (so torch executes on this process's own main thread, which the
editor's embedded interpreter could not do reliably on a worker thread). Communicates with the
editor through GUID named shared memory, mirroring the AnimGen/LearningCore pattern: a small int32
control block plus float and byte buffers for the trajectory inputs and the generated outputs.
Strings, scalars and the constraint arrays travel through small request/result JSON files. Motion
correction round-trips to the editor process through files in KIMODO_CORRECTION_DIR, serviced by
UKimodoPoseGeneratorEditor::TickInference.
"""

import importlib.util
import json
import os
import subprocess
import sys
import time
import traceback


# Control block layout. Must match AnimGenOfflineWorkerProcess.cpp. CTRL_STATUS_CODE carries a
# generator-defined phase code (the KIMODO_STATUS_* values), 0 = no special phase.
CTRL_REQUEST_ID = 0
CTRL_STATUS = 1
CTRL_PROGRESS = 2
CTRL_CANCEL = 3
CTRL_EXIT = 4
CTRL_NUM_FRAMES_OUT = 5
CTRL_NUM_BONES_OUT = 6
CTRL_NUM_CONTACT_OUT = 7
CTRL_STATUS_CODE = 8
CTRL_GPU_MEM_USED_MIB = 9
CTRL_NUM_VARIATIONS_OUT = 10
CTRL_WORKER_STARTED = 11

STATUS_IDLE = 0
STATUS_RUNNING = 1
STATUS_DONE = 2
STATUS_ERROR = 3
STATUS_CANCELLED = 4


def _publish_worker_started(control):
    if control.size > CTRL_WORKER_STARTED:
        control[CTRL_WORKER_STARTED] = 1


def _publish_status_code(control, status_code):
    # Slot 8 needs no length guard: it predates every editor build this worker can be launched by,
    # whose control block has always been at least 10 slots long. Slots 10 and 11 came later.
    control[CTRL_STATUS_CODE] = int(status_code)
    _publish_worker_started(control)


# Plugin-local wheels UE's PipInstall cannot install (antlr4 is sdist-only, omegaconf needs it).
# Install order matters: antlr4 must exist before omegaconf is imported.
_BOOTSTRAP_WHEELS = (
    ("antlr4_python3_runtime-4.9.3-py3-none-any.whl", "antlr4"),
    ("omegaconf-2.3.0-py3-none-any.whl", "omegaconf"),
)


def _ensure_plugin_wheels_installed():
    """Install plugin-local wheels into this venv when missing. This process runs the venv python,
    so sys.executable is the right pip target. Installs persist, making this a one-time cost.
    """
    # This script lives at <plugin root>/Content/Python/kimodo/, so walk three folders up.
    script_dir = os.path.dirname(os.path.abspath(__file__))
    plugin_root_dir = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    wheel_dir = os.path.join(plugin_root_dir, "Content", "Python", "Lib", "wheels")

    for wheel_filename, import_name in _BOOTSTRAP_WHEELS:
        if importlib.util.find_spec(import_name) is not None:
            continue
        wheel_path = os.path.join(wheel_dir, wheel_filename)
        if not os.path.exists(wheel_path):
            print(f"[KimodoWorker] Wheel not found: {wheel_path}; "
                  f"{import_name} will be unavailable", flush=True)
            continue
        print(f"[KimodoWorker] Installing wheel: {wheel_filename}", flush=True)
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", "--no-deps", "--no-index", wheel_path],
            capture_output=True, text=True)
        if result.returncode != 0:
            print(f"[KimodoWorker] {import_name} install failed (exit "
                  f"{result.returncode}):\n{result.stdout}{result.stderr}", flush=True)


def _quiet_transformers_logging():
    """Lower the transformers log level to ERROR; its 'LOAD REPORT' goes through warning() to
    stderr, which UE paints red. Only affects the transformers logger.
    """
    try:
        import transformers
        transformers.logging.set_verbosity_error()
    except ImportError:
        pass


def _make_gpu_memory_publisher(control):
    """Returns a throttled callable that publishes this process's GPU memory footprint (in MiB) into
    the control block. The footprint reads 0 until a model loads, so the publisher needs no device
    init of its own. Works on both CUDA and ROCm builds of torch.

    Reports memory_allocated so the readout reflects active tensors instead of PyTorch's reserved
    caching allocator pool.
    Pass force=True to bypass the throttle after an unload, so the drop shows up immediately.
    """
    last_publish_time = [0.0]

    def publish_gpu_memory(force=False):
        now = time.monotonic()
        if not force and now - last_publish_time[0] < 1.0:
            return
        last_publish_time[0] = now
        try:
            torch = sys.modules.get("torch")
            if torch is None or not torch.cuda.is_available():
                return
            control[CTRL_GPU_MEM_USED_MIB] = torch.cuda.memory_allocated() // (1024 * 1024)
        except Exception:
            pass

    return publish_gpu_memory


def _set_env_or_clear(name, value):
    """Set an env var, or remove it when the value is empty. Clearing on empty matters because the
    worker persists across requests and must never keep a value the user just removed in preferences.
    """
    if value:
        os.environ[name] = value
    else:
        os.environ.pop(name, None)


def _apply_request_environment(request):
    """Push the per-request settings values into os.environ before the model loads. Per-request
    delivery lets the user change the token, the model device, or the text encoder placement in
    Editor Preferences without a relaunch.
    """
    # HF_TOKEN falls back to the cached login when cleared.
    _set_env_or_clear("HF_TOKEN", str(request.get("hf_token", "")).strip())
    _set_env_or_clear("KIMODO_DEVICE", str(request.get("kimodo_device", "")).strip())
    _set_env_or_clear("KIMODO_TEXT_ENCODER_DEVICE",
                      str(request.get("kimodo_text_encoder_device", "")).strip())
    # Where the merged quantized text encoder is cached. Empty turns the cache off, and an older
    # editor build that does not send it leaves the feature dormant.
    _set_env_or_clear("KIMODO_TEXT_ENCODER_CACHE_DIR",
                      str(request.get("text_encoder_cache_dir", "")).strip())

    # Skip Kimodo's gradio-client text-encoder probe; no Gradio service is hosted here.
    os.environ.setdefault("TEXT_ENCODER_MODE", "local")


def main():
    if len(sys.argv) != 2:
        print("[KimodoWorker] Expected a single config path argument", flush=True)
        return 1

    # The editor writes the config and request files as UTF-8, so read them explicitly as UTF-8
    # instead of the locale default, which is cp1252 on a typical Windows machine.
    with open(sys.argv[1], encoding="utf-8") as config_file:
        config = json.load(config_file)

    # Set up imports: LearningCore for the shared memory class, plus the generator's own
    # Content/Python. The venv interpreter already provides torch and numpy.
    sys.path.insert(0, config["LearningCorePythonPath"])
    for extra_path in config.get("ExtraPythonPaths", []):
        sys.path.insert(0, extra_path)

    # Start watching the editor before the heavy imports, so a worker relaunched against an editor
    # that has already gone exits before it spends time and VRAM loading a model.
    from animgenoffline import parent_watchdog
    parent_watchdog.start(config.get("EditorProcessId"), log_prefix="KimodoWorker")

    import numpy as np
    from learning_core.communicators.shared_memory import SharedMemory

    max_frames = int(config["MaxFrames"])
    max_bones = int(config["MaxBones"])
    max_contacts = int(config["MaxContactPoints"])
    control_num = int(config["ControlNum"])
    request_json_path = config["RequestJsonPath"]
    result_json_path = config["ResultJsonPath"]

    # Motion correction round-trips through files next to the request/result JSON. The editor's
    # TickInference services them with the native C++ correction.
    os.environ["KIMODO_CORRECTION_DIR"] = os.path.dirname(os.path.abspath(request_json_path))

    # Bootstrap the wheels UE's PipInstall cannot install before kimodo_generate imports the
    # vendored kimodo package (which needs omegaconf at import time).
    _ensure_plugin_wheels_installed()
    _quiet_transformers_logging()

    import kimodo_generate

    def map_array(guid, count, dtype):
        size = int(count) * np.dtype(dtype).itemsize
        handle = SharedMemory(guid, create=False, size=size)
        array = np.frombuffer(handle.buf, dtype=dtype, count=int(count))
        return handle, array

    # Keep the handles alive for the lifetime of the process so the mappings stay open.
    control_h, control = map_array(config["ControlGuid"], control_num, np.int32)
    trajpos_h, trajpos = map_array(config["TrajPosGuid"], max_frames * 3, np.float32)
    trajorient_h, trajorient = map_array(config["TrajOrientGuid"], max_frames * 4, np.float32)
    outpos_h, outpos = map_array(config["OutPosGuid"], max_frames * max_bones * 3, np.float32)
    outrot_h, outrot = map_array(config["OutRotGuid"], max_frames * max_bones * 4, np.float32)
    outcontact_h, outcontact = map_array(config["OutContactGuid"], max_frames * max_contacts, np.uint8)

    # Optional UTF-8 status detail block next to the status code, absent with an older editor build.
    status_detail_bytes = int(config.get("StatusDetailNumBytes", 0))
    status_detail = None
    if status_detail_bytes > 0 and config.get("StatusDetailGuid"):
        status_detail_h, status_detail = map_array(
            config["StatusDetailGuid"], status_detail_bytes, np.uint8)

    print("[KimodoWorker] Ready, waiting for requests", flush=True)

    publish_gpu_memory = _make_gpu_memory_publisher(control)

    # The editor resets the control block to zero before launch, so the first real request is id 1.
    last_request_id = 0

    # Idle unload policy, delivered per request so a settings change applies from the next generate.
    # Zero disables it and keeps the model resident until the user unloads it from the menu.
    idle_unload_seconds = 0.0
    last_activity_time = time.monotonic()

    while True:
        if int(control[CTRL_EXIT]) != 0:
            print("[KimodoWorker] Exit requested", flush=True)
            break

        request_id = int(control[CTRL_REQUEST_ID])
        if request_id == last_request_id:
            publish_gpu_memory()
            # Backs up the watchdog thread in case it died to an unhandled exception.
            parent_watchdog.check_now()
            # Only reached with no job in flight, but test the status explicitly so a future refactor
            # of this branch cannot start unloading a model out from under a running generation.
            if (idle_unload_seconds > 0.0
                    and int(control[CTRL_STATUS]) != STATUS_RUNNING
                    and time.monotonic() - last_activity_time > idle_unload_seconds):
                try:
                    # A complete unload, releasing VRAM and system RAM as the editor's Unload Model
                    # When Idle tooltip promises. The default CPU park mode is not used here.
                    kimodo_generate.unload_kimodo(mode="complete")
                except Exception as error:
                    print(f"[KimodoWorker] Idle unload failed: {error}", flush=True)
                # Reset first so a failed unload cannot retry every 5 ms, then force publish so the
                # editor's readout shows the drop within a second instead of up to a second late.
                last_activity_time = time.monotonic()
                publish_gpu_memory(force=True)
            time.sleep(0.005)
            continue

        last_request_id = request_id
        last_activity_time = time.monotonic()

        # Zero the published output counts up front, so a failed request can never leave the previous
        # request's counts advertising stale bytes in the persistent output buffers.
        control[CTRL_NUM_FRAMES_OUT] = 0
        control[CTRL_NUM_BONES_OUT] = 0
        control[CTRL_NUM_CONTACT_OUT] = 0
        if control.size > CTRL_NUM_VARIATIONS_OUT:
            control[CTRL_NUM_VARIATIONS_OUT] = 0

        try:
            with open(request_json_path, encoding="utf-8") as request_file:
                request = json.load(request_file)

            _apply_request_environment(request)
            idle_unload_seconds = float(request.get("idle_unload_seconds", 0.0))

            num_frames = int(request["num_frames"])
            use_trajectory = bool(request["use_trajectory"])

            # Variations pack variation-major into the fixed output buffers, so an oversized frame
            # request is rejected here instead of burning a full inference. The bone and contact
            # dimensions are only known post-inference, where the output checks cover them. Both
            # counts need a lower bound first, else a non-positive value makes the budget product
            # vacuous and the trajectory slices below would silently truncate.
            num_variations = int(request.get("num_variations", 1))
            if num_frames < 1 or num_variations < 1:
                raise RuntimeError(
                    f"Request of {num_variations} variations x {num_frames} frames is invalid.")
            if num_variations > kimodo_generate.KIMODO_MAX_VARIATIONS:
                raise RuntimeError(
                    f"Request of {num_variations} variations exceeds the "
                    f"{kimodo_generate.KIMODO_MAX_VARIATIONS} variation cap.")
            if num_variations * num_frames > max_frames:
                raise RuntimeError(
                    f"Request of {num_variations} variations x {num_frames} frames exceeds the "
                    f"{max_frames} frame output budget.")

            # Kimodo treats a non-empty trajectory buffer as a dense Root2D constraint, so only
            # slice the shared buffers when the trajectory drives the generation.
            input_dict = {
                "num_frames": num_frames,
                "frame_rate": int(request["frame_rate"]),
                "use_trajectory": use_trajectory,
                "park_text_encoder": bool(request.get("park_text_encoder", False)),
                "text_encoder_precision": str(request.get("text_encoder_precision", "")),
                "trajectory_positions": trajpos[:num_frames * 3].tolist() if use_trajectory else [],
                "trajectory_orientations": trajorient[:num_frames * 4].tolist() if use_trajectory else [],
                "description": request.get("description", ""),
                "segment_descriptions": request.get("segment_descriptions", []),
                "segment_num_frames": request.get("segment_num_frames", []),
                "random_seed": int(request.get("random_seed", 0)),
                "num_variations": num_variations,
                "diffusion_steps": int(request.get("diffusion_steps", 100)),
                "num_transition_frames": int(request.get("num_transition_frames", 5)),
                "percentage_transition_override": float(request.get("percentage_transition_override", 0.10)),
                "first_heading_orientation": request.get("first_heading_orientation", []),
                "post_processing": bool(request.get("post_processing", True)),
                "root_margin": float(request.get("root_margin", 0.04)),
                "foot_contact_threshold": float(request.get("foot_contact_threshold", 0.5)),
                "enable_cfg": bool(request.get("enable_cfg", True)),
                "cfg_text_weight": float(request.get("cfg_text_weight", 2.0)),
                "cfg_constraint_weight": float(request.get("cfg_constraint_weight", 2.0)),
                "target_pose_frame_indices": request.get("target_pose_frame_indices", []),
                "target_pose_bone_names": request.get("target_pose_bone_names", []),
                "target_pose_positions": request.get("target_pose_positions", []),
                "target_pose_rotations": request.get("target_pose_rotations", []),
                "target_pose_fullbody_flags": request.get("target_pose_fullbody_flags", []),
                "target_effector_frame_indices": request.get("target_effector_frame_indices", []),
                "target_effector_names": request.get("target_effector_names", []),
                "target_transform_frame_indices": request.get("target_transform_frame_indices", []),
                "target_transform_positions": request.get("target_transform_positions", []),
                "target_transform_orientations": request.get("target_transform_orientations", []),
                "target_transform_heading_flags": request.get("target_transform_heading_flags", []),
            }

            def progress_cb(fraction):
                control[CTRL_PROGRESS] = int(max(0.0, min(1.0, fraction)) * 1000)
                _publish_worker_started(control)
                # The main loop is blocked inside infer_only, so keep the stats fresh from here.
                publish_gpu_memory()

            def cancel_cb():
                return int(control[CTRL_CANCEL]) != 0

            def status_code_cb(status_code):
                _publish_status_code(control, status_code)

            def status_detail_cb(text):
                if status_detail is None:
                    return
                encoded = str(text).encode("utf-8")[:status_detail_bytes - 1]
                # The byte-level cut can split a multi-byte UTF-8 sequence, drop any dangling tail
                encoded = encoded.decode("utf-8", errors="ignore").encode("utf-8")
                if encoded:
                    status_detail[:len(encoded)] = np.frombuffer(encoded, dtype=np.uint8)
                status_detail[len(encoded)] = 0

            output = kimodo_generate.infer_only(input_dict, progress_cb=progress_cb,
                                                cancel_cb=cancel_cb, status_code_cb=status_code_cb,
                                                status_detail_cb=status_detail_cb)

            result_code = int(output.get("result_code", -1))
            if result_code == kimodo_generate.KIMODO_GENERATE_CANCELLED:
                _publish_worker_started(control)
                control[CTRL_STATUS] = STATUS_CANCELLED
                continue

            if result_code != 0:
                with open(result_json_path, "w", encoding="utf-8") as result_file:
                    json.dump({"result_code": result_code, "frame_rate": 0, "num_bones": 0,
                               "bone_names": [], "contact_point_names": [],
                               "contact_rear_indices": [], "contact_front_indices": [],
                               "error_message": output.get("error_message", "")}, result_file)
                _publish_worker_started(control)
                control[CTRL_STATUS] = STATUS_ERROR
                continue

            bone_names = output["bone_names"]
            num_bones = len(bone_names)
            contact_names = output["contact_point_names"]
            num_contact = len(contact_names)

            positions = np.asarray(output["position_values"], dtype=np.float32)
            rotations = np.asarray(output["rotation_values"], dtype=np.float32)
            contact_flags = np.asarray(output["contact_flags"], dtype=np.uint8)

            # The editor reads exactly the published counts back out of the persistent shared
            # buffers, so any size mismatch would expose stale bytes of a previous request as data.
            # The produced count keeps its own name so the requested count stays comparable below.
            out_variations = int(output.get("num_variations", 1))
            out_frames = int(output["num_frames"])

            # The editor validates the published counts against the same dimensions, so reject an
            # out-of-range count here with a real message instead of failing the whole read there.
            if out_frames > max_frames or num_bones > max_bones or num_contact > max_contacts:
                raise RuntimeError(
                    f"Output of {out_frames} frames, {num_bones} bones and {num_contact} contact "
                    f"points exceeds the shared buffer dimensions ({max_frames} frames, "
                    f"{max_bones} bones, {max_contacts} contact points).")

            # A non-positive produced count would zero the expected sizes, slipping the equality
            # check below while the editor clamps the published count back up to one variation of
            # stale buffer bytes. More than requested would fail the editor's cross-check with a
            # generic protocol error, so both bounds fail here with a real message instead.
            if not 1 <= out_variations <= num_variations:
                raise RuntimeError(
                    f"Output carries {out_variations} variations for a request of "
                    f"{num_variations}.")

            # The frame count strides the same bulk arrays, so it carries both bounds too: zero would
            # zero the expected sizes below as well, and the editor rejects more than requested.
            if not 1 <= out_frames <= num_frames:
                raise RuntimeError(
                    f"Output carries {out_frames} frames for a request of {num_frames}.")

            # Bones stride the position and rotation arrays, so an empty bone list would zero both
            # expected sizes and slip the equality check, failing later as a generic editor error.
            if num_bones < 1:
                raise RuntimeError("Output carries no bone names.")

            expected_positions = out_variations * out_frames * num_bones * 3
            expected_rotations = out_variations * out_frames * num_bones * 4
            expected_contacts = out_variations * out_frames * num_contact
            if (positions.size != expected_positions or rotations.size != expected_rotations
                    or contact_flags.size != expected_contacts):
                raise RuntimeError(
                    f"Output sizes (positions={positions.size}, rotations={rotations.size}, "
                    f"contacts={contact_flags.size}) do not match the published counts "
                    f"(variations={out_variations}, frames={out_frames}, bones={num_bones}, "
                    f"contact points={num_contact}).")

            if positions.size > outpos.size or rotations.size > outrot.size or contact_flags.size > outcontact.size:
                raise RuntimeError(
                    f"Output exceeds shared buffer capacity (variations={out_variations}, "
                    f"frames={out_frames}, bones={num_bones}, contact points={num_contact}).")

            # Write the bulk arrays into shared memory, then publish the counts.
            outpos[:positions.size] = positions
            outrot[:rotations.size] = rotations
            outcontact[:contact_flags.size] = contact_flags

            with open(result_json_path, "w", encoding="utf-8") as result_file:
                json.dump({
                    "result_code": 0,
                    "num_frames": int(output["num_frames"]),
                    "num_variations": out_variations,
                    "frame_rate": int(output["frame_rate"]),
                    "num_bones": num_bones,
                    "bone_names": list(bone_names),
                    "contact_point_names": list(contact_names),
                    "contact_rear_indices": list(output["contact_rear_indices"]),
                    "contact_front_indices": list(output["contact_front_indices"]),
                    "error_message": "",
                }, result_file)

            control[CTRL_NUM_FRAMES_OUT] = out_frames
            control[CTRL_NUM_BONES_OUT] = num_bones
            control[CTRL_NUM_CONTACT_OUT] = num_contact
            # The variations slot is absent with an older editor build, whose control block is
            # shorter. Such an editor reads a single variation, so the count can be skipped.
            if control.size > CTRL_NUM_VARIATIONS_OUT:
                control[CTRL_NUM_VARIATIONS_OUT] = out_variations
            _publish_worker_started(control)
            control[CTRL_STATUS] = STATUS_DONE

        except Exception as error:
            print("[KimodoWorker] Exception:", str(error), flush=True)
            traceback.print_exc()
            try:
                with open(result_json_path, "w", encoding="utf-8") as result_file:
                    json.dump({"result_code": -1, "frame_rate": 0, "num_bones": 0,
                               "bone_names": [], "contact_point_names": [],
                               "contact_rear_indices": [], "contact_front_indices": [],
                               "error_message": str(error)}, result_file)
            except Exception:
                pass
            _publish_worker_started(control)
            control[CTRL_STATUS] = STATUS_ERROR
        finally:
            # Time the idle window from when the work ended, not when it started, so a long
            # generation cannot leave the model eligible for unload the moment it finishes.
            last_activity_time = time.monotonic()

    return 0


if __name__ == "__main__":
    sys.exit(main())
