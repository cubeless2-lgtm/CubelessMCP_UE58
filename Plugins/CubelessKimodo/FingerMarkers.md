# 프레임별 손 모양 보정

키모도 편집 창에서 현재 클립의 **JSON 저장**을 눌러 원본 작업 ID가 들어 있는 문서를 만든다. 기존 문서의 최상위에 다음 배열을 추가한다. **JSON 불러오기 → IK 적용**으로 다시 베이크한다.

```json
{
  "schema_version": 1,
  "expected_fps": 30,
  "markers": [],
  "finger_markers": [
    {
      "id": "chair_right", "side": "r", "pose": "chair_grip",
      "start_frame": 20, "end_frame": 60,
      "blend_in_frames": 8, "blend_out_frames": 8, "weight": 0.9
    },
    {
      "id": "knob_right", "side": "r", "pose": "doorknob_grip",
      "start_frame": 90, "end_frame": 140,
      "blend_in_frames": 10, "blend_out_frames": 10, "weight": 0.85,
      "curl_deg": {"index": [35, 45, 28]}
    }
  ]
}
```

프레임은 0부터 시작하고 끝 프레임을 포함한다. 위 문고리 예시는 90~100프레임에 손을 닫고 100~130프레임에 유지한 뒤 140프레임까지 기존 모션으로 돌아온다. 손은 왼쪽 `l`, 오른쪽 `r`로 지정한다. `pose`는 `open`, `chair_grip`, `doorknob_grip` 중 하나다. `curl_deg`는 엄지부터 새끼까지 `thumb`, `index`, `middle`, `ring`, `pinky`를 지원한다. 값은 퀸 바인드 포즈 기준 첫마디·중간마디·끝마디의 굽힘 목표각이다. `pose`를 생략하고 필요한 손가락만 지정할 수도 있다.

같은 손가락의 마커는 시간상 겹칠 수 없다. 손 모양은 HandFlow와 기본 손 포즈 위에 블렌딩하고 관절·속도·손바닥 관통 제한을 적용한다. `weight`와 이 제한에 따라 실제 각도는 목표에서 달라질 수 있다. 소품 위치까지 맞추려면 기존 `markers` 배열에 손목의 `offset` 또는 `pin` 마커를 함께 둔다. 잡기 프리셋은 소품 탐색이나 표면 충돌 추적을 수행하지 않는다.

