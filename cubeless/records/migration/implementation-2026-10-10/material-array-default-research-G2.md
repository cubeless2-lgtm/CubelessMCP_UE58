# Material 배열 기본값: G2 실행 및 공식 확장 지점 조사

G2의 실제 등록/호출은 G 기록의 후속 증거다. 일반 배열로 바꾼 `node_position` 명시 호출은 Python/native 작업에 도달했지만, 실제 스키마에서 `node_position`과 `parameter_names`가 required로 생성되었다. Python 기본값이 존재한다는 것만으로 공식 MCP에서 생략 의미가 유지된다고 결론낼 수 없다. G의 변경 의도와 오프라인 테스트는 이 실제 결과를 대체하지 않는다.

## 두 개의 독립적인 UE58 경계

1. `Optional<Array>` 입력 변환 문제: JSON 배열을 Optional unwrap 이전에 ArrayDim=1의 고정배열처럼 처리한다. G의 일반 `list[T]` 변경으로 명시적 배열 입력은 실제 호출에 성공했다.
2. 배열 기본값 스키마 문제: `Source/Editor/JsonUtilitiesEditor/Private/JsonSchema/JsonSchemaGeneratorEditor.cpp:278-415`의 `SetFunctionPropertyDefaultInPropertyMetadata`는 숫자/문자열/bool/enum/struct/object/optional에 대한 분기만 있고 `FArrayProperty` 기본값은 unsupported다. `CPP_Default_*` metadata를 다시 지정해도 같은 분기라 해결되지 않는다.

두 문제 모두 Epic 엔진 소스를 수정하지 않고 프로젝트 플러그인에서 해결해야 한다.

## 공식 public 확장 API

`ToolsetRegistry/Public/ToolsetRegistry/ToolsetRegistry.h`의 `FToolsetRegistry::RegisterConverter`와 `ToolsetJsonConverter.h`의 `FToolsetJsonConverter`는 property별 JSON schema/default/data 변환을 공식적으로 확장한다. `JsonSchemaGeneratorEditor.cpp:294-299`가 기본 타입 분기 전에 custom callback을 호출하므로 배열 default를 공급할 수 있다.

좁은 적용 대상은 우리 Material Toolset의 두 함수 인자뿐이다.

- `add_material_node.node_position`: integer 배열, 정확히 두 int32 값, default `[0,0]`.
- `get_material_parameter_collection_values.parameter_names`: string 배열, default `[]`.

다른 도구 또는 모든 배열에 converter를 적용하지 않는다. 정상 schema와 default 주입 이후의 실제 값 변환도 같은 제한을 검증해야 한다. 타입, 길이, int32 범위, bool/소수 거부를 schema 설명에만 맡기면 안 된다.

`PythonScriptPlugin/Private/PyUtil.cpp:1416-1495`는 Python 파일을 Content mount package로 변환한 뒤 `_PY`를 붙인 package에 generated class를 생성한다. 정상 mount로 유도되는 owner path는 `/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools`다. 이는 소스에 근거한 예상이며 새 에디터의 실제 generated class path와 대조해야 한다. class 단순 이름만 매칭하는 구현은 피한다.

## 생략한 인자가 실제 값으로 넘어가는 경로

`ToolsetRegistry/Private/ToolsetRegistry/ObjectFunctionToolCall.cpp`:

1. `BuildValidFunctionInputParamsJsonObject` 532-558: required 존재를 검사한다. 스키마 타입 전체 검증은 하지 않는다고 엔진 TODO에 명시되어 있다.
2. `ProvideDefaultsForMissingFunctionInputParams` 187-250: 없는 인자에 `inputSchema.properties.<arg>.default` JSON 값을 넣는다.
3. 387: `ToolsetJson::JsonDataToStruct`로 반사 함수 입력을 만든다. 등록된 converter는 이 값에 적용된다.
4. Python wrapper가 목록을 복사하여 operation에 전달하고 실제 native 작업/검수를 수행한다.

따라서 default schema가 올바르기만 한 것이 아니라, 실제 omitted 호출까지 확인해야 한다.

## 등록 순서와 캐시

`FObjectFunctionToolCall` 생성자 293-299는 실행용 `FunctionSchemaJsonObject`를 한 번 생성해 보관한다. 반면 `FunctionLibraryToolset.cpp::GenerateSchema`는 설명 요청 때 새로운 schema를 만든다. converter를 Toolset 등록 후 추가하면 describe 결과만 새롭고 실행의 required/default는 이전 상태일 수 있다.

프로젝트 plugin의 converter를 Python Toolset 등록 전에 등록해야 한다. 이전 등록을 재사용하는 reload 흐름은 우리 Toolset을 공식 API로 재등록해야 실행용 캐시가 갱신된다. 이 차이를 검증 없이 hot reload 성공으로 주장하지 않는다.

## 후속 검증 범위

- 실제 describe에서 올바른 array type/items/default와 required 제외를 확인한다.
- omitted 좌표의 무변이 빈경로 탐침이 좌표 오류가 아닌 native Material resolve 실패에 도달하는지 확인한다.
- 격리 에셋에 omitted 좌표 add 후 independent readback이 `[0,0]`인지 확인한다. explicit 좌표도 별도 확인한다.
- MPC omitted filter, explicit `[]`, explicit 이름 배열을 실제 호출하고 전체/필터 결과를 비교한다.
- malformed 길이·소수·bool·int32 초과·비문자열 필터를 실제 경계에서 거부하는지 확인한다.
- 다른 Toolset의 array schema/호출이 영향을 받지 않는지 확인한다.

이 문서는 조사 및 독립검토 준비 기록이다. converter 구현, 빌드, 실제 등록·호출의 완료 보고서가 아니다. Python source는 G freeze 이후 변경하지 않았다.
