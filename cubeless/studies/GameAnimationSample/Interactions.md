# 상호작용·물리와 제어권

## 다중 캐릭터 선택과 정렬

설치본의 `CanDoCharacterInteraction`/`CanBeInteractedWith`는 래그돌과 `DefaultSlot` 재생 등 진입 조건을 검사한다. `Try_MultiCharacterInteraction`은 `CHT_CharacterInteractionPSDs`로 후보를 선택하고 `MotionMatchMulti`를 거쳐 참가자의 `PlayCharacterInteractionAnimation`으로 이어진다. 단일 캐릭터의 Motion Matching DB 선택과 구분한다.

소유 `L_PhysicsLab`에서 플레이어가 실제 이동으로 다른 캐릭터에게 접근한 뒤 E 입력을 주었다. 최초 스크립트 파일명은 shove 가설을 따랐지만 **실제 선택은 Tackle**이었다. 이름을 근거로 Shove 성공이라고 판단하지 않는다.

- 25.34초 관측, 75개 표본. 두 캐릭터 거리는 약 1,700에서 접근해 상호작용 직전 약 175, 최소 약 65.83 UE 단위였다.
- 실제 Montage는 `AM_M_relaxed_ragdoll_tackle_run_F_Rfoot_A`와 `_V`였다.
- 두 캐릭터에서 OnGround → Traversing → Ragdoll을 관측했다. 대상은 기상 Montage를 거쳐 OnGround로 복귀했고 플레이어는 관측 종료 시 Ragdoll이었다.
- 실제 렌더 캡처에서 두 캐릭터의 물리 상태를 확인했다. 순간이동으로 정렬 성공을 만들어 내지 않았다.

이 결과는 해당 대표 상호작용의 선택·동반 실행·상태 전환 근거다. 두 Montage의 정확한 프레임 동기 오차나 모든 타깃 거리·파괴 경계의 보장이 아니다. Motion Warping Notify 창의 실제 메타데이터는 [Transitions.md](Transitions.md)에 있다. 다중 참가자 Warp target 수명 전체는 후속 범위이며, 단일 캐릭터 Traversal 목표의 생성·갱신·weak 참조 소멸·제거 실험은 [Traversal.md](Traversal.md)에 추가했다.

## Physics Control과 래그돌 복귀

`SandboxCharacter_Mover_Ragdoll`에는 CharacterMover, PhysicsControlComponent와 애니메이션 이후 틱 구성요소가 있다. `ProduceInput`, `On_MovementModeChanged_PostFinalize`, `SetPhysicsProfile` 그래프에서 입력·이동 모드·물리 프로파일 연결을 읽었다. 이는 [UE 5.8 샘플의 물리 확장](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8)과 일치한다.

첫 R/R 입력은 OnGround → Ragdoll만 입증했다. 다음 **별도 상태 관측**에서 R 이후 SpaceBar를 사용해 OnGround → Ragdoll → OnGround를 확인했다. 약 1.67초에 Ragdoll, 약 5.67초에 OnGround가 관측됐으며, 후속 W 이동은 최대 약 296.28 UE 단위/초 후 정지 속도 0으로 돌아왔다. 렌더 캡처는 기상한 캐릭터를 보여 준다. 입력 키와 관측 callback은 해제했다.

Physics Control이 켜진 캐릭터는 OnGround에서도 pelvis의 `is_simulating_physics`가 true일 수 있었다. 따라서 그 플래그 하나로 Ragdoll 진입/종료를 판정하지 않는다. 이동 모드, 실제 AnimInstance 상태·Montage, 캡슐/Actor와 본 포즈를 함께 본다. 애니메이션·물리·캡슐의 제어권을 같은 값으로 취급하면 기상 성공을 오판할 수 있다.

## 재사용 조건과 다음 경계

1. 참가자별 Skeleton, 상호작용 데이터, Montage, Notify와 필요한 MotionWarping·PoseSearch·PhysicsControl·Mover 플러그인을 함께 확인한다.
2. 진입 가능 조건과 실제 후보 선택을 확인한 뒤 동반 애니메이션을 시작한다.
3. 시작 요청, 실제 활성 Montage, 종료 콜백, 이후 이동/물리 상태를 구분한다.
4. 실패 시 각 참가자의 현재 Montage·이동 모드·Actor/mesh 위치를 먼저 읽는다. 불확실한 원 재생 요청을 자동 재실행하지 않는다.

네트워크 참가자 동기화, 파괴된 대상의 정리, 모든 거리·체형, 부분 물리 충격과 각 프로파일의 튜닝은 이번 대표 결과로 완료 처리하지 않는다. 표준 엔진 플러그인은 해당 기능의 정상 의존성이다. Cubeless나 공식 MCP를 런타임에서 호출하도록 에셋을 작성하지 않았다.

## Look-At과 Control Rig의 실제 대상 반응

설치본 `/Game/IsolatedExamples/LookAtPOI/`의 실제 연결은 다음과 같다.

1. `PC_LookAtPOI`의 `Zone Was Selected` 이벤트가 캐릭터·AnimInstance·Gizmo를 선택한다.
2. `Set Look At Target On Anim Instance`가 Gizmo 또는 플레이어 위치를 AnimInstance의 목표로 전달한다.
3. `ABP_LookAtPOI_Char`가 DA의 body-part 설정, Curve 기반 weight/limit, `CR_AdjustToPOI` Control Rig를 포즈에 적용한다. `LookAtPOI Enable Additive`가 제어하는 별도 additive pose correction 경로도 있다.

원본의 `LookAtPOI Enabled`·`LookAtPOI Target`는 일반 Editor 속성 setter가 거절했다. 실제 값이 바뀌지 않은 것을 확인했고 그 제한을 해제하지 않았다. 대신 소유 `ABP_LookAt_Study`에만 공개 `BlueprintEditorLibrary.set_blueprint_variable_instance_editable`을 적용해 두 변수를 편집 입력으로 노출했다. 원본 Blueprint는 보존했다. 이 변경은 **예제 에셋의 입력 계약 확장**이며 MCP의 비공개 속성 우회가 아니다.

소유 `L_LookAtLab`의 중앙 캐릭터 `Study_LookAt_Center`는 위 소유 AnimBP를 참조하도록 저장했다. PIE에서 같은 소유 클래스에 왼쪽/오른쪽 월드 목표를 각각 약 4초 유지하고 비활성화했다. 14.33초 동안 실제 머리·척추 회전과 목표 값을 관측했으며 `lookAt Traget H Angle`은 약 +38.66° / -38.66°로 바뀌었다. 기본 idle 포즈 위에서 머리의 회전도 바뀌었다. 종료 후 원래 enabled/target 값과 callback을 복원했다. 별도 additive correction 스위치는 false였으므로 그 경로까지 실행했다고 주장하지 않는다.

렌더 캡처는 실제 LookAt 장면과 캐릭터 표시를 입증한다. 캡처는 비동기 예약 직후 목표를 전환한 경계에서 수행됐으므로 파일의 left/right 이름을 정확한 촬영 프레임의 방향 근거로 쓰지 않는다. 방향 반응의 근거는 전환 전후에 수집한 실제 목표·본 회전 표본이다. 가까운 고정 카메라의 품질 비교, 모든 body-part·limit quadrant·가산 곡선 튜닝은 후속 범위다.

소유 예제를 다른 프로젝트로 옮길 때는 ControlRig·AnimGraphRuntime과 원본의 DA/Struct/Curve·Skeleton·애니메이션 의존성도 함께 옮긴다. 목표는 월드 좌표이며 actor 이동, mesh 회전과 혼동하지 않는다. Euler yaw가 -180°/180°에서 감기므로 단순 산술평균으로 방향 오차를 판정하지 않는다.
