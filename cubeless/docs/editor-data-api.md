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
| `inspect_editor_world_configuration()` | 현재 Editor 월드의 streaming/loaded level, 실제·요청 가시성, WorldPartition 설정, 표준 WorldSettings bookmark accessor. 셀을 로드하거나 셀별 삼각형/메모리를 계산하지 않는다. |
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
