# Quinn Control Rig 편집과 재생 UI — 2026-11-15

현재 모캡을 편집 가능한 Control Rig 키로 만들고 새 AnimSequence로 저장했다. 원본과 애님 BP/Control Rig Blueprint는 유지했다. Computer Use는 사용하지 않았다.

최종 시퀀스는 `/Game/CubelessMocap/KaXLIZbdnek_236_246_20261115/LS_KaXLIZbdnek_Quinn_EditRig`, 출력은 같은 폴더의 `AN_KaXLIZbdnek_Quinn_EditRig_Base`이다. Quinn 스폰 객체, 비활성 원본 애니메이션 트랙 및 표준 FKControlRig 키 트랙을 사용한다.30fps/300표본/161트랙/길이9.966666s/RateScale1이다. 실제 재생과 같은 Quinn 압축 포즈 평가에서161개 본 최대 위치 차이0.030523cm, 로컬 회전 차이0.174691도였다. RAW 평가의 일부 보조/손가락 트랙은 체형/압축 적용으로 달라 무손실 RAW 복사를 주장하지 않는다.

초기300프레임 소스 섹션은9.9667초 애니메이션 마지막 표본에서 처음 자세로 반복됐다. 끝 오프셋을 한 표시 프레임(-800틱) 연장해 마지막 키를 유지했다. 기본 CR_Mannequin_Body의 다리는 Bool IK 스위치가 True여서 무릎 위치가 달랐다. Float setter는 Bool을 변경하지 못하므로 타입 확인이 필요하다. FK Bool False로 개선했으나 척추/손가락 분배 차이가 남아 표준 FKControlRig을 기본 편집 경로로 채택했다. 템플릿의 절차적 Post Process Anim BP를 끄고 내보낼 때 Quinn 리타깃 소스 정보를 설정했다.

별도 `/Game/_MCP_Temp/ControlRig/CubelessTests/20261115` 시험본에서3~5초 spine_01_CONTROL을5도, 양끝0.5초 램프로 변경했다. 새 애니메이션의105/120/135프레임은 약5도,0/60/90/150/180/299프레임은0도였다. 기본 편집본에는 시험 보정을 넣지 않았다. 재시작 후 동봉 helper로 spine_02_CONTROL의6~7초2도 보정/새 출력도 확인했다. 원본/v2/v4 및 기본 편집 시퀀스의 파일 해시를 유지했다.

API 사용 시 `ArrayOfRigControlTransforms` 배열의 임시 구조체에 대입하면 실제 전달값이 갱신되지 않을 수 있었다. 새 구조체와 대상 섹션을 명시한 뒤 실제 변화가 확인됐다. 첫 실패 출력은 보존했다. helper는 Plugins/CubelessBodyMocap/Tools/control_rig_edit.py, 사용법은 CONTROL_RIG.md에 있다.

처음 `execute_unreal_python`으로 에셋 에디터를 열 때 외부 command transaction 역할 변경으로 unknown 결과를 받았다. 생성된 에셋/현재 시퀀스/바인딩/리그를 확인하고 생성 작업은 반복하지 않았다. 저장 완료와 clean 상태에서 정상 재시작했다. 이후 등록된 `execute_python`의 `transaction_policy=script`로 열기/편집을 정상 검증했다. public execute_unreal_python은 transaction_policy 인수를 거절하므로 두 API를 구분한다. 실패 기록과 결과는 보존했고 MCP 코드는 수정하지 않았다.

기존 재생 실패는 WMF가 끝 위치9.9933초에서0초 Seek를 수락해도 이동하지 않는 문제로 재현됐다. 같은 디코더를 다시 열되 절대 요청 시각/선택 애니메이션/구간/재생 의도를 유지하도록 수정했다. 복구는 한 번, 이후 열기/탐색 시간 초과를 보고한다. Play 후 선택 속도를 적용하고 예약 재생 중인 버튼도 일시 정지로 표시한다.

SCBMPlaybackSlider는 아래 파란 구간 막대와 같은28px 높이,8px 레일,22px 핸들 및 전체 폭을 사용한다. 진행 부분과 단일 재생 헤드는 빨강이다. 캡처 해제와 방향키/Home/End를 구현했다. OS 클릭/드래그는 수행하지 않았으며 버튼과 같은 Control 처리 함수 및 엔진 스크린샷으로 검증했다.

동일 UE5.8.4 CL58902052 Development Editor 빌드는14.01초에 성공했다. 새 에디터에서 stop/play3회, pause/play, 실제 끝 도달 후 replay, 다시 stop/play를 통과했다. 기본 편집 시퀀스와 Quinn/원본 비교창을 PID10908에 열고 사용자에게 인계했다. 독립 QA/새 패키징/배포 ZIP 생성은 하지 않았으며 커밋·푸시도 하지 않았다.

증거: `cubeless/records/verification/quinn-controlrig-playback-20261115/final-verdict.json`.
