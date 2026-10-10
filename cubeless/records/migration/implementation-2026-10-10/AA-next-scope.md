# AA: 원본 마우스 호스트의 상태 전달·초기화 확대안

2026-10-11. **소스 조사/제안만 완료**. 이 문서는 구현·등록·호출·저장·런타임 검증 증거가 아니다. 원본과 엔진은 읽기만 했고, Z의 실제 부모 Tick 호출 검증은 별도 진행 중이다. `migration-ledger.json`의 기존 getter/setter 부분 구현 및 pin-default/InputKey/CustomEvent 미완료 행을 이어간다. 전체 재조사나 원장 완료 판정은 하지 않았다.

## 권고

다음 단위는 **기존 명명 도구의 외부 멤버 접근 + 핀 리터럴 + 객체 상태 전달**이다. 별도 `reset_mouse`/`set_controller` 같은 중복 도구를 만들지 않는다. 기존 `add_blueprint_variable_get_node`/`add_blueprint_variable_set_node`에 원본 `target_class` 범위를 복구하고, 원본 `set_blueprint_pin_default`를 별도 계약 그대로 이식한다. 이어 기존 함수 호출 코어의 객체 핀/Engine 인스턴스 호출을 넓혀 같은 원본 작업 흐름을 완성한다.

외부 bool setter만 추가하면 아래 원본 흐름의 일부만 된다. 반대로 InputKey·렌더링·전체 FluidNinja 호스트까지 한 번에 묶으면 검증 경계가 흐려진다. 상태 전달/초기화를 첫 경계로, 클릭 이벤트/정리 호출을 다음 경계로 이어가는 것이 적절하다.

## 원본 호출 순서와 보존해야 할 의미

이 절의 파일은 모두 `C:/Git/UnrealMCP5_6_1/Python/scripts/analysis/` 아래다.

| 원본 | 실제 순서와 입출력 | 현재 추가로 필요한 부분 |
|---|---|---|
| `build_cube_fluid_ninja_mouse_host.py:16–48` | MouseCapture 컴포넌트 추가 → 중간 compile → 부모 Tick 확인 → 외부 `SampleValid=False`, `MousePressed=False` setter 2개 → Receiver 유효성/`CallbackCount>0` → `GetPlayerController(0)` 및 Receiver.`WorldPosition` → 외부 Capture.`Controller`/`SimulationPosition` 설정 → `CFNL_CaptureMouse` | 외부 bool/int/Vector/object 멤버 접근, setter 입력 핀 기본값, 객체 인수/반환 함수(`IsValid`, `GetPlayerController`). reset은 gate 앞이라 무효 sample도 이전 상태가 남지 않아야 함. |
| `build_cube_fluid_ninja_mouse_event_host.py:14–42` | MouseButtons 추가/compile → LeftMouseButton Pressed/Released 각각 → `Buttons.Controller=GetPlayerController(0)` → 외부 `CFNL_MousePressed`/`CFNL_MouseReleased` → 기존 Capture 호출 뒤에 `Capture.MousePressed=Buttons.MousePressed` setter 삽입 | 외부 getter/setter와 객체 전달, InputKey, 클릭 함수의 본체. 삽입은 이미 구현한 guarded exec replacement 재사용. 이 파일의 `auto_receive_input=PLAYER0` CDO 설정도 독립 남은 항목. |
| `cube_fluid_ninja_mouse_button_events.py:10–59` | `TimeSinceLastClick=0`, `OwnsLookLock=False`, `LookLockController:Controller` 선언 → Pressed/Released/Cleanup custom event 각각에서 `MousePressed` 및 타이머 reset → 소유 상태 Branch → Controller 유효성 → `SetIgnoreLookInput(pressed)` → press에서 Controller 기억 → 소유 bool 갱신 | own object 멤버 getter/setter, bool/float 핀 기본값, CustomEvent, `IsValid` 객체 핀, Engine **인스턴스** 함수 호출. release/cleanup은 현재 Controller가 아니라 저장한 `LookLockController` 사용. |
| `build_cube_fluid_ninja_mouse_buttons_endplay.py:16–25` | ActorComponent `ReceiveEndPlay` 이벤트 → own `CFNL_MouseCleanup` 호출 | Z Actor 이벤트 지원만으로 대체되지 않음. ActorComponent event ancestry + EndPlay enum 인수 및 custom-event 생성/호출 계약 필요. |
| `build_cube_fluid_ninja_mouse_timer_host.py:11–33` | 부모 Tick 뒤 Sequence 삽입 → 기존 체인 보존 → inherited gate 3개 → MouseButtons getter → 외부 `CFNL_AdvanceMouseTime(DeltaSeconds)` | Y/Z/X 기반을 재사용한다. 새 external setter가 이 타이머 축적을 대체하는 것은 아님. |

`build_cube_fluid_ninja_mouse_button_component.py:87–90`의 Controller/MousePressed 선언과 `cube_fluid_ninja_mouse_button_events.py:10–16`은 `is_transient=True`도 요청한다. 해당 숨은 변수 옵션의 실제 이전 동등성은 별도로 확인해야 하며, AA에서 무시하고 원본 컴포넌트 전체를 완료로 표시하면 안 된다.

중요한 실패/검수 경험도 남긴다. 원본 button builder `:45–85`는 중간 compile 이전 own setter가 `Variable not found: MousePressed`로 실패한 뒤, 이미 생성된 선언/CustomEvent와 dirty 상태를 확인하고 제한된 수동 재개를 했다. 새 구현도 skeleton 준비 여부와 실제 부분 변경을 확인해야 하며 자동 재시도로 노드를 중복 생성하지 않는다. 원본 host builder `:63–84`는 단계별 응답/보호 SHA/마지막 compile-save와 `do_not_retry`를 기록한다. 이를 보관한 것 자체는 이식이 아니다.

## 공개 도구 → 활성 명령 → C++ 계약

원본 경로 기준은 `C:/Git/UnrealMCP5_6_1/`이다.

- `Python/tools/node_tools.py:5226–5307`: get/set 양쪽의 정확한 기존 인자는 `(blueprint_name, variable_name, node_position=None, target_class='', graph_name='', graph_id='', graph_type='', create_graph_if_missing=False)`. 위치 생략은 `[0,0]`; 빈 `target_class`는 전송하지 않는다. 외부 클래스의 MCP 의존성을 Python에서 먼저 거부한다. 별도 compile/save 옵션은 없다.
- `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/UnrealMCPBridge.cpp:936–937` → `Commands/UnrealMCPBlueprintNodeCommands_Part1.cpp:389–395` → `Part6.cpp:546–713`. 외부 분기는 class load → property lookup → exact VariableGet/Set 생성 → `VariableReference.SetFromField(Property,false)` → GUID/위치/핀 → MarkBlueprintAsModified다. own 분기는 기존 공통 생성기로 간다. 암묵적인 전체 compile/save는 이 핸들러에 없다.
- 위 `Part6.cpp:583–599,668–683`은 잘못된 클래스/MCP 참조/없는 멤버를 오류로 반환한다. Blueprint visibility/private/read-only 선행 검사는 보이지 않으므로 새 구현은 쓰기 전 명시 거부로 강화해야 한다. 원본 `Commands/UnrealMCPBlueprintNodeCommandsShared.h:1159–1187` 반환은 GUID `node_id`, name/class/title/x/y/comment/pins이고, 핸들러가 graph 정보를 추가한다. 현재 Cubeless 기존 member tool의 path `node_id`/별도 `node_guid` 계약도 깨지 않게 유지해야 한다.
- `Python/tools/node_tools.py:5743–5781` pin-default는 `(blueprint_name,node_id,pin_name,value:Any,direction='',graph_name='',graph_id='',graph_type='')`; native `Part6.cpp:1241–1385`는 GUID/방향으로 핀을 찾고 타입별 문자/객체/클래스 값을 적용한다. **요청값 적용 여부**와 현재 기본값 유효성을 확인하며 실패 때 기존 세 default 필드를 복원한다. 성공은 `{node_id,pin,graph}`이고 compile/save는 없다. 새 구조에서는 명시 JSON adapter로 Any를 표현하고 원자적 롤백은 주장하지 않는다.
- 뒤이어 복구할 공개 명칭: InputKey `node_tools.py:450–498`(modifier 4개 False, consume True, paused False, override-parent False), CustomEvent `:1692–1758`(signature source/alias/call_in_editor 및 graph selectors). Mouse event host는 consume False/override True를 명시한다. 단순 이름/기본값만 지원해 이 전체 계약을 완료 처리하지 않는다.

## UE 5.8 설치 소스에서 확인한 재사용 경로

아래 경로는 `C:/Program Files/Epic Games/UE_5.8/Engine/` 기준이다. 문서 추정이 아니라 설치 파일을 읽었다. 실제 AA 실행은 아직 하지 않았다.

1. `Source/Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary/BlueprintGraphEditor.h:290–303`에 reflected `AddGetMemberVariableNode(MemberName,ClassPath)`/`AddSetMemberVariableNode(MemberName,ClassPath)`가 있다. cpp `:872–950`은 ClassPath가 있으면 해당 클래스를 resolve하고 property 기반 `UBlueprintVariableNodeSpawner::CreateFromMemberOrParam`/Invoke를 쓴다. 현재 Python compatibility의 `add_member`가 같은 함수를 빈 ClassPath로 이미 사용한다. **생성기는 재사용하고 native read-plan과 사전/후검수만 확장**하는 것이 우선이다.
2. 같은 cpp `:1755–1763`의 class resolution은 `LoadSynchronous`다. 외부 클래스 load를 무변이라고 가정하지 말고 기존 Y saved-generated 검사, source SHA/dirty/status/정확한 current Generated/Skeleton 관측을 재사용한다. 이름/partial path/SKEL/REINST를 자동 대체하지 않는다. `:1675–1692`의 dirty/structural 마킹은 엔진에 맡기고 전체 compile/save와 구분한다.
3. `Source/Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:9151–9197`의 exported `IsPropertyPrivate`, `IsPropertyReadableInBlueprint`, `IsPropertyWritableInBlueprint`가 실제 public preflight다. property 존재 확인 후 visibility/read-only/private 여부를 검사한다. 초기 external 범위는 감사된 saved clean non-scene ActorComponent의 **own 선언 멤버**로 제한하고 상속/임의 native/임의 클래스는 확장 근거 없이 열지 않는다.
4. `Source/Editor/BlueprintGraph/Private/K2Node_VariableSet.cpp:99–117`은 execute/then, 변수 input, self, 변수 output 핀을 생성한다. `Classes/K2Node_VariableSet.h:30–62`는 MinimalAPI이므로 비exported 메서드 직접 링크를 가정하지 않는다. 기존 reflected spawner가 public 진입점이다. property identity, authoritative serialized owner, self object class, 실제 raw pin type, 기존 graph 보존을 검수한다.
5. `Source/Editor/BlueprintEditorLibrary/Public/BlueprintEditorLibrary/BlueprintGraphPin.h:67–71`의 reflected `SetPinValue`는 cpp `:74–87`에서 `TrySetDefaultValue(void)` 후 **현재 값 유효성만** 검사한다. 요청이 거부되어 이전 유효값이 남아도 true가 될 수 있고, invalid면 autogenerated default로 reset한다. 따라서 원본 pin-default의 요청값 검수와 동등하지 않다. 기존 `CubelessFunctionPinApi` typed 파서/검수와 public K2 schema를 재사용하는 작은 native 적용 경로가 적합하다. 초기 bool/float 리터럴을 우선하되 object/class defaults의 미지원은 명시한다.
6. `Source/Runtime/Engine/Classes/Kismet/GameplayStatics.h:184–192`의 GetPlayerController는 static BlueprintPure이지만 `WorldContextObject:UObject`, 반환 PlayerController, `UnsafeDuringActorConstruction` metadata가 있다. `Kismet/KismetSystemLibrary.h:128` IsValid도 UObject 인수다. 현재 `CubelessFunctionApi.cpp:451`의 primitive-only gate를 그냥 제거하면 안 된다. 객체 타입의 runtime ancestry/dependency, 방향/const/reference 의미, world-context 숨김/기본 self 처리를 함께 검수해야 한다.
7. `Source/Runtime/Engine/Classes/GameFramework/Controller.h:370–383` SetIgnoreLookInput은 BlueprintCallable **인스턴스** 함수이고 반복 true 호출이 누적된다. 원본 ownership Branch/저장 Controller/cleanup을 유지해야 한다. Tick마다 true를 호출하거나 ResetIgnoreLookInput으로 모든 소유자의 잠금을 해제하는 대체는 동등하지 않다.
8. 공식 `BlueprintGraphEditor.h:315–319`의 CustomEvent도 바로 동등대체할 수 없다. cpp `:1017–1064`는 먼저 노드를 만든 뒤 이름이 잘못되면 unique fallback 이름을 유지한다. 정확한 이름/충돌/no-partial-before-preflight 계약이 필요하다. 공개 `Source/Editor/BlueprintGraph/Classes/K2Node_InputKey.h:35–53`에는 실제 키/consume/paused/parent-binding 필드가 있으나 이번 조사는 InputKey 생성·컴파일 전체 계약을 아직 확정하지 않았다.

## 구현 순서와 완료 경계 제안

**AA 상태 전달/초기화 묶음**

- 기존 member get/set에 `target_class`를 additive로 연결한다. 기존 인자 위치/default/omitted fingerprint와 batch replay를 유지하고, 빈 값은 기존 own 분기를 그대로 사용한다. 외부 bool/float/Vector와 감사된 Engine object reference(Controller)를 단계적으로 같은 read-plan에 지원한다. own object Controller/LookLockController 접근도 같은 타입 검수의 대상이다.
- `set_blueprint_pin_default`라는 기존 이름을 독립 등록하고 동일 action을 batch로 연결한다. 우선 원본 reset에 필요한 bool/float, 이후 안전한 객체 null/default를 명시 확대한다. 멤버 CDO default setter로 대신하지 않는다.
- 기존 call-function named 도구에 GetPlayerController/IsValid의 객체 signature를 같은 core에서 확장한다. 별도 두 함수 전용 MCP 도구는 만들지 않는다. 임의 UObject 함수 개방 대신 실제 function/param metadata와 runtime 의존성을 검사한다.
- 원본 mouse_host/event_host에서 **Capture 상태 reset → 외부 상태 전달 → 기존 component 함수 호출 → Buttons 상태를 Capture로 전달**하는 부분을 하나의 격리 host로 검수한다. 초깃값이 실제로 바뀌는 런타임 후조건, 직접 객체 동일성, 기존 graph/source 보존과 마지막 compile/save 한 번을 확인한다. 불확실한 실패는 같은 state journal/action 관례로 정지한다. 이 검수는 입력 장치 처리 성공을 뜻하지 않는다.

**바로 이어갈 클릭/정리 묶음**

- 원본 세 CustomEvent(Pressed/Released/Cleanup)와 정확한 ownership 상태 전이를 만든다. Engine Controller 인스턴스 call을 확장하고 press/release의 누적 잠금 균형을 확인한다.
- LeftMouseButton InputKey 두 event, host의 input 수신 설정, ActorComponent EndPlay(enum 인수)→cleanup을 연결한다. 생성/compile과 실제 입력 dispatch/EndPlay 실행은 별도 증거다.

**미완료로 남길 범위**: 임의 native/상속 멤버 쓰기, 모든 변수 타입/컨테이너/델리게이트, 모든 pin-default 객체/클래스값, CustomEvent delegate signature/replication 옵션, 전체 mouse capture의 screen/deprojection/vector 연산, GPU merge/painter/touch, 하드웨어 입력 및 전체 FluidNinja 호스트/패키징. 원본 `verify_cube_fluid_ninja_mouse_event_host.py:1,57–95`도 component event 주입/커서 probe를 hardware input 증거와 구분한다. AA에서 이 구분을 유지한다.

이 계획을 구현할 때 도구 수보다 원본 호출 체인의 빠진 단계가 줄었는지로 진행률을 판단한다. 소스 복사, 이름 등록, 단일 호출 성공만으로 어떤 원본 pipeline도 완료 처리하지 않는다.

## AA 구현 후보의 확정 경계 (빌드/실행 전)

2026-10-11 C++ 후보를 작성하고 root 빌드용으로 고정했다. 이 단락은 소스 구현 상태이며 앞 절의 실제 호출/저장/행동 증거를 대신하지 않는다.

- `CubelessRuntimeObjectApi`는 UObject/Actor/Controller/PlayerController/ActorComponent의 **exact native UClass**만 공통 검사한다. 에셋 로드/CDO 생성은 하지 않는다. 기존 `add_blueprint_variable` Object descriptor는 이 검사기를 쓰며 `type_object='/Script/Engine.Actor'` 또는 `'/Script/Engine.Controller'`와 미지정 기본값으로 null 참조를 선언한다. 다른 object 선언은 `variable_object_type_pending`으로 명시 거부한다. 기존 primitive/struct/class/enum 경로는 유지한다. Object 배열 선언과 graph의 scalar object 지원은 별도 범위다.
- 기존 own member plan은 이 native object 타입의 scalar getter/setter를 `own_runtime_object`로 관측한다. 새 숨김 `CubelessGraphLibrary.read_external_member_variable_plan(graph_path,variable_name,setter,target_class_path)`는 로드가 끝난 exact saved/clean/compiled non-scene ActorComponent의 **own 선언**만 받는다. Boolean/float/double/Vector와 위 object 타입을 지원하고, 선언 GUID 및 compiled/skeleton 타입 일치, public readable/writable/private 검사를 수행한다. 기존 공식 reflected spawner가 실제 노드를 만든다. source 로드 및 디스크/dirty/status 보존은 Python의 기존 Y 관측 절차와 결합한다.
- `ReadNodeIdentities`에 `variable_member_parent_path`와 `variable_member_guid`를 추가했다. 외부 plan은 `property_path`(현재 skeleton), `compiled_property_path`, `expected_pin_type_text`, `expected_self_pin_type_text`, `external_source_validation`, `external_source_before/after`를 분리한다. `K2Node_Variable.cpp:144–205`는 external self type을 authoritative property owner로 만들며 `:537–545`가 skeleton property 조회의 근거다. `BlueprintVariableNodeSpawner.cpp:98–108`의 own/inherited self 판정 때문에 외부 plan에서 그 문맥은 거부하고 기존 own 경로로 분리한다.
- 기존 함수 호출 API는 정확히 `GameplayStatics.GetPlayerController`, `KismetSystemLibrary.IsValid`, `ActorComponent.GetOwner`에 공통 audited object signature를 더했다. Engine 인스턴스 호출의 이번 추가는 GetOwner뿐이다. 나머지 기존 primitive Engine static/own generated/external component 호출은 보존한다. `ActorComponent.h:536–538` public BlueprintCallable const GetOwner, `GameplayStatics.h:191–192` WorldContext/UnsafeDuringActorConstruction, `KismetSystemLibrary.h:126–128` IsValid를 근거로 한다. GetPlayerController는 intrinsic GetWorld를 제공하는 nonstatic graph만 받으며 construction graph와 world-context literal override는 거부한다. `BlueprintEditorUtils.cpp:10340–10346`의 native-parent GetWorld 검사, `K2Node_CallFunction.cpp:1184–1257`의 숨김 핀 생성 규칙을 사용한다. 플레이어가 없는 에디터의 null Controller는 실패/nonnull 성공으로 바꾸지 않는다.
- 새 숨김 `CubelessGraphLibrary.set_blueprint_pin_default(graph_path,node_path,pin_name,value_json,direction)`는 기존 `Function::Pin::ParseJson/DefaultText`와 Function 결과 envelope를 재사용한다. 정확한 standard VariableSet/CallFunction/Branch의 visible/unlinked/non-orphan input을 지원한다. scalar primitive와 audited object의 JSON null을 받으며 output/class/concrete object asset defaults, 숨김/self/ignored 입력 및 다른 노드 콜백은 미완료다. 명시 input selector 없는 동명 양방향 핀은 모호성 오류로 처리한다.
- `EdGraphSchema_K2.cpp:4872–4905`의 public TrySetDefaultValue는 이미 PinDefaultValueChanged를 한 번 호출한다. 원본의 추가 수동 콜백은 반복하지 않는다. typed literal/schema preflight → 한번 적용 → 요청값/다른 핀/graph identities/dirty 재관측을 수행하고, Python이 전체 graph delta를 별도로 확인한다. 실제 실패 상태를 남기며 복원·재시도·원자적 롤백을 주장하지 않는다. 동일 기본값은 no-op이다. UE가 real 핀의 과거 single-precision serialization flag를 끄는 동작은 `legacy_float_serialization_flag_cleared`로 명시한다.

결과의 compile/save는 요청/명시 호출이 모두 false다. 이 후보는 게임 C++/엔진 소스를 바꾸지 않으며 게임 에셋에 Cubeless 타입이나 함수 참조를 만들지 않는다. 실제 AA source protection, subclass object direct links, author compile/save, no-MCP 상태 전달/초기화의 실행 증거는 root 검증 후 별도로 기록한다.
