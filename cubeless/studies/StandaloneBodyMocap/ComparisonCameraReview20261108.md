# 비교 뷰 카메라 방향 검토·수정

사용자 스크린샷에서 영상 정면에 대응하는 Manny가 측면으로 표시됐다. 기존 미리보기는 +X에 카메라를 고정했지만, 추출 좌표 변환은 카메라 좌표의 Y/Z 반전 후 SMPL(x,y,z)→UE(x,z,y)를 사용하므로 원본 카메라 쪽은 +Y에 대응한다. 기본 카메라를 +Y에서 -Y를 보는 방향으로 수정했다. 애니메이션 에셋 자체를 90도 회전시키지 않는다.

## 함께 수정한 항목

- 고정 거리·높이를 주요 신체 본의 바운드와 영상 비율에 따른 전신 자동 맞춤으로 변경했다. 애니메이션/메시 변경과 Reset camera 때 맞추며, 재생 중 루트 움직임을 따라 매 프레임 재중앙화하지 않는다.
- Camera yaw, Zoom 및 Reset camera 컨트롤을 추가했다. 마우스로 시점을 바꾼 뒤에도 기본 영상 방향으로 돌아올 수 있다.
- 복잡한 환경 이미지와 바닥을 이 비교 뷰에서만 숨기고 단색 배경을 사용했다. 공유 에셋 뷰어 프로필은 변경하지 않는다.
- 파일 선택을 변경했는데 이전 영상이 재생되는 경우 재생을 멈추고 Load comparison 안내를 표시한다.
- 준비 전 탐색 막대를 비활성화하고, 키보드 탐색도 일시정지 후 이동하도록 수정했다.
- 실제 비교 위젯을 PNG로 저장하는 Editor 전용 SavePreviewScreenshot API를 추가했다. 고정 목적지는 Saved/CubelessBodyMocap/ComparisonPreview.png다. 이전 활성 Level Editor viewport 캡처 대신 두 패널의 시각 검증에 사용한다.

기본 정면 방향과 전신 구도는 근사 비교용이다. 영상 카메라의 정확한 내부 파라미터·피사체 화면 위치·체형을 복원한 렌더링은 아니며, 원본 인물과 Manny의 체형 및 손가락 자세 차이는 남는다.

## 확인

UE5.8.4 Development Editor 최종 빌드 성공, 해당 빌드 로그에 warning/error 없음: `Saved/Logs/CubelessBodyMocap_CameraReviewBuild.log`.

기존 사용자 에디터 PID19408의 clean/Idle 상태를 확인해 정상 종료하고 같은 프로젝트를 PID9888로 다시 열었다. `cbm-camera-review-{preclose,launch}-20261108.json`에 기록했다. test2의 기존 244프레임 결과를 사용했다. 영상 추출·학습모델·애니메이션 출력 에셋은 이 수정에서 다시 계산하지 않았다.

비교 위젯 자체의 0초와2초 PNG를 직접 검사해 정면 방향, 양쪽 팔/다리의 대응, 전신 표시, 같은 영상 비율, 중립 배경 및 카메라 컨트롤을 확인했다. 캡처 파일은 `Saved/CubelessBodyMocap/camera_review_frame{0,2}.png`다. 정확한 픽셀 포즈 일치나 모든 프레임의 영상 정답 대비 추출 정확도를 보증하지 않는다.

후속 재생/탐색 시간 표본과 최종 상태는 `cubeless/records/verification/cbm-camera-review-20261108.json`에 기록한다. 컴퓨터 유즈 없이 자체 Editor API와 MCP로 확인했다. 사용자 확인을 위해 같은 프로젝트의 비교 창을 유지한다.
