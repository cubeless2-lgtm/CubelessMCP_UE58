# T: 변수 노드·typed 연결과 후속 타이머 함수

이 문서는 원본과 설치 엔진 계약 및 T 대표 실행을 구분한다. S/T 통합 ST2 빌드·공식 8 Toolset/48도구 등록과 T 파이프라인 실행을 통과했다. S 변수 상태 pipeline과 별도 fresh BP를 사용했으며 실제 Codex 연결의 새 프로세스 재조회도 통과했다.

현재 root가 정한 T 대표 범위는 **float member getter/setter, bool getter→Branch.Condition, ReceiveBeginPlay→setter→Branch의 exec 연결**, GUID 선택자와 직접 데이터 연결이다. 형변환·함수 graph 생성·함수 입력·일반 call node는 다음 묶음으로 남긴다. 원본 mouse timer 전체의 완료로 계수하지 않는다. 이벤트는 실제 member identity와 기본 factory 그래프를 확인해 중복을 만들지 않는 계약이 필요하며 이름·UI 제목 유사성만으로 선택하지 않는다.

## 원본 timer 계약

`Python/scripts/analysis/build_cube_fluid_ninja_mouse_timer.py`(37줄, SHA256 `696ea2d51014152d55cde30a3f7e7720ad4b230a1153ea6e9be6e8036319ae29`)는 기존 ActorComponent BP 복사본에 `CFNL_AdvanceMouseTime` 함수를 추가한다. 호출자가 실행 여부를 결정하며 무조건 Tick을 설치하지 않는다. 선행 `cube_fluid_ninja_mouse_button_events.py:10`은 `TimeSinceLastClick`, type=float, default=0, is_exposed=false를 선언한다. raw is_transient=true도 보내지만 원본 variable handler에서는 이 옵션을 읽지 않으므로 실제 transient 지원으로 주장하지 않는다.

모든 아래 호출에 blueprint_name=복사본, graph_name=CFNL_AdvanceMouseTime, graph_type=function을 전달한다. node_id는 응답에서 받아 다음 요청에 연결하고 상수로 추측하지 않는다.

| 순서·원본 API | 실제 입력 | 사용하는 출력·검수 |
| --- | --- | --- |
| 1 resolve_blueprint_graph | create_if_missing=true | native resolved/created와 graph metadata; 원본 author는 응답 성공만 소비 |
| 2 add_blueprint_function_parameter | parameter_name=DeltaSeconds, parameter_type=float, direction=input | native entry node metadata/parameter_name/direction/default_value/pin_type; 다음 topology로 entry 재조회 |
| 3 inspect_blueprint_graph_call_topology | include_pins=true, include_links=true, max_nodes/links=-1 | graphs[0], 잘림 없음, nodes=1, K2Node_FunctionEntry, links=[]; entry.node_id 확보 |
| 4 add_blueprint_variable_get_node | variable_name=TimeSinceLastClick | node_id와 변수 출력 pin TimeSinceLastClick |
| 5 add_blueprint_call_function_node | function_class=/Script/Engine.KismetMathLibrary, function_name=Add_DoubleDouble, param_defaults={} | node_id, pins, function identity/is_static/is_pure/is_latent/적용 defaults |
| 6 add_blueprint_variable_set_node | variable_name=TimeSinceLastClick | node_id와 변수 입력 pin, execute 입력 |
| 7–10 connect_blueprint_graph_pins | 아래 네 edge, compile=false, save_asset=false | schema_response, already_connected, 양방향 link readback, compile/save 상태 |

원본의 네 edge는 getter.TimeSinceLastClick→add.A, entry.DeltaSeconds→add.B, add.ReturnValue→setter.TimeSinceLastClick, entry.then→setter.execute다. 추가 return node나 함수 output을 만들지 않는다. author 반환은 function, caller_gating_required=true, unconditional_tick=false이며 런타임 실행 결과가 아니다.

## 타입과 원본 실패 경험

설치 UE58 `Runtime/Engine/Classes/Kismet/KismetMathLibrary.h:547`의 Add_DoubleDouble은 double 입력/출력이다. 원본 member와 DeltaSeconds는 float이므로 width 차이가 있다. 다만 `Editor/BlueprintGraph/Private/EdGraphSchema_K2.cpp:4502–4508`은 real(float/double)끼리 호환이며 필요한 경우 bytecode 암묵 변환한다고 명시하고, :4661도 real 동등성을 인정한다. 향후 timer 묶음에서는 실제 schema response·원래 pin 타입을 기록해야 한다. 별도 conversion node가 반드시 생긴다고 가정하거나 원본 타입을 조용히 double로 바꾸면 안 된다. 현재 T의 직접 연결 대표는 이 width 차이를 다루지 않는다.

원본 `build_cube_fluid_ninja_mouse_button_component.py:41–80`은 변수 선언 뒤 setter가 `Variable not found: MousePressed`로 실패한 과거 보고서를 근거로 제한적 복구한다. 선언·기존 노드/GUID·dirty package를 대조한 뒤 compile 경계를 추가해 생성 클래스 속성을 사용할 수 있게 한다. 따라서 새 fixture에서도 선언만 존재하는 상태와 생성 클래스가 해당 속성을 가진 상태를 구분해야 한다. 현 native 구현이 skeleton의 공개 API로 충분함을 입증하면 불필요한 full compile을 피하고, 그렇지 않으면 필요한 경계를 명시한다. 과거 보고서에 적힌 resume 권한을 이번 실행 승인으로 취급하지 않는다.

## 현재 재사용과 부족한 기능

- Blueprint 생성, S add/inspect/default 상태 코어, 기존 compile/save/journal/batch는 새 graph 작업의 공통 기반이다. S 자체 실행 수준은 별도 S 기록에 따른다.
- 현재 `CubelessBlueprintGraphTools.inspect_graph`는 제한된 graph node/pin readback을 제공한다. 원본 topology의 함수/변수 reference, graph GUID/type selector, 잘림/제한 계약과 동등하다고 확인하지 않았다.
- 기존 add_branch_node는 Branch 작성에 재사용한다. T 이전 connect_graph_pins는 exec-to-exec만 허용했다. T는 기존 이름·배치 계약을 유지하며 엔진 schema의 직접 MAKE 연결로 확대했고 float/bool/exec와 GUID 선택 대표를 실제 검증했다. conversion/promotion 노드와 기존 링크 교체는 제외한다.
- 로컬 getter/setter, 실제 factory 이벤트 member 선택, GUID 선택자는 T 대표에서 검증했다. 이벤트를 새로 작성한 증거는 아니다. 원본 getter/setter의 target_class 외부 객체 옵션과 모든 변수 타입까지 한 대표로 완료 처리하지 않는다.
- 다음 timer 묶음에는 resolve_blueprint_graph, add_blueprint_function_parameter, 검증된 add_blueprint_call_function_node, graph GUID/type 조회와 typed 연결이 추가로 필요하다. 공식 범용 실행 도구로 대체했다고 처리하지 않는다.

## 출처 연결과 검수·보고서

공개 source는 `Python/tools/node_tools.py`의 call-function(:565), typed connect(:791), parameter(:1445), resolve(:2259), topology(:2330), get/set(:5226/:5268)다. active native는 `UnrealMCPBlueprintNodeCommands_Part1.cpp` connect(:1229)/validated call(:1966), Part2 parameter(:266)/resolve(:1566), Part3 topology(:983), Part6 get/set(:546/:631)로 연결된다. `add_blueprint_function_node`의 오래된 target 해석 경로와 `add_blueprint_call_function_node`의 검증된 function_class 경로는 서로 다른 공개 기능이다. 이름이 비슷하다고 삭제하거나 한쪽만 복구하고 양쪽 완료라고 하지 않는다.

원본 timer의 main은 `build_cube_fluid_ninja_mouse_buttons_endplay.py` builder에 author 함수를 교체 주입한다. builder는 parent proof success/asset SHA, protected SHA, exact editor PID/project/dirty guard, 대상 부재를 확인하고 원본 BP를 복제한다. 단계마다 command/params/raw response를 report.json에 저장하고 compile validation/error count, topology 잘림·금지 참조, 원본 SHA 불변, 새 asset SHA를 검사한다. 실패는 success=false/error/do_not_retry=true로 보존하며 runtime_verified=false다. 이 부모 proof·외부 저장소·TCP/execute_python 호출은 새 격리 프로젝트와 공식 MCP로 재작성해야 한다. T1에서 함수 graph·caller gating·전체 builder 계약을 검증했다고 표현하지 않는다.

T 대표 검수는 정확한 변수/이벤트/GUID identity, 네 데이터·exec edge의 실제 양방향 readback, 예상 밖 노드/링크·자동 rename 부재, 명시 compile/save와 저장 후 의미 있는 cold 경계에 집중한다. 불확실한 수정은 재시도하지 않는다. 새 테스트 프레임워크·매 기능 패키징·전체 원본 fixture 복제를 요구하지 않는다.

## T 외부 파이프라인 소스

`cubeless/tools/pipelines/blueprint_variable_graph.py`는 새 UUID 격리 Actor BP를 만든다. Source=float3, Target=float0, Gate=booltrue 선언과 getter 두 개·Target setter·Branch를 7-child 배치로 작성한 뒤 실제 응답 GUID를 받는다. 없는 변수와 bool→float 직접 연결의 선행 거부·무변경을 확인하고, Source→Target/ Gate→Condition/ ReceiveBeginPlay→setter→Branch의 네 연결과 최종 compile/save를 5-child 배치로 실행하도록 작성했다. factory 이벤트는 native event_name=ReceiveBeginPlay와 event_member_parent_path=/Script/Engine.Actor로 고르며 제목은 사용하지 않는다.

명시 full compile/save 각 1회, 양방향 전체 edge 집합·노드 identity·공식 CDO 기본값·dirty/파일 불변을 검사한다. CDO Target은 런타임을 실행하지 않았으므로 0을 기대한다. 원본 함수 timer의 나머지 단계는 계속 미완료다.

## 실제 대표 실행

`blueprint-variable-graph-T.json`은 PID30608에서 7-child 변수·노드 작성 배치와 5-child GUID 연결·compile/save 배치를 통과했다. Source→Target setter, Gate→Branch.Condition, ReceiveBeginPlay→setter→Branch의 4개 직접 연결에 대한 정확한 양방향 edge 집합, 노드 GUID/class/member identity와 기존 factory 노드 보존을 확인했다. 없는 member는 graph_member_not_declared, bool→float 직접 연결은 conversion_contract_not_implemented/preflight에서 거부됐으며 그래프는 변하지 않았다. 공식 CDO Source3/Target0/Gatetrue, 저장 파일 불변과 dirty=[]를 확인했다. BeginPlay 실행 결과는 검증하지 않았다.

17 named calls/26 HTTP 관측, named 단계 elapsed 합계1.1207937초, 전체 파이프라인1.6142526초다. 작성 배치 호출0.2343425초/내부0.182053초, 연결·compile/save 배치 호출0.23915초/내부0.18426초다. 명시 full compile/save 각1회이며 factory 암묵 컴파일은 별도다. warm NullRHI·loopback의 작은 단일 fixture로 속도 개선율을 주장하지 않는다. 실행 source SHA `8cbeab641b03b7bacdf39cadfb3668f45b2be6ad2e0c94292fc582cbb788153c`를 보존한다. cold 재조회, MCP 없는 패키징과 전체 timer는 이 보고서만으로 완료가 아니다.

## 실제 Codex와 cold 확인

`actual-codex-cold-ST.json`의 새 PID3340(작성30608)에서 T 그래프 전체·GUID/노드/핀/링크 및 공식 CDO Source3/Target0/Gatetrue가 작성 보고서와 일치했다. 두 저장 BP의 SHA/mtime와 dirty=[]도 유지됐다. 저장 registry2roots/2packages의 edges/violations/incomplete/dirty=[]는 저장 참조 계층의 증거이며 packaging_verified=false다. `actual-codex-variables-graph-ST.json`의 새 operation ID 변수25·기존 GUID link 호출은 무변경이고 replay가 아니다. `source-and-asset-persistence-ST.json`은 후속 U 변경 전 ST freeze의 root/isolated97파일 일치를, `target-preservation-after-ST.json`은 보호1,059개 승인 uproject 외 변경0을 기록한다. BeginPlay 실행·ST 패키징·전체 원본 계약 동등성은 계속 미검증이다.
