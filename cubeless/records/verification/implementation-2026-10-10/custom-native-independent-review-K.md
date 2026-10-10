# Custom K independent source review

2026-10-10, Python integration reviewer. Read-only C++ review; no editor/tool invocation. Native implementation and builds belong to the native agent/root. This does not claim UE registration, successful creation, shader compilation, save/reload or packaged runtime verification.

Reviewed paths under `Plugins/CubelessMCP_UE58/Source/CubelessMCPUE58Editor/Private/`:

- Registration/CubelessMaterialLibrary.{h,cpp}: positional helper matches the frozen Python adapter, BlueprintCallable+AIIgnore internal exposure.
- Operations/CubelessMaterialOperations.cpp: named AddCustomNode preflight executes before CaptureState and transaction; existing mutation gate/journal ownership, actual before/after and failure-stage handling retained.
- Compatibility/UE58/CubelessMaterialCustomLegacy.inc and CubelessMaterialCustom.inc: legacy identifier/alias/define parser closure; validated typed FCustomInput/FCustomDefine arrays exported via actual FArrayProperty to the existing AddNode property converter. No hand-built engine import text from unescaped user strings.
- CubelessMaterialMutation.inc / CubelessMaterialApi.cpp: shared AddNode/PrepareMaterialProperty and independent property/node/position readback, Custom output rebuild and input topology verification.
- CubelessMaterialCustomGuards.inc: shared typed shader-text admission boundary.

## Findings and fixes

1. **Include directive bypass:** initial whitespace-only `#include` detection missed `#/**/include` and similar comment-separated preprocessing tokens. Native replaced it with conservative rejection of every `#`, `%:` and `??=` marker after line-splice normalization, including comments/strings. Therefore Code-inline preprocessor features are explicitly unsupported; ordinary AdditionalDefines name/value pairs still work. This avoids claiming to implement a full HLSL preprocessor.
2. **Generic Add/Set bypass:** a specialized builder-only guard could be avoided by generic `add_material_node(Custom, properties_json=...)` or `set_material_node_property` on Code/IncludeFilePaths/AdditionalDefines. The shared PrepareMaterialProperty now validates the typed converted candidate before CopyCompleteValue. It also checks shader-inserted Inputs/AdditionalOutputs names. The current field is checked; existing unsafe code is not rewritten or rejected by an unrelated Desc edit. Source placement and scope confirmed.
3. **Combined capacity:** each public field fitting its own limit does not mean combined native encoded properties fit the shared 65536 UTF-16-unit cap. Native checks encoded size before node creation; public docstrings now disclose encoding overhead and the combined cap.

These are source-level fixes confirmed by rereading the implementation. Actual negative calls must test comment-split markers, generic Add/Set attempts and no-change postconditions after root's K build; no runtime pass is inferred from this review.

## Positive contract checks

- Specialized creation performs one shared Add core dispatch, rather than public add followed by multiple separately journaled property writes.
- Defaults preserve Float3 explicitly even though engine's new Custom class defaults to Float1.
- Existing class/runtime-reference guards, graph cap, durable Python checkpoint, native game-thread/PIE/reentrancy checks and partial-cleanup records remain in the call path.
- After all assigned fields pass conversion/readback, Custom calls RebuildOutputs once and reconstructs an existing GraphNode once; it does not call Custom PostEditChange's Super solely to refresh pins. Generic Add's deferred refresh behavior is retained.
- Postcondition requires exactly one new node of the expected class and requested position; all requested reflected property values must match; Custom inputs must have matching counts, names and actual addresses; output count must be one.
- Result preserves created_custom_node, input_count, requested_output_type and explicitly marks custom_shader_code_compiled=false. Node creation does not establish HLSL compile success.
- Generic property guard limits do not prove complete shader-source dependency closure. Existing unrelated nodes, material functions, editor shader settings and arbitrary original shader text are outside this small review.

Python K executable behavior passed 27 wrapper/durability/partial-result tests and 46 existing plugin tests before documentation-only updates. The K3 Python manifest records the final docstring hashes; no executable Python logic changed after those tests. Offline tests do not replace native runtime validation. No remaining blocking source issue was found in the bounded Custom builder paths after the fixes above; actual UE validation remains required.
