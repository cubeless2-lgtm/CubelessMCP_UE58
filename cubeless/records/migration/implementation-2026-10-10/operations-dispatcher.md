# UE58 offline verification dispatcher 이식 기록

2026-10-10. 대상은 `OPS56-VERIFY-001`의 외부 Python 제어 흐름이다. 구현 파일은 `cubeless/tools/pipelines/verification_dispatcher.py`, 자체 검수기는 `cubeless/tools/verification/test_verification_dispatcher.py`다. **디스패처의 작은 실행 범위는 이식·실행 검수했지만, 기존 42개 gate와 9개 profile의 전체 이식은 미완료다.**

## 원본 보존과 현재 범위

원본 `Python/scripts/analysis/run_verification.py`의 SHA-256은 `2568476fee409ea43f774035bf673f2a513d6de39cca3f7fc4d254783c3549c9`다. 전수 분석 JSON에서 AST로 추출했던 42개 key/label/argv/source line/source script identity/proposed mapping와 9개 profile의 순서를 새 파일의 `LEGACY`에 정확히 내장했다. canonical manifest SHA-256은 `0d45de1b90aab8faef004356a52e7024b78cdcf06793a8a7c6f4b8b082b400c2`, profiles SHA-256은 `881f4da6d8d7ba62103ff10019fda25ef91288c9aaf73520a288c2171cc36329`이며 자체 테스트가 잠근다. 원본 저장소나 대용량 inventory JSON은 실행 의존성이 아니다.

기존 `full`, `core`, `catalog`, `transport`, `schema`, `niagara`, `write-observer`, `release-offline`, `windows-transport`는 이름과 순서를 유지한다. 모든 원본 gate의 현재 대응 상태는 명시적으로 `pending`이다. 이 프로필들의 실행 결과는 `incomplete`, Python 실제 종료 코드는 **2**, child 실행 수는 0이다. 새 기능을 원본 gate와 동등하다고 임의로 연결하지 않았다. `verification_dispatcher_contract` 자체도 원본 검사 전체 동등성을 주장하지 않고 남겼다.

기존의 profile 선택 → ordered plan → subprocess 직렬 실행 → 첫 실패에서 중단 → 실제 실패 코드 반환을 유지했다. 새 결과 파일·로그·입력 해시·실행기 fingerprint를 추가했으며, 과거 `uv`/외부 TCP 환경용 argv는 역사 데이터로만 유지한다. 실행할 수 있는 명령은 현재 로컬 검수기의 고정 목록이다.

## 새 프로필과 입출력

| 새 프로필 | 실제 검수 순서 | 필수 입력 | 통과가 뜻하는 범위 |
| --- | --- | --- | --- |
| `bootstrap-offline` | pipeline contract tests → journal unit → package checker tests → marker self-test → dispatcher tests | 설치된 현재 검수기 | 합성 반례/외부 검수기 동작. 엔진 실행 성공을 뜻하지 않는다. |
| `blueprint-author-evidence` | `pipeline_report.py --phase author` | `--author-report` | 생성/변경/negative/readback/compile/save 기록 계약 |
| `blueprint-reload-evidence` | author 검수 → `--phase reload` 검수 → lifecycle link 검수 | `--author-report`, `--reload-report` | 각각의 검수 결과와 author 실제 바이트 SHA, run/fixture/project/parent editor 연결 |
| `blueprint-cleanup-evidence` | author 검수 → `--phase cleanup` 검수 → lifecycle link 검수 | `--author-report`, `--cleanup-report` | 정리 계약과 exact parent/run/fixture 연결. 이 작업에서 실제 cleanup은 실행하지 않았다. |
| `material-author-evidence` | `material_pipeline_report.py --phase author` | `--author-report` |9개 Material 도구의36단계 fixture, 실제 MPC 값·공식 연결 readback·반례·replay·마지막 compile/save 계약 |
| `material-reload-evidence` | Material author 검수 → Material reload 검수(실제 author 바이트 전달) | `--author-report`, `--reload-report` | saved graph, MPC 값, root 연결, instance 변경 및 exact parent SHA; 전체 Material 이식 증거는 아님 |
| `package-static-evidence` | `check_package_independence.py` 새 실행 | `--stage`, `--unrealpak`, 1개 이상 `--receipt` | stage/receipt/Pak 전체 목록 범위. UObject 직렬화 참조 전체 검증이나 packaged runtime 검증은 아니다. |
| `runtime-marker-evidence` | `validate_runtime_marker.py verify` 새 실행 | `--fixture-report`, `--baseline`, `--launch-report` | SaveGame header, 실행 파일·fixture hash, 실제 marker와 제공된 launch 기록. 패키지 플러그인 제외 증명과 분리된다. |

단계들이 읽는 parent report의 `status`만 보고 통과시키지 않는다. 각 독립 검수기를 새로운 Python subprocess로 실행해 결과·exit code를 받는다. result JSON을 생성하는 검수기는 실제 파일의 schema와 판정도 확인한다. Blueprint reload/cleanup은 공급된 author 파일과 연결을 한 번 더 확인한다. 모든 보고서가 사용자 생성 파일이므로 검수는 암호학적으로 독립된 실행 증명이 아니며, 원래 live MCP/engine/launch 증거를 보존해야 한다.

`--output`은 실행 시 필수이며 새 파일만 허용한다. 같은 stem의 `-children/` 폴더에 각 child의 stdout/stderr와 독립 result JSON을 저장한다. plan mode는 파일을 만들거나 child를 실행하지 않는다. package 검사 결과와 child 증거는 stage 바깥에만 쓸 수 있다.

## 실패·재실행·동시 수정

- pending gate 또는 누락/잘못된 입력이 하나라도 있으면 전체 profile을 preflight에서 중단하고 `incomplete`/exit2를 기록한다.
- child가 nonzero면 실제 종료 코드를 보존하고 뒤 단계는 `not_run`으로 남긴다. 자동 재시도는 없다.
- 검수기 실행 파일을 찾지 못하는 등의 OS 오류는 durable `failed`/exit127이다. child가 성공 코드를 내도 결과 파일이 없거나 schema/판정이 틀리면 실패한다.
- 매 단계 시작/종료와 오류 시 report snapshot을 flush한다. 입력 파일 SHA-256의 실행 전후 값이 달라지면 판정 불확실로 실패한다. stage 디렉터리 전체 변화는 package checker의 자체 inventory 검수 범위다.
- 이전 report/child 폴더는 덮어쓰지 않는다. 수정 작업을 하는 도구가 아니므로 새 output으로 다시 검수할 수 있으나 과거 증거와 비교해야 한다.
- 자체 engine/build/game 실행, MCP 호출, 에셋 수정, 프로세스 종료 기능은 없다. 명시 package profile은 제공된 UnrealPak의 `-List`만 실행한다. child에 별도 강제 종료/timeout 정책은 없으며, OS/사용자 중단은 미완료 증거로 다뤄야 한다.
- 모든 결과에 `full_migration_complete=false`, `legacy_gates_closed=[]`, `packaging_independence_proven=false`를 유지한다.

## 이번 실제 실행 결과

| 실행 | 결과 | 증거 |
| --- | --- | --- |
| dispatcher 반례 20개 | 모두 통과 | `operations-dispatcher.tests-A.json` |
| Material 프로필 추가 후 dispatcher 반례23개 | 모두 통과 | `operations-dispatcher.tests-B.json` |
| 실제 Material author-E3의 독립 검수기 실행 | `passed_limited_scope`, exit0 | `operations-dispatcher-material-author-E3.json` |
| bootstrap 5개 독립 child | `passed_limited_scope`, exit0 | `operations-dispatcher.bootstrap-A.json` 및 child logs |
| 실제 Blueprint author-D + 새 프로세스 reload-D 독립 검수와 연결 | 세 단계 통과, exit0 | `operations-dispatcher.reload-D.json` |
| 실제 실패 author-A | 독립 검수기가 exit1로 거부 | `operations-dispatcher.rejected-author-A.json` |
| legacy full 42개 | 전부 pending, incomplete/exit2 | `operations-dispatcher.legacy-full.json` |
| package 필수 입력 누락 | child 실행 없이 incomplete/exit2 | `operations-dispatcher.missing-package.json` |
| Python 직접 CLI exit 전달 | plan=0, pending=2 | `operations-dispatcher.cli-status.json` |

bootstrap에서 pipeline contract 13개, journal unit 5개, package checker 21개, marker self-test, dispatcher 20개를 실제 호출했다. 이것을 UE 기능 수나 이식 완료 수로 합산하지 않는다. 자체 테스트는 정확한 manifest/profile, 모든 원본 profile의 false-pass 방지, 무실행 plan, 누락 입력, fail-fast, OS 시작 실패, 공백 경로, 요약만 위조된 parent 거부, phase 혼동, 잘못된 parent hash/fixture, input race, 독립 결과 누락/schema 오류, stage 쓰기 금지 등을 다룬다.

단일 실행에서 dispatcher가 측정한 시간은 bootstrap 약1.897초, author+reload+link 약0.276초, 실패 author 검수 약0.136초다. 로컬 Windows/Python의 외부 JSON/단위 검수 시간이며 동시 진행 작업이 있는 1회 관측이다. MCP 처리량, engine 에셋 처리 성능, 반복 benchmark로 해석하지 않는다.

## 사용 예

프로젝트 루트에서 실행한다. `NEW-...` 출력은 매 실행 새 경로로 지정한다.

```powershell
python -B cubeless/tools/pipelines/verification_dispatcher.py --profile full --plan json
python -B cubeless/tools/pipelines/verification_dispatcher.py --profile bootstrap-offline --output cubeless/records/verification/NEW-bootstrap.json
python -B cubeless/tools/pipelines/verification_dispatcher.py --profile blueprint-reload-evidence --author-report cubeless/records/verification/implementation-2026-10-10/blueprint-author-D.json --reload-report cubeless/records/verification/implementation-2026-10-10/blueprint-reload-D.json --output cubeless/records/verification/NEW-reload-validation.json
python -B cubeless/tools/pipelines/verification_dispatcher.py --profile material-reload-evidence --author-report 'C:/path/to/actual-material-author.json' --reload-report 'C:/path/to/actual-material-reload.json' --output cubeless/records/verification/NEW-material-validation.json
python -B cubeless/tools/pipelines/verification_dispatcher.py --profile package-static-evidence --stage 'C:/path/to/staged-game' --unrealpak 'C:/path/to/UnrealPak.exe' --receipt 'C:/path/to/Game.target' --output cubeless/records/verification/NEW-package-validation.json
python -B cubeless/tools/pipelines/verification_dispatcher.py --profile runtime-marker-evidence --fixture-report 'C:/path/to/fixture.json' --baseline 'C:/path/to/pre-launch-baseline.json' --launch-report 'C:/path/to/actual-launch.json' --output cubeless/records/verification/NEW-runtime-marker-validation.json
```

새 packaged/cleanup 실행 증거는 이 보고서 시점에 이 디스패처로 실행 검증하지 않았다. 원본 42개 gate 내부 assertion의 개별 이식, 새 MCP schema/catalog 전체 parity, transport behavior 재검증, 대규모 도메인 검수 흐름이 남아 있다. 이번 작은 디스패처 이식으로 3,518개 script 또는 전체 legacy 기능을 완료 처리하지 않는다. 원본/엔진/사용자 에셋 수정, Git commit/push는 수행하지 않았다.
