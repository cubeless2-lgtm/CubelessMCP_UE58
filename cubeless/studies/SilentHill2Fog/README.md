# 사일런트 힐 2풍 거리 안개

[원본 영상 · Dallas Drapeau](https://youtu.be/96sheL5UqJQ). 영상 속 City Sample 거리를 엔진 기본 메시로 단순화했다. 횡단보도, 반복되는 가로등, 양쪽 건물은 각각 근경·중경·원경의 대비를 읽기 위한 기준물이다.

콘텐츠 브라우저에서 `/Game/Study/SilentHill2Fog/L_SilentHillFogStreet`를 연다. 아웃라이너의 `Review camera`가 아래 화면의 기준 위치다.

![거리 입구에서 본 결과](Street.png)

## 영상과 그래프를 함께 보는 순서

| 영상 위치 | 실습 그래프 | 관찰할 부분 |
| --- | --- | --- |
| [09:00](https://youtu.be/96sheL5UqJQ?t=540) | M_StreetFog의 Large, Detail | 월드 좌표에 서로 다른 주파수를 곱하고 시간에 따른 이동량을 더한다. 두 노이즈를 합친 뒤 임계값으로 안개 사이에 빈 곳을 만든다. |
| [15:00](https://youtu.be/96sheL5UqJQ?t=900) | Clear Radius cm, Fade Range cm | 카메라와 샘플 위치 사이의 거리에서 근거리 반경을 빼고 전환 폭으로 나눈다. 횡단보도 가까이는 맑고 가로등이 멀어질수록 안개가 쌓인다. |
| [18:00](https://youtu.be/96sheL5UqJQ?t=1080) | Ceiling cm, Ceiling Falloff cm | 월드 Z가 천장에 가까워질수록 밀도가 줄어든다. |
| [21:00](https://youtu.be/96sheL5UqJQ?t=1260) | DistanceToNearestSurface, Surface Fog | 지면·벽에 가까운 곳의 거리장 값을 반전해 세부 노이즈 밀도를 더한다. 볼륨 박스 자체는 거리장 계산에서 제외했다. |
| [30:00](https://youtu.be/96sheL5UqJQ?t=1800) | Shadow Trace cm, Shadow Softness cm | 태양 방향으로 이동한 위치에서 거리장을 한 번 더 읽고, 그 값에 따라 발광 색을 보간한다. 이번 실습에서 단순화한 그림자 근사다. |
| [35:00](https://youtu.be/96sheL5UqJQ?t=2100) | M_DistanceVeil | 카메라에서 먼 픽셀일수록 PostProcessInput0의 장면색을 Far Fog Color와 섞는다. |

## 한 번에 하나씩 바꿔 보기

1. `MI_DistanceVeil`에서 **Veil Strength**를 0으로 내린다. 같은 카메라에서 먼 건물의 대비를 비교한 뒤 현재 맵의 값 0.22로 되돌린다. **Fog Distance cm**의 현재 맵의 값은 6500이다.
2. `MI_StreetFog`에서 **Clear Radius cm**를 520에서 1500으로 늘린다. 카메라 앞의 맑은 구간이 어디까지 이동하는지 가로등으로 비교한다. **Fade Range cm**는 2600이다.
3. **Surface Fog**를 0과 0.18로 바꾸며 건물 벽과 보도의 경계를 본다. **Extinction**은 0.029로 유지하면 전체 밀도가 같을 때의 차이를 비교할 수 있다.
4. 뷰포트의 실시간 렌더링을 켜고 **Large Velocity**와 **Detail Velocity**를 각각 0으로 바꾼다. 어느 크기의 결이 정지하는지 비교한다. 기본 RGB 값은 각각 (0.045, 0.018, 0.005)와 (0.022, -0.04, 0.012)다.

## 제작과 확인

UE 5.8.4의 프로젝트 에디터에서 공식 MCP를 통해 MaterialEditingLibrary로 그래프와 인스턴스를 생성하고, 거리 메시·조명·Exponential Height Fog·볼륨 머티리얼 박스·무한 범위 Post Process Volume을 배치했다. 노이즈 입력은 첫 번째 World Position 핀에 연결했다. 처음에는 가로등의 따뜻한 산란광이 화면 대부분을 차지해 조도와 볼륨 산란 강도를 낮췄다.

최종 맵을 저장하고 다시 연 뒤 초판 액터 341개, Volumetric Fog 활성, 메시 거리장 활성, 두 안개 인스턴스 연결을 확인했다. 위 이미지는 재로드한 에디터의 캡처다. 영상의 그래프 구성을 참고해 거리 크기에 맞게 밀도와 조명을 조정했다. 영상과의 최종 시각 비교는 사용자 검수로 남겼으며 안개 박스의 충돌 프로필을 NoCollision으로 저장하고 액터 충돌도 껐다. 재로드 후 PIE에서 캐릭터가 시작점의 도로에 착지하고 높이 약 92.15 cm에서 수직 속도 0을 유지하는 것을 확인했다. 패키징과 GPU 시간은 검증하지 않았다.


## 첨부 레퍼런스에 맞춘 수정

횡단보도 앞 카메라에서 좌측 순찰차와 우측 택시가 서로 다른 깊이에 놓이도록 배치했다. 건물에는 창턱·돌림띠·상점 기둥을 더하고 노면에는 골재, 균열, 젖은 부분의 거칠기 변화를 넣었다.

건물과 도로의 안개 모양을 분리하기 위해 국소 안개 머티리얼 `M_DriftingBanks`를 추가했다. 각 박스 안에서 카메라 광선을 따라 최대 32회 밀도를 샘플링하고, 불투명 표면의 깊이에서 적분을 끝낸다. 지도에는 벽면 13개, 도로·연석 5개, 원경 1개의 볼륨이 배치돼 있다. 이 구성과 수치는 첨부 화면을 보며 실습 맵에 맞춰 정했다.

| 위치 | 인스턴스 | 같은 카메라에서 확인하는 법 |
| --- | --- | --- |
| 건물 | MI_FacadePlumes, MI_ShadedFacadePlumes, MI_RisingWisps, MI_SlateWisps | 아웃라이너의 `Fog/Banks/Facade curls`를 숨겼다가 켜 본다. 창과 간판 사이를 가리는 세로 안개 줄기가 높이에 따라 건물에서 떨어져 길 안쪽으로 휜다. |
| 도로 | MI_RoadStrata, MI_CurbMist | `Low drifting ribbons`와 양쪽 `curb drift`의 표시를 전환한다. 바퀴 높이의 옅은 층과 횡단보도 너머에서 가로로 이어지는 층을 비교한다. Bank Density는 각각 0.66, 0.51이다. |
| 원경 | MI_FarMist | `Background fog`를 숨겼다가 켠다. 밝은 중앙 통로와 어두운 벽면 사이의 대비를 본다. |

맵을 다시 열어 액터 729개와 볼류메트릭 안개 활성 상태를 확인했다. PIE에서 시작 캐릭터가 도로에 착지했다. 위 캡처는 최종 도로 밀도를 적용한 에디터 화면이다. 건물·차량은 실습용 단순화 메시이며 GPU 비용은 측정하지 않았다.
## 건물 안개의 텍스처 실험

같은 검토 카메라에서 기존 절차적 안개와 생성 마스크를 적용한 결과를 캡처했다. 실시간 안개의 이동 시점은 서로 다르다.

![같은 시점에서 본 벽 안개 수정 전후](FacadeComparison.jpg)

건물 볼륨 13개는 `M_TexturedFacadeFog`를 사용한다. ImageGen 내장 도구로 만든 [흑백 밀도 원본](FacadeDensity.png)을 `T_FacadeDensity`로 가져와 선형 그레이스케일로 읽는다. 흰 영역은 짙은 안개, 검은 영역은 건물이 드러나는 틈이다. 생성 마스크를 방향과 크기가 다른 두 좌표에서 샘플링하고, 월드 공간 노이즈로 좌표를 휘게 한다. 벽에서 떨어진 거리와 높이에 따라 밀도를 줄이고 최대 28회 누적한다. 좌우 볼륨은 투영 각도와 밝기가 달라 같은 무늬가 나란히 반복되는 현상을 줄인다. 도로·연석·원경 볼륨 6개는 기존 머티리얼을 사용한다.

맵 재로드 후 건물 볼륨 13개의 부모 머티리얼과 생성 텍스처 연결을 확인했다. 수정 후 이미지는 에디터 캡처다. 이번 텍스처 변경의 GPU 시간은 측정하지 않았다.

<details><summary>ImageGen 생성 프롬프트</summary>

A single square grayscale VFX density map of diffuse damp morning mist curling alongside a tall facade in a crosswind. Floating fog alone in a featureless black void; no facade, ground, scenery or lighting baked in. Edge-to-edge cropped texture field, with natural discontinuous diagonal banks of fog interrupted by generous deep black irregular negative spaces. Several large soft billows which turn sideways and dissolve into small scalloped feathered curls. Shapes drift up and right then fold back and break, uneven scale. Soft tonal transitions and sparse fine filaments, photographic fluid simulation, no sharply outlined smoke ropes, no vertical columns, no single centered plume, no gridded repetitions, no flames, no lettering. Scalar grayscale data, black is empty, middle gray is partial, white is dense.

</details>

## 비교 구도와 캐릭터 이동 반응

[레퍼런스와 에디터 비교](ReferenceComparison.jpg) · [플레이 녹화](WalkingFog.webp)

사용자가 제공한 가로 화면을 기준으로 두 가지 밀도 텍스처를 조합했다. [구도 마스크](FacadeFogLayout.png)는 좌측 건물의 갈라진 안개와 호텔 앞의 사선 틈을 지정하고, 기존 FacadeDensity는 안쪽 결을 만든다. 광선마다 월드 공간의 위치를 고정된 기준점에 투영해 구도 마스크를 읽는다. 플레이어가 돌아보거나 이동해도 안개 배치는 거리에 붙어 있으며, 노이즈로 밀도와 가장자리가 움직인다. 차도에는 높이와 밀도가 다른 지면 안개를 배치했다. 횡단보도 뒤와 택시 뒤에는 윗면이 고르지 않은 얕은 층을 추가하고 호텔 앞 안개는 비스듬히 기울였다. [상점과 호텔 입면](StreetFronts.png)은 엔진 건물 메시 위에 입혔다. 비교 이미지는 레퍼런스 하단의 에디터 UI만 잘라 같은 크기로 나란히 놓았다.

진입로, 횡단보도, 주차 차량 옆에 낮은 안개를 한 겹씩 더했다. 아웃라이너에서 `Fog/Banks/Ground curls`를 펼치면 세 볼륨을 찾을 수 있다. `MI_GroundCurls`는 벽 안개에 쓰던 `T_FacadeDensity`를 노면 방향으로 펼쳐 읽는다. 마스크가 짙은 곳은 결이 조금 솟고, 옅은 곳은 가장자리가 끊기며 아스팔트가 드러난다. `Bank Density`의 저장값은 0.56이다.

맵 전용 `BP_WalkingFog`가 캐릭터의 위치와 보간한 XY 속도를 `MPC_Wake`에 전달한다. 7~180 cm/s 사이에서 반응이 커진다. 건물과 바닥의 국소 안개 머티리얼은 이동 경로 주변에서 기존 밀도를 읽는 좌표를 옮겨 결이 양옆으로 밀리게 한다. 거리의 볼류메트릭 안개도 몸이 지나가는 좁은 영역에서 밀도가 줄어든다. 멈추면 이동 반응이 잦아들고 맵의 안개는 원래 흐름을 이어 간다. 발 주변에 별도 안개를 그리던 처리는 제거했다. 플레이 카메라의 스프링 암 높이 오프셋은 75 cm다.

저장한 맵을 다시 열어 바닥 볼륨 세 개와 밀도 텍스처 연결을 확인했다. PIE 녹화에서 캐릭터는 약 14.94 m를 이동했고, 머티리얼에 전달된 속도는 출발 전 0, 이동 중 최대 600, 정지 후 0 cm/s였다. 횡단보도를 지나는 프레임에서 게임을 일시정지하고 속도만 600 → 0 → 600 cm/s로 바꾸자 도로 안개의 결이 변했다가 같은 모양으로 돌아왔다. 패키징과 GPU 시간은 검증하지 않았다.

<details><summary>이번에 추가한 내장 ImageGen 프롬프트</summary>

건물 입면

Photorealistic texture atlas for a realistic survival horror abandoned North American downtown city environment, square image. Exactly TWO adjoining equal-width whole building facades side by side, perfectly orthographic front elevation, full bleed facade from roof cornice at top edge to shop thresholds at bottom edge. No sky, street, sidewalk, trees, people, cars, drawn text, streetlights, fog, borders, angled walls or perspective. The left half is a soot-stained early 20th century six-storey ash-gray urban masonry apartment and shop with narrow double-hung sash windows, dark glass, projecting stone lintels, parapet and weathered coursing; at ground level two closed shop windows, central recessed door, worn black awning, warm tungsten dimly visible behind one pane. The right half is a narrower-taller-looking neighboring seven-floor charcoal brown brick hotel with aged fire escape metal stairs and landings against parts of upper windows, small brass-lit entrance, flanking dusty glass storefront and masonry piers. Both buildings touch each other on a straight vertical seam exactly halfway across. Uneven lights behind a FEW rooms. Exact elevation, parallel vertical lines and level horizontals, natural materials, small-scale aged patina, quiet subtle color, pre-dawn even gray lighting, readable architectural detail and gentle ambient occlusion, no hard sun, not a drawing or game screenshot. Plain unlettered surfaces. Image should work pasted flat onto two solid building fronts, with realistic scale and no margin at any edge.

벽 안개 구도

Reference image is a layout guide only. Create a clean artist-painted VFX grayscale density matte isolating the LOCAL wall-hugging fog shapes visible on the left and right building facades in this specific reference frame. Wide landscape image full bleed. Keep the relative placement, scale, tilt, and uneven silhouettes of those local mist patches, especially the ragged descending lobes on the left and the three separated diagonal folds across the right-hand hotel front. Fog is light gray / off-white on pure black, empty regions are black. Delicate medium-scale turbulent curls and feathered edges with broad smooth translucent interiors; some lower strands descend toward the bottom third. Keep central 20% of frame nearly empty black (the far-distance veil is a separate layer), and the bottom 20% dark. Strip away absolutely all original scenery, lighting, windows, signs, cars, road, architecture, perspective lines, UI and ground. Do not illustrate a city, do not create a picture of buildings or two tapering identical trees; only organic irregular floating wisps form this density field, asymmetric on the two sides.

</details>
