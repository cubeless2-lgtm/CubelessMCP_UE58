"""Windows GUI and CLI for making precise YouTube clips."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import urlparse
import uuid


def parse_time(value: str) -> float:
    parts = value.strip().split(":")
    if not 1 <= len(parts) <= 3 or any(not re.fullmatch(r"\d+(?:\.\d+)?", p) for p in parts):
        raise ValueError("시간은 초(1.5), 분:초(01:10), 시:분:초로 입력하세요.")
    if any(float(p) >= 60 for p in parts[1:]):
        raise ValueError("콜론 뒤의 분과 초는 60보다 작아야 합니다.")
    total = 0.0
    for part in parts:
        total = total * 60 + float(part)
    return total


def validate(url: str, start: str, end: str) -> tuple[str, float, float]:
    url = url.strip()
    if not url.startswith(("https://", "http://")):
        url = "https://" + url
    parsed = urlparse(url)
    if parsed.scheme not in ("https", "http") or (parsed.hostname or "").lower() not in {
        "youtube.com", "www.youtube.com", "m.youtube.com", "music.youtube.com", "youtu.be"
    }:
        raise ValueError("youtube.com 또는 youtu.be 영상 주소를 입력하세요.")
    a, b = parse_time(start), parse_time(end)
    if b <= a:
        raise ValueError("종료 시간은 시작 시간보다 커야 합니다.")
    return url, a, b


def emit(kind: str, **fields):
    print(json.dumps({"event": kind, **fields}, ensure_ascii=False), flush=True)


class DownloadLog:
    def debug(self, message):
        pass

    def warning(self, message):
        emit("log", message="안내: " + str(message))

    def error(self, message):
        emit("log", message=str(message))


def ffmpeg_path() -> str:
    if getattr(sys, "frozen", False):
        return str(Path(sys._MEIPASS) / "ffmpeg.exe")
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()



RESOLUTION_PRESETS = ("원본 유지", "1920x1080", "1080x1920", "1280x720", "720x1280", "640x360", "360x640", "3840x2160")
RESIZE_MODES = {"여백 추가 (비율 유지)": "fit", "잘라 채우기": "fill", "늘려 맞추기": "stretch"}


def output_options(config: dict) -> tuple[str, int, str]:
    value = str(config.get("resolution", "original")).strip().lower().replace("×", "x").replace(" ", "")
    if value in ("", "original", "원본유지"):
        value = "original"
    else:
        match = re.fullmatch(r"(\d+)x(\d+)", value)
        if not match:
            raise ValueError("출력 해상도는 원본 유지 또는 1920x1080 형식으로 입력하세요.")
        width, height = map(int, match.groups())
        if any(n < 2 or n > 8192 or n % 2 for n in (width, height)):
            raise ValueError("출력 가로·세로는 각각 2~8192 범위의 짝수여야 합니다.")
        value = f"{width}x{height}"
    raw_rotation = str(config.get("rotation", 0))
    if raw_rotation not in ("0", "90", "180", "270"):
        raise ValueError("회전은 시계방향 0, 90, 180, 270도 중 선택하세요.")
    rotation = int(raw_rotation)
    mode = config.get("resize_mode", "fit")
    if mode not in RESIZE_MODES.values():
        raise ValueError("크기 맞춤은 fit, fill, stretch 중 선택하세요.")
    return value, rotation, mode


def video_filters(config: dict) -> str:
    resolution, rotation, mode = output_options(config)
    # Normalize display pixels before rotation (including anamorphic inputs).
    filters = ["scale=iw*sar:ih", "setsar=1"]
    if rotation == 90:
        filters.append("transpose=clock")
    elif rotation == 180:
        filters.extend(["hflip", "vflip"])
    elif rotation == 270:
        filters.append("transpose=cclock")
    if resolution != "original":
        width, height = resolution.split("x")
        if mode == "fit":
            filters.extend([f"scale={width}:{height}:force_original_aspect_ratio=decrease:force_divisible_by=2",
                            f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:color=black"])
        elif mode == "fill":
            filters.extend([f"scale={width}:{height}:force_original_aspect_ratio=increase:force_divisible_by=2",
                            f"crop={width}:{height}:(iw-ow)/2:(ih-oh)/2"])
        else:
            filters.append(f"scale={width}:{height}")
    else:
        filters.append("pad=ceil(iw/2)*2:ceil(ih/2)*2:0:0")
    filters.append("setsar=1")
    return ",".join(filters)



def local_duration(source: Path) -> float:
    """Read local container duration with the bundled FFmpeg; no ffprobe install."""
    result = subprocess.run([ffmpeg_path(), "-hide_banner", "-nostdin", "-i", str(source)],
                            capture_output=True, encoding="utf-8", errors="replace",
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    match = re.search(r"Duration: (\d+):(\d+):([\d.]+)", result.stderr)
    if not match:
        raise ValueError("로컬 영상 길이를 확인할 수 없습니다. MP4 등 길이 정보가 있는 영상을 선택하세요.")
    return int(match[1])*3600 + int(match[2])*60 + float(match[3])


def make_clip(config: dict) -> Path:
    output_options(config)
    if config.get("input_file"):
        source = Path(config["input_file"]).expanduser().resolve()
        if not source.is_file():
            raise ValueError("입력 동영상 파일을 찾지 못했습니다.")
        start, end = parse_time(str(config["start"])), parse_time(str(config["end"]))
        if end <= start:
            raise ValueError("종료 시간은 시작 시간보다 커야 합니다.")
        duration = local_duration(source)
        if end > duration + 0.05:
            raise ValueError(f"종료 시간이 영상 길이({duration:g}초)를 초과합니다.")
        emit("metadata", title=source.stem, duration=duration)
        return encode_clip(source, config, start, end, {"id": source.stem, "title": source.stem})
    import yt_dlp
    url, start, end = validate(config["url"], str(config["start"]), str(config["end"]))
    dest = Path(config["output_dir"]).expanduser().resolve()
    dest.mkdir(parents=True, exist_ok=True)
    height = int(config.get("max_height", 1920))
    if height not in (720, 1080, 1920, 2160):
        raise ValueError("지원하는 최대 높이는 720, 1080, 1920, 2160입니다.")
    ffmpeg = ffmpeg_path()
    last_update = [0.0]

    def progress(info):
        if info["status"] == "downloading" and time.monotonic() - last_update[0] > 0.35:
            last_update[0] = time.monotonic()
            total = info.get("total_bytes") or info.get("total_bytes_estimate")
            percent = min(100, 100 * info.get("downloaded_bytes", 0) / total) if total else None
            emit("progress", percent=percent, message="원본 영상 다운로드 중…")
        elif info["status"] == "finished":
            emit("log", message="스트림 다운로드 완료")

    owned_temp = config.get("work_dir")
    with tempfile.TemporaryDirectory(prefix="youtube-clip-", dir=owned_temp) as temp:
        work = Path(temp)
        opts = {
            "format": f"bv[height<={height}][vcodec^=avc1]+ba[ext=m4a]/bv[height<={height}]+ba/b[height<={height}]/b",
            "outtmpl": str(work / "source.%(ext)s"),
            "ffmpeg_location": ffmpeg,
            "merge_output_format": "mp4",
            "noplaylist": True,
            "socket_timeout": 20,
            "retries": 2,
            "fragment_retries": 2,
            "quiet": True,
            "noprogress": True,
            "logger": DownloadLog(),
            "progress_hooks": [progress],
            "windowsfilenames": True,
        }
        emit("log", message="영상 정보 확인 중…")
        with yt_dlp.YoutubeDL(opts) as dl:
            info = dl.extract_info(url, download=False)
            if info.get("is_live") or info.get("live_status") in ("is_live", "is_upcoming"):
                raise ValueError("진행 중이거나 예정된 라이브는 지원하지 않습니다.")
            duration = info.get("duration")
            if duration is not None and end > float(duration) + 0.05:
                raise ValueError(f"종료 시간이 영상 길이({duration}초)를 초과합니다.")
            emit("metadata", title=info.get("title", ""), duration=duration)
            emit("log", message="원본을 임시 저장합니다. 긴 영상은 다운로드에 시간이 걸립니다.")
            dl.process_info(info)
        candidates = [p for p in work.iterdir() if p.suffix in (".mp4", ".webm", ".mkv", ".mov")]
        merged = work / "source.mp4"
        source = merged if merged.exists() else next((p for p in candidates if p.stem == "source"), None)
        if source is None:
            raise RuntimeError("다운로드한 영상 파일을 찾지 못했습니다.")
        return encode_clip(source, config, start, end, info)


def encode_clip(source: Path, config: dict, start: float, end: float, info: dict) -> Path:
    """Shared local/YouTube encoder. Rotation is applied before output sizing."""
    resolution, rotation, mode = output_options(config)
    ffmpeg = ffmpeg_path()
    dest = Path(config["output_dir"]).expanduser().resolve()
    dest.mkdir(parents=True, exist_ok=True)
    emit("log", message=f"출력: {resolution} / 시계방향 {rotation}° / {mode}")
    video_id = re.sub(r"[^A-Za-z0-9_-]", "", info.get("id", "video"))[:40] or "video"
    stamp = time.strftime("%Y%m%d_%H%M%S")
    name = f"{video_id}_{start:g}s-{end:g}s_{stamp}_{uuid.uuid4().hex[:6]}.mp4"
    output = dest / name
    pending = dest / (name + ".partial.mp4")
    emit("progress", percent=None, message="지정한 구간을 MP4로 인코딩 중…")
    command = [ffmpeg, "-hide_banner", "-nostdin", "-loglevel", "error", "-n",
               "-ss", str(start), "-i", str(source), "-t", str(end - start),
               "-map", "0:v:0", "-map", "0:a:0?", "-vf", video_filters(config),
               "-metadata:s:v:0", "rotate=0", "-c:v", "libx264", "-preset", "fast",
               "-crf", "18", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "192k",
               "-movflags", "+faststart", str(pending)]
    creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    try:
        result = subprocess.run(command, capture_output=True, encoding="utf-8", errors="replace", creationflags=creationflags)
        if result.returncode:
            raise RuntimeError("인코딩 실패: " + result.stderr[-1800:])
        emit("log", message="완성된 영상의 디코딩 상태 확인 중…")
        check = subprocess.run([ffmpeg, "-hide_banner", "-nostdin", "-v", "error", "-xerror", "-i", str(pending),
                                "-f", "null", "-"], capture_output=True, encoding="utf-8", errors="replace", creationflags=creationflags)
        if check.returncode:
            raise RuntimeError("영상 검증 실패: " + check.stderr[-1800:])
        pending.rename(output)
    finally:
        pending.unlink(missing_ok=True)
    emit("complete", path=str(output), seconds=end-start, title=info.get("title", ""),
         resolution=resolution, rotation=rotation, resize_mode=mode)
    return output


def default_output_dir() -> Path:
    base = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent.parent
    return base / "work"



def settings_path() -> Path:
    return default_output_dir().parent / "settings.json"


def load_settings() -> dict:
    defaults = {"output_dir": str(default_output_dir()), "max_height": 1920,
                "resolution": "original", "rotation": 0, "resize_mode": "fit"}
    path = settings_path()
    if path.exists():
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        if not isinstance(data, dict):
            raise ValueError("settings.json의 최상위 값은 JSON 객체여야 합니다.")
        defaults.update({k: data[k] for k in defaults if k in data})
    output_options(defaults)
    folder = Path(defaults["output_dir"]).expanduser()
    if not folder.is_absolute():
        defaults["output_dir"] = str(path.parent / folder)
    if defaults["max_height"] not in (720, 1080, 1920, 2160):
        raise ValueError("settings.json의 max_height를 확인하세요.")
    return defaults


def save_settings(config: dict) -> Path:
    resolution, rotation, mode = output_options(config)
    data = {k: config[k] for k in ("output_dir", "max_height")}
    data.update(resolution=resolution, rotation=rotation, resize_mode=mode)
    path = settings_path()
    pending = path.with_suffix(".json.tmp")
    pending.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    pending.replace(path)
    return path


def gui():
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    root = tk.Tk()
    root.title("YouTube 구간 저장")
    root.geometry("820x780")
    root.minsize(740, 720)
    root.configure(bg="#f4f6fb")
    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure("TFrame", background="#f4f6fb")
    style.configure("TLabel", background="#f4f6fb", font=("맑은 고딕", 10))
    style.configure("Title.TLabel", font=("맑은 고딕", 21, "bold"))
    style.configure("TButton", font=("맑은 고딕", 10), padding=8)
    style.configure("TEntry", padding=8)
    style.configure("Accent.TButton", background="#315de5", foreground="white")
    panel = ttk.Frame(root, padding=25)
    panel.pack(fill="both", expand=True)
    panel.columnconfigure(0, weight=1)
    ttk.Label(panel, text="YouTube 구간 저장", style="Title.TLabel").grid(row=0, column=0, sticky="w")
    ttk.Label(panel, text="유튜브 또는 로컬 영상을 자르고, 출력 해상도와 전체 회전을 지정합니다.").grid(row=1, column=0, sticky="w", pady=(5, 18))
    try:
        prefs = load_settings()
    except Exception as exc:
        messagebox.showwarning("설정 확인", f"설정 파일을 읽지 못해 기본값으로 시작합니다. 파일은 유지합니다.\n{exc}")
        prefs = {"output_dir": str(default_output_dir()), "max_height": 1920,
                 "resolution": "original", "rotation": 0, "resize_mode": "fit"}
    url_var = tk.StringVar()
    start_var, end_var = tk.StringVar(value="1"), tk.StringVar(value="10")
    folder_var = tk.StringVar(value=prefs["output_dir"])
    height_var = tk.StringVar(value=str(prefs["max_height"]))
    resolution_var = tk.StringVar(value="원본 유지" if prefs["resolution"] == "original" else prefs["resolution"])
    rotation_var = tk.StringVar(value=str(prefs["rotation"]))
    resize_var = tk.StringVar(value=next(k for k, v in RESIZE_MODES.items() if v == prefs["resize_mode"]))
    status_var = tk.StringVar(value="주소를 붙여 넣어 시작하세요.")
    ttk.Label(panel, text="유튜브 주소 또는 로컬 동영상 경로").grid(row=2, column=0, sticky="w")
    url_entry = ttk.Entry(panel, textvariable=url_var)
    url_entry.grid(row=3, column=0, sticky="ew", padx=(0, 110), pady=(4, 12))
    def choose_video():
        selected = filedialog.askopenfilename(filetypes=[("동영상", "*.mp4 *.mov *.mkv *.webm *.avi"), ("모든 파일", "*.*")])
        if selected:
            url_var.set(selected)
    ttk.Button(panel, text="영상 선택", command=choose_video).grid(row=3, column=0, sticky="e", pady=(4, 12))
    times = ttk.Frame(panel)
    times.grid(row=4, column=0, sticky="ew")
    for i, (label, var) in enumerate((("시작 시간", start_var), ("종료 시간", end_var))):
        ttk.Label(times, text=label).grid(row=0, column=i, sticky="w")
        ttk.Entry(times, textvariable=var, width=18).grid(row=1, column=i, padx=(0, 14), pady=4, sticky="ew")
        times.columnconfigure(i, weight=1)
    ttk.Label(times, text="다운로드 최대 높이").grid(row=0, column=2, sticky="w")
    ttk.Combobox(times, textvariable=height_var, values=("720", "1080", "1920", "2160"), state="readonly", width=10).grid(row=1, column=2)
    ttk.Label(panel, text="시간 예시: 1.5 또는 01:10 · 1초부터 10초까지는 9초 분량입니다.").grid(row=5, column=0, sticky="w", pady=(0, 12))
    transforms = ttk.Frame(panel)
    transforms.grid(row=6, column=0, sticky="ew", pady=(0, 4))
    for i, (label, var, choices, readonly) in enumerate([
        ("최종 출력 해상도 (가로x세로)", resolution_var, RESOLUTION_PRESETS, False),
        ("전체 회전 (시계방향)", rotation_var, ("0", "90", "180", "270"), True),
        ("해상도 맞춤 방식", resize_var, tuple(RESIZE_MODES), True),
    ]):
        transforms.columnconfigure(i, weight=1)
        ttk.Label(transforms, text=label).grid(row=0, column=i, sticky="w")
        ttk.Combobox(transforms, textvariable=var, values=choices, width=19,
                     state="readonly" if readonly else "normal").grid(row=1, column=i, sticky="ew", padx=(0, 8), pady=4)
    ttk.Label(panel, text="회전 후 지정 해상도로 맞춥니다. 원본 유지는 90°/270° 회전 시 가로·세로가 바뀝니다.", wraplength=740).grid(row=7, column=0, sticky="w", pady=(0, 12))
    ttk.Label(panel, text="저장 폴더").grid(row=8, column=0, sticky="w")
    folder_row = ttk.Frame(panel)
    folder_row.grid(row=9, column=0, sticky="ew", pady=(4, 12))
    folder_row.columnconfigure(0, weight=1)
    ttk.Entry(folder_row, textvariable=folder_var).grid(row=0, column=0, sticky="ew")

    def choose_folder():
        selected = filedialog.askdirectory(initialdir=folder_var.get())
        if selected:
            folder_var.set(selected)

    ttk.Button(folder_row, text="찾아보기", command=choose_folder).grid(row=0, column=1, padx=(8, 0))
    buttons = ttk.Frame(panel)
    buttons.grid(row=10, column=0, sticky="ew")
    progress = ttk.Progressbar(panel, mode="indeterminate")
    progress.grid(row=11, column=0, sticky="ew", pady=(14, 8))
    ttk.Label(panel, textvariable=status_var, wraplength=650).grid(row=12, column=0, sticky="w")
    log = tk.Text(panel, height=8, font=("맑은 고딕", 9), relief="flat", bg="white", wrap="word", state="disabled")
    log.grid(row=13, column=0, sticky="nsew", pady=(10, 4))
    panel.rowconfigure(13, weight=1)
    ttk.Label(panel, text="원본은 임시 저장 후 정리합니다. 로그인·연령·지역 제한 영상은 실패할 수 있습니다.", wraplength=650).grid(row=14, column=0, sticky="w")
    events = queue.Queue()
    state = {"process": None, "last": None, "work": None, "config": None}

    def append(message):
        log.configure(state="normal")
        log.insert("end", message + "\n")
        log.see("end")
        log.configure(state="disabled")

    def read_events(process):
        try:
            for line in process.stdout:
                try:
                    events.put(json.loads(line))
                except json.JSONDecodeError:
                    events.put({"event": "log", "message": line.strip()})
            process.wait()
        finally:
            events.put({"event": "exit", "code": process.returncode})

    def option_config():
        if not folder_var.get().strip():
            raise ValueError("저장 폴더를 지정하세요.")
        folder = Path(folder_var.get()).expanduser()
        if not folder.is_absolute():
            folder = default_output_dir().parent / folder
        config = {"output_dir": str(folder), "max_height": int(height_var.get()),
                  "resolution": resolution_var.get(), "rotation": int(rotation_var.get()),
                  "resize_mode": RESIZE_MODES[resize_var.get()]}
        resolution, rotation, mode = output_options(config)
        config.update(resolution=resolution, rotation=rotation, resize_mode=mode)
        return config

    def save_preferences():
        try:
            path = save_settings(option_config())
            append("설정 저장됨: " + str(path))
        except Exception as exc:
            messagebox.showerror("설정 저장 실패", str(exc))

    def begin():
        try:
            text = url_var.get().strip().strip('"')
            local = Path(text).expanduser().is_file() if text and not text.startswith(("http://", "https://")) else False
            if local:
                a, b = parse_time(start_var.get()), parse_time(end_var.get())
                if b <= a:
                    raise ValueError("종료 시간은 시작 시간보다 커야 합니다.")
                source_config = {"input_file": text}
            else:
                url, a, b = validate(text, start_var.get(), end_var.get())
                source_config = {"url": url}
            if not folder_var.get().strip():
                raise ValueError("저장 폴더를 지정하세요.")
            config = option_config()
            config.update(source_config, start=a, end=b)
            work = Path(tempfile.mkdtemp(prefix="YouTubeClipper-"))
            config["work_dir"] = str(work)
            config_file = work / "job.json"
            config_file.write_text(json.dumps(config, ensure_ascii=False), encoding="utf-8")
            command = [sys.executable] if getattr(sys, "frozen", False) else [sys.executable, str(Path(__file__).resolve())]
            command += ["--worker", str(config_file)]
            env = os.environ.copy()
            env["PYTHONIOENCODING"] = "utf-8"
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                       encoding="utf-8", errors="replace", env=env,
                                       creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            state.update(process=process, work=work, config=config)
            start_button.configure(state="disabled")
            status_var.set("작업을 시작합니다…")
            progress.start(12)
            append(f"구간: {a:g}초 → {b:g}초 ({b-a:g}초)")
            threading.Thread(target=read_events, args=(process,), daemon=True).start()
        except Exception as exc:
            messagebox.showerror("입력 확인", str(exc))

    def open_result():
        target = state["last"]
        if target and Path(target).exists():
            os.startfile(target)

    def open_folder():
        folder = Path(state["last"]).parent if state["last"] else Path(folder_var.get()).expanduser()
        folder.mkdir(parents=True, exist_ok=True)
        os.startfile(str(folder))

    start_button = ttk.Button(buttons, text="영상 저장", style="Accent.TButton", command=begin)
    start_button.pack(side="left")
    ttk.Button(buttons, text="설정 저장", command=save_preferences).pack(side="right")
    result_button = ttk.Button(buttons, text="완성 영상 열기", command=open_result, state="disabled")
    result_button.pack(side="left", padx=8)
    ttk.Button(buttons, text="저장 폴더 열기", command=open_folder).pack(side="left")

    def poll():
        while True:
            try:
                event = events.get_nowait()
            except queue.Empty:
                break
            kind = event.get("event")
            if kind == "progress":
                value = event.get("percent")
                status_var.set(event["message"] + (f" {value:.0f}%" if value is not None else ""))
            elif kind == "metadata":
                append(event.get("title", ""))
            elif kind == "log":
                append(event.get("message", ""))
            elif kind == "error":
                append(event["message"])
                status_var.set("실패했습니다. 아래 내용을 확인하세요.")
                messagebox.showerror("저장 실패", event["message"])
            elif kind == "complete":
                state["last"] = event["path"]
                result_button.configure(state="normal")
                status_var.set(f"완료 · {event['seconds']:g}초 MP4")
                append("저장됨: " + event["path"])
            elif kind == "exit":
                progress.stop()
                start_button.configure(state="normal")
                if event["code"]:
                    status_var.set("작업이 실패했습니다. 로그를 확인하세요.")
                state["process"] = None
                if state["work"]:
                    shutil.rmtree(state["work"], ignore_errors=True)
                    state["work"] = None
        root.after(150, poll)

    def close():
        if state["process"] is not None:
            messagebox.showinfo("작업 진행 중", "진행 중인 저장이 끝난 뒤 창을 닫아주세요.")
            return
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", close)
    root.after(150, poll)
    url_entry.focus_set()
    root.mainloop()


def main():
    # A console-free EXE still writes machine-readable events when its stdout is redirected.
    if sys.stdout is None and os.name == "nt" and len(sys.argv) > 1:
        import ctypes
        from ctypes import wintypes
        import msvcrt
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.GetStdHandle.argtypes = [wintypes.DWORD]
        kernel.GetStdHandle.restype = wintypes.HANDLE
        handle = kernel.GetStdHandle(-11 & 0xFFFFFFFF)
        if handle and handle != wintypes.HANDLE(-1).value:
            sys.stdout = os.fdopen(msvcrt.open_osfhandle(handle, os.O_WRONLY | os.O_BINARY), "w", encoding="utf-8", buffering=1)
    if sys.stdout is not None:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if sys.stderr is not None:
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="YouTube clipper. No arguments opens the Windows GUI.")
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument("--url")
    inputs.add_argument("--input-file", help="로컬 동영상 파일 (다운로드 없이 변환)")
    parser.add_argument("--start", default="1")
    parser.add_argument("--end", default="10")
    parser.add_argument("--output-dir", default=str(default_output_dir()))
    parser.add_argument("--max-height", type=int, default=1920, choices=(720, 1080, 1920, 2160))
    parser.add_argument("--resolution", default="original", help="최종 가로x세로, 예: 1920x1080. 기본 original")
    parser.add_argument("--rotation", type=int, default=0, choices=(0, 90, 180, 270), help="시계방향 전체 회전")
    parser.add_argument("--resize-mode", default="fit", choices=("fit", "fill", "stretch"))
    parser.add_argument("--worker", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.worker and not args.url and not args.input_file:
        gui()
        return 0
    try:
        config = json.loads(Path(args.worker).read_text(encoding="utf-8")) if args.worker else vars(args)
        make_clip(config)
        return 0
    except Exception as exc:
        emit("error", message=str(exc))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
