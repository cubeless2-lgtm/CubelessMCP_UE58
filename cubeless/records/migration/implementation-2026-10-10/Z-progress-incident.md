# Z 진행·중단 기록 — Z/Z2와 Z3 후속 결과

2026-10-11 KST. 빌드·공식 등록과 제한된 event 재사용은 확인했으나, 상속 Tick 파이프라인은 아직 완료되지 않았다. 실제 Z no-MCP 이벤트 실행·cold·Shipping은 `not_run`이다. 이 기록은 해당 시점의 사건만 다루며 README·전체 상태표·기능 원장의 완료 판정을 바꾸지 않는다. 모든 에디터 실행·정리는 root가 수행했다.

## 빌드와 등록

- `build-inherited-tick-Z1.log`: 증분 빌드 성공31.69초. `python-contract-tests-Z.log`:111검사 성공6.273초. 이후 의미 비교 관련20개 집중 검사는 담당 agent가 통과를 보고했으며 실제 엔진 실행의 대체 증거가 아니다.
- `registered-schemas-and-diagnostics-Z.json`: 공식8 Toolset/56개 도구, 배치30개 capability, PID65856. 기존 Y54개에서 event 추가와 parent-call 확장이 더해졌지만 전체 원본 기능 동등성 완료를 뜻하지 않는다. parent-call은 별도 신규 확장이라는 구분을 유지한다.
- `build-inherited-tick-Z2.log`: 아래 DTO 교정 후 성공6.52초. `registered-schemas-and-diagnostics-Z2.json`은 PID55548,8/56을 기록한다. DLL SHA256은 `2a39bdc2b97bb2e1434978a049a782755cfb26643bb03e8b3c567f526973f284`다.
- `source-snapshot-Z.json`과 `source-snapshot-Z2.json`은 각각 배포133파일의 일치를 확인하고 source를 보존한다. 바이너리는 hash만 기록하며 후속 개발 중인 root 소스를 해당 snapshot과 같다고 가정하지 않는다.

## 첫 저작: 반환 node identity 누락

`blueprint-inherited-tick-Z.json`은 `parent_before_links`에서 Python 오류 **`'object_path'`**로 중단됐다. 이전 `parent_nodes` batch는 성공 응답을 반환했지만 event 재사용 결과의 node identity 필드가 빠져 후속 파이프라인이 사용할 수 없었다. 이 시점에는 새 부모 fixture에 gate3개와 ParentDelta 변수, setter 노드가 이미 작성됐다. 따라서 전체 작업을 변경 전 실패로 표현하지 않는다. event 자체는 기존 factory 노드를 재사용했다.

native `CubelessEventApi.cpp`의 반환 복사에서 `NodeState(Node)`의 임시 smart reference가 소멸한 뒤 `Values` map을 순회하는 수명 결함을 수정했다. Z2는 `const auto State = NodeState(Node)`로 소유자를 유지하고, 반환 `object_path/node_guid/node_id/node_class`가 실제 노드와 같은지 확인하는 `returned_identity_verified`를 추가했다. 이는 node identity 전달 결함 교정이며 새 에셋 동작 검증과 구분한다.

첫 fixture `/Game/CubelessMCPTests/InheritedTick_da116088ff1d4d5db0bb467ca8ec998a/BP_TickParent`는 명시 full compile/save0, 부모·자식 파일 없음, source component SHA/mtime 불변, `outcome_unknown=false`, 자동 재시도0이었다. `editor-prelaunch-Z2.json`은 dirty 목록에 이 소유 fixture 하나만 있었음을 기록한다. 실패 보고서와 관측을 보존한 뒤 disposable editor 종료로 해당 미저장 fixture만 폐기했다. 이것은 별도 정리이며 원자적 rollback이나 불확실한 수정 재시도가 아니다.

`actual-codex-event-reuse-Z2.json`의 실제 공식 Codex 호출은 수정된 event 재사용 결과를 확인했다. `reused=true`, `created=false`, `returned_identity_verified=true`, GUID/object_path 포함, `changes=[]`, compile/save 미요청·미시도였다. 이 호출 성공만으로 상속 Tick 전체를 완료 처리하지 않는다.

## Z2 저작: 기존 parent-call의 의미 비교에서 중단

`blueprint-inherited-tick-Z2.json`은 새로운 UUID `c0831d0be47944c4986efdfcaad335b7`에서 부모를 작성·full compile/save 각1회 수행했다. 부모 파일 SHA256 `b88a0683fc4efe9907ed9a1868cd8ec0fea21e020bd1350999b060359762dd99`,34,780bytes,mtime `1791646368545473900`은 보고서 종료 시에도 같았다. 부모는 저장·clean 상태로 보존됐다.

자식은 이 saved generated parent에서 생성됐고 local MouseButtons component·ChildDelta·event 단계까지 진행했으나, `child_parent_call`에서 다음 결과로 멈췄다.

- `stage=preflight`, `error_code=parent_function_mismatch`
- `Existing parent call has a different identity/signature; it was not replaced.`
- 해당 호출 `changes=[]`, `mutation_attempted=false`, `changed=false`, `change_state_known=true`, compile/save 미요청·미시도
- 전체 report `outcome_unknown=false`, 자동 재시도0. 자식 `.uasset`은 아직 없다. 앞서 작성된 자식의 메모리 변경은 전체 작업의 알려진 부분 변경이다.

관측된 factory parent-call의 해석은 `/Script/Engine.Actor:ReceiveTick`이고 현재 guard는 가까운 Blueprint 부모 함수와 identity/signature 일치를 기대했다. **이 차이만으로 factory 노드가 잘못됐다고 결론 내리지 않는다.** 엔진의 parent-call 컴파일·상속 함수 해석 의미는 native 담당이 조사 중이다. 기존 노드 교체나 거부 완화가 정당한지 이 조사와 실제 후속 검증으로 결정해야 한다. Z2 부모 저장 결과와 source component는 보존하며, 자식 미저장 상태의 후속 처리는 별도로 관측·기록해야 한다.

## 다음 검증 경계

고정된 `verify_inherited_tick_runtime.py`는 독립 no-MCP 프로세스에서 parent/child/component3개 SHA와 CDO, 실제 host/SCS 생성, 여섯 직접 `ReceiveTick` 호출, ParentDelta/ChildDelta 및 조건부 timer, 소유 Actor 제거와 보존을 확인하도록 준비됐다. gate 입력에는 공개 `PropertyAccessChangeNotifyMode.NEVER`를 사용해 editor PostEdit가 SCS를 재생성하지 않도록 하며 실제 binding과 component identity를 확인한다. 이는 임시 instance의 테스트 입력 준비이지 일반 에디터 속성 설정 도구의 동등성 증거가 아니다.

현재 이 검수기는 실행되지 않았다. 이후 성공하더라도 `call_method`의 직접 ProcessEvent 호출과 실제 게임 월드 Tick 스케줄링·BeginPlay·hardware input·원본 전체 host·Shipping은 구분한다. Z/Z2 raw 보고서와 source snapshot은 그대로 보존한다.

## 후속 소스 조사: Z2 guard의 잘못된 거부로 확인

설치 UE5.8.3의 `Engine/Source/Editor/BlueprintGraph/Private/CallFunctionHandler.cpp:663–684`는 CallParent 노드에서 serialized member의 선언 클래스를 그대로 호출 대상으로 사용하지 않는다. `Context.NewClass->GetSuperClass()->FindFunctionByName`으로 가장 가까운 부모 구현을 찾는다. `:800–821`은 컴파일 상태에 맞는 클래스에서 함수를 다시 찾고, `:584–588`은 이 함수와 `bIsParentContext`를 compiled statement에 기록한다. `Editor/KismetCompiler/Private/KismetCompilerVMBackend.cpp:1331,1377–1393`은 parent context를 final 호출로 처리하여 해당 함수 포인터를 출력한다.

따라서 이 fixture의 factory serialized Actor 선언 참조는 유효하고 컴파일에서는 가까운 Blueprint 부모 구현을 대상으로 삼는다. Z2의 엄격한 문자열 identity 비교는 우리 guard의 false-positive였으며 factory 노드가 잘못된 것은 아니다. native 교정은 exact native 최초 선언·typed pin·native zero GUID(또는 이미 허용한 generated context)를 확인한 경우에만 기존 노드를 수정 없이 재사용하고, `effective_compile_parent`/`effective_compile_function`을 editor 조회·serialized 참조와 구분해 반환하도록 진행 중이다. 임의 ancestor/변경된 signature를 넓게 허용하는 완화가 아니다.

이는 소스 의미 확인과 구현 중인 교정이다. 교정본의 빌드·배포·새 저작과 실제 여섯 직접 이벤트 호출 결과는 아직 이 기록에 없으며, Z2 실패 raw를 성공으로 바꾸지 않는다.

## 후속 Z3: 저작·실제 부모 호출·조건별 실행 통과

위 문단은 각 실패 당시의 상태다. 이후 `build-inherited-tick-Z3.log`가6.63초에 성공했고 `registered-schemas-and-diagnostics-Z3.json`은8 Toolset/56도구·batch30과 PID18616을 기록했다. DLL SHA256은 `ffec85b98177a1d58766995bed0967db1654ec614783d423badc8ab16abef953`다. `source-snapshot-Z3.json`은 실행 당시 root/격리133파일 일치와 실행 source를 보존하며 이후 AA 개발 소스와 구분한다. Z/Z2 원본 실패와 snapshot은 수정하지 않았다.

새 `blueprint-inherited-tick-Z3.json`은28calls/38HTTP·단계합3.8974209초·전체5.7551638초로 부모와 자식을 각각 full compile/save했다(합계2/2). 합계16nodes/17physical edges를 저장했다. event와 parent-call은 기존 factory 노드 재사용이었다. 컴파일 전 parent 참조는 original_native_declaration, 컴파일 후 direct_generated_parent로 관측됐다. 이는 엔진 컴파일의 정규화이며 원본 factory 참조가 잘못됐거나 도구가 강제로 고쳤다는 뜻이 아니다. `actual-codex-inherited-tick-Z3.json`의 새 ID parent-call 재사용은 no-op/changes=[]였고 저장 graph와 parent/child/component3개 의존성 감사·clean 상태가 통과했다.

`inherited-tick-runtime-Z3.json`은 MCP 없는 fresh PID26768에서 저장 사본을 로드하고 Actor/SCS를 실제 생성했다. 여섯 직접 ReceiveTick 호출에서 ParentDelta와 ChildDelta는 매회 delta였고 component timer는0/0/0/.25/.75/.75였다. transient gate 입력만 public PropertyAccessChangeNotifyMode.NEVER로 설정하고 component identity를 계속 관측했다. 소유 Actor1회 제거와 월드0→1→0, 파일·CDO·Config·선택·dirty 보존을 확인했다. 명시 compile/save0, 본체0.2259137초이며 프로세스는 종료됐다. `inherited-tick-verification-Z3.json`13checks가 author/actual Codex/runtime/source 보존 근거를 연결한다.

이 결과는 단일 NullRHI/loopback fixture의 직접 ProcessEvent 실행이며 에디터 시작 시간과 실제 게임 Tick 스케줄링을 포함하지 않는다. 새 event/parent-call 노드 생성, Actor 부모 guard 변경 뒤 실제 상속 자식 duplicate, BeginPlay·hardware input·press/release reset·전체 원본 host·Shipping은 미검증이다. parent-call은 신규 확장이고 원본 이식 identity를 추가하지 않는다. Z3 전체 BuildPlugin/패키징은 반복하지 않았으며 성능 개선율·원자적 rollback·불확실한 수정 자동 재시도를 주장하지 않는다.
