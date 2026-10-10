# 첫 Blueprint/graph batch 구현 F

2026-10-10. 소스 구현과 외부 Python 합성 검증까지 진행했다. **이 기록 시점에는 UE58 batch 등록·호출·저장·재로드·Undo·성능 검증을 하지 않았다.** 실제 엔진 실행은 root 담당이다. 기존 `batch-contract-and-port.md/.json`은 구현 전 조사 당시의 기록으로 유지한다.

## 등록 계약

`CubelessBatchTools.execute_batch(commands_json: str, stop_on_error: bool = True, operation_id: str | None = None)`를 공식 Registry 등록 목록에 추가했다. 기존 개별 도구를 제거·대체하지 않는다. Python 등록이 성공하면 도구 1개가 증가한다. str/bool/nullable reflected 입력을 사용하며 실제 생성 스키마는 다음 에디터 시작 후 확인해야 한다.

```json
[
  {"type":"CubelessBlueprintTools.create_blueprint","params":{"name":"BP_Batch","parent_class":"Actor","package_path":"/Game/CubelessMCPTests/MyRun"}},
  {"type":"CubelessBlueprintTools.compile_and_save_blueprint","params":{"blueprint_name":"/Game/CubelessMCPTests/MyRun/BP_Batch"}},
  {"type":"CubelessBlueprintTools.inspect_blueprint","params":{"blueprint_name":"/Game/CubelessMCPTests/MyRun/BP_Batch"}}
]
```

위 배열을 JSON **문자열**로 `commands_json`에 전달한다. 예시는 아직 실행 결과가 아니다. operation_id는 root batch에 하나만 지정한다. child params의 operation_id, 임의 key, 알 수 없는 명령, 중첩 batch는 전체 실행 전에 거부한다. 1~100개, 입력 최대 1 MiB이며 JSON 중복 key/NaN, 잘못된 타입, int32 범위 초과도 거부한다. asset 존재·parent chain·graph/pin/기존 좌표·exec 연결 등의 엔진 상태 검수는 각 action 직전에 기존 코드를 그대로 실행한다. 앞선 create의 결과 asset을 다음 compile에서 찾을 수 있다.

허용 목록은 고정된 10개다.

- `CubelessBlueprintTools`: create_blueprint, inspect_blueprint, compile_blueprint, compile_and_save_blueprint, compile_and_validate_blueprint.
- `CubelessBlueprintGraphTools`: inspect_graph, add_branch_node, set_node_position, delete_node, connect_graph_pins.

각 params 이름·필수값·기본값은 현재 개별 등록 함수와 동일하다(operation_id 제외). AST 비교 테스트로 10개 모두 대조했다. create package 기본 `/Game/Blueprints`, compile_and_save의 save 기본 True, compile_and_validate의 save=False/refresh_nodes=True를 유지한다. read-only child도 root gate/journal 안에서 순서대로 실행한다.

## 내부 구조와 실패 결과

- `registration/batch_toolset.py`: 공식 reflection 및 설명. 개별 Blueprint/graph 등록 함수의 서명은 변경하지 않았다.
- `operations/blueprint.py`, `operations/graph.py`: 기존 closure를 `*_action` 공통 함수로 분리했다. 기존 standalone 함수는 동일 arguments fingerprint와 `mutate()` 한 번을 유지한다. batch는 이 내부 action을 직접 호출한다.
- `operations/batch.py`: 명시적 capability manifest, 전체 admission 검수, root journal, 순서/중단/합산. `getattr`, 범용 Python 실행, 임의 Toolset 호출 경로는 없다.
- `compatibility/ue58/batch.py`: 설치 엔진에서 확인한 공개 `unreal.ScopedEditorTransaction`만 감싼다. 엔진 버전별 API 위치를 분리했다.
- `operations/state.py`: **변경하지 않았다.** 기존 exclusive journal, checkpoint 소유권, 불완전 요청 거부, replay API를 그대로 사용하므로 Material checkpoint 계약도 동일하다.

root mutation 하나가 gate와 journal을 소유한다. `batchID:index`는 child 보고 ID이며 별도 journal을 만들지 않는다. 각 step 의도, 실제 엔진 쓰기 직전 before/의도, child 결과를 root checkpoint로 기록한다. 실제 engine dispatch 전 checkpoint에서는 `changes[].attempted=False`, `intent_prepared=True`와 row `phase=mutation_intent`를 기록한다. checkpoint 성공 뒤에만 엔진 호출 시도를 표시한다. checkpoint 실패는 “변경했다”로 잘못 보고하지 않는다. 하지만 프로세스가 이후 종료되면 checkpoint 뒤의 실제 결과는 여전히 unknown이다.

`data_json.batch`에는 원본과 같은 total/executed/succeeded/failed/stopped_at_index/stop_on_error/results와 추가 dispatched_count/not_run/forced_stop/stop_reason/tri-state mutation/unknown/partial/in-flight 정보를 둔다. 각 row의 response는 개별 operation의 stage/changes/compile/save/validation/retry/data를 보존한다. 원본 bridge의 status/result 중첩 wire envelope를 억지로 복제하지 않으므로 외부 검수기는 새 위치를 읽어야 한다. compile/save는 root에서 entries로 모두 합산하며 마지막 child가 이전 결과를 지우지 않는다.

stop_on_error=True는 첫 failure에서 중단한다. False는 변경 범위가 확정된 오류에서 계속할 수 있다. native compile 오류처럼 실제 mutation 여부와 compile/save 결과가 확인된 실패도 이에 포함된다. 엔진 쓰기 뒤 readback 실패, unknown/in-flight, journal 오류, PIE guard 실패는 False라도 강제 중단한다. 성공 child가 in-flight marker를 주장하는 모순도 전체 성공으로 처리하지 않는다. 자동 retry/resume는 없다.

같은 ID+정규화된 같은 인수는 complete 결과를 돌려준다. 기본값 생략과 같은 기본값 명시는 같은 fingerprint다. 읽기 batch도 같은 ID이면 과거 관측을 반환하며 새 조회를 수행하지 않는다. 다른 인수의 동일 ID 또는 interrupted ID는 거부한다. journal finalization 실패도 전체 성공으로 반환하지 않는다.

## 원본과의 차이 및 한계

1. 원본 raw bridge 명령 배열 대신 canonical Cubeless ID 배열의 JSON 문자열이다. legacy 별명/자동 파라미터 변환과 외부 timeout 인수는 없다. 외부 HTTP timeout이 엔진 취소를 뜻하지 않는다.
2. 원본은 dispatcher의 거의 모든 raw 명령을 허용했지만 현재는 위 10개만 허용한다. Material/native 직접쓰기/공식 임의 도구/전용 TCP/execute_python은 포함하지 않는다. generic batch를 나머지 기능 이식으로 계산하지 않는다.
3. 기존 graph 부분범위를 그대로 유지한다. K2/기존 graph/object-path ID, exec↔exec 및 교체 거부 조건이 적용된다. 원본 다중 위치 이동의 전체 사전 검수·GUID selector·typed conversion은 별도 미완료다.
4. 입력 순서를 재배열하거나 compile/save를 자동 추가·합치지 않는다. 결과 ID 참조 문법도 없다. node 생성 결과를 이용한 다음 단계는 명시적으로 별도 요청해야 한다.
5. 원본의 False 무조건 계속보다 unknown/journal/session 실패에서 엄격하다. 결과의 forced_stop 및 stop_reason으로 드러낸다.
6. 15초는 **step 사이**의 누적 실행 budget이다. 현재 engine API가 실행 중이면 선점/취소할 수 없다. 첫 동기 compile이 오래 걸리면 UI가 그동안 대기할 수 있다. 공식 async 진행/완료/게임 스레드 분할은 아직 미구현이다.
7. 관측 결과가 8 MiB를 넘으면 그 결과를 보존하고 추가 step을 중단한다. 이는 강제 출력 truncate나 단일 action 결과 크기의 hard cap이 아니다. checkpoint는 fsync 비용을 포함한다. 성능은 아직 측정하지 않았다.
8. write가 하나라도 있으면 하나의 ScopedEditorTransaction을 연다. 실패 시 Undo/rollback을 실행하지 않는다. 모든 action이 한 Ctrl+Z로 복구되는지 아직 실측하지 않았다. 저장된 디스크 파일 rollback은 제공하지 않는다.
9. 프로세스 내 Cubeless gate는 공식 기본 도구, 다른 프로세스의 다른 ID, 사용자 UI까지 잠그는 엔진 전역 mutex가 아니다. 한 소유자가 에디터 mutation을 직렬화하는 운영 규칙은 계속 필요하다.

## 합성 검증과 다음 실제 검증

Python 3.11.15의 `-B -m unittest discover`로 실행했다. fake Unreal/API는 합성 failure injection용이며 UE 동작을 입증하지 않는다.

- 플러그인 tests 35개 통과: 신규 batch 20개, 기존 graph failure 5개, journal 10개. 순서, 동일 ID/default replay, unsupported 전체 거부, True/False 중단, native compile/save 실패, 엔진 변경 뒤 readback 실패, checkpoint 전/후 실패, process interruption, 기존 mutation_busy, 입력 스키마 대조를 포함한다.
- 별도 Material wrapper 계약 14개 통과. 기존 checkpoint API와 9개 Material 등록 함수 계약을 유지하는지 확인했다.

명령:

```powershell
& 'C:\Git\UnrealMCP5_6_1\Python\.venv\Scripts\python.exe' -B -m unittest discover -s '<project>\Plugins\CubelessMCP_UE58\Content\Python\tests' -v
& 'C:\Git\UnrealMCP5_6_1\Python\.venv\Scripts\python.exe' -B -m unittest discover -s '<project>\cubeless\tools\verification' -p 'test_material_python_contracts.py' -v
```

다음 root 검증: 실제 schema/omitted ID → 격리 에셋 create/compile/save/readback 1 batch → 동일 ID replay/충돌 → unsupported 뒤 prefix 없음 → 확정 실패 True/False → 새 프로세스 reload → 실제 graph 편집 batch → 단일 Undo의 실제 범위 → 개별 호출과 같은 조건의 latency 비교. 기존 개별 Blueprint author/reload regression도 새 소스가 로드된 에디터에서 다시 확인해야 한다.

이 구현은 C++/엔진/게임플레이 소스와 기존 사용자 에셋을 변경하지 않았다. engine 호출·빌드·Git commit/push는 수행하지 않았다. source freeze 해시는 `cubeless/records/verification/implementation-2026-10-10/batch-source-freeze-F.json`에 둔다.
