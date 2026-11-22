# 동영상·언리얼 애니메이션 비교 뷰

Cubeless Body Mocap 창에 왼쪽 원본 영상, 오른쪽 실제 SkeletalMeshComponent와 AnimSequence 미리보기를 추가했다. UE MediaPlayer/MediaTexture와 Editor Preview Scene을 사용하며 MetaHuman 플러그인은 필요하지 않다. 에디터 전용 UI이며 생성 애니메이션에 UI·MCP 의존성을 추가하지 않는다.

## 사용법

Video와 추출 구간, Target Mesh를 설정하고 `Load comparison`을 누른다. 추출이 완료되면 해당 Settings의 `Preview Animation`에 저장된 결과가 자동 지정된다. 기존 결과를 선택할 수도 있다. `Play/Pause`, `Stop`, 공통 탐색 막대로 비교한다. Stop은 해당 추출 구간의 시작으로 이동한다.

동영상의 현재 시간에서 Start Seconds를 뺀 값을 오른쪽 애니메이션에 적용한다. 별도 애니메이션 재생 타이머를 누적하지 않는다. 실제 미디어 디코딩·화면 표시에는 프레임 지연이 있을 수 있으며 외부 젠록이나 픽셀 단위 동기화를 주장하지 않는다. 애니메이션 길이·영상 길이·선택 구간 중 가장 짧은 범위에서 재생을 끝낸다. 현재 미리보기에는 음성을 연결하지 않았다.

두 뷰는 영상 트랙의 실제 가로/세로 비율을 읽어 같은 비율로 배치하고 공간에 맞춰 축소한다. 초기 비율은 490/750이다. Target Mesh와 애니메이션 스켈레톤이 다르면 재생을 멈추고 안내한다. 비디오 파일을 변경하면 Load comparison을 다시 누른다.

## 구현과 검증 범위

주요 파일은 `SCBMComparison.{h,cpp}`, `CubelessMocapModule.cpp`, `CubelessMocapSettings.h`, `CubelessMocap.cpp`, `CBMEditor.Build.cs`다. MediaAssets/AdvancedPreviewScene/RenderCore/RHI를 Editor 모듈에 추가했다. 영상 플레이어와 텍스처는 strong reference로 보유하고 창 해제 시 플레이어를 닫는다. 추출 결과의 설정 연결은 기존 리타깃·저장 작업의 게임 스레드에서 수행한다.

UE5.8.4 Development Editor 빌드는 성공했다(`Saved/Logs/CubelessBodyMocap_ComparisonBuild.log`). 같은 프로젝트의 기존 에디터가 clean/Completed임을 확인하고 정상 종료한 뒤 같은 프로젝트를 다시 열었다. 새 PID27424, receipt는 `cubeless/records/verification/cbm-comparison-launch-20261108.json`이다.

사용자가 지정한 `C:/Users/Lenovo/Desktop/test2.mp4`는 H.264, 490×750, 30FPS, 약8.13초다. MediaPlayer의 ready=true/error=false 및 실제 트랙 크기490×750을 확인했다. 전체 244프레임을 고속·발고정·체크포인트 OFF로 추출했다. 후속 실행 결과와 재생/탐색 시간 표본은 `cubeless/records/verification/cbm-comparison-test2-20261108.json`에 기록한다.

컴퓨터 유즈는 사용하지 않았다. 기존 viewport capture는 활성 Level Editor 뷰를 반환해 두 패널의 시각 검증 근거로 사용하지 않는다. 실제 동작 매칭 품질의 최종 시각 검수는 사용자가 진행한다.

최종 test2 실행은 246.19초, 244/244프레임 추적·저장에 성공했다. 재생 구간 0.167~7.867초의 464개 표본에서 영상 clock과 애니메이션 position의 최대 차이는 약0.000000194초였다. 이는 시간값 일치 검사이며 디코더가 실제 표시한 픽셀의 시각적 지연 측정은 아니다. 일시정지 후2초 탐색은 양쪽2.0초로 확인했다.

초기 검증 스크립트가 ObjectIterator의 기본 MediaPlayer까지 GetUrl을 호출해 초기화되지 않은 엔진 내부 shared pointer assertion으로 에디터가 종료됐다. 추출 결과는 이미 저장돼 보존됐다. 실제 transient 플레이어로 조회 범위를 제한하고 같은 프로젝트를 다시 열어 검증을 마쳤다. 중간 Python Timespan 생성 이름 오류도 from_seconds 사용으로 수정했다. 최종 사용자 에디터는 PID19408이며 `cbm-comparison-reopen-20261108.json`에 기록했다. 창을 앞으로 가져와 시작 지점에서 일시정지했다.
