# Blueprint batch 외부 파이프라인 계약

`cubeless/tools/pipelines/blueprint_batch_smoke.py`는 공식 MCP HTTP client이며 별도 MCP 서버가 아니다. 도구 이름/입력 필드는 실제 Registry에서 검색한다. 원본 batch의 모든 명령을 이식한 파이프라인은 아니며 현재 `blueprint-graph-v1` 10개 child action의 일부 경로만 검증한다. 플러그인 계약은 `batch-implementation-F.md`를 따른다.

입력은 `--endpoint`, `--expected-project`, `--phase author|reload`, 새 `--output`이고 reload에는 실제 `--parent-report` 바이트가 필요하다. Author는 `/Game/CubelessMCPTests/Batch_<uuid>/`에만 BP_Batch/BP_Standalone/BP_StopTrue/BP_StopFalse를 만든다. BP_UnsupportedNever는 배열 upfront rejection 때문에 존재하지 않아야 한다. 기존 경로/보고서를 덮어쓰지 않는다. 프로세스 실행/종료는 수행하지 않는다.

Author의 순서는 다음과 같다.

1. UE5.8.3·정확한 프로젝트·게임 스레드·PIE false·dirty 없음 확인.
2. 단일 batch로 Actor Blueprint create→compile/save→inspect. 별도의 동일 작업량 standalone 경로를 create, compile/save, inspect 세 번으로 수행.
3. 동일 ID/동일 args는 정확한 과거 결과 replay, 같은 ID/다른 args는 무변경 conflict. 별도 inspect로 batch 자산 확인.
4. 지원하지 않는 명령이 포함된 배열은 prefix create까지 실행되지 않음. 공식 AssetTools.exists로 부재 확인.
5. [create, 같은 이름 create, compile/save, inspect]에 stop_on_error=True/False 각각 수행. True는 두 번째에서 종료하고 뒤 단계가 not_run, 생성 prefix는 dirty로 남음. 별도 명시 compile/save로 보존. False는 확정 duplicate 실패만 지나 compile/save/inspect를 수행하되 전체 결과는 실패/partial mutation으로 유지.
6. Branch 두 개 add+inspect batch. move→exec connect→inspect→delete→inspect batch. source/target의 양방향 연결과 위치, delete 후 노드/연결 부재 확인. 별도 inspect와 동일한 결과인지 비교.
7. 남은 그래프를 compile/save+inspect batch로 저장. 저장 그래프가 삭제 직후 독립 readback과 완전히 같고 모든 fixture package가 clean인지 확인.

Reload는 author 입력 해시와 editor identity를 묶고 새 instance를 요구한다. 한 read-only batch에서 4개 Blueprint identity/parent/clean 상태와 저장 그래프를 읽는다. compile/save를 요청하지 않고 끝의 dirty 상태도 확인한다. 같은 process를 reload라고 인정하지 않는다.

반환 schema는 `cubeless_blueprint_batch_pipeline_v1`이며 24개 author stage 또는 3개 reload stage를 정확한 순서로 기록한다. 각 결과는 root 성공뿐 아니라 child type/index/phase/status/count, skip, forced stop/unknown, retry, transaction 한계를 포함한다. CLI는 전송 오류나 결과 불확실한 mutation을 자동 재시도하지 않고 partial 보고서를 보존한다. 원자적 롤백이나 actual Undo는 제공/검증하지 않는다.

독립 검수기는 `cubeless/tools/verification/batch_pipeline_report.py`다. 부모 report의 status만 믿지 않고 모든 단계·입력 fingerprint·child 결과·실제 readback을 검사한다. compiler의 compiled뿐 아니라 error_count/status/validation_pass/compile_errors와 save 상태/after dirty도 확인한다. 그래프 node_count/identity와 모든 link의 상대 노드·핀/역방향을 확인해 dangling/일방향 링크를 거부한다. 저장 중 그래프가 바뀌거나 clone된 잘못된 저장 결과가 reload에서 반복되는 경우도 삭제 직후 readback과 연결해 거부한다.

`test_batch_pipeline.py`의 9개 합성 테스트는 위 반례를 검증한다. fake SDK는 소스에서 schema signatures를 읽지만 Unreal 실행 증거는 아니다. 실제 author G는 24개 단계가 통과했고 더 엄격해진 검수기로 `operations-batch-author-G-independent-R2.json`을 새로 생성해 PASS했다. 최초 G 보고서는 수정하지 않았다. Cold reload는 별도 실제 증거가 생기기 전까지 미검증으로 남긴다.

성능은 create→compile/save→inspect의 동일 action 3개, 명시 compile/save 각 1회인 구간만 비교한다. G에서 batch HTTP 1회 0.209543초, standalone HTTP 3회 합계 0.156115초였다. 각 1회, batch가 먼저 실행되었고 cache/disk/warmup 차이를 통제하지 않았다. 통신 호출 감소는 관찰했지만 속도 향상을 주장하지 않는다. 전체 반례 파이프라인의 추가 compile/save와 이 비교 구간을 혼동하지 않는다.

검증 명령 예시는 실제 환경별 endpoint/project/report 경로를 채워 사용한다.

```text
python -B cubeless/tools/pipelines/blueprint_batch_smoke.py --endpoint <official endpoint> --expected-project <isolated uproject> --phase author --output <new author report>
python -B cubeless/tools/verification/batch_pipeline_report.py <author report> --phase author --output <new independent report>
python -B cubeless/tools/pipelines/blueprint_batch_smoke.py --endpoint <official endpoint> --expected-project <isolated uproject> --phase reload --parent-report <author report> --output <new reload report>
python -B cubeless/tools/verification/batch_pipeline_report.py <reload report> --phase reload --parent-report <author report> --output <new independent report>
```
