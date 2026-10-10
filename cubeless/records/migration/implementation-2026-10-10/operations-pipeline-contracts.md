# UE5.6 파이프라인·검수 Python 전수 계약 분석

2026-10-10. 원본 소스를 실행하거나 수정하지 않은 정적 분석이다. **3,518개 identity 전부를 분석했지만 3,518개 이식 완료 또는 고유 파이프라인으로 집계하지 않는다.** 현재 파일과 내용을 조사했고, 실행 경로의 동적 분기·생성 코드·에셋 fixture는 남은 검증 대상이다.

[Machine JSON](operations-pipeline-contracts.json)에 원본 경로·SHA-256·줄·AST 호출·입출력·검수·오류·import 연결과 대표 상세 계약이 있다. JSON은 대용량 정적 증거를 중복 들여쓰기 없이 저장한 약53MiB 파일이다.

## 범위와 보존

- 계획 inventory의 Python3,504개와 PowerShell14개를 모두 읽었다. Python은 전부 AST 파싱 성공, PowerShell은 lexical 근거를 기록했다. source module import/실행은 하지 않았다.
- 기존 migration-ledger의 `script:<source-relative-path>` ID를 그대로 유지했다. 누락0, 추가0, 중복0, 계획 대비 SHA-256 불일치0이다.
- 테스트 모듈·helper·generated template·fixture가 겹칠 수 있다. 기본 유형은 탐색용 분류이며 완료 판정이나 unique flow 계산에 사용하지 않는다.
- ignore된 파일, 미조사 plugin scripts, binary/JSON fixture의 전수 분석까지 포함한 숫자는 아니다. 기존 후보 목록과의 coverage를 확보한 것이다.

## 파일별 근거 분류

| 기본 유형 | 파일 수 | 기준 |
| --- | ---: | --- |
| `explicit_entrypoint` | 2,038 | __main__ guard 또는 argparse.add_argument의 실제 구문 |
| `test_or_verifier_module` | 750 | test_ 함수 또는 unittest/pytest import |
| `helper_or_implicit_entrypoint` | 489 | 정의가 있으나 명시 main/CLI 없음; 호출자 확인 필요 |
| `editor_top_level_script` | 123 | unreal import와 top-level 실행문 |
| `top_level_script_context_unresolved` | 92 | 실행문 있으나 환경 확정 불가 |
| `powershell_script` | 14 | PowerShell 함수·호출·파일쓰기 lexical 근거 |
| `declarative_or_empty_module` | 12 | 호출/실행 정의가 없는 선언 또는 빈 모듈 |

추가 근거는 중복 tag다. assert/검수 호출을 가진 파일2,230개, 파일 출력 코드1,511개, Mock/Fake/Stub/Fixture/TestCase 정의 근거623개를 식별했다. Python 파일 쓰기 또는 open의 명시 `.py` destination은5개 파일에 있었다. generated header가 명시된 파일은 찾지 못했으므로 이름만으로 generated artifact를 확정하지 않았다.

embedded editor Python/template 후보1,032개 중515개는 독립 AST로 파싱됐다. 나머지는 format/f-string 조각 또는 문서 예시일 수 있어 template/non-code로 남겼다. 파싱 성공도 해당 payload가 실제 전달·실행됐다는 증거는 아니다.

## 호출·입출력·실행 환경

- 기록한 호출 지점: **74,538개**. 엔진 API, tool dispatch, 알려진 명령을 인자로 받은 wrapper 후보, local function, subprocess/network, 파일 출력을 분리했다.
- 기존 공개 도구·native command 이름을 포함한 호출 후보: **7,810개**. 문자열 출현 전체 **14,064개**도 별도로 보관하여 간접 dispatch와 테스트 fixture를 놓치지 않게 했다.
- 스캔 범위 내부의 import 연결 **8,215개**, 입력 읽기 지점 **7,697개**를 기록했다. 모호한 import 후보를 하나로 임의 선택하지 않았다.
- 모호하지 않은 import 경로를 따라 tool dispatch 코드에 도달 가능한 파일 **2,269개**다. 조건부 import와 실제 함수 호출 여부는 별개이며 JSON에 첫 증거 경로를 남겼다.
- 각 호출의 `line/column/scope`, literal command, 인자/동적 식, 입력 flag/default, assertion/except/raise, 보고서 write와 embedded payload hash를 연결했다. 긴 표현은 `truncated` 또는 preview로 표시하며 원본 줄에서 다시 조사할 수 있다.
- **lexical 호출 순서는 runtime 순서가 아니다.** 함수 정의 순서, 분기, 반복, callback, 동적으로 조립한 명령을 실행 trace로 표현하지 않았다. 아래 수동 계약만 실제 소스 제어 흐름에 맞춰 단계 순서를 설명한다.

| 직접 근거의 실행 환경 | 파일 수 |
| --- | ---: |
| `editor_python_by_import` | 230 |
| `external_or_embedded_context_unresolved` | 2,331 |
| `external_network_or_orchestrator_candidate` | 142 |
| `external_offline_test_candidate` | 746 |
| `external_orchestrator_with_editor_payload` | 55 |
| `external_powershell_lexical` | 14 |

직접 import만으로 확정할 수 없는2,331개를 외부 실행으로 일괄 단정하지 않았다. 각 항목의 `import_reachable_context_evidence`와 embedded payload를 따라 후속 판정해야 한다.

## 기능군 분배

아래 표는 **경로 기반 작업 분배용**이다. 실제 명령 category와 source content hint는 JSON에서 중복 관계로 유지한다. 예를 들어 CubeFluidNinjaLive 안의 Niagara·Material·Blueprint 호출이 없어지거나 한 기능으로 합쳐진 것이 아니다.

| 기능군 경로 hint | 파일 수 |
| --- | ---: |
| CubeFluidNinjaLive | 2,152 |
| Other | 543 |
| PCG | 319 |
| ReactiveSurface | 150 |
| Niagara | 78 |
| Blueprint | 76 |
| TransportVerification | 59 |
| MaterialTexture | 48 |
| AnimationRigSkeletal | 36 |
| SkyCloud | 30 |
| ChaosPhysics | 27 |

## 다음 현실적 이식: offline verification dispatcher

우선 후보는 **OPS56-VERIFY-001**이다. 기존 `Python/scripts/analysis/run_verification.py`와 자체 검수기를 직접 읽어42개 command manifest와9개 profile의 정확한 순서를 AST로 추출했다. 기존 스크립트를 import해 manifest를 실행하지 않았다.

원본은 profile 선택 → ordered manifest 확인 → plan이면 텍스트/JSON 출력만 → 각 command를 독립 subprocess로 직렬 실행 → 첫 nonzero 종료 코드를 그대로 반환한다. default는 full이며 실패 후 나머지 gate는 실행하지 않는다. UBT나 live editor를 시작하지 않는다. 현재 stdout 위주이고 child 프로세스를 시작하지 못한 예외에 durable 종합 JSON이 보장되지 않는 것이 보완점이다.

제안 파일은 `cubeless/tools/verification/run_verification.py`이다. **이번 분석에서는 만들거나 실행하지 않았다.** 다음 작업의 계약은 다음과 같다.

1. 42개 원본 gate key와 원본 호출/순서를 manifest lineage로 유지한다. 초기 구현 가능한 `bootstrap-offline`과 전체 이식 `full-migration` 상태를 구분하며 미완료 gate가 있는 전체 profile은 PASS를 반환하지 않는다.
2. 초기 gate는 Python AST/undefined-name, Editor-only descriptor·game 의존성 static checks, 현재 외부 report/timeout 검수, source ledger identity/coverage, dispatcher 자체 fail-fast/plan/report 검수로 구성한다.
3. live registry와 packaging은 실행 환경이 다른 명시 profile로 분리한다. 이름만 offline인 gate가 은밀히 editor를 시작하거나 도구를 수정해서는 안 된다.
4. 각 command의 key/order/argv/cwd, 입력·환경 fingerprint, 시작/종료/시간, return code, stdout/stderr 파일, 최초 실패와 뒤 단계 not_run을 stage별 report에 기록한다. timeout/실행파일 누락에도 누적 보고서를 남긴다.
5. plan mode child 호출0, fail-fast 정확한 첫 exit 전파, 미완료42 gate false-PASS 방지, 경로 공백, 실패 fixture·실행파일 누락·잘못된 보고서 거부를 검증한다. Unreal 기능 검증은 별도 actual UE5.8 live profile에서 수행한다.

과거 transport gate를 통째로 삭제하지 않는다. TCP framing/고정 port 구현 검사는 제거 후보지만 timeout·결과 불명 mutation·상관 ID·오류 전파·중복 실행 방지는 공식 HTTP에서 유지·재검증해야 한다. 아래는 **예정 대응**, 최종 분류/완료가 아니다.

| 원본 gate key | 예정 대응 |
| --- | --- |
| `py_compile_core_modules` | `retarget_offline_code_gate` |
| `ruff_f821` | `retarget_offline_code_gate` |
| `local_install_paths_contract` | `retarget_offline_code_gate` |
| `tool_registration` | `rewrite_for_official_registry` |
| `tool_parity` | `rewrite_for_official_registry` |
| `management_contract` | `rewrite_for_official_registry` |
| `chaos_pie_runtime_qa_contract` | `retain_gate_and_inspect_domain_contract` |
| `tool_policy_contract` | `retain_gate_and_inspect_domain_contract` |
| `project_cpp_approval_gate_contract` | `rewrite_current_cpp_scope_contract` |
| `verification_dispatcher_contract` | `port_dispatcher_self_contract` |
| `mutation_result_contract` | `retain_gate_and_inspect_domain_contract` |
| `cpp_server_timeout_contract` | `split_transport_detail_from_behavior_contract` |
| `cpp_phase1_transport_contract` | `split_transport_detail_from_behavior_contract` |
| `cpp_transport_telemetry_contract` | `split_transport_detail_from_behavior_contract` |
| `cpp_event_wake_contract` | `split_transport_detail_from_behavior_contract` |
| `mcp_focused_crash_guard_contract` | `retain_gate_and_inspect_domain_contract` |
| `unreal_texture_import_interchange_contract` | `retain_gate_and_inspect_domain_contract` |
| `cpp_blueprint_component_property_contract` | `retain_gate_and_inspect_domain_contract` |
| `cpp_blueprint_input_key_graph_compatibility_contract` | `retain_gate_and_inspect_domain_contract` |
| `cpp_blueprint_variable_exposure_contract` | `retain_gate_and_inspect_domain_contract` |
| `ai_transport_marker_contract` | `split_transport_detail_from_behavior_contract` |
| `ai_ue56_domain_contract` | `retain_gate_and_inspect_domain_contract` |
| `gas_tools_contract` | `retain_gate_and_inspect_domain_contract` |
| `optional_domain_plugin_contract` | `retain_gate_and_inspect_domain_contract` |
| `controlrig_authoring_contract` | `retain_gate_and_inspect_domain_contract` |
| `niagara_assignment_transport_contract` | `split_transport_detail_from_behavior_contract` |
| `unreal_python_result_file_isolation` | `retain_gate_and_inspect_domain_contract` |
| `tool_discovery_contract` | `rewrite_for_official_registry` |
| `tool_exposure_benchmark` | `rewrite_for_official_registry` |
| `unreal_bridge_connection_contract` | `split_transport_detail_from_behavior_contract` |
| `unreal_bridge_loopback_contract` | `split_transport_detail_from_behavior_contract` |
| `phase1_practical_canaries_contract` | `retain_gate_and_inspect_domain_contract` |
| `phase1_practical_orchestrator_contract` | `retain_gate_and_inspect_domain_contract` |
| `phase1b_live_transport_gate_contract` | `split_transport_detail_from_behavior_contract` |
| `phase1_release_orchestrator_contract` | `retain_gate_and_inspect_domain_contract` |
| `write_tool_inventory_contract` | `retain_gate_and_inspect_domain_contract` |
| `write_postcondition_observer_contract` | `retain_gate_and_inspect_domain_contract` |
| `write_tool_inventory_check` | `retain_gate_and_inspect_domain_contract` |
| `write_postcondition_observer_check` | `retain_gate_and_inspect_domain_contract` |
| `niagara_authoring_offline_gates` | `retain_gate_and_inspect_domain_contract` |
| `response_schema_golden_tests` | `retain_gate_and_inspect_domain_contract` |
| `response_schema_audit` | `retain_gate_and_inspect_domain_contract` |

## 이어질 복합 파이프라인: SplineLoopConveyor

**OPS56-PIPE-001**은 단순 TCP 주소 치환으로 완료할 수 없다. 실제 읽은12개 구성 파일의 세부 계약과22개 요구 command를 JSON에 연결했다. `bridge.py`는 매 요청을 operations.jsonl에 기록하고 결과를 검사하며, builder는 typed graph·함수·변수·배열·loop·cast·component를 조합한다. 현재 Branch/exec subset만으로는 실행할 수 없다.

- 초기화: fresh BP guard → Actor BP → Spline/ISM components →24개 variable spec →9개 초기 function/parameter와 random schema → compile/default/save.
- graph 생성: `build_pure_bp.py`가 인자로 지정된 generator를 실행하고 compile 검수한다. 전체 build를 한 번에 지시하는 고정 진입점은 없으므로 추정 총순서를 과거 실행순서로 쓰지 않는다.
- 특화 conversion 검수: 연결 결과가 `make_with_conversion_node`이면 mutation을 반복하지 않고 정확한 Conv_* 중간 노드와 양쪽 link를 독립 topology로 확인한다. Typed conversion schema와 readback contract가 선행되어야 한다.
- saved 검수: native helper 부재, project-module 의존성 부재,3개 actor/점수2·5·4,0.03cm 이내 간격과0..1600cm 범위. 원본은 success JSON 뒤에 level save를 하므로 저장 결과 이후 최종 성공을 기록하도록 바꿔야 한다.
- runtime 검수: PIE world에서3개 actor, 실제 instance/child transforms와 GetItemWorldTransform 결과를0.1cm 이내 비교하고 nonzero phase·개수 일치를 확인한다.
- 재생성 검수: `generate_offsets → clear → build → transform`을 독립 subprocess로 실행하고 그래프 class/function/variable multiset과 Random* default 보존을 확인한 뒤 random-offset·pure-BP 검수기를 실행한다. 기존 autosave 임시 변경/finally 복원은 세션 부작용이므로 별도 범위/실패 상태로 다룬다.
- fixture: BP_ConveyorSamplePayload, PackedLevelActor 예제, 기본 Cube와3개 actor/map가 필요하다. 사용자 RX_PCGTest 에셋을 직접 수정하지 않고 격리 fixture로 parameter화해야 한다.

과거 `operations.jsonl` 2,469개 record도 읽고 해시/명령별 횟수를 저장했다. 여러 실행이 합쳐진 append journal이며 UE5.8 성공 증거가 아니다. 원본 mutation 실패를 자동 재실행하는 근거로도 쓰지 않는다.

## 기존 Blueprint smoke 연결

**OPS56-VERIFY-002**는 ping/create/Branch2/connect/move/delete/readback/cleanup/report 계약을 유지한다. 원본은 compile/save false여서 persistence 검증이 아니었다. 새 author/reload/cleanup 구현과 root의 실제 엔진 보고서는 별도 증거다. 이 정적 목록에서는 source-reviewed로만 두며 전체 graph/GUID selector 동등성을 주장하지 않는다.

## 남은 작업과 검증 범위

-4분류의 final disposition은 이 분석으로 확정하지 않았다. 문법 파싱/유형 분류/import 연결은 기능 이식 완료가 아니다.
-42 gate 각각의 내부 assertion을 새 엔진/공식 MCP 결과에 매핑하고 dispatcher를 실제 실행해야 한다.
-typed graph·conversion·component/variable/function wrappers와 isolated fixture가 준비된 뒤 SplineLoopConveyor를 compile/save/cold reload/runtime/package까지 실행해야 한다.
-CubeFluidNinjaLive2,152개는 우선순위와 공유 helper는 분석할 수 있지만 각각의 고유 동작·현재 fixture·렌더링·GPU 검수 차이를 상세 대조해야 한다. 공통 부모 script 하나를 이식한 것으로 전체 파일을 완료하지 않는다.
-본 분석은 원본/엔진 코드/에셋을 수정하지 않았고 코드 실행·MCP 에셋 호출·빌드도 하지 않았다. JSON self-check는3518 ID의 원본 ledger 완전 일치와 hash 일치,42 command/9profile의 참조 무결성을 확인했다.
