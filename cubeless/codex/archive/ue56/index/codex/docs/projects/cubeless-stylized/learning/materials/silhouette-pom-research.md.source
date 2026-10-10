# 실루엣 POM 머티리얼 — 1단계 자료조사 (2026-06-12)

## 목표

- `/Game/AI_Generated/Materials/Master` 아래에 실루엣 클리핑을 지원하는 PBR POM 마스터 머티리얼 2종 제작.
  - **A. 기본형**: 핵심 연산(레이마칭 + 실루엣 클립 + 셀프섀도우)을 커스텀 노드 1개에 통합. 파라미터/샘플/루트 연결은 네이티브 노드.
  - **B. 혼합형**: 레이마칭 / 교차 보정 / 셀프섀도우를 별도 커스텀 노드로 분리하고 네이티브 노드로 배선.
- 텍스처 소스: `/Game/AI_Generated/Textures/T_RoundPebbleCobble_*` (1254x1254)
  - `_D` BaseColor (sRGB), `_N` 노멀 (TC_Normalmap), `_H` 하이트 (TC_Grayscale), `_M` 마스크 팩.

## 엔진 내장 POM 분석 결과

분석 대상: `/Engine/Functions/Engine_MaterialFunctions01/Texturing/ParallaxOcclusionMapping` 및 `_BoundedUVs` 변형.

### 코어 알고리즘 (커스텀 노드 'Parallax Occlusion Mapping')

- 선형 탐색 레이마칭: `rayheight`를 1에서 `stepsize`씩 감소시키며 `UVDist`만큼 UV 오프셋 누적.
- 교차 검출 시 직전/현재 스텝 사이를 1회 선형 보간(`xintersect`)으로 보정 — 이진 탐색은 쓰지 않음.
- 출력 float4: `xy` = UV 오프셋, `z` = 교차 높이(`yintersect`, PDO용), `w` = 그림자 항.
- 셀프섀도우: 교차점에서 탄젠트 라이트 벡터를 따라 두 번째 레이마칭. 소프트 섀도우 계수 `k`(Penumbra) 사용.
- **루프 내 텍스처 샘플은 전부 `Tex.SampleGrad(TexSampler, UV+offset, InDDX, InDDY)`** — DDX/DDY를 루프 밖(네이티브 DDX/DDY 노드)에서 계산해 전달. 루프 내 gradient 파탄으로 인한 밉맵 아티팩트 방지. 우리 구현도 동일하게 따른다.

### 실루엣 클리핑 메커니즘 (`_BoundedUVs` 변형에서 확인)

- 레이마칭 중 현재 UV가 `MinMax`(rg=min, ba=max) 경계 밖이면 해당 지점 하이트를 `-1000000` 처리.
- 교차 성공 시 `alpha=1` 반환, 루프 종료까지 교차 실패 시 `return 0` (**alpha=0**).
- 이 alpha를 **Opacity Mask**에 연결하면 그레이징 앵글에서 메쉬 외곽 실루엣이 깎임 → 이것이 실루엣 POM의 핵심.
- 단, `_BoundedUVs`는 `SampleLevel(…, 0)` 고정이라 밉맵이 죽는다. 우리 구현은 SampleGrad + 경계 클립을 결합한다.

### 펑션 셋업(네이티브 노드부)에서 가져올 것

- 스텝 수: 카메라 벡터와 버텍스 노멀의 내적(탄젠트 변환 후)으로 `MinSteps`↔`MaxSteps` 보간(그레이징일수록 많이). `Floor`로 정수화.
- `stepsize = 1 / steps`, `UVDist = TangentViewDir.xy * HeightRatio * stepsize / TangentViewDir.z` 구조 + `Reference Plane` 파라미터로 기준면 오프셋.
- **Pixel Depth Offset**: `(1 - yintersect) * HeightRatio * 텍스처 월드 크기 추정값`을 카메라 방향으로 투영. 월드 크기는 DDX/DDY 기반 자동 추정 또는 `Manual Texture Size`로 지정.
- 입력 파라미터 구성: HeightRatio(권장 0.02~0.1), MinSteps/MaxSteps, ReferencePlane, ShadowSteps, ShadowPenumbra(k), HeightmapChannel(float4 채널 마스크), LightVector.

## 설계 결정

1. **기본형(A)은 커스텀 노드 중심이 맞다.** 레이마칭 루프 + 루프 내 분기(경계 클립, 교차 탈출)는 네이티브 노드로 표현 불가. 단 AGENTS.md 하이브리드 원칙대로 파라미터·텍스처 오브젝트·DDX/DDY·루트 프로퍼티 연결은 네이티브 노드로 유지.
2. **실루엣 클립은 BoundedUVs 방식 채택**: UV 경계 이탈 + 교차 실패 → alpha 0 → Opacity Mask. 블렌드 모드 **Masked** 필수.
3. **샘플링은 SampleGrad 고정** (BoundedUVs의 SampleLevel 0 방식은 채택하지 않음).
4. PDO 출력 연결로 교차 지점의 깊이 보정 → 인터섹션/그림자 정합성 확보.
5. 머티리얼 경로: `/Game/AI_Generated/Materials/Master/M_SilhouettePOM_Custom` (A), `M_SilhouettePOM_Hybrid` (B).

## 재검사 결과 (2026-06-12 2차)

- **밉맵 전부 OFF**: 4장 모두 `TMGS_NO_MIPMAPS`. 원인은 1254×1254 NPOT 해상도. 수정 필요: `Power of Two Mode = Pad to Power of Two`(→2048) + `Mip Gen Settings = FromTextureGroup`. 밉맵 없이는 SampleGrad가 무의미하고 원거리 시머링 발생.
- **`_M`은 ORM 팩이 아님**: R=G=B 동일한 단일 채널이며, `_H`와 상관계수 0.97로 사실상 하이트맵 복제본. Roughness로 배선하면 안 됨. 1차 구현은 스칼라 Roughness 파라미터 사용, 필요 시 `_H` 기반 캐비티 가중만 옵션으로.
- **압축 설정 수정 대상**: `_D` TC_EDITOR_ICON → `TC_Default`(sRGB 유지). `_M`은 배선 제외이므로 수정 불필요(쓰게 되면 TC_Grayscale).
- 검증용 임시 export: `Saved/_mcp_*_check.png` (gitignored, 폐기 가능).

## 주의/리스크

- 커스텀 노드 텍스처 입력은 자동 생성 샘플러 이름이 `<입력명>Sampler` (예: `Tex` → `TexSampler`).
- Masked + PDO 조합은 셰이더 비용 증가. 모바일(ES3.1) 프리뷰에서는 루프 비용 주의 — 엔진 펑션도 FeatureLevelSwitch로 저사양 분기함.
- 하이트맵 `_H`가 TC_Grayscale이므로 HeightmapChannel은 R 채널 (1,0,0,0).
- Virtual Texture는 커스텀 노드 SampleGrad 경로와 호환 제약 있음 — 일반 Texture2D 유지.

---

# 구현 결과 및 작업 기록 (2026-06-12, 전 과정)

> git 커밋 없이 이 문서가 단독 기록으로 남는 것을 전제로, 조사~구현~수정 전 과정을 통합 기록한다.

## 최종 산출물

경로: `/Game/AI_Generated/Materials/Master/` — 단계별 보존, 5종 모두 `compile_error_count=0`.

| 머티리얼 | 구성 | 비고 |
|---|---|---|
| `M_SilhouettePOM_Custom` | 기본형: SampleGrad 레이마칭 + UV 경계 실루엣 클립(교차 실패 시 alpha 0 → Opacity Mask) + PDO | |
| `M_SilhouettePOM_Custom_dither` | + IGN 디더 (레이 시작 지터로 스텝 밴딩 분산) | |
| `M_SilhouettePOM_Custom_dither_shadow` | + 셀프섀도우 (StaticSwitch `SelfShadow`, 기본 ON) | 섀도우 단위 버그 잔존(아래 참조) |
| `M_SilhouettePOM_Custom_dither_shadow_final` | + ReferencePlane | 섀도우 단위 버그 잔존 |
| `M_SilhouettePOM_Custom_dither_shadow_final_fade` | + 그레이징 페이드 + 섀도우 단위 버그 수정 + SkyAtmosphere 태양 추적 + 월드 UV 모드 | **실사용 완성본** |

운용 MI: `MI_SilhouettePOM_WorldUV` (UseWorldUV=ON, ClipMinMax ±100000) — UV가 부정확한 프로토타입 메쉬(SM_Ramp 등)·타일링 바닥용.

## 핵심 HLSL 구조 (완성본 커스텀 노드)

- 메인 루프: `rayheight=1`에서 `stepsize`씩 하강, `Tex.SampleGrad(TexSampler, curUV, InDDX, InDDY)` 샘플, UV가 `MinUV/MaxUV` 밖이면 `texatray=-1e6`. 교차 시 1회 선형 보간 보정 후 `float4(offset, yintersect, 1)` 반환, 루프 종료까지 미교차면 `float4(offset, 0, 0)` (w=0 → 클립).
- 디더: `ign = frac(52.9829189*frac(dot(SvPosition.xy, (0.06711056,0.00583715))))`, 레이 시작을 `ign*DitherStrength` 스텝만큼 전진.
- 셀프섀도우(Additional Output `Shadow`): 교차점에서 라이트 방향 2차 마치. **한 스텝 전진 후 판정**(엔진 원본의 첫 반복 0/0 NaN 제거). 높이 상승은 `TLV.z / max(HeightRatioIn,0.001) * lightstepsize` (단위 보정).

## 네이티브 그래프 공식 (엔진 펑션 T3D 역공학으로 확보)

- `CamT = TransformVector(World→Tangent, CameraVectorWS)`
- `steps = floor(lerp(MaxSteps, MinSteps, clamp(abs(dot(CamWS, 판정노멀)))))`, `stepsize = 1/steps`
- `UVDist = HeightRatio_faded × (CamT.xy × -1 / CamT.z) × stepsize`
- ReferencePlane: 시작 UV와 최종 샘플 UV에 `HeightRatio_faded × rayDir × -(1-RefPlane)` 시프트 (기본 1.0 = 표면 아래로 파임)
- 그레이징 페이드: `HeightRatio_faded = HeightRatio × lerp(GrazingFadeFloor, 1, saturate((|dot|-Start)/(End-Start)))`
- PDO: `length(float3(offset.xy, (1-yintersect)×HeightRatio_faded)) × 스케일` (스케일: 기본 TextureWorldSize, 월드UV 모드 시 WorldUVSize 자동 연동)
- 월드UV 모드: `UV = AbsoluteWorldPosition.xy / WorldUVSize`, 카메라·라이트 탄젠트 변환 생략, 페이드 판정노멀 = (0,0,1)
- 태양 추적: `SkyAtmosphereLightDirection[0] × -1` (UseSkyAtmosphereSun ON), OFF 시 LightDirectionWS 파라미터 폴백

## 파라미터 기본값 (완성본)

Tiling 1.0 / HeightRatio 0.05 / MinSteps 12 / MaxSteps 32 / TextureWorldSize 100 / Roughness 0.85 / DitherStrength 0.35 / ShadowSteps 16 / ShadowPenumbra 4 / ReferencePlane 1.0 / GrazingFadeStart 0.05 / GrazingFadeEnd 0.35 / GrazingFadeFloor 0.25 / WorldUVSize 400 / ClipMinMax (0,0,1,1) / LightDirectionWS (-0.474,0.321,0.82) / SelfShadow ON / UseSkyAtmosphereSun ON / UseWorldUV OFF

## 수정 내역 (시간순)

1. **텍스처 정비**: `_D` TC_EDITOR_ICON→TC_Default. `_D/_N/_H` NPOT(1254²)→Stretch to PowerOfTwo(2048²)+밉맵 활성화. 처음 Pad로 했다가 죽은 테두리 영역 문제로 Stretch 전환. `_M`은 R=G=B 단일채널·`_H`와 상관 0.97(사실상 하이트맵 사본)이라 배선 제외, Roughness는 스칼라 파라미터.
2. **기본형 구현**: 커스텀 노드 1개 + 네이티브 셋업. VectorParameter 기본 출력이 float3라 `.a` 스위즐 컴파일 에러 → R/G/B/A 개별 핀을 AppendVector로 묶어 MinUV/MaxUV float2 2개로 전달.
3. **디더 변형**: IGN 지터. 커스텀 노드 inputs 배열은 기존 CustomInput 구조체 유지한 채 append하면 연결 보존됨.
4. **셀프섀도우 변형**: 엔진 섀도우 루프 이식 + Additional Output. 엔진 원본의 첫 반복 0/0 NaN을 "한 스텝 전진 후 판정"으로 제거.
5. **검은 영역 오진 사건**: 그레이징에서 면이 검게 나온 원인을 PDO×다이내믹 섀도우로 오진(기본값 0으로 변경)했으나, 실제 원인은 **검증 카메라가 SM_Ramp11 메쉬 내부에 위치**한 것(라인 트레이스로 확정). PDO 기본값 100으로 복원. 교훈: 뷰포트 검증 카메라는 배치 전 라인 트레이스로 지오메트리 내부 여부 확인.
6. **ReferencePlane 추가** (`_final`): 엔진 배선 그대로, 기본 1.0.
7. **시차 검수 (유저 제보)**: (a) 탑다운 줄무늬 = 디더 산란 — DitherStrength 1.0→0.35, MinSteps 8→12. (b) 어두움 = LightDirectionWS (0.3,0.3,0.9)가 UDS 태양 (-0.474,0.321,0.82)와 X부호 반대 — 씬 태양값으로 교정.
8. **그레이징 페이드** (`_final_fade`): 레퍼런스(0.1~0.9)보다 좁은 구간(0.05~0.35) + Floor 0.25로 실루엣 클리핑 보존. 트레이드오프: 깊이가 시야각 함수가 되어 카메라 회전 시 미세 스위밍.
9. **셀프섀도우 단위 버그 수정 (유저 지적)**: 섀도우 마치가 높이 스팬(0~1)을 UV 1.0 거리로 취급 → 라이트가 1/HeightRatio(20배) 수평으로 계산돼 경사 시야에서 면 전체 검정. 그레이징 페이드가 평지에서 버그를 가려 각도 의존처럼 보였음. 커스텀 노드에 HeightRatioIn(페이드본) 추가, `TLV.z/max(HeightRatioIn,0.001)` 보정. **`_dither_shadow`/`_final`에는 이 버그가 남아 있음 — 셀프섀도우 실사용은 `_final_fade`만.**
10. **SkyAtmosphere 태양 추적 (유저 제안)**: `SkyAtmosphereLightDirection[0]×-1`, 기본 ON. SkyAtmosphere 태양 없는 씬은 스위치 OFF로 LightDirectionWS 폴백.
11. **월드 UV 모드 (유저 요청)**: UseWorldUV 스위치. UV·카메라·라이트·PDO 스케일·페이드 판정노멀 일괄 전환. 한계: 월드 XY 투영이라 가파른 경사면은 텍스처 늘어남(트라이플래너 아님) — 페이드 노멀을 월드 Z로 전환해 경사면은 점잖게 플랫으로 강등. 비수광면의 어두움은 씬 라이팅(스카이라이트) 영역.

## 메쉬 전제조건 (SM_Ramp11 사례)

같은 머티리얼이라도 메쉬가 다음을 깨면 결과가 다름: (1) 비균일 액터 스케일(예: 4,4,2)은 탄젠트 변환을 왜곡, (2) UV 밀도 불균일은 HeightRatio/PDO 스케일을 어긋나게 함, (3) UV가 0~1 한 장이 아니면 ClipMinMax 클립이 면을 뜯음. 해결: 월드UV 모드(+클립 확장) 또는 메쉬 UV/스케일 정비.

## 검증 스크린샷 (Saved/Screenshots/WindowsEditor/, gitignored)

`pom_test4`(기본형 그레이징), `pom_dither_close`(디더), `pom_final_check`(섀도우+RefPlane), `pom_fix_verify`(디더 산란 수정), `pom_fade_grazing`(페이드), `pom_shadowfix_steep`(섀도우 단위 수정), `pom_worlduv_sunlit`(월드UV 램프).

## 운영 메모

- 에디터가 백그라운드면 HighResShot이 큐에만 쌓임 — 포커스 필요.
- 고해상도 스크린샷은 `defer_to_ticker` + 뷰포트 invalidate 필요.
- Python `unreal.Rotator(a,b,c)` 인자 순서는 (roll, pitch, yaw).
- `_MCP_Temp/Materials/M_POM_Debug_MCP`는 인메모리 참조로 삭제 보류(폐기 대상, 에디터 재시작 후 삭제).
- 테스트 액터 `MCP_POM_Test_Plane`(지상 830,170,30)은 검증용 — 정리 가능.
