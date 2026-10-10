# Material/MPC의 MCP 없는 저장·재로드·Shipping 증거

2026-10-10, 설치 UE 5.8.3 CL58210709, Win64 Shipping, PCD3D_SM5. `material-runtime-proof-B.json`은 각 원본 보고서/로그의 SHA256과 패키지 정확한 항목을 연결한다. 전체 이식 완료 증거는 아니다.

- `UE58MaterialRuntimeProof`에는 Cubeless/공식 MCP/ToolsetRegistry 플러그인을 설치하지 않았다. 기본 플러그인은 비활성화하고 검증용 Python/EditorScriptingUtilities만 Editor 대상으로 켰다. 대상 게임의 기존 Content/사용자 에셋과 원본 UE56 파일은 변경하지 않았다. 대상 `.uproject`의 앞선 승인된 Editor 플러그인 활성화 변경은 별도 기록돼 있다.
- Material E3와 MPC, 기존 Blueprint/맵 4개를 복사했다. 원본 6개와 복사 6개를 보존하면서 새로운 Material clone 및 새 맵만 저장했다. 원래 E3의 MPC expression은 연결되지 않았으므로 clone에 공개 MaterialEditingLibrary로 MPC→Add.B 연결을 추가했다. 이 추가 연결은 Cubeless E3 자체의 기능 증거와 구분한다.
- `material-runtime-fixture-B.json`: 원본 E3 그래프·MPC 기본값 확인, clone의 BaseColor 연결, Engine StaticMeshActor/Cube 할당, 저장 및 의존성 감사 통과.
- `material-runtime-cold-validation-C.json`: 새 프로세스에서 동일 에셋/연결·할당·hard dependency를 확인했다. Blueprint는 메모리에서 컴파일했으며 저장하지 않았다. 입력 14개 파일(복사 8개, 원본 6개)의 해시는 그대로였다.
- `build-material-runtime-shipping-B.log`: build/cook/stage/package/archive 성공, BuildCookRun 38.61초. 캐시가 있는 로컬 단일 측정이며 이전 빌드와 작업량이 달라 성능 비교로 사용하지 않는다. UAT는 Paper2D가 꺼진 content project를 위해 격리 Intermediate에 Core 전용 임시 기본 모듈을 자동 생성했다. 사용자 게임플레이 C++는 작성하지 않았다.
- `material-package-static-B.json`: Game/Shipping receipt와 stage/pak 전체 1,786개 항목을 검사했으며 MCP 관련 위반 0개였다. 새 맵, runtime material, MPC의 정확한 항목은 결합 JSON에 기록했다. 이전 맵은 pak에 없다.
- `material-runtime-launch-B.json`, `material-runtime-marker-verification-B.json`: 새 UserDir에서 Shipping 실행 PID50756, 정상 종료, 약 1.733초. 게임 BeginPlay가 새 SaveGame 1,423바이트를 작성했으며 정확한 SaveGame 클래스와 UE5.8.3 헤더를 독립 검수했다. 실행기는 marker를 만들지 않는다.

실패·한계:

1. fixture-A는 Python에 없는 `get_static_mesh`를 호출해 cube readback 단계에서 실패했다. 저장된 clone은 보존했고 새 `_B` 폴더에서 별도 실행했다. 원본/복사 12개 해시는 유지됐다.
2. cold-B 검수는 SaveMap이 현재 메모리 World의 경로를 바꾸지 않는 점을 잘못 비교해 실패했다. 정확한 source/new map prefix와 동일 subobject suffix를 확인하도록 검수기를 교정해 새 C 읽기 실행을 통과했다. B 원본 결과는 그대로다.
3. cook log에는 MPC 이름이 출력되지 않아 `material-cook-log-audit-B.json`은 여전히 incomplete다. 이를 pass로 바꾸지 않았다. 실제 MPC의 pak 항목을 별도 확인해 포함 여부를 입증했다. cook 자체는 exit0이며 기록된 shader 오류/경고는 없었다.
4. RecompileMaterial의 빈 오류 목록, cook 로그, nullrhi 게임 실행만으로 shader 완료 시점이나 화면 품질을 주장하지 않는다. shader completion와 렌더링 품질은 별도 미검증이다.
5. 이번 증거는 Material9 E3 및 공식 API로 추가한 runtime 연결과 Blueprint marker 동작의 범위다. Add/Set, batch, MPC 상속, 나머지 legacy 기능, 새 전체 배포 플러그인 설치 상태의 회귀는 포함하지 않는다.
6. 의존성 보고서의 실제 query_options는 editor-only package references를 제외한다. 원본 limitations 문구의 포함 표현은 잘못됐으며 runtime chain과 pak 제외 범위로 해석한다. 독립 검토 `operations-material-runtime-proof-B-independent.json`은 11항목을 확인하고 이 불일치를 명시했다.
