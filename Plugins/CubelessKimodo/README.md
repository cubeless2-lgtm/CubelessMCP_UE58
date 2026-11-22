# Kimodo UE 5.8 editor port

The generation backend is from EpicGames/UnrealEngine tag 5.9.0-preview-2, commit 2f66b3f91297f7ad9261f18a70c836fa5472d645, Engine/Plugins/Experimental/Animation/Kimodo/Content/Python/kimodo. ThirdParty/Kimodo59 retains the upstream attribution and license files. The unmodified source and content snapshot is in Saved/Upstream/5.9.0-preview-2/Kimodo.

run_58.py calls the original kimodo_generate.infer_only with the local Kimodo virtual environment and cached SOMA weights. The 5.8 adapter exports the generated SOMA77 standard-pose output before the upstream SOMA30 conversion. bvh_export_58.py comes from the local NVIDIA Kimodo checkout (kimodo/exports/bvh.py). The project retargets BVH onto Quinn and saves a standard AnimSequence. JSON markers use the project post-generation IK solver. The 5.9 Sequencer generator, shared-memory worker protocol, and native contact correction are outside this integration.

Open Window > Kimodo · 모션 편집. Enter a prompt and click 모션 생성. Scrub the animation preview or click 재생 / 일시정지. 현재 프레임에 마커 pauses the preview and adds an editable item under IK > Markers. Select a hand or foot, edit the frame interval, choose Offset (cm) or Pin (anchor frame), then click IK 적용. Every correction starts from the source job. JSON 저장 and JSON 불러오기 use the editable JSON file path.

**생성 → 저장 폴더**에서 콘텐츠 브라우저 폴더를 선택합니다. 기본값은 `/Game/Kimodo`입니다. **모션 생성**은 선택한 폴더에 기본 보정을 마친 애니메이션 한 개를 저장합니다. 첫 이름은 `AS_Kimodo`이며 이미 사용 중인 이름에는 번호가 붙습니다. **IK 적용**은 원본 BVH에서 다시 계산한 결과를 현재 에셋에 덮어씁니다. 마커를 모두 지우고 적용하면 기본 보정 상태로 돌아갑니다. 저장 폴더를 바꿔도 기존 애니메이션은 원래 위치에서 수정됩니다. 원본 BVH, 마커 작업 기록과 편집기 캐시는 프로젝트의 `Saved/Kimodo`에 보관됩니다.

**원본 · 퀸 비교 재생**을 누르면 레벨 뷰포트에서 왼쪽에 원본 SOMA, 오른쪽에 현재 대상 메시가 같은 시간으로 반복 재생됩니다. 이전 결과는 **비교할 클립**에서 선택합니다. 처음 비교할 때는 원본 BVH로 메시 캐시를 준비하며, 이후에는 같은 원본 캐시를 재사용합니다.

## 다른 UE 5.8 프로젝트에서 사용

프로젝트의 `Plugins/CubelessKimodo`에 전체 플러그인을 복사하고 활성화합니다. 사용하는 엔진 빌드에 맞춰 Editor 모듈을 빌드합니다. 실행 PC에는 Kimodo 의존성과 캐시된 SOMA-RP v1.2 모델 및 텍스트 인코더, Blender가 필요합니다.

프로젝트의 `Config/Kimodo.json`에서 대상 메시와 도구 경로를 지정할 수 있습니다. 파일 시스템 상대 경로는 `.uproject` 폴더 기준입니다. 설정을 생략하면 기존 Quinn 템플릿 경로를 사용합니다. 실행 파일은 환경 변수 `KIMODO_PYTHON`, `KIMODO_ROOT`, `KIMODO_BLENDER`로도 지정할 수 있습니다.

```json
{
  "target_mesh": "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
  "python": "../kimodo/.venv/Scripts/python.exe",
  "blender": "../_tools/blender-4.5.12-windows-x64/blender.exe"
}
```

팔과 다리에 UE5 마네킹 본 이름 및 계층을 사용하는 메시가 대상입니다. 새 모션은 선택한 메시의 기준 포즈와 팔 길이로 베이크됩니다. 이후 JSON 마커 적용은 원본 작업에 기록한 메시를 사용합니다. 다른 이름이나 계층의 커스텀 리그에는 별도 본 매핑이 필요합니다.

