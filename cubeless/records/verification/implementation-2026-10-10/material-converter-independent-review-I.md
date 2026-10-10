# Material 배열 converter 독립 검토 I

검토자는 Python Toolset 담당, native converter 구현자는 별도 native 담당이다. 이 기록은 소스 검토와 오프라인 회귀 테스트이며, I 새 에디터의 실제 등록·호출 증거와 구분한다. 검토 중 엔진 호출, 에셋 수정, engine source 수정은 수행하지 않았다.

## 검토한 구현

- `Source/CubelessMCPUE58Editor/Private/Compatibility/UE58/CubelessMaterialArrayConverter.{h,cpp}`
- `Private/Registration/CubelessJsonConverters.{h,cpp}`
- `Private/Registration/CubelessRegistrationLibrary.{h,cpp}`
- `Private/CubelessMCPUE58EditorModule.cpp`
- Python 연결 `Content/Python/cubeless_mcp_ue58/registration/__init__.py`

## 범위와 실제 변환

`MatchParameter`는 정확한 mounted Python class object path, 함수명, 인자명, `FArrayProperty`, ArrayDim=1, input CPF_Parm과 non-return/non-out/non-reference 여부 및 inner `FIntProperty`/`FStrProperty`를 함께 확인한다. 다른 Toolset의 비슷한 함수명/인자명이나 일반 배열에는 적용되지 않는 조건이다. `PyUtil.cpp:374-376`의 Python int→FIntProperty와 `PyWrapperObject.cpp:1754`의 입력 CPF_Parm 생성 방식에 부합한다. 실제 generated class path는 I prepare 보고서로 대조해야 한다.

좌표는 정확히 두 숫자이며 유한·정수·int32 범위를 검사한다. 필터는 문자열 배열인지 검사한다. 모든 원소 검증 후 초기화된 property scratch에 값을 쓰고 `CopyCompleteValue`로 call frame에 적용한다. 검증 중간의 부분적인 call-frame 적용을 피한다. JSON schema에 같은 제한을 표시하지만 schema의 타입 검증만 믿지 않는다. 엔진 `ObjectFunctionToolCall`은 required 존재만 확인하며 전체 JSON 타입 검증은 TODO라고 명시되어 있기 때문이다.

Native default parser는 initialized property scratch에 `ImportText_Direct`를 수행하고 입력 전체 소비를 확인하며, JSON로 내보낸 후 동일 배열 검사를 적용한다. 초기 prepare에서 default가 `[0,0]`/`[]`인지 확인하므로 임의의 유효 배열을 우리의 기본값으로 받아들이지 않는다.

## 빈 배열과 캐시

독립 검토에서 G2의 `parameter_names` 빈 기본값을 schema.default에만 넣는 방법은 부족하다고 지적했다. required 판단은 runtime custom schema보다 먼저 EditorMetadata.default로 이루어진다. 구현은 hidden `PrepareMaterialToolset(UClass*)`로 두 exact property와 등록된 converter를 확인하고, own UFunction의 존재하는 빈 `CPP_Default_parameter_names` 값만 `()`로 정규화한다. 예상 밖 default는 metadata 변경 없이 실패한다.

공개 `toolset_registry.tool_call(func)`에는 metadata override 인자가 없고 `unreal.ufunction` metadata를 내부에서 AICallable로 고정한다. 따라서 private decorator 상태를 수정하지 않고 공개 C++ reflection API를 쓰는 helper를 선택했다. 함수는 BlueprintCallable+AIIgnore이고 ToolsetDefinition을 상속하지 않아 별도 MCP 실행 도구를 추가하지 않는다.

Native helper는 이미 등록된 Material class를 거부한다. Python bootstrap은 기존 Material 등록만 공식 unregister로 제거한 뒤 prepare→register를 수행한다. 나머지 Toolset은 그대로 유지한다. 이는 describe의 fresh schema와 실행용 cached schema가 달라지는 문제를 막기 위한 순서다.

G2 기존 로그에서도 native module 등록 09:39:13.073 → init_unreal 09:39:14.056 → Material Toolset 등록 09:39:14.093 순서가 관측되었다. I 모듈은 converter를 native startup에 등록하여 Python class 등록 이전에 준비한다. 실제 I 로그와 omitted 호출로 이 순서를 다시 확인해야 한다.

## 실패 차단 및 진단

Python bootstrap은 helper 부재/실패/잘못된 default/다른 converter 선점이면 Material만 등록하지 않는다. 기존 Material 캐시도 먼저 무효화한다. 등록 후 실제 schema에서 두 배열의 type/items/default와 required 제외를 확인하고, 불일치하면 Material을 unregister한다. 진단은 `ready=false`, `material_preparation.stage/error/native`를 남기고 전체 정상 등록으로 기록하지 않는다. Registry의 unregister 자체가 실패하면 그 오류와 실제 등록 여부를 드러내며, 차단에 성공했다고 표현하지 않는다.

배열을 제외한 scalar optional 필드와 다른 Toolset의 등록/작업 로직은 변경하지 않았다. 공통 journal과 mutation API도 I 연결에서 변경하지 않았다.

## null 제한

설치 `JsonObjectConverter.cpp`의 `JsonAttributesToUStructWithContainer`는 값이 JSON null이면 해당 property 변환을 건너뛴다. 따라서 converter 내부에서 null을 거부한다고 해서 함수 입력의 모든 null이 거부된다는 뜻은 아니다. 초기 초안의 null 관련 주석/오류 설명은 이 한계를 반영하도록 수정 요청했고 native 담당이 반영했다.

현재 공개 계약은 배열을 공급하거나 생략하는 것이며 null을 지원한다고 약속하지 않는다. 실제 null 입력은 root의 무변이 탐침에서 별도 확인해야 한다. 특히 coordinate null이 empty array로 남으면 Python의 정확히 두 좌표 검사에서 거부되지만, MPC filter null은 empty array→all로 해석될 수 있다. 이 동작은 이 소스 검토만으로 확정하지 않는다.

## 오프라인 검증

`tests/test_registration_boundary.py`의 7개 테스트는 등록 전 prepare, 기존 Material 캐시 무효화, 다른 도구의 재등록 방지, helper 부재/실패/잘못된 default의 Material만 차단, schema required 회귀의 unregister, unavailable registry의 readiness 거부를 검증한다. 실제 UE 경계는 모형이며 엔진 실행 증거가 아니다.

기존 batch/graph/journal 테스트를 포함해 46개가 통과했다. 기록은 `registration-helper-plugin-tests-I.log`다.

Python source freeze:

| 파일 | SHA256 |
| --- | --- |
| registration/__init__.py | 5a5e74e2f2ce4dd2fd92ced3dfc2e7c197bcc7119382869bed66abf07d811aeb |
| tests/test_registration_boundary.py | a130d6513645e4bca0bf12759d9175a4ae61a6c0c21c0dd49c81f410becd1454 |

## I 실제 검증에서 남은 항목

정확한 class path, converter 준비·등록 순서, 2개 배열의 schema 기본값/required 제외, omitted/explicit 좌표·MPC 필터, 잘못된 배열 거부, null 관측, converter 범위 밖 기존 도구 정상 동작을 확인해야 한다. 이 항목들은 Python 테스트 통과로 완료 처리하지 않는다.
