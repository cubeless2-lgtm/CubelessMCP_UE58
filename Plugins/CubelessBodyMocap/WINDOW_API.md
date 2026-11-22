# 모캡 창 직접 제어 API

`unreal.CubelessMocapWindowLibrary`는 마우스·키보드 입력, 창 좌표, Computer Use 없이 모캡 창을 제어하는 **Editor 전용 API**입니다. Unreal Python에서 직접 사용하거나 기존 MCP의 `execute_unreal_python`으로 호출합니다. MCP를 설치하지 않은 프로젝트에서도 Unreal Python으로 사용할 수 있습니다. 플러그인 Editor 모듈과 Python Editor Script Plugin이 필요합니다.

화면 버튼과 같은 재생·시간 탐색·구간 정규화·결과 선택 함수를 사용합니다. 추론 모델, 해상도, 분석 FPS 기본값, 리타깃과 보정 계산은 바꾸지 않습니다. 게임 실행용 API가 아니며 결과 AnimSequence에 이 API나 MCP 의존성을 추가하지 않습니다.

## 호출 목록

`get_settings()`는 창이 사용하는 실제 `CubelessMocapSettings` 객체를 반환합니다. 필요한 경우 창을 한 번 만듭니다. 설정 객체를 찾기 위해 `ObjectIterator`를 사용할 필요가 없습니다. 설정을 직접 수정하기 전에 `get_state()`의 `busy`를 확인합니다.

`get_state()`는 상태 JSON 문자열을 반환합니다. 나머지 명령은 아래 형식의 JSON 문자열을 반환합니다.

```json
{"success": true, "action": "range", "message": "range_set; see normalized state bounds", "state": {}}
```

| Python 호출 | 동작 |
|---|---|
| `open_video(file_path)` | 파일 존재 여부를 확인한 뒤 절대 경로로 정규화하여 엽니다. 파일 대화상자를 띄우지 않습니다. 기존 프레임 폴더 입력을 비우며, 다른 영상이면 창의 이전 결과 연결만 비웁니다. 저장 에셋은 보존합니다. |
| `control("source")` | 원본 전체 검토로 전환합니다. |
| `control("result")` | 계산 결과 또는 선택 구간 검토로 전환합니다. |
| `control("play")`, `control("pause")`, `control("stop")` | 재생, 일시 정지, 현재 검토 구간의 시작으로 정지합니다. 모캡 작업 취소는 기존 `CubelessMocapLibrary.cancel()`입니다. |
| `control("seek", source_seconds)` | **원본의 절대 초**로 탐색합니다. 원본 FPS 격자와 현재 검토 범위에 맞춘 실제 요청값을 상태에서 확인합니다. |
| `control("step", signed_frames)` | 원본 FPS 기준으로 정수 프레임을 이동합니다. 음수는 이전 프레임이며 절댓값은 최대10000입니다. |
| `control("rate", rate)` | 미리보기 속도만 설정합니다. 0.1~4와 디코더 지원 범위를 검사합니다. 저장 RateScale은 바꾸지 않습니다. |
| `set_range(start_seconds, end_seconds)` | 원본 검토로 전환하고 처리 구간을 지정합니다. 끝은 제외 경계이며 실제 스냅·제한 결과는 `selection_start`, `selection_end_exclusive`로 확인합니다. 기존 프레임 폴더 모드에서는 거부합니다. |
| `select_result(hand_flow, finger_correction)` | 두 bool로 네 결과를 선택합니다. 원본이 없으면 거부하며 보정만 없으면 원본을 미리 보고 내보낼 때만 보정합니다. 결과를 바꿀 때 현재 결과 재생 시점을 유지하여 비교합니다. |
| `set_layout(preview_fraction, video_fraction, unreal.Vector2D(width, height))` | 중앙/설정 비율, 영상/3D 비율, 창의 클라이언트 크기를 설정합니다. 창은1000~3840 ×720~2160, 중앙 영역은640 이상, 설정은250 이상이며 영상 비율은0.2~0.8입니다. |
| `close()` | 모캡 창만 닫습니다. 실행 중인 계산/저장 작업이 있으면 거부합니다. 에디터 전체를 종료하지 않습니다. |

`open_video()`는 창이 없으면 만듭니다. 다른 제어 명령은 열린 창을 대상으로 하며, 닫힌 창에는 실패를 반환합니다. 이 API는 작업 파일·프리셋을 저장하는 기능이 아닙니다. 에디터 재시작 간 작업 복원은 별도 기능입니다.

## 준비와 완료 확인

영상 열기와 seek는 비동기입니다. 명령의 `success=true`는 요청 수락을 뜻하며 디코더 완료를 보장하지 않습니다. **에디터 게임 스레드에서 sleep이나 대기 루프를 돌리지 않습니다.** 외부 MCP 클라이언트에서 다음 호출로 상태를 조회하거나 에디터의 비동기 tick을 사용합니다.

- `window_open`, `busy`, `job_status`: 창·작업 상태입니다.
- `media_ready`, `media_error`: 영상 준비 여부와 디코더 오류입니다.
- `seek_pending`, `seek_failed`: 탐색 진행/실패입니다. 수락된 탐색은 최대 약3초 후 완료 또는 실패로 정리하며, 디코더가 응답하지 않는 상태를 무한 대기로 숨기지 않습니다.
- `seek_exact`, `seek_error_seconds`: 디코더 시각이 요청 격자에 맞았는지와 마지막 탐색 오차입니다. 일부 디코더가 0초 대신 첫1~2프레임 시각을 제공하면 실제 시각으로 동기화하고 `seek_exact=false`로 알립니다. 더 큰 불일치는 탐색 실패로 보고합니다. 모든 코덱의 프레임 정확도를 보장하는 값이 아닙니다.
- `requested_source_seconds`, `media_seconds`, `preview_seconds`, `evaluated_animation_seconds`: 마지막 요청, 실제 원본, 구간 상대 시각, 실제 평가된 애니메이션 시각입니다. 실패 후에도 실제 표시 시각을 확인할 수 있습니다.
- `selection_start`, `selection_end_exclusive`, `result_start`, `result_duration`: 다음 계산 구간과 현재 결과의 원본 시간 기준을 구분합니다.
- `results`: 네 결과의 `key`, `source_available`, `ready`, `selected`, `animation`을 반환합니다. `ready=false`인 손가락 보정은 저장 때 계산할 수 있습니다.
- `preview_rate`, `save_rate`, `preview_animation`, `exported_animation`: 미리보기와 저장 선택을 구분합니다.

계산·내보내기 중에는 영상 변경·구간/결과 변경·레이아웃 변경·창 닫기를 거부합니다. 명령 실패의 `message`와 반환 상태를 먼저 확인하며 불확실한 에셋 저장/계산을 자동으로 재실행하지 않습니다.

## 사용 예

첫 MCP 호출 또는 Editor Python 실행:

```python
import json, unreal
w = unreal.CubelessMocapWindowLibrary
reply = json.loads(w.open_video(r"C:\Videos\input.mp4"))
assert reply["success"], reply["message"]
```

다음 호출에서 준비를 확인한 뒤 구간을 지정합니다. 아직 준비 중이면 호출을 끝내고 외부 클라이언트가 나중에 다시 조회합니다.

```python
state = json.loads(w.get_state())
if state.get("media_ready") and not state.get("seek_pending"):
    reply = json.loads(w.set_range(3.0, 10.0))
    assert reply["success"], reply["message"]
```

계산·취소·저장은 검증된 기존 API를 그대로 사용합니다. 분석 FPS, 대상 메시, 리타기터, HandFlow·손가락 보정, 출력 경로는 같은 설정 객체에서 지정할 수 있습니다.

```python
settings = w.get_settings()
# 상태 busy=false를 확인하고 필요한 설정을 지정한 뒤 실행합니다.
message = unreal.CubelessMocapLibrary.start_mocap(settings)
# 진행 중에는 json.loads(w.get_state())로 조회합니다.
# 취소: unreal.CubelessMocapLibrary.cancel()
```

완료 후 결과 선택과 내보내기:

```python
reply = json.loads(w.select_result(True, True))  # HandFlow + 손가락 보정
assert reply["success"], reply["message"]
# settings.output_folder, settings.sequence_name, settings.save_play_rate 지정 후
message = unreal.CubelessMocapLibrary.save_animation_sequence(settings)
# busy=false 및 exported_animation 경로로 완료를 확인합니다.
```

화면 확인이 필요하면 기존 `unreal.CubelessMocapLibrary.save_preview_screenshot()`이 엔진에서 직접 렌더링한 PNG를 저장합니다. 이 호출에도 OS 화면 조작이나 Computer Use가 필요하지 않습니다.

본 디버그는 `settings.set_editor_property('debug_bones', True)`로 제어합니다. 기본값은 켜짐이며 청록 선은 원본 영상의 고정 카메라를 사용합니다. `preview_yaw_offset`과 `preview_zoom`을 바꿔도 영상 위 청록 좌표는 움직이지 않습니다. 저장·보정본은 원본 실행 ID 메타데이터로 캐시를 찾고, 없는 경우 창의 `original_animation` 연결을 사용합니다. 스크린샷 옆 `ComparisonPreview.json`의 `overlay_applied_uv`로13개 투영 좌표를 직접 검증할 수 있습니다.

재생 타임라인은 파란 구간 막대 위의 빨간 막대입니다. 레일 폭/높이는 구간 막대와 같고 빨간 단일 핸들이 현재 위치를 나타냅니다. 드래그와 방향키/Home/End로 이동한 뒤 일시 정지합니다. 아래 파란 막대는 계산 구간 선택 용도를 유지합니다.

영상 끝에서 WMF가 Seek를 수락하고도 시각을 이동하지 않는 경우 같은 영상 디코더를 한 번 다시 열어 요청 시각과 재생 의도를 복구합니다. 정지한 일반 탐색의 복구도 한 번으로 제한하고 이후 열기/탐색 시간 초과는 실패로 반환합니다. `seek_retry_count`, `seek_pending`, `seek_failed`, `play_after_seek`를 확인하세요. stop은 현재 검토 구간의 시작으로 돌아가며 에셋을 변경하지 않습니다. 실제 영상 끝난 뒤 재생, 정지/재생 반복 및 일시 정지/재생을 검증했습니다.


## 대표 자세와 Undo

`start_representative_preview(settings)`로 임시 대표 자세를 준비합니다. `get_representative_state(settings)`의 `frames`, `ready_seconds`, `input_matches`, `preview_animation`, `corrections`, `undo_steps`, `redo_steps`를 확인합니다. 이력 생성 전에는 undo/redo 필드가 없을 수 있습니다.

```python
l = unreal.CubelessMocapLibrary
s = w.get_settings()
w.control('representative', 2)  # seek_pending=false 이후 편집
l.begin_representative_edit(s)
l.set_representative_correction(s, 2, 'spine_01', unreal.Rotator(pitch=5.))
l.end_representative_edit(s)
w.control('undo_pose')
w.control('redo_pose')
```

`pose_begin`/`pose_end`도 같은 이력 묶기를 제공합니다. `pose_rotate_x/y/z`와 각도(호출당 ±15도)는 화면 기즈모와 같은 월드 축 회전 경로를 사용합니다. 원본 로컬 좌표 보정은 set_representative_correction을 사용합니다. 대표 자세가 선택된 시각과 입력이 일치해야 합니다.

`apply_representative_corrections(settings, source, new_package)`는 깨끗한 전체 모캡을 새 에셋에 저장합니다. 보정이 없으면 source 자체를 반환하고 새 파일을 만들지 않습니다. 세션은 실행 폴더의 representative_edits.json을 `load_representative_session(settings, file)`로 복원합니다. Undo 이력은 복원되지 않습니다.

`displayed_source_seconds`, `source_display_mode`, `displayed_frame_rate`는 실제 표시 프레임의 시각·방식·분석 FPS입니다. `media_seconds`는 디코더 시각을 유지합니다. PNG 프레임 표시 시 3D와 디버그는 표시 시각에 맞춰 평가합니다. 스크린샷 JSON은 displayed_frame_file, gizmo_visible, pose_selected_bone, pose_selected_world_quat도 기록합니다.



## 수동 마커와 정확한 본 선택


`get_representative_state`의 version4는 수동으로 지정한 `frames`, `markers`를 제공합니다. 각 marker에는 index, frame, seconds(분석 시작 기준), origin(manual), selected, edited가 있습니다. `selected_bone` / `selected_sample`도 조회할 수 있습니다. 마커 삽입 시 보정 곡선을 유지하고 Sample 인덱스를 재매핑하므로 새 인덱스는 반환 상태에서 다시 읽습니다. 삽입·삭제는 기존 undo_pose / redo_pose에 포함됩니다.

`pick_pose_bone_at(unreal.Vector2D(x,y), cycle=False)`는 3D 뷰포트 내부 픽셀 좌표에서 화면 클릭과 같은 선택 로직을 사용합니다. OS 입력을 주입하지 않습니다. 관절점 우선, 거리·깊이 순서, 부모 본의 연결선, cycle=True인 겹침 후보 순환을 지원합니다. reply.message는 선택한 본 이름 또는 실패 이유입니다. 탐색 중/계산 중에는 거부합니다. 스크린샷 JSON의 pickable_bones, viewport_width/height는 실제 선택 기하를 확인하는 진단 자료입니다. bone_display_scale과 bone_pick_scale은 3입니다.


## IK 편집과 수동 마커

자동 간격 마커와 GIF/보조 영상 보정 API는 제거했습니다. 시작/끝을 포함한 모든 마커는 사용자가 직접 추가하며 자동 생성하지 않습니다. 이전의 대표 프레임 선행 추론은 사용하지 않습니다. `start_representative_preview(settings)`는 현재 모캡 결과의 빈 수동 마커 세션만 준비하는 호환 함수입니다.

```python
window = unreal.CubelessMocapWindowLibrary
lib = unreal.CubelessMocapLibrary
s = window.get_settings()
lib.begin_ik_edit(s)  # 현재 결과에서 시작, 보정이 없으면 자세 변화 없음
lib.set_ik_range(s, 2.0, 4.0, 0.25)  # 애니메이션 초, 양쪽 연결은 구간 안
lib.set_ik_control(s, 'foot_l', unreal.Vector(0, 3, 0), unreal.Rotator())
print(lib.get_ik_state(s, 3.0))
lib.save_ik_animation(s, '/Game/CubelessMocap/AN_Quinn_IK_NewName')
```

컨트롤 이름: pelvis, foot_l/r, knee_l/r, hand_l/r, elbow_l/r, thumb_tip_l/r, index_tip_l/r, middle_tip_l/r, ring_tip_l/r, pinky_tip_l/r. 이동 단위는 cm이며 컴포넌트 축, 회전은 컴포넌트 축 델타입니다. 길이를 늘리지 않고 도달 가능한 범위에서 풉니다. 무릎/팔꿈치는 방향 핸들이며 손끝은 세 관절을 함께 푸는 FABRIK 핸들입니다. 기본 엔진 Control Rig Blueprint를 변경하지 않는 에디터 전용 IK 레이어입니다.

`control('ik_begin')`, `control('ik_filter', 0..3)`(몸/왼손/오른손/전체), `control('ik_range_start', seconds)`, `control('ik_range_end', seconds)`와 기존 `pose_translate_x/y/z`, `pose_rotate_x/y/z`, `pose_begin/end`, `undo_pose/redo_pose`가 화면과 같은 코드를 사용합니다. `ik_state`에는 소스 GUID, 범위, 연결 시간, 핸들 좌표와 레이어가, `ik_screen_handles`에는 선택용 뷰포트 픽셀이 있습니다. `pick_pose_bone_at(pixel, cycle)`는 IK 모드에서 핸들을 선택합니다.

새 경로만 저장할 수 있으며 기존 에셋을 덮어쓰지 않습니다. 결과는 표준 AnimSequence이며 에디터 플러그인 런타임 의존성이 없습니다. 보정 없는 변환은 원본 키를 그대로 복제합니다. 저장 후에는 새 결과를 기준으로 IK 편집을 유지합니다. 수동 마커·편집 구간·현재 시각은 유지하고, 이미 저장한 레이어를 다시 적용하지 않습니다. 다른 애니메이션으로 바꿨다면 begin_ik_edit를 호출합니다.

현재 에디터 세션에서는 소스 애니메이션·메시·원본 데이터 GUID·영상 입력 조건별로 미저장 보정, 구간, 수동 마커와 Undo/Redo 이력을 보관합니다. 다른 결과에서 편집한 뒤 돌아와 `begin_ik_edit`를 호출하면 해당 소스의 상태를 복원합니다. 원본 데이터가 달라진 소스에 이전 레이어를 자동 적용하지 않습니다. 에디터 종료 전에는 새 애니메이션으로 저장해야 합니다.

IK 모드에서는 마커 추가·삭제, 자세 보정, 구간 및 연결 시간 변경을 하나의 작업 순서로 Undo/Redo합니다. `control('ik_ease', seconds)` 또는 같은 구간의 `set_ik_range`로 연결 시간을 바꾸면 그 구간의 기존 보정 레이어에도 즉시 반영됩니다. 슬라이더 드래그는 Undo 한 단계로 묶입니다.

원본에 특정 본 트랙이 없으면 대상 메시의 기준 자세로 새 트랙을 초기화한 뒤 선택 구간만 보정합니다. 이때 구간 밖 기준 자세를 보존합니다. 새 트랙의 쿼터니언은 부호가 반대여도 같은 회전일 수 있으므로 기준 자세 비교에는 회전 각도 오차를 사용합니다.


### 기즈모 포인터 제어와 강조 표시

기즈모에 포인터를 올리면 해당 축/링/중앙 핸들이 노란색으로 강조됩니다. 누른 상태에서는 더 밝고 굵게 표시하며, 드래그 중 다른 축을 지나도 누른 축을 유지합니다. 선택 모드 Q에서는 기즈모를 숨깁니다. Shift+클릭은 겹친 관절을 선택/순환하고, 일반 클릭은 보이는 기즈모를 우선합니다. 화살촉과 스케일 상자도 렌더링과 같은 선분으로 판정합니다.

`get_pose_gizmo_state()`는 `visible`, `hover_axis`, `active_axis`, 뷰포트 크기, 중심과 투영된 `segments`를 반환합니다. 축 번호는 -1 없음, 0 X, 1 Y, 2 Z, 3 중앙입니다. `pose_pointer(action, unreal.Vector2D(x,y))`는 화면 안의 좌표로 `hover`, `press`, `drag`, `release`, `leave`, `cancel`을 처리합니다. 실제 마우스의 Hover/Press/Drag/Release 구현을 공유하며 OS 입력이나 Computer Use가 필요하지 않습니다.

`press` → 여러 번의 `drag` → `release`가 Undo 한 단계입니다. `leave`는 hover만 지우고 캡처 중인 드래그는 유지합니다. `cancel`은 포커스 상실처럼 현재 값을 유지한 채 제스처를 종료하므로 값을 되돌리려면 `control('undo_pose')`를 사용합니다. 재생, 편집 불가 상태, 선택/도구 변경 시 활성 드래그를 종료합니다. IK 모드의 스케일은 기존 정책대로 허용하지 않습니다.
