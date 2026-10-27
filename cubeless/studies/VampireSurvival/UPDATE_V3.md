# Crimson Dawn V3 UI·대기 모션·난이도 수정

2026-10-27. 기존 `/Game/VampireSurvival/Maps/L_CrimsonDawn`과 `BP_CrimsonHUDV2`에 적용했다. V2의 점프·착지·공격 예고는 유지하며 이 문서의 UI·피해 수치가 최신 기준이다.

## 버튼과 화면

- 타이틀, 생존 시작, 돌아가기, 일시정지, 계속하기, 재도전, 영웅 카드와 강화 카드를 같은 입력 방식으로 통일했다.
- 기본은 어두운색, 마우스 오버는 청록색, 누르고 있는 동안은 붉은 주황색, 선택된 영웅은 금색이다. 눌린 버튼의 글자는 살짝 내려간다.
- 누른 위치와 화면을 기록하고, 같은 버튼 안에서 뗐을 때 한 번 실행한다. 계속 누르기만 하면 실행하지 않는다. 바깥에서 놓거나 다른 곳에서 누른 채 끌어와 놓으면 취소한다. 화면이 바뀐 뒤 같은 입력이 다음 버튼으로 전달되지 않는다.
- 표준 Engine/UMG의 게임·UI 입력 모드와 `NoCapture`를 설정해 첫 클릭이 마우스 캡처에 소모되지 않도록 했다. 사용자 코드나 MCP 런타임 API를 추가하지 않았다.
- 타이틀 로고만 영어로 유지했다. 메뉴·선택·성장·HUD·결과는 한글로 바꾸고, 슬로건·장식용 설명·중복 조작 안내를 제거했다. 필요한 능력 수치와 결과는 남겼다.
- Noto Sans CJK KR 폰트를 프로젝트에 포함했다. 표준 HUD `GetTextSize`로 그리는 순간의 폭과 높이를 측정하고, 글자를 지정한 상자의 가로12px·세로6px 여백 안에 맞춰 가운데 정렬한다. 체력 숫자도 프레임 안쪽으로 옮겼다.

## 대기 애니메이션

선택 화면에서는 이동·공격 포즈를 사용하지 않는다. 샤미와 미니에 각각16프레임의 전용 대기 모션을 내장 ImageGen으로 생성했다. 초당8프레임,2초 반복이며 호흡·머리카락·꼬리·무기의 작은 움직임과 눈 깜빡임을 표현한다. 제자리에 선 포즈와 카메라 방향을 유지한다.

초기 아틀라스의 프레임 경계가 좁아 ImageGen으로 여백을 보강했다. 최종 이미지는 `Content/VampireSurvival/SourceArt/Shami_Idle16_Padded.png`, `Minie_Idle16_Padded.png`다. 실행 에셋은 `/Game/VampireSurvival/Art/T_ShamiIdle16`, `T_MinieIdle16`이다. 픽셀을 Python으로 편집하지 않았다. 읽기 전용 알파 검사에서 얻은 발 기준점을 Blueprint의 프레임별 위치 보정으로 적용해 행 전환 때의 위치 흔들림을 줄였다.

[초기 프롬프트](../../records/verification/vampire-survival-v3/imagegen_prompts.json), [최종 여백 수정 프롬프트와 파일 목록](../../records/verification/vampire-survival-v3/imagegen_final_revisions.json), `idle_anchors.json`에 제작 요청과 보정 자료를 보존했다. 이미지 API/CLI 대체 경로는 사용하지 않았다.

## 난이도

"30% 완화"는 **적에게 받는 공격 피해30% 감소**로 구현했다. 난이도 전체가 수학적으로30% 낮아졌다는 측정 주장은 아니다. 적 물량, 이동 속도, 체력, 공격 예고 시간, 판정 반경과 플레이어의 공격력은 유지한다.

| 공격 | 이전 피해 | 현재 피해 |
| --- | ---: | ---: |
| Thrall | 12 | 8.4 |
| Bat | 9 | 6.3 |
| Revenant | 22 | 15.4 |
| Stalker | 18 | 12.6 |
| Bellkeeper 근거리 | 28 | 19.6 |
| Bellkeeper 광역 | 28 | 19.6 |

## 검증과 제작 기록

PIE 버튼 입력 검사는 PlayerController의 실제 마우스 위치와 별도의 pressed/released 키 이벤트를 사용했다. 첫 클릭 진입, 누르는 동안 상태 유지, 떼었을 때1회 실행, 영웅 선택 유지, 드래그 취소, 강화 중복 방지, 일시정지·재개·재도전·타이틀 이동19개 관측을 확인했다. `ui_input.json`과 `ui_input_raw_calls.json`에 남겼다. 강화·결과 화면 검사는 명시적으로 상태를 설정한 통제된 검사이며 정상 플레이 승리를 의미하지 않는다.

734×412로 요청한 작은 PIE 창에서도8개 화면 조건을 확인했다. 실제 DrawHUD 중 수집한760개 글자 치수는 모두 지정한 여백 안에 들어갔다. `layout.json`, `small-*.png`에 결과가 있다. 일반 창의 선택·마우스 오버·누르는 중·선택 완료·강화·HUD·결과 화면도 캡처했다. 모든 해상도를 전수 검증한 것은 아니다.

`functional.json`의 통제된12개 판정 시험에서 적5종과 보스 광역의 피해 감소 및 반경 바로 밖의 무피해를 확인했다. 이번 UI·피해 수정에서180초 정상 플레이를 반복하지 않았으므로 V2의 플레이 결과와 구분한다. 자체 리뷰이며 독립 QA는 수행하지 않았다.

최종 Blueprint 컴파일은 UP_TO_DATE, 오류0, 그래프56개다. 에셋35개의 금지 MCP 의존성은0개이며 신규 UMG 참조는 표준 Engine 런타임 기능이다. `asset_dependencies.json`, `compile.json`, `isolation_manifest.json`에 기록했다. MCP·Python 비활성 콘텐츠 전용 검증 프로젝트에 해시로 동기화해 Win64 패키징을 수행했다. 패키징·실행 및 실제 Windows 클릭 결과는 같은 V3 기록 폴더의 `delivery_report.json`을 기준으로 한다.

제작 중 텍스처 경로를 문자열 조건식으로 선택한 저작 DSL의 타입 오류를 관측했다. 완료된 그래프를 보존하고 해당 대기 함수의 남은 분기를 작성했다. 텍스처 생성 전 작성되어 비어 있던 참조는 화면 검수 후 명시적으로 연결했다. 에디터 크래시는 없었다. Canvas 수명 밖에서 글자 크기를 읽어0이 반환된 첫 검증 시도는 보존하고, 실제 DrawHUD에서만 치수를 수집하도록 검사 방법을 수정했다. 기록 수집은 CDO에서 기본 꺼짐이며 일반 게임은 측정 배열을 누적하지 않는다. 창 크기 검증 후 에디터의1280×720 설정을 복원했다.

프로젝트·MCP·엔진 C++ 변경, 전역 기본 맵 변경, Git stage·commit·push는 없다. V1·V2 보고서와 기존 작업을 보존했다. 원시 기록은 `cubeless/records/verification/vampire-survival-v3/`다.

폰트 원본은 [Noto 공식 저장소](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/Korean/NotoSansCJKkr-Regular.otf)에서 받았으며 [SIL OFL1.1 라이선스](https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE)를 `Content/VampireSurvival/SourceFonts/OFL.txt`와 배포 폴더에 포함한다.
