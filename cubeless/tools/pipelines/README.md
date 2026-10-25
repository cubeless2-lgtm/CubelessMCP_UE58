# 선택형 UE 5.8.3 실무 실행기

프로젝트의 공식 ModelContextProtocol 서버와 등록된 Cubeless 도구를 호출합니다. 별도 MCP 서버나 TCP 브릿지가 아니며, 플러그인 로딩에 필요하지 않습니다. 외부 Python에서는 `unreal`을 import하지 않습니다.

현재 기본 진입점은 `production_blueprint`, `production_niagara`, `production_pcg`입니다. 과거 canary·continue·검증 스크립트를 제품 진입점으로 실행하지 않습니다. 기존 5.6 전체 manifest·고급 타입·모든 고유 흐름의 이식 완료를 뜻하지 않습니다.

## 환경과 실행

UE 5.8.3 에디터에서 공식 ModelContextProtocol, Toolset Registry, CubelessMCP_UE58 및 작업에 필요한 Niagara/PCG 플러그인을 활성화합니다. 실제 실행 중인 프로젝트 파일과 MCP 주소를 지정합니다. 다음은 프로젝트 루트에서 실행하는 PowerShell 예입니다.

```powershell
py -3 -m venv cubeless/.venv-pipelines
& cubeless/.venv-pipelines/Scripts/python.exe -m pip install -r cubeless/tools/pipeline-requirements.txt
$env:PYTHONPATH=(Resolve-Path cubeless/tools).Path
& cubeless/.venv-pipelines/Scripts/python.exe -m pipelines.production_blueprint `
  --endpoint http://127.0.0.1:8000/mcp --project ./UnrealMCP5_8_3.uproject `
  --input cubeless/tools/pipelines/examples/blueprint.json `
  --output cubeless/records/operations/my-bp-preview.json
```

기본 실행은 입력·현재 도구 스키마·대상 프로젝트를 검사하는 preview입니다. 변경하려면 같은 명령에 `--apply`를 추가하고 **새 출력 경로**를 지정합니다. `production_niagara`, `production_pcg`도 동일한 옵션을 사용합니다. 예제의 실행 폴더·에셋 이름은 새 작업마다 바꿉니다. 이름이 충돌하면 덮어쓰지 않습니다.

## 지원 범위와 입력

- Blueprint: 컴포넌트·변수·기본값, 함수/이벤트 그래프, 지원되는 event/call/get/set/return/branch/sequence 노드와 핀 연결을 작성합니다. edit에서는 기존 노드 GUID와 연결, 무관한 변수·컴포넌트를 보존 검사합니다. `existing_node_id`는 실제 GUID를 사용합니다. 저장은 `save`로 지정합니다. 기존 BP가 dirty이면 거절합니다. 임의 Blueprint 전체 상태 동등성, 원본 계획기의 모든 자연어/dispatcher/animation 섹션은 아직 미완료입니다.
- Niagara: CPU sprite 프리셋, 사용자 파라미터, 기존 BP의 own Niagara 컴포넌트 연결을 제공합니다. 기본 생성 경로는 `/Game/_MCP_Temp/NiagaraGenerated/`이며, 그 외에는 `settings.allow_non_temp_path=true`가 필요합니다. 저장하지 않는 upsert → 실제 컴파일 대기/스크립트 상태 검사 → 명시 저장 순서입니다. `compiled=null`이나 idle만으로 성공을 추정하지 않습니다. 노출 파라미터 선언은 모듈 소비나 런타임 전달 검증과 별개입니다. 아래 Scratch/HLSL·렌더러 명세를 지원합니다. GPU 실행과 자동 후보 개선 루프의 전체 통합은 미완료입니다.
- 기존 BP 연결은 `blueprint.path`, `component`, `mode="existing"`, `allow_official_template_write=true`를 지정합니다. 이 경로는 공식 Niagara `SetSystem(reset=False)`를 직접 사용하므로 Cubeless의 모든 콜백 보호가 자동 적용되지 않습니다. 요청 컴포넌트와 최종 Asset, 컴파일/저장, 기존 컴포넌트 구성은 다시 확인합니다. optional `manifest`는 같은 BP의 edit만 허용하고 바깥 저장 정책과 일치해야 합니다. `mode="wrapper"`는 공식 도구로 새 BP를 만들고 사용자 파라미터를 변수와 Construction Script setter로 연결합니다. 기본 float 흐름과 비기본값 전달을 검증했으며 모든 타입의 지원을 주장하지 않습니다. unsaved wrapper+manifest는 생성 전에 거절합니다.
- PCG: 명시한 **현재 월드**에 새 그래프와 PCGVolume을 작성합니다. 노드 이름은 실제 공식 `ListNativeNodes` 값입니다. `input`/`output`은 stock 입출력 노드의 논리 이름입니다. `cycles`는 최대 8개의 명시적 입력 변경/재생성 단계이며 자동 재시도가 아닙니다. 공식 `ExecuteGraphInstance`의 task-dependent 완료 뒤 idle/source/output/resource와 실제 ISM/HISM 인스턴스 좌표를 검사합니다. 엔진의 managed resource 재사용·정리를 사용하며 명시적 hard cleanup 또는 Cubeless Refresh의 delegate 검수와 동등하다고 주장하지 않습니다. 명시적 `expect.instances=0`일 때만 정상적인 빈 출력을 허용합니다. 공식 task 완료·현재 graph·idle·이번 세션 생성과 실제 instance=0을 확인하며, 단순히 resource가 없다는 이유로 성공시키지 않습니다. 32→0→0→32의 이전 결과 정리·복원을 확인했습니다. BP-PCG 통합·GPU/partition 생성과 기존 영역 Include/Exclude·표면 투영·메시 바운드 패킹은 아직 이 진입점에서 미완료입니다.

PCG의 `mode="edit"`는 `graph`, `world`, 기존 `actor_path`, `updates:[{"node":"Sampler","parameters":{"VoxelSize":{"x":100,"y":100,"z":100}}}]`, `allow_official_write=true`를 받습니다. 깨끗한 기존 그래프의 로컬 native 설정을 수정하고, 실제 속성값·설정 객체·무관한 노드/연결/배치/관측된 authored 설정을 보존 검사한 뒤 재생성합니다. 정상적으로 변하는 파생 표시 제목과 직접 작성한 제목을 구분합니다. topology 편집·공유 설정·subgraph 인스턴스 파라미터는 이 edit 모드에서 아직 지원하지 않습니다. 공식 `UpdateNode`의 트랜잭션과 콜백 동작을 사용하며 원자적 롤백이나 Cubeless의 모든 콜백 보호를 제공하지 않습니다.

optional `spline`은 **같은 실행이 새로 만든 PCGVolume**에만 SplineComponent를 추가합니다. `component_name`, 월드 좌표 XYZ 배열 `points`, `closed_loop`, `allow_official_component_write=true`를 지정합니다. 기존 named spline setter의 native 보호를 재사용하며 선형 포인트를 사용합니다. 그래프에는 명시적인 `Get Spline Data`의 `ActorSelector.ActorFilter="Self"`, `bAlwaysRequeryActors=true`가 필요합니다. `cycles[].spline_points`로 입력을 바꾸고 재생성할 수 있으며 cycle에서 해당 소스의 선택/재조회 정책 변경은 첫 writer 전에 거절합니다. 곡선 탄젠트·영역 제외·투영은 이 간단한 입력 경로의 지원 범위가 아닙니다. `expect.unique_positions=true`는 좌표를 0.0001cm로 반올림해 동일 위치 중복을 검사합니다. 서로 다른 메시의 의도적인 겹침에는 지정하지 않습니다.

Niagara의 optional `module_inputs`는 `edits`(1..256), `operation`, `overwrite_existing`, `continue_on_error`를 기존 named 배치 API에 전달합니다. 각 edit의 emitter/module 식별자·입력·값과 원래 기본값은 해당 API 스키마를 따릅니다. 배치 내부의 부분 실패와 실제 변경은 원시 결과로 보존하며, 실패/미처리 edit가 남으면 후속 파이프라인을 멈춥니다. 성공한 배치는 실제 CPU 컴파일을 확인한 뒤 저장합니다.

`scratch_user_modules`는 최대 8개의 `{"kind":"vec2","settings":{...}}` 항목입니다. `vec2`, `color`, `bool`, `visible_toggle`은 각 기존 Scratch/User recipe API에 연결됩니다. vec2와 bool/HLSL 기반 visible_toggle 제작 대표 경로를 검증했습니다. `settings`는 `scratch_pad_name`, `parameter_name`, `parameter_value`, `user_parameter_name`, `user_parameter_default_value`, stack 선택자 등 공개 입력을 받습니다. 대상·컴파일·대기·저장은 실행기가 지정하고 나머지 기본값은 원 API를 따릅니다. 필요하면 `timeout_seconds`로 해당 recipe의 대기 한도를 명시합니다. 컴파일 완료·무관한 사용자 파라미터 보존을 확인한 뒤 정확한 시스템만 저장합니다. 다른 recipe 타입·고급 소유/상속 조건은 사용 검증 대기입니다.

기존 Niagara 시스템에는 `mode="edit"`, `allow_source_edit=true`와 위 제작 섹션 또는 `user_parameters`를 지정합니다. `save`를 생략하면 create에서는 `true`, edit에서는 `false`이며, 이 값을 Niagara 후속 저장과 Blueprint 바인딩의 compile/save에 동일하게 전달합니다. 바인딩의 optional manifest도 같은 저장 정책이어야 합니다. dirty 시스템은 거절하고 프리셋을 재생성하지 않습니다. CPU 편집 경로이며 live world instance와 소유권 등 추가 조건은 내부 API가 판정합니다. 기존 시스템은 공식 `load_asset`의 정확한 refPath를 확인한 뒤 편집합니다. local Scratch source-capture와 RI 의존 스크립트 동기화를 수리했으며, 기존 Scratch 시스템의 순서 있는 두 행 편집·부분 실패를 확인했습니다. 요청한 RI의 정확한 엔진 의존 대상도 에디터 입력 편집기와 같은 규칙으로 갱신하고 각 writer prefix를 보고합니다. 실패한 원 요청을 이 edit 모드로 다시 보내지 않습니다.

`hlsl_replacements`는 최대 16개의 명시적인 HLSL 본문 교체입니다. `node_guid` 또는 `signature_name`과 `expected_contains`로 기존 노드를 선택합니다. 같은 실행에서 만든 `visible_toggle`을 수정하려면 `recipe_index`로 `scratch_user_modules`의 0 기준 인덱스를 지정합니다. 실행기가 성공 receipt의 실제 GUID·생성 이름을 사용하며 literal node/scratch 선택자를 섞으면 변경 전에 거절합니다. 컴파일 후 실제 GUID·source·graph에 한정해 본문을 재조회합니다. 실행기의 검수 상한은 200000 UTF-16 code unit이며 더 긴 본문은 첫 변경 전에 거절합니다. 이 상한을 Native API 전체 지원 한계로 취급하지 않습니다. DynamicInput 본문은 해당 노드가 사용하는 표현식 형식을 유지합니다.

`renderer_edits`는 최대 16개의 `{"operation":"sprite_settings","settings":{...}}` 항목입니다. `sprite_settings`, `material`, `enabled`, `add_sprite`, `add_mesh`, `cleanup`은 기존 named API에 연결됩니다. cleanup은 명시적인 제거/바인딩 해제 인덱스와 expected topology가 필요합니다. 대상·쓰기·컴파일·저장은 실행기가 소유하며, 엔진 API의 부분 실패·객체 보호는 그대로 유지합니다. Sprite settings/재질 대표 경로는 실제 실행·저장·cold readback을 확인했고 add/cleanup의 실행기 변형은 사용 검증 대기입니다. 반복 후보 평가·원상 복구 전체 흐름이 완성됐다는 뜻은 아닙니다.

`examples/niagara-authoring.json`, `niagara-hlsl-renderer.json`, `pcg-empty-lifecycle.json`에 작은 입력 예제가 있습니다. 대상·저장·실행 정책을 대소문자/밑줄 변형으로 덮거나 정규화 후 중복 인수를 전달하면 첫 변경 전에 거절합니다.

PCG 예제 실행 전 분리된 테스트 월드를 준비하고 입력의 `world`와 실제 현재 월드를 일치시킵니다. 공식 MCP에서 다음 named 도구를 명시적으로 호출해 새 테스트 맵을 만들 수 있습니다. 사용자 월드를 자동 전환하거나 저장하지 않습니다.

```json
{
  "toolset": "cubeless_mcp_ue58.registration.editor_level_session_toolset.CubelessEditorLevelSessionTools",
  "tool": "safe_new_preview_map",
  "arguments": {
    "target_path": "/Game/_MCP_Temp/Production/CubelessTests/ExampleRun_001/Map_Work",
    "dry_run": false
  }
}
```

이는 공식 `call_toolset`의 설명용 표기이며 실제 RPC 인자 이름은 연결된 MCP 클라이언트 스키마를 따릅니다. PCG의 `save_graph=true`는 그래프만 저장합니다. **월드 저장은 별도 명시 작업**입니다.

## PIE와 이미지 검수

Blueprint의 optional `playback`에는 `world`, 새로운 `actor_label`, `warmup_seconds`(0..5), optional `transform`을 지정합니다. `engine_log_path`와 `expected_log_marker`를 함께 지정하면 해당 PIE 요청 이후의 로그 구간에서 BeginPlay 표식을 확인합니다. 기존 PIE나 같은 label이 있으면 시작하지 않습니다. 성공적으로 시작한 PIE만 종료하고, 타임아웃을 취소로 간주하지 않습니다. 배치 Actor는 월드에 남으며 자동 저장/삭제하지 않습니다. PIE 성공은 패키징 또는 렌더링 품질 증거가 아닙니다.

`playback.niagara_checks`는 `component`, `system`, `parameters:[{"name":"User.Intensity","value":8.5}]`를 받습니다. PIE의 실제 컴포넌트·시스템·활성 상태·override 값을 확인합니다. 기본 `require_nondefault_override=true`는 에셋 기본값 복사만으로 BP 전달이 입증됐다고 판단하지 않도록 서로 다른 값을 요구합니다. false로 지정하면 현재 값 확인만 수행합니다. setter의 출처는 작성된 그래프 증거와 함께 판단하며 GPU 완료·파티클 콜백·시각 품질은 별도입니다. 검수 실패와 PIE 정리 실패는 각각 보존하고, 이미 종료된 PIE에는 StopPIE를 다시 보내지 않습니다.

Niagara의 optional `visual`은 고정 `warmup_times`, `views`, 카메라/프레이밍과 `output_dir`을 받습니다. 이미지 검수 경로는 실행 중인 프로젝트의 `Saved/MCP/NiagaraReviews` 아래여야 합니다. 현재 월드를 사용하려면 `allow_current_map_preview=true`를 명시합니다. 캡처 SDK는 동기 렌더 readback 비용이 있을 수 있습니다. 기본 critic은 `visible_frame_ratio >= 0.5`를 요구하며, `criteria`로 기존 visual critic 형식의 기준을 지정할 수 있습니다. 검은 화면을 PNG 저장 성공으로 통과시키지 않습니다. 이미지 지표는 미적 품질의 승인과 다릅니다. 자동 수정/반복 최적화는 아직 연결하지 않았습니다.

기존 critic의 `visible`은 밝은 중성색 픽셀 비율 0.0005 이상을 뜻합니다. 어둡거나 성긴 유색 이펙트의 일반적인 가시성과 같지 않습니다. 실제 rain 캡처가 이 기준에서 거절됐으며 기록을 성공으로 바꾸지 않았습니다. 작업에 맞는 기준과 고정 카메라를 먼저 정하고 원본 이미지도 확인하십시오. 프리셋 생성·컴파일·저장 성공과 이미지 기준 통과는 별도 결과입니다.

이미 제작한 시스템은 `mode="review"`, `save=false`, `system`, `visual`만 지정해 검수할 수 있습니다. 이 모드는 프리셋 생성이나 기존 설정을 재실행하지 않습니다. `output_dir`을 생략하면 실제 대상 프로젝트의 `Saved/MCP/NiagaraReviews/<operation_id>`를 사용합니다. `visual.create_refinement_ledger=true`는 실제 컴파일·이미지 지표로 baseline을 기록하며 `max_iterations`는 1..16입니다. 현재는 baseline 연결까지이며 후보 자동 수정·평가 루프는 미완료입니다. baseline이 선택됐다는 사실을 품질 통과로 취급하지 않습니다.

## 실패와 기록

각 호출 전에 입력과 제출 상태를 저장합니다. 실행 결과 JSON은 v2 receipt index이고, 같은 이름의 `.evidence` 폴더가 원시 응답·스키마·저널입니다. 함께 보존합니다. 읽을 때는 `pipelines.recorded_call_storage.materialize_report(Path(...))`를 사용합니다. 컴파일·저장·현재 변경·불확실 상태는 단계별 native receipt에서 확인합니다.

실패한 요청을 그대로 재실행하거나 같은 출력으로 덮어쓰지 않습니다. 타임아웃 뒤에는 실제 에셋·operation ID·진행 상태·dirty/저장 상태부터 확인합니다. 별도 저장/복구 성공은 원 요청을 성공으로 바꾸지 않습니다. 원자적 롤백은 제공하지 않습니다. `outcome_unknown=true`는 일부 readback의 전역 SDK 부작용 미확정까지 포함할 수 있으므로 각 단계의 실제 mutation prefix도 확인합니다.

검증 프로젝트·생성 에셋·이미지·로그·결과·가상환경은 Git ignore 대상입니다. 배포용 플러그인 리소스에 포함하지 않습니다.
