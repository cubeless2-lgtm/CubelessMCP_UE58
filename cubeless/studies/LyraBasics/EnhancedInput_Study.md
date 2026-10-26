# Lyra 분석에서 이어온 Enhanced Input 수명 실습 — UE 5.8.3

**현재 상태: 사용 가능 — 필수 플러그인: EnhancedInput.** 아래에서 확인한 단일 로컬 플레이어의 입력 활성화·해제·수명 관리 실습을 사용할 수 있습니다.

## 출처와 완료 범위

원본 5.6 MCP 저장소의 `Docs/Analysis/Lyra/`와 작업 기록 Section 9.6~9.8에서 입력 이벤트·구독 수명 보강 항목을 이어받았습니다. 해당 분석 대상 Lyra는 EngineAssociation 5.7이었으며, 당시 정적 분류를 현재 기능 제한으로 적용하지 않았습니다. 기존 네 단계 초기화 실습은 [기본 실습](README.md)에 있고 이번에는 반복 검증하지 않았습니다.

이 실습은 표준 UE Enhanced Input의 로컬 플레이어 1명에 대한 Blueprint 구현입니다. 원본 Lyra 샘플을 실행하거나 LyraHeroComponent, GameFeature 입력 액션, 태그 기반 Ability 바인딩을 그대로 이식한 결과는 아닙니다.

## 5.8 API와 확인한 차이

- Lyra의 [입력 설명](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-input-settings-in-unreal-engine)에 나오는 `UPlayerMappableInputConfig`는 설치 소스 `EnhancedInput/Public/PlayerMappableInputConfig.h:23`에서 5.3부터 deprecated입니다. 현행 설정 등록은 `UEnhancedInputUserSettings::RegisterInputMappingContext` 계열로 확인했습니다.
- `InputMappingContext.h`의 `RegistrationTrackingMode=CountRegistrations`는 Add/Remove 횟수를 맞추는 방식입니다. `EnhancedInputSubsystemInterface.cpp:260~348`에서 등록 수 증가·감소, 마지막 제거 시점과 설정 통지 옵션을 확인했습니다. 같은 컨텍스트를 반복 추가한 뒤 한 번 제거하면 반드시 없어질 것이라고 가정하지 않습니다.
- `FModifyContextOptions` 기본값은 pressed-key 무시=true, 즉시 rebuild=false, user-settings 통지=false입니다. 이번 Blueprint는 이 기본값을 유지합니다. 설정 등록과 실제 입력 적용을 별도 작업으로 검증했습니다.
- Python `has_mapping_context`는 이 설치 버전에서 적용 시 우선순위, 미적용 시 `None`을 반환했습니다. 우선순위 0을 False로 오판하지 않도록 존재 확인은 `is not None`으로 구분합니다. 이번 관측값은 50과 5 및 None이며 우선순위 0 자체는 별도 실행하지 않았습니다.
- Python 팩토리의 실제 이름은 `InputAction_Factory`, `InputMappingContext_Factory`였습니다. `Key()`를 만든 뒤 `key_name`을 지정했습니다. 현재 매핑 읽기는 deprecated `mappings` 대신 `default_key_mappings.mappings`를 사용했습니다.

## 저장된 에셋과 재현 방법

맵: `/Game/Study/LyraBasics/Maps/L_LyraBasics`

| 에셋 | 설정 |
| --- | --- |
| `Input/IA_StudyConfirm` | Boolean InputAction |
| `Input/IMC_StudyConfirm` | K → IA_StudyConfirm, CountRegistrations |
| `Input/IMC_StudyUnrelated` | 다른 컨텍스트 보존을 확인하는 빈 컨텍스트 |
| `Blueprints/BP_StudyInputProbe` | Actor, Auto Receive Input=Player 0 |

`Study_InputProbe`가 맵에 배치돼 있습니다. Play를 실행하고 K를 누른 뒤 놓으면 StartedCount와 CompletedCount가 증가합니다. Blueprint 디버그 객체를 PIE의 해당 Actor로 선택해 관찰할 수 있습니다. 프로젝트는 이미 EnhancedPlayerInput·EnhancedInputComponent를 기본 입력 클래스로 사용했습니다. 필수 플러그인은 EnhancedInput입니다.

Blueprint 변수는 `OwnsInputContext:bool=false`, `StartedCount:int=0`, `CompletedCount:int=0`입니다.

```text
ActivateStudyInput:
  OwnsInputContext=false일 때만 진행
  PlayerController(0)의 현재 EnhancedInputLocalPlayerSubsystem 조회
  유효하면 AddMappingContext(IMC_StudyConfirm, priority=50)
  실제 적용을 HasMappingContext로 확인한 뒤 OwnsInputContext=true

DeactivateStudyInput:
  OwnsInputContext=true일 때만 진행
  현재 subsystem이 유효하면 소유 컨텍스트를 한 번 Remove
  그 뒤 OwnsInputContext=false
  subsystem이 무효이면 소유 플래그를 거짓으로 덮지 않음

BeginPlay → ActivateStudyInput
EndPlay → DeactivateStudyInput
IA_StudyConfirm.Started → StartedCount += 1
IA_StudyConfirm.Completed → CompletedCount += 1
```

전역 `ClearAllMappings`는 호출하지 않습니다. 입력 시스템의 다른 컨텍스트와 사용자 설정 저장 파일을 임의로 지우지 않습니다. 로컬 플레이어 교체·분할 화면·외부 코드가 소유 등록을 강제로 제거하는 경우는 이번 단일 플레이어 실습 범위에서 검증하지 않았습니다.

## 공식 MCP 제작·검수 경로

1. 공식 BlueprintTools로 Actor Blueprint·변수·함수 그래프를 작성했습니다.
2. Cubeless `CubelessBlueprintExpressionTools.add_blueprint_enhanced_input_action_node`로 실제 InputAction 참조를 갖는 K2 이벤트 노드를 작성했습니다. 이 공개 도구 이름은 기존 입력 이벤트 계약을 사용합니다.
3. 공식 BlueprintGraphEditor 공개 편집 API로 조건과 실행 핀을 연결하고 컴파일했습니다. 일반 Python 실행 도구는 엔진 API 조합을 위한 제작 수단이며, 저장된 게임 로직은 Blueprint 노드입니다.
4. 공식 EditorAppToolset.StartPIE에서 실행하고 `CubelessPIEInputTools.inject_pie_input_key(key="K", event="pressed"/"released")`를 서로 다른 요청으로 호출했습니다. 각 호출 이후 별도 요청으로 카운터를 읽었습니다. 엔진 틱을 막으며 같은 요청에서 입력 결과를 기다리지 않았습니다.

## 실제 결과와 리뷰

2026-10-26, UE `5.8.3-58210709+++UE5+Release-5.8`, 실제 프로젝트의 공식 MCP 연결에서 확인했습니다.

| 경계 | 결과 |
| --- | --- |
| BeginPlay | 컨텍스트 우선순위 50, 소유=true |
| Activate 3회 추가 호출 | 뒤의 Remove 1회로 해제됨; 중복 등록 누수 없음 |
| 첫 K press/release | Started=1, Completed=1 |
| Deactivate 2회 | 자신의 컨텍스트 없음, 별도 우선순위 5 컨텍스트 유지 |
| 비활성 상태 K press/release | Started=1 유지 |
| 재활성화 후 K press/release | Started=2, Completed=2 |
| 설정에 등록 후 입력만 비활성화 | 설정 등록=true 유지; 활성화와 등록이 구분됨 |
| 명시적인 설정 unregister | 설정 등록=false |
| 재활성화한 Actor를 PIE에서 Destroy | EndPlay로 자기 컨텍스트 제거, 다른 우선순위 5 컨텍스트 유지 |

구현과 실패 기록을 자체 리뷰했습니다. 잘못된 캐시 변수 타입을 제거하고 활성·해제 시 현재 subsystem을 직접 조회하도록 수정한 뒤 위 동작을 확인했습니다. 독립 리뷰를 수행한 것으로 표현하지 않습니다.

Blueprint·입력 에셋 4개와 맵을 저장했습니다. 저장 패키지의 금지 MCP 참조 검사도 통과했습니다. PIE는 종료했습니다. 입력 실습 전부터 dirty였던 기존 BP_StudyCoordinator는 저장·폐기하지 않고 보존했습니다. 최초 입력 실습에서는 재로드를 추가 실행하지 않았으며, 아래 후속 실행에서 저장된 에셋을 새 프로세스로 확인했습니다. 게임 패키징·MCP 없는 게임 독립 실행 확인은 후속 범위입니다.

## 저장 상태의 후속 실행 확인 — 2026-10-26

[기본 실습의 새 프로세스 검증](README.md#저장-에셋의-새-프로세스-실행-확인--2026-10-26)에서 원본과 동일한 파일을 별도 프로젝트로 복사했습니다. 새 에디터에서 K 매핑·CountRegistrations·AutoReceiveInput Player0의 저장값과 Blueprint 컴파일을 확인한 뒤 PIE에서 실제 K press/release를 수행했습니다. Started·Completed가 각각 1회였고, 중복 활성화 뒤 해제·비활성 상태 입력·재활성화 뒤 EndPlay 정리와 다른 컨텍스트 보존도 통과했습니다.

Cubeless·공식 MCP·Toolset Registry가 없는 별도 프로젝트에서도 입력 에셋과 Blueprint의 cold load·컴파일을 통과했습니다. 이는 MCP 없는 에디터 로드·컴파일의 근거이며 패키징 게임 실행 결과로 확대하지 않습니다. 메인 에디터와 원본 학습 파일은 변경하지 않았고, 이전 성공 증거와 원 실패 기록도 보존했습니다.

## 실패에서 확인한 엔진 동작

`BlueprintEditorLibrary.cpp:2163~2176`은 허용되지 않는 object variable 타입 요청에 경고를 남기고 int 타입을 반환합니다. EnhancedInputLocalPlayerSubsystem을 캐시 변수로 만들려던 호출에서 이 동작이 관측됐습니다. 타입 이름만 믿지 않고 실제 생성 핀을 확인했으며, 잘못 생성한 새 실습 변수와 그 부분 연결만 제거했습니다. 캐시가 필요하지 않은 이번 로직은 공개 GetSubsystem 노드의 결과를 직접 사용합니다.

Python에는 이번 설치에서 `SubsystemBlueprintLibrary`가 노출되지 않았고 PlayerController.Player는 protected였습니다. 보호를 우회하지 않았습니다. 검수에서는 공개 ObjectIterator와 GameplayStatics의 WorldContext로 현재 GameInstance에 해당하는 유일한 subsystem을 확인했습니다. LocalPlayer의 Outer가 GameInstance일 것이라는 초기 가정도 실제 관측과 달라 제거했습니다. 게임 Blueprint는 PlayerController를 받는 표준 GetSubsystem 노드를 사용합니다.

원 실패와 실제 부분 생성·연결 상태, 수정 후 결과는 기존 로컬 `lyra-basics-study-20261026/report.json`에 이어서 보존했습니다. 결과가 불확실한 원 요청은 재시도하지 않았습니다. 프로젝트 C++·엔진·MCP 플러그인 소스는 변경하지 않았습니다.
