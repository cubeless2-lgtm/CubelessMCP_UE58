# Lower-body Python environment

Independent CPU-only runtime for `Tools/run_lower_worker.py`. No WiLoR, MediaPipe,
HandFlow, Torch, foot model, or model weights are used by this worker.

With standalone Windows Python 3.12, run:

```powershell
python Tools/prepare_lower_body.py
```

The script creates `ThirdParty/LowerBody/venv` and installs pinned NumPy 1.26.4,
SciPy 1.15.3 and psutil 7.2.2. Use that environment's `Scripts/python.exe` as
**Lower Body Python** in Cubeless Body Mocap. The hand environments remain untouched.
Installed wheels retain their packaged license/notice files, including NumPy/SciPy
and their bundled numerical libraries. `environment.json` records local setup;
the environment and receipt are excluded from source control and can be rebuilt.

Saved body inputs retain historical `FootMR_*` filenames for compatibility. Those
files contain observations/calibration, not an active FootMR dependency. Lower-body
inference does not call the retained diagnostic hand backends.
