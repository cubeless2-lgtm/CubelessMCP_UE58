# Material runtime proof B 독립 대조

원본 보고서와 엔진 에셋을 수정하지 않고 11개 항목을 재검사했다. 결과는 `operations-material-runtime-proof-B-independent.json`에 있다. 연결된 원본 보고서/로그 8개의 SHA256, 현재 Pak/실행 파일/SaveGame 바이트, 정확한 Pak 항목 1,786개, 새 맵·clone·MPC 포함과 이전 맵 제외, clone의 별도 MPC 연결 및 cold Map→Material→MPC hard edge, 원본 6개·복사 8개 파일, 대상 프로젝트의 baseline Content 파일 해시가 일치했다.

`material-cook-log-audit-B.json`은 MPC가 로그에 없으므로 계속 incomplete다. 실제 MPC Pak 항목을 별도로 검증한 것은 이 실패를 PASS로 바꾼 것이 아니다. 원본 E3는 MPC 연결이 없는 그래프로 남고, 공식 API로 연결을 추가한 clone과 새 맵의 런타임 fixture 증거를 구분한다.

해석을 좁혀야 할 문구는 두 가지다. 기존 dependency audit의 `include_editor_only_package_references`는 false인데 limitations에는 포함한다고 적혀 있다. 실제 옵션을 기준으로 editor-only 참조는 검사에서 제외됐다고 해석한다. 또 게임 프로젝트 전체 불변이라는 표현은 승인된 `.uproject` 플러그인 활성화 변경까지 포괄하지 않는다. 이 검토가 확인한 불변 범위는 기존 Content와 명시된 원본/복사 파일이다. 기존 기록은 수정하지 않았다.

Receipt와 staged 실행 파일 연결은 이름/타입/플랫폼 대조이며 암호학적 빌드 연결 증거는 아니다. nullrhi 실행의 새 BeginPlay SaveGame, cold readback, cooked 에셋 포함은 렌더링 품질이나 모든 shader 완료 시점을 증명하지 않는다. 이 독립 검토는 엔진을 다시 실행하지 않았다.
