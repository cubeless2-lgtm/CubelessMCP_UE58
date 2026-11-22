# Quinn Stocking Pull / Animation Follow Study

참고 영상의 스타킹 당기기를 Geometry Script 메시와 WPO로 재구성한 UE 5.8.4 실습이다. 원작 Unity 프로젝트의 이식은 아니다. 초기 불투명 갑옷 복사 결과와 V3 단면 외피는 외형 피드백을 받아 교체했다.

## 레벨과 조작

| 레벨 | 용도 | 조작 |
|---|---|---|
| `/Game/Study/StockingPull/Maps/L_StockingPull` | 기준 포즈에서 표면 잡기·복귀 | 좌클릭 드래그, 놓기, R 초기화, 1 전체 / 2 확대 |
| `/Game/Study/StockingPull/Maps/L_StockingFollow_Animation` | 걷는 중 표면 잡기·추종·복귀 | Play 시 걷기 반복, 좌클릭 드래그 / 놓기 / R, 1 전체 / 2 확대 |

애니메이션 레벨은 현재 **걷는 중에도 당길 수 있는 레벨**이다. 현재 액터는 `BP_StockingWalkPullStudy`다. 좌클릭을 누른 채 이동하면 최초 클릭에 대한 상대 당김량을 유지하고, 잡힌 영역은 다리 움직임을 따라간다. 마우스 위치에 표면을 월드 고정하는 방식은 아니다. 놓으면 걷기가 계속되는 상태에서 복귀한다. 천 물리 솔버는 없다. 안내판은 기존 오프라인 한글 글꼴의 추가 글자 누락을 피하기 위해 영문으로 표시했다.

## 현재 메시와 영역

현재 표면은 `/Game/Study/StockingPull/Meshes/SM_StockingFitted_Left_V5`, `SM_StockingFitted_Right_V5`다. 애니메이션에는 같은 형상의 `SKM_StockingFitted_Left_V5`, `SKM_StockingFitted_Right_V5`를 사용한다.

- Quinn LOD0의 실제 삼각형과 수평 평면 교차선에서 다리 단면을 구한다.
- 각 단면에서 각도별 광선 교차 거리로 외곽을 추출한다. 좌우를 각각 측정한다.
- 약한 평활화와 바깥 방향 여유 0.35cm를 적용한다. 단순 타원이나 크게 감싼 superellipse를 사용하지 않는다.
- 로컬 Z 0.15~89cm, 즉 발바닥부터 골반 아래 허벅지 상단까지 덮는다. 상단은 열린 경계이며 발바닥은 삼각형 fan으로 닫는다.
- 180개 높이 링, 링당 이음새 포함 129정점과 바닥 중심으로 **각 23,221정점 / 45,952삼각형**이다. 최적화된 성능 수치는 아니다.
- Geometry Script `append_buffers_to_mesh`, `create_new_static_mesh_asset_from_mesh`, `create_new_skeletal_mesh_asset_from_mesh`로 독립 메시를 만든다. 원본 Quinn은 수정하지 않는다.

발의 구조를 정밀한 의상 리토폴로지로 재작업한 것은 아니며, 갑옷이 있는 마네퀸의 치수를 기준으로 한다. 원본 영상의 피부·스타킹 광학 표현과 차이가 남는다.

## 애니메이션 연결

기존 UEFN의 `M_Relaxed_Walk_Loop_F`를 프로젝트에 있는 `RTG_UEFN_to_UE5_Mannequin`으로 Quinn에 리타기팅했다. 결과는 `/Game/Study/StockingPull/Animations/RT_Stocking_M_Relaxed_Walk_Loop_F`이며 길이는 약 4.1초다. 원본 애니메이션과 스켈레톤은 수정하지 않았다.

Geometry Script `transfer_bone_weights_from_mesh`의 closest surface 방식으로 Quinn LOD0의 웨이트와 본 정보를 스타킹에 전달한다. SkeletalMesh 생성 시 `use_mesh_bone_proportions=True`로 메시의 기준 본 비율을 사용한다.

`BP_StockingWalkPullStudy`의 Quinn은 Single Node 모드로 걷기를 반복한다. BeginPlay에서 좌우 스타킹의 `SetLeaderPoseComponent`를 Quinn으로 지정한다. 스타킹은 별도 클립을 재생하지 않고 Quinn의 최종 본 포즈를 공유한다. CPU 충돌과 천 솔버는 사용하지 않는다.

## 재질과 당기기

현재 정적 레벨은 `/Game/Study/StockingPull/Materials/M_StockingFishnet_Static_V8`, 걷기·당기기 레벨은 `M_StockingFishnet_Animated_V8`를 사용한다. V6/V7은 이전 재질로 보존한다. Translucent / Two Sided / Unlit이며 얇은 어두운 색의 층을 올린다. 과도한 반사 때문에 덮인 영역이 벗겨져 보이는 문제를 줄이기 위한 가시화용 재질이다. 물리 기반 나일론 광학 모델은 아니다.

당기기 레벨은 기존 `BP_StockingPullStudy`를 사용한다. 최초 복잡 충돌 결과로 선택한 컴포넌트에만 MID를 만들고, 마우스 광선과 카메라 평면의 교차점으로 변위를 구한다. 영향 반경은 9cm, 최대 변위는 18cm다. 중심 0.55cm부터 부드럽게 감쇠하고 UV 높이로 위·아래를 고정한다.

```text
WPO = PullDelta × radialWeight × boundaryMask × response
response = (1 + omega × t) × exp(-omega × t)
omega = 17, t = gameTime - ReleasedAt
```

당기는 중에는 응답을 1로 고정하고, 놓은 뒤에는 위 식으로 감쇠한다. `IsHeld`일 때만 놓기를 처리해 빈 클릭이 이전 복귀 시간을 바꾸지 않게 한다. 늘어남에 따라 불투명도를 낮추는 근사를 사용한다.

## 촘촘한 망사 텍스처

내장 image_gen으로 흰 실 / 검은 구멍의 다이아몬드 망사 마스크를 생성했다. 원본과 정확한 최종 프롬프트는 [source-art/generation-prompt.md](source-art/generation-prompt.md)에 보관한다. 선택된 PNG는 `source-art/T_FishnetFine_Opacity_V1.png`이며 원본 픽셀을 수정하지 않고 복사했다. 첫 투명 PNG 시도는 구멍의 잡티 때문에 사용하지 않았다.

- UE 텍스처: `/Game/Study/StockingPull/Textures/T_FishnetFine_Opacity_V1`
- RGB 1254x1254 원본을 R 채널 마스크로 사용한다. sRGB는 끄고 Masks 압축, Mirror X/Y, Trilinear, Simple Average mip을 사용한다. 2의 거듭제곱이 아닌 원본 해상도는 StretchToPowerOfTwo 빌드 설정으로 2048x2048로 처리한다.
- `FishnetUTiling=6`, `FishnetVTiling=10`으로 의상 UV에 반복한다. 무늬가 정점 UV에 붙어 있으므로 스키닝과 WPO가 표면을 움직일 때 무늬도 함께 늘어난다.
- 기존 당김의 불투명도/밀도 감쇠를 먼저 계산한 뒤, 구멍은 `BaseOpacity × 0.2`, 실은 `min(0.9, BaseOpacity × 2.0)`으로 보간한다. 기본 늘어나지 않은 영역에서 구멍 약 0.08 / 실 약 0.8은 식의 기본값 계산이며 GPU 픽셀 측정값은 아니다.
- 두 재질 모두 **Translucent / Two Sided**를 유지한다. Masked/Opaque로 바꾸지 않았다. 얇은 바탕이 비치고 실만 더 진하게 보이도록 구성했다.
- `FishnetMask`, `FishnetUTiling`, `FishnetVTiling`, `FishnetHoleOpacityScale`, `FishnetThreadOpacityScale`, `FishnetThreadTint` 파라미터로 무늬와 농도를 조절할 수 있다.
- V5 별도 StaticMesh/SkeletalMesh의 기본 재질 슬롯과 Blueprint/레벨의 의상 컴포넌트 재질에 적용한다. 원본 Quinn의 재질은 변경하지 않는다.

확대된 실제 PIE 화면에서 비침과 촘촘한 무늬, 당긴 영역의 UV 늘어남을 확인했다. 최신 근거는 `cubeless/records/verification/stocking-fishnet-20261111/report.json`을 따른다. 가까운 화면과 먼 화면에서 mip 필터에 따라 촘촘한 무늬의 대비는 달라진다. 전 플랫폼의 미세 패턴 앨리어싱 검증이나 물리 기반 섬유 광학 모델은 수행하지 않았다.

저작 스크립트는 `Saved/StockingPullStudy/import_fishnet_texture.py`, `build_fishnet_materials.py`, `assign_fishnet_v8.py`다. 생성 경로가 이미 있으면 중단하므로 전체를 반복 실행하지 않는다. 걷기·당기기 영상은 `render_fishnet_walk_pull_worker.py`로 실제 PIE 포즈를 1/30초 간격으로 샘플링한다.

## 걷는 중 잡기와 표면 추종

1. Quinn의 Physics Asset을 Visibility 채널의 Query Only로 사용한다. 커서의 단순 충돌 결과에서 HitBoneName을 읽는다. 좌우 thigh / calf / foot / ball에 대응하는 기준 포즈 SceneComponent 태그가 있는 경우만 허용한다.
2. 충돌 지점을 현재 본의 월드 변환으로 역변환해 `GrabBoneLocal`에 저장한다. 같은 본의 기준 포즈 변환을 적용해 `GrabRestCenter`를 계산하고, 로컬 Z 0.15~89cm의 스타킹 범위인지 추가로 검사한다. 골반·상체는 선택하지 않는다.
3. 해부학적 `_r`은 음수 X의 `NylonLeft`, `_l`은 양수 X의 `NylonRight`에 대응한다. 선택한 컴포넌트의 MID만 갱신한다.
4. 재질은 `PreSkinnedPosition`과 `GrabRestCenter`의 거리로 영향 영역을 구한다. 그러므로 본이 움직여도 같은 기준 포즈 정점 영역을 계속 변형한다. 픽셀 셰이더의 늘어남/비침 계산에는 Vertex Interpolator를 거쳐 해당 좌표를 전달한다.
5. `DragOrigin`에 최초 클릭의 월드 좌표를 저장한다. 마우스 광선과 그 위치의 카메라 평면 교차점으로 상대 당김량을 계산한다. 마우스를 멈추면 당김량은 유지되고 표면은 스키닝을 따라 움직인다. 최대 18cm 제한은 그대로 적용한다.
6. 디버그 월드 앵커 `AnimatedCenter`는 현재 본 변환에 `GrabBoneLocal`을 적용해 갱신한다. Quinn의 소유 액터 선행 조건을 제거하고, 액터가 Quinn 컴포넌트를 기다린 뒤 Post Update Work에서 갱신되도록 했다. 이 Study는 캐릭터 액터 자체의 이동 제어를 하지 않는다.
7. 놓을 때 복귀 시간을 한 번 기록한다. 기준 포즈 마스크가 유지되므로 걷는 중에도 같은 표면 영역이 감쇠 복원된다. R은 선택 MID의 당김을 0으로 초기화한다.

이 선택은 Physics Asset의 캡슐/구를 사용하는 **근사 표면 선택**이다. 렌더 삼각형의 정확한 barycentric 좌표를 구하는 기능은 아니다. 무릎처럼 여러 본의 웨이트가 섞이는 곳에서 디버그 본 앵커와 실제 변형 정점의 중심이 완전히 일치한다고 보장하지 않는다. 실제 변형 영역은 기준 포즈 좌표에 고정되어 스키닝을 따른다.

## 걷기·당기기 검증

같은 UE 5.8.4 Editor의 실제 PIE 키/마우스 입력으로 27단계 연속 검증을 수행했다. 걷기를 멈추지 않고 오른쪽/왼쪽 허벅지 선택, 약 14cm 당김, 18cm 제한, 놓기·감쇠 복원, 빈 클릭의 복귀 재시작 방지, 골반 선택 거부, 좌우 MID 독립, R 초기화를 확인했다. 종아리와 반대쪽 발도 추가로 잡고 약 10cm 당김을 확인했다.

121개 유지 샘플에서 기준 포즈 중심과 선택 MID가 바뀌지 않았다. 월드 앵커는 시작점으로부터 최대 약 28.3cm 이동했고, 현재 본 변환으로 계산한 위치와의 오차는 0cm였다. 이 값은 본 앵커 측정이며 전 정점의 충돌/관통 검사 수치가 아니다. 갱신 순서를 바꾸기 전의 약 1.57cm 지연 오차 기록도 보존한다.

영상은 실제 PIE에서 마우스 누르기와 당김을 적용한 뒤 걷기 포즈를 1/30초 간격으로 샘플링한다. 각 PNG 저장을 확인하고 다음 포즈로 진행한다. 실시간 성능 측정이나 실시간 30fps 화면 녹화는 아니다. 최신 결과는 `cubeless/records/verification/stocking-walk-pull-20261111/report.json`을 따른다. 원본 증거는 `Saved/StockingPullStudy/WalkPullV7*`에 있다.

## 확인과 한계

초기 순수 걷기 추종 기록은 `cubeless/records/verification/stocking-follow-20261111/report.json`, 최신 걷기·당기기 기록은 위의 보고서를 따른다. 로컬 제작 스크립트와 원본 증거는 `Saved/StockingPullStudy/`에 보존한다. 자체 검증이며 독립 QA가 아니다.

- 초기 웨이트 메시의 반사 표현을 불투명도 1과 본체 숨김 상태로 비교했다. 넓게 벗겨져 보이는 현상을 모두 관통이라고 단정하지 않았다.
- 걷기 중 양쪽 스타킹의 Leader Pose와 허벅지·종아리·발 본 위치를 확인한다.
- 영상은 실제 PIE에서 애니메이션 시간을 1/30초씩 샘플링한 PNG 시퀀스로 제작한다. 실시간 30fps 화면 녹화는 아니다. 각 PNG 저장을 확인한 후 다음 포즈로 넘어간다.
- 초기 캡처는 스크린샷 요청보다 저장이 늦어 프레임이 빠졌다. 원본 기록은 보존하고 파일 저장 대기를 추가한 캡처만 최종 영상에 사용한다.
- WPO 표면에 맞춰 CPU 충돌을 갱신하지 않는다. 변형된 표면의 정확한 재선택, 몸·손·다른 천 충돌, 여러 지점 잡기, 주름 솔버는 없다.
- 다른 애니메이션·극단적인 관절 각도·다른 LOD·플랫폼·다수 캐릭터 성능과 모든 시점의 반투명 정렬은 검증하지 않았다. 미세한 관통 가능성이 남는다.
- cook / package / MCP를 끈 독립 실행은 수행하지 않았다. C++를 추가하거나 커밋·푸시하지 않았다.

## 재현 자료

최신 상호작용 저작 코드는 `build_animated_pull_scaffold.py`와 부분 복구 스크립트, `wire_walk_pull_named.py`, `finish_walk_pull_wire.py`, `add_walk_pull_order_region.py`, `annotate_walk_pull.py`를 참고한다. 최종 실제 동작 검증은 `validate_walk_pull_worker3.py`, 종아리·발 검증은 `test_walk_pull_lower_worker.py`, 영상은 `render_walk_pull_worker.py`다. 머티리얼 V7에서 픽셀 셰이더의 PreSkinnedPosition 직접 사용은 컴파일 오류로 거부되었고 Vertex Interpolator로 수정했다.

기본 메시 생성 순서는 `export_fit_source.py` → `make_fitted_v5.py`(좌우 별도 단면) → `build_fitted_v5.py` → `retarget_walk.py` → Blueprint와 레벨 구성 → `render_follow_fenced.py`다. `material_follow_v6.py`는 최종 재질을 만든다. 부분 실패와 복구 스크립트가 함께 있으므로 전체를 일괄 재실행하지 않는다. 생성기는 새 이름만 만들며 기존 경로에서 중단한다.

이전 V2 검증 보고서와 애셋은 역사 기록으로 보존했다. 초기 스켈레톤 재지정 시도는 읽기 전용 속성으로 거부되어 기존 IK 리타기팅 경로로 전환했다. 그래프 가져오기 도구가 후조건 검증을 거부한 경우 붙여진 노드를 재검사한 뒤 실제 상태를 기준으로 필요한 연결만 완성했다.

## 참고

- [Unity Interactive Stocking Pulling — Sakura Rabbit](https://www.youtube.com/watch?v=QAli29nfTPg)
- [Epic: Material Inputs / World Position Offset](https://dev.epicgames.com/documentation/unreal-engine/material-inputs-in-unreal-engine)
- [Epic: Deformer Graph](https://dev.epicgames.com/documentation/unreal-engine/deformer-graph-in-unreal-engine)
