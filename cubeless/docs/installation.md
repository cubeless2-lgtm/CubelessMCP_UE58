# 설치와 프로젝트 선택

지원 기준은 Windows x64, 설치된 UE 5.8.x, Visual Studio C++ 빌드 도구와 Windows SDK입니다. 다른 엔진·플랫폼·실제 다른 PC는 별도 검증 대상입니다. 배포본은 소스 전용이며 기존 PC의 DLL을 다른 엔진 빌드에 복사하는 방식은 지원하지 않습니다.

외부 실행기는 별도 Python 3.11 x64 환경을 기준으로 하며 이번 환경은 3.11.15입니다. 에디터 내장 Python이나 Codex 내부 환경을 일반 사용의 필수 경로로 지정하지 않습니다.

## 배치와 활성화

ZIP을 별도 폴더에 풀어 파일 목록을 확인하고 `Plugins/CubelessMCP_UE58`와 `cubeless`를 대상 `.uproject` 바로 아래에 함께 복사합니다. 실제 폴더 복사만 지원하며 정션·심볼릭 링크를 만들지 않습니다. 사용자 `.uproject`·AGENTS.md·콘텐츠·기존 로컬 설정과 기록은 배포물이 덮어쓰지 않습니다. 기존 플러그인이 있으면 업데이트 절차를 먼저 따릅니다.

프로젝트의 플러그인 설정에서 `CubelessMCP_UE58`, 공식 `ModelContextProtocol`, `ToolsetRegistry`, `PythonScriptPlugin`을 Editor에 활성화합니다. 실행기에는 공식 `EditorToolset`, `NiagaraToolsets`, `PCGToolset`도 필요합니다. Niagara·PCG·ControlRig·ChaosClothAsset/Dataflow 등 플러그인 descriptor의 의존 엔진 플러그인이 설치되어 있어야 합니다. descriptor의 `TargetAllowList=["Editor"]`를 게임 의존성으로 바꾸지 않습니다. 생성 콘텐츠가 쓰는 일반 Niagara/PCG 런타임 기능과 MCP 의존성은 별개입니다.

## 빌드와 실행

진행 요청과 dirty 에셋을 먼저 확인하고 저장/보존한 뒤 해당 프로젝트 에디터를 닫습니다. 다음 `$Engine`과 `$Project`는 사용자가 확인한 **해당 PC의 값**입니다. 엔진은 `.uproject`의 EngineAssociation과 Epic/커스텀 설치 정보를 대조하고 `Engine/Build/Build.version`의 5.8 계열을 확인하십시오. 폴더명으로 엔진이나 Target 이름을 추측하지 않습니다.

```powershell
$Engine = '<해당 PC의 UE 5.8.x 설치 루트>'
$Project = '<프로젝트 루트>/어떤이름.uproject'
# Blueprint-only 프로젝트: 설치 엔진의 UnrealEditor Target을 사용합니다.
& "$Engine/Engine/Build/BatchFiles/Build.bat" UnrealEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
& "$Engine/Engine/Binaries/Win64/UnrealEditor.exe" $Project -ModelContextProtocolStartServer -ModelContextProtocolPort=8000
```

위 빌드 예시는 기존 에디터를 유지하는 이 PC의 메모리 여유를 고려해 UBA를 끄고 병렬 수를 2로 제한합니다. 설치 소스에서 이 인수를 확인했으며 UBA의 메모리 commit 실패 로그와 일반 컴파일 실패를 구분합니다. 시스템 페이지 파일 설정은 변경하지 않습니다.

실제 C++ 프로젝트는 기존 `Source/*Target.cs`의 Editor Target과 프로젝트 빌드 절차를 사용합니다. 플러그인 설치를 위해 게임 C++ 모듈을 새로 만들지 않습니다. 에디터 내부 Python 의존성은 uplugin의 PythonRequirements이며 외부 실행기의 가상환경과 분리됩니다. 엔진 설치의 Python pip 기능이 꺼졌거나 패키지 설치가 실패하면 로그를 확인해 정상 설치한 뒤 로딩을 재확인합니다.

이 PC의 설치 엔진/UBT/MSVC 조합에서는 한글 경로의 C++ response-file 경로가 잘못 해석되어 C1083이 발생했습니다. 실행기 경로 규칙과 구분되는 네이티브 빌드 제약입니다. 이 문제가 재현되면 같은 배포 소스를 공백이 허용된 영문 소유 빌드 프로젝트에서 빌드하고, 동일한 실제 엔진 빌드의 플러그인 산출물만 대상 프로젝트에 적용한 뒤 실제 로딩을 확인하십시오. 시스템 코드 페이지나 엔진 소스를 임의로 변경하지 않습니다. 한글 경로의 직접 네이티브 빌드가 통과했다고 주장하지 않습니다.

공식 MCP HTTP 주소는 위 예에서는 `http://127.0.0.1:8000/mcp`입니다. Codex 등의 클라이언트에 해당 주소를 연결하고 `initialize`의 세션 ID를 유지합니다. 공식 `describe_toolset`으로 `cubeless_mcp_ue58.registration.toolsets.CubelessBlueprintTools` 등을 조회합니다. 외부 실행기는 연결할 때 실제 editor project_file·엔진 버전·PIE 상태를 검사하며, 다른 프로젝트이면 변경 호출을 보내지 않습니다. 자동 재연결·수정 재시도는 없습니다.

## 외부 Python과 경로

```powershell
python -m venv '<프로젝트 루트>/cubeless/.venv-pipelines'
& '<프로젝트 루트>/cubeless/.venv-pipelines/Scripts/python.exe' -m pip install -r '<프로젝트 루트>/cubeless/tools/pipeline-requirements.txt'
& '<프로젝트 루트>/cubeless/.venv-pipelines/Scripts/python.exe' '<프로젝트 루트>/cubeless/tools/run_pipeline.py' blueprint --endpoint http://127.0.0.1:8000/mcp --input cubeless/tools/pipelines/examples/blueprint.json
```

마지막 명령은 preview이며 실행 위치는 어느 디렉터리여도 됩니다. `-m pipelines.production_*`도 기존과 같이 지원하지만 모듈 검색을 위해 해당 묶음의 `cubeless/tools`를 PYTHONPATH에 지정해야 합니다. 새 진입점은 이 설정이 필요 없습니다.

실행기 자신의 실제 파일 위치와 `distribution.json` 식별자·필수 구조로 cubeless를 확인합니다. 바로 위에 `.uproject`가 하나면 선택하고, 없으면 오류, 여러 개면 `--project 프로젝트명.uproject`를 요구합니다. 상위 폴더를 끝없이 탐색하지 않습니다. 명시한 파일은 실제 `.uproject`여야 하고 같은 부모 프로젝트에 있어야 합니다. 다른 프로젝트의 절대 경로를 지정하면 연결 전에 거절합니다. 링크·정션으로 모호해지는 설치도 거절합니다.

`--input`, `--output`, `--project` 및 설정의 `playback.engine_log_path`, `visual.output_dir` 상대 경로는 **확인한 프로젝트 루트 기준**입니다. 기존 cwd 상대 호출은 이에 맞춰 고칩니다. 절대 경로는 지원하되 프로젝트와 캡처 API의 소유 경계는 유지됩니다. 기본 결과는 `cubeless/records/operations/pipeline-<고유ID>.json`이며, `--verification`은 `cubeless/records/verification` 아래의 출력만 허용합니다. 에셋 경로 `/Game/...`는 변환하지 않습니다.

`--verification`은 허용 루트와 출력 파일·`.partial` 임시 파일·`.evidence` 폴더 및 기존 상위 경로에 있는 정션·심볼릭 링크·reparse point를 거절합니다. 허용 루트를 해석한 결과도 원래 프로젝트의 cubeless 안에 있어야 합니다. 정상적인 새 결과 폴더는 허용하지만 링크로 다른 프로젝트에 기록을 쓰는 구성은 지원하지 않습니다.

## 에이전트 지침 연결

플러그인을 설치했다고 에이전트가 지침을 자동으로 읽는 것은 아닙니다. 호스트 프로젝트의 기존 AGENTS.md를 보존하면서 다음 한 줄을 직접 통합하고, 실행 중인 세션에 읽도록 요청하십시오.

```markdown
작업 전에 `cubeless/codex/AGENTS.md`를 읽고 현재 운영 지침을 적용한다.
```

공통 반복 작업의 자동 승격은 `cubeless/codex/policies/pipeline-promotion.md` 한 곳에서 관리합니다. 역할 문서 제공은 하위 에이전트 실행과 다릅니다. 사용 중이던 프로젝트의 AGENTS.md를 설치 프로그램이 덮어쓰지 않습니다.

현재 확인한 엔진 빌드는 UE 5.8.4 / CL 58902052입니다(2026-10-28). 소스 지원 계열은 5.8.x이며 DLL은 실제 설치 엔진용으로 빌드합니다. 이전 패치의 DLL 호환성이나 후속 패치의 동작을 이 기록으로 보장하지 않습니다.
