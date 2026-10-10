# 첫 Blueprint fixture의 MCP 없는 Shipping 실행

2026-10-10. **최소 fixture 범위 통과. 전체 기능 이식 및 전체 게임 검증 완료를 의미하지 않는다.**

## 실제 실행 증거

1. `runtime-fixture-B.json`: Engine Actor/SaveGame Blueprint와 맵을 작성·컴파일·저장했다. 공식 Cubeless MCP로 생성한 author-D Blueprint도 이 맵에 배치했다.
2. `runtime-project-preparation-B.json`: 에셋 네 개를 SHA-256 대조 후 별도 `UE58RuntimeProof` 프로젝트에 복사했다. Cubeless 또는 공식 MCP 플러그인 파일은 복사하지 않았다. 기본 엔진 플러그인을 끄고 에디터 Python/EditorScriptingUtilities만 허용했다.
3. `runtime-no-mcp-validation-C.json`: 새 에디터 프로세스에서 맵·배치 액터 로드, Blueprint 세 개의 메모리 내 컴파일, 패키지 참조 검사를 통과했다. 저장하지 않았고 에셋 네 개의 디스크 해시가 유지됐다. 활성 플러그인은 ContentBrowserFileDataSource, EditorScriptingUtilities, PythonScriptPlugin뿐이다.
4. `build-runtime-shipping-B.log`: Win64 Shipping 빌드·쿠킹·stage·archive 성공. BuildCookRun 72.56초, 전체 UAT 약 73초. Shader DDC가 존재하는 한 번의 실행이며 냉간 성능이나 5.6 대비 속도가 아니다. 셰이더 컴파일 925건, 쿠킹 처리 패키지 200개(거부된 에디터 콘텐츠도 로그에 별도 표기).
5. `runtime-package-static-B.json`: 실제 설치된 UnrealPak으로 pak 한 개의 전체 1,782개 entry를 읽었고 총계가 일치했다. 조회 전후 pak 해시가 동일하며 stage 파일 23개와 Shipping Game receipt 검사에 위반 0개였다. IoStore는 사용하지 않았다.
6. `runtime-marker-baseline-B.json` → `runtime-launch-B.json` → `runtime-marker-verification-B.json`: 전용 UserDir에서 marker 부재를 먼저 기록한 뒤 실제 Shipping 실행 파일을 실행했다. PID 38384, 약 1.680초, 종료 코드 0. 실행 중 생성한 1,423바이트 SaveGame의 GVAS 헤더는 UE 5.8.3 CL 58210709와 정확한 실행별 BP_MarkerData 클래스를 가리켰다. 외부 검증기는 marker를 만들지 않았다.
7. `operations-dispatcher.package-B.json`, `operations-dispatcher.marker-B.json`: 외부 dispatcher에서 정적 package 감사 및 marker 검수 프로필을 각각 독립 실행해 `passed_limited_scope`를 얻었다.

## 실패 보존과 빌드 보완

- validation-B는 UI LevelEditorSubsystem 부재로 실패했다. C에서는 설치 엔진 공개 `EditorLoadingAndSavingUtils.load_map`과 `GameplayStatics.get_all_actors_of_class`를 사용했다. B의 관리 참조 카테고리는 옵션 혼합으로 잘못 분류됐으므로 증거로 사용하지 않는다. C는 일곱 플래그를 초기화하고 정확히 한 카테고리만 활성화한 값을 보고서에 보존한다.
- Shipping-A는 Game 빌드는 성공했지만 쿠킹이 에디터 receipt 부재로 중단됐다. `build-runtime-editor-A.log`에서 임시 Editor target을 빌드한 다음 새 출력 폴더로 Shipping-B를 실행했다. 에셋 수정 요청을 재시도한 것이 아니다.
- `DisableEnginePluginsByDefault`에 따라 Paper2D 등의 기본 플러그인이 꺼져 있어서 UAT가 **테스트 프로젝트의 `Intermediate/Source`에 임시 target/module stub**을 자동 생성했다. `CubelessRuntimeProof.cpp`는 `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, ...)`만, Build.cs는 Core 의존성만 가진다. Cubeless/MCP 의존성이나 게임플레이 기능을 추가하지 않았다. 사용자 프로젝트의 Source는 생성·수정하지 않았다. 최초 준비 보고서의 `gameplay_cpp_created=false`는 UAT 실행 이전 상태이다.

## 증거 범위와 다음 검증

- 이 결과는 복사된 네 개의 테스트 패키지와 명시된 런타임 SaveGame 동작에 한정한다. 모든 원본 도구·에셋·렌더링·게임플레이 기능의 회귀 통과가 아니다. `-nullrhi`라 화면 렌더링은 검사하지 않았다.
- Asset Registry 검사는 저장된 hard/soft package 참조와 현재 존재하는 management 참조를 다룬다. `/Script`는 종단이며 동적 문자열 로드, searchable-name, 모든 native 바이너리 의존성의 완전 탐색을 주장하지 않는다. SaveGame payload 전체를 역직렬화한 것도 아니다.
- 정적 package 검수는 filename/receipt/entry 감사이며 단독으로 완전한 독립성을 증명하지 않는다. 위의 MCP 없는 로드·컴파일와 실제 실행 증거를 함께 본다.
- 이 프로젝트에는 플러그인이 물리적으로 없다. **Cubeless가 설치·Editor 활성화된 프로젝트에서 Game 빌드에 제외되는지의 별도 stage 검증**, 이후 추가되는 기능군의 생성 에셋 검증은 계속 필요하다.
- 모든 로그·실패·보고서는 보존하며 자동 롤백·불확실한 변이 재시도를 수행하지 않았다.
