# -*- coding: utf-8 -*-
"""
Copyright Epic Games, Inc. All Rights Reserved.

Pure PyTorch NF4 weight-only quantization for the Kimodo text encoder, replacing bitsandbytes.
NF4 stores each weight as a 4 bit index into a 16 level codebook whose levels sit at the quantiles
of a standard normal distribution, scaled per block of 64 weights by that block's absolute maximum.
The forward pass dequantizes one layer at a time and runs a plain F.linear, so it works on every
device PyTorch supports and needs no compiled kernels.

The codec was validated against bitsandbytes NF4 when it replaced that dependency: packed codes
and scales round-trip in agreement on synthetic and Llama-shaped tensors, and the encoded
embeddings of the full text encoder match between the two implementations.
"""

import json
import os

import torch
import torch.nn.functional as F
from torch import nn

# Format version written into the cache metadata, bumped whenever the packed layout changes.
KIMODO_NF4_FORMAT_VERSION = 2

# Weights per absmax block. 64 matches the bitsandbytes default the plugin shipped with, so the
# quantization error stays the same size.
NF4_BLOCK_SIZE = 64

# The 16 NF4 levels from the QLoRA paper, the same table bitsandbytes hardcodes: normal-distribution
# quantiles normalized to [-1, 1], asymmetric so that exact zero is representable.
NF4_CODEBOOK = torch.tensor([
    -1.0, -0.6961928009986877, -0.5250730514526367, -0.39491748809814453,
    -0.28444138169288635, -0.18477343022823334, -0.09105003625154495, 0.0,
    0.07958029955625534, 0.16093020141124725, 0.24611230194568634, 0.33791524171829224,
    0.44070982933044434, 0.5626170039176941, 0.7229568362236023, 1.0,
], dtype=torch.float32)

# Midpoints between adjacent levels. Snapping with bucketize(right=False) sends a value exactly on a
# midpoint to the lower level, matching the strict greater-than tree bitsandbytes quantizes with.
_NF4_BOUNDARIES = (NF4_CODEBOOK[:-1] + NF4_CODEBOOK[1:]) / 2.0

# Per (device, dtype) cache of a 256 entry byte-to-level-pair table, so dequantization indexes once
# per packed byte, and a bf16 dequantize stays in bf16 instead of staging the weight in fp32 first.
_PAIR_LUT_CACHE = {}


def _pair_lut(device, dtype):
    lut = _PAIR_LUT_CACHE.get((device, dtype))
    if lut is None:
        byte_values = torch.arange(256, dtype=torch.long)
        codebook = NF4_CODEBOOK.to(device=device, dtype=dtype)
        lut = torch.stack((codebook[(byte_values >> 4).to(device)],
                           codebook[(byte_values & 0x0F).to(device)]), dim=1)
        _PAIR_LUT_CACHE[(device, dtype)] = lut
    return lut


def quantize_nf4(weight):
    """Quantize a 2D weight to (packed uint8 of numel/2, absmax fp32 of numel/64). The element
    count must be a multiple of twice the block size, since two weights pack into each byte, which
    every Llama linear satisfies.
    """
    numel = weight.numel()
    if numel % (NF4_BLOCK_SIZE * 2) != 0:
        raise ValueError(f"Weight of {numel} elements is not a multiple of "
                         f"{NF4_BLOCK_SIZE * 2}, so it cannot be NF4 packed.")
    blocks = weight.detach().to(torch.float32).reshape(-1, NF4_BLOCK_SIZE)
    absmax = blocks.abs().amax(dim=1)
    # Multiply by the reciprocal rather than divide, matching how bitsandbytes normalizes, so the
    # rounding at level boundaries agrees. An all-zero block keeps absmax 0 and maps to the 0 level.
    inverse_scale = torch.where(absmax > 0, 1.0 / absmax, torch.zeros_like(absmax))
    normalized = blocks * inverse_scale.unsqueeze(1)
    codes = torch.bucketize(normalized, _NF4_BOUNDARIES.to(weight.device), right=False)
    codes = codes.to(torch.uint8).reshape(-1)
    packed = (codes[0::2] << 4) | codes[1::2]
    return packed, absmax


def dequantize_nf4(packed, absmax, shape, dtype):
    """Expand packed NF4 back to a dense tensor of the given shape and dtype. The level table and the
    block scales are cast to dtype first, so no fp32 copy of the whole weight is ever materialized.
    """
    level_pairs = _pair_lut(packed.device, dtype)[packed.long()]
    weight = level_pairs.reshape(-1, NF4_BLOCK_SIZE) * absmax.to(dtype).unsqueeze(1)
    return weight.reshape(shape)


class NF4Linear(nn.Module):
    """Weight-only NF4 linear layer. The packed weight and absmax are registered buffers, so .to()
    moves them and state_dict round-trips them. Forward dequantizes into compute_dtype per call.
    """

    def __init__(self, in_features, out_features, has_bias, compute_dtype, device=None):
        super().__init__()
        self.in_features = int(in_features)
        self.out_features = int(out_features)
        self.compute_dtype = compute_dtype
        numel = self.in_features * self.out_features
        self.register_buffer("packed_weight",
                             torch.empty(numel // 2, dtype=torch.uint8, device=device))
        self.register_buffer("absmax",
                             torch.empty(numel // NF4_BLOCK_SIZE, dtype=torch.float32, device=device))
        if has_bias:
            self.register_buffer("bias", torch.empty(self.out_features, dtype=compute_dtype,
                                                     device=device))
        else:
            self.register_buffer("bias", None)

    @classmethod
    def from_linear(cls, linear, compute_dtype):
        quantized = cls(linear.in_features, linear.out_features, linear.bias is not None,
                        compute_dtype, device=linear.weight.device)
        packed, absmax = quantize_nf4(linear.weight)
        quantized.packed_weight = packed
        quantized.absmax = absmax
        if linear.bias is not None:
            quantized.bias = linear.bias.detach().to(compute_dtype)
        return quantized

    def forward(self, hidden_states):
        weight = dequantize_nf4(self.packed_weight, self.absmax,
                                (self.out_features, self.in_features), self.compute_dtype)
        bias = self.bias.to(self.compute_dtype) if self.bias is not None else None
        output = F.linear(hidden_states.to(self.compute_dtype), weight, bias)
        if output.dtype != hidden_states.dtype:
            output = output.to(hidden_states.dtype)
        return output

    def extra_repr(self):
        return (f"in_features={self.in_features}, out_features={self.out_features}, "
                f"compute_dtype={self.compute_dtype}")


def _named_linear_modules(model):
    return [(name, module) for name, module in model.named_modules()
            if type(module) is nn.Linear]


def quantize_model_nf4(model, compute_dtype, progress_cb=None):
    """Replace every plain nn.Linear under model with an NF4Linear, in place, one layer at a time
    so the fp32 staging buffer never exceeds a single layer. Returns the replaced module names.
    progress_cb, when given, receives the completed fraction after each layer.
    """
    # Snapshot only the names, not the modules, so each original Linear loses its last reference at
    # the setattr and frees right away, instead of the full precision model surviving the whole pass.
    quantized_names = [name for name, _ in _named_linear_modules(model)]
    for module_index, name in enumerate(quantized_names):
        parent_name, _, attribute_name = name.rpartition(".")
        parent = model.get_submodule(parent_name) if parent_name else model
        setattr(parent, attribute_name,
                NF4Linear.from_linear(getattr(parent, attribute_name), compute_dtype))
        if progress_cb is not None:
            progress_cb((module_index + 1) / len(quantized_names))
    return quantized_names


# File names inside a cache directory. The weights hold the full quantized state_dict split into
# shards, the metadata records what is needed to rebuild the module tree and lists the shards.
# Sharding exists for progress reporting: save_file materializes a file at full size in one step,
# so only completed shards measure how far along a multi-gigabyte write or read actually is.
NF4_WEIGHTS_SHARD_PATTERN = "kimodo_nf4_model-{0:05d}.safetensors"
NF4_WEIGHTS_SHARD_TARGET_BYTES = 256 * 1024 ** 2
NF4_METADATA_FILE = "kimodo_nf4_quant.json"
LLM2VEC_CONFIG_FILE = "llm2vec_config.json"

_COMPUTE_DTYPE_NAMES = {torch.float32: "float32", torch.bfloat16: "bfloat16",
                        torch.float16: "float16"}
_COMPUTE_DTYPES_BY_NAME = {name: dtype for dtype, name in _COMPUTE_DTYPE_NAMES.items()}


def save_nf4_llm2vec(llm2vec_model, directory, quantized_names, compute_dtype, progress_cb=None):
    """Write the quantized inner model, its config, and the LLM2Vec pooling settings to directory.
    The tokenizer and the manifest are written by the caller's finalize step. progress_cb, when
    given, receives the completed byte fraction after each weight shard lands on disk.
    """
    from safetensors.torch import save_file
    os.makedirs(directory, exist_ok=True)
    inner_model = llm2vec_model.model
    inner_model.config.save_pretrained(directory)
    state = {key: value.contiguous() for key, value in inner_model.state_dict().items()}

    total_bytes = sum(value.numel() * value.element_size() for value in state.values())
    shard_names = []
    shard = {}
    shard_bytes = 0
    written_bytes = 0

    def _flush_shard():
        nonlocal shard, shard_bytes, written_bytes
        if not shard:
            return
        shard_name = NF4_WEIGHTS_SHARD_PATTERN.format(len(shard_names))
        save_file(shard, os.path.join(directory, shard_name))
        shard_names.append(shard_name)
        written_bytes += shard_bytes
        shard = {}
        shard_bytes = 0
        if progress_cb is not None and total_bytes > 0:
            progress_cb(written_bytes / total_bytes)

    for key, value in state.items():
        shard[key] = value
        shard_bytes += value.numel() * value.element_size()
        if shard_bytes >= NF4_WEIGHTS_SHARD_TARGET_BYTES:
            _flush_shard()
    _flush_shard()

    metadata = {
        "format_version": KIMODO_NF4_FORMAT_VERSION,
        "block_size": NF4_BLOCK_SIZE,
        "compute_dtype": _COMPUTE_DTYPE_NAMES[compute_dtype],
        "quantized_modules": sorted(quantized_names),
        "weight_shards": shard_names,
    }
    with open(os.path.join(directory, NF4_METADATA_FILE), "w") as metadata_file:
        json.dump(metadata, metadata_file, indent=2)

    llm2vec_settings = {
        "pooling_mode": llm2vec_model.pooling_mode,
        "max_length": llm2vec_model.max_length,
        "doc_max_length": llm2vec_model.doc_max_length,
        "skip_instruction": llm2vec_model.skip_instruction,
    }
    with open(os.path.join(directory, LLM2VEC_CONFIG_FILE), "w") as settings_file:
        json.dump(llm2vec_settings, settings_file, indent=2)


def _rebuild_meta_rotary_modules(model):
    """Reconstruct rotary embedding modules after an assign-load. Their inv_freq buffers are
    non-persistent, so they are absent from the saved state and would otherwise stay meta tensors.
    Rebuilding through the module's own constructor recomputes every derived value from the config,
    exactly as a from_pretrained load does, across transformers versions.
    """
    rotary_modules = [(name, module) for name, module in model.named_modules()
                      if isinstance(getattr(module, "inv_freq", None), torch.Tensor)
                      and module.inv_freq.is_meta]
    for name, module in rotary_modules:
        parent_name, _, attribute_name = name.rpartition(".")
        parent = model.get_submodule(parent_name) if parent_name else model
        setattr(parent, attribute_name, type(module)(config=model.config, device=None))


def set_nf4_compute_dtype(model, compute_dtype):
    """Set arithmetic precision without changing packed weights or scales."""
    for module in model.modules():
        if type(module).__name__ == NF4Linear.__name__:
            module.compute_dtype = compute_dtype


def load_nf4_llm2vec(llm2vec_class, directory, progress_cb=None,
                   compute_dtype=None, storage_dtype=None):
    """Rebuild a quantized LLM2Vec from a cache directory written by save_nf4_llm2vec plus the
    finalize step. Raises on any inconsistency, and the caller treats that as a cache miss.
    progress_cb, when given, receives the completed fraction as the weight shards read in.
    """
    from safetensors import safe_open
    from transformers import AutoConfig, AutoTokenizer

    with open(os.path.join(directory, NF4_METADATA_FILE)) as metadata_file:
        metadata = json.load(metadata_file)
    if metadata.get("format_version") != KIMODO_NF4_FORMAT_VERSION:
        raise ValueError(f"NF4 cache format {metadata.get('format_version')} does not match "
                         f"{KIMODO_NF4_FORMAT_VERSION}.")
    if metadata.get("block_size") != NF4_BLOCK_SIZE:
        raise ValueError("NF4 cache block size does not match this quantizer.")
    cached_compute_dtype = _COMPUTE_DTYPES_BY_NAME[metadata["compute_dtype"]]
    if compute_dtype is None:
        compute_dtype = cached_compute_dtype

    config = AutoConfig.from_pretrained(directory)
    model_class = llm2vec_class._get_model_class(type(config).__name__, enable_bidirectional=True)
    # Meta construction allocates nothing. Every real tensor arrives through the assign-load below,
    # so anything the state is missing stays meta and fails the verification loudly.
    with torch.device("meta"):
        model = model_class(config)

    quantized_names = set(metadata.get("quantized_modules", ()))
    for name, linear in _named_linear_modules(model):
        if name not in quantized_names:
            raise ValueError(f"Linear module '{name}' is not in the cached quantized set.")
        parent_name, _, attribute_name = name.rpartition(".")
        parent = model.get_submodule(parent_name) if parent_name else model
        setattr(parent, attribute_name,
                NF4Linear(linear.in_features, linear.out_features, linear.bias is not None,
                          compute_dtype, device="meta"))

    # Read shard by shard rather than in one load_file call, so the dominant stretch of a cached
    # load reports real progress. The shards are near equal in bytes, so their count paces evenly.
    shard_names = metadata.get("weight_shards", ())
    if not shard_names:
        raise ValueError("NF4 cache lists no weight shards.")
    state = {}
    for shard_index, shard_name in enumerate(shard_names):
        with safe_open(os.path.join(directory, shard_name), framework="pt",
                       device="cpu") as shard_file:
            for tensor_key in shard_file.keys():
                state[tensor_key] = shard_file.get_tensor(tensor_key)
        if progress_cb is not None:
            progress_cb((shard_index + 1) / len(shard_names))
    model.load_state_dict(state, strict=True, assign=True)
    set_nf4_compute_dtype(model, compute_dtype)
    if storage_dtype is not None:
        for parameter in model.parameters():
            if parameter.is_floating_point():
                parameter.data = parameter.data.to(dtype=storage_dtype)
    _rebuild_meta_rotary_modules(model)

    remaining_meta = [name for name, tensor in
                      list(model.named_parameters()) + list(model.named_buffers())
                      if tensor is not None and tensor.is_meta]
    if remaining_meta:
        raise ValueError(f"NF4 cache left tensors unmaterialized: {remaining_meta[:8]}")

    # AutoConfig.from_pretrained rewrote _name_or_path to the directory, and prepare_for_tokenization
    # keys the Llama 3 chat header off the original value, so restore it from the raw config.json.
    with open(os.path.join(directory, "config.json")) as config_file:
        saved_name_or_path = json.load(config_file).get("_name_or_path", "")
    model.config._name_or_path = saved_name_or_path
    model.eval()

    tokenizer = AutoTokenizer.from_pretrained(directory)
    tokenizer.pad_token = tokenizer.eos_token
    tokenizer.padding_side = "left"

    with open(os.path.join(directory, LLM2VEC_CONFIG_FILE)) as settings_file:
        llm2vec_settings = json.load(settings_file)
    return llm2vec_class(model=model, tokenizer=tokenizer, **llm2vec_settings)
