# 비교 뷰 잔상·실시간 스크럽 및 HandFlow 깊이 보정

2026-11-08, UE 5.8.4 CL58902052, 기존 UnrealMCP5_8_3 Development Editor. 사용자의 Cubeless Body Mocap `c++1218` 승인 범위와 정상 종료 지시로 진행했다. 컴퓨터 유즈, 관절 제한, WiLoR/HandFlow 회전 혼합, Git publication은 하지 않았다.

## 적용한 변경

- `SCBMComparison.cpp/.h`: 비교 뷰만 독립적인 FPreviewScene으로 변경했다. Lumen GI/reflections, SSR, temporal AA, motion blur, bloom을 끄고 FXAA를 사용한다. 프로젝트 전체 렌더러 설정은 변경하지 않는다. 고정 노출, 스튜디오 cubemap, key/fill/rim 조명, 회색 배경과 지연된 sky capture 갱신으로 금속 Manny의 밝기를 확보했다. directional light 우선순위도 지정했다.
- `CBMPreviewClock.h`, `CBMPreviewClockTests.cpp`: 디코더의 이전 시간이 드래그 목표 시간을 덮어쓰지 않도록 분리했다. `SSlider.PreventThrottling(true)`로 드래그 중 뷰 렌더링 중단을 방지한다. 값 변경 때 즉시 애니메이션을 평가하며, 해제 후에도 최신 seek가 완료될 때까지 목표 자세를 유지한다. 재생은 seek 완료 뒤 시작한다.
- `handflow_image_guidance.py`: 기존 2D 투영만으로 구분하기 어려운 손가락 깊이 방향에 HaMeR의 3D 뼈 방향 관측을 약한 항(weight .03)으로 추가했다. 기존 HandFlow residual 최적화이며 HaMeR 회전을 복사하지 않는다. detection/ownership score .6~.8 구간의 제곱 ramp로 가림·낮은 신뢰도 관측의 추가 영향력을 줄인다. 클립별/프레임별 규칙은 없다.

양손은 기존 canonical right MANO 공간에서 처리하고, palm-only 정렬은 고정한다. HaMeR 21관절 순서(wrist, thumb, index, middle, ring, little)의 손가락 15개 뼈 방향을 사용한다. 평균 자세·손목·좌우 미러 보정은 추가하지 않으며 왼손 미러는 기존 후단에서 한 번만 적용한다. 기존 공통 회전 계약과 Manny 리타깃/접촉 보정 경로를 유지했다. 기존 WiLoR와 MediaPipe 경로는 변경하지 않았다.

worker의 기존 코드 SHA256 캐시 키에 변경한 Python 파일이 포함되므로 다음 실행부터 이전 손 캐시는 재사용되지 않는다. 전신 추출 캐시와 저장된 결과는 그대로다. 모델 가중치나 환경 의존성을 변경하지 않았다.

## 실제 검증

원시 증거는 `Saved/CubelessBodyMocap/PreviewMotionFix20261108/`에 있다.

- PID33036 정상 종료 → build1 성공(11.47초) → PID32052. 밝기 부족과 조명 경고를 확인하고 수정했다.
- PID32052 정상 종료 → build2 성공(6.94초) → PID31756. 같은 프로젝트 에디터만 사용했다. 강제 종료/병렬 에디터 없음. 종료 전 dirty/PIE/job 상태 확인.
- `Cubeless.BodyMocap.Preview.DelayedSeekDuringScrub` 성공. 요청 시간 보존, 이전 seek 완료 무시, 역방향 이동, 재생 추종, 범위 제한, 초기화를 검사했다. 실제 마우스 드래그 자동화는 하지 않았다.
- 네이티브 캡처의 최종 `ComparisonPreview.json`: AA=FXAA(1), GI=None(0), reflections=None(0), Lumen/temporal AA/motion blur=false. 전체·손 확대에서 밝기와 경고 제거를 확인했다. 24장의 최종 전후 캡처에서 요구 시각과 실제 AnimSingleNodeInstance 평가 시각 오차 <.002초. 동영상으로 잔상 감소량을 정량 측정하지는 않았다.
- test2 244프레임, test3 604프레임의 기존 HaMeR 관측과 HandFlow temporal pose를 재사용했다. 전신·손 신경망 추론을 반복하지 않고 변경한 후처리만 실행했다. production 함수와 채택 후보 출력 차이는 두 클립 모두 0이었다. 처음 수식의 곱셈 순서를 바꾼 이식에서는 최대 .00345rad 차이가 발생해 동일 연산 순서로 고친 뒤 재검증했다.
- native 저장 시 두 클립 각각 비손가락 123트랙의 원본 대비 차이 0. 별도 Manny 전체 프레임 평가에서도 양 손목 위치·회전이 기존/수정/전신 원본 간 정확히 같았다. 기존 자산은 덮어쓰지 않았다.

### HandFlow 결과

프레임 간 회전 변화는 30fps에서 손가락 30관절의 local quaternion geodesic angle이다. 구간 수치는 해당 프레임 전후 약 .1초 창의 최대치이며, 각도가 작다는 사실만으로 품질 향상을 뜻하지 않는다.

| 측정 | 기존 | 수정 |
|---|---:|---:|
| test2 5.5초 주변 Manny 최대 변화 | 16.24° | 16.25° |
| test2 7.03초 주변 Manny 최대 변화 | 21.35° | 21.48° |
| test2 7.7초 주변 Manny 최대 변화 | 4.30° | 2.79° |
| test2 전체 p95 / 최대 | 8.62° / 26.18° | 8.68° / 26.33° |
| test3 전체 p95 / 최대 | 8.89° / 38.76° | 8.91° / 38.92° |
| test2 5.5초 왼손 메시 edge→오른손 triangle 교차 수 | 39 | 33 |

관찰상 test3 시작 왼손의 펼쳐져 비틀린 손가락과 test2 7.03초 오른손의 덜 접힌 모양이 개선됐다. test2 7.7초 제스처는 유지됐다. test3 13.83/17.3초도 양손을 비교했다. 모든 형태가 정확해진 것은 아니며 일부 회전 변화는 소폭 증가한다. 교차 지표는 고정 LOD0 메시에서 한 방향으로 센 진단치이며 관통 부피가 아니다. 5.5초의 관통은 남는다.

신뢰도 gate 없는 첫 .03 후보는 5.5초 교차가 39→83으로 악화되어 채택하지 않았다. 최종 수정은 위의 신뢰도 gate를 포함한다. 7.03/7.7초의 손 간 교차 지표는 전후 모두 0이다.

새 영상 증거 항만 실행한 시간은 test2 2.09초/test3 3.79초, 이후 기존 접촉 보정은 test2 약4.23초/test3 비적용이었다. 전체 모델 실행 시간·최대 메모리를 새로 측정한 결과가 아니다. 과거 전체 길이 비교 MP4는 기존 버전 기록이며 덮어쓰지 않았다.

### 다리 후보는 기본값에 미반영

기존 전신 추론 캐시로 (Depth,Contact,Height)=(500,80,40), (500,120,80), (400,100,40)을 test2/test3에 적용했다. 각 후보는 14.6~37.3초의 optimizer/retarget 실행이며 신경망 재추론이 아니다.

첫 후보의 test3 마지막 지지 무릎 굽힘은 Manny 19.05→13.73°로 줄었지만, 고정 지지 프레임 mask의 발바닥 이동속도 p95는 왼쪽13.51→15.18cm/s, 오른쪽20.50→21.13cm/s로 늘었다. 나머지 후보도 발바닥 높이·미끄러짐·투영 오차가 함께 개선되지 않았다. 따라서 기본 (200,80,40)과 기존 전신 자산을 유지했다. 지지 다리 굽힘과 발 관통/미끄러짐은 여전히 남은 문제다.

## 에디터 전달 상태

PID31756에서 test3, 새 HandFlow 결과, 전체 뷰, 0초 일시정지로 열었다. `WiLoR + MediaPipe`, `HandFlow / HaMeR`, `No hand enhancement` 버튼으로 같은 전신 결과를 비교할 수 있다. `Left hand`/`Right hand`로 확대하고 재생바를 드래그한다. test2 수정 자산은 아래 verification JSON의 `assets.test2.asset`에 있다. `Preview Animation`에 해당 자산과 test2 영상을 선택해 볼 수 있다.

원본·고속 옵션과 모델 선택 기본값을 바꾸지 않았다. 임시 background CPU throttle을 원래 값 true로 복원했고 dirty content/map은 없다. 사용자 에디터를 닫지 않고 남긴다. 최종 육안 평가, 다른 영상, 패키징, 독립 리뷰는 이번에 수행하지 않았다.

기계 판독 기록: `cubeless/records/verification/cbm-preview-motion-fix-20261108.json`.
