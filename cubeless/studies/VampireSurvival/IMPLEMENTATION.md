# Crimson Dawn 구현 및 검증 기록

## 실행

에디터 맵: `/Game/VampireSurvival/Maps/L_CrimsonDawn`. 이 맵을 열고 Play를 누른다. 기존 프로젝트 기본 맵·게임 설정은 변경하지 않았다.

독립 실행 파일: 프로젝트 아래 `cubeless/records/verification/vampire-survival/Packaged/Windows/CrimsonVerify58.exe`. `Windows` 폴더 전체를 함께 유지해야 한다. 이 파일은 Content만 복사한 별도의 순수 Blueprint 검증 프로젝트에서 생성했다.

| 입력 | 동작 |
| --- | --- |
| Enter / Space / 버튼 클릭 | 타이틀 진행, 선택 확인 |
| 1 / 2 | 궁수 / 검사 선택 |
| WASD / 방향키 | 이동, 대각 속도 정규화 |
| 자동 | 가까운 적 조준, 궁수 화살 / 검사 부채꼴 베기 |
| 1 / 2 / 3 / 카드 클릭 | 성장 선택 |
| P | 전투 일시정지 / 재개 |
| R / Enter | 결과에서 같은 클래스로 재도전 |
| Backspace | 메뉴·일시정지·결과에서 타이틀 복귀 |

Esc는 기본 에디터 PIE 종료를 유지한다. 180초에 살아 있으면 승리하며 보스 처치는 별도 특별 성과다.

## 에셋과 구조

- `Content/VampireSurvival/Maps/L_CrimsonDawn.umap`: 독립 맵과 전용 GameMode 지정.
- `Blueprints/BP_CrimsonGameMode`, `BP_CrimsonController`, `BP_CrimsonHUD`: 표준 Engine 부모만 사용한다. HUD가 1280×720 가상 화면의 Canvas 렌더링, 상태 전환, 전투 배열과 성장을 관리한다. 고정 경기장 형태의 2D 게임이다.
- `Art`: ImageGen 이미지 4종의 최근접 필터·UI 압축·mipmap 없음 설정.
- `SourceArt`: 타이틀·묘지 전장·영웅 아틀라스·적 아틀라스 PNG 원본. 영웅과 적은 실제 alpha 채널을 보존했다.
- `Audio`, `SourceAudio`: 독자 합성 WAV 6종과 SoundWave 에셋. 배경음은 Engine AudioComponent로 재생한다.

총 14개 Unreal 에셋(맵 포함). 게임 동작에 프로젝트 C++·MCP 클래스·MCP 함수·에디터 Python을 사용하지 않는다. 이번 작업에서 MCP 플러그인 C++을 새로 확장할 필요는 없었으며 기존 Tivret MCP와 엔진의 Blueprint 편집 API를 사용했다. 외부 제작·검증 스크립트는 `cubeless/tools/vampire_survival`에 있으며 게임에 포함되지 않는다.

`BP_CrimsonHUD`의 25개 그래프는 컴파일·저장되었다. 개체 데이터는 표준 Vector4 배열로 보관하며 일반 적 89 + 보스 1, 보석 140, 화살 60, 시각 효과 100의 상한을 둔다. 테스트용 `TestBot` 기본값은 false이며 최종 두 클래스 입력 테스트에서는 이 내부 봇을 사용하지 않았다.

## 실제 검증

검증은 같은 작업자가 수행한 자체 검증이다. 독립 QA로 부르지 않는다.

| 항목 | 결과와 근거 |
| --- | --- |
| 웹 참고 및 기획 자체 리뷰 | 기획서에 출처·캐릭터·적·180초 전개·보강 사항 기록 |
| 이미지 원본·alpha·UV | 타이틀·선택·전투 화면에서 확인, 원본 PNG 보존 |
| Blueprint 컴파일·저장 | `cubeless/records/verification/vampire-survival/compile.json` |
| 궁수 최종 일반 속도 플레이 | `archer_final_result.json`: 180초 승리, 272 처치, 레벨 8, 체력 32, 보스 처치 |
| 검사 일반 속도 플레이 | `swordsman_final_result.json`: 180초 승리, 266 처치, 레벨 10, 체력 130, 보스 미처치 생존 엔딩 |
| 입력 경로 | 외부 드라이버가 기존 PIE 입력 도구로 실제 pressed/released를 보내며 이동·메뉴·성장 선택. 체력·피해·경험치·시간 덮어쓰기 없음 |
| 일시정지 | `pause_verified.json`: 약 19초 동안 전투 시간·체력 불변 |
| 성장 선택 정지 | `boundaries_verified.json`: 약 28초 동안 시간·체력 불변. 이 경계 시험만 경험치를 강제로 주입했으며 일반 플레이 증거와 분리 |
| 개체 상한·보스 예약·보석 위치 | 일반 적 100회 생성 요청에 89개 유지, 보스 추가로 총 90개, 보석 경기장 내부 보정 확인 |
| 사망·재도전 | 자연 사망 114초 사례 및 별도 강제 체력 0 경계 시험, 재도전 배열 초기화·타이틀 복귀 확인 |
| 에셋·그래프 의존성 | `asset_dependencies.json`: 14개 에셋·25개 그래프, 금지된 MCP 참조 없음, TestBot CDO false |
| 별도 Win64 Cook/Stage/Package | MCP/Toolset/Python 비활성 검증 프로젝트에서 성공, `package-release.log`에 마지막 결과 보존 |
| 독립 실행 | 별도 UnrealGame 프로세스의 렌더링·게임 로그 및 PNG 확인. 상세 결과는 최종 `delivery_report.json` 참조 |

독립 패키지의 기능 시험은 엔진 Development 콘솔 `ke BP_CrimsonHUD_C StartRun` 및 `SpawnEnemy` 호출과 HighResShot을 사용했다. MCP API를 호출하지 않는다. 이 강제 시작·생성 smoke test를 일반 180초 입력 플레이나 실제 OS 키보드 시험으로 부르지 않는다. 엔진 공용 스테이징에 PythonScriptPlugin의 descriptor/content mount가 남았지만 게임 에셋은 Python을 참조하지 않으며, Python 편집기 모듈·MCP 플러그인 코드가 게임 제작물의 실행 경로가 되는 구조는 아니다. 로그의 콘텐츠 mount와 게임의 런타임 의존성을 구분한다.

두 클래스의 180초 입력 플레이 이후 변경한 것은 일반 적 상한의 보스 예약·타이틀 버튼 hit box·반복 배경음의 AudioComponent 소유 방식이다. 해당 변경은 경계 시험 또는 별도 패키지 실행으로 검증하며, 같은 180초 플레이를 불필요하게 반복하지 않는다.

## 제작 중 실패와 수정

1. UE5.8의 PromotableOperator 노드가 연결된 숫자 타입을 따라가므로 숫자 리터럴과 정수→실수 변환을 명시했다.
2. Blueprint 이름은 대소문자를 구분하지 않아 지역 변수 `hp`와 멤버 `HP`가 충돌했다. 제작기에서 지역 변수 이름에 `L_`를 붙였다.
3. FKey는 구조체 문자열 `(KeyName=...)`가 아닌 `Enter`, `W` 등의 키 이름으로 직렬화해야 했다. 실제 입력을 검증하고 교정했다.
4. 텍스처가 존재하기 전에 그래프에 지정한 오브젝트 리터럴은 참조가 비어 있었다. 이미지를 임포트한 뒤 참조를 다시 작성하고 실제 표시를 확인했다.
5. 한 번의 에디터 크래시가 발생했다. 스택은 엔진 BlueprintEditorLibrary를 가리키며 그래프 재생성 중 발생했지만 정확한 원인은 확정하지 않았다. 저장 에셋을 다시 로드하고 이후 수정은 노드 삭제 없이 핀·링크를 좁게 변경했다. 크래시 원본은 `Saved/Crashes/UECC-Windows-02174C8E489BA5CDF9B3A9BFB635957D_0000`에 남겼다.
6. 작업 시작 때 다른 작업의 `BP_StudyCoordinator`가 dirty였다. 해당 에셋을 편집·저장하지 않았고 Git상 관련 변경도 만들지 않았다. 크래시 당시 미저장 메모리 상태의 복구 여부는 확인하지 못했다. 디스크·기존 autosave 복사본을 이번 검증 폴더에 보존했다.
7. 독립 실행 로그에서 반복 배경음의 fire-and-forget 경고를 확인하여 SpawnSound2D로 교체했다.
8. 별도 패키지에서 콘솔 `setall ... Hero 1`을 사용한 검사 선택 시험은 이미지에 Lyra가 표시되어 선택 성공으로 인정하지 않았다. 패키지의 검사 전용 동작은 미검증으로 남기며, 두 클래스의 일반 속도 180초 검증은 앞서 기록한 에디터 PIE 입력 시험에 해당한다.

## 검증의 범위

Windows가 잠긴 상태에서는 OS 데스크톱 입력을 조작하지 않았다. 에디터의 입력 검증은 OS 입력 대신 기존 Editor 전용 PIE 입력 API를 통해 수행했다. 독립 패키지에 대한 실제 마우스 조작은 잠금 해제 전까지 미검증이며, 패키지의 콘솔 기반 기능 시험과 구분한다. 다른 해상도·장치에 대한 전수 성능 측정, 컨트롤러 지원, 독립 QA는 수행하지 않았다. 커밋·푸시는 하지 않았다.
