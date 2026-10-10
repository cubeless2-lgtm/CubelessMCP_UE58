# UE5.8.3 Material 도구 실행 계약

적용 범위는 UE5.8.3-58210709의 공식 ToolsetRegistry와 현재 Cubeless Material9개 도구다. UE5.6 경험을 검증 없이 승격한 문서가 아니다. 전체 Material21개 도구의 이식 완료를 뜻하지 않는다.

## UObject 경로와 package 경로

- 원인: 공식 ObjectTools/MaterialTools의 UObject 입력은 실제 객체 경로를 요구한다. `/Game/Folder/M_Asset` 패키지 경로를 `refPath`에 넣으면 parameter error가 발생한다.
- 해결: 생성 응답의 full `refPath`(`/Game/Folder/M_Asset.M_Asset`)를 보존하여 UObject 인자로 전달한다. `exists`, `save_assets`, Cubeless material_path에는 각 도구의 실제 schema에 맞는 패키지 경로를 별도 유지한다.
- 조건: 경로를 단순 문자열 추정으로 만들기보다 생성·조회가 돌려준 실제 객체 경로를 우선한다. expression subobject 경로도 실제 응답을 사용한다.
- 검증: `material-author-E.json` 실패 후 `material-partial-E-reconciliation.json`은 setter가 적용되지 않은 빈 MPC 배열을 읽고 생성된 fixture만 따로 저장했다. 수정된 `material-author-E3.json`에서 정상 동작했다. 실패 mutation을 자동 재시도하지 않았다.

## 연결 없음 반환값

- 원인: 공식 MaterialTools의 expression UObject가 null일 때 UE5.8.3 실제 출력은 JSON null 대신 정확한 문자열 `"None"`이다.
- 소스: `ReferenceConverter.cpp`의 null-object 분기가 custom JSON을 반환하지 않아 `ToolsetJson.cpp` → `JsonObjectConverter.cpp`의 ExportTextItem fallback을 거치며 `PropertyBaseObject.cpp`가 `"None"`을 쓴다.
- 해결: 독립 readback 검수에서 확인된 정확한 sentinel만 인정한다. 빈 문자열·소문자 none·문자열 null·빈 refPath·필드 누락을 연결 없음으로 포괄 허용하지 않는다.
- 검증: `material-author-E2.json`의 삭제 API·native readback은 정상이나 외부 검수 예상값이 잘못되어 실패했다. harness 수정 후 E3 author와 새 프로세스 reload, 잘못된 sentinel의 offline 반례 검수를 통과했다. 엔진 플러그인 소스는 수정하지 않았다.

## 등록·수정·저장 경계

- 공식 schema generator는 빈 문자열 기본값을 필수 인자로 만들 수 있다. 등록 경계의 nullable annotation을 내부 원래 기본값으로 정규화한다. `type:string/default:null` schema에서 명시 JSON null의 허용은 따로 검증해야 하며 현재 smoke는 optional 인자를 생략한다.
- 네 개 수정 도구는 before snapshot과 durable checkpoint 후 native 작업을 수행한다. snapshot은 해당 시점의 관측일 뿐 중단 이후 실제 결과를 보장하지 않는다.
- native postcondition과 공식 독립 readback을 모두 검수한다. 성공 응답만으로 save/reload 완료를 선언하지 않는다.
-9개 도구는 명시 compile/save를 하지 않는다. PostEditChange에 따른 implicit shader 작업은 발생할 수 있다. 공식 recompile/save를 마지막에 한 번 호출한 fixture 검수는 기존 compile_and_save_material의 동등 구현을 뜻하지 않는다.
- 현재 mutation `/Game/` 마운트와 graph2048개 budget 제한은 공개된 구현 한계다. Function/Instance·custom HLSL·대규모 graph·시각 렌더 결과는 별도 검수 대상이다.

실행 기록은 프로젝트의 `cubeless/records/verification/implementation-2026-10-10/`, 상세 계약은 `cubeless/records/migration/implementation-2026-10-10/operations-material-python.json`과 같은 이름의 Markdown에 있다. 과거 보고서의 명령은 현재 실행 승인으로 해석하지 않는다.
