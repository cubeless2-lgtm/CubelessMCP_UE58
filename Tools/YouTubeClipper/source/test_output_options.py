"""Offline integration tests: actual FFmpeg pixels, dimensions and audio."""
import array
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import unittest
import uuid

import youtube_clipper as app


class OutputTransformTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = Path(__file__).resolve().parent.parent / "test-output" / ("transforms-" + uuid.uuid4().hex[:8])
        cls.work.mkdir(parents=True)
        cls.ffmpeg = app.ffmpeg_path()
        cls.source = cls.work / "quadrants.mp4"
        filters = ("drawbox=x=0:y=0:w=96:h=64:color=red:t=fill,"
                   "drawbox=x=96:y=0:w=96:h=64:color=lime:t=fill,"
                   "drawbox=x=0:y=64:w=96:h=64:color=blue:t=fill,"
                   "drawbox=x=96:y=64:w=96:h=64:color=yellow:t=fill,"
                   "drawbox=x=0:y=0:w=8:h=128:color=magenta:t=fill")
        cls.run_ffmpeg(["-f", "lavfi", "-i", "color=black:s=192x128:r=25:d=2",
                        "-f", "lavfi", "-i", "sine=frequency=880:duration=2", "-vf", filters,
                        "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", str(cls.source)])
        cls.outputs = []

    @classmethod
    def run_ffmpeg(cls, args):
        return subprocess.run([cls.ffmpeg, "-hide_banner", "-nostdin", "-y"] + args,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)

    def convert(self, **options):
        config = dict(input_file=str(self.source), start="0.25", end="1.25", output_dir=str(self.work))
        config.update(options)
        with contextlib.redirect_stdout(io.StringIO()):
            output = app.make_clip(config)
        self.outputs.append(str(output))
        result = self.run_ffmpeg(["-i", str(output), "-map", "0:v:0", "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"])
        stderr = result.stderr.decode("utf-8", "replace")
        match = re.search(r"Video: rawvideo[^\n]*? (\d{2,5})x(\d{2,5})(?:[ ,])", stderr)
        self.assertIsNotNone(match, stderr)
        w, h = map(int, match.groups())
        self.assertEqual(len(result.stdout), w * h * 3)
        self.assertIn("Audio: aac", stderr)
        duration = re.search(r"Duration: (\d+):(\d+):([\d.]+)", stderr)
        seconds = int(duration[1])*3600 + int(duration[2])*60 + float(duration[3])
        self.assertAlmostEqual(seconds, 1, delta=.15)
        def pixel(x, y):
            index = (y*w+x)*3
            return tuple(result.stdout[index:index+3])
        return w, h, pixel, stderr

    def assert_color(self, actual, expected):
        self.assertLess(max(abs(a-b) for a,b in zip(actual, expected)), 45, (actual, expected))

    def test_all_clockwise_rotations(self):
        red, green, blue, yellow = (255,0,0), (0,255,0), (0,0,255), (255,255,0)
        for angle, size, colors in [(0,(192,128),(red,green,blue,yellow)),
                                    (90,(128,192),(blue,red,yellow,green)),
                                    (180,(192,128),(yellow,blue,green,red)),
                                    (270,(128,192),(green,yellow,red,blue))]:
            with self.subTest(angle=angle):
                w,h,pixel,_ = self.convert(rotation=angle)
                self.assertEqual((w,h), size)
                for point, color in zip([(w//4,h//4),(3*w//4,h//4),(w//4,3*h//4),(3*w//4,3*h//4)], colors):
                    self.assert_color(pixel(*point), color)

    def test_exact_resolution_and_fitting(self):
        for mode in ("fit","fill","stretch"):
            with self.subTest(mode=mode):
                w,h,pixel,_ = self.convert(resolution="160x160", resize_mode=mode)
                self.assertEqual((w,h),(160,160))
                if mode == "fit":
                    self.assert_color(pixel(80,2),(0,0,0))
                    self.assert_color(pixel(2,40),(255,0,255))
                elif mode == "fill":
                    self.assert_color(pixel(2,40),(255,0,0))
                else:
                    self.assert_color(pixel(2,40),(255,0,255))

    def test_rotation_before_final_resolution(self):
        w,h,pixel,_ = self.convert(rotation=90, resolution="160x240")
        self.assertEqual((w,h),(160,240))
        self.assert_color(pixel(w//4,h//4),(0,0,255))

    def test_existing_display_rotation_not_applied_twice(self):
        rotated = self.work / "tagged-rotation.mp4"
        self.run_ffmpeg(["-display_rotation:v:0","90","-i",str(self.source),"-c","copy",str(rotated)])
        w,h,_,stderr = self.convert(input_file=str(rotated),rotation=0)
        self.assertEqual((w,h),(128,192))
        self.assertNotRegex(stderr,r"rotation of (?:-?90|180|270)")

    def test_supported_video_addresses(self):
        for url in ("https://www.instagram.com/reel/DTUb7-XkniJ/?igsh=example",
                    "instagram.com/p/CvSk4GkgrFH/", "https://instagram.com/reels/Chunk8-jurw/",
                    "https://www.instagram.com/tv/ABC_123-/", "https://youtu.be/W-5q1EarSLA",
                    "https://vimeo.com/76979871", "https://example.com/video.mp4"):
            with self.subTest(url=url):
                normalized, start, end = app.validate(url, "0", "5")
                self.assertTrue(normalized.startswith("https://"))
                self.assertEqual((start,end),(0,5))
        for url in ("ftp://example.com/video.mp4", "file:///C:/video.mp4", "https:///missing-host",
                    "https://user:password@example.com/video.mp4"):
            with self.subTest(url=url), self.assertRaises(ValueError):
                app.validate(url,"0","5")

    def test_bad_options(self):
        for config in [dict(resolution="1921x1080"),dict(resolution="0x100"),dict(resolution="abc"),dict(rotation=45),dict(resize_mode="other")]:
            with self.subTest(config=config), self.assertRaises(ValueError):
                app.output_options(config)

    def test_editable_settings_roundtrip(self):
        from unittest.mock import patch
        settings = self.work / "settings.json"
        with patch.object(app,"settings_path",return_value=settings):
            app.save_settings(dict(output_dir=str(self.work),max_height=1920,resolution="720x1280",rotation=270,resize_mode="fit"))
            loaded = app.load_settings()
            self.assertEqual(app.output_options(loaded),("720x1280",270,"fit"))

    def test_play_rate_changes_video_audio_timing_without_pitch_shift(self):
        source = self.work / "timed-colors-and-tones.mp4"
        self.run_ffmpeg(["-f", "lavfi", "-i", "color=red:s=192x128:r=25:d=4",
                         "-f", "lavfi", "-i", "aevalsrc=if(lt(t\\,2)\\,0.3*sin(2*PI*440*t)\\,0.3*sin(2*PI*880*t)):s=48000:d=4",
                         "-vf", "drawbox=x=0:y=0:w=iw:h=ih:color=blue:t=fill:enable='gte(t,2)'",
                         "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", str(source)])
        for rate in (.125, .5, 1, 2, 8):
            with self.subTest(rate=rate):
                events = io.StringIO()
                with contextlib.redirect_stdout(events):
                    output = app.make_clip(dict(input_file=str(source), start=1, end=3,
                                                output_dir=str(self.work), play_rate=rate))
                self.outputs.append(str(output))
                done = next(json.loads(line) for line in events.getvalue().splitlines() if json.loads(line)["event"] == "complete")
                expected = 2 / rate
                self.assertEqual(done["source_seconds"], 2)
                self.assertEqual(done["seconds"], expected)
                self.assertEqual(done["play_rate"], rate)
                self.assertAlmostEqual(app.local_duration(output), expected, delta=.09)
                for fraction, color in [(.25,(255,0,0)),(.75,(0,0,255))]:
                    frame = self.run_ffmpeg(["-ss", str(expected*fraction), "-i", str(output), "-map", "0:v:0", "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"])
                    self.assertEqual(len(frame.stdout),192*128*3)
                    index = (64*192+96)*3
                    self.assert_color(tuple(frame.stdout[index:index+3]), color)
                pcm = self.run_ffmpeg(["-i",str(output),"-map","0:a:0","-f","s16le","-ac","1","-ar","48000","pipe:1"])
                samples = array.array("h", pcm.stdout)
                self.assertAlmostEqual(len(samples)/48000, expected, delta=.09)
                for fraction, frequency in [(.25,440),(.75,880)]:
                    center = int(expected*fraction*48000)
                    segment = samples[center-1200:center+1200]
                    self.assertGreater(max(abs(x) for x in segment),1000)
                    crossings = sum(a<=0<b for a,b in zip(segment,segment[1:]))
                    measured = crossings / ((len(segment)-1)/48000)
                    self.assertAlmostEqual(measured,frequency,delta=45)

    def test_play_rate_accepts_silent_input_and_combined_transform(self):
        silent = self.work / "silent.mp4"
        self.run_ffmpeg(["-i",str(self.source),"-map","0:v:0","-an","-c:v","copy",str(silent)])
        with contextlib.redirect_stdout(io.StringIO()):
            out = app.make_clip(dict(input_file=str(silent), start=.25,end=1.25,
                                     output_dir=str(self.work),play_rate=.5,rotation=90,resolution="160x240"))
        self.outputs.append(str(out))
        self.assertAlmostEqual(app.local_duration(out),2,delta=.09)
        frame = self.run_ffmpeg(["-i",str(out),"-map","0:v:0","-frames:v","1","-f","rawvideo","-pix_fmt","rgb24","pipe:1"])
        self.assertEqual(len(frame.stdout),160*240*3)
        self.assertNotIn("Audio:",frame.stderr.decode("utf-8","replace"))

    def test_play_rate_validation_and_old_settings(self):
        from unittest.mock import patch
        for value in [0,-1,.01,9,float("nan"),float("inf"),"abc",True]:
            with self.subTest(value=value),self.assertRaises(ValueError):
                app.make_clip({"play_rate":value})
        settings = self.work / "old-settings.json"
        settings.write_text('{"rotation":90}',encoding="utf-8")
        with patch.object(app,"settings_path",return_value=settings):
            self.assertEqual(app.load_settings()["play_rate"],1)
            app.save_settings(dict(output_dir=str(self.work),max_height=1920,play_rate=.75))
            self.assertEqual(app.load_settings()["play_rate"],.75)

    def test_empty_video_is_not_reported_as_complete(self):
        target = self.work / "empty-output"
        events = io.StringIO()
        with contextlib.redirect_stdout(events), self.assertRaisesRegex(ValueError, "영상 프레임"):
            app.make_clip(dict(input_file=str(self.source), start=.001, end=.002,
                               play_rate=8, output_dir=str(target)))
        self.assertFalse(any(json.loads(line)["event"] == "complete" for line in events.getvalue().splitlines()))
        self.assertEqual(list(target.iterdir()), [])

    def test_times_must_be_finite_and_folder_paths_are_consistent(self):
        from unittest.mock import patch
        with self.assertRaises(ValueError):
            app.parse_time("9"*400)
        with self.assertRaises(ValueError):
            app.validate("https://example.com/bad path.mp4", "0", "1")
        settings = self.work / "path-settings.json"
        with patch.object(app, "settings_path", return_value=settings):
            self.assertEqual(app.resolve_gui_folder("relative-output"), (self.work / "relative-output").resolve())
            app.save_settings(dict(output_dir="relative-output", max_height=720))
            self.assertEqual(Path(app.load_settings()["output_dir"]), app.resolve_gui_folder("relative-output"))
            with self.assertRaises(ValueError):
                app.resolve_gui_folder(" ")

    def test_download_quality_limit_and_silent_formats(self):
        import yt_dlp
        from yt_dlp.utils import ExtractorError
        def select(formats):
            with yt_dlp.YoutubeDL(dict(format=app.download_format(720), quiet=True, logger=app.DownloadLog())) as dl:
                with contextlib.redirect_stdout(io.StringIO()):
                    return dl.process_ie_result(dict(id="quality", title="quality", extractor="generic", formats=formats), download=False)
        base = dict(url="https://example.test/video.mp4", ext="mp4", vcodec="avc1", acodec="aac")
        low = dict(base, format_id="720", height=720)
        high = dict(base, format_id="2160", height=2160)
        self.assertEqual(select([low, high])["height"], 720)
        with self.assertRaises(ExtractorError):
            select([high])
        self.assertEqual(select([dict(low, acodec="none")])["acodec"], "none")
        self.assertEqual(select([dict(base, format_id="unknown")])["format_id"], "unknown")

    def test_download_uses_final_filepath_and_checks_actual_duration(self):
        import shutil
        from unittest.mock import patch
        info = dict(id="download-fixture", title="download-fixture", duration=None)
        for end in (1, 3):
            with self.subTest(end=end), patch("yt_dlp.YoutubeDL") as constructor:
                dl = constructor.return_value.__enter__.return_value
                dl.extract_info.return_value = info.copy()
                def download(metadata):
                    work = Path(constructor.call_args.args[0]["outtmpl"]).parent
                    final = work / "final-container.m2ts"
                    shutil.copy2(self.source, final)
                    metadata["filepath"] = str(final)
                dl.process_info.side_effect = download
                config = dict(url="https://example.test/video", start=0, end=end, output_dir=str(self.work))
                with contextlib.redirect_stdout(io.StringIO()):
                    if end == 3:
                        with self.assertRaisesRegex(ValueError, "영상 길이"):
                            app.make_clip(config)
                    else:
                        output = app.make_clip(config)
                        self.outputs.append(str(output))
                        self.assertAlmostEqual(app.local_duration(output), 1, delta=.09)

    def test_delayed_audio_scales_with_play_rate(self):
        source = self.work / "delayed-audio.mp4"
        self.run_ffmpeg(["-f", "lavfi", "-i", "color=red:s=192x128:r=25:d=4",
                         "-itsoffset", "0.5", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000:duration=3.5",
                         "-c:v", "libx264", "-c:a", "aac", str(source)])
        def audio_start(path):
            result = self.run_ffmpeg(["-i", str(path), "-map", "0:a:0", "-c", "copy", "-f", "framehash", "-"])
            rows = [x for x in result.stdout.decode().splitlines() if x.strip() and not x.startswith("#")]
            return int(rows[0].split(",")[2]) / 48000
        original = audio_start(source)
        for rate in (.5, 2):
            with self.subTest(rate=rate), contextlib.redirect_stdout(io.StringIO()):
                out = app.make_clip(dict(input_file=str(source),start=0,end=4,play_rate=rate,output_dir=str(self.work)))
                self.outputs.append(str(out))
                self.assertAlmostEqual(audio_start(out), original/rate - 1024/48000, delta=.012)

    def test_worker_output_and_failure_cleanup(self):
        import queue
        import shutil
        from unittest.mock import patch, Mock
        events = queue.Queue()
        process = Mock(stdout=io.StringIO('null\n123\n{"unexpected":1}\n{"event":"log","message":"ok"}\n'))
        process.wait.return_value = 0
        app.read_worker_events(process,events)
        received = [events.get_nowait() for _ in range(events.qsize())]
        self.assertEqual([e["event"] for e in received], ["log"]*4+["exit"])
        failed = self.work / "failed-launch"
        failed.mkdir()
        with patch.object(app.tempfile, "mkdtemp", return_value=str(failed)), patch.object(app.subprocess, "Popen", side_effect=OSError("launch failed")):
            with self.assertRaises(OSError):
                app.start_worker({})
        self.assertFalse(failed.exists())
        process, work = app.start_worker(dict(input_file=str(self.source), start=.25, end=1.25,
                                              play_rate=.5, output_dir=str(self.work)))
        try:
            events = queue.Queue()
            app.read_worker_events(process,events)
            received = [events.get_nowait() for _ in range(events.qsize())]
            self.assertEqual(received[-1], dict(event="exit", code=0))
            done = next(e for e in received if e["event"] == "complete")
            self.outputs.append(done["path"])
            self.assertAlmostEqual(done["actual_seconds"], 2, delta=.09)
        finally:
            shutil.rmtree(work)

    @classmethod
    def tearDownClass(cls):
        (cls.work / "outputs.json").write_text(json.dumps(cls.outputs, indent=2), encoding="utf-8")
        print("Test evidence:", cls.work)


if __name__ == "__main__":
    unittest.main()
