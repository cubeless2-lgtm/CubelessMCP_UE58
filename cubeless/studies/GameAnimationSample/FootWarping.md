# 회전·보폭·경사·발 보정 비교

UE 5.8.4 / CL 58902052에서 **15도 램프의 정지 자세와 발 보정 적용·해제·재적용을 실제 PIE로 확인했다.** 소유 AnimBP의 Chooser 클래스 문맥 결함도 수정하고 저장·새 프로세스 로드를 확인했다. 기존 램프 통과 성공과 이번 보정 비교의 증거는 구분한다.

## 설치본과 공식 설명을 구분한다

[Epic Pose Warping 설명](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine)은 Orientation·Stride·Slope Warping의 목적, Manual/Graph 평가 방식과 IK 뼈 설정을 설명한다. 설치 헤더는 `Engine/Plugins/Animation/AnimationWarping/Source/Runtime/Public/BoneControllers/AnimNode_OrientationWarping.h`, `AnimNode_StrideWarping.h`, `AnimNode_SlopeWarping.h`다. 문서 자체도 Slope Warping 노드를 실험 대상으로 설명한다.

실제 설치 샘플의 주요 경로는 다음과 달랐다.

- Mover의 `Procedural_Feet`에는 Control Rig, Basic IK와 이동 모드별 포즈 선택이 있다. Rig에 Ground Normal, AllowFootPinning, AllowSlopeWarping, 지면 이동 delta와 reset 조건이 전달된다.
- CMC AnimGraph에는 Foot Placement가 있다.
- 양쪽에는 Offset Root Bone이 있지만, 조사한 AnimGraph와 procedural layer에 전용 Orientation/Stride/Slope Warping 노드가 이미 연결된 것은 아니었다.

따라서 존재하지 않는 노드의 Enable CVar를 바꾸고 그 효과를 확인했다고 기록하지 않는다. 전용 Orientation/Stride 노드의 별도 실시간 구현과, 현재 샘플의 회전·보폭 조정 경로 비교는 남아 있다.

## 저장한 소유 예제

| 에셋 | 구현과 현재 확인 수준 |
|---|---|
| `/Game/Study/GameAnimationSample/Animation/ABP_Mover_FootStudy` | 기존 Control Rig 포즈를 layer 출력으로 연결. 공개 인스턴스 변수 `StudyFootCorrectionAlpha`로 Rig Alpha를 제어하며 저장 기본값은 1. 실제 적용·해제·재적용 확인 |
| `/Game/Study/GameAnimationSample/Animation/ABP_Mover_FeetOffStudy` | 해당 layer가 입력 포즈를 통과하는 비교용 복사본. 수정 후 새 프로세스에서도 실제 실행 확인 |
| `/Game/Study/GameAnimationSample/Maps/L_FootWarpLab` | 기존 소유 램프 맵의 복사본. 비교용 PlayerStart `(0,0,260)`, yaw 90을 저장 |
| `Selection/CHT_FootOn_PoseMatch`, `CHT_FootOff_PoseMatch` | 위 각 AnimBP를 문맥 클래스로 사용하는 소유 Chooser. 원본 Chooser는 보존 |

이 두 AnimBP는 **통제 비교용**이다. 원래 이동 모드별 발 보정 선택을 우회하므로 일반 게임 캐릭터의 모든 공중·래그돌·특수 상태에 그대로 적용할 완성형이 아니다. Rig 내부의 기존 AllowFootPinning/AllowSlopeWarping 조건은 유지했다. 실제 실행 시 해당 조건도 함께 관측해야 한다.

필수 엔진 플러그인은 샘플의 Mover/NetworkPrediction, ControlRig, AnimationWarping, PoseSearch 등 실제 에셋 의존성에 따른다. 활성화·로드 상태가 전제이며, 이 의존성 자체를 결함으로 분류하지 않는다.

## 실제 비교와 범위

같은 PlayerStart·램프·캐릭터 조건에서 두 소유 AnimBP의 실제 렌더와 `foot_l/r`, `ball_l/r`의 위치·지면 trace·수직 거리를 기록했다. 두 독립 실행의 idle 클립은 달랐으므로 그 차이를 순수 보정 효과로 집계하지 않았다. 이후 하나의 살아 있는 AnimInstance에서 Alpha를 1→0→1로 바꾸었다. 같은 클립이 확인된 해제/재적용 구간은 다음과 같다.

| 구간 | 실제 클립 | 왼쪽 ball bone의 지면 수직 간격 중앙값 | 오른쪽 ball bone의 지면 수직 간격 중앙값 |
|---|---|---|---|
| Alpha 0, 3.2~3.9초 | `M_Relaxed_Stand_Idle_Break_v08` | 약 1.40cm | 약 10.88cm |
| Alpha 1, 4.2~4.9초 | 같은 클립 | 약 3.14cm | 약 3.27cm |

각 구간은 14표본이며 Actor 위치는 같았다. 이는 **실제로 달라진 발 보정과 지면 관계의 관측**이다. 애니메이션 시간은 계속 흘렀으므로 동일 프레임의 순수 차이나 모든 자세의 품질 향상으로 일반화하지 않는다. 발 bone 원점은 신발 밑창이 아니며, 이 거리를 곧바로 침투량으로 사용하지 않는다. 처음 Alpha 1 구간은 다른 Idle Loop였으므로 위 수치 비교에서 제외했다. 끝난 뒤 Alpha와 재생률을 시작값으로 복원했다.

원래 보정 전환을 위해 재생률 0을 설정한 후보는 살아 있는 인스턴스 identity 검사에서 실패했다. 원 실패를 보존하고 소유 PIE를 종료했다. 이후에는 컴포넌트 재생률을 바꾸지 않고 Alpha만 전환했다. 이 후보 실패를 발 보정 엔진 자체의 실패로 분류하지 않았다.

초기 자동 승인 차단은 사용자의 명시적인 테스트 맵·PIE 승인 뒤 해소됐다. 이후 실행은 공식 MCP 8002의 소유 AnimRuntime58에서 수행했다. 보행 중 발 미끄러짐·급회전·모든 체형·전용 Orientation/Stride 노드·움직이는 지면·네트워크·성능·패키징은 이 정지 자세 비교에 포함하지 않는다.

## 복사한 Chooser 문맥 수리

원본 `CHT_MoverCharacterAnimations_PoseMatch`는 첫 문맥의 클래스를 원본 `SandboxCharacter_Mover_ABP_C`로 지정한다. AnimBP를 복사한 형제 클래스는 이 타입의 인스턴스가 아니므로 초기 실행에서 타입 오류가 났다. 그 상태를 완전 정상 실행으로 집계하지 않는다.

각 소유 AnimBP 전용으로 Chooser를 복사하고, 첫 `ContextObjectTypeClass`만 해당 소유 generated class로 바꿨다. 나머지 입출력 Struct 문맥은 유지했다. 공개 Editor 속성 API로 `K2Node_EvaluateChooser2.Chooser`를 소유 테이블에 연결했고, 기존 외부 연결 endpoint를 보존했는지 확인한 뒤 AnimBP 컴파일·Chooser 데이터 검증·저장을 수행했다. 원본 테이블 해시는 유지됐다. 수리 후 paired 실행과 새 프로세스의 해제 variant 실행 구간에는 Chooser 타입 오류가 없었다. 이 경험은 다른 복사본의 문맥 계약도 개별 확인해야 한다는 근거이며, 모든 샘플 Chooser를 수리했다는 뜻은 아니다.

## 저작 도구에서 확인한 주의점

- 공식 `BlueprintTools.find_node_types`가 반환하는 Unreal Array는 JSON 저장 전에 list로 변환한다.
- 일반 Blueprint 노드 메뉴를 조회한 소유 테스트 에디터에서 원본 Mover AnimBP가 dirty가 됐다. 원본 writer/save 없이 메뉴·그래프 API를 사용한 세션이었으며, 원본 파일 해시 불변을 확인하고 정확한 그 package만 디스크에서 reload했다. 메인 에디터와 원본 파일은 보존했다.
- 초기 빈 `ABP_Warping_ManualStudy`는 전용 노드 구현물이 아니므로 완료로 세지 않았다. 실제 빈 상태를 확인해 그 소유 임시 후보만 제거했다.
- SDK Python wrapper의 실패 반환과 실제 완료된 compile/save를 분리했다. owner 변경을 보고한 실패는 성공으로 고치지 않았고, 후속 읽기로 실제 에셋 상태를 확인했다.

로컬 호출·실패·관측기·소유 에디터 기록은 `cubeless/records/operations/game-animation-warping-melee-20261028/`에 보존한다. 이 폴더는 Git 제외 대상이다. 이번 변경은 새 MCP 도구나 C++·런타임 MCP 타입을 추가하지 않았다.
