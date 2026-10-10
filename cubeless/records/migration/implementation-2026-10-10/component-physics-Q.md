# Q: 저장된 물리 기본값 계약

Q2 증분 빌드(7.29초), 공식 8 Toolset/42개 도구 등록, 대표 작성·오류 거부·저장·실제 Codex 새 프로세스 조회가 통과했다. Q1은 리뷰 교정 전 빌드로 배포하지 않았다. 전체 legacy 동등성은 미완료다.

원본 공개 `Python/tools/blueprint_tools.py:587`의 `set_physics_properties` 기본값은 simulate=true, gravity=true, mass=1, linear=.01, angular=0이다. Q는 공개 이름과 기본값을 유지하고 기존 공통 journal/action core를 단독·named batch에서 사용한다. 원본 `UnrealMCPBlueprintCommands.cpp:2792–2878`가 gravity_enabled를 읽지 않던 결함은 명시적으로 교정한다. mass는 공개 편집 속성의 최소값에 따라 [0.001,1e12], damping은 [0,1e12]의 유한 수로 제한하며 bool/string/null을 숫자로 바꾸지 않는다. 기존의 느슨한 변환과 동등하다고 주장하지 않는다. 반환의 component 이름과 실제 before/after/실패 상태를 보존하며 명시 compile/save는 요청하지 않는다.

설치 UE58 `Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h`의 공개 ENGINE_API를 사용한다: SetSimulatePhysics(:1661), SetEnableGravity(:2801), SetMassOverrideInKg(:2857), SetLinearDamping(:2825), SetAngularDamping(:2833). 대상은 로컬 SCS의 exact StaticMesh/Sphere/Box/Capsule template이고 미등록·활성 physics state 없음·비용접 조건을 요구한다. Scene/Camera/SpringArm, 상속 template 자체 수정, 외부 인스턴스 전파는 미지원이다. 저장 bSimulatePhysics와 physicsBlendWeight는 실행 중 IsSimulatingPhysics의 증거가 아니다.

Q2는 로컬 archetype 전파 시 leaf 값만 원본과 비교하던 후보 코드를 교정했다. A=1→B=2→C=1처럼 중간 override가 있으면 C가 우연히 A와 같아도 덮어쓰면 안 된다. GetArchetype 전체 경로를 최대 128개·정확 타입·같은 package·cycle 검사로 추적하고 중간 override를 보존한다. 이 교정은 소스 리뷰 결과이며 모든 archetype 조합의 실행 검증 완료를 뜻하지 않는다.

원본 `Python/scripts/node/test_physics_variables.py:164`는 simulate/gravity만 raw command로 요청한 뒤 변수·이벤트 그래프 작업을 계속한다. `test_create_bird_blueprint_with_input_and_camera.py:170`은 true/true/mass2/damping.5/.5 뒤 변수·입력 그래프를 작성한다. 이번 `component_batch_smoke.py --physics-defaults`는 이 물리 설정 단계의 대표 경로만 추출한다. 나머지 원본 파이프라인을 이전했다고 처리하지 않는다.

대표 경로는 새 UUID BP에 Sphere(radius30)와 Engine Cube StaticMesh를 compile=false로 추가하고, Sphere 물리 인자는 모두 생략, Mesh는 false/false/7.5/.2/.3을 지정한 뒤 명시 compile/save 각 1회로 묶는 5-child batch다. 공식 ObjectTools BodyInstance의 bSimulatePhysics/bOverrideMass/bEnableGravity/massInKgOverride/linearDamping/angularDamping/physicsBlendWeight를 독립 비교한다. mass=-1의 Python preflight 거부와 값·dirty·저장 파일 불변만 대표 반례로 확인한다. 새 cold framework·패키징·물리 시뮬레이션은 추가하지 않는다.

원본 공개 `set_collision_profile` 도구/명령은 확인되지 않았고 일부 스크립트의 공개 SetCollisionProfileName 사용은 후속 API 후보다. 이를 기존 공개 도구 복구로 계수하지 않는다. 이전 named mesh의 숨겨진 C++ material 분기는 계속 미완료다.

알려진 엔진 시작 제한: P/Q/Q cold 로그에 동일한 `LogAutomationTest: Error: Condition failed` 15개가 있다. `LowLevelTestAdapter.h:130`의 error 추가와 `LaunchEngineLoop.cpp:4374–4376` RunSmokeTests, `AutomationTest.cpp:557–591,1243–1265` 출력 경로에 근거한 실제 smoke CHECK 실패이며 의도된 오류가 아니다. Q에서는 12:53:08.372에 발생해 Cubeless DLL 로드 12:53:09.099보다 앞섰다. 구체 실패 테스트/조건은 미확인이고 인접 UnifiedErrorTest의 의도된 샘플 로그는 이 15개를 설명하지 않는다. 엔진을 수정하거나 테스트를 비활성화하지 않았다. 플러그인의 기능별 통과를 전체 에디터 로그 무오류로 확대하지 않는다. 재시작 후 첫 MCP 읽기의 오래된 session ID는 connector 초기화 후 정상 조회됐으며 수정 요청을 자동 재시도하지 않았다.


실행 근거는 `physics-defaults-Q.json`(13 calls/21 HTTP/0.9860521초, 핵심5-child batch0.2482488초/내부0.202811초, compile/save 각1회)이다. 공식 BodyInstance readback으로 Sphere true/true/mass1/linear.01/angular0/blend1, Mesh false/false/mass7.5/linear.2/angular.3/blend0와 bOverrideMass=true를 확인했다. float32 저장 반올림은 허용 오차로 비교한다. mass=-1은 component_physics_value_invalid/preflight로 변경·컴파일·저장 없이 거부됐으며 BodyInstance·파일·dirty는 그대로였다. `actual-codex-physics-noop-Q.json`은 같은 값을 새 ID로 호출했을 때 no_op=true/changes=[]/native mutation=false인 실제 요청이며 journal replay가 아니다.

`actual-codex-cold-Q.json`은 author PID22420과 다른 PID43660에서 컴포넌트 GUID·클래스·부모·transform 및 공식 BodyInstance 7필드의 저장 값을 확인했다. `source-and-physics-persistence-Q.json`은 등록 plugin79파일의 root/isolated 일치와 PhysicsDefaults BP SHA/mtime 불변을, `target-preservation-after-Q.json`은 보호1,059파일 중 승인된 uproject 외 차이0을 기록한다. cold 기록에 포함된 저장 의존성 조회는 1root/재귀6패키지, violations=[]/dirty=[]이며 packaging_verified=false다. 작은 warm NullRHI/loopback 단일 표본이고 시뮬레이션·충돌·렌더링·Q 패키징·다단계 상속 조합 실행 검증으로 확대하지 않는다. 이전 P/O 실행 보고서와 source SHA는 해당 시점의 증거로 보존한다.
