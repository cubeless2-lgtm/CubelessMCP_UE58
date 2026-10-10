# Generic Material FName/import boundary: source-confirmed incomplete work

This is a read-only investigation while K source is frozen. No unsafe reproducer was executed in Unreal and no existing asset was changed. K's specialized Custom input-name bound is independent of the generic boundary below.

Follow-up implementation is recorded in `generic-material-fname-guards-L.md`. This K finding remains historical evidence of the earlier unsafe path; do not treat L source edits as runtime verification until the guarded binary has been built and tested.

Installed engine facts:

- `Core/Public/UObject/NameTypes.h:57`: `NAME_SIZE=1024`.
- `Core/Private/UObject/UnrealNames.cpp:3271-3285`: name addition with length at least NAME_SIZE reaches `checkf(false, ...)`; a name lookup (`FNAME_Find`) takes a separate nonfatal not-found path. Builds without that check use the `ERROR_NAME_SIZE_EXCEEDED` fallback; that is also not a valid faithful property result.
- `CoreUObject/Private/UObject/PropertyName.cpp:55-83`: reflected FName import constructs `FName(Buffer)` or `FName(Token)` without a local length rejection, before writing the destination.

Current generic plugin paths:

1. `CubelessMaterialMutation.inc::PrepareMaterialProperty`, FNameProperty branch, constructs `FName(*Value->AsString())` directly. The 65536-unit JSON input bound permits a 1024-character identifier. Both generic Add CDO preflight and Set conversion can reach this before persistent assignment; a journal or scratch allocation cannot catch an engine fatal check.
2. The same function's generic `Property->ImportText_Direct` path can import an array/struct containing FName fields, for example raw UE array text for Custom Inputs. A later converted-value shader/reference guard runs too late to guard name construction inside the engine importer. The specialized Custom builder instead validates each input identifier length before constructing typed inputs, then exports their valid UE text.
3. `CubelessMaterialLegacy.inc::FindPropertyByNameLoose` passes `*PropertyName` to `UStruct::FindPropertyByName(FName)`, implicitly constructing FName with its default add behavior. A very long property-name string is therefore another concrete boundary requiring a guard before this helper is called.
4. Selector names, object load paths and dependency-package name construction deserve a separate complete entry audit. This record does not assert every such path crashes: the engine distinguishes FNAME_Find from FNAME_Add and some object loaders validate names first.

Do not execute a known overlength FName through the unsafe generic path merely to prove the check. A repair should reject direct FName strings before construction and use bounded typed JSON conversion for supported nested fields, or explicitly reject unaudited raw imports containing nested FName values before calling ImportText. A global text-length cap would incorrectly reject legitimate long Code/description strings and is not a complete semantic repair. Any restricted nested/property branch must be tracked as incomplete legacy parity, not silently dropped.

After repair, safe negative tests should call the repaired guards with 1024-character names and verify a structured preflight error plus unchanged graph/property state. Generic Add/Set full input/error equivalence remains incomplete until this boundary and its nested import cases are implemented and actually verified. Successful registration or simple scalar mutation does not close it.
