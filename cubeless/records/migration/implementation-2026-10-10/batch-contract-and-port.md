# UE 5.6 batch 계약 조사와 UE 5.8.3 이식 설계

작성일: 2026-10-10. **조사·설계 기록이며 batch 구현·등록·호출 검증 완료가 아니다.** 원본은 읽기만 했고, 이 조사에서 에디터·MCP 호출은 실행하지 않았다. 상세 함수 목록, 출처 해시, 원본 실제 등록 스키마, 지원 후보와 검증 항목은 같은 이름의 JSON에 보존한다.

## 결론과 적용 범위

원본 `execute_batch`는 명령 1~100개를 한 TCP 요청으로 보내고, `UnrealMCPBridge::HandleBatchCommand`가 게임 스레드에서 순서대로 실행하며 하나의 `FScopedTransaction`으로 묶는다. 순서, 중단 정책, 각 명령의 전체 응답, 성공·실패 합계, 일부 변경 및 불확실한 결과의 보고를 이식해야 한다. TCP 연결·전용 서버·bridge timeout 설정은 새 구조의 필수 의존성에서 제거한다.

새 배치는 공식 MCP → 공식 Registry → `CubelessBatchTools` → 기존 Cubeless domain action 경로로 구현하는 것이 적절하다. 현재 개별 동작이 실제 검증된 Blueprint lifecycle 및 작은 K2 그래프 편집 범위만 최초 허용 목록으로 제안한다. 원본의 모든 raw command, `execute_python`, 임의 공식 도구를 동적 호출하지 않는다. 미지원 명령을 포함하면 전체 실행 전에 명시적으로 거부한다. **이 범용 batch 하나가 다른 도메인 도구, 특화 다중 편집, 파이프라인, 검수 Python의 이식을 대신하지 않는다.**

현재 가장 중요한 선행 작업은 journal 소유권을 보존하는 공통 action 분리다. 기존 `state.mutate()` 안에서 기존 mutation wrapper를 다시 부르면 `mutation_busy`가 발생한다. 잠금을 잠시 해제하는 우회는 금지하고, standalone wrapper와 batch가 같은 검수·작업 로직을 공유하도록 구조적으로 분리해야 한다.

## 실제 등록과 소스의 연결

| 계층 | 확인한 출처 | 확인 범위 |
| --- | --- | --- |
| 공개 Python 도구 | 원본 `Python/tools/batch_tools.py:35` | `execute_batch(ctx, commands, stop_on_error=True, timeout_seconds=0)` |
| 실제 FastMCP 등록 | `legacy-runtime-registration.search.json`, `.eager.json` | 두 mode의 실제 catalog에 등록, eager 공개 목록에도 등록; 아래 입력 스키마 일치 |
| 정책/결과 보정 | `tools/management_tools.py:733`, `tools/mutation_result_contract.py:493`, `:706`, `tools/tool_policy.py:122` | write/high, `mutation_result_v1`, batch 자식 결과 합산 |
| transport | `services/unreal_bridge_connection.py:622` | 단일 `send_command('batch', ...)`, 요청별 socket 대기시간 |
| C++ 진입/배치 | `UnrealMCPBridge.cpp:713`, `:1493` | `batch` 분기 → `HandleBatchCommand` → 자식 `ExecuteCommand` |
| 엔진 실행 | 같은 파일 `:1202`, `:1309`와 batch 본문 | 게임 스레드 재진입 동기 실행, `FScopedTransaction`, 각 기존 domain handler의 엔진 API |

이 등록 근거는 기존 서버 소스를 격리 복사한 뒤 실제 FastMCP 1.5.0으로 등록 함수를 실행한 기록이다. 도구 본문·서버 lifespan·transport·UE 5.6 endpoint는 실행하지 않았다. 등록 증거와 실제 엔진 동작 증거를 혼동하지 않는다.

실제 등록 입력:

```json
{
  "type": "object",
  "properties": {
    "commands": {"type": "array", "items": {"type": "object"}, "title": "Commands"},
    "stop_on_error": {"type": "boolean", "default": true, "title": "Stop On Error"},
    "timeout_seconds": {"type": "integer", "default": 0, "title": "Timeout Seconds"}
  },
  "required": ["commands"],
  "title": "execute_batchArguments"
}
```

`ctx`는 공개 입력에서 제외된다. 등록 스키마 자체는 item의 `type`/`params`, 1~100개 제한을 표현하지 않는다. 이 제약은 도구 본문에 있다. 설명이 강하더라도 실제 스키마의 검증 범위로 확대 해석하지 않는다.

## 원본의 전체 실행 계약

### 입력·사전 검사

- 공개 Python은 **배치 전체**를 먼저 검사한다. 비어 있지 않은 list, 최대 100개, 각 entry가 dict이고 `type`이 빈 문자열 아닌 str인지 검사한다. exact `type == 'batch'`는 거부한다. `params`는 누락 또는 `None`을 허용하며, 그 외에는 dict여야 한다. 실패 시 아직 bridge에 요청하지 않았다.
- 오류 코드는 `batch_invalid_commands`, `batch_too_large`, `batch_invalid_entry`, `batch_nested`, `batch_invalid_entry_params`다. entry 오류에는 index가 붙는다. 재귀 `reject_mcp_dependency_references('commands', commands)`도 연결 전에 적용된다.
- 기존 dependency guard는 문자열 value의 알려진 UnrealMCP/ControlRig 보조 플러그인 token을 검사하는 blacklist다. 모든 직렬화/동적 의존성을 증명하지 않으며 새 이름을 그대로 포괄하지 않는다. UE58 parent 및 domain guard를 batch에서도 유지해야 한다.
- C++ raw bridge만 직접 호출하면 계약이 약해진다. 배열·개수만 먼저 검사하고 잘못된 entry/중첩 batch는 순회 중 실패 row로 처리한다. 잘못된 `params` 타입도 빈 object로 정규화한다. Python 공개 도구는 이런 잘못된 payload를 전체 실행 전에 거부한다. 새 Toolset에는 더 안전한 공개 Python 계약을 기준으로 삼는다.

### 순서·중단·동작 허용 범위

- 입력 순서대로 직렬 실행한다. 자식은 raw bridge 명령이며 자식별 Python 공개 도구를 다시 호출하지 않는다. 따라서 개별 Python wrapper의 parameter 변환, dependency guard, dry-run/confirm 정책을 자동 상속하지 않는다.
- 원본 batch 자체의 command allowlist는 없다. dispatcher가 아는 read/write 명령 및 조건부 provider 명령을 호출할 수 있다. 정책상 전체 batch는 write/high로 분류된다. `batch` 중첩만 명시적으로 막는다.
- `stop_on_error=True`가 기본이며 첫 실패에서 멈춘다. `False`이면 그 이후 entry도 순회한다. 원본 C++ 자체는 실패가 결과 불명확 상태인지 구분하여 중단하지 않는다. Python 결과 정규화는 이미 실행된 뒤 불확실성을 보정한다.
- 자식 params에서 `__mcp_ticker_dispatch_effective`를 제거한다. 게임 스레드의 re-entrant `ExecuteCommand`는 동기 실행한다. 자식 ticker를 기다리는 방식이 아니며, 병렬 처리나 오래 걸리는 engine 호출의 선점·취소를 제공하지 않는다.
- 입력 params는 고정 JSON이다. 앞선 결과의 node ID를 뒤 entry에 자동 참조/대입하는 문법은 없다. 생성→결과 ID 이용 편집 pipeline을 단순히 한 batch에 넣을 수 있다고 주장하지 않는다.

### 출력·일부 결과

자식 row는 `{index, type, status, response}`이며 실패 시 `error`가 추가된다. malformed/nested/응답 JSON 파싱 실패 row에는 `response`가 없을 수 있다. 성공 판단은 자식 bridge envelope의 `status == 'success'`다.

배치 payload는 다음을 보존한다.

```text
success = (failed == 0)
total = requested count
executed = results.length
succeeded / failed = visited row counts
stopped_at_index = first stopping error index, otherwise -1
stop_on_error
results = visited rows in input order
error = aggregate message, only if failures
```

여기서 `executed`는 실제 엔진 변경 횟수가 아니라 **평가한 row 수**다. 형태 오류·중첩 거부 row도 포함한다. 뒤쪽 미실행 entry는 results에 추가하지 않는다. `stop_on_error=False`로 실패가 발생했어도 끝까지 순회하면 `stopped_at_index=-1`이다.

외부 `ExecuteCommand`는 성공 시 `{status:'success', result:<payload>}`, 실패 시 `{status:'error', error:..., result:<payload>}`로 감싸므로 정상 오류 처리에서는 partial results가 유지된다. batch loop 전체 밖으로 예외가 벗어나면 이 구조의 일부 결과가 보존된다고 보장할 수 없다.

Python 관리 wrapper는 `apply_mutation_result_contract`로 다시 정규화한다. `_batch_mutation_summary`는 **정해진 `result.results[].response` 위치만** 합산한다. 임의 배열/중첩 object를 child 계약으로 추측하지 않는다. 시도 여부, tri-state 실제 변경 여부, `mutation_state_unknown`, `partial_mutation`, `do_not_retry`, `command_may_still_be_in_flight`, 완료 mutation stage/index와 개수, 실패 stage의 실제 변경 여부, 복구 필요 상태를 전파한다. 변화가 없었다는 주장과 실제 변경 주장이 모순되면 보수적으로 불확실한 결과로 처리한다.

모든 child가 성공했을 때 일부 legacy child의 mutation metadata 부재를 허용하는 호환 경로가 있다. 반대로 실패·복구 필요·안전상 실패가 섞이면 전체 성공 주장보다 복구/재시도 금지가 우선한다. 새 batch는 처음부터 각 허용 action의 분류를 알고 있으므로 이 legacy metadata 공백을 재현할 필요가 없다.

### 트랜잭션·컴파일·저장

`FScopedTransaction`은 Undo 기록을 묶는 장치다. failure에서 `Cancel()`이나 Undo를 호출하지 않는다. 원자적 rollback, 디스크 저장 취소, 외부 부작용 복구를 제공하지 않는다. 실제 한 번의 Undo로 복원되는 범위는 각 handler의 `Modify`/transaction 지원에 달려 있으며 이 조사만으로 전 명령에 보장하지 않는다.

batch 자체는 compile/save를 추가하지 않는다. 요청에 들어 있는 개별 명령이 지정된 위치에서 처리한다. 예를 들어 여러 그래프 변경 후 compile/save 명령을 한 번 넣어 비용을 줄일 수 있지만, 명시된 중간 compile/save를 임의로 합치거나 제거하면 순서·failure 경계가 달라진다. 새 기본 batch도 이를 최적화라는 이름으로 바꾸지 않는다.

### timeout·재실행·보고서

자동 Python socket timeout은 `min(600, max(120, 120 + 10 * count))`초다. **양수의 명시 timeout은 그대로 사용하므로 600초 상한은 자동값에만 해당한다.** 이 값은 C++ batch params로 전달하지 않는다.

bridge의 `UNREAL_MCP_COMMAND_TIMEOUT_SECONDS`는 별도 환경 설정이다(기본 120초, 유효 범위 10~3600초). `Future.WaitFor` 실패는 이미 큐에 제출된/실행 중인 게임 스레드 작업을 취소하지 않는다. 재진입 동기 자식의 engine API도 선점하지 않는다. 따라서 원본 docstring의 “전체 batch 시간을 제한”한다는 표현을 “시간 초과 시 모든 변경 중단”으로 받아들이면 틀린다.

bridge timeout은 `server_command_timeout_outcome_unknown` 및 transport marker의 `dispatch_submitted=true`, `outcome=unknown`, `command_may_still_be_in_flight=true`를 돌려줄 수 있다. Python transport도 write 시작 뒤 send/receive/decode 실패를 결과 불명확으로 분류하며 자동 재요청하지 않는다. 연결 전 실패와 요청 제출 후 실패를 구분해야 한다.

원본 batch는 operation ID, 영속 journal, 재개, 정확히 한 번 실행, 진행 스트림, 독립 report 파일을 제공하지 않는다. 응답/transport telemetry와 외부 verifier가 보고 근거다. 이전 history의 confirm-required 정책은 당시 검색 dispatcher의 계약이며 이번 사용자의 승인과 별개다. 새 설계에 불필요한 반복 승인 규칙을 자동 복사하지 않는다.

## UE58 등록/구현 설계 — 아직 미구현

### 등록·adapter·작업 로직 분리

예상 구조는 `registration/batch_toolset.py`(공식 등록·입출력), `operations/batch.py`(전체 검수, 순서, 결과·journal), 명시적 capability manifest(허용 action·schema·effect), `compatibility/ue58`(실제 엔진 API·transaction)다. 다른 엔진 버전의 구현을 미리 추가하지 않는다.

첫 등록 signature 후보:

```python
class CubelessBatchTools(unreal.ToolsetDefinition):
    def execute_batch(
        commands_json: str,
        stop_on_error: bool = True,
        operation_id: str | None = None,
    ) -> CubelessPythonOperationResult: ...
```

이는 **제안**이며 실제 Registry schema를 아직 생성·호출하지 않았다. 현재 검증된 reflected str/bool/nullable 입력과 결과 구조를 재사용하고, `commands_json`은 1~100개 `{type: '<canonical Cubeless tool ID>', params: {...}}` 배열로 엄격히 해석한다. 임의 `List[Dict[str,Any]]`가 공식 reflection에서 같은 스키마로 등록된다고 가정하지 않는다. 향후 typed USTRUCT 배열이 필요하면 별도 실제 등록 시험 후 바꾼다.

JSON 오류·중복 key·허용되지 않은 key/명령·함수별 인수/타입·중첩 batch·개수/전체 payload 크기·경로/의존성 정책을 전체 실행 전에 검사한다. entry의 누락/null params는 원본과 같이 빈 object로 정규화할 수 있지만, 필수 함수 인수 누락은 실행 전 오류다. Python bool이 int로 간주되는 등의 느슨한 검사는 피한다. **아직 생성되지 않은 앞선 entry의 결과 에셋 존재 여부까지 upfront 검사하지 않는다**. 구조·capability 검사는 전체 upfront, 시점별 engine precondition은 각 entry 직전에 수행하여 정상적인 create→compile 흐름을 허용한다.

선택적 외부 Python 호환 adapter는 기존 `execute_batch(commands, stop_on_error, timeout_seconds)` 인수를 받아 JSON 직렬화하고 공식 MCP 연결로 전송할 수 있다. timeout은 외부 HTTP client의 대기 정책이며 engine 취소 의미를 부여하지 않는다. raw legacy 명령명을 지원하려면 **검증된 command별 명시적 인수 변환표**가 필요하다. 일부 새 이름이 비슷하다는 이유로 모든 원본 명령명을 alias하지 않는다. 별도 서버/TCP bridge는 필요 없다.

### 최초 허용 후보와 동등성 경계

아래는 **batch에 이미 등록된 목록이 아니라 개별 함수의 검증 증거가 있어 adapter를 만들 수 있는 후보**다. 실제 서명 전체는 JSON에 AST로 보존한다. batch child에는 별도 `operation_id`를 받지 않으며 root batch ID가 소유권을 가진다.

| 후보 ID | effect | 개별 실행 근거 | 원본 관계와 제한 |
| --- | --- | --- | --- |
| `CubelessBlueprintTools.create_blueprint` | write | author-D, cold reload-D | 원본 `create_blueprint`; 명시 parent runtime 의존성 guard, 기본 package, 생성 뒤 compile/save 별도 |
| `CubelessBlueprintTools.inspect_blueprint` | read/load | author-D, reload-D | 관측 도구. 원본의 임의 상세 inspector 전체 대체가 아님 |
| `CubelessBlueprintTools.compile_blueprint` | write | additional-calls-E2 | 원본 compile. compile 자체 in-memory 변경, save/refresh 없음 |
| `CubelessBlueprintTools.compile_and_save_blueprint` | write | author-D, reload-D | save=True 실제 검증; save=False 분기는 다른 compile wrapper와 이름만으로 동일 검증 취급하지 않음 |
| `CubelessBlueprintTools.compile_and_validate_blueprint` | write | additional-calls-E2 | save=False/refresh=True 실제 검증; 다른 옵션 조합의 batch 적합성은 추가 검증 필요 |
| `CubelessBlueprintGraphTools.inspect_graph` | read/load | author-D, reload-D | 최대 2048 K2 node, comments 제외; node ID는 object path |
| `CubelessBlueprintGraphTools.add_branch_node` | write | author-D | 원본 branch의 부분범위. GUID/type selector, graph 생성 없음 |
| `CubelessBlueprintGraphTools.set_node_position` | write | author-D | 단일 위치 변경. 원본 `set_blueprint_node_positions` 다중 사전 검수 계약의 대체가 아님 |
| `CubelessBlueprintGraphTools.delete_node` | write | author-D | K2 node 삭제, exec 연결 삭제 opt-in; 원본 전체 옵션 동등성 미완료 |
| `CubelessBlueprintGraphTools.connect_graph_pins` | write | author-D | exec↔exec, replacement 거부; typed/conversion, GUID selector 등 원본 전체 계약 미완료 |

권장 최초 구현은 이 목록 중 직접 합성 검증까지 끝난 범위만 manifest에 enable한다. 아직 검증하지 않은 옵션·대상·확장 domain은 capability를 추가하며 검증 범위를 명시한다. 실제 registry 자동 열거 결과를 실행 허용 목록으로 쓰지 않는다.

현재 native diagnostics/dependency audit/SkeletalMesh 조회는 개별 증거가 있지만 초기 batch의 최소 범위에는 포함하지 않는다. 필요하면 별도 읽기 adapter·bounded-result 계약을 작성한다. native `CompileBlueprintValidation` 직접 쓰기는 Python journal을 우회하므로 child route로 열지 않고 Python compile action을 경유한다. 신규 Material 도구는 이 문서 작성 시 별도 이식/검증 흐름이 진행 중이다. discovery/negative guard만으로 mutation/readback/save/reload 동등성 검증 완료를 판단하거나 batch allowlist에 자동 추가하지 않는다. 공식 기본 도구로의 동등 대체는 본 조사에서 확인하지 않았다.

### root journal과 공통 action

현재 `operations/state.py`의 장점은 요청 ID와 인수 fingerprint, exclusive create+fsync, 완료 응답의 재실행 없는 replay, 중단 상태 거부, 완료 기록 실패를 성공으로 감추지 않는 처리다. `checkpoint(result)`는 현재 `_active_operation`과 같은 ID, 같은 PID, `started` record 소유권을 확인하고 atomic replace로 snapshot을 기록한다.

그러나 `_active_operation`은 **해당 Python 프로세스 안의 Cubeless mutation gate**다. 다른 에디터 프로세스의 다른 ID, 공식 기본 도구, 직접 노출 native 쓰기, 사용자 UI까지 잠그는 전역 엔진 mutex가 아니다. 동일 ID의 exclusive journal create는 중복 요청 방지이지 모든 작업 직렬화의 대체가 아니다.

안전한 합성을 위해 다음을 구현해야 한다.

1. 기존 mutation 함수의 검수/작업/후조건 action을 명시적 내부 core로 분리한다. standalone wrapper는 기존처럼 `mutate`를 한 번 사용한다. batch도 root `mutate`를 한 번 사용하고 허용 core만 dispatch한다. child로 등록된 public wrapper를 재귀 호출하거나 `getattr`로 임의 이름을 실행하지 않는다.
2. root operation context가 child index/ID, before 관측, attempted 단계, child 결과, 다음 실행 위치를 소유한다. child ID는 보고용(`batchID:index`)이며 독립 replay나 journal 소유자가 아니다. 기존 child `checkpoint(child_result)`를 그대로 호출하지 말고 root snapshot으로 올리는 명시적 checkpoint callback을 사용한다.
3. 다음 변경 전에 root snapshot을 durable checkpoint하고, child 완료 후 관측 결과도 checkpoint한다. 중간 저장 실패면 다음 mutation에 진입하지 않는다. checkpoint가 있다는 이유로 이후 미기록 변경이 없다고 주장하지 않는다.
4. 시작/중단 journal의 `recorded_result_scope`, 마지막 checkpoint stage와 sequence, 현재 실제 결과가 unknown일 수 있다는 설명을 유지한다. 아직 `changes=[]`였던 checkpoint도 “실제 변경 없음” 증거가 아니다. `state='complete'`는 결과 기록이 끝났다는 뜻이며 성공과 별개다.
5. 같은 ID+같은 인수의 complete replay는 저장된 결과를 돌려준다. 새 조회나 engine 재실행이 아니다. read-only batch에도 이 차이를 문서화한다. started/다른 fingerprint는 자동 실행하지 않는다. operation ID 미지정 시 생성 ID를 반환하되 timeout으로 ID를 받지 못할 위험 때문에 외부 pipeline은 명시 ID를 권장한다.

현 state의 complete 저장은 최초 record 객체에서 최종 result를 기록하므로 중간 checkpoint sequence metadata를 최종 record에 보존하지 않는다. 완료 result 자체는 기록된다. batch에서 checkpoint 이력을 보고하려면 최종 data에도 필요한 cursor/summary를 명시적으로 보존해야 한다. full graph snapshot을 매번 fsync하면 기록 비용이 커질 수 있으므로 명확한 크기 제한과 compact summary+관련 before/after 범위가 필요하다. 관련 검수 근거를 버리는 임의 truncation은 하지 않는다.

### failure·compile/save·결과 계약

공통 reflected result의 `success/stage/changes/compile/save/validation/retry/data`를 유지하고 `data.batch`에 원본 summary 필드와 input-order child 결과를 둔다. 각 child는 index, canonical ID, effect, 호출/변경 시도, tri-state 실제 변경, stage, error code, before/after, compile/save/검증 상태, elapsed를 보존한다. `executed`는 원본의 visited-row 의미를 유지하며 `dispatched_count`와 `not_run`을 별도로 둔다.

`success`는 모든 요청 entry가 실행되어 요청된 검수까지 통과했을 때만 true다. prefix가 성공했어도 후반 failure, 결과 unknown, recovery-required는 전체 실패다. compile/save 상태는 마지막 child 한 개로 덮어쓰지 않고 에셋별 이력/summary를 합산한다. compile은 변경 operation이며 저장하지 않아도 write다. save 실패 뒤 재실행하면 앞선 그래프 변경을 중복할 수 있으므로 단계별 관측과 수동 복구 근거를 제공한다.

`stop_on_error=True`는 첫 child failure에서 중단한다. `False`는 결과와 변경 범위가 확정된 실패 뒤 다음 action 실행을 허용한다. **unknown mutation/in-flight, journal checkpoint 실패, 취소·deadline에 따른 중단, session/project guard 실패는 False라도 강제 중단**을 제안한다. 이는 원본의 무조건 계속하기보다 엄격한 변경이며 `stop_reason`, `forced_stop`, `not_run`으로 명시한다. 조용히 동일 동작이라고 주장하지 않는다.

Undo 그룹을 지원하려면 설치 UE58 공개 `unreal.ScopedEditorTransaction`을 사용한다. `PyEditor.cpp:88`의 BeginTransaction, `:97`의 EndTransaction과 `:108`의 CancelTransaction 구현을 확인했다. `__exit__`는 예외 발생 시 자동 Undo하지 않는다. cancel을 rollback API라고 설명하지 않는다. 실제 그룹 Undo 검증 전에는 한 번의 Undo로 모든 작업을 되돌릴 수 있다고 등록 설명에 약속하지 않는다. 디스크 save는 별도의 지속 변경이다.

### 성능·장시간 작업

하나의 공식 HTTP/MCP 요청으로 round trip 수를 줄인다. 각 mutation은 공식 게임 스레드 실행 규칙에 맞춰 직렬화하며 compile/save는 입력에 명시된 횟수만 호출한다. 별도 최적화된 Blueprint domain pipeline은 graph 편집→compile 1회→save 1회→검수로 구성할 수 있으나, 일반 batch의 요청 순서를 임의 재작성하지 않는다.

max 100개는 원본 호환 개수 제한일 뿐 프레임 시간 안전성 보장이 아니다. 최초 구현은 작은 bounded action에 한정하고 action 간 budget 검사만 가능하다는 점을 명시해야 한다. 동기 엔진 compile 한 번을 Python에서 강제 중단하거나 worker thread로 넘겨 안전성을 보장하지 않는다. 공식 native async result 경로는 별도 조사·구현·실행 검증 후 확장한다. 장시간 batch의 비동기 완료·게임 스레드 분할은 현재 미구현이며 이것이 필요한 경우 기능 미완료로 남긴다. 본 조사에서 처리량/latency는 측정하지 않았다.

## 이식 분류와 완료 조건

| 구분 | 분류/현 상태 |
| --- | --- |
| 순차 domain batch, partial result, compile/save 경계, 검수 합산 | cubeless에서 구현할 대상. 현재 설계만 작성, 미완료 |
| 원본에 이미 있는 특화 `set_blueprint_node_positions` 등의 다중 검수 작업 | 각각 독립 이식 대상. generic batch 또는 단일 move 반복으로 완료 처리 금지 |
| 공식 5.8 도구로 동등하게 대체 | 이 조사에서 입출력·오류·검수 동등성까지 확인한 항목 없음 |
| 외부 FastMCP server, 자체 socket bridge, `__mcp_ticker_dispatch_effective`, bridge 환경 timeout | 새 구조의 필수 경로에서 제거. client timeout 관측 의미는 유지 |
| 완료/replay 가능한 root journal | 새 구조의 강화 기능. 기존 state 재사용 가능하지만 nesting refactor·batch 검증은 미완료 |
| 전체 legacy raw 명령/긴 async batch/모든 Undo 보장 | 현재 미완료. unsupported 및 명확한 검증 한계로 표시 |

구현 순서: action core 분리와 standalone regression → root batch journal 및 bounded allowlist → 작은 read/write batch 실제 등록·호출 → 실패·불명확·replay·checkpoint 반례 → 저장·새 프로세스 reload → 실제 Undo 범위 → 동일 입력 individual/batch 성능 비교 → 검증된 domain별 allowlist 확대.

필수 검증:

- 실제 공식 MCP 등록 스키마의 필수/기본/nullable 확인, omitted ID와 명시 ID, 반환 구조 검증.
- 전체 payload 중 뒤쪽 invalid/unsupported/nested/unknown key가 있어도 앞선 write가 실행되지 않음. 0/101개 및 JSON 중복 key 검증.
- 순서와 앞선 create→후속 compile 대상 해석; absent/generated node ID를 임의 해석하지 않음.
- 중간 확정 실패의 `stop_on_error=True/False`, evaluated/dispatched/skipped count 및 partial result 일치.
- 엔진 API 성공 뒤 readback 실패, 응답 손실, journal 실패/프로세스 중단의 unknown·in-flight·재시도 금지. False라도 강제 중단할 조건 확인.
- 완료 replay에서 실제 추가 변경 없음, 충돌 ID 거부, interrupted ID 거부, nested gate regression, 다른 프로세스 같은 ID race.
- compile 실패 시 save 미수행, save 실패 시 compile/변경 사실 보존, 에셋별 compile/save 합산, 저장 뒤 별도 process reload.
- 실제 transaction 가능한 graph fixture에서 한 Undo의 관측 범위. 이미 저장한 디스크 파일 원복이라고 주장하지 않음.
- 원본 `smoke_live_bridge.py:250`의 read/read 성공과 nested 거부 및 `test_mutation_result_contract.py:86`의 known/unknown/contradictory child 합산 반례를 새 계약에 맞게 실행. 원본 파일 복사만으로 완료 처리하지 않음.
- 성능은 같은 새 에셋/노드 수, cold/warm 상태, 고정 compile/save 횟수, engine/version/endpoint/transport, journal 크기와 fsync 포함 여부를 기록해 individual N회와 batch 1회의 총 시간·엔진 시간·응답 크기를 비교. 절약량을 아직 측정한 것처럼 쓰지 않음.

이 두 조사 파일 외의 소스 변경, 엔진 실행, 에셋 변경, 빌드, Git commit/push는 이 조사에서 수행하지 않았다.
