# 도구 중복과 반복 연산 최적화 조사

기존 공개 계약을 보존하면서 구현과 반복 작업을 줄이는 것이 목표다. 이 조사는 문서만 변경했고 원본, 엔진, 에셋, 등록부, 기능 대장 행은 변경하지 않았다. 실제 core 변경과 빌드·등록 검증은 root가 별도 담당자에게 배정한다.

## 전수 표면 대조 범위

동명 JSON은 원본 Python 공개 정의 436개, native 명령 388개, reflected 선언 15개를 원래 ID로 모두 보존한다. 공개 함수 436개의 실제 AST/호출/같은 파일 내부 helper 연결을 조사했으며 원본 해시는 최초 inventory와 일치한다. native 조건 분기에서 직접 반환 handler를 찾은 항목은 356개이고 나머지도 미해결 행으로 유지한다. JSON에는 각 명령의 public candidate와 실제 literal 호출 위치가 있다.

436과 388의 차이 48은 중복 개수가 아니다. 실제 FastMCP eager 등록은 433개이며 discovery meta 3개와 노출 모드가 구분된다. 내부 route, reflected helper, host-only composite가 서로 다른 표면에 있다. 독립 identity를 유지해야 하며 합산도 고유 기능 수가 아니다.

같은 args/body AST가 완전히 일치하는 공개 함수 쌍은 없었다. 이것이 의미적 중복 부재를 증명하지는 않는다. 같은 파일 helper까지 추적한 native literal 명령 공유군은 17개다. 공개 함수가 다른 공개 함수를 이름으로 직접 호출한 뿌리는 5개이며, imported helper·문자열/변수 dispatcher까지 포함한 모든 composite의 수는 아니다. 예를 들어 Niagara recipe들은 `*_impl` helper를 통해 여러 명령을 순서대로 호출하고, texture-set pipeline은 import→material 생성→mesh 적용과 단계별 부분 실패 보고를 묶는다. 이런 흐름은 단순 도구가 존재해도 제거 대상이 아니다.

현재 대응은 **G 실제 등록 28개**를 기준으로 기록했다. 공식 카탈로그의 52개는 toolset 수이며 모든 개별 도구의 동등성 검수 수가 아니다. 현재 구현과 관련된 공식 Python 함수는 설치된 소스에서 정확한 이름·인자·decorator를 확인해 JSON에 매핑했다. 나머지 legacy의 공식 대체 가능성은 미검수다.

## 확정된 관계와 유지할 차이

| 관계 | 실제 근거 | 결정/남은 검수 |
| --- | --- | --- |
| `native:create_actor` / `native:spawn_actor` | 원본 `UnrealMCPEditorCommands.cpp:2357`에서 동일 `HandleSpawnActor(Params)` 호출. create_actor만 deprecation 로그를 기록 | 같은 작업 core를 공유하는 확정 alias. raw/batch caller 호환과 로그 차이는 보존할 것. UE58 actor 기능 자체는 아직 이 조사로 구현 완료되지 않음 |
| Blueprint compile 공개 3종 | 원본 `UnrealMCPBlueprintCommands.cpp:2868/2887/2910`에서 같은 compiler helper를 서로 다른 save/refresh 값으로 호출 | 기존 공개 이름 유지. 현재 `registration/toolsets.py:78/89/102`도 공통 operations core를 호출하므로 이미 구현 중복을 줄임 |
| 새 native `CompileBlueprintValidation` / Python compile 3종 | G Registry에는 native와 Python 모두 노출. native 직접 호출에는 Python durable journal을 거치지 않음 | root가 native helper를 `AIIgnore`로 숨기는 변경을 별도 위임. Python 내부 호출과 공개 3종/기본값/배치 entry는 유지. H 빌드·Registry 제외·호출 증거 전에는 27개 등록 완료로 표시하지 않음 |
| Native diagnostics / Python health / get_operation | editor/dirty/plugin 상태, Python 등록 상태, durable operation 상태로 관찰 대상이 다름 | 이름이 진단이라는 이유로 합치거나 삭제하지 않음 |
| 배치 / 단일 작업 도구 | 같은 action core를 사용하지만 배열 전체 admission, 단계별 결과, stop_on_error, checkpoint/partial mutation 계약은 배치 고유 | 배치 독립 기능 유지. 단일 도구들을 범용 실행기 하나로 대체하지 않음 |
| Material resolve/list/analyze/MPC | 이름/그래프 순회가 겹쳐도 ambiguous resolution, pins/filter, usage/referencers, typed effective collection 값으로 출력과 오류가 다름 | 내부 read context 재사용 후보. public 결과를 하나로 축소하는 근거는 없음 |

## 현재 구현에서 줄일 수 있는 반복 작업

1. **Material read query 중복.** `Operations/CubelessMaterialOperations.cpp:59/87/101`은 모든 호출에서 CaptureState→Execute→CaptureState를 수행한다. `Compatibility/UE58/CubelessMaterialApi.cpp:212`의 CaptureState도 graph summary를 만들며 ListNodes의 Execute는 graph를 다시 조회한다. Python `operations/material.py:82`의 pre-mutation snapshot용 list 호출만으로 이런 read가 반복되고 이어 mutation의 before/after readback이 추가된다. `include_pins=False`여도 snapshot은 GraphSummaryToJson(..., true)를 사용한다. 단일 요청의 동일 시점 resolved target/summary에서 read result를 projection하면 Execute 측 재순회를 줄일 수 있다. **독립 after readback과 durable before checkpoint는 남긴다.** 기존 read 결과와 before/after, dirty, limit, error envelope까지 동일해야 한다.
2. **MPC typed chain 반복 조회.** Api.cpp:222의 GetCollectionValues CaptureState와 Execute:433, after CaptureState에서 effective values를 반복 계산한다. ListCollectionNodes도 CaptureState:270/275에서 direct metadata와 effective nodes를 계산하고 Execute:432에서 다시 계산한다. 한 호출 안에서 bounded base-chain view를 공유하고 값/type/owner를 projection하는 안이 적합하다. cycle/budget/type mismatch 검사는 유지하고 after는 새 관찰로 남긴다. 서로 다른 tick의 world/collection 값을 캐시해 read-only 변경 감지를 없애면 안 된다.
3. **Python mutation preflight와 native before.** 앞의 checkpoint snapshot과 실제 mutation 직전 snapshot은 목적이 다르다. 무조건 하나를 제거하지 않는다. 향후 내부 prepared-state token을 쓰려면 editor instance, UObject identity, generation/fingerprint, 단일 소비, 같은 게임 스레드 실행 구간을 검증해야 한다. 외부에서 받은 JSON을 engine before-state로 신뢰하지 않는다. 이는 더 큰 설계 변경이므로 첫 최적화 대상은 1·2의 순수 read 재사용이다.
4. **Blueprint graph 노드 검색 반복.** `compatibility/ue58/graph.py:58`의 find_node는 호출마다 list_all_nodes를 수행한다. `operations/graph.py:153` 이후 connect는 source/target 검색, 전체 before, 전체 after, source/target 재검색을 한다. 한 phase 안에만 node_id→UObject 인덱스를 만들면 선형 검색 횟수를 줄일 수 있다. mutation 뒤에는 새 인덱스로 재조회하고 node identity, 양방향 연결, dangling-link 검수는 유지한다. add/delete 사이에 캐시를 재사용하지 않는다.
5. **컴파일·저장.** 현재 3개 Blueprint wrapper는 모두 공통 core에서 compile 1회와 성공 후 요청된 save 1회를 수행한다. Material 11개 helper에는 명시 compile/save 호출이 없다. PostEditChange/Create API의 암묵적 shader 작업은 별도 부작용이다. 배치가 받은 compile/save 명령을 자동 삭제하거나 합치면 실패 경계가 바뀌므로 금지한다. 상위 pipeline을 작성할 때 편집→검수→마지막 compile/save로 설계한다.

1·2의 구현을 먼저 하고 traversal/serialization 호출 수를 계측하는 것이 권장 순서다. 3은 durable lease 설계까지 필요해 후속 작업으로 둔다. 읽기 최적화가 mutation 검수 수준을 낮춰서는 안 된다.

## 공식 도구 대체는 아직 동등성 미확정

| 기존/Cubeless 기능 | 설치 소스에서 확인한 공식 후보 | 차이와 검증 계획 |
| --- | --- | --- |
| Blueprint 생성 | `BlueprintTools.create` (`blueprint.py:177`) | folder/name/class→UObject와 Cubeless package/name/parent→structured result 차이. duplicate asset/directory, empty/bare/class 경로, editor-only 부모, 미저장 dirty, exception/error 변환, operation replay를 대조 |
| Blueprint compile 3종 | `BlueprintTools.compile_blueprint` (`:199`), `toolset_registry/helpers.py:29`, `AssetTools.save_assets` (`asset.py:330`) | 공식 함수는 None/exception과 node ErrorMsg, Cubeless는 native compiler tokenized diagnostics·save/refresh·durable retry 결과. warning·error·refresh·save failure·partial state를 각각 검수. 단순 같은 이름으로 대체 불가 |
| Graph list/edit | `get_graph/find_nodes/get_node_infos`, `create_node/delete_node/connect_pins/set_node_position` | NodeInfo/PinID와 legacy GUID 또는 현재 object path 계약 차이. comments·동명 graph/node·typed conversion·기존 링크 교체·양방향 link·batch checkpoint 검수 필요. 현재 Branch/exec 제한은 full legacy graph parity가 아님 |
| Material node add/set/list/connect/delete/layout | `MaterialTools.add_expression/get_expressions/connect_expressions/connect_to_output/delete_expression/layout_expressions`, `ObjectTools.set_properties` | 공식은 UObject 반환/None/exception과 editor refresh. Cubeless의 resolver/filter/strict persistent property/reference guard/structured before-after/partial failure/replay와 같지 않음. pins/ambiguous selector/기존 연결/invalid property/type/MPC/MaterialFunction 등 fixture 필요 |
| Material compile/save 단계 | `MaterialTools.recompile` (`material.py:486`) + `AssetTools.save_assets` | E3 pipeline에서 실제 호출됨. 빈 현재 오류 배열을 shader 완료로 읽지 않음. compile/save 단계의 활용 근거이며 모든 legacy Material compile 도구의 완전 대체 선언은 아님 |

공식 함수를 내부에서 활용하는 것과 기존 공개 도구의 동등 대체는 다른 판정이다. 원본 입력·반환·오류·dirty/transaction·compile/save 횟수·실패 후 실제 변경·readback/reload를 고정 fixture에서 비교한 뒤에만 대체 분류를 닫는다. 이 조사에서 새로 확정한 공식 완전 대체는 0개다.

## 검증과 측정

Batch author G의 동일 작업량 비교는 create→compile/save→inspect 3개 action으로 양쪽 명시 compile/save 각 1회다. batch HTTP 1회와 단일 호출 3회를 측정했지만 한 번씩, batch가 먼저여서 캐시·순서 효과가 있다. 실제 값은 `batch-author-G.json.performance`에 있으며 안정적 속도 향상을 주장하지 않는다. 전체 author는 stop/replay/graph 반례 때문에 별도 compile/save가 더 있으며 비교 구간과 섞지 않는다.

최적화 후에는 작은/중간/상한 근접 그래프의 노드·핀·MPC chain 수, 실제 full traversal/JSON 직렬화 수, dirty 전후, 명시·암묵 compile 구분, elapsed/response bytes를 같은 workload에서 기록한다. 정상/오류/no-op/중복 ID/충돌/배치 stop과 fresh-process reload를 다시 검증한다. 성능 개선이 없어도 검수 기준은 낮추지 않는다.

동명 JSON의 모든 surface는 아직 `semantic_duplicate_status`와 실제 관찰을 분리한다. 실행되지 않은 분기, imported helper, embedded Python, 모든 52 official toolset의 schema/equivalence는 미완료다. 이번 문서만으로 기능 삭제, legacy 전체 동등성, 전체 이식 완료를 선언하지 않는다. 활성 기준은 `cubeless/codex/policies/tool-optimization.md`다.
