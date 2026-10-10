# R: StaticMesh의 숨은 material 슬롯 0 계약

R2 증분 빌드·공식 등록·대표 파이프라인·실제 Codex 새 프로세스 조회가 통과했다. R1의 TObjectPtr 조건식 컴파일 실패(11.68초)를 교정한 R2 빌드(7.27초)를 배포했다. 전체 legacy 동등성은 미완료이며 기존 O/P/Q 실행 보고서와 당시 소스 SHA는 그대로 보존한다.

원본 공개 `Python/tools/blueprint_tools.py:219`의 `set_static_mesh_properties`는 blueprint_name/component_name/static_mesh=Engine Cube만 노출한다(SHA256 `a3905dea7c882660f6d40d75150f1abd72fe4e0bf68184e38890fb7515590f43`). 활성 `UnrealMCPBridge.cpp:861`은 BlueprintCommands로 라우팅하고, `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPBlueprintCommands.cpp:3053`의 handler는 숨은 material 인자를 읽어 UMaterialInterface 로드 성공 시 `SetMaterial(0)`을 호출한다(파일 SHA256 `fa72cb0d37cfbc2377f6cbf5926f82352b5e07ea570aced4125eec732b32446f`). 각 optional asset 로드 실패를 조용히 건너뛰고 Blueprint를 modified로 표시하며 `{component}`를 반환한다. 명시 compile/save는 없다. R은 잘못된 참조를 변경 전에 실패시키므로 이 silent-success 결함과 동등하다고 표현하지 않는다.

`Python/scripts`의 exact 도구 이름 참조 9파일/13곳을 좁게 조사했다. 아래 파일의 source SHA는 기존 ledger 기록과 일치했다. 실제 호출·planner 생성 경로는 static_mesh만 보내며 material 인자를 사용하는 호출은 이 조사 범위에서 발견하지 못했다. 동적 생성 호출까지 없다는 증거는 아니며, 이들을 R 전체 파이프라인 이전 완료로 처리하지 않는다.

| 원본 Python/scripts 상대 경로 | 확인한 역할 |
| --- | --- |
| blueprints/test_create_and_spawn_blueprints_with_different_components.py:178 | 메시 전용 요청 helper |
| blueprints/test_create_and_spawn_cube_blueprint.py:136 | Cube 메시 요청 |
| node/test_create_bird_blueprint_with_input_and_camera.py:203 | Sphere 메시 요청; 나머지 변수·입력 그래프 별도 |
| analysis/bp_authoring_job_contract.py:1246 | allowlist 및 메시 전용 생성 step |
| analysis/bp_authoring_quality_gate.py:53 | 선택 gate 목록 |
| analysis/build_cube_fluid_ninja_moving_ball1.py:1658,2110 | native route/metadata 표시; 인접 material 변수는 별도 검사 결과 |
| analysis/mcp_rpc_phase1_practical_canaries.py:3100 | Cube 메시 요청 |
| analysis/planner_driven_bp_authoring_smoke.py:410 | component_name/static_mesh만 매핑 |
| analysis/test_bp_authoring_job_contract.py:294 | 메시 생성 계약 assertion |

새 공개 계약은 기존 `set_static_mesh_properties(blueprint_name, component_name, static_mesh=Cube, operation_id=None)` 뒤에 `material=None`을 추가한다. 기존 4개 positional 인자와 mesh-only journal 정규화/재실행 ID를 유지한다. material 생략/null은 미변경이고 빈 문자열은 오류다. material-only는 **static_mesh="" + 유효 material 경로**로 표현하며 mesh 생략은 기존 Cube 기본값이다. mesh와 material을 함께 보낼 때 두 참조·클래스·저장 의존성을 먼저 검수하고 수정한다. 로컬 exact StaticMeshComponent와 runtime Material/MaterialInstanceConstant의 슬롯 0만 대상으로 하며 compile/save를 자동 요청하지 않는다. raw OverrideMaterials, slot 0 override와 editor/effective material 관측을 구분하고 렌더러·Nanite 결과의 동일성을 가정하지 않는다.

`component_batch_smoke.py --material-overrides`는 새 격리 UUID BP에 Plane 컴포넌트를 compile=false로 추가한다. material-only로 BasicShapeMaterial을 지정하고 공식 ObjectTools에서 Plane 유지·OverrideMaterials 슬롯 0을 확인한다. 이후 Cube+BasicShapeMaterial_Inst와 compile/save를 2-child batch로 묶고 다시 공식 조회한다. 유효 Plane+없는 material을 함께 보내 `component_material_reference_invalid/preflight`로 거부되며 Cube/MIC·dirty·저장 파일이 그대로인지 대표 반례 하나를 확인한다. 명시 compile/save는 각 1회이며 자동 재시도·새 검수 프레임워크·패키징을 추가하지 않는다. cold 조회는 root의 실제 공식 연결 실행에서 별도로 확인한다.

fixture는 root의 공식 AssetTools 클래스 조회 및 저장 의존성 조사에 따라 `/Engine/BasicShapes/Plane.Plane`, Cube, BasicShapeMaterial(UMaterial), BasicShapeMaterial_Inst(UMaterialInstanceConstant)를 선택했다. Sphere/Cone/Cylinder의 기존 저장 참조는 DefaultMaterial을 통해 `/Script/UnrealEd`를 포함하여 현 runtime 의존성 guard를 통과하지 못한다. 이를 해결하려고 Editor 모듈 allowlist를 넓히거나 엔진 에셋을 수정하지 않는다. 이 guard 범위와 실제 물리·렌더링·shader 완료 미검증은 계속 제한으로 남긴다.

엔진 PostEdit가 줄어든 mesh 슬롯 수에 따라 override·overlay 배열의 꼬리를 삭제하는 경로를 소스 리뷰에서 발견했다. 새 material 경로와 기존 mesh-only typed 경로가 공유하는 guard는 최종 슬롯 수가 보존할 override보다 작거나 mesh 변경이 overlay 꼬리를 지우는 경우 component_material_slots_unsupported/preflight로 거부한다. 새 경로는 raw 배열·slot0·overlay 전후를 관측한다. 다중 슬롯·다단계 상속의 실제 실행 조합까지 검증한 것은 아니다.

`material-overrides-R.json`은 16 named calls/24 HTTP/0.8713781초에 통과했다. Plane 유지+기본 Material raw override 확인 후 Cube+MIC를 명시 compile/save 각1회로 저장했다. 2-child batch 호출은0.1552715초(내부0.116912초)다. 유효 Plane+없는 material 요청은 component_material_reference_invalid/preflight로 변경 없이 거부됐으며 Cube/MIC와 dirty·디스크가 그대로였다. 실행 소스 SHA는 `3010329c1ee950278176313d8479fd39125ecef0021657db713d2e416084c1cb`다. warm NullRHI/loopback 단일 표본이므로 속도 개선율이 아니다.

`actual-codex-material-R.json`의 같은 값을 새 operation ID로 보낸 요청은 no_op=true/changes=[]/native mutation=false다. 이는 journal replay가 아니다. generic StaticMesh=null 요청은 현재 슬롯0 override 손실 가능성 때문에 변경 전 거부됐다. `actual-codex-cold-R.json`은 작성 PID29172와 다른 PID37632에서 계층·GUID·transform, 공식 StaticMesh=Cube/OverrideMaterials=[MIC]/MaterialSlotsOverlayMaterial=[]의 동일성을 확인했다. 의존성 감사는 BP1root/재귀8패키지, violations/incomplete_reasons/dirty_scanned_packages=[]이며 packaging_verified=false다. 검수기의 최초 dirty 필드명 오타는 같은 읽기 응답으로 교정했고 변이를 다시 실행하지 않았다.

`registered-schemas-and-diagnostics-R.json`의 8 Toolset/42개 도구와 DLL SHA `bd16a55615e7b2df661acc77ce36f126862d487b12c3de2d398a6558daee75cf`, `source-and-material-persistence-R.json`의 plugin79파일 root/isolated 일치 및 에셋 SHA/mtime 불변, `target-preservation-after-R.json`의 보호1,059파일 중 승인된 uproject 외 변경0을 확인했다. 공통 Python57개와 Component32개 테스트 통과는 실행 stdout 관측이며 별도 저장된 테스트 보고서가 아니다. R 전체 BuildPlugin·MCP 없는 패키징·shader 완료·렌더링은 수행하지 않았다.

R의 상세 시작 로그에서 엔진 smoke 실패15개를 3개 테스트로 식별했다. [engine-startup-smoke-R.md](engine-startup-smoke-R.md)는 설치 영어 기대값·한국어 번역 대조와 원인 추정의 한계를 기록한다. 엔진·언어·테스트를 수정하지 않았고 에디터 전체 무오류를 주장하지 않는다. 오래된 MCP session은 재초기화 뒤 정상 조회됐으며 수정 요청을 자동 재시도하지 않았다.
