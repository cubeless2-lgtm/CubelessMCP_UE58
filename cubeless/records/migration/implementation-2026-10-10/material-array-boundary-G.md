# Material 배열 입력 경계 수정 G

실제 G 도구 등록은 성공했지만 `add_material_node(node_position=[-700,550])` 호출은 Python 함수에 도달하기 전 공식 Registry의 JSON→UStruct 변환에서 실패했다. 이전 F의 Python 모형 테스트는 이 엔진 변환 경로를 실행하지 않았으므로 등록·호출 검증으로 간주할 수 없다.

## 근거

- `records/verification/implementation-2026-10-10/material-add-schema-negative-probes-G.json`: 유효한 에셋을 찾을 수 없는 빈 `material_path`의 무변이 탐침 네 건. 좌표 배열을 생략한 두 건은 예상한 Material preflight 실패를 반환했고, 좌표 배열을 명시한 두 건은 JSON→UStruct 오류를 반환했다. `selected_asset=''`를 명시해도 배열 오류는 동일했다.
- `registered-schemas-and-diagnostics-G.json`: `node_position`은 array(integer), default null로 실제 노출되었다. 따라서 도구 이름 검색 실패나 등록 누락과는 구분한다.
- 설치 엔진 `Plugins/Experimental/ToolsetRegistry/Content/Python/toolset_registry/tool_call_impl.py:69-115`: `list[T] | None`은 `(unreal.Array(T), None)`으로 변환되며, Python 반사에서 OptionalProperty의 안쪽 ArrayProperty가 된다.
- `Source/Runtime/JsonUtilities/Private/JsonObjectConverter.cpp:1235-1322`: JSON 배열 입력은 OptionalProperty를 풀기 전에 Array/Set 여부와 ArrayDim으로 분기한다. OptionalProperty는 Array가 아니고 ArrayDim=1이라 배열의 첫 원소를 scalar 변환으로 보낸다. 이후 1200-1203의 Optional unwrap은 scalar 원소를 안쪽 ArrayProperty에 전달하여 실패한다.
- `Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/ToolsetJson.cpp:244-247`: 공식 함수 입력 변환은 이 JsonObjectConverter 경로를 사용한다. 엔진 소스는 수정하지 않았다.
- `PythonScriptPlugin/.../Private/PyWrapperObject.cpp:1774-1800`: Python default는 실제 property SetValue/ExportText 후 CPP_Default metadata에 보존된다. 일반 배열 default를 사용하는 근거다. 새 실제 Registry 스키마와 호출은 별도 검증 대상이다.

## 적용

플러그인 `registration/material_toolset.py`만 변경한다.

```python
add_material_node(..., node_position: list[int] = [0, 0], ...)
get_material_parameter_collection_values(..., parameter_names: list[str] = [], ...)
```

이름과 배열 입력 형태를 유지하면서 일반 FArrayProperty를 사용한다. MPC 이름 필터도 같은 optional 배열 결함의 영향을 받으므로 함께 수정한다. 기존 omission 의미는 유지한다. 새 public 계약은 null을 받는다고 약속하지 않는다. Python operation 내부의 None 정규화는 변경하지 않는다.

두 wrapper는 `list(...)`로 복사한 뒤 operation에 전달하므로 default 목록이 작업 함수에 의해 공유 변경되지 않는다. engine/native/gameplay 파일 변경은 없다. 함수 개수, 개별 기능, journal 동작, 파이프라인 43단계와 배열 인자 이름은 동일하다.

## 검증과 한계

- `material-array-contract-tests-G.json`: Python wrapper/native 모형/실제 journal 테스트 22개 통과. 새로운 반례는 bare list annotation과 default 목록 복사, 기존 반례는 잘못된 좌표·partial failure·checkpoint 실패를 검증한다.
- `python -m unittest discover ... -p test_material_pipeline.py`: 파이프라인 모형 테스트 13개 통과.
- 이 결과는 실제 엔진 JSON 변환 검증을 대체하지 않는다. 다음 새 에디터에서 explicit/omitted 좌표와 explicit/omitted MPC 필터를 공식 MCP 연결로 확인해야 한다.
- G 실패 증거 및 reconcile 보고서는 변경하지 않았다. G mutation을 자동 재실행하지 않았다.

## Source freeze

| 파일 | SHA256 |
| --- | --- |
| Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/registration/material_toolset.py | f8e7880891d59d61f620441c9c7dce2ffd754e7323dfa4ed6b32e59d1a73c047 |
| cubeless/tools/verification/test_material_python_contracts.py | 5dff38fd0d7998cc0908057794adda0c1e7a05dcc4b483b860e7e9443ad9b2eb |
