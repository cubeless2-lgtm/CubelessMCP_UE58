# SIGGRAPH 2015 구름 원리의 UE 5.8 구현

**상태: UE 5.8.3에서 사용 가능.** Schneider/Vos의 2015 발표에 나온 저고도 구름의 밀도·날씨·조명 원리를 네이티브 Volumetric Cloud 머티리얼로 구현했다. 노이즈 생성과 공개되지 않은 계수는 자체 선택이며 Guerrilla의 비공개 셰이더를 그대로 복제한 결과는 아니다. 원문 분석과 페이지별 근거는 [README](README.md)에 있다.

## 바로 사용하기

1. 현재 편집 중인 작업을 사용자가 필요한 방식으로 저장한 다음 `/Game/Study/HorizonClouds2015/Maps/L_Horizon2015`를 연다. 이번 적용에서는 기존 dirty Blueprint를 보존하기 위해 실제 프로젝트의 월드를 전환하지 않았다.
2. `HC2015_Camera_Ground`를 Pilot하면 지상 30 m에서 구름층을 볼 수 있다. `HC2015_Camera_Above`는 고도 5 km에서 내려다본다. 뷰포트는 Lit·Perspective·Realtime으로 확인한다.
3. `HC2015_Cloud`의 Material을 아래 MIC로 바꾸어 비교한다. 기본값은 `MI_HC2015_Cumulus`이다. 테스트 후 원복하거나 맵 저장 여부를 명시적으로 결정한다.
4. `HC2015_Sun`의 회전을 바꾸면 Atmosphere Sun Light index 0의 방향이 머티리얼에 자동 반영된다. 별도의 SunDirection 수동 파라미터는 없다.

| MIC 이름 접두사 `MI_HC2015_` | 용도 |
| --- | --- |
| Cumulus | 기본 높이 분포 |
| Stratus | 낮고 얇은 높이 분포 |
| Nimbus | 위로 발달한 높이 분포 |
| NoDetail | 경계 erosion을 제거한 큰 몸체 비교 |
| NoCloud | coverage 0의 빈 밀도 비교 |
| NoPowder | powder 혼합을 제거한 조명 비교 |

예를 들어 공식 MCP의 `CubelessMCPUE58Editor.CubelessPythonExecutionTools.execute_unreal_python`에 아래 코드를 전달하면, 이 학습 맵에 있는 명시적 이름의 구름만 비교용 재질로 바꾼다. 선택 상태를 사용하지 않으며 저장하지 않는다. `interchange_policy`는 `none`으로 둔다.

```python
def change_owned_cloud():
    import unreal
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name() == '/Game/Study/HorizonClouds2015/Maps/L_Horizon2015.L_Horizon2015'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    matches = [a for a in actors if a.get_actor_label() == 'HC2015_Cloud']
    assert len(matches) == 1
    component = matches[0].get_component_by_class(unreal.VolumetricCloudComponent)
    material = unreal.load_asset('/Game/Study/HorizonClouds2015/Materials/MI_HC2015_NoDetail')
    assert component and material
    component.set_material(material)
    assert component.get_editor_property('material') == material
try:
    change_owned_cloud()
finally:
    del change_owned_cloud
```

## 저장된 구성

`Content/Study/HorizonClouds2015`에 **맵 1개, 머티리얼 1개, MIC 6개, 텍스처 6개**를 저장했다. 전체 14개 파일은 29,530,854 bytes이다. 이는 디스크 에셋 크기이며 GPU 메모리 측정값이 아니다.

- `Textures/VT_base`: 128³ RGBA. R은 자체 Perlin-Worley, GBA는 주파수가 증가하는 반전 Worley이다.
- `Textures/VT_detail`: 32³ RGBA. RGB가 주파수별 반전 Worley이며 A는 패딩 1이다.
- `Textures/T_Curl`, `T_Weather`: 각각 128². Weather는 R coverage, G precipitation, B cloud type 계약을 유지한다.
- `T_base_Atlas`, `T_detail_Atlas`: Volume Texture 제작에 사용한 내장 원본 시트이다. 외부 raw 파일이나 Python 생성기가 있어야 렌더링되는 구조가 아니다.
- `M_Horizon2015`: Volume·Additive, Volumetric Cloud usage 활성, 표현식 26개. HLSL은 Custom 노드 안에 저장되며 외부 include 경로가 없다.
- `L_Horizon2015`: SkyAtmosphere, 태양, VolumetricCloud, 지면, 수동 노출 PostProcessVolume, 카메라 2개로 구성했다.

### 파라미터와 단위

| 파라미터 | 저장된 기본/프리셋 값 | 의미 |
| --- | --- | --- |
| Layer | `(6360, 1.5, 2.5, 35)` | 행성 반경 km, 구름 바닥 고도 km, 층 두께 km, 카메라 주변 수평 영역 반경 km |
| Scale | `(12, 2, 6, 70)` | base/detail/curl/weather 주기 km |
| Shape | 기본 Cumulus MIC `(.70, 0, .35, .10)` | coverage 배율, cloud type bias, erosion 강도, curl 변위 km |
| LightParams | `(4, 14, .55, 1)` | extinction /km, 태양 조명 배율, HG g, powder 혼합 |
| SunColor / AmbientColor | `(1, .95, .85)` / `(.45, .60, .85)` | 자체 조명 색·세기 조절 |
| Wind | `(.015, .003, 0)` km/s | base/detail 이동 방향·속도 |
| WindMultiplier | `0` | 비교 화면 재현을 위한 정지. 1이면 이동한다. |
| RainAbsorption | `0` | 기본 강수 흡수 비활성. 양수이면 날씨 G가 태양 광학 깊이를 증가시킨다. |

부모 머티리얼의 Shape.x 기본값은 1이지만 **배포한 MIC는 모두 .70을 명시**했다. NoCloud만 0이다. Stratus/Nimbus는 Shape.y=-1/+1, NoDetail은 Shape.z=0, NoPowder는 LightParams.w=0이다. NoPowder의 차이는 각도에 의존하므로 모든 시점에서 동일한 차이가 나타나지는 않는다.

구름 Actor는 바닥 1.5 km·두께 2.5 km이며 머티리얼의 Layer 값과 함께 변경해야 한다. SkyAtmosphere 반경도 6360 km이다. 이번 모델은 지표 기준 원점 `(0,0,0)`, 행성 중심 `(0,0,-6360 km)`를 전제로 한다. 다른 행성 중심·고도 기준에 그대로 옮기는 범용 설정은 아니다. Scale은 양수, Layer 두께는 양수, 영역 반경은 15 km보다 크게 유지한다.

## 원문과 UE의 역할 분리

| 기능 | 이번 구현 | 구분 |
| --- | --- | --- |
| 큰 몸체·높이·coverage | Perlin-Worley → 높이 프리셋 → coverage remap → 바닥 fade | 원문 순서에 맞춘 자체 수치 구현 |
| 작은 경계 | 고주파 Worley erosion, 바닥 부근 반전, curl로 detail 좌표 변형 | RX CoreGrowth/core-preservation/body warp는 넣지 않았다. |
| 날씨 | RGB 계약, 15→35 km 원경 coverage/type 전환 | 정적인 자체 날씨 텍스처. 발표의 날씨 시뮬레이션·강수 기반 타입 전환 전체는 후속 범위이다. |
| 빛 | 5개 cone+1개 먼 샘플, Beer·HG·시선 의존 powder | 정확한 샘플 거리·각도·혼합 계수는 자체 선택이다. |
| 시선 적분·빈 영역·조기 종료 | 네이티브 Volumetric Cloud, ConservativeDensity 상한 제공 | 발표의 64→128 스텝·되돌림·누적 alpha .3 전환 코드를 복제한 것은 아니다. |
| 시간 처리 | UE Volumetric Render Target의 재구성·history | 발표자의 원래 재투영 거부·필터 코드와 동일하다고 주장하지 않는다. |
| 고고도 얇은 구름 | 이번 저고도 볼륨과 별도 | 원문의 2D 고고도 층은 후속 범위이다. |

**머티리얼 연결:** PaperDensityLighting은 RGB를 Emissive, A를 Extinction에 연결한다. Python MaterialProperty에서는 Volume Extinction의 연결 대상이 `MP_SUBSURFACE_COLOR`이다. Base Color/Albedo는 0으로 두어 자체 조명과 엔진 직접광의 중복을 피했다. Advanced Output은 multi-scattering octave 0, volume shadow ray march false이며, ConservativeDensity에는 detail을 생략한 밀도 상한을 넣었다. 이는 완전한 물리 다중 산란 모델이 아니다.

Custom 입력은 WorldPos·CameraPos·TimeSeconds, BaseTex·DetailTex·CurlTex·WeatherTex, Layer·Scale·Shape·LightParams, SunDirection·SunColor·AmbientColor·Wind, WindMultiplier·RainAbsorption이다. Layer/Scale/Shape/LightParams는 VectorParameter RGB 출력과 A 출력을 AppendVector로 합쳐 float4로 전달한다. SunDirection은 `MaterialExpressionSkyAtmosphereLightDirection`, index 0이다.

월드 좌표 cm는 `*0.00001`로 km로 바꾼다. 엔진 `VolumetricCloud.usf`는 `dtMeters`로 적분하므로 `/km` extinction은 `*0.001`로 `/m`로 바꾼다. `/100000`으로 줄이면 단위가 맞지 않는다.

## 실제 확인 결과

UE **5.8.3-58210709**의 분리된 소유 프로젝트에서 공식 MCP 8002를 통해 작성·컴파일·저장·캡처했다. 검증된 저장 파일 14개를 실제 프로젝트에 복사하고 각 파일의 해시 일치와 Asset Registry의 14개 등록을 확인했다. 메인 에디터의 기존 `BP_StudyCoordinator` dirty 상태는 그대로 유지했다.

| 확인 항목 | 실제 결과와 범위 |
| --- | --- |
| 수식 | 기존 Beer/Powder CPU 참조 7개 재사용 |
| 밀도 | 145,152개 CPU 표본의 유한성·범위·상한·기본 비어 있지 않음·coverage 0·층 외부 0·erosion·높이 차이 관련 7개 검사 통과. 원본 16bit 입력 계산이며 GPU 8bit와 픽셀 단위 동등성을 주장하지 않는다. |
| 수치 안전성 | sun/view 0벡터 CPU 조합 4개 통과. 두 Custom 코드에 scale 하한 반영, 주 조명 코드에 0벡터·비음수 extinction 보호 후 엔진 재컴파일·저장·코드 readback 통과 |
| GPU 화면 | 지상·상공 렌더, Cumulus/Stratus/Nimbus/NoDetail/NoCloud/NoPowder 화면 비교 통과. native sun 회전 -25→-45도 후 수동 머티리얼 수정 없이 변화 확인 |
| 저장·재로드 | 다른 소유 맵으로 전환 후 재로드, 7 Actor·기본 재질·층 설정 유지 |
| 의존성 | 저장 패키지 20개 참조 검사 통과. MCP/Registry/Cubeless 모듈 5개를 로드하지 않은 별도 새 프로세스에서 최종 에셋 13개+맵 로드, 그래프·인라인 HLSL 확인, exit 0 |
| 실제 프로젝트 | 최종 파일 14개 동일 적용·등록 확인. 사용자 dirty 작업을 보존하기 위해 메인 월드의 구름 맵 전환·재시작은 하지 않았다. |

최초 구현(Powder 방향 수정 전)의 고정 시점 화면에서, 하늘 상단 280행의 Cumulus 대비 RGB 평균 절대 차이는 NoCloud 47.907, Stratus 13.136, Nimbus 12.832, NoDetail 6.976, NoPowder 13.581이었다(8bit 값). 변형이 실제 화면에 반영된 역사적 보조 근거이며 Powder 방향의 정확성을 입증하지 못했다. 현재 방향 검증은 아래 수정 기록을 따른다. 물리적 coverage·정확도·성능 점수로 사용하지 않는다. 비교 시 `r.VolumetricRenderTarget.Mode=3`을 사용한 뒤 원래 값 0으로 복원했다. 최종 캡처는 복원된 설정이다.

노이즈 입력 생성은 이 PC에서 15.40초, 최종 보호 코드의 엔진 컴파일·저장 호출은 73.16초였다. 둘 다 GPU 프레임 시간이 아니다. 발표의 약 2 ms·약 20 MB를 이번 구현 성능으로 인용하지 않는다. MCP 없는 cold load는 null RHI 검사이고, GPU 렌더는 별도의 authoring 에디터에서 확인했다. 패키징 게임 실행·대형 해상도 성능·다중 광원·구름 내부 비행·전체 날씨 시스템은 후속 범위이다.

## Powder 시선 방향 리뷰 수정

2026-10-26 리뷰에서 원문 PDF 68~69쪽과 반대인 각도 가중치를 발견해 수정했다. `viewDirection`은 카메라→샘플, `lightDirection`은 샘플→태양이므로 `mu=1`은 태양을 바라보는 쪽이다. **HG는 기존 mu를 유지하고 Powder에만 `powderViewCosine=-mu`를 적용**했다. 태양을 등질수록 효과가 강해진다는 발표 노트의 설명과 일치시켰다. smoothstep의 -.2/.8 경계는 자체 선택이며 발표자의 공개 계수라고 주장하지 않는다.

- 태양 방향 가중치 0·반대 방향 1, 단조성·유한성·범위, NoPowder 항등성, HG·밀도 보존과 변경 범위를 포함한 CPU 참조 검사 8개가 통과했다.
- 분리된 UE 5.8.3 공식 MCP에서 재컴파일·저장 후, pitch 15°·FOV 60°·정지 바람을 고정하고 yaw 180°(태양 쪽)/0°(반대쪽)의 Powder 켜짐·꺼짐 화면 4장을 비교했다. 하늘 상단 280행의 RGB 평균 절대 차이는 태양 쪽 **0.167800**, 반대쪽 **5.222731**이었다. 반대쪽의 켜짐−꺼짐 평균은 **-5.211626**으로, 의도한 어두운 경계 효과가 그쪽에서 나타났다. 태양 쪽의 작은 래스터 잔차는 남으므로 픽셀 단위 동일성이라고 표현하지 않는다.
- 검증 뒤 기본 Cumulus·카메라·렌더 모드를 복원했고 분리 에디터는 PIE false·dirty 0이었다. 실제 프로젝트는 파일을 외부에서 덮어쓰지 않고 공식 MCP의 머티리얼 API를 통해 같은 코드를 적용·컴파일·저장했다. 양쪽 컴파일 오류는 0이었다.
- 실제 프로젝트의 저장 에셋을 MCP 모듈이 없는 새 프로세스에서 다시 로드해 수정 코드·HG·26개 노드·13에셋+맵을 확인했다(exit 0, null RHI). 이것은 GPU 화면 증거와 별개의 저장 지속성 검사이다. 실제 프로젝트와 cold-load 복사본의 14파일이 일치하며, 수정 대상 머티리얼 외 13파일과 사용자 dirty Blueprint는 보존했다.

이전 실패·화면·해시는 기존 보고서에 보존했고 현재 상태는 `powder_fix`의 후속 증거를 사용한다. 최종 주 조명 코드 SHA-256은 `326a91e3a269de6a47484e059f7f3209aa6f9edd5b8055b2180eceaccb9cdf4c`이다. 이번 변경은 머티리얼 셰이더 수정이므로 프로젝트/플러그인 C++ 빌드는 필요하지 않았다. 패키징·GPU 성능에 대한 추가 완료 주장은 없다.

## 실패에서 확인한 UE 5.8 주의점

- VectorParameter의 기본 출력은 RGB이다. float4 Custom에 곧바로 연결하면 형 변환 오류가 난다. RGBA를 명시적으로 조합한 뒤 컴파일했다.
- 일반 Material 컴파일 통과만으로 Volumetric Cloud 전용 usage가 보장되지 않는다. 첫 화면의 실제 오류를 보고 usage를 켜고 전용 셰이더를 컴파일했다. 새 제작 코드는 `set_base_material_usage`를 우선한다.
- 설치된 `SetMaterialInstanceVectorParameterValue`는 값을 적용하고도 false를 반환하는 구현이었다. 해당 API만 소스와 실제 값 readback을 근거로 판정했고, Stratus의 이미 적용된 값은 재실행하지 않고 저장 상태를 정리했다.
- 연결되지 않은 입력의 조회는 빈 배열 대신 `[None]`일 수 있다. 슬롯 구조와 연결 여부를 분리해 검사했다.
- `unreal.Rotator` 위치 인자에 기대한 pitch를 넣었으나 실제 roll로 관측된 호출을 수정했다. 저장된 태양·카메라는 `pitch=`, `yaw=`, `roll=` 키워드를 사용한다.
- generic Python 안의 새 맵 생성은 실제 월드를 바꾼 후 wrapper가 상태 변화로 실패를 반환했다. 원 실패는 유지하고 실제 파일·월드를 확인한 뒤 별도 작업으로 이어갔다. 월드 전환은 공식 SceneTools 호출과 UObject 지역 참조의 해제를 분리한다.
- 전송 전 Python 구문·호스트 context-manager 오류도 기록상 실패와 구분했다. 엔진 변경이 없던 오류를 에셋 작업 실패나 자동 재시도로 확대하지 않았다.

원시 성공·실패, 컴파일·저장 영수증, 화면, 최종 파일 목록은 로컬 `cubeless/records/operations/horizon-clouds-2015-study-20261026/ue58-implementation.json`과 같은 폴더의 결과에 보존했다. 로컬 Python은 단계별 이력이며 단일 무인 재실행용 생성기로 보장하지 않는다. Git 대상은 학습 Markdown과 저장 에셋이고, 원문 PDF·검증 프로젝트·이미지·보조 Python·캐시는 제외한다. Git stage·commit·push는 수행하지 않았다.

## 최종 주 조명 Custom 코드

아래는 실제 저장 에셋에서 readback한 코드와 SHA-256이 일치하는 자체 학습 구현이다. 텍스처·파라미터·연결 계약은 위 설명을 따른다. `ConservativeBase`는 동일 struct의 density를 `detail=false`로 호출해 상한을 R에 반환한다. 에셋을 사용하는 데 이 Markdown이나 별도 HLSL 파일을 로드할 필요는 없다.

```hlsl
// Study implementation of the 2015 presentation. Undisclosed coefficients are
// explicitly our choices; UE handles view-ray integration and temporal history.
struct HC2015
{
    float density(float3 pKm, float3 camKm, float tm,
        Texture3D baseTex, SamplerState baseS, Texture3D detailTex, SamplerState detailS,
        Texture2D curlTex, SamplerState curlS, Texture2D weatherTex, SamplerState weatherS,
        float4 layer, float4 scale, float4 shape, float3 wind, bool detail)
    {
        scale=max(scale,0.001);
        float radialHeight = length(pKm - float3(0,0,-layer.x)) - layer.x;
        float h = (radialHeight-layer.y)/max(layer.z,0.01);
        float distanceKm = length(pKm.xy-camKm.xy);
        if(h<=0 || h>=1 || distanceKm>=layer.w || shape.x<=0) return 0;
        float3 weather = weatherTex.SampleLevel(weatherS,pKm.xy/scale.w+0.5,0).rgb;
        float horizon=smoothstep(15.0,layer.w,distanceKm);
        weather.r=lerp(weather.r,0.5,horizon);
        weather.b=lerp(weather.b,0.5,horizon);
        float coverage=saturate(weather.r*shape.x);
        if(coverage<=0.0001) return 0;
        float type=saturate(weather.b+shape.y);
        float stratus=smoothstep(0.0,0.08,h)*(1-smoothstep(0.16,0.30,h));
        float cumulus=smoothstep(0.0,0.15,h)*(1-smoothstep(0.55,0.90,h));
        float nimbus=smoothstep(0.0,0.10,h)*(1-smoothstep(0.80,1.0,h));
        float profile=type<0.5 ? lerp(stratus,cumulus,type*2) : lerp(cumulus,nimbus,type*2-1);
        float3 q=pKm+tm*wind;
        float4 n=baseTex.SampleLevel(baseS,q/scale.x,0);
        float worley=dot(n.gba,float3(0.625,0.25,0.125));
        float threshold=(1-worley)*0.35;
        float base=saturate((n.r-threshold)/max(1-threshold,0.001))*profile;
        base=saturate((base-(1-coverage))/max(coverage,0.001))*coverage;
        base*=smoothstep(0.0,0.08,h);
        if(!detail || base<=0) return base;
        float3 curl=curlTex.SampleLevel(curlS,q.xy/scale.z,0).rgb*2-1;
        float3 d=detailTex.SampleLevel(detailS,(q+curl*shape.w)/scale.y,0).rgb;
        float erosion=dot(d,float3(0.625,0.25,0.125));
        erosion=lerp(1-erosion,erosion,smoothstep(0.0,0.20,h));
        erosion=saturate(erosion*shape.z*0.35);
        return saturate((base-erosion)/max(1-erosion,0.001));
    }
};
HC2015 cloud;
float3 pKm=WorldPos*0.00001;
float3 cameraKm=CameraPos*0.00001;
float tm=TimeSeconds*WindMultiplier;
float rho=cloud.density(pKm,cameraKm,tm,BaseTex,BaseTexSampler,DetailTex,DetailTexSampler,
    CurlTex,CurlTexSampler,WeatherTex,WeatherTexSampler,Layer,Scale,Shape,Wind.xyz,true);
if(rho<=0) return float4(0,0,0,0);
float sunLengthSquared=dot(SunDirection.xyz,SunDirection.xyz);
float sunPresent=sunLengthSquared>0.00000001 ? 1.0 : 0.0;
float3 lightDirection=sunPresent>0 ? SunDirection.xyz*rsqrt(sunLengthSquared) : float3(0,0,1);
float3 axis=abs(lightDirection.z)>0.95 ? float3(0,1,0) : float3(0,0,1);
float3 tangent=normalize(cross(lightDirection,axis));
float3 bitangent=cross(lightDirection,tangent);
float distances[6]={0.15,0.35,0.65,1.05,1.70,5.0};
float opticalDepth=0;
float previous=0;
[unroll] for(int i=0;i<6;i++)
{
    float t=distances[i];
    float angle=i*2.39996323;
    float radius=i<5 ? t*0.18 : 0;
    float3 sampleKm=pKm+lightDirection*t+radius*(cos(angle)*tangent+sin(angle)*bitangent);
    float d=cloud.density(sampleKm,cameraKm,tm,BaseTex,BaseTexSampler,DetailTex,DetailTexSampler,
        CurlTex,CurlTexSampler,WeatherTex,WeatherTexSampler,Layer,Scale,Shape,Wind.xyz,true);
    opticalDepth+=d*(t-previous);
    previous=t;
}
float rain=WeatherTex.SampleLevel(WeatherTexSampler,pKm.xy/max(Scale.w,0.001)+0.5,0).g;
opticalDepth*=max(LightParams.x,0)*(1+max(RainAbsorption,0)*rain);
float3 viewDelta=pKm-cameraKm;
float3 viewDirection=viewDelta*rsqrt(max(dot(viewDelta,viewDelta),0.00000001));
float mu=dot(viewDirection,lightDirection);
float g=clamp(LightParams.z,-0.95,0.95);
float hg=(1-g*g)/(12.5663706144*pow(max(1+g*g-2*g*mu,0.001),1.5));
float beer=exp(-opticalDepth);
float powder=1-exp(-2*opticalDepth);
// mu uses camera-to-sample versus sample-to-sun. Powder grows looking away
// from the sun (2015 presentation PDF pages 68-69); HG keeps its original mu.
float powderViewCosine=-mu;
float viewWeight=smoothstep(-0.2,0.8,powderViewCosine);
float response=beer*lerp(1,powder,saturate(LightParams.w*viewWeight));
float h=saturate((length(pKm-float3(0,0,-Layer.x))-Layer.x-Layer.y)/max(Layer.z,0.01));
float3 radiance=AmbientColor.rgb*lerp(0.55,1.1,h)+SunColor.rgb*(sunPresent*max(LightParams.y,0)*hg*response);
// VolumetricCloud.usf integrates dtMeters: extinction and emissive per meter.
float sigma=rho*max(LightParams.x,0)*0.001;
return float4(radiance*sigma,sigma);
```
