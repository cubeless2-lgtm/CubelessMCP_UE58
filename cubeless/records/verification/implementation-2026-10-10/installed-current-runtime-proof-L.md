# Distribution L 설치 상태의 Shipping 독립성 검증

Cubeless 플러그인을 Editor 대상으로 설치한 별도 프로젝트에서 D와 동일한 저장 에셋을 Cook·패키징·실행했다. 이 문서는 L의 지정된 에셋과 실행 범위이며 Actor M이나 전체 이식 완료를 뜻하지 않는다.

- 저장 에셋 23개와 설정의 원본/복사 SHA256 일치. 초기 플러그인 90개 파일 중 Source·Python 소스·descriptor 57개는 배포 L과 설치본 모두 보존됐다.
- UAT가 바꾼 Binaries·Intermediate·Python bytecode는 별도 기록했다(67개). 삭제된 초기 생성물 0개도 기록했다. 이 생성물을 소스 불변 검사와 혼동하지 않았다.
- Win64 Shipping BuildCookRun 97.6초. Editor 플러그인 빌드가 포함되므로 D와 시간 비교를 통한 성능 주장은 하지 않는다.
- Pak 1,818개 항목에서 정확한 필수 22개 패키지와 이전 맵 부재를 확인했다. Shipping receipt·stage·Pak에서 MCP/Toolset/Python 런타임 포함 위반은 없었다.
- Shipping PID 46260 정상 종료, 1.65794초. 새로운 UserDir의 1423바이트 SaveGame을 헤더·정확한 클래스·mtime·실행 파일 SHA로 재검수했다. Game 실행 파일은 D와 SHA256이 같다.

ShaderCompiler의 정확한 Display 수준 old-autogen 삭제 관측 2행은 같은 경로의 후속 written 로그 및 ShaderGenerationUtil.cpp 소스 근거로 별도 기록했다. 삭제 실패 원인은 추정하지 않으며 Warning/Error·save/rename/compile 실패와 구분한다.

실제 화면 품질·shader 완료·모든 Blueprint 경로·모든 MCP 호출은 검증하지 않았다. nullrhi 실행의 범위와 D에서 보존한 B 레지스트리 누락/C IoStore 검사 미완료 기록을 그대로 유지한다.
