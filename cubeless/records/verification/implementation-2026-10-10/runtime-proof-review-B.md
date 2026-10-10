# 최소 fixture 런타임 독립성 검토 B

검토일: 2026-10-10. 검토 담당: `python_toolset_implementation`. 이 검토는 기존 파일을 읽고 SHA-256을 다시 계산했다. 에디터·게임·MCP를 호출하거나 에셋·플러그인 소스를 수정하지 않았다.

**판정: 이번 최소 fixture가 Cubeless 및 공식 MCP 없이 에디터에서 로드·컴파일되고 Win64 Shipping 게임에서 실행됐다는 제한된 주장은 근거가 충분하다. 플러그인을 설치한 프로젝트의 Game 타깃 자동 제외, 모든 이식 기능의 런타임 독립성, 모든 직렬화·동적 참조의 부재까지 검증한 것은 아니다.**

실제 MCP로 작성한 `BP_MutationContract`와 Engine API로 구성한 `BP_RuntimeProof`, `BP_MarkerData`, `Map_RuntimeProof` 네 파일이 대상이다. `BP_MutationContract`의 Branch 편집 결과가 맵에 포함됐고 플러그인 없는 에디터에서 컴파일됐다. 런타임 marker를 만드는 BeginPlay 코드는 별도 `BP_RuntimeProof`에 있다. 따라서 marker만으로 기존 Branch 편집 로직의 게임 실행 의미나 모든 이전 기능의 동작이 검증됐다고 표현하면 안 된다.

| 확인 항목 | 관측 근거 | 허용되는 결론 |
|---|---|---|
| 별도 에디터에서 MCP 제거 | `runtime-no-mcp-validation-C.json`: PID 54660, 별도 UE58RuntimeProof 프로젝트. 활성 플러그인은 ContentBrowserFileDataSource, EditorScriptingUtilities, PythonScriptPlugin뿐이다. 로컬 project plugin 없음 검사도 통과했다. | 에셋을 읽고 컴파일하는 데 Cubeless/ModelContextProtocol/ToolsetRegistry가 필요하지 않았다. 에디터 검수용 Python은 사용됐다. |
| Blueprint 및 맵 | Blueprint 3개 compile 반환 true, 모두 `BS_UP_TO_DATE`, 오류·경고 없음. 두 fixture Actor의 class path와 label을 맵에서 확인했다. | 이 네 파일의 로드·컴파일·맵 참조가 해당 에디터 환경에서 유효하다. |
| 원본과 복사본의 동일성 | 네 파일 각각 author SHA = 에디터 검수 전 SHA = 검수 후 SHA. 이번 검토 시 디스크를 다시 읽어 모두 일치했다. dirty packages after compile도 빈 배열이다. | 에디터 검수는 저장으로 내용을 바꾸지 않았고, 검수한 복사본의 출처가 author fixture와 연결된다. |
| 참조 분류 | C는 옵션 7개 중 선택한 분류 하나만 true다. 네 hard-package edge, 관리 참조는 없음, `/Script/NavigationSystem` 하나가 native terminal이다. | 현재 AssetRegistry가 보고한 범위에서 MCP/Toolset 패키지 참조를 찾지 못했다. B의 잘못 섞인 management 분류는 사용하지 않는다. |
| Shipping cook/package | `build-runtime-shipping-B.log`: Win64 Shipping, 해당 맵 명시, `-SkipCookingEditorContent`, BuildCookRun 성공 및 ExitCode 0. | 해당 고립 프로젝트의 cook/stage/pak/package/archive가 성공했다. |
| 실제 staged 내용 | 정적 보고서: 파일 23개, PAK 1개, 직접 UnrealPak 목록 1782개와 보고된 총계 일치. 네 fixture의 cooked uasset/umap/uexp 항목이 목록에 실제 존재한다. 알려진 금지 모듈/플러그인 항목 위반 없음. | 확인한 파일명·receipt·PAK entry 범위에서 MCP 관련 배포 항목이 발견되지 않았다. |
| 실제 게임 실행 | fresh baseline, 실행 PID 38384, ExitCode 0, 실행 파일 SHA 일치. 새 SaveGame 파일 1423 bytes, mtime이 실행 구간 안에 있고 GVAS 헤더의 클래스가 해당 run의 `BP_MarkerData_C`와 정확히 일치한다. | 실행한 Shipping 게임에서 run 전용 Blueprint SaveGame marker 경로가 작동했다. |

**빌드 기록의 해석**

B의 Build 단계는 `Target is up to date`, `0 action(s)`, `No build products were made`였다. B에서 C++ 전체를 새로 빌드했다고 쓰면 안 된다. A 로그에는 `CubelessRuntimeProof.cpp` 컴파일, Shipping exe 링크, receipt 기록이 실제로 있으며 이 C++ 빌드는 성공했다. A의 전체 UAT 실행은 이후 단계에서 실패했고, B가 기존 바이너리를 이용해 cook/package를 성공시켰다. “A의 native 빌드 성공 + B의 cook/package 성공 + B의 실행 검증”으로 구분한다.

B 로그에는 ZenStore/IoStore 설정 조합 경고와 ACL animation compression 설정 오브젝트를 찾지 못한 경고가 있다. 빌드 종료와 fixture 실행은 성공했지만 “경고 없는 패키징”으로 표현하지 않는다. 이 fixture는 skeletal animation 검증을 포함하지 않는다.

**사용자의 C++ 제한과 자동 생성 stub**

`UE58RuntimeProof.uproject`에는 `Modules`가 없고 게임 소스가 추가돼 있지 않다. A 로그는 “Paper2D plugin is disabled” 때문에 code-free 프로젝트를 code-based로 취급하고 임시 Target.cs를 생성했다고 기록한다. 다음 파일들은 `Intermediate/Source`의 UAT 생성물이다.

- `CubelessRuntimeProof.Build.cs`: private dependency `Core`가 두 번 선언돼 있다. 서로 다른 두 의존성이 아니라 같은 Core의 중복 선언이다.
- `CubelessRuntimeProof.cpp`: `CoreTypes.h`, `Modules/ModuleManager.h`와 `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, ...)`만 있다.
- `CubelessRuntimeProof.Target.cs`: Game 타깃에 이 기본 모듈을 추가한다.

이 stub은 실제 Shipping exe에 컴파일된 기본 게임 모듈이므로 “런타임에 C++가 전혀 없다”는 주장은 부정확하다. 다만 Cubeless/공식 MCP/Toolset/Python/UnrealEd 의존성을 선언하지 않으며, 게임플레이 로직·노출 UClass·플러그인 호출을 추가하지 않는다. 사용자가 금지한 MCP 플러그인에 대한 게임의 잔존 의존성을 만드는 C++와 구분된다. 원래 대상 프로젝트의 `Source` 디렉터리는 이번 검토 시 존재하지 않았다. 임시 생성물을 게임 소스나 설치 필수 파일로 배포해서는 안 된다.

**플러그인을 설치한 프로젝트에 남은 검증**

현재 소스의 정적 설계는 Editor 범위를 명확히 제한한다.

- `CubelessMCP_UE58.uplugin`: 모듈 `CubelessMCPUE58Editor`의 `Type=Editor`, `TargetAllowList=[Editor]`, `LoadingPhase=PostEngineInit`. 공식 MCP, ToolsetRegistry, PythonScriptPlugin, EditorScriptingUtilities 의존 플러그인도 Editor 타깃으로 제한된다.
- `CubelessMCPUE58Editor.Build.cs`: Editor 이외 타깃이면 BuildException을 발생시키는 방어 조건이 있다. Engine/UnrealEd/ToolsetRegistry/MaterialEditor/RHI 등은 플러그인의 Editor 모듈 내부 의존성이다.
- 원래 대상 `.uproject`의 Cubeless 플러그인 활성화 항목도 Editor 타깃으로 제한된다. 게임 모듈의 하드 의존성은 추가돼 있지 않다.
- `CanContainContent=true`지만 현재 플러그인 Content 아래 `.uasset`/`.umap`은 없다. Python 소스가 포함된다. 미래에 Editor 도구 에셋을 추가하면 cooking/staging 정책도 다시 확인해야 한다.

그러나 B 패키지는 Cubeless 플러그인을 애초에 복사하지 않은 프로젝트에서 만들었다. 따라서 다음은 아직 직접 증명되지 않았다.

1. 프로젝트 Plugins 안에 플러그인 파일과 Editor 바이너리가 실제로 존재하고 Editor에서 활성화된 상태에서, 동일 프로젝트의 Shipping/Game 빌드가 Cubeless Editor 모듈을 제외하는지.
2. 이 구성에서 ToolsetRegistry·ModelContextProtocol·PythonScriptPlugin 의존성이 Game build receipt, link inputs, staged files 및 PAK에서 제외되는지. Editor 모듈의 BuildException만으로 이 결과를 대신할 수 없다.
3. 활성화한 플러그인의 Content/Python 소스가 Shipping에 불필요하게 스테이징되지 않는지. Game/runtime 의존성과 단순 소스 파일 포함은 다른 문제지만 둘 다 검사해야 한다.

후속 acceptance는 사용자의 기존 에셋을 건드리지 않는 또 다른 고립 프로젝트에서 플러그인을 설치하고, Editor 로드와 Game cook/package를 각각 수행하는 것이다. 실제 Game receipt/link/staging/PAK 검토와 동일 marker 실행을 연결해야 한다. 이 검토가 그 빌드 실행을 수행하거나 완료 처리하지는 않는다.

**직렬화·동적 참조 및 증거 연결의 한계**

- AssetRegistry 조사는 저장된 package hard/soft 참조와 현재 registry의 management 참조를 대상으로 한다. `/Script` native 모듈은 terminal이며 native binary 내부 의존성을 재귀 조사하지 않았다. searchable names, 코드의 동적 문자열 로드, 아직 생성·갱신하지 않은 AssetManager management 규칙은 범위 밖이다.
- PAK 이름과 receipt에 금지 문자열이 없다고 모든 UObject export/import와 serialized field를 파싱한 것은 아니다. 정적 검사기는 이 한계를 올바르게 보고하며 `packaging_independence_proven=false`와 `full_migration_complete=false`를 유지한다. 이번 PAK은 직접 목록을 읽었지만 IoStore 검사 구현은 없다.
- fixture의 런타임 성공은 실제 실행 경로에 MCP가 필요하지 않았다는 강한 관측이다. 실행되지 않은 분기, 모든 게임 콘텐츠, 모든 엔진 플랫폼까지 보편화하지 않는다. `-nullrhi -NoSound` 실행이므로 렌더링·오디오 품질도 검증하지 않는다.
- marker 검증기는 GVAS 헤더를 실제 파싱하고 exact class를 비교한다. payload 14 bytes는 역직렬화하지 않았다. 전용 경로·실행 PID·시간·실행 파일 해시를 연결하지만 전체 게임 기능 검증은 아니다. 시간 허용 오차는 명시된 ±2초다.
- receipt는 Game/Win64/Shipping과 staged 실행 파일 basename으로 연결되며, 빌드 산출물에 대한 암호학적 서명은 아니다. marker baseline은 exe SHA를 고정하지만 실행 시점의 PAK SHA를 launch report에 직접 묶지는 않는다. 이번 검토는 현재 exe·PAK·목록·marker 해시가 각 보고서와 동일함을 다시 확인했다. 다음 실행에서는 exe와 PAK을 함께 launch baseline에 고정하면 산출물 연결이 더 명확해진다.
- 이 검토 시 Build.cs에는 새 MaterialEditor/RHI 의존성이 있다. MCP-free B는 이 플러그인을 빌드하거나 포함하지 않았으므로, B를 최신 Material 구현의 Game 타깃 제외 검증으로 재사용할 수 없다.

**독립 재계산 결과**

네 asset SHA 연결, PAK 전후 SHA, PAK 목록 SHA, 실행 exe SHA, marker SHA, fixture report SHA, MCP-free project descriptor SHA 총 10개 비교가 모두 일치했다. 다음 파일 지문을 기준으로 검토했다.

| 파일 | SHA-256 |
|---|---|
| runtime-no-mcp-validation-C.json | `ae5cd71fef5bbbb4c7e331f423e7aa1df8671ceb6ccbe72b427484eb4e9e8f8a` |
| runtime-package-static-B.json | `a933f52114790fde4d1875ca0db8d35d20af4126f623007f47f93b9513363880` |
| runtime-marker-verification-B.json | `18f8ad3f90969736f53977c03fa84d71a7f242ee1aba59d0a7d2435658861ded` |
| runtime-marker-baseline-B.json | `07060e7dfe0b27e088d45cff95307990d4e0ceeebadd756982f2404a581bbc87` |
| runtime-fixture-B.json | `a0a60c983c6cd292ae2a0838a0a02d294d12f695acfca5f5fea4a841a9d06c2a` |
| build-runtime-shipping-B.log | `b506a4878cc99910705dd6b7b2e41010bd3553e11a81615481279289abd0be34` |
| CubelessMCP_UE58.uplugin (현재) | `aa3d7d74f4bce99663dc9ba3f605589506a201c63c4952539f544d96264b382f` |
| CubelessMCPUE58Editor.Build.cs (현재) | `7005f515b4de9251c6b535d8fdc3b033cd7ed95b0af6aa5a3e6a8ab8103d642e` |

근거 파일은 이 보고서와 같은 `cubeless/records/verification/implementation-2026-10-10` 디렉터리에 있다. 검토한 생성 stub은 `cubeless/workspaces/UE58RuntimeProof/Intermediate/Source`에 있으며, 배포 대상 운영 코드가 아니다. 전체 이식 완료나 전체 런타임 의존성 부재 판정은 보류한다.
