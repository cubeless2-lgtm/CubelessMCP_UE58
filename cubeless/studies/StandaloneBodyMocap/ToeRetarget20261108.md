# HandFlow 기본값 및 발가락 리타깃 수정

2026-11-08, UE5.8.4 CL58902052. 사용자가 HandFlow를 기본값으로 지정하고, 다음 작업으로 발가락이 들리는 문제의 확인·보강을 요청했다. 기존 Cubeless Body Mocap C++ 승인과 정상 종료·재실행 승인 범위에서 진행했다. 컴퓨터 유즈, 관절 제한, 모델 혼합, Git publication은 하지 않았다.

## 변경

- `CubelessMocapSettings.h`: `FingerBackend` 초기값을 MediaPipe에서 HandFlow로 변경했다. 기존 enum 값/선택 항목은 유지했다. Development Editor 빌드 성공(37.68초), 기존 PID1252 정상 종료 후 같은 프로젝트 PID30852로 재실행했다. 새 CDO와 새 transient settings 모두 HandFlow인 것을 확인했다.
- `Tools/create_mannequin_preset.py`: 소스에 `LeftToe: ball_l→ball_l`, `RightToe: ball_r→ball_r` 단일 본 체인을 추가하고 동일 이름의 타깃 체인에 연결한다. 기존 `LeftFoot/RightFoot`를 toe 체인의 대체 매핑으로 사용하던 코드를 제거했다.
- `Tools/cbm_mannequin_toes.py`: 기존 기본 프리셋을 같은 구조로 수정하는 재사용 가능한 editor helper다. 타깃 toe 체인과 기존 소스 체인 정의를 양쪽 모두 확인한 뒤 변경한다. 두 번 실행하면 추가 변경이 없는 것을 시험했다. 커스텀 체인 정의를 임의로 재해석하지 않는다.
- 기본 `/Game/CubelessMocap/Setup/IK_CBM_SMPL` 및 `RTG_CBM_SMPL_to_Manny`도 수정·저장했다. 다른 체인 매핑은 동일하다. 변경 전 프리셋은 `/Game/_MCP_Temp/ToeReview/CubelessTests/20261108_R1/Before/`에 참조까지 분리해 보존했다. 원래 애니메이션은 덮어쓰지 않았다.

## 원인

기존 소스 `LeftFoot/RightFoot`는 `foot→ball` 두 본 체인이지만, Manny `LeftToe/RightToe`는 `ball` 하나다. 서로 다른 부모 프레임의 체인을 대응시켜 발목의 회전이 발가락에도 추가로 전달됐다. 소스/타깃 toe retarget pose offset은 모두 identity였다. 이번 수정은 각도를 강제로 낮추는 보정이 아니라 체인 대응 오류를 고친 것이다.

동일한 원시·최적화 입력으로 체인 매핑만 바꿨을 때 Manny 발가락 회전 크기가 원시 SMPL 발가락 값과 일치했다. 이 비교가 원인 판정의 근거다. 단안 모델의 모든 발 자세가 정답이라는 뜻은 아니다.

## 검증

test2 244프레임, test3 604프레임의 기존 전신 추론 캐시를 재사용했다. optimizer/bake는 각각15.6초/33.3초였다. `Hue_Pose`, `Optimizer_Pose`, `Optimizer_Translation`, `Baked_Quaternions`, `Shape_Bind_Local_SMPL`의 전후 SHA256가 모두 일치한다. 손 신경망 추론도 반복하지 않았다.

아래는 기준 자세 대비 **3D 로컬 회전 크기 중앙값**이다. 위로 드는 단일 축 각도와는 구분한다.

| 영상 | 본 | 이전 Manny | 수정 Manny | 원시 모델 |
|---|---|---:|---:|---:|
| test2 | ball_l | 14.08° | 2.37° | 2.37° |
| test2 | ball_r | 12.43° | 1.91° | 1.91° |
| test3 | ball_l | 19.76° | 2.85° | 2.85° |
| test3 | ball_r | 17.15° | 2.27° | 2.27° |

test3 왼쪽 최대 회전도45.15→18.88°, 오른쪽39.93→15.82°로 원시 모델 범위와 일치했다. 각도 제한이나 flattening을 추가하지 않았다.

161개 본을 모든 프레임에서 RAW 평가했다. 새 전신 결과에 이전과 동일한 HandFlow 손 결과를 적용한 최종 애니메이션에서도 **변경된 본은 ball_l/ball_r 두 개뿐이며 나머지159개 본의 로컬 위치·회전·스케일 최대 성분 차이는0**이었다. 손·손목·발목·다리·루트는 보존된다. WiLoR 결과도 별도 에셋으로 다시 연결해 비교 선택을 유지했다.

네이티브 비교 뷰에서 test2 f78/f101, test3 f177/f282/f601을 정면·70° 측면으로 전후 캡처했다(20장). 영상 시간과 실제 평가 애니메이션 시간 오차는 .002초 미만이었다. f601 측면에서 앞으로 말려 올라가던 발끝이 펴지는 것을 확인했다. 캡처는 `Saved/CubelessBodyMocap/ToeReview20261108/captures/`에 있다.

### 발바닥·미끄러짐의 한계

실제 Manny LOD0 스킨 정점을 재구성해 같은 지지 마스크로 비교했다. 전체 최저 높이는 test2 왼쪽−1.99→−0.49cm/오른쪽−1.87→−1.70cm, test3 왼쪽−2.29→−2.29cm/오른쪽−3.69→−1.98cm였다. 발목을 옮기지 않았으므로 기존 들뜸·침투가 모두 해결되지는 않는다.

발가락 변형을 포함한 발바닥 정점 중심 속도 p95는 test2 왼쪽13.30→13.75cm/s·오른쪽16.74→17.99cm/s, test3 왼쪽13.51→13.75cm/s·오른쪽20.50→21.05cm/s다. 이 대용 지표는 소폭 늘었다. 발가락 회전 오류 수정과 미끄러짐 해결을 동일시하지 않는다. 무릎 굽힘과 원시 발 전체 자세 오차도 이번 범위에서 변경하지 않았다.

## 전달 상태

PID30852, test3 새 HandFlow+수정된 발가락 애니메이션,20.033초,전체 정면 뷰로 복원했다. HandFlow가 기본 손 모델이다. WiLoR·손 보강 없음 버튼도 유지했다. RefineBody는 원래값 false, 원본/고속 옵션과 기타 입력 설정을 복원했고 background CPU throttle=true, dirty package 없음이다. 사용자 에디터는 열어 둔다.

처음 inspector의 Python property 접근 실패는 getter 방식으로 수정했다. 로컬 검사 스크립트 이름 `inspect.py`가 표준 라이브러리를 가려 외부 측정이 실패한 문제는 `inspect_toes_editor.py`로 이름을 바꿔 해결했다. 실패한 호출을 성공으로 계산하지 않았다.

기계 판독 기록: `cubeless/records/verification/cbm-toe-retarget-20261108.json`. 전체 실행 영상 재촬영, 다른 영상 및 커스텀 스켈레톤, 패키징, 독립 리뷰는 수행하지 않았다.
