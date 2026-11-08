# Cubeless Body Mocap 0.1.0

일반 손 보강은 `None (original hands)`와 `HandFlow / HaMeR`만 선택하며 기본값은 HandFlow입니다. WiLoR·MediaPipe는 `Hands > Diagnostics > Enable Legacy Hand Diagnostics`를 켰을 때만 추론·캐시 재적용·비교에 사용합니다. 기존 애니메이션은 계속 열 수 있습니다. [모델 준비·선택·파일별 조건](ThirdParty/HandFlow/README.md)을 확인하세요. HandFlow의 손 겹침 관측 신뢰도와 회전 가속 보정으로 test2 5.5초 주변 과보정과 회전 튐이 감소했습니다. 7.03초 개선과 7.7초 제스처도 유지됐지만, 가려진 손의 정확한 형태나 손끼리의 관통까지 해결한 것은 아닙니다. 상세 수정 전후 비교는 프로젝트 `cubeless/studies/StandaloneBodyMocap/HandFlowOcclusion20261108.md`에 기록했습니다.

영상 근거를 이용한 선택형 다리 보정은 `Refine Body from Video (Experimental)`에서 켭니다. `Foot Locking`이 필요하며 기본값은 꺼짐입니다. test2·test3에서 과한 지지 다리 굽힘과 대부분의 미끄러짐 지표가 개선됐지만, 일부 발 관통·영상 오차는 증가했습니다. 모델 가중치나 관절 제한은 변경하지 않습니다. 비교와 한계는 프로젝트의 `cubeless/studies/StandaloneBodyMocap/BodyDepth20261108.md`를 참조합니다.

UE 5.8.4 CL58902052용 Windows Editor 전용 테스트 버전입니다. MetaHuman 원본 플러그인을 비활성화한 프로젝트에서 빌드했습니다. 에디터 플러그인 로딩과 Manny 프리셋 생성·저장을 확인했습니다. 추론 모델 로딩과 기존 영상의 8프레임 추출·저장을 확인했습니다. 이후 동일한 10초 전체 클립의 속도·메모리·에디터 응답도 재측정했습니다. 시각적 동작 품질 비교와 패키징은 추가 검증 대상입니다.

## 테스트 준비

1. 에디터를 열고 **Tools → Cubeless Body Mocap**을 선택합니다.
2. FFmpeg 경로, Target Mesh(Manny), Retargeter가 자동 선택됩니다.
3. 동영상 파일을 선택하고 2~5초 구간, 30 FPS로 시작합니다. `Fast Mode (Off: Original)`을 켜면 고속, 끄면 원본 계산 경로를 사용합니다.
4. **Start extraction**을 누릅니다. 동일 10초 영상의 추출·저장은 확인했으며, 동작 품질은 생성한 애니메이션을 재생해 확인합니다.

준비된 프리셋은 `/Game/CubelessMocap/Setup/RTG_CBM_SMPL_to_Manny`입니다. 원본 IK Rig를 수정하지 않고 전용 소스·대상 IK Rig를 복제했으며 22개 체인 매핑, 5개 기본 retarget op, 대상 기준 자세 자동 정렬을 저장했습니다. 같은 UE5 스켈레톤의 Quinn을 쓰려면 Target Mesh를 Quinn으로 바꿀 수 있으나 Quinn에 대한 실제 결과는 미검증입니다.

## 입력

- `Video`: 동영상 파일입니다. `FFmpeg`는 플러그인 내부 `ThirdParty/FFmpeg/Win64/ffmpeg.exe`가 자동 선택됩니다. 시스템 설치나 PATH 설정은 필요 없습니다.
- `Start Seconds`, `Duration Seconds`, `Frame Rate`: 추출 구간과 샘플링 FPS입니다. 첫 테스트는 30 FPS를 권합니다. 다른 FPS에서의 품질은 미검증입니다.
- FFmpeg 대신 `Frame Directory`에 PNG 시퀀스 폴더를 지정할 수 있습니다. 이 경우 Video·Start·Duration은 사용하지 않고 폴더 전체를 Frame Rate 기준으로 처리합니다. 00000000.png처럼 같은 길이의 순번 파일명을 사용하며 프레임 크기를 일정하게 유지합니다.
- 예: `ffmpeg -i input.mp4 -t 5 -vf fps=30 -start_number 0 frames/%08d.png` (frames 폴더를 먼저 만듭니다).

## 출력

- Manny와 준비된 Retargeter가 기본 선택됩니다. 다른 캐릭터에는 해당 캐릭터용 리타기터를 지정합니다.
- Output Folder는 `/Game/` 아래를 지정합니다. 매 실행 고유 Take 폴더를 만들어 기존 결과를 덮어쓰지 않습니다.
- **Start extraction**을 누릅니다. 결과 Animation Sequence는 `Take_<id>/Animation` 아래에 저장됩니다. 임시 소스 애니메이션은 저장하지 않습니다.
- 생성한 애니메이션을 열어 대상 캐릭터에서 미리보기·재생합니다. 발 미끄러짐, 몸 방향, 크기, 루트 이동과 영상 마지막 프레임을 확인합니다.
- 대상 캐릭터·Retargeter는 수정하지 않습니다. 저장 실패나 취소 시 이미 생성한 출력이 일부 남을 수 있습니다. 오류 메시지와 폴더를 확인한 뒤 다시 실행합니다.

## 처리와 제한 (2026-11-07 비동기 최적화 적용)

검출/추적 → 관절 추정 → CHMR/Hue 추론 → 바닥/카메라 변환 → 발 고정 optimizer → SMPL-X 변환 → 표준 IK 리타기팅·저장을 연결했습니다. 메타휴먼 얼굴·Creator·Performance 에셋과 MCP는 사용하지 않습니다. UE 일반 플러그인 NNERuntimeORT·IKRig·RigLogic은 필요합니다. RigLogic은 vendor optimizer의 메모리/입출력 유틸리티를 제공합니다.

추론·보정·PNG 디코딩·뼈 트랙 계산은 전용 백그라운드 스레드에서 순차 실행합니다. UObject 로드와 참조 관리, 에셋 생성·리타기팅·저장은 게임 스레드에서만 실행합니다. 모델 패키지는 비동기 로드하고, NNE의 일반 모델 핸들을 게임 스레드에서 준비한 뒤 실제 세션 생성과 추론을 작업 스레드에서 실행합니다.

검출기 → SAM2 추적기 → ViTPose → CHMR → Hue 순서로 세션을 교체합니다. 단계가 끝나면 사용이 끝난 자체 모델 패키지를 명시적으로 언로드합니다. 더티 상태이거나 별도 에셋 편집기에서 열린 모델은 자동 언로드하지 않습니다. 사용자 에셋이나 Undo 기록을 초기화하지 않습니다.

**Start extraction**은 작업을 시작하고 즉시 돌아옵니다. 창 하단에 단계·진행률·시간·에디터 프로세스 메모리 피크가 표시됩니다. **Cancel**은 취소를 요청합니다. 진행 중인 단일 ORT 세션 생성/추론 호출은 중간에 강제 종료할 수 없으므로 반환을 기다린 뒤 중단합니다. Hue는 각 denoising step 사이, 보정기는 목적함수 평가 사이에서도 취소를 확인합니다. 최종 에셋 저장·GC·로드 완료 처리에는 짧은 메인 스레드 대기가 남을 수 있습니다.

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


## 골반·다리 단독 보완 / FootMR 제거 (2026-11-08)

FootMR 기능과 전용 모델을 제거했습니다. `Lower Body > Refine Pelvis and Legs from Video (Experimental)`을 켜고 `Refine pelvis / legs`를 누르면 기존 전신·손 결과에서 하체만 별도로 보완합니다. `Before legs` / `Pelvis / legs` 버튼으로 비교합니다. 기본값은 꺼짐이며 기존 HandFlow 기본값은 유지합니다.

`Refine Supported Ankle and Forefoot`를 끄면 발목 **월드 방향**과 상체 월드 자세를 유지하며, 바뀐 종아리에 대한 발목 로컬 역보상만 적용합니다. 이 모드에서는 손·손목·발가락 로컬 트랙을 정확히 보존합니다. 이 옵션을 켜면 지지/전환 구간의 발목과 앞꿈치 회전을 실제 대상 신발 형상과 지면에 맞춥니다. 공중 구간의 발 방향과 HandFlow 손은 보존합니다. 두 옵션의 기본값은 꺼짐이고 관절 제한은 없습니다.

옆모습에서 하체 보완이 원본 다리의 앞뒤 깊이를 뒤집던 오류를 수정했습니다. 접지 회전 보완을 포함한 test2/test3 검증에서 최대 침투는 약0.39/0.35cm였지만 일부 미끄러짐과 원본 발 회전 튐은 남습니다. 별도 모델을 설치하지 않으며 FootMR을 재도입하지 않습니다. 수정 원인, 옵션별 보존 범위와 실제 전후 수치는 `cubeless/studies/StandaloneBodyMocap/LegDepthAndSupport20261108.md`에 기록했습니다.

`Lower Body Python`은 독립된 `ThirdParty/LowerBody/venv/Scripts/python.exe`를 기본 사용합니다. 독립 Python 3.12로 `Tools/prepare_lower_body.py`를 실행하면 고정 버전의 numpy/scipy/psutil만 설치되며 손 모델 환경은 변경하지 않습니다. 손 모델이나 발 모델을 추론하지 않습니다. 저장된 전신 관측의 `FootMRInput.json`/`FootMR_Keypoints25.f32`/`FootMR_Camera.f32` 파일명은 이전 결과 호환을 위한 이름일 뿐 FootMR 의존성이 아닙니다. 새 결과와 캐시는 각각 `/Legs/`와 `Saved/CubelessBodyMocap/LowerBodyCache/`에 분리합니다. 검증 결과: `cubeless/studies/StandaloneBodyMocap/LegsOnly20261108.md`.


후속 test2/test3 리뷰에서 하체 미끄러짐 기준을 발목 피벗에서 실제 밑창 중심으로 바꾸고, 접지 단계의 최종 밑창 이동·회전 연속성과 착지 전환을 보완했습니다. 원본 손과 공중 발 방향은 유지합니다. 실제 native 전후 수치와 남은 오류는 `cubeless/studies/StandaloneBodyMocap/SupportReview20261108.md`를 참조하십시오.

### 전체 구간의 시간 연속성 보완 (2026-11-09)

`Stabilize Foot Rotation Outliers`를 추가했습니다. 하체·접지 보완을 켠 상태에서 선택하면 공중을 포함해 불균일한 회전으로 판정된 발 프레임만 시간축으로 보완합니다. 그 외 공중 발 방향과 HandFlow 손은 보존하며 관절 제한은 추가하지 않습니다. 기본값은 꺼짐입니다. 하체 출력 위치·회전의 연속성도 보완했고, 재생 중 3D 뷰가 영상 프레임 사이의 포즈를 표시하도록 수정했습니다. test2/test3 전체의 회전 불균일 지표는 약 27% 감소했지만, 이는 전체 품질 향상률이 아닙니다. 일부 빠른 회전의 약화와 잔여 미끄러짐이 있어 전체 재생으로 비교해야 합니다. 상세 수치는 `cubeless/studies/StandaloneBodyMocap/TemporalFeet20261109.md`를 참조하십시오.


Stationary supported-foot refinement (2026-11-09): with lower-body and support-rotation refinement enabled, confident stationary foot observations lasting at least nine samples (0.3 s at the current 30 FPS worker input) strengthen sole stabilization in both fitting stages. This common rule is not tied to test2 timestamps. Existing saved animations remain unchanged until refinement is rerun. HandFlow tracks remain protected; no joint limits are added. See cubeless/studies/StandaloneBodyMocap/StationaryFeet20261109.md for measured benefits and transition-motion tradeoffs.


Contact transitions (2026-11-09): qualified stationary observations now fade symmetrically across seven samples. A modest support-velocity adjustment, contact-weighted trajectory continuity and video-qualified near-ground sole samples reduce residual drift without hard foot locking or joint-angle limits. Existing HandFlow tracks remain unchanged. Detailed test2/test3 results, pose changes and remaining slip/rotation tradeoffs: cubeless/studies/StandaloneBodyMocap/TransitionFeet20261109.md.
