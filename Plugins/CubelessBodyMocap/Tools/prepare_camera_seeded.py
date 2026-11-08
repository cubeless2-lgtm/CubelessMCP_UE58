"""Reproducibly seed the two camera-calibration random basis generators.
Weights, operations, dimensions and precision are unchanged. No source overwrite.
Requires onnx==1.23.2; original model license still applies.
"""
import argparse,hashlib,json,mmap,struct
from pathlib import Path
import onnx
ASSET_SHA256='9a54fd6f24f07f3df56625135ea951e1959c35b5bdef0ed1e12c05723a3ea7ac'
ONNX_SHA256='07ebbcf46d22e9ee47972892f6037f71c820a386cff4d1a96a3a48c2a8a01306'
SEEDS={f'/perspective_decoder/{head}/decoder/hamburger/ham/RandomUniformLike':float(i) for i,head in enumerate(['up_head','latitude_head'])}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();manifest=a.output.with_suffix('.provenance.json')
 if a.output.exists() or manifest.exists():raise FileExistsError('Choose a new output path; preserving existing files')
 raw=a.source.read_bytes();source_hash=hashlib.sha256(raw).hexdigest()
 if a.source.suffix.lower()=='.uasset':
  if source_hash!=ASSET_SHA256:raise ValueError('Unreviewed source asset')
  marker=b'\x05\0\0\0onnx\0';offset=raw.index(marker)+len(marker);size=struct.unpack_from('<q',raw,offset)[0];raw=raw[offset+8:offset+8+size]
 if hashlib.sha256(raw).hexdigest()!=ONNX_SHA256:raise ValueError('Unreviewed ONNX model')
 m=onnx.load_model_from_string(raw);before=m.SerializeToString();found=[]
 for n in m.graph.node:
  if n.op_type.startswith('Random'):
   if n.name not in SEEDS or n.op_type!='RandomUniformLike' or any(x.name=='seed' for x in n.attribute):raise ValueError('Unexpected random operator')
   n.attribute.append(onnx.helper.make_attribute('seed',SEEDS[n.name]));found.append(n.name)
 if set(found)!=set(SEEDS):raise ValueError('Missing expected random operators')
 onnx.checker.check_model(m);result=m.SerializeToString()
 # Removing exactly the two new seed attributes must restore the complete original graph.
 for n in m.graph.node:
  if n.name in SEEDS:del n.attribute[-1]
 if m.SerializeToString()!=before:raise ValueError('Model changed beyond seed attributes')
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(result)
 manifest.write_text(json.dumps({'algorithm':'cbm_camera_seeded_v1','source_sha256':source_hash,'source_onnx_sha256':ONNX_SHA256,'output_sha256':hashlib.sha256(result).hexdigest(),'seeds':SEEDS,'only_seed_attributes_changed':True,'onnx_version':onnx.__version__},indent=2),encoding='utf-8');print(a.output)
if __name__=='__main__':main()
