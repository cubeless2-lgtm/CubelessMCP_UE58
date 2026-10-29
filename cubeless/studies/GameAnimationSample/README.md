# Game Animation Sample 학습

설치된 샘플을 직접 분석하고 UE 5.8 애니메이션 작업에 재사용하는 학습이다. 후속으로 소유 맵의 1대1 전투 실험을 추가했으며 완성된 게임의 구현을 뜻하지 않는다. 소스 지원 계열은 UE 5.8.x이며, 현재 실행 근거는 **UE 5.8.4, CL 58902052**에 한정한다.

## 환경과 출처

- 프로젝트: `UnrealMCP5_8_3.uproject`. 별도 샘플 프로젝트를 다시 설치하지 않았다.
- 설치본: `/Game/Blueprints`, `/Game/Characters/UEFN_Mannequin`, `/Game/Levels`, `/Game/IsolatedExamples`.
- [Epic의 UE 5.8 업데이트 설명](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8).
- [샘플 개요와 구조](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine), [Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine).
- 설치 소스: `Engine/Plugins/Animation/PoseSearch/Source/Runtime/`, `Engine/Plugins/Chooser/Source/Chooser/`, `Engine/Plugins/Animation/AnimationWarping/Source/Runtime/`.

일반 샘플 문서의 이전 CMC 설명을 설치본의 기본값으로 단정하지 않는다. 실제 `DefaultLevel`의 기본 PIE 폰은 `SandboxCharacter_Mover`였으며, 기본 `locomotionSetup=1`에서 Blend Stack 경로를 사용했다. CMC와 Mover의 선택·전환 경로를 구분해서 본다.

## 현재 확인한 결과

| 작업 | 실제 근거와 범위 |
|---|---|
| 샘플 실행 | 공식 MCP로 PIE 시작·종료, 보이는 에디터에서 캐릭터 렌더링 확인 |
| 기본 이동 | 14초 입력 시나리오, 127개 표본. Walk·Run·Sprint, Idle·Moving, OnGround·InAir, Aim 전환과 정지 복귀 관측 |
| 애니메이션 선택 | 기본 Mover는 `blendStackInputs.anim`을 읽는다. 비활성 MM 경로의 `currentSelectedAnim=None`을 선택 실패로 판정하지 않는다 |
| 그래프 | CMC·Mover의 실제 AnimGraph 노드·핀·연결·설정 수집. Motion Matching, Pose History, Slot, Offset Root Bone, Inertialization, Dead Blending 확인 |
| 소유 데이터 사용 | CMC에 소유 `ABP_CMC_Study`·15Hz Schema·고정 Walk DB를 적용해 실제 DB 선택과 이동 포즈 확인 |
| 리타기팅 | 소유 AnimBP를 적용한 TwinBlast에서 리타게터 비동기 준비 후 이동 포즈·발-골반 상대 변화와 렌더 화면 확인 |
| Montage | 실제 종료 delegate로 interrupted=true/false 구분, 후속 기본 이동과 키·delegate 정리 확인. 기록기 철자 오류의 원시 실패는 별도 보존 |
| 물리 | Ragdoll 진입→SpaceBar 기상→OnGround→이동·정지, 실제 포즈와 렌더 확인 |
| 다중 캐릭터 | 실제 접근·E 입력→Tackle A/V 동반 Montage→두 캐릭터 Ragdoll, 대상 기상 확인 |
| 환경 | 소유 15도 램프의 실제 오르기·정지·렌더 확인. slopeAngle≈0 관측과 발 보정 품질 판정은 분리 |
| Traversal | 소유 맵에서 100cm Hurdle·250cm Mantle 목표 도달, 명시적 취소·충돌 복원, 목표 생성/갱신·weak 참조 소멸·명시적 제거 확인 |
| 발 보정 비교 | 15도 램프의 실제 Rig 적용·해제·재적용, 뼈와 지면 관계 관측. 소유 Chooser 클래스 문맥 수리 및 새 프로세스 로드 확인 |
| 근접전 첫 연결 | 소유 맵의 이동·단일 공격/연타 거절·피격·체력, 빗맞음·타격 전 중단·표적 소멸·사망 후 처리 확인 |
| 1대1 전투 | 별도 소유 맵에서 적의 접근·예고·공격·회복, 플레이어 회피·피격·사망과 실제 AI 상대 회피→반격 확인. HP·예고 표시와 새 프로세스 실행 확인 |
| Look-At | 소유 편집 입력을 추가한 AnimBP에서 좌·우 목표에 따른 각도 ±38.66°와 머리 회전, disable·값 복원 확인 |

이 표는 전체 애니메이션 API나 모든 고급 지원의 완료 판정이 아니다. 기본 동작, 에셋 저장, 런타임 관측, 패키징은 각각의 근거로 판단한다.

## 소유 예제

`Content/Study/GameAnimationSample/` 아래에 다음을 보관한다.

- `Animation/ABP_CMC_Study`: 원본 pose chain을 유지하고 소유 Walk DB를 고정 선택하도록 만든 예제. 컴파일·저장과 실제 CMC 실행 확인.
- `Animation/ABP_Mover_Study`: 원본 구조를 보존한 컴파일·저장 복사본. 별도 변경 없이 보관하는 비교 기준이다.
- `Selection/PSS_Study15Hz`: `PSS_Default`의 복사본으로 샘플링 주기를 30→15Hz로 변경했다.
- `Selection/PSD_StudyWalk`: Sparse Walk Loops DB 복사본으로 위 소유 Schema를 참조한다. 소유 CMC 예제에서 실제 선택 확인.
- `Animation/ABP_Retarget_Study`: GenericRetarget 복사본. TwinBlast에서 비동기 초기화 이후 실제 포즈 확인.
- `Animation/ABP_LookAt_Study`: 원본 LookAt 복사본에 enabled/target 두 편집 입력을 명시적으로 노출했다.
- `Maps/L_TraversalLab`: 낮은 장애물·높은 장애물과 Movable `Study_WarpAnchor`를 둔 소유 복사본. 기본 PlayerStart는 낮은 장애물 앞이다.
- `Maps/L_AnimationLab`, `Maps/L_PhysicsLab`, `Maps/L_LookAtLab`: 원본 DefaultLevel·RagdollLevel·LookAtPOI_Level의 소유 복사본. 첫 맵에 램프가 있고 마지막 맵의 중앙 `Study_LookAt_Center`는 소유 LookAt AnimBP를 참조한다.

위 **10개** 예제는 샘플의 Skeleton·애니메이션·Chooser·게임 모드 등 기존 샘플 에셋에 의존한다. 외부 프로젝트에 이 파일들만 복사하면 완성되는 배포물이 아니다. 필요한 표준 엔진 플러그인과 에셋 의존성까지 이전한다. 샘플 전체의 추가 복제나 배포 패키징은 하지 않는다. 새 출력에는 Cubeless·공식 MCP 클래스나 런타임 실행 의존성을 추가하지 않는다.

후속으로 발 보정 AnimBP 2개·맵 1개·Chooser 2개와 근접전 Montage 2개·Blueprint 2개·GameMode 1개·맵 1개를 추가했다. 이 21개를 보존하고 1대1 전투용 Blueprint 2개·GameMode 1개·맵 1개를 별도로 추가해 전투까지의 보존 기준은 **25개**다. 이후 ArenaUI 에셋 11개와 생성 PNG를 추가해 현재 소유 에셋은 **36개**이며 기존 25개 파일은 보존했다. 발 보정·근접전·전투와 UI의 실제 검수 범위는 FootWarping/Melee/Duel/ArenaUI 문서를 따른다. 새 프로세스 로드와 저장·재로드 근거를 구분하며, 에셋 수를 전체 지원 완료율로 사용하지 않는다.

## 주제별 재사용 문서

- [Locomotion](Locomotion.md): 입력·CMC/Mover·업데이트·Trajectory·경사.
- [MotionMatching](MotionMatching.md): Chooser·Schema/DB·검색 채널·두 선택 경로·고정 DB 예제.
- [Transitions](Transitions.md): Blend Stack·레이어·Montage/Slot/Notify·실제 중단과 자연 종료.
- [Retargeting](Retargeting.md): 체형 재사용·부모 메시·비동기 준비·부분 변경 실패.
- [Interactions](Interactions.md): 동반 상호작용·Physics Control·기상·Look-At/Control Rig.
- [Traversal](Traversal.md): 장애물 trace·ledge·Hurdle/Mantle·중단과 목표 참조 수명.
- [FootWarping](FootWarping.md): 설치본의 보정 경로 차이, 실제 보정 전환·지면 관측, 복사 Chooser 문맥 수리.
- [Melee](Melee.md): 3인칭 근접전 Blueprint와 소유 실행 맵, 공격·피격·복귀·실패 경계의 실제 검수.
- [Duel](Duel.md): 별도 1대1 실험 맵의 적 AI·회피·피격·사망, 실제 회피 후 반격과 조작법.
- [ArenaUI](ArenaUI.md): 독립 전투 사본의 실제 UMG HUD·UI 머티리얼·생성 문양·승패/재시작, 세로/가로 배치와 공식 MCP Editor 저작 API.
- [MCP](MCP.md): 실제 도구 대응·C++ 읽기 검수 수정·실패 경험·측정 조건·남은 API 공백.

## 짧은 사용 절차

1. 기존 dirty 에셋을 보존하고 필요한 샘플 플러그인이 활성화·로드됐는지 확인한다.
2. `L_AnimationLab`을 열고 PIE에서 W·Ctrl·Shift·Space로 기본 이동과 램프를 확인한다. 현재 설치본의 기본 경로는 Mover다.
3. `L_PhysicsLab`에서 R→Space로 래그돌/기상, 다른 캐릭터에 접근한 뒤 E로 실제 선택된 상호작용을 확인한다. E가 항상 Tackle을 선택한다고 가정하지 않는다.
4. `L_LookAtLab`의 중앙 예제는 소유 LookAt AnimBP다. PIE의 실제 AnimInstance에 enabled/월드 target을 전달하고 머리 포즈를 본다. 원본의 같은 변수는 일반 Editor setter가 거절할 수 있다.
5. `L_TraversalLab`에서 W로 낮은 장애물에 접근하고 SpaceBar로 Hurdle을 확인한다. 높은 장애물은 소유 PlayerStart를 y=-1600으로 옮겨 접근하며, 필요한 조작·Warp 목표·중단 판정은 [Traversal](Traversal.md)을 따른다. 일반 에디터가 비활성 상태로 심하게 느려지면 같은 동작의 품질을 보증할 수 없다.
6. 도구 호출 예제와 비동기 종료·해제 절차는 주제 문서를 따른다. 실패 시 현재 Actor/AnimInstance·컴파일·dirty·저장 상태를 먼저 읽고 불확실한 요청을 반복하지 않는다.
7. `L_MeleeLab`은 소유 GameMode·플레이어·표적이 저장된 근접전 예제다. PIE에서 WASD·좌클릭으로 사용한다. 발 보정 비교용 `L_FootWarpLab`과 Alpha 전환은 FootWarping 문서의 통제 범위를 따른다.
8. `L_DuelLab`은 실제 적 AI를 상대하는 1대1 전투 예제다. WASD·마우스·좌클릭 공격·우클릭 회피를 사용한다. HP와 `WINDUP` 표시를 보며 회피 후 반격한다. 세부 규칙과 확인 범위는 [Duel](Duel.md)을 따른다.
9. `ArenaUI/L_ArenaUI`는 위 전투의 독립 사본에 실제 UMG를 붙인 예제다. 같은 전투 조작과 Tab 효과 전환, 결과 화면의 클릭/Enter 재시작을 사용한다. 기존 25개에 새 11개 에셋과 생성 PNG를 추가했다. 상세 범위는 [ArenaUI](ArenaUI.md)를 따른다.

급회전·방향 전환과 점프는 대표 입력에서 관측했지만, 모든 보행 각도·체형·네트워크·moving-platform 전체 Warp 수명·동기 오차·LOD/대규모 성능·오프라인 리타기팅은 후속 심화 범위다. 표본 실행의 성공을 이 전체 범위의 완료율로 계산하지 않는다.

## 2026-10-30 게시와 MCP 비활성 패키징

UMG 저작 4개와 Editor 데이터 도구 12개, Niagara 소유 scratch 정리·사전 거절 상태 보강, 읽기 전용 그래프의 owner+PinId 처리를 게시 범위에 포함했다. 기존 검증 빌드 후보와 현재 Native 소스가 같고, 실제 프로젝트에 적용된 DLL도 같은 해시임을 확인해 완료된 동작 검증은 반복하지 않았다. 게임 에셋 36개·생성 PNG 1개·이 폴더의 학습 Markdown을 포함하며 원시 검증 기록과 빌드 출력은 제외한다.

별도 `StudyCook58` 프로젝트에서 CubelessMCP_UE58·ModelContextProtocol·MCPClientToolset·AllToolsets·ToolsetRegistry를 명시적으로 비활성화했다. 샘플의 표준 런타임 플러그인은 유지했다. 기존 AssetManager의 GameFeatureData 스캔에는 **GameFeatures 활성화가 필요**하므로 격리 프로젝트에만 켰다. 원본 `.uproject`와 Config·게임 에셋은 바꾸지 않았다. 현재 프로젝트 설정으로 같은 패키징을 할 때도 이 전제 조건을 맞춘다.

Win64 Development 빌드·Cook·Stage·Archive는 성공했다. 최종 BuildCookRun 152.17초, Cook 프로세스 91.52초이며 Cook 오류 0건·기존 샘플 경고 221건이다. 초기 Editor 영수증 누락과 GameFeatures 비활성 실행은 실패로 보존했고, 영수증 빌드·필수 플러그인 활성화 후의 성공과 구분한다. 경고 전체를 플러그인 결함으로 집계하거나 경고가 없다고 표현하지 않는다.

실제 IoStore 목록에서 학습 에셋 36개와 맵 8개를 확인했다. 게임 타깃의 BuildPlugins와 실제 Pak·IoStore·스테이징 파일 경로에 금지 MCP 플러그인 산출물은 없었다. 보존 대상 에셋·설정의 해시도 유지됐다. 검증용 자동 빌드 scaffolding은 Intermediate 아래에만 있으며 새 게임플레이 C++를 작성하지 않았다.

독립 실행 파일은 D3D12/SM6로 시작했지만 Windows 플랫폼 설정이 우선 적용돼 기본 ThirdPerson 맵을 열었다. **ArenaUI를 포함한 학습 맵의 독립 게임 입력·화면 검수는 Windows 잠금 해제 후 계속할 상태**다. 잠금 화면에는 입력하지 않았다. 이전 Editor/PIE의 전투·HUD·Retry 성공 증거는 유지하며, 이를 이번 패키지의 실제 ArenaUI 실행 성공으로 바꾸지 않는다. 맵을 지정해 실행하려면 패키지의 `StudyCook58.exe /Game/Study/GameAnimationSample/ArenaUI/L_ArenaUI -windowed -ResX=1280 -ResY=720`을 사용한다.

로컬 재현·실패·패키지 목록·해시 근거는 `cubeless/records/operations/study-release-20261030/report.json`에 통합했다. 검증 프로젝트·캐시·DLL/PDB·패키지·원시 기록은 Git 제외 대상이다. 완성 게임의 전체 검수나 모든 애니메이션 지원 범위의 완료 판정은 아니다.

## 기록 경계

학습 Markdown은 이 폴더에 둔다. 기본 학습의 보조 Python과 원시 호출·성공·실패 결과는 로컬 `cubeless/records/operations/game-animation-sample-study-20261028/`에 두며 Git에 포함하지 않는다. 후속 발 보정·근접전·1대1 전투 기록 위치는 각 주제 문서에 연결한다. 원본 에셋의 파일 저장은 요청하지 않았다. 학습 구현 당시에는 Git 쓰기를 수행하지 않았으며, 2026-10-30에는 사용자 승인으로 위 에셋·학습 문서와 필요한 플러그인 소스를 게시한다.

첫 입력 기록기는 Native 호출에 `pie_instance=-1`을 명시해서 사전 거절됐다. Native는 키가 전달되기 전에 `mutation_attempted=false`, `changed=false`를 반환했다. 원 실패 기록을 보존하고, 유일 PIE 선택을 뜻하는 인자 생략으로 수정한 별도 실행에서 정상 동작을 확인했다. 이 테스트 코드의 계약 오용을 플러그인 결함이나 불확실 요청 재시도로 분류하지 않는다.
