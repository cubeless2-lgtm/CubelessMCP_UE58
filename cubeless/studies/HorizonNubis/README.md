# Horizon 구름: Zero Dawn에서 Forbidden West·Nubis³까지

**현재 상태: UE 5.8.3에서 공식 VDB/NVDF 기반 학습 구현 사용 가능.** 기존 5.6의 SVT 샘플링 방식을 재사용하고, 발표자의 실제 VDB와 공개 Up-rez 코드로 새 Study를 작성했다. 기존 [2015 절차적 구름 Study](../HorizonClouds2015/README.md)는 그대로 보존했다.

이번 결과는 **Nubis³의 데이터·밀도 계산을 UE 네이티브 렌더러에 연결한 구현**이다. Decima의 SDF 가속 레이 마처, 조명 캐시, 시간 재구성까지 동일하게 구현했다는 뜻은 아니다. 원문과 UE 대체 범위를 아래에서 구분한다.

## 출처와 세대 구분

| 자료 | 원문에서 확인한 내용 | 현재 결과 |
| --- | --- | --- |
| [Zero Dawn, SIGGRAPH 2015](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn) | 높이·날씨·Perlin-Worley, 경계 erosion, Beer/HG/Powder | 기존 `HorizonClouds2015` 맵·문서·Powder 방향 수정 결과를 재사용한다. |
| [Forbidden West: Nubis Evolved, SIGGRAPH 2022](https://www.guerrilla-games.com/read/nubis-evolved) | Vertical Profile·2.5D NDF, 구름 내부의 Envelope Model, 폭풍 VFX | 원문 계약과 2023 방식의 차이를 대조했다. 2022 Envelope·폭풍 전체를 새로 구현한 결과는 아니다. |
| [Burning Shores: Nubis³, SIGGRAPH 2023](https://www.guerrilla-games.com/read/nubis-cubed) | NVDF 기반 복셀 구름, Alligator/Curly-Alligator Up-rez, 근접 보정, SDF 가속 | 공식 VDB 3개를 실제 SVT로 가져와 밀도 경로를 구현했다. 렌더링은 UE가 담당한다. |

PDF 번호는 **첫 페이지를 1로 세는 파일 페이지 번호**이다. 2022 PDF는 207쪽, 2023 PDF는 220쪽이다. 발표 슬라이드에 인쇄된 번호와 다르다.

- 2022: 18~20쪽의 NDF 영역·최소/최대 높이·coverage·상하단 타입, 64쪽의 2.5D 구조, 94~105쪽의 Envelope 밀도, 113쪽의 높이 기반 ambient를 확인했다. 기존 2015 Weather RGB와 새로운 NDF들을 같은 채널 계약으로 취급하지 않았다.
- 2023: 73·82쪽의 Houdini Atlas/Aero 제작, 88~90쪽의 NVDF 및 샘플러, 98~99쪽의 새 노이즈, 100~125쪽의 Up-rez, 163~169쪽의 압축 SDF 가속, 217쪽의 배포 팩을 확인했다. 핵심 도표와 코드는 페이지 렌더로도 대조했다.
- [공식 Voxel Clouds Pack](https://bit.ly/NubisVoxelCloudPack)의 2023-08-10 수정본을 받았다. 원본 TGA slice #128 수정과 샘플러 코드 추가 이력이 포함되어 있다. 원본 `VoxelCloudSampler.cg`와 Readme를 기준으로 구현했다.

원본 식별: 2022 PDF SHA-256 `eacbfc648252718e07b71163c02ff1cc9760d56c91bdb97053181debb50376a6`, 2023 PDF `8b084d192c72cac4c69e29de40ecf633ac396678a0c5fb83178ad7fee193700f`, 원본 팩 `d2939fa47b8865f4dc09da379130bfcba6f9ab1b99ad3e66ca378c8c371ac0de`.

## 기존 VDB 구현을 어떻게 재사용했는가

UE56의 `Docs/Analysis/CubelessClouds_VolumetricCloudFirst_20260913`, `CubelessClouds_VolumetricCloudLayout_20260913`, `CubelessClouds_Authoring_20260914` 기록과 작성 코드를 읽었다. 당시 구현은 **VDB → Static SVT → 월드 좌표의 역변환 → 네이티브 Volumetric Cloud**이며, 11종·28개 배치 및 편집 기능을 제공했다. 그 작업을 없던 것으로 취급한 앞선 설명은 잘못이었다.

이번에는 같은 구조의 좌표 매핑과 native SVT 샘플 노드를 재사용하고, 실제 `AP_Cumulonimbus_Cloud_01`을 분리 프로젝트에서 복제해 UE58 비교 프리셋으로 보존했다. 원본 저장소·SVT는 수정하지 않았다. 32슬롯 편집 서비스·28개 배치 전체를 이번 Study로 이식했다고 주장하지 않는다.

## 실제 에셋과 사용 방법

맵: **`/Game/Study/HorizonNubis/Maps/L_HorizonNubis`**

실제 프로젝트 `Content/Study/HorizonNubis`에는 현재 17개 파일, 41,732,404 bytes가 있다. 기존 맵 1개·마스터 머티리얼 1개·MIC 5개·Static SVT 4개를 보존하고, 아래 선택형 가속 마스터 1개·MIC 5개를 추가했다. 디스크 크기이며 GPU 사용량이 아니다.

1. 현재 작업을 필요한 방식으로 저장한 뒤 학습 맵을 연다. 적용 당시 메인 에디터의 dirty Blueprint를 보존하기 위해 메인 월드를 전환하지 않았다.
2. `Nubis_Camera_Near`를 Pilot하면 기본 근접 시점이다. Ground·Above·Inside 카메라도 있다. 이들은 서로 다른 관측 시점이며 연속 비행 안정성 시험을 대신하지 않는다.
3. `Nubis_Cloud`의 Material에 아래 MIC를 지정해 비교한다. 기본은 `MI_Nubis_Parkouring`이다. 변경 후 저장 여부는 명시적으로 결정한다.
4. `Nubis_Sun`은 UE 네이티브 DirectionalLight이다. 현재 머티리얼의 조명은 UE 직접광·산란 경로를 사용한다.

| MIC 접두사 `MI_Nubis_` | 데이터와 동작 |
| --- | --- |
| Parkouring | 발표자가 공개한 실제 Parkouring Cloud NVDF + 원본 노이즈 + Up-rez |
| Stormbird | 발표자가 공개한 실제 Stormbird Cloud로 소스 교체 |
| ProfileOnly | Parkouring의 `profile × density_scale`. Up-rez 결과와 비교하는 원문의 coarse/profile 경로 |
| NoCloud | DensityMultiplier=0. 빈 밀도 경계 확인 |
| LegacyCumulonimbus | 기존 UE56 VDB 밀도 R을 사용. NVDF의 profile/type/scale 계약으로 오해하지 않도록 LegacyMode=1 |

선택 상태 없이 재질을 바꾸는 공식 MCP 호출 예제이다. `CubelessMCPUE58Editor.CubelessPythonExecutionTools.execute_unreal_python`에 다음 코드와 `interchange_policy="none"`을 전달한다. 저장은 포함하지 않는다.

```python
def choose_nubis_source():
    import unreal
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name() == '/Game/Study/HorizonNubis/Maps/L_HorizonNubis.L_HorizonNubis'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    matches = [a for a in actors if a.get_actor_label() == 'Nubis_Cloud']
    assert len(matches) == 1
    component = matches[0].get_component_by_class(unreal.VolumetricCloudComponent)
    material = unreal.load_asset('/Game/Study/HorizonNubis/Materials/MI_Nubis_Stormbird')
    assert component and material
    component.set_material(material)
    assert component.get_editor_property('material') == material
try:
    choose_nubis_source()
finally:
    del choose_nubis_source
```

## VDB의 실제 입력 계약

엔진 `SparseVolumeTextureFactory.cpp`의 자동 grid 할당과 VDB descriptor 순서를 대조했다. 원본 NVDF에는 `density`라는 이름의 별도 grid가 없으므로, 이 파일에서는 다음 순서가 Attributes A의 RGBA에 대응한다. 다른 VDB에 같은 순서를 가정해서는 안 된다.

| Attributes A | 공식 NVDF | 공식 Noise VDB |
| --- | --- | --- |
| R | dimensional_profile | lf_curly_worley |
| G | detail_type | hf_curly_worley |
| B | density_scale | lf_alligator |
| A | sdf | hf_alligator |

Noise VDB의 처음 두 grid 이름은 `curly_worley`이지만, 제작자 Readme·발표·제공 데이터의 채널 계약은 Curly-Alligator 계열이다. 이름만 보고 기존 2015 Worley 텍스처로 대체하지 않았다. 원본 noise VDB 자체를 사용한다.

NVDF는 UE에서 **512×64×512**, noise는 **128³**로 가져왔다. NVDF는 **Y-up**, UE는 Z-up이므로 월드→UVW 행에서 축을 변환한다. 원문에 나온 512×512×64 표기와 동일한 수의 복셀이지만 축 순서를 그대로 사용할 수 없다. 기존 UE56 SVT는 **343×347×384**이며 해당 프리셋에서 별도 XYZ 매핑을 사용한다.

Parkouring의 전체 필드는 8 m 복셀 기준 4,096×512×4,096 m지만 실제 profile의 활성 bbox는 `(174,20,110)`~`(267,57,203)`이다. 구름 자체가 필드를 가득 채운 4 km 덩어리는 아니다. 최초 원경 캡처에서 작게 보인 것을 import 실패로 판단하지 않고, 원본 metadata와 모델링 TGA로 확인해 근접 카메라를 배치했다.

## 원문 밀도 계산과 UE 연결

```text
World cm → 명시적 world-to-UVW
  → 공식 모델 SVT의 profile/type/scale
  → 공식 노이즈 SVT의 저·고주파 wispy/billowy 혼합
  → 값 erosion → density_scale^4 → 밀도 power
  → 50~150 m HF 보정 + 10~120 m 근접 밀도 fade
  → UE Extinction(/m)·Albedo
  → 네이티브 Volumetric Cloud 시선 적분·조명·history
```

주요 설정:

- 마스터 `M_NubisVDB`: Volume·Additive, VolumetricCloud usage, 표현식 25개, Custom에 저장한 인라인 HLSL. 외부 include·외부 서버·Python 실행기가 게임 렌더링에 필요하지 않다.
- 공식 NVDF MIC의 ExtinctionPerMeter는 **0.02**, 기존 SVT 프리셋은 **0.003**이다. 부모의 기본값은 0.003이다. 0.02는 수백 m 규모의 소스에 선택한 **UE 어댑터의 광학 계수**이며 발표자가 공개한 값이라고 주장하지 않는다. 원본 profile/type/scale·noise 파일은 변경하지 않았다.
- `DensityMultiplier=1`, `DetailBlend=1`, `HFEnabled=1`. ProfileOnly는 DetailBlend=0, NoCloud는 DensityMultiplier=0이다. LegacyMode는 기존 SVT만 1, LegacyCut=0.04이다.
- noise 좌표는 월드 cm→m 변환 후 원문의 0.01 주파수를 적용하고 Y-up으로 바꾼다. 거리 MIP은 `log2(1+abs(distance_m × .0002))`이다. .0002는 자체 선택이다.
- native cloud layer는 바닥 0.05 km·두께 5 km, View Sample Scale 8, 최대 trace 8 km이다. 기본 테스트 카메라는 근접 시점이다. 이는 원본 게임의 조절값이나 성능 설정을 복제한 값이 아니다.
- Albedo `(0.96,0.98,1)`, native PhaseG=-0.55, multi-scattering octave 1, contribution .6, occlusion .4를 사용했다. native ray-marched volume shadow를 켰다. 이 광학 부분은 **UE 대체**이다.
- ConservativeDensity에는 영역 안의 양의 profile에 대한 **존재 마스크**를 전달한다. `profile × scale`을 정밀 밀도의 수치적 상한이라고 가정하지 않았다. 원문의 power 연산은 그 값을 넘을 수 있다.

| 항목 | 이번 구현의 정확한 범위 |
| --- | --- |
| 공개 구름·noise VDB | 원본 실제 파일을 UE OpenVDB factory로 import하여 Static SVT로 저장 |
| 공개 Up-rez sampler | 제공 코드의 밀도 경로를 UE Custom으로 구현. 0 분모에는 좁은 finite guard 적용 |
| 원본 바람 | 이번 비교는 고정. 동적 wind offset·날씨 시뮬레이션은 후속 범위 |
| SDF | 원본 A에 보존. **SDF 기반 adaptive march는 구현하지 않음** |
| BC6 모델·별도 압축 SDF·저비트 노이즈 | 원문의 Decima 저장 형식. 현재는 UE sparse/Float16 경로이며 메모리 비용이 같지 않음 |
| 2023 dark edges·inner glow·light cache | 이번에는 UE native 조명으로 대체. 원본 조명 알고리즘 재현 완료가 아님 |
| 2015 Powder | 기존 Study에서 방향 수정·검증 완료. 이를 2023 머티리얼에 그대로 붙여 Nubis³ 조명이라고 부르지 않음 |
| 게임 성능·품질 | 가까운 화면에 입자감·부드러운 윤곽이 남는다. 목표 GPU 프레임 비용·콘솔 품질 동등성·최종 아트 완성으로 판정하지 않음 |

## 실행 검증과 보존

UE **5.8.3-58210709**, 분리된 소유 프로젝트의 공식 MCP에서 실행했다. 정상 입력으로 실제 SVT·밀도·엔진 렌더링을 거쳤다.

- 원문 8bit 모델링 TGA와 결정적 noise probes 20,000개로 원본 profile·비어 있지 않은 Up-rez·finite/range·profile 0·density scale 0·HF 150 m 연속성의 수치 검사 6개가 통과했다. grid 순서·import 규칙과 원본 보존의 출처 검사 2개도 통과했다(합계 8개). UE는 float VDB를 사용하므로 이 CPU 결과를 GPU 픽셀 동등성으로 표현하지 않는다.
- 마스터 컴파일 오류 0·저장 완료. 공식 모델 2종·ProfileOnly·기존 SVT·NoCloud를 같은 카메라에서 비교했다. 하늘 상단 300행의 NoCloud 대비 RGB 평균 절대 차이는 Parkouring **4.535510**, ProfileOnly **4.256410**, Stormbird **1.483077**, Legacy **9.269026**이었다. Parkouring과 ProfileOnly의 차이는 **0.731270**이었다. 디스플레이 색 차이이며 물리 정확도·성능 점수가 아니다.
- Above·Inside 별도 시점도 렌더링했다. 연속 비행 중 시간적 안정성까지 확인했다는 뜻은 아니다.
- 검증 시 render target mode 3을 사용한 뒤 기존 mode 0으로 복원했다. 기본 화면은 복원된 설정에서도 추가 캡처했다. 검증 에디터는 PIE false·dirty 0이다.
- 저장 의존성 **17개 패키지** 검사 통과. MCP/Registry/Cubeless 모듈 5개가 없는 새 프로세스에서 **10에셋+맵**, 9 Actor, 25개 표현식, source binding·광학 기본값·최종 HLSL hash를 읽어 확인했다. exit 0, null RHI 검사이며 GPU 화면 증거와 구분한다.
- 검증한 11개 파일을 실제 프로젝트의 새 Study 폴더에 적용하고 해시·등록 수를 확인했다. 기존 dirty `BP_StudyCoordinator`는 저장·discard·재시작하지 않았다. UE56 원본과 기존 HorizonClouds2015 에셋도 보존했다.

실패 원본은 그대로 남겼다. 처음 머티리얼을 작성할 때 SVT MIP 입력 이름을 `MipLevel`로 가정해 20개 노드가 생성된 뒤 연결에서 실패했다. 설치된 API의 실제 목록은 `UVs`, `Tex`, **`Level`**이었다. 실제 부분 상태를 읽고 누락된 연결·노드만 완성했으며 원 생성 요청을 재실행하지 않았다. 원 실패를 후속 성공으로 바꾸지 않았다. SVT MI setter도 false를 반환했지만 실제 값은 적용된 것을 readback한 뒤 저장했다.

현재 성공 증거·실패·소스 해시·단계 시간은 로컬 `cubeless/records/operations/horizon-nubis-study-20261026/report.json`에 통합했다. PDF·배포 팩·raw VDB·화면·검증 프로젝트·보조 Python은 Git 제외, 학습 Markdown과 Content/Study 에셋은 Git 포함 대상이다. 이번에는 stage·commit·push를 수행하지 않았다.

최초 구현의 자체 리뷰는 좌표축·grid 순서·단위·원문 수식·부분 실패·기존 에셋 보존·최종 코드 일치를 확인했다. 아래 후속 작업에서는 GPU 구름 패스 시간을 측정했다. 독립 리뷰나 패키징 게임 실행을 수행했다고 주장하지 않는다.

## UE 기본 빈 공간 건너뛰기: 선택형 가속

UE 5.8.3의 실험적 `r.VolumetricCloud.EmptySpaceSkipping`을 사용하는 별도 머티리얼을 추가했다. **Nubis³의 SDF 거리 기반 adaptive march는 아니다.** 원본 SDF 채널과 밀도 코드는 변경하지 않았다. 엔진·C++·플러그인 소스 수정이나 새 런타임 의존성도 없다.

- 마스터: `/Game/Study/HorizonNubis/Materials/M_NubisVDB_EmptySpace`, 표현식 29개.
- MIC: 기존 Parkouring·Stormbird·ProfileOnly·NoCloud·LegacyCumulonimbus 이름에 `_EmptySpace`를 붙인 5개.
- 기존 마스터·MIC·맵·볼륨 11개 파일은 해시가 그대로이며, 맵 기본 재질과 프로젝트 콘솔 설정도 그대로이다.

### 판정 근거와 한계

설치 엔진의 `MaterialExpressionVolumetricAdvancedMaterialInput.h`와 `MaterialExpressions.cpp`에서 `VolumetricCloudEmptySpaceSkippingInput`의 Sphere Center/Radius를 확인했다. `VolumetricCloudMaterialPixelCommon.ush`는 월드 cm 단위 중심과 `0.5 × slice depth × 1.73` 반경을 전달한다. `VolumetricCloud.usf`의 전처리 패스는 slice 중심·모서리를 평가하고, 비어 있지 않은 첫 slice 앞에서 시선 적분을 시작한다. `VolumetricCloudRendering.cpp`에서는 compute 경로와 PF_R16F typed UAV 지원 조건, 기본 비활성 상태를 확인했다.

빈 공간 출력은 전체 VDB UV 영역 `[0,1]³`에 대해 다음과 같이 계산한다. 기존 world-to-UVW 행 U/V/W와 Origin을 그대로 사용한다.

```hlsl
if (DensityMultiplier <= 0) return 0;
float3 d = Center - Origin;
float3 uv = float3(dot(d,U), dot(d,V), dot(d,W));
float3 pad = max(Radius,0.0) * float3(length(U),length(V),length(W)) + 0.00001;
return (all(uv+pad >= 0.0) && all(uv-pad <= 1.0)) ? 1.0 : 0.0;
```

행 벡터 길이로 구 전체의 UV 투영 범위를 넓힌다. 구와 VDB 영역이 겹칠 가능성이 있으면 반드시 1을 반환한다. 점 하나의 profile/SDF가 0이라는 이유로 slice 전체를 건너뛰지 않는다. 이 방식은 VDB 전체 상자 내부의 빈 구멍을 생략하지 않으므로 의도적으로 보수적이다. 소스별 활성 bbox를 임의로 고정하지 않아 현재 프리셋의 소스 교체도 같은 영역 계약을 사용한다. 원본 NVDF의 SDF는 여전히 거리 기반 가속에 사용하지 않는다.

가속 마스터의 ConservativeDensity도 별도 경량 경로로 연결했다. `UVW 내부 && model.R > 0 && DensityMultiplier > 0`의 존재 마스크만 계산하며, 노이즈와 Up-rez 계산을 연결하지 않는다. 실제 밀도 Custom 코드는 기존과 같다. native empty-space compute도 ConservativeDensity를 평가하므로 이 분리는 불필요한 작업을 줄이기 위한 것이다. 기존 마스터는 변경하지 않았다.

### 사용과 복원

1. 학습 맵의 `Nubis_Cloud` 재질에 `MI_Nubis_Parkouring_EmptySpace`를 지정한다.
2. 아래 콘솔 명령을 실행한다. 이번 검증은 원래의 `r.VolumetricRenderTarget.Mode 0`에서 수행했다.

```text
r.VolumetricCloud.EmptySpaceSkipping.VolumeDepth 8
r.VolumetricCloud.EmptySpaceSkipping.StartTracingSliceBias -1
r.VolumetricCloud.EmptySpaceSkipping.SampleCorners 1
r.VolumetricCloud.EmptySpaceSkipping 1
```

VolumeDepth는 km이며 검증 설정은 8 km/64 slice, slice 앞쪽으로 1칸의 여유를 둔다. SampleCorners는 유지한다. `StepSizeOnZeroConservativeDensity`는 1을 유지했으며, 이 값을 크게 만들어 통과시키지 않았다. 다른 해상도·연속 비행·더 큰 영역·다른 플랫폼에 대한 최적 설정을 뜻하지 않는다. UE의 실험적 기능이며 RHI/compute 조건이 필요하다.

콘솔 값은 프로세스 범위이므로 다른 맵에도 영향을 줄 수 있다. 사용 전 기존 값을 확인하고 종료할 때 복원한다. 이번 세션의 원래 값은 아래와 같았다.

```text
r.VolumetricCloud.EmptySpaceSkipping 0
r.VolumetricCloud.EmptySpaceSkipping.VolumeDepth 40
r.VolumetricCloud.EmptySpaceSkipping.StartTracingSliceBias 0
```

기존 재질 `MI_Nubis_Parkouring`으로 돌아가면 원래 Study 경로이다. 프로젝트 전역 설정에 가속을 강제로 활성화하지 않았다.

### 실제 확인과 측정

- 새 마스터 컴파일 오류 0·저장, MIC 5개 저장 완료. 원 컴파일 요청은 클라이언트에서 120초 후 타임아웃됐지만, native 저널에서는 약 330.74초 후 컴파일·저장 완료를 확인했다. 요청을 재실행하지 않았으며 원 타임아웃 기록을 보존했다.
- 근접·지상·상공·내부 시점과 Stormbird·NoCloud를 같은 재질에서 가속 off/on으로 비교했다. 전체 RGB 평균 절대 차이는 0.120~0.471/255였고 구름 전체 누락은 관측되지 않았다. 이것은 제한된 정지 시점의 관측이며 픽셀 동등성이나 모든 비행 경로의 증명이 아니다.
- 마지막 근접 반복의 off/on 차이는 0.151239/255, off/off 시간 반복 차이는 0.154951/255였다. 기존 마스터 대비 경량 존재 마스크 경로 차이는 0.266837/255였다. Legacy의 off/on 차이도 0.212248/255로 확인했다. 시간 재구성·노이즈 때문에 모든 차이를 가속 오차로 단정하지 않았다.
- 보수적 구 판정은 임의 affine 행렬 20,000개와 구 내부 점 160,000개에서 실제 포함 점을 잘못 제외한 사례가 없었다. 구조적 근거는 Cauchy-Schwarz 부등식이며 이 무작위 검사는 보조 증거이다.

GPU CSV의 `GPU/VolumetricCloud`를 측정했다. RTX 5060 Laptop GPU, UE 5.8.3, 에디터 뷰포트 **714×389**, mode 0, view sample scale 8, tracing max 8 km 조건이다. 각 시점에서 off/on/on/off 순서로 96프레임씩 수집하고 앞뒤 8프레임을 제외해 상태마다 160프레임을 집계했다.

| 시점 | Off 구름 패스 평균 | On 구름 패스 평균 | 관측된 감소 |
| --- | --- | --- | --- |
| 지상 | 1.5367 ms | 1.3862 ms | 9.80% |
| 근접 | 2.1383 ms | 1.9204 ms | 10.19% |

이는 구름 패스 시간이며 GPU 전체 프레임 시간·재구성/합성 시간·FPS 향상률이 아니다. 다른 에디터가 실행 중인 노트북에서 측정한 낮은 해상도의 결과이므로 최종 게임이나 다른 해상도 성능으로 일반화하지 않는다. 다른 source/각도에서의 성능 최적화는 이번 범위에 포함하지 않았다.

새 머티리얼·MIC는 MCP 모듈 5개가 없는 새 프로세스에서도 cold load와 바인딩 검사를 통과했다. null RHI 로드 검사와 위 GPU 화면 증거는 별개이다. 패키징 게임 검사는 이번 변경에서 수행하지 않았다. 실제 프로젝트에는 검증한 새 파일 6개만 추가했으며 기존 11개와 사용자의 dirty Blueprint를 보존했다.

테스트 종료 후 원래 재질·콘솔 변수·dirty 0 상태를 확인했다. Python의 map lifecycle 호출은 실행 전 보호 규칙에 의해 거절됐고, 복원 후보의 잘못된 dirty 필수 가정도 변경 전에 거절됐다. 실제 상태를 읽은 뒤 테스트 재질 할당과 콘솔 값만 복원했으며 맵 저장·dirty flag 강제 해제는 하지 않았다. 원 실패와 후속 복원은 기존 `report.json`에 구분해 보존했다.

## 최종 밀도 Custom 코드

아래는 저장 머티리얼과 cold load에서 확인한 인라인 코드이다. 원본 제작자 데이터와 알고리즘의 출처는 위 공식 팩이다. sampler resource binding, 좌표·단위, legacy 분기는 UE 어댑터에서 작성했다. 실제 실행에는 외부 HLSL 파일이 필요하지 않다.

```hlsl
// UE adapter of the original 2023 VoxelCloudSampler.cg. Source pack fields are
// profile/type/scale/SDF in Modeling.rgba; legacy SVTs carry density in r only.
// Renderer, phase/scattering and ray integration are native UE, not Decima.
float inside=(all(UVW>=0.0) && all(UVW<=1.0)) ? 1.0 : 0.0;
float profile=saturate(Modeling.r);
if(inside==0 || profile<=0 || DensityMultiplier<=0) return float4(0,0,0,0);
if(LegacyMode>0.5)
{
    float density=saturate((profile-LegacyCut)/max(1-LegacyCut,0.001));
    return float4(density*DensityMultiplier,1,0,0);
}
float type=saturate(Modeling.g);
float densityScale=saturate(Modeling.b);
float wispy=lerp(Noise.r,Noise.g,profile);
float billowy=lerp(Noise.b*0.3,Noise.a*0.3,pow(profile,0.25));
float composite=lerp(wispy,billowy,type);
if(HFEnabled>0.5)
{
    float hf=saturate(lerp(1-pow(abs(abs(Noise.g*2-1)*2-1),4),pow(abs(abs(Noise.a*2-1)*2-1),2),type));
    float blend=lerp(0.9,1.0,saturate((DistanceMeters-50)/100));
    composite=lerp(hf,composite,blend);
}
float density=saturate((profile-composite)/max(1-composite,0.000001));
float poweredScale=pow(densityScale,4.0);
density*=poweredScale;
density=pow(density,lerp(0.3,0.6,max(0.000001,poweredScale)));
if(HFEnabled>0.5)
{
    float blend=saturate((DistanceMeters-50)/100);
    density=pow(density,lerp(0.5,1.0,blend))*lerp(0.666,1.0,blend);
}
density*=lerp(0.25,1.0,saturate((DistanceMeters-10)/110));
float finalDensity=lerp(profile*densityScale,density,saturate(DetailBlend));
// A binary conservative presence mask is used, not profile*scale as a supposed
// numerical upper bound: the original density power can exceed that product.
return float4(finalDensity*DensityMultiplier,1,0,0);
```
