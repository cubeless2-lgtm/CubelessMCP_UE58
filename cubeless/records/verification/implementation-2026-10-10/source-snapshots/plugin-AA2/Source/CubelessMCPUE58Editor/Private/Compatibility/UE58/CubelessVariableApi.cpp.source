// UE56 BlueprintNodeCommands Part1 variable contracts, adapted to public UE58 APIs.
// Transport, Python journals and public tool registration remain separate.
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompilerMisc.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Variable
{
namespace
{
constexpr int32 MaxVariables = 4096;
bool Fail(FResult& R, const TCHAR* Code, const FString& Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
bool SafeText(const FString& S, int32 Limit, bool bEmpty = true)
{
    if ((!bEmpty && S.IsEmpty()) || S.Len() > Limit) { return false; }
    for (int32 I = 0; I < S.Len(); ++I)
    {
        const uint32 C = S[I];
        if (C == 0 || (C >= 0xD800 && C <= 0xDBFF && (I + 1 == S.Len() || S[I + 1] < 0xDC00 || S[I + 1] > 0xDFFF))) { return false; }
        if (C >= 0xD800 && C <= 0xDBFF) { ++I; }
        else if (C >= 0xDC00 && C <= 0xDFFF) { return false; }
    }
    return true;
}
bool SafeName(const FString& S, bool bEmpty = false)
{
    if (!SafeText(S, NAME_SIZE - 1, bEmpty)) { return false; }
    for (TCHAR C : S) { if (C < 32 || C == 127) { return false; } }
    return true;
}
bool Identifier(const FString& S)
{
    if (S.IsEmpty() || S.Len() > 100 || S.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { return false; }
    for (int32 I = 0; I < S.Len(); ++I)
    { const TCHAR C = S[I]; if (!(C == '_' || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (I > 0 && C >= '0' && C <= '9'))) { return false; } }
    return true;
}
bool Bounded(const TSharedPtr<FJsonValue>& V, int32 Depth, int32& Count)
{
    if (!V || Depth > 8 || ++Count > 1024) { return false; }
    if (V->Type == EJson::Number) { return FMath::IsFinite(V->AsNumber()) && FMath::Abs(V->AsNumber()) <= 9007199254740991.0; }
    if (V->Type == EJson::String) { return SafeText(V->AsString(), 65536); }
    if (V->Type == EJson::Array) { for (const auto& X : V->AsArray()) { if (!Bounded(X, Depth + 1, Count)) { return false; } } }
    if (V->Type == EJson::Object) { for (const auto& X : V->AsObject()->Values) { if (!SafeName(FString(X.Key.ToView())) || !Bounded(X.Value, Depth + 1, Count)) { return false; } } }
    return true;
}
bool Parse(const FString& Text, TSharedPtr<FJsonValue>& Out, FResult& R)
{
    if (Text.IsEmpty() || Text.Len() > 65536) { return Fail(R, TEXT("variable_json_invalid"), TEXT("JSON must contain 1..65536 UTF-16 characters.")); }
    const FString Wrapped = TEXT("[") + Text + TEXT("]");
    auto Reader = TJsonReaderFactory<>::Create(Wrapped);
    TArray<TSet<FString>> Keys; TArray<bool> Objects; EJsonNotation Token;
    while (Reader->ReadNext(Token))
    {
        if (Token == EJsonNotation::Error) { return Fail(R, TEXT("variable_json_invalid"), TEXT("Malformed JSON.")); }
        if (Token != EJsonNotation::ObjectEnd && Token != EJsonNotation::ArrayEnd && !Objects.IsEmpty() && Objects.Last())
        { const FString& Key = Reader->GetIdentifier(); if (Keys.Last().Contains(Key)) { return Fail(R, TEXT("variable_json_invalid"), TEXT("Duplicate JSON object key.")); } Keys.Last().Add(Key); }
        if (Token == EJsonNotation::ObjectStart || Token == EJsonNotation::ArrayStart)
        { Objects.Add(Token == EJsonNotation::ObjectStart); Keys.AddDefaulted(); if (Objects.Num() > 10) { return Fail(R, TEXT("variable_json_invalid"), TEXT("JSON nesting limit exceeded.")); } }
        else if (Token == EJsonNotation::ObjectEnd || Token == EJsonNotation::ArrayEnd) { Objects.Pop(); Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values; int32 Count = 0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Wrapped), Values) || Values.Num() != 1 || !Bounded(Values[0], 0, Count))
    { return Fail(R, TEXT("variable_json_invalid"), TEXT("Provide one bounded JSON value; depth <=8, values <=1024, finite exact-range JSON numbers.")); }
    Out = Values[0]; return true;
}
TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> J; for (const FString& V : Values) { J.Add(MakeShared<FJsonValueString>(V)); } return J; }

bool RuntimeReference(UObject* Object, FResult& R)
{
    if (!IsValid(Object) || Object->IsEditorOnly() || Object->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Fail(R, TEXT("variable_reference_unsupported"), TEXT("Reference is missing or belongs to an editor/developer/uncooked object.")); }
    const FString Package = Object->GetOutermost()->GetName();
    const TSet<FString> Scripts = {TEXT("/Script/CoreUObject"), TEXT("/Script/Engine"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
    if (Package.StartsWith(TEXT("/Script/")))
    { return Scripts.Contains(Package) || Fail(R, TEXT("variable_reference_unsupported"), TEXT("Native reference module is outside the audited runtime modules: ") + Package); }
    if ((!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/"))) || Object->HasAnyFlags(RF_Transient))
    { return Fail(R, TEXT("variable_reference_unsupported"), TEXT("Asset references require persistent /Game or /Engine assets.")); }
    auto Audit = ReadPackageDependencies({Package}, true, 4096);
    R.Data->SetObjectField(TEXT("last_reference_dependency_audit"), Audit);
    bool Passed = false;
    if (!Audit->TryGetBoolField(TEXT("passed"), Passed) || !Passed)
    { return Fail(R, TEXT("variable_reference_unverified"), TEXT("Referenced asset must have a complete clean saved dependency graph.")); }
    for (const auto& Edge : Audit->GetArrayField(TEXT("edges")))
    {
        const FString To = Edge->AsObject()->GetStringField(TEXT("to"));
        if (!To.StartsWith(TEXT("/Game/")) && !To.StartsWith(TEXT("/Engine/")) && !Scripts.Contains(To))
        { return Fail(R, TEXT("variable_reference_unsupported"), TEXT("Reference dependency uses an unaudited module or plugin: ") + To); }
    }
    return true;
}
bool PinType(const FString& Type, const FString& ObjectPath, bool bArray, FEdGraphPinType& Out, FResult& R)
{
    FString T = Type.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
    if (T == TEXT("bool") || T == TEXT("boolean")) { Out.PinCategory = UEdGraphSchema_K2::PC_Boolean; }
    else if (T == TEXT("int") || T == TEXT("integer")) { Out.PinCategory = UEdGraphSchema_K2::PC_Int; }
    else if (T == TEXT("int64") || T == TEXT("integer64")) { Out.PinCategory = UEdGraphSchema_K2::PC_Int64; }
    else if (T == TEXT("float") || T == TEXT("single") || T == TEXT("double") || T == TEXT("real"))
    { Out.PinCategory = UEdGraphSchema_K2::PC_Real; Out.PinSubCategory = (T == TEXT("float") || T == TEXT("single")) ? UEdGraphSchema_K2::PC_Float : UEdGraphSchema_K2::PC_Double; }
    else if (T == TEXT("string")) { Out.PinCategory = UEdGraphSchema_K2::PC_String; }
    else if (T == TEXT("name")) { Out.PinCategory = UEdGraphSchema_K2::PC_Name; }
    else if (T == TEXT("text")) { Out.PinCategory = UEdGraphSchema_K2::PC_Text; }
    else if (T == TEXT("vector") || T == TEXT("rotator") || T == TEXT("transform"))
    { Out.PinCategory = UEdGraphSchema_K2::PC_Struct; Out.PinSubCategoryObject = T == TEXT("vector") ? TBaseStructure<FVector>::Get() : T == TEXT("rotator") ? TBaseStructure<FRotator>::Get() : TBaseStructure<FTransform>::Get(); }
    else if (T == TEXT("object") || T == TEXT("class") || T == TEXT("struct") || T == TEXT("scriptstruct") || T == TEXT("enum") || T == TEXT("byteenum"))
    {
        if (!SafeName(ObjectPath, T == TEXT("object") || T == TEXT("class"))) { return Fail(R, TEXT("variable_type_invalid"), TEXT("This variable type requires an exact bounded type_object path.")); }
        if (T == TEXT("object"))
        {
            UClass* Class = RuntimeObject::FindSupportedClass(ObjectPath.IsEmpty() ? UObject::StaticClass()->GetPathName() : ObjectPath);
            if (!Class) { return Fail(R, TEXT("variable_object_type_pending"), TEXT("Object declarations currently admit exact runtime UObject/Actor/Controller/PlayerController/ActorComponent native classes; other object types remain pending.")); }
            Out.PinCategory = UEdGraphSchema_K2::PC_Object; Out.PinSubCategoryObject = Class;
            Out.ContainerType = bArray ? EPinContainerType::Array : EPinContainerType::None; return true;
        }
        UObject* O = ObjectPath.IsEmpty() ? UObject::StaticClass() : LoadObject<UObject>(nullptr, *ObjectPath);
        if (!RuntimeReference(O, R)) { return false; }
        if (T == TEXT("object") || T == TEXT("class"))
        {
            UClass* C = Cast<UClass>(O);
            if (!C) { return Fail(R, TEXT("variable_type_invalid"), TEXT("Object/class type_object must resolve to UClass.")); }
            for (UClass* Parent = C; Parent; Parent = Parent->GetSuperClass()) { if (!RuntimeReference(Parent, R)) { return false; } }
            Out.PinCategory = T == TEXT("object") ? UEdGraphSchema_K2::PC_Object : UEdGraphSchema_K2::PC_Class; Out.PinSubCategoryObject = C;
        }
        else if (T == TEXT("struct") || T == TEXT("scriptstruct"))
        { if (!Cast<UScriptStruct>(O)) { return Fail(R, TEXT("variable_type_invalid"), TEXT("Struct type_object must resolve to UScriptStruct.")); } Out.PinCategory = UEdGraphSchema_K2::PC_Struct; Out.PinSubCategoryObject = O; }
        else
        { if (!Cast<UEnum>(O)) { return Fail(R, TEXT("variable_type_invalid"), TEXT("Enum type_object must resolve to UEnum.")); } Out.PinCategory = UEdGraphSchema_K2::PC_Byte; Out.PinSubCategoryObject = O; }
    }
    else { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Unsupported type descriptor; delegate/interface/map/set/soft-reference types are not part of this variable slice.")); }
    Out.ContainerType = bArray ? EPinContainerType::Array : EPinContainerType::None; return true;
}
bool ValidateExistingPin(const FEdGraphPinType& T, FResult& R)
{
    if (T.ContainerType != EPinContainerType::None && T.ContainerType != EPinContainerType::Array)
    { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Map/set member defaults are not supported by this slice.")); }
    const FName C = T.PinCategory;
    if (C == UEdGraphSchema_K2::PC_Struct || C == UEdGraphSchema_K2::PC_Object || C == UEdGraphSchema_K2::PC_Class || C == UEdGraphSchema_K2::PC_Byte)
    { if (!T.PinSubCategoryObject.IsValid()) { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Complex variable type has no resolved runtime type object.")); } return RuntimeReference(T.PinSubCategoryObject.Get(), R); }
    if (C != UEdGraphSchema_K2::PC_Boolean && C != UEdGraphSchema_K2::PC_Int && C != UEdGraphSchema_K2::PC_Int64 && C != UEdGraphSchema_K2::PC_Real
        && C != UEdGraphSchema_K2::PC_String && C != UEdGraphSchema_K2::PC_Name && C != UEdGraphSchema_K2::PC_Text)
    { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Existing member type is outside the supported legacy variable descriptors.")); }
    return true;
}

// A transient reflected property gives the real member-default import/export grammar.
// CreatePropertyOnScope is public/exported and explicitly does not link the result.
struct FTypedDefault
{
    TStrongObjectPtr<UStruct> Scope;
    FProperty* Property = nullptr;
    void* Value = nullptr;
    FString Stored;
    ~FTypedDefault() { if (Value) { Property->DestroyValue(Value); FMemory::Free(Value); } }
    bool Create(const FEdGraphPinType& Type, UBlueprint* BP, FResult& R)
    {
        Scope.Reset(NewObject<UStruct>(GetTransientPackage(), NAME_None, RF_Transient));
        FCompilerResultsLog Log;
        Property = FKismetCompilerUtilities::CreatePropertyOnScope(Scope.Get(), TEXT("Value"), Type, BP->SkeletonGeneratedClass,
            CPF_None, GetDefault<UEdGraphSchema_K2>(), Log);
        if (!Property || Log.NumErrors) { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Public Kismet property creation rejected the variable pin type.")); }
        Scope->AddCppProperty(Property); Scope->Bind(); Scope->StaticLink(true);
        Value = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment()); Property->InitializeValue(Value); return true;
    }
};
bool EnumNumber(UEnum* Enum, const TSharedPtr<FJsonValue>& V, int64& Number)
{
    if (!IsValid(Enum)) { return false; }
    for (int32 I = 0; I < Enum->NumEnums(); ++I)
    {
        if (Enum->GetNameStringByIndex(I).EndsWith(TEXT("_MAX"))) { continue; }
        if ((V->Type == EJson::String && (V->AsString() == Enum->GetNameStringByIndex(I) || V->AsString() == Enum->GetNameByIndex(I).ToString()))
            || (V->Type == EJson::Number && V->AsNumber() == static_cast<double>(Enum->GetValueByIndex(I))))
        { Number = Enum->GetValueByIndex(I); return true; }
    }
    return false;
}
bool Numeric(FNumericProperty* P, double N, void* Address)
{
    if (!FMath::IsFinite(N)) { return false; }
    if (P->IsInteger())
    {
        const bool Unsigned = CastField<FByteProperty>(P) || CastField<FUInt16Property>(P) || CastField<FUInt32Property>(P) || CastField<FUInt64Property>(P);
        const int32 Bits = P->GetElementSize() * 8;
        if (FMath::TruncToDouble(N) != N || FMath::Abs(N) > 9007199254740991.0 || N < (Unsigned ? 0.0 : -FMath::Pow(2., Bits - 1)) || N >= FMath::Pow(2., Unsigned ? Bits : Bits - 1)) { return false; }
        P->SetIntPropertyValue(Address, static_cast<uint64>(static_cast<int64>(N))); return true;
    }
    if (CastField<FFloatProperty>(P) && FMath::Abs(N) > MAX_flt) { return false; }
    P->SetFloatingPointPropertyValue(Address, N); return true;
}
bool Triple(const TSharedPtr<FJsonValue>& V, const TCHAR* const* Keys, double* Out)
{
    if (V->Type == EJson::Array)
    { if (V->AsArray().Num() != 3) { return false; } for (int32 I = 0; I < 3; ++I) { if (V->AsArray()[I]->Type != EJson::Number) { return false; } Out[I] = V->AsArray()[I]->AsNumber(); } }
    else if (V->Type == EJson::Object)
    {
        if (V->AsObject()->Values.Num() != 3) { return false; }
        for (int32 I = 0; I < 3; ++I)
        { const TSharedPtr<FJsonValue> X = V->AsObject()->TryGetField(Keys[I]); if (!X || X->Type != EJson::Number) { return false; } Out[I] = X->AsNumber(); }
    }
    else { return false; }
    for (int32 I = 0; I < 3; ++I) { if (!FMath::IsFinite(Out[I]) || FMath::Abs(Out[I]) > 1.e12) { return false; } } return true;
}
bool Fill(FProperty* P, void* Address, const TSharedPtr<FJsonValue>& V, FResult& R, int32 Depth = 0)
{
    if (!P || !V || Depth > 8 || P->ArrayDim != 1) { return Fail(R, TEXT("variable_default_unsupported"), TEXT("Unsupported property shape or nesting.")); }
    bool Ok = false;
    if (FArrayProperty* A = CastField<FArrayProperty>(P))
    {
        if (V->Type != EJson::Array || V->AsArray().Num() > 256) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Array defaults require a JSON array of at most 256 elements.")); }
        FScriptArrayHelper H(A, Address); H.Resize(V->AsArray().Num());
        for (int32 I = 0; I < H.Num(); ++I) { if (!Fill(A->Inner, H.GetRawPtr(I), V->AsArray()[I], R, Depth + 1)) { return false; } } return true;
    }
    if (FBoolProperty* BooleanProperty = CastField<FBoolProperty>(P)) { Ok = V->Type == EJson::Boolean; if (Ok) { BooleanProperty->SetPropertyValue(Address, V->AsBool()); } }
    else if (FEnumProperty* E = CastField<FEnumProperty>(P))
    { int64 Number = 0; Ok = E->GetUnderlyingProperty() && EnumNumber(E->GetEnum(), V, Number); if (Ok) { E->GetUnderlyingProperty()->SetIntPropertyValue(Address, Number); } }
    else if (FByteProperty* B = CastField<FByteProperty>(P); B && B->Enum)
    { int64 Number = 0; Ok = EnumNumber(B->Enum, V, Number) && Number >= 0 && Number <= 255; if (Ok) { B->SetPropertyValue(Address, static_cast<uint8>(Number)); } }
    else if (FNumericProperty* NumberProperty = CastField<FNumericProperty>(P)) { Ok = V->Type == EJson::Number && Numeric(NumberProperty, V->AsNumber(), Address); }
    else if (FStrProperty* StringProperty = CastField<FStrProperty>(P)) { Ok = V->Type == EJson::String; if (Ok) { StringProperty->SetPropertyValue(Address, V->AsString()); } }
    else if (FNameProperty* NameProperty = CastField<FNameProperty>(P)) { Ok = V->Type == EJson::String && SafeName(V->AsString(), true); if (Ok) { NameProperty->SetPropertyValue(Address, FName(*V->AsString())); } }
    else if (FTextProperty* T = CastField<FTextProperty>(P)) { Ok = V->Type == EJson::String; if (Ok) { T->SetPropertyValue(Address, FText::FromString(V->AsString())); } }
    else if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(P))
    {
        if (V->Type == EJson::Null) { O->SetObjectPropertyValue(Address, nullptr); return true; }
        if (V->Type != EJson::String || !SafeName(V->AsString()) || !V->AsString().StartsWith(TEXT("/"))) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Object defaults require an exact asset/class path or JSON null.")); }
        UObject* Object = LoadObject<UObject>(nullptr, *V->AsString());
        if (!RuntimeReference(Object, R)) { return false; }
        Ok = Object->IsA(O->PropertyClass);
        if (FClassProperty* C = CastField<FClassProperty>(P)) { UClass* Class = Cast<UClass>(Object); Ok = Class && Class->IsChildOf(C->MetaClass); }
        if (Ok) { O->SetObjectPropertyValue(Address, Object); }
    }
    else if (FStructProperty* S = CastField<FStructProperty>(P))
    {
        const TCHAR* XYZ[] = {TEXT("x"), TEXT("y"), TEXT("z")}; const TCHAR* PYR[] = {TEXT("pitch"), TEXT("yaw"), TEXT("roll")}; double N[3];
        if (S->Struct == TBaseStructure<FVector>::Get()) { Ok = Triple(V, XYZ, N); if (Ok) { *static_cast<FVector*>(Address) = FVector(N[0], N[1], N[2]); } }
        else if (S->Struct == TBaseStructure<FRotator>::Get()) { Ok = Triple(V, PYR, N); if (Ok) { *static_cast<FRotator*>(Address) = FRotator(N[0], N[1], N[2]); } }
        else if (S->Struct == TBaseStructure<FTransform>::Get())
        {
            if (V->Type != EJson::Object) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Transform default requires an object with location/translation, rotation and scale/scale3d.")); }
            FVector Location = FVector::ZeroVector, Scale = FVector::OneVector; FRotator Rotation = FRotator::ZeroRotator;
            TSet<FString> Groups;
            for (const auto& Pair : V->AsObject()->Values)
            {
                FString Key(Pair.Key.ToView()); const FString Group = Key == TEXT("translation") ? TEXT("location") : Key == TEXT("scale3d") ? TEXT("scale") : Key;
                if (Groups.Contains(Group) || (Group != TEXT("location") && Group != TEXT("rotation") && Group != TEXT("scale")) || !Triple(Pair.Value, Group == TEXT("rotation") ? PYR : XYZ, N))
                { return Fail(R, TEXT("variable_default_invalid"), TEXT("Invalid, unknown or duplicated Transform member.")); }
                Groups.Add(Group); if (Group == TEXT("location")) { Location = FVector(N[0], N[1], N[2]); } else if (Group == TEXT("scale")) { Scale = FVector(N[0], N[1], N[2]); } else { Rotation = FRotator(N[0], N[1], N[2]); }
            }
            *static_cast<FTransform*>(Address) = FTransform(Rotation, Location, Scale); return true;
        }
        else
        {
            if (!S->Struct || !RuntimeReference(S->Struct, R)) { return false; }
            if (V->Type != EJson::Object) { return Fail(R, TEXT("variable_default_unsupported"), TEXT("Other runtime struct defaults require explicit reflected member objects; raw import text is unsupported.")); }
            TSet<FProperty*> Assigned;
            for (const auto& Pair : V->AsObject()->Values)
            {
                FProperty* Member = nullptr; const FString Key(Pair.Key.ToView());
                for (TFieldIterator<FProperty> It(S->Struct); It; ++It) { if (It->GetName().Equals(Key, ESearchCase::IgnoreCase)) { Member = *It; break; } }
                if (!Member || Assigned.Contains(Member)) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Unknown or case-duplicated struct field.")); }
                Assigned.Add(Member); if (!Fill(Member, Member->ContainerPtrToValuePtr<void>(Address), Pair.Value, R, Depth + 1)) { return false; }
            }
            return true;
        }
    }
    else { return Fail(R, TEXT("variable_default_unsupported"), TEXT("This reflected property is unsupported; no generic raw ImportText or arbitrary object creation is used.")); }
    return Ok || Fail(R, TEXT("variable_default_invalid"), TEXT("JSON value does not fit the requested reflected type or bounds."));
}
bool PrepareDefault(FTypedDefault& Typed, const FString& Json, FResult& R)
{
    TSharedPtr<FJsonValue> Value;
    if (!Parse(Json, Value, R) || !Fill(Typed.Property, Typed.Value, Value, R)) { return false; }
    if (!FBlueprintEditorUtils::PropertyValueToString_Direct(Typed.Property, static_cast<uint8*>(Typed.Value), Typed.Stored, Typed.Scope.Get()))
    { return Fail(R, TEXT("variable_default_unsupported"), TEXT("Member-default export failed.")); }
    void* Check = FMemory::Malloc(Typed.Property->GetSize(), Typed.Property->GetMinAlignment()); Typed.Property->InitializeValue(Check);
    const bool Imported = FBlueprintEditorUtils::PropertyValueFromString_Direct(Typed.Property, Typed.Stored, static_cast<uint8*>(Check), Typed.Scope.Get());
    const bool Same = Imported && Typed.Property->Identical(Typed.Value, Check, PPF_None);
    Typed.Property->DestroyValue(Check); FMemory::Free(Check);
    return Same || Fail(R, TEXT("variable_default_roundtrip_unsupported"), TEXT("UE member-default text cannot preserve this typed value exactly; no Blueprint mutation occurred."));
}
TSharedRef<FJsonObject> PinJson(const FEdGraphPinType& T)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("category"), T.PinCategory.ToString());
    J->SetStringField(TEXT("subcategory"), T.PinSubCategory.ToString()); J->SetBoolField(TEXT("is_array"), T.ContainerType == EPinContainerType::Array);
    J->SetStringField(TEXT("subcategory_object"), T.PinSubCategoryObject.IsValid() ? T.PinSubCategoryObject->GetPathName() : TEXT("")); return J;
}
TSharedPtr<FJsonValue> ReadTyped(FProperty* P, const void* Address, int32 Depth = 0)
{
    if (!P || !Address || Depth > 8 || P->ArrayDim != 1) { return nullptr; }
    if (FArrayProperty* A = CastField<FArrayProperty>(P))
    { FScriptArrayHelper H(A, Address); if (H.Num() > 256) { return nullptr; } TArray<TSharedPtr<FJsonValue>> Values;
      for (int32 I = 0; I < H.Num(); ++I) { auto V = ReadTyped(A->Inner, H.GetRawPtr(I), Depth + 1); if (!V) { return nullptr; } Values.Add(V); } return MakeShared<FJsonValueArray>(Values); }
    if (FBoolProperty* B = CastField<FBoolProperty>(P)) { return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(Address)); }
    if (FEnumProperty* E = CastField<FEnumProperty>(P))
    { if (!E->GetEnum() || !E->GetUnderlyingProperty()) { return nullptr; } return MakeShared<FJsonValueNumber>(static_cast<double>(E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address))); }
    if (FNumericProperty* N = CastField<FNumericProperty>(P))
    { if (N->IsInteger()) { const int64 V = N->GetSignedIntPropertyValue(Address); if (V > 9007199254740991LL || V < -9007199254740991LL) { return MakeShared<FJsonValueString>(LexToString(V)); } return MakeShared<FJsonValueNumber>(static_cast<double>(V)); }
      const double V = N->GetFloatingPointPropertyValue(Address); if (!FMath::IsFinite(V)) { return nullptr; } return MakeShared<FJsonValueNumber>(V); }
    if (FStrProperty* S = CastField<FStrProperty>(P)) { const FString V = S->GetPropertyValue(Address); if (V.Len() > 65536) { return nullptr; } return MakeShared<FJsonValueString>(V); }
    if (FNameProperty* N = CastField<FNameProperty>(P)) { return MakeShared<FJsonValueString>(N->GetPropertyValue(Address).ToString()); }
    if (FTextProperty* T = CastField<FTextProperty>(P)) { const FString V = T->GetPropertyValue(Address).ToString(); if (V.Len() > 65536) { return nullptr; } return MakeShared<FJsonValueString>(V); }
    if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(P)) { const UObject* V = O->GetObjectPropertyValue(Address); if (V) { return MakeShared<FJsonValueString>(V->GetPathName()); } return MakeShared<FJsonValueNull>(); }
    if (FStructProperty* S = CastField<FStructProperty>(P))
    {
        if (!IsValid(S->Struct)) { return nullptr; } auto J = MakeShared<FJsonObject>(); int32 Count = 0;
        for (TFieldIterator<FProperty> It(S->Struct); It; ++It)
        { if (++Count > 64) { return nullptr; } auto V = ReadTyped(*It, It->ContainerPtrToValuePtr<void>(Address), Depth + 1); if (!V) { return nullptr; } J->SetField(It->GetName(), V); }
        return MakeShared<FJsonValueObject>(J);
    }
    return nullptr;
}
TSharedRef<FJsonObject> Metadata(const FBPVariableDescription& V)
{
    auto J = MakeShared<FJsonObject>(); for (const auto& E : V.MetaDataArray) { J->SetStringField(E.DataKey.ToString(), E.DataValue); } return J;
}
TSharedRef<FJsonObject> Row(UBlueprint* BP, const FBPVariableDescription& V, const TCHAR* Scope, int32 Index)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("scope"), Scope); J->SetNumberField(TEXT("index"), Index);
    J->SetStringField(TEXT("name"), V.VarName.ToString()); J->SetStringField(TEXT("guid"), V.VarGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("friendly_name"), V.FriendlyName); J->SetStringField(TEXT("category"), V.Category.ToString());
    J->SetStringField(TEXT("property_flags_string"), LexToString(V.PropertyFlags)); J->SetNumberField(TEXT("property_flags"), static_cast<double>(V.PropertyFlags));
    J->SetBoolField(TEXT("instance_editable"), !!(V.PropertyFlags & CPF_Edit) && !(V.PropertyFlags & CPF_DisableEditOnInstance)); J->SetBoolField(TEXT("blueprint_visible"), !!(V.PropertyFlags & CPF_BlueprintVisible));
    J->SetBoolField(TEXT("expose_on_spawn"), !!(V.PropertyFlags & CPF_ExposeOnSpawn));
    J->SetObjectField(TEXT("pin_type"), PinJson(V.VarType)); J->SetObjectField(TEXT("metadata"), Metadata(V));
    J->SetStringField(TEXT("stored_default"), V.DefaultValue);
    UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
    FProperty* P = BP->GeneratedClass ? FindFProperty<FProperty>(BP->GeneratedClass, V.VarName) : nullptr;
    TSharedPtr<FJsonValue> Value = CDO && P ? ReadTyped(P, P->ContainerPtrToValuePtr<void>(CDO)) : nullptr;
    J->SetBoolField(TEXT("default_value_available"), Value.IsValid()); J->SetBoolField(TEXT("default_value_current_compile"), BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings);
    J->SetStringField(TEXT("default_value_scope"), TEXT("existing_generated_cdo_without_creation; stored_default_is_separate_pending_descriptor_value"));
    if (P) { J->SetStringField(TEXT("property_class"), P->GetClass()->GetName()); }
    if (Value) { J->SetField(TEXT("default_value"), Value); }
    return J;
}
TSharedRef<FJsonObject> Capture(UBlueprint* BP)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("blueprint_name"), BP->GetName()); J->SetStringField(TEXT("asset_path"), BP->GetPathName());
    J->SetBoolField(TEXT("package_dirty"), BP->GetOutermost()->IsDirty()); J->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(BP->Status.GetValue()));
    UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
    J->SetStringField(TEXT("generated_cdo_path"), CDO ? CDO->GetPathName() : TEXT(""));
    TArray<TSharedPtr<FJsonValue>> New, Generated, Categories;
    for (int32 I = 0; I < BP->NewVariables.Num(); ++I) { New.Add(MakeShared<FJsonValueObject>(Row(BP, BP->NewVariables[I], TEXT("new"), I))); }
    for (int32 I = 0; I < BP->GeneratedVariables.Num(); ++I) { Generated.Add(MakeShared<FJsonValueObject>(Row(BP, BP->GeneratedVariables[I], TEXT("generated"), I))); }
    for (const FName& C : BP->CategorySorting) { Categories.Add(MakeShared<FJsonValueString>(C.ToString())); }
    J->SetArrayField(TEXT("new_variables"), New); J->SetArrayField(TEXT("generated_variables"), Generated); J->SetArrayField(TEXT("category_sorting"), Categories);
    J->SetNumberField(TEXT("new_variable_count"), New.Num()); J->SetNumberField(TEXT("generated_variable_count"), Generated.Num()); J->SetNumberField(TEXT("category_count"), Categories.Num()); return J;
}
void Merge(FJsonObject& To, const FJsonObject& From) { for (const auto& P : From.Values) { To.SetField(FString(P.Key.ToView()), P.Value); } }
FString JsonText(const TSharedRef<FJsonObject>& J) { FString Text; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text)); return Text; }
void ObserveAfter(UBlueprint* BP, const TSharedRef<FJsonObject>& Before, FResult& R)
{
    auto After = Capture(BP); R.Data->SetObjectField(TEXT("after"), After);
    R.Data->SetStringField(TEXT("generated_cdo_path"), After->GetStringField(TEXT("generated_cdo_path")));
    R.Data->SetBoolField(TEXT("dirty_before"), Before->GetBoolField(TEXT("package_dirty"))); R.Data->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty());
    R.Data->SetBoolField(TEXT("package_dirty_before"), Before->GetBoolField(TEXT("package_dirty"))); R.Data->SetBoolField(TEXT("package_dirty_after"), BP->GetOutermost()->IsDirty());
    R.Data->SetBoolField(TEXT("dirty_state_unchanged"), Before->GetBoolField(TEXT("package_dirty")) == BP->GetOutermost()->IsDirty());
    R.bChanged = JsonText(Before) != JsonText(After); R.bChangeStateKnown = true;
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("local_variable_descriptors_existing_generated_CDO_dirty_and_status; not arbitrary callback or unloaded asset state"));
}
UBlueprint* LoadBlueprint(const FString& Path, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("variable_editor_state_unsupported"), TEXT("Variable tools require the editor game thread outside PIE/SIE.")); return nullptr; }
    if (!SafeName(Path) || !Path.StartsWith(TEXT("/Game/"))) { Fail(R, TEXT("variable_blueprint_path_invalid"), TEXT("Provide an exact bounded /Game Blueprint asset/package path.")); return nullptr; }
    FString AssetPath = Path; if (!Path.Contains(TEXT("."))) { AssetPath += TEXT(".") + FPackageName::GetShortName(Path); }
    if (!SafeName(AssetPath)) { Fail(R, TEXT("variable_blueprint_path_invalid"), TEXT("The completed Blueprint object path exceeds the safe 1023-character name bound.")); return nullptr; }
    const bool Loaded = FindObject<UBlueprint>(nullptr, *AssetPath) != nullptr;
    R.Data->SetBoolField(TEXT("loaded_during_operation"), !Loaded); R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), !Loaded);
    R.Data->SetStringField(TEXT("load_scope"), TEXT("dirty/status baseline begins after asset load; implicit PostLoad effects are not measured"));
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!IsValid(BP)) { R.bChangeStateKnown = Loaded; Fail(R, TEXT("variable_blueprint_not_found"), TEXT("Blueprint load failed; inspect possible load effects before retry.")); return nullptr; }
    if (BP->NewVariables.Num() + BP->GeneratedVariables.Num() > MaxVariables)
    { Fail(R, TEXT("variable_budget_exceeded"), TEXT("Variable descriptor budget is 4096.")); return nullptr; }
    return BP;
}
bool MutationScope(UBlueprint* BP, FResult& R)
{
    if (BP->BlueprintType != BPTYPE_Normal || !BP->ParentClass || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->Status == BS_BeingCreated)
    { return Fail(R, TEXT("variable_blueprint_state_unsupported"), TEXT("Requires a normal idle Blueprint without suppressed or queued structural modification.")); }
    auto Parent = InspectBlueprintParent(BP->ParentClass->GetPathName()); R.Data->SetObjectField(TEXT("parent_validation"), Parent);
    if (!Parent->GetBoolField(TEXT("success"))) { return Fail(R, TEXT("variable_parent_unsupported"), TEXT("Blueprint runtime parent validation failed.")); }
    for (UClass* C = BP->ParentClass; C; C = C->GetSuperClass())
    { const FString Package = C->GetOutermost()->GetName(); if (C->HasAnyClassFlags(CLASS_Native) && Package != TEXT("/Script/Engine") && Package != TEXT("/Script/CoreUObject")) { return Fail(R, TEXT("variable_parent_unsupported"), TEXT("Native Blueprint parents outside Engine/CoreUObject are unaudited.")); } }
    // AddMemberVariable can rename loaded child members, while compilation can reinstance
    // external objects. Refuse those known effects before dispatching either operation.
    TArray<FString> External;
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != BP && It->ParentClass && BP->GeneratedClass && It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.AddUnique(It->GetPathName()); } }
    if (BP->GeneratedClass)
    {
        TArray<UObject*> Instances; GetObjectsOfClass(BP->GeneratedClass, Instances, true, RF_NoFlags);
        for (UObject* O : Instances) { if (IsValid(O) && O->GetOutermost() != BP->GetOutermost()) { External.AddUnique(O->GetPathName()); } }
    }
    External.Sort(); R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"), StringValues(External));
    return External.IsEmpty() || Fail(R, TEXT("variable_external_instances_unsupported"), TEXT("Loaded derived Blueprints or external instances can be affected by structural modification/compilation; inspect them before editing."));
}
FBPVariableDescription* FindLocal(UBlueprint* BP, const FString& Name, int32& Index, FResult& R)
{
    if (!SafeName(Name)) { Fail(R, TEXT("variable_name_invalid"), TEXT("Variable selector must contain 1..1023 safe UTF-16 characters.")); return nullptr; }
    Index = INDEX_NONE;
    for (int32 I = 0; I < BP->NewVariables.Num(); ++I)
    {
        if (!BP->NewVariables[I].VarName.ToString().Equals(Name, ESearchCase::IgnoreCase)) { continue; }
        if (Index != INDEX_NONE) { Fail(R, TEXT("variable_name_ambiguous"), TEXT("Multiple local descriptors match the requested variable.")); return nullptr; } Index = I;
    }
    if (Index == INDEX_NONE) { Fail(R, TEXT("variable_not_found"), TEXT("Default mutation requires a locally declared NewVariables member.")); return nullptr; }
    return &BP->NewVariables[Index];
}
bool CurrentMatches(UBlueprint* BP, const FBPVariableDescription& V, const FTypedDefault& Typed)
{
    if (!V.DefaultValue.IsEmpty()) { return V.DefaultValue == Typed.Stored; }
    UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
    FProperty* P = BP->GeneratedClass ? FindFProperty<FProperty>(BP->GeneratedClass, V.VarName) : nullptr;
    return CDO && P && P->SameType(Typed.Property) && Typed.Property->Identical(Typed.Value, P->ContainerPtrToValuePtr<void>(CDO), PPF_None);
}
bool FinishRequested(UBlueprint* BP, const FString& Name, const FTypedDefault& Typed, FResult& R)
{
    if (R.bCompileRequested)
    {
        R.Stage = TEXT("compile"); R.bCompileAttempted = true; R.bMutationAttempted = true;
        auto C = CompileBlueprint(BP->GetPathName(), false, false); R.Data->SetObjectField(TEXT("compile"), C);
        bool Compiled = false; C->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled;
        if (!Compiled) { return Fail(R, TEXT("variable_compile_failed"), TEXT("Variable state changed but compilation failed; inspect compiler diagnostics. Save was not attempted.")); }
    }
    R.Stage = TEXT("verification"); int32 Index = INDEX_NONE; FBPVariableDescription* V = FindLocal(BP, Name, Index, R);
    if (!V || !CurrentMatches(BP, *V, Typed)) { return Fail(R, TEXT("variable_default_postcondition_failed"), TEXT("Stored descriptor or generated CDO did not preserve the requested typed default.")); }
    if (R.bSaveRequested)
    {
        R.Stage = TEXT("save"); R.bSaveAttempted = true; R.bMutationAttempted = true; R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(BP, false);
        if (!R.bSaved) { return Fail(R, TEXT("variable_save_failed"), TEXT("Default readback passed but explicit save failed; inspect the dirty package before retry.")); }
    }
    return true;
}
struct FMetadataSuppressionScope
{
    UBlueprint* Blueprint;
    bool Previous;
    explicit FMetadataSuppressionScope(UBlueprint* BP) : Blueprint(BP), Previous(BP->bSuppressStructurallyModified) { Blueprint->bSuppressStructurallyModified = true; }
    ~FMetadataSuppressionScope() { Blueprint->bSuppressStructurallyModified = Previous; }
};
}

FResult Inspect(const FString& Path, const FString& Name, bool bMetadata)
{
    FResult R; R.Stage = TEXT("read"); UBlueprint* BP = LoadBlueprint(Path, R); if (!BP) { return R; }
    const auto Before = Capture(BP); R.Data->SetObjectField(TEXT("before"), Before);
    if (!bMetadata) { Merge(*R.Data, *Before); }
    else
    {
        if (!SafeName(Name)) { Fail(R, TEXT("variable_name_invalid"), TEXT("Variable selector must be bounded and nonempty.")); return R; }
        const FBPVariableDescription* Found = nullptr; FString Scope; int32 Index = INDEX_NONE;
        auto Search = [&](const TArray<FBPVariableDescription>& Variables, const TCHAR* Label)
        { for (int32 I = 0; I < Variables.Num(); ++I) { if (Variables[I].VarName.ToString().Equals(Name, ESearchCase::IgnoreCase)) { if (Found) { return false; } Found = &Variables[I]; Scope = Label; Index = I; } } return true; };
        if (!Search(BP->NewVariables, TEXT("new")) || !Search(BP->GeneratedVariables, TEXT("generated"))) { Fail(R, TEXT("variable_name_ambiguous"), TEXT("Variable occurs in multiple descriptor scopes.")); return R; }
        if (!Found) { Fail(R, TEXT("variable_not_found"), TEXT("Variable metadata descriptor was not found.")); return R; }
        R.Data->SetStringField(TEXT("scope"), Scope); R.Data->SetNumberField(TEXT("index"), Index); R.Data->SetStringField(TEXT("guid"), Found->VarGuid.ToString(EGuidFormats::Digits));
        R.Data->SetStringField(TEXT("variable_name"), Found->VarName.ToString()); R.Data->SetObjectField(TEXT("metadata"), Metadata(*Found));
        TArray<TSharedPtr<FJsonValue>> Entries;
        for (const auto& E : Found->MetaDataArray) { auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("index"), Entries.Num()); J->SetStringField(TEXT("key"), E.DataKey.ToString()); J->SetStringField(TEXT("value"), E.DataValue); Entries.Add(MakeShared<FJsonValueObject>(J)); }
        R.Data->SetArrayField(TEXT("metadata_entries"), Entries); R.Data->SetNumberField(TEXT("metadata_count"), Entries.Num()); R.Data->SetNumberField(TEXT("metadata_entry_count"), Entries.Num());
        R.Data->SetStringField(TEXT("blueprint_name"), BP->GetName()); R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName());
    }
    R.Data->SetBoolField(TEXT("read_only"), true); ObserveAfter(BP, Before, R);
    R.bSuccess = R.bPostconditionPassed = !R.bChanged; R.Stage = R.bSuccess ? TEXT("complete") : TEXT("verification");
    if (!R.bSuccess) { Fail(R, TEXT("variable_read_changed_state"), TEXT("Local variable/dirty/status observations changed during inspection.")); } return R;
}

FResult Add(const FString& Path, const FString& Name, const FString& Type, bool bExposed, const FString& DefaultJson,
    const FString& Category, const FString& Tooltip, const FString& Friendly, const FString& TypeObject, bool bArray,
    const FString& MetadataJson, const FString& Guid, bool bEditable)
{
    FResult R;
    if (!Identifier(Name) || !SafeName(Category, true) || !SafeName(Friendly, true) || !SafeText(Tooltip, 4096) || !SafeName(Type))
    { Fail(R, TEXT("variable_name_invalid"), TEXT("New variable names require an ASCII identifier of 1..100 characters; category/friendly name <=1023 and tooltip <=4096.")); return R; }
    FGuid RequestedGuid;
    if (!Guid.IsEmpty() && (!FGuid::Parse(Guid, RequestedGuid) || !RequestedGuid.IsValid())) { Fail(R, TEXT("variable_guid_invalid"), TEXT("variable_guid must be a valid nonzero GUID.")); return R; }
    TSharedPtr<FJsonValue> MetaJson;
    if (!Parse(MetadataJson.IsEmpty() ? TEXT("{}") : MetadataJson, MetaJson, R)) { return R; }
    if (MetaJson->Type != EJson::Object || MetaJson->AsObject()->Values.Num() > 64) { Fail(R, TEXT("variable_metadata_invalid"), TEXT("metadata_json requires an object with at most 64 scalar entries.")); return R; }
    TArray<TPair<FName, FString>> Entries; TSet<FName> MetaNames;
    for (const auto& Pair : MetaJson->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView()); const FName MetaName(*Key);
        if (MetaName.IsNone() || MetaNames.Contains(MetaName)) { Fail(R, TEXT("variable_metadata_invalid"), TEXT("Metadata keys must be non-None and unique ignoring case.")); return R; }
        FString Value;
        if (Pair.Value->Type == EJson::String) { Value = Pair.Value->AsString(); }
        else if (Pair.Value->Type == EJson::Boolean) { Value = Pair.Value->AsBool() ? TEXT("true") : TEXT("false"); }
        else if (Pair.Value->Type == EJson::Number) { Value = FString::SanitizeFloat(Pair.Value->AsNumber()); }
        else { Fail(R, TEXT("variable_metadata_invalid"), TEXT("Metadata supports only string, boolean or numeric scalar values.")); return R; }
        Entries.Emplace(MetaName, Value); MetaNames.Add(MetaName);
    }
    UBlueprint* BP = LoadBlueprint(Path, R); if (!BP || !MutationScope(BP, R)) { return R; }
    const auto Before = Capture(BP); R.Data->SetObjectField(TEXT("before"), Before);
    if (BP->NewVariables.Num() + BP->GeneratedVariables.Num() >= MaxVariables) { Fail(R, TEXT("variable_budget_exceeded"), TEXT("Adding a variable would exceed the 4096 descriptor budget.")); return R; }
    const FName VarName(*Name); TSet<FName> Existing; FBlueprintEditorUtils::GetClassVariableList(BP, Existing, true);
    for (const auto& V : BP->NewVariables) { Existing.Add(V.VarName); }
    for (const auto& V : BP->GeneratedVariables) { Existing.Add(V.VarName); }
    if (Existing.Contains(VarName)) { Fail(R, TEXT("variable_name_collision"), TEXT("A local or inherited member already uses this variable name; no automatic rename is performed.")); return R; }
    for (const auto& V : BP->NewVariables) { if (!Guid.IsEmpty() && V.VarGuid == RequestedGuid) { Fail(R, TEXT("variable_guid_collision"), TEXT("variable_guid is already in use.")); return R; } }
    for (const auto& V : BP->GeneratedVariables) { if (!Guid.IsEmpty() && V.VarGuid == RequestedGuid) { Fail(R, TEXT("variable_guid_collision"), TEXT("variable_guid is already in use.")); return R; } }
    FEdGraphPinType Pin; if (!PinType(Type, TypeObject, bArray, Pin, R)) { ObserveAfter(BP, Before, R); return R; }
    FTypedDefault Typed; if (!ValidateExistingPin(Pin, R) || !Typed.Create(Pin, BP, R) || (!DefaultJson.IsEmpty() && !PrepareDefault(Typed, DefaultJson, R))) { ObserveAfter(BP, Before, R); return R; }
    if (!MutationScope(BP, R)) { ObserveAfter(BP, Before, R); return R; }
    Existing.Reset(); FBlueprintEditorUtils::GetClassVariableList(BP, Existing, true);
    for (const auto& V : BP->NewVariables) { Existing.Add(V.VarName); }
    for (const auto& V : BP->GeneratedVariables) { Existing.Add(V.VarName); }
    if (Existing.Contains(VarName) || BP->NewVariables.Num() + BP->GeneratedVariables.Num() >= MaxVariables)
    { Fail(R, TEXT("variable_preflight_changed_blueprint"), TEXT("Name availability or descriptor budget changed while resolving input types/defaults.")); ObserveAfter(BP, Before, R); return R; }
    R.Stage = TEXT("add"); R.bMutationAttempted = true;
    if (!FBlueprintEditorUtils::AddMemberVariable(BP, VarName, Pin, Typed.Stored))
    { ObserveAfter(BP, Before, R); Fail(R, TEXT("variable_add_failed"), TEXT("AddMemberVariable returned false; inspect actual before/after state before retry.")); return R; }
    int32 Index = INDEX_NONE; FBPVariableDescription* Variable = FindLocal(BP, Name, Index, R);
    if (!Variable) { ObserveAfter(BP, Before, R); return R; }
    const bool MetadataWork = !Category.IsEmpty() || !Tooltip.IsEmpty() || !Friendly.IsEmpty() || !Guid.IsEmpty() || !Entries.IsEmpty() || bExposed || bEditable;
    if (MetadataWork)
    {
        // Blueprint.h explicitly permits this scope only when followed by MarkStructural.
        // No compile, save, asynchronous task or early return occurs within the scope.
        {
        FMetadataSuppressionScope Suppress(BP);
        if (!Category.IsEmpty()) { Variable->Category = FText::FromString(Category); FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, VarName, nullptr, FText::FromString(Category), true); }
        if (!Tooltip.IsEmpty()) { FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, VarName, nullptr, TEXT("Tooltip"), Tooltip); }
        if (!Friendly.IsEmpty()) { Variable->FriendlyName = Friendly; }
        if (!Guid.IsEmpty()) { Variable->VarGuid = RequestedGuid; }
        for (const auto& E : Entries) { FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, VarName, nullptr, E.Key, E.Value); }
        if (bExposed || bEditable) { FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, VarName, false); }
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    R.Data->SetNumberField(TEXT("unsuppressed_structural_regeneration_requests"), MetadataWork ? 2 : 1);
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), true); R.Data->SetBoolField(TEXT("explicit_full_compile_performed"), false);
    Variable = FindLocal(BP, Name, Index, R); ObserveAfter(BP, Before, R);
    if (!Variable) { return R; }
    R.Data->SetStringField(TEXT("variable_name"), Name); R.Data->SetStringField(TEXT("variable_type"), Type);
    R.Data->SetStringField(TEXT("default_value"), Variable->DefaultValue); R.Data->SetStringField(TEXT("stored_default"), Variable->DefaultValue);
    R.Data->SetObjectField(TEXT("pin_type"), PinJson(Variable->VarType)); R.Data->SetStringField(TEXT("variable_guid"), Variable->VarGuid.ToString(EGuidFormats::Digits));
    R.Data->SetObjectField(TEXT("verified_variable"), Row(BP, *Variable, TEXT("new"), Index));
    bool Valid = Variable->VarType == Pin && (DefaultJson.IsEmpty() || Variable->DefaultValue == Typed.Stored)
        && (Guid.IsEmpty() || Variable->VarGuid == RequestedGuid) && (Category.IsEmpty() || Variable->Category.ToString() == Category)
        && (Friendly.IsEmpty() || Variable->FriendlyName == Friendly) && (!(bExposed || bEditable) || !(Variable->PropertyFlags & CPF_DisableEditOnInstance));
    for (const auto& E : Entries) { const int32 I = Variable->FindMetaDataEntryIndexForKey(E.Key); Valid &= I != INDEX_NONE && Variable->MetaDataArray[I].DataValue == E.Value; }
    if (!Valid) { Fail(R, TEXT("variable_add_postcondition_failed"), TEXT("Created variable did not retain its requested descriptor metadata/type/default; no rollback is claimed.")); R.Stage = TEXT("verification"); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}

FResult SetDefault(const FString& Path, const FString& Name, const FString& Json, bool bCompile, bool bSave, bool bDryRun)
{
    FResult R; R.bCompileRequested = bCompile; R.bSaveRequested = bSave;
    UBlueprint* BP = LoadBlueprint(Path, R); if (!BP || !MutationScope(BP, R)) { return R; }
    const auto Before = Capture(BP); R.Data->SetObjectField(TEXT("before"), Before); int32 Index = INDEX_NONE;
    FBPVariableDescription* V = FindLocal(BP, Name, Index, R); if (!V) { return R; }
    const FBPVariableDescription OriginalVariable = *V;
    FTypedDefault Typed; if (!ValidateExistingPin(OriginalVariable.VarType, R) || !Typed.Create(OriginalVariable.VarType, BP, R) || !PrepareDefault(Typed, Json, R)) { ObserveAfter(BP, Before, R); return R; }
    if (!MutationScope(BP, R)) { ObserveAfter(BP, Before, R); return R; }
    V = FindLocal(BP, Name, Index, R);
    if (!V || V->VarGuid != OriginalVariable.VarGuid || V->VarType != OriginalVariable.VarType || V->DefaultValue != OriginalVariable.DefaultValue)
    { Fail(R, TEXT("variable_preflight_changed_member"), TEXT("Member identity/type/default changed while resolving input references; no variable write was dispatched.")); ObserveAfter(BP, Before, R); return R; }
    const FString Previous = V->DefaultValue; const bool WouldChange = !CurrentMatches(BP, *V, Typed);
    R.Data->SetStringField(TEXT("previous_stored_default"), Previous); R.Data->SetStringField(TEXT("requested_stored_default"), Typed.Stored);
    R.Data->SetBoolField(TEXT("dry_run"), bDryRun); R.Data->SetBoolField(TEXT("would_change"), WouldChange);
    // KismetCompiler treats an empty descriptor string as "no pending default".
    // Writing it cannot clear a nonempty existing string CDO, including in dry-run.
    if (WouldChange && Typed.Stored.IsEmpty())
    { Fail(R, TEXT("variable_empty_default_unsupported"), TEXT("Clearing an existing nonempty string default needs a separate CDO edit path; the compiler ignores an empty pending descriptor. No variable write was dispatched.")); ObserveAfter(BP, Before, R); return R; }
    // UBlueprint::PreSave may recompile stale UDS dependencies without SkipSave.
    // Reject the known risky compile=false path before marking this asset dirty.
    if (!bCompile && bSave && BP->CachedUDSDependencies.Num() > 0 && (WouldChange || BP->Status != BS_UpToDate))
    { Fail(R, TEXT("variable_save_requires_compile"), TEXT("Saving a dirty Blueprint with cached user-defined-struct dependencies may implicitly compile in PreSave. Request compile=true before save; no variable write was dispatched.")); ObserveAfter(BP, Before, R); return R; }
    if (bDryRun)
    {
        R.Data->SetStringField(TEXT("stored_default_after"), Previous); R.Data->SetObjectField(TEXT("verified_variable"), Row(BP, *V, TEXT("new"), Index));
        R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetStringField(TEXT("compile_status"), TEXT("not_attempted"));
        ObserveAfter(BP, Before, R); R.bSuccess = R.bPostconditionPassed = !R.bChanged; R.Stage = R.bSuccess ? TEXT("complete") : TEXT("verification");
        if (!R.bSuccess) { Fail(R, TEXT("variable_dry_run_observed_change"), TEXT("No explicit mutation was dispatched, but local observations changed during dry-run preflight.")); } return R;
    }
    if (WouldChange)
    { R.Stage = TEXT("set_default"); R.bMutationAttempted = true; BP->Modify(); V->DefaultValue = Typed.Stored; FBlueprintEditorUtils::MarkBlueprintAsModified(BP); BP->MarkPackageDirty(); }
    const bool Finished = FinishRequested(BP, Name, Typed, R);
    ObserveAfter(BP, Before, R); V = FindLocal(BP, Name, Index, R);
    if (V) { R.Data->SetStringField(TEXT("stored_default_after"), V->DefaultValue); R.Data->SetObjectField(TEXT("verified_variable"), Row(BP, *V, TEXT("new"), Index)); }
    R.Data->SetBoolField(TEXT("compiled"), R.Compiled.Get(false)); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
    R.Data->SetStringField(TEXT("compile_status"), !R.bCompileAttempted ? TEXT("not_attempted") : R.Compiled.Get(false) ? TEXT("passed") : TEXT("failed"));
    if (!Finished || !V) { return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
