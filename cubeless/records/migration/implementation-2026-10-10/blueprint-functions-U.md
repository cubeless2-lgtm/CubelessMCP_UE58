# U: 함수 그래프와 타이머 저작

설치 UE 5.8.3 공개 API에 근거한 제한 범위를 구현하고 U1 빌드·공식 8 Toolset/51도구 등록·대표 timer 함수 저작·저장 및 실제 Codex 새 프로세스 재조회를 통과했다. 아래 초기 설계 근거와 미지원 범위를 보존하며 실제 검증 수준은 마지막 단락에 구분한다. 전체 원본 pipeline이나 함수 실행 완료를 뜻하지 않는다.

목표는 기존 `build_cube_fluid_ninja_mouse_timer.py`의 `CFNL_AdvanceMouseTime` 저작 흐름이다. `TimeSinceLastClick:float`에 `DeltaSeconds:float`를 더해 다시 저장하는 함수만 작성하고, Tick이나 다른 이벤트에 무조건 연결하지 않는다. 호출 시점은 기존 caller gating 책임이다. 원본 전체 builder의 복제·검수·부모 proof 이전과 함수 작성 도구 이전은 별도 항목이다.

## 세 도구와 숨김 어댑터

새 `CubelessFunctionApi/Operations/Library`에 UE 접근·결과 인코딩·숨김 반사 어댑터를 둔다. 실제 공식 Toolset의 공개 이름은 다음 원본 세 이름을 유지한다. C++ 함수는 `BlueprintCallable, AIIgnore`, FString JSON 반환이며 외부 공개 generic dispatcher는 만들지 않는다.

| 공개 도구 | 제안 숨김 C++ positional 계약 |
| --- | --- |
| resolve_blueprint_graph | BlueprintPath, GraphName, GraphId, GraphType, bCreateIfMissing |
| add_blueprint_function_parameter | BlueprintPath, ParameterName, ParameterType, Direction, DefaultJson, TypeObject, bIsArray, GraphName, GraphId, GraphType, bCreateGraphIfMissing |
| add_blueprint_call_function_node | BlueprintPath, FunctionClass, FunctionName, ParamDefaultsJson, NodePositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing, bAllowLatent, bAllowEditorOnly, bAllowWildcard, bAllowDeprecated, bAllowInternal |

Python은 기존 순서를 가능한 한 보존하고 Any/dict/position을 명시 JSON string adapter로 바꾼다. `operation_id`와 배치 child identity는 기존 공통 journal을 사용한다. 생략된 parameter default는 `DefaultJson=""`, 생략된 call defaults는 `{}`, 위치는 `[0,0]`다. 미지원 옵션은 입력 단계에서 명시적으로 거부하며 받아 놓고 무시하지 않는다. 구현된 public signature는 공식 U schema에 보존하며 미지원 옵션은 명시 거부한다.

## 설치 엔진의 공개 API와 직접 공식 도구의 차이

기준 엔진 루트는 `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`다.

- `Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h:338`의 exported `CreateNewGraph`와 `:399–408`의 public `AddFunctionGraph<UFunction>`으로 정확한 이름의 함수 그래프를 만들 수 있다. 후자는 자식 Blueprint 변수 조정과 `MarkBlueprintAsStructurallyModified`를 호출하므로 외부 자식/인스턴스 선행 검사가 필요하다. 자동 full compile/save를 요청하지 않지만 skeleton regeneration은 발생할 수 있다.
- `Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary.cpp:606–635`의 공식 `AddFunctionGraph`는 이름이 잘못됐거나 충돌하면 `FindUniqueKismetName`으로 자동 변경한다. exact-name 계약에서는 호출 전 검증과 실제 이름 후검수가 필요하며, 직접 위 공개 C++ API를 쓰는 것이 더 명확하다.
- `Editor/BlueprintGraph/Classes/K2Node_EditablePinBase.h:142,169,178,211`에는 `UserDefinedPins`, `CanCreateUserDefinedPin`, `CreateUserDefinedPin(..., bUseUniqueName=false)`, `ModifyUserDefinedPinDefaultValue`가 공개되어 있다. `Private/K2Node_EditablePinBase.cpp:162–178`의 생성은 이름/핀 배열을 바꾸지만 구조 변경 완료를 대신 처리하지 않는다. `:421–478`의 default setter는 실제 K2 pin default 검증 실패를 bool로 돌려준다.
- 공식 반사 API `BlueprintGraphEditor.add_graph_input_parameter`는 `Private/BlueprintEditorLibrary/BlueprintGraphEditor.cpp:1612–1637`에서 unique-name 기본값으로 생성하고 void인 `TrySetDefaultValue`를 호출한다. 이 함수에는 명시 `MarkBlueprintAsStructurallyModified`가 없다. 따라서 그대로 호출 성공만 확인해서 원본의 exact-name/default/reconstruct/structural 검수와 동등하다고 판정할 수 없다.
- output 함수는 같은 파일 `:305–337`에서 결과 노드가 없으면 만들고 모든 결과 노드에 출력을 추가한다. input 대표를 확인했다고 output·복수 return node까지 확인한 것으로 처리하지 않는다.
- `BlueprintFunctionNodeSpawner.cpp:210–276`의 두 `Create` overload는 명시 node class를 받아도 전역 promotion operator cache의 spawner를 반환할 수 있다. 따라서 exact primitive signature를 요구하는 이 구현에서는 사용하지 않는다. 공개 `Runtime/Engine/Classes/EdGraph/EdGraph.h:274–316`의 `FGraphNodeCreator<UK2Node_CallFunction>`과 `K2Node_CallFunction.h:209`의 exported `SetFromFunction`을 사용하고 `Finalize`로 GUID/기본 pin을 만든다. 전역 promotion 설정을 변경하지 않으며 `GetTargetFunction` 및 실제 모든 함수 parameter pin type으로 exact identity를 후검수한다.
- 공식 반사 `BlueprintGraphEditor.add_call_function_node` 구현 `:770–798`은 경로를 load하고 실패 시 local skeleton function 이름을 찾으며, spawner 결과를 `CastChecked`한다. legacy의 callable/editor/latent/wildcard/default 검수 전체를 제공하는 것은 아니다. 선언 헤더가 엔진 Private 아래 있으므로 이를 C++에서 include하지 않는다. 공개 BlueprintGraph C++ 헤더 또는 반사 Python API만 사용한다.
- `EdGraphSchema_K2.h:922,1228`의 공개 `CanUserKismetCallFunction`/`CanFunctionBeUsedInGraph`를 추가 근거로 쓸 수 있다. 구현 `Private/EdGraphSchema_K2.cpp:709`는 graph 종류별 latent 가능 여부를 판정한다. `:899`의 callable 판정은 에디터 deprecated 노출 설정에 영향을 받으므로 플러그인 자체의 명시 옵션 검증도 유지해야 한다.
- 공식 `BlueprintEditorLibrary.get_basic_type_by_name` 구현 `BlueprintEditorLibrary.cpp:2105–2127`은 `float` 문자열을 기본 타입으로 인정하지 않아 int로 fallback하고, `real`은 double로 만든다. 원본 float/single은 **real/float**, double/real은 **real/double**로 구별해야 하므로 C++에서 정확한 `FEdGraphPinType`을 만든다. S 멤버 default 문법과 함수 pin default 문법은 서로 같다고 가정하지 않는다.

## 최초 coherent 범위와 미완료 옵션

함수 그래프는 `/Game` 정상 idle Blueprint의 소유 그래프만 수정한다. exact graph name/GUID/type를 교차 확인하고 불일치/모호함은 실패다. 기존 그래프 읽기는 원본 event/function/macro/delegate/any 분류를 사용할 수 있으나 새 생성은 명시 function name만 허용한다. explicit graph GUID로 새 생성하거나 잘못된 이름을 자동 보정하지 않는다. 원본은 selector가 모두 비면 `create_if_missing=false`여도 event graph를 만들 수 있었으므로 새 구조에서는 이 숨은 변이를 재현하지 않는다. 별도 event graph 생성 이전은 미완료로 남긴다.

첫 timer 필수 parameter는 input, scalar float/single 또는 double/real이며 `DeltaSeconds:float`를 보존한다. 다른 primitive를 같은 경로로 안전하게 지원할 수 있는지는 구현 시 확정한다. output/return, array, enum/object/class/custom struct, by-reference 관련 동작은 이 대표 검수로 닫지 않는다. 원본의 `type_object`, `is_array`, 방향 옵션은 미지원일 때 이름을 포함해 명시 거부한다.

call node 최초 범위는 정확한 `/Script/Engine` 런타임 native owner의 static BlueprintCallable/Pure 함수와 검증 가능한 primitive pin literal이며, `KismetMathLibrary.Add_DoubleDouble`이 대표다. owner 클래스·함수 flags·모든 pin type/object 참조를 검토하고 MCP/editor/임의 plugin 런타임 참조를 거부한다. Engine 모듈에 있다는 이유만으로 `FUNC_EditorOnly`나 내부/latent/custom thunk를 허용하지 않는다. 원본 `allow_editor_only`는 사용자의 게임 런타임 독립성 요구와 충돌하므로 true를 지원한다고 약속하지 않는다. latent/wildcard/deprecated/internal true, nonstatic self/외부 target, 사용자 Blueprint 함수 등은 후속으로 명시 추적한다. raw C++ 함수 클래스 alias(`class_name/target_class/target`)와 defaults alias(`params`) 역시 공개 canonical signature 완료와 구별한다.

Graph creation을 parameter/call의 `create_graph_if_missing` 옵션으로 수행하려면 함수/type/default 선행 검증이 먼저다. 이를 한 mutation으로 안전하게 묶지 못하는 옵션은 unsupported로 남기고, graph를 만든 뒤 validation 실패를 preflight/no-change처럼 보고하지 않는다. timer 대표는 resolve(create)→parameter→call의 별도 journal action 순서를 사용한다.

## 변경·실패·검수

공통 envelope는 success/stage/error_code/error, mutation_attempted, changed(bool/null), change_state_known, postcondition_passed, compile_requested=false, compile_attempted=false, compiled=null, save_requested=false, save_attempted=false, saved=false다. data에는 before/after, dirty/status, load 관찰, graph identity, 실제 신규 node/pin identity와 적용된 default를 둔다. 생성된 객체를 오류 후 삭제했다면 그 시도와 실제 남은 상태를 보고하며 원자적 rollback으로 표현하지 않는다.

모든 mutation에서 원래 graph GUID/경로와 기존 node·link를 보존하고, 새로운 graph의 entry가 정확히 하나인지, parameter 이름·방향·pin subtype/default가 요청과 같은지, call node의 target UFunction path가 요청과 같은지 독립 재조회한다. 명시적인 graph/parameter name collision은 아무것도 만들기 전에 거부한다. 참조 load 이후에는 소유 graph/Blueprint와 사전조건을 다시 확인한다. 저장된 사용자 에셋·외부 archetype에 영향이 있는 경우 지원 범위 밖으로 선행 거부한다.

대표 흐름은 새 격리 BP에서 S float member→U exactfunctiongraph→U DeltaSeconds float input→T getter→U Add_DoubleDouble→T setter→네 typed/exec link→공통 compile/save 한 번이다. real(float/double) 연결의 실제 schema response와 readback을 기록하며 자동 conversion node가 생긴다고 가정하지 않는다. 호출되지 않은 함수의 실행 효과나 runtime accumulation을 저작·컴파일 통과로 주장하지 않는다. 필요 시 기존 실행 검수 경계에서 확인하고 매 단계 별도 패키징/검수 프레임워크는 추가하지 않는다.

원본 연결은 `Python/tools/node_tools.py:565,1445,2259` → BlueprintNode active handler → `UnrealMCPBlueprintNodeCommands_Part1.cpp:1966`(call), `Part2.cpp:266`(parameter), `:1566`(resolve)다. shared type/default/function-validation/graph 선택은 `UnrealMCPBlueprintNodeCommandsShared.h:956–1011,9202–9351,9583–9664`에 있다. 원본 timer와 부모 pipeline 검수 맥락은 인접 `blueprint-variable-graph-T.md`에 보존했다.

## U 구현 후보의 구체 범위

새 함수 parameter는 scalar bool/boolean, int/integer, int64/integer64, float/single, double/real, name/string/text를 지원한다. JSON default는 최대 65536 UTF-16, depth 8, 값 수 1024이며 실수 포함 숫자 크기는 JSON 정확 정수 범위 이내로 제한한다. 새 이름은 ASCII identifier 1..100, graph 256/node 2048/parameter 128 상한이다. 이러한 제한과 미지원 방향·type_object·array는 legacy 전체 동등성 완료로 세지 않는다. 

entry ReconstructNode는 설치 EdGraphPin.cpp:731–738의 TransferPersistentDataFromOldPin에서 PinId를 보존한다. 기존 entry pin의 ID/default/link 전체 snapshot을 비교하여 보존을 검사한다. default의 false/0처럼 문자열표현이 달라도 타입상 같은 경우는 공개 DoesDefaultValueMatch를 사용한다. 모든 source 검토는 root의 실제 빌드·공식 MCP 호출 검수와 별개다.

## U 실제 저작·저장·cold 증거

`build-functions-U1.log`의 증분20.50초, `python-contract-tests-U.log`의81tests/3.092초를 통과했다. `registered-schemas-and-diagnostics-U.json`은8Toolset/51도구, 작성PID30308, plugin108파일과 DLL SHA `87e5d18c7434d0687894dcb16b0c16c8d7ebaeab837041396deeb3d12089a16a`를 기록한다. 등록부/버전 호환부/작업 로직 분리를 유지하며 별도 외부 MCP 서버·게임 C++·엔진 수정은 추가하지 않았다.

`cubeless/tools/pipelines/blueprint_function_timer.py`(SHA `b3359e5ac7fb7c849ef044c11f2c4b935fd6232cf2a5c88c7193e425f63d4c7e`)의 U 실행 원문은 후속 V 부모 선택 추가 전에 `../../verification/implementation-2026-10-10/source-snapshots/blueprint_function_timer-U.py.source`에 동일 SHA로 보존했다. 이 U 스크립트는 원본 timer 저작 순서를 fresh UUID Actor BP에 적용한다. float 변수0·category="Mouse Interaction"을 보존하고3-child 변수/함수/입력,3-child getter/call/setter,5-child 네 연결/최종 compile-save 배치를 수행했다. 마지막 full compile/save는 각1회이며 factory 암묵 컴파일은 별도 관측이다. `blueprint-function-timer-U.json`은 정확한 graph GUID/path, entry 한 개와 총4노드, exact plain K2Node_CallFunction/UFunction identity, data3/exec1 양방향 연결과 기존 노드/핀 보존을 검수한다. 실수 세 연결은 float→double/float→double/double→float이고 type_exactly_equal=false이지만 엔진 direct MAKE가 성공했다. 이는 bytecode 변환 실행 결과의 검증은 아니다.

동일 입력명은 function_parameter_collision/preflight에서 변경 전에 거부됐고 전체 graph가 같았다. EventGraph는 factory 상태 그대로이며 Tick이나 함수 caller를 설치하지 않았다. 공식 반환 CDO 경로에서 TimeSinceLastClick=0을 읽었다. 단계별 원응답·실제 변경·compile/save·dirty·불확실성·재시도 여부를 보고하며 자동 재시도는0회다.

20 named calls/29 HTTP, 단계 elapsed 합계1.1892045초·전체1.7205148초다. 세 배치 호출0.1411293/0.1301898/0.2678329초, 내부0.088888/0.088654/0.213807초는 warm NullRHI·loopback 작은 단일 fixture 측정이다. 이전 버전 대비 개선율로 해석하지 않는다.

`actual-codex-functions-U.json`은 실제 Codex 공식 연결의 새 ID graph GUID 조회가 created=false/changes=[]/replay=false임을 기록한다. `actual-codex-cold-U.json`의 새PID51508(작성30308) cold9checks에서 전체 함수 graph/EventGraph/GUID/공식 CDO0/저장 SHA·mtime가 같고 dirty=[]였다. 저장 registry1root/1package의 edges/violations/incomplete/dirty=[]이며 packaging_verified=false다. `source-and-asset-persistence-U.json`은 U 등록 snapshot의 root/isolated108파일 일치와 에셋 바이트 불변을 확인한다. `target-preservation-after-U.json`에서 원래 보호1,059개 승인 uproject 외 변경0을 확인했다. U 전체 BuildPlugin·MCP 없는 패키징·함수 누적 실행은 not_run이다. N 전체 배포와 S/T 이하 증거는 해당 과거 source 범위다.

원본 ActorComponent 부모/복제·부모 proof builder, transient call_method 누적/press-release reset 검수, 실제 host의 세 조건 Tick gate는 이 Actor 저작 fixture에 포함되지 않았다. `verify_cube_fluid_ninja_mouse_buttons.py:114`의 transient 검수도 caller_gate_verified=false를 반환한다. 함수 실행과 caller gating을 별개로 추적하며 전체 원본 pipeline·parameter/call 옵션의 동등성은 계속 미완료다.
