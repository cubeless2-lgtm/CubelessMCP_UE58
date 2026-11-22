# 기존 볼륨 구름 질감을 유지하는 파노라마 형태 스터디

## 현재 기준: 2026-11-15

사용자 교정: 기존 볼륨메트릭 구름의 질감과 빛 반응을 유지하고, 큰 쉐잎만 파노라마 레퍼런스처럼 만든다. 환경은 그대로 유지한다. 이전 이미지 기반 높이/산란 재구성은 이 요구와 달라 더 이상 활성 머티리얼로 사용하지 않는다.

- 작업 레벨: `/Game/Study/SwirlClouds/L_01_LargeFlow`
- 활성 머티리얼: `/Game/Study/SwirlClouds/Materials/M_NativePanoramaClouds`
- 활성 인스턴스: `/Game/Study/SwirlClouds/Materials/MI_NativePanoramaClouds`
- 비교 레벨: `/Game/Study/SwirlClouds/L_00_ReferencePanorama`
- 원본 파노라마: [reference-panorama.png](reference-panorama.png)

원본 `M_07_StormLighting`과 `MI_07_StormLighting`을 각각 복제했다. 원래 3D 볼륨 노이즈, 침식, 높이 마스크, Flowmap, albedo, 산란과 조명 그래프를 그대로 유지한다. 기존 원본 에셋에는 변경이 없다.

## 큰 형태만 제한하는 방식

[큰 형태 마스크 코드](native_shape_mask.hlsl)는 파노라마의 mip 3에서 낮은 주파수의 구름 영역을 읽는다. 원본 투영 메타데이터가 없으므로 가로 360도/세로 상반구 90도라는 기존 비교 가정을 유지한다.

커버리지 C를 다음 두 최종 출력에만 적용한다.

- Extinction: `max(0, OriginalDensity - (1-C) * NativeDensityScale * 0.35) * C`
- EmissiveColor: `OriginalEmissiveColor * C`

마스크 경계에서 기존 3D 밀도에 따라 구름이 깎이도록 하여 단순한 이미지 오려내기를 완화한다. C=1인 영역은 기존 양의 밀도를 그대로 사용한다. 파노라마 밝기를 높이, 구름 색, 산란 반응으로 변환하지 않는다. 조명 계산과 기존 번개 관련 값도 원본 인스턴스에서 그대로 상속한다(ThunderLightIntensity 1).

원본 내부 노드의 연결은 모두 보존했고 최종 BSDF의 Extinction/EmissiveColor 연결 두 개만 위 처리로 감쌌다. 원본 139개 노드에 마스크/텍스처/밀도 게이트/발광 마스크 노드 4개를 추가했다.

## 유지한 값과 환경

원본 인스턴스의 모든 기존 스칼라·벡터 값을 대조하여 일치를 확인했다. 주요 값은 CloudDensity 2.052937, CloudTiling 65, ErosionScale 3.544607, FlowSpeed 0.525, PhaseG 0.5, PhaseG2 -0.5, 다중 산란 기여 1, Occlusion 0.45이다. 이 숫자는 이전 재구성 머티리얼의 0.002 밀도와 직접 비교하지 않는다. 원본 그래프의 단위 변환도 그대로 유지했다.

기존 DirectionalLight, SkyAtmosphere, SkyLight, 하늘 돔, 안개, 노출 액터 6개의 조사 대상 속성은 기준 스냅샷과 동일하다. 레이어 하단 5 km/높이 10 km, 기존 비교 카메라도 유지한다. 구름 view/shadow sample scale 3은 직전 수정 상태에서 유지했다.

| 비교 | 위치(cm) | Pitch | Yaw | FOV |
|---|---|---|---|---|
| 오른쪽 소용돌이 | -1424278.8, 595502.6, 200 | 32 | 119 | 90 |
| 왼쪽 구름층 | 동일 | 40 | -43 | 90 |

## 확인한 결과와 한계

같은 작업 레벨에서 원본 인스턴스를 임시 적용해 기준 화면을 확인한 뒤, 큰 형태 마스크가 추가된 복제본을 양쪽 시점에서 비교했다. 앞선 매끈한 높이 표면 대신 기존 노이즈로 형성되는 구름의 결이 돌아왔다. 큰 틈과 소용돌이 영역은 파노라마로 제한되지만, 내부 디테일은 원본 노이즈이므로 레퍼런스와 일대일로 같지 않다. 원본 질감 유지가 우선이라는 사용자 교정을 반영한 결과다.

로컬 증거는 `Saved/SwirlClouds/`에 있다.

- 기준/결과: `native_baseline_right.png`, `native_final_right.png`, `native_final_left.png`
- 원본 그래프와 인스턴스 대조: `native_preservation_verification.json`
- 에셋 참조: `native_final_dependencies.json`

컴파일 오류 없음, 저장과 실제 레벨 재로드, 활성 인스턴스 일치, 환경 스냅샷 일치, 원본 노드 연결/스칼라/벡터 유지, 8개 소유 에셋의 직접 MCP 의존성 없음, 미저장 패키지 없음을 확인했다. 원본 기준 캡처와 결과 모두에서 미세한 격자 모양의 렌더링 패턴이 보였으며, 이번 작업에서 그 원인을 규명하거나 해결했다고 주장하지 않는다. 현재 수정본의 PIE, 장시간 동작, 자유 이동, GPU 성능, cook/패키지는 미검증이다.

레퍼런스 투영은 고정 관찰 위치 기준의 큰 형태 가이드다. 자유 비행을 위한 완성된 기상 시스템이나 최종 시각 품질로 판정하지 않았다. 커밋·푸시는 수행하지 않았다.

이전 구현과 실패한 방향은 [반복 작업 기록](iteration-history.md)에 남겨 둔다. `M_LargeFlow`, `MI_LargeFlow`, `large_flow.hlsl`은 이전 접근의 보관본이며 현재 L_01에는 적용하지 않는다.

## 2026-11-15: 실제 3D 소용돌이 검증본

현재 작업 레벨은 `/Game/Study/SwirlClouds/L_02_PrimarySwirl3D`이다. 위 L_01 기록은 이전 2D 커버리지 접근의 검증 기록으로 보존한다. 새 버전은 원본 구름 노이즈/광학 그래프를 복제하고 월드 좌표의 3D 타원체 합성으로 부피를 만든다. 구현과 비교 한계는 [3D 부피 스터디](primary3d-study.md)에 기록했다.

한쪽 구름층 연결 테스트에서는 L_02에 `MI_ConnectedBank3D`를 적용했다. 기존 `MI_PrimarySwirl3D`는 연결 전 비교본으로 보존했다. 정면/옆 시점 결과와 한계는 위 3D 부피 스터디의 연결 테스트 절을 참고한다.

최신 형태 구성은 L_02의 `MI_VariedShapes3D`이다. 넓은 층, 둥근 덩어리, 아래쪽 휘어진 띠와 오른쪽 말림을 함께 구성했다. 컴파일/저장/레벨 재로드, 환경 및 원본 광학 그래프 대조를 확인했다. 자세한 변경과 시각적 한계는 3D 부피 스터디의 다양한 부피 형태 구성 절에 기록했다.
