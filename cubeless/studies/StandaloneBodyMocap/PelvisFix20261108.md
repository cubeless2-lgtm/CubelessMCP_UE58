# Manny 골반 리타깃 수정 (2026-11-08)

사용자 미리보기에서 발견한 골반 이상은 실제 출력 오류였다. 고속 최적화 전후 결과에 공통으로 존재해 상대 오차 비교로는 발견되지 않았다.

## 원인과 수정

1. Manny 프리셋의 Root Motion 연산이 TargetRoot=pelvis로 설정돼 앞 단계에서 계산한 골반 회전을 root 회전으로 덮어썼다. SourceRoot=root, TargetRoot=root, TargetPelvis=pelvis로 수정하고 프리셋 생성 스크립트에도 적용했다.
2. 솔버용 SMPL 좌표의 높이 보정 비활성 상태가 본 내보내기까지 이어졌다. root 이동에서 HeightOffset을 빼는 기존 처리와 맞도록, BoneBake 직전에 높이 보정을 복원했다. 솔버와 지면 계산 설정은 유지했다.

수정은 원본·고속 공통 내보내기 경로에 적용된다. 기존 결과는 보존했으며 수정한 프리셋의 이전 버전은 `RTG_CBM_SMPL_to_Manny_BeforePelvisFix_20261108`에 백업했다. 중간 root-only 시험 결과는 최종 사용자 테스트 결과가 아니다.

## 검증

UE5.8.4 Editor 빌드 성공: `Saved/Logs/CubelessBodyMocap_PelvisFixBuild.log`. 기존 사용자 에디터의 미저장 상태를 확인하고 정상 종료한 뒤 같은 프로젝트를 다시 열었다.

동일 dance01.mp4의 0~10초, 30FPS, 고속, 발 고정, 추론 체크포인트 OFF로 전체 재생성했다. PNG 300장 모두 이전 입력과 SHA256이 일치했고 300/300프레임 추적·저장에 성공했다. 이번 실행은 350.28초, 프로세스 RAM 피크 11,980.94MiB였다. 과거 282.27초 측정값을 이번 수정 버전의 실행 시간으로 주장하지 않는다.

저장된 RAW 애니메이션의 300프레임을 검사했다.

| 항목 | 수정 전 | 수정 후 |
| --- | --- | --- |
| 골반 월드 Z 범위 | -16.17 ~ -5.52cm | 79.41 ~ 90.06cm |
| 첫 프레임 대비 골반 회전 최대 변화 | 0° | 31.78° |
| spine_01 기준 로컬 위치 최대 오차 | 32.90cm | 수치 오차 수준 |
| thigh_l 기준 로컬 위치 최대 오차 | 34.68cm | 0cm |
| thigh_r 기준 로컬 위치 최대 오차 | 34.31cm | 0cm |

척추·허벅지·종아리·상완·전완의 13개 본 로컬 위치가 전 프레임에서 Manny 기준과 0.001cm 이내로 일치했다. 기존 spine_01 로컬 길이 약29cm는 기준값 3.677cm로 복구됐다. 이 검사는 본 연결 오류의 수정 근거이며 영상 정답 대비 전신 품질이나 모든 보조 본의 정확도를 보증하지 않는다. 이번 수정 후 원본/고속의 전체 상대 품질 비교는 다시 실행하지 않았다.

최종 출력: `/Game/CubelessMocap/PelvisFixed_20261108/Take_B53AC2984A82CAF31D1E9291C4CB6CDA/Animation/Body_B53AC2984A82CAF31D1E9291C4CB6CDA`.

사용자 에디터 PID2804에서 새 애니메이션과 Manny를 열고 반복 재생을 설정했다. API로 재생 위치가 변하는 것을 확인했다. 화면 캡처용 toolset 조회는 실패해 시각 검증 완료로 기록하지 않는다. 컴퓨터 유즈는 사용하지 않았다.

근거: `cubeless/records/verification/cbm-full10-pelvis-fixed-fast-20261108.json`, `cbm-pelvis-validation-20261108.json`, `Saved/CubelessBodyMocap/pelvis_validation_20261108.txt`. 기존 상대 품질 수치는 당시 출력 간 차이로 보존하며, 두 출력에 공통인 리타깃 오류가 없다는 근거로 사용하지 않는다.
