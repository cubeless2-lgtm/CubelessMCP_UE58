# CubelessMCP_UE58

UE 5.8.x 공식 ModelContextProtocol 서버와 Toolset Registry를 확장하는 Editor 전용 프로젝트 플러그인입니다. 기존 UE 5.6 기능의 이식은 진행 중이며, 전체 기능 이식을 완료한 배포판은 아닙니다.

이 저장소에는 실행 가능한 `UnrealMCP5_8_3.uproject`, 프로젝트 공용 `Config/`, 일반 `Content/`와 에디터 플러그인 소스가 포함됩니다. `Source/`와 `Build/`의 프로젝트 소스·리소스도 추적할 수 있도록 구성합니다. 현재 게임은 Blueprint 프로젝트이며 별도 게임 C++ 모듈은 없습니다. `Content/_MCP_Temp/`, 개인 작업용 `Content/Developers/` 및 대응하는 외부 액터·오브젝트는 제외합니다. 일반 월드의 `__ExternalActors__`·`__ExternalObjects__`는 콘텐츠와 함께 추적합니다. 프로젝트 Git 추적과 플러그인 배포 ZIP은 별개이며 ZIP에는 명시 배포 목록의 파일만 포함합니다. 검증용 프로젝트·자료, 개인 설정, 인증값, 빌드 산출물과 캐시는 제외합니다.

## 저장소를 복제한 뒤 프로젝트 실행

1. Unreal Engine **5.8.x**과 해당 엔진용 Visual Studio C++ 빌드 도구·Windows SDK를 설치하고 저장소를 복제합니다. 게임 C++은 없지만 소스로 제공하는 에디터 플러그인을 처음 빌드할 때 C++ 도구가 필요합니다.
2. `UnrealMCP5_8_3.uproject`를 UE 5.8.x으로 엽니다. 엔진 선택이 필요하면 파일의 `Switch Unreal Engine version`에서 5.8을 지정합니다. 누락된 플러그인 모듈을 다시 빌드하라는 메시지가 나오면 빌드합니다. `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, IDE 작업 파일은 각 PC에서 생성합니다.
3. 기본 맵은 기존 `ThirdPerson/Lvl_ThirdPerson`입니다. Crimson Dawn은 `/Game/VampireSurvival/Maps/L_CrimsonDawn`을 열고 새 에디터 창에서 PIE로 실행합니다. 창 크기는432×768 또는720×1280을 권장합니다. [게임 조작·검수 기록](cubeless/studies/VampireSurvival/UPDATE_V8.md)을 참조하십시오.

`.uproject`의 MCP/Python/Cubeless 플러그인은 Editor 대상으로 제한되어 있으며 게임 에셋은 MCP 런타임을 요구하지 않습니다. 게임을 플레이하는 데 MCP 서버를 시작할 필요는 없습니다. 플러그인만 다른 프로젝트에 설치하는 방법은 아래 절차를 따릅니다.

공용 Engine 설정은 UE와 빌드 도구가 읽는 `Config/<플랫폼>/<플랫폼>Engine.ini`에 저장합니다. Android와 iOS는 세로 방향으로 고정합니다. UE가 Android 파일 서버의 PC별 인증값을 자동 기록하는 `Config/DefaultEngine.ini`는 로컬 생성 파일로 취급하여 Git에서 제외합니다. 기존 로컬 파일과 `Saved/Config/WindowsEditor/Engine.ini`의 인증값은 유지합니다. 개인 에디터 환경인 `DefaultEditorPerProjectUserSettings.ini`와 `User*.ini`도 게시하지 않습니다. 설정 수정·플랫폼 추가 방법은 [공용 설정 안내](Config/README.md)를 참조하십시오.

에디터 플러그인을 명령줄에서 빌드하려면 저장소 루트의 PowerShell에서 다음을 실행합니다. 엔진 설치 위치가 다르면 첫 경로를 바꿉니다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' UnrealEditor Win64 Development "-Project=$((Resolve-Path '.\UnrealMCP5_8_3.uproject').Path)" -WaitMutex -NoHotReloadFromIDE
```

2026-10-27 검증: Git 게시 대상만 추출한 검증본에서 기존 프로젝트의 `Binaries/`·`Intermediate/`·`Saved/`·`DerivedDataCache/`를 복사하지 않고 에디터 플러그인을 새로 빌드했다. 기본 ThirdPerson 맵과 Crimson Dawn 맵, 게임 Blueprint3개의 로드 및432×768 게임 타이틀 렌더를 확인했다. 클린 빌드의 익명 네임스페이스 충돌을 방지하도록 에디터 모듈은 파일별 컴파일을 사용한다. 게임 C++ 모듈은 추가하지 않았다. 게임 전체 플레이·모바일 기기 검증 범위는 [V8 기록](cubeless/studies/VampireSurvival/UPDATE_V8.md)을 따른다.

설치 및 사용:

1. `Plugins/CubelessMCP_UE58` 폴더를 사용할 프로젝트의 `Plugins/` 아래에 복사합니다.
2. UE 5.8.x에 맞는 C++ 빌드 환경에서 `CubelessMCPUE58Editor` 모듈을 빌드합니다. 이 저장소에는 미리 빌드한 DLL을 포함하지 않습니다.
3. 프로젝트에서 `CubelessMCP_UE58` 플러그인을 활성화하고 에디터를 다시 시작합니다. 의존 플러그인은 `.uplugin`에 선언되어 있습니다.
4. 에디터의 공식 ModelContextProtocol 서버를 시작합니다. 같은 공식 MCP 연결에서 Cubeless Toolset을 검색하고 호출합니다. 별도 Python MCP 서버나 TCP 브릿지는 필요하지 않습니다.

에디터 Python은 플러그인 내부 `Content/Python/`에서 등록됩니다. `.uplugin`에 고정된 PythonRequirements는 NumPy 2.4.6과 Pillow 12.2.0입니다. 설치 환경에서 해당 의존성을 준비해야 합니다.

게임 모듈에 이 플러그인을 하드 의존성으로 추가하지 마십시오. 모듈은 Editor 대상에만 로드되며, 생성한 게임 에셋에도 Cubeless/MCP 런타임 타입을 참조시키지 않아야 합니다.

기존 UE 5.6 전체 지원 범위의 이식은 아직 완료되지 않았습니다. 소스 등록이나 대표 샘플 성공이 모든 원본 계약의 완료를 뜻하지 않으며, 각 도구 설명에 명시된 지원 범위와 실패 시 변경·저장 상태를 확인해야 합니다.

Blueprint·Niagara·PCG의 선택형 외부 실행기는 [실행기 안내](cubeless/tools/pipelines/README.md)를 참조하십시오. 플러그인 로딩에 필요한 구성은 아니며, 공식 MCP 연결을 사용합니다. Niagara edit 모드의 `save` 기본값은 `false`이고 Blueprint 바인딩과 manifest에도 같은 저장 정책을 적용합니다.

프로젝트에 설치할 때는 `Plugins/CubelessMCP_UE58`와 필요한 `cubeless` 파일을 함께 배치합니다. [도구 묶음 안내](cubeless/README.md)의 소스 배포·설치·업데이트 절차를 따르십시오. 새 `cubeless/tools/run_pipeline.py`는 실행 위치와 관계없이 자신의 배포 식별자와 바로 위 `.uproject`를 확인합니다. Git 저장소의 개발 자료 전체를 프로젝트에 복사할 필요는 없습니다.

로드된 LevelInstance 패킹:

- 공식 MCP의 `describe_toolset`으로 `cubeless_mcp_ue58.registration.loaded_level_instance_packing_toolset.CubelessLoadedLevelInstancePackingTools`를 조회합니다.
- `inspect_loaded_level_instance_packing(source_actor_path, target_actor_path)`으로 기존 Actor의 정확한 객체 경로를 검사합니다. 선택 상태·Actor 라벨은 사용하지 않습니다.
- 같은 Toolset의 `pack_loaded_level_instance`를 다음 입력으로 호출합니다. 먼저 `dry_run=true`로 확인하고, 실제 반영할 때만 `false`로 바꿉니다.

```json
{
  "source_actor_path": "/Game/Maps/Host.Host:PersistentLevel.SourceLevelInstance",
  "target_actor_path": "/Game/Maps/Host.Host:PersistentLevel.TargetPackedLevelActor",
  "operation_id": "pack-unique-request-001",
  "dry_run": true,
  "allow_replace": false,
  "save": false
}
```

현재는 같은 Editor 월드의 native PLA 대상과 이미 완전히 로드된 평면 구조의 non-World-Partition 원본을 지원합니다. 양쪽 WorldAsset은 비어 있지 않고 같아야 하며, 필터와 LevelInstance 편집 세션은 없어야 합니다. Blueprint/SCS 대상·중첩 LevelInstance·partitioned 원본은 거절합니다. 원본 Actor 512개, 컴포넌트 4096개, 메시 인스턴스 2048개가 초기 작업 한도입니다. 기존 대상에 데이터가 있으면 명시적인 `allow_replace=true`가 필요합니다.

좌표는 UE 5.8의 streaming transform·대상 transform·pivot 계산을 따릅니다. 서로 다른 위치의 PLA로 패킹하면 원본 메시의 월드 위치가 그대로 유지된다고 가정하지 말고 `expected_engine_placement`와 실제 결과를 확인하십시오. 엔진 bool이 true여도 미지원 컴포넌트나 관측된 경고가 있으면 `packing_incomplete`이며, 대상에는 이미 메시가 생성됐을 수 있습니다. 콜백으로 상태가 바뀌면 불확실 실패로 반환하고 자동 재시도하지 않습니다. 트랜잭션은 원자적 롤백을 보장하지 않습니다.

저장은 기본적으로 하지 않습니다. `save=true`는 독점 외부 Actor 패키지만 허용하며, 일반 맵 Actor에서는 변경 전에 거절합니다. 일반 맵은 결과를 확인한 뒤 별도의 명시적 맵 저장이 필요합니다. 원본을 삭제하거나 저장하지 않으며, 성공 반환도 원본 삭제의 안전성을 판정하지 않습니다.

지원 계열은 UE 5.8.x입니다. 플러그인 DLL은 실제 사용할 엔진 빌드에 맞춰 재빌드합니다. 패치별 실행 근거는 별도로 기록하며, 계열 지원이 모든 후속 패치의 사전 검증을 의미하지 않습니다.

2026-10-28 기준 UE 5.8.4 / CL 58902052에서 Editor 모듈 재빌드, 공식 MCP 등록·스키마와 Blueprint·Material·Niagara 파라미터·PCG 대표 경로를 확인했습니다. 모든 API 변형이나 후속 패치의 검증 완료를 뜻하지 않습니다.
