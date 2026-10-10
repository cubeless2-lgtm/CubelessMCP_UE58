# UE 5.8.3 공식 Toolset 등록 조사

작성일: 2026-10-10 (Asia/Seoul). 범위: 설치된 엔진 소스 읽기와 계획 문서 작성. 구현, 빌드, 플러그인 설정 변경, 에셋 변경은 수행하지 않았다. 아래의 “확인”은 소스에서 확인한 의미이며 실제 공식 MCP 호출 검증과 구분한다.

## 결론과 검증 경계

- 설치본은 UE **5.8.3**, Changelist **58210709**, CompatibleChangelist **55116800**, Branch **++UE5+Release-5.8**이다. 근거: `C:/Program Files/Epic Games/UE_5.8/Engine/Build/Build.version:2`.
- 프로젝트 플러그인의 C++ 또는 Python Toolset을 공식 Toolset Registry에 등록하면 공식 MCP Editor adapter가 발견하여 노출하는 경로가 실제 소스에 존재한다. 외부 Python MCP 서버나 자체 TCP 브릿지는 이 경로의 필수 요소가 아니다.
- 최초 공식 연결의 `list_toolsets` 호출은 `MCP tool call requires approval, but approval policy is never`로 차단되었다. 같은 날 권한 변경 후 정식 도구로 재조회하여 Toolset 52개 목록, EditorAppToolset 스키마, `IsPIERunning=false` 읽기 응답을 확보했다. [연결 증거](official-readonly-call-evidence.json). CubelessMCP_UE58 자체 등록·검색·호출은 아직 검증하지 않았다.
- 기존 5.6 기능의 동등성은 이 조사만으로 확정하지 않는다. 아래 공식 도구·API는 대체 후보이며 기능별 입력, 출력, 오류, 부작용, 배치·검증 계약을 비교해야 한다.

이하 엔진 근거 경로에서 `E`는 `C:/Program Files/Epic Games/UE_5.8/Engine`, `P`는 `E/Plugins/Experimental`이다.

## 확인된 등록 경로

| 항목 | 실제 설치 소스 근거 | 설계에 반영할 내용 |
|---|---|---|
| C++ Toolset 기반 클래스 | `P/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/ToolsetDefinition.h:12` | `UToolsetDefinition` 상속, 정적 UFUNCTION, `meta=(AICallable)`가 공식 계약이다. |
| 등록·해제 API | `P/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/UToolsetRegistry.h:26` | `RegisterToolsetClass`, `UnregisterToolsetClass`, `IsToolsetClassRegistered`, `GetToolsetJsonSchema`, `GetAllToolsetJsonSchemas`가 공개 API다. |
| 실제 C++ 사용례 | `P/Toolsets/EditorToolset/Source/EditorToolset/Private/EditorToolsetModule.cpp:13` | Startup에서 등록하고 Shutdown에서 역순으로 해제한다. |
| 초기화 시점 | `P/Toolsets/EditorToolset/EditorToolset.uplugin:18` | 공식 EditorToolset은 Editor 모듈, `PostEngineInit`, `TargetAllowList=[Editor]`를 사용한다. |
| Python 정의 | `P/ToolsetRegistry/Content/Python/toolset_registry/tests/demo_toolset.py:41` | `@unreal.uclass()` + `unreal.ToolsetDefinition` + `@toolset_registry.tool_call` + `@staticmethod`; 매개변수와 반환 타입을 명시한다. |
| Python 시작 | `P/Toolsets/EditorToolset/Content/Python/init_unreal.py:6` | Content/Python/init_unreal.py에서 toolset 패키지를 import하고 등록하는 실제 공식 예제가 있다. |
| Python 등록 객체 | `P/ToolsetRegistry/Content/Python/toolset_registry/registration.py:27` | `Registration.register/unregister`는 Registry 공개 API를 호출한다. |
| 자동 MCP 반영 | `P/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolEditor.cpp:49` | Registry 변경을 구독하고 도구 adapter를 다시 등록한다. 엔진 adapter 소스를 프로젝트에서 수정할 필요가 없다. |
| 검색 모드 | `P/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.cpp:159` | `bEnableToolSearch=true`이면 list_toolsets / describe_toolset / call_tool이 Registry 도구를 발견·호출한다. 개별 도구가 MCP 최상위 목록에 없다는 이유만으로 누락으로 판정하면 안 된다. |
| 즉시 노출 모드 | 같은 파일 `:191` | 검색 모드가 꺼져 있으면 각 Toolset 도구를 MCP native tool로 등록한다. 두 노출 방식은 구분해 기록한다. |
| 도구 이름 | `P/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/FunctionLibraryToolset.cpp:211` | C++는 Script 모듈명 + 클래스명, Python은 모듈 경로 + 클래스명으로 Toolset 이름이 만들어진다. 실제 생성된 전체 이름과 스키마를 기준으로 Cubeless 이름 충돌을 검사한다. |

`RegisterToolsetClass` 반환형은 void이다. Python `Registration.register()`의 True도 Registry 가용성과 등록 루프 실행을 뜻하며 개별 등록 성공을 보증하지 않는다. 실제 구현은 등록 후 `IsToolsetClassRegistered`, 스키마, 공식 list/describe/call을 모두 확인해야 한다. 중복 이름은 Registry에서 거부한다. 근거: `UToolsetRegistry.cpp:22`, `ToolsetRegistry.cpp:122`, `registration.py:27` (모두 위 ToolsetRegistry 디렉터리).

## 세 계층 분리 제안

사용자의 최종 선택에 따라 Python 구현은 공식 관례인 `Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58`에 둔다. 플러그인의 `Content/Python/init_unreal.py`가 패키지를 import하고 등록하며, 별도의 외부 Python 경로와 경로 추가 전용 C++ bootstrap은 사용하지 않는다. C++ 에디터 구현과 런타임 의존성 금지 조건은 유지한다. 구체적인 생명주기·배포 계약은 [현재 이식 계획](../UE58_MIGRATION_PLAN.md)에 기록했다. 실제 CubelessMCP_UE58 시작·종료 실행은 아직 검증하지 않았다.

1. **Registration**: Editor 모듈 생명주기, C++/Python Toolset 정의, 설명·입력·반환 스키마, 등록 상태 검사만 담당한다. 기능군별 Cubeless Toolset을 제공한다. 범용 실행 도구 하나를 기능 이식의 대용으로 삼지 않는다.
2. **Compatibility/UE58**: 실제 작업에서 필요한 UE 5.8.3 API 호출과 타입 차이를 모은다. 현재 5.8.3만 구현·검증하고 다른 버전 분기나 가짜 호환 구현은 추가하지 않는다. 비공개 헤더 의존이 발견되면 기능별 장애로 기록한다.
3. **Operations**: 배치 계획·실행, 입력 의미 검증, 에셋 변경, 컴파일·저장·검증, 작업 결과와 재실행 정책을 담당한다. 이 계층은 MCP 전송이나 JSON-RPC에 의존하지 않는다.

Editor 전용 C++는 사용한다. 등록·비동기 작업 관리·Python에 노출되지 않은 엔진 공개 API 래퍼를 맡기고 Python 가능한 작업은 Python으로 구현할 수 있다. C++ 래퍼가 필요한 기능의 최종 목록은 원본 핸들러 → 엔진 API 매핑을 완료한 뒤 결정한다. 5.6에서 C++였다는 이유만으로 5.8에서도 C++ 전용이라고 단정하지 않는다.

5.8 공식 Python Blueprint 도구는 이미 `unreal.BlueprintGraphEditor`, `BlueprintEditorLibrary`, `BlueprintFactory` 등을 사용한다. 근거: `P/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/blueprint.py:62`, `:177`, `:199`. 이것은 API 접근 가능성의 증거이며 기존 특화 배치·복구·검증 흐름과의 동등성 증거는 아니다.

## 게임 및 패키징 의존성 차단

필수 조건은 게임 → CubelessMCP_UE58, CubelessMCP_UE58 → 프로젝트 게임 모듈의 양방향 하드 의존성이 없고, 생성한 Blueprint·에셋에도 CubelessMCP_UE58 타입·함수·구조체·열거형 등의 직렬화 참조가 남지 않는 것이다. 엔진 기본 타입과 사용자가 선택한 기존 게임 타입을 작업 입력으로 사용할 수 있으나 프로젝트 게임 모듈을 플러그인 Build.cs에 연결하지 않는다.

- CubelessMCP_UE58에는 Runtime 모듈을 두지 않는다. Editor 모듈, Editor 타깃 제한을 적용한다. `.uplugin`에 EditorOnly를 기재하는 공식 사례는 있으나 이것 하나를 패키징 제외 증거로 사용하지 않는다.
- Toolset의 입력·출력용 UObject/구조체와 작업 상태 객체는 에디터 도구 실행용이다. 생성한 게임 Blueprint의 부모, 멤버 타입, 함수 노드, 컴포넌트 타입, DataAsset 클래스 또는 기본값으로 저장하지 않는다.
- 게임 C++의 Build.cs, Target.cs, include, module/plugin 참조와 에셋의 하드·소프트 참조, Blueprint 노드의 함수 소유 클래스·구조체·열거형 참조를 검사한다. 단순 텍스트 검색만으로 바이너리 에셋 참조 없음이라고 판정하지 않는다.
- 플러그인 비활성화 후 테스트 에셋 로드·Blueprint 컴파일, Game/Shipping 빌드·Cook·패키징·실행을 검증한다. `.target`/receipt, staged 파일, 패키지 의존성을 증거로 저장한다.

**공식 ModelContextProtocol도 별도로 제한해야 한다.** `P/ModelContextProtocol/ModelContextProtocol.uplugin:21`의 ModelContextProtocol과 `:26`의 ModelContextProtocolEngine은 Runtime 모듈이다. CubelessMCP_UE58 모듈만 Editor로 만드는 것으로 공식 MCP의 패키징 제외를 보증할 수 없다.

프로젝트 `.uproject`의 Plugins 참조와 CubelessMCP_UE58의 Plugins 참조에서 필요한 MCP 관련 플러그인을 Editor 타깃으로 한정하는 방안은 실제 descriptor API에 근거한다:

- UBT 필드·파싱: `E/Source/Programs/UnrealBuildTool/Configuration/Descriptors/PluginReferenceDescriptor.cs:70`, `:320`.
- UBT 필터: 같은 파일 `:405` / `:411`의 `IsEnabledForTarget`에서 TargetAllowList에 없는 타깃을 제외한다.
- 런타임 descriptor 파싱과 필터: `E/Source/Runtime/Projects/Private/PluginReferenceDescriptor.cpp:64`, `:161`.
- PluginManager 적용: `E/Source/Runtime/Projects/Private/PluginManager.cpp:2425`.
- 모듈 TargetAllowList 필터: `E/Source/Runtime/Projects/Private/ModuleDescriptor.cpp:549`.

설정 변경 전 기존 .uproject 및 관련 플러그인의 전이 의존성을 조사한다. AllToolsets는 여러 플러그인을 활성화하는 aggregator이므로 현재 사용자 작업과 기존 GameFeatures 관련 commandlet 오류를 먼저 분리해야 한다. 본 문서 작성 중 설정은 변경하지 않았다.

MCPClientToolset은 공식 MCP 서버 확장에 필수인 등록부가 아니다. `P/Toolsets/MCPClientToolset/MCPClientToolset.uplugin:6`은 Registry 고객이 local/private MCP 서버에 연결하는 adapter라고 명시한다. CubelessMCP_UE58가 이 플러그인이나 외부 서버에 의존하도록 설계하지 않는다. 현재 프로젝트에서 이미 활성화되어 있다는 사실만으로 사용자 기존 설정을 제거하지 않는다.

## 스레드·비동기·오류 계약

| 확인 사항 | 근거 | 계획상 처리 |
|---|---|---|
| Toolset 실행 및 오류 수집은 게임 스레드 요구 | `P/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/FunctionLibraryToolset.cpp:158`, `:172` | UObject·에셋 변경을 게임 스레드에서 실행한다. 무조건 worker thread로 보내지 않는다. |
| Python 예외를 script error로 변환 | `P/ToolsetRegistry/Content/Python/toolset_registry/__init__.py:44`, `tool_call_impl.py:188` | script error가 게임 스레드 밖에서 정상 수집되지 않는 점을 지킨다. 작업별 실제 변경과 실패 단계는 별도로 결과에 보존한다. |
| Python decorator는 동기 함수 래퍼 | `tool_call_impl.py:193`, `:206` | 단순 async def에 decorator만 붙이면 자동 비동기 도구가 된다고 가정하지 않는다. |
| 공개 비동기 결과 타입 존재 | `P/ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/ToolCallAsyncResult.h:27` | UToolCallAsyncResult의 파생 결과 타입을 반환하고 완료 시 SetValue/SetError로 완료한다. 객체 수명과 대기 작업 상태를 소유한다. |
| 완료 알림의 스레드 처리 | 같은 파일 `:81`, `:105` | SetError/완료 broadcast는 메인 스레드에 전달되도록 구현되어 있다. 이것이 임의의 엔진 API를 worker thread에서 호출해도 된다는 뜻은 아니다. |
| 실제 비동기 Toolset 사례 | `P/Toolsets/AutomationTestToolset/Source/AutomationTestToolset/Public/AutomationTestToolset.h:40`, `:55` | DiscoverTests/RunTests가 UToolCallAsyncResultString을 반환한다. 기존 엔진 비동기 API·delegate와 결합하는 설계의 근거다. |
| MCP 결과 콜백 | `P/ModelContextProtocol/Source/ModelContextProtocol/Public/IModelContextProtocolTool.h:30` | 결과 콜백은 1회 완료 계약이다. MCP 계층은 결과 수신 시 게임 스레드로 전환하지만 Registry 실행 자체의 스레드 계약은 유지된다. |

게임 스레드에서 시작한다는 사실은 긴 비동기 작업들 사이에 에셋 변경 충돌이 자동으로 막힌다는 의미가 아니다. CubelessMCP_UE58 작업 관리자에서 수정 작업을 직렬화하고 read/write 대상 충돌, 다음 batch 단계 진입, 컴파일·저장 완료 시점을 관리한다. 긴 연산은 엔진 비동기 API 또는 tick 단계 분할을 사용하고, 공유 상태를 바꾸는 작업은 완료 경계를 명확히 한다.

공식 Python 패키지 내부 `_asyncio`에는 editor event-loop 구현과 16ms tick 예산이 있지만 내부 모듈·테스트 helper를 안정적 공개 async Toolset API로 간주하지 않는다. 비동기 Python 경로가 필요하면 공개 결과 타입 반환과 실제 엔진 실행을 먼저 최소 사례로 검증한다.

## 취소와 타임아웃의 중요한 한계

서버는 취소 요청을 받으면 해당 MCP tool의 CancelAsync를 호출하고 ActiveRequests에서 제거한다. 근거: `P/ModelContextProtocol/Source/ModelContextProtocol/Private/ModelContextProtocolServer.cpp:697`, `:719`.

하지만 기본 `IModelContextProtocolTool::CancelAsync`는 빈 구현이며, 조사한 Registry adapter와 call_tool 메타 도구는 CancelAsync를 override하지 않는다. 근거: `IModelContextProtocolTool.h:97`, `P/ModelContextProtocol/Source/ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.h:13`, `ModelContextProtocolToolSearch.h:66`.

따라서 클라이언트 취소·연결 종료·타임아웃을 엔진 수정 작업의 취소 또는 롤백이라고 표현하면 안 된다. 긴 수정 작업은 Cubeless 작업 ID, 상태 조회, 결과 조회, 가능한 경우 명시적 협력 취소 기능을 설계한다. 엔진 API가 취소 불가하면 다음 단계 진입 중지만 가능하다는 식으로 한계를 표시한다. 클라이언트 응답이 없어도 결과가 불확실한 수정 작업을 자동 재시도하지 않는다. 동일 요청 재실행은 작업 기록을 먼저 확인한다.

## 구현 단계의 필수 통과 기준

1. 사용자 변경 기준선과 원본 기능 목록을 확보하고 공식 연결 대상 및 상태를 다시 확인한다. 최신 공식 읽기 호출은 성공했으며, 실제 Cubeless 도구의 증거를 별도로 확보한다.
2. Editor C++의 최소 Cubeless 진단 Toolset을 등록하고 스키마·버전·작업 식별자를 반환한다. Python 경로를 쓰면 별도 최소 Python 도구도 확인한다.
3. 공식 list_toolsets → describe_toolset → call_tool 경로로 이름, 스키마, 정상·오류 응답을 검증한다. 등록 후 검사, 중복 등록 거부, 해제, 에디터 재시작, MCP 재연결을 포함한다.
4. 격리된 테스트 폴더에서 작은 에셋 생성·수정·컴파일·저장·재로드와 플러그인 참조 없음까지 확인한다. 실제 사용자 에셋이나 기존 레벨을 사용하지 않는다.
5. 기능군별 5.6 계약을 구현/동등대체/통신 제거/장애로 기록하며 확대한다. 대체된 단순 도구를 묶는 특화 batch·검증 계약도 별도 기능으로 유지한다.
6. 긴 작업의 중간 실패, 부분 변경, 저장 실패, 취소·타임아웃 이후 상태 확인, 중복 실행 방지를 검증한다. 원자적 롤백은 제공한 경우에만 주장한다.
7. 마지막에 게임·패키징 의존성 차단, 기능별 동등성, 성능 측정, 전체 공식 연결 검증을 통과한 항목만 완료로 처리한다.

성능 측정 조건은 에셋 수·복잡도, cold/warm 상태, compile/save 횟수, MCP 호출 횟수, 전체 경과 시간, 최장 게임 스레드 점유와 오류 조건을 기록한다. 아직 성능을 측정하지 않았으며 개선 수치를 주장하지 않는다.
