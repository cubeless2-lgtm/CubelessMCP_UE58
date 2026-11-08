import json,hashlib,urllib.request,zipfile,shutil,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
dest=root/'ThirdParty/FFmpeg/Win64'
meta=json.loads((dest/'PROVENANCE.json').read_text())
exe=dest/'ffmpeg.exe'
if exe.exists():
 assert hashlib.sha256(exe.read_bytes()).hexdigest()==meta['executable_sha256'], 'Existing FFmpeg differs; preserve it and inspect.'
 print('FFmpeg already prepared.')
else:
 with tempfile.TemporaryDirectory(prefix='cubeless_ffmpeg_') as tmp:
  archive=Path(tmp)/'ffmpeg.zip'
  urllib.request.urlretrieve(meta['download_url'],archive)
  assert hashlib.sha256(archive.read_bytes()).hexdigest()==meta['archive_sha256'], 'Upstream archive changed; review provenance before updating.'
  with zipfile.ZipFile(archive) as z:
   entry=next(n for n in z.namelist() if n.endswith('/bin/ffmpeg.exe'))
   with z.open(entry) as src,exe.open('xb') as out:shutil.copyfileobj(src,out)
  assert hashlib.sha256(exe.read_bytes()).hexdigest()==meta['executable_sha256']
 print('Prepared',exe)
