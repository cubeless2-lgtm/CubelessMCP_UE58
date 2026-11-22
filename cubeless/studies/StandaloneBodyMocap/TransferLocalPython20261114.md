# PC별 로컬 Python 설치로 배포 변경 — 2026-11-14

사용자 요청에 따라 Python/Torch/venv를 배포하지 않는다. Release에는 plugin/body/hands/manny 4개 ZIP, 설치기, 요구사항·설치 문서만 포함한다. 기존 runtime ZIP은 같은 작업의 History/before-local-python 아래에 보존했고 현재 작업 PC의 환경은 변경하지 않았다.

압축 약 9.24 GB, 플러그인/모델 설치 약 10.26 GB이다. 실제 총 설치 용량에는 각 PC에서 설치하는 Python/Torch와 pip 캐시가 추가된다. 기존 약 8 GB 런타임을 제외한 값이므로 최종 PC 디스크 사용량이 그만큼 줄어든다는 의미는 아니다. 모델팩 설치기 여유 공간 기준은 약 23.03 GB이며 Python 설치 공간은 별도다.

빌더는 Python 실행 파일/표준 라이브러리/site-packages를 수집하지 않으며 runtime 구성 요소 선택을 거절한다. 설치기는 기본적으로 runtime을 선택하거나 요구하지 않는다. 이전 --keep-runtime 옵션은 호환성을 위한 무동작 옵션으로만 남겼다. TRANSFER.md에서 각 PC의 Python3.12.10/Git 및 README 6절의 Torch CUDA/공통 요구사항/overlay/Seaborn/Chumpy 설치를 안내한다. 원본 Markerless 설치와 모델 재다운로드는 요구하지 않는다.

전신·손·Manny ZIP 영수증은 이전 배포와 정확히 같고 모든 팩에 venv/site-packages/python.exe/wheel이 없음을 확인했다. 현재 C++305개 및 보호된 계산/도구328개를 비교했으며 기존 prepare_content 변경 외 계산 변경은 없다. 최종 source ZIP384개 파일 복원과 해시 검증, 전체 배포 문서/설치기 체크섬 검증을 완료했다. 실제 프로젝트의 PowerShell 설치전 검사에서 runtime ZIP 또는 -KeepRuntime 없이 body/hands를 확인했다. 별도 Unreal 프로젝트와 Editor를 만들지 않았다.

새 PC의 pip 신규 설치/실제 추론은 이번 변경에서 실행하지 않았다. 기존 배포의 실제 추론 검증은 해당 기록의 환경 범위에서 유효하며 이번 변경은 묶음/설치 안내만 바꾼다. 커밋·푸시는 하지 않았다.

현재 안내: Plugins/CubelessBodyMocap/TRANSFER.md
증거: cubeless/records/verification/mocap-transfer-local-python-20261114/final-verdict.json
이전 추론 증거: cubeless/records/verification/mocap-transfer-20261114/final-verdict.json
