"""Original-video whole-body observations using the installed ViTPose weights.

No downloads, hidden estimator replacement or prediction cache. Run `extract`
in an image-capable Python; --inference-python may name a separate local ORT
environment. The raw image timestamps and crop transform accompany every key.
"""
import argparse
import hashlib
import json
import mmap
import struct
import subprocess
import sys
import time
from pathlib import Path


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def raw_model(source, destination):
    """Read NNE's retained ONNX payload, without importing/modifying the asset."""
    source, destination = Path(source), Path(destination)
    if destination.exists():
        raise FileExistsError(destination)
    additional=[]
    with source.open('rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as data:
        marker = b'\x05\0\0\0onnx\0'
        at = data.find(marker)
        if at < 0:
            raise ValueError('NNE source ONNX was not retained')
        at += len(marker)
        count = struct.unpack_from('<q', data, at)[0]
        start = at + 8
        if not 0 < count <= len(data)-start:
            raise ValueError('Invalid retained ONNX byte count')
        digest = hashlib.sha256()
        with destination.open('xb') as out:
            for pos in range(start, start+count, 8*1024*1024):
                block = data[pos:min(pos+8*1024*1024, start+count)]
                out.write(block); digest.update(block)
        # Large NNE models retain their ONNX external tensors after FileData.
        # UE5.8 NNEModelData V4/V5: int32 count, FString key, TArray64<uint8>.
        pos=start+count;items=struct.unpack_from('<i',data,pos)[0];pos+=4
        if not 0<=items<=10000:raise ValueError('Unsupported NNE additional file count')
        for _ in range(items):
            length=struct.unpack_from('<i',data,pos)[0];pos+=4
            if not 1<=length<=1024:raise ValueError('Unsupported NNE external tensor name')
            name=data[pos:pos+length-1].decode('utf-8');pos+=length
            if Path(name).name!=name or '/' in name or '\\' in name or name in ('.','..'):
                raise ValueError('Unsafe external model filename')
            size=struct.unpack_from('<q',data,pos)[0];pos+=8
            if not 0<size<=len(data)-pos:raise ValueError('Invalid NNE external tensor size')
            h=hashlib.sha256()
            with (destination.parent/name).open('xb') as out:
                for p in range(pos,pos+size,8*1024*1024):
                    block=data[p:min(p+8*1024*1024,pos+size)];out.write(block);h.update(block)
            additional.append(dict(name=name,bytes=size,sha256=h.hexdigest()));pos+=size
    descriptor=destination.parent/'OnnxExternalDataDescriptor';bucket=destination.parent/'OnnxExternalDataBytes'
    if any(x['name']=='OnnxExternalDataDescriptor' for x in additional) and bucket.exists():
        blob=descriptor.read_bytes();count_items=struct.unpack_from('<i',blob,0)[0];pos=4;offset=0
        with bucket.open('rb') as tensors:
            for _ in range(count_items):
                length=struct.unpack_from('<i',blob,pos)[0];pos+=4
                if not 1<=length<=1024:raise ValueError('Unsupported external descriptor name')
                name=blob[pos:pos+length-1].decode('utf-8');pos+=length
                if Path(name).name!=name or '/' in name or '\\' in name or name in ('.','..'):
                    raise ValueError('Unsafe external descriptor path')
                size=struct.unpack_from('<q',blob,pos)[0];pos+=8
                if not 0<size<=bucket.stat().st_size-offset:raise ValueError('Invalid external descriptor size')
                with (destination.parent/name).open('xb') as out:
                    remaining=size
                    while remaining:
                        block=tensors.read(min(8*1024*1024,remaining));out.write(block);remaining-=len(block)
                offset+=size
        if pos!=len(blob) or offset!=bucket.stat().st_size:raise ValueError('External descriptor coverage mismatch')
    return dict(asset=str(source.resolve()), asset_sha256=sha(source),
                onnx_sha256=digest.hexdigest(), bytes=count,external_tensors=additional)


def extract(media, output, model_dir, inference_python, bbox=None, sample_hz=5., provider='CPUExecutionProvider'):
    import numpy as np
    import cv2
    from PIL import Image
    media, output, model_dir = Path(media).resolve(), Path(output).resolve(), Path(model_dir).resolve()
    if not 0 < sample_hz <= 30 or output.exists():
        raise ValueError('Need a new output directory and sampling rate 0..30 Hz')
    output.mkdir(parents=True)
    (output/'images').mkdir()
    rows, tensors = [], []
    started = time.perf_counter()

    def add(rgb, seconds, duration):
        height, width = rgb.shape[:2]
        if max(width,height)>8192 or min(width,height)<64:
            raise ValueError('Unsupported source image dimensions')
        box = [0., 0., float(width), float(height)] if bbox is None else list(map(float,bbox))
        x1,y1,x2,y2=box
        if not (0<=x1<x2<=width and 0<=y1<y2<=height):
            raise ValueError('Person box must be inside the image')
        cx,cy=(x1+x2)/2,(y1+y2)/2
        h=max(y2-y1,(x2-x1)*256/192)*1.10;w=h*192/256
        # Map image pixel centers to model input centers, preserve aspect ratio.
        affine=np.array([[192/w,0,96-cx*192/w],[0,256/h,128-cy*256/h]],np.float32)
        crop=cv2.warpAffine(rgb,affine,(192,256),flags=cv2.INTER_LINEAR,borderMode=cv2.BORDER_CONSTANT,borderValue=(128,128,128))
        normalized=(crop.astype(np.float32)/255-np.array([.485,.456,.406],np.float32))/np.array([.229,.224,.225],np.float32)
        tensors.append(normalized.transpose(2,0,1))
        name=f'{len(rows):04d}.png';Image.fromarray(rgb).save(output/'images'/name)
        rows.append(dict(seconds=float(seconds),duration=float(duration),width=width,height=height,
                         image='images/'+name,crop_box=[cx-w/2,cy-h/2,cx+w/2,cy+h/2]))

    if media.suffix.lower() not in {'.mp4','.mov','.mkv','.avi','.webm','.m4v','.wmv'}:
        raise ValueError('An original video file is required')
    cap=cv2.VideoCapture(str(media))
    if not cap.isOpened():raise ValueError('Could not open movie')
    fps=cap.get(cv2.CAP_PROP_FPS);count=cap.get(cv2.CAP_PROP_FRAME_COUNT)
    if fps<=0 or count<=0:raise ValueError('Movie needs known timing')
    now=0.
    while now < count/fps-1e-6:
        cap.set(cv2.CAP_PROP_POS_MSEC,now*1000);ok,bgr=cap.read()
        if not ok:break
        add(cv2.cvtColor(bgr,cv2.COLOR_BGR2RGB),now,1/sample_hz);now+=1/sample_hz
    cap.release()
    if not 2<=len(rows)<=3600:raise ValueError('Expected 2..3600 observations')
    np.save(output/'inputs.npy',np.stack(tensors))
    manifest=dict(version=1,source=str(media),source_sha256=sha(media),frames=rows,
                  model_dir=str(model_dir),provider=provider,cache_reuse=False)
    (output/'input.json').write_text(json.dumps(manifest,indent=2))
    subprocess.run([str(inference_python),str(Path(__file__).resolve()),'infer','--input',str(output/'input.json')],check=True)
    raw=np.load(output/'raw-keypoints.npy')
    if raw.shape!=(len(rows),133,3) or not np.isfinite(raw).all():raise ValueError('Invalid pose estimator output')
    for row,k in zip(rows,raw):
        x1,y1,x2,y2=row['crop_box'];k=k.astype(float)
        # ViTPosePost outputs in the 48x64 heatmap coordinate system.
        k[:,0]=k[:,0]/47*(x2-x1)+x1;k[:,1]=k[:,1]/63*(y2-y1)+y1
        row['keypoints']=k.tolist()
    manifest['schema']='cbm_pose_observations_v1';manifest['keypoint_layout']='coco_wholebody_133'
    manifest['seconds']=time.perf_counter()-started
    manifest['inference']=json.loads((output/'inference.json').read_text())
    (output/'observations.json').write_text(json.dumps(manifest,allow_nan=False,indent=2))
    draw_observations(manifest,output/'observed-first.png',0)
    draw_observations(manifest,output/'observed-middle.png',len(rows)//2)
    return manifest


def infer(input_file):
    import numpy as np
    import onnxruntime as ort
    input_file=Path(input_file);work=input_file.parent;c=json.loads(input_file.read_text())
    if (work/'raw-keypoints.npy').exists():raise FileExistsError('Fresh observation output already exists')
    provenance={}
    for name in ['ViTPose','ViTPosePost']:
        provenance[name]=raw_model(Path(c['model_dir'])/(name+'.uasset'),work/(name+'.onnx'))
    options=ort.SessionOptions();options.intra_op_num_threads=4;options.inter_op_num_threads=1
    options.enable_mem_pattern=False;options.execution_mode=ort.ExecutionMode.ORT_SEQUENTIAL
    provider=c['provider']
    if provider not in ort.get_available_providers():raise ValueError('Requested ORT provider is unavailable')
    started=time.perf_counter();print('Loading installed ViTPose',flush=True)
    pose=ort.InferenceSession(str(work/'ViTPose.onnx'),sess_options=options,providers=[provider])
    post=ort.InferenceSession(str(work/'ViTPosePost.onnx'),sess_options=options,providers=['CPUExecutionProvider'])
    X=np.load(work/'inputs.npy');result=[]
    for i,x in enumerate(X):
        heat=pose.run(None,{pose.get_inputs()[0].name:x[None]})[0]
        key=post.run(None,{post.get_inputs()[0].name:heat})[0]
        result.append(key.reshape(133,3));print(f'Fresh keypoints {i+1}/{len(X)}',flush=True)
        (work/'progress.json').write_text(json.dumps(dict(done=i+1,total=len(X),seconds=time.perf_counter()-started)))
    np.save(work/'raw-keypoints.npy',np.stack(result))
    record=dict(models=provenance,onnxruntime=ort.__version__,provider=provider,
                session_providers=pose.get_providers(),keys=len(result),seconds=time.perf_counter()-started,cache_hit=False)
    (work/'inference.json').write_text(json.dumps(record,indent=2))


def draw_observations(observations,destination,index):
    from PIL import Image, ImageDraw
    row=observations['frames'][index]
    image=Image.open(Path(destination).parent/row['image']).convert('RGB');draw=ImageDraw.Draw(image)
    k=row['keypoints'];edges=[(5,6),(5,7),(7,9),(6,8),(8,10),(5,11),(6,12),(11,12),(11,13),(13,15),(12,14),(14,16),(15,17),(16,20)]
    for a,b in edges:
        if min(k[a][2],k[b][2])>.25:draw.line([tuple(k[a][:2]),tuple(k[b][:2])],fill=(30,255,90),width=3)
    for i in range(23):
        x,y,score=k[i]
        if score>.25:draw.ellipse((x-3,y-3,x+3,y+3),fill='red');draw.text((x+4,y),str(i),fill='blue',stroke_width=1,stroke_fill='white')
    image.save(destination)


def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='command',required=True)
    a=s.add_parser('extract');a.add_argument('--media',required=True);a.add_argument('--output',required=True);a.add_argument('--model-dir',required=True);a.add_argument('--inference-python',required=True);a.add_argument('--bbox',nargs=4,type=float);a.add_argument('--sample-hz',type=float,default=5);a.add_argument('--provider',default='CPUExecutionProvider')
    a=s.add_parser('infer');a.add_argument('--input',required=True)
    a=p.parse_args()
    if a.command=='infer':infer(a.input)
    else:extract(a.media,a.output,a.model_dir,a.inference_python,a.bbox,a.sample_hz,a.provider)


if __name__=='__main__':main()
