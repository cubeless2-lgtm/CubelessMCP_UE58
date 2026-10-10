# Material add/set Python 경로 F

2026-10-10. 원본 `Python/tools/material_tools.py:303`, `:399` 및 실제 FastMCP 등록 스키마를 확인하여, 새 `CubelessMaterialLibrary`의 hidden native Add/Set을 기존 Material Python 구조에 연결했다. **현재 소스·합성 검증 단계다. 실제 UE58 등록·호출·저장·재로드는 이 기록의 증거에 포함되지 않는다.** 기존 E3 에셋/실행 기록은 수정하지 않았다.

## 정확한 공개 입력

```python
CubelessMaterialTools.add_material_node(
    material_path: str,
    expression_class: str,
    graph_type: str = "auto",
    node_position: list[int] | None = None,
    selected_asset: str | None = None,
    properties_json: str | None = None,
    operation_id: str | None = None,
) -> CubelessPythonOperationResult

CubelessMaterialTools.set_material_node_property(
    material_path: str,
    node_id: str,
    property_name: str,
    value_json: str,
    graph_type: str = "auto",
    operation_id: str | None = None,
) -> CubelessPythonOperationResult
```

원본과 다른 reflected 입력은 `properties: dict` → `properties_json: str`, `value: Any` → `value_json: str`이다. 예: `properties_json='{"R":0.125,"Desc":"source"}'`, 숫자 변경은 `value_json='0.875'`, 문자열 변경은 `value_json='"caption"'`다. 실제 공식 Registry가 가변 Any map을 지원한다고 추측하지 않는다.

생략/null `node_position`은 `[0,0]`, `selected_asset`은 `""`, `properties_json`은 `{}`로 정규화한다. 원본 node_position의 느슨한 수치 입력과 달리 두 int32 좌표만 받는다. `graph_type='auto'`는 유지한다. 빈 문자열을 optional default로 쓰면 required가 되는 실제 UE58 schema 생성 문제를 피하기 위해 nullable optional을 사용했다.

설치 `ToolsetRegistry/Content/Python/toolset_registry/tool_call_impl.py:96`에서 `list[int]` → `unreal.Array(int)` 매핑을 확인했다. 등록 wrapper에서 Python list로 정규화한 뒤 operations의 int32/길이 검사를 적용한다. 실제 nullable array schema와 전달 동작은 다음 에디터에서 확인해야 한다.

native positional 계약은 다음과 정확히 매핑한다.

```text
add_material_node(material_path, expression_class, graph_type,
                  node_position[0], node_position[1], selected_asset,
                  normalized_properties_json, 2048)
set_material_node_property(material_path, node_id, property_name,
                           normalized_value_json, graph_type, 2048)
```

JSON은 native 한계에 맞춰 65536 UTF-16 code unit 이하로 제한한다. 중복 key, 비유한 수, 깊이 16/값 8192 초과를 쓰기 전에 거부한다. properties는 object이고 최대 64개다. 정렬된 JSON으로 journal 인수를 정규화하므로 공백/key 순서만 다른 동일 입력은 같은 요청으로 재조회할 수 있다.

## 의존성·변경·실패 처리

기존 `_mutation` 경로를 그대로 사용한다: 읽기 snapshot → canonical `/Game` asset 확인 → root journal checkpoint → native 호출 → 실제 before/after와 postcondition → complete. compile/save는 추가하지 않는다. native 응답 파싱 실패는 이미 호출했을 가능성을 유지하며 자동 retry하지 않는다.

native C++는 이 작업에서 수정하지 않았다. 확인한 native guard 범위:

- 추가/속성 쓰기는 native Engine expression 클래스와 Engine/CoreUObject superclass chain으로 제한한다. 임의 플러그인/생성/에디터 클래스는 미지원이다.
- 새 object/class reference는 실제 클래스·package flags·editor-only/transient 여부, `/Game` 또는 `/Engine` mount, saved hard/soft dependencies와 bounded loaded references를 검사한다. 미감사 runtime plugin reference도 현재는 거부한다. 이 제한을 전체 프로젝트 의존성 증명이라고 표현하지 않는다.
- writable persistent property만 받는다. EditConst/transient/deprecated/instanced/export property 및 일부 형태는 명시적으로 거부한다. 숫자/enum/문자열/object/지원 struct 등을 임시 property storage로 변환·검사한 뒤 복사하고 실제 값을 대조한다. 모든 임의 legacy property가 동등 이식됐다는 뜻은 아니다.
- `selected_asset`은 해당 texture/function/collection expression 생성 API가 소비하는 class 조합만 허용한다.
- 추가 전 노드 수가 최대치이면 거부한다. 생성 후 일부 속성 적용 실패 시 새 expression 제거를 시도할 수 있지만, 중간 변화·적용된 속성·cleanup 결과를 보고하며 package atomic rollback을 보장하지 않는다.
- 원본 `add_custom_material_node` HLSL 편집 계약 등은 별도 기능이다. 이번 두 도구 연결로 완료 처리하지 않는다.

## 실제 검증용 pipeline 확장

기존 `material_graph_smoke.py`의 기본 v1 실행/독립 검수는 유지한다. 새 author 실행에 `--exercise-add-set`을 추가하면 fresh run ID 아래 `cubeless_material_pipeline_v2`를 만든다. v2 author는 43개 단계이며 다음 7개가 기존 36개에 추가된다.

1. Constant expression 추가: R=0.125, Desc, 명시 위치.
2. 같은 operation ID로 Add 재조회; node identity 유지.
3. R=0.875로 변경.
4. 공식 `ObjectTools.get_properties`로 R/Desc 독립 조회.
5. R에 잘못된 문자열 입력을 보내 native preflight 거부 확인.
6. 독립 조회로 값이 바뀌지 않았는지 확인.
7. 기존 명시 compile 요청 1회/save 1회 뒤 retained node의 R/Desc 조회.

초기 5개 expression 중 기존 delete fixture만 제거해 4개를 저장한다. v2 parent로 reload하면 자동으로 8개 단계가 실행되며, 새 프로세스에서 동일 retained node의 R/Desc와 graph identity를 확인한다. v1 기록을 v2로 다시 쓰거나 이전 검증을 확대 해석하지 않는다.

```powershell
python -B cubeless/tools/pipelines/material_graph_smoke.py --endpoint <official-http> --expected-project <isolated.uproject> --phase author --exercise-add-set --output <fresh-material-author-F.json>
python -B cubeless/tools/pipelines/material_graph_smoke.py --endpoint <official-http> --expected-project <isolated.uproject> --phase reload --parent-report <fresh-material-author-F.json> --output <fresh-material-reload-F.json>
```

root만 실제 실행한다. 새 report는 `compile.status='returned_without_reported_errors'`, `shader_completion_verified=false`를 기록한다. void recompile 반환/즉시 오류 없음이 비동기 shader 완료 증거가 되는 것은 아니다. v1 독립 검수에는 새 필드를 강제로 요구하지 않아 E3 기록을 그대로 검사할 수 있다.

## 합성 검증과 graph 경계 수정

Material wrapper 21개(기존 14 + 새 7), pipeline 13개(기존 10 + v2/반례 3), plugin 36개가 합성 검증 대상이다. fake native/MCP와 real Python journal을 사용하며 실제 engine API 검증이 아니다. 새 도구의 실제 runtime schema는 아직 수집하지 않았다. 정확한 해시/실행 결과는 `material-add-set-source-freeze-F.json`에 기록한다.

별도 native 리뷰에서 기존 Branch 추가가 정확히 2048 K2 node인 graph를 2049개로 만든 뒤 readback 한계로 실패하는 경계를 발견했다. root 승인 후 `operations/graph.py`에서 추가 전에 `graph_node_limit_reached`로 거부하게 수정하고, API/checkpoint 미호출 반례를 추가했다. Batch manifest/실행 로직과 state.py는 변경하지 않았다. 이전 `batch-source-freeze-F.json`은 당시 기록으로 보존하고, 이 새 freeze manifest가 두 graph 파일의 후속 해시를 기록한다.
