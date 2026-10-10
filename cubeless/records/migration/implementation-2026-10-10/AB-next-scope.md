# AB 후보: 버튼 이벤트·입력 바인딩·잠금 소유권

2026-10-11. 원본 및 설치 UE 5.8.3 소스 조사만 수행했다. 아래 API는 공개 선언과 구현을 확인한 후보이며 AB 구현·빌드·등록·실제 실행 성공이 아니다. AA verifier는 별도로 고정했고 이 조사에서 에디터·MCP 호출이나 원본 수정을 하지 않았다.

원본 루트는 `C:/Git/UnrealMCP5_6_1`, 설치 엔진 소스 루트는 `C:/Program Files/Epic Games/UE_5.8/Engine/Source`다. 아래 원본·엔진 경로는 각각 이 루트에 상대적이다. 원장 identity를 새로 만들거나 기존 항목을 완료로 바꾸지 않는다.

## 원본에서 복구할 계약

| 대상 | 실제 입력·동작·반환 | 범위 차이 및 검수 요구 |
|---|---|---|
| `add_blueprint_custom_event_node` | `blueprint_name`, `custom_event_name`, position 기본 `[0,0]`; dispatcher/class/delegate signature 선택, `native_delegate_name` 별칭, nullable `call_in_editor`, graph name/id/type/create 옵션. node/pins/name/signature/graph를 반환한다. | native에는 `event_name` 별칭도 있다. 기존 동명 custom event는 signature 요청이 없으면 재사용하고 명시 call_in_editor 요청이면 수정한다. signature 요청과 동명 event는 오류다. 처음에는 무인자 event 세 개만 대표로 이식하고 delegate/모든 graph 옵션은 별도 미완료로 남긴다. 재사용을 무조건 무변경이라고 보고하면 안 된다. |
| `add_blueprint_input_key_node` | key·position, control/alt/shift/command 기본 false, consume_input true, execute_when_paused false, override_parent_binding false, graph selector/create 옵션. native는 유효·digital·nondeprecated·Blueprint-bindable FKey를 검사하고 flags를 적용한 노드/graph를 반환한다. | 원본은 같은 key 중복도 새 노드로 생성한다. 새 구현이 중복 거부/재사용을 선택하면 명시적 계약 차이다. 실제 host는 LeftMouseButton, consume_input=false, paused=false, override_parent_binding=true를 쓴다. project input mapping을 바꾸지 않는다. |
| ActorComponent `ReceiveEndPlay` | 기존 `add_blueprint_event_node`로 override 생성 후 `then`을 자체 `CFNL_MouseCleanup` 호출에 연결한다. enum EndPlayReason은 원본 흐름에서 사용하지 않는다. | 신규 전용 도구가 아니라 기존 event 지원 확대다. 현재 Z 코어는 Actor 부모만 허용하고 입력 signature는 primitive만 허용하므로 ActorComponent와 Engine EndPlayReason enum 두 경계를 명시적으로 추가해야 한다. |
| variable `is_transient` | button-component와 event author가 Controller/MousePressed/TimeSinceLastClick/OwnsLookLock/LookLockController 선언에 true를 전송한다. | **현재 원본 공개 wrapper에 이 인자가 없고 활성 native add-variable도 읽지 않는다.** 원본 의도 복구·결함 교정으로 분류해야 하며 기존 구현 동등성 또는 과거 실제 transient 검증을 주장하면 안 된다. |
| Controller look-input | Press는 MousePressed=true와 timer=0을 먼저 쓰고, 소유 잠금이 없고 Controller가 유효할 때만 SetIgnoreLookInput(true)를 호출한다. 그 Controller를 LookLockController에 저장하고 OwnsLookLock=true로 한다. Release/Cleanup은 false/0으로 리셋하고, 소유한 잠금만 저장 Controller에 SetIgnoreLookInput(false)로 반환한다. invalid stored Controller도 ownership flag를 내린다. | Controller 속성이 나중에 바뀌거나 null이어도 저장한 대상에 반환해야 한다. 반복 Press가 counter를 늘리거나 반복 cleanup이 외부 잠금을 제거하면 실패다. 원본은 LookLockController 참조를 cleanup 때 null로 지우지 않으므로 이를 필수 후조건으로 새로 가정하지 않는다. |

근거: `Python/tools/node_tools.py:450–498,1018–1085,1692–1760`; `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintNodeCommands_Part1.cpp:2094–2235,3673–3879`, `Part2.cpp:1352–1444`. bridge의 활성 routing은 `Private/UnrealMCPBridge.cpp:931,940,973`이며 public→command→해당 handler 관계를 사용했다.

is_transient 차이는 원본 Blueprint command 파일들에서 `is_transient`, `CPF_Transient`, `SetVariableTransientFlag`를 찾아 확인했다. 현재 native는 hidden `variable_guid` 및 `is_editable`을 처리하지만 transient를 처리하지 않는다. 스크립트의 property_flags 동등성 비교는 요청한 transient 비트의 존재를 독립적으로 확인하지 않으므로 그것만으로 과거 지원을 추론하지 않는다.

## 실제 파이프라인 연결과 실패 처리

`build_cube_fluid_ninja_mouse_button_component.py`는 native ActorComponent 생성→Controller/MousePressed 선언→`cube_fluid_ninja_mouse_button_events.author` 호출→최종 compile/validate/save→graph 금지 참조·source/map SHA 보존을 검사한다. events helper는 timer/ownership/stored-controller 변수→중간 compile(save=false)→Pressed/Released/Cleanup custom event 세 개→bool/float pin 기본값→own 변수 get/set·Branch·IsValid·SetIgnoreLookInput→exec/data 연결 순서다. 이 흐름의 선언/graph 정확성과 소유권 의미를 유지하고 새 구조에서는 필요한 skeleton 갱신과 마지막 full compile/save를 구분한다.

원본 첫 실패 복구 `--resume-known-declarations`는 이전 9단계·정확 오류·빈 graph·GUID/선언/dirty 상태를 확인한 특수 재개다. 이것을 일반 자동 재시도로 복사하지 않는다. 새 journal의 알려진 단계와 실제 readback으로 현재 결과를 보고하고 결과 불명 수정은 재시도하지 않는다.

`build_cube_fluid_ninja_mouse_buttons_endplay.py`는 해시로 승인된 component 복사→없는 ReceiveEndPlay 생성→cleanup 호출 연결→compile/save→전체 topology와 원본 SHA 보존을 수행한다. `build_cube_fluid_ninja_mouse_event_host.py`는 MouseButtons SCS 추가→compile/readback→LeftMouseButton InputKey→각 Pressed/Released에서 Controller 저장 후 component custom event 호출→기존 capture 뒤 MousePressed 전달을 보호된 exec 교체로 끼워 넣는다. 마지막 `auto_receive_input=PLAYER0`은 원본의 별도 editor Python CDO 수정이며 InputKey 도구 자체 기능이 아니다. 자동 입력 수신 옵션까지 이식됐다고 간주하지 않는다.

`verify_cube_fluid_ninja_mouse_buttons.py`의 첫 검수는 PIE의 기존 PlayerController에서 direct custom event 호출로 duplicate Press 균형, 외부 잠금 보존, stored-controller 반환을 검사한다. 원본 코드가 존재한다는 사실과 과거 실행 성공은 구분한다. `endplay_code`는 PIE owner/component를 deferred 생성한 뒤 owner 파괴로 자동 EndPlay를 관측하도록 작성되어 있다. timer 경로는 직접 누적 호출·press/release reset이며 입력 하드웨어와 caller gating을 증명하지 않는다. report의 success만 재사용하지 않고 현재 UE 5.8 동작과 실질 후조건을 확인한다.

## 설치 UE 5.8.3 공개 API와 export 경계

| API | 확인한 근거 | 적용과 주의점 |
|---|---|---|
| `UBlueprintEventNodeSpawner::Create(NodeClass,CustomEventName)` / `Invoke` | `Editor/BlueprintGraph/Classes/BlueprintEventNodeSpawner.h:20,41–68`의 UE_API=BLUEPRINTGRAPH_API. cpp:160–235,245–262 | 무인자 custom event 후보. Invoke는 동명 function graph가 있으면 FunctionEntry를 반환하고 자동 ghost를 지울 수 있다. 이름/graph/기존 노드 사전 검사와 반환 exact class/name/GUID 및 기존 노드 보존 검수가 필요하다. |
| `UK2Node_CustomEvent::CreateFromFunction` / `SetDelegateSignature` | `K2Node_CustomEvent.h:35,93,119`의 MinimalAPI 클래스이지만 두 메서드는 BLUEPRINTGRAPH_API. cpp:460–505,507–538 | non-null signature가 있을 때만 CreateFromFunction이 생성한다. 무인자 생성을 위해 임의 signature를 만들 필요가 없다. SetDelegateSignature는 user pin 목록을 비우고 재구성하므로 기존 linked event에 무조건 적용하면 안 된다. delegate 지원은 별도 preflight/서명·링크 보존 검증 후 확대한다. |
| CustomEvent callable GUID | `BlueprintEditorUtils.h:1931`의 UNREALED_API `GetFunctionGuidFromClassByFieldName`; cpp:9759–9823, 특히9792–9814 | CustomFunctionName에 맞는 event의 **NodeGuid**가 함수 GUID다. FunctionGraph의 GraphGuid와 다르다. 기존 Cubeless FunctionApi의 ResolveOwnCompiledFunction은 FunctionGraphs/GraphGuid만 인정하므로 own/external custom-event 호출 분기를 추가해야 한다. compiled/skeleton signature·event node/소유 graph identity를 함께 검수하며 임의 이름으로 완화하지 않는다. |
| `UK2Node_InputKey` public fields / `AllocateDefaultPins` / `IsCompatibleWithGraph` | `K2Node_InputKey.h:25,40–69,86–91`에서 fields public, 함수 UE_API export. cpp:61–67,206–221 | Pressed/Released exec 출력과 Key FKey struct 출력. 직접 public node creator 경로 사용 가능 후보이며 새 도구의 타입/graph 제한은 명시한다. UE `Blueprint.cpp:1280–1283`의 SupportsInputEvents는 UObject ancestry 조건이므로 이 함수만으로 ActorComponent의 실제 입력 수신까지 보증하면 안 된다. |
| InputKey compile binding | `K2Node_InputKey.cpp:256–365`; `K2Node_InputKeyEvent.cpp:29–41`; runtime `Engine/InputKeyDelegateBinding.h:14–45` | 연결된 exec pin만 중간 input event로 확장하고 chord/event/flags/function-name을 runtime delegate binding에 기록한다. raw editor node flags와 compiled binding은 다른 검증 단계다. 후자는 공개 C++ read helper 후보이며 Python 배열 노출을 아직 가정하지 않는다. |
| `FBlueprintEditorUtils::SetVariableTransientFlag` | `Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h:1126` UNREALED_API; cpp:3867–3883 | NewVariables의 CPF_Transient를 설정/해제하고 structurally modified를 호출한다. 변수 존재를 먼저 확인해야 하며 implicit skeleton 작업을 결과에 남긴다. `KismetCompiler.cpp:1276`은 descriptor flags를 generated property에 복사한다. descriptor 비트와 compiled FProperty 비트를 구분해 readback한다. |
| `UActorComponent::ReceiveEndPlay` / `EndPlay` | `Runtime/Engine/Classes/Components/ActorComponent.h:945–959` ENGINE_API 및 BlueprintImplementableEvent. cpp:1664–1687,2153–2166 | EndPlay는 bHasBegunPlay 전제를 검사한다. DestroyComponent도 이 값이 true일 때만 EndPlay를 호출한다. idle editor Actor 제거만으로 자동 EndPlay를 증명할 수 없다. enum pin 형식은 정확 `/Script/Engine.EEndPlayReason`을 소스/실제 반환으로 확인한다. |
| `AController::SetIgnoreLookInput` / `IsLookInputIgnored` | `Controller.h:374–383` UFUNCTION(BlueprintCallable)+ENGINE_API. cpp:190–202 | true는 counter+1, false는 -1 후 하한0이다. `ResetIgnoreLookInput`은 전체 counter를0으로 만들므로 타 소유자의 잠금을 지운다. 이 파이프라인에서 사용하지 않는다. |

따라서 위 최소 기능의 장애를 단순히 MinimalAPI 비export 때문이라고 분류하지 않는다. 공개 exported entry point와 현 구현의 안전한 범위 확장이 먼저다. 게임 코드나 엔진 소스를 변경하지 않고 에디터 플러그인 내부 wrapper를 사용한다. 직접 호출할 virtual override에 export가 없는 경우 exact C++ 호출 표현의 링크 여부까지 빌드로 확인하며 공개 API 존재와 실제 빌드 성공을 혼동하지 않는다.

native 담당은 무인자 custom event에 exact `FGraphNodeCreator<UK2Node_CustomEvent>`도 후보로 확인했다. public CustomFunctionName과 constructor(`K2Node_CustomEvent.cpp:167–177`)의 bOverrideFunction=false, BlueprintCallable/Event/Public flags를 사용하면 spawner의 이름 충돌·ghost 제거 부작용을 피할 수 있다. 최종 creator 선택은 plugin 구현 담당이 공개 호출의 빌드·정확 후조건으로 결정한다. 이 조사에서는 어느 경로도 실행하지 않았다.

## 권장 구현 순서와 대표 검증

1. **CustomEvent와 transient 선언**: 기존 add-variable에 호환 가능한 optional transient 옵션을 추가하고 descriptor/generated flags 관측을 지원한다. 먼저 무인자 Press/Release/Cleanup event와 call_in_editor=false, 동일 call-node 코어의 event NodeGuid 기반 own/external 호출 해석을 함께 구현한다. event를 만드는 것만으로 cleanup 호출까지 지원됐다고 하지 않는다. source의 ignored transient 요청을 명시적으로 교정한다. signature/delegate/RPC/복잡 graph 옵션은 아직 미완료다.
2. **소유권 파이프라인**: AA의 외부 object getter/setter·native instance call·bool pin 기본값·IsValid를 재사용해 원본 button event helper를 작성한다. 중복 Press→Release, 외부 잠금1개+자체 잠금1개→Controller 변경/null→Cleanup→반복 Cleanup의 대표 흐름으로 counter 균형을 검사한다. ResetIgnoreLookInput은 호출하지 않는다.
3. **ActorComponent EndPlay**: 기존 named event의 parent/signature 범위를 좁게 확장하고 cleanup에 연결한다. idle no-MCP의 직접 ReceiveEndPlay 호출은 reflected 이벤트 동작 증거로만 기록한다. 자동 teardown은 실제 BeginPlay가 된 격리 game/PIE world에서 별도 주요 통합 검증한다. 사전 상태를 억지로 세팅하거나 idle component에 EndPlay C++를 직접 호출하지 않는다.
4. **InputKey host**: LeftMouseButton 한 key의 Pressed/Released→Controller 저장→component event를 작성한다. flags/두 exec 경로·compile binding·저장 경계로 대표 검수하고 실제 하드웨어, pause/우선순위/상속 binding/auto_receive_input은 별도 미검증이다. 모든 modifier 조합의 반복 fixture는 만들지 않는다.

검증은 fresh `/Game/CubelessMCPTests/...`와 MCP 없는 정확한 `UE58RuntimeProof`에서 수행하는 안이다. 기존 source SHA/CDO·두 SCS identity·소유 Actor cleanup·dirty/config/selection 보존을 재사용한다. 저장 재로드는 이 묶음의 의미 있는 경계에 한 번 두며 기능마다 패키징을 반복하지 않는다. no-MCP 검수기를 범용 MCP Python 실행 도구로 대체하지 않는다.

**Controller 생성은 아직 실행으로 확인할 경계다.** AController는 abstract/notplaceable(`Controller.h:39`), APlayerController는 concrete(`PlayerController.h:261`)다. 기존 editor placement 경로는 `LevelEditorViewport.cpp:321`에서 NotPlaceable/Abstract를 거부하므로 성공을 가정하지 않는다. 공개 `GameplayStatics::BeginDeferredActorSpawnFromClass`/`FinishSpawningActor`는 header:66–71에 ENGINE_API+UFUNCTION이지만 BlueprintInternalUseOnly이며, cpp:856–899는 전달 world에서 SpawnActorDeferred/FinishSpawning을 수행한다. 기존 원본과 같은 Python `call_method`의 별도 검수 경로 후보이지, Cubeless의 일반 function-call 허용 범위를 넓힐 근거는 아니다.

최소 대안은 격리 idle world에서 검수 전용 PlayerController를 생성·반환 identity로 소유하고 direct custom event의 counter만 관측하는 것이다. `PlayerController.cpp:1069–1090`에는 PlayerState·CameraManager/HUD 생성 가능성이 있으므로 부수 Actor 생성과 정리 결과를 관측해야 한다. 전체 actor 집합이 복원되지 않으면 성공 처리하지 않는다. 이 경로가 적합하지 않으면 원본처럼 별도 no-MCP PIE world에서만 수행한다. 실제 Controller를 무조건 `new_object`로 만들거나 사용자 PlayerController의 잠금을 건드리지 않는다. 첫에는 lock-free인 검수 소유 대상만 사용하고, 검수기가 추가한 외부 잠금1개도 정확히1회만 반환한다. 호출 결과가 불확실하면 Reset 또는 반복 false 호출로 수치를 추정 복구하지 않는다.

Transient 검증은 flags 저장·compiled readback과 runtime instance 값 동작을 분리한다. CDO가0/null이라는 사실만으로 serialization의 transient 제외 동작을 증명하지 않는다. generated property flag의 no-MCP Python 직접 노출은 아직 확인하지 않았으므로, 초기에는 공식 MCP의 좁은 C++ 관측과 다음 로드 경계의 readback으로 기록하고 serializer 전체 동등성을 주장하지 않는다.

현재 남은 결정은 Controller 생성 방식의 좁은 실증, EndPlay의 직접 호출과 자동 teardown을 어느 통합 경계에 묶을지, CustomEvent delegate 선택 옵션의 첫 지원 범위다. AA 실행 완료 후 root가 구현 묶음을 선택하며 이 문서는 승인되지 않은 엔진 작업을 실행하지 않는다.

## 후속 확인: no-MCP PlayerController의 Python 진입점

자동 생성된 `unreal.GameplayStatics.begin_deferred_actor_spawn_from_class` 같은 method를 가정하면 안 된다. 설치 `Engine/Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyGenUtil.cpp:1615–1624,1817–1819`는 BlueprintInternalUseOnly 함수를 자동 Python export에서 제외한다. 반면 `PyWrapperObject.cpp:1051–1109`의 공개 Object.call_method는 이름으로 UFunction을 찾고 임시 glue를 생성하며 해당 metadata export 필터를 적용하지 않는다. 원본 검수와 같은 GameplayStatics CDO의 reflected call 후보는 다음과 같다. **아직 실행하지 않은 호출 설계다.**

```python
statics = unreal.get_default_object(unreal.GameplayStatics)
controller_class = unreal.load_class(None, '/Script/Engine.PlayerController')
controller = statics.call_method('BeginDeferredActorSpawnFromClass', (
    validated_editor_world, controller_class, unreal.Transform(),
    unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None,
    unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT), {})
# 반환 객체의 exact class/world/신규 identity를 기록한 후 한 번만 finish한다.
finished = statics.call_method('FinishSpawningActor', (
    controller, unreal.Transform(),
    unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT), {})
```

enum 선언은 `EngineTypes.h:4410`과 `Actor.h:86`의 UENUM(BlueprintType)이며 실제 enum binding은 변이 전 getattr으로 확인한다. 두 단계 각각 durable intent/반환 객체/최종 actor 집합을 기록하고 어느 단계가 불확실하면 생성·finish를 재시도하지 않는다. 두 함수에는 RF_Transient 요청 인자가 없으므로 temporary lifetime과 RF_Transient를 혼동하지 않는다. 테스트 map을 저장하거나 dirty flag를 지우지 않고 정확한 반환 Controller를 한 번 제거한다.

`Actor.cpp:4435–4501`은 AreActorsInitialized일 때만 PostInitializeComponents를 호출하므로 idle editor에서 PlayerState/CameraManager가 반드시 생성된다고도, 절대 생성되지 않는다고도 가정하지 않는다. world actor 집합·Controller와 관련 객체를 관측하고 예상 밖 소유 관계가 생기면 불명 객체를 추정 삭제하지 않는다. 필요시 fresh disposable process의 실패 상태를 보존한 뒤 다음 실험을 분리한다. 게임 C++ runtime helper·추가 Cubeless MCP executor는 필요하다고 결론 내리지 않았다.
