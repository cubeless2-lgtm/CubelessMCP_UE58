# 영상 기반 손가락 보강 조사 (2026-11-08)

## 현재 플러그인에서 확인한 점

test2의 저장된 Manny 애니메이션에서 양손30개 손가락 본을 31프레임(8프레임 간격) 표본 조사했다. 전부 로컬 회전 변화가 있고 손가락 리타깃 체인도 양쪽에 존재한다. 따라서 손가락 트랙이 누락돼 고정된 상황은 아니다. 최대 회전 변화138.57도는 움직임 존재의 근거이며 정확도·오류량을 뜻하지 않는다. 체인 존재만으로 축/기준 포즈의 정확성까지 검증한 것은 아니다.

`OfflineBodyTracker.h`에는 SMPL-X165축각 출력과 Hue의17점 body keypoint 입력이 있다. `OfflineBodyTracker.cpp`는133점 wholebody 출력을 받지만 Hue에는17점만 전달한다. 현 경로는 전신 추론을 통해 손 포즈를 포함하며, 원본 해상도 손 crop을 별도 손 전용 모델로 재추정하는 단계는 없다. 새 모델을 붙이기 전 손 체인의 기준 자세·축·좌우 매핑도 확인해야 한다.

근거: `cubeless/records/verification/cbm-finger-research-20261108.json`. 에디터에서 읽기만 했고 출력·설정·모델을 변경하지 않았다.

## 웹 후보

| 후보 | 출력·연결 방식 | 장점 | 제약·선택 판단 |
| --- | --- | --- | --- |
| MediaPipe Hand Landmarker | 손당21개2D/3D landmark, 좌우 분류 → 손바닥 기준축과 손가락 굽힘을 계산 | 가벼운 1차 보강 후보, 영상 추적 지원, 코드·공식 모델 카드 Apache2.0 | 곧바로 UE 본 quaternion이 나오지는 않음. 작은 손/가림/블러에서 손실·떨림 가능. 모델3D는 측정된 정답 좌표가 아님 |
| RTMPose Hand | 손 crop의21점2D → 기존3D 손 추정의 재투영 제약 | 공식 ONNX 제공, 기존 NNE/ORT 경로에 연결 검토 가능 | 2D만으로 깊이·비틀림 복원이 확정되지 않음. 코드 Apache2.0과 별개로 선택 모델·데이터 조건 확인 |
| HaMeR | 손 crop에서3D 손 메시·MANO pose → 손가락 로컬 회전 변환 | 정밀한3D 손 복원을 비교할 후보 | 코드 MIT지만 MANO는 별도 비상업 연구 조건. 모델 가중치도 코드와 따로 확인. 현재 환경 속도/VRAM 미측정 |
| WiLoR / WiLoR-mini | 손 검출+3D 손 복원 | 손 전용 비교 후보 | WiLoR 모델 CC-BY-NC-ND, MANO/검출기 별도 조건. mini는 간소화한 Python 패키징이며 작은 모델이라는 증거가 아님 |
| FAH | 손 관절·표면점, MANO/MHR-Hand fit, 영상 refinement | 손 전용+영상 후처리의 최근 비교 후보 | 코드 Apache2.0인 부분과 모델은 구분. 가중치 명시적 연구·비상업 제한. SAM/DINOv3/MANO 등 조건 존재. 공개 수치는 저자 장치의 결과이며 이 PC에서 검증하지 않음 |

SAM3D Body도 손을 포함하지만 전신 엔진 교체는 이번 손 보강의 최소 경로가 아니다. 기존 전신 결과를 보존하고 손만 후처리하는 방식을 우선 권고한다. 어느 후보가 test2에서 더 정확한지는 아직 실행 비교하지 않았다.

## 권고 연결 구조

1. 기존 전신 추출 결과를 재사용하고 원본 영상 해상도에서 손·손목 부근만 crop한다. 업스케일은 없던 손가락 정보를 복원하는 수단이 아니다.
2. MediaPipe를 빠른 경로 후보로 먼저 평가한다. 정밀 연구 비교에는 FAH/HaMeR 등 별도 백엔드를 사용한다. 상업 배포용 기본 경로와 비상업 연구 경로를 혼동하지 않는다.
3. 좌우 손 배정은 모델 label만 믿지 않고 기존 전신 손목·팔의 위치와 시간적 연결로 검증한다. 미러 영상 규칙을 확인한다.
4. 손바닥의 기준축을 잡고 타깃 본의 기준 포즈·각도 한계에 맞춰 local rotation을 푼다. MediaPipe world landmark의 원점은 손 중심이며 전신 월드 원점과 직접 합치지 않는다. 기존 손목 위치와 팔 모션을 보존하고 손목 회전은 별도 신뢰도에 따라 제한적으로 섞는다.
5. 손 검출/존재 점수, 재투영 오차, 시간적 일관성으로 결과를 선택한다. 좌우 검출 점수와 landmark별 정확도를 같은 것으로 취급하지 않는다. 유효하지 않은 구간은 짧게 보간하거나 기존 손 포즈로 완만하게 복귀하고 낮은 신뢰도를 표시한다.
6. Manny의 손가락30본에 추가한 회전을 별도 출력 에셋에 굽는다. 원본과 `Off / Fast / Accurate`를 비교할 수 있는 설계를 제안한다. 이 옵션은 아직 구현되지 않았다.
7. 에디터 밖 작업 프로세스로 짧은 표본을 먼저 비교한 후 필요하면 ONNX/NNE 통합을 검토한다. TFLite 모델을 ONNX로 무손실·무수정 변환할 수 있다고 가정하지 않는다.

test2는490×750 전신 영상이다. 손이 작거나 서로 가리는 프레임에서 완벽한 복원은 보장할 수 없다. 먼저 손이 잘 보임/주먹/손가락 펼침/겹침/빠른 동작 구간을 표본으로 비교하고 검출률·떨림·시각적 포즈 일치·추가시간/메모리를 측정하는 것이 효율적이다. 실제 성능·정확도 개선 수치는 아직 없다. 이번 작업은 조사이며 라이브러리/모델 설치와 구현은 하지 않았다.

## 1차 출처

- [MediaPipe Hand Landmarker 공식 문서](https://developers.google.com/edge/mediapipe/solutions/vision/hand_landmarker): 출력·영상 추적·벤치마크. Pixel6 수치를 현재 PC 처리시간으로 환산하지 않는다.
- [공식 손 모델 카드](https://storage.googleapis.com/mediapipe-assets/Model%20Card%20Hand%20Tracking%20%28Lite_Full%29%20with%20Fairness%20Oct%202021.pdf): Apache2.0, 입력 crop, 가림·블러 등 한계. 실제 배포에는 선택한 모델 파일의 버전·조건을 고정한다.
- [RTMPose 공식 모델 목록](https://github.com/open-mmlab/mmpose/tree/main/projects/rtmpose): Hand2d21점 및 ONNX.
- [HaMeR](https://github.com/geopavlakos/hamer), [코드 MIT](https://github.com/geopavlakos/hamer/blob/main/LICENSE.md), [MANO 조건](https://mano.is.tue.mpg.de/license.html).
- [WiLoR](https://github.com/rolpotamias/WiLoR), [모델 라이선스](https://github.com/rolpotamias/WiLoR/blob/main/license.txt), [WiLoR-mini](https://github.com/warmshao/WiLoR-mini).
- [FAH](https://github.com/noahcao/FAH), [가중치 조건](https://github.com/noahcao/FAH/blob/main/WEIGHTS_LICENSE), [제3자 조건](https://github.com/noahcao/FAH/blob/main/THIRD_PARTY_NOTICES.md).
- [SAM3D Body 공식 저장소](https://github.com/facebookresearch/sam-3d-body).

## 사용자 제공 GVHMR R&D 문서 대조

[초기 문서](https://app.notion.com/p/407dadb56b2f81cfb63cc1292251afaf)와 연결된 [v2](https://app.notion.com/p/408dadb56b2f816eb185d055f6a24721), [v3](https://app.notion.com/p/40bdadb56b2f81049463efb200f5dfde)를 읽었다. 아래는 문서의 자체 실험 보고이며 이 프로젝트에서 재현한 결과는 아니다. 첨부 키트 소스와 영상은 이번 대조에서 내려받거나 실행하지 않았다.

- 초기 문서7절은 WiLoR/HaMeR을 비교해 WiLoR를 선택하고, 손 검출→SMPL-X 손목·손가락→보간·평활→리타깃을 분리한다. 몸22+손가락30본 내보내기까지 보고한다. 현재 플러그인에도 손 모듈만 후처리로 연결하는 구조가 적합하다는 근거다.
- v2의1절은 WiLoR 손 모양이 ExMR보다10개 비교 중7개에서 우세했고, ExMR은 손 떨림이 적었다고 보고한다. 품질 우선 사내 R&D의 다음 벤치마크는 MediaPipe만 시험하기보다 기존 손/MediaPipe/WiLoR을 같은 test2 구간에서 비교하는 것이 유용하다. 이7/10을 다른 영상에서의 우세나 절대 정확도로 일반화하지 않는다.
- 초기 문서11절에는 손목–팔뚝 비틀림30–60도가 남은 과제로 기록돼 있다. 손 전용 모델을 붙인 것만으로 연결이 완성되지 않으므로 좌우·미러·손바닥 좌표계·손목 로컬 회전·타깃 기준 자세를 확인해야 한다. 현재 손목 위치·팔 체인은 보존하고 손가락부터 적용하는 단계적 접근을 권고한다.
- v2의2D 재정합과 v3의단계 분리는 리타깃 전/후 어디서 틀어졌는지 가르는 데 참고할 수 있다. 비교 뷰에 원본 손 확대/검출21점/타깃 손 확대를 같은 프레임으로 보여주는 기능을 제안한다. 아직 구현하지 않았다.
- 카메라·중력·축척·발 접촉 측정은 움직이는 카메라와 루트 모션 개선에 참고할 내용이다. 손가락 문제가 우선인 test2에 전신 엔진 교체나 SLAM 도입을 먼저 요구하지 않는다. v2의메타휴먼 처리시간은 하드웨어·입력·설정이 같지 않으므로 현재 속도 비교 근거로 사용하지 않는다.
- 최신 첨부는 v3 페이지의v13 키트다. 옛 v1 첨부로 시작하지 않는다. 라이선스는 문서도 사내 연구용으로 한정한다. 상업 배포 판단은 각 실제 코드·가중치·MANO 조건을 기준으로 한다.

현재 권고를 구분하면, 상업 배포를 염두에 둔 가벼운 경로 후보는 MediaPipe이고, 품질 우선 사내 R&D의 우선 비교 후보는 문서에 사례가 있는 WiLoR다. 키트 코드를 실제로 검토하기 전에는 수정 없이 재사용 가능하다고 단정할 수 없다.
