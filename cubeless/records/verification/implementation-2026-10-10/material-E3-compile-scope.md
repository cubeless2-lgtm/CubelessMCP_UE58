# Material E3의 컴파일 증거 범위

E3의 `compile.passed=true`는 **공식 MaterialTools.recompile 호출이 현재 보고된 오류 없이 반환했다**는 좁은 의미다. 셰이더 작업 완료나 렌더링 성공을 뜻하지 않는다. 원본 보고서의 method 설명과 실행 응답을 보존하며 전체 컴파일 성공으로 확대 해석하지 않는다.

설치된 UE 5.8.3의 `Engine/Source/Editor/MaterialEditor/Private/MaterialEditingLibrary.cpp:1035`에서 `RecompileMaterial`은 PostEditChange 및 streaming data 갱신을 수행한다. `GetMaterialResource(GMaxRHIShaderPlatform)`이 있으면 현재 `GetCompileErrors()`를 반환하며, **resource가 없어도 빈 오류 배열을 반환한다**. 이 경로에는 `IsCompilationFinished()`를 최종 확인하는 검사가 없다. 공식 `EditorToolset/Content/Python/editor_toolset/toolsets/material.py:486`은 그 오류 배열이 비어 있으면 예외 없이 반환한다.

E3의 에디터는 `-nullrhi`로 실행했다. 그래프 편집·독립 연결 readback·저장·새 프로세스 재로드의 통과는 유효하지만 shader resource 가용성과 완료 여부는 별도로 관측하지 않았다. 따라서:

- 명시적 recompile 요청/반환 1회: 확인.
- 명시적 save 요청/성공 1회 및 cold reload: 확인.
- shader compilation completion: 미확인.
- 화면 렌더링 및 시각 품질: 미검증.
- 모든 legacy Material 컴파일 동등성: 미완료.

후속 native analyze 진단에 resource 가용성·완료·현재 오류를 추가하고, MCP 없는 별도 프로젝트에서 실제 Win64 cook의 해당 Material 셰이더 증거를 수집한다. 비동기 작업 설계에서 단순한 빈 오류 배열이나 성공 응답을 완료 신호로 사용하지 않는다.
