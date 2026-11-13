# 모캡 UI 비교와 개선 여지 — 2026-11-13

> 아래 비교는 UI 변경 전 기준입니다. 사용자 승인에 따른 1차 구현 결과는 문서 끝에 따로 기록했습니다.

## 판단

현재 Cubeless UI를 공식 마커리스 모캡 작업 화면과 비슷한 사용성으로 개선할 여지가 크다. 우선순위는 기능 버튼 추가보다 배치, 처리 구간 선택, 결과 비교, 작업 상태 표현이다. 기존 계산·HandFlow·본 보정·저장 흐름을 유지하면서 1차 개선이 가능하다. 정식 도킹 에디터·Sequencer·작업 애셋은 그 다음 단계이며 단순 외형 수정과 구분한다.

이 검토는 설치된 UE5.8.4의 공식 소스, 현재 프로젝트 소스, 2026-11-12 실제 실행 캡처를 비교한 것이다. 11월13일 현재 CBMEditor 소스 5개가 캡처 당시 검증 지문과 같음을 확인했다. 검토 시 실행 중인 UnrealEditor는 없었다. 공식 플러그인을 켜서 두 UI를 동시에 조작한 검증은 수행하지 않았고, 플러그인 소스·설정·에디터 활성 상태는 변경하지 않았다. 기준 프로젝트 HEAD: d4e23ecc529953e1364c87ab098566cc65037111.

## 비교 대상의 실제 구조

공식 MetaHumanBodyTracker 모듈은 body tracker를 modular feature로 등록한다. 작업 화면은 MetaHuman Animator의 FMetaHumanPerformanceEditorToolkit가 구성한다. 즉 공식 화면의 완성도는 별도 Markerless 전용 창보다 기존 Performance 애셋 에디터, 이미지 검토, 3D 뷰포트, Details, Timeline의 결합에서 나온다.

우리 모듈은 SWindow(초기1280×900)에 좌측 Details30%·우측 비교 화면70%를 배치한다. 영상/3D는 각각 750 높이를 기준으로 ScaleToFit한다. 처리 상태와 실행·저장 제어는 하단의 여러 행을 사용한다. 비율은 창 크기에 따라 변하지만 사용자가 패널 경계를 끌어 조절하는 Splitter나 도킹 탭은 없다.

## 확인한 차이와 개선안

| 항목 | 현재 Cubeless | 공식/개선 방향 | 필요한 작업 |
|---|---|---|---|
| 작업 화면 | 독립 창에 전체 설정 노출 | 공식은 도킹 가능한 Image Review·Viewport·Details·Timeline 구성 | 우선 SSplitter로 조절 가능하게, 이후 AssetEditorToolkit 도입 |
| 실행 전 영상 검토 | 재생 버튼이 PreviewAnimation 존재도 요구함 | 모캡 전에도 원본을 재생하며 구간을 고를 수 있어야 함 | 재생 조건 분리; 결과가 있으면 동기화 유지 |
| 시간 탐색 | 전체 구간을 나타내는 SSlider와 초 표시 | 공식 Sequencer는 처리 범위와 시간 표시를 제어함 | 프레임 표시·앞뒤1프레임·시작/끝 지정, 이후 범위 핸들/줌 |
| 처리 구간 | 시작 초·길이를 숫자로 입력 | 보이는 영상에서 In/Out을 고르는 흐름 | 시점→구간 설정 연결 및 FPS/종료 경계 검증 |
| 설정 발견 | FFmpeg·프레임 폴더·Python 경로·스텝 등과 일상 설정이 섞임 | 기본 입력·대상·처리 옵션과 환경/고급 설정 분리 | 전용 Detail customization 또는 섹션·고급 접기 |
| 계산과 결과 선택 | 실행 HandFlow/본 보정과 저장 HandFlow/본 보정이 반복됨 | 계산할 옵션과 표시/내보낼 결과가 명확해야 함 | 계산 옵션 유지, 결과는 전신/전신+손보정/HandFlow/HandFlow+손보정 선택기로 정리 |
| 명칭 | 본 보정이 실제로는 손가락 관절 범위·관통 보정 | 전신 보정으로 오해하기 쉬움 | ‘손가락 관절 보정’ 등 실제 역할을 표시하고 하체 보완과 구분 |
| 상태/오류 | 하단 문자열에 단계·RAM·긴 에셋 경로까지 표시 | 완료 상태·취소 대기·다음 행동을 쉽게 알아야 함 | 단계 진행 표시·요약 상태·접을 수 있는 상세 로그·저장 후 에셋 열기 |
| 뷰포트 검사 | 영상 추정 카메라, yaw/zoom, 자유 시점 기반이 이미 있음 | 영상과 같은 구도와 전체 캐릭터 검사는 목적이 다름 | ‘영상 구도’/‘전체 캐릭터’ 명시, 전체/손/발 맞춤 기능 노출 검토 |
| 설정 보관 | NewObject로 만든 transient Settings; 일반 UI에 작업 저장/재개 없음 | 공식은 Performance 애셋 중심 작업 | 우선 프리셋/작업 기록, 이후 에디터 전용 Take 애셋 |

위 공식 비교는 바디 작업에 확인되는 공통 구조를 기준으로 했다. Identity의 얼굴 마커 편집/Promoted Frame을 바디 기능으로 간주하지 않았다. 공식 설치 소스의 CanExportLevelSequence는 !bBodyTracking 조건이 있어, 문서의 Level Sequence 내보내기를 바디에서도 그대로 된다고 주장하지 않는다.

## 권장 배치

- 상단 툴바: 영상 열기 / 모캡 실행 / 취소 / 선택 결과 내보내기.
- 중앙 넓은 영역: 원본 영상과 3D 결과. 패널 크기 조절과 한 화면 확대를 제공한다.
- 우측 설정: 입력·대상·처리 옵션을 우선 표시하고 고급/환경 설정은 접는다.
- 뷰포트 상단: 표시할 결과 선택, 영상 구도/전체 캐릭터 검사 모드.
- 하단 타임라인: 재생·프레임 이동·시간/프레임 표시·처리 In/Out·처리 완료 구간.
- 상태줄: 현재 단계와 진행률, 취소 대기, 오류 요약. 상세 로그는 별도 접기/탭.

결과 선택은 현재 SaveHandFlow/SaveBoneCorrection→ResolveSaveAnimation 계약을 보존해야 한다. 아직 계산하지 않은 손가락 보정 결과를 선택하면 현재처럼 보정만 추가 계산하며 전신·HandFlow 추론을 반복하지 않는다. 재생 속도와 저장 RateScale도 서로 다른 기능으로 유지한다.

## 단계별 범위

1. **우선 적용 가능한 UI 개선:** 가변 패널, 상단 툴바, 원본 단독 재생, 기본/고급 구분, 결과 선택기, 명확한 상태줄. 기존 계산 API를 재사용할 수 있다.
2. **사용성 효과가 큰 후속 작업:** 프레임 단위 이동, 처리 구간 핸들, 타임라인 확대, 작업 프리셋과 최근 작업. 미디어 seek와 프레임 인덱스/디코더 정확도를 함께 검증해야 한다. 버튼만 추가해 frame-accurate를 보장할 수는 없다.
3. **공식에 가까운 에디터 구조:** 자체 FAssetEditorToolkit·FTabManager와 작업 보관, 필요시 Sequencer 연동. Slate·Unreal 기본 에디터 프레임워크를 사용하고 MetaHumanPerformance의 private toolkit를 직접 재사용하는 방식은 피하는 것이 좋다. 직접 결합하면 MetaHuman 애셋/footage/rig/pipeline 전제까지 따라온다.

마우스로 대상 인물을 고르는 기능, 검출 관절을 직접 고쳐 재추론하는 기능, 프레임별 키 교정, 얼굴+전신 결합은 새 데이터 흐름과 계산 기능이 필요하다. 현재 UI 배치 수정만으로 제공된다고 표현하면 안 된다. 현재 본 디버그는 진단 표시이며 수동 관절 교정 도구가 아니다.

작업 저장 애셋을 추가할 경우 에디터용 작업 데이터와 최종 AnimSequence를 분리하고 최종 결과에 에디터/MCP 의존성을 남기지 않아야 한다. UI 개선 때문에 뷰포트 렌더링 일시정지·취소·기존 영상/애니메이션 동기화·원본 보존·저장 안전 장치를 약화시키지 않는다.

## 검증 기준과 한계

- 변경 전/후 같은 입력·옵션으로 추론 결과와 저장 조합이 유지되는지 확인한다.
- 모캡 전 원본 재생, 프레임 이동, 범위 경계, 다른 FPS, 원본 시간과 선택 구간 상대 시간의 대응을 검사한다.
- 네 결과 조합의 미리보기/저장 일치, 없는 결과의 안내, 저장 RateScale과 미리보기 속도 분리를 검사한다.
- 창 크기 변경·패널 조절·영상 비율 변경·에디터 재시작 시 작업 복원은 구현 범위에 맞춰 검사한다.
- 성공·오류·취소·창 닫기 중 작업 수명과 렌더링 설정 복원을 보존한다.

비교 검토를 마친 시점에는 UI 구현·새 빌드·독립 리뷰·실제 양쪽 에디터 조작 비교를 수행하지 않았다. 이후 사용자가 네 가지 UI 개선과 c++1218을 명시적으로 승인하여 아래 범위로 구현했다.

## 근거

- 공식 설명: https://dev.epicgames.com/documentation/en-us/metahuman/metahuman-performance-asset
- 현재 화면: cubeless/records/verification/sam-tracking-speed-20261111/full-final-gui-workflow.png
- 자체 창/설정/실행·저장: Plugins/CubelessBodyMocap/Source/CBMEditor/Private/CubelessMocapModule.cpp:58,94–117.
- 비교 영상·슬라이더·재생 제한: Plugins/CubelessBodyMocap/Source/CBMEditor/Private/SCBMComparison.cpp:225–255.
- 재생 동기화 기반: Plugins/CubelessBodyMocap/Source/CBMEditor/Private/CBMPreviewClock.h.
- 설치된 공식 레이아웃: UE_5.8/Engine/Plugins/MetaHuman/MetaHumanAnimator/Source/MetaHumanPerformance/Private/MetaHumanPerformanceEditorToolkit.cpp:99–159,640–670,2183–2190.
- 공식 바디 모듈 등록: UE_5.8/Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.8/Source/MetaHumanBodyTracker/Private/MetaHumanBodyTrackerModule.cpp.


## 승인된 1차 구현 결과

- 상단 영상 열기 → 모캡 실행 → 선택 결과 내보내기, 취소·저장 에셋 열기와 상태/진행률/상세 로그를 배치했다.
- 중앙 영상/3D와 우측 설정에 SSplitter를 적용했다. 기본 설정과 접힌 고급/환경 설정을 분리했다. 별도 도킹 에디터나 작업 에셋은 추가하지 않았다.
- 결과 없이 원본 전체를 여는 모드, 원본 FPS 기준 프레임 이동, 분석 FPS 기준 시작/끝 핸들·숫자 입력·현재 시점 지정 기능을 추가했다. 끝은 제외 경계이며 원본 끝·최소 정수 프레임 길이·최대120초를 제한한다. 기존 프레임 폴더 입력은 범위 편집을 잠가 계산 입력을 바꾸지 않는다. 결과 재생의 시작 시각은 다음 구간 선택과 분리했다.
- 계산 체크와 네 가지 표시/내보내기 결과 선택을 분리했다. 기존 SaveHandFlow/SaveBoneCorrection과 ResolveSaveAnimation, 지연 손가락 보정, 출력 덮어쓰기 보호, 저장 RateScale 계약을 재사용했다. 새로운 전신·손 추론 또는 품질 설정 변경은 없다.

변경 범위는 CBMEditor의 CubelessMocapModule.cpp, SCBMComparison.cpp/.h, Build.cs와 UI 범위 위젯/경계 계산/자동 검사 신규 파일이다. 모듈에 DesktopPlatform과 InputCore를 추가했다. 엔진·추론·보정 코드, 모델, 해상도, FPS 기본값은 바꾸지 않았다. 변경 전 계산/도구 파일 328개의 SHA256을 다시 비교해 모두 같았다. 기존 run_hand_worker.py 로컬 수정도 그대로다.

### 확인한 결과

- 실제 프로젝트 UnrealMCP5_8_3Editor Win64 Development 빌드 성공: UE5.8.4 CL58902052, 최종 build-release.log.
- 최종 빌드에서 Cubeless.BodyMocap.Preview의 ClipRange, ContinuousPlayback, DelayedSeekDuringScrub 3개 성공. 24fps 최소 구간·교차·비정수 영상 끝과 120초 제한을 포함한다.
- 기존450프레임·161트랙의 네 결과 조합을 별도 소유 검증 경로로 내보냈다. 지연 전신 손가락 보정도 완료됐다. 원본 대 내보내기 RAW 변환은 위치/스케일 차이0, 정규화 쿼터니언 각 계산의 최대차이 약0.000002415도였다. 저장 속도0.5인 보정 HandFlow도 키 재샘플링 없이 유지됐다. 계산 원본과 기존 최종 에셋은 덮어쓰지 않았다.
- 최종 DLL에서 원본 모드와 결과 모드의 실제 Slate 렌더링을 엔진 스크린샷 API로 확인했다. 결과 캡처의 원본/애니메이션 시간차는 약1.5e-9초였다. 특정 캡처 시점의 결과이며 전체 seek 정확도 검증을 대신하지 않는다.
- 근거: cubeless/records/verification/mocap-ui-20261113/ 아래 build-release.log, editor-regression.log, *-comparison.json, source-workflow.png, result-workflow.png, result-playback.json, final-verdict.json.

### 11월13일 당시 남은 직접 조작 검증

Windows 잠금 화면이 확인되어 computer-use guidance에 따라 네이티브 입력을 중단했다. 파일 열기 대화상자, 재생/프레임 버튼, 범위 핸들, 패널 크기 변경, 결과 메뉴의 실제 클릭 경로는 미검증이다. 엔진 API 검증을 마우스 검증으로 보고하지 않는다. PC 잠금 해제 후 진행할 수 있다. 1000×720 최소 창·극단적 패널 비율·가변 FPS 디코더 정확도와 독립 리뷰도 미검증이다. 이번 UI 작업에서 모델 전체 재추론이나 패키징은 반복하지 않았다.


## 2026-11-14 직접 제어 API와 최종 검증

잠금 해제 후 파일 대화상자, 원본 재생·정지,60fps 앞뒤 한 프레임,3~11초 범위 핸들, 두 패널 경계와 고급 설정 접기를 실제 입력으로 확인했다. 작은 창에서 원본 시간·프레임 정보가 잘려 별도 줄로 옮겼고, 계산 전 구간 검토 안내를 명확하게 바꿨다. 이후 사용자가 Computer Use 중단과 직접 조작 기능을 요청했다. 그 이후 Computer Use는 사용하지 않았다.

Editor 전용 UCubelessMocapWindowLibrary를 추가했다. get_settings는 UI 설정 객체를 직접 반환하며 open_video/control/set_range/select_result/set_layout/get_state/close로 창을 제어한다. 기존 StartMocap·Cancel·SaveAnimationSequence API에 같은 설정 객체를 전달한다. 새 서버, OS 입력 주입이나 MCP 실행 의존성을 추가하지 않았다. 모듈은 기존 CBMEditor이며 결과 에셋은 일반 AnimSequence다. 사용법: Plugins/CubelessBodyMocap/WINDOW_API.md.

명령은 JSON 수락/실패와 상태를 반환한다. 비동기 영상 준비·탐색·계산과 출력 에셋 경로를 상태로 확인한다. 결과 선택은 같은 시점을 유지하며 불필요한 재탐색을 하지 않는다. 원본 절대 시각과 현재 결과의 상대 시각/실제 평가 시각을 구분한다. 실행 중에는 영상·구간·결과·레이아웃 변경과 창 닫기를 거부한다.

첫 API 검증에서는 디코더가 0초 요청에0.033초를 반환해 Clock.Pending이 풀리지 않는 기존 프리뷰 문제를 재현했다. 첫 보완의 반복 seek도0.133초 불일치를 보여 자동 재시도는 제거했다. 최종 구현은 원본 FPS 격자로 탐색하고, 시작의 가까운 첫 샘플은 실제 시각을 표시하며 정확하지 않았음을 보고한다. 더 큰 불일치는 약3초 후 실패로 보고하고 실제 원본/3D 시각을 동기화한다. 첫 실패 로그와 테스트의 부정확한 초기 기대값은 보존했다. 모든 코덱의 프레임0 정확도를 검증했다고 주장하지 않는다. 기존 CBMPreviewClock과 계산 알고리즘은 변경하지 않았다.

최종 build-api-final.log 빌드가 성공했다. control-final/api-result.json의 직접 API 시나리오13개, 최종 Editor 로그의 Preview 자동 검사3개가 모두 성공했다. 실제 API 저장은 지연 손가락 보정과 계산 중 변경 거부를 포함한다.450프레임×161트랙 내보내기 비교의 위치/스케일 차이는0, 정규화 쿼터니언 각 최대차이는2.415e-6도였으며 RateScale0.75를 키 재샘플링 없이 유지했다. 다음 구간 편집 후 기존 결과의 원본 시간 기준과5초 시점의 영상/애니메이션 동기화도 확인했다. 실제 작은 창 렌더링을 엔진 API로 다시 확인했다.

최종 검증은 같은 UnrealMCP5_8_3 프로젝트, UE5.8.4 CL58902052에서 수행했다. 기존 계산/도구328파일의 SHA256이 모두 동일하다. 새 전체 모델 재추론, 패키징, 독립 리뷰, 작업 파일/프리셋·재시작 복원은 이번 검증 범위에 포함하지 않는다. 네이티브 결과 메뉴 클릭 검증은 사용자 요청으로 직접 API 검증으로 대체했다. 근거: cubeless/records/verification/mocap-ui-20261114/final-verdict.json 및 control-final/.
