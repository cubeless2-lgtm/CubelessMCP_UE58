# Motion Matching과 Pose Match Chooser

## 두 선택 경로

설치본의 기본 Mover는 `DDCVar.LocomotionSetupMover=1`에서 상태별 Blend Stack 경로를 사용했다. `SetBlendStackAnimFromChooser`가 `CHT_MoverCharacterAnimations_PoseMatch`를 평가하고 Pose History를 전달한다. 결과에는 애니메이션뿐 아니라 시작 시간·블렌드 시간·프로파일·태그가 포함된다.

이 루트 Chooser와 로드된 하위 테이블 50개를 공개 관측 API로 조사했으며, 그 안에 Pose Search Column 38개를 확인했다. Enum·FloatRange·Bool·MultiEnum 필터 등으로 후보를 좁힌 뒤 Pose Match가 작동하는 구성이다. 설치 소스 `PoseSearch/Chooser/PoseSearchChooserColumn.h`는 Pose Match의 pose-history 입력·시작 시간 출력과 재선택 설정을 정의한다. 컬럼을 다른 필터 뒤에 배치하는 규칙과 `PoseSearchBranchIn` Notify State 조건도 소스에 명시돼 있다.

반면 `locomotionSetup=0`의 Motion Matching 경로는 `Update_MotionMatching`에서 다음을 수행한다.

1. `CHT_PoseSearchDatabases_Relaxed` 평가
2. `ValidDatabases` 갱신
3. Motion Matching node reference 변환
4. `Get_MMInterruptMode`와 함께 `Set Databases to Search` 호출

따라서 AnimGraph의 정적 `Database=None`만 보고 DB 연결이 없다고 판단하면 안 된다. 실제 On Update 함수의 동적 할당을 확인한다. 비활성 경로의 `currentSelectedAnim=None`도 실패 판정 근거가 아니다.

## Schema·채널·가중치

`PSS_Default`와 `PSS_Default_Mover`는 모두 30Hz로 샘플링한다. 그러나 검색 채널 구성은 다르다.

| Schema | 설치본 관측 |
|---|---|
| PSS_Default | Trajectory와 Pose 채널 그룹. foot_l 위치 weight 1, 양발 속도 각각 약 0.3, pelvis heading 약 0.1 |
| PSS_Default_Mover | Trajectory와 Phase curve 채널 두 개. Trajectory 샘플에는 과거 -0.5초, 현재, 미래 0.75·1·1.5초가 포함됨 |

채널의 정규화·샘플링·역할·Skeleton 설정과 DB의 Pose Search mode·normalization set까지 함께 본다. 한 Schema에서 측정한 비용 점수를 다른 구성과 곧바로 비교하지 않는다. 샘플링 주기, 특징 차원, 데이터 범위 변경은 검색 인덱스의 재구축을 고려해야 한다.

CMC Motion Matching 노드의 authored 값은 BlendTime 0.5초, PoseReselectHistory 약 0.3초, SearchThrottleTime 0이었다. 런타임 입력·함수에서 바뀔 수 있으므로 모든 프레임의 최종값이라고 단정하지 않는다.

## 실제 경로 비교

소유 `L_AnimationLab`에서 Mover의 경로를 일시적으로 0으로 바꾸고 이동·질주·방향 변경·점프 입력을 주었다. 10초 실행의 30개 표본에서 `locomotionSetup=0`, 선택 애니메이션 11개, DB 8개를 관측했다. 종료 시 `DDCVar.LocomotionSetupMover`는 1로 복원됐고 주입 키는 남지 않았다.

기본 경로의 선택 애니메이션은 `blendStackInputs.anim`, 전통 MM 경로는 `currentSelectedAnim`/`currentSelectedDatabase`로 구분해서 읽었다. `search Cost`, `mM Search Cost`는 **매칭 비용 점수**이며 CPU/GPU 밀리초가 아니다. 두 실행의 표본 수·백그라운드 조건이 다르므로 이 기록으로 어느 경로가 더 빠르다고 결론 내리지 않는다.

## 소유 데이터 실험

`PSS_Study15Hz`는 원본 Schema의 샘플링 주기를 15Hz로 바꾼 소유 복사본이다. `PSD_StudyWalk`는 해당 Schema를 참조하도록 변경·저장했다. 원본 Schema/DB 파일은 보존했다.

소유 `ABP_CMC_Study`의 Motion Matching 노드는 이 DB를 고정 참조하며, 해당 노드의 동적 DB 할당 OnUpdate 바인딩을 비웠다. Pose History→기존 ComponentToLocalSpace→Offset Root/Blend Stack을 포함한 최종 pose chain은 유지했다. 처음 그래프를 지나치게 단순화했을 때 컴파일러가 필요한 노드 태그가 사라진 것을 지적했으며, 실제 부분 상태를 읽고 소유 그래프의 끊어진 연결을 복구해 컴파일·저장했다. 원본 그래프는 변경하지 않았다.

실제 CMC 폰을 명시적으로 선택해 이 소유 AnimBP를 적용했고, `PSD_StudyWalk`가 런타임에서 선택되는 것과 애니메이션 3개, 22개 표본의 이동·포즈 변화를 확인했다. 왼발-Actor 상대 좌표의 축별 범위는 약 123.83/114.38/29.02 UE 단위였다. 종료 시 Mover 선택을 복원했다. 이는 작은 고정 걷기 DB의 실제 사용 근거이며 전체 원본 locomotion의 대체 검증이 아니다.

실무에서는 DB 범위에 원하는 정지·시작·회전·공중 동작이 포함되는지 먼저 확인한다. 걷기 루프만 있는 DB로 원본 전체 locomotion을 대체하면 필요한 동작을 검색할 수 없다. 고정 DB 실험은 좁은 학습 예제로만 사용한다.

## 공식 출처

- [Motion Matching 개요와 Schema/DB](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine)
- [UE 5.8 샘플의 Pose Match Chooser 개선](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8)
- [FPoseSearchColumn API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PoseSearch/FPoseSearchColumn)

위 문헌의 일반 설명과 설치본 에셋에서 읽은 값, 이번 런타임 관측은 서로 구분한다.
