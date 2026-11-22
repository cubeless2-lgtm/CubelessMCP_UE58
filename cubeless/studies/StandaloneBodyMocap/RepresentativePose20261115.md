# 대표 자세 보정, 실제 영상 프리뷰와 전용 Undo

## 대상과 범위

UE 5.8.4 CL58902052의 현재 개발 프로젝트에서 CubelessBodyMocap CBMEditor만 수정했다. 사용자 c++1218 승인 범위는 대표 프레임 사전 분석/퀴니 자세 편집/전체 결과 반영이며, 추가 요구는 3D 기즈모, 원본 영상의 검은 화면, 녹색선 표시, Ctrl+Z이다. 원본 애니메이션, FK Control Rig 및 Anim BP를 편집하지 않는다. Computer Use와 다른 프로젝트, 별도 에이전트는 사용하지 않았다.

## 구현과 재현

설정은 UCubelessMocapSettings의 RepresentativeCount 및 transient 대표 프레임/오프셋으로 보관한다. StartRepresentativePreview는 별도 작업 설정으로 5개 프레임만 추적/키포인트/CHMR 처리한다. Hue는 각 특징을 64회 유지한 가상 시퀀스의 중앙 결과를 선택한다. 임시 이동을 안정화하고 전체 카메라 최적화/손 계산을 생략하며, 전체 시간축으로 확장한 결과는 임시임을 표시한다.

SCBMComparison은 UDebugSkelMeshComponent와 UAnimPreviewInstance를 사용한다. 대표 시각에서 25개 본 중 하나를 선택하고 회전한다. 기즈모는 축별 hit proxy 고리, 레이/축 평면 교차와 signed angle로 드래그한다. 동일 회전 경로를 pose_rotate_x/y/z API로 검증했다. 수동 마우스 드래그와 물리 키 입력은 검증하지 않았다.

전용 데이터 이력은 원본 에셋이나 임시 UObject 생성을 전역 트랜잭션에 넣지 않는다. Ctrl+Z/Y/Shift+Z를 모캡 창 루트의 OnPreviewKeyDown에서 처리한다. Begin/EndRepresentativeEdit로 드래그를 한 단계로 묶고 최대 100단계를 유지한다. 프리뷰 월드/객체 생성 구간에서 외부 GUndo를 억제하며 자동화는 공식 execute_python의 transaction_policy=script를 사용한다. 이력을 디스크에 직렬화하지 않고 보정 데이터만 JSON에 기록한다.

전체 모캡은 기존 전 프레임 모델·정밀도·추적·카메라·손 경로를 유지한다. 마지막에 선택 본의 로컬 회전만 Slerp 오프셋으로 바꾸고 새 애니메이션에 저장한다. 나머지 모든 로컬 회전·위치·스케일과 키 수/FPS/길이는 네이티브 저장 전에 검사한다. 0도일 때는 원본 자체를 반환한다. 입력 경로·크기·수정 시각·구간·FPS·메시와 프레임 맵이 다르면 보정을 거부한다.

영상은 DebugBones.json의 출처와 일치하는 Frames PNG를 사용해 검출과 같은 프레임을 보여준다. 별도 캐시가 없을 때 정지 프레임은 소유한 FFmpeg 프로세스로 비동기 추출한다. PNG 디코드 후 하나의 transient 텍스처를 UpdateTextureRegions로 재사용하여 매 프레임 UObject를 생성하지 않는다. 3D 평가와 오버레이를 displayed_source_seconds에 맞추고 디코더 media_seconds는 별도로 유지한다. 전체 결과 청록선은 고정 촬영 카메라에 투영한다. 임시 결과는 정합되지 않은 자홍/청록선을 숨기며 실제 대표 키 외에 초록 관측을 만들어내지 않는다.

API 예제와 사용자 절차는 Plugins/CubelessBodyMocap/WINDOW_API.md 및 REPRESENTATIVE_POSES.md에 있다. 엔진 API 근거는 설치된 UnrealEd DebugSkelMeshComponent/EditorViewportClient, Engine PrimitiveDrawInterface/Texture2D/ImageUtils, ImageCore의 헤더와 구현을 직접 확인했다.

## 실패에서 수정한 사항

첫 5프레임 Hue 직접 입력은 약 43초였지만 카메라/이동값이 발산해 화면이 비어 있었다. 이 측정을 유효한 사전 편집 시간으로 사용하지 않는다. 원본 수치 기록과 실패 스크린샷은 representative-v1-invalid-camera.json에 남겼다. 특징 64회 유지/중앙 출력/임시 카메라 분리 후 가시 본 13개와 유한한 카메라를 확인했다.

전역 TRANSACTION UNDO가 MCP 명령의 프리뷰 객체 생성까지 되돌리며 IsRooted assert로 에디터를 종료시킨 기록이 있다. 원본 로그/크래시를 보존하고 전용 자세 이력과 프리뷰 생성의 트랜잭션 분리를 적용했다. 변경 후 API의 Undo/Redo/이력 분기와 같은 기즈모 회전 경로는 통과했다. 전역 Undo에 사용자 작업을 노출하는 재시험은 하지 않았다.

기즈모/본을 월드 굵기로 그려 화면을 가리는 문제를 확인했다. 표시선은 얇게 그리고 hit test 패스만 넓게 처리했다. 전체 완료 후 원본 검토 상태에 남는 경로도 완료 콜백에서 프리뷰 재로드하도록 보강했다.

## 측정과 검증 범위

KaXLIZbdnek의 236~246초, 10초/분석30fps/300키/Quinn/대표5개를 사용했다. 체크포인트 재사용을 끈 전체 경로는 425.119초였다. 정상 대표 결과는 약 40~49초였으며 화면의 영상·기즈모·편집 가능 상태까지 확인한 실행은 42.050초다. 이는 첫 보정 대기 약 90.1% 감소/약 10.1배다. 전체 완료 시간 단축은 아니며 대표 분석 시간과 사용자 편집 시간이 기존 전체 추론에 추가된다. 최종 자동 표시 검증은 캐시 재사용 실행으로 별도 기록한다.

기존 전체 경로와 추적/키포인트/CHMR 특징/손 할당 키포인트의 네 단계 수치 MD5가 모두 같았다. 5도 척추 보정본의 300프레임/161트랙에서 나머지 160개 로컬 회전과 모든 위치/스케일 차이는 0이었다. 적용/별도 내보내기/0도 원본 반환/원본 파일 SHA 유지도 확인했다. 검사용 5도는 사용자 보정으로 남기지 않는다.

대표 5시각의 실제 영상·3D 시각 일치, 기즈모와 같은 경로의 회전 및 Undo/Redo, 단일 제스처 묶기와 새 편집 시 Redo 제거, 정지/재생 반복 3회, 전체 청록선의 프리뷰 카메라 회전 독립, 분석 전 FFmpeg 정지 화면을 확인했다. 원본/Contact_v2/HandFront_v4 보존 해시와 마지막 PID/표시 에셋은 final-verdict.json에 기록한다.

독립적인 스레드/수명 리뷰, 실제 마우스·물리 키 입력, 이번 변경의 게임 패키징, 새 배포 ZIP 생성과 다른 PC 검증은 수행하지 않았다. 엔진 Editor 모듈 변경이며 출력은 표준 AnimSequence를 사용한다. 커밋/푸시는 하지 않는다. 원본 단안 검출의 가림/깊이 한계나 모든 영상의 정확도를 해결했다고 주장하지 않는다.

증거: cubeless/records/verification/representative-pose-20261115/.
