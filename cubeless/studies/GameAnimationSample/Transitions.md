# 재생·전환·완료 판정

## 설치본의 포즈 전환

`SandboxCharacter_CMC_ABP`와 `SandboxCharacter_Mover_ABP`에는 Pose History, Blend Stack, Inertialization, Dead Blending과 `DefaultSlot`이 있다. Mover는 Linked Anim Layer로 발·가산 Lean·Aim Offset·전신 보정을 분리한다. 이는 연결 구조의 확인이며, 모든 레이어의 극단 입력 품질을 개별 검증했다는 뜻은 아니다.

기본 Mover 실행에서 Walk/Run/Sprint, Idle/Moving, Aim과 OnGround/InAir를 실제 관측했다. 별도 Motion Matching 경로에서는 상태에 따라 8개 DB와 11개 애니메이션을 선택했다. 실행되지 않는 경로의 출력 변수가 비어 있는 것은 실패가 아니다. [MotionMatching.md](MotionMatching.md)를 함께 본다.

관성화는 전환 오차를 감쇠하는 방법이며 애니메이션 선택 자체를 대신하지 않는다. Sync Group/Marker의 위상 정렬과도 다르다. 이번 Tackle Montage의 실제 `sync_group=None`, sync marker 수 0을 확인했으므로, 두 캐릭터의 상호작용을 Sync Marker 성공 예로 설명하지 않는다. 이 샘플의 상호작용 선택·정렬 경로는 [Interactions.md](Interactions.md)에 분리한다.

## Montage의 실제 데이터

`/Game/Characters/UEFN_Mannequin/Animations/Interactions/Tackle/AM_M_relaxed_ragdoll_tackle_run_F_Rfoot_A`의 보호된 메타데이터 관측 결과는 다음과 같다.

- 길이 약 1.7333초, `DefaultSlot` 한 개, 원본 AnimSequence 세그먼트 한 개.
- `Default` Section 한 개, 다음 Section 없음. Blend In/Out 각각 0.25초, Auto Blend Out 활성.
- Notify 5개. 이 중 `BP_NotifyState_MMI_MotionWarping`의 창은 약 0.66665~1.06648초이고 `PoseSearchSamplingRange`도 포함된다.
- Capsule collision override 등 Notify State는 단순 표시가 아니라 실행 중 권한·상태에 영향을 줄 수 있다. 메타데이터를 읽었다는 이유만으로 Notify 콜백 전체를 검증했다고 표현하지 않는다.

Curve는 연속값, Notify는 시점/구간 이벤트, Attribute는 포즈와 함께 전달되는 데이터라는 역할을 구분한다. 설치본의 LookAt은 Curve로 body-part의 weight/limit를 보정한다. Curve 이름과 소비 그래프를 함께 이전해야 한다. 임의의 Attribute 작성·전달 체인은 이번 대표 실행 범위 밖이다.

## 중단과 자연 종료를 나눈 실제 실행

소유 `L_AnimationLab`의 플레이어 AnimInstance에서 기존 활성 Montage가 없는 것을 확인한 후, Idle AnimSequence를 `DefaultSlot`의 일회성 Dynamic Montage로 재생했다. 시간은 **벽시계 관측값**이며 애셋 재생 시간과 다르다.

| 경계 | 실제 결과 |
|---|---|
| 중단 | Rate 0.5로 재생하고 `montage_stop(0.1, owned_montage)` 호출. Stop 반환 후 `on_montage_ended`에서 같은 Montage와 `interrupted=true`를 관측 |
| 자연 종료 | 다음 일회성 재생은 Rate 3.0. 같은 delegate에서 해당 Montage의 `interrupted=false`를 관측 |
| 기본 이동 복귀 | W 입력 후 `Speed2D` 최대 약 340.43, 종료 관측 시 활성 Montage 없음. 키 해제·delegate 해제·Slate callback 해제 확인 |

`get_current_active_montage()`는 첫 활성 Montage 조회이므로 전체 Slot/Group 동시 재생 목록을 대신하지 않는다. 이번에는 기존 재생 없음과 단일 소유 Montage라는 조건을 뒀다. 모든 블렌드 충돌·다중 그룹·대상 소멸 경계를 검증한 결과는 아니다.

원시 관측 `montage-boundaries-runtime.json`은 최종 `failed_or_partial`을 유지한다. 기록기가 반환 키 `Speed2D`를 `speed2D`로 잘못 조회해 마지막 이동 검사를 거절했기 때문이다. 실제 표본과 두 종료 콜백은 위 동작을 입증한다. 기록기의 철자를 수정했으며 엔진 재생을 반복하지 않았다. 이 검수 오판을 엔진 재생 결함으로 집계하거나 원 실패를 덮어쓰지 않는다.

## 재사용 예제

아래는 Unreal Python의 실제 노출 메서드다. `ai`, `clip`은 준비된 **소유 PIE 대상**이어야 한다. 원본 에셋을 수정하거나 저장하는 호출은 없다.

```python
ended = []
def on_end(montage, interrupted):
    ended.append((montage, bool(interrupted)))

ai.on_montage_ended.add_callable(on_end)
montage = ai.play_slot_animation_as_dynamic_montage(
    clip, 'DefaultSlot', blend_in_time=0.1,
    blend_out_time=0.15, play_rate=1.0, loop_count=1)
# 중단이 필요할 때만: ai.montage_stop(0.1, montage)
# 다음 프레임의 ended에서 해당 montage와 interrupted를 확인한다.
# 작업 종료 시 반드시: ai.on_montage_ended.remove_callable(on_end)
```

재생 함수의 반환은 시작 요청 결과다. 실제 종료·중단·포즈 복귀는 후속 관측으로 확인한다. 시간 제한이나 클라이언트 disconnect를 엔진 재생 취소로 취급하지 않는다.
