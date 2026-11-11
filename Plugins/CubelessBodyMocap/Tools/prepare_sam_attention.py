"""Prepare FP32 SAM2 attention variants without dropping keys or changing weights.

Requires onnx==1.23.2. Keeps FP32, 4096 queries, every spatial memory frame and
all object pointers. Original source assets are never modified. The candidate
must pass numerical and in-Editor tracking comparisons before production use.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct

import onnx
from onnx import TensorProto, helper

ASSET_SHA256 = "16981f356c824becb67ca2878a6747ac62ebf8c5e84dac0c74a1a536972803c9"
ONNX_SHA256 = "eaf1fe0f801a046378ed86fedc1ccc939050c737eadf8929fa5851859dea2d7d"


def read_source(path: Path) -> bytes:
    raw = path.read_bytes()
    if path.suffix.lower() == '.uasset':
        if hashlib.sha256(raw).hexdigest() != ASSET_SHA256:
            raise ValueError('Unrecognized source asset; review provenance before conversion')
        marker = b'\x05\0\0\0onnx\0'
        offset = raw.index(marker) + len(marker)
        size = struct.unpack_from('<q', raw, offset)[0]
        raw = raw[offset+8:offset+8+size]
    if hashlib.sha256(raw).hexdigest() != ONNX_SHA256:
        raise ValueError('Unrecognized source ONNX; original weights/graph required')
    return raw


def transform(model: onnx.ModelProto, rows: int) -> tuple[onnx.ModelProto, list[str]]:
    if rows not in (256, 512, 1024, 2048):
        raise ValueError('Query rows must be 256, 512, 1024 or 2048')
    model = copy.deepcopy(model)
    original_weights = {v.name: v.SerializeToString() for v in model.graph.initializer}
    original_inputs = [v.SerializeToString() for v in model.graph.input]
    original_outputs = [v.SerializeToString() for v in model.graph.output]
    nodes = list(model.graph.node)
    producers = {v: n for n in nodes for v in n.output}
    users = {}
    for node in nodes:
        for value in node.input:
            users.setdefault(value, []).append(node)
    replacements, removed, sites = {}, set(), []
    for softmax in nodes:
        if softmax.op_type != 'Softmax':
            continue
        axes = {a.name: helper.get_attribute_value(a) for a in softmax.attribute}
        if axes.get('axis', -1) != -1:
            raise ValueError('Attention softmax must retain the complete key axis')
        score = producers[softmax.input[0]]
        consumers = users.get(softmax.output[0], [])
        if score.op_type != 'MatMul' or len(consumers) != 1 or consumers[0].op_type != 'MatMul':
            raise ValueError('Unexpected attention graph structure')
        result = consumers[0]
        if len(users.get(score.output[0], [])) != 1 or result.input[0] != softmax.output[0]:
            raise ValueError('Shared attention score/probability output is unsupported')
        prefix = 'cbm_rows_' + str(len(sites))
        generated, outputs = [], []
        for first in range(0, 4096, rows):
            tag = f'{prefix}_{first}'
            for suffix, value in [('start', first), ('end', first+rows), ('axis', -2), ('step', 1)]:
                generated.append(helper.make_node('Constant', [], [tag+'_'+suffix],
                    name=tag+'_'+suffix, value=helper.make_tensor(tag+'_'+suffix, TensorProto.INT64, [1], [value])))
            generated.append(helper.make_node('Slice', [score.input[0], tag+'_start', tag+'_end', tag+'_axis', tag+'_step'], [tag+'_q'], name=tag+'_slice'))
            generated.append(helper.make_node('MatMul', [tag+'_q', score.input[1]], [tag+'_score'], name=tag+'_score'))
            generated.append(helper.make_node('Softmax', [tag+'_score'], [tag+'_prob'], name=tag+'_softmax', axis=-1))
            generated.append(helper.make_node('MatMul', [tag+'_prob', result.input[1]], [tag+'_out'], name=tag+'_out'))
            outputs.append(tag+'_out')
        generated.append(helper.make_node('Concat', outputs, list(result.output), name=prefix+'_concat', axis=-2))
        replacements[result.name] = generated
        removed.update([score.name, softmax.name])
        sites.append(softmax.name)
    if len(sites) != 8 or len(replacements) != 8:
        raise ValueError(f'Expected four self/cross attention pairs, found {len(sites)}')
    updated = []
    for node in nodes:
        if node.name in removed:
            continue
        updated.extend(replacements.get(node.name, [node]))
    del model.graph.node[:]
    model.graph.node.extend(updated)
    del model.graph.value_info[:]
    assert original_weights == {v.name:v.SerializeToString() for v in model.graph.initializer}
    assert original_inputs == [v.SerializeToString() for v in model.graph.input]
    assert original_outputs == [v.SerializeToString() for v in model.graph.output]
    onnx.checker.check_model(model)
    return model, sites


def fuse(model: onnx.ModelProto) -> tuple[onnx.ModelProto, list[str]]:
    """Use the runtime attention kernel, retaining the existing Q/K scaling."""
    model = copy.deepcopy(model)
    weights = {v.name:v.SerializeToString() for v in model.graph.initializer}
    nodes = list(model.graph.node)
    producers = {v:n for n in nodes for v in n.output}
    users = {}
    for node in nodes:
        for value in node.input:
            users.setdefault(value, []).append(node)
    removed, replacements, sites = set(), {}, []
    for softmax in nodes:
        if softmax.op_type != 'Softmax':
            continue
        axes = {a.name:helper.get_attribute_value(a) for a in softmax.attribute}
        score = producers[softmax.input[0]]
        consumers = users.get(softmax.output[0], [])
        if axes.get('axis', -1) != -1 or score.op_type != 'MatMul' or len(consumers) != 1 or consumers[0].op_type != 'MatMul':
            raise ValueError('Unexpected attention topology')
        result = consumers[0]
        if len(users.get(score.output[0], [])) != 1 or result.input[0] != softmax.output[0]:
            raise ValueError('Shared attention intermediate is unsupported')
        prefix = 'cbm_fused_' + str(len(sites))
        replacements[result.name] = [
            helper.make_node('Constant', [], [prefix+'_axis'], name=prefix+'_axis', value=helper.make_tensor(prefix+'_axis', TensorProto.INT64, [1], [1])),
            helper.make_node('Squeeze', [score.input[0], prefix+'_axis'], [prefix+'_q'], name=prefix+'_q'),
            helper.make_node('Transpose', [score.input[1]], [prefix+'_kt'], name=prefix+'_kt', perm=[0,1,3,2]),
            helper.make_node('Squeeze', [prefix+'_kt', prefix+'_axis'], [prefix+'_k'], name=prefix+'_k'),
            helper.make_node('Squeeze', [result.input[1], prefix+'_axis'], [prefix+'_v'], name=prefix+'_v'),
            helper.make_node('MultiHeadAttention', [prefix+'_q', prefix+'_k', prefix+'_v'], [prefix+'_result'], domain='com.microsoft', num_heads=1, scale=1.0, name=prefix+'_attention'),
            helper.make_node('Unsqueeze', [prefix+'_result', prefix+'_axis'], list(result.output), name=prefix+'_restore'),
        ]
        removed.update([score.name, softmax.name])
        sites.append(softmax.name)
    if len(sites) != 8:
        raise ValueError('Expected four original self/cross attention pairs')
    updated = []
    for node in nodes:
        if node.name not in removed:
            updated.extend(replacements.get(node.name, [node]))
    del model.graph.node[:]
    model.graph.node.extend(updated)
    del model.graph.value_info[:]
    if not any(item.domain == 'com.microsoft' for item in model.opset_import):
        model.opset_import.append(helper.make_opsetid('com.microsoft', 1))
    assert weights == {v.name:v.SerializeToString() for v in model.graph.initializer}
    onnx.checker.check_model(model)
    return model, sites


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('rows', 'fused'), default='rows')
    parser.add_argument('--query-rows', type=int, default=512)
    args = parser.parse_args()
    provenance = args.output.with_suffix('.provenance.json')
    if args.output.exists() or provenance.exists():
        raise FileExistsError('Preserving existing model/provenance')
    raw = read_source(args.source)
    source = onnx.load_model_from_string(raw)
    model, sites = fuse(source) if args.mode == 'fused' else transform(source, args.query_rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    data = model.SerializeToString()
    args.output.write_bytes(data)
    report = {'version':'sam2_fused_attention_v1' if args.mode == 'fused' else 'sam2_query_rows_v1','source_asset_sha256':ASSET_SHA256,
              'source_onnx_sha256':ONNX_SHA256,'output_sha256':hashlib.sha256(data).hexdigest(),
              'weights_byte_identical':True,'dtype':'float32','queries':4096,'mode':args.mode,'query_rows':args.query_rows if args.mode == 'rows' else None,
              'key_and_value_tokens_unchanged':True,'attention_sites':sites,
              'largest_score_bytes_before':4096*28736*4,
              'largest_score_bytes_per_block':args.query_rows*28736*4 if args.mode == 'rows' else None,
              'note':'Per-block tensor bound; actual runtime peak/speed require GPU measurement.',
              'onnx_version':onnx.__version__}
    provenance.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
