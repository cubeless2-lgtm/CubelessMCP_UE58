# UE 5.6 운영 문서·파이프라인·검수 Python 이식 조사

작성일: 2026-10-10 (Asia/Seoul). 원본: `C:/Git/UnrealMCP5_6_1`. Git HEAD: `1e74770bf54be3df54247da07d06a161ccf56637`.

상태: **계획용 정적 조사이며 이식·실행 검증 완료가 아니다.** 원본 스크립트를 import하거나 실행하지 않았고 원본 파일, Git 설정, 에디터, 에셋을 변경하지 않았다. 이번 산출물은 이 문서와 `legacy-operations-inventory.json`이다. 실제 archive 복사는 아직 하지 않았다. 과거 문서의 명령·승인·역할 지시는 현재 작업 지침으로 활성화하지 않았다.

## 조사 범위와 정확한 한계

`rg --files --hidden`으로 아래 루트의 `.md`, `.py`, `.ps1`, `.bat`, `.cmd`, `.toml`, `.yaml`, `.yml`을 수집했다. `node_modules`, `.venv`, `dist`, `build`, `bin`, `obj` 하위는 제외했다. `.gitignore` 등 ignore 규칙은 적용된다. 각 후보의 실제 byte 길이, SHA256, NUL byte 수를 기록했다.

| 위치 | 후보 파일 수 | 주요 내용 |
| --- | ---: | --- |
| `codex/` | 552 | 지침, 역할, 워크플로, 정책, 스킬, 과거 snapshot, 시스템 경험, 저작·검수 runner |
| `Docs/` | 540 | 도구 계약, API 연구, 실험·검수 보고, 디자인·학습 기록 |
| `Python/scripts/` | 2,782 | 검증 dispatcher, 계약 테스트, 분석, 도메인 저작·검수·배치 |
| `scripts/` | 426 | 개별 시스템 저작·변환·검수 파이프라인 |
| `Tools/` | 4 | 선택 확장자에 해당하는 UnrealPerfCapture 문서·배포 스크립트 |
| `.github/` | 3 | CI 및 운영 텍스트 |
| 루트 추가 | 6 | `AGENTS.md`, `CLAUDE.md`, `README.md`, `.editorconfig`, `.gitignore`, `.mcp.json`의 무결성 메타데이터 |
| 합계 | 4,313 | **파일 후보 수이며 기능 수 또는 완료된 분석 수가 아니다.** |

이름 기준 파이프라인 후보 1,131개, 검수 후보 1,491개를 별도 tag로 남겼다. 두 집합은 겹치며 이름 분류만으로 기능·실행 위치·동등성을 확정하지 않는다. 대표 흐름 4개는 아래와 JSON에 호출 순서·입출력·실패·검수 기준을 정리했다. 나머지는 대부분 경로/무결성 조사 단계다.

후속 전수 조사에서 반드시 추가해야 할 위치:

- `Python/tools`, `Python/services`, 서버 등록·discovery·policy와 각 `Python/scripts`의 import/호출 관계. 공개 도구 목록에 나타나지 않는 독립 runner·검수기를 별도 ID로 등록한다.
- `MCPGameProject/Plugins/*/Scripts`, 각 플러그인의 `Content/Python`, editor module, `MCPGameProject/Docs/REFACTOR_COMMANDS.md`. 예: SkyDomeTools의 panorama/polar-cloud/VDB/Blender/Hunyuan/UDS 배치와 GUI·검수 코드, RXSpeedTreeImporter, MeshBlend의 검수 스크립트. 서드파티 라이브러리와 프로젝트 작성 확장분은 출처를 구분한다.
- `Docs/Analysis`, `Docs/Research`, `Docs/Tools`에 동반된 JSON, JSONL, CSV, 이미지, 로그와 fixture. `SourceArt`, `output`, `tmp`, 프로젝트 `Saved/MCP`의 검증 증거. 이들은 현재 4,313개 텍스트 목록에 전부 포함되지 않았다.
- `.githooks`의 `pre-commit`, `pre-push`, `post-checkout`, `post-commit`, `post-merge`, `.github/workflows/verify.yml`, `verify-transport-windows.yml`, `.claude`와 `codex/integrations/claude-code`. 어떤 것도 설치하거나 실행하지 않는다.
- ignore된 파일과 untracked/삭제 상태를 Git 목록과 비교한다. 원본 dirty 작업은 HEAD 내용으로 덮어쓰거나 무시하지 않는다. 원본 Git read가 sandbox에서 worktree/Permission denied로 실패한 현상은 정식 escalation 후 읽기 가능한 것으로 별도 확인되었으므로 저장소 손상으로 단정하지 않는다.

`codex/scripts/volumetric_dynamic_fog/volumetric_dynamic_fog_builder.py`는 현재 경로에 없고 `__pycache__`의 CPython 3.11/3.12 cache만 관찰했다. 삭제/누락 여부와 HEAD/index/history 복구 가능성을 추가 확인해야 한다. cache 존재만으로 해당 소스와 기능을 확보했다고 처리하지 않는다.

## 손상과 보존 계획

현재 읽은 후보에서 **최소 16개 파일이 전체 NUL**이다. 개별 경로·byte 길이·실제 SHA256은 JSON `recovery_checks`에 기록했다. 주요 항목은 다음과 같다.

- 루트 `AGENTS.md`: 4,602 bytes 모두 NUL.
- `codex/AGENTS.md`: 6,321 bytes 모두 NUL.
- `codex/docs/policies/git-repositories.md`.
- `codex/skills/mcp-release-gate`와 `unreal-mcp-safety-review`의 `SKILL.md`, `references/ops-references.md` 각 2개.
- `codex/snapshots/20260908-astra-instruction-alignment`의 두 AGENTS 및 위 두 skill의 4개 문서, 합계 6개.
- `codex/docs/projects/cubeless-stylized/systems/rx-sky-system.md`.
- `Docs/Tools/play_asset_inspector.md`, `Docs/Analysis/Astra_Instruction_Alignment_20260908.md`.

16개 모두 위 HEAD의 `git show HEAD:<path>`가 성공했고 NUL 없는 읽을 수 있는 텍스트가 있음을 확인했다. **정상 HEAD가 현재 변경 의도를 전부 복구한다는 뜻은 아니다.** 특히 staged/unstaged 내용은 별도 출처다. 아직 byte-exact HEAD 추출·hash·archive 생성이나 index blob 대조를 하지 않았다.

보존 시 `cubeless/codex/archive/ue56/` 안에서 `working-tree`, `index`, `git/<commit>` 출처를 구분한다. 손상 원본도 byte 그대로 별도 보존하고, 복구한 텍스트에는 원본 경로, blob/commit, 추출 시각, SHA256, 복구 출처와 미복구 차이를 manifest로 연결한다. 추출 전후 hash를 비교하고 원본을 고치지 않는다. archive의 옛 AGENTS/SKILL은 역사 자료라는 상위 안내와 범위 경계를 두고 현재 bootstrap에서 직접 활성화하지 않는다. 경로·시간·포트만 치환한 문서를 현재 정책으로 취급하지 않는다.

## 현재 지침으로 다시 작성할 내용

7개 역할(`ieta`, `keilan`, `tivret`, `tivret-mcp`, `mcp-upstream-manager`, `mcp-release-verifier`, `unreal-cpp-reviewer`)과 6개 스킬(`map-preview-safety`, `mcp-release-gate`, `niagara-authoring-review`, `unreal-material-decomposition`, `unreal-mcp-safety-review`, `unreal-pcg-authoring`)이 있다. 역할의 책임/입력/산출물/검증/인계 조건을 현재 아키텍처에 맞춰 정리한다. 문서 이전은 실제 하위 에이전트 실행과 다른 상태다.

| 원본 규칙 또는 사실 | UE 5.8.3 현재화 |
| --- | --- |
| UE 5.6.1이 능력 상한, 5.7/5.8 API 금지 | 실제 5.8.3 설치·build 식별자와 엔진 소스로 API 확인. 과거 버전 fallback을 자동 유지하지 않음 |
| `127.0.0.1:55561`, `UNREAL_MCP_5_6_1_PORT`, 자체 TCP와 외부 Python 서버 | 공식 MCP 연결, 공식 Registry, Cubeless Toolset 등록·검색·호출을 기준으로 재작성 |
| `MCPGameProject` 빌드 타깃·경로와 외부 binding | 대상 `.uproject` 디렉터리를 기준으로 engine/project 경로를 발견. 외부 Ops와 sibling MCP는 필수 의존에서 제외 |
| 과거 `c++1218` 및 UnrealMCP 경로만 예외 | 현재 요청은 **CubelessMCP_UE58의 editor-only C++ 구현을 적극 허용**. 옛 승인 token을 새 플러그인 구현 장애로 적용하지 않음. 게임플레이 C++는 별도 범위 |
| MCP-only dependency 금지 | `CubelessMCP_UE58`와 보조 editor module까지 검사. 완성 에셋·게임·패키지에 런타임 의존 금지. 플러그인 비활성/부재 환경의 load/compile/cook/package 검증 |
| chongdashu upstream sync, `update_with_pcg_extension.ps1` | 과거 upstream/브릿지 운영은 archive. 공식 엔진 plugin 소스는 수정하지 않음 |
| 옛 역할별 고정 모델/대리 승인/엄격한 규칙 우선 문구 | 현재 사용자 지시·runtime 권한·지침 우선순위에 맞춰 일관되게 재작성. 역할 문서가 승인이나 모델 변경을 만들어내지 않음 |
| 과거 `full`, transport 프로파일 | 핵심 검증 기준은 보존하고 공식 MCP 아키텍처용 검증 manifest로 재구성. 사라진 TCP framing 테스트와 남는 mutation 안전 계약을 구별 |

재사용할 경험은 원인·해결·적용 조건·검증 근거·엔진/프로젝트/commit·현재 5.8 검증 상태로 나눈다. 예: map load의 `World Memory Leaks`, inline LoadMap 회피/defer 요청 상태, 존재하지 않는 UV channel 접근 crash, Blueprint compile owner chain, GPU RT 갱신과 CPU Tick의 순서, timeout 뒤 mutation 결과 불명, 취소 요청과 실제 취소 구별, Undo와 원자적 rollback 구별, MCP-only 에셋 의존 방지.

`promoted-lessons.md`는 유용한 요약이지만 각 항목의 원본 실행 증거를 전부 담지 않는다. `mcp-rpc-verification-first.md`는 안전 계약과 날짜가 있는 과거 단계/측정/모델 규칙을 함께 담는다. `Docs/Analysis/verification-policy-review-20260906.md`는 명시적으로 정적 문서 검토이며 모델 행동·성능 benchmark가 아니다. `failure-and-exception-register.md`의 과거 crash/예외도 UE 5.8 pass로 승격하지 않는다. 파일에 없는 과거 대화 경험은 가져왔다고 쓰지 않는다.

## 파이프라인과 검수 Python의 독립 이식 계약

각 항목에 `PIPE-*` 또는 `VERIFY-*` ID를 부여하며 공개 도구 이식표와 별도로 추적한다. 파일 하나에 여러 흐름이 있으면 phase/entrypoint별로 나누고, 여러 파일이 한 흐름이면 dependency edge로 묶는다. 최종 목록에는 실행 위치(`external offline`, `external official MCP client`, `editor Python`, `editor C++`, `hybrid`), 호출 순서, 입력/기본값, 반환·보고서 schema, 통과 기준, 실패 단계, 변경 에셋, compile/save/reload, 중복 실행 위험과 복구 절차가 필요하다.

외부 Python 검수 harness는 개발·검증 용도로 남을 수 있다. 최종 CubelessMCP_UE58 기능 제공 경로에 외부 Python 서버/TCP 브릿지를 필수로 두지 않는다. 에디터 내부 작업은 Toolset→작업 로직→UE 5.8 호환부로 분리하고, 외부 검수기는 공식 MCP 호출/보고서 소비를 담당하도록 설계한다. archive/copy, import 성공, 등록 성공만으로 파이프라인 완료를 선언하지 않는다.

### 대표 1 — OPS56-VERIFY-001: offline verification dispatcher

근거: `Python/scripts/analysis/run_verification.py`의 `COMMANDS`, `PROFILES`, `verification_plan`, `run_profile`, `main`.

외부 Python에서 명시적 profile→명령 manifest→각 명령을 독립 subprocess로 순차 실행→최초 nonzero exit에서 종료한다. `--plan json`은 명령 계획을 stdout으로 내보내며 editor/build를 실행하지 않는다. live 무결성과 자산 지속성을 증명하는 검수는 아니다. UE 5.8에서는 등록/schema/behavior 계약을 새 경로로 옮기고, 사라진 TCP 전용 검사와 살아남는 timeout/mutation 검수 목적을 분리한다. 실행 결과 JSON, 환경/입력 fingerprint, 단계별 stdout 경로를 새 records 아래에 남기는 기능을 보강한다.

### 대표 2 — OPS56-VERIFY-002: Blueprint mutation contract live smoke

근거: `Python/scripts/smoke_blueprint_mutation_contract_live.py`의 `_verify_contract`, `_record`, `_cleanup`, `main`.

외부 Python에서 ping→UUID 기반 임시 BP 생성→Branch 2개 추가→pin 연결→두 번째 node 이동→삭제→list readback→finally cleanup→JSON 보고를 수행한다. 핵심 mutation 3개는 `mutation_result_v1`, before/after, verified postcondition, verification method/status, 6개 delta count를 검사한다. exit 0=pass, 1=failure, 2=no editor이며 skip을 pass로 취급하지 않는다.

현재 연결/이동은 compile/save false이므로 persistence pass가 아니다. UE 5.8 포트는 공식 MCP 호출로 교체하고 분리된 compile/save/reload 검수를 추가한다. cleanup 자체가 예외를 던지면 최종 보고서 작성 전에 빠질 수 있는 경로도 처리해야 한다. 결과 불명 mutation에 대한 자동 재시도는 허용하지 않는다.

### 대표 3 — OPS56-PIPE-001: SplineLoopConveyor 저작·저장·런타임 검수

근거: `scripts/study_spline_loop_conveyor/{bridge.py,bp_graph.py,build_pure_bp.py,verify_pure_saved.py,verify_pure_runtime.py}`.

외부 builder→bp_graph→TCP bridge로 native Blueprint graph 명령을 조합한다. editor Python saved 검수는 native helper class 부재, project module dependency 부재, actor 3개, spline point 2/5/4, count/spacing과 0.03cm 이내 직선 간격을 검사하고 레벨을 저장한다. PIE 검수는 실제 instance/child actor transform을 `GetItemWorldTransform`과 0.1cm 이내로 비교하고 phase 이동을 확인한다. `operations.jsonl`, graph dump, saved/runtime JSON이 산출된다.

`bp_graph.Graph.link`에는 자동 conversion node 삽입으로 직접 링크 postcondition이 실패했을 때의 특화 처리가 있다. `schema_response=make_with_conversion_node`와 `mutation_performed`를 확인한 뒤 그래프를 다시 읽어 정확한 중간 `Conv_*` 노드의 양쪽 연결을 검증한다. mutation을 재실행하는 것이 아니다. 이 검증 흐름도 독립 요구사항으로 보존해야 한다.

검수 Python 자체가 `RebuildConveyor`, selection 변경과 레벨 저장을 수행하므로 read-only가 아니다. assert 실패 시 구조화된 실패 보고가 없고 saved JSON이 `save_current_level`보다 먼저 기록된다. 이식 시 fixture 경로/actor 계약을 parameter화하고 테스트 맵에서 실행하며 save 결과까지 보고해야 한다. 원본의 55561 고정 연결과 repository parent 기준 출력 경로를 제거한다. 나머지 init/build/compile/reload 스크립트를 추가 연결해 전체 flow를 확정해야 한다.

### 대표 4 — OPS56-PIPE-002: ReactiveSurface recovery commit gate

근거: `codex/scripts/reactive_surface/reactive_surface_ground_recovery_commit_gate_runner.py`와 `demo_runtime_wiring`, `fixed_30hz`, `half_life`, `niagara` runner import 관계.

외부 Python이 직접 socket/TCP와 editor Python payload를 혼용한다. `--phase temp`는 깨끗하고 정지한 격리 editor 확인→임시 BP 복제→graph migration→compile/topology/hash/editor state 확인→cleanup을 수행한다. `production`은 이전 temp/fixed30/Niagara PASS를 검사하고 PIE/기존 dirty target을 거부한 뒤 graph 수정→compile→target만 save→변경 file hash·의존성 audit을 한다. `cleanup`은 temp 삭제와 hash/state 보존을 검수한다. phase별 JSON에 PASS/FAIL, checks, exception을 기록한다.

고정 GUID에 의존한 과거 일회성 migration과 재사용 가능한 기능을 분리해야 한다. 현재 prerequisite 검사는 phase/status 중심이어서 입력·환경 fingerprint를 추가해야 하며 예외 시 누적 단계 정보가 사라지는 점도 보강해야 한다. GPU 갱신을 기다리는 제한된 presentation 재시도와 불확실한 RPC mutation 재시도는 다른 문제다. 전자의 행동 보장은 보존하고 후자의 자동 반복은 만들지 않는다. 이번 이식 검증에서는 production asset을 변경하지 않고 독립 fixture에서 수행한다.

## 차기 작업 순서와 완료 조건

1. 현재 파일/Git index/HEAD 차이와 ignore/untracked/deleted 기록을 확보하고 원본 상태를 보존한다. 현재 조사 JSON의 hash는 읽은 시점의 값이므로 실제 archive 전에 재확인한다.
2. 전체 스크립트·보고서 후보를 확장 수집하고 import/CLI/phase/embedded Python/`send_command`/`execute_python`/subprocess/엔진 API/결과 파일 연결을 정적으로 추출한다. 동적 명령 조합과 공통 helper는 수동 확인한다.
3. 공개 도구, 내부 명령, C++ handler, 독립 pipeline, verifier와 문서 경험을 대조한다. 각 기능은 구현·등록 / 검증된 공식 동등 대체 / 통신 전용 제거 / 미완료 중 하나로 설명한다. pipeline 전체를 transport 교체만으로 제거 처리하지 않는다.
4. 정상·복구 원본을 출처별 archive로 보존하고 현재 지침은 `cubeless/codex`에 새로 작성한다. 새 policy는 editor-only C++ 적극 사용과 runtime dependency 금지를 동시에 명시한다.
5. UE 5.8.3 Toolset의 작은 읽기 경로부터 검증한 뒤 기능군과 대표 pipeline을 이식한다. 독립 tests에 정상/실패/부분 변경/중복 실행/timeout/readback/compile/save/reload를 적용하고 실제 공식 MCP 연결 결과를 보관한다.
6. pipeline과 verifier의 5.8 실행 증거까지 기능 대응표에 연결한다. 과거 5.6 수치와 현재 5.8 측정은 조건·입력·환경을 구별한다. UE 5.8 실행 또는 동등 기준이 없는 항목은 미완료로 유지한다.

이 조사 자체의 검증은 생성 JSON parse, 후보 byte/hash 기록과 선택 소스 읽기, 손상 16개 Git HEAD readability 확인이다. **UE 5.8 엔진 실행, 빌드, Toolset 등록·호출, 자산 compile/save/reload, 성능 측정은 이 문서의 증거 범위에 없다.**
