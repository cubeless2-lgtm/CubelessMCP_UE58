# UE 5.8.3 구현 상태

전체 이식은 진행 중이며 완료가 아니다. 아래는 2026-10-10 첫 구현 구간의 증거를 구분한 기록이다. 기능별 전체 추적은 `migration-ledger.json`을 사용한다.

## 최신 완료 검증 상태: Z3

Z3는 **8 Toolset/56개 도구·30개 batch capability**를 공식 등록했다. `build-inherited-tick-Z3.log` 증분 빌드6.63초가 통과했고 주 에디터 PID18616은 clean이었다. DLL SHA는 `ffec85b98177a1d58766995bed0967db1654ec614783d423badc8ab16abef953`다. `source-snapshot-Z3.json`의133파일은 실행 당시 root/격리 배포본과 일치하며 이후 AA 개발 소스와 구분한다. event는 기존 원본 이름의 이식 범위, parent-call은 별도 신규 확장으로 추적한다.

`inherited-tick-verification-Z3.json`13checks는 공식 author·실제 Codex parent-call 새 ID 무변경/저장 graph/의존성/clean 조회와 별도 no-MCP PID26768의 저장 사본 로드·여섯 직접 ReceiveTick 호출·소유 Actor 제거·보존을 연결한다. runtime 프로세스는 종료됐다. 이것은 공식 MCP cold·실제 게임 Tick 스케줄링·Shipping 증거가 아니다. 최신 보호1,059개 원본 파일 전수 비교는 `target-preservation-after-Y.json`의 승인 uproject 외 변경0이며 Z3에서 반복했다고 주장하지 않는다. 최신 전체 BuildPlugin은 N, Z3 전체배포/Shipping은 not_run이다.

사용자 지침에 따라 이제 검증은 플러그인 건강성을 우선한다. 공통 구현은 대표 정상 흐름과 중요한 실패 경계로 확인하고, 패키징은 주요 통합 또는 런타임 의존성 구조 변경에 묶는다. 전수 기능 추적은 유지하며 미구현·미검증 항목을 완료로 바꾸지 않는다. 기존 기록을 없애거나 매 기능마다 중복 보고서·반례 검수기를 추가하지 않는다.

- Blueprint 명시적 컴파일은 `SkipSave`로 엔진의 SaveOnCompile 자동 저장을 차단한다. 세 공개 wrapper × Always/SuccessOnly 6사례와 새 프로세스 저장 그래프 재로드가 통과했다. 명시적 save가 있을 때만 저장한다.
- Blueprint 생성 factory는 공개 SkipSave 인자가 없어 **SaveOnCompile=Never를 요구**한다. 설정을 임의로 바꾸지 않는다. 직접/배치 4개 거부와 Never 생성·명시 저장이 실제 MCP에서 통과했다. 생성 뒤 조회가 실패해도 이미 알려진 생성·암묵 컴파일 상태를 보존하도록 수정했다. 해당 Python 회귀 테스트는 51개 통과했다.
- Component 조회 2개는 N의 별도 fixture 3개에서 로컬 계층·10개 속성·오류·파일 불변을 검증했다. O는 로컬 SCS 추가·typed 속성 설정 2개를 확장해 네 native 클래스와 제한된 속성을 실제 작성·저장·재로드했다. P는 Shape 세 클래스의 크기와 named mesh 설정을 추가 확인했다. 상속된 최종값, native CDO, orphan SCS, 임의 클래스·속성 수정은 미완료다.
- Actor M의 저장 결과도 MCP 없는 Shipping에서 로드·쿠킹·실행을 통과했다. 기존 생성 에셋의 게임 런타임 참조에 Cubeless를 추가하지 않았다. 전체 게임 로직·렌더링 품질 검증을 뜻하지 않는다.

등록은 정상이나 전체 5.6 기능 동등성은 여전히 미완료다. 다음 기능군은 누락 대장에서 고유 동작과 실사용 파이프라인을 기준으로 이식하며, 같은 공통 코어의 검증을 파일 수만큼 반복하지 않는다.

## Z3: 저장된 Blueprint 상속·부모 이벤트와 조건부 컴포넌트 호출

부모 Actor BP는 gate3개와 ParentDelta를 갖고 ReceiveTick→부모 marker를 작성했다. 저장된 부모에서 생성한 자식은 inherited gate getter, ChildDelta, local MouseButtons component와 외부 timer call을 사용한다. factory ReceiveTick 및 parent-call을 재사용하고 기존 exec 흐름을 보호 조건으로 교체·재연결했다. 이 대표 결과가 새 event/parent-call 노드 생성의 성공을 뜻하지 않는다. Actor 부모 guard 확장 뒤 실제 상속 자식 duplicate는 실행하지 않았으며 V2의 기존 복제 근거와 구분한다.

첫 Z는 임시 smart reference의 수명 결함으로 반환 object_path가 빠져 부모 graph 연결 전 중단됐다. Z2는 이 결함을 고쳤으나 유효한 factory parent-call의 native Actor 선언 참조를 잘못 거부했다. 엔진 compiler가 가까운 Blueprint 부모 구현으로 해석하는 근거에 맞춰 guard를 교정했다. Z3에서 컴파일 전 original_native_declaration, 컴파일 뒤 direct_generated_parent로 엔진이 참조를 정규화했고 실제 Codex 새 ID 재사용도 changes=[]였다. node editor lookup·serialized declaration·effective compiled parent를 구분한다. 앞선 부분 변경·저장 부모·미저장 fixture 정리와 실패 raw는 [Z 사건 기록](Z-progress-incident.md)에 보존했다.

`blueprint-inherited-tick-Z3.json`은28calls/38HTTP·단계합3.8974209초·전체5.7551638초, 부모/자식 각각 full compile/save1회(합계2/2), 합계16nodes/17physical edges를 확인했다. 저장 parent/child/component3개 의존성 감사는 금지 참조·incomplete·dirty가 없었으며 packaging_verified=false다. 새 no-MCP 프로세스에서6회 직접 ReceiveTick의 ParentDelta·ChildDelta는 매회 delta, component timer는0/0/0/.25/.75/.75였다. host0→1→0 및 파일/CDO/월드 Actor 집합/Config/선택/dirty가 보존됐다. 입력 gate는 transient instance에 public notify_mode=NEVER로 설정해 editor reconstruction을 피했으며 실제 component identity를 계속 검수했다. 이는 일반 에셋 setter 동등성 주장이 아니다.

runtime 본체0.2259137초·명시 compile/save0이다. NullRHI/loopback 한 fixture이며 에디터 시작을 제외하고 속도 개선율을 주장하지 않는다. 자동 게임 Tick·BeginPlay·hardware input·press/release reset·전체 원본 host·모든 event/parent 옵션과 Shipping은 미검증이다. [공개 API와 제한](Z-API.md)을 참조한다.

## Y: 생성된 컴포넌트의 로컬 SCS 추가와 외부 함수 호출

Y는 기존 세 도구의 범위를 넓혀 **8 Toolset/54개 도구·28개 batch capability**를 유지했다. Y1 Cast 컴파일 오류(14.83초)를 교정한 `build-component-host-Y2.log`가9.89초, `python-contract-tests-Y.log`106tests가5.580초에 통과했다. 공식 에디터는 UE58Verification PID30644, DLL SHA `dc666e7aebdf02c7f951e87ccb39a9f1e3b4dae3268ceb58ef451c07f0217904`다. `source-snapshot-Y.json`의124파일은 Y 등록 당시 배포본과 일치하며 이후 Z 개발 소스와 구분한다.

`component-host-verification-Y.json`12checks는 공식 author·실제 Codex component/graph/dependency/clean 조회와 별도 no-MCP PID24028의 저장 사본 로드·실제 Actor/SCS 생성·두 함수 호출·제거·보존을 연결한다. no-MCP 프로세스는 종료됐다. 공식 MCP cold 또는 Shipping이라고 표시하지 않는다. 이 문단의 최초 작성 때 보호1,059개 원본 파일 전수 비교는 앞선 W3 근거였다. 후속 target-preservation-after-Y.json이 승인 uproject 외 변경0을 확인했다. 최신 전체 BuildPlugin은 N, Y 전체배포/Shipping은 not_run이다.

원본 mouse_event_host의 MouseButtons 추가/getter와 mouse_timer_host의 외부 component self 연결을 묶었다. 저장·clean·컴파일된 non-scene ActorComponent의 정확한 generated class만 허용하며 로컬 SCS root로 추가한다. 공간 부착이 아니므로 identity transform·parent 없음·초기 property edits 없음으로 제한한다. 추가 후 skeleton-only 갱신을 실제 컴파일 작업으로 구분하고,3/2/4-child 배치의 마지막에만 명시 full compile/save 각1회를 요청했다.

`blueprint-component-host-Y.json`23calls/33HTTP·단계합1.2591456초·전체1.9087676초에서3nodes/3physical edges와 component source/기존 root 보존을 확인했다. 에디터 inspect의 함수 해석은 `SKEL_BP_FunctionTimer_C`를 가리킬 수 있지만 serialized member parent와 self object pin type은 `BP_FunctionTimer_C`다. skeleton 조회를 게임 런타임 참조라고 잘못 판정하지 않으며 실제 결과의 두 필드를 분리한다.

`component-host-runtime-Y.json`은 fresh no-MCP PID24028의 실제 Actor host 생성→MouseButtons property/객체 목록/정확 class/owner 일치→.25/.5 호출 결과 .25/.75→소유 Actor1회 제거를 통과했다. 월드 host0→1→0, 기존 Actor path/class 집합·BP/CDO/status/file SHA/mtime·Config·선택·dirty가 보존됐다. 본체0.1627278초·명시 compile/save0이며 암묵 load 작업은 측정하지 않았다. warm NullRHI/loopback 한 fixture, 에디터 시작 제외이며 성능 개선율이 아니다. 파괴는 별도 비원자적 작업이고 전체 undo/click 상태 복원은 검증하지 않았다.

상속 Tick·hardware input·외부 variable setter·generated SceneComponent·상속 SCS 수정·임의 생성자 동작·원본 전체 host/Shipping은 미완료다. [공개 API와 제한](Y-generated-component-api.md), [runtime 검수 범위](Y-runtime-plan.md)에 설계 근거가 있으며 실제 성공 근거는 위 실행 보고서다.

## X: 보호 조건이 있는 exec 연결 교체

X는 **8 Toolset/54개 도구**, 배치 manifest 28개 named capability를 공식 등록했다. `build-guarded-links-X1.log` 증분 빌드18.65초, `python-contract-tests-X.log` 101tests/4.019초를 통과했다. 공식 검증 에디터는 UE58Verification PID39104이며 로드 DLL SHA는 `0e56f1de117741f11badce4d1cc5367d9d92a90247674467d8db30f10866787d`다. `source-snapshot-X.json`의 121파일은 X 등록 당시 배포본과 일치하며 이후 Y 개발 중인 root 소스와 동일하다고 가정하지 않는다.

`guarded-links-verification-X.json`11checks는 실제 Codex의 named no-op·저장 graph·dependency·dirty 조회와 별도 no-MCP PID23012의 새 저장 사본 로드/타이머6사례 실행을 연결한다. 공식 MCP cold나 Shipping 증거로 표시하지 않는다. 보호 대상1,059파일의 최신 전수 비교는 앞선 W3 기록이며 X에서 같은 검사를 다시 했다고 주장하지 않는다. 최신 전체 BuildPlugin은 N이며 X 전체배포/Shipping은 수행하지 않았다.

새 named `connect_blueprint_graph_pins`는 기존 안전한 연결 코어를 공유한다. source1/old1/new0과 정확한 old node/pin·상호 링크·`break_others_a`를 확인한 exec 한 연결만 교체한다. 잘못된 old endpoint와 성공 후 새 ID의 오래된 조건은 변경 전 거부했고, 같은 ID는 journal replay로 재수정하지 않았다. 이후 별도 연결로 원래 흐름을 이어 마지막 batch에서 compile/save 각1회만 요청했다. 11nodes/12physical edges, 다른 graph·기본값·source 보존을 확인했다.

`blueprint-guarded-links-X.json`:25named calls/34HTTP, 단계합1.4361454초·전체2.3835839초. `gated-timer-runtime-X.json`은 fresh no-MCP PID23012에서6/6 통과했고 timer0/0/0/.25/.75/.75·LastDelta·CDO/file/Config/dirty 보존을 확인했다. 본체0.1356369초, 명시 compile/save0이며 암묵 load 작업은 측정하지 않았다. warm NullRHI/loopback 단일 fixture이고 에디터 시작 제외·성능 개선율 주장은 없다.

현재 5.6 공개 wrapper/활성 C++에는 pipeline 문서가 요구하는 expected-old 보호 필드의 강제가 없다. 가용 Git 검색과 원본 test도 그 불일치를 뒷받침하여, X는 복구 가능한 문서 계약을 좁게 구현했다. 넓은 원본 허용 동작과 동일하다는 주장이 아니다. 데이터/target 교체·conversion/promotion·occupied new target·`compile=True`/`save_asset=True`·외부 component/full host는 미완료다. 원자적 rollback이나 불확실한 수정 재시도를 제공하지 않는다. [원본 차이·API·X 실행 기록](X-guarded-links-plan.md)을 참조한다.

## W: Sequence와 자체 함수의 조건부 호출

W2는 **8 Toolset/53개 도구**를 공식 등록했고 배치 manifest는27개 named capability다. W1 C++ 증분19.01초, Python93tests/3.689초를 통과했다. 첫 Sequence 생성 후 GUID 표기 비교 결함을 Python UUID 정규화로 교정했고 집중4tests/0.209초와 새 W2 등록을 확인했다. 로드 DLL SHA는 `611fec4b924241eb94f5170301f88ec370eb7429d0b56b1c3f94a10b60cfed25`다. W3는 이 plugin 그대로 테스트 gate3개의 is_exposed=True만 바꾼 fixture다.

공식 MCP8000은 `UE58Verification` PID35420이며 actual Codex의 저장 graph 동일성, Sequence output_count=1 변경 전 거부와 저장 dependency/dirty=[]를 확인했다. 별도 no-MCP PID46076에서 W3 저장 사본을 새로 로드하여 실제 gated timer6/6을 실행했다. 이를 공식 MCP cold로 표시하지 않는다. V2 clone의 공식 cold는 W 배포 PID42012에서11checks 통과했다. `gated-timer-verification-W3.json`15checks는 source/사본SHA·CDO·설정·dirty 보존, plugin116파일 W2 일치와 보호1,059개 승인uproject 외 변경0을 연결한다. `source-snapshot-W2.json`은 후속 X 수정 전 배포 소스를 보존한다. 최신 전체 BuildPlugin은 N이며 W 전체배포/Shipping은 수행하지 않았다.

원본 mouse timer host의 FieldResourcesReady→PawnInside→VisibilityInputAllowed 순서를 새 ActorComponent clone의 자체 함수 호출로 추출했다. Sequence then_0은 테스트 marker LastDelta를 항상 갱신하고 then_1은 세 Branch를 거쳐 CFNL_AdvanceMouseTime을 호출한다. 실제 fixture는 출력3개로 public pin extension도 확인하며 then_2는 미연결이다(원본 host는2출력). 기존 timer/EventGraph/source는 보존했다. own generated/skeleton 함수 identity·핀 signature를 확인하고 직접 재귀는 변경 전에 거부한다. 외부 component target·상속 Tick·기존 exec 교체·전체 host는 미완료다.

`blueprint-gated-timer-W3.json`:21calls/30HTTP·단계합2.1638213초·전체3.0985859초, 6/9/12-child batch와 마지막 명시 compile/save 각1회다.10nodes/11edges를 exact readback했다. warm NullRHI/loopback 단일 fixture이며 개선율이 아니다. `gated-timer-runtime-W3.json`은 MCP 없는 fresh PID46076에서 false gate 각각3사례→all true 누적2사례→false 복귀를 실행했다. 모든 LastDelta가 전달값과 같고 타이머는0/0/0/.25/.75/.75, CDO는False/0, dirty=[]·파일·project Config 보존이었다. 명시 compile/save0, 본체0.1494675초는 에디터 시작을 제외한다.

첫 W Sequence 실패는 노드 생성 후 GUID 대소문자 비교 오류이며 변이 전 실패가 아니다. 독립 관측 뒤 소유한 미저장 fixture만 폐기했고 재시도하지 않았다. W2 author는 성공했지만 첫 runtime-W는 CPF_DisableEditOnInstance로 gate setter에서 실패하여 함수 calls0, 보존PASS/outcome_unknown=true를 남겼다. W3는 새로운 fixture의 gate를 편집 가능하게 작성했고 verifier/plugin은 바꾸지 않았다. 모든 원본 실패와 실행 source snapshot을 보존한다. [W API·실행 증거](W-next-api.md)를 참조한다.

## V: ActorComponent·보호된 복제와 내부 실행

기존 timer author에 부모 선택을 추가했다. Actor 기본값과 U 원문 SHA snapshot을 보존하고 V ActorComponent의 공식 get_parent와 함수 graph/CDO0을 확인했다. author21calls/30HTTP·단계합1.3476237초·전체1.9764706초, 명시 compile/save 각1회다. `timer-runtime-V.json`은 source 에셋을 MCP 없는 UE58RuntimeProof로 해시 일치 복사한 뒤 PID57380의 ownerless transient component에 .25/.5/1.25를 호출해 .25→.75→2.0을 관측했다. CDO0·source/사본SHA·dirty=[]·MCP 비활성 및 명시 compile/save0을 확인했다. 연결 검수9checks가 통과했고 본체0.054618초는 에디터 시작 시간을 제외한다.

첫 `blueprint-duplicate-V.json`은 SourceControlHelpers 비노출 오류로 dry-run preflight에서 복제/저장 전 실패했다. 설치 ScriptName=SourceControl에 맞게 수정한 V2는 새 UUID·새 요청으로 실행했다. `blueprint-duplicate-V2.json`에서 source graph/GUID/SHA 불변, clone의 경계 정규화 node/핀/edge·parent/CDO0·저장/dirty를 검수했다. 20calls/28HTTP·단계합0.9012397초·전체1.3527397초, 실제 duplicate0.1634635초/내부0.118669초다. 명시 compile0/save1이고 암묵 PostDuplicate compile은 UpToDate 상태만 관측했다. warm NullRHI/loopback 단일 fixture이며 성능 개선율이 아니다.

실제 Codex의 새 ID dry-run도 changes=[]로 통과했다. 이후 W 배포 PID42012의 `actual-codex-cold-duplicate-V2.json`에서 source/clone의 전체 graph·CDO·파일과 saved dependency를11checks로 확인했다. 원래 V2 clone의 직접 runtime·press/release reset·원본 전체 builder/host·V 패키징은 미완료다. W3 own caller gate 실행은 위 별도 fixture 증거다. named batch capability는 등록했지만 duplicate child 실제 호출은 이번 standalone 결과와 구분한다. [V 원본 연결·실패·실행 범위](blueprint-duplicate-V.md)를 참조한다.

## U: 함수 그래프·입력·정확한 호출 노드

원본 mouse timer author의 함수 생성→DeltaSeconds float input→변수 getter→KismetMathLibrary.Add_DoubleDouble→setter→data3/exec1 연결을 새 Actor BP에 이식했다. 3/3/5-child 배치로 진행하고 마지막에 명시 full compile/save 각1회만 요청했다. exact graph/node/function identity와 GUID, 양방향 네 edge, float→double 두 연결·double→float 한 연결의 엔진 direct MAKE를 확인했다. duplicate parameter는 function_parameter_collision/preflight로 거부됐고 그래프가 변하지 않았다. EventGraph는 factory 상태를 유지하며 공식 CDO TimeSinceLastClick은 실행하지 않은 기본값0이다.

`blueprint-function-timer-U.json`: 20 named calls/29 HTTP, 단계 elapsed 합계1.1892045초·전체1.7205148초. 세 배치 호출은0.1411293/0.1301898/0.2678329초, 내부는0.088888/0.088654/0.213807초다. warm NullRHI·loopback 단일 fixture 측정으로 개선율을 주장하지 않는다. 실제 Codex의 새 operation ID graph GUID 조회는 created=false/changes=[]이며 replay가 아니다(`actual-codex-functions-U.json`). cold 저장 registry1root/1package의 edges/violations/incomplete/dirty=[]는 저장 참조 계층의 증거이며 packaging_verified=false다.

처음 지원하는 parameter는 primitive scalar input, call은 정확한 native Engine static 함수와 primitive pins다. 원본 ActorComponent 복제·부모 proof·transient 누적 호출·호출 조건, instance/Blueprint function 호출·output/배열/참조 parameter·latent/wildcard·링크 교체 등 전체 계약은 미완료다. [U 계약과 증거](blueprint-functions-U.md)에 범위를 기록했다. 엔진 시작 smoke 실패의 과거 R 원인 기록은 유지하며 에디터 전체 오류0을 주장하지 않는다.

## S/T: 변수 상태와 member 그래프

S는 원본 physics_variables의 네 변수 이름·타입·초기값을 추출하고 hidden GUID/editable, metadata, Mass10→25 기본값 변경을 검증했다. 6-child batch의 명시 full compile/save 각1회 뒤 공식 CDO와 metadata를 읽었다. 중복 변수 거부, dry_run, 새 ID 동일값 요청에서 변경이 없었다. full compile이 descriptor default를 CDO로 옮겨 비우는 엔진 동작도 구분했다. `blueprint-variables-S.json`: 16 calls/24 HTTP, 단계 elapsed 합계0.8903376초, 전체1.2777615초, 핵심 batch0.2993754초(내부0.248267초).

T는 새 BP의 Source/Target float와 Gate bool로 getter2/setter1/Branch1을 만들고 GUID로 data2/exec2 연결을 작성했다. BeginPlay는 실제 native event member로 골랐고 양방향 exact edge·노드 identity·공식 CDO 기본값·저장 불변을 확인했다. 없는 변수/부적합 bool→float 연결은 변경 전 거부됐다. `blueprint-variable-graph-T.json`: 17 calls/26 HTTP, 단계 elapsed 합계1.1207937초, 전체1.6142526초. 7-child 작성 batch0.2343425초/내부0.182053초, 5-child 연결·compile/save batch0.23915초/내부0.18426초이며 명시 full compile/save 각1회다.

작은 warm NullRHI/loopback 단일 fixture 측정이며 속도 개선율을 주장하지 않는다. 실제 Codex cold에서 변수 rows·metadata entries·그래프 전체가 작성 결과와 정확히 일치했다. 저장 registry2roots/2packages는 edges/violations/incomplete/dirty=[]이며 packaging_verified=false다. 별도의 실제 Codex 새 ID 변수25·기존 GUID link 요청도 무변경으로 끝났고 journal replay가 아니다(`actual-codex-variables-graph-ST.json`). S/T 당시 BeginPlay 실행·형변환/promotion·외부/상속 member·함수 graph/parameter/call-node 및 원본 mouse timer/physics 파이프라인 전체는 미완료였다. 함수 저작 대표의 후속 U 증거는 위에 분리했다. [S 계약](blueprint-variables-S.md)과 [T 계약](blueprint-variable-graph-T.md)에 출처와 범위를 기록했다.

## R: 숨은 material 슬롯 0 계약

R2 증분 빌드가 7.27초에 성공했고 **8 Toolset / 42개 도구**의 공식 등록과 실제 Codex 새 프로세스 조회를 확인했다. 기존 named set_static_mesh_properties에 material 슬롯 0 계약을 추가했으므로 도구 수는 늘지 않았다. R1은 TObjectPtr 조건식 컴파일 오류로 실패했고 교정한 R2를 배포했다. 상세 근거는 `build-component-R2.log`, `registered-schemas-and-diagnostics-R.json`, `actual-codex-cold-R.json`이다. 로컬·격리 plugin의 79개 파일은 등록 snapshot과 일치한다(`source-and-material-persistence-R.json`). 최신 전체 BuildPlugin 배포본은 이전 N(`cubeless/build/Distribution-20261010-N`, 약 106초 성공)이다. R 전체 BuildPlugin과 MCP 없는 패키징은 수행하지 않았다.

R 검증 당시 공식 MCP 8000 연결 대상은 사용자 게임이 아닌 `cubeless/workspaces/UE58Verification`의 격리 에디터(PID 37632)다. R 통합 후 보호 파일 1,059개 중 승인된 `.uproject` 외 변경 0개를 확인했다(`target-preservation-after-R.json`). 저장 에셋 재조회 후 dirty 패키지는 없었다.

기존 named mesh 도구 뒤에 material=None을 추가해 공개 인자 순서·Cube 기본값·mesh-only journal 계약을 유지했다. material-only는 static_mesh=""를 명시한다. 로컬 exact StaticMeshComponent에 runtime Material/MaterialInstanceConstant의 슬롯 0을 지정하며 명시 compile/save를 자동 실행하지 않는다. 두 asset 참조를 쓰기 전에 검수하고 원본의 잘못된 로드 silent-success를 명시 오류로 교정했다.

`material-overrides-R.json`은 Plane+기본 Material의 material-only 상태, 이후 Cube+MIC와 compile/save를 묶은 2-child batch, 잘못된 material 요청의 변경 전 거부를 공식 ObjectTools readback과 파일 불변으로 검수했다. 전체 16 calls/24 HTTP/0.8713781초, 핵심 batch0.1552715초(내부0.116912초), 명시 compile/save 각1회다. 실제 Codex의 새 ID 동일값 요청은 no_op=true이고 journal replay가 아니다. 기존 generic mesh-clear null은 현재 override 손실을 막는 component_material_slots_unsupported/preflight로 거부됐다.

새 PID37632(작성 PID29172)의 실제 Codex cold 조회로 컴포넌트 GUID·계층·transform, Cube+MIC raw override, 빈 overlay 배열이 같고 파일 SHA/mtime와 dirty=[]가 유지됨을 확인했다. 저장 BP1root/재귀8패키지 의존성 감사는 violations/incomplete/dirty=[]이며 packaging_verified=false다. 작은 warm NullRHI/loopback 단일 표본으로 성능 개선율을 주장하지 않는다. 엔진 PostEdit의 override·overlay 꼬리 삭제를 막는 공통 slot-count guard를 적용했으나 다중 슬롯·다단계 상속 실행 조합과 렌더링·shader 완료는 미검증이다. [원본 조사·계약·실행 범위](component-material-R.md)를 참조한다.

## Q: 저장 물리 기본값

공개 `set_physics_properties` 이름과 기본 인자를 복구하고 원본 C++의 gravity_enabled 무시 결함을 교정했다. 대상은 로컬 미등록 exact StaticMesh/Sphere/Box/Capsule template이며 저장 기본값만 다룬다. 명시 compile/save를 요청하지 않는 단독·named batch 공통 코어를 사용한다. mass [0.001,1e12], damping [0,1e12] 및 엄격한 bool 계약은 원본의 느슨한 입력 허용과 구분한다. 공개 다섯 physics setter와 중간 override를 보존하는 bounded archetype chain 검사를 사용한다. 복잡한 다단계 상속 전파는 소스 리뷰만 수행했으며 실제 실행 조합은 미검증이다.

대표 `--physics-defaults`는 Sphere(radius30)/Cube StaticMesh 추가, Sphere 기본 인자 생략, Mesh 명시 false/false/mass7.5/linear.2/angular.3, 마지막 compile/save를 5-child batch에 묶었다. 전체 13 calls/21 HTTP/0.9861초, 핵심 batch 0.2482초(내부 0.202811초), 명시 compile/save 각 1회다. 공식 BodyInstance의 bSimulatePhysics/bOverrideMass/bEnableGravity/massInKgOverride/linearDamping/angularDamping/physicsBlendWeight 7개 필드를 float32 허용 오차 내 비교했다. mass=-1은 Python preflight에서 변경·native dispatch·컴파일·저장 없이 거부됐고 저장 값과 파일은 그대로였다. 같은 값을 새 operation ID로 호출한 실제 Codex 요청도 no_op=true/changes=[]/native mutation=false로 끝났다. 이 결과는 journal replay와 구분한다.

새 PID 43660의 실제 Codex cold 조회에서 컴포넌트 GUID·클래스·부모·transform 및 공식 저장 BodyInstance 값이 동일했고 파일 SHA/mtime와 dirty=[]도 유지됐다. 저장 BP 1개/재귀 registry 6패키지의 금지 참조·dirty는 0개이나 `packaging_verified=false`다. 작은 warm 격리 fixture·NullRHI·loopback 단일 표본으로 속도 개선율을 주장하지 않는다. 원본 변수·이벤트·입력 그래프 파이프라인 전체, 실행 중 물리·충돌·렌더링과 Q 패키징은 미검증이다. [계약·공개 API·원본 차이](component-physics-Q.md)를 참조한다.

## P: named mesh 복구와 Shape 크기

`set_static_mesh_properties` 공개 이름을 복구했다. 기본 static_mesh 인자를 생략한 asset-only cube author P는 21 calls/29 HTTP/1.0505초에 컴파일·저장 각 1회를 통과했고, 새 프로세스 reload P는 9 calls/17 HTTP/0.3608초에 저장 값을 확인했다. 잘못된 mesh는 실제 Codex 연결에서 `component_mesh_reference_invalid`/preflight/변경0/compile0/save0으로 거부됐다. P는 새 액터·맵을 만들지 않았고 O의 이전 spawn 검증을 P 실행으로 계수하지 않는다.

기존 batch script의 `--shape-bounds`는 새 BP 하나의 Sphere/Box/Capsule add, SphereRadius setter, 마지막 compile/save를 5-child batch로 묶었다. 저장 값은 SphereRadius=64, BoxExtent=[20,30,40], CapsuleRadius=30/CapsuleHalfHeight=60이다. CapsuleRadius=61 거부 후 값·dirty·저장 파일 불변을 확인했다. 전체 15 calls/23 HTTP/0.9452초, 핵심 batch 0.2089초(내부 0.16057초), 명시 compile/save 각 1회다. 작은 warm 격리 fixture·NullRHI·loopback 단일 표본이며 속도 개선율은 주장하지 않는다.

새 PID 26272의 실제 Codex 조회에서 Shape 계층·GUID·클래스·부모·transform과 공식 ObjectTools 크기 readback이 일치했고 디스크 SHA/mtime도 유지됐다. 저장 BP 2개의 재귀 registry 7패키지에서 금지 참조·dirty 0개를 확인했지만 `packaging_verified=false`다. 공개 SetSphereRadius/SetBoxExtent/SetCapsuleSize를 사용하며, companion 자동 보정에 의존하지 않고 유효 Capsule pair를 사전검사한다. 원본 multi-component 전체 시나리오, 숨겨진 C++ material 옵션, collision profile·물리 시뮬레이션·임의 클래스 동등성은 미완료다. [범위·원본 계약·API 메모](component-shapes-P.md)를 참조한다.

## O: 기존 큐브 저작 흐름과 Component 배치

원본 `test_create_and_spawn_cube_blueprint.py`에 대응한 `component_authoring.py`가 공식 MCP에서 통과했다. 새 Actor Blueprint에 CubeVisual을 추가하고 Engine Cube를 지정한 뒤 BP 컴파일·저장 각 1회, 정확한 UObject 이름·클래스·transform으로 액터 생성, 맵 저장 1회를 수행했다. author O2는 29 named calls/39 HTTP 관측/1.7321초이며, 새 프로세스 reload O는 12 calls/22 HTTP/0.8364초로 BP·컴포넌트 GUID·속성·액터·저장 파일 불변을 확인했다. add의 중간 컴파일을 억제하고 DefaultSceneRoot를 명시 보존하는 계층은 원본과 구분한 조정이다. 기존 BP 재사용과 원본 전체 계약 동등성은 미완료다.

`component_batch_smoke.py`는 17 calls/25 HTTP/1.0004초에 통과했다. 단독 add의 기본 compile=true/save=false, 잘못된 bool 문자열의 변경 전 거부, 5개 child 명령을 한 MCP batch로 실행한 compile/save 각 1회를 확인했다. 전체 스크립트의 명시 컴파일은 단독 add와 마지막 batch의 합계 2회이며 factory 암묵 컴파일은 별도 관측이다. 핵심 batch 호출 0.1731초·내부 batch 0.1454초는 warm 작은 fixture/NullRHI/loopback의 단일 표본으로 속도 개선율을 뜻하지 않는다. 실제 Codex 연결의 cold 조회도 4노드 계층·GUID·transform, bVisible=true, AttachSocketName=Grip, 파일 hash/mtime 불변과 dirty=[]를 확인했다.

도메인 Python 계약 75개(Component 23/Actor 25/Material 27), 공통 plugin offline 검사 54개가 통과했다. 저장 BP 2개/map 1개의 재귀 registry 조회 8패키지는 금지 참조 0개였다(`component-saved-dependencies-O.json`, `packaging_verified=false`). O의 MCP 없는 패키징은 `not_run`이다. [계약·활성 핸들러·실패 범위 메모](component-authoring-O.md)에 원본과의 차이와 증거를 연결했다.

## 구현·등록·실행

- `Plugins/CubelessMCP_UE58`에 Editor C++ 모듈과 Python 패키지를 구현했다. 등록부, UE58 API 호환부, 작업 로직을 분리했다.
- 실제 UE 5.8.3 CL 58210709의 공식 MCP에서 최초 Toolset 4개/도구 16개에 이어 Material 9개를 추가한 Toolset 5개/도구 25개의 스키마를 조회했다(`registered-schemas-and-diagnostics-E.json`). 별도 자체 MCP 서버나 TCP 브릿지는 없다.
- 초기 C++ 증분 빌드 B/C/D와 bootstrap 배포 빌드 B가 성공했다. 이후 전체 배포 빌드 L은 약 82초, N은 약 106초에 성공했다. 각 로그는 검증 기록 폴더에 있다. L 증분 빌드는 9.65초, N2 증분 빌드는 11.14초다. 작업량·캐시·동시 부하가 달라 속도 개선율로 비교하지 않는다.
- Blueprint author-D의 19단계가 성공했다. 생성, 중복·잘못된 입력 거부, Branch 추가, exec 연결, stale 좌표 거부, 이동, 연결된 노드 삭제 보호, 삭제, 독립 readback, 컴파일·저장을 검증했다.
- reload-D는 새로운 에디터 프로세스에서 저장한 Blueprint와 그래프를 다시 읽어 통과했다. 외부 독립 보고서 검수기도 author/reload의 에셋·프로젝트·프로세스·SHA 연결을 확인했다.
- 비어 있는 문자열 기본값이 공식 스키마에서 필수 인자가 되는 문제를 확인했다. nullable operation ID와 명시적 기본 패키지 경로로 수정했고, 두 인자를 생략한 실제 호출이 에셋 수정 전 예상 오류를 반환하는 것을 검증했다.

최신 완료 등록 검증은 Z3의 8개 Toolset/56개 도구이며 `registered-schemas-and-diagnostics-Z3.json`과 `source-snapshot-Z3.json`이 실행 DLL/Python 파일을 식별한다. 이후 AA 개발 소스와 구분한다. 최신 전체 배포본 N과 이전 기록은 해당 시점의 증거로 유지하며 현재 소스의 전체 동등성 증거로 확대하지 않는다.

| Toolset | 도구 | 검증 범위 |
| --- | --- | --- |
| CubelessNativeToolset | GetEditorDiagnostics, AuditPackageDependencies, GetSkeletalMeshSections | 신규 중복 CompileBlueprintValidation 직접 공개는 I에서 제외. 내부 Python helper는 유지하며 기존 Python 컴파일 3개 실제 호출을 I2에서 검증. SkeletalCube LOD0·잘못된 LOD/타입 검수는 E/E2 |
| CubelessPythonDiagnostics | health, get_operation | 시작·등록 상태 조회와 내구 저널. 중단 기록은 실제 변경 결과를 확정하지 않음 |
| CubelessBlueprintTools | create_blueprint, inspect_blueprint, compile_blueprint, compile_and_save_blueprint, compile_and_validate_blueprint, add_blueprint_variable, inspect_blueprint_variables, inspect_blueprint_variable_metadata, set_blueprint_variable_default | 기존 lifecycle과 S 변수·GUID/editable·metadata/default, Z3 saved Actor BP 부모의 자식 생성 검수. 모든 타입·원본 옵션의 동등성은 미완료 |
| CubelessBlueprintTools 복제 | duplicate_blueprint_to_temp | V2 standalone dry-run/new clone save/source 보존과 W 배포 시 실제 Codex cold11checks 통과. 해당 V2 clone의 직접 실행·Z Actor 부모 guard 확장 후 실제 상속 자식 복제·overwrite/dirty source·모든 원본 옵션은 미완료 |
| CubelessBlueprintGraphTools | inspect_graph, add_branch_node, connect_graph_pins, connect_blueprint_graph_pins, set_node_position, delete_node, add_blueprint_variable_get_node, add_blueprint_variable_set_node | T/U direct와 X guarded exec, Y local SCS getter/외부 self, Z3 inherited bool getter와 보호 교체 batch 실행 검증. 데이터 교체·변환·임의 외부/상속 member·compile/save True 옵션은 미완료 |
| CubelessBlueprintGraphTools 이벤트·부모 호출 | add_blueprint_event_node, add_blueprint_parent_function_call | Z3 factory ReceiveTick/parent-call 재사용·없는 event 거부·parent/child marker와 조건부 component 직접 실행 검증. parent-call은 신규 확장으로 원본 이식 identity에 미산입. 새 노드 생성·전체 event 옵션·게임 Tick 스케줄링은 미검증 |
| CubelessBlueprintGraphTools 함수·Sequence | resolve_blueprint_graph, add_blueprint_function_parameter, add_blueprint_call_function_node, add_blueprint_sequence_node | U native static, V timer, W/X 자체 조건부 호출, Y clean saved component의 own 함수 외부 호출 실행 검증. 임의 외부/상속 함수·모든 옵션·전체 caller는 미완료 |
| CubelessMaterialTools | resolve_material_graph, list_material_nodes, analyze_material_graph, list_material_collection_parameter_nodes, get_material_parameter_collection_values, connect_material_nodes, connect_material_property, delete_material_node, layout_material_nodes | Build E·공식 등록 E2, author-E3 36단계·새 프로세스 reload-E3와 독립 검수 통과. 작은 Material/MPC fixture 범위이며 전체 legacy 옵션·MaterialFunction·렌더링 검증은 아님 |
| CubelessMaterialTools 추가 | add_material_node, set_material_node_property | I/J author 43단계, I cold reload·독립 검수, I2 입력 경계 author/cold 검수 통과. JSON scalar 파서와 공식 배열 기본값 보완 포함. 모든 클래스/속성 옵션의 동등성은 미완료 |
| CubelessMaterialTools Custom | add_custom_material_node | K author 59단계, L cold 8단계 및 독립 검수. 기본값·별칭·입력·매크로·연결·중복 요청·오류·저장 검증. include/preprocessor와 큰 이름 배열 등은 미완료 |
| CubelessBatchTools | execute_batch | Z event/parent-call을 포함한 Blueprint/graph24개와 Component6개의 30개 capability. Z3 parent/child 작성·보호 exec 교체·compile/save 배치와 기존 증거를 유지한다. V duplicate child 실행·임의 원본 명령군/Undo/async 배치는 미검증 |
| CubelessActorTools | get_actors_in_level, find_actors_by_name, get_actor_properties, spawn_actor, create_actor, set_actor_transform, delete_actor | M author 49단계와 새 프로세스 M3 reload 13단계 통과. 저장된 단일 non-WP 레벨과 native 5클래스의 제한된 수정 범위. 저장된 Actor의 MCP 없는 Shipping 실행도 통과. 나머지 Actor 옵션·클래스·배치는 미완료 |
| CubelessActorTools 추가 | spawn_blueprint_actor | O2 저작 BP의 정확한 UObject 이름·클래스·transform 생성·맵 저장, O cold 및 실제 Codex 중복 이름 거부/대표 cold 조회 통과. 저장·컴파일된 /Game Actor Blueprint와 저장된 non-WP 레벨에 한정. O 런타임 패키징은 미실행 |
| CubelessComponentTools | list_blueprint_components, get_component_property | N5 read 51단계/N6 대표 cold와 O 저작 결과 조회 통과. 로컬 SCS·bounded typed 속성에 한정. native CDO/상속 최종값/orphan은 미완료 |
| CubelessComponentTools 추가 | add_component_to_blueprint, set_component_property | O/P native7클래스·typed/Shape setter와 Y saved generated non-scene component 추가/실제 host 실행 통과. Y는 identity transform·parent/초기 property edits 없음으로 제한. 일반 UObject setter·generated SceneComponent·임의 클래스·상속 수정·전체 parity는 미완료 |
| CubelessComponentTools named mesh | set_static_mesh_properties | P 기본 Cube·오류·저장/cold에 이어 R의 숨은 material-only/mesh+MIC 슬롯0·batch·오류·동일값·cold 검증 통과. 모든 입력/오류 동등성, 다중 슬롯/상속 실행·렌더링은 미완료 |
| CubelessComponentTools physics | set_physics_properties | Q Sphere 기본값·StaticMesh 명시값 batch/저장/cold, 잘못된 mass 거부와 새 ID 동일값 no-op 통과. exact StaticMesh/Sphere/Box/Capsule의 저장 기본값만 지원. gravity 누락 교정·엄격한 bounds는 원본과 구분하며 physics simulation/외부·다단계 상속 실행/전체 parity는 미완료 |

Material E3는 Constant/Add/CollectionParameter 그래프에서 연결·속성 연결·레이아웃·삭제와 MPC 기본값을 확인하고 저장 후 새 프로세스에서 재로드했다. author는 loopback 공식 HTTP 45관측/2.0154초의 단일 표본이며 시작 시간은 제외한다. 명시적 recompile과 save는 각 1회다. 공식 RecompileMaterial의 빈 오류 목록은 shader 완료를 보장하지 않으므로 `compile.passed`의 범위는 반환 오류 없음으로 제한한다(`material-E3-compile-scope.md`).

G/G2에서 확인한 JSON primitive 파싱과 배열 기본값 문제는 I에서 교정·실행 검증했다. 공식 Registry의 converter 확장 API를 정확한 Cubeless 파라미터 두 개에 적용하고, Python 생성 클래스의 빈 배열 default 메타데이터만 등록 전에 보완했다. I 빌드 17.91초, 실제 기본 좌표 생략/명시, MPC 필터 생략, 잘못된 배열·속성 입력 거부와 독립 readback을 통과했다. UE 상위 변환기는 null 필드를 skip하므로 MPC explicit null은 []처럼 관찰됐다. 스키마는 null을 허용하지 않지만 항상 거부된다고 주장하지 않는다. 다른 공식 ObjectTools 배열 인자와 MaterialTools 배열 반환도 검증했다.

Blueprint/graph 10개 명시적 명령 배치는 author-G/I 24단계와 새 프로세스 reload-G2/I, 독립 검수를 통과했다. 전체 legacy 동등성이나 Undo 검증 완료는 아니다. Actor 생성/컴파일·저장/조회 3작업은 G에서 batch 0.209543초 대 개별 0.156115초, 저널 최적화 적용 I에서 batch 0.155993초 대 개별 0.175761초였다. 각 한 표본·batch 우선·캐시 미보정이므로 통신 횟수 3→1 감소만 확정하고 속도 향상은 주장하지 않는다.

중복 최적화는 기존 3개 Python 컴파일 도구의 기본값/계약을 유지하고 공통 코어를 공유한다. 새 native 직접 공개 경로만 AIIgnore로 숨겼고 I 공식 스키마 27개 및 직접 호출 Unknown tool, 3개 Python wrapper 기본 refresh/save 동작을 실제 확인했다. 공통 저널 JSON은 동일 바이트를 한 번에 기록하며 exclusive/ownership/checkpoint/flush/fsync/replace를 유지한다. I는 이 serializer를 로드한 실제 batch 회귀다.

J는 MPC 값/CollectionParameter 노드 조회에서 같은 요청의 Before payload를 재사용한다. 마지막 독립 After 읽기는 유지한다. J 빌드 12.27초, I/J 비교 6사례×(warmup 1+측정 10)의 정상/오류 계약이 일치했다. 정상 4사례는 실제 capture 2회/reuse 1회/Execute 0회/해당 read helper 2회였고 기존 helper 3회는 소스 근거다. 서로 다른 프로세스·고정 순서·작은 fixture 및 에디터 tick의 영향을 받으므로 HTTP 측정치를 일반적인 속도 향상으로 확대하지 않는다. `../../verification/implementation-2026-10-10/material-read-reuse-results-J.md` 참조.

MPC Base→Mid→Leaf의 상속 이름/기본값/GUID와 직접 그래프 메타데이터도 author-I/cold-I 및 강화 독립 검수에서 통과했다. private override-map 값 변경, world runtime override, nested function, shader/render는 미검증이다.

M Actor는 5개 native 클래스 생성, 내부 create_actor 별칭의 공통 코어, UObject 이름과 라벨의 구분, 이동, null no-op, 동일 요청 replay, 중복 이름/잘못된 입력/보호 Actor 거부, 삭제를 검증했다. author 49 named calls/58 HTTP 관측/8.6204초, 명시적 맵 저장 1회, 컴파일 0회다. 한 프로세스의 단일 작은 fixture 측정이며 성능 향상을 주장하지 않는다. cold M3는 테스트가 소유한 Actor와 WorldSettings의 저장 상태 및 파일 해시를 확인했고, transient 엔진 Actor의 프로세스 간 동일성을 요구하지 않는다. `actor-author-M.json`, `actor-reload-M3.json`, `actual-codex-actor-saved-read-M2.json` 참조.

긴 FName 거부가 일반 FString까지 잘못 제한하지 않는지도 검수했다. Custom Code 2,080→3,104자와 Description 2,048자의 Add/Set·공식 속성 readback·저장·새 프로세스 M2 재로드가 통과했다. 정확히 1,024 UTF-16인 패키지명 거부와 실제 존재하는 MPC scalar 필터도 별도 통과했다. 이 추가 fixture의 패키징·렌더링은 미검증이다.

`bootstrap-additional-calls-E2.json`의 14단계는 `compile_blueprint`와 `compile_and_validate_blueprint` 자체 공개 경로를 각각 실행했다. 후자는 refresh_nodes=true/save=false이며 테스트 BP의 메모리 dirty 변화와 디스크 해시 불변을 별도로 기록했다. 테스트 종료 후 해당 격리 에디터만 재시작했다. 모든 원본 컴파일 오류·옵션의 동등 검증 완료는 아니다.

## 런타임 의존성 검증

Actor 추가 검증은 `actor-runtime-fixture-A.json`, `actor-runtime-cold-B.json`, `actor-package-static-A.json`, `actor-runtime-marker-verification-A.json`으로 연결된다. MCP 플러그인이 없는 별도 프로젝트의 저장 맵(소유 Actor 5개, 고유 native 클래스 4개와 실행 확인용 순수 Engine Blueprint)을 Win64 Shipping으로 빌드했다. 38.49초 build/cook/stage, Pak 1,780개 항목과 receipt에서 금지 참조 0개, PID 42924의 약 1.791초 정상 종료와 새 1,423바이트 SaveGame을 확인했다. 원본 테스트 파일은 보존했다. marker는 Blueprint BeginPlay 실행 증거이며 각 Actor의 전체 게임플레이 의미를 검증하지 않는다.

대상 `.uproject`에 Cubeless 활성화와 MCP/Python/AllToolsets의 Editor 대상 제한을 적용했다. 원본 파일의 바이트 백업과 변경 해시는 `target-plugin-enablement.json`에 있다. 사용자 에디터를 재시작하지 않았다. 초기 검증은 격리 프로젝트의 공식 MCP 포트 8001을 사용했고, L은 비어 있는 8000에 같은 격리 프로젝트를 실행해 실제 Codex 연결까지 확인했다. 원래 게임 프로젝트의 에셋에는 호출하지 않았다.

지원되지 않는 descriptor의 `EditorOnly` 필드를 제거했다. 실제 제한은 Editor 모듈 타입, TargetAllowList, Build.cs의 비에디터 대상 거부에 있으며 descriptor만으로 패키징 독립성을 주장하지 않는다.

최소 런타임 fixture-B를 작성·컴파일·저장했다. 순수 Engine의 Actor/SaveGame Blueprint와 테스트 맵이며 기존 author-D Blueprint도 배치했다. Cubeless/MCP 플러그인 파일이 전혀 없는 `cubeless/workspaces/UE58RuntimeProof`로 에셋 4개를 복사했다. 이 프로젝트는 기본 엔진 플러그인을 비활성화하고 검증용 에디터 Python만 허용한다.

MCP 없는 에디터 검사 B는 LevelEditorSubsystem 부재로 중단됐으며, 공개 정적 맵 로드 API와 의존성 카테고리 옵션을 교정한 검사 C가 통과했다. 이어 Win64 Shipping build/cook/stage와 pak 전체 1,782 entry/receipt 감사, 실제 게임 실행과 SaveGame 생성 검수가 통과했다. 외부 dispatcher의 package/marker 검수도 각각 통과했다. 추가로 초기 Distribution B 플러그인을 설치·Editor 활성화한 프로젝트도 Shipping 빌드·패키지 제외·실행을 통과했다. 이는 네 개 테스트 에셋과 한 가지 실행 동작 및 해당 배포 snapshot의 증거이며, 현재 Material 추가분과 이후 기능군 전체의 회귀는 남아 있다. 세부 범위·실패·UAT 자동 임시 모듈은 `../../verification/implementation-2026-10-10/runtime-proof-B.md`, `installed-plugin-runtime-proof-A.md`에 기록했다.

Material9 E3를 포함한 별도 MCP 없는 runtime clone/맵도 cold 검수 C, Shipping build/cook/stage 38.61초, pak 1,786개 항목의 MCP 제외, 실제 게임 SaveGame 생성 1.733초를 통과했다. 이 clone은 공개 API로 MPC 연결을 추가했고 원본 E3 에셋은 불변이다. Add/Set·MPC 상속·batch·렌더링 품질·최신 전체 플러그인 설치 상태의 증거는 아직 아니다. `../../verification/implementation-2026-10-10/material-runtime-proof-B.md`에 실패·범위·독립 검토를 구분했다.

이후 현재 저장된 Blueprint/batch/Material Add·Set/MPC 상속/Custom 묶음의 MCP 없는 Shipping D 검증과 **Distribution L을 Editor 대상으로 설치한 프로젝트**의 Shipping 검증을 모두 통과했다. 원본 19개와 사본, 새 runtime 연결 clone 3개 및 새 맵의 23개 저장 파일, 필수 22개 cooked 패키지, Pak 1,818개 항목을 확인했다. 플러그인 설치본의 보호 소스 57개는 배포 L과 일치했다. D 실행 1.729초와 설치 L 실행 1.658초에서 새 UserDir에 1,423바이트 SaveGame을 만들었고 MCP 런타임 제외를 검수했다. Shader 완료·화면 품질·모든 Blueprint 분기·Actor M은 이 범위에 포함하지 않는다. `current-runtime-proof-D.md`, `installed-current-runtime-proof-L.md` 및 독립 JSON 검수를 참조한다.

MCP 없는 통합 검증의 author A는 변경 전 실패, B의 새 clone 레지스트리 연결 누락은 단독 실패로 남았다. C의 새 프로세스에서 같은 파일의 연결을 확인했다. Shipping C는 IoStore 내용 검사 미지원으로 미완료이며, Pak-only를 명시한 별도 D를 통과시켰다. 과거 Material runtime B의 dependency 옵션 해석 오류는 `operations-material-runtime-B-query-scope-correction-D.md`로 정정했다. 실제 양쪽 selector=false는 Game 속성 필터 없음이므로 editor-only 참조도 포함한다.

## 전수 조사와 운영 자료

- 원본 공개 Python 정의 436개와 실제 FastMCP 등록 이름 합집합 436개가 정확히 일치했다. search 모드 공개 3개/내부 433개, eager 공개 433개다. UE 호출 없이 실제 등록만 검증했고 원본 41개 Python 파일 해시가 유지됐다. `legacy-runtime-registration.md` 참조.
- 원본 실행 스크립트 3,518개의 identity와 해시를 유지해 호출·입출력·검수·오류·보고서 정적 근거를 추출했다. lexical 순서는 런타임 trace와 구분한다. `operations-pipeline-contracts.md/.json` 참조.
- 과거 문서 792개, working tree/index/HEAD 사본 2,331개를 `cubeless/codex/archive/ue56`에 출처·해시와 함께 보존했다. NUL 손상 16개는 원본을 고치지 않았다. 보관은 이식 완료가 아니다.
- 현재 `cubeless/codex/AGENTS.md`, 역할 7개, 정책·워크플로·경험 문서를 작성했다. 역할 문서는 하위 에이전트를 자동 실행하지 않는다.
- 외부 공식 HTTP 클라이언트, Blueprint pipeline, 독립 보고서 검수기, offline dispatcher를 구현했다. 원본 dispatcher 42 gate/9 profile의 manifest는 보존했지만 기존 gate들은 아직 pending이며 실행 시 incomplete로 실패한다. 새 bootstrap/evidence profile만 검증했다.

## 실패와 남은 작업

- 엔진 시작 smoke CHECK 실패 15개는 R의 상세 로그에서 FUnifiedErrorTest_CreateErrorMessage(7), FUnifiedErrorTest_CreateErrorMessageWithContext(4), FStructuredLogFormatTest(4)로 식별했다. Cubeless 로드 전에 발생하며 의도된 샘플 오류와 구분한다. 영어 고정 기대값과 설치 한국어 번역의 불일치가 유력하지만 런타임 실제 비교 값은 로그에 없어 원인 확정·영어 재실행 통과를 주장하지 않는다. 엔진·언어 설정·테스트를 변경하지 않았고 전체 에디터 무오류도 아니다. [소스·번역 자료 조사](engine-startup-smoke-R.md)를 참조한다. 이전 Q 당시 미식별 기록은 그대로 보존한다.
- R1은 TObjectPtr 조건식 컴파일 오류로 실패했고 R2 수정 빌드 후 실행했다. cold 의존성 검수기의 dirty_packages 오타는 동일한 읽기 응답의 dirty_scanned_packages로 교정했으며 에셋 변이를 재시도하지 않았다. 재시작 직후 오래된 MCP session은 재초기화 후 정상 조회됐다.
- 최초 component-author-O는 mesh 참조 검사에서 정상 Engine Cube의 NavigationSystem 의존성을 거부했다. setter는 변경 전 중단됐고 이미 만든 BP/component는 별도 명시 저장으로 보존했다. 맵 로드의 recent-map bookkeeping으로 ini가 바뀌면서 최초 보고서 최종 오류를 덮은 검사기도 교정했다. 이후 전체 ini 해시는 관측으로 남기고 SaveOnCompile=Never의 전후 실제 값을 검수한다. 최초 실패·보존 기록을 유지한 채 새 UUID의 O2와 cold O를 통과시켰으며 원래 변이를 자동 재시도하지 않았다.
- N fixture 최초 시도는 Python에 노출되지 않은 BlueprintEditorSettings 이름 때문에 에셋 변경 전 실패했다. 실제 CDO와 공식 Toolset 속성 API로 교정했다. 두 번째는 UE5.8 SubobjectData gather가 동일 child handle을 중복 반환하여 미저장 fixture 생성 후 실패했다. 동일 handle만 제거해 새 UUID의 N3 fixture가 통과했고 기존 42개 파일은 유지됐다. 원본 실패 기록과 부분 상태를 보존했다.
- Component N4 조회 값은 맞았으나 검수기가 bVisible 비트필드의 C++ 타입을 bool로 잘못 예상했다. 설치 엔진의 uint8 선언/FBoolProperty 구현을 근거로 기대값만 정정하고 N5 조회를 통과시켰다. fixture 에셋을 다시 생성하지 않았다.
- Blueprint/Material 컴파일은 지원 엔진 API의 동기 실행이다. 큰 에셋에서 게임 스레드 지연 가능성이 있으며 작은 fixture 시간으로 일반 성능을 보장하지 않는다. no-save 확인은 엔진 compilation-manager 경로에 대한 것으로 모든 외부 callback의 부작용 부재까지 의미하지 않는다.

- 최초 C++ 빌드에서 `Error` 이름 충돌을 수정했다.
- author-A/B는 비노출 superclass API와 native UClass의 정상 RF_Transient 플래그를 잘못 다룬 검사 때문에 생성 전 실패했다.
- author-C는 생성 후 ParentClass readback에서 실패했다. 생성 에셋은 격리 폴더에 명시적으로 보존했다. 보존 호출 직후 외부 기록기의 AttributeError로 응답을 잃었으므로 해당 응답을 성공 증거로 쓰지 않았다. 로그·파일·dirty 상태의 별도 읽기 조사만 기록했다.
- fixture-A는 핀 래퍼의 객체 동일성 비교 오류로 실패했고 새 이름의 fixture-B에서 경로·핀명·방향 비교로 수정했다. 실패 기록을 덮어쓰거나 같은 변이를 자동 재시도하지 않았다.
- Material author-E는 공식 ObjectTools의 UObject 인자에 패키지 경로를 넣어 setter 실행 전 파라미터 변환에서 실패했다. 생성된 Material/MPC를 읽기로 조사하고 빈 MPC 기본값을 확인한 뒤 별도의 명시적 저장으로 두 테스트 에셋을 보존했다(`material-partial-E-reconciliation.json`). 원래 setter를 자동 재시도하지 않았으며 author-E는 실패로 유지한다.
- Material author-E2는 연결 없음의 공식 반환값이 문자열 `"None"`임을 검수기가 처리하지 못해 실패했다. 실제 연결을 읽기로 확인하고 별도 저장한 뒤 새 E3 fixture에서 검수했다. E3 첫 독립 검수는 Windows 경로 구분자 비교 오류로 실패했고 경로 정규화 후 R2 검수가 통과했다. 원래 실패 기록은 유지한다.
- MCP 없는 Material runtime fixture-A는 clone 저장·cube 생성 뒤 비노출 getter 때문에 실패했다. 원본/복사 12개 파일은 보존했고, public property readback으로 교정한 새 fixture-B와 cold-C/Shipping-B는 통과했다. 최초 실패는 유지한다.
- I edges 첫 검수와 native 중복 경로 검사기는 실제 오류 문구를 예상하지 못해 변이 전에 실패했다. 문구 검수를 교정한 I2는 통과했다. 별도 compiler wrapper 첫 호출은 필수 name/parent_class 누락으로 스키마에서 거부됐고, asset 없음/저널 없음/dirty 없음의 읽기 조사를 보존했다. 새 이름의 I2 fixture에서 정확한 공개 스키마로 세 wrapper를 검증했다. 원래 수정 요청은 재실행하지 않았다.
- 대상 파일 보존 검수의 첫 기록은 기준선 root를 잘못 적용한 검사 오류여서 무효로 표시했다. 교정한 v2는 1,059개 중 의도한 `.uproject` 외 변경 0개다. 원본 게임 콘텐츠·RetargetTools·기존 시작 Python을 수정하지 않았다.
- M 후 재검사도 원래 보호 파일 1,059개 중 승인된 `.uproject` 변경 외 차이 0개다(`target-preservation-after-M.json`).
- 전체 기능 대장 4,764행은 Python·명령·핸들러·반영 API·스크립트의 중복되는 소스 표면이다. 고유 기능 수나 완료율로 합산하지 않는다.
- `native:ping`만 구형 TCP 응답 전용임을 소스로 확인해 제거 분류를 확정했다. 공식 도구로 완전히 동등 대체됐다고 확정한 기능은 아직 없다.
- Material 등 다음 도메인, 나머지 그래프/배치/특화 기능, 대부분의 파이프라인·검수기는 미완료다. 복사, 등록, 범용 실행 가능성으로 완료 처리하지 않는다.

Git 초기화·remote 변경·커밋·푸시는 수행하지 않았다.
