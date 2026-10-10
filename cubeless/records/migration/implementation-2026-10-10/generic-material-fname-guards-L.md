# FName conversion guards, native slice L

Status: implemented, built and guarded-engine behavior verified for the listed cases. Root reported `material-fname-guards-L2.json`: 54 stages passed on the L binary, PID 32868. Add/Set overlength property/FName/nested text and legitimate long aggregate rejected at preflight with independent unchanged observations; MPC filter and dependency-audit package cases passed, with dirty packages remaining empty. The initial L report's checker expected the phrase `raw import` while the native error said `raw property import`; its actual rejection and unchanged state were retained, and the corrected verifier produced L2. No unsafe counterexample was executed on K. This narrows the source-confirmed issues in `generic-material-fname-boundary-K.md`; it is not a proof that every loader, selector or arbitrary native importer has been audited.

## Minimum fail-before-conversion changes

1. `CubelessMaterialLegacy.inc::FindPropertyByNameLoose` returns null when the authored property-name string has length at least `NAME_SIZE`. The check precedes `FindPropertyByName(FName)`, including calls made during capture/readback before mutation validation.
2. `CubelessMaterialMutation.inc::PrepareMaterialProperty` rejects the same long property name with a clear message before lookup. Its direct FNameProperty branch rejects a string of length at least `NAME_SIZE` before constructing FName.
3. Before generic `ImportText_Direct`, raw text at least `NAME_SIZE` is rejected when its property can import names, is a struct, or uses an unaudited importer. Arrays/sets/maps recursively inherit this requirement from elements/keys/values; recursion deeper than 16 is conservatively bounded. Only plain FString/bool/numeric container leaves are exempt. Enum, FName, struct, object and other importer types remain bounded.
4. Independent review identified another FNAME_Add route in MPC `parameter_names`. `GetOptionalParameterNameFilter` now validates every item and trimmed name length before constructing any FName, and returns an explicit error instead of silently dropping invalid names. Both its active effective-values caller and retained legacy caller propagate failure. An invalid filter must never become an empty filter (which means all parameters).
5. Native `AuditPackageDependencies` rejects any package string of length at least NAME_SIZE before the compatibility layer constructs FName. Installed `FPackageName::IsValidTextForLongPackageName` and `IsValidLongPackageName` only check minimum length, syntax and mount validity, not maximum length; those checks alone were insufficient. This read-only tool retains its existing error envelope and reports the explicit length rejection before dependency enumeration.

All lengths use Unreal FString UTF-16 code-unit counts on this Windows build. The installed `NAME_SIZE` is 1024, so accepted guarded strings must have at most 1023 units. There is no public boolean, JSON field, class-name alias or local-cache escape hatch. The specialized Custom builder's typed-export strings are subject to the same aggregate cap. A future relaxation requires passing validated typed values directly or an internal proof object, not trusting a request-supplied marker.

## Why structs are included even without an FName member

Installed `PropertyStruct.cpp:287` delegates to `UScriptStruct::ImportText`; `Class.cpp:3437` may use a native override and otherwise imports authored members. `Property.cpp:1599-1634` converts each authored member key to FName before resolving its value property. Therefore a long unknown member key can reach FName creation even in a struct composed only of numeric/string values. Bounding the entire raw struct text also bounds those ordinary parsed keys; inspecting only whether the schema has an FName value would miss this path.

Direct scalar enum conversion was examined separately: `Enum.cpp:1001-1003` uses `FNAME_Find` for both plain and qualified search names. That path is distinct from the fatal FNAME_Add condition and does not need a false generic length-failure claim here.

The source-confirmed fatal condition remains `UnrealNames.cpp:3271-3285`, with `NAME_SIZE=1024` in `NameTypes.h:57`. Standard reflected FName import in `PropertyName.cpp:55-83` performs name construction before destination assignment. Scratch storage and later shader/reference checks cannot protect that construction; the new rejection precedes it.

## Error and preservation contract

For generic Add/Set, the existing envelope is retained: `success=false`, `stage=preflight`, `error_code=material_mutation_target_rejected`, `mutation_attempted=false`, `changed=false`, before/after capture of the existing state and unchanged dirty flag. Messages identify one of:

- Property name must be shorter than NAME_SIZE, rejected before lookup.
- FName value must be shorter than NAME_SIZE, rejected before construction.
- Name-bearing/struct/unaudited raw import must be shorter than NAME_SIZE; long aggregate import is unsupported, rejected before ImportText.

The low-level lookup helper returns null rather than attempting an unsafe lookup. The public operation subsequently performs explicit validation and returns the structured error above. No node is created or persistent property assigned for these rejected requests; no compile/save is requested. Loading and reading the target for preflight are still possible, as in the existing operation.

The read-only MPC failure uses outer `material_preflight_failed` with inner `data.error_code=collection_parameter_filter_invalid` and the explicit FName limit message. It performs no mutation. Asset loading may already occur during the containing CaptureState; the filter reader validates before its own asset/runtime-value lookup. Readback and successful-query behavior remain unchanged for valid inputs.

## Deliberate incomplete functionality

A valid array containing many individually short names can have encoded UE text longer than 1023 and now be rejected. Long struct imports and specialized Custom input/define aggregate exports can also hit the cap. These are explicit unsupported branches, not equivalent legacy behavior or completed migration. Long ordinary Code/Description FString values retain their existing limits because they are directly assigned and do not go through raw ImportText.

The bound closes the documented direct-name/property-name and ordinary name-bearing raw-text token hazards. It does not establish the behavior of arbitrary engine native importer overrides that synthesize new strings, all object load paths, all selectors, or total name-pool resource usage. Those remain separate audit scope.

## Actual tests required after L is built

Use isolated fixtures and the new L binary only. Reject a 1024-character direct ParameterName/FName value, a 1024-character property name, and an overlength raw Custom Inputs/struct import; verify official graph/property readback and dirty state are unchanged. Exercise both generic Add and Set. Include a legitimate long aggregate of short names to verify its explicit unsupported outcome and a normal short array/scalar/vector regression. Confirm a long ordinary FString Code/Description is not rejected merely by the FName bound. Do not send a known fatal case to the previous K process.

Also reject a 1024-character MPC filter name, preserve ordinary short/empty filters, and verify the rejected request does not return the entire collection as a successful result.
Reject a native dependency-audit package argument with total length 1024 and valid `/Game/` syntax; verify the editor remains available and valid short package audits still work. This test must also wait for the L binary.

Source edits are confined to three plugin compatibility `.inc` files (Legacy, Mutation and CollectionRead) and `Operations/CubelessInspectionOperations.cpp`. Engine, original UE56 repository, Python schemas, module dependencies and game content are unchanged. The initial enum review concern was withdrawn after confirming both enum string lookup constructors use safe FNAME_Find, not FNAME_Add.
