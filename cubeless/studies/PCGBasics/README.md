# PCG 기본 실습 — UE 5.8.3

실제 프로젝트의 `/Game/Study/PCGBasics/Maps/L_PCGBasics`를 엽니다. 맵과 그래프는 저장했고, 다른 맵으로 이동한 뒤 다시 로드해 배치가 유지되는 것을 확인했습니다. 필수 엔진 플러그인은 PCG입니다. MCP는 편집·관측에 사용했으며, 저장 에셋은 기본 엔진/PCG 타입만 사용합니다. 새 게임 C++나 MCP 런타임 타입은 추가하지 않았습니다.

## 01 스플라인과 묶음 복사

그래프: `/Game/Study/PCGBasics/Graphs/PCG_01_SplineAssembly`

`Get Spline Data → Spline Sampler → Copy Points → Transform Points → Static Mesh Spawner`

Copy Points의 Source에는 원점 주변 Y=-120, 0, 120인 모티프 3개를 넣고, Target에는 Actor 소유 스플라인의 샘플 9개를 넣었습니다. 실제 Copy 출력과 메시 인스턴스가 모두 27개입니다. 모티프는 샘플의 위치·회전을 따라 복사됩니다. 스플라인은 높이 변화가 있는 네 제어점으로 구성했습니다.

학습: `Study_01_SplineAssembly`의 `StudySpline` 제어점을 움직이거나 Samples 노드의 `SamplerParams.NumSamples`를 바꾼 뒤 Generate합니다. 모티프 수가 같으면 결과는 샘플 수 × 3입니다. 이 예제는 native Copy Points의 로컬 묶음 복사이며, 5.6의 별도 Parent/Depth 메타데이터·커스텀 PCG Blueprint 노드 전체를 재현한 것은 아닙니다.

## 02 밀도와 겹침 제거

그래프: `/Game/Study/PCGBasics/Graphs/PCG_02_DensityPruning`

`Create Points Grid → Attribute Noise($Density) → Filter Attribute Elements by Range → Bounds Modifier → Self Pruning → Transform Points → Static Mesh Spawner`

| 단계 | 실제 결과 |
|---|---:|
| 120cm 간격의 10×8 그리드 | 80개 |
| Seed 1337, 밀도 0~1 노이즈 | 80개 |
| 밀도 0.35~1 필터 | 47개 |
| XY ±85cm 바운드의 Self Pruning | 17개 |
| 밀도 하한을 0.8로 변경한 최종 배치 | 9개 |
| 하한을 0.35로 복원 | 원래 17개 위치 복원 |

학습: Density 노드의 `MinThreshold.AttributeTypes.DoubleValue`를 0.35에서 0.8로 바꿉니다. Bounds 노드의 크기는 메시의 표시 크기와 별개인 PCG 제외 영역입니다. 실제 잔존 포인트의 XY 바운드가 서로 겹치지 않는지 검사했습니다. Self Pruning은 임계값별 최적 개수를 보장하는 알고리즘으로 해석하지 않습니다.

## 03 표면 투영과 경사 필터

그래프: `/Game/Study/PCGBasics/Graphs/PCG_03_SurfaceProjection`

`Create Points Grid → Projection → Filter(ImpactNormal.Z) → Transform Points → Static Mesh Spawner`

Projection Target에는 `World Ray Hit Query`를 연결합니다. 쿼리는 `PCGBasicsTerrain` 태그가 있는 두 메시 지형만 검사하고, PCG가 만든 메시의 충돌은 제외합니다. 지형은 roll 방향으로 기울어진 15°·45° 기본 큐브이며, 표면 높이와 노멀을 계산해 대조할 수 있습니다.

| 단계/조건 | 실제 결과 |
|---|---:|
| 표면 위 그리드 후보 | 80개 |
| 실제 메시 충돌면에 투영된 포인트 | 70개 |
| `ImpactNormal.Z >= 0.85` (약 31.79° 이하) | 37개 |
| 하한 0.65로 완화 | 70개 |
| 하한 0.99로 강화 | 0개, 기존 인스턴스 제거 확인 |
| 하한 0.85 복원 | 원래 37개 위치 복원 |

80개 모두 충돌면에 닿는 구성은 아닙니다. 유한한 두 지형의 범위·회전·틈으로 인해 70개가 투영되며, 나머지를 생성 실패로 집계하지 않습니다. 출력 포인트를 실제 Actor transform의 역변환으로 대조했으며 표면의 로컬 Z=50과 최대 약 0.000045cm 차이였습니다. 45° 표면 노멀은 투영 출력에 있고, 기본 경사 필터 뒤에는 15° 표면 노멀만 남습니다.

학습: Slope 노드의 `MinThreshold.AttributeTypes.DoubleValue`를 0.85 → 0.65 → 0.99 → 0.85로 바꿉니다. 정상적인 빈 결과도 완료이며 이전 인스턴스가 남지 않아야 합니다. 이 예제는 메시 지형의 월드 표면 투영입니다. Landscape 높이맵·레이어·경사 샘플링, World Partition, GPU 경로는 이번 실습에서 실행하지 않았습니다.

## 실행 결과와 주의점

- 공식 MCP의 PCGToolset으로 실제 그래프 생성·연결·실행을 수행했고, Cubeless typed spawner 구성과 출력/배치 검사로 관측했습니다.
- 세 그래프 모두 같은 입력 재생성에서 월드 위치·개수 일치 및 중복 없음이 확인됐습니다. 저장 후 맵 전환·재로드에서도 위치와 개수가 유지됐습니다. 새 에디터 프로세스 또는 패키징 실행 검증은 이번에 수행하지 않았습니다.
- 저장된 맵·그래프의 재귀 Asset Registry 의존성 15개를 검사해 검사 대상 MCP/Python/EditorScripting 참조가 없었습니다. 이는 패키징/실행 검증과 구분합니다.
- UpdateNode는 에디터 자동 재생성을 시작할 수 있습니다. 이미 실행 중이면 상태만 관측하고, 완료 전에 추가 Generate를 보내지 않습니다. 노드별 Data View는 처음 사용할 때 inspection을 켜므로 그 뒤의 성공한 생성 데이터가 필요합니다.
- 일반 Python 실행 래퍼는 실행 중 월드가 바뀌면 보호 실패로 보고합니다. 최초 맵 생성 스크립트는 실제 생성·저장 뒤 이 보호에 걸렸고, 별도 현재 상태 확인으로 작업을 이어갔습니다. 원 요청을 재실행하거나 성공으로 바꾸지 않았습니다. 이후 맵 전환은 전용 공식 SceneTools로 수행했습니다.
- 스터디 호출 코드의 reflected 결과 디코딩과 자동 재생성 대기, 뷰포트 함수 인자 오류를 수리했습니다. 원 실패와 실제 상태 확인은 보고서에 보존했습니다. 이 결과를 플러그인 API 결함이나 전체 PCG 지원 완료로 확대하지 않습니다.
- 뷰포트의 cached lighting/sky capture 경고는 이번 PCG 배치 검수와 별개입니다. 이 작은 학습 맵의 최종 조명·렌더링 품질은 검수 대상이 아닙니다.

원시 입력·응답·실패·시간·검사 결과는 프로젝트의 `cubeless/records/operations/pcg-basics-study-20261025/report.json`에 있습니다. 공식 Generate 14회 합계는 이 실행에서 약 10.86초였으며, 전체 제작 시간이나 일반 PCG 성능 벤치마크가 아닙니다.

`study.py`, `scene.py`, `appearance.py`는 이번 제작 과정의 코드입니다. 고정된 소유 경로가 이미 있거나 단계가 한 번 실행됐으면 중단합니다. 기존 맵에 일괄 재실행하지 마십시오. 실패 이후에는 원시 결과와 현재 상태를 확인하고 아직 실행되지 않은 단계부터 명시적으로 이어갑니다.

## 학습 출처

원본 UE 5.6 저장소의 `Docs/Analysis/ElectricDreams/` 중 다음 자료를 참조했습니다. 원본은 수정하지 않았으며 과거 성공을 5.8 성공으로 취급하지 않았습니다.

- `electric_dreams_pcg_learning_plan.md`: SplineExample → 계층 복사 → 필터·지형 학습 순서.
- `build_spline_assembly_with_post_copy_offset.py`: 스플라인 대상·소스 모티프·밀도/역할 기반 필터의 구성 원리.
- `electric_dreams_pcg_cubeless_regression_runner_pass.md`: 중간 포인트/ISM 검수와 다음 tick의 출력 materialization 경험.

UE 5.8.3의 실제 노드 스키마 및 설치 엔진의 `PCGToolset.cpp`, `PCGCreatePointsGrid.cpp`, `PCGSelfPruning.h`, `PCGWorldData.cpp`를 대조했습니다. 이 실습은 native 표준 노드로 기본 원리를 학습한 범위이며 Electric Dreams 전체 그래프나 원본 스크립트의 전체 이식이 아닙니다.
