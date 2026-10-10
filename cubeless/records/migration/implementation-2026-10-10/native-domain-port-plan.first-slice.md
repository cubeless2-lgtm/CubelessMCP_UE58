# Material 첫 구현 범위와 native 계약

2026-10-10. 도메인 계획 검토 후 root가 승인한 첫 범위는 **9개**다. 실제 UHT/C++ 첫 빌드 E는 통과했다(UBT 20.34초, 실행 action 12.87초; `cubeless/records/verification/implementation-2026-10-10/build-material-E.log`). 이 기록 시점의 공식 MCP 등록·실행 검증은 아직 대기 중이다. 전체 Material 21개 중 나머지 12개와 다른 도메인은 미완료다.

읽기 5개: resolve_material_graph, list_material_nodes, analyze_material_graph, list_material_collection_parameter_nodes, get_material_parameter_collection_values.

변경 4개: connect_material_nodes, connect_material_property, delete_material_node, layout_material_nodes.

## 파일과 역할

모두 `Plugins/CubelessMCP_UE58/Source/CubelessMCPUE58Editor/` 아래다.

- `Private/Registration/CubelessMaterialLibrary.h/.cpp`: `UCubelessMaterialLibrary : UBlueprintFunctionLibrary`. 개별 정적 BlueprintCallable+AIIgnore 함수 9개. 이 클래스는 ToolsetDefinition이 아니며 Registry에 직접 등록하지 않는다. 실제 MCP 도구는 Python의 명명된 9개 래퍼다.
- `Private/Operations/CubelessMaterialOperations.h/.cpp`: 게임 스레드·PIE·예산·대상·재진입 검사, undo 구간, 변경 전후 상태, 결과·오류·후조건 검수. durable journal은 공개 Python 래퍼가 소유한다.
- `Private/Compatibility/UE58/CubelessMaterialApi.h/.cpp`: 실제 engine API, asset/expression class 검사, snapshot, 노드·property 연결/삭제 readback, 관측 delta.
- `Private/Compatibility/UE58/CubelessMaterialLegacy.inc`: 원본의 필요한 helper closure 61개 / 2,428줄. 원본 경로와 SHA-256, 기존 주석 보존. 한 cpp에서만 include한다. compile/save/cache repair/ticker/생성/property 쓰기 핸들러는 포함하지 않는다.
- Build.cs 변경: Editor 모듈 private dependency에 MaterialEditor, RHI 추가. 원본의 Sockets, Networking, HTTP, Niagara Internal include path 및 다른 사용자 플러그인은 가져오지 않았다.

원본 Material cpp SHA-256은 `8c41e028d0f6b27e0d5c2d85cad37216bab8d2234e61189c812e6a463b0d5a91`이다. 정확한 원본 파일·추출 symbol 범위는 `native-domain-port-plan.extraction.json`에 기록했고 추출 후 원본 해시가 동일함을 확인했다.

## Python에서 호출할 정확한 함수

클래스는 `unreal.CubelessMaterialLibrary`, 모든 반환은 JSON 문자열이다. Python compatibility는 위치 인자로 호출한다.

```text
resolve_material_graph(material_path, graph_type, max_graph_nodes)
list_material_nodes(material_path, graph_type, node_type, desc_contains, include_pins, max_graph_nodes)
analyze_material_graph(material_path, graph_type, include_nodes, include_usage, max_referencers, max_graph_nodes)
list_material_collection_parameter_nodes(material_path, graph_type, max_graph_nodes)
get_material_parameter_collection_values(collection_path, parameter_names, include_asset_defaults, include_runtime)
connect_material_nodes(material_path, from_node_id, to_node_id, to_input, from_output, graph_type, max_graph_nodes)
connect_material_property(material_path, from_node_id, material_property, from_output, max_graph_nodes)
delete_material_node(material_path, node_id, graph_type, max_graph_nodes)
layout_material_nodes(material_path, graph_type, max_graph_nodes)
```

graph 예산은 native 1..10000 허용, 공개 Python 래퍼 기본 2048이다. collection은 scalar/vector 합계 2048개 한도로 검사한다. 이 제한은 원본과 다른 명시적 범위 제한이며 초과 시 수정 전에 실패한다. 변경은 해석된 `/Game/` Material/Function만 허용하고 engine/plugin mount, PIE/SIE, MCP/editor 전용 expression class를 거부한다. 읽기는 진단을 위해 engine/plugin 에셋도 허용한다.

## 결과와 검증 범위

항상 `success`(bool), `data`(object), `stage`, `mutation_attempted`, `change_state_known`, `postcondition_passed`, `compile_status`, `save_status`, `implicit_compile_possible`, `atomic_rollback=false`, `auto_retry_safe`, `limits`, `journal_owner`, `validation_scope`를 반환한다. 관측 가능한 경우 `changed`가 있고, 실패 시 `error_code`, `error`가 추가된다.

대상 capture 이후에는 `asset_path`, `before`, `dirty_before`를 포함한다. 실행 후 readback이 가능하면 `after`, `dirty_after`, `delta`, `verification`을 기록한다. 초기 preflight 실패에는 이 필드들이 없을 수 있다. 변경 후 readback에 실패하면 실제 변경을 false로 단정하지 않고 `changed`를 제거하며 `change_state_known=false`로 반환한다. Python은 이 상태를 journal에 보존한 다음 오류를 반환한다.

`data`에는 원본 도구 상세 결과를 보존한다. `before/after`는 expression node JSON·연결·좌표·material root property 연결의 snapshot이다. `delta`의 created/removed/updated node 수는 이 snapshot에서 비교한 개수이며 asset 바이너리 전체 변경량이 아니다. root property 연결 변화는 `material_property_connections_changed`로 별도 기록한다.

- 연결: 엔진 FExpressionInput의 실제 expression pointer와 output index를 재조회한다.
- 삭제: 해석된 대상 node_key 부재, node 수 1개 감소, 남은 expression/root property가 삭제된 expression을 참조하지 않는지 확인한다.
- layout: 좌표 readback, node 집합과 topology 보존을 확인한다. 미적 배치 품질을 검증했다고 주장하지 않는다.
- 읽기: 로드 후 capture한 graph 상태가 조회 동안 바뀌지 않았는지 확인한다. 최초 asset load 전 상태나 숨겨진 전체 UObject 상태를 증명하는 검사는 아니다.

명시적 compile/save API는 호출하지 않는다. 원본 `MarkChanged/PostEditChange`와 MaterialEditingLibrary 통지가 엔진 내부 shader/UI 갱신을 시작할 수 있으므로 `compile_status=not_requested`, `implicit_compile_possible=true`를 함께 보고한다. shader 완료·render 결과·저장·재로드·패키징 통과는 이 9개 helper의 후조건에 포함하지 않는다.

native undo는 현재 동기 mutation 구간에만 적용한다. root asset과 expression subobject를 변경 전에 Modify(false)하여 transaction 상태를 잡는다. atomic rollback을 제공하지 않는다. 내부 직접 helper 호출에는 durable journal이 없으므로 공식 도구 사용은 Python 래퍼를 거쳐야 한다.

## 남은 검증

UHT/C++ 빌드는 통과했다. Python binding명 확인, 공식 MCP 검색·describe·9개 각각의 성공/오류 호출, 모호 경로·GUID 충돌·budget 초과·PIE 거부·부분 실패, snapshot과 실제 그래프 비교가 남아 있다. 사용자가 작업한 에셋 대신 별도 fixture에서 실행한다. 자료만 추출한 기존 12개 도구는 등록하거나 완료로 집계하지 않는다.
