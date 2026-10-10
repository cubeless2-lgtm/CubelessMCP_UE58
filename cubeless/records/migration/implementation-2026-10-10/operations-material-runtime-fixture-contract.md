# Material/MPC no-MCP fixture 계약

실행 상태: root의 실제 author B, 새 프로세스 cold C, Shipping cook/stage/Pak 및 fresh BeginPlay marker 검증이 명시 범위에서 통과했다. `material-runtime-proof-B.json/.md`가 원본 증거를 연결하며 `operations-material-runtime-proof-B-independent.json/.md`는 바이트/정확한 Pak 항목/기록 계약을 별도 대조한 결과다. Shader 완료와 렌더링 품질, 전체 Material 기능 이식은 검증되지 않았다.

## 독립 대상과 보존

프로젝트를 `cubeless/workspaces/UE58MaterialRuntimeProof/CubelessMaterialRuntimeProof.uproject`로 제한한다. 준비 보고서는 `material-runtime-project-preparation-A.json`이며 root가 복사한6개 에셋과 각 원본6개 파일의 SHA-256을 확인한다. 원본 프로젝트, UE 엔진, gameplay C++, descriptor와 Config를 스크립트가 변경하지 않는다. 엔진 기본 플러그인 비활성화, 에디터용 Python/EditorScriptingUtilities만 활성, 프로젝트 플러그인 부재, actual enabled plugin 목록에서 MCP/Toolset 부재를 확인한다.

기존 E3 Material은 Constant→Add.A→BaseColor이며 MPC 표현식은 연결되어 있지 않다. 따라서 원본 Material/MPC의 editor load만으로 MPC가 실제 cook에서 사용됐다고 주장할 수 없다. 원본 graph readback을 별도로 보존한 뒤 공식 AssetTools.duplicate_asset로 `RuntimeMaterial_<E3 run>_<run suffix>/M_RuntimeMPC`를 만들고 공식 MaterialEditingLibrary.connect_material_expressions로 CollectionParameter→Add.B를 연결한다. 이 추가 연결은 공식 API를 사용한 runtime fixture 구성이고 기존 Cubeless graph 수정 증거와 구분한다.

원본 복사맵을 메모리에 로드하고 Engine.StaticMeshActor 하나에 `/Engine/BasicShapes/Cube.Cube`와 clone material slot0을 할당한다. 원본 RuntimeProof Actor와 author-D Blueprint Actor의 class/label을 확인한 뒤 **새 경로** `RuntimeMaterial_<E3 run>_<run suffix>/Map_MaterialRuntimeProof`에 저장한다. 원본 복사6개와 source6개는 모두 disk bytes가 불변이어야 한다. 새로 저장하는 파일은 clone Material과 새 Map2개뿐이다.

## 두 실행 환경

`create_material_runtime_fixture.py`는 에디터 내부 Python이다. 명시 입력은 `--project-file`, `--preparation-report`, `--material-author-report`, `--marker-fixture-report`, `--run-suffix`, `--report`이며 선택적인 `--prior-attempt-report`는 실패 이력 연결용이다. 새 suffix의 새 경로가 이미 있으면 거부한다. 실패한 이전 clone을 재사용/덮어쓰거나 중단 작업을 자동 재시도하지 않는다. 함수/기본값/재시도/검수 근거는 설치된 UE5.8.3 공개 헤더와 구현을 직접 읽었다. EditorLoadingAndSavingUtils.load_map/save_map을 사용하며 별도 UI 선택 상태에 기대지 않는다. PIE/게임 실행, marker 생성, 프로세스 시작/종료, cook을 수행하지 않는다.

`validate_material_fixture_without_mcp.py --project-file ... --fixture-report ... --report ...`도 에디터 내부 Python이며 **다른 PID**에서 수행한다. 원본 E3 graph/MPC, clone의 실제 MPC 출력 연결, map cube의 mesh/material 슬롯과 기존 Actor를 다시 읽는다. AssetRegistry hard/soft package와 management category를 분리한 bounded recursive audit를 재사용하고 Map→clone Material→MPC의 on-disk hard package edge를 명시적으로 요구한다. Blueprint3개와 clone Material은 메모리에서만 compile 요청하며 저장하지 않는다. 새2개와 원본복사6개, source6개 해시 불변을 확인한다.

같은 validator의 `--cook-log <파일>` 모드는 **외부 일반 Python**이다. `--project-file`과 상호 배타이며 unreal을 import하지 않는다. 로그의 정확한 long package/full Content path, cook 완료 문자열, shader warning/error/failure, 기타 fatal/error를 기록한다. Map/clone/MPC 중 하나의 경로 근거가 없거나 cook 끝이 없으면 incomplete로 남는다. 로그에서 이름을 찾았다는 것만으로 실제 staged package 존재를 보장하지 않으므로 root의 UAT exit/receipt/Pak inspection과 함께 판단한다.

## compile·runtime 증거 한계

RecompileMaterial은 resource가 없을 때도 빈 오류 배열을 반환하므로 `status=returned_without_reported_errors`, `shader_completion_verified=false`다. 이 상태를 모든 셰이더 완료로 격상하지 않는다. cook의 실제 shader 로그와 패키지 포함 검수는 별도로 수행한다. 렌더링 이미지 품질은 미검증이다.

기존 `runtime-fixture-B.json`은 수정하지 않고 SaveGame class/slot/run/Engine function의 marker 계약으로만 재사용한다. 새 Material report의 `parents.runtime_marker`가 경로와 hash를 보존한다. marker verifier에는 기존 B 보고서를 그대로 전달하되 fresh UserDir/marker 부재 baseline/새 실행 파일 hash/launch 기록을 사용해야 한다. 새 맵과 Material/MPC가 포함됐다는 근거는 새 report와 cook/Pak 검사에서 별도로 확인한다. Marker 성공만으로 머티리얼이 렌더링됐다고 주장하지 않는다.

## 반환·실패

새 author schema는 `cubeless_material_runtime_fixture_v1`이다. `assets.map/runtime_material/collection`, `source_assets`의 원본6개, `asset_files`의 source/copy before-after, `new_asset_files`의 새2개 hash, `parents`, `marker_contract`, `material_readback`, `runtime_material_readback`, `map_after`, `compile`, `save`, `dependency_audit`를 분리한다.

검수 schema는 `cubeless_material_without_mcp_verification_v1`이다. 성공·실패는 report.success로 확인한다. Python startup process의 exit만으로 성공 처리하지 않는다. 보고서는 덮어쓰지 않으며 stage/시도 상태를 변경 전에 기록한다. 실패 mutation은 자동 재시도하지 않고 partial assets와 실제 dirty/disk 결과를 읽어 별도 reconciliation을 기록한다. atomic rollback은 제공하지 않는다.

offline 검수9개는 cook 종료/자산 근거 누락, shader/error 진단, 잘못된 schema, empty log, short-name 혼동, 다른 프로젝트 거부, 기존 보고서 보존과 다음 두 API/경로 반례를 검증했다. `operations-material-runtime-offline-tests-D.json`은 합성 검수이며 UE 실행 증거가 아니다.

실제 A는 `get_static_mesh`가 Python에 노출되지 않아 실패했다. reflected `static_mesh` property readback으로 고쳤고 저장된 A clone은 보존한 채 새 B 경로에서 별도 실행했다. B author가 통과한 뒤 cold B는 SaveMap 직후 live World가 원래 경로를 유지하는 것을 잘못 비교해 실패했다. 현재 cold validator는 source/new map의 정확한 package.object prefix만 허용하고 동일 `:PersistentLevel...` suffix를 유지해 새 맵 경로로 매핑한다. 임의 경로 완화 없이 cold C가 통과했다. A/cold B 원본 실패 증거는 불변이다.

Shipping 검수에서 새 맵·runtime Material·MPC의 정확한 Pak 항목과 이전 맵 부재를 별도로 확인했다. `material-cook-log-audit-B.json`은 로그에 MPC가 없어 계속 incomplete다. Pak 포함 확인으로 그 보고서 자체를 PASS로 바꾸지 않는다. AssetRegistry 검수의 실제 옵션은 `include_editor_only_package_references=false`이며 과거 limitations 문구와 다르면 실제 옵션이 범위를 정한다.
