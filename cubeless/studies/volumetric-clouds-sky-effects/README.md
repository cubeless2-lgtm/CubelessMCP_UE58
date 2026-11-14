# Volumetric Clouds & Sky Effects 학습 프로젝트

상태: **강좌별 핵심 기능 재현 및 에디터 실행 검증 완료.** 현재 프로젝트 UE 5.8.4의 `/Game/Study/VolumetricCloudsSkyEffects`에서 작업한다. 아래는 최신 교정 상태이며, 과거 수치와 가설은 [교정 이력](CORRECTION_HISTORY_20261114.md) 및 [초기 조사 기록](INVESTIGATION_HISTORY.md)에 보존했다.

사용자는 2026-11-14에 “완전히 똑같지 않아도 된다”, “비슷하게만 해”라고 범위를 명확히 했다. 따라서 원본의 정확한 좌표·환경 설정 및 픽셀 단위 일치는 완료 조건이 아니다. 아래에 기록한 형태·색감·배치 차이를 유지하며, 핵심 효과와 강좌별 단계 재현을 완료 범위로 삼는다. 패키징 및 MCP 플러그인 비활성화 독립 실행은 미검증이며 완료 주장에 포함하지 않는다.

## 실행과 강좌 순서

콘텐츠 브라우저에서 아래 맵을 순서대로 열고 Play한다. GameModeBase/DefaultPawn과 PlayerStart가 저장되어 있다. 재생 화면을 클릭한 뒤 WASD/마우스로 이동·회전하며 Esc로 종료한다.

| 강 | 원본 영상 | 맵 | 주요 구현 |
|---|---|---|---|
| 1 | [Clouds Base Shape](https://www.youtube.com/watch?v=paO-Its5P2Q) | L_01_CloudBase | M_01_CloudBaseNative, 기본 볼륨 밀도 |
| 2 | [Cloud noise details](https://www.youtube.com/watch?v=ND1A-9n6e0Y) | L_02_CloudErosion | M_02_CloudErosionNative, 침식 |
| 3 | [Storm flowmap](https://www.youtube.com/watch?v=Zy4GGO7-9ig) | L_03_StormFlow | M_03_StormFlowNative, 폭풍 흐름/개구부 |
| 4 | [Storm eye 01](https://www.youtube.com/watch?v=x36BCa_Rotw) | L_04_EyeBase | HalfSphere, M_04_EyeBase, 디더링 |
| 5 | [Storm eye 02](https://www.youtube.com/watch?v=zg2tYGTznho) | L_05_EyeFlowParallax | M_05_EyeFlowParallax, 흐름/시차 |
| 6 | [Storm eye 03](https://www.youtube.com/watch?v=ypfoC--ABmM) | L_06_StormEye | M_06_EyeStarsAndCore, 별/코어/대기광 |
| 7 | [Improving Sky & Clouds](https://www.youtube.com/watch?v=GwOln8Ckdio) | L_07_StormSky | MI_07_StormEye, MI_07_StormLighting, M_07_SkyDome |

[플레이리스트](https://www.youtube.com/playlist?list=PLIxJB8bm2Y11ZX8oCMZQxtX4Nx_0kmwj3), [저자 배포 자료](https://drive.google.com/file/d/1XNi98Ierc0EkV0dtUwjteLoWXsUvOSCs/view).
저자의 storm_flowmap, stars, HalfSphere, Custom_volume_flowmap을 사용했다. T_SoftSmoke/T_TipNoise는 엔진 텍스처 복제본이다. 콘텐츠는 맵 7개, 함수 2개, 머티리얼 9개, 인스턴스 9개, 소스 5개로 구성된다.

## 현재 그래프와 근거

- **최신 교정:** 1강1:36/1:46/1:56 HD 설정 패널의 `Multi Scattering Approximation Octaves=1`, Ray March Volume Shadow=true, Ground Contribution=false, Gray Scale Material=false, Per Sample Phase Evaluation=false를 직접 확인했다. 초기 진단에서 남은 Octaves=2를 M_01/M_02/M_03/M_07 볼륨 머티리얼에서1로 교정했다. 1강은 직접 근거이며 후속 강좌는 같은 기본 노드를 이어 쓴다는 추정이다. 나머지 네 옵션은 이미 일치했다. 긴 컴파일·저장 중 클라이언트가 timeout을 반환했으나 변경을 재실행하지 않고 에디터 로그와 별도 읽기로 네 머티리얼 모두1, 미저장 패키지 없음 및 정상 실행을 확인했다. 1/2/3/7강 레벨 재로드·PIE 실제 카메라·화면을 확인했다. 3강에서는 구름의 밝고 어두운 부분이 더 구분된다. 4~6강도 이후 각각 레벨 재로드·PIE 실제 카메라와 화면을 확인했다. 4강 개구부, 5강 푸른 흐름 반구, 6강 별·코어는 계속 표시되며 주변 구름의 어두운 부분이 구분된다. 따라서 산란 횟수 교정 후 1~7강 개별 실행 확인을 모두 마쳤다. 이는 원본과의 외관 차이가 모두 해소됐다는 뜻은 아니다.

- 볼륨 높이 프로파일은 `1-(1-exp(h))*(1-exp(1-h))`. HeightMaskBias는 Multiply이며, 밀도는 `(noiseG-profile*bias)*CloudDensity/100`에서 침식을 반영한다.
- 1강 약12:04 HD 최종 인스턴스에서 Cloud density=3.0, Height Mask Bias=1.52, Cloud Speed=0.02, Cloud Base Tiling=65를 직접 확인했다. MI_01_CloudBase의 밀도10을3으로 교정하고 저장·레벨 재로드·PIE 실제 카메라 및 구름 화면을 확인했다. 속도0.02는 이미 일치하여 유지했다. 밀도 설정 함수가 반환값 없이 성공하는데 bool로 검사한 첫 스크립트는 중단됐으나, 별도 읽기로 변경된 값3과 해당 인스턴스만 dirty임을 확인한 후 저장했으며 변경 명령을 재실행하지 않았다.
- 1강 산란은 PhaseG=0.5, PhaseG2=-0.5, PhaseBlend=0.2, MultiScatteringContribution=1, Occlusion=0.1, Eccentricity=0.1이다. 약 11:30 HD 인스턴스에 보이는 기본값으로 누락된 여섯 파라미터 연결을 복원했다. 저장·재로드·PIE 화면을 확인했으며 당시 남았던 지평선 아래 검은 영역은 이후 높이 안개 추가로 보정했다. 2~3강도 여섯 연결을 복원했다. 2강 6:45 및 3강 29:30 인스턴스를 대조하여 차폐값은0.2, 나머지는1강과 같게 적용했다. 각각 저장·재로드·PIE 화면을 확인했다. 3강 최종 밀도/마스크 인스턴스 값은 아래 기록처럼 교정했다.
- 폭풍 중심 마스크는 `saturate(pow(flowB*bounds,StormCenterContrast))`이다. 3강 후반에 추가되는 Power/Saturate를 복원했다.
- 볼륨 폭풍 UV는 `(WorldPosition-StormPositionKm*100000+StormScaleKm*100000)/(2*StormScaleKm*100000)`의 XY이다. 3강 약 3~4분 HD 그래프와 대조하여 초안의 위치 덧셈을 뺄셈으로 교정했다. 현재 3~6강 StormPositionKm=(-14.242788,6.15991,0), 7강=(-14.242788,5.955026,0)이다. StormScaleKm은 3강19.843781, 4~7강25이다. 반구 X의 정확한 원본 값과 상대 배치는 아직 미확정이다.
- 흐름 ConstantBiasScale은 Bias=-0.5, Scale=0.5이다(3강 4:55). 반구 Flowmaps의 Time은 원본처럼 엔진 함수의 기본 입력을 사용하며, 범위 마스크를 흐름 벡터에 곱한다. UV 회전은 엔진 CustomRotator를 사용한다.
- 반구 구름색은 `CloudLightColor*saturate(pow(noiseG,CloudContrast)) + CloudDarkColor*saturate(pow(1-noiseG,CloudDarkContrast))`이다. 초안의 별도 배경색 Lerp를 제거했다.
- 대기광 분기는 `noiseG² * SkyAtmosphereLightIlluminanceOnGround * AtmosphereSunLightIntensity`이다(6강 9:50/10:05). 균일한 밝은 고리를 만들던 노이즈 제곱 누락을 복원했다. vertex R의 대비/강도 마스크로 구름색과 혼합한 뒤 별/코어를 합성한다.
- 4~6강 Opacity Mask는 `DitherTemporalAA(saturate(pow(VertexColor.R, DitherMaskContrast)*DitherIntensity)) * (1-Fresnel*FresnelDitherMultiplier)`이다(4강 22:35). 7강은 이 기존 마스크 경로를 Opacity로 연결한다. 초안의 DitherTemporalAA 우회를 제거했다(7강 28:20~28:43의 기존 OpacityMask 연결 이동). UE5.8에서 Translucent+IsSky가 컴파일되지 않아 IsSky=false이다.
- 별/코어 노이즈는 G 채널이다. 코어 마스크는 범위 제한된 flowmap B이며, 코어 색은 `color*(saturate(pow(noiseG*intensity,contrast))+offset)`이다.
- MF_ThunderPulse는 `w=t mod 2.88`, `saturate(cos(2πt/4.5)+sin(2πw²)*cos(2π(1-w)/3.25))`이다(7강 8:40 HD). 이전 1.86초/곱셈 결합 초안은 폐기했다.
- 7강 반구 번개는 CloudLightColor를 공유하며 대비=6, 강도=100이다. 약 8:38 HD 전체 그래프에서 구름 대비 이전의 노이즈 분기를 확인하여, 번개 Power의 Base를 CloudContrast 출력에서 원래 노이즈 G로 교정했다. 저장·재로드·PIE 화면을 확인했다. 번개를 일반 구름색에 더하고 대기광 Lerp, 별 Lerp, 코어 Lerp 순으로 합성하는 현재 연결도 같은 전체 그래프와 대조했다.
- 7강 볼륨의 TipFillIntensity=2.75는 원본의 50/0 비교 이후 최종 설정과 일치한다. 색상 번개 합성은 `LightColor + LightColor*pulse*ThunderLightIntensity`와 동치인 현재 곱셈식을 유지한다.
- 7강 볼륨 산란은 PhaseG=0.5, PhaseG2=-0.5, PhaseBlend=0.2, MultiScatteringContribution=1, Occlusion=0.45, Eccentricity=0.1이다. 약 6:20 이후 HD 인스턴스의 값으로, 초안 상수를 여섯 파라미터 연결로 교체하고 저장·재로드·PIE 화면을 확인했다.

- 7강 스카이돔의 높이 마스크는 기본 UV의 V를 제곱한 `V*V`이다(7강 25:35 HD). 초안의 `pow(saturate(V*2),2)`와 추가 ZenithFadeContrast 파라미터를 제거했다. 노이즈 대비, 지면 대기광 및 밝기0.1과 곱한 뒤 기존 하늘 출력에 더한다.

3강 맵의 볼륨 두께도 원본 초반 HD 설정인10km로 교정하고 저장·재로드·PIE 화면을 확인했다. 하단 고도는 현재2km이며 직접 대조가 남아 있다. 4강12분대는 반구를 수동으로 중심에 배치하는 과정이며, 위치 자동 연결 블루프린트는 설명상의 제안이다.

## 주요 현재 값

최신 사용자 조정: ThunderLightIntensity는 눈 부분 MI_07_StormEye=7, 주변 구름 MI_07_StormLighting=1이다. 각각 이전25와0.1에서 변경하여 저장하고 읽기 검증했다. 기본 밝기와 펄스 주기는 그대로다. 아래 수치는 이전 조정 이력이다.

**사용자 요청 보정:** 기본 상태는 유지하고 깜빡임만 약하게 하도록 7강 인스턴스의 ThunderLightIntensity를 MI_07_StormEye 100→25, MI_07_StormLighting 1.5→0.5로 낮췄다. 기본 구름/코어/별 색상, 노출, 펄스 주기·수식은 변경하지 않았다. 저장 후 레벨 재로드·PIE 화면 및 파라미터 읽기로 확인했다. 아래 원본 강도100/1.5와 이전 진단 기록은 원본 비교 이력이다.

3강 최종 인스턴스(약29:30 이후 HD 저장 화면)를 반영했다. MI_03_StormFlow는 현재 3강만 사용한다. 4~6강은 각각 MI_04_StormVolume, MI_05_StormVolume, MI_06_StormVolume을 사용한다. CloudDensity=2.698985, ErosionScale=4.136317, FlowSpeed=0.525, FlowIntensity=5.596205, StormBaseTiling=2.75, StormErosionTiling=1.25, HeightMaskBias=1.511366, HeightContrast=2.720406, HeightMultiplier=1.659618, StormCenterContrast=0.896941, StormHeightBias=1.241962, StormCenterHeightBias=1.611099, TimeNoiseScaleKm=66.986305이다. 초안의 밀도20 및 강한 중심 개구부 설정을 대체했다. 변경 후 3~6강 재로드/PIE 화면을 확인했다. 4강 초반 HD 화면에서 달라진 값을 확인하여 별도 인스턴스로 분리했다. 4~6강은 CloudDensity=2.052937, ErosionScale=3.544607, HeightMaskBias=1.495, StormHeightBias=1.175, StormCenterHeightBias=1.95, TimeNoiseScaleKm=50, Occlusion=0.45를 사용한다. 4강은 직접 확인한 값이며 5~6강은 4강에서 이어진다고 추정한 값이다.

| 항목 | 6강 | 7강 |
|---|---:|---:|
| CloudFlowIntensity | 10.348573 | 10.348573 |
| CloudTiling / CloudContrast / CloudDarkContrast | 3.1 / 7.648363 / 5.658167 | 동일 |
| ParallaxHeightRatio | 0.25 | 0.25 |
| 대기광 마스크 대비 / 강도 / 광도 | 8.614141 / 0.742638 / 0.155214 | 동일 |
| TipMaskIntensity | 1.581477 | 1.581477 |
| TipNoise 흐름 / 강도 / 타일링 / 오프셋 | 0.75 / 1.25 / 3.500281 / 0.05 | 0.8 / 2 / 5.123601 / 0.1 |

6강 대기광 값은 약 30:35 이후 HD 인스턴스 화면의 후반 조정값이다. 이전 중간 구간 값을 교체하고 저장·재로드·PIE 화면을 확인했다. 6강 코어 흐름0.75와 마스크1.581477은 44:45 이후 HD 인스턴스 화면으로 재확인했다. 7강 값은 약 8:35 HD 인스턴스에서 확인했다. 공통 구름색은 밝은 색 `(25,2.916667,0)`, 어두운 색 `(0.1,0.035,0)`이다. 4~7강 디더링 관련 값은 강도1.184393, 대비0.921836, Fresnel 지수2.391718, 배율1.627194이다.

6강 반구 Y=663174cm는 원본44:21 HD 패널의663173.9 부근을 반올림한 값이며 저장·재로드·PIE 화면을 확인했다. X는 아직 폭풍 중심 근사값이다. 6강 반구는 Scale=(19.5,19.5,20.75), yaw20, Z=772697cm이다. 원본 후반 HD 화면으로 높이를 재확인하여 진단용으로 남아 있던 Z=150000cm를 복원했다. 저장·재로드·PIE 화면을 확인했다. 4~6강 구름층은 현재 하단5km/두께10km이다. 4강은 직접 확인했으며 5~6강은 4강에서 이어진다는 추정이다. 5~6강 해당 패널 직접 대조는 남아 있다.

7강 반구는 Scale=(22.25,22.25,22), yaw20, Z=660934cm이다. 원본 최종 화면의 값이며 Z 소수부는 읽지 못했다. 반구 XY=(-1424278.8,834040)cm를 사용한다. X는 폭풍 중심에 맞춘 값이며 원본 반구의 정확한 X는 미확정이다. 7강 구름층은 하단5km/두께10km이다.

7강 원본 볼륨 중심은 약 6:20 이후 HD에서 StormPosition=(-14.242788,5.955026,0)km로 읽힌다. 반구 X 입력칸의 뒷부분이 잘려 있으므로 앞서 읽은 반구 XY를 완전한 좌표로 취급하지 않는다. 볼륨에는 이 좌표를 반영했고 반구와 관찰 위치에도 동일한 중심 이동을 적용했다. 반구의 정확한 상대 X 차이는 아직 미확정이다.

관찰 위치(cm)는 1~2강 (0,0,200), pitch25/yaw0; 3강 (-1424278.8,615991,200), pitch80/yaw90; 4강 (-1424278.8,-1184009,200), pitch20/yaw90; 5~6강 (-1424278.8,-584009,200), pitch40/yaw90; 7강 (-224278.8,595502.6,200), pitch50/yaw180/FOV90이다. 원본 카메라 좌표가 아니라 학습용 구도다.

5강 12:56 HD 최종 화면을 대조하여 CloudTiling=-2.0, CloudFlowIntensity=8.600579, ParallaxHeightRatio=0.25, FlowmapRotation=0.672를 반영했다. 반구 Y=663174cm는 원본 표시값의 정수 반올림이다. 컴파일·저장·재로드·PIE 화면과 실제 머티리얼 할당을 확인했다. 푸른 중심은 원본에도 있는 표현이다. 4강의 임시 PreviewColor 발광 노드는 원본 최종 그래프에 없어 제거했다. 4~6강 태양 pitch=-3은 주황색 주변 구름을 맞추기 위한 시각적 근사값이며 원본 직접 수치는 아니다.


2강 최종 값은 CloudDensity=2.989934, ErosionScale=4.136317, HeightContrast=2.720406, HeightMultiplier=1.659618이다. 원본 약7:00의 복숭아색 구름을 대조하여 태양 pitch=-1/yaw=-40으로 시각 보정했다. 원본 직접 판독 각도는 아니다. 저장·재로드·PIE 화면에서 일몰 색상을 확인했으며 원본 지형 차이는 남아 있으며 검은 하단은 이후 높이 안개 추가로 보정했다.

7강 볼륨 LightColor=(1,0.172772,0,0), StormAreaAlbedo=(0.25,0.05,0,0), ErosionScale=3.544607이다. 반구 UV는 5~7강 모두 `(UV-0.5+Scale)/(2*Scale)`로, 초안에서 누락됐던 분모2를 복원했다. 6~7강 FlowmapScale=0.921031, FlowmapRotation=1.381258이며 5강 FlowmapScale=0.45를 유지한다.

7강 반구의 Apply Cloud Fogging=true를 적용했다. 동일 게임 시간·카메라에서 전경 구름이 반구 외곽을 가리는 차이를 확인한 UE5.8 시각 보정이며 원본 직접 판독값은 아니다. TranslucencyPass는 기존 After DOF를 유지했다. 저장·레벨 재로드·PIE 화면을 확인했으며 격자 잔여와 색감 차이는 남아 있다.

3강 태양은 pitch=-7/yaw=-40으로 시각 보정하고 저장·재로드·PIE 화면을 확인했다. 원본 직접 각도는 아니며 전경 구름 분포의 차이는 남아 있다. 약0:29 HD 패널의 구름 두께10km 및 추적 거리350/50km는 현재 값과 일치한다. 하단 고도2km는 여전히 직접 대조가 필요하다.

6강에 원본 Outliner에 존재하는 ExponentialHeightFog를 추가했다. 현재 FogDensity=0.02, HeightFalloff=0.2, MaxOpacity=1은 엔진 기본값이며 원본 수치를 직접 판독하지 않았다. 저장·재로드·PIE 화면을 확인했다. 이후 1~5강에도 동일한 기본값의 높이 안개를 추가했다. 7강에도 이후 높이 안개를 추가했다. 정확한 원본 안개 수치와 PostProcessVolume 설정 대조는 남아 있다.

1~5강에 원본 환경에서 확인되는 ExponentialHeightFog의 누락을 보완했다. Z=0, FogDensity=0.02, HeightFalloff=0.2, MaxOpacity=1의 엔진 기본값을 사용했으며 원본 수치 직접 판독값은 아니다. 각 레벨을 저장·재로드·PIE 실행했다. 1/2/4강에서 보이던 검은 하단과 지평선의 밝은 점이 안개색으로 이어짐을 확인했고, 3/5강 중심 표현도 유지된다. 6강은 앞서 추가한 같은 안개를 유지한다. 원본 산 지형을 만든 것은 아니다. 이후 7강에도 같은 기본값의 높이 안개를 추가했다. 게임 시간3.342069와 실제 카메라를 고정해 안개 켜기/끄기를 비교하자 하단의 짙은 갈색 영역이 옅어졌으며 반구 형태는 유지됐다. 원본26:16~26:30 설명에서도 안개/대기에 의한 탈색을 언급한다. 저장·재로드·PIE 화면 및 안개 액터1개, dirty map 없음까지 확인했다. 현재 1~7강 모두 높이 안개가 있으나 원본 설정의 정확한 일치는 미검증이다.

## 검증과 한계

- 1~7강을 각각 저장·로드하고 실제 UEDPIE 월드/DefaultPawn 및 화면을 확인했다. 변경한 맵은 해당 변경 후 다시 확인했으며 최근 7강 번개 값 변경도 재로드·PIE로 확인하고 종료했다.
- 5강은 PIE 전용 인스턴스로 시간/카메라를 고정하여 시차0 대0.2를 비교했고, 재생 재개 후 흐름 변화를 관찰했다. 7강도 서로 다른 게임 시간에서 구름 이동을 관찰했다. 이 기록은 원본 전체 동작 일치의 증거가 아니다.
- 최신 번개 함수 GPU 검사는 10초/64개 샘플, 출력0~1, 최대 절대오차1.7934e-5이다. `thunder_source_gpu_samples.json`이 최신이며 `thunder_gpu_samples.json`은 이전 초안 결과다.
- 최신 7강 시작 위치에서 실제 카메라를 고정하고 게임 시간4.310353/16.731862의 구름 형태 및 강한/약한 발광 변화를 확인했다. 원본 수식의 두 시각 출력은 각각1/0이다. 이어 시간17.454023(원본 수식 출력0.718303)을 고정하고 PIE 전용 MID의 ThunderLightIntensity를100→0→100으로 바꾸자 반구의 강한 발광이 사라졌다가 복원됐다. 코어/별은 남아 있어 해당 밝기 변화가 반구 번개 분기에서 발생함을 구분했다. 이후 게임 시간9.329787(수식 출력1)과 실제 저장 카메라를 고정하고 볼륨에만 PIE 전용 MID를 할당하여 ThunderLightIntensity를1.5→0→1.5로 비교했다. 강도0에서 중심 위쪽 구름의 붉은 발광이 줄고1.5에서 복원됐으며 반구의 강한 발광은 유지됐다. 따라서 반구와 볼륨의 번개 분기가 각각 화면에 반영됨을 확인했다. PIE 종료 뒤 미저장 content/maps 없음도 확인했다. 원본 영상의 동일 프레임/동일 시간과의 일치 검사도 아니다. 진단 MID는 PIE 종료로 제거했고 dirty content/maps가 없음을 확인했다.
- 전체 높이 안개 추가 후 소유 에셋32개를 시작점으로 hard/soft 참조64개 패키지를 다시 재귀 검사했으며 MCP/Toolset 참조가 없었다. 패키징 독립 실행과 MCP/Toolset 플러그인을 비활성화한 로드는 미실시다. 현재 프로젝트의 MCP 관련 플러그인은 Editor 대상에만 활성화되어 있으나 이 선언만으로 독립 실행을 증명하지 않는다. 현재 에디터의 활성 플러그인/프로젝트 설정은 변경하지 않았다. 프로젝트 C++ 또는 MCP 런타임 기능을 추가하지 않았다.
- 현재 원본보다 옅은 색과 다른 중심 형태가 남아 있다. 원본의 산 지형은 강좌 제작 대상 밖의 배경이며 현재 Study에 없다.

## 최신 강좌별 화면 확인

실제 PlayerCameraManager 위치·회전·FOV를 검사하는 도우미로 1~6강을 순서대로 다시 실행했다. 7강은 바로 앞 실행에서 같은 검사와 시간별 발광 비교를 마쳤다. 아래는 화면에서 확인한 범위이며 원본 프레임 일치 판정은 아니다.

| 강 | 현재 화면에서 확인 | 남은 차이/확인 범위 |
|---|---|---|
| 1 | 기본 흰 볼륨 구름 | 원본보다 단순한 외관, 원본 지형 부재; 검은 하단은 높이 안개로 보정 |
| 2 | 일몰 색상의 침식 적용 구름 | 세부 윤곽/분포 차이; 검은 하단은 높이 안개로 보정 |
| 3 | 중심 바로 아래에서 폭풍 개구부와 흐름 형태 | 시작 위치를 중심 아래로 교정; 원본 카메라 좌표는 아님 |
| 4 | 폭풍 개구부와 반구 단계 | 원본 지형 부재, 반구 상대 X 미확정 |
| 5 | 푸른 반구 흐름/시차 결과 | 외곽 겹침과 배치의 원본 일치 미확정 |
| 6 | 별·어두운 구름·밝은 코어 | 원본과 색감/중심 형태 차이 |
| 7 | 동쪽 근접 구도에서 코어·별·어두운 구름·번개 강약 변화 | 원본 카메라 좌표는 아니며 외관 차이는 남음 |

3강 기존 시작 위치에서는 전경 구름에 중심이 가렸다. PIE에서 중심 바로 아래 위치로 옮긴 뒤 실제 카메라를 읽어 개구부 표시를 확인했다. PlayerStart와 에디터 시점만 변경하여 저장했고, 재실행 및 별도 레벨 재로드 후 저장된 위치/각도를 확인했다. 머티리얼과 구름층 수치는 유지했다. 최신 직접 의존성 검사는32개 에셋에서 금지 참조0건이다. 패키징 검증을 대체하지 않는다.

7강 반구 X의 잘린 앞부분을 일곱 자리 좌표로 가정한 -1552900cm를 PIE에서 비교했다. 시간3.839451, 실제 카메라(-1424278.8,-1204497.4,200), pitch30/yaw90/FOV90을 고정했다. 기존 -1424278.8에서 -128621.2cm 이동시 반구가 화면 오른쪽으로 조금 이동했지만, 원본과 중심 형태가 더 가까워졌다는 명확한 판단은 할 수 없었다. 전체 좌표의 자릿수도 직접 확인하지 못해 저장하지 않았다. PIE 종료 뒤 편집 반구의 원래 X, Static mobility, dirty maps/content 없음까지 확인했다. 이 가정값을 원본 측정값으로 사용하지 않는다.

## 알려진 차이와 비교 이력

7강 시작 시점을 동쪽 근접 위치(-224278.8,595502.6,200), pitch50/yaw180/FOV90으로 교정했다. 북쪽과 동쪽 시점을 실제 PlayerCameraManager 값으로 확인했고, 동쪽에서 코어와 아래쪽 어두운 구름이 더 구분됐다. 근접 FOV60은 화면이 잘려 기본90으로 유지했다. 번개 수식 출력0인 시간37.389076에 멈춰 코어·별·어두운 구름을 확인했다. 일시정지용 Slate 콜백은 실행 후 해제됐음을 로그로 확인했다. 시점 비교 사이에는 시간이 진행되어 밝기를 동일 조건으로 비교한 것은 아니다. PlayerStart/에디터 시점 저장 후 PIE 실제 카메라와 화면을 확인했다. 아래 남쪽 시점 비교 기록은 이전 진단 이력이며 현재 시작 위치는 이 문단의 값이다.

노출 보정 전 7강 편집 월드를 직접 열거했을 때 PostProcessVolume은 없었다. 실행 중 콘솔 값은 AutoExposure=1, Method=0, ExtendDefaultLuminanceRange=1, Bias=1, EyeAdaptationQuality=2이며 해당 조회에 미등록 변수 경고는 없었다. 이는 현재 프로젝트 기본 자동 노출 상태를 확인한 것으로, 원본의 노출 설정과 같다는 증거는 아니다. 원본과의 색감 비교에서는 자동 노출 적응 및 화면 구도 차이를 통제해야 한다. 프로젝트 렌더링 설정은 변경하지 않았다. 이후 시간8.666716과 실제 카메라를 고정해 Bias0과1을 비교했다. 0에서 어두운 구름과 주황색 발광이 더 구분됐으며 중심 형태는 바뀌지 않았다. 전역 Bias는1로 복원하고, 7강에만 Study_07_ExposureApproximation이라는 Unbound PostProcessVolume을 추가하여 AutoExposureBias=0만 override했다. 이는 원본 직접 수치가 아니라 시각적 근사값이다. 저장·레벨 재로드·PIE 실제 카메라와 화면 및 런타임 PP의 override/값을 확인했다. 나머지 강좌와 전역 설정은 유지했다.

3강 초반 약0:08 HD 구름 컴포넌트 패널은 Layer Height부터 표시되고 Layer Bottom Altitude는 위쪽 스크롤 영역에 가려져 있었다. 같은 초반 패널을 다시 확대해도 하단 고도2km의 직접 근거는 얻을 수 없으므로, 그 구간을 반복 확인하거나 이를 확정 수치로 취급하지 않는다.

7강 26:30~30:07의 설명을 재확인했다. 강사는 Is Sky를 켠 Masked 방식의 채도 보정과 후처리 머티리얼을 대안으로 설명하고, 마지막에는 Translucent와 Opacity 연결을 선택한다. 후처리 머티리얼은 필수 제작 단계가 아니다. 이는 배경 PostProcessVolume 설정이 일치한다는 증거는 아니다. 현재 M_07_EyeTranslucent는 Translucent, IsSky=False, ApplyCloudFogging=True, AfterDOF임을 에디터에서 확인했다.

7강의 이전 시간17.6986375 시점 비교는 무효다. 일시정지 중 Pawn/ControlRotation/default_fov를 변경해도 PlayerCameraManager의 실제 카메라가 갱신되지 않은 상태였다. 해당 비교로 시점 가설을 배제할 수 없다. 이후 재생을 잠깐 진행하고 다시 정지하여 get_camera_location/get_camera_rotation/get_fov_angle로 실제 값을 검증했다. (-1424278.8,-1204497.4,200), pitch30/yaw90/FOV60, 시간20.818608에서 밝은 코어가 위쪽에 보이고 전경 구름이 아래를 가리는 형태를 확인했다. FOV90도 실제 갱신을 확인했다. 비교 도중 시간이 진행되었으므로 두 시점의 노이즈·번개 밝기는 동일 시간 비교가 아니다. 이후 기본 FOV90에서 같은 위치/각도로 PlayerStart와 에디터 관찰 위치를 저장했다. 재로드한 PlayerStart 값과 PIE의 실제 카메라 위치·회전·FOV를 각각 검증했다. 원본보다 넓은 구도이며 원본 카메라 수치로 주장하지 않는다.

7강 태양은 같은 시간20.818608에서 pitch=-7과 -3을 비교하여 원본의 따뜻한 주황색에 가까운 -3/yaw=-40으로 시각 보정했다. 원본 직접 판독 각도는 아니다. 편집 월드의 Stationary 설정을 유지하여 저장하고 레벨 재로드·PIE 화면을 확인했다. 반구 단독 표시에서는 코어가 실제로 렌더링됨을 확인했다. 중심 형태 차이를 코어 누락으로 단정하지 않는다. 진단용 숨김/카메라 변경은 PIE 종료로 제거했다.

1. 7강 번개 입력·수식·합성 순서와 현재 장면의 시간 변화, 반구 번개 켜기/끄기 비교는 확인했다. 원본과의 최종 외관 대조에는 밝은 구간과 어두운 구간을 모두 포함해야 한다.
2. 6강 44:52~44:57 HD 최종 그래프에서 두 구름색 Multiply의 Add가 대기광 Lerp로 직접 이어짐을 확인했다. 현재 M_06의 Add_3→Lerp_3 연결과 일치하며 이 경로에는 7강의 별도 번개 합산이 없다. 7강 8:15~8:40에는 반구 시간 함수를 복사하는 설명과 그래프가 있으므로 번개 구현은 7강에 유지한다. 영상 사이에 추가된 과정의 정확한 시점은 불명확하지만 6강 최종 연결을 7강 방식으로 소급 변경하지 않는다.
3. 1~7강 최종 상대 배치·조명·시점과 시간 변화 조건을 대조한다. 특히 폭풍 개구부/반구의 상대 XY, 1~3강 조명과 4~7강 색감과 원본 배경 차이를 확인한다.
4. 최종 변경 후 맵별 저장·재로드·실행, 전체 직접 의존성 검사를 마무리한다. 검증 범위를 넘는 완료 주장을 하지 않는다.

## 로컬 근거와 작업 방식

원본 영상/자동 자막/프레임/스크립트/실행 결과는 `Saved/CloudTutorialReference/`에 있다. `stage01_runtime_check.json`~`stage07_runtime_check.json`은 시작 직후 월드/ Pawn 검증이다. 7강 최신 실행 기록은 Pawn뿐 아니라 PlayerCameraManager의 실제 위치·회전·FOV까지 검사한다. 검증 도우미는 레벨 전환 전에 미저장 패키지가 없음을 확인하며, PlayerStart/에디터 시점을 설정하고 저장하므로 읽기 전용 도구가 아니다. UI 시각 확인 및 StopPIE 결과는 대화 도구 기록에 있다. 이 파일들의 존재만으로 원본 일치를 판정하지 않는다.

현재 프로젝트의 실행 중인 에디터와 공식 MCP/Cubeless Python 도구를 사용한다. 불확실한 변경 결과는 자동 재시도하지 않고 현재 상태를 확인한다. 사용자 작업을 보존하며, 사용자의 최종 게시 요청에 따라 이 Study 에셋과 학습 문서만 커밋·푸시 범위로 지정했다.
