# 플러그인 설치 상태의 Game 대상 제외 검증

2026-10-10. `UE58InstalledPluginProof`에 **Distribution-20261010-B** 플러그인 33개 파일과 앞선 테스트 에셋 네 개를 복사했다. `.uproject`에는 Cubeless를 Enabled=true/TargetAllowList=Editor로 지정했다. 파일별 SHA와 descriptor는 `installed-plugin-project-preparation-A.json`에 보존했다. 이 snapshot은 Material 추가 이전이며, 이후 코드 전체가 동일하게 검증됐다고 해석하지 않는다.

- `build-installed-plugin-shipping-A.log`: UAT가 Editor Development와 Game Shipping target을 함께 빌드하고 cook/stage/archive를 마쳤다. Cubeless C++는 Editor 대상에서만 컴파일됐다. Game 대상에는 UAT가 생성한 Core-only 기본 module stub만 추가됐으며 사용자 프로젝트 Source 변경은 없다. BuildCookRun 76.70초, 전체 UAT 약 77초. 앞선 실행으로 DDC가 준비된 단일 표본이다.
- `installed-plugin-package-static-A.json`: 실제 UnrealPak으로 pak 1개/entry 1,782개 전체를 대조했다. stage 파일과 Shipping Game receipt 감사에서 Cubeless/공식 MCP/Toolset 참조 위반 0개, 누락된 컨테이너 0개였다.
- `installed-plugin-marker-baseline-A.json`, `installed-plugin-runtime-launch-A.json`, `installed-plugin-marker-verification-A.json`: 새 UserDir에서 marker 부재를 확인하고 실제 Shipping 실행 파일을 실행했다. PID 34584, 약 1.718초, 종료 0. 정확한 fixture SaveGame 클래스와 UE 5.8.3 헤더를 가진 새 파일이 실행 시간 내 생성됐다.

따라서 이 배포 snapshot과 네 fixture에 대해서는 **설치·에디터 활성화 상태에서도 Game 패키지에서 제외되어 실행할 수 있음**을 확인했다. 이 fixture는 실행 화면이나 모든 Blueprint 분기의 의미를 검증하지 않는다. SaveGame marker는 RuntimeProof Actor의 BeginPlay 증거이며 author-D Branch 로직의 실행 결과는 아니다. 정적 감사의 serialized/dynamic 의존성 범위는 `runtime-proof-review-B.md`와 동일하다.

기본 엔진 플러그인을 모두 끈 테스트 설정 때문에 ACL 기본 애니메이션 압축 설정을 찾지 못한다는 경고가 있었고, pak 전용 설정에 따라 ZenStore를 사용하지 않는다는 경고가 있었다. 이번 fixture에는 애니메이션 데이터가 없으며 경고를 전체 애니메이션 기능 통과로 해석하지 않는다. 프로젝트 및 에셋 출처는 같은 폴더의 준비·작성 보고서에서 추적한다.
