# Duel Protocol: 전투 UMG와 UI 머티리얼

UE 5.8.4 CL 58902052에서 기존 1대1 전투의 독립 사본에 실제 UMG HUD·결과 화면·재시작을 구현했다. 기존 Study 25개 파일은 보존했다. 새 콘텐츠는 `Content/Study/GameAnimationSample/ArenaUI/`에 있으며, 게임 그래프는 표준 Blueprint·UMG·Material을 사용한다. 아래 기존 검수는 Editor/PIE 범위다. 후속 MCP 비활성 Win64 패키징과 독립 게임의 ArenaUI HUD·공격·피해·패배·Retry·효과 전환 대표 검수를 마쳤다. 기존 샘플의 PoseHistory 진단과 상세 미검증 범위는 별도로 남겼다. 현재 상태는 [게시와 패키징](README.md#2026-10-30-게시와-mcp-비활성-패키징)을 따른다.

**후속 리뷰 3건은 수정하고 빌드·공식 MCP 회귀 검증을 마쳤다.** UI 에셋 수정은 현재 프로젝트에 저장되어 있다. 예약 이름 보호가 포함된 새 DLL은 격리 에디터 검증 후 승인된 정상 재시작을 거쳐 실제 프로젝트에도 적용했다. 후속 Material 코멘트 수정까지 포함한 동일 소스 빌드의 해시와 공식 MCP 연결 근거를 확인했으며, 이전 재시작 승인 대기 표기는 해소됐다. 2026-10-30 게시 작업 시작 시 에디터는 실행 중이 아니었다.

- **UI 시간 기준:** `RefreshHUD`의 timestamp 세 곳을 표준 pure `GetAccurateRealTime`의 Seconds+PartialSeconds로 바꿨다. 설치 소스에서 확인한 Slate UI 머티리얼의 앱 시간과 일치한다. 실제 피해25 이후 체력75를 유지한 채 금색 잔상 영역은 early225→middle170→late0픽셀로 줄었고 붉은 피격 edge도 감쇠했다. 런타임 timestamp를 덮어쓰지 않았다. 저장본을 새 에디터에서 로드한 경우와 `RetryRound` 이후도 확인했다.
- **결과 콜백 이후 수명:** `BeginResult`는 HP·공격 횟수·경과 시간·결과와 문맥을 private local에 먼저 캡처하고 `SealArenaRound`를 호출한다. 이후 위젯·컨트롤러·GameMode의 유효성/동일성과 viewport 연결을 확인한다. 공격 정리 콜백에서 실제 플레이어를 소멸시켜도 `VICTORY / STRIKES1 / DAMAGE TAKEN0`을 유지하며, 제거된 위젯은 다시 표시하거나 포커스를 부여하지 않는다. 콜백이 바꾼 member PlayerRef를 덮어쓰지 않는다.
- **예약 이름:** 공통 이름 검사에서 NAME_None에 해당하는 `None`·`none`·`NONE`을 변경 전에 거절한다. 에셋 basename도 같은 검사를 사용한다. 위젯/애니메이션 생성 후에는 실제 FName이 요청 이름과 일치하는지도 확인한다. 공식 MCP의 쓰기 전 거절과 정상 트리/opacity·translation 애니메이션 생성·컴파일·저장을 확인했다.

기존 실패 재현은 `report.json.followup_review`와 원시 결과에 그대로 남겼고, 후속 성공은 `followup_fixes`에 별도로 연결했다. 기존 Study25개와 ArenaUI의 다른 출력은 해시가 같고, 게임 에셋 중 `WBP_DuelHUD`만 변경됐다. 검증된 DLL의 게임 런타임 배포를 뜻하지 않으며, 게임 그래프는 표준 엔진 기능만 사용한다.

## 사용

1. `/Game/Study/GameAnimationSample/ArenaUI/L_ArenaUI`를 연다. 저장된 GameMode는 `GM_ArenaUI`, 플레이어는 `BP_ArenaPlayer`, 상대는 `BP_ArenaOpponent`다.
2. PIE를 시작하고 게임 뷰포트를 클릭해 입력을 받는다. WASD 이동, 마우스 시점, 좌클릭 공격, 우클릭 회피를 사용한다.
3. 상단 적 상태가 `WINDUP / EVADE NOW`일 때 회피하고, `RECOVERY / COUNTER WINDOW`에서 반격한다. 하단 체력·회피 표시는 실제 Actor 상태를 읽는다.
4. 게임 중 **Tab**으로 전체/감소 효과를 전환한다. F2는 엔진의 기본 `viewmode unlit`과 충돌하므로 사용하지 않는다.
5. 승리·패배·표적 소멸 시 결과 화면이 열린다. `REINITIALIZE / RETRY`를 클릭하거나 기본 포커스에서 Enter를 누르면 이 맵을 새로 연다. 결과 화면의 Tab은 버튼 선택이며 효과 버튼도 사용할 수 있다.

재시작은 현재 예제 맵 `/Game/Study/GameAnimationSample/ArenaUI/L_ArenaUI`로 향한다. 다른 게임 맵에 재사용할 때는 `RetryRound`의 목적지와 Actor 문맥을 함께 바꾼다. 감소 효과 설정은 라운드별로 초기화되며 영구 사용자 설정으로 저장하지 않는다.

## 참고한 원칙과 독자 디자인

- [Returnal UI 제작자 설명](https://blog.playstation.com/?p=348960): 게임 중 필요한 정보의 우선순위, 조작/상태 피드백과 복고 미래주의 문법을 참고했다. 게임의 화면·로고·아이콘을 복사하지 않았다.
- [God of War Ragnarök 접근성 설명](https://www.playstation.com/en-us/games/god-of-war-ragnarok/accessibility/): 색상 외에 상태 문구를 함께 제공하고 움직이는 효과를 줄이는 선택지를 적용했다. 해당 게임의 접근성 기능 전체를 구현했다는 뜻은 아니다.
- [Epic UMG 최적화 지침](https://dev.epicgames.com/documentation/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine?lang=en-US): raw attribute binding과 매 프레임 머티리얼 재생성을 사용하지 않는다. 체력·공격 단계는 캐시 비교 후 갱신하고 시각 효과는 shader/WidgetAnimation으로 분리한다.
- [Epic WidgetAnimation 설명](https://dev.epicgames.com/documentation/en-us/unreal-engine/animating-umg-widgets-in-unreal-engine): 표준 opacity·translation 트랙을 사용한다.

`CUBELESS / DUEL PROTOCOL`은 이번 예제의 독자 UI다. 어두운 패널·밝은 본문·청록 플레이어·호박색 적을 사용하고, 공격 예고·무적·경직·치명 상태를 문자로도 구분한다. 실제 전투에 없는 스태미나나 임의의 능력치는 표시하지 않는다. 세로 430×768급 PIE와 가로 창 확대 화면에서 상단 제목/적 패널이 겹치지 않도록 두 줄로 배치했다. 세로 배치 확인은 모바일 터치 입력이나 기기 실행 검증을 뜻하지 않는다.

## 에셋과 연출

| 에셋 | 용도 |
|---|---|
| `WBP_DuelHUD` | 기본 위젯 33개, 상태 표시·결과·재시작·효과 전환 |
| `BP_ArenaPlayer`, `BP_ArenaOpponent`, `GM_ArenaUI`, `L_ArenaUI` | 기존 Duel의 독립 사본. 원본 25개는 수정하지 않음 |
| `M_UI_DuelBar` | 플레이어/적/회피 바. Fill·Previous·ChangedAt·ReducedFX·Tint 파라미터 |
| `M_UI_Panel` | 어두운 bevel 패널과 청록 테두리 |
| `M_UI_DamageVeil` | 피격 가장자리 효과와 체력 30 이하 경고 |
| `M_UI_ArenaSigil` | 결과 화면 문양과 약한 shimmer |
| `M_UI_ResultShade` | 결과 배경·약한 scanline |
| `T_ArenaSigil`, `Source/ArenaSigil.png` | 이번에 ImageGen으로 만든 투명 원본과 UI Texture |

머티리얼 5개는 User Interface domain·Translucent이며 표준 Custom HLSL, Emissive/Opacity를 사용한다. 체력 바는 변경 전 체력을 0.2초 유지한 뒤 0.6초 smoothstep으로 현재 체력에 접근한다. 피격 효과는 `exp(-age*7)`로 감소한다. 감소 효과에서는 trail·pulse·scanline·shimmer와 WidgetAnimation을 줄이거나 멈추고 정적 상태 문구를 유지한다. 애니메이션 중 정지해도 패널/제목의 translation=0·opacity=1로 돌아가도록 명시적으로 복원한다.

표준 WidgetAnimation 5개는 `HUDReveal` 0.45초, `ResultReveal` 0.7초, `ThreatPulse` 0.7초 반복, `PlayerHit`/`EnemyHit` 0.24초다. 매 프레임 동적 머티리얼을 만들지 않고 HUD 초기화 때 MID 6개를 만든다. 중앙 Widget Tick은 Actor 유효성과 상태를 읽으며, 체력·공격 단계는 변경 때만 갱신한다. 상태 문구 2개와 회피/저체력 파라미터는 Tick에서 갱신하므로 완전한 이벤트 기반 UI라고 표현하지 않는다. GPU/CPU 성능 개선 수치는 측정하지 않았다.

문양 원본은 1254×1254 RGBA이며 투명도를 확인했다. 생성 지시는 은색 마름모와 끊어진 금속 링, 청록 필라멘트·절제된 호박색 불꽃, 독자 미래주의 결투 문양, 글자/로고/배경 없이 투명한 정사각형이었다. 다른 게임의 아트나 Windows 폰트를 추출하지 않았다. 본문은 엔진 Roboto를 사용한다.

## 실제 연결과 실패 처리

플레이어의 기존 부모 BeginPlay는 유지한다. 첫 전투 Tick에서 HUD를 한 번 생성하고 AddToViewport한다. HUD는 OwningPlayerPawn과 명시적인 상대 클래스로 문맥을 확보한다. 모든 전투 규칙은 [Duel](Duel.md)의 기본값을 유지한다.

`BeginResult`는 중복 진입 방지 상태와 결과 스냅샷을 먼저 확정한다. 플레이어의 `SealArenaRound`는 라운드 종료 상태·공격/타격 Pending·회피/무적 해제·피해 거절·이동 잠금을 확정한 뒤 해당 공격 Montage만 정리한다. 다음 공격·회피·이동 복원은 라운드 종료 guard가 거절한다. 결과가 준비되면 UIOnly 입력·마우스 표시·Retry 포커스를 설정한다. 플레이어 사망을 우선하고, 살아 있는 플레이어의 상대가 사라지면 `INTERRUPTED`로 구분한다.

`RetryRound`는 재진입 guard를 먼저 설정하고 표준 InputModeGameOnly/`OpenLevel`을 사용한다. MCP나 Editor 콘솔 명령으로 게임을 재시작하지 않는다. 월드 이동은 원자적 롤백이 아니며 실패한 travel을 자동 재시도하지 않는다. 목적지 누락·플레이어 교체·멀티플레이·컨트롤러 전환을 일반화한 시스템은 아니다.

## 추가한 Editor API

공식 MCP의 등록 이름은 `cubeless_mcp_ue58.registration.umg_composition_toolset.CubelessUMGCompositionTools`다.

| 도구 | 계약 |
|---|---|
| `compose_widget_blueprint(asset_path, nodes_json)` | 새 `/Game` native UserWidget Blueprint의 순서 있는 위젯 트리 생성. 기존 에셋/패키지는 쓰기 전 거절. 컴파일·저장하지 않음 |
| `add_widget_animation(asset_path, animation_name, duration, tracks_json)` | 새 opacity/translation 애니메이션 생성. 예약 NAME_None·기존 애니메이션/멤버 충돌·없는 위젯·시간 범위/24000Hz key 충돌을 거절. 컴파일·저장하지 않음 |
| `inspect_widget_blueprint(asset_path)` | source 트리·애니메이션 바인딩 수·종료 시간·compile/dirty 관측 |
| `finalize_widget_blueprint(asset_path, save)` | 명시 대상 구조 갱신·컴파일, save=true일 때 그 에셋만 저장 |

간단한 입력 예시는 다음과 같다. `nodes_json`/`tracks_json`은 아래 배열을 JSON 문자열로 전달한다.

```json
[
  {"name":"Root","type":"CanvasPanel"},
  {"name":"Art","type":"Image","parent":"Root"}
]
```

```json
[{"widget":"Art","property":"opacity","keys":[[0,0],[0.4,1]]}]
```

layout/style은 Editor Python의 `unreal.CubelessUMGCompositionLibrary.get_source_widget(path, name)`으로 source widget을 얻어 공개 setter로 지정한 뒤 finalize한다. 저장 옵션은 명시적이며 생성 단계·부분 변경·컴파일/저장 시도·dirty를 구분한다. 실패 시 source/패키지를 관측하고 불확실한 요청을 반복하지 않는다. 원자적 롤백은 제공하지 않는다.

설치는 기존 Editor 모듈 내부 C++와 Python 등록부로 제한했다. 등록부 → JSON operations → UE58 compatibility를 분리했고 엔진·공식 MCP·게임 C++는 수정하지 않았다. `UWidgetTree::ConstructWidget`, 표준 MovieScene float/2D transform track, WidgetAnimation binding, Kismet compile/save를 사용한다. 작업 로직은 서버 전송 코드에 결합하지 않는다.

## 검수 결과와 복구 경험

- Editor C++ 빌드·로딩과 공식 MCP 등록/스키마/4개 API의 실제 호출을 확인했다. 새 트리/애니메이션·compile/save·기존 대상 거절·중복 이름·subframe 충돌·사용자 변수 이름 충돌을 확인했다. 최종 보호 조건 빌드는 11.72초에 통과했다.
- 체력 100→75와 실제 숫자/MID Fill=0.75·Previous=1.0, 회피 무적 상태와 피격 거절, 감소 효과 MID 6개 값 1, 정지 후 패널 pose 복원을 확인했다.
- 적의 정상 접근/예고/공격/회복과 플레이어 피해, 패배 화면을 확인했다. 승리는 소유 PIE 상대에 80 피해를 준비한 뒤 실제 공격 Montage/trace의 마지막 20 피해로 확인했다. 상대 소멸은 INTERRUPTED·전투 잠금으로 확인했다.
- Computer Use로 결과 화면의 효과/재시작 버튼을 실제 클릭했고 Enter 재시작도 확인했다. 실제 월드 교체·HP100/100·HUD 1개·라운드 잠금 해제를 관측했다.
- 실제 `UnrealMCP5_8_3.uproject`와 공식 MCP8000의 새 세션에서 로드·등록·저장 바이트·정상 전투를 확인했다. 격리 프로젝트의 SM5 경로에 더해 실제 프로젝트 SM6 렌더와 세로/가로 배치를 확인했다. 뷰포트 포커스 후 실제 Tab 입력으로 전체/감소 효과를 전환하고 렌더 모드가 유지됨을 확인했다.
- 마지막 Tab/배치 수정을 저장한 뒤 소유 WBP 패키지만 비대화식 dirty 거절 모드로 다시 로드했다. 실제 키 Tab·적 패널 y=134·애니메이션 5개·compile 상태·파일 바이트를 확인했다. 외부 실행 응답은 retained-owner/transaction 전이 때문에 current_unknown이었으며 원 요청은 재시도하지 않았다. 후속 읽기로 실제 상태·dirty 0·기존 25개 및 새 저장 파일 보존을 별도로 확인했다.
- 새 11개 에셋의 직접 패키지 참조에서 Cubeless/ModelContextProtocol/Toolset 의존성은 발견하지 않았다. **MCP 비활성 cook/package/독립 게임 실행, GPU 시간 측정, 독립 QA는 이번 검수에 포함하지 않았다.**

첫 그래프 변환에서는 DSL decompiler의 Montage 자산 기본값이 손실돼 공격 재생이 시작되지 않았다. 실제 AttackCount=0·상대 HP20을 확인한 뒤, 기존 성공 DSL의 명시적 Montage/재생률을 복원했다. 새 PIE의 실제 마지막 타격과 승리로 수리를 확인했다. 원 실패를 승리로 바꾸거나 같은 불확실 요청을 재실행하지 않았다.

버튼 바인딩의 보존 검수는 false를 반환했지만 실제 생성/컴파일/저장은 진행됐다. 실제 차이는 FKey의 잘못된 구조체 문자열이 `(`로 해석된 것과 Vector2D 기본값의 숫자 표기 정규화였다. 전부 검수 오판으로 취급하지 않았다. 생성된 bound event를 재사용해 표준 함수를 연결하고, 키 이름을 직접 전달하도록 수정했다. 이후 실제 F2 입력에서 엔진 BaseInput.ini의 Unlit debug binding 충돌을 확인해 최종 키는 Tab으로 바꿨다. 보호 검사는 끄지 않았다.

원시 호출·실패/수리·빌드·화면·해시 근거는 로컬 `cubeless/records/operations/game-animation-arena-ui-20261029/report.json`에 연결한다. 보조 Python/원시 기록은 Git 제외 대상이고, 이 학습 문서·프로젝트 콘텐츠·플러그인 소스의 Git 포함 가능 여부와 실제 커밋 승인은 별개다. 이 구현 당시에는 Git 쓰기를 하지 않았으며, 이후 2026-10-30 게시 작업은 사용자의 별도 승인 범위에서 진행한다.

## 남은 범위

현재는 로컬 키보드/마우스 1대1 전투 예제다. 사용자 폰 교체·네트워크·터치 조작·영구 옵션 저장·일반 레벨 travel 실패 복구·새 pause 메뉴·접근성 전 항목·완성 게임 패키징은 후속 범위다. 기존 샘플 맵의 StateTree/PoseSearch 로그는 이번 UI의 완료 근거와 구분하며 원본 샘플 에셋을 수정해 숨기지 않는다. 성능과 런타임 독립성은 직접 참조 검사 이상의 범위를 검증했다고 주장하지 않는다.
