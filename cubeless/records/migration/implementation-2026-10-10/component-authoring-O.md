# O: 큐브 컴포넌트 저작 파이프라인 대응

추가 공개 API 검토(소스 근거이며 새 구현·실행 통과가 아님): UE 5.8.3 `Engine/Source/Runtime/Engine/Public/Materials/MaterialParameterCollectionInstance.h:39–40`의 exported setter는 월드 인스턴스 값을 변경하므로 에셋 기본값 저장과 동등하지 않다. 자체 선언 MPC 기본값은 `Public/Materials/MaterialParameterCollection.h:90–94`의 공개 editable 배열에서 이름/GUID를 유지하고 DefaultValue를 편집하는 후속 구현 경로가 있다. `Private/Materials/ParameterCollection.cpp:149–171`의 엔진 setter도 자체 선언값은 이 배열을 변경한다. 반면 상속값은 private override map을 사용하며, 헤더의 기본값 setter는 export/UFUNCTION가 없어 동등한 공개 쓰기 경로를 아직 확인하지 못했다. 상속 기본값은 미완료로 유지한다. 편집 알림의 파생 collection·월드 인스턴스 전파도 별도 영향 범위로 관측해야 한다.

O4 증분 빌드와 공식 8 Toolset/40개 도구 등록, 선택된 저작 흐름과 새 프로세스 재로드가 통과했다. 전체 legacy 계약 동등성은 미완료다. `component-author-O2.json`, `component-reload-O.json`, `component-batch-O.json`, `actual-codex-health-O.json`이 실제 실행 근거이며 모두 검증 기록 폴더에 있다.

원본은 `Python/scripts/blueprints/test_create_and_spawn_cube_blueprint.py`이며 SHA-256은 `aeac8725e0e4b7a15bf18da44185104c7649bf6048a6e124aa0780dac0f055b4`다. `create_blueprint(Actor)` → `add_component_to_blueprint(CubeVisual, StaticMeshComponent, identity transform)` → `set_static_mesh_properties(Engine Cube)` → `compile_blueprint` → `spawn_blueprint_actor(TestBPInstance, [0,0,100])` 순서이고, 각 응답의 status 실패 시 즉시 중단한다. 명시 저장·재로드·독립 속성 검수는 없으며 로그를 출력한다.

대상 `cubeless/tools/pipelines/component_authoring.py`는 공식 MCP 클라이언트와 개별 등록 도구를 사용한다. 새 UUID 테스트 경로만 허용하고 결과·실패 단계·이미 확인된 변경·컴파일/저장 상태·불확실한 응답을 기록한다. 자동 재시도와 원자적 롤백을 제공하지 않는다. author/reload 모드를 분리하고, root가 만든 새 Actor map fixture를 지정했을 때만 마지막 Blueprint 액터 생성·맵 저장을 실행한다. O2는 이 마지막 단계까지 포함해 통과했다.

의도된 변경은 기존 BP 재사용 대신 새 에셋 생성, `StaticMesh` 한 속성의 typed setter 대응, 명시 `DefaultSceneRoot` 부착, 중간 add 컴파일 억제 후 최종 컴파일·저장 1회다. 원본 `UnrealMCPBlueprintCommands.cpp:1306,1418,1422`는 부모 인자 생략 시 `SCS.AddNode` 후 바로 컴파일한다. 따라서 새 루트 보존 계층과 컴파일 횟수를 원본과 동일하다고 주장하지 않는다. 저장 후 readback과 대표 cold read는 추가된 검증 경계다. 나머지 mesh 속성, 기존 에셋 재사용, 일반 Component/Actor 동등성은 미완료다.

`spawn_blueprint_actor`의 활성 원본 경로는 `UnrealMCPBridge.cpp:815` → `UnrealMCPEditorCommands.cpp:6770`이다. 이 핸들러는 `SpawnParams.Name`에 actor_name을 넣고 scale을 포함한 transform을 전달한다. `UnrealMCPBlueprintCommands.cpp:2939`의 같은 이름 핸들러는 이 활성 경로의 근거로 사용하지 않는다. 새 도구는 기존 Cubeless Actor 공통 작업 코어를 재사용한다. 공식 SceneTools 생성 경로의 label 설정만으로 UObjectName 계약까지 동등 대체됐다고 판정하지 않는다. 이름이 겹치는 핸들러는 별도 고유 기능 수로 더하지 않는다.

공개 엔진 API를 우선한다. UE58의 AttachSocketName 전용 setter는 private이므로 접근을 우회하지 않고, 미등록 template의 전제조건을 확인한 public `USceneComponent::SetupAttachment`와 실제 readback을 사용한다(`CubelessComponentMutationApi.cpp:267–274`). 에디터 플러그인의 공개 API 경로로 구현하며 게임 모듈 의존성이나 엔진 소스 수정을 추가하지 않는다.

O2 author는 29 named calls/39 HTTP 관측/1.7321308초, BP 명시 compile/save 각 1회와 map save 1회다. factory의 암묵 컴파일은 별도 기록한다. PID 4768에서 작성하고 새 PID 50916의 reload 12 calls/22 HTTP/0.8364492초에서 두 컴포넌트의 GUID·클래스·부모·transform과 Cube 참조, 생성 액터의 UObjectName·클래스·transform, BP/map 파일 불변 및 dirty=[]를 확인했다. 기존 에셋 재사용 분기는 이식하지 않았으므로 `selected_legacy_flow_completed=true`, `full_legacy_pipeline_complete=false`다.

별도 `component_batch_smoke.py`는 이번 Component capability 검증이다. 단독 SceneComponent add의 기본 compile=true/save=false와 bVisible=false, bool 문자열 거부 후 불변, Camera/SpringArm 추가·visibility/socket 설정·마지막 compile/save의 5-child batch가 통과했다. 명시 compile은 스크립트 전체 2회, save는 1회다. 실제 Codex cold read로 4노드 계층·GUID·transform, visibility=true, socket=Grip과 파일 hash/mtime 불변을 확인했다. 전체 17 calls/25 HTTP/1.0003873초, 핵심 batch 1회 0.1730835초(내부 batch 0.145426초)다. 작은 warm fixture/NullRHI/loopback 단일 표본으로 속도 개선율을 주장하지 않는다.

초기 `component-author-O.json`은 mesh 검사에서 Engine Cube의 NavigationSystem 의존성을 허용하지 않아 set 단계 전에 거부됐다. 이미 생성한 BP/component는 `component-author-O-partial-preservation.json`의 별도 명시 저장으로 보존했다. 원래 요청을 재시도하지 않았다. 최초 보고서에서 ini 변경 검사기가 원 오류를 덮은 문제도 교정했다. 전체 ini 변화는 map-load bookkeeping 관측으로 남기고 중요한 SaveOnCompile=Never 값을 처음과 끝에 직접 검수한다. 최초 실패 기록은 유지한다.

새 파이프라인의 선택된 흐름 통과와 원본 전체 기능 동등성은 별개다. 지원 클래스 네 개, bounded typed 속성, 좁은 StaticMesh 참조만 검증했으며 임의 Component/Actor·상속 수정·나머지 mesh 옵션은 미완료다. `component-saved-dependencies-O.json`은 저장된 BP 2개와 map 1개의 재귀 registry 조회 8개 패키지에서 forbidden violations=[]를 확인했다. 이는 저장 registry 참조 층의 증거이며 `packaging_verified=false`다. O 전체 BuildPlugin과 MCP 없는 패키징은 `not_run`이다. N 전체 배포 빌드와 이전 runtime 결과는 이전 시점의 근거로 보존한다. 기존 실패·증거 파일과 원본은 수정하지 않는다.
