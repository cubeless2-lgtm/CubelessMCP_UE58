# CubelessMCP_UE58

UE 5.8.3 공식 ModelContextProtocol 서버와 Toolset Registry를 확장하는 프로젝트 에디터 플러그인입니다. 현재 이식 구현 중이며 전체 UE 5.6 기능 이전을 완료한 배포판이 아닙니다.

## 구성

- `Plugins/CubelessMCP_UE58/`: 에디터 C++와 Python Toolset 구현.
- `cubeless/codex/`: 현재 지침·역할·워크플로·정책 및 별도로 보존한 과거 자료.
- `cubeless/tools/`: 공식 MCP를 사용하는 외부 파이프라인과 검수 도구.
- `cubeless/records/`: 기능 대응, 이식·검증·성능 증거.

게임 프로젝트의 기존 Content, Config, .uproject와 빌드 캐시는 이 플러그인 배포 저장소의 추적 대상이 아닙니다. 현재 로컬 호스트 프로젝트의 설정은 설치 검증을 위해 별도로 관리합니다.

## 설치 계약

프로젝트의 `Plugins` 아래 플러그인을 설치하고 에디터에서 활성화합니다. 공식 MCP·Toolset Registry와 PythonScriptPlugin이 필요합니다. 별도 Cubeless MCP 서버나 자체 TCP 브릿지는 사용하지 않습니다. 실제 지원 기능과 검증 수준은 기록에 표시하며, 빌드·도구 등록·동작 성공을 구분합니다.

`.uproject`의 Plugins 항목에 에디터 대상 제한을 명시합니다. 게임 모듈의 Build.cs에는 이 플러그인을 추가하지 않습니다.

```json
{ "Name": "CubelessMCP_UE58", "Enabled": true, "TargetAllowList": ["Editor"] }
```

설치한 UE 5.8.3의 개발 도구로 Editor 플러그인을 빌드하고 에디터를 시작한 뒤, 기존 공식 MCP 연결에서 `list_toolsets`를 호출합니다. `Cubeless`가 포함된 **전체 등록 이름**으로 `describe_toolset`을 호출하고 반환된 스키마를 사용합니다. 예를 들어 `cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools`이며, 짧은 클래스 이름만 전달하면 검색되지 않습니다. 개발 중인 소스와 검증 시점의 배포본은 기록의 파일 해시로 구분합니다.

운영 지침도 사용하려면 `cubeless`를 프로젝트 루트에 배치하고 루트 `AGENTS.md`의 안내를 기존 지침에 병합합니다. 기존 지침을 덮어쓰지 않습니다. 해당 프로젝트를 연 새 Codex 작업에서 현재 지침을 읽는지 확인합니다. 역할 문서 설치만으로 하위 에이전트가 자동 실행되지는 않습니다.

생성된 게임 에셋에 이 플러그인의 타입·함수·클래스 참조를 남기지 않습니다. MCP 없는 로드·컴파일·Win64 패키징·실행 검증은 별도 완료 조건입니다.

## 현재 사용 범위

2026-10-11 현재 사용자의 요청으로 AA3에서 이식 작업을 일시 중단했습니다. AA는 57개 도구 등록과 Python 계약 검사 121개를 확인했고, 복제 GUID 및 객체 null 기본값 비교 문제를 수정한 AA3 빌드가 통과했습니다. **AA3 수정 후 실제 상태 전달·MCP 없는 실행 검증은 아직입니다.** 아래 Z3는 마지막으로 대표 실행 검증까지 마친 범위입니다. [재개 체크포인트](cubeless/records/migration/implementation-2026-10-10/PAUSED-AA3-20261011.md)를 참고하세요.

최신 완료 검증본 Z3는 8개 Cubeless Toolset / 56개 도구이며, 배치 manifest는 30개 named capability입니다. 저장된 Actor Blueprint를 부모로 둔 자식, 상속 변수 getter, ReceiveTick event와 부모 호출, 조건부 외부 컴포넌트 호출을 대표 경로로 검증했습니다. parent-call 도구는 신규 확장이며 원본 기능 이식 수에 추가하지 않습니다. 전체 5.6 기능 호환 완료는 아닙니다.

Z3는 부모·자식을 각각 compile/save한 뒤 MCP 없는 새 프로세스에서 여섯 직접 ReceiveTick 호출을 실행했습니다. ParentDelta·ChildDelta는 매회 입력과 같고 컴포넌트 타이머는 조건에 따라 0/0/0/.25/.75/.75로 누적됐습니다. 소유 Actor 제거와 파일·CDO·월드·설정·선택·dirty 보존을 확인했습니다. factory event/parent-call 재사용을 검증한 것으로 새 노드 생성, 실제 게임 Tick 스케줄링·BeginPlay·hardware input·원본 전체 host·Shipping은 미검증입니다. [Z 실패·교정·실행 범위](cubeless/records/migration/implementation-2026-10-10/Z-progress-incident.md)에 이전 실패도 보존했습니다.

앞선 Y는 MouseButtons component와 caller를 작성·compile/save 각 1회 후, MCP 없는 새 프로세스에서 실제 Actor를 생성해 component 소유 관계와 .25/.5 호출의 누적 .25/.75를 확인하고 제거했습니다. 파일·CDO·월드 Actor 집합·설정·선택·dirty 상태가 보존됐습니다. 에디터 함수 조회는 skeleton을 사용할 수 있지만 저장 member parent와 self 핀은 컴파일된 generated class를 참조합니다. Y 당시 상속 Tick은 미검증이었으며 Z3의 직접 이벤트 호출과 구분합니다. 외부 변수 setter·generated SceneComponent·원본 전체 host·Shipping은 미완료입니다.

X는 W3 clone의 연결을 교체·재연결한 뒤 compile/save 각 1회와 11개 노드/12개 연결을 확인했습니다. MCP 없는 새 프로세스의 타이머 6/6 실행도 통과했습니다. 현재 5.6 소스에는 원본 문서·파이프라인이 요구하는 보호 필드의 강제가 없어, 그 문서 계약을 근거로 복구한 범위입니다. 데이터 연결 교체, 도구 옵션 `compile=True`/`save_asset=True`, 원본 전체 host는 미완료입니다. [X 계약·실행 범위](cubeless/records/migration/implementation-2026-10-10/X-guarded-links-plan.md)를 참조합니다.

V2 복제본과 source는 W 배포 재시작 후 실제 Codex 공식 연결에서 graph/GUID·CDO·파일 불변을 확인했습니다. 별도 W3 fixture는 ownerless ActorComponent에서 각 조건 false 세 사례, 모두 true 누적 두 사례와 false 복귀를 실행해 6/6 통과했습니다. LastDelta는 항상 갱신되고 타이머는 세 조건이 모두 true일 때만 누적되어 최종 .75였습니다. 이는 MCP 없는 새 프로세스의 직접 함수 실행이며 원본 inherited Tick·외부 component host·press/release reset·Shipping을 검증한 것은 아닙니다. 최초 Sequence GUID 비교 실패와 인스턴스 편집 제한 실패를 보존하고, 후자는 테스트 gate만 is_exposed=True로 교정했습니다.

로컬 SCS 추가·제한된 typed 속성 설정은 SceneComponent·StaticMeshComponent·CameraComponent·SpringArmComponent와 SphereComponent·BoxComponent·CapsuleComponent를 지원합니다. named set_static_mesh_properties의 기본 Cube 설정과 세 Shape 크기 작성·대표 오류·저장·새 프로세스 조회를 확인했습니다. R은 같은 도구에 숨은 C++ material 슬롯 0 계약을 추가해 Material-only·mesh+Material Instance·잘못된 참조의 변경 전 거부·저장·cold 조회를 검증했습니다. 기존 인자 순서와 mesh 기본값을 유지하며, material-only는 static_mesh=""를 명시합니다. 다중 슬롯·임의 클래스·상속 수정·모든 원본 오류 동작의 동등성은 미완료입니다.

기존 O의 큐브 저작→액터 생성·맵 저장과 배치 결과를 보존합니다. Q의 named set_physics_properties는 StaticMesh·Sphere·Box·Capsule template의 저장 물리 기본값만 설정합니다. 실제 시뮬레이션·충돌·렌더링 검증과 구분합니다. 최신 전체 BuildPlugin 배포본은 N이며 Z3는 증분 DLL과 Python 코드를 사용한 검증 상태입니다. Z3 전체 배포·패키징은 반복하지 않았습니다. Z3 배포 133파일의 일치와 실행 source snapshot을 보존했으며 이후 AA 개발 중인 root 소스와 구분합니다. 엔진 자체 시작 smoke 3개 테스트의 15개 실패는 [별도 조사 기록](cubeless/records/migration/implementation-2026-10-10/engine-startup-smoke-R.md)에 남겨 두었으며 전체 에디터 무오류를 주장하지 않습니다.

Blueprint 생성은 에디터의 **Save on Compile = Never** 설정을 요구합니다. 엔진 factory의 암묵 저장을 피하기 위한 조건이며 도구가 사용자 설정을 자동 변경하지 않습니다. 컴파일·저장은 별도 요청으로 제어하고, 실패 결과의 변경·컴파일·저장 상태를 확인합니다. 불확실한 수정 호출은 자동 재시도하지 않습니다.

유지보수는 빌드·등록·대표 흐름·중요한 실패 처리를 우선합니다. 공통 코드의 동작 검증은 재사용하고 런타임 패키징은 주요 통합 시점에 묶습니다. 기능 및 파이프라인 전수 추적과 미검증 표시는 유지합니다.

## 진행 기록

[현재 구현·미완료 항목](cubeless/records/migration/implementation-2026-10-10/IMPLEMENTATION_STATUS.md) · [이식 계획](cubeless/records/migration/UE58_MIGRATION_PLAN.md) · [작업 기록](cubeless/work-log.md)

GitHub 대상: https://github.com/cubeless2-lgtm/CubelessMCP_UE58

이 저장소는 진행 중인 소스와 기록을 보존합니다. Git 커밋이나 푸시를 전체 이식 완료의 증거로 취급하지 않습니다.
