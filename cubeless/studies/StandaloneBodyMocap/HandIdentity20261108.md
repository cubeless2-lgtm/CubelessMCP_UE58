# test3 오른손 검출 보완 및 다리 단계별 조사 (2026-11-08)

## 적용한 공통 수정

`hand_image_support.py`에서 이미 추출된 ViTPose 2D 손 관절을 MediaPipe 후보의 독립적인 위치 근거로 사용한다. 신뢰도 0.5 이상인 점이 12개 이상이고 손바닥 기준점이 3개 이상일 때만 사용한다. 손 크기로 정규화한 평균 위치 차이가 0.8을 넘는 후보는 거절하고, 0.2를 넘는 경우 조건부 재검출을 수행한다. 대체 후보는 기존 후보보다 오차가 0.08 이상 및 25% 이상 줄어들 때 선택한다. 약한 기준 관측은 후보를 거절하지 않는다.

문제는 test3 13.83초 주변에서 MediaPipe가 팔찌/팔뚝을 오른손으로 검출하고, 손목 거리와 크롭 순위만으로 그 후보를 선택한 것이다. 13.87/13.90초의 오검출이 인접 프레임 필터에도 영향을 주었다. 이번에는 실제 손 영역 후보를 선택한다. 모델 가중치, 관절 축 솔버, 시간 필터와 관절 제한은 변경하지 않았다.

## 확인한 결과

| 구간 | 이전 최대 관절 프레임 변화 | 수정 후 | 관측 |
|---|---:|---:|---|
| test3 13.833초 오른손 |118.99°|12.63°|직접 검출|
| test3 13.867초 오른손 |115.72°|13.33°|팔찌 대신 실제 손|
| test3 13.900초 오른손 |34.62°|8.99°|팔찌 대신 실제 손|
| test3 14.033초 왼손 |18.68°|18.68°|기존 짧은 보간 유지|
| test3 17.300초 왼손 |17.34°|11.27°|직접 검출|

각도 변화는 시간적 안정성 지표이며 정답 정확도가 아니다. 원본 영상/MediaPipe 오버레이/Manny 네이티브 캡처도 비교했다. test2 3.2/5.5/7.7초 왼손 회전은 수치 오차 0.000003° 이내로 동일하다. test2 최대 지원 관절 변화 52.68° 유지, test3 118.99→83.80°. 다른 어려운 프레임과 가림·깊이 추정 한계는 남는다.

전체 test2 244프레임, test3 604프레임을 다시 실행했다. 두 영상 모두 새로 원본 복귀한 프레임이 없으며, 전체 원본 복귀 L/R은 test2 0/44, test3 7/7이다. 18개 단위 테스트 통과. 두 새 애니메이션의 비손가락 123개 트랙은 원본과 최대 차이 0이다. 원본 애셋과 MediaPipe/MANO/WiLoR 모델 해시 불변을 확인했다. WiLoR 전체 추론은 재실행하지 않았다. 추가 후보 탐색으로 실행 시간이 증가했으며 이번 실행은 병행 작업이 있어 성능 비교 판정에는 사용하지 않는다.

결과 애셋:
- `/Game/CubelessMocap/Test2_IdentityVerified_20261108/Hands/Body_DC5AF0E84CC150606168F0B22FE0423D_MediaPipe_46D498C8`
- `/Game/CubelessMocap/Test3_IdentityVerified_20261108/Hands/Body_8F3F98A7490870E8A06282BEA7C29534_MediaPipe_C80AB822`

## 사용자가 지적한 지지다리

현재 프레임은 test3 20.1초/f603이었다. 화면 왼쪽 지지다리 = 캐릭터 해부학적 오른다리이다. 카메라 각도나 손 보강만의 문제는 아니다.

- 영상 2D 엉덩이-무릎-발목 관절선의 굽힘: 8.76°. 2D 값은 3D 정답이 아니다.
- 캐시 Hue 출력의 무릎 axis-angle 크기: 27.66°. 아래 기하학적 굽힘과 같은 척도는 아니지만, 리타깃 이전부터 굽힌 추정이 있음을 보여준다.
- 내보낸 SMPL의 실제 세 관절 위치로 계산한 굽힘: 29.85°.
- 기존 Manny: 42.29°.
- 시험용 FK 다리 위치를 IK 목표로 유지한 Manny: 28.93°.

기존 Run IK Rig의 LeftLeg/RightLeg `chain_position_alpha=1`을 시험용 복제본에서만 0으로 바꾸면 추가 굽힘이 줄어든다. IK 전체를 껐을 때도 같은 다리 결과가 나왔고, IK를 유지한 채 다리 목표 계산만 분리해 원인을 좁혔다. test3 전체604프레임의 SMPL 대비 평균 무릎 굽힘 오차 L/R은 11.45/11.65°에서 0.70/0.54°로 감소했다. 그러나 발 높이 중앙값이 약3cm 내려가므로 바닥 접촉·발 미끄러짐의 최종 검증 없이 기본 프리셋으로 승격하지 않았다. 기존 리타기터와 원본 전신 애셋은 변경하지 않았다. 남은 모델 추정 오류를 무릎 강제 펴기나 관절 제한으로 숨기지 않았다.

## 에디터 종료 및 복구 상태

손 비교와 저장 완료 후 다리 네이티브 캡처용 검증 스크립트에서 MediaPlayer ObjectIterator 조회의 `/Engine/Transient.` 필터를 누락했다. 기본 객체의 `GetUrl` 호출에서 UE SharedPointer assertion이 발생해 사용자 에디터 PID32592가 종료됐다. 이는 이번 검증 스크립트 오류이다. 종료 직전 dirty 패키지는 이번 시험용 `RTG_NoIK`, `RTG_FKLegGoals` 두 개였으며 손 결과는 이미 디스크에 저장돼 있다.

같은 프로젝트의 정규 Development Editor PID31476를 다시 실행했다. 현재 '패키지 복원' 모달에서 사용자 선택을 기다린다. 컴퓨터 유즈 금지 요청을 따라 UI 조작으로 닫지 않았으며 사용자에게 창 처리를 요청했다. 기존 사용자 에디터를 병렬로 실행하거나 별도 프로젝트를 쓰지 않았다. 다리 시각 비교·최종 화면 복구는 아직 완료되지 않았다.

## 근거

로컬 원시 증거: `Saved/CubelessBodyMocap/HandIdentity20261108/` 아래 `validation.json`, `comparison.json`, `leg_probe.json`, `leg_noik_probe.json`, `leg_fkgoal_probe.json`, `leg_full_positions.json`, `leg_metrics.json`, `test3_release_observations.jpg`, `test3_right_interval.jpg`, `test3_release_before_after.jpg`, `test2_release_key_comparison.jpg` 및 모든 전후 네이티브 캡처.

공식 엔진 구현 확인: UE5.8 `IKRig/Private/Retargeter/RetargetOps/RunIKRigOp.cpp`의 `FRunIKRigChain::Run`, `IKRig/Public/Retargeter/RetargetOps/RunIKRigOp.h`의 `ChainPositionAlpha` 정의. 소스/타깃 기준 길이와 현재 source chain 길이로 목표를 계산한다. 정확한 체형/기준 자세 기여도 분해는 후속 검증 대상이다.

C++ 변경·빌드·모델 재학습·관절 제한·커밋·푸시는 수행하지 않았다.
