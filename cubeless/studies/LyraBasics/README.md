# Lyra 기본 구조 학습 — UE 5.8.3

**현재 상태: 사용 가능.** 아래에 기록한 Blueprint 실습과 확인된 동작을 사용할 수 있습니다. 추가 학습·확인 항목은 후속 범위로 구분합니다.

## 목적과 근거

Lyra의 데이터 기반 설정, 준비 조건에 따른 단계별 초기화, 이벤트 구독 수명 관리를 작은 Blueprint 실습으로 재현합니다. 공식 MCP와 Cubeless를 제작 도구로 사용하며, 결과 에셋은 표준 엔진 기능과 Blueprint로 구성합니다.

현재 로컬에서 Lyra 원본 샘플 프로젝트를 찾지 못했습니다. 따라서 이 실습은 **공식 문서에 근거한 핵심 개념의 최소 재현**입니다. 원본 Lyra 에셋·C++ 구현의 직접 검증이나 전체 변환으로 간주하지 않습니다.

- [Lyra Sample Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
- [Game Framework Component Manager](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-framework-component-manager-in-unreal-engine)
- [Lyra Input Settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-input-settings-in-unreal-engine)
- 기존 자료: 원본 UE 5.6 저장소의 `Docs/Analysis/Lyra/`, 2026-06-07 정적 분석. 과거 지원·차단 판정은 현재 지원 범위를 뜻하지 않습니다. 옛 `D:` 경로는 현재 실행 경로로 사용하지 않습니다.

## 이번 실습 범위

1. `PrimaryDataAsset` 기반 설정을 두 개의 데이터 에셋으로 분리합니다.
2. 준비 조건을 확인하며 `Spawned → DataAvailable → DataInitialized → GameplayReady` 순서로 진행합니다.
3. 완료 이벤트 구독·해제와 반복 호출·재진입 방지를 확인합니다.

Lyra의 실제 상태는 Gameplay Tag와 ModularGameplay의 초기화 관리 기능을 사용합니다. 이번 최소 재현은 int 상태를 사용한 단일 Actor의 로컬 학습 모델입니다. 네트워크 복제, Actor 간 기능 조율, GameFeature 활성화, AssetManager 비동기 로드 및 Lyra의 능력 부여를 구현한 것은 아닙니다.

설치된 UE 5.8.3의 `GameFrameworkComponentManager.h`에서 `RegisterInitState`, `ChangeFeatureInitState`, `RegisterFeatureImplementer`는 일반 C++ API이고, 상태 통지 구독의 Blueprint 오버로드는 별도로 노출됨을 확인했습니다. 이 학습에서는 프로젝트 C++를 추가하지 않습니다. 단일 Actor의 `BeginPlay` 실습을 원본의 `PreInitializeComponents` 수명과 동등하다고 표현하지 않습니다.

## 에셋과 호출 흐름

에셋 루트: `/Game/Study/LyraBasics`

- `Blueprints/BP_StudyExperienceData`: `ExperienceName`(name), `MoveSpeed`(float).
- `Data/DA_Walk`: 300.0.
- `Data/DA_Run`: 600.0.
- `Blueprints/BP_StudyCoordinator`: 데이터 참조와 준비 조건을 확인하는 Actor.

`CheckInitialization`은 Profile이 없으면 Spawned에서 기다립니다. Profile이 생기면 DataAvailable로 진행하고, `DependenciesReady=true`이며 속도가 양수일 때 한 번만 값을 적용합니다. GameplayReady 상태를 먼저 기록한 뒤 `OnGameplayReady`를 통지하므로 콜백에서 같은 초기화 함수를 호출해도 다시 초기화하지 않습니다.

`StartListening`은 자기 자신의 `ObserveReady` 함수를 구독하고 `StopListening`은 그 구독만 해제합니다. BeginPlay에서 구독·초기화를 요청하고 EndPlay에서 해제합니다. 이미 Ready인 객체를 늦게 구독하는 경우의 즉시 통지는 이번 간단한 Event Dispatcher 모델에 포함하지 않습니다.

`AppliedSpeed`는 데이터 적용을 관찰하기 위한 값입니다. 실제 CharacterMovement의 이동 속도를 변경하는 실습은 아닙니다. `DependenciesReady`도 실제 Enhanced Input 준비 상태를 읽는 값이 아닙니다.

## 재현 절차와 MCP 호출 순서

UE 5.8.3에서 `/Game/Study/LyraBasics/Maps/L_LyraBasics`를 엽니다. 맵에 있는 네 Coordinator의 `Profile`과 `DependenciesReady`를 Details에서 확인하고 Play를 실행합니다. Blueprint 디버그 객체를 각 PIE Actor로 선택하면 아래 결과를 확인할 수 있습니다.

에셋 없이 재구성할 때의 제작 순서는 다음과 같습니다.

1. 공식 `editor_toolset.toolsets.blueprint.BlueprintTools.create`로 PrimaryDataAsset 파생 `BP_StudyExperienceData`를 만듭니다. `add_variable`로 `ExperienceName:name`, `MoveSpeed:float`를 추가하고 컴파일합니다.
2. 공식 `editor_toolset.toolsets.data_asset.DataAssetTools.create`의 `asset_type`을 해당 Blueprint의 generated class로 지정하여 DA_Walk·DA_Run을 만듭니다. 이름과 300.0·600.0 값을 설정합니다.
3. Actor 파생 `BP_StudyCoordinator`를 만들고 아래 변수를 추가합니다. `Profile`은 `add_object_variable`로 위 데이터 클래스의 object reference를 지정합니다. `Profile`, `DependenciesReady`는 `set_variable_instance_editable(..., true)`로 인스턴스 편집을 허용합니다.
4. `add_event_dispatcher`로 인자 없는 `OnGameplayReady`를 선언합니다. 아래 함수 네 개와 BeginPlay·EndPlay 연결을 작성하고 컴파일합니다.
5. 작성된 에셋만 저장하고 별도 학습 월드에 네 Actor를 배치합니다. PIE는 공식 `EditorToolset.EditorAppToolset.StartPIE`로 시작하고 완료 후 `StopPIE`로 종료했습니다.
6. PIE 상태 관측·조건 주입은 `CubelessMCPUE58Editor.CubelessPythonExecutionTools.execute_unreal_python`을 통해 수행했습니다. 이때만 `allow_unsafe_editor_scripting_during_pie=true`를 명시했습니다. 관측 코드는 에디터 검수용이며 저장 Blueprint에는 들어가지 않습니다.

Coordinator 변수는 `InitState:int=0`, `DependenciesReady:bool=false`, `InitializationCount:int=0`, `AppliedSpeed:float=0`, `ReadyObserved:int=0`, `StateTrace:string=""`, `Profile:BP_StudyExperienceData object=null`입니다.

함수별 실제 작업 흐름은 다음과 같습니다. 각 조건은 순서대로 독립적으로 검사하여 한 호출에서 준비된 다음 상태까지 진행할 수 있게 합니다.

```text
CheckInitialization():
  state == 0: state = 1; trace = "Spawned"
  state == 1 AND Profile 유효:
    state = 2; trace = "Spawned>DataAvailable"
  state == 2 AND DependenciesReady AND Profile 유효:
    speed = Profile.MoveSpeed
    speed > 0:
      AppliedSpeed = speed
      InitializationCount += 1
      state = 3; trace = "Spawned>DataAvailable>DataInitialized"
  state == 3:
    state = 4; trace = "Spawned>DataAvailable>DataInitialized>GameplayReady"
    OnGameplayReady 호출

ObserveReady():
  ReadyObserved += 1
  CheckInitialization()  # 재진입 경계 확인

StartListening():
  자기 자신의 ObserveReady를 OnGameplayReady에 Bind

StopListening():
  동일 객체·동일 함수의 구독만 Unbind

BeginPlay: StartListening → CheckInitialization
EndPlay: StopListening
```

실제 Blueprint에서는 `Profile`의 유효성을 확인한 실행 경로 안에서만 속도를 읽습니다. 순수 AND 노드의 단락 평가를 가정하여 무효 객체를 읽지 않습니다. `StateTrace`는 도달한 단계의 표시 문자열이며, 독립적인 타임스탬프 이벤트 저널은 아닙니다.

## 실제 검증 결과 — 2026-10-26

엔진: `5.8.3-58210709+++UE5+Release-5.8`. 실제 프로젝트의 공식 MCP 포트 8000에서 제작·PIE 실행·관측을 수행했습니다.

| Actor / 조건 | 관측 결과 |
| --- | --- |
| Study_Walk: DA_Walk, 준비=true | state=4, 적용값=300, 초기화=1, 완료 관측=1 |
| Study_Run: DA_Run, 준비=true | state=4, 적용값=600, 초기화=1, 완료 관측=1 |
| Study_Deferred: Profile 없음, 준비=false | state=1, 초기화=0, 완료 관측=0 |
| Study_Unsubscribed: DA_Walk, 준비=false | state=2, 초기화=0, 완료 관측=0 |
| Ready Actor에 CheckInitialization 3회 추가 호출 | 초기화·완료 관측이 각각 1회로 유지됨 |
| Profile 없이 준비 조건만 먼저 true | state=1 유지 |
| DA_Walk 제공, 준비=false | state=2 대기 |
| 초기화 전에 Profile 제거, 준비=true | state=2, 초기화=0 유지; 무효 참조 접근 없음 |
| Deferred에 구독을 2회 더 요청한 뒤 유효 데이터 제공 | state=4, 초기화=1, 완료 관측=1 |
| 준비 전 StopListening 후 조건 충족 | state=4, 초기화=1, 완료 관측=0 |

완료 콜백 내부에서 다시 초기화를 호출했지만 중복 초기화가 발생하지 않았습니다. 위 반복 구독 결과는 이 Blueprint Event Dispatcher 경로에서 관측한 것이며, 모든 native delegate나 Lyra의 공유 핸들 수명에도 같은 동작이 적용된다는 뜻은 아닙니다.

- Blueprint 컴파일 통과. 저장된 함수 그래프와 Bind·Unbind의 `ObserveReady` 선택을 확인했습니다.
- 생성 에셋 5개를 저장했습니다. PIE 종료 후 Editor Actor의 state는 모두 0이었고 런타임 조건 변경이 저장 에셋에 섞이지 않았습니다.
- PCG 학습 맵으로 이동한 뒤 돌아와 Actor 4개의 프로필·준비 조건과 그래프가 유지됨을 확인했습니다.
- 저장 패키지 의존성 검사: 재귀 11개 패키지, 설정된 Cubeless/MCP/Python/EditorScripting 금지 참조 없음. 이는 저장 참조 검사 결과이며 게임 패키징·독립 실행 검증은 별도입니다.
- 종료 시 PIE=false, dirty 패키지 없음. 프로젝트 C++와 플러그인 코드는 변경하지 않았습니다.
- 최초 검사에서는 임시 데이터 객체의 속도 변경이 편집 플래그에 의해 거절되어 0 이하 속도의 초기화 호출에 도달하지 않았습니다. 아래 저장 에셋 후속 실행에서는 별도 프로젝트의 데이터 에셋으로 -1·0 경계를 확인했습니다. 최초 거절 기록은 보존합니다.

원시 결과: 로컬 `cubeless/records/operations/lyra-basics-study-20261026/report.json`. 원본 실패 결과와 후속 수정·관측은 각각 보존했습니다. 이 README와 Content 에셋은 Git 포함 대상이며 Python 보조 코드·원시 기록은 제외됩니다. 아직 커밋·푸시하지 않았습니다.

## 저장 에셋의 새 프로세스 실행 확인 — 2026-10-26

원본 학습 파일 9개(에셋 8개·맵 1개)를 별도 소유 프로젝트 `LyraVerify58`로 복사하고, 복사 전후 SHA-256 일치를 확인했습니다. 메인의 dirty BP_StudyCoordinator를 저장하거나 재시작하지 않았습니다. 새 UE 5.8.3 프로세스와 공식 MCP 포트 8002에서 아래를 확인했습니다. Blueprint는 경고도 오류로 처리하여 컴파일했습니다.

| 확인 항목 | 실제 결과 |
| --- | --- |
| 저장된 에셋·맵 cold load | 에셋 8개·맵 1개와 학습 Actor 5개 로드 |
| Blueprint 컴파일 | 3개 모두 통과 |
| Walk·Run 데이터 기반 초기화 | state=4, 적용값=300·600, 초기화·완료 관측 각각 1회 |
| Profile 없는 Deferred | state=1, 초기화=0 |
| 준비 조건이 false인 Actor | state=2, 초기화=0 |
| Deferred에 속도 -1·0 제공 | 두 조건 모두 state=2, 초기화·완료 관측=0 |
| 같은 Deferred의 속도를 300으로 변경 | state=4, 적용값=300, 초기화·완료 관측=1 |
| 저장된 입력 설정 | K 매핑·CountRegistrations·AutoReceiveInput Player0 유지 |
| 실제 K press/release | Started=1, Completed=1 |
| 중복 활성화 3회 후 해제 2회 | 자기 컨텍스트 해제, 별도 우선순위 5 컨텍스트 보존 |
| 입력 비활성 상태 K | 카운터 1·1 유지 |
| 재활성화 후 Actor Destroy | EndPlay에서 자기 컨텍스트 해제, 별도 컨텍스트 보존 |

추가로 프로젝트 플러그인 폴더가 없는 `LyraNoMCP58`에서 Cubeless·공식 MCP·Toolset Registry를 비활성화하고 원본과 동일한 9개 파일을 새 commandlet 프로세스로 로드했습니다. 해당 MCP 스크립트 모듈 5개가 로드되지 않았음을 확인했고, Blueprint 3개 모두 `BS_UP_TO_DATE`로 컴파일됐습니다. 프로세스 종료 코드는 0입니다. Python은 이 에디터 검수의 실행 수단이며 저장된 게임 로직의 의존성이 아닙니다.

기본 플러그인들을 끈 최소 검증 환경에서 ACLPlugin의 기본 애니메이션 압축 설정을 찾는 경고 4개가 있었습니다. 학습 에셋에는 애니메이션이 없으며 학습 에셋의 로드·컴파일 오류는 0개였습니다. 원 경고는 로그에 보존했습니다.

속도 변경은 검증용 복사본에서만 수행하고 300으로 복원했습니다. 검증 종료 후 메인의 원본 학습 파일 9개는 모두 최초 해시와 같았고, 기존 dirty Blueprint도 그대로였습니다. 원시 결과는 기존 보고서의 `saved_runtime_followup`, MCP 없는 로드 결과는 같은 기록 폴더의 `no-mcp-cold-result.json`에 있습니다. 이번 확인은 새 프로세스의 에디터 PIE와 MCP 없는 로드·컴파일이며, cook/package와 패키징 게임 실행은 후속 범위입니다.

## 시행착오와 적용 조건

### 한국어 에디터의 공식 Blueprint DSL

- **원인:** 설치 버전의 노드 메뉴 이름과 핀 표시 문자열은 한국어인데 DSL 내부는 일부 영어 노드 ID 및 `Exec` 문자열을 사용합니다. 따라서 이름 조회 실패뿐 아니라 실행 핀 분류가 달라지는 경우도 있었습니다. `Create Event`의 OutputDelegate도 일반 데이터 출력에서 제외되어 별도 연결이 필요했습니다.
- **해결:** 이번 학습 호출부에서 조회한 정확한 노드 이름을 사용하고 전달되는 NodeInfo의 실행·와일드카드 구분을 정규화했습니다. Unreal 구조체 배열은 꺼낸 원소 수정 후 배열 자체를 다시 대입했습니다. Create Event의 실제 OutputDelegate를 Bind/Unbind의 Delegate에 연결한 뒤 `ObserveReady`를 선택했습니다. 공식 MCP·엔진 소스는 수정하지 않았습니다.
- **적용 조건:** UE 5.8.3의 현재 한국어 에디터와 이 실습에 필요한 좁은 노드 집합입니다. 영어 에디터나 다른 노드 전체를 검증한 범용 변환기가 아닙니다.
- **근거:** 컴파일뿐 아니라 PIE에서 상태 전이·실제 콜백·구독 해제를 확인했습니다. 초기 실패와 부분 그래프는 후속 관측으로 확인한 뒤 소유 그래프만 수정했습니다.

### Python UObject 참조와 맵 전환

- **원인:** `ExecuteFile` 전역의 `world` 변수가 새로 만든 World를 보유한 상태로 다른 MCP 요청에서 맵을 열었습니다. 메모리 정리 중 `FPyReferenceCollector`가 해당 World를 참조하여 엔진이 `World Memory Leaks`로 종료됐습니다.
- **해결:** 저장된 에셋 상태를 확인하고 에디터를 복구했습니다. 이후 Python 작업을 요청별 함수 범위에 두어 World·Actor 래퍼가 전역에 남지 않게 하고, 맵·PIE 전환은 함수 반환 후 별도 요청으로 수행했습니다.
- **적용 조건:** Python 실행 상태가 요청 사이에 유지되는 에디터에서 World·Actor 참조를 만드는 경우입니다. 단순 Python 함수 범위만으로 모든 native 수명 문제를 해결한다는 뜻은 아닙니다.
- **근거:** 원 크래시 로그와 덤프는 `Saved/Crashes/UECC-Windows-15EDA0D247AA34E6007F8383F0717961_0000`에 보존했습니다. 복구 후 PIE 종료와 두 번의 맵 전환이 성공했습니다. 크래시 전 사용자 dirty 패키지는 없었고 생성 에셋 저장은 완료돼 있었습니다. 원 맵 로드 요청은 실패로 유지하며 복구 성공으로 바꾸지 않았습니다.

### Blueprint 기본 변수 플래그와 검수 순서

- Actor 인스턴스에서 Profile·DependenciesReady를 지정하려면 인스턴스 편집 가능 설정이 필요했습니다. 기본 변수 생성만으로 이 조건이 충족된다고 가정하지 않습니다.
- 월드 배치는 완료됐으나 변경한 Blueprint의 저장 전 dirty=0을 요구하여 검수 순서가 잘못 거절한 경우가 있었습니다. 실제 배치를 확인하고 남은 Blueprint 저장만 수행했습니다.

## 후속 학습

실제 Lyra 샘플을 확보한 뒤 Experience → PawnData → Hero/PawnExtension → 입력 초기화의 실제 호출 관계를 대조합니다. [Enhanced Input 후속 실습](EnhancedInput_Study.md)으로 표준 Enhanced Input의 좁은 로컬 사용은 확인했습니다. Lyra 고유 입력 연결, GameFeature, GAS, CommonUI, 네트워크·비동기 조건은 아직 실행 검증하지 않았습니다.

## 공식 자료 확보와 기존 활용 확인 — 2026-10-26

공식 UE 5.8 문서 5개를 로컬 `cubeless/records/operations/lyra-basics-study-20261026/official-docs/`에 임시 보관했습니다. 위의 Lyra 개요·Component Manager·입력 문서와 [Enhanced Input](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine), [Abilities in Lyra](https://dev.epicgames.com/documentation/en-us/unreal-engine/abilities-in-lyra-in-unreal-engine)입니다. 다운로드 영수증에 URL, 응답 상태, 바이트 수와 SHA-256을 남겼고, 문서 제목과 본문의 관련 설명이 포함됐음을 확인했습니다. 이 임시 원문은 Git 제외 대상입니다.

현재 전체 샘플 확보 상태는 별개입니다. Launcher 라이브러리에는 Lyra Starter Game이 있으나 재접속 오류가 표시됐고, Fab 웹은 로그아웃 상태였습니다. 공식 EpicGames/UnrealEngine 저장소도 현재 연결과 브라우저에서 404를 반환했습니다. 기존 사용자 프로젝트에 추가하는 화면은 취소했습니다. 샘플 소스·에셋 다운로드 또는 원본 실행을 완료했다고 처리하지 않습니다.

공식 입력 문서에 나오는 `UPlayerMappableInputConfig`는 설치된 UE 5.8.3의 `PlayerMappableInputConfig.h:23`에서 5.3부터 deprecated로 표시되며 `UEnhancedInputUserSettings` 사용을 안내합니다. 현재 `EnhancedInputUserSettings.h`의 `RegisterInputMappingContext`와 `EnhancedInputSubsystemInterface.h`의 `AddMappingContext`·`RemoveMappingContext`도 확인했습니다. 설정 등록과 실제 입력 활성화를 구분해야 하며, 문서의 과거 클래스 이름만으로 현재 구현을 고정하지 않습니다. 실제 Lyra 최신 소스의 사용 방식은 아직 대조하지 못했습니다.

기존 5.6 저장소의 `codex/docs/projects/cubeless-stylized/logs/work-log.md` Section 9.6~9.8에는 다음 활용 기록이 있습니다.

- Event Dispatcher custom event·bind 지원을 추가하고 임시 Blueprint 제작·컴파일을 검증한 뒤 Lyra 분석 보고서를 갱신했습니다.
- assign·unbind·clear 지원도 같은 방식으로 보강하고 분석 결과에 반영했습니다.
- Lyra delegate 사용 지점을 분석하는 수명 분류기를 추가했습니다.

따라서 과거에는 MCP의 기능 공백을 찾고 보강 우선순위를 정하는 자료로 반복 활용됐다고 볼 수 있습니다. 이 기록은 당시 성공의 역사 자료이며 이번 세션에서 재실행한 증거는 아닙니다. Lyra 전체 실무 파이프라인을 자주 실행했다는 근거로 확대하지 않습니다. 당시 readiness 보고서의 분석 대상 프로젝트 EngineAssociation은 `5.7`이며, 보고서가 5.6 MCP 저장소에 있다는 사실과 샘플 자체의 엔진 버전을 구분합니다.
