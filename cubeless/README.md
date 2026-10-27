# CubelessMCP UE 5.8.x 도구 묶음

공식 Unreal MCP에 붙는 Editor 전용 플러그인과 선택형 BP·Niagara·PCG 외부 실행기입니다. 소스 배포이며 DLL·PDB·가상환경은 포함하지 않습니다. 설치 PC에서 UE 5.8.x과 C++ 빌드 도구로 빌드해야 합니다. 외부 실행기 없이도 에디터 플러그인을 사용할 수 있습니다.

```text
프로젝트 루트/
  어떤이름.uproject
  Plugins/CubelessMCP_UE58/   Editor C++ / Content/Python
  cubeless/
    distribution.json       배포 식별과 포함 목록
    distribution-files.txt  명시적인 필수 제품 파일 목록
    README.md
    tools/                  실행기·MCP 클라이언트·의존성·작은 입력 예제
    codex/                  선별된 현재 운영 지침
    docs/                   설치·사용·운영 안내
    records/                실행 시 생성, 배포 제외
```

- [설치·빌드·경로 규칙·에이전트 연결](docs/installation.md)
- [실행·업데이트·제거·문제 해결](docs/operations.md)
- [BP·Niagara·PCG 입력과 현재 지원 범위](tools/pipelines/README.md)
- [공통 반복 작업 승격 정책](codex/policies/pipeline-promotion.md)

실행 파일 위치에서 이 묶음의 `cubeless`를 식별하고 바로 위 프로젝트를 선택합니다. 터미널의 현재 디렉터리나 개발 PC 경로는 사용하지 않습니다. `/Game/...`는 UE 에셋 경로이며 파일 경로와 구분합니다.

현재 기본 BP 제작·edit/보존·compile/save·PIE, Niagara CPU 제작·BP 바인딩·실제 compile·고정 캡처, PCG 입력 변경/빈 결과 정리/재생성 경로에는 기존 대표 성공 증거가 있습니다. 모든 고급 타입·GPU·영역 packing·전체 UE 5.6 계약의 완료를 뜻하지 않습니다. 새 배포본의 독립 설치 검증 상태와 범위는 `docs/operations.md`를 확인하십시오.

개발 checkout의 `records`, `work-log.md`, `codex/archive/ue56`, 테스트·검증 에셋과 캐시는 로컬 보존 자료이며 배포 묶음에 들어가지 않습니다. 배포 ZIP의 `cubeless/bundle-manifest.json`은 배포 파일별 SHA-256 목록입니다. 소스에서 재생성할 때는 다음을 사용합니다(기존 출력은 덮어쓰지 않음).

배포 생성기는 `distribution-files.txt`에 명시된 제품 파일만 포함합니다. 명시된 파일이 없으면 ZIP을 만들지 않으며, 목록 밖의 로컬 설정·추가 파일은 보존하고 배포에서 제외합니다. 새 제품 파일을 추가할 때는 의존 관계와 배포 필요성을 확인한 뒤 이 목록도 수정해야 합니다. 설치 프로젝트 전체나 Git 미추적 파일을 자동으로 포함하지 않습니다.

```powershell
python <프로젝트>/cubeless/tools/package_distribution.py --output cubeless/build/distribution/Cubeless-source.zip
```
