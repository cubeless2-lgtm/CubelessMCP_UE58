# Python 공통 작업 경로와 저널 최적화 G

## 이미 공유하는 실제 작업

| 공개 진입점 | 공통 core | 실행 의미 |
| --- | --- | --- |
| Blueprint compile / compile-and-save / compile-and-validate | `operations.blueprint.compile_blueprint` → `compile_blueprint_action` → `compatibility.ue58.blueprint.compile_validation` → native `CompileBlueprintValidation` → UE58 `CompileBlueprint` | wrapper별 `save`/`refresh_nodes` 기본값만 다르다. 한 호출은 native compile 한 번, 요청된 성공 후 save 최대 한 번이다. |
| 개별 Blueprint/graph 도구와 batch allowlist | 동일한 `*_action` 함수 | standalone은 자기 mutation journal을 소유하고, batch는 root journal과 각 단계 checkpoint를 소유한다. batch가 개별 공개 wrapper를 다시 호출하거나 중첩 journal/잠금을 만들지 않는다. |
| Material 6 mutation | `operations.material._mutation` + 개별 native 작업 | 공통 before snapshot, canonical path, durable checkpoint, native 결과/실패 반영을 공유한다. 각 native 작업의 구체적 속성·참조·연결 검사는 유지한다. |
| 모든 Python mutation | `state.mutate`, `state.checkpoint`, `_write_record` | replay/충돌/중단/배타 생성/기록 완료 처리의 단일 구현이다. |

native `CubelessNativeToolset.CompileBlueprintValidation`의 직접 MCP 노출과 세 Python 호환 wrapper는 같은 native compile 구현을 사용하지만 계약은 다르다. native 직접 호출에는 Python operation ID/journal/부모 체인 검사/`/Game` 경로 제한이 없다. 이름이 비슷하다는 이유로 제거하거나 완전 동등한 대체로 분류하지 않는다. 에디터 game-thread/PIE 검사는 공개 native 호출 경계를 보호하므로 Python 호출 전 검사와 중복되어 보이더라도 삭제하지 않는다.

compile 전·후 `snapshot`은 부모 체인과 dirty 상태 등을 각각 관측한다. 이 두 관측은 같은 시점의 중복 결과가 아니므로 캐시로 대체하지 않는다. batch 안에 명시된 compile/save 단계를 합치거나 건너뛰면 중간 단계 결과·오류·저장 상태와 원본 순서 계약이 바뀐다. 이번 최적화는 그 동작을 바꾸지 않는다.

## 적용한 공통 저널 최적화

`operations/state.py`의 초기 exclusive journal 및 checkpoint/finalize 임시 파일 쓰기를 다음처럼 변경했다.

```python
encoded = json.dumps(record, ensure_ascii=False, sort_keys=True)
with path.open("x", encoding="utf-8") as stream:
    stream.write(encoded)
    stream.flush()
    os.fsync(stream.fileno())
```

checkpoint/finalize에서는 기존 UUID 임시 파일과 `os.replace`를 그대로 유지한다. 이전 `json.dump`의 Python iterator와 수많은 작은 stream write를 complete encoder + single write로 바꿨다. JSON options, 정렬, 값, fingerprint, checkpoint 횟수와 의미, durable flush/fsync, process ownership 검사, exclusive create 경쟁, 완료 기록 실패 처리, 재실행 차단은 그대로다. 공개 도구를 삭제하지 않았다.

전체 직렬화 문자열 한 개를 추가로 메모리에 유지한다. 이 최적화는 메모리 사용 감소를 주장하지 않는다. 파일 시스템 장애나 전원 손실에 대한 새로운 원자성 보장도 추가하지 않는다.

### 오류 시점의 변화

- 초기 encoder 실패: exclusive create 전에 실패하므로 빈/불완전 journal 파일을 새로 만들지 않는다. action은 실행하지 않고 `_active_operation`을 점유하지 않는다. 다른 프로세스와의 경쟁은 여전히 `open('x')`로 결정되며, 선점한 타 프로세스의 파일을 덮어쓰지 않는다.
- checkpoint encoder 실패: 이전 durable checkpoint는 그대로이고 임시 파일도 만들지 않는다. `journal_checkpoint_failed`가 다음 mutation을 막는다.
- final encoder 실패: 이전 started checkpoint를 보존하고 `journal_finalize_failed`, 불확실한 완료, 재실행 차단을 유지한다. 엔진에서 수행된 변경을 롤백했다고 주장하지 않는다.
- write/flush/fsync/replace 실패 경로는 기존 실패 처리와 소유권 규칙을 사용한다.

## 측정

실제 `batch-author-G`의 원본 journal 세 개를 읽기만 했다. 입력 파일을 수정하지 않고 OS tempfile 디렉터리(C: 볼륨)의 폐기 파일에 A/B를 수행했다. 각 파일에서 30회씩 교대 순서로 측정하고, encoder 측정 한 sample마다 100회 반복했다. durable sample에는 serialize/exclusive temporary write/flush/fsync/replace를 포함한다. 각 결과 바이트를 원본과 비교하고 원본 SHA/bytes 불변을 확인했다.

| 실제 입력 | 크기 | stream write 수 dump→dumps | encode 중앙값 ms | durable 중앙값 ms |
| --- | ---: | ---: | ---: | ---: |
| standalone create | 2,214 B | 241→1 | 0.046259→0.012850 | 1.5610→1.4448 |
| lifecycle batch | 11,352 B | 1,439→1 | 0.419033→0.063865 | 21.5449→2.0364 |
| graph-edit batch | 40,108 B | 5,749→1 | 2.355596→0.257202 | 22.1875→20.6025 |

인코딩 중앙값 감소는 약 72.2/84.8/89.1%다. durable write에는 큰 지터가 있었고, 특히 중간 크기의 90% 감소를 일반 성능 개선으로 해석하면 안 된다. 프로세스/스토리지 환경은 측정 JSON에 기록했다. 이 오프라인 결과는 MCP 왕복·Unreal API·전체 배치 지연을 측정하지 않는다. 실제 G의 기존 1회 lifecycle 비교는 batch 0.209543s/1회 호출 대 standalone 0.156115s/3회 호출이므로 통신 횟수 감소만 확인하며 속도 향상을 주장하지 않는다. 새 source의 실제 엔진 성능은 후속 실행으로 분리한다.

재현 스크립트: `cubeless/tools/verification/benchmark_journal_serialization.py`. 결과: `records/verification/implementation-2026-10-10/journal-serializer-benchmark-G.json`.

## 검증

- 플러그인 Python 테스트 39개 통과: batch stop/unknown/partial/replay/busy/crash, graph readback 실패, journal 경쟁/중단/완료 실패 포함. 초기·checkpoint·final encoder 실패 반례 3개 추가. `journal-serializer-plugin-tests-G.log`.
- Material wrapper/native 모형과 실제 journal 테스트 22개 통과. `journal-serializer-material-tests-G.json`.
- 실제 UE 호출은 root 소유로 별도 실행한다. 여기서 Python 테스트를 엔진 검증으로 표현하지 않는다.

## 남긴 후보

- `delete_node_action`의 `before=graph_info(editor)` 직후 `target=node_info(node)`는 같은 노드의 핀을 다시 읽는다. `before.nodes`의 해당 object-path 항목을 선택하면 동일 시점의 관측을 재사용할 수 있다. 현 단계에서 제거하지 않았으며, 유일성/노드 누락 실패와 기존 partial evidence를 유지하는 반례가 필요하다.
- add-node의 단독 node read와 전체 after graph read도 겹치지만, 후자가 실패했을 때 앞선 node identity/좌표가 실제 변경 증거로 남는다. 단순 제거하면 실패 보고가 약해지므로 변경하지 않았다.
- batch의 결과 크기 확인은 checkpoint 직후 전체 JSON을 다시 인코딩한다. checkpoint가 직렬화 바이트 수를 내부적으로 안전하게 반환하는 별도 설계는 가능하지만, checkpoint API/공개 결과/중단 의미를 바꾸지 않는지 검증해야 한다. 현재 유지한다.
- batch의 `_aggregate` 및 checkpoint read/deepcopy는 누적 결과를 반복 처리한다. ownership 재검증과 결과 checkpoint를 생략하지 않고도 최적화하려면 더 큰 실제 workload profile이 먼저 필요하다. 현재 3개의 작은 journal만으로 병목이라고 단정하지 않는다.

## Source freeze

| 파일 | SHA256 |
| --- | --- |
| Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/operations/state.py | d9d63886ecd6348922aa95692d362500b04d149950bd5020c87357f6dabdca06 |
| Plugins/CubelessMCP_UE58/Content/Python/tests/test_operation_journal.py | ccf582eec6e26b4d1d09d5adc6708888d6f3d73ebdab3aadc6e8b32c887193ac |
