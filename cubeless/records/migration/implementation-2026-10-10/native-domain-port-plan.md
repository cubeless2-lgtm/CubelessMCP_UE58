# 기존 C++ 도메인 로직 재사용 이식 계획

2026-10-10. **Material 도메인을 첫 재사용 이식 대상으로 권고한다.** 자체 TCP/외부 Python 서버/`UUnrealMCPBridge`를 가져오지 않고, 기존 Material 작업 로직과 보조 함수를 Cubeless Editor 모듈 내부로 추출할 수 있다. 총 21개 기존 도구 중 다음 구현 후보는 19개이며, 캐시 강제 복구와 지속 MPC 동기화 2개는 별도 설계가 필요하다. 2개를 삭제하거나 이전 완료로 분류하는 뜻은 아니다.

현재 작업은 소스 조사와 계획이다. 플러그인 Source, 원본 파일, 엔진 소스, 프로젝트 설정을 수정하지 않았고, 이 도메인의 빌드·등록·실행은 수행하지 않았다. 아래 내용에서 “확인”은 설치 소스 확인을 뜻한다. C++ 컴파일 성공이나 동등성 검증으로 계산하지 않는다.

## 도메인 비교

아래 줄 수는 주석·공백을 포함한다. CommonUtils와 다른 보조 파일은 별도이므로 단순 합산을 전체 이식 비용으로 사용하지 않는다.

| 후보 | 주요 C++ 파일/줄 수 | 재사용에 영향을 주는 연결 | 우선순위 판단 |
| --- | ---: | --- | --- |
| Material | 2개 / 6,630줄 | 작업 cpp 6,588줄 + 선언 42줄. CommonUtils 함수 5개. Bridge·프로젝트 게임 클래스 직접 참조 없음 | 단일 기능군의 21개 실등록 도구를 비교적 작은 경계로 확보 가능 |
| Blueprint 기본·물리 | 3개 / 4,227줄 | CommonUtils, SCS·컴포넌트·부모 클래스 검사, 컴파일·저장, 물리 제약 체인 | 두 번째 좋은 후보. 현재 구현된 생성·검수 도구와 합치는 범위를 먼저 정해야 함 |
| Blueprint 노드 | 11개 / 36,819줄 | 공유 헤더에 AnimGraph·ControlRig·RigVM·Input 노드 등 혼재, 별도 BlueprintFunctionExtractor | 기본 노드만 추출은 가능하지만 전체 파일 재사용 시 많은 기능군·모듈이 따라옴 |
| Editor | 2개 / 7,765줄 | `FUnrealMCPModule::CleanupMCPWeatherEditorPreviewDriver`, 자체 Niagara preview UI, viewport/PIE/Niagara | 일반 actor 조작만 따로 추출 가능. 전체 파일은 UI·모듈 서비스와 결합됨 |
| Batch | 독립 domain cpp 없음 | `UnrealMCPBridge.cpp:1493`의 `HandleBatchCommand`가 같은 Bridge의 `ExecuteCommand`로 재진입 | 배치 반복·중단·집계 규칙을 새 작업 executor로 추출해야 함. Bridge 자체 복사 금지 |

CommonUtils는 2개 / 2,191줄이다. Material 때문에 전체 CommonUtils를 복사하면 Blueprint·actor·dependency audit 유틸리티까지 끌어오므로 필요한 5개만 분리한다. 전체 원본 Build.cs에 있는 Sockets, Networking, HTTP, Niagara Internal include path 및 다른 도메인 모듈 목록도 복사하지 않는다.

원본 UnrealMCP 자체도 Editor 모듈이다. 그러나 기존 모듈 형식만으로 새 플러그인의 독립성을 보증할 수는 없다. 새 모듈과 `.uplugin`의 Editor target 제한, 생성 에셋의 클래스·패키지 참조 검사를 함께 유지한다.

## Material 의존성 경계

원본 위치: `C:/Git/UnrealMCP5_6_1/MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/`.

- 작업 파일: `Private/Commands/UnrealMCPMaterialCommands.cpp`, `Public/Commands/UnrealMCPMaterialCommands.h`.
- 자체 클래스 참조는 `FUnrealMCPMaterialCommands`, `FUnrealMCPCommonUtils` 두 가지뿐이다. 이 두 파일에서 Bridge, singleton, `MCPGameProject` 게임 클래스, 별도 사용자 플러그인 클래스 참조는 발견되지 않았다.
- Material 작업 클래스에는 직렬화되는 UCLASS/USTRUCT 타입이 없다. 생성 노드는 엔진 `UMaterialExpression` 파생 클래스를 사용한다. 단, 사용자 입력으로 임의 파생 클래스를 로드하므로 이것만으로 모든 생성 결과가 안전하다고 주장하지 않는다.
- CommonUtils 의존은 `CreateErrorResponse`, `GetVector2DFromJson`, `IsMCPDependencyReference`, `GetMCPDependencyReferenceError`, `IsReadOnlyCommandName`의 **5개**다. JSON·입력 변환·의존성 거부·작업 종류 판정으로 나눠 기존 Cubeless 계약과 합친다.
- cpp의 모든 include에 대해 실제 UE 5.8 설치 파일을 찾았다. 자체 Commands 헤더 2개 외에 Public/Classes 경로에서 찾을 수 없는 include는 0개다. Private/Internal 전용 include도 0개다. 이는 공개 헤더 접근의 근거이며 모든 멤버 접근·export/link 성공의 증거는 아니다.
- 주요 공개 모듈은 Engine, UnrealEd, MaterialEditor, EditorScriptingUtilities, AssetRegistry, RHI, Json이다. 현재 Cubeless 모듈에 없는 **MaterialEditor, RHI**가 우선 추가 후보다. 실제 컴파일 시 직접 사용 심볼을 기준으로 누락 의존성만 추가한다. RenderCore 등 원본에 있었다는 이유만으로 추가하지 않는다.
- `UNREALMCP_API`와 기존 클래스 이름은 새 private operation 클래스로 바꾼다. 원본 플러그인 DLL이나 별도 플러그인을 링크하지 않는다.
- 원본 `UnrealMCPModule.cpp:92`가 Material sync ticker 종료를 담당한다. 지속 sync를 이식할 때는 Cubeless 작업 관리자와 모듈 Shutdown에서 종료 책임을 직접 소유해야 한다.

Material이라는 이름을 가진 모든 기존 기능이 이 21개에 속하는 것은 아니다. 예를 들어 volume texture 생성과 `set_material_texture_sample_sampler_type_mcp`는 Project 도메인에 있다. 이번 선택은 전체 436개 기능 대응표의 `material` 범주만을 의미하며 다른 범주의 Material 관련 기능을 제외 판정하지 않는다.

## 실제 UE 5.8 API 차이와 위험

아래 엔진 경로의 기준은 `C:/Program Files/Epic Games/UE_5.8/Engine/`이다. 비교한 5.6 헤더도 같은 설치 루트의 `UE_5.6/Engine/`에서 직접 읽었다.

| 항목 | 실제 근거 | 이식 처리 |
| --- | --- | --- |
| Material resource 조회 인자 | 5.6 `Source/Runtime/Engine/Public/Materials/Material.h:1243`은 `ERHIFeatureLevel`; 5.8 같은 파일 `:1312`는 `EShaderPlatform` | UE58 compatibility에서 `GMaxRHIShaderPlatform` 사용. 원본 cpp `:79/:92`에 이미 5.7 분기가 있으나 새 코드에는 현재 5.8 경로만 구현 |
| 재컴파일 반환형 | 5.6 `MaterialEditingLibrary.h:166`은 void, 5.8 `Source/Editor/MaterialEditor/Public/MaterialEditingLibrary.h:267`은 `TArray<FString>` | 반환 오류를 읽되 컴파일 완료 여부·resource 오류·저장 결과를 독립적으로 확인 |
| 재컴파일 내부 부작용 | 5.8 `MaterialEditingLibrary.cpp:984/:1035`는 PostEditChange·MI 갱신·viewport 갱신. `MaterialEditorUtilities.cpp:817`의 스트리밍 데이터 갱신은 GC, SlowTask UI, debug shader 작업 포함 | `RecompileMaterial`을 무조건 가벼운 비동기 제출 API라고 취급하지 않음. UI·스트리밍 갱신 필요성을 분리하고 공개 API 조합을 작은 fixture로 측정해야 함 |
| 명시적 동기 대기 | 원본 Material cpp `:4537/:4803/:5040/:5661/:5768`; 5.8 `MaterialShared.h:2613/:2625/:2678`에 Finish/IsFinished/errors 공개 API | 5개 경로의 `FinishCompilation`을 그대로 MCP 게임 스레드에 두지 않음. 작업 상태·pending resource 검사·tick 완료 확인·마지막 저장으로 재구성 |
| 공개 async 결과 | `Plugins/Experimental/ToolsetRegistry/.../Public/ToolsetRegistry/ToolCallAsyncResult.h:27` 및 기존 `ToolCallAsyncResultString` | 공식 결과 객체를 소유하고 한 번만 완료. UObject 수정은 게임 스레드. disconnect/timeout을 취소·롤백으로 표시하지 않음 |
| 그래프 API | `MaterialEditingLibrary.h:92/154/168/179/232/242/325/373/381/388/402` | 생성·복제·삭제·연결·함수 갱신·layout 공개 함수 존재 확인. 노드 이름·마스크·GUID 충돌 등 기존 보조 의미는 별도로 보존 |
| 노드 입출력 순회 | `Materials/MaterialExpression.h:339/:343/:345/:352/:503` | `GetInput`, `FExpressionInputIterator`, `GetInputName`, `GetOutputs`, `ConnectExpression` 사용. GetInputsView는 5.5부터 deprecated; 원본은 주요 순회에서 이미 Iterator를 사용 |
| 캐시 강제 수정 | 원본 cpp `:5747` const_cast로 FunctionInfos/CRC 삭제. 5.8 `MaterialInterface.h:255/:680`은 const getter, `MaterialCachedData.h:384/:388`에 필드 존재 | 필드가 남아 있다는 이유로 이식 완료 처리하지 않음. 캐시 재생성·저장·재로드·의존성 검증으로 대체 가능성을 먼저 확인. 강제 수정은 별도 미완료 기능 |
| 렌더링 동기화 | 5.8 `MaterialShared.cpp:5007/:5049`의 FMaterialUpdateContext는 옵션에 따라 render thread flush | shader 대기 제거와 전체 게임 스레드 무정지를 혼동하지 않음. 최대 tick 시간 측정과 긴 단계 분할 필요 |

공개 API가 있다는 이유만으로 작업 의미가 완전히 동등하다고 판정하지 않는다. 특히 Material Function은 기존 코드가 `UpdateMaterialFunction` 후 `compiled=true` 기본값을 유지한다. 함수 검수는 임시 consumer material을 컴파일하는 방식 등 별도 검증이 필요하며 검증하지 않은 함수까지 compiled로 확정하지 않는다.

## 등록부 / 호환부 / 작업부 추출 방식

예정 Toolset은 `CubelessMCPUE58Editor.CubelessMaterialToolset`이다. 아직 실제 등록된 이름이 아니며 UHT/Registry 생성 스키마와 공식 describe 호출로 확정한다.

1. **Registration**: 개별 `ResolveMaterialGraph`, `ListMaterialNodes`, `MirrorMaterialParameterCollection` 등의 명명된 정적 UFUNCTION을 제공한다. 기존처럼 `BlueprintCallable, meta=(AICallable)`을 사용하고, 고정된 작업 함수에 전달한다. `ExecuteMaterialCommand(command, params)` 하나를 공개해서 기능 전체 이전으로 처리하지 않는다.
2. **Compatibility/UE58**: 엔진 클래스 로드·graph/expression API·resource 조회·컴파일 상태·save·Asset Registry 부분을 제한된 helper로 추출한다. 5.6/5.7 전처리 분기를 가져오지 않는다.
3. **Operations/Materials**: path/GUID 충돌 식별, 입력·출력 이름 대체 규칙, HLSL 입력 검수, MPC 매핑, texture 치환, 함수 펼치기, 단계 결과를 재사용한다. 순수 graph/domain 코드에서 공식 MCP나 socket을 호출하지 않는다.
4. **Operations 작업 관리자**: mutation gate, 대상 에셋 잠금, durable journal, 컴파일/저장 완료·부분 실패·취소 요청을 소유한다. 기존 Python journal을 거치지 않는 native Toolset도 같은 계약을 구현해야 한다. native 직접 호출 경로에 기록이 없는 상태를 숨기지 않는다.

초기 복사 대상은 Material 파일의 필요한 블록과 작은 helper만이다. CommonUtils 전체·Bridge·listener·socket receiver·기존 모듈·원본 `.uplugin/.Build.cs`는 복사하지 않는다. 처음에는 읽기 5개 등록/조회/호출로 경계를 확인하고, 작업부 함수 단위로 mutation을 활성화한다. 등록하지 않은 미검증 함수도 전체 이식 완료 수에 넣지 않는다.

공식 JSON schema 생성기는 문자열/FName 키의 TMap을 지원한다(`Source/Runtime/JsonUtilities/Private/JsonSchema/JsonSchemaGenerator.cpp:609`). 문자열·bool·정수·배열·문자열 map·scalar map은 구체적 매개변수로 노출한다. 기존 `Any` property value나 동적 properties는 필요시 **해당 도구의 명명된 `ValueJson`/`PropertiesJson` 문자열**로 명시하고 내부 JSON 종류·필드 검사를 수행한다. 전체 작업 매개변수를 무타입 문자열 하나로 감싸는 것을 기본안으로 삼지 않는다. 초기 입력 구조와 바뀐 계약은 파이프라인 어댑터에 명시적으로 반영하고 실제 describe schema를 원본 실등록 schema와 대조한다.

## 첫 확대 후보 19개

전체 Python 원본 서명과 실등록 inputSchema, command 이름, C++ handler 행, 반환 필드, 직접 엔진 호출 목록은 `native-domain-port-plan.json`에 21개 모두 기록했다. 아래 행 번호는 원본 `UnrealMCPMaterialCommands.cpp` 기준이다. 순번은 조사 우선순위이며 컴파일 공통부가 필요한 특화 동작의 실제 등록은 그 공통부 검증 뒤에 한다.

| 후보 | 핵심 입력 / 연결 | 유지할 의미와 검증 |
| --- | --- | --- |
| 1 `resolve_material_graph` | material_path, graph_type → HandleResolveMaterialGraph:3801 → ResolveMaterialGraph/AssetRegistry/LoadObject | 이름·package·object 경로, Material/Function 구분, 이름 중복 후보·오류 |
| 2 `list_material_nodes` | material_path, graph_type, node_type, desc_contains, include_pins → :3840 → GetExpressions/ExpressionToJson | `path:` node_key가 기본 식별자. legacy GUID/이름 호환, GUID 충돌 목록, pin 이름·연결 readback |
| 3 `analyze_material_graph` | material_path, graph_type, include_nodes, include_usage, max_referencers → :3921 | Material/Function/Instance 분기, 설정·semantic properties·usage hints. 분석 전후 dirty 상태 동일 |
| 4 `list_material_collection_parameter_nodes` | material_path, graph_type → :4036 | collection path·parameter ID·이름·종류를 실제 expression과 대조 |
| 5 `get_material_parameter_collection_values` | collection_path, parameter_names, include_asset_defaults, include_runtime → :6092 | asset 기본값과 world instance 값을 분리. PIE/editor world 선택·없음 상태 명시 |
| 6 `mirror_material_parameter_collection` | source/target collection, parameter_names, copy_defaults, preserve_ids, save → :4139 | scalar/vector 누락·타입 충돌·ID 보존·기본값 복사·한 번 저장·재로드 |
| 7 `replace_material_collection_references` | material, source/target collection, graph_type, save → :4376 | 같은 이름 매핑·없는 매개변수·already-target skip, 변경 목록, 컴파일 실패 시 저장 안 함 |
| 8 `replace_material_collection_parameters` | material_path, use_runtime_values, save → :4586 | runtime/default 선택, scalar/vector node 변환, consumer mask·연결 보존, 최종 compile/save |
| 9 `replace_material_texture_references` | material_path, replacements map, graph_type, save → :4848 | old→new path 검수, 실제 교체/skip 목록, source별 변경 수, compile/save |
| 10 `add_material_node` | material_path, expression_class, graph_type, position, selected_asset, properties → :5092 | 클래스·참조 검사, 실패한 새 노드 cleanup 결과, custom/external-code 즉시 갱신 유예 보존 |
| 11 `set_material_node_property` | material_path, node_id, property_name, value, graph_type → :5298 | object/class/struct/enum 값 변환, MaterialFunction 속성 특수 처리, 쓰기 전 검수·후 readback |
| 12 `connect_material_nodes` | material_path, from/to IDs, input/output 이름, graph_type → :5385 | fallback output/input 명칭, 유효하지 않은 pin 오류, 기존 연결 변경 여부와 실제 링크 확인 |
| 13 `connect_material_property` | material_path, from ID/output, material property → :5468 | material property 지원 여부, root 연결/마스크 readback |
| 14 `delete_material_node` | material_path, node_id, graph_type → :5541 | 삭제 대상·consumer 연결 변화·재실행시 not-found 계약 |
| 15 `layout_material_nodes` | material_path, graph_type → :5589 | Material/Function API 분기, 좌표 변경 readback, 불필요한 compile/save 없음 |
| 16 `set_material_parameter_collection_values` | collection_path, scalars/vectors, set_asset_defaults, set_runtime, save → :6200 | asset/runtime 각각 결과, 없는 이름, 부분 성공, 단일 저장, 재로드와 runtime 지속 범위 |
| 17 `add_custom_material_node` | material_path, HLSL code, output_type, inputs, defines, include paths, position → :5196 | HLSL identifier 검수, custom input 연결 전 갱신 유예, 정의/경로 readback, 후속 compile 오류 |
| 18 `expand_material_function_calls` | material_path, node_id, recursive, exclude_engine_functions, max_passes, save, allow_partial_save → :5831 | 복제·preview default·input/consumer 연결·mask 보존·nested pass 한도·부분 복구·재로드 |
| 19 `compile_and_save_material` | material_path, graph_type, save → :5625 | 비동기 완료, resource 오류, save requested/result, dirty 전후, 함수 검수 범위 분리 |

실질적인 첫 end-to-end fixture는 **MPC mirror → collection reference/parameter 치환 → 분석 → 컴파일 → 저장 → 새 에디터 재로드**로 잡는다. 단순 노드 생성만 성공했다고 복잡한 기능군이 완료됐다고 판정하지 않는다. 함수 펼치기 fixture는 scalar/vector 입력, preview default, nested function, engine function 제외, 여러 소비자, channel mask, 반복한도와 의도적 실패를 포함한다.

미완료 유지 2개:

- `refresh_material_cached_expression_data`(:5699): 단순 캐시 갱신 외에 const_cast로 stale function 참조를 제거하고 특정 `/Game/UltraDynamicSky`, `/Game/_MCP_Temp` 경로를 검수한다. 이 하드코딩을 현재 프로젝트 규칙으로 활성화하지 않는다. 필요 정책을 입력/설정으로 분리하고 실제 5.8 손상 fixture에서 저장·재로드 후 의존성 제거를 입증해야 한다.
- `set_material_parameter_collection_sync`(:6451): status/start/stop, 0.1초 기본 ticker, weak collection cache, world runtime값 복사가 있다. 게임 런타임 컴포넌트로 옮기지 않는다. 에디터 전용 작업 수명, PIE/world 교체, Shutdown 해제, 중복 start, 동기화 실패 시 실제 변경·저장 상태를 먼저 구현·검증한다.

## 기존 배치와 결과 계약에서 함께 추출할 부분

`Python/tools/batch_tools.py:34` → command `batch` → `UnrealMCPBridge.cpp:1493`이다. 1..100개, nested batch 거부, 순서대로 실행, stop_on_error, executed/succeeded/failed/stopped_at_index/results를 보존할 가치가 있다. socket timeout·기존 port·`__mcp_ticker_dispatch_effective` 같은 transport 내부 필드는 제거 대상이다.

새 executor는 도구별 검증된 operation 함수를 직접 호출한다. batch가 다시 MCP connection/call_tool을 호출하거나 Bridge를 재진입하지 않는다. 도구별 스키마·검수를 우회하지 않도록 명시된 operation allowlist를 사용한다. 배치 입력을 모두 검수한 뒤 실행하고, 실제 실행 결과와 미실행 항목, 부분 변경, 단계별 compile/save, 재시도 위험을 기록한다. domain 변경을 묶은 뒤 영향 asset마다 컴파일·저장을 한 번 수행하는 모드를 제공하되 기존 의미와 바뀐 부분을 파이프라인에 반영한다.

기존 `FScopedTransaction`은 undo 묶음이며 atomic rollback이 아니다. async 작업 전체에 살아 있는 단일 transaction을 유지하면 다른 에디터 작업까지 잘못 묶일 수 있다. mutation 구간만 명확히 묶고, tick을 넘는 compile/save의 취소·timeout을 rollback으로 표현하지 않는다. 예전 “배치 전체 Ctrl+Z 1개”와 변경된 범위는 별도 호환성 항목으로 기록한다.

원본 함수 펼치기는 `FMaterialFunctionExpansionBatchState`(:3054)가 생성 노드, 입력 복원 포인터, commit 때 삭제할 함수 노드를 보관한다. `RollBackMaterialFunctionExpansionBatch`(:3115)는 이 범위만 복원한다. 프로세스 종료·asset save·다른 작업·GC까지 원자적으로 복구한다는 계약이 아니다. 특히 이 포인터 상태를 그대로 여러 tick에 걸쳐 보존하지 않는다. 실제 복원 여부를 graph snapshot으로 검증하고 `rollback_attempted`, `rollback_verified`, `remaining_changes`를 나눠 보고한다.

원본 Bridge의 결과 래핑(`UnrealMCPBridge.cpp:1203` 부근)은 `success` 필드가 없으면 성공으로 처리한다. Material compile 결과는 `compiled=false`여도 별도 `success=false` 없이 돌아올 수 있다. 따라서 이 transport 래퍼의 성공 판정을 복사하면 compile 실패를 성공으로 노출할 위험이 있다. 새 도구에서는 명시적 후조건을 계산하되 원본 상세 필드를 결과 안에 보존한다. “기존이 성공으로 반환했으므로 동등하다”는 판정은 하지 않는다.

## 에셋 독립성과 검증 기준

원본 문자열 거부는 UnrealMCP/CubelessControlRigTools 이름 중심(`CommonUtils.cpp:305`)이다. 새 CubelessMCP_UE58, 공식 MCP/Registry/Python Editor 타입을 포함한 현재 정책으로 바꾸고 expression class 상속·package module flags, 참조 property/selected_asset의 실제 객체를 검사한다. `FindMaterialExpressionClass`(:1028)의 첫 full path 분기는 abstract 검사 없이 반환하므로 모든 분기에서 abstract·타입·허용 모듈 정책을 일관되게 확인한다.

게임 모듈 include/Build.cs 의존성, 새 Runtime 모듈, 저장되는 Cubeless 전용 material expression/actor/component는 만들지 않는다. 검증 에셋은 `/Game/CubelessTests/<run_id>/Material/` 같은 별도 위치에만 생성한다. 실제 최종 root는 root 에이전트가 사용하는 현재 테스트 정책과 맞춘다.

등록 → 성공 호출 → 오류/부분 실패 → 내용 readback → compile → save → 새 에디터 reload → package dependency 검사 수준을 각각 기록한다. 마지막 Game/Cook/Shipping/실행에서 플러그인 독립성을 확인하기 전에는 완성된 게임에 영향이 없다고 확정하지 않는다. read-only 초기 도구도 load가 유발하는 dirty 변화가 없는지 확인한다.

성능 측정 항목은 asset/node 수, shader cache 상태, MCP 횟수, compile/save 횟수, 전체 시간, 최대 게임 스레드 tick 점유, compile pending 시간, failure/timeout 복구 시간이다. 아직 Material 성능 측정은 하지 않았다. 반복 PostEditChange가 암묵적으로 재컴파일을 유발하는 부분도 측정해 적절한 최종 갱신 단계로 묶는다.

## 증거 파일

- `native-domain-port-plan.json`: 원본 파일 해시·include·의존성·위험 마커, 실제 UE58 헤더 위치, Material 21개 도구의 원본 실등록 schema → Python source/signature → command → C++ handler/행 → 직접 API 호출·결과 필드.
- `native-domain-port-plan.py`: 위 JSON을 만든 읽기 전용 검사 스크립트. C++ 전처리기·컴파일러·전체 call graph가 아니며 결과를 덮어쓰지 않는다.
- `legacy-runtime-registration.eager.json`: 이 분석에 사용한 실제 FastMCP 1.5.0 등록 스키마 근거.

전수 111개 native 파일의 완전한 API·런타임 동등성 감사를 끝냈다는 보고는 아니다. 이번에는 네 후보의 의존성 경계를 비교하고, Material 21개를 다음 구현 단위로 구체화했다.
