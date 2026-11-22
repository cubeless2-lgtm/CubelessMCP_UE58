# -*- coding: utf-8 -*-
"""
Copyright Epic Games, Inc. All Rights Reserved.

Kimodo animation generation entry point, called from kimodo_worker.py with a plain input dict.
infer_only runs the diffusion model and returns the raw output arrays; the C++ FinalizeFrameData
phase converts them into UE frame data.

Coordinate system is Kimodo's Y-up right-handed; the C++ side handles UE conversion both ways.
Quaternions are wxyz from matrix_to_quaternion and reordered to xyzw for FQuat before flattening.
"""

import contextlib
import gc
import hashlib
import importlib.util
import json
import os
import shutil
import sys
import threading
import time
import types
from dataclasses import dataclass

from filelock import FileLock

# This script lives in the vendored root Content/Python/kimodo/, next to the kimodo/ package
# folder, so add this directory to sys.path so `import kimodo` resolves to the real package.
_VENDORED_KIMODO_ROOT = os.path.dirname(os.path.abspath(__file__))
if _VENDORED_KIMODO_ROOT not in sys.path:
    sys.path.insert(0, _VENDORED_KIMODO_ROOT)

# Import the motion_correction package from source for its motion_postprocess shim
# (the correction itself is native UE C++). Put its python/ folder on the path.
_MOTION_CORRECTION_ROOT = os.path.join(_VENDORED_KIMODO_ROOT, "MotionCorrection", "python")
if _MOTION_CORRECTION_ROOT not in sys.path:
    sys.path.insert(0, _MOTION_CORRECTION_ROOT)


def _purge_stale_kimodo_module():
    """If a prior import cached the wrong (namespace) kimodo module, drop it so
    the subsequent imports below resolve against the vendored package."""
    cached = sys.modules.get("kimodo")
    if cached is None:
        return
    cached_file = getattr(cached, "__file__", None) or ""
    if cached_file.startswith(_VENDORED_KIMODO_ROOT):
        return
    for module_name in [name for name in sys.modules
                        if name == "kimodo" or name.startswith("kimodo.")]:
        del sys.modules[module_name]


_purge_stale_kimodo_module()


def _install_hydra_shim():
    """Provide a minimal `hydra.utils.instantiate` to avoid the hydra-core dependency. Walks a
    config, finds `_target_` keys, and imports and calls the target with the other keys as kwargs.
    """
    import importlib

    def _instantiate(config, *args, **kwargs):
        # hydra.utils.instantiate supports positional overrides, but Kimodo
        # never uses them. Accept and ignore to keep the signature compatible.
        try:
            from omegaconf import DictConfig, ListConfig, OmegaConf
            if isinstance(config, (DictConfig, ListConfig)):
                config = OmegaConf.to_container(config, resolve=True)
        except ImportError:
            pass

        if isinstance(config, dict):
            if "_target_" in config:
                target_path = config["_target_"]
                target_kwargs = {
                    key: _instantiate(value)
                    for key, value in config.items()
                    if key != "_target_"
                }
                target_kwargs.update(kwargs)
                module_path, _, attribute_name = target_path.rpartition(".")
                target_module = importlib.import_module(module_path)
                target_callable = getattr(target_module, attribute_name)
                return target_callable(*args, **target_kwargs)
            return {key: _instantiate(value) for key, value in config.items()}
        if isinstance(config, list):
            return [_instantiate(item) for item in config]
        return config

    hydra_module = types.ModuleType("hydra")
    hydra_utils_module = types.ModuleType("hydra.utils")
    hydra_utils_module.instantiate = _instantiate
    hydra_module.utils = hydra_utils_module
    sys.modules["hydra"] = hydra_module
    sys.modules["hydra.utils"] = hydra_utils_module


# Absence from sys.modules only means hydra was not imported yet. Probe importability too, so an
# installed hydra-core package is never shadowed by the incomplete shim.
if "hydra" not in sys.modules and importlib.util.find_spec("hydra") is None:
    _install_hydra_shim()

import numpy as np
import torch

import kimodo
import kimodo_nf4
from kimodo.constraints import (
    EndEffectorConstraintSet,
    FullBodyConstraintSet,
    LeftFootConstraintSet,
    LeftHandConstraintSet,
    RightFootConstraintSet,
    RightHandConstraintSet,
    Root2DConstraintSet,
)
from kimodo.geometry import matrix_to_quaternion, quaternion_to_matrix
from kimodo.skeleton import SOMASkeleton30
from kimodo.tools import seed_everything


# Hips has no prebuilt subclass upstream, so it uses the base class. Its generic "end-effector" name is
# silently ignored by the postprocess mask dispatch, so relabel it to reuse the existing Root mask. Only
# the mask dispatch and a log message read .name, and the postprocess input extraction keys off the
# class, so the relabel does not change how the pose is applied.
_HIPS_EFFECTOR_NAME = "Hips"
_HIPS_CONSTRAINT_MASK_NAME = "root2d"


class _DeviceSafeEffectorMixin:
    """Keeps an effector constraint's joint index tensors on the same device as its frame indices.

    EndEffectorConstraintSet builds pos_indices and rot_indices with a plain torch.tensor, so they stay on
    the CPU while the frame indices and pose arrays are on the GPU, and create_pairs then raises a device
    mismatch. FullBodyConstraintSet passes the device explicitly, which is why only the effector path hits
    this. Doing it in __init__ also covers the multi-prompt path, where crop_move rebuilds through
    type(self). The index tensors have to end up on the frame indices' device rather than the CPU, because
    create_conditions concatenates the index tensors of every constraint before moving them.
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        device = self.frame_indices.device
        self.pos_indices = self.pos_indices.to(device)
        self.rot_indices = self.rot_indices.to(device)


class _LeftHandConstraint(_DeviceSafeEffectorMixin, LeftHandConstraintSet):
    pass


class _RightHandConstraint(_DeviceSafeEffectorMixin, RightHandConstraintSet):
    pass


class _LeftFootConstraint(_DeviceSafeEffectorMixin, LeftFootConstraintSet):
    pass


class _RightFootConstraint(_DeviceSafeEffectorMixin, RightFootConstraintSet):
    pass


class _HipsConstraint(_DeviceSafeEffectorMixin, EndEffectorConstraintSet):
    """Hips effector. Both attributes are class level so crop_move's rebuild keeps them."""
    name = _HIPS_CONSTRAINT_MASK_NAME
    joint_names = [_HIPS_EFFECTOR_NAME]

    def __init__(self, *args, **kwargs):
        kwargs.setdefault("joint_names", self.joint_names)
        super().__init__(*args, **kwargs)


# Constraint class per end effector name sent by the editor. Each limb class carries a name that
# postprocess._build_constraint_masks_dict recognizes, which is what earns it an IK pin.
_EFFECTOR_CONSTRAINT_CLASSES = {
    "LeftHand": _LeftHandConstraint,
    "RightHand": _RightHandConstraint,
    "LeftFoot": _LeftFootConstraint,
    "RightFoot": _RightFootConstraint,
    _HIPS_EFFECTOR_NAME: _HipsConstraint,
}

_LEGAL_EFFECTOR_NAMES = tuple(_EFFECTOR_CONSTRAINT_CLASSES)


# Cache survives importlib.reload(kimodo_generate) by living on a sentinel
# module entry rather than this module's namespace.
_PERSISTENT_STATE_KEY = "_kimodo_persistent_state"


def _get_persistent_state():
    state = sys.modules.get(_PERSISTENT_STATE_KEY)
    if state is None:
        state = types.ModuleType(_PERSISTENT_STATE_KEY)
        state.model_cache = None
        # Device the cached model was loaded for (the diffusion model runs bf16 on cuda, fp32 on
        # cpu; the text encoder dtype additionally follows loaded_text_encoder_device). Distinct
        # from the model_cache tuple's device element, which may be "cpu" after a Park-on-CPU unload.
        state.loaded_device = None
        sys.modules[_PERSISTENT_STATE_KEY] = state
    # Backfill fields on a state object created by an older script version surviving a module reload,
    # so readers can access every field directly. A cached model carries its own placement, so read it
    # back off the encoder rather than leaving None behind, which would compare unequal to every
    # resolved placement and drop a warm cache the requested placement actually matches.
    if not hasattr(state, "loaded_text_encoder_device") or not hasattr(state, "loaded_text_encoder_parked"):
        # Full precision placement requires matching storage dtypes.
        # NF4 placement preserves packed weights and changes arithmetic dtype.
        encoder_device, encoder_parked = _read_cached_text_encoder_placement(state)
        state.loaded_text_encoder_device = encoder_device
        state.loaded_text_encoder_parked = encoder_parked
    if not hasattr(state, "loaded_text_encoder_precision"):
        # Weight precision the encoder was loaded at, tracked because the quantizer fixes it at load
        # and no code path requantizes a resident model.
        state.loaded_text_encoder_precision = _read_cached_text_encoder_precision(state)
    return state


def _read_cached_text_encoder_precision(state):
    """Recover the weight precision of an already cached encoder. Returns None without a cached
    model, which forces a reload.
    """
    if getattr(state, "model_cache", None) is None:
        return None
    return _detect_text_encoder_precision(getattr(state.model_cache[0], "text_encoder", None))


def _detect_text_encoder_precision(text_encoder):
    """Weight precision of a loaded encoder, read by looking for the quantized layer type the NF4
    quantizer swaps in. Returns None when the encoder's module tree is not reachable.
    """
    if text_encoder is None:
        return None
    # Unwrap the parking proxy, then the LLM2VecEncoder, to reach the LLM2Vec nn.Module.
    inner = getattr(text_encoder, "_inner", text_encoder)
    llm2vec_module = getattr(inner, "model", None)
    if llm2vec_module is None or not hasattr(llm2vec_module, "modules"):
        return None

    for module in llm2vec_module.modules():
        # Name comparison keeps this working across an importlib.reload of either module.
        if type(module).__name__ == "NF4Linear":
            return TEXT_ENCODER_PRECISION_NF4
    return TEXT_ENCODER_PRECISION_FULL


def _text_encoder_storage_dtype(text_encoder):
    """Dtype name of the encoder's floating point weights, e.g. 'bfloat16', or None when unreadable.
    Distinguishes the fp32 and bf16 full precision variants, which produce different embeddings.
    """
    if text_encoder is None:
        return None
    inner = getattr(text_encoder, "_inner", text_encoder)
    llm2vec_module = getattr(inner, "model", None)
    if llm2vec_module is None or not hasattr(llm2vec_module, "parameters"):
        return None
    for parameter in llm2vec_module.parameters():
        if parameter.is_floating_point():
            return str(parameter.dtype).replace("torch.", "")
    return None


def _read_cached_text_encoder_placement(state):
    """Recover (device, parked) for an already cached model whose state predates those fields.
    Returns (None, None) without a cached model.
    """
    if getattr(state, "model_cache", None) is None:
        return None, None

    text_encoder = getattr(state.model_cache[0], "text_encoder", None)
    if not _is_cached_text_encoder(text_encoder):
        # No proxy means the encoder sits inside the model, on the model's own device.
        return state.loaded_device, False

    # The proxy's compute device is where it encodes: the model's device for a parked encoder that
    # ferries across per encode, or "cpu" for one that encodes in place and never parks.
    compute_device = getattr(text_encoder, "_compute_device", None)
    if compute_device == "cpu":
        return "cpu", False
    return compute_device, getattr(text_encoder, "_park_on_cpu", True)


_CUDA_COMPATIBILITY_ERRORS = (
    "no kernel image is available for execution on the device",
    "invalid device function",
    "cublas_status_arch_mismatch",
    "cublas_status_not_supported",
    "operation not supported",
    "current cuda device does not support bfloat16",
    "cuda driver version is insufficient for cuda runtime version",
    "the nvidia driver on your system is too old",
)


def _is_cuda_usable(probe_if_needed=True, device_index=None):
    """Cache an execution probe per GPU because CUDA detection does not
    verify kernel support.
    """
    state = _get_persistent_state()
    if not hasattr(state, "cuda_compatibility"):
        state.cuda_compatibility = {}
    if not probe_if_needed and not state.cuda_compatibility:
        return False
    if not torch.cuda.is_available():
        return False
    cache_key = (None, torch.__version__, torch.version.cuda)
    if cache_key in state.cuda_compatibility:
        return state.cuda_compatibility[cache_key]

    device_name = "unknown GPU"
    capability_text = "unknown"
    try:
        if device_index is None:
            if not hasattr(state, "cuda_device_index"):
                state.cuda_device_index = torch.cuda.current_device()
            device_index = state.cuda_device_index
        if not 0 <= device_index < torch.cuda.device_count():
            print(f"Warning: [Kimodo] CUDA device {device_index} is unavailable; "
                  "requests targeting this GPU will fall back to CPU.", flush=True)
            return False
        cache_key = (device_index, torch.__version__, torch.version.cuda)
        if cache_key in state.cuda_compatibility:
            return state.cuda_compatibility[cache_key]
        if not probe_if_needed:
            return False
        device_name = torch.cuda.get_device_name(device_index)
        capability = torch.cuda.get_device_capability(device_index)
        capability_text = f"{capability[0]}.{capability[1]}"
        probe = torch.ones(
            (2, 2), device=f"cuda:{device_index}", dtype=torch.bfloat16,
        )
        probe = (probe + 1.0) @ probe
        torch.cuda.synchronize(device_index)
    except RuntimeError as error:
        error_text = str(error).lower()
        if not any(message in error_text
                   for message in _CUDA_COMPATIBILITY_ERRORS):
            raise
        state.cuda_compatibility[cache_key] = False
        reason = " ".join(str(error).split())
        print(
            f"Warning: [Kimodo] CUDA execution check failed for {device_name} "
            f"(compute capability {capability_text}, PyTorch {torch.__version__}, "
            f"CUDA {torch.version.cuda}): {reason}. Requests targeting this GPU "
            "will fall back to CPU. CPU execution will be slower. "
            "Use an engine PyTorch/CUDA build and GPU driver that support this GPU.",
            flush=True,
        )
        return False
    state.cuda_compatibility[cache_key] = True
    return True


def _get_device():
    """Resolve the device to load and run Kimodo on, honoring KIMODO_DEVICE from settings.
    'cpu' forces CPU, otherwise CUDA must pass an execution check before use.
    """
    requested = os.environ.get("KIMODO_DEVICE", "").lower()
    if requested == "cpu":
        return "cpu"
    if _is_cuda_usable():
        return f"cuda:{_get_persistent_state().cuda_device_index}"
    if requested == "cuda" and not torch.cuda.is_available():
        print("Warning: [Kimodo] CUDA was requested but is unavailable. "
              "Kimodo is falling back to CPU.", flush=True)
    return "cpu"


def _get_text_encoder_device(target_device):
    """Device the text encoder loads and encodes on. KIMODO_TEXT_ENCODER_DEVICE='cpu' pins it to
    the CPU while diffusion stays on target_device; anything else follows target_device.
    """
    if os.environ.get("KIMODO_TEXT_ENCODER_DEVICE", "").lower() == "cpu":
        return "cpu"
    return target_device


_BYTE_UNITS = ("b", "ib", "byte", "bytes")
_TEXT_ENCODER_DESC_HINTS = ("llama", "llm2vec", "adapter", "loading weights")
_AGGREGATOR_DESC_HINTS = ("fetching", "loading weights", "downloading")
# Heuristic threshold: any byte-unit bar over this size belongs to the text encoder phase
# (Llama-3 shards are ~4.7 GB each, the Kimodo checkpoint is much smaller).
_TEXT_ENCODER_BYTE_THRESHOLD = 500 * 1024 * 1024

# Cache folder names for the three text encoder repos. We poll their blob sizes directly for
# download visibility even when HF's tqdm bars are silent.
_TEXT_ENCODER_REPOS = (
    "models--meta-llama--Meta-Llama-3-8B-Instruct",
    "models--McGill-NLP--LLM2Vec-Meta-Llama-3-8B-Instruct-mntp",
    "models--McGill-NLP--LLM2Vec-Meta-Llama-3-8B-Instruct-mntp-supervised",
)
# Approximate total download for a cold first run, used to map downloaded bytes to an overall
# percentage and shown as the "of ~X" total. Kept slightly above the real size so the displayed
# downloaded bytes (capped at this value) never read over the listed total.
_TEXT_ENCODER_EXPECTED_BYTES = int(17.5 * 1024 ** 3)

# Approximate size of the pinned Kimodo checkpoint repo, for the cached-or-not size heuristic.
_KIMODO_CHECKPOINT_EXPECTED_BYTES = int(1.1 * 1024 ** 3)

# Worker status codes reported through status_code_cb while a model load, download, or device move
# runs. The code set is Kimodo-private: must match EKimodoWorkerStatus in KimodoPoseGeneratorEditor.cpp.
KIMODO_STATUS_NONE = 0
KIMODO_STATUS_LOADING_MODEL = 1
KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS = 2
KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER = 3
KIMODO_STATUS_LOADING_TEXT_ENCODER = 4
KIMODO_STATUS_UPLOADING_TO_GPU = 5
# The steps that follow the text encoder's weight load. Each one runs for tens of seconds with no
# tqdm bar of its own, so the load thread names them explicitly through _LoadState.set_phase.
KIMODO_STATUS_APPLYING_TEXT_ENCODER_ADAPTER = 6
KIMODO_STATUS_UPLOADING_TEXT_ENCODER = 7
KIMODO_STATUS_PREPARING_MODEL = 8
KIMODO_STATUS_WARMING_UP_TEXT_ENCODER = 9
KIMODO_STATUS_SAVING_TEXT_ENCODER_CACHE = 10
KIMODO_STATUS_QUANTIZING_TEXT_ENCODER = 11
KIMODO_STATUS_ENCODING_TEXT = 12

# Log-only wording for each status code. The editor has its own localized text for the same codes.
_STATUS_CODE_LABELS = {
    KIMODO_STATUS_NONE: "Load finished",
    KIMODO_STATUS_LOADING_MODEL: "Loading the Kimodo model",
    KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS: "Downloading the Kimodo checkpoint",
    KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER: "Downloading the Llama 3 text encoder weights",
    KIMODO_STATUS_LOADING_TEXT_ENCODER: "Loading the Llama 3 text encoder weights",
    KIMODO_STATUS_UPLOADING_TO_GPU: "Uploading the model to the GPU",
    KIMODO_STATUS_APPLYING_TEXT_ENCODER_ADAPTER: "Applying the LLM2Vec adapter",
    KIMODO_STATUS_QUANTIZING_TEXT_ENCODER: "Quantizing the Llama 3 text encoder to NF4",
    KIMODO_STATUS_UPLOADING_TEXT_ENCODER: "Uploading the text encoder to the GPU",
    KIMODO_STATUS_PREPARING_MODEL: "Preparing the Kimodo model",
    KIMODO_STATUS_WARMING_UP_TEXT_ENCODER: "Warming up the text encoder",
    KIMODO_STATUS_SAVING_TEXT_ENCODER_CACHE: "Saving the quantized text encoder to disk",
    KIMODO_STATUS_ENCODING_TEXT: "Encoding the animation description",
}


def _status_code_label(status_code):
    """Log wording for a status code, for tracing how long each worker phase took."""
    return _STATUS_CODE_LABELS.get(status_code, f"Worker phase {status_code}")


class _LoadState:
    """Thread-safe state shared between the load_model worker and the main-thread UI loop.
    The worker writes via the patched tqdm callback; the main thread reads via snapshot().
    """

    def __init__(self):
        self._lock = threading.Lock()
        self._bars = {}  # id(tqdm) -> (desc, n, total, unit)
        self._text_encoder_active = False
        # Worker override naming the step that is running, for the stretches (adapter merge, device
        # transfer, CUDA warm-up) that no tqdm bar covers and where the status would otherwise sit
        # unchanged for a minute. Code 0 hands the status back to the evidence-driven tracker.
        self._phase_code = 0
        self._detail = ""
        # Real completed fraction of the current phase for the stretches without a tqdm bar, fed
        # by the phase itself: quantized layers, weight shards written, or weight shards read.
        self._phase_fraction = None
        self.model = None
        self.error = None

    def set_phase(self, code, detail=""):
        with self._lock:
            if int(code) != self._phase_code or (detail or "") != self._detail:
                self._phase_fraction = None
            self._phase_code = int(code)
            self._detail = detail or ""

    def get_phase(self):
        with self._lock:
            return self._phase_code, self._detail

    def set_phase_fraction(self, fraction):
        with self._lock:
            self._phase_fraction = max(0.0, min(1.0, float(fraction)))

    def get_phase_progress(self):
        """Real completed fraction of the current phase, or None when it reported none."""
        with self._lock:
            return self._phase_fraction

    def update_bar(self, bar_id, desc, n, total, unit):
        with self._lock:
            self._bars[bar_id] = (desc or "", n or 0, total or 0, unit or "")
            if not self._text_encoder_active and _looks_like_text_encoder_bar(desc, total, unit):
                self._text_encoder_active = True

    def remove_bar(self, bar_id):
        with self._lock:
            self._bars.pop(bar_id, None)

    def signal_text_encoder(self):
        with self._lock:
            self._text_encoder_active = True

    def snapshot(self):
        with self._lock:
            return self._text_encoder_active, list(self._bars.values())


def _looks_like_text_encoder_bar(desc, total, unit):
    """Heuristic: does this tqdm bar belong to the text encoder phase
    (Llama-3 base + LLM2Vec adapters)?
    """
    desc_lower = (desc or "").lower()
    if any(hint in desc_lower for hint in _TEXT_ENCODER_DESC_HINTS):
        return True
    if (unit or "").lower() in _BYTE_UNITS and total and total > _TEXT_ENCODER_BYTE_THRESHOLD:
        return True
    return False


def _hf_cache_dir():
    """Return the HuggingFace Hub cache directory, or None when unavailable."""
    try:
        from huggingface_hub.constants import HF_HUB_CACHE
        return HF_HUB_CACHE
    except (ImportError, AttributeError):
        fallback = os.path.join(os.path.expanduser("~"), ".cache", "huggingface", "hub")
        return fallback if os.path.isdir(fallback) else None


def _cache_repo_bytes(repo_dir_names):
    """Sum the byte size of every regular file across the given HF cache repo dirs, including
    in-progress `*.incomplete` blobs. The whole repo tree is walked, not just blobs/: without
    symlink support huggingface_hub MOVES each finished blob into snapshots/, so a blobs-only sum
    drops by whole shards mid-download. Symlinks are skipped to avoid double counting the symlink
    layout, where snapshot entries just point back at the blobs.
    """
    cache_dir = _hf_cache_dir()
    if not cache_dir:
        return 0
    total = 0
    for repo_dir_name in repo_dir_names:
        repo_dir = os.path.join(cache_dir, repo_dir_name)
        if not os.path.isdir(repo_dir):
            continue
        for walk_dir, _subdirs, filenames in os.walk(repo_dir):
            for filename in filenames:
                file_path = os.path.join(walk_dir, filename)
                try:
                    if not os.path.islink(file_path):
                        total += os.path.getsize(file_path)
                except OSError:
                    pass
    return total


def _text_encoder_cache_bytes():
    """Total cached bytes of the three text encoder repos, the growth signal for download progress."""
    return _cache_repo_bytes(_TEXT_ENCODER_REPOS)


def _format_bytes(num_bytes):
    """Format a byte count as a short human-readable string (KB / MB / GB)."""
    if not num_bytes or num_bytes <= 0:
        return "0 B"
    for size, label in ((1024.0 ** 4, "TB"), (1024.0 ** 3, "GB"),
                        (1024.0 ** 2, "MB"), (1024.0, "KB")):
        if num_bytes >= size:
            return f"{num_bytes / size:.1f} {label}"
    return f"{int(num_bytes)} B"


def _pick_primary_bar(bars):
    """Choose the most informative active bar to drive the dialog. Prefers bars with a known total,
    then aggregator bars (e.g. "Fetching N files"), else the bar with the largest total.
    """
    if not bars:
        return None
    with_total = [bar for bar in bars if bar[2] > 0]
    if not with_total:
        return bars[0]
    for bar in with_total:
        desc_lower = (bar[0] or "").lower()
        if any(hint in desc_lower for hint in _AGGREGATOR_DESC_HINTS):
            return bar
    return max(with_total, key=lambda bar: bar[2])


def _format_bar_label(phase_label, bar):
    """Build a sub-status line from the primary bar."""
    if bar is None:
        return phase_label
    desc, n, total, unit = bar
    unit_lower = (unit or "").lower()
    if unit_lower in _BYTE_UNITS:
        if total > 0:
            return f"{phase_label}: {desc} ({_format_bytes(n)} / {_format_bytes(total)})"
        return f"{phase_label}: {desc} ({_format_bytes(n)})"
    if total > 0:
        return f"{phase_label}: {desc} ({n} / {total})"
    return f"{phase_label}: {desc}" if desc else phase_label


def _bar_progress_pct(bar):
    """Return progress 0..100 from a bar, or None if no total is known."""
    if bar is None:
        return None
    _, n, total, _ = bar
    if total > 0:
        return max(0.0, min(100.0, 100.0 * n / total))
    return None


# Bar descriptions that mean a download is running even when the bar counts files, not bytes.
_DOWNLOAD_DESC_HINTS = ("download", "fetch")


def _bar_is_download(bar):
    """True when the bar tracks a download: it counts bytes, or its description says so
    (e.g. HF's "Fetching 4 files" aggregator, which counts files).
    """
    if bar is None:
        return False
    if (bar[3] or "").lower() in _BYTE_UNITS:
        return True
    desc_lower = (bar[0] or "").lower()
    return any(hint in desc_lower for hint in _DOWNLOAD_DESC_HINTS)


def _bar_detail(bar):
    """Short progress detail from a bar for the editor status line, e.g. "1.2 GB / 4.7 GB".
    Count-based bars show a percentage: the weight loader counts tensors (290 for the pinned
    Llama-3), and a bare "137 of 290" reads as arbitrary while tensors differ wildly in size.
    """
    if bar is None:
        return ""
    _, n, total, unit = bar
    if (unit or "").lower() in _BYTE_UNITS:
        if total > 0:
            return f"{_format_bytes(n)} / {_format_bytes(total)}"
        return _format_bytes(n)
    if total > 0:
        return f"{100.0 * n / total:.0f}%"
    return ""


def _capture_tqdm_progress(state):
    """Patch tqdm.std.tqdm.display to publish per-bar state into the _LoadState, returning a context
    manager (or None if tqdm is unavailable). Patching the method covers subclasses via inheritance.
    """
    try:
        import tqdm.std
    except ImportError:
        return None

    def _hooked_display(self, msg=None, pos=None):
        n = getattr(self, "n", 0) or 0
        total = getattr(self, "total", 0) or 0
        state.update_bar(id(self), getattr(self, "desc", None),
            n, total, getattr(self, "unit", ""))
        # Drop completed bars so they stop dominating the primary-bar pick.
        if total > 0 and n >= total:
            state.remove_bar(id(self))

    @contextlib.contextmanager
    def _patch():
        original_display = tqdm.std.tqdm.display
        tqdm.std.tqdm.display = _hooked_display
        try:
            yield
        finally:
            tqdm.std.tqdm.display = original_display

    return _patch()


class _GenerationCancelled(Exception):
    """Raised inside the diffusion loop when the editor requested cancellation."""


class _WorkerDiffusionProgress:
    """tqdm-shaped callable reporting denoising progress through progress_cb and polling cancel_cb.
    Kimodo's `_generate` calls it once per segment with `range(num_denoising_steps)`.
    """

    def __init__(self, total_segments, steps_per_segment, progress_cb, cancel_cb,
                 status_code_cb=None):
        self._total_steps = max(1, total_segments) * max(1, steps_per_segment)
        self._progress_cb = progress_cb
        self._cancel_cb = cancel_cb
        self._completed_steps = 0
        self._started = False
        self._status_code_cb = status_code_cb

    def __call__(self, iterable, *_args, **_kwargs):
        if not self._started:
            self._started = True
            if self._status_code_cb is not None:
                self._status_code_cb(KIMODO_STATUS_NONE)
            if self._progress_cb is not None:
                self._progress_cb(0.0)
        # The list materialization is cheap (a range of denoising steps) and
        # keeps the generator below simple.
        return self._iterate(list(iterable))

    def _iterate(self, items):
        for item in items:
            if self._cancel_cb is not None and self._cancel_cb():
                raise _GenerationCancelled()
            yield item
            self._completed_steps += 1
            if self._progress_cb is not None:
                self._progress_cb(min(1.0, self._completed_steps / self._total_steps))


# Overall-progress mapping. The Kimodo checkpoint gets a small slice. The text encoder owns the
# bulk, split into a download sub-range (HF cache polling) and a load sub-range (drift / tqdm).
_KIMODO_PHASE_END_PCT = 15.0
_TEXT_ENCODER_DOWNLOAD_END_PCT = 80.0
_TEXT_ENCODER_LOAD_END_PCT = 99.0

# Bands for the phases that follow the text encoder's weight read, chained in the order they run
# (weights, quantize, save, upload, prepare, warm up) so the bar never has to move backwards.
_TEXT_ENCODER_WEIGHTS_END_PCT = 60.0
_TEXT_ENCODER_QUANTIZE_END_PCT = 72.0
_TEXT_ENCODER_SAVE_END_PCT = 82.0
_TEXT_ENCODER_UPLOAD_END_PCT = 90.0
_TEXT_ENCODER_PREPARE_END_PCT = 95.0

@contextlib.contextmanager
def _reported_phase(load_state, status_code, detail=""):
    """Name the load step running inside the block for the editor status, then restore whatever was
    reported before. Restoring covers the nested case: a parked encoder is ferried to the GPU from
    inside the warm-up, so the transfer phase runs within the warm-up phase.
    """
    if load_state is None:
        yield
        return
    previous_code, previous_detail = load_state.get_phase()
    load_state.set_phase(status_code, detail)
    try:
        yield
    finally:
        load_state.set_phase(previous_code, previous_detail)


def _install_text_encoding_progress_hooks(text_encoder, num_encoding_calls, progress_cb,
                                          cancel_cb=None):
    """Report completed decoder layers across text encoder calls. Returns the hook handles,
    or an empty list when the encoder does not expose decoder layers.
    """
    if progress_cb is None:
        return []
    # Unwrap the caching proxy and the LLM2VecEncoder down to the LLM2Vec nn.Module.
    encoder = getattr(text_encoder, "_inner", text_encoder)
    transformer = getattr(encoder, "model", None)
    if not isinstance(transformer, torch.nn.Module):
        return []
    decoder_layers = [module for _, module in transformer.named_modules()
                      if type(module).__name__.endswith("DecoderLayer")]
    num_layers = len(decoder_layers)
    if num_layers == 0:
        return []
    total_layer_calls = max(1, num_encoding_calls) * num_layers
    completed_layer_calls = 0
    hook_handles = []

    def _report_layer_encoded(_hook_module, _hook_args, _hook_output):
        nonlocal completed_layer_calls
        if cancel_cb is not None and cancel_cb():
            raise _GenerationCancelled()
        completed_layer_calls += 1
        progress_cb(min(1.0, completed_layer_calls / total_layer_calls))

    try:
        for decoder_layer in decoder_layers:
            hook_handles.append(decoder_layer.register_forward_hook(_report_layer_encoded))
    except Exception:
        for hook_handle in hook_handles:
            hook_handle.remove()
        return []
    return hook_handles


def _warm_up_text_encoder(model, load_state=None):
    """Run a dummy encode pass to bake in CUDA kernel JIT inside the visible load progress.
    Without it the first real model() call compiles kernels for 30+ seconds in silence.
    """
    try:
        text_encoder = getattr(model, "text_encoder", None)
        if text_encoder is not None:
            with _reported_phase(load_state, KIMODO_STATUS_WARMING_UP_TEXT_ENCODER,
                                 "compiling kernels"):
                progress_cb = (load_state.set_phase_fraction
                               if load_state is not None else None)
                hook_handles = _install_text_encoding_progress_hooks(
                    text_encoder, 1, progress_cb)
                try:
                    # The proxy's warm_up bypasses the prompt cache, so an encoding preloaded from
                    # disk cannot turn the warm-up into a hit that skips the kernel bake.
                    warm_up = getattr(text_encoder, "warm_up", None)
                    if warm_up is not None:
                        warm_up()
                    else:
                        text_encoder(["warmup"])
                finally:
                    for hook_handle in hook_handles:
                        hook_handle.remove()
    except Exception as warmup_error:
        print(f"[Kimodo] Text encoder warm-up skipped: {warmup_error}")


def _finish_model_load(model, device, park_text_encoder, text_encoder_device=None,
                       load_state=None):
    """Cache prompt encodings on every placement and optionally park GPU weights on the CPU.
    GPU encoding warms CUDA kernels during loading, while CPU encoding skips warm-up.
    """
    encoder_on_cpu = text_encoder_device == "cpu" and device != "cpu"
    _install_cached_text_encoder(
        model, "cpu" if encoder_on_cpu else device, park_on_cpu=park_text_encoder,
    )
    if encoder_on_cpu or device == "cpu":
        # The warm-up exists to bake CUDA kernel JIT. Any CPU encode, split or full CPU pipeline,
        # would just burn a full multi-second fp32 forward on a throwaway prompt.
        return
    _warm_up_text_encoder(model, load_state)


def _load_phase_band(phase_code, weights_end_pct):
    """Band of the overall progress a named load phase owns, or None for phases that share the
    generic weight load band with the tqdm and downloaded byte signals.
    """
    if phase_code == KIMODO_STATUS_QUANTIZING_TEXT_ENCODER:
        return weights_end_pct, _TEXT_ENCODER_QUANTIZE_END_PCT
    if phase_code == KIMODO_STATUS_SAVING_TEXT_ENCODER_CACHE:
        return _TEXT_ENCODER_QUANTIZE_END_PCT, _TEXT_ENCODER_SAVE_END_PCT
    if phase_code == KIMODO_STATUS_UPLOADING_TEXT_ENCODER:
        return _TEXT_ENCODER_SAVE_END_PCT, _TEXT_ENCODER_UPLOAD_END_PCT
    if phase_code == KIMODO_STATUS_PREPARING_MODEL:
        return _TEXT_ENCODER_UPLOAD_END_PCT, _TEXT_ENCODER_PREPARE_END_PCT
    if phase_code == KIMODO_STATUS_WARMING_UP_TEXT_ENCODER:
        return _TEXT_ENCODER_PREPARE_END_PCT, _TEXT_ENCODER_LOAD_END_PCT
    return None


def _compute_load_progress(state, kimodo_cache_baseline, text_encoder_bytes,
                           weights_end_pct=_TEXT_ENCODER_LOAD_END_PCT):
    """Pick the most reliable progress source and return (target_pct, label, phase_end_pct) for the
    next tick. Prefers the reporting phase's own fraction, then a tqdm bar with a total, then HF
    cache byte growth, then slow drift (pct None). The caller passes the measured cache bytes so the
    blob scan can run less often than the tick, and weights_end_pct caps the weight load band so a
    fresh quantize or a cached read keeps room for the phases that follow it. phase_end_pct is the
    ceiling the caller's drift fallback may creep toward, so silence never outruns the current
    phase. The editor status code is decided separately by _StatusTracker, not here.
    """
    text_encoder_active, bars = state.snapshot()
    downloaded_bytes = max(0, text_encoder_bytes - kimodo_cache_baseline)

    # HF cache growth of any size in the text encoder repos is a definitive
    # "we are in the text encoder phase" signal, even if no tqdm bar fired.
    if not text_encoder_active and downloaded_bytes > 1024 * 1024:
        state.signal_text_encoder()
        text_encoder_active = True

    if text_encoder_active:
        phase_label = "Loading text encoder"
        phase_start, phase_end = _KIMODO_PHASE_END_PCT, weights_end_pct
    else:
        phase_label = "Loading Kimodo model"
        phase_start, phase_end = 0.0, _KIMODO_PHASE_END_PCT
    phase_span = phase_end - phase_start

    # A phase with a band of its own pins the bar into that band, ahead of any lingering bar or
    # byte count from an earlier phase. Its reported fraction (quantized layers, cache bytes
    # written, shards read, bytes uploaded, layers warmed) moves the bar through the band, and
    # merely entering the phase already advances the bar to the band start.
    phase_code, _ = state.get_phase()
    phase_fraction = state.get_phase_progress()
    phase_band = _load_phase_band(phase_code, weights_end_pct)
    if phase_band is not None:
        band_start, band_end = phase_band
        band_fraction = phase_fraction if phase_fraction is not None else 0.0
        return band_start + band_fraction * (band_end - band_start), phase_label, band_end

    # The cached shard read reports through the same fraction slot but shares the weight load band.
    if phase_fraction is not None:
        return phase_start + phase_fraction * phase_span, phase_label, phase_end

    primary_bar = _pick_primary_bar(bars)
    real_pct = _bar_progress_pct(primary_bar)
    bar_pct = None
    if real_pct is not None:
        bar_pct = phase_start + (real_pct / 100.0) * phase_span

    # Downloaded-bytes candidate, mapped against the expected total. Aggregator bars like "Fetching 4 files"
    # only advance when a whole multi-gigabyte shard completes, so byte growth is the smoother download signal.
    bytes_pct = None
    bytes_label = None
    if text_encoder_active and downloaded_bytes > 1024 * 1024:
        download_fraction = min(1.0, downloaded_bytes / _TEXT_ENCODER_EXPECTED_BYTES)
        # The download is part of the weight read, so it may never creep past that band's end.
        download_span = min(_TEXT_ENCODER_DOWNLOAD_END_PCT, phase_end) - phase_start
        bytes_pct = phase_start + download_fraction * download_span
        bytes_label = (f"Downloading text encoder: {_format_bytes(downloaded_bytes)}"
                       f" of ~{_format_bytes(_TEXT_ENCODER_EXPECTED_BYTES)}")

    # Report whichever signal is further along. The caller keeps the displayed value monotonic and
    # capped below 100, so briefly returning the smaller candidate is harmless.
    if bar_pct is not None and (bytes_pct is None or bar_pct >= bytes_pct):
        return bar_pct, _format_bar_label(phase_label, primary_bar), phase_end
    if bytes_pct is not None:
        return bytes_pct, bytes_label, phase_end

    # No reliable signal. Return pct None so the caller can apply slow drift.
    return None, phase_label, phase_end


class _StatusTracker:
    """Turns load signals into a stable editor status code plus detail. Evidence-driven and sticky:
    downloading is shown while cache bytes actually grow (with a quiet-window hysteresis so gaps
    between shards do not flicker), loading is shown during the long load stretches in between, and
    the authoritative needs_download check the caller passes in makes the final downloading to
    loading switch one-way. A phase the load thread names explicitly wins over the heuristic, except
    against a live download, whose byte readout the hooks running inside it cannot see.
    """

    # A download counts as running until its cache bytes have been quiet this long. Bytes refresh
    # once a second, so shard gaps and rescans stay well inside the window.
    DOWNLOAD_QUIET_SECONDS = 5.0

    def __init__(self, initial_status_code):
        # Start from the same up-front code _get_model reported (_initial_load_status_code), so the
        # first watcher ticks (before any tqdm bar or cache growth exists) do not contradict it.
        self._status_code = initial_status_code
        self._last_text_encoder_bytes = -1
        self._text_encoder_growth_time = 0.0
        self._text_encoder_growth_seen = False
        self._last_checkpoint_bytes = -1
        self._checkpoint_growth_time = 0.0

    def update(self, state, text_encoder_bytes, checkpoint_bytes, needs_download, now=None):
        if now is None:
            now = time.monotonic()
        text_encoder_active, bars = state.snapshot()
        has_download_bar = any(_bar_is_download(bar) for bar in bars)

        # Track byte growth per download. The first observation is a baseline, not growth, so a
        # partially cached resume does not count as an active download until new bytes arrive.
        if self._last_text_encoder_bytes >= 0 and text_encoder_bytes > self._last_text_encoder_bytes:
            self._text_encoder_growth_time = now
            self._text_encoder_growth_seen = True
        self._last_text_encoder_bytes = text_encoder_bytes
        if self._last_checkpoint_bytes >= 0 and checkpoint_bytes > self._last_checkpoint_bytes:
            self._checkpoint_growth_time = now
        self._last_checkpoint_bytes = checkpoint_bytes

        text_encoder_downloading = (self._text_encoder_growth_seen
            and now - self._text_encoder_growth_time <= self.DOWNLOAD_QUIET_SECONDS)
        checkpoint_downloading = (self._checkpoint_growth_time > 0.0
            and now - self._checkpoint_growth_time <= self.DOWNLOAD_QUIET_SECONDS)

        if text_encoder_active:
            if not needs_download:
                # Authoritative: every weight file is fully on disk, and this can never flip back.
                self._status_code = KIMODO_STATUS_LOADING_TEXT_ENCODER
            elif text_encoder_downloading:
                self._status_code = KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER
            elif self._text_encoder_growth_seen and self._status_code == KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER:
                # A quiet stretch mid-download means an already fetched piece is loading (the base
                # model loads for minutes before the adapters download). Growth flips it back.
                self._status_code = KIMODO_STATUS_LOADING_TEXT_ENCODER
        elif checkpoint_downloading or has_download_bar:
            self._status_code = KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS
        elif bars and self._status_code == KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS:
            # The checkpoint download gave way to plain load bars.
            self._status_code = KIMODO_STATUS_LOADING_MODEL

        # The step the load thread named, once no download is running to describe instead.
        phase_code, phase_detail = state.get_phase()
        status_code = self._status_code
        download_codes = (KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER,
                          KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS)
        phase_reported = bool(phase_code) and status_code not in download_codes
        if phase_reported:
            status_code = phase_code

        # Byte details for downloads, capped at the listed total so they never read over it. Other
        # phases fall back to the most informative tqdm bar, e.g. loaded shard counts.
        detail = ""
        if status_code == KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER:
            if text_encoder_bytes > 1024 * 1024:
                shown_bytes = min(text_encoder_bytes, _TEXT_ENCODER_EXPECTED_BYTES)
                detail = (f"{_format_bytes(shown_bytes)}"
                          f" / ~{_format_bytes(_TEXT_ENCODER_EXPECTED_BYTES)}")
        elif status_code == KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS:
            if checkpoint_bytes > 1024 * 1024:
                shown_bytes = min(checkpoint_bytes, _KIMODO_CHECKPOINT_EXPECTED_BYTES)
                detail = (f"{_format_bytes(shown_bytes)}"
                          f" / ~{_format_bytes(_KIMODO_CHECKPOINT_EXPECTED_BYTES)}")
        else:
            detail = _bar_detail(_pick_primary_bar(bars))
        # A named phase carries its own detail, which beats an empty or stale bar reading.
        if phase_reported and phase_detail:
            detail = phase_detail
        return status_code, detail


def _override_text_encoder_preset(device, text_encoder_fp32, park_on_cpu=False,
                                  text_encoder_device=None, quantized=False):
    """Force the LLM2VecEncoder preset to load on the given device and dtype. TEXT_ENCODER_PRESETS
    hardcodes device='auto', which resolves to cuda regardless, so we patch the dict before
    load_model runs. Both values are set unconditionally because load_model mutates that
    module-level dict in place and never restores it, so a prior CPU load leaves dtype at float32
    and the next GPU load would try to place the 8B text encoder in fp32.

    park_on_cpu loads the encoder straight into system RAM even for a GPU run, so its weights never
    land in VRAM at all. _CachedTextEncoder then brings it over only to encode a new prompt.
    text_encoder_device='cpu' also loads it into system RAM, but the encode then runs there too.

    A quantized load keeps bf16 storage even for a CPU encode: the dtype then only covers
    embed_tokens and the norms, which need a gather rather than bf16 math, and the quantized linears
    carry their own compute dtype. That saves about a gigabyte over fp32 storage.
    """
    try:
        import kimodo.model.load_model  # noqa: F401  ensure the submodule is loaded
    except ImportError:
        return
    kimodo_load_model_module = sys.modules.get("kimodo.model.load_model")
    if kimodo_load_model_module is None:
        return
    presets = getattr(kimodo_load_model_module, "TEXT_ENCODER_PRESETS", None)
    if not presets:
        return
    preset = presets.get("llm2vec")
    if preset is None:
        return
    kwargs = preset.setdefault("kwargs", {})
    kwargs["device"] = "cpu" if park_on_cpu else (text_encoder_device or device)
    # Storage dtype when parked (the encode still runs on the compute device), the compute dtype
    # when the encode itself runs on the CPU, which cannot reliably run bf16 ops.
    kwargs["dtype"] = "float32" if (text_encoder_fp32 and not quantized) else "bfloat16"


class _CachedTextEncoder:
    """Cache pooled prompt embeddings in memory and on disk for the encoder configuration.
    GPU weights can remain resident or park on the CPU between uncached encoding requests.
    """

    # Each entry is a few KB, so this only bounds a very long session and the file on disk.
    _MAX_ENTRIES = 128

    def __init__(self, inner, compute_device, disk_cache_path=None,
                 park_on_cpu=True):
        self._inner = inner
        self._compute_device = compute_device
        self._park_on_cpu = bool(park_on_cpu)
        # Machine local file the encodings persist in, or None to keep the cache in memory only.
        self._disk_cache_path = disk_cache_path
        self._embedding_cache = _load_text_encoding_cache(disk_cache_path)
        self._pending_cache_sentences = {}
        self._recovery_cache_sentences = {}

    def _encode_uncached(self, text):
        """Run the real encode on the compute device, returning (encoded_text, lengths) with the
        tensor moved to the CPU. lengths is a plain int or int list, so it lives on no device.
        """
        # A parked GPU encoder moves across for each uncached batch and returns afterward.
        # Resident encoders run in place on their configured device.
        ferry = self._park_on_cpu and self._compute_device != "cpu"
        if ferry:
            print(f"[Kimodo] Encoding text on {self._compute_device} "
                  f"(prompt not cached); moving the text encoder over and back.", flush=True)
            self._inner.to(self._compute_device)
        elif self._compute_device != "cpu":
            print(f"[Kimodo] Encoding text on {self._compute_device} "
                  "(prompt not cached).", flush=True)
        else:
            print("[Kimodo] Encoding text on the CPU (prompt not cached); "
                  "this can take a while.", flush=True)
        try:
            encoded_text, lengths = self._inner(text)
            return encoded_text.detach().to("cpu"), lengths
        finally:
            if ferry:
                # Park it again even if the encode raised, so a failure cannot strand 16 GB in VRAM.
                self._inner.to("cpu")
                _trim_gpu_allocator(self._compute_device)

    def warm_up(self):
        """Encode a throwaway prompt bypassing the cache both ways, so an encoding preloaded from
        disk cannot skip the CUDA kernel bake and the warm-up never occupies a cache slot.
        """
        self._encode_uncached(["warmup"])

    def __call__(self, text):
        is_string = isinstance(text, str)
        sentences = [text] if is_string else text
        keys = [("list", (sentence,)) for sentence in sentences]
        missing_sentences = list(dict.fromkeys(
            sentence for sentence, key in zip(sentences, keys)
            if key not in self._embedding_cache
        ))
        if missing_sentences:
            encoded_text, lengths = self._encode_uncached(missing_sentences)
            for sentence_index, sentence in enumerate(missing_sentences):
                self._embedding_cache[("list", (sentence,))] = (
                    encoded_text[sentence_index:sentence_index + 1].clone(),
                    [lengths[sentence_index]],
                )
                if self._disk_cache_path:
                    self._pending_cache_sentences[sentence] = None
        else:
            print("[Kimodo] Prompt encoding served from the cache; "
                  "no encode needed.", flush=True)

        cached = [self._embedding_cache[key] for key in keys]
        # Kimodo.sample() zeroes empty prompt rows in place, so assemble a separate output
        # instead of handing out the cached tensors.
        encoded_text = torch.cat([entry[0] for entry in cached], dim=0)
        lengths = [entry[1][0] for entry in cached]
        if missing_sentences:
            while len(self._embedding_cache) > self._MAX_ENTRIES:
                evicted_key = next(iter(self._embedding_cache))
                self._embedding_cache.pop(evicted_key)
                self._pending_cache_sentences.pop(evicted_key[1][0], None)
                self._recovery_cache_sentences.pop(evicted_key[1][0], None)
            # Only a fresh encode changes the cache, so a served hit never rewrites the file.
            if self._disk_cache_path and _save_text_encoding_cache(
                    self._disk_cache_path, self._embedding_cache,
                    self._pending_cache_sentences, self._recovery_cache_sentences):
                self._pending_cache_sentences.clear()
                self._recovery_cache_sentences.clear()
        if is_string:
            return encoded_text[0], lengths[0]
        return encoded_text, lengths

    # Passthroughs so the encoder still looks like an LLM2VecEncoder to the rest of the model.
    def to(self, device):
        self._inner.to(device)
        return self

    def eval(self):
        self._inner.eval()
        return self

    def get_device(self):
        return self._inner.get_device()


def _is_cached_text_encoder(candidate):
    """True when candidate is a _CachedTextEncoder, including one built by a pre-reload copy of this
    module. importlib.reload rebinds the class to a new object, so isinstance misses those proxies and
    would let callers wrap one twice or move the parked encoder onto the GPU for good.
    """
    return type(candidate).__name__ == _CachedTextEncoder.__name__


def _upgrade_text_encoding_cache(cache):
    """Normalize legacy prompt entries to bounded single-sentence batches."""
    embedding_cache = {}
    for cache_key, cache_entry in cache.items():
        key_kind, key_payload = cache_key
        encoded_text, lengths = cache_entry
        if key_kind == "str":
            sentences = [key_payload]
            encoded_text = encoded_text.unsqueeze(0)
            lengths = [lengths]
        else:
            sentences = key_payload
        for sentence_index, sentence in enumerate(sentences):
            embedding_cache[("list", (sentence,))] = (
                encoded_text[sentence_index:sentence_index + 1].clone(),
                [lengths[sentence_index]],
            )
    while len(embedding_cache) > _CachedTextEncoder._MAX_ENTRIES:
        embedding_cache.pop(next(iter(embedding_cache)))
    return embedding_cache


def _refresh_nf4_module():
    global kimodo_nf4
    if not hasattr(kimodo_nf4, "set_nf4_compute_dtype"):
        kimodo_nf4 = importlib.reload(kimodo_nf4)


def _refresh_text_encoding_cache(text_encoder, compute_device):
    previous_device = getattr(text_encoder, "_compute_device", compute_device)
    if not hasattr(text_encoder._inner, "_kimodo_prompt_cache_key"):
        text_encoder._inner._kimodo_prompt_cache_key = None
    cache_path = _resolve_text_encoding_cache_file(
        text_encoder._inner, compute_device)
    if (previous_device == compute_device
            and text_encoder._disk_cache_path == cache_path):
        return
    cache_states = getattr(text_encoder, "_arithmetic_cache_states", {})
    previous_dtype = "float32" if previous_device == "cpu" else "bfloat16"
    requested_dtype = "float32" if compute_device == "cpu" else "bfloat16"
    previous_cache = text_encoder._embedding_cache
    if not isinstance(text_encoder, _CachedTextEncoder):
        previous_cache = _upgrade_text_encoding_cache(previous_cache)
    cache_states[previous_dtype] = (
        text_encoder._disk_cache_path, previous_cache,
        getattr(text_encoder, "_pending_cache_sentences", {}),
        getattr(text_encoder, "_recovery_cache_sentences", {}))
    cache_state = cache_states.pop(requested_dtype, None)
    if (cache_state is not None and cache_path is None
            and text_encoder._inner._kimodo_prompt_cache_key is None):
        cache_state = (None, cache_state[1], {}, {})
    if cache_state is None or cache_state[0] != cache_path:
        cache_state = (cache_path, _load_text_encoding_cache(cache_path), {}, {})
    (text_encoder._disk_cache_path, text_encoder._embedding_cache,
     text_encoder._pending_cache_sentences,
     text_encoder._recovery_cache_sentences) = cache_state
    text_encoder._arithmetic_cache_states = cache_states


def _reuse_nf4_text_encoder(text_encoder, compute_device, park_on_cpu,
                            progress_cb=None, status_code_cb=None,
                            status_detail_cb=None):
    """Move a cached NF4 encoder without changing its packed weights."""
    if (not _is_cached_text_encoder(text_encoder)
            or _detect_text_encoder_precision(text_encoder)
            != TEXT_ENCODER_PRECISION_NF4):
        return False
    _refresh_nf4_module()
    if any(type(module).__name__ == kimodo_nf4.NF4Linear.__name__
           and not isinstance(module, kimodo_nf4.NF4Linear)
           for module in text_encoder._inner.model.modules()):
        return False
    previous_device = text_encoder._compute_device
    storage_device = "cpu" if park_on_cpu else compute_device
    compute_dtype = (torch.float32 if compute_device == "cpu"
                     else torch.bfloat16)
    try:
        if status_code_cb is not None:
            status_code_cb(KIMODO_STATUS_LOADING_TEXT_ENCODER
                           if storage_device == "cpu"
                           else KIMODO_STATUS_UPLOADING_TEXT_ENCODER)
        payload_bytes = _module_payload_bytes(text_encoder._inner.model)
        if status_detail_cb is not None:
            status_detail_cb(_format_bytes(payload_bytes))
        if progress_cb is not None and payload_bytes > 0:
            progress_cb(0.0)

            def move_encoder(module, *args, **kwargs):
                return text_encoder._inner.to(*args, **kwargs)

            _move_module_reporting_progress(
                text_encoder._inner.model, move_encoder, payload_bytes,
                progress_cb, (storage_device,), {})
        else:
            text_encoder._inner.to(storage_device)
        kimodo_nf4.set_nf4_compute_dtype(
            text_encoder._inner.model, compute_dtype)
    except RuntimeError as error:
        print(f"[Kimodo] Could not move the NF4 text encoder ({error}). "
              "Reloading its cached weights.", flush=True)
        return False
    _refresh_text_encoding_cache(text_encoder, compute_device)
    text_encoder._compute_device = compute_device
    text_encoder._park_on_cpu = bool(park_on_cpu)
    if progress_cb is not None:
        progress_cb(1.0)
    if previous_device != "cpu" and storage_device != previous_device:
        _trim_gpu_allocator(previous_device)
    print(f"[Kimodo] Reusing the NF4 text encoder on {compute_device} "
          f"(parked={bool(park_on_cpu)}).", flush=True)
    return True


def _install_cached_text_encoder(model, compute_device, park_on_cpu=True):
    """Swap the model's text encoder for the caching proxy. Safe to call twice."""
    text_encoder = getattr(model, "text_encoder", None)
    if text_encoder is None:
        return
    if _is_cached_text_encoder(text_encoder):
        _refresh_text_encoding_cache(text_encoder, compute_device)
        if isinstance(text_encoder, _CachedTextEncoder):
            return
        upgraded_encoder = _CachedTextEncoder(
            text_encoder._inner, compute_device, text_encoder._disk_cache_path,
            park_on_cpu,
        )
        embedding_cache = _upgrade_text_encoding_cache(text_encoder._embedding_cache)
        spare_capacity = _CachedTextEncoder._MAX_ENTRIES - len(embedding_cache)
        supplemental_cache = {}
        disk_cache = upgraded_encoder._embedding_cache
        for key, entry in disk_cache.items():
            if key in embedding_cache:
                embedding_cache[key] = entry
            elif spare_capacity:
                supplemental_cache[key] = entry
                if len(supplemental_cache) > spare_capacity:
                    supplemental_cache.pop(next(iter(supplemental_cache)))
        supplemental_cache.update(embedding_cache)
        upgraded_encoder._embedding_cache = supplemental_cache
        upgraded_encoder._arithmetic_cache_states = getattr(
            text_encoder, "_arithmetic_cache_states", {})
        if upgraded_encoder._disk_cache_path:
            pending_sentences = getattr(text_encoder, "_pending_cache_sentences", None)
            if pending_sentences is None:
                recovery_sentences = (key[1][0] for key in embedding_cache)
            else:
                for sentence in pending_sentences:
                    key = ("list", (sentence,))
                    if key in embedding_cache and key not in disk_cache:
                        upgraded_encoder._pending_cache_sentences[sentence] = None
                recovery_sentences = getattr(text_encoder, "_recovery_cache_sentences", {})
            for sentence in recovery_sentences:
                key = ("list", (sentence,))
                if key in embedding_cache and key not in disk_cache:
                    upgraded_encoder._recovery_cache_sentences[sentence] = None
        model.text_encoder = upgraded_encoder
        return
    encoding_cache_path = _resolve_text_encoding_cache_file(text_encoder, compute_device)
    model.text_encoder = _CachedTextEncoder(
        text_encoder, compute_device, encoding_cache_path, park_on_cpu,
    )
    if compute_device == "cpu":
        print("[Kimodo] Text encoder resident in system RAM; it encodes uncached prompts "
              "on the CPU and never touches VRAM.", flush=True)
    elif park_on_cpu:
        print(f"[Kimodo] Text encoder parked in system RAM; it visits {compute_device} "
              "only to encode a prompt that is not already cached.", flush=True)
    else:
        print(f"[Kimodo] Text encoder resident on {compute_device}; "
              "prompt encodings are cached.", flush=True)


def _module_payload_bytes(module):
    """Bytes a module's tensors occupy, the size a transfer between devices has to move. Quantized
    weights report their packed size, since that is what the tensors actually hold. Returns 0 rather
    than failing, because this only labels a status line.
    """
    try:
        tensors = list(module.parameters()) + list(module.buffers())
        return sum(tensor.numel() * tensor.element_size() for tensor in tensors)
    except Exception:
        return 0


def _move_module_reporting_progress(module, original_to, payload_bytes, progress_cb,
                                    move_args, move_kwargs):
    """Move the module's leaf submodules one at a time so the device transfer reports real byte
    progress, then run the original to() so tensors owned by container modules still move. Tensors
    shared between leaves count once per owner, so the fraction is clamped rather than exact.
    """
    try:
        moved_bytes = 0
        for leaf_module in module.modules():
            if next(leaf_module.children(), None) is not None:
                continue
            torch.nn.Module.to(leaf_module, *move_args, **move_kwargs)
            moved_bytes += _module_payload_bytes(leaf_module)
            progress_cb(min(1.0, moved_bytes / payload_bytes))
    except Exception as move_error:
        # The plain call below moves whatever the leaf walk left behind, so this only costs progress.
        print(f"[Kimodo] Per module transfer progress unavailable ({move_error}).", flush=True)
    return original_to(module, *move_args, **move_kwargs)


def _install_text_encoder_phase_hooks(load_state):
    """Report the text encoder load steps that carry no progress bar of their own: the LLM2Vec
    adapter load and merge, the transfer onto the compute device, and the tail where the Kimodo
    model itself finishes. Together they run for about a minute on a warm cache, during which the
    editor would otherwise show one unchanged line.

    The hooks are installed once and retargeted at the live load state through the returned holder,
    which the caller clears when the load ends so a later per-encode ferry reports nothing. The
    holder is read back off the installed wrapper rather than kept in a module global, because an
    importlib.reload rebinds module state while leaving the installed wrappers in place.
    """
    try:
        import kimodo.model.llm2vec.llm2vec  # noqa: F401
        import kimodo.model.llm2vec.llm2vec_wrapper  # noqa: F401
    except ImportError:
        return None
    llm2vec_module = sys.modules.get("kimodo.model.llm2vec.llm2vec")
    wrapper_module = sys.modules.get("kimodo.model.llm2vec.llm2vec_wrapper")
    LLM2VecClass = getattr(llm2vec_module, "LLM2Vec", None) if llm2vec_module is not None else None
    EncoderClass = getattr(wrapper_module, "LLM2VecEncoder", None) if wrapper_module is not None else None
    if LLM2VecClass is None or EncoderClass is None:
        return None

    holder = getattr(LLM2VecClass.to, "_kimodo_phase_holder", None)
    if holder is not None:
        holder["state"] = load_state
        return holder
    holder = {"state": load_state}

    original_to = LLM2VecClass.to

    def _phase_reporting_to(self, *move_args, **move_kwargs):
        # Only the trip onto an accelerator is worth naming. Parking back on the CPU is fast, and
        # nn.Module.to also takes a dtype or a tensor, neither of which moves anything.
        target = move_args[0] if move_args else move_kwargs.get("device", None)
        moves_to_accelerator = (isinstance(target, (str, torch.device))
                                and "cpu" not in str(target))
        if not moves_to_accelerator:
            return original_to(self, *move_args, **move_kwargs)
        payload_bytes = _module_payload_bytes(self)
        detail = _format_bytes(payload_bytes) if payload_bytes > 0 else ""
        load_state_now = holder["state"]
        with _reported_phase(load_state_now, KIMODO_STATUS_UPLOADING_TEXT_ENCODER, detail):
            # Only a load reports transfer progress. The per encode ferry of a parked encoder
            # outside a load takes the plain move, since nothing is watching the fraction then.
            if load_state_now is None or payload_bytes <= 0:
                return original_to(self, *move_args, **move_kwargs)
            return _move_module_reporting_progress(self, original_to, payload_bytes,
                load_state_now.set_phase_fraction, move_args, move_kwargs)

    _phase_reporting_to._kimodo_phase_holder = holder
    LLM2VecClass.to = _phase_reporting_to

    original_encoder_init = EncoderClass.__init__

    def _phase_reporting_encoder_init(self, *init_args, **init_kwargs):
        try:
            return original_encoder_init(self, *init_args, **init_kwargs)
        finally:
            # The encoder is up. Everything left belongs to the diffusion model, so name that
            # rather than leaving the text encoder on screen for the rest of the load.
            load_state_now = holder["state"]
            if load_state_now is not None:
                load_state_now.set_phase(KIMODO_STATUS_PREPARING_MODEL)

    EncoderClass.__init__ = _phase_reporting_encoder_init

    # Lazy: peft is an optional install, and the hooks below are skipped entirely when it is absent.
    try:
        from peft import PeftModel
        from peft.tuners.tuners_utils import BaseTuner
    except ImportError:
        return holder

    if not getattr(PeftModel.from_pretrained, "_kimodo_phase_reported", False):
        # Rebind as a classmethod rather than a staticmethod: from_pretrained dispatches on cls to
        # pick the task specific PeftModel subclass.
        original_adapter_load = PeftModel.from_pretrained.__func__

        def _phase_reporting_adapter_load(cls, *load_args, **load_kwargs):
            with _reported_phase(holder["state"], KIMODO_STATUS_APPLYING_TEXT_ENCODER_ADAPTER,
                                 "reading weights"):
                return original_adapter_load(cls, *load_args, **load_kwargs)

        _phase_reporting_adapter_load._kimodo_phase_reported = True
        PeftModel.from_pretrained = classmethod(_phase_reporting_adapter_load)

    if not getattr(BaseTuner.merge_and_unload, "_kimodo_phase_reported", False):
        original_merge = BaseTuner.merge_and_unload

        def _phase_reporting_merge(self, *merge_args, **merge_kwargs):
            merging_load_state = holder["state"]
            # peft's own bar is off by default, and folding an adapter into every layer is among the
            # longest unreported steps of the load. Switching the bar on is the only real progress
            # signal for it, and the tqdm capture consumes it so nothing reaches the console. A
            # caller that already made the choice keeps it.
            if (merging_load_state is not None and not merge_args
                    and "progressbar" not in merge_kwargs):
                merge_kwargs["progressbar"] = True
            with _reported_phase(merging_load_state, KIMODO_STATUS_APPLYING_TEXT_ENCODER_ADAPTER):
                return original_merge(self, *merge_args, **merge_kwargs)

        _phase_reporting_merge._kimodo_phase_reported = True
        BaseTuner.merge_and_unload = _phase_reporting_merge

    return holder


def _patch_llm2vec_encode_default_device():
    """Run LLM2Vec batches on the resolved device without multiprocessing.
    Preserve CPU placement and the shared CUDA target across visible GPUs.
    """
    try:
        import kimodo.model.llm2vec.llm2vec  # noqa: F401
    except ImportError:
        return
    module = sys.modules.get("kimodo.model.llm2vec.llm2vec")
    if module is None:
        return
    LLM2VecClass = getattr(module, "LLM2Vec", None)
    if LLM2VecClass is None:
        return
    if getattr(LLM2VecClass.encode, "_kimodo_single_device_patched", False):
        return

    def _device_aware_encode(
            self, sentences, batch_size=32, show_progress_bar=True,
            convert_to_numpy=False, convert_to_tensor=False, device=None):
        if device is None:
            try:
                device = str(next(self.parameters()).device)
            except StopIteration:
                device = _get_device()
        if isinstance(sentences[0], str) and isinstance(sentences[-1], int):
            sentences = [sentences]
        if isinstance(sentences[0], str):
            sentences = [["", sentence] for sentence in sentences]
        input_texts = []
        for sentence in sentences:
            assert isinstance(sentence[0], str)
            assert isinstance(sentence[1], str)
            input_texts.append(self._convert_to_str(sentence[0], sentence[1]))

        self.eval()
        self.to(device)
        if convert_to_tensor:
            convert_to_numpy = False
        sorted_indices = np.argsort([
            -self._text_length(sentence) for sentence in input_texts
        ])
        sorted_sentences = [
            input_texts[sentence_index] for sentence_index in sorted_indices
        ]
        all_embeddings = []
        for start_index in module.trange(
                0, len(sorted_sentences), batch_size,
                desc="Batches", disable=not show_progress_bar):
            sentences_batch = sorted_sentences[
                start_index:start_index + batch_size
            ]
            all_embeddings.append(self._encode(
                sentences_batch, device=device,
                convert_to_numpy=convert_to_numpy, multiprocessing=False,
            ))
        embeddings = torch.cat(all_embeddings, dim=0)
        embeddings = embeddings[np.argsort(sorted_indices)].to(torch.float32)
        if convert_to_numpy:
            embeddings = np.asarray([
                embedding.numpy() for embedding in embeddings
            ])
        return embeddings

    _device_aware_encode._kimodo_device_default_patched = True
    _device_aware_encode._kimodo_single_device_patched = True
    LLM2VecClass.encode = _device_aware_encode


# Weight precision for the text encoder. nf4 packs the 6.98B decoder linears through the pure
# PyTorch quantizer in kimodo_nf4 and leaves embed_tokens and the norms at the storage dtype,
# measuring about 4.5 GB of live tensors against 14.1 GB for full bf16.
TEXT_ENCODER_PRECISION_FULL = "full"
TEXT_ENCODER_PRECISION_NF4 = "nf4"
_TEXT_ENCODER_PRECISIONS = (TEXT_ENCODER_PRECISION_FULL, TEXT_ENCODER_PRECISION_NF4)
# nf4 costs the driver about 6 GB against 15 GB for full precision, and encodes just as fast, at the
# price of drifting the pooled embedding about 5%.
DEFAULT_TEXT_ENCODER_PRECISION = TEXT_ENCODER_PRECISION_NF4


def _resolve_text_encoder_precision(requested):
    """Normalize a requested precision, falling back to the default rather than failing a
    generation when the value is unknown. The pure PyTorch quantizer runs wherever torch does,
    so no availability probe is needed.
    """
    precision = (requested or "").strip().lower() or DEFAULT_TEXT_ENCODER_PRECISION
    if precision not in _TEXT_ENCODER_PRECISIONS:
        print(f"[Kimodo] Unknown text encoder precision '{requested}'; using "
              f"'{DEFAULT_TEXT_ENCODER_PRECISION}'.", flush=True)
        precision = DEFAULT_TEXT_ENCODER_PRECISION
    return precision


# Disk cache for the merged, quantized text encoder. Reading and quantizing the bf16 shards and
# then merging the LLM2Vec adapter dominate a cold load (measured 53 s and 37 s out of 114 s), and
# both only recompute a deterministic artifact. Saving the merged base once turns the cold load into
# a read of a much smaller file: measured 50 s down to 10 s, for 4.35 GB on disk. The cached tensors
# are exactly what the merge produces, so the embeddings stay bit identical.
#
# The artifact is a derivative of the gated Llama 3 weights. The Meta Llama 3 Community License
# grants local modification, and every notice obligation is conditioned on distribution, so this
# must stay a machine-local cache: never submit it, never ship it. Living under the project's
# Intermediate folder keeps it out of source control by construction.
# Bump when the key or manifest layout changes, so the prune reclaims artifacts older versions wrote.
_TEXT_ENCODER_CACHE_VERSION = 4
_TEXT_ENCODER_CACHE_MANIFEST = "kimodo_cache.json"
# Room for the artifact itself plus the temp copy it is renamed from, and slack so filling the drive
# is never this feature's fault.
_TEXT_ENCODER_CACHE_FREE_MARGIN_BYTES = 20 * 1024 ** 3


def _text_encoder_cache_root():
    """Cache root the editor passes per request, or None when caching is off. Empty means off, which
    is how the editor disables the feature without a settings round trip.
    """
    root = os.environ.get("KIMODO_TEXT_ENCODER_CACHE_DIR", "").strip()
    return root or None


def _repo_snapshot_sha(repo_id, filename):
    """Commit the local cache resolved repo_id to, read out of the snapshots/<sha>/ path segment.
    The text encoder repos are not revision pinned, so this is what keys the cache to its inputs.
    Returns None when the file is not cached, which makes the key incomplete and the cache unusable.
    """
    path = _cached_file_path(repo_id, filename)
    if not path:
        return None
    parts = os.path.normpath(path).split(os.sep)
    if "snapshots" in parts:
        snapshots_index = parts.index("snapshots")
        if snapshots_index + 1 < len(parts):
            return parts[snapshots_index + 1]
    return None


def _text_encoder_cache_key(precision, compute_device):
    """Everything the cached tensors depend on. A mismatch on any of it must miss rather than serve
    weights built from different inputs, so the whole dict is stored in the artifact and compared.
    """
    # Lazy: peft and transformers are heavy optional installs the worker may bootstrap after this
    # module imports, so pulling them in at module scope would cost seconds or fail outright.
    import peft
    import transformers
    return {
        "format_version": _TEXT_ENCODER_CACHE_VERSION,
        "precision": precision,
        # Prompt embeddings depend on arithmetic precision.
        # Packed NF4 weights support either compute dtype.
        "compute_dtype": "float32" if compute_device == "cpu" else "bfloat16",
        # Both adapters shape the cached tensors: the mntp adapter through the loaded architecture
        # and the supervised adapter through the merge, so each repo's resolved snapshot is keyed.
        "base_repo": _TEXT_ENCODER_ADAPTER_REPOS[0],
        "base_sha": _repo_snapshot_sha(_TEXT_ENCODER_ADAPTER_REPOS[0], "adapter_config.json"),
        "supervised_repo": _TEXT_ENCODER_ADAPTER_REPOS[1],
        "supervised_sha": _repo_snapshot_sha(_TEXT_ENCODER_ADAPTER_REPOS[1], "adapter_config.json"),
        "weights_sha": _repo_snapshot_sha(_GATED_TEXT_ENCODER_REPO, "config.json"),
        "torch": torch.__version__,
        "transformers": transformers.__version__,
        "peft": peft.__version__,
        "quantizer": f"kimodo-nf4-v{kimodo_nf4.KIMODO_NF4_FORMAT_VERSION}",
    }


def _build_complete_cache_key(precision, compute_device, cache_name):
    """Cache key for this configuration, or None when it cannot be built or the weights are not in
    the local cache yet. Keying against a revision we cannot read could serve the wrong tensors.
    """
    try:
        key = _text_encoder_cache_key(precision, compute_device)
    except Exception as error:
        print(f"[Kimodo] {cache_name} disabled, cannot build its key ({error}).", flush=True)
        return None
    if not key["base_sha"] or not key["supervised_sha"] or not key["weights_sha"]:
        return None
    return key


def _cache_key_digest(key):
    """Short stable digest of a cache key so different configurations never collide."""
    return hashlib.sha1(json.dumps(key, sort_keys=True).encode("utf-8")).hexdigest()[:16]


def _resolve_text_encoder_cache(precision, compute_device):
    """Return (cache_dir, key) for this configuration, or (None, None) when caching cannot apply.
    Full precision never caches: its artifact would read the same bytes the shards already hold, so
    the only prize is the adapter merge, not worth 15 GB or more on disk. The directory name is a
    digest of the key so different configurations never collide.
    """
    if (precision == TEXT_ENCODER_PRECISION_FULL
            or os.environ.get("TEXT_ENCODERS_DIR")):
        return None, None
    root = _text_encoder_cache_root()
    if root is None:
        return None, None
    key = _build_complete_cache_key(precision, compute_device, "Text encoder cache")
    if key is None:
        return None, None
    cache_dir = os.path.join(root, _cache_key_digest(key))
    _prune_text_encoder_cache_root(root, cache_dir, key)
    if not _text_encoder_cache_is_valid(cache_dir, key):
        alternate_key = dict(key)
        alternate_key["compute_dtype"] = (
            "bfloat16" if key["compute_dtype"] == "float32" else "float32")
        alternate_dir = os.path.join(root, _cache_key_digest(alternate_key))
        if _text_encoder_cache_is_valid(alternate_dir, alternate_key):
            return alternate_dir, alternate_key
    return cache_dir, key


def _text_encoder_cache_is_valid(cache_dir, key):
    """True when cache_dir holds a complete artifact built from exactly this key."""
    if not cache_dir or not os.path.isdir(cache_dir):
        return False
    manifest_path = os.path.join(cache_dir, _TEXT_ENCODER_CACHE_MANIFEST)
    try:
        with open(manifest_path) as manifest_file:
            manifest = json.load(manifest_file)
    except (OSError, ValueError):
        return False
    # A manifest holding valid JSON that is not an object is corruption, and an unusable cache must
    # miss rather than fail the load.
    if not isinstance(manifest, dict) or manifest.get("key") != key:
        return False
    # The manifest is written last, so its presence implies the rest, but a truncated weight file
    # would still be a hard failure much later. Check the byte size of every file it claims.
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        return False
    for filename, expected_size in files.items():
        file_path = os.path.join(cache_dir, filename)
        try:
            if os.path.getsize(file_path) != int(expected_size):
                return False
        except (OSError, TypeError, ValueError):
            return False
    return True


def _discard_text_encoder_cache(cache_dir):
    """Delete a cache directory that failed to load, so the next run rebuilds it."""
    try:
        shutil.rmtree(cache_dir, ignore_errors=True)
        print(f"[Kimodo] Discarded the unusable text encoder cache at {cache_dir}.", flush=True)
    except Exception as error:
        print(f"[Kimodo] Could not discard the text encoder cache ({error}).", flush=True)


# A temp directory this much older than now is an orphan from an interrupted finalize, not a save
# still in flight, so the prune may reclaim its gigabytes.
_TEXT_ENCODER_CACHE_TEMP_MAX_AGE_SECONDS = 24.0 * 60.0 * 60.0


def _prune_text_encoder_cache_root(root, active_cache_dir, active_key):
    """Reclaim sibling cache directories that can never serve a load again: orphaned temp saves,
    artifacts of older format versions, and artifacts whose recorded library versions no longer
    match the running environment. Repo revision mismatches are kept, since a rolled back snapshot
    would validate again. Never raises, because pruning is housekeeping and a generation is not.
    """
    try:
        entry_names = os.listdir(root)
    except OSError:
        return
    active_normalized = os.path.normcase(os.path.normpath(active_cache_dir))
    for entry_name in entry_names:
        entry_path = os.path.join(root, entry_name)
        if (not os.path.isdir(entry_path)
                or os.path.normcase(os.path.normpath(entry_path)) == active_normalized):
            continue
        if ".tmp-" in entry_name:
            # Another live worker may still be writing a young temp directory, so only age qualifies.
            try:
                is_orphaned = (time.time() - os.path.getmtime(entry_path)
                               > _TEXT_ENCODER_CACHE_TEMP_MAX_AGE_SECONDS)
            except OSError:
                continue
            if is_orphaned:
                _discard_text_encoder_cache(entry_path)
            continue
        try:
            with open(os.path.join(entry_path, _TEXT_ENCODER_CACHE_MANIFEST)) as manifest_file:
                manifest = json.load(manifest_file)
        except (OSError, ValueError):
            manifest = None
        manifest_key = manifest.get("key") if isinstance(manifest, dict) else None
        if not isinstance(manifest_key, dict):
            # Keyed directories only ever arrive complete via the atomic rename, so this is debris.
            _discard_text_encoder_cache(entry_path)
            continue
        # Full precision artifacts predate the quantized-only rule and can never be served again.
        if (manifest_key.get("format_version") != _TEXT_ENCODER_CACHE_VERSION
                or manifest_key.get("precision") == TEXT_ENCODER_PRECISION_FULL):
            _discard_text_encoder_cache(entry_path)
            continue
        # A version or format pin recorded at build time can never validate again once the installed
        # environment moves on. Fields only one side carries are skipped rather than judged.
        pinned_fields = ("torch", "transformers", "peft", "quantizer")
        if any(field in manifest_key and field in active_key
               and manifest_key[field] != active_key[field] for field in pinned_fields):
            _discard_text_encoder_cache(entry_path)


def _text_encoder_cache_has_room(root, model):
    """True when the drive holding root can take the artifact plus its temp copy and slack."""
    needed = _module_payload_bytes(model) + _TEXT_ENCODER_CACHE_FREE_MARGIN_BYTES
    try:
        os.makedirs(root, exist_ok=True)
        free = shutil.disk_usage(root).free
    except Exception as error:
        print(f"[Kimodo] Skipping the text encoder cache, cannot measure free space ({error}).",
              flush=True)
        return False
    if free < needed:
        print(f"[Kimodo] Skipping the text encoder cache: {_format_bytes(free)} free where "
              f"{_format_bytes(needed)} is needed.", flush=True)
        return False
    return True


def _finalize_text_encoder_cache(temp_dir, cache_dir, tokenizer, name_or_path, key):
    """Complete a cache directory and move it into place atomically.

    The tokenizer has to live here too: the vendored loader reads it from base_model_name_or_path,
    and the tokenizer files ship in the adapter repo rather than the gated weights repo.

    _name_or_path needs restoring by hand. transformers strips it when saving, and the vendored
    loader rewrites model.config._name_or_path from a local directory's config.json, where an absent
    value would silently stop prepare_for_tokenization wrapping prompts in the Llama 3 chat header
    and shift every embedding. Writing back what the hub path produced keeps the two identical.
    """
    tokenizer.save_pretrained(temp_dir)

    config_path = os.path.join(temp_dir, "config.json")
    with open(config_path) as config_file:
        config_dict = json.load(config_file)
    config_dict["_name_or_path"] = name_or_path
    with open(config_path, "w") as config_file:
        json.dump(config_dict, config_file, indent=2)

    manifest = {
        "key": key,
        # Name to byte size per artifact file, so validation can spot a truncated one.
        "files": {name: os.path.getsize(os.path.join(temp_dir, name))
                  for name in sorted(os.listdir(temp_dir))
                  if name != _TEXT_ENCODER_CACHE_MANIFEST},
        "name_or_path": name_or_path,
    }
    # Written last so a half finished directory can never validate.
    with open(os.path.join(temp_dir, _TEXT_ENCODER_CACHE_MANIFEST), "w") as manifest_file:
        json.dump(manifest, manifest_file, indent=2)

    if os.path.isdir(cache_dir):
        shutil.rmtree(cache_dir, ignore_errors=True)
    os.makedirs(os.path.dirname(cache_dir), exist_ok=True)
    os.replace(temp_dir, cache_dir)


# Disk cache for the pooled prompt encodings the _CachedTextEncoder proxy produces, one file per
# encoder configuration under the same root as the quantized weight cache. Each entry is a 16 KB
# float32 vector per prompt, so a full file stays around 2 MB. It shares the weight cache key, so
# NF4 and full precision encodings never mix and a weight or library bump invalidates the file.
# Bump when the payload layout changes.
_TEXT_ENCODING_CACHE_VERSION = 1


def _resolve_text_encoding_cache_file(text_encoder, compute_device):
    """Path the prompt encodings persist at for this encoder configuration, or None when caching is
    off or the key cannot be built. The precision and storage dtype are read off the loaded encoder
    itself, so a load that silently demoted to full precision keys honestly. Model identity stays
    pinned for the resident encoder while arithmetic precision follows its current placement.
    """
    root = _text_encoder_cache_root()
    storage_dtype = _text_encoder_storage_dtype(text_encoder)
    if not hasattr(text_encoder, "_kimodo_prompt_cache_key"):
        resident_key = getattr(text_encoder.model, "_kimodo_prompt_cache_key", None)
        text_encoder._kimodo_prompt_cache_key = (
            dict(resident_key) if resident_key is not None else None)
    resident_key = text_encoder._kimodo_prompt_cache_key
    if root is None or resident_key is None or storage_dtype is None:
        return None
    key = dict(resident_key)
    key["compute_dtype"] = "float32" if compute_device == "cpu" else "bfloat16"
    key["encodings_version"] = _TEXT_ENCODING_CACHE_VERSION
    key["storage_dtype"] = storage_dtype
    return os.path.join(root, f"kimodo_encodings_{_cache_key_digest(key)}.pt")


def _load_text_encoding_cache(cache_path, *, report_unusable=False):
    """Load usable encodings in insertion order with the oldest entries first.
    Missing or unusable files return None with report_unusable, else {}.
    """
    if not cache_path or not os.path.isfile(cache_path):
        return None if report_unusable else {}
    try:
        payload = torch.load(cache_path, map_location="cpu", weights_only=True)
        if (not isinstance(payload, dict)
                or payload.get("version") != _TEXT_ENCODING_CACHE_VERSION
                or not isinstance(payload.get("entries"), list)):
            raise ValueError("unrecognized layout")
        cache = {}
        embedding_width = None
        for key_kind, key_payload, encoded_text, lengths in \
                payload["entries"][-_CachedTextEncoder._MAX_ENTRIES:]:
            if not isinstance(encoded_text, torch.Tensor):
                raise ValueError("unrecognized entry tensor")
            if key_kind == "str" and isinstance(key_payload, str):
                sentences = [key_payload]
                encoded_text = encoded_text.unsqueeze(0)
                lengths = [lengths]
            elif key_kind == "list" and isinstance(key_payload, (list, tuple)):
                sentences = key_payload
            else:
                raise ValueError("unrecognized entry key")
            # Every prompt holds one pooled vector, and a hit concatenates entries with fresh encodings.
            if (encoded_text.ndim != 3 or encoded_text.shape[0] != len(sentences)
                    or encoded_text.shape[1] != 1
                    or not isinstance(lengths, list)
                    or len(lengths) != len(sentences)):
                raise ValueError("unrecognized entry dimensions")
            if embedding_width is None:
                embedding_width = encoded_text.shape[2]
            elif encoded_text.shape[2] != embedding_width:
                raise ValueError("inconsistent entry dimensions")
            for sentence_index, sentence in enumerate(sentences):
                if not isinstance(sentence, str):
                    raise ValueError("unrecognized prompt")
                cache[("list", (sentence,))] = (
                    encoded_text[sentence_index:sentence_index + 1].clone(),
                    [lengths[sentence_index]],
                )
        while len(cache) > _CachedTextEncoder._MAX_ENTRIES:
            cache.pop(next(iter(cache)))
        if cache:
            print(f"[Kimodo] Loaded {len(cache)} prompt encodings from {cache_path}.", flush=True)
        return cache
    except Exception as error:
        print(f"[Kimodo] Discarding the unusable prompt encoding cache ({error}).", flush=True)
        return None if report_unusable else {}


def _save_text_encoding_cache(cache_path, embedding_cache, new_sentences=None,
                              recovery_sentences=None):
    """Merge and atomically persist encodings under a file lock across worker processes.
    Return whether persistence succeeded without raising on a failed save.
    """
    if not cache_path:
        return False
    temp_path = f"{cache_path}.tmp-{os.getpid()}-{threading.get_ident()}"
    try:
        cache_directory = os.path.realpath(os.path.dirname(cache_path))
        os.makedirs(os.path.dirname(cache_directory), exist_ok=True)
        with FileLock(f"{cache_directory}.lock", timeout=10):
            os.makedirs(cache_directory, exist_ok=True)
            disk_cache = _load_text_encoding_cache(
                cache_path, report_unusable=True)
            if disk_cache is None:
                merged_cache = dict(embedding_cache)
            else:
                merged_cache = {}
                for sentence in recovery_sentences or ():
                    key = ("list", (sentence,))
                    entry = embedding_cache.get(key)
                    if entry is not None and key not in disk_cache:
                        merged_cache[key] = entry
                merged_cache.update(disk_cache)
            if new_sentences is None:
                newest_key = next(reversed(embedding_cache), None)
                if newest_key is not None:
                    merged_cache.pop(newest_key, None)
                    merged_cache[newest_key] = embedding_cache[newest_key]
            else:
                for sentence in new_sentences:
                    key = ("list", (sentence,))
                    entry = embedding_cache.get(key)
                    if entry is not None:
                        merged_cache.pop(key, None)
                        merged_cache[key] = entry
            while len(merged_cache) > _CachedTextEncoder._MAX_ENTRIES:
                merged_cache.pop(next(iter(merged_cache)))
            entries = [
                [key_kind, list(key_payload) if isinstance(key_payload, tuple)
                 else key_payload, encoded_text, lengths]
                for (key_kind, key_payload), (encoded_text, lengths)
                in merged_cache.items()
            ]
            torch.save(
                {"version": _TEXT_ENCODING_CACHE_VERSION, "entries": entries},
                temp_path,
            )
            os.replace(temp_path, cache_path)
    except Exception as error:
        print(f"[Kimodo] Could not save the prompt encoding cache ({error}).", flush=True)
        try:
            os.remove(temp_path)
        except OSError:
            pass
        return False
    return True


def _current_load_state():
    """The _LoadState the phase hooks currently report into, or None outside a load. Read off the
    installed wrapper so it survives an importlib.reload of this module.
    """
    module = sys.modules.get("kimodo.model.llm2vec.llm2vec")
    LLM2VecClass = getattr(module, "LLM2Vec", None) if module is not None else None
    holder = getattr(getattr(LLM2VecClass, "to", None), "_kimodo_phase_holder", None) \
        if LLM2VecClass is not None else None
    return holder["state"] if holder else None


def _quantize_encoder_with_kimodo_nf4(encoder, pending):
    """Merge the supervised adapter into the bf16 weights, quantize every linear with the pure
    PyTorch NF4 quantizer, and stage the packed model for the disk cache. Runs on the CPU resident
    weights, so the full precision tensors never reach VRAM.
    """
    load_state = _current_load_state()
    # Lazy: peft is an optional install, and only a quantized load ever reaches this merge.
    from peft import PeftModel
    if isinstance(encoder.model, PeftModel):
        encoder.model = encoder.model.merge_and_unload()
    compute_dtype = torch.float32 if pending["device"] == "cpu" else torch.bfloat16
    with _reported_phase(load_state, KIMODO_STATUS_QUANTIZING_TEXT_ENCODER):
        quantize_start = time.monotonic()
        quantized_names = kimodo_nf4.quantize_model_nf4(encoder.model, compute_dtype,
            progress_cb=load_state.set_phase_fraction if load_state is not None else None)
    print(f"[Kimodo] Quantized {len(quantized_names)} linear layers to nf4 in "
          f"{time.monotonic() - quantize_start:.1f}s "
          f"({_format_bytes(_module_payload_bytes(encoder.model))} packed).", flush=True)
    _write_kimodo_nf4_cache(encoder, quantized_names, compute_dtype, pending, load_state)
    return encoder


def _write_kimodo_nf4_cache(encoder, quantized_names, compute_dtype, pending, load_state):
    """Stage the packed encoder in a temp directory for _complete_cache_write to finalize. Doing
    nothing is always a valid outcome, and a failure only costs the next load its head start.
    """
    if not pending.get("cache_dir") or pending.get("temp_dir"):
        return
    if _text_encoder_cache_is_valid(pending["cache_dir"], pending["cache_key"]):
        return
    if not _text_encoder_cache_has_room(os.path.dirname(pending["cache_dir"]), encoder.model):
        return
    temp_dir = f"{pending['cache_dir']}.tmp-{os.getpid()}"
    try:
        with _reported_phase(load_state, KIMODO_STATUS_SAVING_TEXT_ENCODER_CACHE,
                             _format_bytes(_module_payload_bytes(encoder.model))):
            save_start = time.monotonic()
            kimodo_nf4.save_nf4_llm2vec(encoder, temp_dir, quantized_names, compute_dtype,
                progress_cb=load_state.set_phase_fraction if load_state is not None else None)
        print(f"[Kimodo] Wrote the quantized text encoder in "
              f"{time.monotonic() - save_start:.1f}s.", flush=True)
        pending["saved_name_or_path"] = str(encoder.model.config._name_or_path)
        pending["temp_dir"] = temp_dir
    except Exception as cache_error:
        print(f"[Kimodo] Could not save the text encoder cache ({cache_error}).", flush=True)
        _discard_text_encoder_cache(temp_dir)
        pending["temp_dir"] = None


def _patch_llm2vec_loading(precision, compute_device, cache_dir=None, cache_key=None,
                           load_from_cache=False):
    """Take over the next text encoder load: quantize its weights with the pure PyTorch NF4
    quantizer, and read from or write to the quantized disk cache.

    The wrapper is installed once per implementation version and reads its settings from a dict, not a
    module global: importlib.reload rebinds module state but retains compatible installed wrappers,
    so a reloaded copy of this script must be able to retarget the existing wrapper. A cached load
    bypasses the vendored loader entirely through kimodo_nf4.load_nf4_llm2vec. Returns nothing: an
    absent vendored loader leaves the load untouched, which every caller treats as normal.
    """
    try:
        import kimodo.model.llm2vec.llm2vec  # noqa: F401
    except ImportError:
        return
    module = sys.modules.get("kimodo.model.llm2vec.llm2vec")
    if module is None:
        return
    LLM2VecClass = getattr(module, "LLM2Vec", None)
    if LLM2VecClass is None:
        return

    def _retarget(target):
        # Retarget the installed wrapper, including back to full precision so it passes through.
        target["precision"] = precision
        target["device"] = compute_device
        target["cache_dir"] = cache_dir
        target["cache_key"] = cache_key
        target["from_cache"] = bool(load_from_cache)
        # Reset the per load save state, so a cache written last time is not finalized again.
        target["temp_dir"] = None
        target["saved_name_or_path"] = None
        return target

    _refresh_nf4_module()
    installed_loader = LLM2VecClass.from_pretrained
    pending = getattr(installed_loader, "_kimodo_quantization", None)
    if (pending is not None
            and getattr(installed_loader, "_kimodo_loading_version", 0) == 3):
        _retarget(pending)
        return

    if pending is not None:
        pending.update(precision=TEXT_ENCODER_PRECISION_FULL, from_cache=False,
                       cache_dir=None, cache_key=None, temp_dir=None,
                       saved_name_or_path=None)

    original_from_pretrained = LLM2VecClass.from_pretrained
    pending = _retarget({})

    def _uses_standard_repositories(args, kwargs):
        base_path = kwargs.get("base_model_name_or_path", args[0] if args else None)
        adapter_path = kwargs.get("peft_model_name_or_path", args[1] if len(args) > 1 else None)
        return (
            not os.environ.get("TEXT_ENCODERS_DIR")
            and base_path == _TEXT_ENCODER_ADAPTER_REPOS[0]
            and adapter_path == _TEXT_ENCODER_ADAPTER_REPOS[1])

    def _load_original_with_identity(*args, **kwargs):
        identity = (_build_complete_cache_key(
            pending["precision"], pending["device"], "Prompt encoding cache")
            if _uses_standard_repositories(args, kwargs) else None)
        encoder = original_from_pretrained(*args, **kwargs)
        if identity is not None and identity != _build_complete_cache_key(
                pending["precision"], pending["device"], "Prompt encoding cache"):
            identity = None
        encoder._kimodo_prompt_cache_key = dict(identity) if identity is not None else None
        if (identity is not None and pending["cache_key"] is not None
                and identity != pending["cache_key"]):
            alternate_identity = dict(pending["cache_key"])
            alternate_identity["compute_dtype"] = identity["compute_dtype"]
            if alternate_identity == identity and pending["cache_dir"]:
                pending["cache_dir"] = os.path.join(
                    os.path.dirname(pending["cache_dir"]), _cache_key_digest(identity))
                pending["cache_key"] = dict(identity)
        if identity is None or identity != pending["cache_key"]:
            pending["cache_dir"] = None
            pending["cache_key"] = None
        return encoder

    def _kimodo_from_pretrained(*args, **kwargs):
        if not _uses_standard_repositories(args, kwargs):
            pending.update(from_cache=False, cache_dir=None, cache_key=None)
        if pending["from_cache"]:
            print(f"[Kimodo] Loading the text encoder from its cache at {pending['cache_dir']}.",
                  flush=True)
            try:
                # Name the phase and report per-tensor read progress, since the cached read has no
                # tqdm bar and grows no file the byte poller watches.
                cached_load_state = _current_load_state()
                if cached_load_state is not None:
                    cached_load_state.signal_text_encoder()
                with _reported_phase(cached_load_state, KIMODO_STATUS_LOADING_TEXT_ENCODER,
                                     "reading cache"):
                    encoder = kimodo_nf4.load_nf4_llm2vec(
                        LLM2VecClass, pending["cache_dir"],
                        progress_cb=(
                            cached_load_state.set_phase_fraction
                            if cached_load_state is not None else None),
                        compute_dtype=(
                            torch.float32 if pending["device"] == "cpu"
                            else torch.bfloat16),
                        storage_dtype=torch.bfloat16)
                    cache_key = pending["cache_key"]
                    encoder._kimodo_prompt_cache_key = (
                        dict(cache_key) if cache_key is not None else None)
                    return encoder
            except Exception as cache_error:
                # A truncated or otherwise unusable artifact must not fail the generation. Drop it
                # and fall through to the full load, which will write a fresh one.
                print(f"[Kimodo] The cached text encoder failed to load ({cache_error}); "
                      "rebuilding it from the original weights.", flush=True)
                _discard_text_encoder_cache(pending["cache_dir"])
                pending["from_cache"] = False

        quantized_load = pending["precision"] != TEXT_ENCODER_PRECISION_FULL
        if quantized_load:
            # Load and quantize in system RAM so the full precision weights never reach VRAM.
            # Without this transformers loads on the accelerator, where the bf16 shards plus the per
            # layer merge peak near 14 GB, well over twice the packed model. LLM2VecEncoder then
            # moves the packed weights over in one transfer, so VRAM peaks at the quantized size
            # and the allocator barely fragments.
            kwargs.setdefault("device_map", {"": "cpu"})
        try:
            encoder = _load_original_with_identity(*args, **kwargs)
            if quantized_load:
                encoder = _quantize_encoder_with_kimodo_nf4(encoder, pending)
            return _complete_cache_write(encoder)
        except Exception as quantization_error:
            # A quantized load can still fail on a broken environment or a mid-quantize error, and
            # failing the whole generation over an optional memory optimization is the wrong trade,
            # so retry exactly once unquantized.
            out_of_memory_error = getattr(torch.cuda, "OutOfMemoryError", None)
            if out_of_memory_error is not None and isinstance(quantization_error, out_of_memory_error):
                # Full precision needs strictly more memory than nf4, so a retry would only fail
                # harder and burn another multi-minute load doing it.
                raise
            if not quantized_load:
                raise
            print(f"[Kimodo] The nf4 text encoder load failed ({quantization_error}); retrying "
                  "once at full precision. The encoder will use more memory than requested.",
                  flush=True)
            # Clear the injected arguments and deselect quantization before the retry so a
            # half-initialized 4-bit state cannot poison the full precision load, and so a later
            # request is not served a quantized wrapper that is known to fail on this host.
            pending["precision"] = TEXT_ENCODER_PRECISION_FULL
            kwargs.pop("device_map", None)
            if pending["device"] == "cpu":
                kwargs["torch_dtype"] = torch.float32
            # Pin the whole worker process to full precision. The failure is a property of this
            # host's backend, so every later nf4 request would fail the same way; _get_model reads
            # this before the cache check, which also stops the drop-and-reload churn a fresh
            # nf4 attempt per request would cost.
            _get_persistent_state().text_encoder_quantization_unavailable = True
            # The failed attempt may have merged and saved its nf4 tensors already, and nothing the
            # retry produces may finalize under the nf4 key: a full precision artifact served to
            # later nf4 requests would silently oversize the encoder and, because a cached load
            # cannot fail like the quantizer did, defeat this pin and reload on every generation.
            # Drop the temp save and detach the cache, since full precision loads never cache.
            if pending.get("temp_dir"):
                _discard_text_encoder_cache(pending["temp_dir"])
                pending["temp_dir"] = None
            pending["saved_name_or_path"] = None
            pending["cache_dir"] = None
            pending["cache_key"] = None
            pending["from_cache"] = False
        encoder = None
        gc.collect()
        return _load_original_with_identity(*args, **kwargs)

    def _complete_cache_write(encoder):
        """Finish a cache the merge hook started, now that the tokenizer exists. A failure here only
        costs the next load its head start, so it is logged rather than raised.
        """
        temp_dir = pending.get("temp_dir")
        if not temp_dir:
            return encoder
        pending["temp_dir"] = None
        try:
            _finalize_text_encoder_cache(temp_dir, pending["cache_dir"], encoder.tokenizer,
                pending["saved_name_or_path"], pending["cache_key"])
            print(f"[Kimodo] Cached the merged text encoder at {pending['cache_dir']}; later loads "
                  "skip the quantization and the adapter merge.", flush=True)
        except Exception as cache_error:
            print(f"[Kimodo] Could not finish the text encoder cache ({cache_error}).", flush=True)
            _discard_text_encoder_cache(temp_dir)
        return encoder

    _kimodo_from_pretrained._kimodo_quantization = pending
    _kimodo_from_pretrained._kimodo_loading_version = 3
    # staticmethod so the already bound original is not handed a second implicit class argument.
    LLM2VecClass.from_pretrained = staticmethod(_kimodo_from_pretrained)


# Pin the exact checkpoint the plugin loads. The registry's versionless alias resolves to the
# newest registered version, so an updated vendored package could silently switch models otherwise.
_PINNED_KIMODO_MODEL = "Kimodo-SOMA-RP-v1.2"
_PINNED_KIMODO_REPO_ID = "nvidia/Kimodo-SOMA-RP-v1.2"

# Commit hash of the approved snapshot of the pinned repo. NVIDIA can push new commits to the same
# repo, and an unpinned snapshot_download follows main. Bump deliberately when a new drop is approved.
_PINNED_KIMODO_REVISION = "0e3c89024d896064b6851201373f7834784a386b"


def _pin_kimodo_checkpoint_revision():
    """Wrap load_model's snapshot_download so the pinned repo always resolves to the pinned commit.
    A fully cached commit hash revision also loads without the per-generation online check.
    """
    try:
        import kimodo.model.load_model  # noqa: F401  ensure the submodule is loaded
    except ImportError:
        return
    kimodo_load_model_module = sys.modules.get("kimodo.model.load_model")
    if kimodo_load_model_module is None:
        return
    original_download = getattr(kimodo_load_model_module, "snapshot_download", None)
    if original_download is None or getattr(original_download, "_kimodo_revision_pinned", False):
        return

    def _pinned_snapshot_download(repo_id=None, *args, revision=None, **kwargs):
        if repo_id == _PINNED_KIMODO_REPO_ID and revision is None:
            revision = _PINNED_KIMODO_REVISION
        return original_download(repo_id, *args, revision=revision, **kwargs)

    _pinned_snapshot_download._kimodo_revision_pinned = True
    kimodo_load_model_module.snapshot_download = _pinned_snapshot_download


def _load_model_with_progress(device, text_encoder_fp32, progress_cb,
                              status_code_cb, status_detail_cb,
                              park_text_encoder, text_encoder_device,
                              text_encoder_precision):
    """Run kimodo.load_model on the pinned checkpoint, reporting 0..1 load progress through
    progress_cb when given (tqdm capture plus HF cache polling on a watcher loop). The watcher also
    reports the current phase through status_code_cb (KIMODO_STATUS values) and status_detail_cb.
    """
    quantized = text_encoder_precision != TEXT_ENCODER_PRECISION_FULL
    # Where the encode itself runs, which sets the dequantization compute dtype. A parked encoder
    # still computes on the GPU it is ferried to.
    text_encoder_compute_device = "cpu" if text_encoder_device == "cpu" else device
    # load_model re-forces float32 onto the preset from under the patch below whenever this is True,
    # so a quantized load must not ask for it. The flag only ever touched the text encoder.
    load_model_fp32 = text_encoder_fp32 and not quantized

    # Make the text encoder follow its own resolved device. The first patch fixes the preset's
    # hardcoded device='auto'. The second fixes LLM2Vec.encode defaulting device to cuda, and it also
    # makes a CPU-resident encoder compute wherever it currently sits. The last takes over the load
    # to quantize and cache the encoder when a quantized precision is requested.
    _override_text_encoder_preset(device, text_encoder_fp32, park_text_encoder,
        text_encoder_device, quantized)
    _patch_llm2vec_encode_default_device()
    _pin_kimodo_checkpoint_revision()

    # Decide the cache here rather than inside the wrapper, so the status tracker and the load agree
    # on whether this is a cached read before the first tick reports anything.
    cache_dir, cache_key = _resolve_text_encoder_cache(text_encoder_precision,
                                                       text_encoder_compute_device)
    load_from_cache = _text_encoder_cache_is_valid(cache_dir, cache_key)
    _patch_llm2vec_loading(text_encoder_precision, text_encoder_compute_device,
        cache_dir, cache_key, load_from_cache)

    if progress_cb is None:
        # No progress to report. Installing with a cleared state keeps the hooks pass-through and
        # retargets any state a previous load left behind.
        _install_text_encoder_phase_hooks(None)
        model = kimodo.load_model(modelname=_PINNED_KIMODO_MODEL, device=device,
            text_encoder_fp32=load_model_fp32)
        _finish_model_load(model, device, park_text_encoder, text_encoder_device)
        return model

    state = _LoadState()
    # Baseline the text encoder cache so we measure new bytes against
    # whatever was on disk before this run.
    cache_baseline = _text_encoder_cache_bytes()
    needs_download = _needs_weight_download()
    status_tracker = _StatusTracker(_initial_load_status_code())
    phase_hooks = _install_text_encoder_phase_hooks(state)
    # Only a fresh quantize has the quantize and save bands ahead of it. A cached read and a full
    # precision load go straight to the upload, so their weight read owns everything below it.
    weights_end_pct = (_TEXT_ENCODER_WEIGHTS_END_PCT if quantized and not load_from_cache
                       else _TEXT_ENCODER_SAVE_END_PCT)

    def _worker():
        try:
            tqdm_capture = _capture_tqdm_progress(state)
            with tqdm_capture if tqdm_capture is not None else contextlib.nullcontext():
                model = kimodo.load_model(modelname=_PINNED_KIMODO_MODEL, device=device,
                    text_encoder_fp32=load_model_fp32)
                # The tensor counter reaching its total is not the end: the proxy install and the
                # CUDA warm-up encode still run, both with no bar of their own. The warm-up names
                # itself; this covers the stretch before it, and the hooks the rest of the load.
                state.set_phase(KIMODO_STATUS_PREPARING_MODEL)
                _finish_model_load(model, device, park_text_encoder, text_encoder_device, state)
                state.model = model
        except BaseException as exception:
            state.error = exception

    thread = threading.Thread(target=_worker, name="KimodoModelLoader",
        daemon=True)
    thread.start()

    return _report_model_load_progress(
        state, thread, phase_hooks, cache_baseline, needs_download, status_tracker,
        weights_end_pct, progress_cb, status_code_cb, status_detail_cb)


def _report_model_load_progress(state, thread, phase_hooks, cache_baseline,
                                needs_download, status_tracker, weights_end_pct,
                                progress_cb, status_code_cb, status_detail_cb,
                                text_encoder_only=False):
    """Publish Kimodo load phases and progress until the loader finishes."""
    checkpoint_repo_dir = "models--" + _PINNED_KIMODO_REPO_ID.replace("/", "--")
    checkpoint_bytes = 0
    DRIFT_PCT_PER_TICK = 0.5
    STALL_CREEP_PCT_PER_TICK = 0.03
    # Drift stays this far under the current phase's band end, so a silent stretch can never look
    # like the phase, or the whole load, finished before a real signal says so.
    DRIFT_MARGIN_PCT = 2.0
    completed = 0.0
    last_label = ""
    last_status_code = None
    last_detail = None
    tick_index = 0
    text_encoder_bytes = cache_baseline
    while thread.is_alive():
        # The blob scans and the cache completeness check stat files on disk, so refresh them once a
        # second instead of every tick. needs_download is checked only while still True: it can only
        # flip to False (download finished), which keeps the status transition one-way.
        if tick_index % 10 == 0:
            text_encoder_bytes = _text_encoder_cache_bytes()
            if not text_encoder_only:
                checkpoint_bytes = _cache_repo_bytes((checkpoint_repo_dir,))
            if needs_download:
                needs_download = _needs_weight_download()
        tick_index += 1
        target_pct, label, phase_end_pct = _compute_load_progress(state, cache_baseline,
                                                                  text_encoder_bytes, weights_end_pct)
        # The drift cap only limits the fallbacks while the shown value sits below it. Once a real
        # signal passed the cap, capping at it again would compute a target below completed and
        # freeze the bar for good.
        drift_ceiling = max(phase_end_pct - DRIFT_MARGIN_PCT, completed)
        if target_pct is None:
            target_pct = min(drift_ceiling, completed + DRIFT_PCT_PER_TICK)
        elif target_pct <= completed:
            # A real signal that is not ahead of the shown value, e.g. a shard-count load bar that
            # maps below the byte-driven download pct. Creep slowly so long loads show visible motion.
            target_pct = min(drift_ceiling, completed + STALL_CREEP_PCT_PER_TICK)
        # Monotonic: never go backwards. Cap below 100 so completion is
        # marked clearly by the final report below.
        target_pct = max(target_pct, completed)
        target_pct = min(target_pct, _TEXT_ENCODER_LOAD_END_PCT)
        if target_pct > completed:
            completed = target_pct
            progress_cb(completed / 100.0)
        status_code, detail = status_tracker.update(
            state, text_encoder_bytes, checkpoint_bytes, needs_download)
        if status_code != last_status_code:
            if status_code_cb is not None:
                status_code_cb(status_code)
            # Log every phase change, so the time each step of a load costs is readable afterwards.
            print(f"[Kimodo] {_status_code_label(status_code)}", flush=True)
            last_status_code = status_code
        if status_detail_cb is not None and detail != last_detail:
            status_detail_cb(detail)
            last_detail = detail
        if label != last_label:
            print(f"[Kimodo] {label}", flush=True)
            last_label = label
        thread.join(timeout=0.1)
    # The loop only ends once the loader thread has, so no hook can still be running. Clearing keeps
    # the per-encode ferry that reuses these hooks from reporting a phase outside a load.
    if phase_hooks is not None:
        phase_hooks["state"] = None
    progress_cb(1.0)
    # Drop any lingering byte-count detail; the status code itself is cleared by infer_only.
    if status_detail_cb is not None and last_detail:
        status_detail_cb("")

    if state.error is not None:
        raise state.error
    return state.model


def _normalize_cached_cuda_placement(state):
    """Resolve legacy CUDA aliases from resident motion tensors without probing CUDA.
    CPU parked models retain ambiguous aliases until a reload.
    """
    if state.model_cache is None or state.model_cache[2] != "cuda":
        return
    model, frame_rate, _cached_device = state.model_cache
    motion_model = getattr(model, "denoiser", model)
    for accessor_name in ("parameters", "buffers"):
        accessor = getattr(motion_model, accessor_name, None)
        if accessor is None:
            continue
        for tensor in accessor():
            if tensor.device.type != "cuda" or tensor.device.index is None:
                continue
            indexed_device = str(tensor.device)
            state.model_cache = (model, frame_rate, indexed_device)
            if state.loaded_device == "cuda":
                state.loaded_device = indexed_device
            if state.loaded_text_encoder_device == "cuda":
                state.loaded_text_encoder_device = indexed_device
            text_encoder = getattr(model, "text_encoder", None)
            if (_is_cached_text_encoder(text_encoder)
                    and text_encoder._compute_device == "cuda"):
                text_encoder._compute_device = indexed_device
            if getattr(model, "device", None) == "cuda":
                model.device = indexed_device
            return


def _get_model(progress_cb=None, status_code_cb=None, status_detail_cb=None,
               park_text_encoder=False, text_encoder_precision=None):
    """Load the Kimodo model on first use and cache it. If a prior Unload parked it on CPU, move it
    back to the active device before returning so callers always see it on the expected device.
    Also completes the SOMA 30 skeleton with the stream buffers the UE export path reads.
    Status callbacks report the load, download, and device move phases to the editor.
    """
    state = _get_persistent_state()
    target_device = _get_device()
    _normalize_cached_cuda_placement(state)
    text_encoder_device = _get_text_encoder_device(target_device)
    text_encoder_precision = _resolve_text_encoder_precision(text_encoder_precision)
    if (text_encoder_precision != TEXT_ENCODER_PRECISION_FULL
            and getattr(state, "text_encoder_quantization_unavailable", False)):
        # The first nf4 attempt on this host already failed and fell back. Serve full precision
        # directly so the request matches the cached full precision model instead of dropping it
        # and paying a guaranteed-to-fail quantized reload first.
        print("[Kimodo] Text encoder quantization is unavailable on this host (an earlier nf4 load "
              "failed); using full precision.", flush=True)
        text_encoder_precision = TEXT_ENCODER_PRECISION_FULL

    # Parking only makes sense for a GPU run with a GPU-computing encoder; a CPU run or a CPU
    # placement has the encoder in RAM already.
    park_text_encoder = (park_text_encoder and target_device != "cpu"
                         and text_encoder_device != "cpu")

    if (state.model_cache is not None
            and state.loaded_device == target_device
            and state.loaded_text_encoder_precision == text_encoder_precision
            and text_encoder_precision == TEXT_ENCODER_PRECISION_NF4
            and (state.loaded_text_encoder_device != text_encoder_device
                 or state.loaded_text_encoder_parked != park_text_encoder)):
        if _reuse_nf4_text_encoder(
                getattr(state.model_cache[0], "text_encoder", None),
                text_encoder_device, park_text_encoder, progress_cb,
                status_code_cb, status_detail_cb):
            state.loaded_text_encoder_device = text_encoder_device
            state.loaded_text_encoder_parked = park_text_encoder

    # Other placement or precision changes need matching storage tensors.
    if state.model_cache is not None and (state.loaded_device != target_device
                                          or state.loaded_text_encoder_device != text_encoder_device
                                          or state.loaded_text_encoder_parked != park_text_encoder
                                          or state.loaded_text_encoder_precision != text_encoder_precision):
        print(f"[Kimodo] Placement changed from '{state.loaded_device}' (text encoder "
              f"'{state.loaded_text_encoder_device}', parked={state.loaded_text_encoder_parked}, "
              f"precision={state.loaded_text_encoder_precision}) to "
              f"'{target_device}' (text encoder '{text_encoder_device}', parked={park_text_encoder}, "
              f"precision={text_encoder_precision}); "
              "dropping cached model so it can be reloaded to match.")
        state.model_cache = None
        state.loaded_device = None
        state.loaded_text_encoder_device = None
        state.loaded_text_encoder_parked = None
        state.loaded_text_encoder_precision = None
        gc.collect()
        _clear_cuda_cache()

    if state.model_cache is None:
        # Report the first load phase (checkpoint download, text encoder download, or plain load)
        # up front, so the status is right before the watcher produces its first signals. Reporting
        # here rather than in infer_only also covers a placement-change reload of a warm cache.
        if status_code_cb is not None:
            status_code_cb(_initial_load_status_code())
        text_encoder_fp32 = (target_device == "cpu") or (text_encoder_device == "cpu")
        if text_encoder_device == "cpu" and os.environ.get("KIMODO_CPU_BF16") == "1":
            text_encoder_fp32 = False
        print(f"[Kimodo] Loading {_PINNED_KIMODO_MODEL} on device '{target_device}'"
              f" (text_encoder_device='{text_encoder_device}',"
              f" text_encoder_fp32={text_encoder_fp32},"
              f" text_encoder_precision='{text_encoder_precision}')...")
        model = _load_model_with_progress(target_device, text_encoder_fp32=text_encoder_fp32,
            progress_cb=progress_cb, status_code_cb=status_code_cb,
            status_detail_cb=status_detail_cb, park_text_encoder=park_text_encoder,
            text_encoder_device=text_encoder_device,
            text_encoder_precision=text_encoder_precision)
        frame_rate = int(round(float(model.fps)))
        state.model_cache = (model, frame_rate, target_device)
        state.loaded_device = target_device
        state.loaded_text_encoder_device = text_encoder_device
        state.loaded_text_encoder_parked = park_text_encoder
        # Record what actually loaded rather than what was asked for, by inspecting the model for
        # the layer type the quantizer swaps in. A quantized load that fell back to full precision
        # would otherwise key the cache as nf4, so every later nf4 request would match the cache and
        # silently keep serving the unquantized encoder with no signal that the downgrade happened.
        # Only trust the probe when it recognizes the encoder; None means it could not tell, and
        # storing that would drop a usable cache on every request.
        detected_precision = _read_cached_text_encoder_precision(state)
        state.loaded_text_encoder_precision = (detected_precision if detected_precision is not None
                                               else text_encoder_precision)
        if state.loaded_text_encoder_precision != text_encoder_precision:
            print(f"[Kimodo] Text encoder precision '{text_encoder_precision}' was requested but "
                  f"'{state.loaded_text_encoder_precision}' actually loaded.", flush=True)
        print(f"[Kimodo] Model loaded. FPS={frame_rate}")
    else:
        model, frame_rate, cached_device = state.model_cache
        if cached_device != target_device:
            # Same loaded_device but parked elsewhere. A fast PCIe move restores it without disk
            # reads. A proxied encoder stays in system RAM instead of following the model over.
            print(f"[Kimodo] Moving cached model from '{cached_device}' to '{target_device}'...")
            if status_code_cb is not None and target_device != "cpu":
                status_code_cb(KIMODO_STATUS_UPLOADING_TO_GPU)
            _move_kimodo_model_to(model, target_device)
            state.model_cache = (model, frame_rate, target_device)
            print("[Kimodo] Model restored to active device.")
        else:
            print("[Kimodo] Reusing cached model.")

    _install_cached_text_encoder(
        state.model_cache[0], text_encoder_device, park_on_cpu=park_text_encoder,
    )

    # The UE SOMA asset uses the native 30 joint skeleton, so complete the skeleton with the stream
    # buffers its asset folder lacks. Idempotent, and also covers a model cached by an older script.
    _ensure_native_soma30_buffers(state.model_cache[0].skeleton)
    return state.model_cache


def _move_kimodo_model_to(model, device):
    """Move the motion model and resident GPU text encoder to the target device.
    Parked and CPU-computing encoders manage their placement separately.
    """
    text_encoder = getattr(model, "text_encoder", None)
    if text_encoder is not None and hasattr(text_encoder, "to"):
        cached_encoder = _is_cached_text_encoder(text_encoder)
        if (not cached_encoder
                or (text_encoder._compute_device != "cpu"
                    and not getattr(text_encoder, "_park_on_cpu", True))):
            text_encoder.to(device)
    model.to(device)


def unload_kimodo(mode=None):
    """Unload the cached Kimodo model. Mode 'cpu' parks weights on CPU RAM and frees VRAM; 'complete'
    drops the model entirely. Defaults to KIMODO_UNLOAD_MODE then 'cpu'; safe when no model is loaded.
    """
    state = _get_persistent_state()
    if state.model_cache is None:
        print("[Kimodo] Unload: no model is loaded; nothing to do.")
        return

    if mode is None:
        mode = os.environ.get("KIMODO_UNLOAD_MODE", "cpu")
    mode = str(mode).lower()
    model, frame_rate, _cached_device = state.model_cache

    if mode == "complete":
        state.model_cache = None
        state.loaded_device = None
        state.loaded_text_encoder_device = None
        state.loaded_text_encoder_parked = None
        state.loaded_text_encoder_precision = None
        # Drop the local reference so the only remaining strong ref is gone
        # before we ask the allocator to release its pool.
        del model
        gc.collect()
        _clear_cuda_cache()
        print("[Kimodo] Unload: model dropped; VRAM and Python-side references released.")
        return

    # CPU park: move both the nn.Module children and the plain LLM2VecEncoder wrapper, then
    # collect unreferenced CUDA tensors and return the allocator's pool to the OS.
    _move_kimodo_model_to(model, "cpu")
    state.model_cache = (model, frame_rate, "cpu")
    gc.collect()
    _clear_cuda_cache()
    print("[Kimodo] Unload: model parked on CPU; VRAM released.")


def _clear_cuda_cache(compute_device=None):
    if not torch.cuda.is_initialized():
        return False
    if compute_device is None:
        state = _get_persistent_state()
        if not hasattr(state, "cuda_device_index"):
            state.cuda_device_index = torch.cuda.current_device()
        device_index = state.cuda_device_index
    else:
        device = torch.device(compute_device)
        if device.type != "cuda":
            return False
        device_index = (device.index if device.index is not None
                        else torch.cuda.current_device())
    with torch.cuda.device(device_index):
        torch.cuda.synchronize(device_index)
        torch.cuda.empty_cache()
    return True


def _trim_gpu_allocator(compute_device=None):
    """Drop unreferenced CUDA tensors and return the caching allocator's free pool to the driver.
    Run after every generation: frame and constraint counts vary per request, so the pool fragments
    and memory_reserved() only ever grows without this.
    """
    gc.collect()
    if not _clear_cuda_cache(compute_device):
        return
    device_index = (torch.device(compute_device) if compute_device is not None
                    else _get_persistent_state().cuda_device_index)
    reserved_mib = torch.cuda.memory_reserved(device_index) // (1024 * 1024)
    allocated_mib = torch.cuda.memory_allocated(device_index) // (1024 * 1024)
    print(f"[Kimodo] GPU memory after trim: {allocated_mib} MiB allocated, "
          f"{reserved_mib} MiB reserved.", flush=True)


@dataclass
class _GenerationInputs:
    """Snapshot of the request input dict, parsed once per generation."""
    description: str
    segment_descriptions: list             # list[str], empty when single-prompt
    segment_num_frames: list               # list[int], parallel to segment_descriptions
    duration_seconds: float
    num_frames: int
    random_seed: int
    num_variations: int                    # Variations per request, batched in one call.
    use_trajectory: bool
    park_text_encoder: bool                # Keep the 8B text encoder in system RAM, not VRAM.
    text_encoder_precision: str            # "full" or "nf4" weight precision.
    post_processing: bool
    root_margin: float
    foot_contact_threshold: float          # Confidence above which a predicted foot contact counts as planted.
    diffusion_steps: int
    enable_cfg: bool
    cfg_text_weight: float
    cfg_constraint_weight: float
    num_transition_frames: int             # Multi-prompt segment blend length in frames.
    percentage_transition_override: float  # Extra shared transition as a fraction of the previous segment.
    first_heading_orientation: np.ndarray  # (4,) float32 wxyz initial heading, empty (0,) when unset.
    trajectory_positions: np.ndarray         # (T, 3) float32, possibly empty (0, 3)
    trajectory_orientations: np.ndarray      # (T, 4) float32 wxyz, possibly empty (0, 4)
    target_pose_frame_indices: np.ndarray    # (K,) int64, possibly empty
    target_pose_bone_names: list             # list[str]
    target_pose_positions: np.ndarray        # (K, B, 3) float32; (0, B, 3) when buffer absent/mismatched
    target_pose_rotations: np.ndarray        # (K, B, 4) float32 wxyz world-space; (0, B, 4) when absent/mismatched
    target_pose_fullbody_flags: np.ndarray   # (K,) bool, True where the frame is a whole-body target pose
    target_effector_frame_indices: np.ndarray  # (M,) int64, subset of target_pose_frame_indices
    target_effector_names: list              # list[str] parallel to target_effector_frame_indices
    target_transform_frame_indices: np.ndarray   # (KT,) int64, possibly empty. Populated only when use_trajectory is False.
    target_transform_positions: np.ndarray       # (KT, 3) float32, Kimodo Y-up meters. Possibly empty (0, 3).
    target_transform_orientations: np.ndarray    # (KT, 4) float32 wxyz, Kimodo Y-up. Possibly empty (0, 4).
    target_transform_heading_flags: np.ndarray   # (KT,) bool, False where a key pins position only.


# Hard cap on variations per batched diffusion call, matching UKimodoPoseGenerator::MaxNumVariations
# and the worker transport's MaxVariations, which must all change together. The worker also rejects
# requests beyond it up front, before burning a full inference.
KIMODO_MAX_VARIATIONS = 8


def _read_inputs(input_dict):
    """Parse the request input dict exactly once so the downstream helpers stay pure-numpy."""
    description = str(input_dict.get("description", ""))
    segment_descriptions = [str(s) for s in input_dict.get("segment_descriptions", [])]
    segment_num_frames = [int(n) for n in input_dict.get("segment_num_frames", [])]
    num_frames = int(input_dict["num_frames"])
    frame_rate = max(1, int(input_dict.get("frame_rate", 30)))
    duration_seconds = num_frames / float(frame_rate)
    random_seed = int(input_dict.get("random_seed", 0))
    # kimodo_worker.py is the trust boundary and rejects a non-positive count, one over
    # KIMODO_MAX_VARIATIONS, or an over-budget product before this runs, so no clamp is needed here.
    num_variations = int(input_dict.get("num_variations", 1))
    use_trajectory = bool(input_dict.get("use_trajectory", True))
    post_processing = bool(input_dict.get("post_processing", True))
    root_margin = float(input_dict.get("root_margin", 0.04))
    foot_contact_threshold = float(input_dict.get("foot_contact_threshold", 0.5))
    diffusion_steps = int(input_dict.get("diffusion_steps", 100))
    enable_cfg = bool(input_dict.get("enable_cfg", True))
    cfg_text_weight = float(input_dict.get("cfg_text_weight", 2.0))
    cfg_constraint_weight = float(input_dict.get("cfg_constraint_weight", 2.0))
    num_transition_frames = max(0, int(input_dict.get("num_transition_frames", 5)))
    percentage_transition_override = float(input_dict.get("percentage_transition_override", 0.10))

    # The initial heading arrives as a wxyz quaternion in Kimodo's frame. An empty or malformed
    # buffer means unset, which keeps the model's own default heading.
    first_heading_orientation_flat = np.asarray(
        input_dict.get("first_heading_orientation", []), dtype=np.float32)
    if first_heading_orientation_flat.size == 4:
        first_heading_orientation = first_heading_orientation_flat
    else:
        first_heading_orientation = np.zeros(0, dtype=np.float32)

    trajectory_positions_flat = np.asarray(
        input_dict.get("trajectory_positions", []), dtype=np.float32)
    trajectory_orientations_flat = np.asarray(
        input_dict.get("trajectory_orientations", []), dtype=np.float32)

    if trajectory_positions_flat.size > 0 and trajectory_positions_flat.size % 3 == 0:
        trajectory_positions = trajectory_positions_flat.reshape(-1, 3)
    else:
        trajectory_positions = np.zeros((0, 3), dtype=np.float32)
    if trajectory_orientations_flat.size > 0 and trajectory_orientations_flat.size % 4 == 0:
        trajectory_orientations = trajectory_orientations_flat.reshape(-1, 4)
    else:
        trajectory_orientations = np.zeros((0, 4), dtype=np.float32)

    # The worker fills both trajectory buffers from the same frame count, so a mismatch means a corrupt
    # or foreign producer. Truncating keeps the downstream constraint builders indexing in lockstep.
    if trajectory_positions.shape[0] != trajectory_orientations.shape[0]:
        num_shared_frames = min(trajectory_positions.shape[0], trajectory_orientations.shape[0])
        print(f"[Kimodo] Trajectory buffer frame count mismatch "
              f"({trajectory_positions.shape[0]} positions vs "
              f"{trajectory_orientations.shape[0]} orientations); "
              f"truncating both to {num_shared_frames} frames.")
        trajectory_positions = trajectory_positions[:num_shared_frames]
        trajectory_orientations = trajectory_orientations[:num_shared_frames]

    target_pose_bone_names = [str(name) for name in input_dict.get("target_pose_bone_names", [])]
    target_pose_frame_indices = np.asarray(
        input_dict.get("target_pose_frame_indices", []), dtype=np.int64)

    num_sampled_bones = len(target_pose_bone_names)
    num_keyframes = int(target_pose_frame_indices.size)

    target_pose_positions_flat = np.asarray(
        input_dict.get("target_pose_positions", []), dtype=np.float32)
    target_pose_rotations_flat = np.asarray(
        input_dict.get("target_pose_rotations", []), dtype=np.float32)

    expected_pos = num_keyframes * num_sampled_bones * 3
    expected_rot = num_keyframes * num_sampled_bones * 4

    if num_keyframes > 0 and num_sampled_bones > 0 and target_pose_positions_flat.size == expected_pos:
        target_pose_positions = target_pose_positions_flat.reshape(num_keyframes, num_sampled_bones, 3)
    else:
        if num_keyframes > 0 and num_sampled_bones > 0:
            print(f"[Kimodo] Target pose buffer size mismatch "
                  f"({target_pose_positions_flat.size} vs {expected_pos}); "
                  f"target pose constraint will be skipped.")
        target_pose_positions = np.zeros((0, max(num_sampled_bones, 1), 3), dtype=np.float32)

    if num_keyframes > 0 and num_sampled_bones > 0 and target_pose_rotations_flat.size == expected_rot:
        target_pose_rotations = target_pose_rotations_flat.reshape(num_keyframes, num_sampled_bones, 4)
    else:
        if num_keyframes > 0 and num_sampled_bones > 0 and target_pose_rotations_flat.size > 0:
            print(f"[Kimodo] Target pose rotation buffer size mismatch "
                  f"({target_pose_rotations_flat.size} vs {expected_rot}); "
                  f"per-bone keyframe rotations will fall back to the rest-yaw baseline.")
        target_pose_rotations = np.zeros((0, max(num_sampled_bones, 1), 4), dtype=np.float32)

    # An absent or mismatched flag array means every flagged frame is a whole-body target pose, which is
    # how the editor behaved before end effector targets existed.
    fullbody_flags_raw = np.asarray(
        input_dict.get("target_pose_fullbody_flags", []), dtype=np.int64)
    if fullbody_flags_raw.size == num_keyframes:
        target_pose_fullbody_flags = fullbody_flags_raw.astype(bool)
    else:
        if fullbody_flags_raw.size > 0:
            print(f"[Kimodo] Target pose full-body flag count mismatch "
                  f"({fullbody_flags_raw.size} vs {num_keyframes}); "
                  f"treating every target pose keyframe as full body.")
        target_pose_fullbody_flags = np.ones(num_keyframes, dtype=bool)

    target_effector_frame_indices_raw = np.asarray(
        input_dict.get("target_effector_frame_indices", []), dtype=np.int64)
    target_effector_names_raw = [str(name) for name in input_dict.get("target_effector_names", [])]

    # The two arrays are parallel, so a length mismatch means one of them is stale. Keep the common
    # prefix rather than dropping every effector target.
    num_effector_keys = min(int(target_effector_frame_indices_raw.size), len(target_effector_names_raw))
    if int(target_effector_frame_indices_raw.size) != len(target_effector_names_raw):
        print(f"[Kimodo] Effector target arrays are not parallel "
              f"({target_effector_frame_indices_raw.size} frames vs "
              f"{len(target_effector_names_raw)} names); using the first {num_effector_keys}.")

    target_effector_frame_indices = []
    target_effector_names = []
    for effector_index in range(num_effector_keys):
        effector_name = target_effector_names_raw[effector_index]
        if effector_name not in _LEGAL_EFFECTOR_NAMES:
            print(f"[Kimodo] Unknown effector '{effector_name}'; skipping. "
                  f"Supported: {', '.join(_LEGAL_EFFECTOR_NAMES)}.")
            continue
        target_effector_frame_indices.append(int(target_effector_frame_indices_raw[effector_index]))
        target_effector_names.append(effector_name)
    target_effector_frame_indices = np.asarray(target_effector_frame_indices, dtype=np.int64)

    target_transform_frame_indices = np.asarray(
        input_dict.get("target_transform_frame_indices", []), dtype=np.int64)
    target_transform_positions_flat = np.asarray(
        input_dict.get("target_transform_positions", []), dtype=np.float32)
    target_transform_orientations_flat = np.asarray(
        input_dict.get("target_transform_orientations", []), dtype=np.float32)

    num_target_transform_keys = int(target_transform_frame_indices.size)
    expected_tt_pos = num_target_transform_keys * 3
    expected_tt_rot = num_target_transform_keys * 4

    if num_target_transform_keys > 0 and target_transform_positions_flat.size == expected_tt_pos:
        target_transform_positions = target_transform_positions_flat.reshape(num_target_transform_keys, 3)
    else:
        if num_target_transform_keys > 0:
            print(f"[Kimodo] Target transform position buffer size mismatch "
                  f"({target_transform_positions_flat.size} vs {expected_tt_pos}); "
                  f"target transform constraint will be skipped.")
            target_transform_frame_indices = np.zeros(0, dtype=np.int64)
        target_transform_positions = np.zeros((0, 3), dtype=np.float32)

    if num_target_transform_keys > 0 and target_transform_orientations_flat.size == expected_tt_rot:
        target_transform_orientations = target_transform_orientations_flat.reshape(num_target_transform_keys, 4)
    else:
        if num_target_transform_keys > 0:
            print(f"[Kimodo] Target transform orientation buffer size mismatch "
                  f"({target_transform_orientations_flat.size} vs {expected_tt_rot}); "
                  f"target transform constraint will be skipped.")
            target_transform_frame_indices = np.zeros(0, dtype=np.int64)
            target_transform_positions = np.zeros((0, 3), dtype=np.float32)
        target_transform_orientations = np.zeros((0, 4), dtype=np.float32)

    # Sized against the keys that survived the buffer validation above. An absent or mismatched flag
    # array constrains the heading at every key, which is how the editor behaved before the flags existed.
    heading_flags_raw = np.asarray(
        input_dict.get("target_transform_heading_flags", []), dtype=np.int64)
    num_surviving_tt_keys = int(target_transform_frame_indices.size)
    if heading_flags_raw.size == num_surviving_tt_keys:
        target_transform_heading_flags = heading_flags_raw.astype(bool)
    else:
        if heading_flags_raw.size > 0 and num_surviving_tt_keys > 0:
            print(f"[Kimodo] Target transform heading flag count mismatch "
                  f"({heading_flags_raw.size} vs {num_surviving_tt_keys}); "
                  f"constraining the heading at every key.")
        target_transform_heading_flags = np.ones(num_surviving_tt_keys, dtype=bool)

    return _GenerationInputs(
        description=description,
        segment_descriptions=segment_descriptions,
        segment_num_frames=segment_num_frames,
        duration_seconds=duration_seconds,
        num_frames=num_frames,
        random_seed=random_seed,
        num_variations=num_variations,
        use_trajectory=use_trajectory,
        park_text_encoder=bool(input_dict.get("park_text_encoder", False)),
        text_encoder_precision=str(input_dict.get("text_encoder_precision", "")),
        post_processing=post_processing,
        root_margin=root_margin,
        foot_contact_threshold=foot_contact_threshold,
        diffusion_steps=diffusion_steps,
        enable_cfg=enable_cfg,
        cfg_text_weight=cfg_text_weight,
        cfg_constraint_weight=cfg_constraint_weight,
        num_transition_frames=num_transition_frames,
        percentage_transition_override=percentage_transition_override,
        first_heading_orientation=first_heading_orientation,
        trajectory_positions=trajectory_positions,
        trajectory_orientations=trajectory_orientations,
        target_pose_frame_indices=target_pose_frame_indices,
        target_pose_bone_names=target_pose_bone_names,
        target_pose_positions=target_pose_positions,
        target_pose_rotations=target_pose_rotations,
        target_pose_fullbody_flags=target_pose_fullbody_flags,
        target_effector_frame_indices=target_effector_frame_indices,
        target_effector_names=target_effector_names,
        target_transform_frame_indices=target_transform_frame_indices,
        target_transform_positions=target_transform_positions,
        target_transform_orientations=target_transform_orientations,
        target_transform_heading_flags=target_transform_heading_flags,
    )


def _wxyz_yaw_around_y(quat_wxyz):
    """Extract yaw (rotation around Y) from a (..., 4) wxyz quaternion array, right-handed."""
    quat_w = quat_wxyz[..., 0]
    quat_x = quat_wxyz[..., 1]
    quat_y = quat_wxyz[..., 2]
    quat_z = quat_wxyz[..., 3]
    numerator = 2.0 * (quat_w * quat_y + quat_x * quat_z)
    denominator = 1.0 - 2.0 * (quat_y * quat_y + quat_z * quat_z)
    return np.arctan2(numerator, denominator)


def _derive_pose_self_anchor(skeleton, sampled_bone_names, sampled_positions):
    """Derive a per-keyframe (position, yaw) anchor from sampled bones when no trajectory is given.
    Position is the sampled root joint or hip midpoint; yaw is atan2(hip.z, -hip.x) on the leg vector.
    """
    num_keyframes = sampled_positions.shape[0]
    if num_keyframes == 0:
        return None, None

    name_to_sampled = {name: idx for idx, name in enumerate(sampled_bone_names)}

    root_kimodo_index = int(skeleton.root_idx)
    root_kimodo_name = skeleton.bone_order_names[root_kimodo_index]

    anchor_positions = None
    if root_kimodo_name in name_to_sampled:
        sampled_idx = name_to_sampled[root_kimodo_name]
        anchor_positions = sampled_positions[:, sampled_idx, :].astype(np.float32, copy=True)
    elif "RightLeg" in name_to_sampled and "LeftLeg" in name_to_sampled:
        right_idx = name_to_sampled["RightLeg"]
        left_idx = name_to_sampled["LeftLeg"]
        anchor_positions = (
            0.5 * (sampled_positions[:, right_idx, :] + sampled_positions[:, left_idx, :])
        ).astype(np.float32, copy=True)

    if anchor_positions is None:
        return None, None

    if "RightLeg" in name_to_sampled and "LeftLeg" in name_to_sampled:
        right_idx = name_to_sampled["RightLeg"]
        left_idx = name_to_sampled["LeftLeg"]
        hip_diff = sampled_positions[:, right_idx, :] - sampled_positions[:, left_idx, :]
        anchor_yaw = np.arctan2(hip_diff[:, 2], -hip_diff[:, 0]).astype(np.float32)
    else:
        anchor_yaw = np.zeros(num_keyframes, dtype=np.float32)

    return anchor_positions, anchor_yaw


def _build_trajectory_constraint(inputs, skeleton, num_frames, device):
    """Build a Root2DConstraintSet from the trajectory buffers."""
    trajectory_positions = inputs.trajectory_positions
    trajectory_orientations = inputs.trajectory_orientations

    num_trajectory_frames = trajectory_positions.shape[0]
    if num_trajectory_frames == 0:
        return None

    num_constrained_frames = int(min(num_trajectory_frames, num_frames))
    if num_constrained_frames <= 0:
        return None

    smooth_root_2d_np = trajectory_positions[:num_constrained_frames][:, [0, 2]]
    smooth_root_2d = torch.from_numpy(np.ascontiguousarray(smooth_root_2d_np)).to(device)

    yaw_angles = _wxyz_yaw_around_y(trajectory_orientations[:num_constrained_frames]).astype(np.float32)
    yaw_angles_tensor = torch.from_numpy(np.ascontiguousarray(yaw_angles)).to(device)
    global_root_heading = torch.stack(
        [torch.cos(yaw_angles_tensor), torch.sin(yaw_angles_tensor)], dim=-1
    )

    frame_indices = torch.arange(num_constrained_frames, device=device)

    return Root2DConstraintSet(
        skeleton,
        frame_indices=frame_indices,
        smooth_root_2d=smooth_root_2d,
        global_root_heading=global_root_heading,
    )


def _blend_trajectory_to_pose_constraints(trajectory, pose_constraints):
    """Blend target root offsets while retaining the sampled path's shape.
    Unkeyed endpoints keep their trajectory position and heading.
    """
    if not pose_constraints or trajectory.frame_indices.numel() == 0:
        return trajectory

    device = trajectory.frame_indices.device
    pose_frames = torch.cat([
        constraint.frame_indices.to(device) for constraint in pose_constraints
    ])
    pose_positions = torch.cat([
        constraint.smooth_root_2d.to(device) for constraint in pose_constraints
    ])
    pose_headings = torch.cat([
        constraint.global_root_heading.to(device)
        for constraint in pose_constraints
    ])
    order = torch.argsort(pose_frames, stable=True)
    pose_frames = pose_frames[order]
    unique_mask = torch.cat((
        torch.ones(1, dtype=torch.bool, device=device),
        pose_frames[1:] != pose_frames[:-1],
    ))
    pose_frames = pose_frames[unique_mask]
    pose_positions = pose_positions[order][unique_mask]
    pose_headings = pose_headings[order][unique_mask]

    frame_indices = trajectory.frame_indices
    in_range = (
        (pose_frames >= frame_indices[0]) & (pose_frames <= frame_indices[-1])
    )
    pose_frames = pose_frames[in_range]
    if pose_frames.numel() == 0:
        return trajectory
    pose_positions = pose_positions[in_range]
    pose_headings = pose_headings[in_range]
    pose_rows = torch.searchsorted(frame_indices, pose_frames)
    position_offsets = pose_positions - trajectory.smooth_root_2d[pose_rows]
    trajectory_yaw = torch.atan2(
        trajectory.global_root_heading[:, 1],
        trajectory.global_root_heading[:, 0],
    )
    heading_offsets = (
        torch.atan2(pose_headings[:, 1], pose_headings[:, 0])
        - trajectory_yaw[pose_rows]
    )
    heading_offsets = torch.atan2(
        torch.sin(heading_offsets), torch.cos(heading_offsets)
    )
    offsets = torch.cat((position_offsets, heading_offsets[:, None]), dim=1)

    endpoint_frames = torch.stack((frame_indices[0], frame_indices[-1]))
    endpoint_frames = endpoint_frames[
        ~torch.isin(endpoint_frames, pose_frames)
    ]
    anchor_frames = torch.cat((pose_frames, endpoint_frames))
    anchor_offsets = torch.cat((
        offsets, offsets.new_zeros((endpoint_frames.numel(), 3))
    ))
    order = torch.argsort(anchor_frames)
    anchor_frames = anchor_frames[order]
    anchor_offsets = anchor_offsets[order]

    right_rows = torch.searchsorted(anchor_frames, frame_indices).clamp(
        max=anchor_frames.numel() - 1
    )
    left_rows = (right_rows - 1).clamp(min=0)
    spans = (anchor_frames[right_rows] - anchor_frames[left_rows]).clamp(min=1)
    alpha = ((frame_indices - anchor_frames[left_rows]) / spans).to(
        offsets.dtype
    )
    weights = (alpha * alpha * (3.0 - 2.0 * alpha))[:, None]
    offset_delta = anchor_offsets[right_rows] - anchor_offsets[left_rows]
    offset_delta[:, 2] = torch.atan2(
        torch.sin(offset_delta[:, 2]), torch.cos(offset_delta[:, 2])
    )
    blended_offsets = anchor_offsets[left_rows] + weights * offset_delta
    corrected_yaw = trajectory_yaw + blended_offsets[:, 2]

    return Root2DConstraintSet(
        trajectory.skeleton,
        frame_indices=frame_indices,
        smooth_root_2d=trajectory.smooth_root_2d + blended_offsets[:, :2],
        global_root_heading=torch.stack(
            (torch.cos(corrected_yaw), torch.sin(corrected_yaw)), dim=-1
        ),
    )


def _frame_zero_pose_heading_angle(pose_constraints):
    """Heading in radians that a pose constraint covering frame 0 conditions the model with, or None
    when none does. Read off the constraint itself, which derives it from the authored hip vector.
    """
    for constraint in pose_constraints:
        global_root_heading = getattr(constraint, "global_root_heading", None)
        if global_root_heading is None:
            continue
        matching_rows = torch.nonzero(constraint.frame_indices == 0, as_tuple=False)
        if matching_rows.numel() == 0:
            continue
        heading = global_root_heading[int(matching_rows[0])]
        return float(torch.atan2(heading[1], heading[0]))
    return None


def _resolve_first_heading_angle(inputs, pose_constraints=()):
    """Pick the initial body heading in radians for the model's first heading conditioning, or None.
    Trajectory mode uses the frame 0 trajectory orientation. Target Transform mode prefers the frame 0
    pose constraint's own heading, then a heading key on frame 0, and falls back to the actor's heading
    at the section start. Every path reads the heading whichever constraint owns frame 0 carries, so
    the two conditioning channels never disagree there.
    """
    if inputs.use_trajectory:
        if inputs.trajectory_orientations.shape[0] > 0:
            return float(_wxyz_yaw_around_y(inputs.trajectory_orientations[0]))
        return None

    # A pose constraint on frame 0 drops any trajectory key that overlaps it and conditions the
    # heading channel from its own hip vector, so that value wins over the actor's section-start yaw.
    pose_heading_angle = _frame_zero_pose_heading_angle(pose_constraints)
    if pose_heading_angle is not None:
        return pose_heading_angle

    frame_indices = inputs.target_transform_frame_indices
    orientations = inputs.target_transform_orientations
    heading_flags = inputs.target_transform_heading_flags
    if (frame_indices.size > 0 and orientations.shape[0] == frame_indices.size
            and heading_flags.size == frame_indices.size):
        frame_zero_mask = (frame_indices == 0) & heading_flags
        if frame_zero_mask.any():
            first_key = int(np.flatnonzero(frame_zero_mask)[0])
            return float(_wxyz_yaw_around_y(orientations[first_key]))

    if inputs.first_heading_orientation.size == 4:
        return float(_wxyz_yaw_around_y(inputs.first_heading_orientation))
    return None


def _build_target_transform_constraint(inputs, skeleton, num_frames, device):
    """Build Root2DConstraintSets from sparse actor transforms on the Target Transform channel. Used
    only when Follow Sequencer Trajectory is off; each key adds one (xz, yaw) constraint at its grid
    frame. Keys flagged position-only go into a second, heading-free set. Returns a list of 0 to 2 sets.
    """
    frame_indices_np = inputs.target_transform_frame_indices
    positions = inputs.target_transform_positions
    orientations = inputs.target_transform_orientations
    heading_flags = inputs.target_transform_heading_flags

    if frame_indices_np.size == 0 or positions.shape[0] == 0 or orientations.shape[0] == 0:
        return []

    # Drop keyframes outside the generation window.
    in_range_mask = (frame_indices_np >= 0) & (frame_indices_np < num_frames)
    if not in_range_mask.any():
        return []
    frame_indices_np = frame_indices_np[in_range_mask]
    positions = positions[in_range_mask]
    orientations = orientations[in_range_mask]
    if heading_flags.size == in_range_mask.size:
        heading_flags = heading_flags[in_range_mask]
    else:
        heading_flags = np.ones(frame_indices_np.size, dtype=bool)

    def build_set(subset_mask, with_heading):
        subset_frames = frame_indices_np[subset_mask]
        if subset_frames.size == 0:
            return None

        smooth_root_2d_np = np.ascontiguousarray(positions[subset_mask][:, [0, 2]])
        smooth_root_2d = torch.from_numpy(smooth_root_2d_np).to(device)

        global_root_heading = None
        if with_heading:
            yaw_angles = _wxyz_yaw_around_y(orientations[subset_mask]).astype(np.float32)
            yaw_angles_tensor = torch.from_numpy(np.ascontiguousarray(yaw_angles)).to(device)
            global_root_heading = torch.stack(
                [torch.cos(yaw_angles_tensor), torch.sin(yaw_angles_tensor)], dim=-1
            )

        frame_indices = torch.from_numpy(np.ascontiguousarray(subset_frames)).to(device)
        return Root2DConstraintSet(
            skeleton,
            frame_indices=frame_indices,
            smooth_root_2d=smooth_root_2d,
            global_root_heading=global_root_heading,
        )

    constraints = []
    heading_set = build_set(heading_flags, with_heading=True)
    if heading_set is not None:
        constraints.append(heading_set)
    # No producer clears a heading flag yet (the C++ fills them uniformly with one), so this set is
    # always None today. It stays for the per-key heading UI the transport already carries room for.
    position_only_set = build_set(~heading_flags, with_heading=False)
    if position_only_set is not None:
        constraints.append(position_only_set)

    print(f"[Kimodo] Target transform constraint: {int(heading_flags.sum())} keyframes with heading, "
          f"{int((~heading_flags).sum())} position-only (Follow Sequencer Trajectory is off).")

    return constraints


@dataclass
class _TargetPoseScaffold:
    """Full-body joint arrays for every flagged frame, shared by the full-body and effector constraints.

    Kimodo's effector constraints take the whole joint array and index their own joints out of it, so one
    scaffold per generation feeds both kinds. Sharing it also keeps the diffusion conditioning and the
    postprocess correction aimed at the same pose, which matters because the two disagree on precedence
    when constraints collide (conditioning keeps the first writer, the correction input keeps the last).
    """
    frame_indices: np.ndarray      # (K,) int64, in-range flagged frames
    full_positions: torch.Tensor   # (K, J, 3) on device
    full_rotations: torch.Tensor   # (K, J, 3, 3) on device
    fullbody_mask: np.ndarray      # (K,) bool
    row_for_frame: dict            # frame index -> row into the arrays above
    mapped_bone_names: set         # sampled bone names that matched the Kimodo skeleton


def _neutral_hip_height(skeleton):
    """Height of the hips over the floor in the skeleton's hip-centered neutral pose, measured down to
    its lowest joint. Converts a floor-level world anchor into the hips anchor the baseline needs.
    """
    neutral_joints = skeleton.neutral_joints.detach().cpu().numpy()
    return float(max(0.0, -neutral_joints[:, 1].min()))


def _build_target_pose_scaffold(inputs, skeleton, num_frames, device):
    """Build the shared joint arrays from per-keyframe sampled bone WORLD positions (Kimodo Y-up, spline
    yaw baked in). Unsampled Kimodo joints are filled from the neutral pose at the anchor.
    """
    if inputs.target_pose_frame_indices.size == 0:
        return None
    sampled_bone_names = inputs.target_pose_bone_names
    num_sampled_bones = len(sampled_bone_names)
    if num_sampled_bones == 0:
        return None

    sampled_positions = inputs.target_pose_positions
    if sampled_positions.shape[0] == 0:
        return None

    # Per-keyframe world anchor (position + yaw) places unmapped joints consistently. The trajectory
    # provides it when available, otherwise self-anchor from sampled root or hip midpoint.
    trajectory_positions = inputs.trajectory_positions
    trajectory_orientations = inputs.trajectory_orientations
    has_trajectory_anchor = (
        trajectory_positions.shape[0] > 0 and trajectory_orientations.shape[0] > 0
    )

    # Drop keyframes outside the generation window, and (when trajectory
    # anchors the constraint) any past the trajectory buffer.
    frame_indices_np = inputs.target_pose_frame_indices
    in_range_mask = (frame_indices_np >= 0) & (frame_indices_np < num_frames)
    if has_trajectory_anchor:
        in_range_mask = in_range_mask & (frame_indices_np < trajectory_positions.shape[0])
    if not in_range_mask.any():
        return None
    frame_indices_np = frame_indices_np[in_range_mask]
    sampled_positions = sampled_positions[in_range_mask]
    num_keyframes = frame_indices_np.shape[0]

    # Filter the flags with the same mask so they stay parallel to the surviving keyframes.
    fullbody_flags = inputs.target_pose_fullbody_flags
    if fullbody_flags.size == in_range_mask.size:
        fullbody_mask = fullbody_flags[in_range_mask]
    else:
        fullbody_mask = np.ones(num_keyframes, dtype=bool)

    # Per-bone authored world rotations (wxyz) match the position buffer's K dimension when present.
    # Mismatches were logged in _read_inputs. We fall back to the R_Y(anchor_yaw) baseline below.
    sampled_rotations_full = inputs.target_pose_rotations
    have_sampled_rotations = (
        sampled_rotations_full.shape[0] == inputs.target_pose_positions.shape[0]
        and sampled_rotations_full.shape[0] > 0
    )
    if have_sampled_rotations:
        sampled_rotations = sampled_rotations_full[in_range_mask]
    else:
        sampled_rotations = None

    nbjoints = skeleton.nbjoints
    name_to_index = {name: idx for idx, name in enumerate(skeleton.bone_order_names)}

    # Pick the per-keyframe anchor. Trajectory provides it when present,
    # otherwise self-anchor from sampled root or hip midpoint.
    if has_trajectory_anchor:
        anchor_yaw = _wxyz_yaw_around_y(
            trajectory_orientations[frame_indices_np]
        ).astype(np.float32)
        # The baseline below offsets hip-centered neutral joints by the anchor, so the anchor has to be
        # the world HIPS position. The trajectory carries the actor root at floor level, so lift it.
        anchor_positions = trajectory_positions[frame_indices_np].astype(np.float32, copy=True)
        anchor_positions[:, 1] += _neutral_hip_height(skeleton)
        anchor_source = "trajectory-anchored"
    else:
        anchor_positions, anchor_yaw = _derive_pose_self_anchor(
            skeleton, sampled_bone_names, sampled_positions
        )
        if anchor_positions is None:
            print("[Kimodo] Pose constraint cannot self-anchor without a sampled root or hip pair "
                  "(Follow Sequencer Trajectory is off and trajectory buffer is empty); "
                  "skipping pose constraint.")
            return None
        anchor_source = "pose-self-anchored"

    # R_Y(anchor_yaw) orients the neutral baseline so unmapped joints face
    # the same heading as the mapped ones.
    cos_yaw = np.cos(anchor_yaw)
    sin_yaw = np.sin(anchor_yaw)
    ry_per_keyframe_np = np.zeros((num_keyframes, 3, 3), dtype=np.float32)
    ry_per_keyframe_np[:, 0, 0] = cos_yaw
    ry_per_keyframe_np[:, 0, 2] = sin_yaw
    ry_per_keyframe_np[:, 1, 1] = 1.0
    ry_per_keyframe_np[:, 2, 0] = -sin_yaw
    ry_per_keyframe_np[:, 2, 2] = cos_yaw

    # Baseline = neutral pose rotated to per-keyframe heading and anchored
    # at the per-keyframe world position. Sampled UE bones overwrite below.
    neutral_joints = skeleton.neutral_joints.detach().cpu().numpy().astype(np.float32)
    full_positions = np.einsum("kij,bj->kbi", ry_per_keyframe_np, neutral_joints)
    full_positions += anchor_positions[:, None, :]

    mapped_bone_names = set()
    for sampled_idx, bone_name in enumerate(sampled_bone_names):
        target_idx = name_to_index.get(bone_name)
        if target_idx is None:
            continue
        full_positions[:, target_idx, :] = sampled_positions[:, sampled_idx, :]
        mapped_bone_names.add(bone_name)

    if not mapped_bone_names:
        print("[Kimodo] No sampled bone names matched the Kimodo skeleton; skipping pose constraint.")
        return None

    full_positions_tensor = torch.from_numpy(full_positions).to(device)

    # FullBodyConstraintSet wants standard T-pose rotations. Rebase the literal Kimodo rotations as
    # literal_world[j] @ offsets[j].T. Baseline R_Y(anchor_yaw), mapped bones overwrite, others inherit.
    ry_per_keyframe_torch = torch.from_numpy(ry_per_keyframe_np).to(device)
    full_rotations = ry_per_keyframe_torch[:, None, :, :].expand(
        num_keyframes, nbjoints, 3, 3
    ).contiguous().clone()

    # SOMASkeleton30 gets sliced somaskel77 offsets registered at model load, so a plain attribute
    # lookup covers it. Skeletons without offsets keep the rest-yaw baseline.
    global_rot_offsets = getattr(skeleton, "global_rot_offsets", None)

    mapped_kimodo_indices = set()
    use_per_bone_rotations = have_sampled_rotations and global_rot_offsets is not None
    if use_per_bone_rotations:
        sampled_rot_quats_torch = torch.from_numpy(np.ascontiguousarray(sampled_rotations)).to(device)
        sampled_rot_matrices_torch = quaternion_to_matrix(sampled_rot_quats_torch)  # (K, B, 3, 3)
        global_rot_offsets_T = (
            global_rot_offsets.to(device=device, dtype=sampled_rot_matrices_torch.dtype).mT
        )  # (J, 3, 3)
        for sampled_idx, bone_name in enumerate(sampled_bone_names):
            target_idx = name_to_index.get(bone_name)
            if target_idx is None:
                continue
            full_rotations[:, target_idx, :, :] = (
                sampled_rot_matrices_torch[:, sampled_idx, :, :] @ global_rot_offsets_T[target_idx]
            )
            mapped_kimodo_indices.add(target_idx)

        # bone_order_names is parent-before-child by Kimodo convention, so a single forward pass
        # over indices is enough to propagate parent rotations into unmapped descendants.
        joint_parents_list = skeleton.joint_parents.tolist()
        for kimodo_idx in range(nbjoints):
            if kimodo_idx in mapped_kimodo_indices:
                continue
            parent_idx = joint_parents_list[kimodo_idx]
            if parent_idx < 0:
                continue  # root: keep R_Y(yaw) baseline
            full_rotations[:, kimodo_idx, :, :] = full_rotations[:, parent_idx, :, :]
    elif have_sampled_rotations:
        # No global_rot_offsets to rebase the C++ rotations into the standard
        # frame. Fall back to the rest-yaw baseline.
        print("[Kimodo] Skeleton has no global_rot_offsets; per-bone authored rotations will be "
              "ignored for the postprocess pin (rest-yaw baseline used).")

    full_rotations_tensor = full_rotations

    return _TargetPoseScaffold(
        frame_indices=frame_indices_np,
        full_positions=full_positions_tensor,
        full_rotations=full_rotations_tensor,
        fullbody_mask=fullbody_mask,
        row_for_frame={int(frame): row for row, frame in enumerate(frame_indices_np)},
        mapped_bone_names=mapped_bone_names,
    )


def _build_full_body_constraint(scaffold, skeleton, device):
    """Build the FullBodyConstraintSet from the scaffold rows flagged as whole-body target poses."""
    rows = np.flatnonzero(scaffold.fullbody_mask)
    if rows.size == 0:
        return None

    frame_indices_np = scaffold.frame_indices[rows]
    print(f"[Kimodo] Target pose constraint: {rows.size} keyframes at frames "
          f"{frame_indices_np.tolist()}.")

    return FullBodyConstraintSet(
        skeleton,
        frame_indices=torch.from_numpy(frame_indices_np).to(device),
        global_joints_positions=scaffold.full_positions[rows],
        global_joints_rots=scaffold.full_rotations[rows],
    )


def _build_effector_constraints(inputs, scaffold, skeleton, device):
    """Build one constraint per end effector from the scaffold, grouping the frames each was keyed at.

    Unlike the full-body constraint these also pin joint rotations, which FullBodyConstraintSet never
    emits, so an effector layered on a full-body frame still contributes its wrist or ankle orientation.
    """
    if inputs.target_effector_frame_indices.size == 0:
        return []

    frames_by_effector = {}
    for effector_name, frame_index in zip(inputs.target_effector_names,
                                          inputs.target_effector_frame_indices.tolist()):
        if frame_index not in scaffold.row_for_frame:
            print(f"[Kimodo] Effector '{effector_name}' at frame {frame_index} has no sampled pose "
                  f"(frame out of range); skipping.")
            continue
        frames_by_effector.setdefault(effector_name, []).append(frame_index)

    constraints = []
    for effector_name, frames in frames_by_effector.items():
        # The effector's joints carry the whole constraint, so a joint missing from the sampled skeleton
        # would pin the limb to the neutral filler pose instead. For the full-body constraint that filler
        # is a deliberate baseline, here it would be wrong, so skip the effector entirely.
        rot_joint_names, pos_joint_names = skeleton.expand_joint_names([effector_name])
        required_joints = set(rot_joint_names) | set(pos_joint_names)
        missing_joints = sorted(required_joints - scaffold.mapped_bone_names)
        if missing_joints:
            print(f"[Kimodo] Effector '{effector_name}' needs joints {missing_joints} which the sampled "
                  f"skeleton does not provide; skipping this effector.")
            continue

        rows = [scaffold.row_for_frame[frame] for frame in frames]
        frame_indices_tensor = torch.from_numpy(np.asarray(frames, dtype=np.int64)).to(device)
        positions = scaffold.full_positions[rows]
        rotations = scaffold.full_rotations[rows]

        # smooth_root_2d None makes the constraint derive it from the captured pose root, which also
        # satisfies the model's rule that a joint position target needs the root constrained at that frame.
        constraint_class = _EFFECTOR_CONSTRAINT_CLASSES[effector_name]
        constraints.append(constraint_class(skeleton, frame_indices_tensor, positions, rotations, None))
        print(f"[Kimodo] Effector constraint '{effector_name}': {len(frames)} keyframes at frames "
              f"{sorted(frames)}.")

        # An effector pins two joints (wrist plus a fingertip, ankle plus a toe) and both positions arrive
        # from the retarget, not from this skeleton's own geometry. A span that disagrees with the neutral
        # span makes the pair unsatisfiable together, so the solve splits the difference and the primary
        # joint drifts off the position that was authored.
        primary_index = skeleton.bone_index[pos_joint_names[0]]
        secondary_index = skeleton.bone_index[pos_joint_names[-1]]
        if secondary_index != primary_index:
            neutral = skeleton.neutral_joints.detach().to(device=positions.device, dtype=positions.dtype)
            neutral_span = float(torch.linalg.vector_norm(neutral[primary_index] - neutral[secondary_index]))
            authored_span = torch.linalg.vector_norm(
                positions[:, primary_index, :] - positions[:, secondary_index, :], dim=-1
            )
            worst_mismatch = float((authored_span - neutral_span).abs().max()) * 100.0
            print(f"[Kimodo] Effector '{effector_name}' span {pos_joint_names[0]}->{pos_joint_names[-1]}: "
                  f"authored {float(authored_span.min()):.3f}-{float(authored_span.max()):.3f} m vs neutral "
                  f"{neutral_span:.3f} m, worst mismatch {worst_mismatch:.1f} cm.")

    return constraints


def _ensure_native_soma30_buffers(skeleton):
    """Register the stream buffers SOMASkeleton30 lacks on disk so UE export runs natively on the
    30 joint skeleton: standard T-pose offsets sliced from somaskel77 plus the matching BONES-SEED
    rest layout, each parent delta over the 30 joint chain rotated by the transposed parent offset.
    The UE soma_neutral asset is built from the exact same construction, so the stream matches it.
    """
    if not isinstance(skeleton, SOMASkeleton30):
        return
    if (getattr(skeleton, "bvh_neutral_joints", None) is not None
            and getattr(skeleton, "global_rot_offsets", None) is not None):
        return

    neutral_joints = skeleton.neutral_joints
    skel_slice = skeleton.get_skel_slice(skeleton.somaskel77)
    offsets_77 = getattr(skeleton.somaskel77, "global_rot_offsets", None)
    if offsets_77 is None:
        raise RuntimeError("The somaskel77 assets lack standard_t_pose_global_offsets_rots.p, "
                           "which the UE SOMA export needs.")
    offsets = offsets_77[skel_slice].to(device=neutral_joints.device, dtype=neutral_joints.dtype)

    bvh_neutral_joints = torch.zeros_like(neutral_joints)
    for joint_index, parent_index in enumerate(skeleton.joint_parents.tolist()):
        if parent_index < 0:
            bvh_neutral_joints[joint_index] = neutral_joints[joint_index]
            continue
        parent_delta = neutral_joints[joint_index] - neutral_joints[parent_index]
        bvh_neutral_joints[joint_index] = (
            bvh_neutral_joints[parent_index] + offsets[parent_index].T @ parent_delta
        )

    # Register only what is missing, so buffers loaded from a future asset file are kept as is.
    if getattr(skeleton, "global_rot_offsets", None) is None:
        skeleton.register_buffer("global_rot_offsets", offsets, persistent=False)
    if getattr(skeleton, "bvh_neutral_joints", None) is None:
        skeleton.register_buffer("bvh_neutral_joints", bvh_neutral_joints, persistent=False)


def _build_positions_array(output, skeleton, num_frames, sample_index, out_positions):
    """Fill out_positions, a (num_bones, num_frames, 3) view, from one batch sample. Root animates;
    non-root bones use bvh_neutral_joints parent-relative deltas so an identity-rotation key gives
    the BONES-SEED rest.
    """
    num_bones = len(skeleton.bone_order_names)
    root_index = int(skeleton.root_idx)

    root_positions = output["root_positions"][sample_index].detach().cpu().numpy().astype(np.float32)
    if root_positions.shape != (num_frames, 3):
        raise ValueError(f"Sample {sample_index} carries root positions of shape "
                         f"{root_positions.shape}, expected {(num_frames, 3)}.")
    bvh_neutral_joints = skeleton.bvh_neutral_joints.detach().cpu().numpy().astype(np.float32)
    joint_parents = skeleton.joint_parents.detach().cpu().numpy()

    # offset[i] = bvh_neutral_joints[i] minus bvh_neutral_joints[parent(i)].
    # For root or self-parented bones, substitute parent = self (zero offset).
    parents = joint_parents.astype(np.int64).copy()
    bone_indices = np.arange(num_bones, dtype=np.int64)
    self_loop_mask = (parents < 0) | (parents == bone_indices)
    parents[self_loop_mask] = bone_indices[self_loop_mask]
    local_offsets = bvh_neutral_joints - bvh_neutral_joints[parents]

    out_positions[:] = local_offsets[:, None, :]
    out_positions[root_index] = root_positions


def _build_rotations_array(output, skeleton, num_frames, sample_index, out_rotations):
    """Fill out_rotations, a (num_bones, num_frames, 4) xyzw quaternion view, from one batch sample.
    Model output is standard T-pose; rebase via from_standard_tpose() into BONES-SEED so it pairs
    with _build_positions_array.
    """
    local_rot_mats = output["local_rot_mats"][sample_index]  # (num_frames, num_bones, 3, 3), standard frame
    new_local_rot_mats, _ = skeleton.from_standard_tpose(local_rot_mats)
    quaternions_wxyz = matrix_to_quaternion(new_local_rot_mats)  # (num_frames, num_bones, 4)
    quaternions_xyzw = quaternions_wxyz[..., [1, 2, 3, 0]]
    quaternions_xyzw_np = quaternions_xyzw.detach().cpu().numpy().astype(np.float32)
    expected_shape = (num_frames, out_rotations.shape[0], 4)
    if quaternions_xyzw_np.shape != expected_shape:
        raise ValueError(f"Sample {sample_index} carries rotations of shape "
                         f"{quaternions_xyzw_np.shape}, expected {expected_shape}.")
    # (num_frames, num_bones, 4) -> (num_bones, num_frames, 4) to match the flat layout expected in C++.
    out_rotations[:] = np.transpose(quaternions_xyzw_np, (1, 0, 2))


def _split_root_bone_in_place(out_positions, out_rotations, skeleton):
    """Turn slot 0 of the output views into the synthetic floor-level 'root' bone the UE SOMA mesh
    expects above Hips, by splitting the pelvis into a floor projection and a vertical Hips offset.
    """
    # Slots 1.. already hold the skeleton's own bones, so the pelvis is one slot past its joint index.
    pelvis_slot = 1 + int(skeleton.root_idx)
    out_positions[0] = out_positions[pelvis_slot]
    out_positions[0, :, 1] = 0.0
    out_positions[pelvis_slot, :, 0] = 0.0
    out_positions[pelvis_slot, :, 2] = 0.0

    out_rotations[0] = 0.0
    out_rotations[0, :, 3] = 1.0


def _empty_foot_contacts():
    """Fresh, unshared payload for a generation with no usable foot contact data. A factory rather
    than a module constant, so no two results can alias the same lists and array.
    """
    return {
        "contact_point_names": [],
        "contact_flags": np.zeros(0, dtype=np.uint8),
        "contact_rear_indices": [],
        "contact_front_indices": [],
    }


@dataclass
class _FootContactLayout:
    """Emission order and pairing of the foot contact points, a pure property of the skeleton."""
    point_names: list          # list[str], ankle then ball per foot
    rear_pair_indices: list    # Index into point_names of each pair's rear point.
    front_pair_indices: list   # Index into point_names of each pair's front point.
    flag_columns: list         # The contact column (0 left, 1 right) that feeds each emitted point.
    num_feet: int


def _build_foot_contact_layout(skeleton):
    """Derive the foot contact point layout once per generation, since every variation shares it."""
    # Emit the ankle and ball of each foot so the viewport can orient the marker. The ankle is the
    # rear joint, the ball/toe the front. Left foot first, then right (drawing colors left red, right green).
    foot_joint_idx_lists = [list(skeleton.left_foot_joint_idx), list(skeleton.right_foot_joint_idx)]

    joint_indices = []
    flag_columns = []
    rear_pair_indices = []
    front_pair_indices = []
    for foot_column, foot_idx in enumerate(foot_joint_idx_lists):
        if len(foot_idx) == 0:
            continue
        rear_point_index = len(joint_indices)
        joint_indices.append(foot_idx[0])      # Ankle is the rear joint.
        flag_columns.append(foot_column)
        if len(foot_idx) > 1:
            front_point_index = len(joint_indices)
            joint_indices.append(foot_idx[1])  # Ball/toe is the front joint.
            flag_columns.append(foot_column)
            rear_pair_indices.append(rear_point_index)
            front_pair_indices.append(front_point_index)

    return _FootContactLayout(
        point_names=[str(skeleton.bone_order_names[joint_index]) for joint_index in joint_indices],
        rear_pair_indices=rear_pair_indices,
        front_pair_indices=front_pair_indices,
        flag_columns=flag_columns,
        num_feet=len(foot_joint_idx_lists),
    )


def _foot_contact_payload(layout):
    """Shared foot contact payload for a generation. The caller fills the flags per variation."""
    return {
        "contact_point_names": list(layout.point_names),
        "contact_flags": np.zeros(0, dtype=np.uint8),
        "contact_rear_indices": list(layout.rear_pair_indices),
        "contact_front_indices": list(layout.front_pair_indices),
    }


def _read_foot_contact_predictions(output):
    """Copy the model's predicted foot contacts to the host once for the whole batch, or None when
    the output carries none. One copy of the (B, T, C) tensor serves every variation.
    """
    contacts = output.get("foot_contacts", None)
    if contacts is None:
        print("[Kimodo] No foot_contacts in the model output; skipping foot contact debug data.")
        return None
    if hasattr(contacts, "detach"):
        contacts = contacts.detach().cpu().numpy()
    return np.asarray(contacts)


def _extract_foot_contacts(contacts, layout, num_frames, sample_index, contact_threshold):
    """Extract one batch sample's predicted per-frame foot contacts as dense frame-major flags, or
    None when the sample is unusable. Positions are not sent; C++ recomputes them via forward
    kinematics. contact_threshold is the user's planting threshold, so the markers match the pinning.
    """
    if contacts is None:
        return None
    if contacts.ndim == 3:
        # The batch dimension is untrusted model output like the frame count below, so an absent
        # sample degrades to empty contact data instead of aborting the whole generation.
        if sample_index >= contacts.shape[0]:
            print(f"[Kimodo] foot_contacts holds {contacts.shape[0]} samples but sample "
                  f"{sample_index} was requested; skipping foot contact debug data.")
            return None
        contacts = contacts[sample_index]
    elif sample_index > 0:
        # Without a batch dimension there is only one sample, so every variation shares its flags.
        print(f"[Kimodo] foot_contacts has no batch dimension; variation {sample_index} "
              f"reuses the single sample's flags.")
    # Keep only the toe channels so there is one planted flag per foot (left, right). The somaskel77
    # layout carries six labels with duplicated ToeEnd columns, the native SOMA layout four.
    if contacts.shape[-1] == 6:
        contacts = contacts[:, [1, 4]]
    elif contacts.shape[-1] == 4:
        contacts = contacts[:, [1, 3]]
    if contacts.shape[-1] != layout.num_feet or contacts.shape[0] < num_frames:
        print(f"[Kimodo] Unexpected foot_contacts shape {contacts.shape}; "
              "skipping foot contact debug data.")
        return None
    # The ankle and ball of a foot share its planted state, so expand the per-foot flags per point.
    # Frame-major [T, N] flattens to the frame * num_points + point layout C++ expects.
    foot_flags = contacts[:num_frames] > contact_threshold
    return foot_flags[:, layout.flag_columns].astype(np.uint8).ravel()


_GATED_TEXT_ENCODER_REPO = "meta-llama/Meta-Llama-3-8B-Instruct"

# The LLM2Vec adapter repos loaded on top of the gated base model.
_TEXT_ENCODER_ADAPTER_REPOS = (
    "McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp",
    "McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp-supervised",
)

# Error codes returned through the UAnimGenOfflinePoseGeneratorEditor pipeline (0 = success,
# anything else = plugin-defined error). Negative so they don't collide with future C++ codes.
KIMODO_GENERATE_SUCCESS = 0
KIMODO_GENERATE_GENERIC_FAILURE = -1
KIMODO_GENERATE_HF_TOKEN_MISSING = -100
KIMODO_GENERATE_HF_TOKEN_NO_ACCESS = -101

# Result code the worker maps to STATUS_CANCELLED, returned when cancel_cb fired mid-generation.
KIMODO_GENERATE_CANCELLED = 2


def _cached_file_path(repo_id, filename):
    """The local HF cache path of repo_id/filename, or None when it is not fully downloaded."""
    from huggingface_hub import try_to_load_from_cache
    path = try_to_load_from_cache(repo_id=repo_id, filename=filename)
    return path if isinstance(path, str) else None


def _base_weights_cached(repo_id):
    """True when the base model's safetensors weights (every shard listed in the index, or the
    single-file fallback) are fully present in the local HF cache.
    """
    index_path = _cached_file_path(repo_id, "model.safetensors.index.json")
    if index_path is None:
        return _cached_file_path(repo_id, "model.safetensors") is not None
    with open(index_path) as index_file:
        shard_names = set(json.load(index_file).get("weight_map", {}).values())
    if not shard_names:
        return False
    return all(_cached_file_path(repo_id, shard_name) is not None for shard_name in shard_names)


def _adapter_weights_cached(repo_id):
    """True when a PEFT adapter repo's weights are present in the local HF cache."""
    return (_cached_file_path(repo_id, "adapter_model.safetensors") is not None
            or _cached_file_path(repo_id, "adapter_model.bin") is not None)


def _needs_weight_download():
    """True when the upcoming model load will hit the network for the multi-gigabyte text encoder
    weights, rather than loading everything from the local HF cache. Checks the exact weight files
    the load requests, so a complete cache reports a plain load. Small sidecar files (tokenizer,
    configs) are ignored since fetching them is not the long wait the download stage warns about.
    """
    try:
        if not _base_weights_cached(_GATED_TEXT_ENCODER_REPO):
            return True
        for adapter_repo in _TEXT_ENCODER_ADAPTER_REPOS:
            if not _adapter_weights_cached(adapter_repo):
                return True
        return False
    except Exception as error:
        # Cache inspection failed, so fall back to the coarse size heuristic over the repo blobs.
        print(f"[Kimodo] Weight cache inspection failed ({error}); using the size heuristic.")
        return _text_encoder_cache_bytes() < _TEXT_ENCODER_EXPECTED_BYTES * 0.8


def _kimodo_checkpoint_cached():
    """True when the pinned Kimodo checkpoint repo looks fully cached, judged by a size heuristic
    against the known repo size. Display-only: the status tracker corrects from live signals anyway.
    """
    repo_dir_name = "models--" + _PINNED_KIMODO_REPO_ID.replace("/", "--")
    return _cache_repo_bytes((repo_dir_name,)) >= _KIMODO_CHECKPOINT_EXPECTED_BYTES * 0.8


def _initial_load_status_code():
    """The status to show before the load produces any live signals, from what the cache is missing.
    kimodo.load_model fetches its checkpoint before the text encoder, so a missing checkpoint wins:
    reporting the text encoder download up front would contradict the first phase the user watches.
    """
    if not _kimodo_checkpoint_cached():
        return KIMODO_STATUS_DOWNLOADING_MODEL_WEIGHTS
    if _needs_weight_download():
        return KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER
    return KIMODO_STATUS_LOADING_MODEL


def _check_huggingface_token():
    """Return (code, message); code == 0 means the token is good. Verifies a token is resolvable and
    has gated-repo access via auth_check. Non-auth probe failures are treated as success.
    """
    try:
        from huggingface_hub import HfApi, get_token
        from huggingface_hub.errors import GatedRepoError, RepositoryNotFoundError
    except ImportError:
        # huggingface_hub is a hard dependency. Let downstream code surface
        # the broken install rather than report it as a token problem.
        return KIMODO_GENERATE_SUCCESS, ""

    if not get_token():
        return KIMODO_GENERATE_HF_TOKEN_MISSING, (
            "**No Hugging Face token is configured.**\n\n"
            "Set one in **Editor Preferences -> Plugins -> Kimodo -> Hugging Face Token**, "
            "or run `hf auth login` to write it to ~/.cache/huggingface/token, "
            "or set the HF_TOKEN environment variable.\n\n"
            "Generate a read token at https://huggingface.co/settings/tokens."
        )

    # auth_check verifies access to the gated repo without downloading. Plain model_info() would
    # miss a "valid token but no granted access" case because the repo card is publicly visible.
    try:
        HfApi().auth_check(_GATED_TEXT_ENCODER_REPO)
    except (GatedRepoError, RepositoryNotFoundError) as error:
        print('[Kimodo]', str(error))
        return KIMODO_GENERATE_HF_TOKEN_NO_ACCESS, (
            f"**Your Hugging Face token does not have access to the gated text encoder repo.**\n\n"
            "**To fix this, do one of the following:**\n\n"
            "1. The token is wrong or expired. Update it in "
            "**Editor Preferences -> Plugins -> Kimodo -> Hugging Face Token**\n"
            "Generate a new read token at https://huggingface.co/settings/tokens.\n\n"
            f"2. The token is valid but your account has not been granted access to the gated model.\n"
            f"Request access at https://huggingface.co/{_GATED_TEXT_ENCODER_REPO} "
            f"while signed in with the same HF account, then retry once approval lands."
        )
    except Exception as error:
        print('[Kimodo]', str(error))
        # Probe failed for non-auth reasons (offline, DNS, server error). Don't block generation.
        # cached weights may still load, and if not the real load surfaces the actual problem.
        return KIMODO_GENERATE_SUCCESS, ""

    return KIMODO_GENERATE_SUCCESS, ""


def infer_only(input_dict, progress_cb=None, cancel_cb=None, status_code_cb=None,
               status_detail_cb=None):
    """Run one generation from a plain input dict and return the raw output arrays as a dict.
    Called by kimodo_worker.py. progress_cb receives 0..1, cancel_cb returning True aborts with
    result_code 2, status_code_cb receives KIMODO_STATUS values while a load, download, or device
    move runs, and status_detail_cb receives a short free form detail string for the current phase.
    On failure the dict carries result_code plus a user-facing error_message.

    The implementation runs in a nested call so that returning drops every device tensor it held as
    a local. empty_cache() only releases unreferenced blocks, so the trim has to happen after that.
    """
    try:
        return _infer_only_impl(input_dict, progress_cb=progress_cb, cancel_cb=cancel_cb,
                                status_code_cb=status_code_cb, status_detail_cb=status_detail_cb)
    finally:
        _trim_gpu_allocator()


def _infer_only_impl(input_dict, progress_cb=None, cancel_cb=None, status_code_cb=None,
                     status_detail_cb=None):
    """Body of infer_only. Every returned value is a numpy array or a plain Python object, so no
    CUDA tensor outlives this frame.
    """
    # Validate the HF token up front so a misconfigured token surfaces as a clear dialog in the
    # editor instead of a deep download failure inside the model load.
    token_code, token_message = _check_huggingface_token()
    if token_code != KIMODO_GENERATE_SUCCESS:
        return {"result_code": token_code, "error_message": token_message}

    inputs = _read_inputs(input_dict)

    print(f"[Kimodo] Description: '{inputs.description}'")
    print(f"[Kimodo] Duration: {inputs.duration_seconds}s, Seed: {inputs.random_seed}, "
          f"UseTrajectory: {inputs.use_trajectory}, PostProcessing: {inputs.post_processing}")

    # A load and the diffusion each own the full progress range. The editor bar completes at the
    # end of the load and restarts at zero for the generation stage its status announces.
    # _get_model drives the callback only when it actually loads, which includes a placement-change
    # reload this call site cannot see coming, and it reports the first load phase itself.
    load_progress_cb = progress_cb

    try:
        try:
            model, frame_rate, device = _get_model(progress_cb=load_progress_cb,
                status_code_cb=status_code_cb, status_detail_cb=status_detail_cb,
                park_text_encoder=inputs.park_text_encoder,
                text_encoder_precision=inputs.text_encoder_precision)
        except OSError as error:
            if getattr(error, "winerror", None) != 1314:
                raise
            # WinError 1314: huggingface_hub raced its symlink-support probe and tried a symlink without the privilege.
            # The verdict is now cached as unsupported, so one retry takes the copy fallback and reuses downloaded blobs.
            print("[Kimodo] Weight download hit the Windows symlink privilege error (WinError 1314); "
                  "retrying with the file copy fallback...", flush=True)
            model, frame_rate, device = _get_model(progress_cb=load_progress_cb,
                status_code_cb=status_code_cb, status_detail_cb=status_detail_cb,
                park_text_encoder=inputs.park_text_encoder,
                text_encoder_precision=inputs.text_encoder_precision)
    finally:
        # Clear the status on every outcome so a failed or cancelled load cannot leave a stale value.
        if status_code_cb is not None:
            status_code_cb(KIMODO_STATUS_NONE)
        if status_detail_cb is not None:
            status_detail_cb("")

    if cancel_cb is not None and cancel_cb():
        return {"result_code": KIMODO_GENERATE_CANCELLED, "error_message": ""}

    # Hand the bar to the generation stage at zero, so a completed load bar does not linger
    # through the constraint build and the description encode below.
    if progress_cb is not None:
        progress_cb(0.0)

    # Authoritative count from C++ (via FFrameRate's rational math). Avoids float32 drift on
    # non-30-aligned section rates. kimodo_worker.py rejects a non-positive count before this runs.
    num_frames = inputs.num_frames
    seed_everything(inputs.random_seed)

    constraint_list = []

    # The trajectory slot holds either a dense per-frame Root2D constraint from the spline
    # (use_trajectory) or sparse sets from Target Transform keys, split by their heading flag.
    # Downstream handling is identical over the list.
    trajectory_constraints = []
    if inputs.use_trajectory:
        dense_constraint = _build_trajectory_constraint(
            inputs, model.skeleton, num_frames, device)
        if dense_constraint is not None:
            trajectory_constraints.append(dense_constraint)
    else:
        trajectory_constraints = _build_target_transform_constraint(
            inputs, model.skeleton, num_frames, device)

    # Target poses anchor against the trajectory when present, or
    # self-anchor against their own sampled root / hips otherwise.
    scaffold = _build_target_pose_scaffold(inputs, model.skeleton, num_frames, device)
    target_pose_constraint = None
    effector_constraints = []
    if scaffold is not None:
        target_pose_constraint = _build_full_body_constraint(scaffold, model.skeleton, device)
        effector_constraints = _build_effector_constraints(inputs, scaffold, model.skeleton, device)

    pose_constraints = (
        [target_pose_constraint] if target_pose_constraint is not None else []
    )
    pose_constraints.extend(effector_constraints)
    first_heading_angle = _resolve_first_heading_angle(inputs, pose_constraints)
    if inputs.use_trajectory and trajectory_constraints and pose_constraints:
        trajectory_constraints[0] = _blend_trajectory_to_pose_constraints(
            trajectory_constraints[0], pose_constraints)
        first_heading = trajectory_constraints[0].global_root_heading[0]
        first_heading_angle = float(
            torch.atan2(first_heading[1], first_heading[0])
        )

    # Targets own overlapping roots, with dense paths approaching smoothly.
    # Sparse Target Transform values on other frames remain unchanged.
    constrained_frames = []
    if target_pose_constraint is not None:
        constrained_frames.append(target_pose_constraint.frame_indices)
    for effector_constraint in effector_constraints:
        constrained_frames.append(effector_constraint.frame_indices)

    if trajectory_constraints and constrained_frames:
        target_device = trajectory_constraints[0].frame_indices.device
        pose_frames = torch.unique(torch.cat([frames.to(target_device) for frames in constrained_frames]))
        kept_constraints = []
        for trajectory_constraint in trajectory_constraints:
            keep_mask = ~torch.isin(trajectory_constraint.frame_indices, pose_frames)
            num_dropped = int((~keep_mask).sum().item())
            if num_dropped == 0:
                kept_constraints.append(trajectory_constraint)
                continue
            num_kept = int(keep_mask.sum().item())
            if num_kept == 0:
                print(f"[Kimodo] All {num_dropped} trajectory keyframes overlap target pose keyframes; "
                      f"dropping this trajectory constraint entirely.")
                continue
            kept_heading = (
                trajectory_constraint.global_root_heading[keep_mask]
                if trajectory_constraint.global_root_heading is not None
                else None
            )
            kept_constraints.append(Root2DConstraintSet(
                model.skeleton,
                frame_indices=trajectory_constraint.frame_indices[keep_mask],
                smooth_root_2d=trajectory_constraint.smooth_root_2d[keep_mask],
                global_root_heading=kept_heading,
            ))
            print(f"[Kimodo] Dropped {num_dropped} trajectory keyframes overlapping target pose "
                  f"keyframes; {num_kept} trajectory keyframes remain.")
        trajectory_constraints = kept_constraints

    constraint_list.extend(trajectory_constraints)

    if target_pose_constraint is not None:
        constraint_list.append(target_pose_constraint)

    # After the full-body constraint: at a frame carrying both, the full-body joint positions are written
    # first and the effector's identical values dedup away, while its joint rotations still land because
    # FullBodyConstraintSet emits none.
    constraint_list.extend(effector_constraints)

    print(f"[Kimodo] Sampling {num_frames} frames at {frame_rate} fps...")
    if inputs.enable_cfg:
        cfg_type = "separated"
        cfg_weight = [inputs.cfg_text_weight, inputs.cfg_constraint_weight]
    else:
        # nocfg ignores cfg_weight, but pass the defaults to keep the call signature stable.
        cfg_type = "nocfg"
        cfg_weight = [inputs.cfg_text_weight, inputs.cfg_constraint_weight]

    print(f"[Kimodo] CFG: type={cfg_type}, text_weight={cfg_weight[0]}, "
          f"constraint_weight={cfg_weight[1]}")

    # Multi-prompt path: C++ fills the segment arrays when the channel has multiple keys and the
    # override is empty. Kimodo's _multiprompt blends segments. The output shape is unchanged.
    use_multi_prompt = len(inputs.segment_descriptions) > 1
    if use_multi_prompt:
        if len(inputs.segment_descriptions) != len(inputs.segment_num_frames):
            raise ValueError(
                f"Segment descriptions ({len(inputs.segment_descriptions)}) and segment frame "
                f"counts ({len(inputs.segment_num_frames)}) must be the same length.")
        if sum(inputs.segment_num_frames) != num_frames:
            raise ValueError(
                f"Segment frame counts sum to {sum(inputs.segment_num_frames)} but "
                f"NumFramesToGenerate is {num_frames}.")
        print(f"[Kimodo] Multi-prompt: {len(inputs.segment_descriptions)} segments, "
              f"frame counts={inputs.segment_num_frames}")
        segment_start_frame = 0
        for segment_index, segment_text in enumerate(inputs.segment_descriptions):
            segment_frame_count = inputs.segment_num_frames[segment_index]
            segment_end_frame = segment_start_frame + segment_frame_count - 1
            print(f"[Kimodo]   Segment {segment_index}: '{segment_text}' "
                  f"frames {segment_start_frame}-{segment_end_frame} "
                  f"({segment_frame_count} frames)")
            segment_start_frame += segment_frame_count

    # Variations batch through num_samples in one diffusion call, sharing the constraint list across the
    # batch. The multi-prompt path stitches its segment transitions per batch sample, so it batches too.
    num_variations = inputs.num_variations

    model_kwargs = {
        "num_denoising_steps": inputs.diffusion_steps,
        "num_samples": num_variations,
        "constraint_lst": constraint_list,
        "post_processing": inputs.post_processing,
        "root_margin": inputs.root_margin,
        "contact_threshold": inputs.foot_contact_threshold,
        "cfg_type": cfg_type,
        "cfg_weight": cfg_weight,
        "return_numpy": False,
    }

    # Initial heading must agree with the root, including frame zero targets.
    if first_heading_angle is not None:
        model_kwargs["first_heading_angle"] = first_heading_angle
        print(f"[Kimodo] Initial heading: {first_heading_angle:.3f} rad.")

    if use_multi_prompt:
        model_kwargs["num_transition_frames"] = inputs.num_transition_frames
        model_kwargs["percentage_transition_override"] = inputs.percentage_transition_override

    # One wrapper spans all segments so the reported progress is monotonic across multi-prompt
    # calls. It also polls cancellation between denoising steps.
    total_segments = len(inputs.segment_descriptions) if use_multi_prompt else 1
    model_kwargs["progress_bar"] = _WorkerDiffusionProgress(
        total_segments, inputs.diffusion_steps, progress_cb, cancel_cb, status_code_cb)

    text_encoder = getattr(model, "text_encoder", None)
    text_encoding_hook_handles = _install_text_encoding_progress_hooks(
        text_encoder, total_segments, progress_cb, cancel_cb)
    if status_code_cb is not None:
        status_code_cb(KIMODO_STATUS_ENCODING_TEXT)
    try:
        if use_multi_prompt:
            model_kwargs["multi_prompt"] = True
            output = model(inputs.segment_descriptions, inputs.segment_num_frames, **model_kwargs)
        else:
            output = model(inputs.description, num_frames, **model_kwargs)
    except _GenerationCancelled:
        print("[Kimodo] Generation cancelled by the editor.")
        return {"result_code": KIMODO_GENERATE_CANCELLED, "error_message": ""}
    finally:
        for hook_handle in text_encoding_hook_handles:
            hook_handle.remove()
        if status_code_cb is not None:
            status_code_cb(KIMODO_STATUS_NONE)

    # The vendored model expands SOMA output to somaskel77 for its external API. The UE asset is the
    # native 30 joint skeleton, so take the subset back out with the lib's own inverse. The model's
    # rotations sit unchanged in those slots, the other 47 joints only carry the constant relaxed
    # hands pose. _extract_foot_contacts handles the matching contact channel layouts itself.
    if input_dict.get("bvh_output"):
        from bvh_export_58 import save_motion_bvh
        skel = model.skeleton
        if isinstance(skel, SOMASkeleton30):
            skel = skel.somaskel77.to(device)
        save_motion_bvh(input_dict["bvh_output"], output["local_rot_mats"][0],
                        output["root_positions"][0], skeleton=skel, fps=frame_rate, standard_tpose=True)
        return {"result_code": KIMODO_GENERATE_SUCCESS}

    output_skeleton = model.skeleton
    if isinstance(output_skeleton, SOMASkeleton30):
        output["local_rot_mats"] = output_skeleton.from_SOMASkeleton77(output["local_rot_mats"])

    joint_names = ["root"] + list(output_skeleton.bone_order_names)
    num_bones = len(joint_names)

    # The batch dimensions are what the model actually produced, so bound the loop by the position
    # and rotation tensors it indexes per sample instead of failing the whole generation on the
    # requested count. Foot contacts guard their own batch dimension inside _extract_foot_contacts.
    returned_variations = min(int(output["root_positions"].shape[0]),
                              int(output["local_rot_mats"].shape[0]))
    if returned_variations == 0:
        return {"result_code": KIMODO_GENERATE_GENERIC_FAILURE,
                "error_message": "The model returned no samples."}
    if returned_variations < num_variations:
        print(f"[Kimodo] Model returned {returned_variations} of {num_variations} requested variations.")
        num_variations = returned_variations

    # Build every variation's bone-major arrays straight into one variation-major buffer. Slot 0 of
    # each variation is the synthetic floor-level 'root' bone UE's SOMA mesh expects above the pelvis.
    position_buffer = np.empty((num_variations, num_bones, num_frames, 3), dtype=np.float32)
    rotation_buffer = np.empty((num_variations, num_bones, num_frames, 4), dtype=np.float32)
    # Contact point names and foot pairs are skeleton properties every variation shares, and the
    # predicted flags copy off the device once for the whole batch, so both are read before the loop.
    foot_contact_layout = _build_foot_contact_layout(output_skeleton)
    foot_contact_predictions = _read_foot_contact_predictions(output)
    foot_contacts_meta = None
    contact_flag_buffer = None
    num_contact_flags = 0
    for variation_index in range(num_variations):
        # The FK rebuild and GPU to CPU copies here run well after the last denoising step, so poll
        # cancellation per variation instead of publishing a result the editor already gave up on.
        if cancel_cb is not None and cancel_cb():
            print("[Kimodo] Generation cancelled by the editor.")
            return {"result_code": KIMODO_GENERATE_CANCELLED, "error_message": ""}

        _build_positions_array(output, output_skeleton, num_frames, variation_index,
                               position_buffer[variation_index, 1:])
        _build_rotations_array(output, output_skeleton, num_frames, variation_index,
                               rotation_buffer[variation_index, 1:])
        _split_root_bone_in_place(position_buffer[variation_index],
                                  rotation_buffer[variation_index], output_skeleton)

        variation_flags = _extract_foot_contacts(foot_contact_predictions, foot_contact_layout,
                                                 num_frames, variation_index,
                                                 inputs.foot_contact_threshold)
        if foot_contacts_meta is None:
            # The first variation decides the shared payload: an unusable first sample publishes no
            # contact points at all, which the editor reads as "no foot contact debug data".
            foot_contacts_meta = (_empty_foot_contacts() if variation_flags is None
                                  else _foot_contact_payload(foot_contact_layout))
            num_contact_flags = num_frames * len(foot_contacts_meta["contact_point_names"])
            contact_flag_buffer = np.zeros((num_variations, num_contact_flags), dtype=np.uint8)

        # Every variation must contribute exactly the flag block the shared meta promises, otherwise
        # the variation-major packing shifts, so a missing or malformed sample keeps unplanted flags.
        variation_flag_count = 0 if variation_flags is None else variation_flags.size
        if variation_flag_count != num_contact_flags:
            if variation_flag_count > 0:
                print(f"[Kimodo] Variation {variation_index} produced {variation_flag_count} contact "
                      f"flags instead of {num_contact_flags}; substituting unplanted flags.")
            else:
                print(f"[Kimodo] Variation {variation_index} produced no contact flags; "
                      f"substituting unplanted flags.")
        elif num_contact_flags > 0:
            contact_flag_buffer[variation_index] = variation_flags

    # Variation-major float32 arrays flatten to consecutive per-variation blocks in the BoneIndex * NumFrames +
    # FrameIndex layout the C++ FinalizeFrameData phase expects. Contact point world positions are recomputed in C++ via FK.
    result = {
        "result_code": KIMODO_GENERATE_SUCCESS,
        "error_message": "",
        "num_frames": num_frames,
        "frame_rate": frame_rate,
        "num_variations": num_variations,
        "bone_names": joint_names,
        "position_values": position_buffer.ravel(),
        "rotation_values": rotation_buffer.ravel(),
    }
    result.update(foot_contacts_meta)
    result["contact_flags"] = contact_flag_buffer.ravel()

    if progress_cb is not None:
        progress_cb(1.0)
    print("[Kimodo] Generation complete.")
    return result


_TEXT_ONLY_ENCODER = None
_TEXT_ONLY_CONFIGURATION = None


def encode_text_only(descriptions, precision="", compute_device="cuda",
                     park_on_cpu=True, status_code_cb=None, progress_cb=None,
                     status_detail_cb=None, cancel_cb=None):
    """Encode prompts without constructing or loading a motion model."""
    global _TEXT_ONLY_ENCODER, _TEXT_ONLY_CONFIGURATION

    from kimodo.model.llm2vec.llm2vec_wrapper import LLM2VecEncoder

    precision = _resolve_text_encoder_precision(precision)
    requested_device = torch.device(compute_device)
    if requested_device.type == "cuda":
        if _is_cuda_usable(device_index=requested_device.index):
            device_index = (requested_device.index
                            if requested_device.index is not None
                            else _get_persistent_state().cuda_device_index)
            compute_device = f"cuda:{device_index}"
        else:
            compute_device = "cpu"
    else:
        compute_device = ("cpu" if requested_device.type == "cpu"
                          else str(requested_device))
    configuration = (precision, compute_device, bool(park_on_cpu),
                     _text_encoder_cache_root())
    if (_TEXT_ONLY_CONFIGURATION is not None
            and _TEXT_ONLY_CONFIGURATION != configuration
            and _TEXT_ONLY_CONFIGURATION[0] == precision
            and precision == TEXT_ENCODER_PRECISION_NF4
            and _TEXT_ONLY_CONFIGURATION[3] == configuration[3]
            and _reuse_nf4_text_encoder(
                _TEXT_ONLY_ENCODER, compute_device, park_on_cpu, progress_cb,
                status_code_cb, status_detail_cb)):
        _TEXT_ONLY_CONFIGURATION = configuration
    if _TEXT_ONLY_CONFIGURATION != configuration:
        if _TEXT_ONLY_ENCODER is not None:
            previous_device = _TEXT_ONLY_CONFIGURATION[1]
            _TEXT_ONLY_ENCODER = None
            _TEXT_ONLY_CONFIGURATION = None
            gc.collect()
            _trim_gpu_allocator(previous_device)
            if compute_device != previous_device:
                _trim_gpu_allocator(compute_device)
        _patch_llm2vec_encode_default_device()
        cache_dir, cache_key = _resolve_text_encoder_cache(precision, compute_device)
        _patch_llm2vec_loading(precision, compute_device, cache_dir, cache_key,
                              _text_encoder_cache_is_valid(cache_dir, cache_key))

        def load_encoder(load_state):
            dtype = (
                "float32" if compute_device == "cpu"
                and precision == TEXT_ENCODER_PRECISION_FULL else "bfloat16")
            encoder = LLM2VecEncoder(
                base_model_name_or_path="McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp",
                peft_model_name_or_path="McGill-NLP/LLM2Vec-Meta-Llama-3-8B-Instruct-mntp-supervised",
                dtype=dtype, llm_dim=4096,
                device="cpu" if park_on_cpu else compute_device)
            encoder = _CachedTextEncoder(
                encoder, compute_device,
                _resolve_text_encoding_cache_file(encoder, compute_device),
                park_on_cpu=park_on_cpu,
            )
            if compute_device != "cpu":
                _warm_up_text_encoder(types.SimpleNamespace(text_encoder=encoder), load_state)
            return encoder

        if progress_cb is None:
            _install_text_encoder_phase_hooks(None)
            encoder = load_encoder(None)
        else:
            state = _LoadState()
            state.signal_text_encoder()
            cache_baseline = _text_encoder_cache_bytes()
            needs_download = (not _text_encoder_cache_is_valid(cache_dir, cache_key)
                              and _needs_weight_download())
            initial_status = (KIMODO_STATUS_DOWNLOADING_TEXT_ENCODER if needs_download
                              else KIMODO_STATUS_LOADING_TEXT_ENCODER)
            status_tracker = _StatusTracker(initial_status)
            phase_hooks = _install_text_encoder_phase_hooks(state)
            weights_end_pct = (
                _TEXT_ENCODER_WEIGHTS_END_PCT
                if precision != TEXT_ENCODER_PRECISION_FULL
                and not _text_encoder_cache_is_valid(cache_dir, cache_key)
                else _TEXT_ENCODER_SAVE_END_PCT)
            progress_cb(0.0)
            if status_code_cb is not None:
                status_code_cb(initial_status)

            def load_worker():
                try:
                    tqdm_capture = _capture_tqdm_progress(state)
                    with tqdm_capture if tqdm_capture is not None else contextlib.nullcontext():
                        state.model = load_encoder(state)
                except BaseException as exception:
                    state.error = exception

            thread = threading.Thread(target=load_worker, name="KimodoTextEncoderLoader",
                                      daemon=True)
            thread.start()
            encoder = _report_model_load_progress(
                state, thread, phase_hooks, cache_baseline, needs_download,
                status_tracker, weights_end_pct, progress_cb, status_code_cb,
                status_detail_cb, text_encoder_only=True)
        _TEXT_ONLY_ENCODER = encoder
        _TEXT_ONLY_CONFIGURATION = configuration
    if progress_cb is not None:
        progress_cb(0.0)
    if status_code_cb is not None:
        status_code_cb(KIMODO_STATUS_ENCODING_TEXT)
    hook_handles = _install_text_encoding_progress_hooks(
        _TEXT_ONLY_ENCODER, len(descriptions), progress_cb, cancel_cb)
    embeddings = []
    try:
        for description in descriptions:
            if cancel_cb is not None and cancel_cb():
                raise InterruptedError("Text encoding canceled.")
            encoded, _lengths = _TEXT_ONLY_ENCODER([description])
            values = encoded.detach().to(device="cpu", dtype=torch.float32).reshape(-1)
            if values.numel() != 4096 or not torch.isfinite(values).all():
                raise RuntimeError("The text encoder returned invalid embeddings.")
            embeddings.append(values.tolist())
    except _GenerationCancelled as error:
        raise InterruptedError("Text encoding canceled.") from error
    finally:
        for hook_handle in hook_handles:
            hook_handle.remove()
    if progress_cb is not None:
        progress_cb(1.0)
    return embeddings
