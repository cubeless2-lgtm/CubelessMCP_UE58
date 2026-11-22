# SoftbodySystem 구현 실습

원본 `/Game/SoftbodySystem`의 동작을 `/Game/Study/SoftbodySystem`에서 기능별로 재현한다. 메시, 스켈레톤, 애니메이션, 텍스처는 원본을 참조한다. 각 실습의 공통 구현은 `Shared/`에 둔다. 기능별 레벨은 `Maps/`에서 바로 실행할 수 있도록 만들고, 레벨별 비교 입력과 보조 에셋은 `Lessons/`에 둔다.

| 순서 | 레벨 | 학습 범위 | 상태 |
| --- | --- | --- | --- |
| 01 | `L_01_DeformOutput` | Render Target, 삼선형 보간, WPO | 구현·리뷰·수정·PIE 비교 완료 |
| 02 | `L_02_GridDebug` | Grid3D 초기화, 좌표 변환, 디버그 스프라이트 | 구현·리뷰·수정·PIE 비교 완료 |
| 03 | `L_03_ElasticResponse` | 복원 계산, `WriteRT`, Blueprint와 머티리얼 연결 | 구현·리뷰·수정·PIE 비교 완료 |
| 04 | `L_04_ColliderInput` | 충돌체 등록, Data Channel, 접촉 반응 | 구현·리뷰·수정·PIE 비교 완료 |
| 05 | `L_05_ShapeAndPressure` | 큐브 변형, 대형 데모, 압력 업데이트 | 구현·리뷰·수정·PIE 비교 완료 |
| 06 | `L_06_GrabAndReticle` | 조준, 잡기, 끌기, 놓기 | 구현·리뷰·수정·PIE 입력 검증 완료 |
| 07 | `L_07_SkeletalAndMask` | 애니메이션 충돌체, 좌우 변형, 버텍스 컬러 마스크 | 구현·리뷰·수정·PIE 비교 완료 |
| 08 | `L_08_Integration` | 여러 액터의 변형과 입력 분리 | 구현·리뷰·수정·PIE 입력 검증 완료 |

각 단계에서는 원본의 설정과 입력을 확보한 후 Study를 구현한다. 원본 대조 리뷰에서 발견한 차이를 수정한 뒤, 저장한 레벨을 다시 열고 PIE에서 동작과 한글 안내를 검증한다. Blueprint, Niagara, 머티리얼, HLSL에는 그래프를 따라갈 수 있도록 해당 에디터에서 지원하는 주석을 남긴다.

## 01 변형 출력

원본은 `Core/MF_PPSoftbodyDeform`, `Material/M_SoftbodyDemo`, `Texture/RT_SoftbodyPreview`, `Demo/StaticMesh/SM_ICOSphere`다. Study의 `Shared/Materials/MF_StudySoftbodyDeform`과 `M_StudySoftbody`는 별도 그래프로 재구성했다. 회전된 기준축에 변형 전 정점 좌표를 투영하고, 64 × 64 RGBA32F Render Target의 4 × 4 타일에서 16³ 그리드의 인접한 8개 셀을 읽는다. 보간한 RGB 변위에 그리드 경계 감쇠와 구형 마스크를 적용한다. A 그룹이 기본이며, 활성화된 B·C·D 그룹의 결과를 합산해 WPO에 전달한다.

`Maps/L_01_DeformOutput`을 열어 PIE를 실행하면 원본 구체 메시 세 개를 비교할 수 있다. 왼쪽은 RGB가 모두 0인 입력을 쓰고, 가운데와 오른쪽은 같은 Render Target의 RGB `(0, 0, 55)`를 원본 머티리얼과 Study 머티리얼에 각각 연결한다. 입력을 바꾸려면 `Lessons/01_DeformOutput/RT_L01_Up`의 Clear Color를 수정하고, 원본과 Study 재질 인스턴스가 같은 Render Target을 가리키는 상태에서 다시 비교한다.

- UE 5.8.4에서 변형 함수와 머티리얼을 컴파일했고 오류가 없었다. 19개 변형 함수 노드, 40개 원본 대응 머티리얼 노드, 원본 출력의 기본색을 명시하는 추가 노드 1개를 만들었다. 머티리얼 코멘트 박스 5개와 HLSL 설명을 추가했다.
- 머티리얼 그래프 조회가 같은 노드의 두 출력 핀을 구별하지 못하는 경우가 있었다. T3D로 원본과 Study를 내보내 네 개 회전 그룹에서 `XYZ`와 `W`가 각각 올바른 핀에 연결됐음을 확인했다.
- 첫 PIE 리뷰에서 한글이 사각형으로 보였고 Study 머티리얼에 원본 출력의 인라인 기본색이 빠져 있었다. NanumGothic 오프라인 글꼴의 알파를 읽는 공용 텍스트 머티리얼을 만들고 기본색을 추가했다. 안내 문구 간격을 조정한 뒤 다시 연 레벨에서 한글 제목과 안내, 세 구체의 비교 라벨을 확인했다.
- 같은 상대 위치에서 Unlit PIE 화면을 캡처해 조명 차이를 배제했다. 1,280 × 720 이미지의 구체 영역을 밝기 26 초과로 분리한 결과, 원본과 Study의 실루엣이 각각 28,612픽셀이었고 두 마스크의 IoU는 1.0이었다. 무변위 입력과 Study 결과의 IoU는 약 0.627이었다. 이 비교는 같은 정적 변위 입력의 실루엣에 한정된다. 시간에 따른 Niagara 변형은 03단계부터 확인한다.
- 원본 조회 중 유효하지 않은 머티리얼 출력 항목을 API에 넘기는 경로에서 에디터가 종료됐다. 조회 범위를 엔진 소스에서 확인한 출력 항목으로 제한하고 같은 프로젝트의 에디터를 다시 시작했다. 종료 직전 저장 대기 패키지는 없었다.

로컬 검증 자료는 `Saved/SoftbodyStudy/`에 있다. `stage01_review_overview.png`는 PIE 시작 화면이고, `stage01_unlit_silhouette.json`은 실루엣 비교 결과다. `capture_stage01_overview.json`에는 시작 카메라, 안내 컴포넌트, Render Target 픽셀의 읽기 결과가 있다.

## 02 그리드와 디버그

원본은 `Core/NS_Grid3DSoftbodySim`, `Core/Grid3D_CreateUnitToWorldTransform`, `Demo/SoftbodyDemo_debug`다. Study에서는 Niagara 시스템과 좌표 변환 모듈을 별도 자산으로 옮기고, Scratch Pad와 스테이지의 연결을 보존했다. 시스템의 스폰 그래프를 Study 좌표 변환 모듈에 연결했고, 행렬 곱을 설명하는 HLSL 주석을 6곳에 넣었다. 두 시스템의 8개 스테이지 순서, 반복 대상, 실행 조건이 일치했다.

`Maps/L_02_GridDebug`을 열어 PIE를 실행한다. 왼쪽은 원본, 오른쪽은 Study다. 위에는 기본 자세, 아래에는 피치 19°, 요 30°, 롤 15°를 적용한 자세를 배치했다. 양쪽에 16³ 그리드와 같은 크기 `(122, 196, 145)`를 지정했고, 인스턴스별 Render Target을 썼다. 격자와 메시의 정렬을 보기 위해 원본 `SM_SubdividedCube`를 반투명 와이어프레임으로 표시한다. 한글 안내에는 01단계의 공용 안내판을 사용했다.

- 두 시스템에서 Niagara 컴파일 상태가 `UpToDate`였고 컴파일 오류가 없었다. PIE 로그에는 양쪽의 Grid3D에서 이름 있는 속성과 이름 없는 속성의 혼용을 알리는 같은 엔진 경고가 있었다. 원본과 동일한 스테이지 동작을 유지했다.
- 첫 PIE 리뷰에서는 메시 기준선이 빠져 있었다. 원본 메시를 가이드로 추가하고 제목과 안내의 크기를 조정했다. 저장 후 다시 연 레벨에서 안내와 네 인스턴스의 디버그 스프라이트를 확인했다.
- 같은 상대 카메라 위치에서 파란 디버그 셀의 픽셀 마스크를 비교했다. 기본 자세의 IoU는 0.9934, 회전 자세는 0.9877이었다. 비교 범위는 레벨에 배치한 위치와 회전이다.
- Data Channel의 Study 연결과 충돌 입력 비교는 04단계에 해당한다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage02_review_overview.png`와 `stage02_grid_comparison.json`이다.

## 03 탄성 복원

원본 `Core/SoftbodySimulation`은 `SoftbodyDemo`에서 Child Actor로 사용된다. 초기화 시 Niagara 사용자 파라미터를 설정하고 64 × 64 Render Target을 생성한다. 시뮬레이션은 `SoftbodySim`에서 셀의 이전 위치·속도, 스프링 힘, 외력, 감쇠를 계산하고 `WriteRT`에서 변위를 기록한다. 같은 Render Target을 부모 메시의 동적 머티리얼 `A_RT`에 전달한다.

Study의 `BP_StudySoftbodySimulation`과 `BP_StudySoftbodyDemo`는 원본 Blueprint의 그래프를 Study로 옮긴 뒤, Study Niagara 시스템·변형 머티리얼·Child Actor로 다시 연결했다. 공통 Blueprint에 코멘트 박스 5개를 추가했고, `SoftbodySim`의 HLSL에 이전 프레임 값, 복원력, 외력, 감쇠에 대한 설명을 넣었다.

`Maps/L_03_ElasticResponse`에서 왼쪽과 오른쪽에 같은 수직 왕복 입력을 준다. 원본의 작은 구체를 관찰할 수 있게 카메라를 가까이 배치하고, 두 구체의 Blueprint에 같은 이동 범위와 속도를 설정했다. 초기 PIE 리뷰에서 두 구체가 지나치게 작게 보였던 문제를 카메라와 레벨 배치로 수정했다.

- 새 에디터에서 주석을 저장하고 맵을 디스크에서 다시 열어 PIE를 실행했다. 각 Niagara 인스턴스가 별도의 Render Target을 사용하며, 각 부모 메시의 `A_RT`가 그 인스턴스의 Render Target을 가리켰다. 시작 화면에서 한글 안내와 두 구체의 변형을 확인했다.
- 기본값 `SpringK=1, Damping=2.5`, 감쇠 변경 `Damping=8`, 스프링 변경 `SpringK=3`, 왕복 입력 정지 후 안정 상태를 각각 12프레임씩 읽었다. 같은 프레임의 원본과 Study 간 RGB 채널 최대 오차는 전 구간에서 약 `1.91 × 10⁻⁶`이었다. 기본값의 평균 프레임 간 RMS는 0.0683, 감쇠를 높였을 때는 0.0497, 왕복을 멈춘 뒤에는 0.000238이었다.
- 주석 도구는 Study Blueprint의 외부 인스턴스가 로드된 상태에서 쓰기를 거부했다. 외부 인스턴스를 정리하려고 작성한 맵 전환 스크립트가 월드 참조를 유지해 엔진이 `World Memory Leaks`로 종료됐다. 이전 레벨과 03단계 변경은 저장돼 있었다. 같은 프로젝트를 새로 열고 실습 레벨보다 먼저 Blueprint에 주석을 추가해 검증을 이어갔다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage03_review_overview.png`와 `compare_stage03_runtime.json`이다.

## 04 충돌 입력

원본 `SoftbodyColliderDemo`의 박스, `SoftbodySimulation.GetColliderData`, `NDC_PhysicsBodyData`를 기준으로 삼았다. Study에 `BP_StudySoftbodyCollider`와 전용 채널 `NDC_StudyPhysicsBodyData`를 두었다. 채널의 `PrimitiveType`, `Center`, `HalfExtent`, `Rotation`, `Velocity` 필드를 보존했다. Blueprint의 쓰기 노드 두 개와 Niagara의 읽기를 Study 채널로 연결했다. 채널을 바꾸면 Blueprint가 해당 필드 핀을 다시 만들기 때문에 기존 입력 링크를 읽고 새 핀에 복원했다. `SoftbodyCollision` HLSL에는 콜라이더 버퍼가 공급하는 데이터와 역할을 적었다.

`Maps/L_04_ColliderInput`을 열어 PIE를 실행한다. 같은 시퀀스가 두 박스를 구체 위에서 같은 위치와 피치로 내려 접촉시킨 뒤 다시 올린다. 박스의 반투명 표시에는 02단계의 와이어프레임 머티리얼과 원본 `SM_SubdividedCube`를 사용했다.

- 저장한 맵을 다시 열고 59프레임을 읽었다. 원본과 Study 모두 34프레임에서 접촉했고, 25프레임에서 이탈했다. 접촉 여부가 서로 다른 프레임은 없었다. Render Target의 RGB 채널 최대 오차는 약 `1.91 × 10⁻⁶`이었다.
- 접촉한 화면에서 박스 바로 아래의 메시가 눌리는 모습과 한글 안내를 확인했다. 이 비교는 데모 박스의 접촉과 이탈 경로를 검증한다. 캐릭터의 손가락 캡슐은 07단계 범위다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage04_review_contact.png`와 `compare_stage04_runtime.json`이다.

## 05 형상과 압력

`SoftbodyDemo_Cube`와 `SoftbodyDemo_Large`를 Study에 옮겨 공통 데모 Blueprint를 부모로 연결했다. 두 프리셋은 Study 시뮬레이션을 상속하며 각각 원본의 형상, 고정 영역, 압력, 감쇠, 머티리얼 슬롯 값을 보존한다. 큐브에는 원본 `SM_SubdividedCube`를, 대형 데모에는 `SM_ICOSphere`를 사용한다. `PressureUpdate` HLSL에는 이웃 셀 거리에서 압력을 생성하고 확산·팽창 값을 계산하는 부분을 설명했다.

`Maps/L_05_ShapeAndPressure`을 열어 PIE를 실행한다. 위쪽은 큐브, 아래쪽은 대형 데모다. 같은 행에서 왼쪽 원본과 오른쪽 Study를 비교한다.

- 첫 PIE 리뷰에서는 상속된 Child Actor 템플릿이 두 프리셋 사이에서 공유되어 Study 큐브가 구형 설정으로 초기화됐다. 형상별 Child Actor 클래스에 기본값을 분리하고 공통 데모의 기본값을 원본과 일치하도록 복원했다. 큐브가 너무 작게 보여 두 큐브를 같은 거리만큼 카메라 가까이 옮겼다.
- 저장 후 다시 연 레벨에서 큐브의 `ShapeType=1`, 압력 확장값 `0.5`와 대형 데모의 `ShapeType=0`, 압력 확장값 `0.75`를 확인했다. 네 인스턴스의 동적 머티리얼 `A_RT`가 각 인스턴스의 Render Target을 가리켰다.
- 같은 설정의 12프레임에서 원본과 Study의 RGB 채널 최대 오차는 큐브 약 `3.81 × 10⁻⁶`, 대형 데모 약 `7.63 × 10⁻⁶`이었다. PIE에서 Study 대형 데모의 압력 확장값만 0으로 바꾼 13프레임에서는 두 대형 데모의 최대 차이가 3.49로 늘었다. 큐브의 최대 오차는 이전과 같았다. 제목, 관찰 안내, 비교 라벨을 화면에서 확인했다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage05_review_final.png`와 `compare_stage05_runtime_final.json`이다.

## 06 잡기와 조준

원본 `PPS_PlayerCharacter`, `PoyoPoyoSoftbodyTestGameMode`, `WBP_HandReticle`의 그래프를 Study로 옮겼다. 원본 `BPI_PPSoftbody` 계약, 손 조준 텍스처, 플레이어 메시를 재사용한다. 시야의 PhysicsBody를 추적하는 노드는 유지하고, 잡을 대상의 캐스트와 함수 호출, 저장 변수의 타입을 Study 시뮬레이션으로 연결했다. 잡기와 좌표 갱신을 따라갈 수 있도록 플레이어 그래프에 코멘트를 추가했다.

`Maps/L_06_GrabAndReticle`에서 PIE를 실행하면 첫 구체가 시작 시선 앞에 온다. 손 조준점을 맞춘 뒤 왼쪽 마우스 버튼을 누르고, 시선을 움직인 다음 버튼을 놓는다.

- 원본의 캐스트를 제자리에서 바꿨을 때 연결된 예전 핀이 남아 컴파일 오류가 발생했다. 이전 핀의 링크를 해제하는 스크립트에서 에디터가 접근 위반으로 종료됐다. 새 에디터에서 캐스트 노드 자체를 교체하고 연결을 확인한 뒤, 플레이어 Blueprint의 컴파일 오류가 없는 상태로 저장했다.
- 첫 PIE에서 카메라 높이와 대상 높이가 달라 시선이 구체를 벗어났다. 구체를 시작 시선에 맞추고 안내를 내렸다. 공용 한글 글꼴에 없는 영문 키 표기는 엔진의 기본 TextRender 글꼴로 표시했다.
- 저장한 맵을 다시 열고 PIE 컨트롤러에 왼쪽 마우스 누름과 놓기를 전달했다. 첫 대상의 `User.Grab`이 활성화됐고, 시선의 요를 6° 바꾸자 `GrabPoint`가 약 11.35 유닛 이동했다. 놓은 뒤 잡기 값은 비활성화됐으며 두 번째 대상은 전 구간에서 잡히지 않았다. Study 손 조준 위젯, 한글 안내와 `LMB` 표기를 화면에서 확인했다. 마우스 이동은 컨트롤러 회전으로 재현했다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage06_final_held.png`, `stage06_final_dragged.png`와 각 `stage06_phase_*.json`이다.

## 07 스켈레탈 메시와 마스크

Study 캐릭터 Blueprint에는 원본 스켈레탈 메시, 스켈레톤과 애니메이션을 연결했다. Manny의 손에 붙은 박스 2개와 손가락 캡슐 30개가 움직이며, Poyo의 두 Study 시뮬레이션이 각 소켓에서 동작한다. 별도로 재구성한 버텍스 컬러 머티리얼은 R과 G를 각각 A와 B 변형의 가중치로 사용한다. 캐릭터 그래프에 해당 연결을 설명하는 코멘트를 남겼다.

`Maps/L_07_SkeletalAndMask`에서 위쪽의 Manny와 Poyo로 애니메이션 충돌을 비교한다. 아래쪽은 같은 Poyo 메시의 마스크 보기다. `ShowVertexColor`는 BeginPlay에서 머티리얼에 적용되므로 아래쪽 두 인스턴스에서 미리 켰다. 첫 리뷰에서 겹치던 라벨을 캐릭터 위로 옮겼다.

- 저장 후 다시 연 레벨에서 원본과 Study 모두 A와 B에 서로 다른 Render Target이 할당됐다. 31프레임에서 각 손이 감지한 16개 콜라이더의 이름이 일치했다. 아래쪽 두 인스턴스는 `VertexColorAlpha`가 1.0이었고, 화면에서 양쪽 R/G 영역을 확인했다.
- 충돌체 순서와 Render Target 픽셀은 인스턴스에 따라 달라졌다. 같은 원본을 한 쌍 더 둔 31프레임 대조에서도 같은 크기의 변동이 나타났다. A/B의 프레임별 최대 절대 오차를 평균하면 원본과 Study는 4.35/2.91, 원본 두 쌍은 4.26/2.94였다. 이 비교에서 픽셀 단위 일치는 확인되지 않았다. 대조군은 검증 후 레벨에서 제거했다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage07_review_saved2.png`, `compare_stage07_final.json`과 `compare_stage07_with_reference_control.json`이다.

## 08 통합 데모

`Maps/L_08_Integration`을 열어 PIE를 실행한다. 중앙 구체를 조준하고 왼쪽 마우스 버튼을 누른 채 시선을 움직인 다음 버튼을 놓는다. 좌우 박스는 04단계의 Level Sequence에 액터 바인딩만 바꿔 연결했다. 같은 레벨에서 05단계 형상 프리셋, 06단계 조준 위젯과 플레이어, 07단계의 스켈레탈 충돌과 마스크도 볼 수 있다. 원본의 Tapping, Oscillating, Bouncing 애니메이션은 Study 캐릭터의 세 인스턴스에 지정했다.

- 첫 PIE 리뷰에서 제목 일부가 잘렸고 설명 끝이 대형 구체와 겹쳤다. 안내 높이와 형상 배치를 조정한 뒤 저장한 레벨을 다시 열어 제목, 한글 설명, LMB 표기와 손 조준점을 확인했다.
- 9개 액터의 13개 Render Target이 서로 다른 인스턴스였으며 각 메시의 A/B 머티리얼 파라미터에 올바르게 연결됐다. 53프레임 중 좌우 박스가 각각 27프레임에서 접촉했고 그중 13프레임에서 중앙 구체 잡기도 동시에 활성화됐다. 두 박스는 중앙 구체와 접촉하지 않았다. 네 Poyo 애니메이션의 시간이 진행됐고 Manny와 짝을 이룬 인스턴스는 양쪽 손에서 각각 16개 충돌체를 감지했다.
- 왼쪽 마우스 버튼 입력은 중앙 구체에만 적용됐다. 버튼을 누른 채 시선의 요를 6° 바꾸자 잡힌 지점이 약 11.35 유닛 이동했고, 버튼을 놓은 뒤 비활성화됐다.
- Asset Registry에서 Study 패키지 41개와 레벨 8개를 확인했다. 검사한 직접 패키지 참조에 MCP 에셋은 없었다. 실행 검증은 UE 5.8.4의 PIE에서 진행했다.

로컬 검증 자료는 `Saved/SoftbodyStudy/stage08_final_idle.png`, `stage08_final_dragged.png`, `compare_stage08_final.json`과 `audit_study_packages.json`이다.

## 직접 확인하기

콘텐츠 브라우저에서 /Game/Study/SoftbodySystem/Maps를 연다. 기능을 순서대로 따라가려면 L_01_DeformOutput부터, 전체 결과를 먼저 보려면 L_08_Integration부터 연다. 에디터의 Play를 눌러 레벨 카메라와 조작을 시작한다. 06·08단계에서는 중앙 구체에 조준점을 맞추고 왼쪽 마우스 버튼을 누른 채 시선을 움직인 다음 놓는다. 07단계는 위쪽의 손 충돌과 아래쪽의 빨강·초록 변형 마스크를 비교한다.

## 인계 전 리뷰

- 새 UE 5.8.4 에디터에서 디스크의 레벨 8개를 각각 열어 PIE로 실행했다. 모든 한글 안내를 화면에서 확인했고, 06·08단계의 손 조준 위젯과 03–08단계의 Study Render Target 연결을 확인했다. 저장된 Blueprint·머티리얼의 한글 코멘트와 Niagara의 좌표 변환·복원·충돌·압력 HLSL 코멘트를 다시 읽었다.
- 07단계 Poyo Blueprint의 코멘트에서 NodeGuid 누락 경고를 발견했다. 에디터가 생성한 식별자를 해당 Blueprint에 다시 저장했다. 새 에디터로 재확인했을 때 동일한 식별자가 저장돼 있었고 경고가 재발하지 않았다.
- 08단계 안내 끝이 대형 구체의 움직임과 겹치는 프레임이 있어 문구를 줄였다. 수정 후 07·08단계를 새 에디터에서 다시 열었고, 마스크와 안내가 보이는 상태에서 08단계의 13개 Render Target 연결과 중앙 구체의 잡기·놓기를 재확인했다. 원본 Niagara에서도 나오는 Grid3D 속성 혼용 경고는 재현됐다.
- 로컬 검증 자료는 Saved/SoftbodyStudy의 handoff_maps_review.json, handoff_maps_after_fixes.json, handoff_graph_review.json과 handoff_08_held_showui.png에 있다.
