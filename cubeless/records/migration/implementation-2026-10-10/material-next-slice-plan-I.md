# Material 후속 3개 기능 조사와 Custom K 구현 계약

2026-10-10. 원본은 읽기 전용으로 조사했다. 이 문서는 API 소스 및 원본 runtime 등록 스키마에 근거한 설계이며, 새 Custom의 실제 UE 호출 검증은 아직 수행하지 않았다. 이미 통과한 I 실행 증거와 새 K 소스를 구분한다.

## 우선순위

1. `add_custom_material_node`: 생성 코어를 공유하지만 별도 공개 도구와 특화 검증을 유지한다. 원본에서 명시적 compile/save를 하지 않아 async compile 기반과 독립적으로 구현할 수 있다. Root 승인으로 K 구현을 진행한다.
2. `set_material_parameter_collection_values`: 기존 MPC 읽기 및 inheritance 검수와 이어지는 후속 기능. UE58 inherited parameter의 asset override 처리 및 외부 모듈 API export 문제를 먼저 해결해야 한다. 아직 구현하지 않는다.
3. `replace_material_texture_references`: 공개 표현식·텍스처 API는 존재하나 원본의 compile/save 의미를 보존하려면 async compile job이 필요하다. 편집만 구현하고 원본 전체를 완료로 세지 않는다.

검색 범위는 원본 `scripts/**/*.py`, `codex/scripts/**/*.py` 581개 파일이다. 세 이름은 합계 35개 줄, 22개 파일에서 발견했다. 이는 텍스트 사용처 수이며 실행 검증이나 모든 외부 파이프라인의 전수 성공을 뜻하지 않는다. 예: Custom은 fluid_fire/reactive_fog/reactive_surface water·snow·grass builder, MPC setter는 weather 및 sky full-system verifier, texture replacement는 `normalize_hero_cumulus01_parent_ue.py` 등에 사용된다. 실제 도구 호출 순서의 파이프라인 이식은 각 외부 검수 문서에서 별도 추적한다.

## 원본과 API 연결

원본 Python은 `Python/tools/material_tools.py`, C++는 `MCPGameProject/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/UnrealMCPMaterialCommands.cpp`이다. 원본 SDK runtime 등록은 `legacy-runtime-registration.eager.json`의 `catalog_tools`를 사용했다. 이 캡처는 SDK decorator 등록 결과이며 엔진 tool body 실행 증거가 아니다. 동반 JSON에 선택한 3개의 정확한 입력 스키마를 보존한다.

| 도구 | Python → route → handler | 설치 UE58 공개 API |
| --- | --- | --- |
| add_custom_material_node | Python:346 → C++:3751 → :5196 | MaterialEditingLibrary.h:92 `CreateMaterialExpressionEx`; MaterialExpressionCustom.h의 `Code`, `OutputType`, `Inputs`, `Description`, `AdditionalDefines`, `IncludeFilePaths`; `RebuildOutputs` |
| set_material_parameter_collection_values | Python:668 → C++:3791 → :6200 | MaterialParameterCollection.h:91/94 editable own parameter arrays; exported effective getters/base accessor :118–175; MaterialParameterCollectionInstance.h:39–50 exported setters/getters; World public collection instance lookup |
| replace_material_texture_references | Python:273 → C++:3743 → :4848 | MaterialExpressionTextureBase.h:33 `Texture`, :73 `AutoSetSampleType`; existing Material resolver/expression iteration; compile completion is a separate requirement |

All engine source references are relative to `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`. Public declarations are source evidence, not build/link/runtime proof.

## Custom 원본 계약과 K 경계

Original signature: `material_path, code, output_type='CMOT_Float3', inputs=None, description='', graph_type='auto', node_position=None, additional_defines=None, include_file_paths=None`. Python normalizes optional arrays to `[]` and position to `[0,0]`. Native rejects empty code, validates all custom inputs before `CreateMaterialExpressionEx`, then assigns Code/OutputType/Description/Desc/Inputs/AdditionalDefines/IncludeFilePaths. It returns expression JSON plus `material`, `graph_type`, `created_custom_node`, `input_count`, `requested_output_type`. No explicit compile/save or rollback is performed.

Native parsers (:516–770): ASCII nonreserved HLSL identifiers, input strings or objects with case-insensitive name/input_name keys, case-insensitive duplicate input names rejected; defines accept name/define_name and string/number/bool value/define_value with default `1`, duplicate names rejected; include strings trimmed, empty/newline rejected, case-insensitive duplicates removed. Output aliases strip `_`, `-`, spaces and optional `CMOT` prefix; accept float/float1/1 through float4/4 and materialattributes/attributes. Do not silently change this specialized contract into arbitrary property assignment.

K public Python signature:

```python
add_custom_material_node(material_path: str, code: str,
    output_type: str = "CMOT_Float3", inputs_json: str = "[]",
    description: str | None = None, graph_type: str = "auto",
    node_position_json: str = "[0,0]", additional_defines_json: str = "[]",
    include_file_paths_json: str = "[]", operation_id: str | None = None)
```

The four `_json` inputs explicitly adapt legacy arrays to JSON strings. Their omission meanings are preserved; callers must change field names and encode their arrays. JSON literal `null` or direct reflected array values are not accepted by this adapter. `description` omitted/null normalizes to the old empty string. Nonempty string defaults avoid the installed engine's empty-string/default-array schema defects without widening the two existing exact-owner array converters.

Python validates JSON syntax, duplicate keys, finite numbers, depth 16, 8192 values and 65536 UTF-16 code units, array shapes and exactly two int32 coordinates. Code must be nonblank and <=65536 UTF-16 units; description <=4096; inputs/defines <=64; includes <=32. Native repeats its own necessary safety boundaries and owns HLSL alias/name/define semantic validation. C++ helper is hidden `BlueprintCallable+AIIgnore`:

```text
CubelessMaterialLibrary.add_custom_material_node(
 material_path,code,output_type,inputs_json,description,graph_type,
 node_x,node_y,additional_defines_json,include_file_paths_json,max_graph_nodes)
```

It calls the existing AddNode core once after normalization, retaining direct native engine class/reference guards and 2048-expression pre-mutation limit. Python uses the existing `_mutation` before snapshot, durable checkpoint, single dispatch and native readback. No duplicate compile/save call is added. A replayed completed operation ID returns historical evidence; a failed/uncertain interrupted result is not automatically retried. Post-create cleanup never implies atomic package rollback.

This slice explicitly rejects nonempty `include_file_paths_json` and all preprocessor markers `#`, `%:`, `??=` in Code/define values before creation (`custom_shader_includes_unsupported`). The check conservatively also rejects these markers inside strings/comments; inline `#define` and conditional-preprocessor code are therefore unsupported, while ordinary AdditionalDefines name/value pairs remain supported. This replaces an insufficient whitespace-only `#include` check that missed comment-separated directives such as `#/**/include`. Combined native-encoded Custom properties must fit 65536 UTF-16 units including JSON/import-text escaping overhead, even if each individual field fits its own limit. A textual custom include may reference project/plugin shader files without appearing in Asset Registry dependencies. No transitive shader include resolver/cooked dependency audit has been proved here. Therefore this named tool is a **partial legacy implementation**, not full parity, until that boundary is handled. Function graphs are part of the API contract, but their runtime validation must be tracked independently of a Material-only fixture.

`MaterialExpressions.cpp:12842` calls `RebuildOutputs` and reconstructs graph pins for Inputs/AdditionalOutputs member property changes. Native postconditions must inspect actual inputs and outputs after applying fields, not merely count nodes. `PostEditChange` may start implicit engine shader work; creation success is not HLSL compile success. Explicit compile results, save, cold reload, packaged runtime and renderer quality remain separate proof levels.

Independent K review also found generic Add/Set could otherwise bypass the specialized include guard. The native shared typed-property preparation now checks Custom Code, IncludeFilePaths, AdditionalDefines and shader-inserted input/output names after temporary conversion and before persistent assignment. This also rejects raw UE array import strings carrying new shader directives. It inspects the field currently being assigned; unrelated edits such as Desc do not rewrite or reject pre-existing shader code. This is an explicit supported-scope restriction on the two generic mutation tools as well, not a claim that all shader dependency paths or arbitrary existing assets were audited.

Ownership: native agent owns Source; Python agent owns Material registration/operations/compat and wrapper tests; operations agent owns new `custom_material_smoke.py`, `custom_material_report.py`, `test_custom_material_pipeline.py`; root alone builds/calls UE. Existing Material v1/v2 pipelines and raw evidence remain unchanged. New tests must cover default omission, aliases, inputs/defines, invalid identifiers/output/duplicates/include failure, no-change readback, replay, independent property readback and save/cold reload. Compile API empty errors only means return without reported errors until shader completion is independently observed.

## MPC setter: exact migration issues

Legacy defaults are `scalars=None→{}`, `vectors=None→{}`, `set_asset_defaults=true`, `set_runtime=true`, `save=true`. The handler processes entries individually; invalid/missing parameters accumulate errors and valid entries may still change. It modifies own arrays only, updates the editor or PIE world instance, calls PostEditChange once if asset defaults changed, and saves only if defaults changed. It reports asset/runtime counts, updated entries, missing names, errors, world availability, dirty and save states. A top-level transport-success object can contain partial entry failure; new domain success must reflect that.

UE58 inheritance changes matter: `Runtime/Engine/Private/Materials/ParameterCollection.cpp:149–221` implements setters that update a local default or create an override on the child, preserving the ancestor. However these methods are not `ENGINE_API` and are not UFUNCTION in the `MinimalAPI` class (`Public/Materials/MaterialParameterCollection.h:98–107`). No call site outside Engine was found in installed Plugins/Editor sources. Thus a declaration alone does **not** establish that our external editor module can link or call the inherited setter. A link probe/public alternative is required; private map access or Engine source modification is not an accepted shortcut.

Own editable arrays can support non-inherited defaults, but inherited default writes must stay explicitly unsupported until a public supported route is verified. Runtime instance setters are exported and query effective parameters; copying the old own-array loop would mislabel inherited runtime success as missing. World identity and inherited ownership need explicit preflight/readback; never mutate an ancestor implicitly. PIE/SIE mutation policy needs a named approved scope since current shared mutation gate rejects PIE. `PreEditChange` establishes PreviousTotalVectorStorage before `PostEditChangeProperty` (:231 onward); skipping it can induce unintended layout/recompile behavior. Compile effects on loaded referencing assets and save scope must be reported. Missing world may permit asset-only work according to flags, but must not claim a runtime update succeeded.

## Texture replacement: compile prerequisite

Legacy accepts source-object/package/export-text paths mapped to replacement texture paths, plus graph_type=auto and save=true. Invalid pairs are accumulated while valid pairs proceed. It changes every matching MaterialExpressionTextureBase reference, auto-selects sampler type, reports replaced nodes/old-new paths/unmatched textures, then compiles even if no node changed. Save happens only when replacements occurred and compilation was verified. The old code clears a newly dirty package on a no-change path; this is not safe to repeat without proving no engine side effects.

The node operation is directly portable with public APIs and canonical path normalization, but compile/save must use the shared future compile subsystem described in `async-material-compile-plan.md`. Original blocking `FinishCompilation` is not moved into a nominal async wrapper. Function's legacy unconditional compiled=true after UpdateMaterialFunction is not shader proof. Input/output compatibility wrappers remain separate from the shared replace core and compile core; standalone named-tool support must not be inferred from generic Add/Set or a generic batch tool.

## Deferred cache refresh

`refresh_material_cached_expression_data` is valuable to material decomposition but was not chosen as a small next slice. The legacy handler const_casts cached FunctionInfos and has hardcoded `/Game/UltraDynamicSky` and `/Game/_MCP_Temp` dependency prefixes. UE58 `Material.cpp:2262` rebuilds via `AnalyzeMaterial` but returns early when bLoadedCachedExpressionData is set. This does not prove the old stale-cache repair is unnecessary or equivalent. Do not port the private-cache hack or project-specific prefixes as a generic supported behavior; investigate reproducible stale-cache fixtures and public invalidation separately.
