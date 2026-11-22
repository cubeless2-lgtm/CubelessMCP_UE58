# Quinn 팔다리 관통 보정 API

기본 대상은 `SKM_Quinn`입니다. 기존 Manny 리타기터는 같은 UE Mannequin 스켈레톤을 사용하므로 유지합니다. 원본 모캡/HandFlow/손가락 보정 계산과 모델 가중치는 바꾸지 않습니다.

이 기능은 **저장된 결과에 별도로 적용하는 두 단계 API**입니다. 일반 Start Mocap에 자동으로 끼워 넣지 않습니다. 먼저 대상 LOD0 형상과 애니메이션을 내보내고, 외부 HandFlow Python에서 관통 주변의 회전 보정량을 구한 뒤 새 AnimSequence에 적용합니다. Computer Use, 별도 서버, 게임용 MCP 의존성은 필요 없습니다.

## 1. 준비 — Unreal Python

```python
import unreal
source = unreal.load_asset('/Game/YourFolder/YourAnimation')
mesh = unreal.load_asset('/Game/Characters/UE5_Mannequins/Meshes/SKM_Quinn')
work = 'D:/Project/Saved/CubelessBodyMocap/ContactUniqueRun'
print(unreal.CubelessMocapLibrary.export_body_contact_data(source, mesh, work))
```

현재 프로젝트 `Saved/CubelessBodyMocap` 하위의 새 폴더를 사용합니다. 소스/메시가 저장돼 있어야 하고, 같은 스켈레톤 및 표준 Mannequin 팔다리 본 이름이 필요합니다. 처리 중인 모캡/PIE가 있으면 거절합니다. 기존 request.json을 덮어쓰지 않습니다.

## 2. 보정량 계산 — 외부 PowerShell

```powershell
& 'D:/Project/Plugins/CubelessBodyMocap/ThirdParty/HandFlow/venv/Scripts/python.exe' -I `
  'D:/Project/Plugins/CubelessBodyMocap/Tools/run_body_contact.py' `
  --config 'D:/Project/Saved/CubelessBodyMocap/ContactUniqueRun/request.json' `
  --iterations 400
```

이 PC의 NumPy/SciPy/Torch 환경을 사용하며 새 모델 추론·다운로드는 하지 않습니다. 기본은 CUDA이며 `--device cpu`도 선택할 수 있지만 CPU 실행 성능은 이번 검증에서 측정하지 않았습니다. 출력은 corrected.json, corrected.metrics.json, corrected.evidence.npz입니다. 출력이 있으면 거절합니다.

## 3. 적용 — Unreal Python

```python
output, message = unreal.CubelessMocapLibrary.apply_body_contact_data(
    source, mesh, work + '/corrected.json', '/Game/YourFolder/YourAnimation_Quinn_Contact')
assert output, message
print(message)
```

입력 에셋 GUID, 대상 메시 경로, 프레임 수/FPS, 허용 트랙 이름·순서와 유한한 단위 쿼터니언을 검증한 뒤 **새 에셋만** 저장합니다. 결과가 이미 있거나 소스가 바뀌면 거절합니다. 편집 대상은 양쪽 thigh/calf/foot/upperarm/lowerarm/hand의 12개 회전 트랙입니다. 골반·척추·손가락을 포함한 나머지 로컬 트랙, 모든 로컬 위치·스케일, 타이밍과 RateScale은 보존합니다. 기본 모드는 손·발의 전역 방향을 유지하고 팔다리 위치를 주변으로 조정합니다. 메시는 새 에셋의 프리뷰에 설정합니다. 원본의 디버그 실행 ID도 새 에셋으로 전달합니다.

전체 프로젝트를 원자적으로 롤백하는 기능은 아닙니다. 적용 후 저장 단계가 실패하면 미저장 새 에셋을 확인하세요. 입력 에셋은 교체하지 않습니다. 이 API 호출 사이에 다른 소스 편집이나 모캡을 병렬로 실행하지 마세요.

## 범위와 한계

LOD0 표본에서 반경을 구한 캡슐로 반대쪽 다리와 손/팔–다리 겹침을 줄입니다. 원래 자세 및 시간 변화에 대한 잔차를 함께 최소화하며, 접촉과 실제 관통을 구별하기 위한 표면 여유를 사용합니다. 손가락 내부 관통은 기존 손가락 보정 기능의 범위입니다. 몸통·옷·외부 물체의 정확한 충돌이나 모든 피부 삼각형의 무관통을 보장하지 않습니다. 수치 점수만으로 완료 판단하지 말고 실제 Quinn 렌더링도 확인해야 합니다.

초기 KaXLIZbdnek 236~246초 예시의 `_Quinn_Contact_v2`는 캡슐 기준 최대 겹침을15.14cm에서0.116cm로 줄였으나, 손이 영상과 다른 방향으로 빠지는 문제가 확인됐습니다. 겹침 점수만으로 영상과 같은 접촉 방향을 보장하지 않습니다. 이 후보는 비교용으로 보존합니다.

## 영상의 접촉 방향을 지정하는 보강 모드

원본 영상에서 확인한 손–다리 대응을 `surface_hints`로 지정할 수 있습니다. 자동으로 앞뒤 관계를 판별하는 기능은 아닙니다. `hand`는 hand_l 또는 hand_r, `segments`는 접촉할 다리 본 목록, `outward`는 Unreal 월드 좌표의 표면 바깥 방향입니다. 0이 아닌 유한한3차원 벡터를 사용합니다. `view_outward`는 기본 [0,1,0], `weight`는 양수이며 `axis_offset_cm`으로 축 방향 위치를 조정합니다. 영상에서 보이는 방향을 유지하는 잔차와 손 캡슐의 표면 바깥쪽 제약을 함께 사용합니다.

`freeze_legs=true`는6개 다리 회전을 입력 그대로 복사합니다. `free_hand_orientation=true`는 손의 전역 회전도 조정하므로 기본 모드의 손 전역 방향 보존을 해제합니다. 발 전역 방향과 손가락 로컬 회전은 유지합니다. `image_observations`의 fit(12개 카메라 계수), origin(3개), focal, l/r(프레임×어깨/팔꿈치/손목×x/y/점수)를 제공하면 검출 신뢰도로 가중한 어깨 상대2D 위치도 사용합니다. 영상별 카메라·관측값과 실제 렌더 검토가 필요합니다.

방향 보강 모드는 최대 캡슐 겹침이 `max_proxy_penetration_cm`(기본1cm)을 넘으면 결과 파일을 만들지 않습니다. 이 한계 이내여도 영상과 같은 위치나 정확한 피부 분리를 보장하는 것은 아닙니다.

이 모드로 만든 `_Quinn_HandFront_v4`에서는 문제의 가까운 손이 다리 뒤에 가려지는 캡슐 검사가20/300프레임에서0/300으로 줄었고1·2·3초 렌더에서도 바깥쪽에 보였습니다. 다리·몸통·손가락을 포함한155개 로컬 회전 트랙과 모든 위치·스케일·시간축은 유지했습니다. 반대쪽 손의2D 평균 오차는 증가했고21프레임의 캡슐 가림이 남습니다. 최대 캡슐 겹침0.683cm로, 전체 피부의 무관통이나 영상의 모든 관절 일치를 보장하지 않습니다. 손 방향 보강을 자동 StartMocap에 추가하지 않았으며 독립 QA와 다른 영상 일반화는 미검증입니다.
