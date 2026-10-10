# 운영 지침·외부 파이프라인 구현 기록

2026-10-10. 이 문서는 구현 산출물과 증거 범위를 구별한다. 전체 UE5.6 운영 체계·3,518개 실행 스크립트가 이전됐다는 보고가 아니다.

## 현재 사용 가능한 지침

`cubeless/codex/AGENTS.md`를 현재 진입 지침으로 만들고, 7개 역할·2개 정책·2개 실행 절차·5.6 경험 요약을 재작성했다. root의 짧은 AGENTS 진입 파일에서 연결한다. 역할 문서를 작성한 사실과 실제 하위 에이전트 실행은 별개다. 과거 모델·승인 token·고정 드라이브·55561 브릿지·5.6 API 상한을 현재 규칙으로 활성화하지 않았다.

## 원본문서 보존

원본 `C:/Git/UnrealMCP5_6_1`의 조사 목록 중 Markdown/TOML/YAML 792개를 보존했다. working-tree/index/HEAD가 다른 byte를 구별하여 물리적 사본 2,331개를 만들었다. `.source` 확장자는 오래된 AGENTS/SKILL 자동 적용을 막으며 내용 byte는 바꾸지 않았다. 각 원본 경로·SHA-256·blob·commit은 `cubeless/codex/archive/ue56/manifest.json`에 있다.

16개 all-NUL 문서도 손상본 그대로 보존했다. Git 대안은 별도 보존했으며 최신 미커밋 의도가 복구됐다고 주장하지 않는다. 792개 source hash가 계획 조사와 같았고, 2,331개 사본을 다시 전수 해시 검사하여 불일치 0개였다. 원본 수정·과거 스크립트 실행은 하지 않았다. `.mcp.json`과 credentials/settings는 보관하지 않았다.

원본 6개 skill 문서는 archive 상태다. 현재 Codex skill로 설치하지 않았고 전체 의미 이전 완료로 집계하지 않는다. 실행 스크립트·binary/JSON 증거·ignore된 미조사 자료는 이 archive 수에 포함하지 않는다.

## 공식 HTTP 클라이언트

`cubeless/tools/official_mcp_client.py`는 서버를 시작하지 않는 표준 Python 클라이언트다. 설치된 UE5.8.3 `ModelContextProtocolServer.cpp`와 `ModelContextProtocol.h`를 읽어 `/mcp`, 초기화 협상, session/version header, initialized notification, progressToken 없는 JSON 결과 경로를 구현했다. 타임아웃·오류 시 mutation을 재시도하지 않는다.

기존 에디터의 공식 endpoint에서 실제 읽기 조회 3개가 성공했다. 증거: `operations-official-discovery.json`, `operations-programmatic-schema.json`, `operations-plugin-schema.json`. 등록된 top-level은 list_toolsets/describe_toolset/call_tool이며 임의 Python 실행을 가정하지 않았다. ProgrammaticToolset은 도구 orchestration용 sandbox라는 실제 schema를 확인했다. PluginToolset의 SetPluginEnabled는 다음 재시작 적용이다.

이 조회는 사용자 에셋을 변경하지 않았다. 초기 localhost 표본 4 requests 합계 9.234681초, 127.0.0.1 schema 표본은 각 5 requests 합계 약 1.403초였다. 조건과 수치는 `operations-implementation-status.json`에 있다. 한 번씩 실행한 읽기 표본이며 원인·mutation 처리량·batch 성능을 입증하는 benchmark가 아니다.

## 독립 파이프라인·검수기

`cubeless/tools/pipelines/blueprint_mutation_smoke.py`는 원본 `Python/scripts/smoke_blueprint_mutation_contract_live.py`의 대표 계약 OPS56-VERIFY-002를 대상으로 작성했다. 실제 Toolset 목록·스키마를 조회하여 native diagnostic, Cubeless Blueprint lifecycle과 bounded graph tools를 연결한다.

author 순서: 프로젝트/5.8.3/PIE/dirty preflight → 잘못된 이름 거부 → 임시 Actor Blueprint 생성 → 중복 생성 거부 → Branch 2개 → exec 연결/독립 readback → stale 좌표 사전조건 거부/좌표불변 확인 → 이동/독립 좌표 readback → 연결된 exec 삭제 opt-in 없는 요청 거부/연결불변 확인 → 명시 opt-in 삭제/부재 readback → 한 번 compile+save → 저장 상태 readback → saved dependency audit.

reload는 author report의 입력·환경 fingerprint를 검사하고 **다른 에디터 instance**에서 에셋/부모/잔존 노드/삭제 노드를 재조회한다. 새 HTTP 세션만 만든 것을 reload로 통과시키지 않는다. cleanup은 저장된 fixture로 no-MCP packaging 검증을 한 뒤 별도 호출하며 정확한 생성 에셋 하나만 삭제한다. 빈 가상 폴더는 남을 수 있다. 불확실한 mutation 이후 자동 cleanup/재시도는 하지 않는다.

기존 mutation_result_v1의 before/after, 6종 delta, postcondition, 검수 method/status를 확인한다. 현재 graph 노드 식별자는 object path이고 원본 GUID selector와 동등하다고 주장하지 않는다. Branch/exec 연결 subset이므로 전체 graph authoring·conversion-node 동등성은 미완료다. source smoke에 없었던 저장·cold reload를 별도 필수 단계로 보강했다.

`cubeless/tools/verification/pipeline_report.py`는 누락/실패 단계, 잘못된 fingerprint, compile/save 요약과 실제 stage 불일치, 결과 불명과 같은-instance reload를 거부한다. `operations-offline-tests-v3.json`에 합성 report·timeout 반례 13개 통과를 기록했다. 이는 live Unreal pass가 아니다. 실제 author/reload/cleanup과 no-MCP package/run은 root의 격리 에디터 검증 보고서에 연결해야 한다.

원본 후보 inventory는 변경하지 않았다. 현재 구현 상태는 별도 `operations-implementation-status.json`으로 추적한다. 나머지 모든 파이프라인·검수기와 경험은 범위에서 제거하지 않고 미완료로 남는다.
