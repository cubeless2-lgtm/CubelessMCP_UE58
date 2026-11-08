"""Build a local CHMR attention-fused ONNX; does not overwrite original assets.

Requires onnx==1.23.2 and numpy. Run with --source <original .uasset or .onnx>
--output <new .onnx>. Import the result using import_chmr_fused.py in this Editor.
The asset reader accepts only the exact source reviewed for UE 5.8.4. It is not
a general Unreal package parser. Model use remains subject to its original license.
"""
import argparse
import collections
import hashlib
import json
import mmap
import struct
from pathlib import Path

import numpy as np
import onnx
from onnx import helper, numpy_helper

ASSET_SHA256 = "97c4ff3da28fd84c98ae3db2d9ef0a36ab81fa96ede1a710af421d1887ec3ebf"
ONNX_SHA256 = "2c2dba8a17039d852c43c784e62dff3646ab907104306b74a585b123218d086e"


def read_source(path):
    with path.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        digest = hashlib.sha256(data).hexdigest()
        if path.suffix.lower() == ".uasset":
            if digest != ASSET_SHA256:
                raise ValueError("Unrecognized source asset. Review its serialization/model before converting.")
            marker = b"\x05\x00\x00\x00onnx\x00"
            offset = data.find(marker)
            if offset < 0:
                raise ValueError("NNE source data marker missing")
            count = struct.unpack_from("<q", data, offset + len(marker))[0]
            start = offset + len(marker) + 8
            if not 0 < count <= len(data) - start:
                raise ValueError("Invalid NNE source byte count")
            raw = data[start:start + count]
        else:
            raw = data[:]
        if hashlib.sha256(raw).hexdigest() != ONNX_SHA256:
            raise ValueError("Source ONNX differs from the reviewed CHMR backbone")
        return raw, digest


def fuse(model):
    by_name = {node.name: node for node in model.graph.node}
    weights_before = {item.name: hashlib.sha256(item.SerializeToString()).hexdigest()
                      for item in model.graph.initializer}
    precision_before = collections.Counter(item.data_type for item in model.graph.initializer)
    shape_name = "cbm_attention_flat_shape"
    model.graph.initializer.append(numpy_helper.from_array(
        np.array([1, 4097, 1024], dtype=np.int64), shape_name))
    remove, replacements = set(), {}
    for index in range(24):
        prefix = f"/backbone/blocks.{index}/attn/"
        expected = {"MatMul": "MatMul", "Softmax": "Softmax", "MatMul_1": "MatMul",
                    "Transpose_3": "Transpose", "Reshape_1": "Reshape", "Transpose_1": "Transpose"}
        for suffix, op in expected.items():
            if by_name[prefix + suffix].op_type != op:
                raise ValueError("Attention topology changed")
        # Preserve the original fp16 Q and K scaling operations, including their
        # rounding. No weight conversion, resolution change or frame sampling.
        nodes = [
            helper.make_node("Transpose", [prefix + "Mul_output_0"], [prefix + "cbm_q_bshd"],
                             perm=[0, 2, 1, 3], name=prefix + "cbm_q_transpose"),
            helper.make_node("Transpose", [prefix + "Mul_1_output_0"], [prefix + "cbm_k_bshd"],
                             perm=[0, 3, 1, 2], name=prefix + "cbm_k_transpose"),
        ]
        for label, source in [("q", prefix + "cbm_q_bshd"), ("k", prefix + "cbm_k_bshd"),
                              ("v", prefix + "Squeeze_2_output_0")]:
            nodes.append(helper.make_node("Reshape", [source, shape_name], [prefix + "cbm_" + label],
                                          name=prefix + "cbm_" + label + "_flat"))
        nodes.append(helper.make_node("MultiHeadAttention",
            [prefix + "cbm_q", prefix + "cbm_k", prefix + "cbm_v"],
            list(by_name[prefix + "Reshape_1"].output), domain="com.microsoft",
            num_heads=16, scale=1.0, name=prefix + "cbm_fused_attention"))
        replacements[prefix + "MatMul"] = nodes
        remove.update(prefix + suffix for suffix in expected)
    nodes = []
    for node in model.graph.node:
        nodes.extend(replacements.get(node.name, []))
        if node.name not in remove:
            nodes.append(node)
    del model.graph.node[:]
    model.graph.node.extend(nodes)
    if not any(item.domain == "com.microsoft" for item in model.opset_import):
        model.opset_import.append(helper.make_opsetid("com.microsoft", 1))
    del model.graph.value_info[:]  # discard stale intermediate shape metadata
    for item in model.graph.initializer:
        if item.name in weights_before:
            if hashlib.sha256(item.SerializeToString()).hexdigest() != weights_before[item.name]:
                raise ValueError("Original initializer changed")
    onnx.checker.check_model(model)
    return dict(precision_before)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = args.output.with_suffix(".provenance.json")
    if args.output.exists() or manifest.exists():
        raise FileExistsError("Destination exists; preserve it and choose a new output path")
    raw, source_hash = read_source(args.source)
    model = onnx.load_model_from_string(raw)
    del raw
    precision = fuse(model)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(model.SerializeToString())
    with args.output.open("rb") as stream:
        output_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    manifest.write_text(json.dumps({
        "algorithm": "cbm_chmr_fused_attention_v1", "source": str(args.source),
        "source_sha256": source_hash, "source_onnx_sha256": ONNX_SHA256,
        "output_sha256": output_hash, "onnx_version": onnx.__version__,
        "fused_blocks": 24, "original_initializers_unchanged": True,
        "original_initializer_dtype_counts": precision,
        "input_shape": [1, 3, 896, 896], "tokens": 4097, "heads": 16,
        "precision_note": "Original mixed precision retained; fused arithmetic may round differently.",
    }, indent=2), encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()
