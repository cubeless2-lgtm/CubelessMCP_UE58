# 손목 회전 차이 추적과 고속 모드 안정화 (2026-11-07)

대상은 UE 5.8.4 CL58902052, Cubeless Body Mocap, `dance01.mp4`의 처음 10초·30FPS·300프레임·발 고정·Manny다. 기존 결과와 사용자 프리셋은 보존했다. Computer Use는 사용하지 않았다.

## 원인 분리

직전의 서로 다른 Editor 세션에서 생성한 원본/고속 결과에는 왼쪽 손목 최대27.093° 차이가 있었다. 이 수치만으로 통합 Attention의 오차가 증폭된 것이라고 해석했던 이전 문서의 원인 추정은 수정한다.

이번에는 Camera/Tracking/Keypoints 체크포인트만 공유하고 CHMR부터 각각 새로 계산했다. 두 실행은 원인 분리용이며 속도 비교에 사용하지 않는다. 이 조건에서 Hue 출력의 왼쪽 손목 최대 차이는0.6990°, 오른쪽0.4214°였다. Ground와 Optimizer는 손목값을 바꾸지 않았고, 베이크 후에도 동일했다. 최종 Manny 애니메이션의 전체161본×300프레임 회전 최대 차이는0.6913°였다. 267프레임의 왼손 손목은 Hue/베이크 단계에서 약0.0420° 차이에 그쳤다.

원본 모드를 재실행한 결과와 과거 원본 결과 사이에서도 최대56.914° 차이가 나왔다. 따라서 과거27° 차이를 CHMR 모델 선택의 효과만으로 볼 수 없다. Hue final step 입력을 덤프하고 별도 CPU ONNX 실행으로 재생한 axis-angle 출력은 에디터 출력과 바이트 수준으로 일치했다. 회전6D 벡터의 퇴화나 리타깃 증폭은 이번 통제 비교에서 관측되지 않았다.

`camera_calib` 그래프에는 up/latitude decoder의 `RandomUniformLike` 연산 두 개가 있고 둘 다 seed 속성이 없었다. [ONNX 연산 정의](https://onnx.ai/onnx/operators/onnx__RandomUniformLike.html)는 seed 생략 시 자동 생성한다고 명시한다. 이 모델의 출력 초점거리는 CHMR Head의 카메라 역행렬과 Hue의 카메라 조건 양쪽으로 전달된다. 이 난수 입력이 달라지면 고속 모드와 무관하게 최종 손목도 달라질 수 있다.

추가로 원본 CHMR을 유지한 채 수정 전 진단과 고정 카메라 실행을 비교했다. TrackingBoxes/Keypoints 해시는 모두 같았고, 초점거리는959.264px→1033.964px였다. CHMR 토큰 상대L2 차이는 평균0.020405였으며, Hue 단계에서 이미267프레임 왼손 손목 차이가36.4237°였다. Ground/Optimizer/BoneBake는 이 손목 차이를 유지했다. 반면 동일 카메라에서 CHMR만 바꾼 토큰 상대L2는 평균0.001720이었다. 큰 차이가 최초로 관찰되는 관절 회전 출력은 Hue이며, 카메라 조건이 CHMR Head와 Hue 양쪽으로 유입되는 경로가 원인이다. 리타기터 설정이나 손목 평활화로 차이를 숨기지 않았다. 근거는 `cbm-wrist-camera-only-impact-20261107.json`이다. 과거 실행에는 카메라 수치가 없어 당시27.09°를 같은 시드로 역재생하지는 못했다.

## 수정

- 두 난수 연산에 고정 seed0/1만 추가한 `camera_calib_seeded`를 별도 생성했다. 가중치·노드·정밀도·차원은 유지했고 새 seed 속성만 제거하면 원본 그래프가 정확히 복원되는지 검사했다. 특정 영상에 맞춰 seed를 탐색하거나 튜닝하지 않았다.
- 원본/고속 CHMR 모두 같은 고정 카메라 보정 모델을 사용한다. 각 작업은 새 카메라 세션에서 첫 프레임을 한 번 추론한다.
- 캐시 키를 `v4_seeded_camera`로 바꿔 과거의 무작위 카메라 캐시 재사용을 방지했다. 기존 캐시는 삭제하지 않았다.
- profile에 카메라 초점거리·pitch·roll, TrackingBoxes/Keypoints/CHMR 토큰 해시를 추가했다.
- `cbm.DumpStages 1`을 켜면 다음 작업의 카메라 입력, CHMR 토큰, Hue/Ground/Optimizer 포즈, 베이크 quaternion 및 Hue final 입력을 해당 Saved 실행 폴더에 기록한다. 기본값0이며 정상 추출에 추가 덤프를 만들지 않는다.
- Fast CHMR을 기본 선택으로 변경했다. 가중치·896×896 입력·300프레임·Hue50회·보정·Manny 리타기터 설정을 유지한다. 옵션을 끄면 고정 카메라를 사용하는 원본 CHMR로 비교할 수 있다.

## 동일 영상 전체 재측정

수정 빌드는 성공했다(`Saved/Logs/CubelessBodyMocap_WristFixBuild.log`,15.98초). 같은 프로젝트 Editor PID6652에서 원본/고속 모두 체크포인트를 끄고 디코딩부터 저장까지 수행했다. 두 실행 모두 원본 PNG300장과 SHA256이 일치했고, 카메라 세 값과 TrackingBoxes/Keypoints MD5도 정확히 일치했다. 추적300/300, Manny AnimSequence 저장 성공이다. 엔진/OS 캐시는 warm 상태이며 진단 덤프는 두 실행 모두 켰다.

| 항목 | 수정 후 원본 CHMR | 수정 후 고속 CHMR |
| --- | ---: | ---: |
| 전체 시간 | 427.06초 | 294.66초 |
| CHMR 시간 | 280.46초 | 147.57초 |
| 에디터 RAM 피크 | 11.56GiB | 11.59GiB |
| 완료 후 RAM 표본 | 4.42GiB | 4.46GiB |
| PID별 GPU 전용 메모리 표본 피크 | 4.60GiB | 4.60GiB |
| PID별 GPU 공유 메모리 표본 피크 | 2.56GiB | 2.57GiB |
| 메인 스레드 API 표본 최대 응답 | 0.745초 | 0.415초 |
| Slate 최대 틱 공백 | 1.239초 | 1.191초 |

고속 모드는 원본 대비31.0% 단축(1.45배)됐다. 메모리 감소는 주장하지 않는다. 이전 고속 실행300.91초와 이번294.66초의 차이는 실행 편차가 포함되어 추가 CHMR 최적화 효과로 해석하지 않는다. 이번 수정의 목적은 속도를 유지하면서 비교 재현성을 확보하는 것이다.

161본×300프레임을 RAW AnimPose로 평가했다. 전체 로컬 회전 차이는 평균0.01005°,99백분위0.11049°,최대1.27997°였다. 왼손 손목 최대1.27997°(267프레임), 오른손0.55620°(229프레임)다. 주요25본의 위치 차이는 평균0.02587cm/최대0.07305cm, 손가락을 포함한 전체 본은 최대0.29503cm였다. 양쪽 루트 위치 차이는0이다. 이전 카메라 미고정 결과27.09°와는 비교 기준이 다르므로 단순한 오차 감소율을 계산하지 않는다. 새로운 기준은 동일 카메라·입력에서의 원본 CHMR이다.

별도 CPU ONNX 프로세스를 매번 새로 띄워 같은 카메라 입력을 반복했다. 무시드 원본 초점거리는931.945px와1007.675px로 달랐고, 고정 시드 모델은 두 번 모두1034.0235px와 동일한 pitch/roll을 반환했다. 이것은 프로세스 간 난수 변동을 확인한 보조 시험이며, DML 에디터 수치1033.9636px와 CPU 결과의 완전 동일성을 주장하지 않는다. 실제 에디터 원본/고속의 DML 카메라 출력은 정확히 일치했다. 전체 에디터 파이프라인의 재시작 후 반복은 별도로 수행하지 않았다.

완료 후 두 실행 모두 NNE 모델 에셋0개, 미저장 패키지0개였다. API와 Slate 표본은 작업 중 응답이 유지됐다는 근거이며, 수동 UI 조작 검수는 하지 않았다. 사용자 Editor와 Cubeless Body Mocap 창을 열고 원본 영상 경로, Fast CHMR=true, 정상 출력 폴더를 복원했다. 기본 CDO도true인 것을 재조회했다. 테스트 애니메이션은 다음 경로에 보존되어 있다.

- 고속: `/Game/_MCP_Temp/BodyMocap/CubelessTests/WristFixed_fast_20261107/Take_08F4CB7E462D57A479BF0C8D2D98D3A3/Animation/Body_08F4CB7E462D57A479BF0C8D2D98D3A3`
- 원본: `/Game/_MCP_Temp/BodyMocap/CubelessTests/WristFixed_reference_20261107/Take_3F1D218445C0AD06F273F889F3D5695C/Animation/Body_3F1D218445C0AD06F273F889F3D5695C`

## 근거

- 원인 분리: `cubeless/records/verification/cbm-wrist-diagnostics-v2-20261107.json`
- 단계별 차이: `cubeless/records/verification/cbm-wrist-stages-20261107.json`
- 통제 비교 최종 애니메이션: `cubeless/records/verification/cbm-wrist-native-target-quality-20261107.json`
- 과거 원본 대비 재실행: `cubeless/records/verification/cbm-wrist-reference-historical-quality-20261107.json`
- 과거 고속 대비 재실행: `cubeless/records/verification/cbm-wrist-fused-historical-quality-20261107.json`
- 최종 요약: `Plugins/CubelessBodyMocap/WRIST_STABILITY_BENCHMARK.json`
- 원본/고속 전체 측정: `cbm-full10-wrist-fixed-reference-20261107.json`, `cbm-full10-wrist-fixed-fast-20261107.json`
- 최종 품질: `cbm-wrist-fixed-quality-20261107.json`, `cbm-wrist-fixed-stages-20261107.json`
- 프로세스별 카메라 반복: `cbm-camera-seed-causal-20261107.json`
- 인계: `cbm-wrist-fixed-handoff-20261107.txt`
- 변환 재현 및 SHA256: `Plugins/CubelessBodyMocap/CAMERA_CALIBRATION_PROVENANCE.json`

수치 차이는 원본 CHMR 결과에 대한 비교이며 영상 정답에 대한 정확도 측정이 아니다. 시각적 동작 검수, 다른 영상·캐릭터·GPU, 패키징, 기존 스레딩 코드의 독립 리뷰는 추가 검증 대상이다.
