# Material9개 도구 검수

외부 Python은 공식 MCP의 client다. 별도 MCP 서버나 TCP bridge를 기동하지 않는다. 플러그인 내부 Python/C++ 구현과 외부 검수기는 별도 이식 대상으로 추적한다.

1. 담당자가 isolated test project와 공식 endpoint를 확인한다. 사용자 editor 종료나 사용자 에셋 수정은 이 workflow에 포함되지 않는다.
2. `cubeless/tools/pipelines/material_graph_smoke.py --phase author`에 실제 endpoint, `--expected-project`, 새 `--output`을 전달한다. fresh `/Game/CubelessMCPTests/Material_<uuid>` 안에서만 fixture를 만든다. 공식 schema/preflight가 맞지 않으면 중단한다.
3. author는9개 Cubeless 도구, 실제 selector, invalid pin, 같은 ID replay, 공식 연결 readback, 마지막 명시 compile1회와 fixture2개 save1회를 검수한다. setter/factory의 implicit shader work는 전체 compile 횟수에 포함해서 추정하지 않는다.
4. 실패하면 보고서의 failed_stage, 실제 result, changes, compile/save, outcome_unknown을 읽는다. 불확실한 mutation을 같은 요청으로 재실행하지 않는다. 필요한 읽기 reconciliation과 별도 보존 작업은 새 보고서로 기록하며 기존 실패를 성공으로 바꾸지 않는다.
5. author 성공 후 담당자가 새 editor process를 준비한다. `--phase reload --parent-report <실제 author>`로 다른 instance에서 saved graph, MPC 값, root 연결, clean 상태를 검수한다. 파이프라인 자체는 프로세스를 시작하거나 종료하지 않는다.
6. `cubeless/tools/pipelines/verification_dispatcher.py --profile material-reload-evidence --author-report <실제 author> --reload-report <실제 reload> --output <새 verification 보고서>`로 각각 독립 검수한다. author 실제 파일의 SHA와 연결까지 확인한다.

실행 보고서는 `cubeless/records/verification/`에, 이식 설계와 계약은 `cubeless/records/migration/`에 둔다. 출력 보고서와 children 폴더는 덮어쓰지 않는다. Material 독립 프로필 통과로 원본42개 legacy gate, 나머지12개 Material 도구, packaging/runtime 독립성까지 완료 처리하지 않는다.
