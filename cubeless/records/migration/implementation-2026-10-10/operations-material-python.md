# Material 9개 Python Toolset와 검수 파이프라인

2026-10-10. 이 기록은 원본 Material 21개 공개 도구 중9개의 Python 등록·작업 로직·UE58 호환 호출과 별도 검수 Python 구현 범위를 설명한다. **9개는 구현·등록 및 실제 작은 Material fixture의36단계 author와 새 프로세스7단계 cold reload 검증을 통과했다. 모든 기능 분기의 동등성은 별도 증거로 판정한다.** 실제 실행 증거는 root가 실행한 보고서와 로그다.

## 출처와 입력 계약

원본 `Python/tools/material_tools.py`의 공개 함수, `legacy-runtime-registration.eager.json`의 실제 FastMCP catalog와 `UnrealMCPMaterialCommands.cpp`의9개 handler를 대조했다. wrapper golden test `Python/scripts/analysis/test_material_tools_response_schema.py`의 연결 부재/응답 부재/예외/원본 명령 인자 전달 검수도 읽었다. 새 구조는 외부 TCP 전달을 제거하므로 transport 전용 오류 대신 native binding 부재, 구조가 잘못된 native 결과, 에셋/선택자/preflight/변경/readback 단계 오류를 구분한다.

새 Toolset은 `cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools`다. 원본 함수 이름을9개 명명 도구로 유지한다. `registration/__init__.py`에서 공식 Toolset Registry 등록 클래스 목록에 추가했다. 범용 executor를 공개하지 않았다.

| 공개 도구 | 기존 필수 입력 | 이번 상태 |
| --- | --- | --- |
| resolve_material_graph | material_path | 구현, 등록 확인 |
| list_material_nodes | material_path | 구현, 등록 확인 |
| analyze_material_graph | material_path | 구현, 등록 확인 |
| list_material_collection_parameter_nodes | material_path | 구현, 등록 확인 |
| get_material_parameter_collection_values | collection_path | 구현, 등록 확인 |
| connect_material_nodes | material_path, from_node_id, to_node_id, to_input | 구현, 등록 확인 |
| connect_material_property | material_path, from_node_id, property | 구현, 등록 확인 |
| delete_material_node | material_path, node_id | 구현, 등록 확인 |
| layout_material_nodes | material_path | 구현, 등록 확인 |

root의 실제 `material-discovery-E2.json`에서9개 모두 기존 필수 입력 집합과 일치함을 재검사했다. 초기 E discovery는 짧은 이름으로 조회한 오류이므로 정상 등록 근거로 쓰지 않았다. mutation4개에만 nullable `operation_id`를 추가했다. 기존 빈 문자열 기본값은 UE58 schema generator가 required로 만드는 문제가 있어 등록 경계에서 `str | None = None`을 쓰고 None을 원본 빈 문자열로 정규화한다. 적용 대상은 node_type, desc_contains, 두 연결 도구의 from_output이다. operations/native 기본값 의미는 유지한다. **실제 schema는 nullable annotation이어도 type:string/default:null을 보이므로 명시적 JSON null은 아직 검증하지 않았다. 파이프라인은 해당 인자를 생략한다.**

## 파일과 책임

- `Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/registration/material_toolset.py`: 이름·설명·reflected 입력·`CubelessPythonOperationResult` 반환만 담당한다.
- `operations/material.py`: 독립된 각 작업 진입점, durable journal, before snapshot, 실패와 실제 변경 결과를 관리한다.
- `compatibility/ue58/material.py`: 실제 `unreal.CubelessMaterialLibrary`의9개 정적 C++ wrapper를 정확한 positional 인자로 호출하고 JSON envelope를 검사한다. 현재 UE5.8.3만 지원한다.
- `operations/state.py`: root 승인으로 `checkpoint(result)`를 좁게 추가했다. 기존 동기 mutate/replay 완료 순서는 유지한다. 이 API를 비동기 작업에 그대로 사용하는 것은 지원하지 않는다.

Native helper는 C++ 담당 소유이며 직접 공식 MCP 도구로 공개하지 않는다. Python이 받은 native data는 data_json root에 두고 native envelope는 data.native_outcome, before/after는 data.before/after로 보존한다. 변화 여부가 불명확하면 None/unknown 상태를 보존하며, 추정 delta 수치를 만들어내지 않는다.

## 변경·검수·실패 계약

수정 작업은 전체 그래프 snapshot → 실제 canonical `/Game/` 경로 확인 → 디스크 before checkpoint → native mutation → native before/after·dirty·change_state_known 반영 → postcondition_passed 확인 → 최종 journal 순서다. 원본보다 좁은 현재 제한으로 Engine/Plugin 마운트의 Material은 수정하지 않는다. `/Game/CubelessMCPTests` 같은 폴더 이름을 문자열 `cubelessmcp` 포함만으로 거부하는 기존 parent-class guard는 에셋 경로에 사용하지 않는다.

그래프 최대 표현식2048개를 내부 budget으로 설정하고 초과 시 변경 전에 거부한다. 작은 slice의 한계이며 모든 대규모 graph 작업 이식 완료를 뜻하지 않는다. 에셋은 공식 game-thread 호출 경로에서 직렬 수정하고, Python journal이 중복/재진입을 차단한다.

checkpoint는 active ID·started 상태·현재 process 소유권을 확인하고 fsync 후 atomic file replace로 저장한다. 중단된 기록의 `journal_observation`은 저장 당시 stage/sequence와 실제 이후 결과 unknown을 구분한다. `changes=[]`였던 before checkpoint도 “실제로 아무것도 바뀌지 않았다”는 증거가 아니다. checkpoint 쓰기 실패 뒤에는 native mutation을 실행하지 않는다. 완료 기록 없는 동일 ID는 자동 재실행하지 않는다. atomic rollback은 제공하지 않는다.

9개 도구는 **명시적인 compile/save를 호출하지 않는다**. PostEditChange 등에서 엔진이 shader update를 시작할 수 있으므로 compile.implicit_compile_possible를 별도로 기록한다. shader 완료, save, cold reload, 렌더링, packaged runtime을 자동으로 성공 처리하지 않는다. Native success만으로 mutation 검증 성공을 선언하지 않고 postcondition_passed=true를 요구한다. 예외/실패에서도 native stage/error, 후보 경로, before/after, dirty와 실제 변경 여부를 먼저 반영한다.

## 독립 외부 검수 파이프라인

`cubeless/tools/pipelines/material_graph_smoke.py`는 공식 MCP client로 실행하는 새 검수 harness다. 원본9개 기능의 검수 계약을 확장한 것이며 원본 모든 Material 파이프라인을 이식했다고 주장하지 않는다. 별도 서버, private socket, arbitrary Python execution 도구는 추가하지 않는다.

입력은 official endpoint, 정확한 isolated project, author/reload phase, 새 output 파일이다. 각 run은 `/Game/CubelessMCPTests/Material_<32hex>/` 아래의 Material·MPC와4개 표현식만 사용한다. author 단계는36개 순서다.

1. 실제 schema 검색과 project/version/game-thread/PIE/dirty preflight.
2. 공식 MaterialTools로 fresh Material/MPC와 Constant2·Add·CollectionParameter 생성. 공식 ObjectTools로 scalar/vector 기본값과 expression 속성 지정. 기존 에셋이면 덮어쓰지 않는다.
3. resolve/list/analyze/collection-node/value 조회. node_key는 실제 목록 결과에서 취한다.
4. A/B 연결, 같은 operation_id replay, 공식 MaterialTools.get_expression_inputs로 독립 연결 검수.
5. 잘못된 pin 연결 거부와 before/after graph 동일성 확인.
6. root BaseColor 연결, 공식 get_property_input 검수, layout, 연결된 Constant 삭제, 공식 input readback으로 끊김 확인.
7. 배치 종료 뒤 공식 MaterialTools.recompile **명시 호출1회**, 정확히 두 fixture만 AssetTools.save_assets **호출1회**, graph와 dirty 재검수.
8. root가 별도 새 editor를 시작한 뒤 reload phase에서 instance 변경, saved graph 전체 일치, MPC 기본값, root wiring, clean 상태를 확인한다. 파이프라인은 editor를 시작/종료하지 않는다.

여기서 공식 recompile/save를 호출하는 것은 fixture 검수 방법이며 기존 `compile_and_save_material`의 전체 입출력·오류·비동기·journal 계약을 동등하게 대체했다고 분류하지 않는다. setter나 factory가 implicit shader work를 발생시킬 수 있으므로 전체 shader compile 횟수가1이라고 주장하지 않는다.

`MaterialEditingLibrary.cpp:1035–1059`에서 RecompileMaterial은 현재 RHI resource의 GetCompileErrors를 반환하지만 resource가 없어도 빈 배열을 반환하고 IsCompilationFinished를 확인하지 않는다. 따라서 기존 E3 raw report의 `compile.passed=true`는 **공식 호출이 현재 보고된 오류 없이 반환했다는 뜻**으로 제한한다. `shader_completion_verified=false`이며 NullRHI 검수를 셰이더 완료나 렌더링 품질 증명으로 쓰지 않는다. 원본 raw 실행 기록은 수정하지 않고 이 계약과 보정 기록에 의미를 명시한다.

실제 확인한 설치 소스는 EditorToolset `toolsets/material.py`, `object.py`, `asset.py`; MaterialEditor `MaterialEditingLibrary.h`; Engine/Public/Materials의 MPC·Constant·CollectionParameter 헤더; ToolsetRegistry의 `ReferenceConverter.cpp`, `ToolsetLibrary.cpp`다. UObject 입출력 `{refPath:...}`, 실제 property 이름/기본값, Material recompile의 오류 전파를 확인했다. 공식 schema는 `operations-material-official-schemas-C.json`에 있다. A/B는 editor 미기동으로 connection refused였으며 재시도 성공/엔진 실행 증거가 아니다.

실행 E에서 create 결과의 객체 경로 대신 패키지 경로를 ObjectTools에 전달한 검수 harness 오류를 확인했다. `report.assets`는 exists/native 경로/저장용 패키지 경로이고 `report.objects`는 생성 응답의 실제 full refPath다. 공식 UObject 입력과 Collection 참조는 모두 후자를 사용한다. E 보고서는 실패로 보존하고 root의 `material-partial-E-reconciliation.json`은 별도 읽기 검수와 부분 fixture 저장 근거로만 사용한다. 동일 실패 setter를 자동 재시도하지 않았다.

실행 E2에서는9개 Cubeless 도구의 정상 호출과 invalid pin·operation replay까지 통과했으나 독립 삭제 연결 검수기가 엔진의 null-object 직렬화를 잘못 예상하여 중단했다. 실제 B.expression은 JSON null이 아니라 문자열 `"None"`이다. 설치 소스 `ReferenceConverter.cpp:214`는 null UObject에서 custom 결과를 반환하지 않고, `ToolsetJson.cpp:174` → `JsonObjectConverter.cpp:350` → `PropertyBaseObject.cpp:282`의 ExportTextItem fallback으로 정확히 이 문자열을 만든다. 검수 harness/mock/독립 검수기는 **정확한 `"None"`만** 연결 없음으로 허용하고 빈 문자열·다른 대소문자·null 문자열·빈 refPath·필드 누락을 거부한다. E2는 실패 기록 그대로 보존한다. root가 수행한 별도 `material-partial-E2-reconciliation-R2.json`의 compile/save는 E2 파이프라인 통과로 합산하지 않는다.

독립 검수기는 `cubeless/tools/verification/material_pipeline_report.py`다. 부모 status 하나가 아니라 필수36/7단계의 순서·판정, mutation 검증 상태, retry, compile/save 실제 단계, 실패핀 before/after, fixture identity, saved graph, 새 instance와 author 실제 파일 SHA를 확인한다. 기록 파일의 실행 진위 자체는 live MCP/엔진 로그 증거에 의존한다.

E3의36단계 실제 엔진 author는 통과했다(`material-author-E3.json`). 최초 독립 검수는 Windows 경로 구분자 표기 차이를 동일 파일로 인식하지 못해 실패했다. 검수기를 Path.resolve 비교로 수정하고 대소문자·구분자 회귀 테스트와 다른 프로젝트 거부를 추가했다. 기존 author를 그대로 읽은 `operations-material-author-E3-independent-R2.json`은 통과했으며 엔진 수정 작업을 다시 실행하지 않았다. 디스패처 `material-author-evidence`도 해당 독립 검수기를 새 subprocess로 실행하여 통과했다. 원본42개 gate의 완료 상태에는 반영하지 않는다.

성능은 UE5.8.3-58210709, isolated CubelessMCPVerification 프로젝트, localhost:8001 공식 MCP, Material1·MPC1·표현식4개에서3개로 삭제하는 작은 fixture의 단일 관측이다. author 전체2.0154초, HTTP 관측45개, 명시 compile1회 약0.0756초, 정확한2개 fixture save1회 약0.0664초였다. HTTP/JSON/보고서 처리 시간을 포함하고 editor 기동 시간을 제외한다. shader implicit work·완료 대기 전체 시간을 분리 측정한 수치가 아니며 이전5.6 대비 개선율이나 반복 benchmark로 해석하지 않는다.

새 PID21900의 `material-reload-E3.json`은 다른 instance에서7단계 cold reload를 통과했다(0.5028초, HTTP 관측16개, compile/save0회). 실제 author 파일 SHA를 대조한 `material-reload-E3-independent.json`과 디스패처 `operations-dispatcher.material-E3.json`도 통과했다. 이 보고서들은 `cubeless/records/verification/implementation-2026-10-10/`에 있다. 다음 runtime 독립 fixture는 별도 작업으로 다루며 이 editor 재로드를 packaged runtime 증거로 쓰지 않는다.

```powershell
python -B cubeless/tools/pipelines/material_graph_smoke.py --endpoint http://127.0.0.1:8001/mcp --expected-project 'C:/path/to/isolated.uproject' --phase author --output cubeless/records/verification/NEW-material-author.json
python -B cubeless/tools/verification/material_pipeline_report.py cubeless/records/verification/NEW-material-author.json --phase author --output cubeless/records/verification/NEW-material-author-verification.json
# root-owned new editor process 이후
python -B cubeless/tools/pipelines/material_graph_smoke.py --endpoint http://127.0.0.1:8001/mcp --expected-project 'C:/path/to/isolated.uproject' --phase reload --parent-report cubeless/records/verification/NEW-material-author.json --output cubeless/records/verification/NEW-material-reload.json
python -B cubeless/tools/verification/material_pipeline_report.py cubeless/records/verification/NEW-material-reload.json --phase reload --parent-report cubeless/records/verification/NEW-material-author.json --output cubeless/records/verification/NEW-material-reload-verification.json
```

## 수행한 offline 검수와 미완료

journal10개, Material Python14개, fake MCP pipeline10개를 통과했다(`operations-material-pipeline.tests-F.json`). fake pipeline은 실제 캡처한 공식/Cubeless schema를 사용해 full author/reload, compile 오류 후 save 금지, 잘못된 pin에 변경이 발생한 경우 거부, postcondition 누락, 동일 editor reload, 위조된 요약/누락 단계, package 경로를 객체로 전달하는 오류, Windows 프로젝트 경로 동일성을 검수한다. 독립 검수기는 stage.passed를 유지한 채 MPC 값·parameter ID·공식 A/B/root readback을 위조한 사례도 거부한다. tests-B는 테스트 추가 위치 오류로 실패했고 수정 후 C/D/E/F에서 통과했으며 이 실패 기록도 보존한다. **합성 테스트는 native 엔진 API 호출 검증이 아니다.**

남은12개는 add_material_node, add_custom_material_node, set_material_node_property, compile_and_save_material, refresh_material_cached_expression_data, expand_material_function_calls, mirror_material_parameter_collection, set_material_parameter_collection_values, set_material_parameter_collection_sync, replace_material_collection_references, replace_material_collection_parameters, replace_material_texture_references다. 관련 현재 후속 설계가 있어도 이 기록에서는 구현·검증 완료로 처리하지 않는다.

9개 내부의 Material Function/Instance 분기, 대규모 그래프, custom/HLSL, 렌더 결과와 전체 runtime 의존성 검증도 별도 근거가 필요하다. 공식 schema 조회만으로 호출/동작 검증 상태를 올리지 않았다. 원본/엔진/게임플레이 C++/사용자 에셋/Git을 변경하지 않았다.
