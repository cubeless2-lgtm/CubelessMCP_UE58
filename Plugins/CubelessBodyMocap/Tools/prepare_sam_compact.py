"""Move the two exact SAM2 high-resolution projections before CPU transfer.
Requires onnx==1.23.2. Original weights, precision and dimensions of every
computed feature are preserved. No quantization, frame skipping or downsampling.
Source assets remain untouched; original model licensing continues to apply.
"""
import argparse,struct
EXPECTED={'sam2_image_encoder': {'asset_sha256': 'e6f71999fe2610992ecf6a9c26e0850666cc1e26782feeec2ff92fd3fc9cace0', 'onnx_sha256': 'ccb43999f375972b71f6fcb5ce96ad273d3e2cf22612e5e619075f608d60b120', 'output_sha256': '135d19a49718c99198da136ca50e3bc38aec118ba8cf4d7b75f808feb40fb80e'}, 'sam2_mask_decoder': {'asset_sha256': '2113753233e8327c71ff84d13e2f9e33dcb51efa769a0f7c3a9e87f0e5fea7e5', 'onnx_sha256': '5cbe98b5b38e3401a8e84d7fe98953a2269e6eb02a56b45a7c73e5f79260e5e0', 'output_sha256': '6169ef4652cb0aff0584394428f65210134e5c990c4b484145944f9867119e01'}}
import copy,onnx,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-dir',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args();b=a.output_dir
if any((b/(n+'_compact.onnx')).exists() for n in EXPECTED) or (b/'compact.provenance.json').exists():raise FileExistsError('Preserving existing outputs')
b.mkdir(parents=True,exist_ok=True)
models=[]
for name,expected in EXPECTED.items():
 raw=(a.source_dir/(name+'.uasset')).read_bytes()
 if hashlib.sha256(raw).hexdigest()!=expected['asset_sha256']:raise ValueError('Unreviewed source asset: '+name)
 marker=b'\x05\0\0\0onnx\0';offset=raw.index(marker)+len(marker);size=struct.unpack_from('<q',raw,offset)[0];raw=raw[offset+8:offset+8+size]
 if hashlib.sha256(raw).hexdigest()!=expected['onnx_sha256']:raise ValueError('Unreviewed ONNX: '+name)
 models.append(onnx.load_model_from_string(raw))
e,d=models
weights_before={v.name:v.SerializeToString() for m in models for v in m.graph.initializer}
init={v.name:v for v in d.graph.initializer};remove=set();outputs=[]
for i,(ch,side) in enumerate([(32,256),(64,128)]):
 name=f'/conv_s{i}/Conv';n=next(n for n in d.graph.node if n.name==name);assert n.op_type=='Conv';new=copy.deepcopy(n);new.name=f'cbm_projection_{i}';new.input[0]=f'fpn{i}';new.output[0]=f'cbm_fpn{i}';e.graph.node.append(new)
 for w in n.input[1:]:e.graph.initializer.append(init[w]);remove.add(w)
 old=n.output[0]
 for z in d.graph.node:
  for k,s in enumerate(z.input):
   if s==old:z.input[k]=f'high_res_feat{i}'
 nodes=[x for x in d.graph.node if x.name!=name];del d.graph.node[:];d.graph.node.extend(nodes)
 di=next(x for x in d.graph.input if x.name==f'high_res_feat{i}');di.type.tensor_type.shape.dim[1].dim_value=ch
 outputs.append(onnx.helper.make_tensor_value_info(f'cbm_fpn{i}',onnx.TensorProto.FLOAT,[1,ch,side,side]))
outputs.extend([onnx.helper.make_tensor_value_info(n,onnx.TensorProto.FLOAT,[1,256,64,64]) for n in ['fpn2','pos_enc2']]);del e.graph.output[:];e.graph.output.extend(outputs)
left=[x for x in d.graph.initializer if x.name not in remove];del d.graph.initializer[:];d.graph.initializer.extend(left)
for m,name in [(e,'sam2_image_encoder_compact'),(d,'sam2_mask_decoder_compact')]:
 del m.graph.value_info[:];onnx.checker.check_model(m);p=b/(name+'.onnx');assert not p.exists();onnx.save(m,p);print(name,p.stat().st_size,hashlib.sha256(p.read_bytes()).hexdigest())

weights_after={v.name:v.SerializeToString() for m in models for v in m.graph.initializer}
assert weights_before==weights_after, 'Weights must remain identical'
for n,v in EXPECTED.items():
 assert hashlib.sha256((b/(n+'_compact.onnx')).read_bytes()).hexdigest()==v['output_sha256']
(b/'compact.provenance.json').write_text(json.dumps({'algorithm':'cbm_sam2_compact_features_v1','models':EXPECTED,'weights_byte_identical':True,'encoder_output_bytes_before':176160768,'encoder_output_bytes_after':20971520,'onnx_version':onnx.__version__},indent=2),encoding='utf-8')
