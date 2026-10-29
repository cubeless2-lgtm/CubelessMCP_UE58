# 다른 캐릭터에 포즈 재사용

## 설치본 구조

원본 locomotion은 UEFN Mannequin을 구동하고, 시각적 캐릭터는 ChildActor로 붙일 수 있다. `/Game/Blueprints/RetargetedCharacters/ABP_GenericRetarget`의 실제 그래프에는 Retarget Pose From Mesh와 Copy Pose From Mesh가 있다. 런타임 리타기팅과 동일 Skeleton의 복사는 같은 작업이 아니다.

Retarget Pose From Mesh의 authored 모드는 `ParentSkeletalMeshComponent`다. 자식 메시에서 부모 체인을 따라 소스 SkeletalMesh에 도달해야 한다. 실제 Echo 체인은 EchoBody → DefaultSceneRoot → VisualOverride → 원본 SkeletalMesh → Capsule이었다. 두 메시 모두 AlwaysTickPoseAndRefreshBones를 사용했다. 숨긴 소스 메시에서 포즈 갱신이 끊기지 않는지 확인해야 하며, 이번 A-pose 관측의 원인을 단순히 비가시 틱 제한으로 단정하지 않았다.

`ABP_GenericRetarget.EventGraph`는 소유 컴포넌트 태그와 `IKRetargeter_Map`을 이용하고, 리타게터를 **비동기 로드**한 뒤 IKRetargeter와 RetargetProfile을 초기화한다. 이 때문에 ChildActor 생성·AnimInstance 생성·리타게터 준비·최종 포즈 갱신은 서로 다른 경계다.

## 실제 실험과 판정 수정

처음 M 입력 후 Echo ChildActor와 GenericRetarget AnimBP가 생성된 것은 확인했지만, 짧은 초기 표본의 발-골반 상대 변화는 0이었고 캡처는 A-pose였다. 이를 리타기팅 성공으로 처리하지 않았다. 이후 조사에서 비동기 초기화 경로와 캐릭터 교체 중 실제 자식 Actor가 바뀔 수 있다는 점을 확인했다.

별도 소유 `ABP_Retarget_Study`를 복사·컴파일·저장하고, 안정된 TwinBlast 런타임 컴포넌트에 적용한 뒤 원본 Blueprint의 초기화 완료를 관측했다. 현재 인스턴스의 IKRetargeter가 `RTG_UEFN_to_TwinBlast`를 가리키는 것을 확인한 후 이동 입력을 주었다. 26개 표본에서 발-골반 상대 좌표의 축별 범위는 약 42.89/18.44/60.22 UE 단위였으며, 렌더 캡처에서도 달리는 포즈를 확인했다. 외형 교체나 Actor 이동만으로 포즈 갱신을 판정하지 않았다.

이 결과는 TwinBlast의 대표 런타임 경로다. 모든 Echo/MetaHuman/체형·LOD의 품질이나 손·발 접촉 정확도를 완료했다는 의미는 아니다. 머리카락 메시처럼 별도 제어를 사용하는 컴포넌트의 뼈 값은 몸통 리타기팅 근거와 분리한다.

## 실패·부분 변경 경계

- C++ 공개 getter라도 Python 메서드가 항상 존재하지는 않는다. ChildActorComponent의 `get_child_actor` 가정은 제거하고, 실제 노출된 `Actor.get_all_child_actors(include_descendants=True)`를 사용했다.
- M 입력은 고정 캐릭터 선택 API가 아니다. 현재 선택과 비동기 로드에 따라 결과가 달라진다. 이름·인덱스만 예측하지 말고 실제 Actor·컴포넌트를 다시 확인한다.
- AnimInstance 클래스 변경은 성공해도 후속 속성 쓰기가 거절될 수 있다. 소유 복사본 적용 후 `IKRetargeter`를 일반 속성 setter로 쓰려던 요청은 false를 반환했고, 클래스는 이미 바뀐 부분 상태였다. 이 실패는 그대로 보존했다.
- 비공개 변수의 쓰기 제한을 우회하지 않았다. 후속 Blueprint 비동기 초기화가 정상적으로 리타게터를 할당한 사실을 별도 관측했다. 그것을 앞선 속성 쓰기 요청의 성공으로 바꾸지 않는다.
- 하나의 관측 호출에서 얻은 두 컴포넌트 값도 완전한 프레임 원자적 스냅샷이라고 표현하지 않는다.

## 다른 프로젝트에서 적용

1. 소스·대상 Skeleton과 SkeletalMesh를 확인하고, 각 IK Rig의 retarget root와 체인을 정한다.
2. 적절한 IK Retargeter에서 소스/대상 포즈·루트 이동·비율·FK/IK 연산을 조정한다.
3. 런타임 재사용이라면 소스 메시의 틱·부모 연결·타깃 초기화 경로를 마련한다. 샘플의 태그→리타게터 맵을 사용할 경우 해당 태그·에셋도 함께 이전한다.
4. 초기화가 끝난 실제 인스턴스를 관측한 뒤 이동·회전·점프에서 포즈와 루트/발을 비교한다.
5. 리타기팅한 AnimSequence를 생성하는 방식과 런타임 Retarget Pose From Mesh 방식은 출력·비용·의존성이 다르다. 이번 실험은 후자의 대표 경로이며, 오프라인 에셋 생성과 전체 체인 편집은 후속 작업이다.

일반 AnimBP 복사본이 원본 Chooser의 컨텍스트 클래스·고정 참조를 자동으로 재설계한다고 가정하지 않는다. 재사용할 데이터와 새 Skeleton에 맞춰 재구축할 데이터를 구분한다. [공식 IK Rig 리타기팅 설명](https://dev.epicgames.com/documentation/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine)
