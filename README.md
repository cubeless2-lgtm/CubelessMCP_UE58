# CubelessMCP_UE58

UE 5.8.3 공식 ModelContextProtocol 서버와 Toolset Registry를 확장하는 Editor 전용 프로젝트 플러그인입니다. 기존 UE 5.6 기능의 이식은 진행 중이며, 전체 기능 이식을 완료한 배포판은 아닙니다.

이 저장소에는 플러그인의 빌드·로드·도구 등록에 필요한 소스와 설정만 포함합니다. 검증용 Unreal 프로젝트, 테스트 에셋·코드, 빌드 산출물, 실행 결과·로그·스냅샷, 개발 기록과 외부 Python 파이프라인은 배포 대상에서 제외합니다.

설치 및 사용:

1. `Plugins/CubelessMCP_UE58` 폴더를 사용할 프로젝트의 `Plugins/` 아래에 복사합니다.
2. UE 5.8.3에 맞는 C++ 빌드 환경에서 `CubelessMCPUE58Editor` 모듈을 빌드합니다. 이 저장소에는 미리 빌드한 DLL을 포함하지 않습니다.
3. 프로젝트에서 `CubelessMCP_UE58` 플러그인을 활성화하고 에디터를 다시 시작합니다. 의존 플러그인은 `.uplugin`에 선언되어 있습니다.
4. 에디터의 공식 ModelContextProtocol 서버를 시작합니다. 같은 공식 MCP 연결에서 Cubeless Toolset을 검색하고 호출합니다. 별도 Python MCP 서버나 TCP 브릿지는 필요하지 않습니다.

에디터 Python은 플러그인 내부 `Content/Python/`에서 등록됩니다. `.uplugin`에 고정된 PythonRequirements는 NumPy 2.4.6과 Pillow 12.2.0입니다. 설치 환경에서 해당 의존성을 준비해야 합니다.

게임 모듈에 이 플러그인을 하드 의존성으로 추가하지 마십시오. 모듈은 Editor 대상에만 로드되며, 생성한 게임 에셋에도 Cubeless/MCP 런타임 타입을 참조시키지 않아야 합니다.

현재 소스 체크포인트는 VE까지의 대표 검증 결과와 VF의 수리·소스 검토를 마친 후보를 포함합니다. VF의 Niagara Factory Batch 연결은 아직 실제 UE 공식 MCP 실행으로 검증하지 않았습니다. 소스 등록이나 대표 샘플 성공이 모든 원본 계약의 완료를 뜻하지 않습니다.
