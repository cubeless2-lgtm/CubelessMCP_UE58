# Motion Matching 근접전 통합

UE 5.8.4 CL58902052에서 기존 Motion Matching·리타기팅·근접전·ArenaUI 학습을 별도 실행 예제로 통합했다. 원본 샘플과 기존 ArenaUI는 수정하지 않았다. 신규 에셋 15개는 `Content/Study/GameAnimationSample/MotionMatchingCombat/`에 있다.

## 실행

1. 샘플에 필요한 PoseSearch·Chooser·AnimationWarping·IKRig 등 기존 프로젝트 플러그인을 활성화한 상태에서 사용한다.
2. `/Game/Study/GameAnimationSample/MotionMatchingCombat/L_MMCombat`을 열고 PIE를 시작한다. 맵에 저장된 `GM_MMCombat`이 새 플레이어를 생성한다.
3. WASD 이동, Ctrl 걷기, Shift 스프린트, 마우스 시점, 좌클릭 공격, 우클릭 회피, Q 락온·해제, Tab 효과 전환을 사용한다. 승패 화면에서는 Retry로 같은 새 맵을 다시 연다.

기존 `/Game/Study/GameAnimationSample/ArenaUI/L_ArenaUI`는 이전 예제로 그대로 남아 있다. 새 맵을 기존 맵에 덮어쓰지 않는다.

## 에셋과 호출 관계

| 에셋 | 역할 |
|---|---|
| `BP_MMCombatPlayer` | 기존 Arena Player의 자식이다. 기존 HUD/적의 부모 타입 참조를 유지하면서 UEFN 메시와 새 AnimBP를 사용한다. |
| `ABP_MMCombat` | 원본 CMC AnimBP의 Trajectory·Chooser·PoseSearch·PoseHistory·Blend Stack·Slot·발 보정 경로를 재사용한다. |
| `Selection/CHT_PoseSearchDatabases` 및 Dense/Sparse/ExtremeSparse | 원본 표를 보존한 선택표 사본이다. Context Class와 표 사이의 참조를 새 AnimBP/선택표로 연결했다. |
| `Selection/CHT_CMCCharacterAnimations` | 원본의 보조 선택 경로를 보존한 사본이다. 중첩 표도 복제 내부에 유지한다. |
| `RTG_Mannequin_to_UEFN` | Quinn 계열 전투 애니메이션을 UEFN 메시로 리타기팅하는 소유 Retargeter다. 원본 IK Rig는 참조만 한다. |
| `Animations/AM_StudyAttack_UEFN`, `MM_Attack_01_UEFN` | 리타기팅한 공격 Montage와 Sequence다. 원래 Notify/BPI 타격 계약을 유지한다. |
| `Animations/AM_StudyHit_UEFN`, `MM_HitReact_Front_Lgt_01_UEFN` | 리타기팅한 피격 Montage와 Sequence다. |
| `WBP_MMCombatHUD`, `GM_MMCombat`, `L_MMCombat` | 새 HUD·플레이어 생성·실행 맵이다. Retry 목적지도 새 맵이다. |

게임 스레드의 `BlueprintUpdateAnimation → Update_PropertiesFromCharacter`가 Character/CharacterMovement 값을 구조체로 수집하고, 원본의 애니메이션 업데이트 경로가 이를 소비한다. 전역 CVar를 바꾸지 않고 이 AnimBP의 LocomotionSetup만 Motion Matching 경로 0으로 유지한다.

플레이어의 활성 Tick은 `MMEnsureArenaHUD → StudyLockOnTick → MMStudyMeleeTick`을 호출한다. 여섯 개 void 계약은 원래 이름의 Event override에서 이름이 분리된 `MM*` 일반 함수로 연결한다. 이동 속도, 입력 처리, Montage 종료 정리와 HUD 확인은 Sequence의 독립 분기다. 한 분기의 거짓 조건이 다른 정리 작업을 막지 않는다.

공격은 Busy를 먼저 확정한 뒤 Montage를 재생한다. 실제 Notify가 원래 BPI로 들어와 메시·출처 Montage·유효 시각·Pending·생존 상태를 검사한 뒤 한 번만 타격한다. 회피와 라운드 종료는 `StudyAttackMontage`에 지정된 공격만 중단한다. 피격은 사망 여부와 잠금을 Montage 호출 전에 확정한다. 원래 어깨 카메라 SocketOffset `(0,75,20)`도 최초 한 번 적용한다.

## 실제 검증 결과

분리된 `UMGFix58` 검증 프로젝트에서 공식 MCP 8002로 실행했다. 이 프로젝트의 Content는 실제 프로젝트 Content를 공유한다. 최종 저장 후 **새 Editor PID 36120**에서 직접 맵을 열어 다음 12개 조건을 확인했다.

- 새 플레이어·UEFN 메시·AnimBP·HUD와 실제 Idle DB/애니메이션 선택.
- W 달리기 400cm/s, Ctrl 걷기 180cm/s, Shift 스프린트 600cm/s.
- D 방향 전환과 정지 후 Idle 복귀.
- Q 락온과 AnimBP의 Strafe 반영.
- 리타기팅한 공격 Montage의 실제 Notify 1회, 적 체력 100→80, 종료 후 Busy/Pending 해제.
- Notify 전 우클릭 회피로 공격을 취소하고 추가 피해 없이 상태 복원.
- 피해 25와 리타기팅한 피격 Montage, 락온 해제 후 회전 설정 복원.
- 라운드 종료·결과 HUD와 Retry 후 새 맵·플레이어·HUD·체력 100/100 복원.

별도 중요한 경계로 피해 1000 후 사망을 먼저 확정하고 공격을 거절하며, 피격 애니메이션 종료 후에도 MovementMode=None을 유지하는 것을 확인했다. 카메라 오프셋도 실제 인스턴스에서 확인했다. 마지막 검증은 Computer Use 없이 공식 MCP와 엔진 PIE 입력 API로 수행했다. 콜백과 눌린 키를 모두 해제하고 dirty/PIE 없이 검증 에디터를 정상 종료했다.

최종 플레이 구간에는 Chooser 문맥 오류와 PoseHistory 누락 오류가 없다. 기존 Editor 시작 시의 Automation/GameplayTags 진단은 별도 원시 로그에 보존하며, 전체 Editor 로그가 오류 0이라고 주장하지 않는다.

기존 Study 97개 파일, 원본 샘플 181개 파일, 참조한 원본 Chooser 5개 파일의 해시가 보존됐다. 새 Blueprint의 컴파일 상태, 저장된 선택표 문맥과 직접 패키지 참조도 새 프로세스에서 확인했다. 새 에셋에 Cubeless/MCP 패키지 직접 참조는 없다.

## 구현 중 확인한 실패와 해결

- 원본 Chooser는 원본 CMC AnimBP 클래스만 받아 사본 AnimBP를 거절했다. 원본을 수정하지 않고 연결된 다섯 표와 클래스 문맥을 함께 복제·연결했다.
- Chooser의 `ResultsStructs`/`FallbackResult`는 설치 Python에서 보호 속성이다. Editor 전용 `CubelessStudyChooserLibrary.RemapNewStudyChooser`가 새로 복제한 미저장 Study Chooser 내부의 참조만 엔진 `FArchiveReplaceObjectRef`로 변경한다. 저장된 대상은 거절하고 저장·삭제·원자적 롤백을 제공하지 않는다. 중복 dirty 알림 콜백은 발생시키지 않는다. 비반영 compiled binding 캐시는 별도 검증/콜드 로드 경계가 필요하다.
- Chooser의 클래스 문맥을 바꾸면 입력 핀 이름도 바뀐다. 새 문맥 핀에 실제 self를 연결하고 이전 orphan 연결을 제거한 뒤 컴파일했다.
- 동일한 메뉴 이름의 Pawn/Controller `GetControlRotation`을 잘못 선택하면 자동 변환이 `LastHitBy`를 참조할 수 있다. Pawn 선언 클래스를 명시한 노드로 교체했다.
- 초기 함수그래프형 override와 Factory의 기본 비활성 Tick을 둔 상태에서는 콜드 실행 시 새 HUD가 생성되지 않았다. 최종안은 명시적인 활성 Tick과 Event override → `MM*` 일반 함수로 구성했다. 부모 함수의 `FUNC_Final` 가설은 실제 flags로 배제했으며, 내부 lookup 원인을 확정했다고 표현하지 않는다.
- DSL의 positional 인수만으로 `PlayAnimMontage`를 작성하면 `AnimMontage` 핀이 비어 있을 수 있다. 공격·피격 핀을 명시적으로 설정하고 취소/완료 검사는 지정 멤버에 직접 연결했다.
- decompile→transpile 과정에서 독립적인 종료 검사가 입력 조건의 `else`에 연결됐다. 실제 실행 핀을 조사한 뒤 Sequence 분기로 분리했다. 이름이나 컴파일 성공만으로 흐름 동등성을 판정하지 않는다.
- 같은 세션에서 복제한 World를 저장한 뒤 다시 여는 초기 요청은 standalone World 보존 상태로 GC 오류가 났다. 저장 파일을 확인한 뒤 새 프로세스에서 직접 맵을 열었다. 원 요청을 반복하지 않았다.
- 초기 검수기의 HUD 준비 전 참조, 전투 중 Retry를 허용한다고 본 가정과 짧은 입력 관측은 원 기록으로 남겼다. 최종 검수는 플레이 시작 전에 관측을 등록하고 실제 변경·완료 상태를 확인한다.

## 지원 범위와 후속 범위

지상 3인칭 이동과 기존 단일 적 근접전·회피·락온·피격·승패/Retry를 사용할 수 있다. 원본 전체 샘플의 기능을 이 예제가 모두 재현한 것은 아니다. 현재 수집기는 `JustLanded`/`LandVelocity`, crouch와 플랫폼 이동 델타를 갱신하지 않는다. 점프·착지 품질, Traversal·래그돌·상호작용·멀티플레이·다수 적·전 체형 성능은 별도 확장 범위다.

게임플레이 C++나 MCP 런타임 모듈은 추가하지 않았다. 새 C++는 Editor 전용 복제 보조 함수뿐이다. 이전 MCP 비활성 패키징은 이전 예제의 근거이며, **이번 새 맵의 Cook/패키징/독립 게임 실행 증거로 확대하지 않는다.** 이번 결과는 에디터 실제 PIE와 저장·새 프로세스 재로드 범위다.

사용 중 실패하면 현재 Player/AnimInstance, AttackBusy/HitPending/IsDead/ArenaRoundOver, 실제 Montage, 컴파일·dirty·저장 상태를 먼저 확인한다. 결과가 불확실한 수정 요청을 반복하지 않는다. 원 API 실패와 별도 저장·복구 결과도 구분한다.

## 재사용한 학습과 근거

- [Motion Matching](MotionMatching.md): 동적 선택표·Schema/DB·PoseHistory.
- [Retargeting](Retargeting.md), [Transitions](Transitions.md): IK Retargeter·Montage·Slot·Notify.
- [Melee](Melee.md), [ArenaUI](ArenaUI.md): 기존 공격/취소/사망 보호·BPI Notify·락온·HUD/Retry.
- [FootWarping](FootWarping.md): 복제 Chooser의 문맥 수리와 소유 에셋 보존.
- 설치 엔진의 `Chooser.h/.cpp`, `ChooserPropertyAccess.h`, `ArchiveReplaceObjectRef.h`, `Pawn.h`, BlueprintEditorLibrary/BlueprintGraphEditor와 KismetCompiler/ScriptCore 구현을 대조했다.
- 원시 빌드·실행·실패·콜드 재로드·보존 근거: `cubeless/records/operations/motion-matching-combat-20261030/report.json` 및 같은 폴더의 원시 결과. 이 로컬 검증 폴더는 Git 배포 대상이 아니다.

Git stage·commit·push는 수행하지 않았다.

## 2026-10-31 전투 전환 안정성 추가 확인

UE 5.8.4의 소유 `UMGFix58`에서 공식 MCP 8002로 기존 맵을 실행했다. 공격 도중 비치명 피격으로 Busy/Pending이 해제되고 오래된 Notify와 경직 중 공격이 거절되는 것을 확인했다. 연속 피격은 경직 종료 시간을 연장하며, 종료 후 누르고 있던 이동 입력과 Walking이 복원됐다. 이어 실제 입력으로 재공격해 실제 Notify 1회·적 HP100→80·공격 종료 후 이동 복원을 확인했다. 새 정상/실패 경계 3건은 초기 프레임 안정화 포함 7.5초에 통과했다. 이 값은 성능 벤치마크가 아니다.

첫 실행은 첫 PIE 프레임 지연 뒤에도 경직 중이라고 가정한 검수 조건에서 실패했다. 원 실패와 실제 상태 조회를 보존했으며, 게임 상태는 HP75·Walking·Pending 없음이었다. 검수기는 초기 프레임 안정화 후 게임 시간으로 연속 피격 시점을 판단하도록 수정했고, 게임의 경직 시간과 보호 조건은 변경하지 않았다. 기존 중복/잘못된 Notify 거절·회피 취소·사망 잠금·Retry 복원 4건과 저장·재로드 성공 증거를 재사용했다.

이번 범위에서 게임 구현 결함이 확인되지 않아 에셋 수정·저장과 새 C++ 빌드는 없었다. 기준 Study 54개 파일의 해시가 동일하며, 관측기·입력을 정리하고 PIE/dirty 0에서 소유 PID8236의 정상 종료와 `LogExit: Exiting`을 확인했다. 새 경계 검증에서는 적 AI를 일시 중지했으므로 전체 AI 전투, 멀티플레이, 점프·Traversal, 새 패키징 검증으로 확대하지 않는다. 근거는 `cubeless/records/operations/combat-stability-20261031/report.json`과 같은 폴더의 첫 실패·후속 조회·최종 성공 결과다.


## 2026-10-31 Niagara 타격 이펙트 소유 복사본

원래 MotionMatchingCombat·ArenaUI·샘플 에셋은 보존하고 로컬 검수 폴더에 별도 맵·Player/HUD/GameMode 복사본과 Opponent/Burst Blueprint를 연결했다. 실제 체력 감소 뒤 이펙트를 만들며 중복 Notify와 회피 취소는 추가 이펙트/피해를 만들지 않는다. User.HitColor/HitSize의 setter 전달과 Scratch 모듈 소비를 구분하고 CPU/GPU 렌더에서 색상·크기 반영을 확인했다. 생성 콜백 이후 receiver/FX 수명, OnSystemFinished의 sender와 Ending 상태, EndPlay 정리와 활성화 시각 기준 방출 중단을 보완했다. 저장 정책·현재 호출 이름·로컬 검수 맵·한계는 [운영 가이드](../../docs/operations.md#niagara-실무-경로-보강--2026-10-31)에 유지한다. GPU 프레임의 정확한 내부 수치와 새 패키징 완료를 주장하지 않는다. 원시 근거는 records/operations/niagara-practical-20261031/report.json이다.

후속 P2 수정에서는 GPU 정상 경고 상태를 준비 판정에 포함하고, Burst의 정상 완료·활성화 실패·조기 종료·활성화 미확인을 별도 상태로 구분했다. `Completed`는 공유 활성화 수락 관측→자체 방출 중단→동일 FX 완료 콜백의 정상 흐름에서만 참이다. 취소는 결과를 먼저 확정하며 `HitEffectCount`는 성공한 방출 수가 아닌 생성·시도 수다. Solo 경로는 관측 미확인을 유지하고 GPU 프레임 진행을 주장하지 않는다.
