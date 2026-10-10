# V: 소유한 타이머 BP의 에디터 내부 실행 검수

설치 UE 5.8.3 소스를 읽어 작성한 API 근거와 검수 스크립트다. 이 기록만으로 실제 실행 통과를 주장하지 않는다. U 플러그인 C++ 소스는 변경하지 않았다.

원본 `C:/Git/UnrealMCP5_6_1/Python/scripts/analysis/verify_cube_fluid_ninja_mouse_buttons.py:114–130`의 `timer_code`는 임시 ActorComponent를 만든 뒤 `CFNL_AdvanceMouseTime`에 0.25, 0.5, 1.25를 순서대로 전달해 2초를 확인한다. 이어 MousePressed 초기화, 0.125초 증가, MouseReleased 초기화까지 검사한다. timer의 `guard`는 `build_cube_fluid_ninja_t01_no_offset_host.py:15–22`에서 **PIE가 아님**, 지정 PID·맵·dirty 없음 등을 확인한다. 파일 맨 위 일반 mouse button 검수의 PIE 조건과 이 timer 조건은 다르다.

이번 `cubeless/tools/verification/verify_function_timer_runtime.py`는 세 누적 호출만 이식한다. press/release, 입력 하드웨어, caller gating, Tick 연결, 자동 EndPlay, 패키징 및 Shipping 실행은 미검증이다. 월드 없이 실행할 수 있다는 판단은 이 함수가 인스턴스의 숫자 멤버만 읽고 쓰는 그래프라는 근거에 한정하며, 실제 실행 결과로 확인한다.

## 공개 API 근거

이하 경로는 `C:/Program Files/Epic Games/UE_5.8/Engine/` 기준이다.

- `Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyCore.cpp:1370–1425`: 공개 `unreal.new_object(type, outer=None, name="", base_type=None)`의 기본 outer는 `GetTransientPackage()`다. None이 이 기본값을 nullptr로 덮어쓰지 않도록 명시 처리한다. `base_type=unreal.ActorComponent`는 생성 전에 실제 class의 `IsChildOf` 검사로 이어진다.
- 같은 플러그인 `Private/PyUtil.cpp:779–871`: outer/ClassWithin, base class, abstract 여부를 검사하고 `NewObject<UObject>(outer,class,name,RF_Transactional)`을 호출한다. `/Engine/Transient` outer와 명시 `RF_Transient` 플래그는 같지 않다. 스크립트/보고서는 후자를 주장하지 않는다.
- `Private/PyWrapperObject.cpp:1051–1110`: 공개 `call_method(name, args_tuple, kwargs_dict)`는 정확한 reflected UFunction을 찾아 임시 Python glue를 만들고 `CallFunction`을 실행한다. Python에 전용 glue가 없는 Blueprint 함수에도 적용된다. 함수가 없거나 인자가 맞지 않으면 예외가 발생한다. 함수 이름은 고정 `CFNL_AdvanceMouseTime`이며 임의 실행 MCP 도구를 새로 만들지 않는다.
- `Private/PyUtil.cpp:606–648`: `InvokeFunctionCall`은 게임 스레드를 검사하고 `FScopedScriptExceptionHandler`, `FEditorScriptExecutionGuard`를 설치한 뒤 `ProcessEvent`를 호출한다. 따라서 이 Python 경로에 별도 C++ 가드나 `CallInEditor` 메타데이터를 덧붙일 필요가 없다. 임의 worker thread에서 호출하지 않는다.
- `Source/Runtime/CoreUObject/Private/UObject/ScriptCore.cpp:450–465`: 해당 RAII 가드는 기존 `GAllowActorScriptExecutionInEditor` 값을 저장하고 잠시 true로 바꾼 뒤 복원한다. 에디터에서는 runaway 상태도 초기화한다. 엔진 설정을 영구 변경하는 코드가 아니다.
- 같은 파일 `:2055–2103`: `ProcessEvent`는 unreachable/PostLoad 중 호출을 금지하고, 디버거 정지 또는 비어 있는 script body에서는 실행을 생략할 수 있다. 따라서 Python 호출이 반환됐다는 사실만으로 통과시키지 않고 매 단계 0.25→0.75→2.0의 실제 인스턴스 값과 CDO 불변을 검사한다.
- `Source/Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary.cpp:1290–1303`: 공개 ScriptMethod `bp.generated_class()`는 이미 존재하는 generated class의 authoritative class를 반환한다. 없으면 경고/None이며 자체 compile 호출은 없다. `get_blueprint_parent_class`와 함께 정확한 native `/Script/Engine.ActorComponent` parent 및 `<asset>_C` 경로를 확인한다.
- `Source/Runtime/Engine/Private/Components/ActorComponent.cpp:581–599`: `PostInitProperties`는 에디터 component element/storage 등록을 수행할 수 있다. owner가 있으면 owner component 목록도 다루지만 이번 객체는 transient package outer와 owner=None을 실제 확인한다. 에디터 내부 임시 bookkeeping까지 전혀 발생하지 않는다고 주장하지 않는다.
- `Private/PyReferenceCollector.cpp:30–53`: 살아 있는 Python wrapper는 UE reference collector에 포함된다. 스크립트는 생성 객체 참조를 모듈 목록에 유지하고, 별도 전역 GC/Actor 파괴/Component destroy 호출 없이 소유한 임시 에디터 프로세스 종료로 정리한다. `ActorComponent.cpp:2153–2241`의 destroy 경로는 EndPlay/Uninitialize/Unregister/OnComponentDestroyed 등의 부수효과가 있으므로 검수에 불필요하게 추가하지 않는다.

## 실행 범위와 기록

기본 대상은 MCP 및 Cubeless가 없는 `cubeless/workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject`다. 이 정확한 프로젝트만 허용하고 `/Game/CubelessMCPTests/<folder>/<asset>` 아래의 지정 저장 파일 SHA256을 load 전에 비교한다. enabled plugin 목록에서도 MCP/Toolset이 없음을 전후 확인한다. 원본 UE5.6 프로젝트, 사용자 레벨 또는 실제 작업 에디터에서는 실행하지 않는다.

입력은 `--project-file`, `--package`, `--expected-asset-sha256`, `--report`이며, 알려진 PID라면 `--expected-pid`를 추가한다. 새 `-ExecutePythonScript` 프로세스의 PID는 실행 전 알 수 없으므로 이를 생략할 때 root 실행기가 새 프로세스 PID와 결과 `process_id`를 대조해야 한다. 스크립트는 프로세스를 시작하거나 종료하지 않는다.

로드 전 dirty가 없어야 하며, 로드 이후 저장 BP status가 UpToDate/UpToDateWithWarnings이고 정확한 generated CDO의 타이머 기본값이 0이어야 한다. 생성된 객체는 CDO와 다른 객체이고 실제 outer/outermost가 `/Engine/Transient`, owner가 None이어야 한다. 명시 component registration 및 월드/Actor 생성 호출은 없다. `IsRegistered()` 자체는 이 Python 검수에서 별도 반사 관측한 것으로 주장하지 않는다.

세 호출 각각의 durable intent, attempted/returned/verified 횟수, 인자, 호출 전후 인스턴스 값, CDO 값을 기록한다. 실패하면 다음 호출을 중단하며 자동 재시도/원자적 rollback은 없다. exception 뒤에도 가능한 최종 인스턴스·CDO·status·dirty·파일 SHA·plugin 목록을 관측한다. 보고서 쓰기는 기존 `create_component_read_fixture.write_report`의 fsync/atomic replace를 재사용한다.

명시 compile/save 호출은 0회다. asset/class load 중 엔진의 암묵적 PostLoad/compile 가능성은 측정하지 않았다고 기록하며, 이를 무조건 0이라고 주장하지 않는다. 최종 통과에는 타이머 합 2.0, 세 검증 성공, CDO 기본값·BP status·타깃 파일 SHA·dirty 목록 불변 및 MCP/Toolset 비활성이 모두 필요하다. 이 결과가 모든 가능한 함수 부수효과나 cooked/runtime 독립성 전체를 증명하지는 않는다.
