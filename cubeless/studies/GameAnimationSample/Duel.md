# 3인칭 1대1 근접전 실험

UE 5.8.4 / CL 58902052에서 기존 Melee Study를 복사해, 적의 접근·공격 예고·타격·회복과 플레이어의 공격·회피·피격·사망을 연결했다. 공식 MCP 8002로 에셋을 작성하고 실제 PIE 입력으로 확인했다. 표준 Engine Blueprint로 동작하며 새 프로젝트 C++·MCP API·DLL은 추가하지 않았다.

후속 리뷰에서 확인한 **Montage 콜백 취소 후 공격 재생이 남는 P2를 2026-10-29에 수정했다.** 플레이어 `TryStudyAttack`과 적 `EnemyBeginAttack`은 재생 호출 후 피격·회피·사망 등으로 승인이 취소됐으면 유효한 현재 AnimInstance에서 `AM_StudyAttack`만 지정해 멈춘다. 피격 Montage와 확정된 상태는 유지한다. 기존 세 재현, 공격 시작 콜백의 피격 전환, 치명타 취소가 통과했고 새 프로세스에서도 취소와 실제 회피 후 반격을 확인했다. 원 실패 `review-player_hurt.json`·`review-player_evade.json`·`review-enemy_hurt.json`은 보존하며, 후속 성공으로 덮어쓰지 않는다. 현재 근거는 기존 기능군 `report.json.cancel_cleanup_fix`에 연결했다.

## 실행과 소유 에셋

현재 프로젝트에서 `/Game/Study/GameAnimationSample/Maps/L_DuelLab`을 열고 PIE를 시작한다.

| 입력 | 동작 |
|---|---|
| WASD / 마우스 | 기존 ThirdPerson 이동·시점 조작 |
| 좌클릭 | 전방 1회 근접 공격 |
| 우클릭 | 이동 입력 방향으로 회피. 입력 방향이 없으면 뒤로 이동 |
| PIE 종료 후 다시 시작 | 체력·사망·배치 상태 초기화 |

| 새 에셋 | 구성 |
|---|---|
| `Duel/BP_DuelPlayer` | 기존 `Melee/BP_MeleeStudyCharacter`의 독립 복사본. 체력·피격·회피·사망 추가 |
| `Duel/BP_DuelEnemy` | 기존 `Melee/BP_MeleeStudyTarget`의 독립 복사본. 표준 AIController와 단순 접근·공격 상태 추가 |
| `Duel/GM_DuelStudy` | 소유 게임 모드 복사본. 기본 폰은 위 플레이어 |
| `Maps/L_DuelLab` | 소유 Melee 맵 복사본. 복사된 고정 표적만 새 적으로 교체. 시작 거리는 약 450cm |

상대는 자동으로 접근한다. 머리 위 `HP`, `WINDUP`, `ATTACK`, `APPROACH / RECOVER`, `DEFEAT`·`VICTORY` 표시는 표준 HUD의 개발용 debug text다. 완성된 게임용 체력 바나 메뉴는 아니다. 어깨 시점은 첫 Tick에서 소유 플레이어의 CameraBoom SocketOffset `(0,75,20)`을 한 번 설정한다. 부모 BeginPlay나 입력 초기화를 덮어쓰지 않는다.

기존 Melee·발 보정·이전 Study 21개 에셋의 파일 해시를 보존했다. 기존 ThirdPerson·원본 샘플·공용 GameMode·기본 맵 설정을 변경하지 않았다. 소유 Montage `AM_StudyAttack`·`AM_StudyHit`를 재사용한다.

## 동작과 실패 경계

플레이어 체력은 100, 공격력은 20이며 적의 체력은 100, 공격력은 15다. 적은 중심 거리 190cm 밖에서 직접 이동 입력으로 접근하고, 범위에 들어오면 0.65초간 공격을 예고한다. 공격 Montage 시작 0.25초 뒤 전방 Sphere Trace를 한 번 수행하고, Montage 종료 후 0.55초 회복한 뒤 다음 행동을 시작한다. 추적 중에만 방향을 갱신하므로 예고 후 공격은 자동으로 방향을 따라 꺾지 않는다. Montage의 원래 이동과 캡슐 충돌은 유지한다.

회피 이동은 최대 0.4초이며 속도는 초당 650cm다. 처음 0.25초만 무적이고, 이후에는 회피 동작 중에도 피해를 받을 수 있다. 바닥 상태에서만 시작하며 Actor 이동은 sweep을 사용한다. 회피 승인 상태와 타격 대기 해제를 Montage 중단 전에 확정해 공격을 취소한다. 전용 회피 애니메이션 대신 이동으로 표현한 첫 구현이다.

양수 피해를 받으면 타격 대기·공격·회피를 취소하고 0.45초 동안 이동과 공격·회피 승인을 막는다. 이동 잠금 전에 낙하 여부를 보존하고, 생존 상태에서 경직·회피가 끝났을 때 Walking/Falling으로 복원한다. 체력이 0이면 Montage 호출 전에 사망을 확정하고, 표준 `CanBeDamaged=false`와 `DuelDead` Actor 태그를 설정한다. 사망 후에는 이동·공격·회피가 복구되지 않는다. 적은 사망한 플레이어나 소멸한 플레이어를 향해 새 공격을 시작하지 않는다.

Montage 호출 전 공격 상태를 잠그고, 재생 실패나 콜백 후 상태 변경을 구분한다. 피해 처리 중 사망·경직이 확정됐다면 재생 호출의 양수 반환만으로 공격 대기를 다시 켜지 않는다. 피해 적용 후에는 자신이 유효한 경우에만 전달 횟수를 갱신한다. `HitCount`와 ApplyDamage 반환은 피해 전달 관측이며, 실제 HP 변화와는 별도로 판단한다. 무적 회피나 사망 상태에서는 양수 요청과 실제 HP 처리가 다를 수 있다.

재생 호출 후 취소 상태라면 표준 `UAnimInstance::Montage_Stop(0.05, AM_StudyAttack)`으로 해당 공격의 활성 재생을 종료한다. 0.05초는 블렌드아웃 시간이며 모든 포즈 가중치를 즉시 0으로 만든다는 뜻은 아니다. Montage 인자를 생략해 전체 재생을 멈추지 않는다. 정리 호출 뒤에는 Busy·Pending·체력·회피·사망·적 Phase를 다시 쓰지 않아 정리 콜백이 만든 상태도 보존한다. 공격 시작 콜백에서 이미 피격 Montage로 교체된 경우도 피격 재생이 유지되는 것을 확인했다.

## 실제 확인 결과

| 검증 | 관측 결과와 근거 |
|---|---|
| 적의 실제 AI 흐름 | 접근·예고·공격·회복 상태 모두 관측. 7초 관측에서 피해 3회, 플레이어 체력 100→55, 피격 Montage와 경직 후 이동 복귀 확인. `enemy_loop-runtime.json` |
| 피격에 따른 공격 취소·재공격 | 타격 창 전 피해로 Pending/Busy 해제. 회복 후 두 번째 실제 좌클릭만 타격해 적 체력 80·피격 1회, 플레이어 체력 85. 이 경계는 적 Tick을 PIE에서만 정지해 분리했다. `hit_cancel-runtime.json` |
| 회피 무적의 경계 | 실제 우클릭으로 공격을 취소하고 회피. 무적 중 피해는 HP100·이벤트0·회피 관측1, 무적 종료 후 피해는 HP85·이벤트1. 상태 복귀와 다음 공격 승인 확인. `evade-runtime.json` |
| 플레이어 사망 | 공격 중 치명타 후 HP0·피해 이벤트1·MovementMode None 유지. 직접 재진입 공격·회피도 거절. 적 이동 정지 확인. `death-runtime.json` |
| 적 사망 | 정상 ApplyDamage80을 준비 조건으로 적용하고 좌클릭의 추가20으로 사망. 이후 공격에도 HP0 유지, 적의 공격·이동 재개 없음. 준비 피해와 치명타를 합친 피해 이벤트는2다. `enemy_lethal-runtime.json` |
| 표적 소멸 | 적 예고 중 소유 PIE 플레이어만 파괴. 적의 타격 대기 해제·이동 정지, 새 공격0 확인. 저장된 맵·폰 에셋은 보존. `target_loss-runtime.json` |
| 실제 회피 후 반격 | AI를 켜 둔 채 우클릭 회피→W로 접근→회복 중 좌클릭 반격. 플레이어 HP100·피해0·회피1·공격1, 적 HP80. 적의 피격 경직 해제와 다음 행동 준비 확인. 위치·체력을 직접 설정하지 않았다. `live-counter-runtime.json` |
| 저장·재로드 | 새 프로세스에서 기본 HP100/100과 컴파일 상태를 확인하고 정상 AI 흐름·실제 반격을 실행했다. 카메라 오프셋과 HUD 포함 화면도 확인했다. 최종25개 저장 파일 해시가 실행 전과 일치. `cold-final-state.json` |
| 콜백 취소의 실제 재생 정리 | 플레이어 피격·회피·적 피격·공격 시작 콜백 피격·치명타의5경계에서 호출 반환 직후 공격 재생 false·Pending false·공격 승인0 확인. 피격 재생과 회피/사망 상태 유지. `cancel-fixed-session.json` |
| 취소 수리의 저장·후속 전투 | 새 프로세스에서 취소 경계1개와 AI를 유지한 회피→접근→반격을 확인. 플레이어 HP100·적 HP80, 나머지23개 파일 보존, 수정한 두 Blueprint를 포함한 저장 바이트 유지. `cancel-fix-cold-final-state.json`·`cancel-live-counter-runtime.json` |

이번 작업의 원시 기록과 화면은 로컬 `cubeless/records/operations/game-animation-duel-20261028/`에 있다. `duel-final-ui.png`는 예고·HP 표시, `duel-counter-ui.png`는 반격 후 플레이어 HP100·적 HP80을 포함한다. 같은 작업자의 실제 검수와 자체 검토이며 독립 QA는 아니다.

## 저작 과정의 실패와 현재 범위

Python의 `CollisionResponse`는 이 환경에서 응답 enum 자체가 아니었다. 실제 캡슐 조회가 반환한 enum 타입과 설치 API로 Block 값을 선택해 소유 복사본만 설정했다. 초기 생성 기록과 중단 위치를 보존하고 남은 설정만 이어갔다.

공식 DSL Sequence의 기본 출력은 두 개였다. 여러 실행 분기는 두 출력 Sequence를 중첩했고, 벡터는 핀별 문자열 형식에 기대지 않고 `MakeVector`로 연결했다. 자신의 상속 속성 getter는 외부 Actor용 self 핀이 없으므로 적이 플레이어의 사망 여부를 확인할 때 표준 ActorHasTag를 사용한다. 원 실패를 보존했으며 Epic 소스나 MCP 구현을 우회 수정하지 않았다.

취소 수리 시 메뉴의 `Animation|Montage|MontageStop`은 실제 NodeInfo에서 `Animation|Montage|Montage_Stop`으로 읽혔다. 문자열 완전 일치 검사는 정상 컴파일된 첫 함수를 잘못 거절했다. 실제 AnimInstance self 연결·`InBlendOutTime`·명시적 Montage 핀 값을 확인한 뒤 첫 writer를 재실행하지 않고 저장과 남은 적 함수만 이어갔다. 같은 표시 이름의 Ability 노드는 사용하지 않았다. 설치 `AnimInstance.h/.cpp`의 공개 호출과 특정 Montage 인스턴스 정리 경로를 근거로 판단했다.

검증 준비의 `set_editor_property(Health)`는 인스턴스 편집 제한으로 거절됐다. 해당 PIE를 정리한 뒤 정상 대미지 API로 별도의 치명타 검증을 수행했으며 원 요청의 성공으로 바꾸지 않았다.

현재는 단일 플레이어·평탄한 실험 맵의 비무장 전투다. 적의 이동은 NavMesh 경로 탐색이 아니며 복잡한 장애물·계단·다수 적용 AI를 구현한 것은 아니다. 콤보·무기 궤적 sweep·히트스톱·락온·전용 회피/사망 연출·재시작 UI·네트워크는 후속 범위다. 회피 중 중력 처리는 별도 캐릭터 이동 기능이 아닌 짧은 sweep 이동이므로 공중 회피는 시작하지 않는다. 이번에는 패키징·MCP 비활성 독립 실행을 수행하지 않았고, 직접 에셋 의존성 검사와 동등하게 취급하지 않는다. 입력 주입과 관측에만 Editor 도구를 사용했으며 생성한 Blueprint에는 MCP 호출·클래스·모듈을 넣지 않았다.

소유 검증 세션은 PIE와 관측기를 종료하고 dirty0을 확인한 뒤 정상 종료한다. 사용자 에디터를 임의로 종료하거나 Git stage·commit·push를 하지 않는다.
