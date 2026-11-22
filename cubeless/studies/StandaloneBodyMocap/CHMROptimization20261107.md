# CHMR 최적화 후보의 구현과 실제 영상 비교

> 후속 조사: 이 문서는 최초 비교 당시의 기록입니다. 이후 고정 입력 비교로 카메라 모델의 무시드 난수가 교란 요인임을 확인했습니다. 고속 모드 기본값과 현재 검증 결과는 [손목 안정화 조사](WristStability20261107.md)를 참조합니다. 아래 27.09°를 CHMR 통합만의 효과로 해석하지 않습니다.
## 판정

고속 Attention 후보를 구현하고 동일 영상의 전체 재측정을 완료했다. 전체 시간은 423.17초 → 300.91초로 28.9% 단축, CHMR은 280.11초 → 145.31초로 1.93배였다. 다만 일부 손목 회전의 큰 차이 때문에 **품질 동등성 기준으로 기본 적용하지 않았다**. `Fast CHMR (Experimental)`을 켜면 시험할 수 있고, 기본값은 원본 계산 경로다. 정상 종료·저장은 확인했으나 품질 유지가 완료된 최적화라는 판정은 하지 않는다.

## 병목과 변경

원본 모델 8프레임의 ORT 추적에서 CHMR Backbone model_run 합계 7.285825초, Head 0.080983초였다. 주요 비용은 Backbone 추론이며 초기화나 이미지 전처리만 줄여서는 충분하지 않았다.

원본 모델은 이미 FP16/FP32 혼합 정밀도였다. 가중치·정밀도·896×896 입력·4097개 토큰·16개 헤드·모든 300프레임·Q/K의 기존 FP16 사전 스케일링을 유지한 채, 24개 블록의 attention MatMul/Softmax/MatMul을 `com.microsoft.MultiHeadAttention`으로 교체했다. Head, Hue 50회, 보정기, 리타기터는 변경하지 않았다. 통합 커널의 중간 계산 반올림이 원본과 달라질 수 있다.

[ONNX Runtime 연산 스키마](https://github.com/microsoft/onnxruntime/blob/main/docs/ContribOperators.md#com.microsoft.MultiHeadAttention)와 [Microsoft DirectML 구현](https://github.com/microsoft/onnxruntime/blob/main/onnxruntime/core/providers/dml/DmlExecutionProvider/src/Operators/DmlOperatorMultiHeadAttention.cpp)을 참고했다. 실제 지원 여부는 설치된 UE 5.8.4 NNERuntimeORTDml 실행으로 확인했다.

네이티브 변경은 선택 옵션, 실제 사용 모델 기록, 모델별 캐시 분리(v3), CHMR 세부 시간 측정, CHMR RunSync 실패/비정상 수치 검사다. 기존 비동기 작업 구조와 UObject 수명 규칙은 유지했다. 최적화 모델/DirectML 런타임이 없으면 원본 모델을 선택한다. 준비된 세션 생성 실패를 자동 재시도하지 않는다.

## 동일 입력의 속도와 메모리

- 입력: `C:/Users/Lenovo/Desktop/dance01.mp4`, 0~10초, 30FPS, 발 고정, Manny, 기존 전용 리타기터.
- 추론 체크포인트 사용/쓰기 모두 끔. 기존과 새 PNG 300장 SHA-256 전부 일치. 300/300 추적 및 저장 완료.
- GPU: RTX 5060 Laptop, Windows, UE 5.8.4 CL58902052. 동일 프로젝트. 별도 테스트 에디터를 병렬 실행하지 않았다.
- 현재 세션/OS/드라이버 캐시가 준비된 조건이며 통제된 cold boot 비교는 아니다. 각각 1회의 전체 측정이며 다른 단계도 실행 상태에 따라 변동한다.

| 항목 | 원본 모드의 직전 측정 | 고속 후보 |
| --- | ---: | ---: |
| 전체 | 423.17초 | 300.91초 |
| CHMR 전체 | 280.11초 | 145.31초 |
| CHMR Backbone만 | 별도 기록 없음 | 138.26초 |
| CHMR 전처리 / Head | 별도 기록 없음 | 0.82초 / 3.09초 |
| 검출·추적 | 98.30초 | 103.92초 |
| 에디터 물리 RAM 피크 | 11.66GiB | 11.83GiB |
| 에디터 RAM 완료 후 표본 | 4.54GiB | 4.71GiB |
| PID별 GPU 전용 표본 피크 | 4.68GiB | 4.86GiB |
| PID별 GPU 공유 표본 피크 | 2.57GiB | 2.59GiB |
| 메인 스레드 API 최대 왕복 | 0.69초 | 0.80초 |
| Slate 최대 틱 공백 | 1.23초 | 1.26초 |

메모리 피크 감소는 확인되지 않았다. 공유 GPU 메모리를 프로세스 RAM에 단순히 더하지 않는다. 고속 후보의 API 60회 중앙값은 0.10초였다. Slate 틱 간격은 배경 제한과 다른 에디터 작업도 포함하며 직접 클릭 반응 시간이나 플러그인 단독 프레임 시간은 아니다. 수분간의 메인 스레드 정지는 관측되지 않았지만 순간 지연은 남아 있다. 완료 후 추론 모델 0개, 미저장 패키지 없음.

## 품질 비교와 적용 제한

저장된 두 Animation Sequence를 RAW 평가, 루트 모션 추출 없이 300개 프레임의 161개 본에서 비교했다. 회전은 정규화한 쿼터니언의 부호 불변 각도 차이, 위치는 컴포넌트 좌표 cm이다. 주요 몸 관절은 손목을 포함한 25개이며, 전체 161개에는 손가락과 보조 본이 포함된다. 이 비교는 이전 출력과의 차이이며, 영상 정답에 대한 정확도 측정이 아니다.

| 차이 지표 | 평균 | 99백분위 | 최대 |
| --- | ---: | ---: | ---: |
| 전체 161개 본의 로컬 회전 | 0.165° | 1.626° | 27.093° |
| 주요 25개 본의 로컬 회전 | 0.240° | 0.825° | 27.093° |
| 주요 25개 본의 위치 | 0.519cm | 1.158cm | 1.406cm |
| 전체 본의 위치 | 0.569cm | 1.445cm | 6.938cm |

왼손 손목 차이: 프레임 267에서 27.09°, 268에서 18.40°, 269에서 13.92°. 오른손 손목 최대 차이: 프레임 267에서 9.46°. 손목 위치 차이는 작아도 손가락 끝 위치는 크게 달라질 수 있다. 이 최초 비교에서는 Backbone의 계산 차이와 카메라 조건 변화의 영향을 분리하지 못했다. 후속 통제 비교에서 기존 증폭 원인 추정을 수정했다. 원본 또는 후보 중 어느 쪽이 실제 영상에 더 정확한지는 시각적/정답 비교 없이 판정할 수 없다. 따라서 품질 우선으로 원본 모드를 기본 유지한다.

## 추가 후보와 재현

별도 Python 도구에서 동일 난수 입력으로 원본 모델 세션을 재생성한 출력은 완전히 일치했다. 통합 후보의 상대 L2 차이는 약0.00332, cosine 약0.9999945였다. query 512개 단위로 attention을 분할한 후보는 속도가 개선되지 않았고 출력 차이도 감소하지 않아 제외했다. 앞/뒤 12개 블록만 통합한 후보는 약0.67~0.72초/프레임으로 전체 통합의 약0.46초보다 느렸고, 상대 L2 차이도 각각 약0.00336/0.00297로 충분히 감소하지 않아 전체 영상 시험 대상으로 승격하지 않았다. 이 단독 입력 시험을 영상 품질 검증으로 취급하지 않는다.

`Plugins/CubelessBodyMocap/Tools/prepare_chmr_fused.py`는 검토한 원본 SHA-256에만 적용되고 기존 초기화 텐서 바이트가 유지되는지 검증한다. 이 도구가 생성한 ONNX와 실제 시험한 ONNX의 SHA-256 일치를 확인했다. `import_chmr_fused.py`는 기존 에셋을 덮어쓰지 않고 같은 프로젝트에 설치한다. 변환 도구는 실행 의존성이 아니다. 원본 모델 에셋과 이전 사용자 결과는 보존했다. MetaHuman 플러그인 의존성이나 프로젝트 등록을 추가하지 않았다.

## 근거와 남은 검증

- 수치 요약: `Plugins/CubelessBodyMocap/CHMR_BENCHMARK.json`
- 모델 지문과 재현: `Plugins/CubelessBodyMocap/CHMR_FUSED_PROVENANCE.json`
- 전체 측정: `cubeless/records/verification/cbm-full10-chmr-fused-20261107.json`
- 전 본/프레임 수치 비교: `cubeless/records/verification/cbm-chmr-full10-quality-20261107.json`
- native profile: `Saved/CubelessBodyMocap/F09D66904D593C275A1D509EC6BDF581/profile.json`
- 최종 빌드: `Saved/Logs/CubelessBodyMocap_CHMRFinalBuild.log` (성공)
- 테스트 출력: `/Game/_MCP_Temp/BodyMocap/CubelessTests/CHMRFusedFull10_20261107/Take_F09D66904D593C275A1D509EC6BDF581/Animation/Body_F09D66904D593C275A1D509EC6BDF581`

시각적 비교, 다른 영상/캐릭터/GPU, 패키징, 독립 C++ 검수는 미실행/대기다. 최종 기본값 변경은 계산 코드를 변경하지 않았으며 재빌드·로드 후 꺼진 기본값을 확인했다. 같은 프로젝트 에디터(PID10176)와 Cubeless Body Mocap 창을 열고 원본 영상 경로를 복원했으며, 작업 Idle·미저장 패키지 없음·프로세스 응답을 확인했다. 원본과 품질이 동등하면서 더 빠른 기본 경로는 아직 확보하지 못했다.
