# 사용·업데이트·제거와 문제 해결

## 짧은 사용 흐름

`<Python>`은 프로젝트의 외부 가상환경, `<Run>`은 해당 프로젝트의 `cubeless/tools/run_pipeline.py` 절대 경로입니다. 입력 예제는 실행마다 새 소유 에셋 이름을 지정합니다.

```powershell
& '<Python>' '<Run>' blueprint --endpoint http://127.0.0.1:8000/mcp --input cubeless/tools/pipelines/examples/blueprint.json
& '<Python>' '<Run>' niagara --endpoint http://127.0.0.1:8000/mcp --input cubeless/tools/pipelines/examples/niagara.json
& '<Python>' '<Run>' pcg --endpoint http://127.0.0.1:8000/mcp --input cubeless/tools/pipelines/examples/pcg.json
```

각 명령은 preview로 입력·스키마·현재 대상의 일부 사전 조건을 확인합니다. 모든 엔진 변경을 시뮬레이션하는 dry-run이나 실행 성공 보장이 아닙니다. 실제 변경에는 `--apply`를 추가합니다. 결과 경로를 명시한다면 매 실행 새 `--output`을 씁니다. PCG 예제의 `world`는 별도 소유 테스트 맵과 일치해야 하며 실행기는 현재 사용자 월드를 자동 전환·저장하지 않습니다.

BP는 create/edit·기본값·지원 노드/연결·컴포넌트와 보존 검사를 제공합니다. Niagara는 CPU preset·User·모듈 batch·기존 BP 연결과 고정 캡처를 제공합니다. `save` 생략 시 BP=false, Niagara create=true/edit=false이며 Niagara manifest와 바깥 저장 정책은 일치해야 합니다. PCG의 `save_graph=true`는 그래프만 저장하고 월드는 별도 명시 저장입니다. `expect.instances=0`일 때만 빈 결과를 허용하며 공식 task 완료와 현재 실제 결과를 함께 검사합니다. 상세 입력과 고급 지원 공백은 [실행기 계약](../tools/pipelines/README.md)에 유지합니다.

## 결과와 실패

플러그인에 의존하는 API는 해당 플러그인이 활성화되고 로드된 환경에서 사용합니다. 기능별 필수 플러그인을 확인하며, 꺼져 있다면 상태는 ‘필수 플러그인 활성화 필요’입니다. 이 조건만으로 API의 구현 결함이나 이식 미완료로 판단하지 않습니다. 활성화된 환경에서 실제 정상 동작과 필요한 검증을 마쳤다면 ‘정상 동작 확인 — 필수 플러그인: …’으로 보고합니다. 모든 프로젝트에서 모든 선택 플러그인을 켤 필요는 없습니다. 실제 로딩·실행 실패와 아직 검증하지 않은 범위는 별도로 표시합니다.

콘텐츠가 사용하는 정상적인 엔진 런타임 플러그인은 패키징에도 필요할 수 있습니다. 관련 이름·설명·설정 파일이 포함된 것만으로 게임 실행 실패라고 판단하지 않습니다. 완성된 게임이 Cubeless·공식 MCP·에디터 전용 작업 코드에 의존하지 않아야 한다는 기준은 유지합니다.

최종 `status=passed`와 단계별 native `changes`, `compile`, `save`, 실제 readback을 함께 확인합니다. 등록·접수·idle·PNG 생성만으로 완전 성공을 판단하지 않습니다. 이미지 critic 통과도 미적 승인과 다릅니다. 결과 JSON과 같은 이름의 `.evidence` 폴더를 함께 보존합니다. `pipelines.recorded_call_storage.materialize_report(Path(...))`는 분리된 receipt와 무결성을 검증해 읽습니다.

결과 파일·`.partial`·`.evidence` 이름이 이미 있으면 링크 대상의 존재 여부와 관계없이 새 실행을 거절합니다. `.partial`은 배타적 생성으로 열기 때문에 사전 확인 이후 같은 이름이 생겨도 기존 파일이나 dangling 링크를 따라가거나 덮어쓰지 않습니다. 기록 저장이 실패하면 의심되는 임시 파일을 자동 삭제·재사용하지 않고 원 기록과 실제 변경 상태부터 확인합니다.

실패한 수정 요청은 그대로 재실행하지 않습니다. 마지막 제출 단계·operation ID·원시 응답·현재 대상·진행 작업·dirty·컴파일·저장 상태부터 확인합니다. 클라이언트 타임아웃은 엔진 취소가 아닙니다. `outcome_unknown=true`이면 관측 후 조치하며, saved=true여도 나중 단계 검수가 실패할 수 있습니다. `dirty=false`만으로 실제 변경이 없다고 판단하지 않습니다. 별도 복구/저장 성공은 원 요청의 성공이 아닙니다. 원자적 롤백을 제공하지 않습니다.

공식 SetSystem/UpdateNode 등 직접 호출은 Cubeless의 모든 콜백·수명 보호와 동등하지 않습니다. 원시 단계별 receipt와 엔진 상태를 확인해야 합니다.

## 업데이트·제거

검증마다 같은 DLL·PDB를 별도 백업하지 않고 소스 bundle manifest·엔진 빌드·DLL 해시·기존 산출물 위치를 기록해 재사용합니다. 현재 적용본과 필요한 심볼, 검증된 최신 복구본, 고유 실패 재현 자료는 보존합니다. 종료된 소유 검증 프로젝트의 재생성 가능한 Intermediate/캐시만 별도로 정리하며 records·프로젝트 소스·설정·에셋을 일괄 삭제하지 않습니다.

업데이트 전 진행 요청·dirty 상태를 확인하고 해당 에디터를 안전하게 닫습니다. 기존 코드·로컬 설정·records·사용자 콘텐츠를 구분해 백업합니다. 새 배포의 manifest와 파일별 hash를 확인하고 배포 코드·가이드만 교체합니다. 기존 프로젝트 파일·AGENTS.md·설정·가상환경·records를 통째로 덮어쓰거나 삭제하지 않습니다. 코드 파일이 배포에서 제거됐다면 이전 manifest와 비교해 그 파일의 소유권·사용 여부를 확인한 뒤 별도로 처리합니다. 다른 UE 빌드의 DLL을 혼용하지 말고 새 소스를 빌드한 후 연결과 실제 project_file을 다시 확인합니다.

`distribution-files.txt`는 제품 파일의 명시 목록이며, 목록 밖의 로컬 설정을 삭제하거나 배포에 포함하지 않습니다. 목록의 파일이 하나라도 없으면 배포 생성은 실패합니다. 설치·업데이트 시 같은 이름의 파일에 사용자 수정이 있으면 백업과 차이 확인 후 통합하며, 목록에 있다는 이유만으로 사용자 수정을 자동 덮어쓰지 않습니다.

제거할 때는 진행 작업을 마무리하고 기록과 사용자 콘텐츠를 보존한 뒤 해당 프로젝트에서 Cubeless를 비활성화합니다. 에디터 종료 후 소유한 `Plugins/CubelessMCP_UE58`만 제거할 수 있습니다. `cubeless`에는 로컬 기록·설정이 있을 수 있으므로 자동 일괄 삭제하지 않습니다. 필요 없는 외부 가상환경·도구는 각각 소유권과 실행 종료를 확인해 제거합니다. 공식 MCP나 다른 프로젝트의 공유 도구를 함께 지우지 않습니다.

## 문제 해결

| 증상 | 확인할 사항 |
| --- | --- |
| 프로젝트 없음/복수/불일치 | cubeless가 .uproject 바로 아래에 있는지, 같은 루트의 --project인지 확인 |
| 빌드 실패 | UE 5.8.x Build.version, C++/SDK, 정확한 Editor Target, descriptor 의존 엔진 플러그인, 원 빌드 로그 확인 |
| Cubeless 도구 미등록 | 플러그인/DLL·공식 Registry·Python 초기화와 pip 로그 확인; 검색 성공을 행동 검증으로 해석하지 않음 |
| 외부 import 실패 | 외부 venv에 pipeline-requirements 설치; 프로젝트 로컬 run_pipeline.py 사용; 에디터 Python과 혼용하지 않음 |
| MCP 연결/재시작 오류 | 실제 포트·프로세스·프로젝트 확인, 새 initialize 세션 ID 유지; 수정 재전송 금지 |
| dirty 사전 거절 | 현재 변경을 먼저 관측하고 해당 소유 에셋의 저장 여부를 별도로 결정; 저장 정책을 임의로 바꾸지 않음 |

## 검증 범위와 직접 검수

2026-10-25에 Windows x64/UE 5.8.x에서 배포 소스만 설치한 별도 `Studio58` 프로젝트를 빌드하고, 별도로 만든 외부 Python 3.11.15 환경에서 공식 MCP 대표 실행을 확인했습니다. C++ 도구는 MSVC 14.44.35228/Windows SDK 10.0.22621이었습니다. 플러그인 시작 코드가 도구를 자동 등록했으며 별도 검증 fixture나 시작 스크립트를 넣지 않았습니다.

| 배포본 대표 흐름 | 엔진 도구 호출 수 | 실행 시간 |
| --- | ---: | ---: |
| BP 제작·compile/save | 14 | 2.01초 |
| Niagara CPU 제작·실제 compile·User·BP 연결·저장 | 18 | 12.16초 |
| BP 기본값 편집·기존 실행 연결 보존·PIE 로그 확인 | 22 | 3.11초 |
| PCG 32→0→0→32·이전 결과 정리·중복/좌표 검사 | 45 | 2.47초 |

시간은 해당 PC의 대표 입력 한 번을 측정한 외부 실행기 벽시계 시간이며, 발견용 메타 도구 호출·별도 월드 저장은 호출 수에서 제외했습니다. PCG 결과와 소유 월드를 명시 저장한 뒤 새 에디터 프로세스에서 BP 값·실행 연결·Niagara 참조/User 값과 PCG 그래프·32개 인스턴스의 좌표를 확인했습니다. 저장된 네 패키지의 직접 참조에는 Cubeless/MCP/Python 런타임 의존성이 없었습니다. 기존 고정 캡처·저장 정책 경계·런타임 패키징 증거는 구조가 변하지 않은 범위에서 재사용했으며, 이번 신규 배포 프로젝트의 별도 패키징을 수행한 것은 아닙니다.

영문+공백 경로와 이름을 바꾼 한글+공백 경로에서 프로젝트 로컬 실행기를 `C:/Windows/Temp`에서 PYTHONPATH 없이 실행했습니다. 한글 경로에서는 BP 제작·compile/save가 14호출/1.32초로 통과했습니다. 이 환경의 한글 경로 직접 C++ 빌드는 C1083으로 실패해 같은 엔진의 영문 경로 빌드 산출물을 사용했습니다. [설치 가이드](installation.md)의 제한과 대안을 함께 확인하십시오. 잘못된 프로젝트 선택·복수/누락 후보·MCP 프로젝트 불일치는 변경 전에 거절했습니다.

이는 동일 PC의 별도 소유 프로젝트 검증입니다. 실제 다른 PC·다른 플랫폼·GPU·모든 고급 타입·원본 UE 5.6 전체 계약을 검증했다는 의미는 아닙니다. 원시 결과·실패 로그·재로드 증거는 개발 checkout의 `cubeless/records/verification/practical-pipelines-20261025-AV/`에 보존하고 배포에서는 제외합니다.

직접 검수는 새 소유 폴더에서 ① 공식 MCP 프로젝트 일치와 Toolset 조회 ② BP preview→apply와 compile/save ③ Niagara CPU 생성→BP 연결 및 save=false/true receipt ④ PCG 소유 월드에서 생성·입력 변경·빈 결과 정리 ⑤ 실패 기록과 별도 저장 구분 순서로 확인합니다. 사용자 콘텐츠로 처음 시험하지 않습니다.

## Niagara 실무 경로 보강 — 2026-10-31

UE 5.8.4에서 기존 시스템의 User 파라미터·모듈 입력·스프라이트 렌더러 편집, 컴파일, 실제 타격 Blueprint 전달과 종료를 소유 복사본에서 확인했습니다. 필수 기능 플러그인은 Niagara입니다. 저작과 MCP 호출에는 기존 공식 MCP·Toolset Registry·Python·Cubeless Editor 플러그인을 활성화합니다. 게임 콘텐츠에는 표준 Niagara와 Blueprint만 사용합니다.

기존 등록 이름과 입력 기본값은 유지합니다. `cubeless_mcp_ue58.registration` 아래 Toolset 이름을 공식 `describe_toolset`으로 조회한 뒤 `call_tool`로 호출합니다.

| Toolset / 메서드 | 짧은 입력 예제와 역할 |
| --- | --- |
| `niagara_authoring_inspection_toolset.CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack` | `{"system_path":"/Game/MyFX/NS_Hit","include_pins":true}` — 실제 모듈과 핀 조회 |
| `niagara_parameters_toolset.CubelessNiagaraParameterTools.upsert_niagara_user_parameters` | `{"system_path":"/Game/MyFX/NS_Hit","parameters_json":"[{\"name\":\"User.HitSize\",\"type\":\"vec2\",\"value\":[14,14]}]","allow_source_edit":true,"save":false,"request_compile":true}` |
| `niagara_modules_toolset.CubelessNiagaraModuleTools.set_niagara_module_inputs_batch` | `{"system_path":"/Game/MyFX/NS_Hit","operation":"set_existing","edits":[{"emitter_index":0,"module_name":"SpawnRate","input_name":"SpawnRate","value":96}],"allow_source_edit":true,"save":false,"request_compile":true,"wait_for_completion":true}` |
| `niagara_renderers_toolset.CubelessNiagaraRendererTools.set_niagara_renderer_material` | `{"system_path":"/Game/MyFX/NS_Hit","emitter_index":0,"renderer_index":0,"material_path":"/Niagara/DefaultAssets/DefaultSpriteMaterial.DefaultSpriteMaterial","allow_source_edit":true,"save":false}` |
| `niagara_emitter_configuration_toolset.CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target` | `{"system_path":"/Game/MyFX/NS_HitGPU","emitter_index":0,"sim_target":"GPU","auto_fixed_bounds":true,"allow_source_edit":true,"save":false,"request_compile":true}` — 명시적 GPU 전환 |
| `niagara_authoring_inspection_toolset.CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status` | `{"system_path":"/Game/MyFX/NS_HitGPU","request_compile":false,"wait_for_completion":true}` — 현재 컴파일 결과 관측 |

User 선언이나 component setter의 성공은 모듈 소비의 증거와 구분합니다. 대표 시스템은 기존 Scratch/User recipe로 `User.HitColor`를 `Particles.Color`에, `User.HitSize`를 `Particles.SpriteSize`에 연결했습니다. Blueprint는 활성화하지 않은 컴포넌트를 만들고 `Set Niagara Variable (LinearColor/Vector2)`를 실행한 뒤 활성화합니다. 머티리얼이 비어 있으면 fallback이 그려질 수 있으므로 렌더러의 실제 `primary_material`도 확인합니다. 위 경로와 다른 임의 시스템은 같은 이름·타입의 입력을 실제로 소비하도록 구성해야 합니다.

컴파일 조회의 `success=true`는 조회 완료입니다. `data.legacy_result`의 active/outstanding 요청·오류·필수 실행 스크립트 상태를 확인해야 합니다. 기존 `is_ready_gpu`는 VM/DI 실행 적합성입니다. GPU Compute Script에는 현재 Editor RHI 기준 `gpu_shader_compilation_pending`, `gpu_shader_compilation_succeeded`, `gpu_ready_to_execute`를 추가했습니다. `gpu_shader_compilation_applicable=false`인 CPU 스크립트의 세 필드는 `null`입니다. GPU 사용 판정에는 pending=false·succeeded=true·ready=true와 전체 시스템의 컴파일 상태를 함께 확인합니다. 정상 컴파일뿐 아니라 `NCS_UpToDateWithWarnings`·`NCS_ComputeUpToDateWithWarnings`도 다른 준비 조건을 만족하면 허용합니다. 이것을 GPU 프레임 완료나 시각적 출력의 증거로 사용하지 않습니다.

개발 checkout의 소유 검수 맵은 `/Game/_MCP_Temp/NiagaraGenerated/Practical/CubelessTests/Run20261031/L_NiagaraCombat`입니다. 기존 MotionMatchingCombat·ArenaUI·원본 샘플을 덮어쓰지 않은 로컬 자료이며 배포 파일에는 포함하지 않습니다. 적 인스턴스의 `EffectSystem`, `HitColor`, `HitSize`로 CPU/GPU 시스템과 전달값을 바꿀 수 있습니다. WASD·LMB 공격·RMB 회피·Q 락온·결과 화면 Retry를 사용합니다. 원래 피해량과 Notify 승인 기준은 유지하고, 실제 체력 감소 후에만 별도 이펙트를 만듭니다. 표적 소멸과 이펙트 수명은 분리하며, 방출 중단은 활성화 요청 시점부터 게임 시간 0.10초 임계값을 넘은 Tick에서 처리합니다. 렌더 캡처나 긴 프레임으로 중단 관측 시각이 늦어질 수 있습니다.

`BP_HitBurst`는 `EffectState`·`FailureReason`·`OutcomeFinal`로 결과를 구분합니다. `normal_complete`는 이 예제의 공유 시뮬레이션 활성화 수락 관측, 자체 방출 중단, 동일 FX의 완료 콜백이 모두 확인된 경우이며 이때만 `Completed=true`입니다. 원래 Niagara lifetime에 의한 자연 종료나 GPU 프레임 진행을 증명하는 값은 아닙니다. `ActivationObserved`는 `IsActive`와 컴포넌트 Tick 비활성 상태를 조합한 공유 시뮬레이션 경로 관측입니다. Solo·PSO 대기 등 관측이 확인되지 않은 경로는 `activation_pending`에서 완료 시 `activation_unconfirmed`로 남기며 성공으로 집계하지 않습니다.

유효하지 않은 System, `fx.NiagaraComponentsEnabled=0`, 컴포넌트 생성 실패는 `activation_failed`로 구분합니다. `CancelBurst`나 관측된 실행의 조기 소멸·비활성은 `early_terminated`입니다. 취소는 최종 결과를 먼저 확정한 뒤 FX를 정리하며 늦은 콜백이 결과를 성공으로 덮어쓰지 않습니다. `OnSystemFinished`만으로 성공을 판정하지 않고 sender·수명·Ending·최초 최종 결과 보호를 유지합니다. 관측 불확실 경로에서도 기존 0.10초 방출 중단 정책은 유지합니다. `HitEffectCount`는 Burst 생성·시도 수이며 성공한 방출 수가 아니므로 실제 결과는 각 Burst 상태로 확인합니다.

편집 호출은 `save=false`로 묶고 확인한 소유 패키지만 별도로 저장합니다. 생성 기본 save=true·편집 실행기 기본 save=false 및 Blueprint 바인딩/manifest 저장 정책은 바꾸지 않았습니다. 사용자 월드를 저장하거나 save-all을 호출하지 않습니다. 실패 시 원시 결과의 `changes`, `compile`, `save`, `retry`, 상세 `data`를 함께 확인합니다. 이번 의도적 HLSL 오류에서는 그래프 수정·컴파일 실패·미저장이 관측됐고, 원 API의 불확실 상태는 그대로 보존했습니다. 후속 읽기와 별도 보존 저장은 원 실패의 성공이나 자동 재시도로 처리하지 않았습니다. `NS_CompileFailure`는 의도적인 컴파일 오류를 보존한 소유 테스트 에셋이며 정상 사용·게임 콘텐츠의 대상이 아닙니다.

CPU/GPU 대표 렌더의 색상·크기 반영과 종료를 확인했으며, 정확한 GPU 파티클 수·좌표·모든 타입/플랫폼의 지원을 검증한 것은 아닙니다. 관측은 Windows D3D12/SM6·RTX 5060 Laptop GPU·256×256/FOV45 고정 상대 카메라에서 수행했습니다. GPU seed를 고정하지 않아 완전한 결정성을 주장하지 않습니다. 새 패키징은 수행하지 않았으며 기존 런타임 모듈 구조는 유지했습니다. 원시 성공·실패, 저장·재로드, 소스 검토와 적용 DLL 근거는 `cubeless/records/operations/niagara-practical-20261031/report.json`에 통합합니다.


## DataTable 새 생성과 기존 행 편집의 지원 범위 — 2026-10-31

공식 MCP의 `cubeless_mcp_ue58.registration.datatable_toolset.CubelessDataTableTools`를 사용한다. 기존 CRUD가 허용하는 saved-clean compiled UDS의 bounded 배열·중첩 UDS·Set/Map·byte enum·resident hard/soft 참조 필드도 `create_datatable`에서 같은 source/default/수명 보호를 거쳐 새 ordinary UDataTable을 생성할 수 있다. 새 공개 도구나 통신 경로를 추가하지 않았다. 기존 경로의 타입·깊이·원소 수·참조 제한과 K2 제한은 유지한다.

```json
{"asset_path":"/Game/Data/DT_Item","row_struct_path":"/Game/Data/S_ItemRow","save":false}
```

`asset_path`는 존재하지 않는 새 대상이어야 하며 `row_struct_path`는 먼저 저장하고 컴파일한 구조체다. dirty 구조체와 기존 대상은 변경 전에 거절한다. `save`의 기존 기본값은 true이며 false는 생성/행 수정만 하고 파일 저장을 요청하지 않는다. true는 검사한 대상 DataTable 패키지를 직접 저장하며 원본 구조체·열거형·참조 에셋을 자동 저장하지 않는다. 실패하면 success/error와 changes/save/data의 실제 변경·저장 결과를 확인한다. 원자적 롤백과 자동 재실행은 제공하지 않는다.

UE5.8.4에서 byte enum+double 필드 구조체의 생성→행 추가·수정→명시 저장→새 에디터 재로드, 기존 대상/dirty source 거절을 확인했다. 후속 검증에서 int32/FString 배열, 중첩 UDS, int Set, Name→int Map의 생성·행 저장·새 프로세스 재로드와 resident CurveFloat의 non-null hard/soft 기본 참조 보존도 통과했다. 각 유형의 대표 경로 근거이며 모든 조합·경계 크기·콜백 변형의 검증 완료를 뜻하지 않는다. 후속 원시 근거는 `cubeless/records/operations/datatable-variants-20261031/report.json`이다. 전체 원본 계약이나 모든 변형 검증 완료를 의미하지 않는다. 기존 행 편집의 성공 증거는 재사용한다. 원시 근거는 로컬 `cubeless/records/operations/datatable-support-20261031/report.json`이며 배포 자료에 포함하지 않는다.
