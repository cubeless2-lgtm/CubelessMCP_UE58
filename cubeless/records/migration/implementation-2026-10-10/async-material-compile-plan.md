# UE 5.8.3 Material compile/save 비동기 이식 설계

2026-10-10. **소스 조사와 구현 설계이며 아직 구현·등록·실행 검증한 도구가 아니다.** Material 첫 9개 도구 및 Build E의 Source는 변경하지 않았다. 설치 엔진은 `Engine/Build/Build.version` 기준 5.8.3, CL 58210709, `++UE5+Release-5.8`이다.

## 결론과 구현 경계

공식 `UToolCallAsyncResultString`은 사용할 수 있다. Python `@tool_call`의 반환 타입을 `unreal.ToolCallAsyncResultString`으로 선언하고 실제 객체를 반환하면 Registry는 완료 이벤트까지 future를 유지한다. 외부 Python MCP 서버나 TCP bridge는 필요 없다.

긴 shader/DDC 완료 대기는 `FMaterialResource::BeginCacheShaders(Background)` 제출과 게임 스레드의 `IsCompilationFinished()` polling으로 나눌 수 있다. **그러나 기존 `RecompileMaterial()`을 이 promise 뒤에 숨기는 구현은 불가**하다. 이 함수는 GC, debug shader 완료 대기, texture streaming export를 직접 수행한다. `ForceRecompileForRendering(Background)`도 내부 `CacheShaders → FinishCacheShaders` 때문에 DDC 완료를 기다릴 수 있다. 공개 submit API의 이름만 보고 전체 호출을 nonblocking이라고 표시하면 안 된다.

권고는 비동기 job/journal 기반을 먼저 만들고, 이미 공식 편집 API로 갱신된 일반 `/Game/` UMaterial의 렌더 resource를 대상으로 작은 검증을 시작하는 것이다. 이 첫 resource 범위는 **준비된 현재 resource의 컴파일/저장 검증**이다. 임의의 raw graph 수정, 강제 DDC key 갱신, child MI 전체 갱신, streaming data 재생성 및 Material Function 전체 동작까지 기존 함수와 동등하게 이식했다고 집계하지 않는다. 이들 범위는 아래 준비 단계의 차이를 해소하고 실제 5.8 실행으로 검증할 때까지 미완료로 남긴다. 기존 전체 계약과 엄격한 전체 nonblocking을 동시에 보장하는 공개 API 경로는 이번 조사에서 확인하지 못했다.

## 1. 기존 기능 계약

원본 Python `Python/tools/material_tools.py:549`:

```text
compile_and_save_material(material_path: str, graph_type: str = "auto", save: bool = True)
```

명령은 `compile_and_save_material`, 구현은 원본 `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp:5625`이다.

- resolver로 Material 또는 Function을 해석하고 package dirty 상태를 기록한다.
- Material: `RecompileMaterial` → current platform resource → **FinishCompilation 동기 대기** → IsCompilationFinished/GetCompileErrors/IsCompilingOrHadCompileError 검사.
- Function: `UpdateMaterialFunction(Function, nullptr)`만 호출한다. 원본은 이 경로에서 shader 진단 없이 초기값 `compiled=true`, `compilation_finished=true`를 반환한다. 이것은 모든 호출자 permutation의 shader 컴파일 증거가 아니다.
- `save=true && compiled=true`: `Target.MarkChanged()` 후 `SaveLoadedAsset(false)`를 호출한다. MarkChanged의 PostEditChange가 직전 검증 후 다시 작업을 시작할 수 있으므로 새 구현에서 그대로 반복하지 않는다.
- 기존 반환 키는 `material`, `graph_type`, `compiled`, `compilation_finished`, `compile_error_count`, `compile_errors`, `saved`, `save_skipped_due_to_compile_errors`, `was_dirty_before_compile`, `dirty_after_compile`이다. 실패는 공통 error response로 반환한다.

새 결과는 이 키를 보존하면서 stage, scope, job ID, compile/save 시도 및 불확실 상태를 추가한다. 기존 Function의 근거 없는 compiled=true는 그대로 답습하지 않는다. 외부 검수 Python 중 단순 compiled 체크를 하는 항목은 새 scope 검사를 함께 요구하도록 별도 이식해야 한다.

## 2. 공식 비동기 반환 경로: 실제 근거

아래 경로는 설치 엔진 루트 `C:/Program Files/Epic Games/UE_5.8/Engine/` 기준이다.

| 근거 파일/줄 | 확인한 동작 |
| --- | --- |
| `Plugins/Experimental/ToolsetRegistry/Content/Python/toolset_registry/tool_call_impl.py:119,163,216` | annotation을 unreal.ufunction의 `ret`으로 전달하며 AICallable을 붙인다. coroutine를 자동 await하지 않는다. `async def`만 작성하는 방식은 선택하지 않는다. |
| `Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/programmatic.py:906,938,952` | 공식 Python tool이 `ToolCallAsyncResultString` 생성 후 즉시 반환하고 나중에 완료시키는 실제 구현. `:619`는 Python의 Unreal 호출은 worker thread에서 차단됨을 명시한다. |
| `Plugins/Experimental/ToolsetRegistry/Source/ToolsetRegistry/Private/ToolsetRegistry/ObjectFunctionToolCall.cpp:464–491` | reflected return object가 async result면 `UToolCallAsyncResultFutureHandler`를 구독하고 완료 future를 반환한다. null result는 오류. `:516` 최종 JSON은 `returnValue`에 감싼다. |
| `.../Public/ToolsetRegistry/ToolCallAsyncResultString.h:17` | `SetValue(FString)` 및 Value UPROPERTY. JSON object가 아니라 JSON 문자열이 returnValue에 담기는 계약이다. |
| `.../Public/ToolsetRegistry/ToolCallAsyncResult.h:91,103` 및 `.../Private/ToolsetRegistry/ToolCallAsyncResult.cpp:56–123` | SetError 또는 SetValue로 완료하고 OnCompleted 방송 후 delegate를 비운다. 구독 후 `BroadcastOnCompletedIfComplete()`가 필수. 직접 bIsComplete 쓰기 금지. |
| `.../Private/ToolsetRegistry/ToolCallAsyncResultFutureHandler.cpp:19–85` | 구독/완료는 게임 스레드. future가 handler를 강하게 보유한다. Unsubscribe는 future에 Canceled 오류를 설정하지만 compiler를 취소하는 함수가 아니다. |
| `.../Private/ToolsetRegistry/FunctionLibraryToolset.cpp:158–194` | 호출과 continuation은 게임 스레드를 요구하며 script error를 수집한다. |
| `Plugins/Experimental/PythonScriptPlugin/Source/PythonScriptPlugin/Private/PyWrapperDelegate.cpp:1207–1209` | Python multicast delegate에 add_callable/add_callable_unique가 실제 노출된다. |

`UToolCallAsyncResultFutureHandler`의 Result 멤버는 UPROPERTY로 표시되지 않은 TObjectPtr이다. 따라서 Registry가 result 객체까지 GC로부터 보유할 것이라고 추정하지 않는다. native job manager가 pending inner promise를 `TStrongObjectPtr`로 보유하고 Python job table도 outer promise와 callback을 보유한다. 실제 GC 도중 생존 테스트가 필요하다.

새 등록 함수의 제안 타입은 아래와 같다. 엔진 API가 아니라 **우리 구현 예정 계약**이다.

```python
@tool_call
@staticmethod
def compile_and_save_material(
    material_path: str, graph_type: str = "auto", save: bool = True,
    operation_id: str = "", timeout_seconds: float = 120.0,
) -> unreal.ToolCallAsyncResultString:
    # validate -> durable async lease -> native compile -> later finalize
    ...
```

원본 3개 입력/기본값은 유지한다. operation_id/timeout_seconds는 실패 복구용 새 선택 입력이며 describe schema에 명시한다. 상한과 기본 timeout은 실행 측정 후 확정한다. 반환은 기존 Cubeless JSON envelope를 JSON 문자열로 만든 값이다. native helper는 BlueprintCallable+AIIgnore로 감추며 범용 command executor를 추가하지 않는다.

## 3. 취소와 timeout의 실제 한계

`ModelContextProtocolServer.cpp:697–727`은 notifications/cancelled를 받으면 해당 tool의 CancelAsync를 호출하고 ActiveRequests에서 지운다. `:878–882`는 취소되거나 session이 사라진 요청의 나중 결과를 버린다.

하지만 `ModelContextProtocolEditor/Private/ModelContextProtocolToolsetRegistryAdapter.h:12–26`의 Registry adapter는 **CancelAsync를 override하지 않는다**. `ModelContextProtocol/Public/IModelContextProtocolTool.h:97`의 기본 구현은 비어 있다. eager adapter 및 Registry로 보내는 call_tool 경로에서 이 신호가 우리 UObject job까지 전달된다는 근거는 없다. socket/client timeout도 editor operation 중단 증거가 아니다. 이번 확인 범위에서 Registry async result에 compiler cancellation/timeout API는 없다.

- transport 취소/단절: native job이 계속 실행될 수 있고, 이미 요청한 save까지 실행될 수 있다. 호출자는 operation_id로 조회해야 한다. 이 경우 자동 저장 금지를 보장하려면 **우리 explicit cancel tool 또는 native deadline**까지 도달해야 한다. MCP의 취소 신호만으로 저장이 금지됐다고 말하지 않는다.
- 우리 deadline 또는 명시적 cancel: 다음 단계로의 진행, 특히 save를 금지한다. compiler가 즉시 멈췄다고 표시하지 않는다. `timed_out`/`cancelled`와 `engine_jobs_may_continue=true`를 분리한다.
- `UMaterial::CancelOutstandingCompilation()`은 모든 platform resource를 순회한다(`Material.cpp:6164`). resource의 CancelCompilation도 shader map ID의 공유 작업을 취소한다(`MaterialShared.cpp:892`). 다른 사용자의 작업/공유 shader map을 건드릴 가능성 때문에 초기 구현은 호출하지 않는다.
- deadline 뒤 compiler resource가 계속 살아 있으면 passive observer와 package quarantine를 유지하고 save를 하지 않는다. 엔진 작업이 종료/대상 소멸했음을 확인할 때만 quarantine를 해제한다. 재실행 가능한 상태인지와 재실행 허용은 별개다.
- editor 종료/모듈 unload는 best-effort journal checkpoint 후 tick/delegate를 제거하고 inner promise를 오류 완료시킨다. Python도 shutdown 시 outer promise/lease를 정리해야 한다. 프로세스가 갑자기 종료하면 started checkpoint가 남는 것이 정상이며 자동 재실행하지 않는다.

## 4. compiler API와 숨은 동기 작업

| API/소스 | 사용 판단 |
| --- | --- |
| `MaterialEditingLibrary.cpp:984–1059` RecompileMaterial | child instance 갱신, PostEditChange, viewport/UI 알림, BuildTextureStreamingData까지 실행. 비동기 submit으로 사용하지 않는다. |
| `MaterialEditorUtilities.cpp:817–853` BuildTextureStreamingData | CollectGarbage, SlowTask dialog, debug shaders, UV density export, 추가 GC. |
| `DebugViewModeHelpers.cpp:394–417` | PendingMaterials while 루프에서 Sleep(0.1)와 ProcessAsyncResults. RecompileMaterial에 숨은 실제 긴 동기 대기 근거다. |
| `Material.cpp:7289–7293`, `:2767–2777`; `MaterialShared.cpp:3408–3412` | ForceRecompileForRendering(Background) → CacheShaders → BeginCacheShaders 바로 뒤 FinishCacheShaders. Background라는 인자가 DDC 동기 대기를 제거하지 않는다. |
| `MaterialShared.h:2285`, `MaterialShared.cpp:2994,3375` BeginCacheShaders | 실제 public Editor API. current resource에 Background로 제출한다. 처음에 FinishCacheShaders를 호출하므로 기존 caching이 있으면 호출하지 않고 먼저 poll한다. |
| `MaterialShared.cpp:876–889` IsCompilationFinished | DDC future가 아직 ready가 아니면 즉시 false. ready이면 FinishCacheShaders continuation 실행 후 shader jobs 확인. 이 continuation 자체의 CPU 비용은 있다. |
| `MaterialShared.cpp:3714–3779` BeginCompileShaderMap | graph Translate는 동기 CPU 작업. default material 또는 async compilation 비활성 설정에서 Synchronous로 강제될 수 있다. 엔진 설정을 몰래 바꾸지 말고 preflight에서 거부한다. |
| `MaterialShared.cpp:2351` / `ShaderCompiler.h:1158,1267` | default material의 RequiresSynchronousCompilation, AllowAsynchronousShaderCompiling, IsShaderCompilationSkipped를 실제로 검사할 수 있다. |
| `Material.cpp:5350–5418` PostEditChangeProperty | FlushRenderingCommands, 기존 compile 취소, graph 보정, CacheResourceShadersForRendering(None). None도 CacheShaders의 DDC 대기를 없애지 않는다. |
| `MaterialShared.h:3525`; `MaterialShared.cpp:5007,5049` | 기본 FMaterialUpdateContext는 render-state recreate와 SyncWithRenderingThread를 포함, 생성/소멸 때 rendering flush. 컨텍스트를 여러 tick에 걸쳐 보유하지 않는다. |
| `Material.h:1722–1758` | ReleaseResourcesAndMutateDDCKey, CacheResourceShadersForRendering, BeginCacheShadersForResources는 private. ENGINE_API 표시가 있어도 public 접근 권한이 아니다. 접근 우회/엔진 수정 금지. |

따라서 `BeginCacheShaders`도 완전히 일정 시간 안에 끝나는 API라고 주장할 수 없다. graph translation/submit CPU 비용과 render sync는 개별 호출을 중간에 선점할 수 없다. 작업 사이에 yield하는 방식으로 shader/DDC **대기 루프**는 없앨 수 있지만, 단일 graph가 큰 경우의 프레임 지연은 노드 수·resource 수 제한과 실측으로 별도 공개해야 한다. worker thread에서 UObject/Resource API를 호출해 이를 우회하지 않는다.

## 5. native job과 Python journal의 분리

등록/Python orchestration/UE58 compatibility를 계속 분리한다. 다음 이름은 구현 제안이지 현재 API가 아니다.

- native `BeginMaterialCompileJob(canonical_path, operation_id, timeout, ...) -> UToolCallAsyncResultString*`: 입력 확인 후 editor job manager에 등록, 다음 game-thread tick부터 compile 단계를 진행한다. 결과에는 job token과 generation을 포함한다.
- native `SaveCompiledMaterialJobOnce(job_token, generation) -> FString`: 해당 job의 compile 성공, asset identity/generation/lock, deadline/cancel 및 save_attempted=false를 확인하고 **한 번**만 저장한다.
- native `InspectMaterialCompileJob`/`RequestMaterialCompileJobCancel`: status/query와 우리의 명시적 cancel. 이를 노출한다면 각각 이름/스키마를 가진 보조 도구로 등록하며 원본 기능 이전 수에 중복 집계하지 않는다.
- Python async lease: begin/checkpoint/complete/abandon API. 현재 동기 `state.mutate()`는 action 반환 직후 complete 기록과 `_active_operation=None`을 실행하므로 native async object를 기존 mutate로 감싸면 안 된다.

Python outer promise와 native inner promise를 나눈다. 호출 순서는 다음과 같다.

1. Python 입력 검증 → journal ID/fingerprint/exclusive create → 전역 Cubeless mutation lease → canonical package lease → before snapshot → durable before-submit checkpoint.
2. native BeginMaterialCompileJob. inner promise를 job table에 보유하고 `on_completed.add_callable_unique(callback)` 후 `broadcast_on_completed_if_complete()`를 호출한다. 빠른 실패/즉시 완료도 놓치지 않는다.
3. native는 game-thread tick에서 제출·poll·diagnostic 수집만 한다. compile phase가 끝나도 **asset lease는 save/finalize 결정까지 유지**한다. inner completion은 아직 공개 MCP 성공이 아니다.
4. Python callback이 native 실제 변화/compile 상태를 envelope에 먼저 반영한다. durable compile checkpoint. 실패라면 save하지 않고 terminal journal을 작성한다.
5. save 요청·compile 성공·cancel 없음이면 durable `before_save` checkpoint(`save.attempted=false`, 다음 결과는 아직 unknown). 실패하면 SaveCompiledMaterialJobOnce를 호출하지 않는다.
6. native save helper가 메모리 내 save_attempted를 먼저 true로 고정하고 저장 API를 한 번 호출한다. 반환 직후 결과와 dirty/file 상태를 기록한다. 저장 중 프로세스가 죽거나 API 결과가 불명확하면 재시도하지 않는다.
7. Python durable complete 쓰기(`flush + os.fsync + os.replace`)가 성공한 뒤 lease를 해제하고 **outer** `SetValue(JSON)`로 공식 MCP를 완료한다. complete 쓰기 실패는 성공을 반환하지 않고 uncertain journal completion으로 남긴다.

정상 domain 실패(compile error 등)는 기존 envelope의 success=false와 관측 데이터를 보존해 SetValue(JSON)로 전달할 수 있다. 이때 transport 성공과 domain 성공은 구분하며 검수기는 envelope.success를 확인해야 한다. 약속한 JSON을 생성할 수 없는 infrastructure 오류에는 SetError를 쓰고 오류 문자열에 operation_id와 journal 조회 방법을 포함한다. SetError는 result Value를 함께 전송하지 않으므로 부분 변화는 반드시 journal에 남긴다. native/Python 모두 game-thread 단일 terminal 전이와 자체 완료 guard를 두어 두 callback이 중복 완료하지 않게 한다.

현재 `operations/state.py:86–111` checkpoint는 같은 `_active_operation`, operation_id, state=started, PID를 확인한다. 실패는 다음 mutation 전에 중단시킨다. 이 계약을 async lease에도 유지한다. `get_operation`의 started 응답은 마지막 durable snapshot만 설명하며 실제 실패 단계/변화/완료는 unknown이다. 새 job record는 process session UUID도 추가하여 PID 재사용을 구분한다. 동일 ID+동일 입력 complete만 재생 가능하며 started/timeout unknown은 자동 재실행하지 않는다.

2026-10-10 operations 담당과 이 구조를 협의했다. 담당자는 동기 mutate와 async lease 분리, complete durable쓰기 뒤 outer 완료, timeout/cancel 후 save 금지, checkpoint 실패 시 다음 mutation 미실행 계약에 동의했다. 기존 Python Source는 freeze 상태로 유지했다.

## 6. 상태 기계와 resource proof

```text
prepared_journal → queued → wait_existing_resource → submit → poll
  → compile_failed ─────────────────────────────────────────────┐
  → compile_verified → before_save_checkpoint → save_once      │
  → compile_verified (save=false)                              │
  → cancelled/timed_out/asset_lost/generation_changed           │
  → terminal_journal → outer completion ←──────────────────────┘
```

각 단계는 deadline, ownership, valid weak asset, pending kill/destroy, canonical path, PIE/SIE, expected asset generation을 먼저 확인한다. mutation 전 durable checkpoint 실패면 다음 단계로 진행하지 않는다. `FTSTicker::GetCoreTicker().AddTicker`/RemoveTicker는 `Core/Public/Containers/Ticker.h:45,66`의 공개 API를 사용하고 callback마다 IsInGameThread를 확인한다. plugin manager는 pending job마다 interval(예: 0.05초)을 적용하되 Sleep/while 완료대기/FinishCompilation/FinishAllCompilation/강제 ProcessAsyncResults 루프를 쓰지 않는다.

Native job은 `TWeakObjectPtr<UMaterial>`과 canonical object/package path를 보유한다. UObject를 장시간 강하게 붙잡아 삭제/재로드를 가로막지 않는다. 한 tick 안에서만 안전한 짧은 strong pin을 사용할 수 있다. `FMaterialResource*`를 tick 사이에 dereference 가능한 포인터로 저장하지 않는다. 매 tick weak asset를 확인한 후 `GetMaterialResource(EShaderPlatform, QualityLevel)`로 재조회하고 shader platform/quality/material StateId 및 graph generation을 대조한다. pointer 주소만으로 같은 compile이라고 판단하지 않는다.

첫 범위는 `GMaxRHIShaderPlatform`과 capture한 active quality를 검수한다. 이 범위를 결과에 적고 모든 platform/quality/target cook이 검증됐다고 표현하지 않는다. Submit은 BeginCacheShaders(Background)를 한 번만 호출한다. 기존 pending resource는 먼저 IsCompilationFinished로 넘겨 보고, 새 제출과 기존 완료를 구분한다. caching callback의 bool만 shader 최종 완료라고 해석하지 않는다.

성공 조건은 현재 resource가 존재하고 요청 후 generation이 동일하며 IsCompilationFinished=true, GetCompileErrors 비어 있음, HasValidGameThreadShaderMap/IsGameThreadShaderMapComplete 확인, Material의 IsCompilingOrHadCompileError(platform)=false를 만족하는 것이다. ODSC/skip-compile 상태는 no-errors만으로 성공으로 간주하지 않는다. 실제 API 조합/캐시 적중 동작은 smoke에서 검증하고 제출했다고 반드시 새 shader worker 작업이 생겼다고 표시하지 않는다.

resource 준비가 불충분하면 `material_preparation_not_supported_nonblocking`으로 변경 전에 종료한다. private DDC-key API를 우회하거나 임의 StateId를 직접 바꾸지 않는다. 준비된 graph임을 보장할 증거는 단순 dirty 여부가 아니라 이전 Cubeless 편집 단계의 generation/snapshot 및 resource 상태다. 이 제한 때문에 처음에는 legacy 전체 compile_and_save_material을 완성으로 분류하지 않는다.

## 7. 직렬화·사용자 편집·package 범위

초기 구현은 하나의 전역 Cubeless async mutation lease를 유지해 현행 `_active_operation`과 같은 직렬화를 보장한다. 기존 Python mutation과 직접 native mutation entrypoint도 같은 native job gate를 확인하도록 후속 변경이 필요하다. 현재 Material 9개의 함수-local 재진입 guard만으로 tick 사이 동시 접근을 막을 수 없다. package key는 resolver로 확정된 canonical long package name을 사용한다.

이 lock은 사용자 UI나 다른 official tool의 수정까지 막는 시스템 lock이 아니다. `FCoreUObjectDelegates::OnObjectModified`/OnObjectPropertyChanged(`UObjectGlobals.h:3293,3305`)와 관련 package/expression 변경을 감시하고 save 직전 콘텐츠 fingerprint를 다시 비교한다. 자기 제출의 통지 구간만 제한적으로 표시한다. 외부 수정/rename/reload 감지 시 compile proof를 무효화하고 저장하지 않는다. 통지 없는 임의 C++ 메모리 변경을 완벽하게 감지했다고 주장하지 않는다.

compile/update가 loaded child MI나 FunctionInstance를 건드릴 수 있으므로 target만의 dirty 변화로 전체 실제 변화를 표현하면 안 된다. 준비 단계 전후 관련 loaded package의 path/dirty/generation을 기록한다. 특히 사용자 dirty package/다른 mount에 예상 외 수정이 발생하면 부분 실패로 보고하고 자동 save하지 않는다. undo transaction을 비동기 job 전체에 걸쳐 열어 두지 않는다. 실제 동기 편집 단계에서만 짧게 쓰며 rollback 보장을 하지 않는다.

## 8. 저장은 한 번, 저장 증거는 별도

기존 public 저장 경로는 `EditorAssetLibrary.cpp:435` → `EditorAssetSubsystem.cpp:1290–1309` → `FileHelpers.cpp:6019`. SavePackages는 `:5996–6014`에서 FullyLoad를 호출하고 checkout/save를 동기 수행한다. 따라서 save도 파일 I/O·serialization·source control에 따른 동기 구간이다. 이를 worker thread로 옮기거나 SAVE_Async만 붙였다고 안전한 비동기 UObject 저장이라고 가정하지 않는다.

초기 save API는 `UEditorAssetSubsystem::SaveLoadedAsset(target, false)`를 한 번만 호출한다. source-control이 활성화되어 있다면 checkout/add 부작용도 결과·운영 계약에 명시해야 한다. `FileHelpers.cpp:5932–5951`은 save 후 신규 package add/checkout 동작을 포함한다. 이 조사/계획에서 source control 상태는 변경하지 않았다.

compile 뒤 `Target.MarkChanged()/PostEditChange/RecompileMaterial`를 다시 호출하지 않는다. save 전에 resource proof와 generation을 재확인한다. save_requested, save_attempted, API 반환, dirty_before/after, 대상 package filename, 존재/크기/mtime를 구분한다. API true+dirty=false는 saved-call 증거이고 reload 증거는 아니다. 외부 검증 프로세스에서 재로드·graph snapshot·resource 재검수 후 disk_reload를 별도 pass 처리한다. 저장 후 resource 상태가 다시 pending이면 후속 poll로 기록하고 compile을 재제출하지 않는다.

save=false는 compile 완료 뒤 그대로 terminal 처리한다. compile error, timeout, cancellation, lost asset, journal error, generation mismatch에서는 save_attempted=false를 유지한다. save API 호출 도중 실패/timeout은 저장이 전혀 안 됐다고 단정하지 않는다. 정확한 중복 방지 한계는 프로세스 내 at-most-once 시도 + crash 후 durable unknown/no-retry이며, crash까지 포괄하는 exactly-once 보장은 아니다.

## 9. Material Function 별도 증명 범위

`MaterialEditingLibrary.cpp:1368–1449` UpdateMaterialFunction은 Function StateId 갱신, loaded material들의 compile, loaded FunctionInstance parameter set/StateId/dirty 변경과 UI 갱신을 포함한다. `MaterialExpressions.cpp:13498–13549`는 메모리에 있는 모든 UMaterialInterface의 함수 의존성을 순회하고 `ForceRecompileForRendering(None)`을 호출한다. `:13813`은 input/output type 및 dependent function 후보도 갱신한다. 따라서 target Function 하나만 lock하고 즉시 compiled=true 반환하면 원본의 약한 증거를 반복한다.

Function은 독립 shader resource가 없다. 다음 증거를 분리해야 한다.

1. function graph/input/output type 구조 검수와 update 호출 완료.
2. 실행 시 loaded dependent materials/instances 목록 및 각 current platform/quality resource 완료/오류.
3. 별도 fixture wrapper material이 함수 output을 실제로 사용했을 때의 컴파일.
4. asset registry에만 존재하는 unloaded callers, 모든 static permutation, target-platform cook는 미검증 범위.

실제 loaded consumer가 없으면 `function_updated`는 가능해도 shader_validated가 아니다. wrapper 테스트도 특정 입력/출력 연결에 대한 증거다. 첫 nonblocking 구현에서는 function 요청을 preflight `function_async_update_not_implemented`로 거부하고 **이 branch를 명시적 미완료**로 남기는 편이 안전하다. 후속에서는 loaded dependency closure 상한·전체 package lock·snapshot을 확보한 후, 동기 UpdateMaterialFunction의 DDC/전역 순회 비용을 해결해야 한다. 단순 ticker로 UpdateMaterialFunction을 다음 frame에 실행하는 것은 비동기 이식이 아니다.

## 10. 필요한 검증과 완료 기준

| 검증 | 통과 근거 |
| --- | --- |
| 공식 등록·schema·즉시 오류 | Python 반환 타입이 실제 async-result schema로 변환, inner 빠른 완료도 outer 완료 1회. |
| pending compile | isolated 일반 Material에서 여러 editor tick 진행, 다른 read-only 공식 도구 호출 응답 가능, final response가 compile 완료 뒤에만 옴. |
| shader 오류 | 잘못된 fixture expression/custom source로 compiler 오류 재현, compile_errors 보존, save 호출 수 0. |
| warm/cold cache | submit 시간, DDC wait 총시간, poll callback 최대/합산 시간, Translate 비용, compile wall time, save 시간 분리. cache deletion으로 사용자 DDC를 훼손하지 않고 독립 fixture/새 key 사용. |
| shared state | GC, rename/delete/reload, 사용자/다른 도구 graph 변경, PIE 시작 → 안전한 중단/저장 금지, resource pointer 생존 가정 없음. |
| deadline/explicit cancel | outer 실패와 engine continuation을 구분하고 save 0회, quarantine/reconciliation 확인. transport 취소는 별개로 검증. |
| durable 실패 | before-submit fsync 실패→submit 0회; before-save 실패→save 0회; terminal journal 실패→성공 응답 금지. |
| interruption/replay | submit 후, save 전후, final journal 전 각 중단점에서 started unknown 유지; 동일 ID 재실행 0회; complete만 replay. |
| save/reload | save API 1회, final generation 일치, 독립 재로드 graph/resource 검사. |
| Function | 위 4단계 증거를 분리. update 호출만으로 compile pass 금지. |

수치 예산은 먼저 측정한다. node/resource 상한은 비용 제한일 뿐 wall-time deadline의 선점 보장이 아니다. timer 만료도 현재 동기 engine API가 끝난 뒤 관찰되므로 submit/translation/save의 실제 block_ms를 보고한다.

다음 구현 순서는 (1) pure async result 수명/GC/취소 smoke, (2) async lease와 기존 gate 통합 및 fault-injection, (3) 준비된 일반 Material resource submit/poll, (4) save-once/독립 reload, (5) 강제 준비/child MI/streaming/Function parity 해결이다. 단계 3이나 4가 통과해도 단계 5가 남으면 기존 compile_and_save_material 전체 이식 완료라고 선언하지 않는다.
