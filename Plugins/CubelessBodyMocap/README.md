# Cubeless Body Mocap 0.1.0

## 현재 UI와 저장 흐름

1. 영상·대상 메시·리타기터를 지정합니다. **Start Mocap** 위의 **HandFlow**, **본 보정**은 모두 기본 켜짐입니다. 본 보정은 손가락 관절 범위·관통 후처리이며 전신이나 손목을 변경하는 기능이 아닙니다.
2. **Start Mocap** 한 번으로 전신 → 선택한 HandFlow → 선택한 본 보정을 처리합니다. 본 보정은 현재 선택한 결과에 한 번만 적용합니다. 다른 저장 조합의 보정은 저장할 때 필요하면 계산하며 전신·손 추론은 반복하지 않습니다. 실행 중 취소와 진행 표시를 제공합니다.
3. 아래 **애니메이션 시퀀스 저장** 옆의 HandFlow·본 보정 체크로 저장할 결과를 고릅니다. 이 선택은 비교 뷰에도 반영됩니다. 전신 또는 HandFlow 원본이 없으면 저장을 막습니다. 원본은 있고 본 보정만 없으면 저장 버튼이 보정만 계산한 뒤 같은 이름으로 저장합니다. 계산 전 비교 뷰는 보정 전 결과를 표시합니다. 실행 옵션과 저장 옵션은 별개이며, 새 실행 시작 시 저장 옵션을 실행 옵션과 맞춥니다.
4. **최종 저장 폴더 / 애니메이션 이름**의 같은 애셋을 갱신합니다. 옵션이나 플레이 레이트를 바꿔도 다른 이름을 만들지 않습니다. 최초 저장 후 플러그인이 생성한 애셋만 갱신하며, 다른 기존 애셋이나 미저장 변경이 있는 대상은 덮어쓰지 않습니다.
5. 저장 **플레이 레이트**는 AnimSequence의 `RateScale`에 기록하며 키프레임은 재샘플링하지 않습니다. 비교 뷰의 **재생 속도**는 영상·애니메이션 미리보기용이며 저장 속도와 독립적입니다. 디코더가 지원하는 0.1~4배 범위에서 선택합니다.

계산 중간 결과는 `/Game/_MCP_Temp/CBMWork/Take_<ID>/`에 보관합니다. 최종 이름의 애셋은 저장 버튼을 눌러야 생성·갱신됩니다. 중간 결과와 기존 애셋을 자동 삭제하지 않습니다. 손 검사·진단 버튼과 중복 선택 UI는 제거했으며 기존 진단 API는 유지합니다. 각 노출 파라미터에는 한글 설명 툴팁이 있습니다.

아래 과거 검증 기록의 버튼 이름·저장 경로보다 이 절의 현재 UI 설명을 우선합니다.

## 다른 PC에서 처음 설치하기

**Git에는 소스만 들어 있습니다.** 모델, FFmpeg 실행 파일, Python 가상환경, 컴파일된 DLL, Manny 리타깃 에셋, 테스트 영상·결과는 포함하지 않습니다. 아래 순서로 해당 PC에서 준비해야 합니다. 명령이 실패하면 다음 단계로 넘어가지 말고 [설치 오류 점검](#설치-오류-점검)을 확인하세요.

검증 기준은 **Windows x64 / UE 5.8.4 CL58902052 / 독립 Python 3.12.10**입니다. 다른 UE 패치·플랫폼의 빌드와 새 PC에서의 전체 설치는 별도 검증 대상입니다. 아래 안내는 실제 준비 스크립트, 현재 설치 파일, 모델 manifest와 대조한 절차이며 깨끗한 다른 PC에서 일괄 설치를 완료했다는 의미는 아닙니다.

| 사용 기능 | 필요한 준비 |
|---|---|
| 영상 → 전신 애니메이션 | 플러그인 Editor 빌드, 전신 원본 콘텐츠, **고정 시드 카메라 모델**, FFmpeg, 대상 메시·IK Retargeter |
| 고속 전신 모드 | 위 항목 + fused CHMR, compact SAM2 모델 쌍. 없으면 해당 단계는 원본 경로 사용 |
| HandFlow 손 보강 | 독립 HandFlow Python, CUDA 가능한 NVIDIA GPU·드라이버, HandFlow/HaMeR/검출기 파일, 사용자 제공 MANO |
| WiLoR·MediaPipe 진단 | 선택 사항. 일반 설치에는 필요하지 않으며 [진단용 환경 설명](ThirdParty/README-Hands.md) 참조 |

HandFlow의 현재 구현은 `.cuda()`를 사용하므로 CPU 전용이나 AMD/Intel GPU 경로로 자동 전환되지 않습니다. CUDA를 사용할 수 없다면 우선 실행 옵션의 HandFlow를 해제합니다. 본 보정에도 Python 환경이 필요하므로 환경 미준비 시 본 보정도 해제합니다. 전신 추출의 UE NNE/DirectML 경로와 외부 Python HandFlow/CUDA 경로는 서로 다릅니다.

### 1. 소스·엔진·기본 도구

1. 이 폴더 전체를 대상 프로젝트의 `Plugins/CubelessBodyMocap/`에 둡니다. `.uplugin`과 `Source`, `Tools`, `Resources`, `ThirdParty`의 작은 설정·라이선스 파일을 함께 보존합니다.
2. UE 5.8.4와 해당 엔진이 지원하는 Visual Studio C++ 빌드 도구 및 Windows SDK를 준비합니다. DLL은 Git에 없으므로 해당 PC에서 **프로젝트 Editor 타깃**을 빌드해야 합니다.
3. [독립 Python 3.12](https://www.python.org/downloads/release/python-31210/) 64비트와 Git을 준비합니다. 3.12.10은 이 프로젝트의 검증 버전이며 최신 Python 버전이라는 뜻은 아닙니다. Unreal 내장 Python에 pip 패키지를 설치하지 않습니다.
4. 에디터의 Plugins 설정에서 `Cubeless Body Mocap`을 활성화하고 재시작·컴파일 안내를 따릅니다. 소스 설정의 `EnabledByDefault`는 `false`이고, 대상 `.uproject`의 활성화 변경은 이번 소스 배포에 포함되지 않았습니다. 의존 플러그인은 `NNERuntimeORT`, `IKRig`, `RigLogic`입니다. 뒤의 에셋 생성·모델 가져오기 단계에는 `Python Editor Script Plugin`도 활성화합니다. MCP는 필요하지 않습니다.

이하 **PowerShell** 명령은 `.uproject`가 있는 폴더에서 실행합니다. 경로가 다른 PC에서는 첫 두 경로를 바꿉니다. `&`는 공백이 있는 실행 파일 경로를 호출하는 PowerShell 연산자입니다. 아래 변수는 같은 PowerShell 창에서 계속 사용합니다.

```powershell
Set-Location 'D:/Projects/MyProject'  # 실제 .uproject가 있는 폴더
$CBMProject = (Get-Location).Path
$CBMEngine = 'C:/Program Files/Epic Games/UE_5.8'
$CBMPlugin = Join-Path $CBMProject 'Plugins/CubelessBodyMocap'
Get-ChildItem -LiteralPath $CBMProject -Filter *.uproject
py -3.12 --version
```

`py` 런처가 없다면 설치한 Python 3.12의 `python.exe` 전체 경로로 `py -3.12` 부분을 대체합니다. 기존 가상환경을 복사하거나 활성화한 상태로 진행하지 않습니다.

### 2. 전신 원본 모델·SMPL 콘텐츠 복원

준비 도구는 이용 권한이 있는 **MetaHuman Animator Markerless Motion Capture** 엔진 설치본에서 콘텐츠를 복사합니다. 준비 시 원본 데이터가 필요하지만, 복사 후 Cubeless 실행을 위해 원본 MetaHuman 플러그인을 프로젝트에서 활성화할 필요는 없습니다.

현재 도구가 찾는 원본 경로는 정확히 다음과 같습니다.

```text
<UE 설치 폴더>/Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.8/
  MetaHumanBodyTracker.uplugin
  Content/
```

Epic 설치 경로·버전이 다르면 자동 검색하지 않습니다. 설치 위치와 도구의 지원 버전을 먼저 확인하세요. 실행할 때 에디터는 닫아 두고, 복사 중인 에셋을 읽지 않도록 합니다.

```powershell
Test-Path "$CBMEngine/Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.8/MetaHumanBodyTracker.uplugin"
py -3.12 "$CBMPlugin/Tools/prepare_content.py" --engine "$CBMEngine"
```

도구는 `Content/Models/Offline`, `Content/Detectron2`, `Content/SAM2`, SMPL-X 데이터·메시·스켈레톤·IK Rig와 라이선스 고지를 준비합니다. 기존 파일을 덮어쓰지 않으며 기존 파일 검사는 크기 비교입니다. 이후 모델 변환 도구는 검토된 원본 SHA256을 추가 검사합니다. **원본 콘텐츠 복사만으로 설치가 끝나지는 않습니다. 다음 카메라 변환은 필수입니다.**

현재 PC의 원본·변환 모델을 포함한 플러그인 Content는 약 6.1GB입니다. 모델 이용 조건과 원본 고지는 유지됩니다. 필요한 파일을 Git에 강제로 추가하는 방식으로 해결하지 않습니다.

### 3. 모델 변환용 Python과 카메라·고속 모델 생성

이 환경은 ONNX를 준비할 때만 사용합니다. 전신 추출 실행 자체는 Unreal C++/NNE에서 이루어집니다. HandFlow 환경과 분리합니다.

```powershell
$CBMModelEnv = Join-Path $CBMProject 'Saved/CubelessBodyMocap/ModelTools/venv'
py -3.12 -m venv "$CBMModelEnv"
$CBMModelPy = Join-Path $CBMModelEnv 'Scripts/python.exe'
& $CBMModelPy -m pip install 'onnx==1.23.2' 'numpy==1.26.4'

# 필수: 원본/고속 양쪽이 사용하는 고정 시드 카메라 모델
& $CBMModelPy "$CBMPlugin/Tools/prepare_camera_seeded.py" --source "$CBMPlugin/Content/Models/Offline/camera_calib.uasset" --output "$CBMProject/Saved/CubelessBodyMocap/WristInvestigation/camera_calib_seeded.onnx"

# 고속 모드를 사용할 경우: CHMR와 SAM2 쌍도 생성
& $CBMModelPy "$CBMPlugin/Tools/prepare_chmr_fused.py" --source "$CBMPlugin/Content/Models/Offline/chmr_backbone.uasset" --output "$CBMProject/Saved/CubelessBodyMocap/CHMRModels/chmr_backbone_fused.onnx"
& $CBMModelPy "$CBMPlugin/Tools/prepare_sam_compact.py" --source-dir "$CBMPlugin/Content/SAM2" --output-dir "$CBMProject/Saved/CubelessBodyMocap/CompactSAM"
```

기존 ONNX/변환 기록이 있으면 중단하는 도구입니다. 이미 준비된 설치에 반복 실행하거나 해시 검사를 제거하지 않습니다. 원본 모드만 사용할 경우 마지막 두 변환은 생략할 수 있지만 `camera_calib_seeded`는 생략할 수 없습니다.

다음은 **PowerShell이 아니라 대상 프로젝트의 Unreal Editor Python 콘솔**에서 실행합니다. 플러그인을 빌드·활성화하고 에디터를 연 뒤 Output Log의 입력 모드를 Python으로 바꿉니다. `unreal` 모듈이 필요한 `import_*.py`를 외부 Python으로 실행하면 안 됩니다.

```python
import unreal
from pathlib import Path
cbm_tools = Path(unreal.Paths.project_dir()).resolve() / 'Plugins/CubelessBodyMocap/Tools'
exec((cbm_tools / 'import_camera_seeded.py').read_text(encoding='utf-8-sig'))
# 위에서 고속 모델도 생성했다면 각각 실행
exec((cbm_tools / 'import_chmr_fused.py').read_text(encoding='utf-8-sig'))
exec((cbm_tools / 'import_sam_compact.py').read_text(encoding='utf-8-sig'))
```

생성·저장되어야 할 Unreal 에셋 경로입니다. 기존 에셋이 있으면 import 스크립트도 덮어쓰지 않고 중단합니다.

- 필수: `/CubelessBodyMocap/Models/Offline/camera_calib_seeded`
- 고속: `/CubelessBodyMocap/Models/Offline/chmr_backbone_fused`
- 고속: `/CubelessBodyMocap/SAM2/sam2_image_encoder_compact`, `/CubelessBodyMocap/SAM2/sam2_mask_decoder_compact`

### 4. FFmpeg 준비

```powershell
py -3.12 "$CBMPlugin/Tools/prepare_ffmpeg.py"
& "$CBMPlugin/ThirdParty/FFmpeg/Win64/ffmpeg.exe" -version
```

`ThirdParty/FFmpeg/Win64/PROVENANCE.json`의 URL에서 내려받아 기록된 ZIP·실행 파일 SHA256을 검사합니다. 시스템 PATH 등록은 필요 없으며 UI가 위 플러그인 내부 경로를 기본 사용합니다. 실행 파일은 약 105MB이고 다운로드 ZIP·임시 공간은 별도로 필요합니다.

**배포 URL은 최신 파일을 가리키므로 시간이 지나면 해시가 달라질 수 있습니다.** `Upstream archive changed`가 나오면 임의의 최신 파일을 검증된 버전으로 취급하지 않습니다. 이용 조건을 충족하는 기존 설치본의 동일 `ffmpeg.exe`를 위 위치에 복사하고 준비 스크립트를 다시 실행하면 실행 파일 해시를 확인할 수 있습니다. 새 버전을 쓰려면 별도로 검증하고 provenance를 갱신해야 합니다.

### 5. HandFlow 모델 준비

손 보강이 필요 없으면 이 절과 다음 절을 생략하고 UI의 `Enhance Hands (HandFlow)` 체크를 해제합니다. 기본값은 체크 상태이므로 모델을 준비하지 않았다면 해제해야 합니다.

먼저 [공식 MANO 사이트](https://mano.is.tue.mpg.de/)에서 이용 조건에 동의하고 받은 `mano_v1_2`의 **`MANO_RIGHT.pkl`**을 준비합니다. `MANO_LEFT.pkl`, UV 파일, SMPL+H 다운로드는 이 손 경로에 필요하지 않습니다. MANO 파일은 자동 다운로드하지 않습니다.

```powershell
$CBMHandRoot = Join-Path $CBMPlugin 'ThirdParty/HandFlow'
New-Item -ItemType Directory -Force -Path "$CBMHandRoot/models" | Out-Null
# 새 설치에만 복사합니다. 이미 파일이 있다면 출처·해시부터 확인하세요.
if (Test-Path "$CBMHandRoot/models/MANO_RIGHT.pkl") { throw 'MANO 파일이 이미 있습니다. 덮어쓰지 말고 확인하세요.' }
Copy-Item -LiteralPath 'D:/Downloads/mano_v1_2/models/MANO_RIGHT.pkl' -Destination "$CBMHandRoot/models/MANO_RIGHT.pkl"
py -3.12 "$CBMPlugin/Tools/prepare_handflow.py" --weights
```

`manifest.json`에 고정한 리비전·SHA256으로 runtime 소스와 아래 파일을 준비합니다. MANO도 같은 manifest의 해시와 일치해야 합니다. MANO를 먼저 두지 않으면 다른 다운로드가 일부 완료되어도 준비 작업은 실패합니다.

| `ThirdParty/HandFlow/models/` 파일 | 역할 |
|---|---|
| `handflow_denoiser.pt`, `normalization_stats.npz` | HandFlow 시간축 모델·정규화 통계 |
| `hamer.ckpt`, `model_config.yaml`, `mano_mean_params.npz` | HaMeR 영상 특징·설정·평균 파라미터 |
| `detector.pt` | 손 영역 검출기. WiLoR 손 포즈 모델이 아님 |
| `MANO_RIGHT.pkl` | 사용자가 준비하는 MANO 손 형상 |

위 모델 파일 합계는 manifest 기준 약 **3.41GB**입니다. 현재 허용 MANO SHA256은 `45d60aa3b27ef9107a7afd4e00808f307fd91111e1cfa35afd5c4a62de264767`입니다. 다른 MANO 배포본이나 pickle이면 해시 검사를 우회하지 말고 호환성을 검토해야 합니다. 다운로드 실패 후에는 불일치 파일을 확인하고 원인을 해결한 뒤 다시 실행합니다. 일치하는 완료 파일은 재사용하며 `.partial`은 성공한 모델이 아닙니다.

### 6. HandFlow 독립 Python 환경

`prepare_handflow.py --weights`는 **Python 패키지를 설치하지 않습니다.** 새 PC에서는 아래처럼 HandFlow 자체 환경에 패키지를 설치합니다. 기존 PC의 `cbm_shared_torch.pth`는 다른 PC의 WiLoR 절대 경로를 가리키므로 복사하지 않습니다. 아래 방식은 WiLoR 가상환경·포즈 가중치 없이 패키지를 독립 설치하는 절차입니다. 기존 공유 환경의 완전한 신규 대체 설치는 아직 다른 PC에서 실행 검증하지 않았으므로 끝의 import/GPU 확인과 실제 짧은 추론을 모두 수행하세요.

```powershell
$CBMHandRoot = Join-Path $CBMPlugin 'ThirdParty/HandFlow'
if (Test-Path "$CBMHandRoot/venv") { throw '신규 환경용 절차입니다. 기존 venv는 먼저 상태를 확인하세요.' }
py -3.12 -m venv "$CBMHandRoot/venv"
$CBMHandPy = Join-Path $CBMHandRoot 'venv/Scripts/python.exe'

# 검증 PC의 Torch/CUDA 조합. 다른 조합은 별도 검증 대상입니다.
& $CBMHandPy -m pip install 'torch==2.8.0' 'torchvision==0.23.0' --index-url https://download.pytorch.org/whl/cu128
# 파일 이름은 WiLoR이지만 공통 Python 의존성 목록입니다. WiLoR 모델은 설치하지 않습니다.
& $CBMHandPy -m pip install --no-deps -r "$CBMPlugin/ThirdParty/WiLoR/requirements-hands.txt"
# HandFlow용 버전이 마지막에 적용되어야 합니다(ultralytics 8.3.80 등).
& $CBMHandPy -m pip install --no-deps -r "$CBMHandRoot/requirements-overlay.txt"
# 기존 lock에 빠져 있지만 Ultralytics가 선언한 의존성
& $CBMHandPy -m pip install --no-deps 'seaborn==0.13.2'
```

Torch 조합은 [공식 이전 버전 설치 안내](https://pytorch.org/get-started/previous-versions/#v280)를 참조했습니다. 고정 패키지 목록과 overlay 순서를 유지하기 위해 나머지는 `--no-deps`로 설치하며, 다음 `pip check`에서 누락·충돌이 없는지 반드시 확인합니다. [seaborn 0.13.2](https://pypi.org/project/seaborn/0.13.2/)는 기존 공유 환경의 `pip check`에서 발견한 누락을 새 설치 절차에 명시한 것입니다. 현재 사용 중인 환경은 이 문서 작업으로 변경하지 않았습니다.

MANO pickle에 필요한 구형 `chumpy`는 아래 고정 소스의 순수 Python 패키지를 설치합니다. 과거 pip 빌드 경로의 Python 3.12 호환 문제를 피하며, worker의 기존 NumPy/inspect 호환 처리가 함께 필요합니다.

```powershell
$CBMChumpy = Join-Path $CBMProject 'Saved/CubelessBodyMocap/SetupSources/chumpy'
git clone --no-checkout https://github.com/mattloper/chumpy.git "$CBMChumpy"
git -C "$CBMChumpy" checkout --detach 580566eafc9ac68b2614b64d6f7aaa84eebb70da
Copy-Item -LiteralPath "$CBMChumpy/chumpy" -Destination "$CBMHandRoot/venv/Lib/site-packages/chumpy" -Recurse
Copy-Item -LiteralPath "$CBMChumpy/LICENSE.txt" -Destination "$CBMHandRoot/venv/Lib/site-packages/chumpy/LICENSE.txt"

& $CBMHandPy -m pip check
& $CBMHandPy -c "import torch,torchvision,numpy,scipy,cv2,smplx,torchdiffeq,lmdb,dill; print(torch.__version__,torchvision.__version__); print('CUDA:',torch.cuda.is_available()); assert torch.cuda.is_available(); print(torch.cuda.get_device_name(0))"
```

`pip check`가 통과해도 모델·MANO 로딩까지 검증한 것은 아닙니다. `chumpy`는 호환 처리가 없는 별도 `import chumpy`만으로 검사하지 말고, 아래 UI에서 실제 짧은 HandFlow 작업까지 확인합니다. 현재 버전은 CUDA 필수이며 `CUDA: False`를 CPU 추론 성공으로 해석하지 않습니다. 가상환경·wheel 캐시의 추가 디스크 공간도 필요합니다.

### 7. Manny 메시·리타깃 프리셋 생성

Git에 리타깃 `.uasset`을 넣지 않았으므로 새 프로젝트에서 생성해야 합니다. Unreal의 Manny 콘텐츠를 준비해 다음 두 경로가 있는지 먼저 확인합니다. 다른 폴더에 둔 메시에는 현재 생성 스크립트를 그대로 실행하지 말고 경로를 맞추거나 직접 해당 캐릭터용 IK Retargeter를 구성합니다.

```text
/Game/Characters/UE5_Mannequins/Meshes/SKM_Manny
/Game/Characters/UE5_Mannequins/Rigs/IK_UE5_Mannequin_Retarget
```

원본 모델 복원이 끝난 뒤 **Unreal Editor Python 콘솔**에서 실행합니다.

```python
import unreal
from pathlib import Path
cbm_tools = Path(unreal.Paths.project_dir()).resolve() / 'Plugins/CubelessBodyMocap/Tools'
exec((cbm_tools / 'create_mannequin_preset.py').read_text(encoding='utf-8-sig'))
```

`/Game/CubelessMocap/Setup/`에 `IK_CBM_SMPL`, `IK_CBM_Manny`, `RTG_CBM_SMPL_to_Manny`를 저장합니다. 골반을 root로 덮어쓰지 않는 설정과 독립 발가락 체인을 포함합니다. 기존 동명의 에셋이 하나라도 있으면 생성하지 않으므로, 기존 설정을 확인하고 사용할지 판단합니다. 프리셋 생성 스크립트가 Manny 메시나 원본 모델까지 내려받는 것은 아닙니다.

### 8. 창에서 경로 확인 후 짧은 영상으로 점검

**Tools → Cubeless Body Mocap**에서 다음 항목을 확인합니다. 프리셋 생성 전에 창을 열었다면 메시·Retargeter를 직접 다시 선택합니다.

| UI 항목 | 값 |
|---|---|
| FFmpeg | `<Plugin>/ThirdParty/FFmpeg/Win64/ffmpeg.exe` |
| Target Mesh | `SKM_Manny` |
| Retargeter | `RTG_CBM_SMPL_to_Manny` |
| Enhance Hands (HandFlow) | 기본 체크. 모델이 준비되지 않았거나 원본 손을 원하면 해제 |
| HandFlow Python | `<Plugin>/ThirdParty/HandFlow/venv/Scripts/python.exe` |
| HandFlow Directory | `<Plugin>/ThirdParty/HandFlow` (`models` 하위 폴더가 아님) |

`영상 파일`을 지정하고 30 FPS, 2~5초로 **Start Mocap**을 실행합니다. 위의 **HandFlow**와 **본 보정**이 기본으로 켜져 있어 선택한 단계를 자동 처리합니다. 모델이나 Python이 준비되지 않았다면 해당 실행 옵션을 해제하세요. 완료 후 아래 저장 옵션과 이름을 확인하고 **애니메이션 시퀀스 저장**을 누릅니다. 실행 실패·취소 시 완료된 중간 결과와 기존 애셋은 보존합니다.

준비 상태 표시는 주로 파일 존재 검사이며 실제 Python import·CUDA·모델 호환성 검사를 대체하지 않습니다. `Completed`와 출력 애니메이션, 손 보존 기록, 영상과의 전체 재생을 확인합니다. 오류는 `Saved/CubelessBodyMocap/` 아래 해당 작업의 `worker.log`, `worker_status.json`, 전신 `profile.json` 및 에디터 로그를 확인합니다. 모델·중간 ONNX·빌드 결과 외에도 영상 프레임과 실행 캐시를 저장할 여유 공간이 필요합니다.

### 설치 오류 점검

| 증상 | 확인할 내용 |
|---|---|
| `Install the Epic markerless plugin...` | 원본 엔진 설치의 `MetaHumanBodyTracker_5.8` 경로·descriptor 존재 여부 |
| `Unreviewed source asset` / 모델 해시 불일치 | 검증한 원본 모델 버전인지 확인. 해시 검사를 제거하지 않음 |
| `camera_calib_seeded` 없음 | ONNX 생성과 Unreal import 두 단계 모두 수행했는지 확인 |
| `No module named unreal` | `import_*.py`/프리셋 스크립트를 외부 Python에서 실행한 경우. UE Python 콘솔 사용 |
| `Upstream archive changed` | FFmpeg 최신 URL의 내용 변경. 동일 검증 파일 확보 또는 새 버전 검증 필요 |
| `Missing ... MANO_RIGHT.pkl` | HandFlow `models` 아래 제공 여부 및 manifest 해시 확인 |
| HandFlow 파일은 Ready인데 import 실패 | HandFlow Python 경로, base 요구사항→overlay 순서, Chumpy, `pip check` 확인 |
| `CUDA: False` / CUDA 초기화 실패 | NVIDIA GPU·드라이버 및 CUDA Torch wheel 확인. HandFlow CPU fallback 없음 |
| CUDA 메모리 부족 | `HandFlow Batch`를 1로 낮추고 다른 GPU 작업 점유를 확인. 배치 1에서도 부족하면 중단하며 품질 축소를 자동 보장하지 않음 |
| 프리셋 없음/기본 Target이 비어 있음 | Manny 원본 콘텐츠와 7단계 생성 여부 확인. 창에서 경로 다시 지정 |

기존 PC의 가상환경·절대 경로 설정을 통째로 복사하는 대신 각 PC에서 환경을 다시 만듭니다. 모델·MANO·코드의 파일별 이용 조건은 [HandFlow 고지](ThirdParty/HandFlow/README.md#provenance-and-terms)와 `Resources/Licenses`를 따릅니다. 프로젝트 전체가 하나의 MIT 라이선스라는 뜻은 아닙니다.

---

이하에는 기능 설명과 기존 설치의 검증 이력을 보존합니다. 과거 벤치마크 JSON 및 프로젝트 `cubeless/studies` 자료는 소스 전용 배포에서 제외될 수 있으며, 신규 설치에 필수인 파일은 아닙니다.

일반 손 보강은 `None (original hands)`와 `HandFlow / HaMeR`만 선택하며 기본값은 HandFlow입니다. WiLoR·MediaPipe는 `Hands > Diagnostics > Enable Legacy Hand Diagnostics`를 켰을 때만 추론·캐시 재적용·비교에 사용합니다. 기존 애니메이션은 계속 열 수 있습니다. [모델 준비·선택·파일별 조건](ThirdParty/HandFlow/README.md)을 확인하세요. HandFlow의 손 겹침 관측 신뢰도와 회전 가속 보정으로 test2 5.5초 주변 과보정과 회전 튐이 감소했습니다. 7.03초 개선과 7.7초 제스처도 유지됐지만, 가려진 손의 정확한 형태나 손끼리의 관통까지 해결한 것은 아닙니다. 상세 수정 전후 비교는 프로젝트 `cubeless/studies/StandaloneBodyMocap/HandFlowOcclusion20261108.md`에 기록했습니다.

영상 근거를 이용한 선택형 다리 보정은 `Refine Body from Video (Experimental)`에서 켭니다. `Foot Locking`이 필요하며 기본값은 꺼짐입니다. test2·test3에서 과한 지지 다리 굽힘과 대부분의 미끄러짐 지표가 개선됐지만, 일부 발 관통·영상 오차는 증가했습니다. 모델 가중치나 관절 제한은 변경하지 않습니다. 비교와 한계는 프로젝트의 `cubeless/studies/StandaloneBodyMocap/BodyDepth20261108.md`를 참조합니다.

UE 5.8.4 CL58902052용 Windows Editor 전용 테스트 버전입니다. MetaHuman 원본 플러그인을 비활성화한 프로젝트에서 빌드했습니다. 에디터 플러그인 로딩과 Manny 프리셋 생성·저장을 확인했습니다. 추론 모델 로딩과 기존 영상의 8프레임 추출·저장을 확인했습니다. 이후 동일한 10초 전체 클립의 속도·메모리·에디터 응답도 재측정했습니다. 시각적 동작 품질 비교와 패키징은 추가 검증 대상입니다.

## 설치 완료 후 테스트 준비

1. 에디터를 열고 **Tools → Cubeless Body Mocap**을 선택합니다.
2. FFmpeg 경로, Target Mesh(Manny), Retargeter가 자동 선택됩니다.
3. 동영상 파일을 선택하고 2~5초 구간, 30 FPS로 시작합니다. `Fast Mode (Off: Original)`을 켜면 고속, 끄면 원본 계산 경로를 사용합니다.
4. **Start Mocap**을 누릅니다. 동일 10초 영상의 추출·저장은 확인했으며, 동작 품질은 생성한 애니메이션을 재생해 확인합니다.

준비된 프리셋은 `/Game/CubelessMocap/Setup/RTG_CBM_SMPL_to_Manny`입니다. 원본 IK Rig를 수정하지 않고 전용 소스·대상 IK Rig를 복제했으며 22개 체인 매핑, 5개 기본 retarget op, 대상 기준 자세 자동 정렬을 저장했습니다. 같은 UE5 스켈레톤의 Quinn을 쓰려면 Target Mesh를 Quinn으로 바꿀 수 있으나 Quinn에 대한 실제 결과는 미검증입니다.

## 입력

- `Video`: 동영상 파일입니다. `FFmpeg`는 플러그인 내부 `ThirdParty/FFmpeg/Win64/ffmpeg.exe`가 자동 선택됩니다. 시스템 설치나 PATH 설정은 필요 없습니다.
- `Start Seconds`, `Duration Seconds`, `Frame Rate`: 추출 구간과 샘플링 FPS입니다. 첫 테스트는 30 FPS를 권합니다. 다른 FPS에서의 품질은 미검증입니다.
- FFmpeg 대신 `Frame Directory`에 PNG 시퀀스 폴더를 지정할 수 있습니다. 이 경우 Video·Start·Duration은 사용하지 않고 폴더 전체를 Frame Rate 기준으로 처리합니다. 00000000.png처럼 같은 길이의 순번 파일명을 사용하며 프레임 크기를 일정하게 유지합니다.
- 예: `ffmpeg -i input.mp4 -t 5 -vf fps=30 -start_number 0 frames/%08d.png` (frames 폴더를 먼저 만듭니다).

## 출력

- Manny와 준비된 Retargeter가 기본 선택됩니다. 다른 캐릭터에는 해당 캐릭터용 리타기터를 지정합니다.
- Output Folder는 `/Game/` 아래를 지정합니다. 매 실행 고유 Take 폴더를 만들어 기존 결과를 덮어쓰지 않습니다.
- **Start Mocap**으로 계산 후 **애니메이션 시퀀스 저장**을 누릅니다. 최종 애셋은 지정한 폴더·이름으로 저장하며, 내부 계산 결과는 별도 작업 폴더에 남습니다.
- 생성한 애니메이션을 열어 대상 캐릭터에서 미리보기·재생합니다. 발 미끄러짐, 몸 방향, 크기, 루트 이동과 영상 마지막 프레임을 확인합니다.
- 대상 캐릭터·Retargeter는 수정하지 않습니다. 저장 실패나 취소 시 이미 생성한 출력이 일부 남을 수 있습니다. 오류 메시지와 폴더를 확인한 뒤 다시 실행합니다.

## 처리와 제한 (2026-11-07 비동기 최적화 적용)

검출/추적 → 관절 추정 → CHMR/Hue 추론 → 바닥/카메라 변환 → 발 고정 optimizer → SMPL-X 변환 → 표준 IK 리타기팅·저장을 연결했습니다. 메타휴먼 얼굴·Creator·Performance 에셋과 MCP는 사용하지 않습니다. UE 일반 플러그인 NNERuntimeORT·IKRig·RigLogic은 필요합니다. RigLogic은 vendor optimizer의 메모리/입출력 유틸리티를 제공합니다.

추론·보정·PNG 디코딩·뼈 트랙 계산은 전용 백그라운드 스레드에서 순차 실행합니다. UObject 로드와 참조 관리, 에셋 생성·리타기팅·저장은 게임 스레드에서만 실행합니다. 모델 패키지는 비동기 로드하고, NNE의 일반 모델 핸들을 게임 스레드에서 준비한 뒤 실제 세션 생성과 추론을 작업 스레드에서 실행합니다.

검출기 → SAM2 추적기 → ViTPose → CHMR → Hue 순서로 세션을 교체합니다. 단계가 끝나면 사용이 끝난 자체 모델 패키지를 명시적으로 언로드합니다. 더티 상태이거나 별도 에셋 편집기에서 열린 모델은 자동 언로드하지 않습니다. 사용자 에셋이나 Undo 기록을 초기화하지 않습니다.

**Start Mocap**은 작업을 시작하고 즉시 돌아옵니다. 창 하단에 단계·진행률·시간·에디터 프로세스 메모리 피크가 표시됩니다. **Cancel**은 취소를 요청합니다. 진행 중인 단일 ORT 세션 생성/추론 호출은 중간에 강제 종료할 수 없으므로 반환을 기다린 뒤 중단합니다. Hue는 각 denoising step 사이, 보정기는 목적함수 평가 사이에서도 취소를 확인합니다. 최종 에셋 저장·GC·로드 완료 처리에는 짧은 메인 스레드 대기가 남을 수 있습니다.

동시 작업과 PIE/SIE 중 시작을 거절합니다. 실행 중 에디터 닫기를 요청하면 먼저 취소를 요청하며, 작업이 정리된 뒤 다시 닫아야 합니다. 강제 종료나 DLL 동적 언로드는 사용하지 않습니다.

DML GPU 모델을 우선 사용하며, DML 모델을 만들 수 없는 환경에서는 CPU 모델을 준비합니다. 이미 준비된 GPU 모델의 세션 생성 자체가 실패하면 작업은 오류로 중단합니다. Hue final step은 정확도를 위해 CPU를 유지합니다. 모델 정밀도·50회 Hue 스텝·보정 반복 한도·기본 FPS를 낮추지 않았습니다.

`Reuse Checkpoints`를 켜면 프레임 내용·FPS·알고리즘 버전으로 구분한 검출/관절/특징/시계열 추론 캐시를 사용합니다. 영상 구간과 FPS가 같아 생성된 PNG 내용이 일치하면 다른 캐릭터에 다시 적용할 때도 추론을 재사용합니다. 발 고정·대상 캐릭터 리타기팅은 다시 실행합니다. 캐시는 `Saved/CubelessBodyMocap/Cache` 아래에 있으며 필요하면 사용자가 삭제할 수 있습니다. 실행별 시간·RAM 측정은 `Saved/CubelessBodyMocap/<run-id>/profile.json`에 저장됩니다. RAM 값은 에디터 프로세스 전체의 물리 메모리이며 GPU 전용 메모리와 다릅니다.

Python/API에서 `CubelessMocapLibrary.extract(settings)`의 성공은 **시작 접수**를 뜻합니다. `get_job_status()`로 Completed/Failed/Cancelled를 확인하고, `cancel()`로 취소합니다.

디코딩한 영상 프레임은 `Saved/CubelessBodyMocap/<id>/Frames`에 보존합니다. 필요할 때 사용자가 삭제할 수 있습니다.

## 모델 설치와 원본 코드

현재 프로젝트에는 모델과 SMPL-X 데이터가 복사되어 있습니다. Git에서는 약 5 GiB의 Content를 제외했습니다. 다른 설치에서 복원하려면 이미 Epic Markerless 플러그인이 설치된 엔진을 지정합니다.

```powershell
python Plugins/CubelessBodyMocap/Tools/prepare_content.py --engine 'C:/Program Files/Epic Games/UE_5.8'
```

원본 모듈 이름으로 저장된 SMPL-X/SAM2 자료형과 일부 패키지 참조는 시작 시 Core Redirect로 자체 모듈·복사본에 연결합니다. 이 프로젝트에서는 원본 MetaHuman 플러그인을 비활성화한 상태에서 복사된 모델 로딩과 전체 영상 추출을 확인했습니다. 모델과 기존 에셋을 새 자체 형식으로 재저장하는 영구 이관은 후속 정리 대상입니다.

Epic Games 소스를 수정·분리해 재사용했습니다. 원본 저작권 고지와 `Resources/Licenses`를 보존했습니다. 에픽 코드·가중치 이용 조건은 계속 적용되며 공개 재배포 권한까지 검증한 버전은 아닙니다.

## 초기 준비 단계 검증 상태

- UE 5.8.4 Editor 빌드 성공, 새 4개 모듈 컴파일·링크 성공.
- 빌드 영수증 BuildPlugins에 MetaHuman 이름의 원본 플러그인 없음.
- 원본 MetaHuman 플러그인이 비활성화된 에디터에서 새 플러그인 로딩, 복사된 SMPL 메시·IK Rig 로딩, Manny Retargeter 생성·저장 성공.
- 기본값 자동 선택 코드의 Editor 빌드 성공. FFmpeg 실행 및 배포본 SHA256 일치 확인.
- 추론 모델 로딩·기존 영상 8프레임 추출·애니메이션 저장/조회: 실행 확인. 원본 전체 클립 재실행·시각적 품질 비교·패키지 실행: 미실행.
- 최적화 요청에 따라 기존 영상의 8프레임만 최소 측정했습니다. 원본 300프레임 전체 반복·시각적 품질 비교·패키징·독립 C++ 검수는 수행하지 않았습니다.

## 포함된 FFmpeg

[FFmpeg 공식 다운로드 페이지](https://ffmpeg.org/download.html)에서 안내하는 [gyan.dev Windows builds](https://www.gyan.dev/ffmpeg/builds/)의 9.0.2 Essentials 배포본입니다. ffplay·ffprobe와 기타 도구는 포함하지 않았습니다. ffmpeg.exe의 논리 크기는 100.5 MiB이며 이 PC의 NTFS LZX 압축 후 디스크 사용량은 약 36.4 MiB입니다. 압축 파일을 다른 드라이브로 복사하면 논리 크기만큼 공간이 필요할 수 있습니다. 최소 기능으로 직접 재빌드한 바이너리는 아닙니다.

원본 라이선스·README와 SHA256은 `ThirdParty/FFmpeg/Win64`에 있습니다. FFmpeg는 별도 프로세스로 실행합니다. 실행 파일은 Git에서 제외했습니다. 다른 PC에서 동일 바이너리를 준비하려면 `Tools/prepare_ffmpeg.py`를 실행합니다. 이 스크립트는 기록된 SHA256과 일치하는 배포본만 설치하며 기존 파일을 덮어쓰지 않습니다.

## 초기 비동기 최적화 8프레임 검증

같은 개발 프로젝트에서 기존 영상의 8프레임을 사용한 최소 실행 검증입니다.

| 항목 | 결과 |
| --- | --- |
| 캐시 없이 모델 준비·추론·저장 | 37.28초 |
| 추론 캐시 재사용 후 보정·저장 | 1.90초 |
| 표본 중 메인 스레드 MCP/API 최대 왕복 | 0.80초 |
| 에디터 프로세스 물리 메모리 피크 | 약 11.6 GiB |
| GPU 전체 사용 메모리 표본 최대 | 약 4.93 GiB (다른 앱 포함) |
| 완료 후 남은 NNE 모델 에셋 | 0개 |
| 큐/로드 단계 취소 | 약 0.33초, 새 추출 자동 재시도 없음 |

8프레임 출력은 Manny 스켈레톤의 약 0.233초 Animation Sequence로 생성·저장·조회했습니다. 에디터 미저장 패키지는 없었습니다. 원본 300프레임의 38분 실행에는 단계별 기록이 없어 구간별 병목 비율을 소급 측정할 수 없습니다. 이 8프레임 검증 시점에는 전체 재실행을 하지 않았습니다. 이후 전체 재측정은 아래 절을 참조합니다. 시각적 품질 비교, 패키징, 독립 C++ 검수는 미실행/대기입니다. 따라서 위 짧은 시험을 10초 영상의 배속으로 환산하지 않습니다.

근거: `OPTIMIZATION_VALIDATION.json` 및 프로젝트의 `cubeless/records/verification/cbm-optimization-measurements-20261107.json`.

## 동일 10초 전체 재측정 결과

2026-11-07, 원본과 PNG 300장 전부의 SHA-256이 일치하는 입력에서 추론 체크포인트를 끄고 새 계산했다. 약 38분27초 → 7분3초, 약 5.45배였다. 에디터 RAM 피크 11.66GiB, 완료 후 4.54GiB, 에디터 GPU 전용 표본 최대 4.68GiB, 공유 표본 최대 2.57GiB였다. 메인 스레드 API 80회 측정의 최대 응답은 0.69초, Slate 최대 틱 공백은 1.23초였다. 300/300 프레임 추적과 저장에 성공했다. 기준 실행 시간은 파일 시각으로 복원한 근삿값이며, 이전 메모리 값은 피크 기록이 아니므로 정확한 감소율을 주장하지 않는다. 현재 엔진/OS 캐시는 warm 상태다. 자세한 근거는 `FULL10_BENCHMARK.json`과 프로젝트의 `cubeless/studies/StandaloneBodyMocap/Full10Benchmark20261107.md`에 있다.


## CHMR 통합 Attention (2026-11-07)

`Fast Mode (Off: Original)`은 **기본으로 켜져 있습니다**. Python 설정 이름은 `use_fused_chmr`입니다. 손목 차이를 후속 조사한 결과, 카메라 모델의 난수 시드가 지정되지 않아 실행 세션마다 카메라 조건이 달라지는 문제를 발견했습니다. 원본/고속 양쪽에 고정 시드 카메라 보정을 적용했습니다. 원본 Backbone의 24개 Attention 블록에서 MatMul → Softmax → MatMul을 DirectML의 통합 MultiHeadAttention 연산으로 교체합니다. 원본 가중치, 혼합 정밀도, Q/K 사전 스케일링, 896×896 입력, 모든 프레임, Head, Hue 50회와 보정 설정을 유지합니다. 연산을 통합하면서 반올림 차이는 발생할 수 있습니다. 옵션을 끄면 동일한 고정 카메라 조건의 원본 CHMR을 사용합니다. 생성한 동작의 시각적 품질은 영상과 함께 확인합니다.

최적화 모델 또는 DirectML 런타임이 없으면 원본 모델을 선택합니다. 선택한 모델은 실행별 `profile.json`의 `chmr_model`에 기록됩니다. 두 모델의 체크포인트는 분리되며, 현재 v4_seeded_camera는 이전 v2/v3 카메라·추론 체크포인트를 사용하지 않습니다. 카메라 초점거리/pitch/roll과 추적/키포인트/토큰 해시도 profile에 기록합니다. `CHMR_Preprocess`, `CHMR_Backbone`, `CHMR_Head` 시간은 `CHMR` 전체 시간에 포함된 세부 측정값이므로 합계에 중복해서 더하지 않습니다.

현재 프로젝트에는 `/CubelessBodyMocap/Models/Offline/chmr_backbone_fused`가 준비되어 있습니다. 다른 PC에서는 별도 Python 환경에 `onnx==1.23.2`와 numpy를 설치하고 `Tools/prepare_chmr_fused.py --source <원본 chmr_backbone.uasset 또는 .onnx> --output <프로젝트>/Saved/CubelessBodyMocap/CHMRModels/chmr_backbone_fused.onnx`를 실행한 뒤 같은 프로젝트의 Unreal Python에서 `Tools/import_chmr_fused.py`를 실행합니다. 변환 도구는 검토한 원본 SHA-256만 허용하고 원본과 기존 출력은 덮어쓰지 않으며 provenance를 기록합니다. Python/ONNX 변환 도구는 준비 단계에만 필요하고 추출 실행에는 필요하지 않습니다.

전체 10초·300프레임을 체크포인트 없이 처리한 고속 후보는 **300.91초(5분1초)**로, 직전 원본 모드의 423.17초보다 **28.9% 단축**되었습니다. CHMR만 보면 280.11초 → 145.31초(1.93배)입니다. 그러나 161개 본 × 300프레임 비교에서 왼쪽 손목 최대 27.09°, 오른쪽 손목 최대 9.46° 차이가 있었습니다. 주요 25개 본의 위치 차이는 평균 0.52cm/최대 1.41cm였지만 이것으로 회전 품질 동등성을 보증하지 않습니다. 메모리 피크는 11.83GiB로 이전 11.66GiB와 비슷합니다. 상세 결과와 제외한 후보는 프로젝트의 `cubeless/studies/StandaloneBodyMocap/CHMROptimization20261107.md`, 수치는 `CHMR_BENCHMARK.json`을 참조합니다.


## 카메라 보정 재현성 수정

실행 시 `/CubelessBodyMocap/Models/Offline/camera_calib_seeded`가 필요합니다. 현재 프로젝트에는 설치되어 있습니다. 다른 PC에서는 `prepare_content.py`로 원본을 준비한 뒤 다음 변환을 실행하고, 같은 프로젝트의 Unreal Python에서 `Tools/import_camera_seeded.py`를 실행합니다.

```powershell
python Plugins/CubelessBodyMocap/Tools/prepare_camera_seeded.py --source Plugins/CubelessBodyMocap/Content/Models/Offline/camera_calib.uasset --output Saved/CubelessBodyMocap/WristInvestigation/camera_calib_seeded.onnx
```

변환은 두 RandomUniformLike의 seed만 0/1로 고정합니다. 특정 영상에 맞춘 seed 탐색, 가중치·정밀도·해상도 변경은 없습니다. 원본 에셋은 보존하며 기존 목적지를 덮어쓰지 않습니다. 준비용 Python 도구는 추출 실행 의존성이 아닙니다. 모델이 없으면 설치를 완료해야 하며 무작위 원본으로 조용히 되돌리지 않습니다.

이전 27.09° 비교에는 서로 다른 카메라 조건이 섞여 있었으므로 CHMR 통합 Attention만의 오차로 해석하지 않습니다. 같은 추적·키포인트·카메라를 공유한 진단 비교에서는 최종 회전 최대0.6913°였습니다. 최종 전체 재측정과 상세 원인은 프로젝트의 `cubeless/studies/StandaloneBodyMocap/WristStability20261107.md`를 참조합니다. 과거 수치 기록은 당시의 관측으로 보존합니다.


고정 카메라 적용 후 같은10초·300프레임을 체크포인트 없이 다시 측정한 결과는 원본427.06초→고속294.66초(31.0%단축)입니다. 동일 카메라 조건에서 손목 최대 회전 차이는 왼쪽1.28°/오른쪽0.56°,전체 본 평균0.010°였습니다. 고속 RAM피크11.59GiB,API최대0.415초,Slate최대공백1.191초였습니다. 시각적 품질은 미검증이며 원본 결과와 수치상 완전히 같지는 않습니다. 현재 수치와 지문은 `WRIST_STABILITY_BENCHMARK.json`에 있습니다.


## 원본/고속 선택 및 SAM2 전송 최적화

`Fast Mode (Off: Original)`을 끄면 원본 CHMR·SAM2 모델을 사용하고, 켜면 통합 CHMR Attention과 compact SAM2 모델 쌍을 사용합니다. Python 속성 `use_fused_chmr`은 호환성을 위해 유지합니다. 원본은 비교 기준 계산 경로이며 실제 영상의 정답 동작이나 더 높은 정확도를 보증한다는 뜻은 아닙니다. 양쪽 모두 고정 시드 카메라를 공유하고 원본 가중치, 해상도, 전체 프레임, Hue 50스텝과 발 고정 설정을 유지합니다.

Compact SAM2는 mask decoder의 기존 두 고해상도 Conv를 image encoder 출력 직전으로 옮깁니다. 동일한 가중치·FP32 연산으로 계산한 결과만 CPU로 보내고, 사용하지 않는 위치 인코딩 출력 두 개를 제외합니다. Encoder 출력 전송은 프레임당 168MiB에서 20MiB로 줄어듭니다. 메모리 bank의 중복 복사와 배열 전치 접근을 줄이고, 고속 모드의 PIL resize는 독립적인 32행 묶음으로 계산합니다. 각 픽셀의 정수 합산 순서와 두 패스 사이의 대기는 유지합니다. 원본 모델 파일은 덮어쓰지 않습니다.

다른 PC에서 준비할 때는 앞서 안내한 원본 모델 설치 후 다음을 실행하고, 같은 프로젝트의 Unreal Python에서 `Tools/import_sam_compact.py`를 실행합니다.

```powershell
python Plugins/CubelessBodyMocap/Tools/prepare_sam_compact.py --source-dir Plugins/CubelessBodyMocap/Content/SAM2 --output-dir Saved/CubelessBodyMocap/CompactSAM
```

변환 도구는 검토한 원본 해시만 허용하고 생성 모델 해시와 가중치 바이트 일치를 확인합니다. 기존 목적지는 덮어쓰지 않습니다. `SAM_COMPACT_PROVENANCE.json`에 근거를 기록했습니다. 추출 실행 시 Python/ONNX 변환 환경은 필요 없습니다. Compact 모델이 없으면 해당 SAM2 단계만 원본으로 동작합니다. 모델이 잘못 연결된 경우 작업 초기화 오류로 중단합니다.

캐시는 계산 경로를 구분하는 v5 키를 사용합니다. 진단용 `cbm.CompactSAM 0`은 다음 고속 작업에서 이전 SAM2/직렬 전처리를 선택하고, `1`은 새 고속 경로를 선택합니다. 일반 사용자는 창의 원본/고속 체크박스만 선택하면 됩니다. 에디터 렌더링·전원·영상 FPS 설정은 자동으로 변경하지 않습니다.

최종 동일 영상 비교는 기존 고속 292.90초 → 개선 고속 282.27초(3.63% 단축)입니다. 원본은 468.04초였습니다. 기존 고속과의 단계별 수치/최종 애니메이션 차이는 `THROUGHPUT_BENCHMARK.json`, 상세 경위는 프로젝트의 `cubeless/studies/StandaloneBodyMocap/ThroughputOptimization20261107.md`를 참조합니다.


## Manny 골반 수정 (2026-11-08)

골반을 root로 덮어쓰던 프리셋과 본 내보내기의 SMPL 높이 보정 누락을 수정했습니다. 동일 영상 300프레임 재생성 후 골반 회전 및 검사한 13개 본의 기준 로컬 위치를 확인했습니다. 기존 원본/고속 상대 비교에는 이 공통 오류가 있었으므로 절대적인 리깅 품질 검증으로 해석하지 않습니다. 기존 결과는 자동 변경되지 않습니다. 자세한 원인·수치·새 결과 경로는 프로젝트의 `cubeless/studies/StandaloneBodyMocap/PelvisFix20261108.md`를 참조합니다.


## 영상과 애니메이션 동기 비교

도구 창의 왼쪽은 Video, 오른쪽은 실제 언리얼 Target Mesh/Preview Animation입니다. `Load comparison`으로 영상을 열고, 추출 완료 후 자동 연결된 결과를 `Play/Pause`, `Stop`, 공통 탐색 막대로 비교합니다. 기존 결과는 Preview Animation에서 선택할 수 있습니다. 영상의 재생 시간이 애니메이션 포즈를 구동하고, 두 뷰는 영상 비율을 따라 배치됩니다. Video 파일을 바꾸면 Load comparison을 다시 누릅니다. 음성 출력은 포함하지 않습니다. 구현과 test2 검증 근거는 프로젝트의 `cubeless/studies/StandaloneBodyMocap/ComparisonViews20261108.md`를 참조합니다.


비교 카메라는 추출 좌표계의 원본 카메라 방향(+Y)을 기본으로 사용합니다. 전신 자동 맞춤, Camera yaw/Zoom, Reset camera를 제공합니다. 단색 배경으로 동작을 비교하며 재생 중 매 프레임 몸을 따라 카메라를 재중앙화하지 않습니다. 방향·크기는 근사 비교용이고 원본 인물의 체형 및 손가락까지 동일한 결과를 보증하지 않습니다. 후속 시각 검토는 `cubeless/studies/StandaloneBodyMocap/ComparisonCameraReview20261108.md`에 기록했습니다.

## 손 모델의 원본 영상 대조 (2026-11-08)

MediaPipe는 기존 크롭에서 수용할 손이 없을 때만 더 넓은 크롭으로 한 번 재시도합니다. 같은 손목/좌우 배정 검사를 거치며 기존 성공 검출을 우선합니다. 공통 시간 필터는 앞뒤 자세가 서로 비슷한 단발성 튐만 제거하여 빠른 자세 전환을 결측으로 잘못 보간하지 않게 했습니다. 관절 제한은 추가하지 않았습니다.

test2에서 기존 검출245개를 그대로 보존하고68개의 손/프레임 표본을 추가했습니다. 손 worker 시간은12.60→17.71초로 늘었습니다. 이는 검출 범위 개선이며 자세 정답의 보증은 아닙니다. 3.2초 왼손 누락, 겹치는 양손의 불확실성, 7.7초 WiLoR 왼손 새끼손가락 오추정이 남습니다. WiLoR worker 설정에 `diagnostics:true`를 지정하면 필터 전 MANO 메시와 투영 자료를 `model_evidence.npz`에 저장하며 일반 실행은 이 자료를 만들지 않습니다. 원본 영상·관절·실제 MANO 메시 비교 및 수정 결과는 `cubeless/studies/StandaloneBodyMocap/ModelReview20261108.md`를 참조합니다.

후속 개선은 `hand_observations.py`의 반사/회전 재검출과 좌우 정체 검사·겹침 추적을 사용합니다. 위 수치는 이전 단계 기록입니다. test2의3.2초 관측을 복구하고5.5초 잘못된 관측을 제외해 검지 돌출을 줄였으나, 불확실한 손은 더 보수적으로 거절하므로 전체 검출 개수는 줄 수 있습니다. 학습 가중치나 관절 제한을 변경하지 않습니다.

WiLoR는 MediaPipe 환경/모델이 설치되어 있으면 독립 관측을 자동 생성하고 기본 padding2.0과 후보1.6의 영상 관절 일치도를 비교합니다. 선택한 회전은 여전히 WiLoR 출력입니다. 관측이 없으면 기본 크롭을 유지합니다. 외부 worker 설정의 `adaptive_crop:false`는 기본 크롭 경로를 사용하며, `landmark_reference`는 동일 입력임을 검증한 관측 JSON 캐시를 지정합니다. 이 검사는 추가 처리 비용이 듭니다. test2 손 단계의 이번 측정은 MediaPipe57.28초, 참조를 재사용한 WiLoR123.51초였으며 전체 추출 시간은 아닙니다. 실제 모델/언리얼 전후 비교와 남은 가림·충돌 문제는 `cubeless/studies/StandaloneBodyMocap/ModelRefine20261108.md`에 기록했습니다.


## 골반·다리 추가 보완 제거 (2026-11-09)

다른 영상에서의 자세 왜곡 때문에 `Refine pelvis / legs`, 접지·시간축 추가 보완, 캐시 적용 및 전용 LowerBody Python 환경을 제거했습니다. 신규 설치에는 하체 전용 Python이 필요하지 않습니다. 기존 애니메이션 에셋·테스트 기록은 삭제하지 않으며 Preview Animation에서 직접 열 수 있습니다. 전신 추출의 기본 foot locking 및 HandFlow 손 경로는 유지합니다. 동기화 뷰의 영상 카메라·프레이밍은 전용 `Preview_*.f32`로 저장하고 이전 `FootMR_*` 카메라 파일도 읽습니다.

## Manny 손바닥 체인 중복 매핑 수정 (2026-11-09)

`create_mannequin_preset.py`는 Manny의 `*Metacarpal` 8개 체인을 명시적으로 미매핑 처리합니다. 기본 자동 매핑이 SMPL 손가락 체인을 손바닥뼈에도 연결해, 손 보강을 끈 전신 결과에 손가락 움직임이 중복 적용되는 문제를 막습니다. 기존 프리셋을 사용하는 PC에서는 IK Retargeter의 해당 8개 체인 Source Chain을 `None`으로 지정하고 저장합니다. 기존 애니메이션은 자동으로 바뀌지 않습니다.

HandFlow 적용 경로는 이미 같은 중복 매핑을 차단하고 있습니다. 이번 수정은 블러·가림 구간의 손 추정 정확도를 높이는 변경이 아니며 관절 제한도 추가하지 않습니다. 현재 작업 PC의 기본 프리셋에는 수정이 적용됐습니다. test5 403프레임의 회전 전달 및 42포즈 매핑 비교 결과는 프로젝트 검증 기록 `cubeless/records/verification/hand-retarget-20261109/report.md`를 참조합니다.


### 단일 시작과 품질 유지 최적화

- 기본 UI에는 **Start Mocap**과 **Cancel**이 있습니다. **Solve > Enhance Hands (HandFlow)**는 기본 체크이며, 처리 중 설정은 잠깁니다. 전신 단계와 손 단계를 한 비동기 작업에서 순서대로 처리하므로 다시 시작 버튼을 누를 필요가 없습니다.
- 전신에서 디코딩한 원본 PNG를 손 단계가 그대로 재사용합니다. 전신의 사람 영역 ViTPose와 손의 전체 화면 ViTPose는 입력 영역이 다르므로 합치지 않습니다. 모델, 해상도, 프레임 수, HandFlow steps/batch/seed, 회전 변환·리타깃·접촉 보정은 변경하지 않았습니다. 관절 제약도 추가하지 않았습니다.
- `Reuse Checkpoints`를 켜면 손용 전체 화면 ViTPose 결과를 `Saved/CubelessBodyMocap/HandCueCache`에 별도로 저장합니다. PNG 내용·FPS·모델 파일·관련 실행 모듈 내용·엔진 버전이 같은 경우에만 검사한 수치 데이터를 재사용합니다. 최초 실행은 기존 손 추론을 수행하며, 캐시 효과는 같은 입력의 반복 실행에 해당합니다. 하드웨어/드라이버를 바꾼 후 수치를 새로 비교하려면 `Reuse Checkpoints`를 끄십시오.
- HandFlow의 기존 `HandCache`와 전신 `Cache`는 유지됩니다. 원본/손 결과 전환은 비교 뷰에서 가능합니다. 기존 에셋과 JSON은 삭제하지 않습니다. 진단용 실행 API는 유지하며 진단 UI는 제거했습니다. 진단 선택은 **Start Mocap**의 HandFlow 체크박스를 덮어쓰지 않습니다.
- Python에서 `extract(settings)`도 같은 자동 흐름을 사용합니다. 전신만 필요한 기존 자동화는 `enhance_hands=False`를 명시하십시오. 기존 `refine_hands` / `apply_hand_result` API는 진단·재적용용으로 유지됩니다.
- **Reuse Checkpoints**를 끄면 전신 중간 결과, 손 검출, 외부 HandFlow 결과 캐시의 읽기·쓰기를 모두 생략합니다. 기존 캐시 파일은 삭제하지 않습니다. `metrics.json`의 `cache_reuse_enabled=false`, `cache_hit=false`로 손 단계의 실제 재계산 여부를 확인할 수 있습니다.

## 선택형 손가락 관절 후처리

현재는 Start Mocap 위의 본 보정 체크가 기본 켜짐이며 자동 실행합니다. 아래는 독립 후처리 API와 알고리즘의 이전 검증 기록입니다.

1. `Postprocess > Enable Joint Correction`을 켭니다. 기본값은 해제입니다.
2. `Joint Correction Source`를 비우면 현재 HandFlow 결과를 사용하고, 없으면 Preview Animation을 사용합니다. 저장된 이전 Manny 결과를 직접 지정할 수도 있습니다. 해당 결과와 같은 Target Mesh/Retargeter를 지정해야 합니다.
3. `Joint Correction Strength`를 조절하고 `Apply joint correction (optional postprocess)`를 누릅니다. 1은 전체 보정, 중간값은 부분 적용이며 범위 이탈이 남을 수 있습니다. 0은 원본을 선택하고 새 에셋을 만들지 않습니다.
4. `Before correction`과 `Joint corrected`로 같은 재생 시점에서 비교합니다. 원본은 수정하지 않으며 결과는 Output Folder의 `JointCorrection`에 `_JointCorrected_` 접미사로 저장됩니다. 반복 적용은 보정 전 소스를 사용하고, 보정 결과에 다시 누적하는 것은 거절합니다.

손가락 30개 본의 **로컬 회전만** 수정합니다. 나머지 131개 Manny 본(전신, 손목, metacarpal 포함), 모든 본의 위치·스케일, 프레임 수와 FPS는 저장 전에 원본과 정확히 대조합니다. 추가 학습이나 모델 추론, WiLoR/MediaPipe 혼합, 발·골반 보완은 수행하지 않습니다.

`run_joint_post.py`, `joint_constraints.py`, `joint_contact.py`는 기존 HandFlow Python의 NumPy/SciPy만 사용합니다. 추가 모델·Python 환경이 필요 없으며 GPU 추론도 없습니다. 외부 비동기 작업과 기존 Cancel을 사용합니다. Python/스크립트가 없으면 원본을 보존하고 필요한 경로를 안내합니다.

회전은 현재 Manny 결과를 기존 손 리타깃 변환으로 역산해 중립 SMPL 손의 공통 회전으로 복원합니다. 역변환→정변환 오차가 0.01°를 넘으면 중단합니다. 좌우 손, index/middle/pinky/ring/thumb 순서, 관절별 중립 축을 명시적으로 사용하며 이미 적용된 평균 자세·손바닥·말단 축 보정을 중복 적용하지 않습니다. 엄지는 손바닥 안쪽을 향하는 별도 사선 굽힘 축과 넓은 기저 관절 범위를 사용합니다.

범위는 애니메이션용 근사 프로파일이며 사람 모두에게 동일한 의학적 가동 범위를 보증하지 않습니다. 경계 부근은 부드럽게 투영하고, 범위 이탈 또는 일치하는 영상 관측의 낮은 신뢰도가 있는 짧은 회전 반전만 시간축 보완 대상으로 삼습니다. 신뢰도 자료가 없는 기존 에셋은 범위를 벗어난 관절만 시간축 보완 대상으로 삼습니다. 전체 영상에 저역통과 필터를 적용하지 않습니다.

v4는 굽힘이 커질수록 긴 손가락의 벌림·비틀림 허용 폭을 줄이고, 중간·끝 관절이 독립적으로 크게 옆으로 꺾이거나 비틀리지 않도록 여유 범위를 좁혔습니다. 엄지는 별도의 넓은 프로파일을 유지합니다. 이상이 확인된 축만 시간축 보완하므로, 옆 꺾임 때문에 정상 굽힘까지 보간하지 않습니다. 현재 Manny 본의 길이와 기준 축으로 만든 캡슐로 같은 손의 손가락 사이 관통을 줄입니다. 같은 손가락의 첫 마디와 끝 마디도 검사하며, 바로 연결된 마디와 손바닥 기저끼리의 접촉은 제외합니다. 깊은 겹침을 더 크게 벌점화하고, 네 손가락을 펴는 대신 엄지의 작은 이동으로 주먹의 접촉을 해결할 여지를 늘렸습니다.

관통 회피로 **추가하는 보정량만** 시간축으로 완화하며, 추가 보정의 프레임 간 변화는 30fps 기준 3°(90°/초) 이내로 제한합니다. 원본 제스처의 회전 속도를 이 값으로 제한하지 않습니다. 정상 접촉과 빠른 동작을 과도하게 바꾸지 않도록 보정 폭을 제한하므로 일부 겹침이 남을 수 있습니다. 캡슐은 실제 스킨의 근사이며 손바닥·몸·반대편 손과의 충돌 해결이나 블러에 가려진 실제 제스처 복원을 보장하지 않습니다. 정상 범위의 잘못된 모델 추정은 그대로 남을 수 있습니다.

`Joint Correction Report` 및 `Saved/CubelessBodyMocap/JointPost_<ID>/`의 `metrics.json`, `preservation.json`, `joint_diagnostics.npz`, `request.json`, `profile.json`에서 설정·코드 해시·보정량·처리 시간·보존 검사를 확인할 수 있습니다. 후처리 자체는 추론 캐시와 분리된 새 결과를 만들므로 다시 추론할 필요가 없습니다. test2·test3·test5 전체 비교와 남은 사례는 프로젝트의 `cubeless/records/verification/finger-fist-contact-20261109/report.md`에 기록했습니다.

### 블러 구간의 영상 보강 신뢰도 수정

HandFlow 이후 HaMeR 영상 관측에 맞추는 단계에서, 낮은 신뢰도의 2D 관측이 시간축 자세를 과도하게 바꾸는 경우를 수정했습니다. `handflow_image_guidance.py`는 보조 점수0.3~0.7 사이에서 영상 오차의 영향력을 부드럽게 낮추고 기존 시간축 자세를 더 강하게 유지합니다. 0.7 이상은 기존 영상 가중치를 유지합니다. 이 점수는 손 형태 정답 확률이나 직접적인 블러 측정값이 아닙니다. 정상 제스처·가려진 실제 동작의 복원을 보증하지 않으며, WiLoR/MediaPipe 결과를 섞지 않습니다. 변경된 스크립트 해시가 기존 손 추론 캐시와 새 계산을 구분합니다. 저장된 애니메이션은 자동 변경하지 않습니다. 이 수정은 새 HandFlow 계산에 적용되고, 위 관절 후처리는 계속 별도 선택 기능입니다.

기준 자세에서 해부학적 축을 구성하는 설계는 [manotorch의 AxisLayer](https://github.com/lixiny/manotorch/blob/master/manotorch/axislayer.py), 운동학적 후처리의 범위와 한계는 [Corrective Filter Based on Kinematics of Human Hand for Pose Estimation](https://www.frontiersin.org/journals/virtual-reality/articles/10.3389/frvir.2021.663618/full)을 참고했습니다. 외부 코드를 복사하거나 추가 가중치를 편입하지 않았으며, 이 구현의 축·수치 프로파일은 별도로 작성했습니다.

CPU 회귀 검사는 HandFlow Python으로 `Tools/test_joint_constraints.py`를 실행합니다. 검증 범위는 정상적인 빠른 굽힘 보존, 좌우/엄지 축, 고립된 비정상 회전, 강도 0, quaternion 부호 동등성, 굽힘별 범위, 캡슐 FK·미분·관통 감소·보정 변화량, Windows 상태 파일의 일시적 읽기 잠금입니다.


### dance01 HandFlow 처리 비용 점검

손 처리 기록에는 `stage_seconds`(검출·모델 로딩·HaMeR 이미지 처리·시간축 추론·영상 보강)와 `gpu_component_seconds`를 남깁니다. 실행 중에는 해당 작업 폴더의 `stage_profile.json`도 확인할 수 있습니다.

중복 YOLO 검출을 한 번으로 합치고, 배치별 GPU 결과를 한 번에 읽으며, MANO의 고정 부모 인덱스를 CPU에 둡니다. 체크포인트는 mmap/assign 방식으로 불필요한 CPU 복사를 줄이고, 한 검증 단계 안에서 같은 모델 해시를 중복 계산하지 않습니다. 처리 전후 무결성 검증은 유지합니다. 가중치·FP32·프레임·기본 배치 2·시간축 창/스텝과 보정 알고리즘은 유지합니다.

2026-11-09 dance01 403프레임, 동일 전신 결과와 입력 프레임을 재사용하고 손 결과 캐시는 끈 비교에서 손 작업 전체 88.17초 → 80.68초, Python 프로세스 트리 RSS 피크 6771.73MiB → 5734.59MiB였습니다. 최종 손 회전은 전체 프레임에서 동일했습니다. 앞서 관측된 1546초 손 작업은 수정 전 재실행에서도 재현되지 않았으므로 이 차이를 최적화 효과로 주장하지 않습니다. 짧은 지연·환경 차이를 포함하는 단일 영상 측정이며 모든 PC/영상의 배속 보장은 아닙니다.


### 영상 위 본 디버그

원본 영상 뷰 상단의 **본 디버그 켜기/끄기** 버튼으로 몸통·팔·다리의 비교 선을 표시합니다. 초록은 영상에서 검출한 2D 관절(점수 0.6 이상), 자홍은 전신 모델 Hue 출력의 재투영, 청록은 현재 선택된 언리얼 애니메이션의 실제 본 위치를 원본 추정 카메라에 재투영한 값입니다. 검출값은 정답이 아니며 사람과 Manny의 체형·관절 정의 차이, 카메라 추정 오차가 포함됩니다. 손가락 상세 검출 표시는 포함하지 않습니다. 오른쪽 뷰의 회전·확대는 영상 위 디버그 좌표를 바꾸지 않습니다.

새 모캡 실행 시 원래 계산된 포즈로 작은 `DebugBones.f32`와 `DebugBones.json` 캐시를 기록합니다. 버튼은 캐시를 비동기로 한 번 읽고 재생 중에는 이미 평가된 본 13개만 투영합니다. 영상 경로·크기·수정 시각·시작 시점과 캐시 형식을 검사하며 불일치/누락 시 선을 숨기고 준비 상태를 안내합니다. 자동 모캡 재추론은 하지 않습니다. 기존 결과는 디버그 캐시가 없을 수 있습니다. 탐색 중에는 선을 숨겨 서로 다른 시점의 비교를 피합니다.
