# Crimson Dawn V2 구현·검증 기록

2026-10-27. 현재 맵은 `/Game/VampireSurvival/Maps/L_CrimsonDawn`, 활성 HUD는 `BP_CrimsonHUDV2`다. 이 문서가 현재 영웅·조작·난이도에 대한 기준이며, README와 IMPLEMENTATION의 V1 수치 및 궁수/검사 명칭을 대체한다. 기존 V1 에셋은 보존했다. 기획 보강, 자체 리뷰, 구현과 아래 검증을 완료했다. 독립 QA는 수행하지 않았다.

## 변경 내용

- 궁수는 샤미 마법사로 교체했다. 분홍색 땋은 머리, 고양이 귀와 흰 끝의 두 꼬리, 붉은 운동복을 유지하고 보라색 마법탄과 폭발을 사용한다. 체력85, 이동220, 피해22, 공격 간격0.65초이며 폭발 반경65px, 보조 피해60%다.
- 검사는 미니에로 교체했다. 은보라색 머리, 검은 복장, 갈색 부츠와 가방을 유지했다. Slack의 최종 무기 설정에 나온 노란 산업용 수평계 검을 사용한다. 체력115, 이동195, 피해26, 공격 간격0.85초이며 전방 베기와 밀어내기를 사용한다.
- 각 영웅은 걷기·달리기·공격24포즈와 점프·급강하·착지16포즈를 사용한다. 전후 시점과 좌우 반전으로 이동 방향을 표시하고, 공격 준비 동작 뒤 실제 발사/피해가 발생한다.
- 적5종은 각각 이동4포즈와 공격4포즈를 사용한다. 개별 애니메이션 위상, 방향 반전, 공격 예고·회복, 피격 점멸을 적용했다. 몬스터 삭제 시 관련 배열의 순서를 함께 유지한다.
- ImageGen으로 타이틀 로고, 상태/성장 아이콘16개, 장식 프레임4종과 영웅 초상·능력 아이콘을 제작했다. 선택 화면 겹침과 작은 글씨를 수정하고 체력·스태미나·시간·경험치의 계층을 정리했다.
- 일반 적 상한179와 보스 전용1자리를 사용한다. 60초 이후 한 번에2마리, 90초 이후3마리를 생성하며 혼합 적·시간에 따른 체력 증가·150초 보스를 적용했다. 회복은32처치마다14HP다. 전투180초 생존이 승리 조건이며 보스 처치는 추가 목표다.

## 조작과 점프 판정

| 입력 | 동작 |
| --- | --- |
| WASD / 방향키 | 이동, 자동 공격 |
| 왼쪽 또는 오른쪽 Shift | 스태미나를 사용하는 달리기, 설정 이동 속도×1.55 |
| Space | 점프. 착지 후 재사용 대기10초 |
| 공중에서 Space 재입력 | 점프 시작 때 기록한 방향으로 급강하 착지 공격 |
| 1 / 2 / 3 | 성장 선택 |
| P | 일시정지 / 재개 |
| Enter / 마우스 | 메뉴 결정 |
| R / Backspace | 결과에서 재도전 / 타이틀 |

게임의 가상 화면에서 캐릭터 높이75px를1m로 정의했다. 점프 최고점은225px로 캐릭터 높이의3배다. 일반 점프는0.95초, 급강하는0.30초다. 급강하 목적지는 점프 시작 위치에서 기록한 방향으로375px(5m)이며 경기장 경계에서는 제한된다. 일반 점프 착지는 피해를 주지 않으며, 공중 재입력 착지만 약한 피해와 밀어내기를 준다.

**급강하 중 착지 예정 지점에 반경95px(약1.27m)의 청록색 원을 표시한다. 착지 순간 같은 중심·반경으로 흰색/청록색 섬광을 표시하며 실제 피해 판정에도 동일한 값을 사용한다.** 피해는 기본 공격력의35%, 최소6이며 일반 적에는 추가110px, 보스에는22px 밀어내기를 적용한다. 일반 피격 함수의 밀어내기도 합산된다. 지면 그림자를 유지하고 높이30px 초과에서는 지상 공격을 피한다.

적의 강한 공격은 준비 중 금색, 타격 중 붉은색 지면 원으로 표시한다. 표시와 판정이 같은 중심과 반경을 사용한다.

| 공격 | 반경 |
| --- | ---: |
| Revenant | 66px |
| Stalker | 58px |
| Bellkeeper 근거리 | 75px |
| Bellkeeper 광역 파동 | 165px |

북쪽 경계에서 점프가 화면 위로 사라지는 문제를 카메라 이동으로 보완했다. 지면·적·원 표시를 함께 이동하고 HUD는 고정했다. 스태미나100, 초당 소모34,0.65초 지연 후 초당 회복24이며 소진 후25 이상 회복해야 다시 달릴 수 있다.

## 아트와 참고 출처

이미지는 내장 ImageGen으로 생성·수정했다. 별도 이미지 API나 CLI는 사용하지 않았다. 실제 사용 원본은 `Content/VampireSurvival/SourceArt/`의 `Title_Logo.png`, `Shami_Motion.png`, `Minie_Motion_Clean.png`, `Jump_Motion.png`, `Hero_Icons.png`, `UI_Icons.png`, `UI_Frames.png`, `Enemy_Thrall.png`, `Enemy_Bat.png`, `Enemy_Revenant.png`, `Enemy_Stalker.png`, `Enemy_Boss.png`다. V1 배경과 오디오도 유지한다. 사용하지 않는 초안도 이력으로 보존했다. [최종 프롬프트](../../records/verification/vampire-survival-v2/imagegen_final_prompts.json)와 초기 `imagegen_prompts.json`에 요청을 기록했다.

- [샤미 원화 공유](https://rxbase.slack.com/archives/C06DFPPKKJ8/p1786326279516209): Slack 파일 `F0BP5358AKB`.
- 미니에 원화: Slack 파일 `F095KFE8SEB`. [미니에 무기 설정](https://rxbase.slack.com/archives/C070N6MRSQ4/p1740449995999469): 최종 파일 `F08HSQHKCP9`, `미니에 수평계.jpg`.
- 다운로드한 참조는 `SourceArt/References/`에 보존했다. Slack 메시지 전송·수정은 하지 않았다.
- [Hades II 공식 페이지](https://www.supergiantgames.com/games/hades-ii/)의 아트 중심 진입 화면과 집중된 행동 선택, [Darkest Dungeon II 공식 페이지](https://www.darkestdungeon.com/darkest-dungeon-2/)의 어두운 패널·따뜻한 강조색·제목 계층을 설계에 참고했다. 이는 확인한 공식 자료에 대한 설계 해석이며 로고·그림·레이아웃을 복제하지 않았다.
- Diablo IV403, Awwwards 시간 초과, Path of Exile2 JavaScript/브라우저 시간 초과는 완료된 시각 리뷰로 세지 않았다.

## 검증 결과와 제한

[전체 보고서](../../records/verification/vampire-survival-v2/delivery_report.json)에 원시 결과와 파일 해시가 있다. 컴파일은 UP_TO_DATE, 오류0, 그래프41개이며 저장된 Unreal 에셋31개의 금지 런타임 참조는0개다.

- `functional_fixture.json`: 최고점225px, 이동375px, 쿨다운10초와 재입력 차단, 착지 반경95px 안/밖, 밀어내기, 일반 적179+보스1 상한과 배열 정합성을 확인했다. 샤미 착지 피해는7.7, 반경96px 적은 피해0이었다.
- `input_checks.json`: PIE의 실제 입력 폴링 경로로 메뉴·이동·Shift·Space/공중 재입력·쿨다운 거절·P 정지/재개를 확인했다. 입력 측정 최고점224.985px이었다. 짧은 측정 구간의 달리기/걷기 비율1.489에는 키/프레임 경계 영향이 포함되며 설정값×1.55와 구분한다.
- `final_functional.json`: 공격 모션 이후 피해, 적66/67·58/59·75/76·165/166px 경계, 마법 폭발65/66px 경계, 카메라와 HUD 복원, 첫 클래스 강화 제공을 확인했다.

정상 속도에서 외부 키 입력만 사용한 플레이 결과다. TestBot=False이며 HP·시간·피해를 강제로 바꾸지 않았다. 각 결과는 당시 빌드 관측이며 아래 과거 승리를 최종 빌드 전 클래스 완주로 일반화하지 않는다.

| 기록 | 생존 | 결과 | 처치 / 레벨 | 범위 |
| --- | ---: | --- | --- | --- |
| minie_hard_result | 180초 | 승리 | 1030 / 13 | 이후 카메라·강화 제공·표현 보강 전 |
| minie_final_result | 126.424초 | 패배 | 462 / 9 | 보강 후 입력 플레이 |
| shami_hard_result | 81.093초 | 패배 | 53 / 3 | 폭발 보강 전 |
| shami_final_result | 116.449초 | 패배 | 355 / 10 | 폭발 반경52px 단계 |
| shami_final65_result | 122.276초 | 패배 | 384 / 9 | 최종 반경65px |
| shami_safe_final_result | 103.676초 | 패배 | 272 / 7 | 최종 반경65px, 다른 이동 전략 |

**샤미180초 완주는 아직 확인하지 못했다.** 난이도는 승리와 패배가 모두 관측되는 수준이며 사람 대상 균형 평가나 전수 성능 측정은 하지 않았다.

최종 `package-65-final.log`는 Win64 BuildCookRun 성공, ExitCode0을 기록한다. MCP와 Python 플러그인을 비활성화한 별도 콘텐츠 전용 프로젝트에서 Cook/Stage/Package 후 실제 실행했으며 금지 플러그인 마운트도0개였다. 패키지의 게임 화면은 Development 콘솔 StartRun/SpawnEnemy를 사용한 강제 스모크 검사다. 패키지의 OS 키보드·마우스 입력은 별도로 자동 검증하지 않았으며 PIE 입력 시험과 구분한다.

화면 증거는 `packaged-title-final.png`, `character-select-final.png`, `telegraphs-and-dive-final.png`다. 마지막 범위 표시 화면은 통제된 시각 검사이며 전역 시간 배율0.0001을 사용했다. World pause만으로 Canvas HUD 시계가 정지하지 않았던 시도는 `telegraphs-world-pause-not-frozen.png`에 보존했다. 게임의 P 정지는 정상 입력 시험에서 확인했다. 정상 플레이 결과에는 시간 배율 변경이 없고 마지막 PIE는 종료했다.

실행 파일은 `cubeless/records/verification/vampire-survival-v2/Packaged/Windows/CrimsonVerify58.exe`다. 같은 Windows 폴더의 동반 파일을 유지해야 한다. 최종 일반 타이틀 창을 열어 두었다.

## 제작 과정과 런타임 독립성

공식 노드 추가 경로에서 promoted 산술/비교 노드 작성 중 에디터 크래시4회를 관측했다. 정확한 원인은 확정하지 않았다. 로그와 저장된 그래프를 보존하고, 기존 editor-only `CubelessFunctionLibrary.add_blueprint_call_function_node`로 표준 Engine 호출 노드를 작성해 이어갔다. 그래프 삭제나 불확실 요청의 맹목적 재시도는 하지 않았다. 관련 로그는 기존 V1 `editor-live.log`와 V2 `editor-recovered*.log`에 있다.

프로젝트·MCP·엔진 C++ 변경은 없다. Python과 MCP는 제작 도구로만 사용하며 게임 실행에는 필요하지 않다. 별도 검증 프로젝트는 `cubeless/workspaces/CrimsonVerify58/`이며 패키징과 실제 실행을 통해 결과물의 독립성을 확인했다. 이 과정은 메인 에디터의 MCP 서버 종료 시험을 의미하지 않는다. Git stage·commit·push는 하지 않았다.
