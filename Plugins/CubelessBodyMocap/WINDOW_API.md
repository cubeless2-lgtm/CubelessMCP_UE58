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
