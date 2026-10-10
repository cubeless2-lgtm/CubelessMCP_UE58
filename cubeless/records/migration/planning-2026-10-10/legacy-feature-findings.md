# UE 5.6 기능 인벤토리: 계획용 정적 조사

기준일: 2026-10-10. 원본: `C:/Git/UnrealMCP5_6_1`.
원본 소스와 에셋을 변경하지 않았다. 플러그인 구현, 빌드, 에디터 실행, tool module 동적 import 및 라이브 호출은 수행하지 않았다.
이 문서와 `legacy-feature-inventory.json`은 **계획 자료**다. 모든 이식 분류는 `unassessed`이며 UE 5.8 동작은 미검증이다.

## 범위가 한정된 수량

| 조사 단위 | 수량 | 의미와 제한 |
|---|---:|---|
| `Python/tools/*.py`의 `@mcp.tool` 정의 | 436 | AST 정적 열거, 이름 중복 0. 라이브 등록 수가 아님 |
| 도메인 도구 정의 | 432 | 서버가 등록하는 도메인 24개 그룹 |
| 관리 도구 | 1 | `manage_tools` |
| 검색 메타 도구 | 3 | `list_toolsets`, `describe_toolset`, `call_tool` |
| 기존 정적 parity 감사가 추출한 native route | 388 | 중앙 라우터 + AI SupportsCommand + prefix/optional provider 경로 포함 |
| MCP 3개 플러그인 소스의 reflected UFUNCTION 선언 | 15 | Bridge 4, Skeletal Editor subsystem 2, ControlRig authoring subsystem 9 |
| native `F...::Handle...` JSON 함수 정의 | 407 | 정규식 후보 목록; public command와 일대일 관계가 아님 |
| MCP 3개 플러그인 C++/header/Build.cs 파일 | 111 | UnrealMCP, UnrealMCPStateTree, UnrealMCPMetaSound |
| 보조 스크립트 후보 경로 | 3,382 | 지정 6개 루트의 rg 기본 ignore 규칙 적용 목록. 의미 분석 완료 수가 아님 |

이 수량들은 겹치므로 합산해 고유 기능 수라고 표현하면 안 된다. 현재 문서의 429 도구라는 수는 `Docs/Tools/README.md:3,7`의 2026-08-27 과거 snapshot이다.

도메인별 정의 수: Blueprint node/AnimBP 106, Niagara 90, Editor 35, Chaos 24, PCG 23, Material 21, Blueprint 18, AI 15, Project 11, Texture generation 11, Sequencer 10, Data 9, Skeletal 9, ControlRig authoring 8, Niagara planning 8, UMG 7, Landscape 7, GAS 6, StateTree 5, Audio 4, Networking 2, Batch 1, Python 1, Ieta 1.

## 등록과 숨겨진 기능

- `Python/unreal_mcp_server.py:270-300`에서 그룹별 등록 및 정책 캡처를 수행한다. `:304-311`은 전체 catalog를 보관한다.
- `Python/tools/tool_discovery.py:766-800`은 catalog manager와 public manager를 분리한다. 기본 search 모드는 메타 도구 3개만 공개하고 전체 도메인 catalog는 `describe_toolset`/`call_tool` 뒤에 있다. MCP `tools/list` 수만 비교하면 대부분을 놓친다.
- `add_checkbox_to_widget`: `UnrealMCPBridge.cpp:1184` → `Commands/UnrealMCPUMGCommands.cpp:401,673`. Python public tool wrapper가 없다.
- `add_niagara_scratch_pad_di_function_call`: `UnrealMCPBridge.cpp:1055` → `Commands/UnrealMCPNiagaraCommands_Part1.cpp:237`. Python wrapper는 없지만 `Python/scripts/analysis/cube_fluid_ninja_sand_input_writer.py:113` 등 실제 복합 스크립트가 호출한다.
- StateTree 5개 route는 별도 `UnrealMCPStateTree/Source/UnrealMCPStateTree/Private/UnrealMCPStateTreeModule.cpp:10-37`에서 optional provider로 등록한다. MetaSound도 companion plugin 경로이므로 중앙 라우터 파일만 보면 누락된다.
- `UnrealMCPSkeletalMeshEditorSubsystem.h:15,21`의 `GetSections`, `SetSectionEnabled`는 별도 reflected editor API다. public tool와 command 집계 밖인 기능도 기록해야 한다.
- `UnrealMCPControlRigAuthoringSubsystem.h:15-45`에 recipe/profile용 reflected API 9개가 있다. Python → execute_python → editor subsystem → C++ 경로이므로 단순 send_command 문자열만 찾으면 작업 로직을 놓친다.

위 C++ 경로의 공통 접두어는 `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/`이며, header는 같은 플러그인의 해당 모듈 `Public/`에 있다. JSON에는 전체 원본 상대 경로와 줄 번호를 보관했다.

## 대표 연결과 동등성 비교의 최소 단위

1. Blueprint compile/validation: `Python/tools/blueprint_tools.py:824,856` → `compile_and_validate_blueprint` → `UnrealMCPBridge.cpp:859` → `UnrealMCPBlueprintCommands.cpp:2910` → 공용 `CompileBlueprintAndBuildValidationResult`(`:825`) → `FKismetEditorUtilities::CompileBlueprint`(`:857`). 원본은 PIE/SIE 금지, compile 상태, tokenized error/warning, node 진단, save 여부를 다룬다. 공식 도구의 이름이 compile이라고 해서 동일하지 않다.
2. Batch: `Python/tools/batch_tools.py:36,104` → `batch` → `UnrealMCPBridge.cpp:1493`. 1~100개, 중첩 batch 금지, game-thread 직렬 실행, 한 editor transaction, stop_on_error, item별 response와 stopped_at_index를 제공한다. 현재 코드는 실패 시 자동 원자 롤백을 구현하지 않는다. 새 구조도 이 사실을 정직하게 반환해야 한다.
3. 콘텐츠 pipeline: `Python/tools/project_tools.py:585` → `run_content_validation_pipeline_mcp` → `UnrealMCPProjectCommands.cpp:8342`. 재생성, 후처리, 선택적 world repair, 검증 단계와 단계별 결과를 묶는다. 단일 공식 primitive로 대체 처리하면 안 된다.
4. ControlRig recipe+AnimBP: `Python/tools/controlrig_authoring_tools.py:1472`; reflected route 생성 `:198-227`; native `UnrealMCPControlRigAuthoringSubsystem.cpp:3735`. native BuildRecipe는 AnimBP binding 자체를 수행하지 않는 제약(`:3761`)이 있고 Python composite가 후속 도구들을 조정한다. recipe 도구와 전체 pipeline을 각각 검증해야 한다.
5. Niagara composite: `niagara_tools.py:3924` promotion manifest, `:5670` source-template system, `:8716` module-input batch. PCG composite: `pcg_tools.py:948` graph spec authoring, `:1845` contract audit, `:2666` promotion. 각각 하위 도구 호출 순서, compile/save 횟수, dry-run 및 실패 후 상태를 보존하는 별도 이식 단위다.

## runtime C++ 의존성 금지에 따른 계획 조건

- 원본 `UnrealMCP.uplugin`의 두 모듈은 이미 `Type: Editor`다. `MCPGameProject/Source/MCPGameProject/MCPGameProject.Build.cs`에는 MCP module 의존성이 없다. 조사한 핵심 MCP header에서 game runtime actor/component/parent class는 발견되지 않았다. 이것은 모든 에셋의 무의존성을 입증하지 않는다.
- 기존 `Python/tools/dependency_guard.py:7-15` 및 `UnrealMCPCommonUtils.cpp:305-328`은 MCP 참조를 거부한다. `:839-959`에는 authored asset 의존성 감사가 있다. 새 이름 `/Script/CubelessMCPUE58Editor`와 실제 editor module/plugin mount 경로까지 반영해야 한다.
- ControlRig subsystem API는 `BlueprintCallable`; Skeletal API는 `BlueprintCallable, DevelopmentOnly`이다. 이런 metadata 자체가 저장된 Blueprint의 참조가 없음을 보장하지 않는다. CubelessMCP_UE58 wrapper는 editor authoring 전용으로 두고 생성 에셋의 parent/component/function/property/type/soft-reference 및 persisted dependency를 검사해야 한다.
- plugin→game module 및 game→plugin module 하드 의존성 모두 금지한다. 엔진/정상 게임 플러그인 타입을 에셋에 작성하는 것은 별도 문제이며, MCP helper 타입을 런타임에 넣어 해결해서는 안 된다.
- ControlRig/RigVM/AnimGraph의 생성 노드가 MCP 전용 함수/타입을 저장하지 않는지 개별 검수한다. packaged runtime에는 CubelessMCP_UE58가 없어야 하며, plugin 비활성화·에디터 재시작·에셋 재로드·게임 cook/package가 완료 판정의 별도 gate다.
- `UnrealMCP.Build.cs:29-31`에 Niagara `Internal` include 경로가 있다. UE 5.8의 공개 대체 API를 우선 조사하고, 그대로 복사해서 API 호환 완료라고 처리하지 않는다.
- 별도 서비스 `Python/services/speedtree_import_client.py`와 `pivot_paint_client.py`는 RXSpeedTreeImporter의 reflected API를 호출한다. 이들은 전체 script 감사에 포함하되 CubelessMCP_UE58의 필수 런타임 의존성으로 결합하지 않는다. 실제 이식 방식은 아직 미판정이다.

## pipeline·검수 Python의 독립 이식 장부

사용자 요구에 따라 pipeline 및 검수 Python을 public tool의 부록으로 취급하지 않는다. 각 pipeline에는 다음 항목이 필요하다.

- 고유 pipeline ID, 원본 경로/호출 진입점, 사용 tool/native/reflection feature ID.
- 호출 순서, 단계별 입출력, 외부 CPython과 Unreal 내부 Python 실행 경계.
- 성공/실패/부분 변경/미확정 판정, 검수 기준, 재실행 조건과 중복 실행 위험.
- compile/save/reload 단계, 보고서 생성 경로와 schema, 원본 5.6 증거와 신규 5.8 증거의 구분.
- UE 5.8에서 전체 pipeline의 실제 실행 결과. copy/archive/개별 tool 등록/범용 execute 성공은 pipeline 이식 완료가 아니다.

JSON의 `pipeline_migration_contract`에 이 완료 조건을 기록하고 3,382개 후보에 `execution_environment`, `pipeline_id`, `required_tool_feature_ids`를 미판정 상태로 남겼다. 후보 전부를 독립 pipeline이라고 주장하지 않는다. 운영 문서 조사 작업에서 대표 pipeline을 확정하고, 전체 후보의 재사용·검수·일회성 기록·통신 전용·장애 항목을 빠짐없이 분류해야 한다.

## 수행한 검사와 남은 조사

`python -B Python/scripts/analysis/mcp_tool_parity_audit.py`를 원본에서 출력 파일 옵션 없이 실행했다. 해당 스크립트는 AST/정규식 기반 파일 읽기만 수행한다. 결과는 Python 436/native 388, missing/duplicate/error 0, pass였다. **기존 allowlist를 포함한 정적 이름 대응 검사**이므로 runtime, 입력 schema, 오류 처리 또는 동작 동등성 통과를 의미하지 않는다.

다음 단계는 원본의 격리 복사본에서 전체 runtime catalog를 캡처하여 이 목록과 대조하는 것이다. 원본 저장소의 에디터나 파이프라인을 직접 시작하지 않는다. 그 다음 native handler → 실제 엔진 API, scripts → composite call graph, 엔진 5.8 변경점을 상세 추적한다. 현재 JSON의 후보 관계는 이름과 기존 alias 기반이며 transitive call graph 확정본이 아니다.

기능별 상세 조사·판정이 끝나면 최종 분류는 `CubelessMCP_UE58 구현·등록`, `공식 도구 동등 대체`, `통신 전용 제거`, `장애 미완료` 중 하나여야 한다. 현재 계획용 후보 목록의 `unassessed`는 그 이전 상태다. 기능 자체가 남아 있는데 구현이 어렵다는 이유로 통신 전용 제거로 분류하면 안 된다. 미판정 또는 장애 항목이 남아 있으면 전체 이식 완료가 아니다.
