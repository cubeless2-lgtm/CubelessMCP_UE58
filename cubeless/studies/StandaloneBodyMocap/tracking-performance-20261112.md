# 인물 추적 및 전체 모캡 성능 점검 — 2026-11-12

## 범위와 재현 조건

사용자 `c++1218` 승인 범위인 CubelessBodyMocap 성능 개선이다. UE 5.8.4 CL58902052, RTX 5060 Laptop 8GB, 같은 UnrealMCP5_8_3 프로젝트에서 실행했다. YouTube `5u6yVQv3Zmk`의 45–60초를 시계 방향 90도 돌린 1080×1920, 15초 영상과 기존 30fps 추출 PNG 450장을 사용했다. 원본 MP4는 60fps이며, 모캡 샘플링 30fps는 이전 결과와 동일하다. 영상 해상도, 추출 프레임, 모델 가중치, 정밀도를 줄이지 않았다.

원본 모델의 SAM2 입력은 고정 1024이다. 이전 실행과 같은 PNG를 다시 사용했으므로 단순한 입력 해상도 차이로 장시간 지연을 설명할 수 없다. 체크포인트 재사용을 끄고 같은 Manny·Retargeter로 측정했다. 기존 추출 PNG를 사용하므로 이번 시간에는 YouTube 다운로드·회전 인코딩·FFmpeg 프레임 추출 시간이 포함되지 않는다. OS·드라이버 캐시, 전원·클록·온도를 고정한 통계 벤치마크는 아니다.

## 확인한 병목과 원인의 한계

과거 실패 실행 `45682923443502ADED2FC39AD2E4AF5A`의 DetectionTracking은 2181.95초, SAM2는 2162.38초였다. 그중 image encoder 871.43초, memory attention 1248.26초가 대부분이다. CPU 전처리·flatten·bank 조립은 각각 한 자릿수 초 수준이었다. 이후 CHMR 시작에서 GPU crash가 발생했다. 초기 GPU 메모리 사용량과 다른 에디터 장면의 렌더링은 경합 가능성을 보여 주지만 메모리 누수나 특정 장면을 단독 원인으로 확정하지 못했다.

같은 입력 32프레임은 정상 세션에서 SAM2 8.0–10.7초였다. 450프레임 전체 처리 직후 원본 attention으로 다시 실행한 32프레임도 SAM2 8.30초로, 이 순서에서는 지속 실행에 따른 급격한 저하가 재현되지 않았다. 당시 `/Game/Study/StockingPull/Maps/L_StockingFollow_Animation`을 다시 열어도 32프레임 SAM2 8.04초로 지연이 재현되지 않았다. 배경 에디터에서는 엔진 자체의 백그라운드 realtime override가 이미 적용되어 있어 장면 렌더링 일시정지의 단독 속도 효과를 이 측정으로 입증할 수 없다.

복구 실행의 445초는 추적·키포인트 캐시를 재사용한 수치이다. 새로운 전체 무캐시 실행과 직접 비교하지 않는다. 과거 36분과 현재 약 2–3분의 추적 차이 전체를 이번 코드 변경의 효과로 주장하지 않는다.

## 구현과 기본 설정

- SAM2 query 4096개를 512행씩 처리한다. 각 블록은 전체 key/value, 최대 7개 공간 메모리와 객체 포인터를 사용한다. FP32, Q/K scaling과 모든 가중치를 보존한다. attention 중간 score tensor의 계산상 상한은 블록당 약 449→56MiB이며, 실제 프로세스 메모리 감소와는 별도 수치이다.
- `cbm.SAM2QueryRows` 기본 512, 값 0은 기존 attention 그래프이다. 고속 모드와 준비된 파생 모델이 모두 있어야 사용한다. 없으면 기존 모델로 동작한다. 원본 애셋은 수정하지 않으며 cache fingerprint를 분리했다.
- `추론 중 에디터 렌더링 일시정지`는 기본 켜짐이다. 게임 스레드에서 표준 realtime override API를 사용하며 뷰포트 포인터를 장기 보관하지 않는다. 완료·실패·취소 시 자체 태그만 제거한다. 새 작업으로 교체하기 전에 이전 worker를 join하고 이전 override를 먼저 해제한다. 결과를 기다리는 비교 창의 skeletal pose/카메라 갱신과 realtime draw도 일시정지한다.
- 별도 fused FP32 attention 후보도 시험했다. CPU 수치 오차는 약 1.6e-6 이하, 32프레임 GPU 추적 박스는 동일했지만 속도 우위가 명확하지 않아 `cbm.SAM2FusedAttention` 기본값 0으로 유지한다. fused의 450프레임 최종 손 보정 품질은 미검증이다.
- 프로파일에 attention 경로·FP32·프레임별 median/p95/max·뷰포트 일시정지 여부를 추가했다. 직접 Extract 실행에 이전 파이프라인의 전체 시간이 잘못 붙는 표시도 수정했다.

## 통제 비교와 전체 품질

| 32프레임, 동일 입력 | SAM2 초 | DetectionTracking 초 | 추적 중 전용 GPU 메모리 표본 최댓값 MiB |
|---|---:|---:|---:|
| 원본 attention, reference-patched | 10.732 | 17.003 | 4382.062 |
| rows512 | 10.421 | 16.596 | 3617.684 |

메모리는 약 17.4% 감소했다. 단일 짧은 구간의 약 3% 시간 차이는 변동 범위에 가까우므로 연산 속도가 뚜렷하게 빨라졌다고 판단하지 않는다. GPU 메모리는 1초 간격 에디터 프로세스 PDH 샘플이므로 순간 피크는 아니다.

첫 450프레임 rows512 실행은 SAM2 152.12초, DetectionTracking 174.64초, 전신+HandFlow 네이티브 처리 666.56초였다. 전체 추적 박스·키포인트·CHMR token hash가 이전 원본 결과와 같았다. 손 보정까지 끝낸 최종 Manny 애니메이션의 161개 트랙 × 450프레임, 총 72,450개 RAW local transform을 비교했다. 전신 123개와 손가락 38개 트랙 모두 위치·회전·스케일 차이가 정확히 0이었다. 이 비교는 기존 결과 보존 검증이며 기존 모캡 자체의 동작 정확도가 정답임을 뜻하지 않는다.

## 실제 창을 포함한 최종 검증

실제 모캡 창을 열고 최종 DLL·기본 rows512·렌더링 일시정지 설정으로 450프레임 전체를 다시 처리했다. 완료 후 표준 AnimSequence로 저장했다.

| 단계 | 실제 초 |
|---|---:|
| 카메라 | 6.63 |
| DetectionTracking (SAM2 포함) | 139.16 |
| SAM2만 (위 단계의 부분집합) | 120.65 |
| 키포인트 | 39.38 |
| CHMR | 221.45 |
| Hue | 8.05 |
| 지면·최적화·본 굽기·영상 정렬·전신 저장·접촉 발 보정 | 약 14.6 |
| 손 배정 키포인트 | 38.54 |
| HandFlow 손 추론 | 137.95 |
| 전신 + 손 네이티브 작업 | 607.22 |
| 본 보정까지 포함한 전체 파이프라인 | 619.0 |

SAM2 프레임 처리 중앙값은 0.2675초, p95 0.2748초, 최대 0.3939초였다. 추적 박스·키포인트·CHMR token·손 배정 keypoint hash가 기존 결과와 모두 같았다. 최종 72,450개 변환값도 모두 차이 0이었다. 최종 애셋은 `/Game/_MCP_Temp/SAMSpeed/CubelessTests/full-final-gui/AN_TrackingAudit_full-final-gui`에 저장했다. 기존 사용자 결과는 덮어쓰지 않았다.

계산 중 비교 창의 `inference_rendering_suspended=true`, `preview_realtime=false`를 확인했다. 정상 완료 후 비교 창 일시정지가 해제됐고, 레벨 뷰포트 상태는 시작 전과 정확히 일치했다. 이어서 실제 창의 설정으로 32프레임 작업을 시작해 Tracking 중 취소했으며, 빈 영상·없는 프레임 폴더의 실패도 실행했다. 두 경로 모두 job busy/자체 realtime override가 해제되고 기존 뷰포트 상태가 복원됐으며 비교 창의 일시정지도 해제됐다.

기본 realtime 자체는 엔진의 백그라운드 정책 때문에 false일 수 있다. 복원 검사는 무조건 true로 만드는 검사가 아니라 시작 전 상태 보존과 모캡 자체 override 제거를 확인하는 검사다. 실제 창의 결과 화면도 확인했다. 빌드·원본/파생 모델 재시작 로드·실행·저장·수치 비교·종료 복원은 검증했으며, 새 PC·다른 영상/인물·패키징·독립 리뷰는 완료로 표시하지 않는다.

최종 요약 증거는 `final-result.json`, `final-source-inventory.json`, `final-animation-comparison.json`, `terminal-restoration.json`, `full-final-gui-preview-during.json`, `full-final-gui-preview-after.json`, `full-final-gui-workflow.png`에 보관했다.

## 재현 및 증거

- 원본 실패/복구: `cubeless/records/verification/youtube-5u6yVQ-20261111/`.
- 측정·CPU 수치 비교·GPU 표본·원시 애니메이션 비교: `cubeless/records/verification/sam-tracking-speed-20261111/`.
- 모델 생성: `Plugins/CubelessBodyMocap/Tools/prepare_sam_attention.py`, 가져오기: `import_sam_attention.py`. 설치 명령은 플러그인 README 참조.
- 같은 프로젝트 Editor 타깃 빌드 성공: `build-final.log`. 모델 원본·파생 모델·가상환경·영상과 원시 기록은 Git에 포함하지 않는다.
- 다른 GPU/드라이버·영상·다중 인물·새 PC 설치·패키징·별도 독립 리뷰는 이번 측정 범위가 아니다. 새 runtime 모듈이나 MCP runtime 의존성을 추가하지 않았다.

검증 종료 시 미저장 패키지·PIE·실행 중 작업·모캡 override가 없음을 확인하고, 검증용 에디터 PID 26796을 정상 창 닫기로 종료했다. 종료 확인은 `cleanup.json`에 기록했다.

후속 CHMR 레이아웃 최적화 후보도 GPU에서 시험했으나 유의미한 가속이 없어 승격하지 않았다. 결과와 복원 기록은 [CHMR 후속 실험](CHMRLayout20261112.md)에 정리했다.
