# 영상에서 정지한 지지 발의 미세 미끄러짐 보완

2026-11-09. 기존 HandFlowOnly20261109 결과를 기준으로 비교했다. test2 마지막 자세에서 영상의 발은 거의 정지하지만 3D 발바닥 중심은 계속 움직였다. 하체 단계만 보완하면 뒤의 발목·앞꿈치 회전 적합이 이동을 다시 만들 수 있어 두 단계가 같은 정지 관측 신호를 사용하도록 수정했다.

## 공통 구현과 적용 조건

- `Plugins/CubelessBodyMocap/Tools/lower_body.py`: `target_video_legs_only_v6_stationary_observation`.
- `Plugins/CubelessBodyMocap/Tools/support_contact.py`: `target_support_rotation_v3_stationary_observation`.
- 영상의 발목·앞꿈치·뒤꿈치 4점 중 3점의 움직임과 전체 검출 신뢰도를 사용한다. 7프레임 평균, 5프레임 최소 필터 후 9프레임 이상 지속한 정지 관측만 사용한다. 현재 작업 입력은 30 FPS이므로 최소 지속 시간은 약 0.3초다.
- 지지 가중치와 정지 신호가 함께 있을 때 밑창 이동 비용을 높인다. 같은 신호를 이후 발목·앞꿈치 적합에도 전달한다. 정지 신호가 없으면 기존 산술 경로를 유지한다.
- 영상 이름·시간 구간을 코드에 지정하지 않았다. 하체 및 접지 회전 보완을 켜고 다시 처리하는 모든 영상에 같은 조건이 적용된다. 이미 저장한 애니메이션은 자동 수정하지 않는다.
- 기존 코드 해시 기반 캐시 무효화를 사용한다. 기존 전신·HandFlow 결과와 영상 관절 캐시를 재사용했으며 신경망 전신/손 추론을 반복하지 않았다. FootMR, 관절 제한, 모델 혼합을 추가하지 않았다.

## 실제 Manny 결과 비교

| test2 구간 / 발바닥 중심 누적 이동 | 이전 | 수정 후 |
|---|---:|---:|
| 7.8–8.1초 왼발 | 0.6058 cm | 0.1157 cm |
| 7.8–8.1초 오른발 | 0.4004 cm | 0.0741 cm |
| 7.5–8.1초 왼발 | 2.7866 cm | 1.9838 cm |
| 7.5–8.1초 오른발 | 2.1258 cm | 1.1349 cm |

test2 정지 신호는 왼발 231–243프레임, 오른발 228–243프레임에서만 활성화됐다. 골반 위치 변화 최대는 0.01628 cm(약 0.163 mm), 골반 회전 변화 최대는 0.568도다. HandFlow를 포함한 보호 대상 151개 로컬 트랙의 차이는 0이다. test3은 추가 정지 신호가 없으며 worker 하체 포즈 및 측정 결과가 이전 결과와 정확히 같다.

전체 구간 지지 속도 p95는 test2 왼발 9.5943→9.6906 cm/s, 오른발 8.1411→8.6987 cm/s로 소폭 증가했다. 평균은 왼발 5.3119→5.3175, 오른발 3.7881→3.6143 cm/s다. 마지막 정지 구간은 개선됐지만 전환부를 포함한 모든 지표가 개선된 것은 아니다. 잔여 미끄러짐과 원본 자세 차이는 남아 있다. 다른 영상 전반에서 더 좋다는 결론은 내리지 않는다.

## 실행·검증 및 한계

- 동일 프로젝트의 기존 Development Editor PID 31368, UE 5.8.4 CL58902052에서 외부 Python 비동기 보완과 native 적용·저장·프레임 평가를 수행했다. C++ 변경/빌드/패키지 검증은 수행하지 않았다.
- test2: worker 4.785초 / peak RSS 276.17 MiB, 에디터 작업 표시 5.9초. test3: worker 14.384초 / 580.12 MiB, 작업 표시 16.1초. 에디터 상태의 peak RAM은 전체 프로세스 수치이므로 worker RSS와 구분한다.
- 최종 test2 전체 재생과 9개 native 비교 화면 캡처를 완료했다. test3 추가 재생 점검은 시작 위치 0초를 기다리는 검사 스크립트의 `seek timeout`으로 완료하지 못했다. test3 수치 검증은 완료됐으며 포즈가 기존 검증 결과와 동일하다. 실패 기록은 보존했다. 최종 육안 평가는 사용자에게 남긴다.
- 현재 비교 뷰는 test2의 캐시 적용 결과이며 test2/test3 최종 에셋 탭을 열었다. 원본/HandFlow/하체 비교 참조를 test2로 복원했다. 사용자 에디터를 유지했고 handoff 후 dirty package는 없다.
- 기준 메타데이터의 초기 runner가 오래된 SupportReview 경로를 참조한 점은 HandFlowOnly 기준으로 정정했다. 원래 포인터는 별도로 보존했다. 최종 수치는 올바른 최신 기준 native 자료와 비교했다.

원시 자료: `Saved/CubelessBodyMocap/StationaryFeet20261109/`의 `final_assets.json`, `final_comparison.json`, `native_summary.json`, `playback_interpolated_error.json`, `handoff.json`. 재현 흐름은 `run_final.py` → `measure.py` → `feet_metrics.py` → `compare_final.py`이며 현재 프로젝트의 guarded official MCP Python 실행을 이용했다. 이전 결과를 덮어쓰지 않고 `/Game/_MCP_Temp/StationaryFeet/CubelessTests/` 아래 별도 저장했다.
