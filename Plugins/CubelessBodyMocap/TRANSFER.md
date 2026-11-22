# 원본 마커리스 플러그인 없이 다른 PC로 이전하기

준비된 전신·손 모델과 플러그인을 **같은 이용자의 다른 PC로 이전하는 패키지**입니다. 대상 PC에는 원본 MetaHuman Animator Markerless Motion Capture를 설치하지 않습니다. 원본 모델 변환·임포트나 HandFlow 모델 다운로드도 반복하지 않습니다. Python·Torch 실행 환경은 묶지 않고 각 PC에서 직접 설치합니다. 모델의 정밀도·가중치·해상도와 처리 기본값은 그대로 유지합니다.

현재 바이너리와 Unreal 모델 에셋은 **Windows x64 / UE5.8.4 CL58902052** 기준입니다. 모델·코드별 이용 조건과 고지는 그대로 적용됩니다. 이 패키지는 공개 재배포 허가나 상업용 모델 라이선스를 제공하지 않습니다. Epic 콘텐츠와 MANO 등의 기존 고지를 각 팩에 보관합니다. [Epic 콘텐츠 약관](https://www.unrealengine.com/eula/content), [MANO 이용 조건](https://mano.is.tuebingen.mpg.de/license.html)을 참고하세요.

## 옮길 파일

빌드 도구가 만드는 Release 폴더 전체를 옮깁니다. ZIP은 풀지 않고 같은 폴더에 둡니다. 전신 ZIP은 4 GB를 넘으므로 USB 파일시스템은 NTFS 또는 exFAT를 사용합니다. 전체 압축 용량은 약 9.24 GB, 플러그인·모델 설치 데이터는 약 10.26 GB입니다. 모델팩 설치 시작 시 대상 드라이브에 약 24 GB의 여유 공간을 확보합니다. 각 PC에서 설치할 Python·Torch와 pip 다운로드 캐시 용량은 별도로 추가됩니다. 이전 환경 크기는 약 8 GB였으며 실제 로컬 설치 용량은 설치한 패키지와 캐시에 따라 달라집니다.

| 파일 | 포함 내용 |
|---|---|
| `CubelessMocap-plugin-UE5.8.4.zip` | 플러그인 소스, 검증된 4개 Editor DLL, FFmpeg, 설치 도구·고지 |
| `CubelessMocap-body-UE5.8.4.zip` | 전신 모델·Detectron2·SAM2·SMPL, 고정 시드 카메라, 기존/고속/rows512 경로와 라이선스 |
| `CubelessMocap-hands-UE5.8.4.zip` | HandFlow·HaMeR·손 검출기·MANO와 필요한 런타임 소스·라이선스 |
| `CubelessMocap-manny-UE5.8.4.zip` | Manny와 리타깃 프리셋의 실제 `/Game` 에셋 의존성 묶음 |
| `release.json`, `*-receipt.json` | ZIP과 모든 내부 파일의 크기·SHA256 및 버전 |
| `Install-CubelessMocap.ps1`, `install_transfer.py`, `model_pack.py` | 원본 플러그인을 조회하지 않는 설치기 |
| `READ-ME-FIRST.txt`, `TRANSFER.md`, `release-files.sha256` | 시작 안내와 파일 전송 검사용 해시 |

소스 팩은 공개된 기준 커밋과 이번 설치 도구, 기존 작업 환경의 JSON 입력 인코딩 호환성 보완을 사용합니다. 이 보완은 UTF-8/UTF-16 요청 파일 읽기 한 줄이며 계산 알고리즘을 바꾸지 않습니다. 작업 PC의 관련 없는 로컬 수정, PDB, 중간 ONNX, 캐시, 테스트 영상·애니메이션은 포함하지 않습니다. 손 팩에는 기존 사용자 제공 MANO가 들어 있으므로 별도 소유자에게 전달하는 공개 패키지로 사용하지 않습니다.

## 새 PC 설치

1. UE 5.8.4와 사용할 기존 Unreal 프로젝트를 준비합니다. 전신은 UE의 NNE/DirectML을 사용하며, HandFlow에는 CUDA 12.8과 호환되는 NVIDIA GPU·드라이버가 필요합니다. MCP는 필요하지 않습니다. 손 모캡용 Python은 아래 절차에 따라 이 PC에서 직접 준비합니다.
2. Unreal Editor를 닫습니다. 새 설치는 대상 `Plugins/CubelessBodyMocap` 폴더가 없는 상태를 권장합니다. 기존 파일이 팩과 다르면 설치기는 덮어쓰지 않고 중단합니다.
3. Release 폴더의 PowerShell 설치기를 실행합니다. `Project`에는 폴더가 아닌 실제 `.uproject`를 지정합니다.

```powershell
& 'D:/Transfer/CubelessMocap/Install-CubelessMocap.ps1' `
  -Project 'D:/Projects/MyProject/MyProject.uproject' `
  -Engine 'C:/Program Files/Epic Games/UE_5.8'
```

PowerShell 스크립트 실행이 제한된 환경에서는 실행 정책을 바꾸지 않고 엔진 Python으로 직접 호출할 수 있습니다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' `
  'D:/Transfer/CubelessMocap/install_transfer.py' `
  --release 'D:/Transfer/CubelessMocap' `
  --project 'D:/Projects/MyProject/MyProject.uproject' `
  --engine 'C:/Program Files/Epic Games/UE_5.8'
```

설치기는 UE에 포함된 Python 3.11을 **표준 라이브러리 설치 작업에만** 사용합니다. UE Python에 pip 패키지를 설치하지 않습니다. HandFlow는 각 PC에서 만든 로컬 Python 환경을 사용합니다. 설치기는 Python·Torch를 설치하거나 기존 환경을 덮어쓰지 않습니다. 대상 엔진 버전과 DLL BuildId를 확인한 뒤 ZIP·파일 해시를 검사하고 복원합니다. 모든 팩의 충돌을 먼저 확인합니다. 각 팩은 전체 내용을 임시 폴더에서 검증한 후 새 파일을 배치합니다. 설치는 프로젝트 전체에 대한 원자적 롤백을 제공하지 않으며 오류가 나면 출력된 부분 완료 상태를 확인합니다.

프로젝트의 `CubelessBodyMocap`, `PythonScriptPlugin`을 켜며 Manny 팩을 설치하면 `ControlRig`, `ControlRigSpline`도 켭니다. 프로젝트 설정을 바꿀 때는 원본 `.uproject`를 별도 백업하고 다른 플러그인 설정은 보존합니다. 다른 엔진 버전은 바이너리를 강제로 재사용하지 말고 해당 엔진용 소스 빌드·모델 호환성 검증이 필요합니다.

확인만 하려면 `-Plan`, 기존 코드/DLL을 유지하려면 `-SkipPlugin`을 사용할 수 있습니다. 로컬 Python 환경은 기본적으로 보존하며 런타임 ZIP을 요구하지 않습니다. `-SkipPlugin`은 FFmpeg·플러그인이 이미 준비된 설치에 사용합니다. 전신만 옮길 때는 `-WithoutHands`를 사용하고 실행 UI에서 **HandFlow와 손가락 관절 보정**을 해제합니다. 없는 환경을 자동으로 다른 모델·정밀도로 바꾸지 않습니다.

## 로컬 Python 설치

손 모캡을 사용할 PC마다 Python 3.12.10 x64와 Git을 설치하고 **[플러그인 README의 6절: HandFlow 독립 Python 환경](README.md#6-handflow-독립-python-환경)**을 실행합니다. Release 설치 후 프로젝트 안의 `Plugins/CubelessBodyMocap/README.md`에서도 같은 절차를 볼 수 있습니다.

README 6절을 실행할 PowerShell 창에서 먼저 실제 프로젝트 경로를 지정합니다.

```powershell
$CBMProject = 'D:/Projects/MyProject'
$CBMPlugin = Join-Path $CBMProject 'Plugins/CubelessBodyMocap'
```

- `<프로젝트>/Plugins/CubelessBodyMocap/ThirdParty/HandFlow/venv`에 새 환경을 만듭니다. 기존 venv가 있으면 먼저 확인하며 설치기가 자동 교체하지 않습니다.
- 해당 환경에 Torch 2.8.0·Torchvision 0.23.0의 CUDA 12.8 휠을 설치합니다.
- README에 있는 공통 요구사항 → HandFlow overlay → Seaborn 0.13.2 → 고정 Chumpy 소스 순서를 따릅니다. 필요한 작은 요구사항 파일은 플러그인 ZIP에 포함됩니다. WiLoR 모델·환경은 설치하지 않습니다.
- `pip check`, import, CUDA 확인 후 짧은 실제 HandFlow 추론으로 확인합니다. 패키지 다운로드를 위해 인터넷이 필요합니다. 이 PC의 기존 환경은 이번 배포 변경으로 수정하지 않았습니다.

전신·손 모델과 MANO, Manny 프리셋은 이미 팩에 있으므로 README의 원본 모델 복사·변환·다운로드·Manny 생성 단계는 반복하지 않습니다. **이 배포 경로에서는 Python 환경에 관한 6절만 추가 진행합니다.** Python 준비 전 전신만 사용하려면 UI에서 HandFlow와 손가락 관절 보정을 해제합니다.

## 설치 후 확인

PowerShell에서 설치 위치의 파일을 확인합니다.

```powershell
$CBMPlugin = 'D:/Projects/MyProject/Plugins/CubelessBodyMocap'
& "$CBMPlugin/ThirdParty/FFmpeg/Win64/ffmpeg.exe" -version
& "$CBMPlugin/ThirdParty/HandFlow/venv/Scripts/python.exe" -I -c `
  "import sys,torch,numpy,scipy,cv2,smplx; print(sys.version); print(sys.path); print(torch.__version__,torch.cuda.is_available())"
& "$CBMPlugin/ThirdParty/HandFlow/venv/Scripts/python.exe" -B `
  "$CBMPlugin/Tools/prepare_content.py" --verify
```

`sys.path`에 이전 PC의 사용자 폴더나 WiLoR 환경이 없어야 합니다. `--verify`는 설치 영수증과 전신 파일 해시를 확인하며 원본 마커리스 플러그인을 찾지 않습니다. 최초 실행 캐시는 대상 PC에서 다시 만들 수 있습니다.

에디터를 열어 **Tools → Cubeless Body Mocap**에서 짧은 영상으로 확인합니다. 기본 Manny·리타깃 프리셋과 FFmpeg·손 Python 경로는 현재 프로젝트/플러그인 기준으로 설정됩니다. 다른 캐릭터에는 해당 메시·리타기터를 지정합니다. 프로그램 제어는 [직접 제어 API](WINDOW_API.md)를 사용합니다.

## 패키지 재생성

모델 준비가 끝난 원본 작업 PC에서만 실행합니다. 대상 PC의 설치 절차가 아닙니다.

```powershell
python Plugins/CubelessBodyMocap/Tools/build_transfer.py `
  --engine 'C:/Program Files/Epic Games/UE_5.8' `
  --project UnrealMCP5_8_3.uproject `
  --output Saved/CubelessBodyMocap/Transfer20261114/Release `
  --manny-dependencies Saved/CubelessBodyMocap/Transfer20261114/manny-dependencies.json
```

Python·venv·site-packages를 수집하지 않으며 4개 팩만 생성합니다. 기존 runtime ZIP/영수증이 남은 출력 폴더는 거절하므로 새 출력 폴더를 사용합니다. 이미 만들어진 ZIP은 덮어쓰지 않습니다. 새 출력 폴더를 사용합니다. 학습 모델은 재계산하거나 변경하지 않으며, 미채택 CHMR layout 실험과 개인 캐시는 포함하지 않습니다. Manny 의존성 목록은 기존 프로젝트의 Asset Registry에서 실제 사용할 메시·리타기터의 의존성으로 생성한 파일입니다.
