# MetaHuman 플러그인 없이 전신 모션 추출 분리 가능성

조사일: 2026-11-07 (Asia/Seoul). 상태: `source_reviewed`.

## 판단

설치된 코드에는 영상에서 SMPL-X 포즈·이동·체형을 추정하는 부분과 MetaHuman용 출력 부분 사이에 분리 가능한 경계가 있다. 기존 추론 코드와 모델을 재사용하고 처리 실행기·자료형·에셋·리타기팅·애니메이션 저장을 교체하는 별도 에디터 플러그인이 기술적으로 유력하다. MetaHuman 캐릭터나 Creator는 추론의 필수 입력이 아니다.

현재 파일을 그대로 복사하거나 `.uplugin`의 의존성만 삭제해서는 작동하지 않는다. 모든 MetaHuman 계열 플러그인을 끈 빌드·에셋 로딩·실제 영상 추출은 아직 검증하지 않았다. 따라서 완전 분리 성공이나 기존 솔버와 동등한 품질을 확정하지 않는다. 에픽 코드·모델의 재사용이므로 기술적 플러그인 독립성과 라이선스 독립성은 별개다.

## 조사 대상과 방법

- 프로젝트: `C:/Git/UnrealMCP5_8_3/UnrealMCP5_8_3/UnrealMCP5_8_3.uproject`.
- 엔진: `C:/Program Files/Epic Games/UE_5.8`, UE 5.8.4, CL 58902052, Compatible CL 55116800.
- 설치 플러그인: `Engine/Plugins/Marketplace/MetaHumanBodyTracker_5.8`, descriptor VersionName 1.0.0, EngineVersion 5.8.0.
- 소스 루트 `B`: 위 플러그인의 `Source/`.
- 코어 루트 `C`: `Engine/Plugins/MetaHuman/MetaHumanCoreTechLib/Source/`.
- descriptor, Build.cs, 직접 include, 추론 호출, 모델 경로, 후처리와 출력 코드를 읽었다. 선택한 uasset의 ASCII 이름 테이블을 읽어 직렬화 클래스 참조를 확인했다. 이는 에디터의 재귀 참조 검사나 실제 로딩 검증을 대체하지 않는다.
- 주요 소스 지문은 인접 `source-inventory.json`에 기록한다. 설치된 소스에 대한 결론이며 기존 DLL과 소스가 일치한다고 검증한 것은 아니다.

## 실제 실행 경로

`B/MetaHumanBodyTracker/Private/MetaHumanBodyTracker.cpp:31`의 `ExtendPipeline`이 오프라인 처리를 다음 다섯 단계로 구성한다.

1. 카메라 보정: GeoCalib으로 초점거리·pitch·roll 추정.
2. 사람 검출·추적: Detectron2와 SAM2로 프레임별 박스와 유효 여부 수집.
3. 2D 관절 추정: ViTPose와 ViTPosePost.
4. 3D 동작 추정: CHMR backbone/head에서 특징을 생성하고 Hue step/finalStep으로 시계열 포즈·이동·체형·접촉 신뢰도를 추정.
5. 마무리: 체형 평균, 카메라/바닥/높이 변환, 선택한 발 고정 보정, SMPL-X 포즈를 뼈 변환으로 바꾸고 현재는 MetaHuman으로 리타기팅.

근거: 같은 파일의 단계 분기 51, 94, 127, 155, 187행. 얼굴 소스는 선택적이며 79행에서 유무를 검사한다. 얼굴 저장·복구·병합 노드는 전신 전용 실행기에 넣을 필요가 없다.

`B/BodyTracker/Public/OfflineBodyTracker.h`에는 별도의 `FOfflineBodyTracker::Make/EnqueFrame/GetResults` 편의 경로도 있다. 그러나 위 실제 통합 경로는 이 래퍼 전체를 호출하는 대신 `FEstimateKeypoints`, `FCHMR`, `FHue`를 단계별로 사용한다. 이 둘을 동일한 검증 경로로 간주하면 안 된다.

중간 출력은 165개 axis-angle 값(55관절 × 3), 이동 3개, 체형 10개와 접촉 logits 6개 등이다. 이 배열은 MetaHuman 메시를 필요로 하지 않지만, 대상 캐릭터에 곧바로 적용할 UE 뼈 트랙도 아니다. SMPL-X 기준 자세·관절 계층·좌표계·체형을 반영한 변환과 리타기팅이 필요하다.

## 의존성별 처리

| 부분 | 확인한 결합 | 분리 방향 |
| --- | --- | --- |
| Segmentation | Build.cs는 Core, ImageCore, NNE, CoreUObject, Engine. 검출·추적 모델 경로는 원래 플러그인 mount를 사용한다. | 코드 재사용 후보. 모델 경로와 모듈 이름, SAM2 가중치 행 구조체를 이관한다. |
| BodyTracker | Build.cs에 MetaHumanPipelineCore·MetaHumanCoreTech가 들어 있다. 직접 MetaHuman include는 조사한 해당 모듈에서 `Pipeline/DataTreeTypes.h`였다. | 필요한 이미지 자료형을 자체 형식 또는 FImage 기반으로 교체하고 include를 정리한다. 미사용으로 보이는 의존성 삭제는 빌드로 검증한다. |
| 이미지 자료형 | `C/MetaHumanPipelineCore/Public/Pipeline/DataTreeTypes.h:45`의 FUEImageDataType은 Width·Height·BGRA 바이트 배열이다. 헤더 전체는 얼굴·신뢰도·애니메이션 헤더까지 포함한다. | 이 단순 이미지 표현 때문에 MetaHuman pipeline 전체를 가져오지 않는다. 타입 이름만 바꾸는 경우에도 함수 시그니처·전처리를 함께 정리한다. |
| 실행기 | 기존 FPipeline/FNode/FPipelineData, UMetaHumanPerformance, 프레임 애니메이션 구조체와 결합돼 있다. | 다섯 단계의 자체 작업 실행기, 진행·취소·오류·부분 결과 관리와 중간 데이터 구조를 작성한다. |
| 보정 | MetaHumanBodyOptimizer에 TitanBodyOpt의 API·SMPL-X forward·목적함수·LBFGS 등 cpp/h가 제공된다. Build.cs에는 Eigen·simde·RigLogicLib가 있다. | 얼굴 기능 없이 재사용할 후보. 먼저 전체 경로를 보존하고, 이후 실제 호출에 필요한 파일만 줄인다. |
| RigLogicLib | `Engine/Plugins/Animation/RigLogic` 소속. pma 메모리와 trio 입출력 헤더 사용도 발견했다. | MetaHuman 이름의 플러그인과는 별개지만 엄격한 최소 구성에서는 남는 의존성이다. 완전 제거에는 필요한 유틸리티의 분리 또는 교체가 추가로 필요하다. |
| MetaHuman 출력 | BaseBodyTracker가 SMPL-X 및 MH 메시·스켈레톤·IK Retargeter를 만든다. MetaHumanSMPLX.cpp:946은 MetaHumanCharacter의 IdentityTemplate/SKM_Body를 로드한다. | MH 메시 생성·크기 조정·MH 리타기팅을 제외하고 SMPL-X → 대상 캐릭터의 IKRig/IKRetargeter로 교체한다. |
| 저장 | 기존 Performance·Sequencer 출력과 연결된다. | 자체 UI와 표준 Animation Sequence 저장을 구현한다. 결과에는 제작 플러그인 클래스·에셋 참조를 남기지 않는다. |

NNERuntimeORT는 유지해야 할 일반 엔진 플러그인 후보다. 소스는 NNERuntimeORTDml GPU와 NNERuntimeORTCpu CPU 경로를 요청하며, Hue final step에는 CPU 경로를 직접 사용하는 부분도 있다(OfflineBodyTracker.cpp:321). 모든 추론을 GPU에만 올리는 구현으로 설명하면 틀린다. MP4 디코딩·프레임 시각/FPS 관리도 별도로 연결해야 한다. 추론 API는 파일 경로 대신 디코딩된 프레임을 받는다.

## 모델·에셋 이관이 필수인 이유

디스크에서 측정한 폴더 크기:

| 경로 | 크기 |
| --- | ---: |
| Content/Models/Offline | 약 4.085 GiB |
| Content/Detectron2 | 약 0.456 GiB |
| Content/SAM2 | 약 0.244 GiB |

합계 약 4.79 GiB이며 위 값은 해당 폴더 내 모든 파일의 디스크 길이 합계다. SMPL-X 데이터 약 111 MiB, GMM prior 및 다른 에셋은 별도다. 이는 GPU 메모리 요구량이나 최종 배포 크기 측정치가 아니다. 모델을 재학습해야 한다는 증거는 없지만, 기존 패키지의 재사용·이관·로드 성공 여부를 검증해야 한다.

- 추론 모델 경로: OfflineBodyTrackerNode.cpp:200, 207, 296, 303, 310, 316; 카메라 모델: OfflineCameraCalibrationNode.cpp:31.
- Detectron2.cpp:16–20, SAM2.cpp:23–31은 `UE_PLUGIN_NAME`으로 원래 mount 경로를 구성한다.
- `SMPLX_NEUTRAL_2020_locked_head_array_f32.uasset`의 이름 테이블에서 `/Script/MetaHumanBodyTracker.MetaHumanSMPLXData`를 확인했다. 클래스 정의는 같은 플러그인의 MetaHumanSMPLX.h:13에 있다.
- `SAM2/sam2_weights.uasset`의 이름 테이블에서 `/Script/Segmentation.SAM2WeightRow`를 확인했다.
- 따라서 모듈을 새 이름으로 만들면 이 두 패키지는 새 클래스/구조체에 맞춰 변환·재저장하거나 별도 데이터 형식으로 이관해야 한다. 모델·클래스 redirect와 콘텐츠 경로 수정도 검증해야 한다. 단순 파일 복사는 독립성의 증거가 아니다.
- `ViTPosePost.uasset`의 확인된 클래스는 `/Script/NNE.NNEModelData`다. 모든 모델의 재귀 참조까지 검증한 것은 아니다.
- BodyTrackingOptimizer.cpp:321 부근은 플러그인 Content의 `gmmPriorExport.json`을 읽는다. 원본 플러그인 탐색에 기대지 않게 경로를 교체한다.
- 기존 `RTG_SMPL_MH.uasset`는 원래 플러그인의 IK_Metahuman·IK_SMPL·SKM_Body·SKM_SMPL을 참조한다. 일반 캐릭터용 새 리타기팅 구성을 만든다.

## 권장 구현 경계

하나의 별도 Editor 전용 플러그인 내부에 추론·보정·편집 UI를 둔다. 원본의 Runtime 모듈 표기를 그대로 복제할 필요는 없고, 게임에는 생성된 일반 Animation Sequence만 사용한다. 이 도구는 MCP로 실행할 수 있어도 MCP를 추론 코어나 결과물의 필수 의존성으로 만들지 않는다.

목표 경로:

`영상 디코딩 → 자체 이미지 자료형 → 검출/추적 → ViTPose → CHMR/Hue → 카메라·바닥·발 고정 보정 → SMPL-X 뼈 변환 → 대상 캐릭터 리타기팅 → Animation Sequence`

Creator, 얼굴 추적·표정 병합, Live Link, MetaHuman Performance 에셋/UI, MetaHuman 메시 생성 경로를 제외하는 설계다. 같은 솔버의 보정까지 유지하려면 optimizer를 생략하지 않는다. 이를 빼면 원시 동작 추출 시제품은 만들 수 있어도 원본 결과와 동등하다고 주장할 수 없다.

이번 조사에서는 원본 엔진·플러그인 코드와 프로젝트 설정을 수정하지 않았다. 새 플러그인 구현은 이번 조사 범위에 포함하지 않았다. 현재 프로젝트의 C++ 작업 정책에 맞춰 구현 위치와 승인 범위는 실제 구현 시 별도로 확정한다.

## 라이선스 확인 범위

2026-11-07 공식 페이지를 열어 확인했다.

- [MetaHuman 라이선스 안내](https://www.metahuman.com/license): 표준 UE 라이선스에 포함된다고 안내한다.
- [UE EULA](https://www.unrealengine.com/eula/unreal): 적용 대상 Licensed Technology의 내부 수정과 배포 조건을 구분한다. Engine Tools 공개 배포에는 별도 유통 경로 제한이 있다.
- [Epic Content License](https://www.unrealengine.com/eula/content): 플러그인에 대한 부속 조건과 코드/콘텐츠 적용 범위를 확인해야 한다.
- [Fab 제품 페이지](https://www.fab.com/listings/4095b8e0-3eff-44f1-acb4-cb40b99228b9): 이번 도구가 읽은 본문의 License terms는 `-`로 나타나 이 페이지 추출만으로 개별 취득 약관을 확정하지 못했다.

설치 폴더에는 Detectron2, DINOv2, GeoCalib, SAM2, ViTPose 등의 라이선스 파일과 학습 데이터 attribution notice가 있다. 파일명·초기 라이선스 표기를 확인했지만 모든 코드·가중치·SMPL-X 데이터·GMM prior의 외부 재배포 허용을 확정한 감사는 아니다. 공개 배포 가능/불가능 어느 쪽도 이번 조사만으로 단정하지 않는다. 재사용 시 저작권·라이선스·고지 파일을 보존한다.

## 실제 분리 성공 판정에 필요한 검증

1. 원본을 수정하지 않고 이 프로젝트 안의 분리 플러그인을 정확한 엔진 버전으로 빌드한다.
2. 새 코드의 Build.cs·헤더·동적 모듈 로딩·문자열 경로에 MetaHuman 원본 모듈이 남지 않는지 검사한다. 관련 플러그인의 자동 의존성 활성화도 검사한다.
3. 모든 MetaHuman 계열 원본 플러그인을 끈 같은 프로젝트의 새 에디터에서 새 모델·SMPL-X·SAM2 데이터 로드를 확인한다. 기존 DLL이 이미 로드된 세션으로 대체하지 않는다.
4. 짧은 1인 전신 영상으로 추론, 창 경계/마지막 창 처리, 좌표계·크기·루트 이동, 발 고정, 취소와 인식 실패 경로를 확인한다. FPS의 정확한 전달·학습 시간축에 대한 적합성도 검증한다.
5. 프로젝트 캐릭터용 Animation Sequence를 저장·재로드·재생하고, 모델·원본 MetaHuman·새 제작 플러그인으로의 직렬화 의존성이 없는지 검사한다.
6. 제작 플러그인과 MetaHuman을 끈 패키지에서 애니메이션이 재생되는지 확인한다. 원본 솔버와의 품질 동등성은 같은 영상의 비교 검증으로 따로 판정한다.

빌드, 에디터 실행, 모델 로딩, 영상 추론, 패키징, 독립 QA는 모두 `not_run`이다. 이 문서는 소스 기반 타당성 조사와 구현 설계 근거이며 기능 완료 보고가 아니다.


## 2026-11-08 test3 추가 검증

[별도 영상 test3의 모델·Manny 비교](Test3Review20261108.md): 전체604프레임 추출과 두 손 보강본 저장 완료. MediaPipe의14.03초·17.3초 손 형태 실패를 확인했으며, 검사 구간에서는 WiLoR가 더 안정적이었다. 공통 코드·학습 가중치·관절 제한은 변경하지 않았다.


## 2026-11-08 MediaPipe 관측 기하·깊이 반전 보완

[공통 코드 수정과 test2/test3 회귀 비교](HandGeometry20261108.md): test3 14.03초·17.3초의 손 형태를 개선했다. 관측 기하 검사·조건부 문맥 재검출·영상 증거를 이용한 단독 깊이 반전 거절을 추가했다. 관절 제한 없음. test2 주요 구간과 원본 복귀 범위를 유지했으나 test3의 다른 오른손 구간에는 시간적 변동 한계가 남는다.

## 2026-11-08 오른손 후보 식별 및 다리 조사

[오른손 공통 수정과 다리 단계별 진단](HandIdentity20261108.md): test3 13.83초 주변의 팔찌 오검출을 독립적인 2D 손 위치 근거로 거절했다. test2/test3 재실행과 손 전후 비교 완료, 비손가락123트랙 보존. 다리는 원시 추정과 IK 리타깃 양쪽에 굽힘이 있으며 시험 설정만으로 최종 해결했다고 판단하지 않았다. 검증 스크립트 오류로 에디터 종료 후 같은 프로젝트를 재실행했으며 패키지 복원 창 처리가 남아 있다.

## 2026-11-08 체형 기준 다리 리타깃 수정

[공통 리타깃 수정과 발 높이·미끄러짐 검증](LegRetarget20261108.md): 체형을 반영한 바인드와 소스 애니메이션 기준을 일치시켜 test3 지지 무릎42.29→30.00°, 소스29.85°에 맞췄다. test2/test3 전체 프레임 검증과 손 보강 재적용 완료. 원시 모델 깊이 오차, 일부 발 침투·들뜸·미끄러짐은 남는다. 관절 제한 없음. 복원 창 후속 작업을 마치고 같은 에디터에 test3 20.10초 결과를 표시했다.


## 2026-11-08 FootMR 접지 보완

[실제 신발 형상과 시간축 접지 보완](FootContact20261108.md): 전신·HandFlow·발가락 로컬 회전을 보존하면서 FootMR 적용 후 침투와 미끄러짐을 줄였다. test2/test3 전체 구간 비교, 남은 test3 오른발 미끄러짐과 측정 조건을 기록했다. 관절 제한 없음.


## 2026-11-08 잔여 미끄러짐과 골반·다리 영상 보완

[발목 전용 수정과 선택형 하체 보완](LowerBody20261108.md): test2/test3 전체 프레임 비교. 기존 전신 보존 모드와 골반·다리 변경 모드를 구분하며 HandFlow와 상체 월드 자세를 보존한다. 관절 제한 없음. 남은 침투·시간 안정성·처리 비용을 함께 기록했다.


## 2026-11-08 하체 보완 속도·접지 개선

[하체 최적화와 전후 검증](LowerBodyOptimization20261108.md): 모든 프레임을 유지하는 대역 행렬 해법으로 test2 61.9→9.6초, test3 166.5→17.9초. 기존 HandFlow 데이터 보존, 침투·대부분의 회전 변화 개선. 최대 약1.48cm 침투와 test3 오른발 미끄러짐은 남는다.


## 2026-11-08 FootMR 제거와 하체 단독 보완

[하체 단독 경로와 원본 비교](LegsOnly20261108.md): FootMR UI·코드·전용 모델 제거. 기존 전신+HandFlow에서 골반·다리만 보완하고 발목 월드 방향을 보존한다. test2/test3 전체 검증,9.03/12.89초. 영상 자세 지표는 개선됐지만 일부 접지는 원본보다 악화돼 기본값Off 유지.


[하체 깊이 순서와 접지 회전 보완](LegDepthAndSupport20261108.md): 옆모습에서 하체 조정이 다리 앞뒤를 뒤집던 오류를 수정하고 지지 구간 발목·앞꿈치 회전 옵션을 추가했다. FootMR·관절 제한 없이 test2/test3 검증, 접지 포함4.5/10.2초. HandFlow와 공중 발 방향은 보존하지만 일부 미끄러짐·원본 회전 오류는 남는다.


[test2·test3 접지 재검토](SupportReview20261108.md): 밑창 중심 이동과 최종 출력 회전의 연속성으로 공통 보완을 수정했다. 실제 Manny 지지 속도p95는 test2 좌/우24.45/10.62→9.59/8.93cm/s, test3 12.02/19.94→7.31/14.96cm/s. HandFlow·공중 발 방향·깊이 순서를 보존하며 일부 원본 회전 오류는 남는다.

[전체 하체 시간 연속성 검증](TemporalFeet20261109.md): 허용된 공중 회전 오류의 선택적 보완, 최종 위치·회전 연속성, 미리보기 프레임 사이 보간을 추가했다. test2/test3 하체 평균 회전 불균일은 27.2%/26.7% 감소했다. HandFlow를 유지하며 관절 제한은 없다. 일부 빠른 동작 약화와 접지·미끄러짐의 상충 지표를 포함해 기록했다.

[HandFlow 중심 UI와 하체 환경 분리](HandFlowOnly20261109.md): 일반 손 선택을 없음/HandFlow로 정리하고 WiLoR·MediaPipe를 진단 전용으로 유지했다. 독립 하체 환경의 기존 결과 재현이 정확히 일치했다. 작은 접지 가중치 보완으로 test3 좌/우 미끄러짐 p95가7.49/14.66→6.59/13.24cm/s로 감소했다. 네이티브 결과 보존·전체 재생, 남은 상충 지표 및 점검 스크립트 오류로 인한 에디터 종료와 복구를 기록했다.


[영상 정지 발의 미세 미끄러짐 보완](StationaryFeet20261109.md): 공통 정지 관측을 하체·접지 적합에 함께 사용했다. test2 마지막 0.3초 밑창 이동은 좌/우 0.606/0.400→0.116/0.074 cm, test3 결과와 HandFlow는 유지된다. 전체 구간 p95의 소폭 증가 및 test3 추가 재생 검사 seek timeout은 보고서에 기록했다.


[접지 전환과 잔여 미끄러짐](TransitionFeet20261109.md): 정지 신호 전환, 실제 접지 표본 이동, 약한 경로 연속성을 보완했다. test2 전환 이동은 좌/우 약 16/20% 감소했고 test2·test3 지지 속도 p95는 약 4~9% 감소했다. HandFlow 보호 트랙은 동일하며 잔여 미끄러짐과 일부 회전 지표 상충은 남는다. 두 영상 전체 재생 완료.

[Quinn Control Rig 편집과 재생 UI](ControlRigPlayback20261115.md): 원본 보존 편집본과 새 애니메이션 저장, 실제 허리 키 보정/왕복 검증, 두 번째 재생 복구 및 빨간 재생 막대.
