# Editor 데이터 조회와 명시적 갱신

Slack `기능확장.md`의 호출 실패 목록을 설치 UE 5.8.4 CL58902052와 Cubeless 구현에 대조해 보강했다. 모찌의 실패 목록은 조사 자료이며, 모찌 라이브러리나 RX 게임 코드를 수정하지 않았다. 공식 MCP 서버와 엔진 소스도 변경하지 않았다.

공식 Toolset 이름은 `cubeless_mcp_ue58.registration.editor_data_toolset.CubelessEditorDataTools`다. 아래 도구의 반환값은 구조화된 결과를 담은 JSON 문자열이다. 새 DLL과 Python 파일을 함께 적용하고 Editor를 다시 시작해야 한다. 현재 적용·연결 상태는 이번 작업 보고서의 `deployment`를 확인한다.

## 새 도구

### Material/MaterialFunction 코멘트 박스

`cubeless_mcp_ue58.registration.material_comments_toolset.CubelessMaterialCommentTools`에서 다음 도구를 제공한다.

- `inspect_material_comments(asset_path)`: 코멘트의 두 목록 소속, GUID, 텍스트와 영역을 조회한다.
- `add_material_comment(asset_path, text, x=0, y=0, width=400, height=100, execute=False, save=False)`: 일반 노드 목록 대신 `EditorComments`에 새 코멘트 박스를 만든다.
- `repair_material_comments(asset_path, execute=False, save=False)`: 잘못 `Expressions`에 들어간 코멘트를 같은 객체 그대로 이동한다. 기존 GUID·텍스트·영역·색·서브그래프 참조를 유지한다. 이미 정상인 목록은 변경·저장하지 않는다.

대상은 명시적인 `/Game` Material 또는 MaterialFunction 에셋이다. 대상 Material Editor를 닫은 후 실행하고 다시 연다. 열린 편집기의 미저장 preview를 뒤에서 덮어쓰지 않도록 변경 전에 거절한다. `execute=False`는 사전 확인이며, 실제 변경은 `execute=True`로 요청한다. `save=True`는 생성 또는 실제 복구가 발생한 대상 패키지 전체를 저장하므로 기존 미저장 편집도 포함한다. 기본값은 저장하지 않는다. 복구할 코멘트가 없는 경우 저장만 수행하지 않는다. 원래 영역이 0이면 그대로 보존하며, 새 코멘트의 영역은 양수로 지정해야 한다.

예: `repair_material_comments(asset_path="/Game/Materials/MF_Example", execute=True, save=True)`.

일반 `add_material_node`에 `MaterialExpressionComment`를 지정하면 변경 전에 전용 도구를 안내한다. 설치 UE 5.8의 `CreateMaterialExpressionEx`는 코멘트를 일반 `Expressions`에 넣지만, 그래프는 `EditorComments`를 별도의 `MaterialGraphNode_Comment`로 구성한다. 이 전용 경로는 엔진 소스를 수정하지 않는다. 명시적 셰이더 컴파일·전체 월드 저장·원자적 롤백·불확실 요청 재시도를 수행하지 않는다.

UE 5.8.4에서 Material/MaterialFunction의 생성·동일 객체 복구·저장·새 프로세스 재로드, 함수의 Constant→Output 연결 보존, 실제 함수 편집기의 코멘트 박스 클래스 3개 생성을 확인했다. GUI 캡처 검수는 숨김 창 활성화 실패로 수행하지 못했으며 그래프 클래스 관측과 구분한다. 사용자 함수 `MF_RS_SandGround`/`MF_RS_SandFootprint`는 현재 프로젝트에 없어 해당 에셋을 수정했다고 주장하지 않는다.

콜백 이후 보호 검사는 표현식의 편집 속성·GUID·서브그래프·좌표와 실제 입력 연결을 비교한다. 속성은 텍스트 표시가 아닌 `FProperty::Identical`로 비교하므로 작은 float 변화도 포착한다. 입력은 연결 객체의 정체성·출력 인덱스·마스크를 확인하며 Material 루트 연결도 포함한다. 임시 `GraphNode`와 컴파일러·프리뷰 캐시는 불변 조건으로 고정하지 않는다. Material 자체 설정 전체나 루트 입력의 `UseConstant`·`Constant` 기본값까지 비교한다는 뜻은 아니다.

차이가 발견되면 `first_mismatch`에 객체·속성·기대값·실제값을 남기고 `success=false`, `changed=true`, `change_state_known=false`로 반환한다. Modify 콜백에서 발견하면 코멘트 목록 변경·저장을 중단한다. 저장 이후 콜백에서 발견하면 이미 발생한 `save_attempted`·`saved` 결과를 유지한 채 실패로 보고하며 자동 재시도·롤백하지 않는다. UE 5.8.4의 실제 Modify/PackageSaved 콜백 테스트 8건과 공식 MCP의 Material/Function 정상 생성·복구·저장 호출로 확인했다.

| 도구 | 제공하는 범위 |
|---|---|
| `inspect_blueprint_definition(asset_path)` | 부모/생성 클래스, 로컬·상속 변수와 SCS 선언, 이미 생성된 Actor CDO 컴포넌트. CDO가 없으면 unavailable로 구분하며 조회 때문에 새로 만들지 않는다. |
| `inspect_editor_world_configuration()` | 현재 Editor 월드의 streaming/loaded level, 가시성, World Partition 설정 및 북마크를 읽는다. `include_partition_actors=true`로 로드·미로드 액터와 Data Layer 소속을 함께 조회한다. |
| `inspect_static_mesh_metadata(asset_path)` | 렌더 LOD UV 수, source-model build/reduction 설정, 소켓 이름·변환. 빌드 중인 메시를 기다리지 않고 거절한다. |
| `inspect_component_authoring_data(component_path)` | 로드된 StaticMesh 컴포넌트의 LOD 페인트/override 컬러 수, SkeletalMesh의 저장된 single-animation 기본값, HeightFog 설정. 실제 컬러 원소나 현재 재생 상태의 검수와 구분한다. |
| `inspect_widget_animation_data(asset_path)` | 사용자 부모 클래스도 허용하는 source-local 위젯 트리·애니메이션, display/tick rate와 2D transform section mask. 상속된 source-local 배열을 하나의 배열로 합쳤다고 표현하지 않는다. |
| `inspect_pcg_extraction_settings(settings_object_path)` | GetActorProperty의 현재 extractor enum과 deprecated force-extraction 값, Polygon2D의 Operation/MetadataMode. PCG 활성화가 필요하다. |
| `inspect_editor_performance_settings()` | 실제 EditorPerformanceSettings CDO의 config 값. 읽기만 수행하며 ini나 설정을 바꾸지 않는다. |
| `inspect_niagara_particle_positions(component_path, emitter_index=-1, max_particles=1024)` | 이미 solo·paused이고 tick이 완료된 CPU Niagara 위치를 LWC/로컬 좌표계에 맞춰 월드 좌표로 관측한다. Niagara 활성화가 필요하다. |
| `rerun_actor_construction(actor_path, execute=false)` | 기본값은 사전 확인만 한다. true일 때 명시한 Editor Actor의 construction을 다시 호출한다. ChildActor의 부모 리다이렉트, suspended/reconstructing 상태, compile-error/컴파일 중인 BP 계층과 잠긴 level은 변경 전에 거절한다. |
| `refresh_component_render_state(component_path, execute=false)` | 등록된 Editor 컴포넌트의 `MarkRenderStateDirty` 요청. Decal/PostProcess에도 같은 공개 엔진 경로를 사용한다. 큐 등록과 render thread/GPU 완료를 구분한다. |
| `set_asset_package_dirty(asset_path, dirty, allow_clear=false)` | `/Game`의 명시한 비-map 에셋 **패키지** dirty flag. false로 지우려면 allow_clear=true가 필요하다. 저장·복원·수정 취소가 아니다. |
| `spawn_editor_actor_deferred(class_path, actor_name, transform_json="{}", execute=false)` | 명시한 이름으로 Editor persistent level에 deferred spawn 후 FinishSpawning한다. 기존 이름을 교체하지 않는다. MCP 개발 클래스와 이를 참조하는 Blueprint는 생성 전에 기존 의존성 검사로 거절한다. 분리된 begin/finish 토큰이나 생성 전 임의 프로퍼티 주입은 제공하지 않으며, 임의 BP의 크래시를 방지한다고 보장하지 않는다. |

`inspect_editor_world_configuration(include_partition_actors=true, actor_offset=0, actor_limit=100)`은 현재 Editor 월드의 초기화된 World Partition에 등록된 액터 디스크립터를 읽는다. `partition_actor_inventory.actors`에는 GUID, 액터 경로, 패키지, 레이블, 월드 좌표, 로드 상태, Data Layer 에셋 경로 및 엔진이 이미 해석한 레이어 인스턴스 이름이 포함된다. 레이어 이름이 아직 해석되지 않았다면 `data_layer_instance_names_resolved=false`로 표시하며 빈 소속으로 간주하지 않는다. 좌표와 소속은 디스크립터 기준이므로 저장 전 액터 편집과 다를 수 있다.

기존 인자 없는 호출의 결과는 유지한다. 목록은 액터 경로와 GUID 순이며 `next_offset`으로 이어 읽는다. 페이지당 1~200개, 전체 디스크립터 10만 개까지 지원한다. 호출 간 월드가 변경되면 페이지 결과도 바뀔 수 있다. 일반 레벨과 초기화 전 World Partition에는 `available=false`와 이유를 반환한다. 셀·액터 패키지를 로드하거나 레이어를 배정·저장하지 않는다.

기존 레이어에 액터를 추가하려면 `add_editor_actors_to_data_layer`를 사용한다. 월드 조회의 `actor_path`와 해석된 `data_layer_instance_names`의 정확한 이름을 전달한다. 현재 World Partition 맵의 퍼시스턴트 레벨에 로드된 외부 액터 1~32개와 같은 월드의 기존 일반 Data Layer를 지원한다. 잠긴 레이어, 미로드 액터, 다른 월드의 대상은 배정 전에 거절한다. 레이어 생성·제거와 다른 레이어에서의 이동은 지원하지 않는다.

```json
{
  "actor_paths": ["/Game/Maps/L_Example.L_Example:PersistentLevel.StaticMeshActor_0"],
  "data_layer_instance_name": "DataLayer_0123456789ABCDEF",
  "execute": false,
  "save": false
}
```

기본 호출은 `would_add`와 기존 소속을 반환한다. `execute=true`로 실행하면 기존 소속을 유지한 채 새 소속을 추가하고, 이미 배정된 액터는 건너뛴다. 엔진은 배정 후 에디터 가시성과 선택 상태를 갱신할 수 있다. `save=true`이면 시작 시 깨끗했던 변경 액터의 외부 패키지만 각각 저장한다. 결과의 `actors`에서 액터별 소속·저장 결과를 확인한다. 최상위 `saved`는 한 개 이상 저장 성공, `partial_save`는 일부 저장 성공, `all_requested_actor_packages_saved`는 요청된 변경 패키지의 전체 저장 성공을 뜻한다. 실패하면 남은 배정이나 저장을 중단하며 자동 롤백과 재시도는 없다. 저장하려는 액터의 기존 소속 에셋도 모두 이미 로드된 표준 Data Layer 에셋이어야 한다. 엔진이 저장 과정에서 제거할 수 있는 중복·참조 불가 소속은 쓰기 전에 거절한다.

배정 직후에는 결과의 `memberships_after`나 월드 인벤토리의 `loaded_data_layers.asset_paths`에서 현재 로드된 액터의 실제 소속을 확인한다. 인벤토리의 기존 `data_layer_asset_paths`와 `data_layer_instance_names`는 디스크립터 값으로 유지한다. 같은 에디터에서 맵을 다시 열어도 이 스냅샷에는 저장 전 값이 보일 수 있다. `loaded_data_layers.resolved_direct_instance_names`는 액터의 직접 소속에서 현재 해석할 수 있는 인스턴스 이름이다. 상위 Level Instance로부터 상속한 소속은 이 필드에 포함하지 않는다. 이 조회는 액터나 레이어 에셋을 로드하지 않는다.

Actor·컴포넌트는 전체 object path로 지정한다. label·선택 상태·짧은 이름으로 대상을 추정하지 않는다. 에셋은 `/Game`·`/Engine`의 package/object path를 받으며 다른 플러그인 mount의 읽기 대상은 이미 로드되어 있어야 한다.

갱신 도구는 기존 공통 mutation slot과 저널을 사용한다. 반환된 `operation_id`로 기록을 연결하며, 진행 중인 다른 Cubeless 변경 작업을 우회하지 않는다. construction은 컴포넌트를 재생성할 수 있으므로 호출 후 경로를 다시 해석해야 한다. SDK 반환과 임의 사용자 construction의 결과 검증은 별개다.

```json
{
  "toolset_name": "cubeless_mcp_ue58.registration.editor_data_toolset.CubelessEditorDataTools",
  "tool_name": "inspect_static_mesh_metadata",
  "arguments": {"asset_path": "/Engine/BasicShapes/Cube"}
}
```

이는 공식 MCP `call_tool`의 인자다. 스폰의 `transform_json`은 `{"location":[300,400,50],"rotation":[0,90,0],"scale":[1,2,1]}`처럼 전달하며 회전 순서는 pitch/yaw/roll이다. 실제 Unreal Python Transform 생성에는 이름 있는 Rotator 인자를 사용한다.

## 기존 Niagara 정리 보강

기존 `CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes`의 공개 이름·입력·기본값은 유지한다. 시스템/에미터 main graph뿐 아니라 소유 scratch와 연결된 소유 script를 중복 없이 검색하며 source/graph 경로를 반환한다.

- 먼저 `dry_run=true`로 후보를 읽고 `node_guid`로 좁힌다. 같은 핀을 쓰는 노드가 여러 개이면 기존 `allow_multiple` 보호가 거절한다.
- emitter selector를 지정했는데 그 emitter가 참조하는 system-owned scratch도 영향을 받으면 전체 System scope를 요구한다. 실제 쓰기 전에 전체 요청을 거절하며 일부부터 삭제하지 않는다.
- 외부 shared/inherited source는 검색될 수 있지만 소유권이 증명되지 않으면 수정하지 않는다. 비대상 파라미터를 함께 쓰는 MapSet을 통째로 제거하지 않는다.
- 완전하고 동일한 전후 관측과 writer/compile/save/wait 0이 확인된 의미상 사전 거절은 실패를 유지하면서 known으로 종결한다. 실제 변경·불일치·불완전 관측은 unknown을 유지한다. 전역 SDK 순수성까지 입증됐다는 뜻이 아니다.
- 공식 검색 스키마의 boolean·array·string을 그대로 사용한다. Python 소스의 일부 내부 JSON-string carrier를 공식 공개 입력으로 잘못 사용하지 않는다.

## C++를 추가할 필요가 없었던 경로

설치 엔진과 실제 Python 호출에서 확인한 경로다. 기능에 필요한 표준 플러그인이 활성화되어 있어야 한다.

| 요청에서 찾지 못한 경로 | 실제 경로 |
|---|---|
| `StaticMesh.get_num_uv_channels` / source build settings | `StaticMeshEditorSubsystem.get_num_uv_channels(mesh, lod)` / `get_lod_build_settings`. 새 metadata 도구는 이 정보와 source model·socket을 묶는다. |
| `SingleAnimationPlayData.b_saved_looping` | `get_editor_property("saved_looping")`. 저장된 기본값이며 현재 재생 상태가 아니다. |
| HeightFog `volumetric_fog` | `get_editor_property("enable_volumetric_fog")`. 해당 이름의 단일 구조체는 없다. |
| PCG Polygon `mode` | `get_editor_property("operation")`, `metadata_mode`. |
| `GeometryScriptLibrary_MeshBasicEditing` | `unreal.GeometryScript_MeshEdits`; 단순 클래스 미존재와 플러그인 비활성화를 구분한다. |
| `GeometryScriptUVList.list` | `unreal.GeometryScript_List.get_uv_list_item` / `convert_uv_list_to_array`. 실제 UV 2개 왕복을 확인했다. |
| `KismetMaterialLibrary` | **`unreal.MaterialLibrary`**. 기존 런타임 MPC/동적 머티리얼 기능을 MaterialEditingLibrary로 일괄 대체하지 않는다. 에셋 저작은 MaterialEditingLibrary와 기존 CubelessMaterialTools를 사용한다. |
| `SequenceTimeUnit` | `unreal.MovieSceneTimeUnit`. |
| `MovieScene.get_display_rate` | SequencerScripting 플러그인이 활성화된 시퀀스에는 `MovieSceneSequenceExtensions.get_display_rate(sequence)`; WidgetAnimation의 MovieScene rate/mask는 새 조회 도구로 읽는다. |
| `SplineMeshParams.start_scale` | 해당 struct의 `get_editor_property("start_scale")`로 읽힌다. |
| MPC scalar/vector `parameter_name` | 각 struct의 `get_editor_property("parameter_name")`로 읽힌다. 기존 MPC values 도구도 유지한다. |
| `Rotator.rotate_vector` | `rotator.quaternion().rotate_vector(vector)`. 회전 생성은 `Rotator(pitch=..., yaw=..., roll=...)`로 인자를 명시한다. |
| `SystemLibrary.get_project_name` | `SystemLibrary.get_game_name()` 및 `Paths.get_project_file_path()`. |
| 컴포넌트 `get_relative_location` | `get_editor_property("relative_location")`. |
| `unreal.Engine` / GEngine | 범용 GEngine 노출을 추가하지 않는다. Editor subsystem·GameplayStatics·각 기능별 도구를 사용한다. |

머티리얼/함수 노드는 기존 `CubelessMaterialTools.list_material_nodes`가 컬렉션을 순회한다. MPC 조회도 기존 도구를 사용하므로 동명의 중복 getter를 추가하지 않았다. 원문의 RXWorldSettings 클래스는 현재 설치 프로젝트에서 검증하지 않았으며, 표준 AWorldSettings bookmark accessor만 RX 모듈 의존성 없이 지원한다.

## 검증과 한계

검증은 소유 `_MCP_Temp/FeatureExpansion/CubelessTests` 에셋과 격리 에디터에서 수행한다. 주요 근거는 `cubeless/records/operations/feature-expansion-20261029/report.json`에 연결한다. 원 실패·자체 검수의 잘못된 가정·후속 수리는 분리해 보존한다.

Niagara 조회는 컴포넌트를 자동 pause/solo로 바꾸거나 tick/wait/GPU readback을 수행하지 않는다. GPU·실행 중·미완료 데이터와 정상 빈 CPU buffer, 출력 상한에 따른 부분 결과를 구분한다. 일반 배치 시뮬레이션의 실시간 무중단 snapshot은 이번 지원 범위가 아니다.

`changed`·`change_state_known`·`mutation_attempted`, 실제 단계와 compile/save 필드를 함께 읽는다. Native 호출 후 반환 파싱이나 저널 기록에 실패해도 변경 없음으로 보고하지 않는다. 원자적 롤백이나 불확실 요청의 자동 재시도는 제공하지 않는다. 새 도구는 자동 저장하지 않으며, 소유 대상의 별도 저장·복구 성공을 원 실패의 성공으로 바꾸지 않는다.

엔진 API 호출부·Python 작업 로직·공식 등록부는 분리되어 있다. C++는 Editor 플러그인 모듈에만 추가했고 게임 C++·Runtime 모듈·외부 MCP 서버는 추가하지 않았다. 이번 검증은 Editor/MCP/저장·재로드 범위이며 별도 패키징·게임 실행이나 성능 배수 측정의 증거는 아니다.

## StaticMeshComponent 인스턴스 버텍스 컬러

UE 5.8.4의 공개 C++ `UMeshPaintingSubsystem::SetInstanceColorDataForLOD`는 `UFUNCTION`이 아니며 Python에 노출되지 않는다. 이를 Editor 전용 래퍼로 연결했다. 공식 MCP Toolset 이름은 `cubeless_mcp_ue58.registration.instance_vertex_colors_toolset.CubelessInstanceVertexColorTools`이다.

| 도구 | 동작 |
|---|---|
| `inspect_instance_vertex_colors` | 명시적인 `component_path`와 `lod_index`의 유효 RGBA8·원본/override 구분을 조회한다. `count=0`은 메타데이터, `include_positions=true`는 메시 로컬 좌표를 포함한다. 페이지 상한은4096이다. |
| `set_instance_vertex_colors` | `colors_json`의 `[[r,g,b,a],...]`를 지정한 render LOD의 컴포넌트 override에 쓴다. 기본 `execute=false`는 사전 확인이며, 기존 데이터 교체에는 `allow_replace=true`가 필요하다. |
| `clear_instance_vertex_colors` | 지정 LOD의 override와 painted cache만 제거하여 원본 메시/기본 흰색으로 되돌린다. 기본은 사전 확인이며 실제 삭제에는 `execute=true, allow_remove=true`가 필요하다. |

색상 배열은 render vertex 수와 정확히 같아야 하며 각 채널은0..255 정수다. 감마 변환은 하지 않는다. UV/노멀 경계에서 분리된 정점도 각각의 인덱스이므로 원본 모델의 고유 위치 수와 다를 수 있다. 최대1048576개 정점과 전체 보존 snapshot 상한이 적용된다. 변경 후 모든 LOD의 override 합계와 painted cache 합계도 각각 이 상한을 만족해야 한다. 다른 LOD에서 엔진이 재생성할 캐시까지 사전 계산하며, 초과하면 `Modify()` 전에 `projected_color_snapshot_bound`로 거절한다.

Python에서 현재 Editor 월드의 컴포넌트 객체를 명시적으로 받은 경우:

```python
import json, unreal
lib = unreal.CubelessInstanceVertexColorLibrary
info = json.loads(lib.inspect_instance_vertex_colors(component, count=0))
assert info["success"]
colors = [[255, 32, 0, 255] for _ in range(info["vertex_count"])]
result = json.loads(lib.set_instance_vertex_colors(
    component, json.dumps(colors), execute=True, save=False))
```

공식 MCP에서는 같은 인자에 `component` 객체 대신 정확한 `component_path`를 쓴다. 별도 MCP 서버는 필요 없다. 이 Editor 라이브러리 함수를 게임 Blueprint에 삽입하지 않는다. 결과는 표준 Engine 컴포넌트 데이터로 저장되며 게임용 MCP 모듈이나 클래스를 추가하지 않는다.

필수 조건은 MeshPainting 플러그인의 활성화·로딩, 유휴 Editor의 로드·등록된 배치 StaticMeshComponent와 CPU render 데이터다. CDO/템플릿, PIE/SIE, ISM/HISM의 개별 인스턴스, SkeletalMesh에는 쓰지 않는다. 엔진의 `CanMeshPaintVertexColors()`가 거절하는 Nanite proxy/paint-disabled 대상은 쓰기 전에 거절한다. 이 API가 Nanite 설정을 자동 변경하지 않는다. 이전 메시 topology와 맞지 않는 override key는 자동 remap하지 않고 먼저 명시적 fixup을 요구한다.

`save=false`가 기본이다. `save=true`는 해당 컴포넌트의 **정확한 소유 패키지**를 `UPackage::SavePackage`로 저장한다. 외부 Actor와 별도 BuiltData로 재귀 확장하는 FileHelpers 저장 경로를 사용하지 않는다. 같은 패키지의 다른 객체도 함께 저장되므로, 호출 전에 이미 dirty였다면 `allow_save_dirty_package=true`가 추가로 필요하다. `save_filename`에 대상 파일을 반환하며 이 API에서 자동 소스 제어 checkout을 요청하지 않는다. 다른 레벨·원본 메시를 일괄 저장하지 않는다. 동일 색상·캐시 또는 이미 비어 있는 삭제의 no-op은 저장하지 않는다. 원본 메시 색상·다른 LOD·transform·material은 보호한다. PaintedVertices의 Color·Position·Normal을 보존 검사하며, 새 캐시는 변경 전에 캡처한 엔진 render position·TangentZ 기대값과 대조한다. GPU 자원 해제에 엔진의 render fence를 사용하며 새 자원 초기화는 큐에 등록한다. CPU readback 성공을 GPU 완료로 표현하지 않는다.

실패 시 `stage`, `first_mismatch`, `writer_entered`, `changed`, `change_state_known`, `saved`와 `package_dirty_after`를 함께 확인한다. `change_state_known=false`이면 실제 대상을 관측해야 하며 원 요청을 자동 재실행하지 않는다. 저장 후 검수가 실패하더라도 실제 `saved=true`를 유지한다. 원자적 롤백은 없다.

UE 5.8.4 CL58902052에서 빌드·로딩·공식 MCP3개 스키마, 정점별 RGBA 쓰기, 두 컴포넌트·두 LOD 보존, 잘못된 배열/타입/LOD와 교체·삭제·dirty 저장 경계, 명시적 저장과 새 프로세스 재로드를 확인했다. 저장 중 정상 생성되는 lighting ID와 cache는 설치 엔진 `StaticMeshComponent::PreSave` 동작에 맞춰 준비한 뒤 다시 검사한다. 원 저장 성공을 실패로 거절했던 초기 기록은 보존했다. 빈 캐시의 다른 LOD 재생성 경계는 독립 소스 검토 근거이며 별도 상태 주입 실험은 하지 않았다. 임의 콜백 부작용·대형 메시 성능·새 패키징 검증으로 결과를 확대하지 않는다. 원시 근거는 `cubeless/records/operations/instance-vertex-colors-20261030/report.json`에 있다.

후속 리뷰의 저장 범위·캐시 geometry·변경 후 총량 결함은 `instance-vertex-colors-reviewfix-20261030/report.json`에서 추적한다. 실제 엔진 콜백으로 Modify 및 저장 후 Position/Normal 변조 4조건을 감지했고, 저장 후 실패도 `saved=true`와 실제 dirty 상태를 유지했다. 외부 Actor·BuiltData의 파일 해시와 dirty 상태를 보존하면서 월드 패키지만 저장했으며, 외부 Actor 자체를 대상으로 한 독립 저장도 확인했다. 총량은 같은 production 계산 함수의 초과·정확한 상한·상한+1·cache 단독 초과 4조건으로 확인했다. 대형 엔진 메시 성능 검증은 아니다. 최초 회귀 실행의 추상 표식 객체 ensure는 테스트 자료 결함으로 보존하고 정상 메타데이터로 수정한 저장 범위 검사만 새 소유 월드에서 다시 실행했다.


## 2026-11-04 모찌 C++ 노출 요청 2차

요청서는 RX UE 5.6 관측이다. 설치된 UE 5.8.4 소스에서 다시 확인해 엔진 수정 대신 Editor 전용 `CubelessAuthoringAccessTools`와 기존 도구를 연결했다. 현재 검증 상태는 로컬 `records/operations/authoring-access-20261104/report.json`에 기록한다. 등록만으로 모든 변형의 동작 완료를 뜻하지 않는다.

공식 Toolset 이름은 `cubeless_mcp_ue58.registration.authoring_access_toolset.CubelessAuthoringAccessTools`다. 도구는 명시적인 전체 asset/component 경로를 받으며 UI 선택에 의존하지 않는다.

| 원문 | 현재 경로와 범위 |
|---|---|
| 최우선 Skeleton.remove_bones_from_skeleton | `remove_skeleton_bones`: 기본 사전 확인, 루트 제외, 부모 제거 시 자손 집합 계산. 실행에는 `allow_remove=true`와 `allow_dependent_updates=true` 모두 필요하다. |
| 최우선 OptimusDeformer.compile | `compile_optimus_deformer`, `inspect_optimus_deformer`. 필수 플러그인 DeformerGraph를 Editor 대상 의존성으로 선언한다. |
| A-1 CurveFloat.float_curve | `inspect_float_curve`, `set_float_curve_keys`: 키·모드·탄젠트·가중치, 기본값·외삽 보존. 샘플링으로 키를 추측하지 않는다. |
| A-2 NiagaraSystem.exposed_parameters | 기존 `CubelessNiagaraParameterTools.inspect_niagara_user_parameters`; 새 중복 도구를 등록하지 않는다. |
| A-3/4 Timeline tracks / Blueprint.Timelines | `inspect_blueprint_timelines`: source-local float/vector/color/event 트랙과 곡선 참조. 반환된 곡선은 별도로 조회한다. |
| A-5 EdGraph.Nodes | 기존 graph `inspect_graph`; 추가 `inspect_specialized_anim_graph`는 ControlRig 실제 클래스와 PoseDriver 이름·코멘트의 묶음 조회다. |
| A-6 Texture2D.Source | `inspect_texture_source_pixels`: mip별 원래 포맷 바이트, 포맷·bytes_per_pixel 포함. 한 block/layer, mip 256MiB와 페이지 4096픽셀 상한. RGBA8·GPU 데이터로 오인하지 않는다. |
| A-7/B-3/B-4 World persistent/streaming/visibility | 기존 `inspect_editor_world_configuration`: loaded_levels의 persistent 식별자와 visible, streaming 실제·목표 상태. |
| A-8 AnimSequence.target_frame_rate | 기존 `inspect_animation_asset_metadata_guarded`의 `sampling_frame_rate`와 `data_model_frame_rate`; 설치 엔진의 실제 target getter는 GetSamplingFrameRate다. |
| A-9 AssetImportInfo.source_files | `inspect_asset_import_sources`: import-data 경로, 원본 상대·해석 경로, timestamp와 기록 hash. 파일 읽기·재임포트는 하지 않는다. |
| A-10 PoseDriverTarget.target_name | 실제 필드는 `DrivenName`; `inspect_specialized_anim_graph`의 `pose_driver_driven_names`. 존재하지 않는 이름을 엔진에 추가하지 않는다. |
| A-11/12 ControlRig class/comment | `inspect_specialized_anim_graph`: Node.GetControlRigAssetReference().GetBlueprintClass()와 UEdGraphNode.NodeComment. |
| A-13 CameraAnimationSequence.bound_actor_class | `inspect_camera_animation_binding`: BoundActorClass soft 경로. 클래스 로드·바인딩 변경 없음. |
| A-14 MaterialEditorOnlyData.expression_collection | 기존 `CubelessMaterialTools.list_material_nodes`와 머티리얼 함수 코멘트 도구를 사용한다. 컬렉션 직접 노출을 추가하지 않는다. |
| A-15 Montage.composite_sections | 기존 `inspect_animation_asset_metadata_guarded`: 이름·시작/끝·길이·다음 섹션 및 슬롯·세그먼트; 기존 Montage 편집 17개 도구를 유지한다. |
| B-1/2 Package.is_dirty / opened assets | `inspect_asset_editing_state`: 이미 로드된 에셋 패키지 dirty·전체 열린 에디터 에셋. 관측 결과는 저장 잠금이나 덮어쓰기 승인이 아니다. |
| B-5 deferred spawn | 기존 `spawn_editor_actor_deferred`: Editor 월드에서 한 요청 안에 begin/finish 수행. 요청 사이에 미완성 Actor를 남기는 토큰 API는 제공하지 않는다. |
| B-6 PoseAsset.is_valid_additive | `inspect_pose_asset_additive`: 엔진 flag 조회이며 포즈 품질 보증은 아니다. |
| B-7/8/9 SkeletalMeshComponent | `inspect_skeletal_component_state`, `set_skeletal_post_process_disabled`, `tick_skeletal_animation`. 로드된 Editor 컴포넌트만 수정·tick하며 PIE/SIE·CDO는 거절한다. |
| B-10 Texture2D.post_edit_change | `refresh_texture_editor_data`: 명시 실행, authored 값·소스 식별자 보존 검사, 자원 갱신 요청과 GPU 완료 구분. |
| B-11 BoneReference.bone_name | 설치 Python 반영 여부를 실행 기록에서 확인한다. 구조체를 UObject처럼 get_class로 호출하지 않는다. |
| B-12 binding ID | 기존 SequencerScripting `MovieSceneBindingExtensions.get_id(binding)`. 의미가 다른 binding-ID struct와 GUID를 혼동하지 않는다. |
| B-13 ObjectLibrary | 기존 AssetRegistry 필터와 EditorAssetLibrary.load_asset/AssetData.get_asset 조합을 사용한다. 모든 에셋을 무조건 로드하는 새 ObjectLibrary 도구를 추가하지 않는다. |
| C/E RX 전용 클래스 | 현재 표준 엔진 프로젝트에 RX 클래스가 없으므로 구현·설치됐다고 표현하지 않는다. RX 쪽 라이브러리의 정확한 함수명을 별도로 확인해야 한다. |
| D 오호출 | 올바른 기존 이름을 사용한다. 새 엔진 함수·동명 alias를 양산하지 않는다. 특히 AnimationLibrary Notify, Blueprint.get_blueprint_parent_class, MovieSceneTimeUnit은 새 C++ 노출 대상이 아니다. |

### 호출 예제와 중요한 경계

```json
{"toolset_name":"cubeless_mcp_ue58.registration.authoring_access_toolset.CubelessAuthoringAccessTools","tool_name":"remove_skeleton_bones","arguments":{"asset_path":"/Game/MySkeleton","bone_names_json":"[\"unused_leaf\"]"}}
```

사전 확인의 `planned_removed_bones`와 `loaded_animations_subject_to_validation`을 먼저 확인한다. 실제 제거는 같은 명시 입력에 `execute=true, allow_remove=true, allow_dependent_updates=true`를 사용한다. 엔진은 GUID·remapping·retarget metadata·blend profile을 갱신하고 유효하지 않은 virtual bone을 제거하며 이미 로드된 애니메이션을 검증한다. 이 부작용은 원자적이지 않으며 메시/애니메이션 원본을 일괄 재작성·저장하는 기능이 아니다. 제거 성공을 원본/참조 에셋을 안전하게 삭제해도 된다는 판정으로 사용하지 않는다.

`set_float_curve_keys`는 `keys_json='[{"time":0,"value":0},{"time":1,"value":1}]'`처럼 전달한다. 기존 키 교체·전체 비우기는 `allow_replace=true`가 필요하다. interp/tangent 모드는 0..2, weight 모드는 0..3이고 기본값은 linear/auto/unweighted다. 명시한 탄젠트를 자동 재계산하지 않는다. 기본값·외삽·event 설정을 바꾸지 않는다.

Skeleton/Curve 저장은 기본 false이고 `save=true`일 때 정확한 대상 패키지만 UPackage::SavePackage로 저장한다. 이미 dirty인 패키지는 `allow_save_dirty_package=true`도 필요하다. 다른 패키지를 자동 저장하지 않는다. Texture 갱신·컴포넌트 조작·Optimus 컴파일은 자동 저장하지 않는다. 실패에서는 `stage`, `first_mismatch`, `mutation_attempted`, `changed`, `change_state_known`, `compile_attempted`, `save_attempted`, `saved`, `package_dirty_after`를 함께 읽는다. 저장 후 검수가 실패해도 실제 saved 결과를 유지한다. 불확실한 요청은 재실행하지 않는다.

Optimus CompileEnd는 비동기 셰이더 컴파일이 끝나기 전에 발생한다. `graph_compile_succeeded=true`와 CompiledWithWarnings는 그래프 단계 성공이며 `full_compile_complete=null`, `shader_completion_verified=false`를 유지한다. 현재 도구의 상태 조회는 GPU 준비 완료를 인증하지 않는다. 늦게 오는 shader 진단까지 수집했다고 주장하지 않으며 콜백 관찰자는 동기 호출 범위가 끝나면 해제한다.

### 2026-11-04 공식 MCP 실행 결과

현재 개발 프로젝트의 UE5.8.4 CL58902052, 검증된 DLL로 도구16개의 검색·등록·입출력 스키마와 도구별 기본 정상 호출을 확인했다. 대표 호출 31건 중 정상 22건, 의도한 사전 거절 9건이 기대 결과와 일치했다. CurveFloat의 키·탄젠트 가중치 저장과 교체/no-op/dirty 저장 경계, Skeleton161→160 leaf 제거·승인/루트/자손 보호, 원래 예제 사본의 Optimus 그래프 컴파일, 텍스처 source bytes·import 정보·갱신, PoseAsset·카메라 soft binding, 컴포넌트 상태/토글/tick을 확인했다. CurveFloat와 Skeleton 패키지를 실제 reload하여 값과 제거 결과를 확인했으며, 컴포넌트 변경은 소유 테스트 맵에 명시 저장했다. 기존 Content8551개 파일은 모두 기준 해시를 유지했고 dirty/PIE 없이 소유 검증 에디터를 정상 종료했다. 프로젝트 재실행 시 Python startup에서 자동 등록한다.

원문 BoneReference.bone_name은 설치5.8 Python의 get_editor_property로 실제 조회되어 추가 getter가 필요하지 않다. 일반 AnimBlueprint1315개 노드의 조회를 확인했으나 해당 에셋에는 ControlRig/PoseDriver 노드가 없었다. 그 특화 분기와 비어 있지 않은 Timeline 트랙, 임의 콜백·고급 변형의 상세 검증 및 새 C++ 독립 검토는 후속 범위다. Optimus의 Compiled 상태와 단일 커널 shader-success 로그를 전체 GPU 준비 완료로 확대하지 않는다. 전체 프로젝트 빌드의 기존 RX_SMLODGen unity 이름 충돌은 별도이며, 이번 Cubeless 모듈 빌드와 실제 프로젝트 로딩은 통과했다.

검수 준비 스크립트는 소유 맵으로 전환하면서 Python 실행기의 context 검사를 실패시켰다. 원 실패를 유지하고 생성·저장된 실제 상태와 비활성 transaction을 별도 관측한 뒤 진행했으며 원 요청은 반복하지 않았다. 후속 저장·재로드 성공도 원 실패의 성공으로 바꾸지 않았다. 원시 근거는 records/operations/authoring-access-20261104/report.json에 연결한다.

### 2026-11-04 리뷰 P2 3건 수정

`set_float_curve_keys`는 탄젠트를 자동 계산하는 `FRichCurve::SetKeys()` 대신 `Reset`·`ReserveKeys`·`AddKey`·`GetKey`로 입력 키 전체를 구성한다. Auto 모드와 Linear/User 모드의 명시 탄젠트 및 가중치가 저장·재로드 후에도 유지된다. 기본값과 infinity 설정도 유지하며, 같은 입력은 저장하지 않는 no-op다.

`remove_skeleton_bones`는 `Modify()` 직후 소켓 값과 참조·순서를 검사하고 통과한 경우에만 본 제거 API를 호출한다. `set_skeletal_post_process_disabled`도 setter 전에 소유 Actor·월드·transform·메시·등록 상태를 다시 검사한다. 콜백이 상태를 바꾸면 추가 writer와 저장을 중단하고 변경 상태를 불확실로 유지한다. 콜백이 이미 만든 변경을 롤백했다고 표현하지 않는다.

현재 프로젝트 UE5.8.4의 수정 DLL 빌드·로딩, 공식16개 스키마와 영향 경로의 대표9회 호출(정상7·의도 거절2)을 확인했다. 실제 Modify 콜백을 주입한 소켓 값/목록 및 컴포넌트 등록/Actor 위치 경계와 명시 탄젠트 검사 등 Automation 3건이 통과했다. CurveFloat·Skeleton 저장·패키지 재로드와 기존 Content8551개 해시 보존을 확인했다. 저장·재로드를 감싼 Python 요청은 실행 완료 후 문맥 검사에서 실패했으며 원 실패를 보존했다. 해당 요청을 반복하지 않고 별도 읽기 전용 조회로 실제 값·clean 상태와 비활성 transaction을 확인했다. 소유 에디터는 정상 종료했다. 상세 증거와 남은 범위는 기존 `records/operations/authoring-access-20261104/report.json`의 `review_p2_repairs`에 연결한다. 임의의 모든 콜백·부분 저장·고급 변형과 독립 리뷰까지 완료한 것은 아니다.


### 2026-11-04 마무리 검증 완료

앞선 미검증 목록 중 비어 있지 않은 Timeline과 ControlRig/PoseDriver 특화 조회를 이번에 검증했다. 현재 UE5.8.4 CL58902052에서 수정 모듈 빌드, 공식 MCP16개 스키마와 정상 CurveFloat 조회, 네이티브 회귀6개가 모두 통과했다. 특화 분기는 네이티브 API 호출로 검증했으며 모든 변형을 공식 MCP로 다시 실행했다고 확대하지 않는다. 변경하지 않은 도구의 기존 공식 MCP 기본 동작과 저장·재로드 근거는 그대로 유지한다.

`inspect_float_curve`와 `set_float_curve_keys`는 기존 default/key time/value/tangent/weight의 NaN/±Inf를 `non_finite_curve_data`로 거절하고 `Modify`·저장에 진입하지 않는다. 7개 필드×3개 비정상 값×읽기/편집42조건과 유효한 FLT_MAX 기본값을 검사했다. 응답 직렬화 실패 경로도 관측한 변경·저장 상태를 유지한다.

하나의 Timeline에 float/vector/linear_color/event 트랙4개를 넣어 이름·정확한 곡선 참조·external flag를 확인했다. Blueprint와 정확한 EdGraph 경로에서 ControlRig 클래스·코멘트, PoseDriver의 DrivenName2개를 확인했다. 별도 소스 리뷰의 잔여 P2는 해결됐으며 최종 소스 독립 리뷰에서 추가 결함은 발견되지 않았다.

원본과 이전 소유 fixture를 포함한 Content8562개 파일의 해시를 유지했고 콘텐츠 추가도 없다. dirty/PIE 없이 소유 PID33452가 exit0과 `LogExit: Exiting`으로 종료됐다. 이번 요청의 구현과 해당 기본·특화 검증은 완료다. 임의의 모든 상속·콜백 조합, 새 패키징 실행과 Optimus 전체 GPU 완료 인증은 이 결과에 포함하지 않는다. 상세 결과는 `records/operations/authoring-access-20261104/finish-complete.json`, 원시 실행은 `finish-8454f7bf16/report.json`에 있다. 커밋·푸시는 하지 않았다.
