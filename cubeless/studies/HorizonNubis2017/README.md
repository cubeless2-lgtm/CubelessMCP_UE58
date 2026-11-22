# Nubis 2017 제작 시스템과 2022·2023 확장 비교

2026-11-16, 현재 개발 프로젝트의 **UE 5.8.4-58902052**에서 작성했다. 새 에셋은 `/Game/Study/HorizonNubis2017/`에 있으며 기존 `HorizonClouds2015`와 `HorizonNubis` 에셋은 수정하지 않았다.

2017년의 구름 제작·날씨·조명 원리를 실제 머티리얼로 추가하고, 2022년 Envelope 구름과 단순화한 폭풍 VFX, 기존 2023년 공식 복셀 데이터를 한 학습 순서로 연결했다. 밀도 계산은 인라인 HLSL이며 시선 적분·그림자·시간 재구성은 UE 기능을 사용한다. Decima 렌더러 전체를 복제한 결과가 아니다.

## 검수 순서

Content Browser에서 `Content/Study/HorizonNubis2017/Maps`를 연다. 각 맵에는 해당 실습에 사용할 구름과 카메라가 있다.

| 순서 / 맵 | 확인할 동작 |
| --- | --- |
| `L_01_Fair2017` | 기본 절차적 하늘. 큰 구름 배치는 월드 좌표에 고정되고 내부 노이즈가 바람에 따라 이동한다. |
| `L_02_Regional2017` | 지역 마스크로 좌우 coverage를 다르게 만든다. 같은 노이즈라도 지역별 하늘이 달라지는지 본다. |
| `L_03_WeatherCycle2017` | 120초 주기의 맑음↔흐림 전환. 약 60초에서 가장 흐려진다. 상태 전환은 머티리얼 Time으로 실행된다. |
| `L_04_Sunset2017` | 낮은 태양 고도에서 네이티브 대기·구름 조명의 변화를 본다. |
| `L_05_Envelope2022` | 지형 높이와 독립적으로 최소·최대 높이를 갖는 유한한 구름 덩어리. 분석적으로 만든 둥근 envelope 안에 2022년 밀도 프로파일을 적용했다. |
| `L_06_Superstorm2022` | 높이에 따라 넓어지는 회전 구름과 국소 내부 발광. 형상·회전·발광을 분리한 학습용 VFX이다. |
| `L_07_Voxel2023` | 공식 Parkouring NVDF·노이즈·공개 Up-rez 경로. ProfileOnly와 Stormbird로 데이터·디테일 차이를 비교한다. |

01~06의 Outliner에서 `N17_Camera_Ground`, `N17_Camera_Above`, `N17_Camera_Inside`를 Pilot할 수 있다. 07은 기존 `Nubis_Camera_Near`와 Above·Inside 카메라를 사용한다. 01은 Play에서 네이티브 DefaultPawn으로 이동할 수 있다. 에디터 뷰포트의 Realtime을 켜야 애니메이션이 갱신된다.

## 실제 구현과 원문 근거

원문 PDF의 **파일 첫 페이지를 1로 세는 번호**를 사용한다. 슬라이드에 인쇄된 번호와 차이가 있다.

| 자료 | 검토한 부분 | 이번 구현과 차이 |
| --- | --- | --- |
| [Nubis 2017 공식 발표](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine) | 108쪽 PDF의 밀도·제작·조명 관련 페이지. 29·36·40, 45~59, 67~72, 75~95의 수식·설명과 핵심 도표를 대조했다. | 2015의 노이즈 볼륨을 복제해 재사용하고 2017용 cloud map을 새로 생성했다. 공개 Houdini 노이즈 생성기를 실행한 결과는 아니다. |
| [Nubis Evolved 2022](https://www.guerrilla-games.com/read/nubis-evolved) | 기존 학습 기록에 더해 PDF 94~105쪽의 Envelope Model 수식을 다시 대조했다. | 분석적으로 만든 min/max height와 가장자리 신호를 사용한다. 원작의 NDF 저작 도구와 전체 지형 구름 시스템을 복제하지 않았다. |
| [Nubis³ 2023](https://www.guerrilla-games.com/read/nubis-cubed) | 기존 [공식 VDB/NVDF 구현 기록](../HorizonNubis/README.md)과 공개 샘플러를 재사용한다. | 기존 SVT·머티리얼을 참조하는 새 비교 맵과 MIC를 만들었다. 원본 VDB 필드를 수정하지 않았다. |
| [발표자의 Forbidden West 하늘 사례](https://andrewschneider.artstation.com/projects/LeOOyv) | 작품과 기술 세대의 시각적 참고. | 화면 일치나 원작 품질·성능 동등성을 측정하지 않았다. |

### 2017 제작·밀도

- `T_CloudMap2017`: 512², sRGB off. R은 맑은 상태의 coverage, G는 흐린 상태의 coverage, B는 cloud type이다. 2015의 G=강수 계약과 구분했다. 원문에서 두 coverage 신호를 다루는 아이디어를 학습용 상태 쌍으로 구체화한 자체 선택이다.
- 월드 좌표의 cloud map과 시간에 따라 움직이는 3D 노이즈 좌표를 분리했다. 고도에 따른 wind skew와 상층 anvil inflation을 추가했다.
- 구름 타입은 얇은 층→중간층→높은 적운의 높이 프로파일을 보간한다. Perlin-Worley 몸체와 고주파 Worley erosion에 기존 2015 자체 제작 텍스처를 사용했다.
- coverage map은 매 프레임 생성하는 유체 시뮬레이션이 아니다. `weather_source.py`가 seed 2017로 만든 타일링 fBm 신호를 에셋으로 저장한다.
- `AutoWeather`는 연속 보간을 보여주는 120초 결정적 왕복 주기이다. 시간대·확률·지역 이벤트를 통합하는 원작의 게임플레이 날씨 스케줄러는 구현하지 않았다.

### 2017 조명 비교

`M_Nubis2017`은 발표의 두 HG 함수 비교, 두 Beer 감쇠 경로, 저주파 밀도·높이에 따른 산란 확률을 인라인으로 계산한다. 태양 방향은 SkyAtmosphere index 0에서 받는다. 표본 거리, 색, 높이 경계, 강도, 0분모 방지는 이 스터디의 선택이다. 2015 Powder를 그대로 2017 조명이라고 부르지 않았다.

`M_NubisNativeLighting`은 같은 밀도의 A 출력을 Extinction에 연결하고, Albedo .95·UE 직접광·볼륨 그림자·다중 산란 근사를 사용한다. 기본 01~05 맵은 이 버전이다. Paper RGB 조명 출력은 네이티브 버전에서 Emissive에 연결하지 않아 조명을 중복 적용하지 않는다. 06은 내부 발광을 확인하기 위해 Paper 경로를 쓴다.

| MIC | 비교 용도 |
| --- | --- |
| `MI_Fair` | 기본 네이티브 조명 |
| `MI_Frozen` | 기본 밀도의 고정 시간 비교. `UseTimeOverride=1`, `TimeOverride=0` |
| `MI_NoDetail` | 고주파 erosion 제거. 네이티브 조명 |
| `MI_Paper2017` | 2017 발표 조명 근사의 기준 |
| `MI_NoSilver` | Paper와 동일한 밀도·조명에서 은빛 테두리 항 제거 |
| `MI_NoScatter` | Paper의 확장 감쇠·산란 확률 제거 |
| `MI_Clear` | coverage=0. 빈 밀도 경계 확인 |

01의 `N17_Cloud_Fair` → VolumetricCloud Component → Material에서 MIC를 바꾸어 비교한다. NoSilver/NoScatter는 **MI_Paper2017과** 비교해야 한다. 네이티브 조명인 MI_Fair와의 차이에는 조명 경로 차이도 포함된다. 테스트 뒤 MI_Fair로 돌린다.

### 2022 Envelope와 폭풍

Envelope는 수평 타원 영역에서 변하는 최대 높이를 만들고, 정규화한 높이의 하단·상단 곱, 가장자리 신호, 높이별 wispy/billowy 혼합, 공개 밀도 power .27을 사용한다. max height 모양과 프로파일 정규화 계수는 자체 선택이다. native layer 범위 안에 envelope가 들어가야 한다.

폭풍은 별도의 단순한 회전 프로파일이다. 높이에 따른 반경·노이즈에 따른 외곽 침식·회전 좌표·국소 파란 발광을 사용한다. 2022년 발표의 빠른 구름/VFX 개념을 체험하는 예시이며 원작의 슈퍼스톰 형상이나 번개 볼트, 전용 temporal upscaling 개선을 재현하지 않는다. 회전 구름의 시간 안정성은 UE의 재구성에 의존한다.

### 2023 VDB

이번 절차적·Envelope 구현에는 새 VDB가 필요하지 않아 Blender를 실행하지 않았다. 2023 비교에는 이미 가져온 발표자 공식 Parkouring·Stormbird·Noise SVT를 참조한다. 임의의 웹 VDB를 추가하지 않았다.

새 `MI_VoxelParkouring`, `MI_VoxelProfileOnly`, `MI_VoxelStormbird`는 원본 MIC 복제본이다. 구름을 보기 쉽게 월드 매핑을 3배로 확대하고 extinction을 약 1/3로 줄였다. 근접 fade·거리 MIP 기준은 원래 거리 단위를 유지하므로 완전한 광학적 스케일 불변이라고 주장하지 않는다. 원본 SVT grid와 profile/type/scale 채널은 그대로이다. 07의 `Nubis_Cloud` Material에서 세 MIC를 바꾼다. 최초 로드·소스 교체 직후 SVT 스트리밍으로 잠깐 비어 보일 수 있어 렌더가 안정된 뒤 비교한다.

## 주요 조절값

| 파라미터 | 성분과 단위 |
| --- | --- |
| Layer | 행성 반경 km, 층 바닥 km, 층 두께 km, 카메라 주변 추적 반경 km |
| Scale | base 주기 km, detail 주기 km, 예약, cloud map 주기 km |
| Weather | coverage 배율, R↔G 보간, 지역 마스크 강도, 지역 경계 이동 |
| Shape | 예약, 타입 bias, erosion 강도, anvil bias |
| Wind | XY 이동 km/s, ZW는 높이에 따른 XY skew km |
| WindEnabled | 0이면 노이즈 이동 정지. WeatherCycle 시간은 별도이다. |
| AutoWeather | 0이면 Weather 수동값, 1이면 120초 왕복 보간 |
| UseTimeOverride / TimeOverride | 1이면 Time 대신 지정한 초를 사용. 동일 시점 비교용 |
| Light | extinction /km, Paper 태양 강도, HG g, 확장 Beer 감쇠 혼합 |
| Silver | Paper 은빛 테두리 강도, 퍼짐, 예약, 예약 |
| ScatterProbability | Paper 산란 확률 항 혼합 |
| Storm | 구름 중심 XY km, 내부 발광 강도, 예약 |
| Mode | 0=2017, 1=Envelope, 2=학습용 회전 폭풍 |

Layer의 바닥·두께는 VolumetricCloud Component와 함께 변경한다. 원점 지표·행성 중심 `(0,0,-6360 km)`를 전제로 한다. extinction은 km 기준 계산 후 UE의 meter 적분에 맞춰 .001을 곱한다. ConservativeDensity는 안전하게 1을 사용한다. 이 스터디는 원문 SDF 가속이나 빈 공간 최적화를 구현하지 않는다.

## 실제 검증

현재 프로젝트의 regular Development Editor 하나에서 공식 MCP를 통해 작성했다. 프로젝트·엔진·플러그인 C++ 변경과 별도 테스트 프로젝트 생성은 없다.

- 두 볼륨 마스터의 컴파일·저장 성공, 인라인 코드 readback 일치.
- 01~07 저장·다른 맵 전환·재로드 및 렌더 캡처. 07 최초 캡처는 SVT 준비 전 비어 있었고, 후속 실제 렌더와 소스 비교를 별도로 보존했다.
- 01 PIE 실행에서 네이티브 DefaultPawn과 `MI_Fair` 구름을 확인하고 정상 EndPlay했다. 다른 맵의 PIE와 패키징 게임은 미검증이다.
- 고정 시간의 화면 비교 7개에서 비영(非零) 차이를 확인했다. 화면 차이는 기능이 보이는지 확인한 보조 근거이며 원작과의 정확도·물리적 품질·성능 점수가 아니다.

| 비교 | 하늘 영역 RGB 평균 절대 차이, 8bit |
| --- | ---: |
| 구름 ↔ 빈 밀도 | 37.5771 |
| 바람 t=0 ↔ 60초 | 22.6572 |
| erosion 켜짐 ↔ 꺼짐 | 21.3118 |
| Paper 은빛 테두리 켜짐 ↔ 꺼짐 | 0.9549 |
| Paper 산란 보정 켜짐 ↔ 꺼짐 | 4.5938 |
| 날씨 주기 t=0 ↔ 60초 | 33.5963 |

최종 폭풍 외곽 수정 뒤 발광 켜짐/꺼짐을 별도 재검사했다. 전체 화면 RGB 평균 절대 차이는 2.9341이다. 2023 Parkouring 대비 ProfileOnly/Stormbird의 전체 화면 차이는 각각 .2697/1.7063이다. 비교 간 화면 크기·관측 대상이 달라 위 표와 직접 순위를 매기면 안 된다.

Study 경로의 저장 의존성과 실제 native 클래스 구성을 검사했으며 MCP 참조를 발견하지 않았다. 전체 cook/package 또는 MCP 비활성 게임 실행을 입증한 결과는 아니다. GPU 프레임 시간·메모리·연속 비행 안정성·콘솔 성능은 측정하지 않았다. 원작의 2 ms 수치를 이 구현에 적용하지 않는다. 검증 캡처는 render target mode 3을 사용했고, 인계 시 세션 설정을 mode 0으로 둔다. 프로젝트 Config는 수정하지 않았다.

## 파일과 원시 기록

- 에셋: `Content/Study/HorizonNubis2017/`.
- 최종 밀도·Paper 조명: `Nubis2017.hlsl`. 실제 런타임은 머티리얼에 저장된 인라인 코드를 사용한다. 이 외부 파일이나 Python이 실행 중 필요하지 않다.
- 원문·페이지 텍스트·도표·호출별 원본 결과·이미지: `cubeless/records/operations/nubis2017-20261116/`.
- `visual-verification.json`, `storm-final-comparison.json`, `voxel-comparison.json`, 최종 `final-verdict.json`을 구분해 본다.
- 호출 스크립트는 이번 실행의 이력이며 PID와 단일 사용 라벨을 고정했다. 현재 상태 확인 없이 전체를 재실행하는 설치기로 사용하면 안 된다.

자체 검토만 수행했다. 독립 QA나 원작 개발자의 검토를 받았다고 주장하지 않는다. Git stage·commit·push는 수행하지 않았다.

## 실패와 수정 이력

원래 실패 기록을 유지했다. 호스트의 `inspect.py` 이름이 Python 표준 모듈을 가려 노이즈 생성이 실패했고, 이름을 변경했다. 생성 파일이 없었던 import 결과와 실제 에셋 부재를 확인한 뒤 새 단계로 가져왔다. 첫 맵 전환은 완료됐지만 caller가 None 반환을 출력하는 단계에서 실패했다. 실제 월드를 확인하고 전환을 반복하지 않았다. 첫 맵의 Actor들은 생성됐지만 viewport 함수의 필수 인자 차이로 카메라·저장 단계가 중단됐다. 실제 9 Actor를 확인한 뒤 누락된 카메라 설정·저장만 수행했다. 소스·시간·입력 타입을 포함한 원시 기록은 삭제하지 않았다.
