"""Rebuild the standalone Windows EXE using this directory's requirements."""
from pathlib import Path
import shutil
import imageio_ffmpeg
import PyInstaller.__main__

source = Path(__file__).resolve().parent
root = source.parent
shutil.copy2(imageio_ffmpeg.get_ffmpeg_exe(), source / "ffmpeg.exe")
PyInstaller.__main__.run([
    "--noconfirm", "--clean", "--onefile", "--windowed", "--name", "YouTubeClipper",
    "--collect-all", "yt_dlp", "--hidden-import", "tkinter",
    "--add-binary", f"{source / 'ffmpeg.exe'};.",
    "--distpath", str(root), "--workpath", str(root / "build"),
    "--specpath", str(source), str(source / "youtube_clipper.py"),
])
