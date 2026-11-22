# 주 소용돌이 실제 3D 부피 — 첫 검증본

2026-11-15. 작업 레벨 `/Game/Study/SwirlClouds/L_02_PrimarySwirl3D`. 활성 에셋은 `M_PrimarySwirl3D`, `MI_PrimarySwirl3D`이다. 기존 L_01은 남겨 두었다.

월드 좌표에서 32개 타원체를 나선 경로에 배치하고 부드럽게 합친 실제 3D 밀도 영역이다. 카메라나 파노라마 이미지 샘플에 종속되지 않는다. [형태 코드](primary_swirl3d.hlsl)는 큰 형태만 정의한다. 기존 Shape128/Erosion32 노이즈와 침식, Flowmap, albedo, 산란 그래프는 원본에서 복제했다. 원본 139개 노드에 6개를 추가했다.

원본 높이 마스크를 3D 형태의 밀도 임계값 `1-Macro*0.20`으로 교체했다. 원래 밀도 출력은 양수 제한 후 Macro로 제한하고, 원래 발광도 Macro로 제한한다. 유한한 구름 덩어리에 노이즈 크기를 맞추기 위해 기본/침식 노이즈 좌표에 관찰 기준점 중심 3배 스케일을 적용했다. 따라서 노이즈의 공간 배치는 원본과 같지 않다. 원래 MI 스칼라·벡터 값과 albedo/광학 계산은 유지했다. MacroOnlyPreview=0으로 저장했다.

## 검증과 비교

컴파일 오류 없음과 에셋/레벨 저장을 확인했다. 기존 환경 6개 액터의 기록 속성, 원본 MI 파라미터, 원본 노드 연결을 대조했다. 변경 연결은 두 노이즈 좌표, 높이 마스크, BSDF 밀도/발광뿐이다. 원본 tutorial 에셋은 수정하지 않았다.

기준 위치 (-1424278.8,595502.6,200), pitch32/yaw119/FOV90으로 파노라마 돔과 비교했다. 추가로 가로 방향 2km 이동한 위치 (-1599202.8,498540.6,200)에서 시점 변화도 캡처했다. 원래 비교 시점으로 복귀했다.

- 결과: `Saved/SwirlClouds/primary3d_final.png`
- 옆 시점: `Saved/SwirlClouds/primary3d_side.png`
- 동일 시점 레퍼런스: `Saved/SwirlClouds/reference_right_final.png`
- 구조/환경 검증: `Saved/SwirlClouds/primary3d_verification.json`

큰 소용돌이와 실제 부피는 구현했지만 최종 품질은 아니다. 레퍼런스보다 빈 공간이 넓고, 안쪽이 매끈한 관처럼 보인다. 바깥쪽은 기존 노이즈 결이 더 드러난다. 다음 형태 수정은 납작하고 넓은 구름 덩어리의 중첩, 비대칭 단면과 깊이 변화에 집중해야 한다. 색/산란으로 형태를 위장하지 않는다. 원본 기준에서도 관찰된 미세 격자 패턴은 미해결이다.

이번 버전의 재로드, PIE, 장시간 애니메이션, GPU 성능 및 패키징은 미검증이다. 커밋/푸시는 하지 않았다.

## 2026-11-15 한쪽 구름층 연결 테스트

L_02_PrimarySwirl3D의 활성 인스턴스를 MI_ConnectedBank3D로 변경해 저장했다. M/MI_PrimarySwirl3D는 연결 전 비교본으로 보존했다. M_ConnectedBank3D는 기존 머티리얼의 복제본이며, 큰 형태 Custom 코드만 변경했다. [연결 형태 코드](connected_bank3d.hlsl)에 오른쪽으로 넓어지는 10개 타원체의 부드러운 합성을 추가했다. 중심 깊이와 단면 크기가 변하며 주 소용돌이의 바깥 팔과 같은 밀도장으로 합쳐진다.

컴파일 오류 없음, 머티리얼/인스턴스/현재 레벨 저장, 기존 환경 6개 액터와 원본 파라미터/광학 연결 유지 확인. 정면과 가로 2km 이동 시점에서 연결을 확인했다. 증거: Saved/SwirlClouds/connected_bank3d_front.png, connected_bank3d_side.png, connected_bank3d_verification.json. 원래 정면 카메라로 복귀했다.

관찰: 오른쪽 넓은 구름 덩어리가 외곽 팔에 이어진다. 주 소용돌이 내부의 매끈한 관 모양, 넓은 빈 공간, 미세 격자 렌더 패턴은 여전히 남아 있다. 이는 한쪽 연결의 첫 테스트이며 레퍼런스 최종 품질로 판정하지 않는다. 재로드/PIE/성능/패키징 미검증. 커밋과 푸시 없음.

## 2026-11-15 다양한 부피 형태 구성

현재 L_02_PrimarySwirl3D 활성 인스턴스는 MI_VariedShapes3D이다. 이전 PrimarySwirl3D와 ConnectedBank3D는 비교본으로 보존했다. [형태 코드](varied_shapes3d.hlsl)는 오른쪽 비대칭 말림 27개, 위쪽 넓은 층 7개, 왼쪽 둥근 덩어리 11개, 아래쪽 휘어진 띠 20개의 타원체를 월드 공간에서 합성한다. 단면/높이/깊이 변화가 있으며 모든 영역은 기존 노이즈와 침식을 공통으로 사용한다. 형태 내부 밀도 임계값은 1-Macro*0.16으로 조정했다. 노이즈 좌표 3배 스케일은 직전 버전과 동일하다.

기존 환경 6개 액터, 원본 MI 스칼라/벡터, albedo와 산란 그래프의 보존을 확인했다. 컴파일 오류 없음, 저장 및 레벨 재로드 후 활성 MI 검증 통과. 재로드 호출은 world transition으로 current_unknown을 반환했지만 LOAD True와 후속 실제 월드/MI 검증으로 결과를 확인했으며 재시도하지 않았다. 신규 M/MI의 직접 의존성에서 MCP 참조 없음. PIE/성능/패키징은 미검증이다.

정면과 가로 2km 이동 시점에서 형태를 확인하고 정면으로 복귀했다. Saved/SwirlClouds/varied_shapes3d_final.png, varied_shapes3d_side.png, varied_shapes3d_verification.json에 증거가 있다. 넓은 층/둥근 덩어리/긴 띠가 구분되지만 레퍼런스 수준의 세부 중첩은 아직 부족하다. 일부 띠는 매끈하고 안쪽 말림이 약하다. 기존 미세 격자 패턴도 남아 있다. 최종 품질로 판정하지 않았다. 커밋/푸시 없음.
