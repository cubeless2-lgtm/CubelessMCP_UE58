"""Offline integration tests: actual FFmpeg pixels, dimensions and audio."""
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

    @classmethod
    def tearDownClass(cls):
        (cls.work / "outputs.json").write_text(json.dumps(cls.outputs, indent=2), encoding="utf-8")
        print("Test evidence:", cls.work)


if __name__ == "__main__":
    unittest.main()
