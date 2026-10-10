# N native component read: independent source review

The reviewed candidate requires one correction before runtime verification. This is a source review of the six new N C++ files and their design document, compared with the UE56 implementations and installed UE58 source. No engine/build command ran and no implementation file was edited. The accompanying JSON records exact file hashes; later corrections require separate evidence.

## Required correction

**N-ENUM-NULL (P2):** `CubelessComponentApi.cpp` reads `FEnumProperty::GetEnum()` and its underlying property without validating them. Installed `EnumProperty.cpp:411–425` explicitly handles a missing enum after its class is moved or deleted. The new read can therefore dereference null instead of returning a structured failure. `GetCPPType():387–388` also checks this metadata. Validate scalar enum metadata before reading, and validate enum array metadata even when the array is empty: `PropertyArray.cpp:987–996` calls the inner `GetCPPType()` after element inspection would have been skipped. The issue was sent to the native owner. It is source-confirmed, not runtime-reproduced.

## Scope observations

- Template validity and finite transform checks apply only to selected rows because the component filter precedes them. Ownership, repeated-node and name checks cover the full traversed tree. Clarify this distinction in the design claim, or move required whole-tree invariants before the filter.
- `max_value_chars` limits returned strings but currently checks scalar text after JSON string construction. It does not strictly cap temporary allocation. An FString precheck would avoid an unnecessary large copy. FText conversion and object-reference resolution can also do implicit engine work.

## Confirmed design properties

Root/child traversal avoids two real mutation paths: `GetAllNodes()` may populate the legacy cache, and inherited SubobjectData gathering may create override templates. Selector length checks and textual property lookup avoid making unbounded request-derived FNames. Generic text export is restricted to inspected scalar arrays; unsupported exporters fail explicitly.

The observation envelope accurately limits its claim to Blueprint dirty/status after loading. The scope-exit capture also runs on early failures; a failed load has no known baseline. The design discloses PostLoad/reference resolution and does not equate a stable dirty flag with complete object-state or disk verification. Hidden reflected adapters remain in the existing editor module and introduce no gameplay C++.

Local-only components, sorted order, absent inherited/native CDO/orphan coverage, unsupported property types and incomplete legacy equivalence are documented. Build, official registration/calls, independent value checks and cold reload remain pending. A source review does not close those gates.
