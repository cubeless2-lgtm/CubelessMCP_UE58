# 현재 저장 에셋의 MCP 없는 Shipping 검증 D

UE 5.8.3의 지정된 저장 에셋 묶음에 대한 검증이다. 전체 이식 또는 화면 품질 완료를 뜻하지 않는다.

- B 생성과 C 새 프로세스: 원본 19개·복사 19개·새 4개 파일 해시, Material/Custom/MPC 속성과 연결, 맵의 8개 큐브와 Blueprint 8개 컴파일을 독립 확인했다.
- B 직후 새 clone의 레지스트리 연결 누락은 단독 실패로 보존했다. C의 새 프로세스 디스크 조회로 같은 저장 파일의 연결을 확인해 결합했다.
- D Win64 Shipping BuildCookRun 11.1초. 로컬 캐시를 사용하는 단일 측정이며 비교 성능 수치가 아니다.
- Pak 항목 1,818개에서 필수 22개 패키지를 정확히 확인했다. 이전 맵은 없고 MCP/Python/Toolset 관련 staged·receipt·Pak 위반은 없었다.
- D 실행 PID 50164, 정상 종료, 1.729초. 새 UserDir에 생성된 SaveGame 헤더·클래스·시간·실행 파일 해시를 다시 검수했다.
- Cook 로그의 일반 Warning 관측 8행, shader/material 진단 0행이다. 개별 에셋 로그 누락은 incomplete로 유지하고 포함 여부는 Pak에서 확인했다.

C는 빌드 성공 후 IoStore 내용 검사 미지원으로 검사 미완료였다. C 기록을 보존하고 DefaultGame.ini에 Pak-only와 대상 맵을 명시한 별도 D를 검증했다. B/C의 기존 descriptor·DefaultEngine 해시는 그대로다.

Custom HLSL의 BaseColor 연결 및 MPC 사용 연결은 저장·재로드·Cook 대상으로 포함됐다. shader 완료 시점, 실제 화면 품질, 모든 Blueprint 경로, 새 Actor 도구 및 나머지 legacy 기능은 검증하지 않았다. 원본 그래프와 공개 API로 만든 연결 clone은 구분한다.

AssetRegistry 조회 해석: 설치 소스 `C:\Program Files\Epic Games\UE_5.8\Engine\Source\Runtime\AssetRegistry\Private\AssetRegistry.cpp` 10355–10373행, SHA256 `0200285704dd44d2a43e4143d9cb78c5918c9a8f5381bc8dc946aca1534aeafe`. Game/EditorOnly selector가 둘 다 false여서 Game 속성 필터가 없으므로 editor-only 참조도 포함한다. 과거 B의 반대 해석은 `operations-material-runtime-B-query-scope-correction-D.json/md`로 정정했으며 원본 기록은 보존했다.
