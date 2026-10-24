// Finite original function-node9 route, distinct from public validated-call7.
#include "Compatibility/UE58/CubelessFunctionNodeApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Variable.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "UObject/NoExportTypes.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/DefaultValueHelper.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::FunctionNode
{
namespace
{
using Function::FResult;
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Error; return false; }
bool HasNul(const FString& S)
{ for (int32 I = 0; I < S.Len(); ++I) { if (S[I] == TCHAR(0)) { return true; } } return false; }
bool Bounded(const FString& S, int32 Limit) { return S.Len() <= Limit && !HasNul(S); }
// The native serializer needs an object/array root; a one-element carrier keeps JSONAny's value kind.
// Bound the original text and reject interior NUL before adding the fixed carrier characters.
bool Parse(const FString& S, TSharedPtr<FJsonValue>& Value, FResult& R)
{
    if (!Bounded(S, 65536))
    { return Fail(R, TEXT("function_node_input_invalid"), TEXT("Bounded valid JSON required; no function-node work was dispatched.")); }
    TArray<TSharedPtr<FJsonValue>> Values;
    const FString Wrapped = TEXT("[") + S + TEXT("]");
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Wrapped), Values) || Values.Num() != 1 || !Values[0].IsValid())
    { return Fail(R, TEXT("function_node_input_invalid"), TEXT("Bounded valid JSON required; no function-node work was dispatched.")); }
    Value = Values[0]; return true;
}
FString Text(const TSharedRef<FJsonObject>& O)
{ FString S; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&S)); return S; }
FString Text(const TArray<TSharedPtr<FJsonValue>>& A)
{ FString S; FJsonSerializer::Serialize(A, TJsonWriterFactory<>::Create(&S)); return S; }

// Observe the real native declaration, rather than mapping an arbitrary name to it.
bool Source(UClass* Owner, UFunction* F, const TSharedRef<FJsonObject>& Row)
{
    const bool PlayerObject=IsValid(F)&&(F->GetName()==TEXT("GetPlayerPawn")||F->GetName()==TEXT("GetPlayerCharacter"));
    UClass* ExpectedReturn=PlayerObject?(F->GetName()==TEXT("GetPlayerPawn")?APawn::StaticClass():ACharacter::StaticClass()):APlayerController::StaticClass();
    const bool Identity = IsValid(Owner) && Owner == UGameplayStatics::StaticClass()
        && Owner->GetPathName() == TEXT("/Script/Engine.GameplayStatics")
        && Owner->HasAnyClassFlags(CLASS_Native) && (PlayerObject?!Owner->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly):!Owner->IsEditorOnly())
        && Owner->GetOutermost()->GetName() == TEXT("/Script/Engine")
        && IsValid(F) && F->GetOuterUClass() == Owner && (F->GetName() == TEXT("GetPlayerController")||PlayerObject)
        && Owner->FindFunctionByName(F->GetFName()) == F;
    Row->SetBoolField(TEXT("identity_observed"), Identity);
    if (!Identity) { return false; }
    Row->SetStringField(TEXT("class_path"), Owner->GetPathName()); Row->SetStringField(TEXT("function_path"), F->GetPathName());
    Row->SetStringField(TEXT("function_name"), F->GetName()); Row->SetStringField(TEXT("function_flags"), LexToString(static_cast<uint32>(F->FunctionFlags)));
    Row->SetStringField(TEXT("world_context"), F->GetMetaData(TEXT("WorldContext")));
    Row->SetBoolField(TEXT("unsafe_during_actor_construction"), F->GetBoolMetaData(TEXT("UnsafeDuringActorConstruction")));
    bool Signature = F->HasAllFunctionFlags(FUNC_Native | FUNC_Public | FUNC_Static | FUNC_BlueprintCallable | FUNC_BlueprintPure)
        && !F->HasAnyFunctionFlags(FUNC_EditorOnly | FUNC_Private | FUNC_Protected | FUNC_Delegate)
        && F->GetMetaData(TEXT("WorldContext")) == TEXT("WorldContextObject")
        && F->GetBoolMetaData(TEXT("UnsafeDuringActorConstruction"));
    int32 Count = 0; TArray<TSharedPtr<FJsonValue>> Parameters;
    for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
    {
        FProperty* P = *It; auto Parameter = MakeShared<FJsonObject>();
        Parameter->SetStringField(TEXT("name"), P->GetName()); Parameter->SetStringField(TEXT("field_class"), P->GetClass()->GetName());
        Parameter->SetStringField(TEXT("cpp_type"), P->GetCPPType());
        Parameter->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        const auto* Object = CastField<FObjectProperty>(P);
        bool Match = P->ArrayDim == 1 && P->GetOwnerStruct() == F;
        if (Count == 0) { Match &= P->GetFName() == TEXT("WorldContextObject") && Object && Object->PropertyClass == UObject::StaticClass()
            && !P->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm); }
        else if (Count == 1) { Match &= P->GetFName() == TEXT("PlayerIndex") && CastField<FIntProperty>(P)
            && !P->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm); }
        else if (Count == 2) { Match &= P->GetFName() == TEXT("ReturnValue") && Object && Object->PropertyClass == ExpectedReturn
            && P->HasAnyPropertyFlags(CPF_ReturnParm) && !P->HasAnyPropertyFlags(CPF_ReferenceParm); }
        else { Match = false; }
        Parameter->SetBoolField(TEXT("source_signature_match"), Match); Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
        Signature &= Match; ++Count;
    }
    Signature &= Count == 3; Row->SetArrayField(TEXT("parameters"), Parameters); Row->SetBoolField(TEXT("source_signature_match"), Signature);
    Row->SetBoolField(TEXT("prior_runtime_signature_capture_available"), false);
    return Signature;
}

bool Position(const TSharedPtr<FJsonValue>& Value, TArray<TSharedPtr<FJsonValue>>& Out,
    const TSharedRef<FJsonObject>& Adapter, FResult& R)
{
    int32 X = 0, Y = 0; TArray<TSharedPtr<FJsonValue>> Requested;
    if (Value->Type != EJson::Null)
    {
        if (Value->Type != EJson::Array || (Value->AsArray().Num() != 0 && Value->AsArray().Num() != 2))
        { return Fail(R, TEXT("function_node_position_unsupported"), TEXT("Use null, empty origin, or exactly two integral signed-int32 JSON numbers; original string/fractional/extra-coordinate coercions remain outside this slice.")); }
        Requested = Value->AsArray();
        for (int32 I = 0; I < Requested.Num(); ++I)
        {
            double N = 0;
            if (!Requested[I].IsValid() || Requested[I]->Type != EJson::Number || !Requested[I]->TryGetNumber(N)
                || !FMath::IsFinite(N) || N < MIN_int32 || N > MAX_int32 || N != FMath::TruncToDouble(N))
            { return Fail(R, TEXT("function_node_position_unsupported"), TEXT("Position coordinates must be finite integral signed-int32 numbers.")); }
            // Original GetVector2DFromJson casts to float before NodePos assignment.
            const float OriginalFloat = static_cast<float>(N); const double OriginalPosition = static_cast<double>(OriginalFloat);
            if (!FMath::IsFinite(OriginalPosition) || OriginalPosition < MIN_int32 || OriginalPosition > MAX_int32)
            { return Fail(R, TEXT("function_node_position_unsupported"), TEXT("Original float32 coordinate conversion would exceed signed-int32 storage; no node work was dispatched.")); }
            const int32 Actual = static_cast<int32>(OriginalFloat); if (I == 0) { X = Actual; } else { Y = Actual; }
        }
    }
    Out = { MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y) };
    Adapter->SetField(TEXT("requested_position"), Value); Adapter->SetArrayField(TEXT("prepared_position"), Out);
    Adapter->SetStringField(TEXT("position_conversion"), TEXT("original JSON number -> float32 -> checked int32; nullable/empty origin -> [0,0]"));
    return true;
}

// Actor getter inputs contain only the native object/self pin. Match the original
// value-kind dispatch: strings/array/object/null do not write that object pin;
// numeric/bool raw DefaultValue bypasses remain outside the finite adapter.
bool ActorDefaults(const TSharedPtr<FJsonValue>& Value, const TSharedRef<FJsonObject>& Adapter, FResult& R)
{
    Adapter->SetField(TEXT("requested_params"),Value);Adapter->SetBoolField(TEXT("nonobject_params_ignored"),Value->Type!=EJson::Object&&Value->Type!=EJson::Null);
    TArray<TSharedPtr<FJsonValue>> Rows;Adapter->SetArrayField(TEXT("parameter_adaptation"),Rows);
    if(Value->Type!=EJson::Object) { return true; }
    if(Value->AsObject()->Values.Num()>64) { return Fail(R,TEXT("function_node_parameter_budget"),TEXT("At most 64 parameter keys are admitted.")); }
    for(const auto& Pair:Value->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView());auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_key"),Key);Row->SetField(TEXT("requested_value"),Pair.Value);Row->SetBoolField(TEXT("default_prepared"),false);Rows.Add(MakeShared<FJsonValueObject>(Row));Adapter->SetArrayField(TEXT("parameter_adaptation"),Rows);
        if(!Pair.Value.IsValid()) { return Fail(R,TEXT("function_node_input_invalid"),TEXT("Invalid parameter JSON value.")); }
        if(!Key.Equals(TEXT("self"),ESearchCase::IgnoreCase)) { Row->SetStringField(TEXT("disposition"),TEXT("ignored_no_input_pin"));continue; }
        if(Pair.Value->Type==EJson::Number||Pair.Value->Type==EJson::Boolean)
        { Row->SetStringField(TEXT("disposition"),TEXT("raw_object_default_bypass_unsupported"));return Fail(R,TEXT("function_node_parameter_unsupported"),TEXT("Original numeric/boolean object-self DefaultValue bypass is outside this finite adapter; no core writer dispatched.")); }
        Row->SetStringField(TEXT("disposition"),TEXT("ignored_original_object_pin_value_kind"));
    }
    return true;
}

bool Defaults(const TSharedPtr<FJsonValue>& Value, const TSharedRef<FJsonObject>& Out,
    const TSharedRef<FJsonObject>& Adapter, FResult& R)
{
    Adapter->SetField(TEXT("requested_params"), Value); Adapter->SetBoolField(TEXT("nonobject_params_ignored"), Value->Type != EJson::Object && Value->Type != EJson::Null);
    TArray<TSharedPtr<FJsonValue>> Rows; Adapter->SetArrayField(TEXT("parameter_adaptation"), Rows);
    if (Value->Type != EJson::Object) { return true; } // Original TryGetObjectField ignores other whole-value kinds.
    if (Value->AsObject()->Values.Num() > 64)
    { return Fail(R, TEXT("function_node_parameter_budget"), TEXT("At most 64 parameter keys are admitted.")); }
    bool PlayerSeen = false;
    for (const auto& Pair : Value->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView()); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("requested_key"), Key);
        Row->SetField(TEXT("requested_value"), Pair.Value); Row->SetBoolField(TEXT("default_prepared"), false);
        Rows.Add(MakeShared<FJsonValueObject>(Row)); Adapter->SetArrayField(TEXT("parameter_adaptation"), Rows);
        if (Key.Equals(TEXT("WorldContextObject"), ESearchCase::IgnoreCase) && Pair.Value.IsValid() && Pair.Value->Type != EJson::Null)
        { Row->SetStringField(TEXT("disposition"), TEXT("explicit_world_context_unsupported"));
            return Fail(R, TEXT("function_node_world_context_unsupported"), TEXT("This route uses the Blueprint's compiler-provided world context; non-null explicit context defaults are outside its finite admission.")); }
        if (!Key.Equals(TEXT("PlayerIndex"), ESearchCase::IgnoreCase))
        { Row->SetStringField(TEXT("disposition"), TEXT("ignored_no_supported_input_pin")); continue; }
        if (PlayerSeen)
        { Row->SetStringField(TEXT("disposition"), TEXT("case_equivalent_duplicate_unsupported"));
            return Fail(R, TEXT("function_node_duplicate_parameter_unsupported"), TEXT("Multiple case-equivalent PlayerIndex keys are outside this slice; original iteration-order overwrites remain qualified.")); }
        PlayerSeen = true;
        if (!Pair.Value.IsValid()) { return Fail(R, TEXT("function_node_input_invalid"), TEXT("Invalid parameter JSON value.")); }
        if (Pair.Value->Type == EJson::Null || Pair.Value->Type == EJson::Array || Pair.Value->Type == EJson::Object)
        { Row->SetStringField(TEXT("disposition"), TEXT("ignored_original_int_pin_value_kind")); continue; }
        if (Pair.Value->Type != EJson::String && Pair.Value->Type != EJson::Number)
        { Row->SetStringField(TEXT("disposition"), TEXT("boolean_raw_int_default_unsupported"));
            return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original boolean-to-int raw pin text bypasses schema validation and remains outside this slice.")); }
        double N = 0; const bool Parsed = Pair.Value->TryGetNumber(N); if (!Parsed) { N = 0; }
        Row->SetBoolField(TEXT("numeric_conversion_succeeded"), Parsed); Row->SetBoolField(TEXT("original_as_number_zero_fallback"), !Parsed);
        if (!FMath::IsFinite(N)) { return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("PlayerIndex numeric conversion must be finite.")); }
        const double Rounded = FMath::FloorToDouble(N + 0.5);
        if (!FMath::IsFinite(Rounded) || Rounded < MIN_int32 || Rounded > MAX_int32)
        { return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original rounded PlayerIndex must fit signed-int32 without overflow narrowing.")); }
        const int32 Index = static_cast<int32>(FMath::RoundToInt(N));
        Out->SetNumberField(TEXT("PlayerIndex"), Index); Row->SetNumberField(TEXT("prepared_value"), Index);
        Row->SetStringField(TEXT("disposition"), TEXT("prepared_original_round_to_int")); Row->SetBoolField(TEXT("default_prepared"), true);
    }
    return true;
}

// KB is a separate original value-kind adapter, not an alias of checked defaults.
// In particular, BoneName strings, short Vector arrays and nonobject params ignore.
const TCHAR* PhysicsName(const FString& Name)
{
    for (const TCHAR* Candidate : {TEXT("AddForce"), TEXT("AddImpulse"), TEXT("AddTorqueInRadians"), TEXT("SetMassScale")})
    { if (Name.Equals(Candidate, ESearchCase::IgnoreCase)) { return Candidate; } }
    return nullptr;
}
bool PhysicsDefaults(const TSharedPtr<FJsonValue>& Value, const FString& FunctionName,
    const TSharedRef<FJsonObject>& Out, const TSharedRef<FJsonObject>& Adapter, FResult& R)
{
    Adapter->SetField(TEXT("requested_params"), Value);
    Adapter->SetBoolField(TEXT("nonobject_params_ignored"), Value->Type != EJson::Object && Value->Type != EJson::Null);
    TArray<TSharedPtr<FJsonValue>> Rows; Adapter->SetArrayField(TEXT("parameter_adaptation"), Rows);
    if (Value->Type != EJson::Object) { return true; }
    if (Value->AsObject()->Values.Num() > 64)
    { return Fail(R, TEXT("function_node_parameter_budget"), TEXT("At most 64 parameter keys are admitted.")); }
    const FString VectorName = FunctionName == TEXT("AddForce") ? TEXT("Force") : FunctionName == TEXT("AddImpulse") ? TEXT("Impulse")
        : FunctionName == TEXT("AddTorqueInRadians") ? TEXT("Torque") : FString();
    const FString BoolName = FunctionName == TEXT("AddImpulse") ? TEXT("bVelChange") : FunctionName == TEXT("SetMassScale") ? FString() : TEXT("bAccelChange");
    TSet<FName> Prepared;
    for (const auto& Pair : Value->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView()); const auto V = Pair.Value; auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("requested_key"), Key); Row->SetField(TEXT("requested_value"), V);
        Row->SetBoolField(TEXT("default_prepared"), false); Rows.Add(MakeShared<FJsonValueObject>(Row)); Adapter->SetArrayField(TEXT("parameter_adaptation"), Rows);
        if (!V) { return Fail(R, TEXT("function_node_input_invalid"), TEXT("Invalid parameter JSON value.")); }
        const bool Vector = !VectorName.IsEmpty() && Key.Equals(VectorName, ESearchCase::IgnoreCase);
        const bool Flag = !BoolName.IsEmpty() && Key.Equals(BoolName, ESearchCase::IgnoreCase);
        const bool Bone = Key.Equals(TEXT("BoneName"), ESearchCase::IgnoreCase);
        const bool Mass = FunctionName == TEXT("SetMassScale") && Key.Equals(TEXT("InMassScale"), ESearchCase::IgnoreCase);
        const bool Physical = Key.Equals(TEXT("self"), ESearchCase::IgnoreCase) || Key.Equals(TEXT("execute"), ESearchCase::IgnoreCase);
        if (!Vector && !Flag && !Bone && !Mass && !Physical)
        { Row->SetStringField(TEXT("disposition"), TEXT("ignored_no_input_pin")); continue; }
        if (V->Type == EJson::Null || V->Type == EJson::Object)
        { Row->SetStringField(TEXT("disposition"), TEXT("ignored_original_value_kind")); continue; }
        FString Canonical = Vector ? VectorName : Flag ? BoolName : Bone ? TEXT("BoneName") : Mass ? TEXT("InMassScale") : Key;
        TSharedPtr<FJsonValue> Converted;
        if (Vector && V->Type == EJson::Array)
        {
            const auto& A = V->AsArray();
            if (A.Num() != 3) { Row->SetStringField(TEXT("disposition"), TEXT("ignored_original_vector_array_length")); continue; }
            float XYZ[3]; TArray<TSharedPtr<FJsonValue>> Conversions;
            for (int32 I = 0; I < 3; ++I)
            {
                double Number = 0; const bool Parsed = A[I] && A[I]->TryGetNumber(Number); if (!Parsed) { Number = 0; }
                if (!FMath::IsFinite(Number) || FMath::Abs(Number) > MAX_flt)
                { Row->SetStringField(TEXT("disposition"), TEXT("nonfinite_float32_projection_unsupported")); return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original Vector projection must remain finite float32; no core writer dispatched.")); }
                XYZ[I] = static_cast<float>(Number); auto Element = MakeShared<FJsonObject>();
                Element->SetBoolField(TEXT("numeric_conversion_succeeded"), Parsed); Element->SetBoolField(TEXT("original_as_number_zero_fallback"), !Parsed);
                Element->SetNumberField(TEXT("float32_value"), static_cast<double>(XYZ[I])); Conversions.Add(MakeShared<FJsonValueObject>(Element));
            }
            const FString Literal = FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), XYZ[0], XYZ[1], XYZ[2]);
            Row->SetArrayField(TEXT("vector_element_adaptation"), Conversions); Row->SetStringField(TEXT("original_literal_text"), Literal);
            Row->SetStringField(TEXT("disposition"), TEXT("prepared_original_float32_vector_six_decimal_literal")); Converted = MakeShared<FJsonValueString>(Literal);
        }
        else if (Flag && (V->Type == EJson::Boolean || V->Type == EJson::String))
        {
            const bool Bool = V->AsBool(); Converted = MakeShared<FJsonValueBoolean>(Bool);
            Row->SetStringField(TEXT("original_literal_text"), Bool ? TEXT("true") : TEXT("false"));
            Row->SetStringField(TEXT("disposition"), V->Type == EJson::String ? TEXT("prepared_original_string_as_bool") : TEXT("prepared_original_bool"));
        }
        else if (Bone && (V->Type == EJson::Number || V->Type == EJson::Boolean))
        {
            FString Literal;
            if (V->Type == EJson::Boolean) { Literal = V->AsBool() ? TEXT("true") : TEXT("false"); }
            else
            {
                const double Number = V->AsNumber();
                if (!FMath::IsFinite(Number) || FMath::Abs(Number) > MAX_flt)
                { return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original numeric name projection must remain finite float32; no writer dispatched.")); }
                Literal = FString::SanitizeFloat(static_cast<float>(Number));
            }
            Converted = MakeShared<FJsonValueString>(Literal); Row->SetStringField(TEXT("original_literal_text"), Literal);
            Row->SetStringField(TEXT("disposition"), TEXT("prepared_original_numeric_or_bool_name_literal"));
        }
        else if (Mass && V->Type == EJson::Number)
        {
            const double Number = V->AsNumber();
            if (!FMath::IsFinite(Number) || FMath::Abs(Number) > MAX_flt)
            { return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original mass projection must remain finite float32; no writer dispatched.")); }
            const float OriginalFloat = static_cast<float>(Number); const FString Literal = FString::SanitizeFloat(OriginalFloat);
            const double ParsedLiteral = FCString::Atod(*Literal);
            if (!FMath::IsFinite(ParsedLiteral) || FMath::Abs(ParsedLiteral) > 9007199254740991.0)
            { return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original mass literal exceeds the reused checked JSON numeric bound; no writer dispatched.")); }
            Converted = MakeShared<FJsonValueNumber>(ParsedLiteral); Row->SetNumberField(TEXT("original_float32_value"), static_cast<double>(OriginalFloat));
            Row->SetStringField(TEXT("original_literal_text"), Literal); Row->SetNumberField(TEXT("prepared_numeric_value"), ParsedLiteral);
            Row->SetBoolField(TEXT("literal_text_identity_promised"), false);
            Row->SetStringField(TEXT("disposition"), TEXT("prepared_original_six_decimal_mass_value_with_checked_literal_canonicalization"));
        }
        else if (V->Type == EJson::Number || V->Type == EJson::Boolean)
        { Row->SetStringField(TEXT("disposition"), TEXT("original_raw_default_bypass_unsupported")); return Fail(R, TEXT("function_node_parameter_unsupported"), TEXT("Original numeric/bool writes to Vector, bool-numeric, float-bool, self or execute raw DefaultValue remain outside this finite checked adapter; no core writer dispatched.")); }
        else
        { Row->SetStringField(TEXT("disposition"), TEXT("ignored_original_value_kind")); continue; }
        const FName Name(*Canonical);
        if (Prepared.Contains(Name))
        { Row->SetStringField(TEXT("disposition"), TEXT("case_equivalent_prepared_duplicate_unsupported")); return Fail(R, TEXT("function_node_duplicate_parameter_unsupported"), TEXT("Multiple actual writes to the same case-equivalent pin remain outside this finite adapter; no core writer dispatched.")); }
        Prepared.Add(Name); Out->SetField(Canonical, Converted); Row->SetStringField(TEXT("canonical_pin"), Canonical);
        Row->SetField(TEXT("prepared_value"), Converted); Row->SetBoolField(TEXT("default_prepared"), true);
    }
    return true;
}

// A bounded selected-scope value observation only for the new physics adapter's
// successful core return and its outer-transaction destruction boundary.
// Failed/unknown cores are never probed. Strong holds outlive that destructor.
bool PhysicsEndpoint(UBlueprint* BP, UEdGraph* Selected, UFunction* Function, const TSharedRef<FJsonObject>& Out,
    TArray<UObject*>& Objects, TArray<UEdGraphPin*>& Pins, TArray<TStrongObjectPtr<UObject>>* Holds)
{
    auto Object = [&](UObject* O) -> bool
    {
        if (!IsValid(O) || O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed)) { return false; }
        Objects.Add(O);
        if (Holds)
        {
            bool Found = false; for (const auto& H : *Holds) { if (H.Get() == O) { Found = true; break; } }
            if (!Found) { Holds->Emplace(O); }
        }
        return true;
    };
    if (!Object(BP) || !Object(Selected) || !Object(Function) || !Object(BP->GetClass()) || !Object(BP->GetPackage())
        || !Object(BP->ParentClass.Get()) || !Object(Function->GetOuterUClass()) || !Object(Function->GetPackage())) { return false; }
    for (UClass* Class : {BP->GeneratedClass.Get(), BP->SkeletonGeneratedClass.Get()}) { if (Class && !Object(Class)) { return false; } }
    if (Selected->GetOutermost() != BP->GetOutermost() || Selected->GetTypedOuter<UBlueprint>() != BP || Selected->Nodes.Num() > 2048) { return false; }
    auto Identity = [&](UObject* O) -> TSharedRef<FJsonObject>
    {
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O->GetPathName()); J->SetStringField(TEXT("class"), O->GetClass()->GetPathName());
        J->SetStringField(TEXT("outer"), O->GetOuter() ? O->GetOuter()->GetPathName() : FString());
        J->SetStringField(TEXT("flags"), LexToString(static_cast<uint32>(O->GetFlags()))); return J;
    };
    Out->SetObjectField(TEXT("blueprint"), Identity(BP)); Out->SetBoolField(TEXT("dirty"), BP->GetPackage()->IsDirty());
    Out->SetNumberField(TEXT("status"), static_cast<int32>(BP->Status.GetValue()));
    Out->SetObjectField(TEXT("source_function"), Identity(Function)); Out->SetNumberField(TEXT("function_flags"), static_cast<uint32>(Function->FunctionFlags));
    UClass* Owner = Function->GetOuterUClass(); UObject* NativeCDO = Owner->GetDefaultObject(false);
    if (!Object(Owner->GetSuperClass()) || !Object(NativeCDO)) { return false; }
    auto OwnerRow = Identity(Owner); OwnerRow->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint32>(Owner->GetClassFlags())));
    OwnerRow->SetStringField(TEXT("super"), Owner->GetSuperClass()->GetPathName()); OwnerRow->SetStringField(TEXT("package_flags"), LexToString(static_cast<uint32>(Owner->GetPackage()->GetPackageFlags())));
    OwnerRow->SetObjectField(TEXT("existing_cdo"), Identity(NativeCDO)); Out->SetObjectField(TEXT("source_owner"), OwnerRow);
    Out->SetStringField(TEXT("target_package_flags"), LexToString(static_cast<uint32>(BP->GetPackage()->GetPackageFlags())));
    auto Metadata = MakeShared<FJsonObject>();
    for (const TCHAR* Key : {TEXT("Category"), TEXT("UnsafeDuringActorConstruction"), TEXT("WorldContext"), TEXT("AutoCreateRefTerm"), TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("BlueprintInternalUseOnly"), TEXT("CustomThunk"), TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("ScriptName"), TEXT("ScriptMethod")})
    { Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key)); Metadata->SetStringField(Key, Function->GetMetaData(Key)); }
    Out->SetObjectField(TEXT("source_metadata"), Metadata);
    TArray<TSharedPtr<FJsonValue>> Parameters; int32 ParameterCount = 0;
    for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm)) { continue; } if (++ParameterCount > 4) { return false; }
        auto P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), It->GetName()); P->SetStringField(TEXT("field_class"), It->GetClass()->GetName());
        P->SetStringField(TEXT("cpp_type"), It->GetCPPType()); P->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(It->GetPropertyFlags())));
        const FName DefaultKey(*(TEXT("CPP_Default_") + It->GetName())); P->SetBoolField(TEXT("default_present"), Function->HasMetaData(DefaultKey));
        P->SetStringField(TEXT("default"), Function->GetMetaData(DefaultKey)); Parameters.Add(MakeShared<FJsonValueObject>(P));
    }
    Out->SetArrayField(TEXT("source_parameters"), Parameters);
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); if (Graphs.Num() > 256 || !Graphs.Contains(Selected)) { return false; }
    TArray<TSharedPtr<FJsonValue>> GraphRows;
    for (UEdGraph* G : Graphs)
    {
        if (!Object(G) || !Object(G->GetClass()) || !G->GraphGuid.IsValid()) { return false; }
        auto Row = Identity(G); Row->SetStringField(TEXT("guid"), G->GraphGuid.ToString(EGuidFormats::Digits)); Row->SetNumberField(TEXT("node_count"), G->Nodes.Num()); GraphRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("graphs"), GraphRows);
    TArray<TSharedPtr<FJsonValue>> Variables; if (BP->NewVariables.Num() > 2048) { return false; }
    for (const FBPVariableDescription& V : BP->NewVariables)
    { FString Value; FBPVariableDescription::StaticStruct()->ExportText(Value, &V, nullptr, nullptr, PPF_None, nullptr); if (Value.Len() > 65536) { return false; } Variables.Add(MakeShared<FJsonValueString>(Value)); }
    Out->SetArrayField(TEXT("variables"), Variables);
    TSet<UEdGraphPin*> Inventory;
    for (const auto& Entry : Selected->Nodes)
    {
        UEdGraphNode* N = Entry.Get(); if (!Object(N) || !Object(N->GetClass()) || N->GetGraph() != Selected || N->Pins.Num() > 256) { return false; }
        for (auto* P : N->Pins)
        {
            if (!P || Inventory.Contains(P) || P->WasTrashed() || P->GetOwningNodeUnchecked() != N) { return false; }
            Inventory.Add(P); Pins.Add(P); if (Pins.Num() > 32768) { return false; }
            for (UObject* Ref : {P->PinType.PinSubCategoryObject.Get(), P->PinType.PinValueType.TerminalSubCategoryObject.Get(), P->DefaultObject.Get()})
            { if (Ref && !Object(Ref)) { return false; } }
        }
    }
    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (const auto& Entry : Selected->Nodes)
    {
        UEdGraphNode* N = Entry.Get(); auto Node = Identity(N); Node->SetStringField(TEXT("guid"), N->NodeGuid.ToString(EGuidFormats::Digits));
        Node->SetNumberField(TEXT("x"), N->NodePosX); Node->SetNumberField(TEXT("y"), N->NodePosY);
        const FMemberReference* Member = nullptr;
        if (const auto* Call = Cast<UK2Node_CallFunction>(N)) { Member = &Call->FunctionReference; }
        else if (const auto* Variable = Cast<UK2Node_Variable>(N)) { Member = &Variable->VariableReference; }
        if (Member)
        {
            UClass* Parent = Member->GetMemberParentClass(); if (Parent && !Object(Parent)) { return false; }
            auto M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("name"), Member->GetMemberName().ToString()); M->SetStringField(TEXT("guid"), Member->GetMemberGuid().ToString(EGuidFormats::Digits));
            M->SetStringField(TEXT("parent"), Parent ? Parent->GetPathName() : FString()); M->SetStringField(TEXT("scope"), Member->GetMemberScopeName());
            M->SetBoolField(TEXT("self"), Member->IsSelfContext()); M->SetBoolField(TEXT("local"), Member->IsLocalScope()); M->SetBoolField(TEXT("deprecated"), Member->IsDeprecated()); Node->SetObjectField(TEXT("member_reference"), M);
        }
        TArray<TSharedPtr<FJsonValue>> PinRows;
        for (auto* P : N->Pins)
        {
            auto Pin = MakeShared<FJsonObject>(); Pin->SetStringField(TEXT("name"), P->PinName.ToString()); Pin->SetStringField(TEXT("id"), P->PinId.ToString(EGuidFormats::Digits));
            Pin->SetStringField(TEXT("persistent"), P->PersistentGuid.ToString(EGuidFormats::Digits)); Pin->SetNumberField(TEXT("direction"), P->Direction);
            FString Type; FEdGraphPinType::StaticStruct()->ExportText(Type, &P->PinType, nullptr, nullptr, PPF_None, nullptr);
            Pin->SetStringField(TEXT("type"), Type); Pin->SetStringField(TEXT("default"), P->DefaultValue); Pin->SetStringField(TEXT("autogenerated"), P->AutogeneratedDefaultValue);
            Pin->SetStringField(TEXT("text"), P->DefaultTextValue.ToString()); Pin->SetStringField(TEXT("object"), P->DefaultObject ? P->DefaultObject->GetPathName() : FString());
            Pin->SetBoolField(TEXT("hidden"), P->bHidden); Pin->SetBoolField(TEXT("ignored"), P->bDefaultValueIsIgnored); Pin->SetBoolField(TEXT("readonly"), P->bDefaultValueIsReadOnly);
            Pin->SetBoolField(TEXT("not_connectable"), P->bNotConnectable); Pin->SetBoolField(TEXT("orphan"), P->bOrphanedPin);
            if (P->ParentPin || !P->SubPins.IsEmpty()) { return false; }
            TArray<TSharedPtr<FJsonValue>> Links;
            for (auto* L : P->LinkedTo)
            { if (!Inventory.Contains(L) || L->WasTrashed() || !L->LinkedTo.Contains(P)) { return false; } Links.Add(MakeShared<FJsonValueString>(L->PinId.ToString(EGuidFormats::Digits))); }
            Pin->SetArrayField(TEXT("links"), Links); PinRows.Add(MakeShared<FJsonValueObject>(Pin));
        }
        Node->SetArrayField(TEXT("pins"), PinRows); Nodes.Add(MakeShared<FJsonValueObject>(Node));
    }
    Out->SetArrayField(TEXT("nodes"), Nodes); return true;
}

FResult PhysicsAdd(const FString& BlueprintPath, const FString& Target, const FString& RequestedFunction,
    const FString& ParamsJson, const FString& PositionJson, const FString& GraphName, const FString& GraphId,
    const FString& GraphType, bool CreateGraph, FResult R, const TSharedRef<FJsonObject>& Adapter)
{
    Adapter->SetStringField(TEXT("scope"), TEXT("KB original loose9 explicit PrimitiveComponent quartet only; original ignored value kinds and float32/six-decimal projections are adapted before one JZ checked writer. UPrimitiveComponent is a deliberate explicit-owner alias repair, not broad original inference. No inherited component self, graph creation, raw invalid-default bypass or runtime physics."));
    Adapter->SetBoolField(TEXT("explicit_U_prefix_owner_alias_repair"), Target == TEXT("UPrimitiveComponent"));
    Adapter->SetBoolField(TEXT("literal_text_full_original_parity"), false);
    const TCHAR* Canonical = Bounded(RequestedFunction, 128) ? PhysicsName(RequestedFunction) : nullptr;
    if (!Canonical) { Fail(R, TEXT("function_node_source_unsupported"), TEXT("Only AddForce/AddImpulse/AddTorqueInRadians/SetMassScale are admitted for this explicit PrimitiveComponent owner.")); return R; }
    if (CreateGraph) { Fail(R, TEXT("function_node_graph_creation_unsupported"), TEXT("Select an existing graph; create-before-validation effects remain outside this finite route.")); return R; }
    TSharedPtr<FJsonValue> Params, Coordinates; auto PreparedDefaults = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> PreparedPosition;
    if (!Parse(ParamsJson, Params, R) || !Parse(PositionJson, Coordinates, R)
        || !PhysicsDefaults(Params, Canonical, PreparedDefaults, Adapter, R) || !Position(Coordinates, PreparedPosition, Adapter, R)) { return R; }
    Adapter->SetStringField(TEXT("canonical_function"), Canonical); Adapter->SetObjectField(TEXT("prepared_defaults"), PreparedDefaults);
    if (!IsEditorGameThread() || IsGarbageCollecting() || IsEngineExitRequested() || GIsRequestingExit || UE::IsSavingPackage() || !IsValid(GEditor) || IsPlaySessionActive())
    { Fail(R, TEXT("function_node_editor_state_invalid"), TEXT("Safe Editor game thread outside PIE/GC/exit/save required; no transaction or core dispatch.")); return R; }
    TStrongObjectPtr<UEditorEngine> Editor(GEditor); TStrongObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());
    auto Context = [&]() -> bool
    {
        if (!IsEditorGameThread() || IsGarbageCollecting() || IsEngineExitRequested() || GIsRequestingExit || UE::IsSavingPackage()
            || !Editor.IsValid() || GEditor != Editor.Get() || !IsValid(GEditor) || !World.IsValid()) { return false; }
        if (Editor->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || World->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) || IsPlaySessionActive()) { return false; }
        return !World->bIsTearingDown && GEditor->GetEditorWorldContext().World() == World.Get();
    };
    if (!Context()) { Fail(R, TEXT("function_node_editor_state_invalid"), TEXT("Exact live Editor world required before transaction or writer.")); return R; }
    const auto* PriorUndo = GUndo;
    TStrongObjectPtr<UBlueprint> KeepBP; TStrongObjectPtr<UEdGraph> KeepGraph; TStrongObjectPtr<UFunction> KeepFunction;
    TArray<TStrongObjectPtr<UObject>> Holds; TArray<UObject*> BeforeObjects; TArray<UEdGraphPin*> BeforePins;
    auto BeforeEnd = MakeShared<FJsonObject>(); bool EndpointCaptured = false, CoreReturned = false;
    {
        Adapter->SetNumberField(TEXT("outer_transaction_calls"), 1); FScopedTransaction Transaction(NSLOCTEXT("Cubeless", "OriginalFunctionNode", "Add Blueprint Function Node"));
        if (!Context()) { R.bChangeStateKnown = false; Fail(R, TEXT("function_node_context_changed"), TEXT("Outer transaction callback changed Editor/world; no core writer dispatched.")); }
        else
        {
            Adapter->SetNumberField(TEXT("core_delegate_calls"), 1);
            R = Function::AddCall(BlueprintPath, TEXT("/Script/Engine.PrimitiveComponent"), Canonical, Text(PreparedDefaults), Text(PreparedPosition), GraphName, GraphId, GraphType, false, false, false, false, false, false);
            // Adopt the complete actual result before any object access or postcondition.
            CoreReturned = true; Adapter->SetNumberField(TEXT("core_delegate_returns"), 1);
            auto CoreReceipt = MakeShared<FJsonObject>(); CoreReceipt->SetBoolField(TEXT("success"), R.bSuccess); CoreReceipt->SetBoolField(TEXT("mutation_attempted"), R.bMutationAttempted);
            CoreReceipt->SetBoolField(TEXT("change_state_known"), R.bChangeStateKnown); if (R.bChangeStateKnown) { CoreReceipt->SetBoolField(TEXT("changed"), R.bChanged); } else { CoreReceipt->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); } CoreReceipt->SetBoolField(TEXT("postcondition_passed"), R.bPostconditionPassed);
            CoreReceipt->SetStringField(TEXT("stage"), R.Stage); CoreReceipt->SetStringField(TEXT("error_code"), R.ErrorCode); CoreReceipt->SetStringField(TEXT("error"), R.Error); CoreReceipt->SetObjectField(TEXT("data"), R.Data);
            Adapter->SetObjectField(TEXT("core_receipt_at_return"), CoreReceipt);
            auto Combined = MakeShared<FJsonObject>(); for (const auto& Pair : R.Data->Values) { Combined->SetField(FString(Pair.Key.ToView()), Pair.Value); }
            R.Data = Combined; R.Data->SetObjectField(TEXT("original_function_node_adapter"), Adapter);
            if (R.bSuccess && R.bChangeStateKnown && R.bPostconditionPassed && Context())
            {
                FString BPPath, FunctionPath; const TSharedPtr<FJsonObject>* GraphRow = nullptr; FString GraphPath;
                if (R.Data->TryGetStringField(TEXT("blueprint"), BPPath) && R.Data->TryGetStringField(TEXT("function_path"), FunctionPath)
                    && FunctionPath == FString(TEXT("/Script/Engine.PrimitiveComponent:")) + Canonical
                    && R.Data->TryGetObjectField(TEXT("graph"), GraphRow) && GraphRow && (*GraphRow)->TryGetStringField(TEXT("object_path"), GraphPath))
                {
                    KeepBP.Reset(FindObject<UBlueprint>(nullptr, *BPPath)); KeepGraph.Reset(FindObject<UEdGraph>(nullptr, *GraphPath)); KeepFunction.Reset(FindObject<UFunction>(nullptr, *FunctionPath));
                    EndpointCaptured = Context() && PhysicsEndpoint(KeepBP.Get(), KeepGraph.Get(), KeepFunction.Get(), BeforeEnd, BeforeObjects, BeforePins, &Holds) && Context();
                }
            }
        }
    } // Normal EndTransaction callbacks run while all captured UObject holds survive.
    Adapter->SetBoolField(TEXT("outer_transaction_scope_exited"), true);
    Adapter->SetBoolField(TEXT("core_returned"), CoreReturned); Adapter->SetBoolField(TEXT("post_transaction_observation_complete"), false);
    Adapter->SetStringField(TEXT("endpoint_observation_scope"), TEXT("Current Editor/world/GUndo and selected Blueprint/class/package/graph/variables/call-or-variable member references/physical pins/defaults/ordered links plus declared native function/owner metadata only; other node properties and graphs' bodies, full CDO contents, global callback/disk effects and runtime physics are unobserved."));
    if (!CoreReturned || !R.bSuccess || !R.bChangeStateKnown || !R.bPostconditionPassed)
    {
        // No engine/object follow-up after a failed or unknown core. Its complete
        // receipt remains nested; transaction callbacks make outer authority unknown.
        R.bChangeStateKnown = false; R.bSuccess = R.bPostconditionPassed = false;
        Adapter->SetBoolField(TEXT("failed_core_engine_followup_skipped"), true);
        if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("function_node_transaction_endpoint_unverified"), TEXT("No successful known core endpoint; outer transaction ended without follow-up engine observation. Core prefix retained; no retry.")); }
        return R;
    }
    if (!EndpointCaptured || !Context() || GUndo != PriorUndo)
    { R.bChangeStateKnown = false; Fail(R, TEXT("function_node_transaction_endpoint_unverified"), TEXT("Outer transaction ended with unavailable scope/context/GUndo; actual core prefix retained, no further SDK or retry.")); return R; }
    auto AfterEnd = MakeShared<FJsonObject>(); TArray<UObject*> AfterObjects; TArray<UEdGraphPin*> AfterPins;
    const bool Read = PhysicsEndpoint(KeepBP.Get(), KeepGraph.Get(), KeepFunction.Get(), AfterEnd, AfterObjects, AfterPins, nullptr);
    const bool Same = Read && Context() && GUndo == PriorUndo && BeforeObjects == AfterObjects && BeforePins == AfterPins && Text(BeforeEnd) == Text(AfterEnd);
    Adapter->SetBoolField(TEXT("post_transaction_observation_complete"), Read); Adapter->SetBoolField(TEXT("post_transaction_selected_scope_preserved"), Same);
    if (!Same)
    {
        Adapter->SetObjectField(TEXT("scope_before_transaction_end"), BeforeEnd); Adapter->SetObjectField(TEXT("scope_after_transaction_end"), AfterEnd);
        R.bChangeStateKnown = false; Fail(R, TEXT("function_node_transaction_endpoint_changed"), TEXT("Selected source/Blueprint/graph/pin/default state changed or became unavailable across EndTransaction; original core prefix retained and no retry or rollback attempted.")); return R;
    }
    const TSharedPtr<FJsonObject>* Applied = nullptr; bool Verified = R.Data->TryGetObjectField(TEXT("param_defaults_applied"), Applied) && Applied && Applied->IsValid();
    for (const auto& Value : Adapter->GetArrayField(TEXT("parameter_adaptation")))
    {
        auto Row = Value->AsObject(); bool Prepared = false; if (!Row->TryGetBoolField(TEXT("default_prepared"), Prepared) || !Prepared) { continue; }
        FString Pin, Actual; bool Found = Row->TryGetStringField(TEXT("canonical_pin"), Pin) && Verified && (*Applied)->TryGetStringField(Pin, Actual);
        Row->SetBoolField(TEXT("actual_core_default_observed"), Found); if (!Found) { Verified = false; continue; }
        Row->SetStringField(TEXT("actual_pin_literal_text"), Actual); FString Original; if (Row->TryGetStringField(TEXT("original_literal_text"), Original)) { Row->SetBoolField(TEXT("literal_text_matches_original"), Actual == Original); }
        if (Pin == TEXT("InMassScale"))
        {
            const bool SameFloat = static_cast<float>(FCString::Atod(*Actual)) == static_cast<float>(Row->GetNumberField(TEXT("prepared_numeric_value")));
            Row->SetBoolField(TEXT("parsed_float_matches_original_literal"), SameFloat); Verified &= SameFloat;
        }
        else { Verified &= Actual == Row->GetStringField(TEXT("original_literal_text")); }
    }
    Adapter->SetBoolField(TEXT("postcondition_verified"), Verified);
    if (!Verified) { Fail(R, TEXT("function_node_adapter_postcondition_failed"), TEXT("Actual checked defaults differ from the prepared original value-kind contract; complete known core and EndTransaction scope retained without retry.")); }
    return R;
}

// MW adds only original self/empty-target adaptation. All function admission,
// creation, own GUID/signature and authored-pin checks stay in the shared core.
struct FOwnEndpoint
{
    struct FObject { UObject* Pointer; UClass* Class; UObject* Outer; FName Name; EObjectFlags Flags; };
    struct FNode { UEdGraphNode* Pointer; UEdGraph* Graph; FGuid Guid; int32 X, Y; TArray<UEdGraphPin*> Pins; uint8 Kind; FMemberReference Reference; bool Deprecated;
        bool SelectedEvent=false;FName EventName;uint32 EventFlags=0;bool CallInEditor=false,Override=false,EventDeprecated=false,Internal=false;FMemberReference EventReference;TArray<FUserPinInfo> EventPins; };
    struct FPin
    {
        UEdGraphPin* Pointer; UEdGraphNode* Node; FGuid Id, Persistent; FName Name;
        FEdGraphPinType Type; EEdGraphPinDirection Direction; FString Default, Auto, Tooltip;
        FText Text, Friendly; TObjectPtr<UObject> Object; UEdGraphPin* Parent;
        TArray<UEdGraphPin*> Children, Links; uint32 Flags; int32 SourceIndex;
    };
    TArray<FObject> Objects; TArray<FNode> Nodes; TArray<FPin> Pins; TFunction<bool()> Fence;
    TArray<UEdGraph*> Graphs; TArray<TStrongObjectPtr<UObject>> Holds;
    TArray<TObjectPtr<UEdGraph>> OwnFunctions, OwnEvents, OwnMacros;
    struct FGraph { UEdGraph* Pointer; FGuid Guid; TObjectPtr<UClass> Schema; TArray<TObjectPtr<UEdGraph>> Children; bool Editable; };
    TArray<FGraph> GraphRoles;
    UBlueprint* Blueprint = nullptr; UPackage* Package = nullptr;
    TObjectPtr<UClass> Parent, Generated, Skeleton;
    EBlueprintStatus Status = BS_Unknown; uint32 PackageFlags = 0; bool Dirty = false;bool SelectedEventScope=false;
    static bool Loaded(const UObject* O)
    { return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
    static uint32 PinFlags(const UEdGraphPin* P)
    {
        return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)
            |(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)
            |(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)
            |(uint32(P->ShouldSavePinIfOrphaned())<<9)|(uint32(P->bUseBackwardsCompatForEmptyAutogeneratedValue)<<10);
    }
    bool Hold(UObject* O)
    {
        if (!Loaded(O)) return false;
        for (const auto& V:Objects) if (V.Pointer==O) return true;
        Holds.Emplace(O); Objects.Add({O,O->GetClass(),O->GetOuter(),O->GetFName(),O->GetFlags()}); return true;
    }
    bool Capture(UBlueprint* B,TFunction<bool()> InFence,FName SelectedEventName=NAME_None)
    {
        Fence=MoveTemp(InFence); if (!Fence||!Fence()) return false;
        Blueprint=B; if (!Hold(B) || !Hold(B->GetClass()) || !Hold(B->GetPackage())) return false;
        Package=B->GetPackage(); Status=B->Status; PackageFlags=uint32(Package->GetPackageFlags()); Dirty=Package->IsDirty();
        if (!B->ParentClass.GetGCPtr().IsResolved() || !B->GeneratedClass.GetGCPtr().IsResolved() || !B->SkeletonGeneratedClass.GetGCPtr().IsResolved()) return false;
        Parent=B->ParentClass.GetGCPtr(); Generated=B->GeneratedClass.GetGCPtr(); Skeleton=B->SkeletonGeneratedClass.GetGCPtr();
        if (!Hold(Parent.Get()) || !Hold(Generated.Get()) || !Hold(Skeleton.Get())) return false;
        OwnFunctions=B->FunctionGraphs; OwnEvents=B->UbergraphPages; OwnMacros=B->MacroGraphs;
        B->GetAllGraphs(Graphs);
        if (!Loaded(B) || B->ParentClass.GetGCPtr()!=Parent || B->GeneratedClass.GetGCPtr()!=Generated || B->SkeletonGeneratedClass.GetGCPtr()!=Skeleton
            || B->FunctionGraphs!=OwnFunctions || B->UbergraphPages!=OwnEvents || B->MacroGraphs!=OwnMacros || Graphs.Num()>256) return false;
        TSet<UEdGraphPin*> Inventory;
        for (auto* G:Graphs)
        {
            if (!Hold(G) || G->GetTypedOuter<UBlueprint>()!=B || !G->GraphGuid.IsValid() || !G->Schema.GetGCPtr().IsResolved() || !Hold(G->Schema.GetGCPtr()) || G->Nodes.Num()>2048) return false;
            GraphRoles.Add({G,G->GraphGuid,G->Schema.GetGCPtr(),G->SubGraphs,bool(G->bEditable)});
            for (const auto& H:G->Nodes)
            {
                if (!H.IsResolved()) return false;
                UEdGraphNode* N=H.Get(); if (!Hold(N) || !Hold(N->GetClass()) || N->GetOuter()!=G || N->Pins.Num()>512) return false;
                FNode V; V.Pointer=N; V.Graph=G; V.Guid=N->NodeGuid; V.X=N->NodePosX; V.Y=N->NodePosY; V.Pins=N->Pins; V.Kind=0; V.Deprecated=false;
                if (const auto* C=Cast<UK2Node_CallFunction>(N)) { V.Kind=1; V.Reference=C->FunctionReference; }
                else if (const auto* Variable=Cast<UK2Node_Variable>(N)) { V.Kind=2; V.Reference=Variable->VariableReference; }
                if (V.Kind) V.Deprecated=V.Reference.IsDeprecated();
                if(!SelectedEventName.IsNone()&&N->GetClass()==UK2Node_CustomEvent::StaticClass())
                {
                    const auto* E=static_cast<UK2Node_CustomEvent*>(N);
                    if(E->CustomFunctionName==SelectedEventName)
                    {
                        SelectedEventScope=V.SelectedEvent=true;V.EventName=E->CustomFunctionName;V.EventFlags=E->FunctionFlags;V.CallInEditor=E->bCallInEditor;
                        V.Override=E->bOverrideFunction;V.EventDeprecated=E->bIsDeprecated;V.Internal=E->bInternalEvent;V.EventReference=E->EventReference;
                        if(E->UserDefinedPins.Num()>128)return false;for(const auto& U:E->UserDefinedPins){if(!U)return false;V.EventPins.Add(*U);}
                    }
                }
                Nodes.Add(MoveTemp(V));
                for (auto* P:N->Pins)
                { if (!P || Inventory.Contains(P) || P->WasTrashed() || P->GetOwningNodeUnchecked()!=N) return false; Inventory.Add(P); }
                if (Inventory.Num()>32768) return false;
            }
        }
        // A copied reference may resolve its parent. No copied pin is read until
        // every such read has returned and physical membership has been reclosed.
        for (const auto& V:Nodes) if (V.Kind)
        {
            UClass* C=V.Reference.GetMemberParentClass(); if (C&&!Hold(C)) return false;
            if (!Fence()||!Membership()) return false;
        }
        if (!Membership()) return false;
        for (const auto& V:Nodes) for (auto* P:V.Pins)
        {
            if (!P->DefaultObject.IsResolved()) return false;
            UObject* O=P->DefaultObject.Get(); if (O&&!Hold(O)) return false;
            const auto& M=P->PinType.PinSubCategoryMemberReference.MemberParent; if (!M.IsResolved()) return false; if (UObject* C=M.Get()) if (!Hold(C)) return false;
            for (const auto& W:{P->PinType.PinSubCategoryObject,P->PinType.PinValueType.TerminalSubCategoryObject})
            { if (!W.IsExplicitlyNull()&&!W.IsValid()) return false; if (UObject* T=W.Get()) if (!Hold(T)) return false; }
            if (P->ParentPin&&!Inventory.Contains(P->ParentPin)) return false;
            for (auto* C:P->SubPins) if (!Inventory.Contains(C)) return false;
            TSet<UEdGraphPin*> Unique;
            for (auto* L:P->LinkedTo)
            { if (!Inventory.Contains(L) || Unique.Contains(L) || L->WasTrashed() || !L->LinkedTo.Contains(P)) return false; Unique.Add(L); }
            FPin Q; Q.Pointer=P; Q.Node=V.Pointer; Q.Id=P->PinId; Q.Persistent=P->PersistentGuid; Q.Name=P->PinName; Q.Type=P->PinType; Q.Direction=P->Direction;
            Q.Default=P->DefaultValue; Q.Auto=P->AutogeneratedDefaultValue; Q.Tooltip=P->PinToolTip; Q.Text=P->DefaultTextValue; Q.Friendly=P->PinFriendlyName;
            Q.Object=P->DefaultObject; Q.Parent=P->ParentPin; Q.Children=P->SubPins; Q.Links=P->LinkedTo; Q.Flags=PinFlags(P); Q.SourceIndex=P->SourceIndex; Pins.Add(MoveTemp(Q));
        }
        return Current();
    }
    bool Membership() const
    {
        for (const auto& V:Objects)
            if (!Loaded(V.Pointer)||V.Pointer->GetClass()!=V.Class||V.Pointer->GetOuter()!=V.Outer||!V.Pointer->GetFName().ToString().Equals(V.Name.ToString(),ESearchCase::CaseSensitive)||V.Pointer->GetFlags()!=V.Flags) return false;
        for (const auto& V:GraphRoles)
            if (!Loaded(V.Pointer)||V.Pointer->GraphGuid!=V.Guid||!V.Pointer->Schema.GetGCPtr().IsResolved()||V.Pointer->Schema.GetGCPtr()!=V.Schema||V.Pointer->SubGraphs!=V.Children||V.Pointer->bEditable!=V.Editable) return false;
        auto* B=Blueprint;
        if (!Loaded(B)||!B->ParentClass.GetGCPtr().IsResolved()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()
            ||B->ParentClass.GetGCPtr()!=Parent||B->GeneratedClass.GetGCPtr()!=Generated||B->SkeletonGeneratedClass.GetGCPtr()!=Skeleton
            ||B->FunctionGraphs!=OwnFunctions||B->UbergraphPages!=OwnEvents||B->MacroGraphs!=OwnMacros
            ||B->Status!=Status||B->GetPackage()!=Package||uint32(Package->GetPackageFlags())!=PackageFlags||Package->IsDirty()!=Dirty
            ||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad) return false;
        int32 Index=0;
        for (auto* G:Graphs)
        {
            if (!Loaded(G)) return false;
            for (const auto& H:G->Nodes)
            {
                if (Index>=Nodes.Num()||!H.IsResolved()||H.Get()!=Nodes[Index].Pointer||Nodes[Index].Graph!=G) return false;
                const auto& V=Nodes[Index++];if (!Loaded(V.Pointer)||V.Pointer->Pins!=V.Pins) return false;
            }
        }
        return Index==Nodes.Num();
    }
    bool Current() const
    {
        if (!Fence||!Fence()) return false;
        // Member-reference comparisons finish before the final membership
        // barrier; the physical-pin comparison itself performs no lookup/query.
        for (const auto& V:Objects)
            if (!Loaded(V.Pointer) || V.Pointer->GetClass()!=V.Class || V.Pointer->GetOuter()!=V.Outer || !V.Pointer->GetFName().ToString().Equals(V.Name.ToString(),ESearchCase::CaseSensitive) || V.Pointer->GetFlags()!=V.Flags) return false;
        auto* B=Blueprint;
        if (!B || B->GetPackage()!=Package || !B->ParentClass.GetGCPtr().IsResolved() || !B->GeneratedClass.GetGCPtr().IsResolved() || !B->SkeletonGeneratedClass.GetGCPtr().IsResolved() || B->ParentClass.GetGCPtr()!=Parent || B->GeneratedClass.GetGCPtr()!=Generated || B->SkeletonGeneratedClass.GetGCPtr()!=Skeleton
            || B->FunctionGraphs!=OwnFunctions || B->UbergraphPages!=OwnEvents || B->MacroGraphs!=OwnMacros
            || B->Status!=Status || uint32(Package->GetPackageFlags())!=PackageFlags || Package->IsDirty()!=Dirty || B->bBeingCompiled || B->bQueuedForCompilation || B->bIsRegeneratingOnLoad) return false;
        for (const auto& V:GraphRoles)
            if (V.Pointer->GraphGuid!=V.Guid || !V.Pointer->Schema.GetGCPtr().IsResolved() || V.Pointer->Schema.GetGCPtr()!=V.Schema || V.Pointer->SubGraphs!=V.Children || V.Pointer->bEditable!=V.Editable) return false;
        int32 Index=0;
        for (auto* G:Graphs)
        {
            if (!Loaded(G) || G->GetTypedOuter<UBlueprint>()!=B || G->Nodes.Num()>2048) return false;
            for (const auto& H:G->Nodes)
            {
                if (Index>=Nodes.Num() || !H.IsResolved() || H.Get()!=Nodes[Index].Pointer || Nodes[Index].Graph!=G) return false;
                const auto& V=Nodes[Index++]; const auto* N=V.Pointer;
                if (N->NodeGuid!=V.Guid || N->NodePosX!=V.X || N->NodePosY!=V.Y || N->Pins!=V.Pins) return false;

            }
        }
        if (Index!=Nodes.Num()||!Membership()) return false;
        // Compare references through held local rows, without retaining a mutable
        // graph array iterator across public member-reference access.
        for (const auto& V:Nodes)
        {
            const auto* N=V.Pointer;
            const FMemberReference* Ref=V.Kind==1?&static_cast<const UK2Node_CallFunction*>(N)->FunctionReference:V.Kind==2?&static_cast<const UK2Node_Variable*>(N)->VariableReference:nullptr;
            if (Ref&&(!Ref->IsSameReference(V.Reference)||Ref->IsDeprecated()!=V.Deprecated)) return false;
            if(V.SelectedEvent)
            {
                if(N->GetClass()!=UK2Node_CustomEvent::StaticClass())return false;const auto* E=static_cast<const UK2Node_CustomEvent*>(N);
                if(E->CustomFunctionName!=V.EventName||E->FunctionFlags!=V.EventFlags||E->bCallInEditor!=V.CallInEditor||E->bOverrideFunction!=V.Override
                    ||E->bIsDeprecated!=V.EventDeprecated||E->bInternalEvent!=V.Internal||!E->EventReference.IsSameReference(V.EventReference)||E->UserDefinedPins.Num()!=V.EventPins.Num())return false;
                for(int32 I=0;I<V.EventPins.Num();++I)
                {
                    const auto& U=E->UserDefinedPins[I];const auto& D=V.EventPins[I];if(!U||U->PinName!=D.PinName||U->DesiredPinDirection!=D.DesiredPinDirection
                        ||U->PinType!=D.PinType||U->PinType.bIsUObjectWrapper!=D.PinType.bIsUObjectWrapper||U->PinType.bSerializeAsSinglePrecisionFloat!=D.PinType.bSerializeAsSinglePrecisionFloat
                        ||U->PinType.PinValueType.bTerminalIsUObjectWrapper!=D.PinType.PinValueType.bTerminalIsUObjectWrapper||!U->PinDefaultValue.Equals(D.PinDefaultValue,ESearchCase::CaseSensitive))return false;
                }
            }
            if (!Fence()||!Membership()) return false;
        }
        if (!Membership()) return false;
        for (const auto& Q:Pins)
        {
            // Membership has been proven for every node's complete physical array first.
            const auto* P=Q.Pointer;
            if (P->WasTrashed() || !P->DefaultObject.IsResolved() || !P->PinType.PinSubCategoryMemberReference.MemberParent.IsResolved() || P->GetOwningNodeUnchecked()!=Q.Node || P->PinId!=Q.Id || P->PersistentGuid!=Q.Persistent || P->PinName!=Q.Name || P->PinType!=Q.Type
                || P->PinType.bIsUObjectWrapper!=Q.Type.bIsUObjectWrapper || P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Q.Type.PinValueType.bTerminalIsUObjectWrapper
                || P->PinType.bSerializeAsSinglePrecisionFloat!=Q.Type.bSerializeAsSinglePrecisionFloat || P->Direction!=Q.Direction
                || !P->DefaultValue.Equals(Q.Default,ESearchCase::CaseSensitive) || !P->AutogeneratedDefaultValue.Equals(Q.Auto,ESearchCase::CaseSensitive)
                || !P->PinToolTip.Equals(Q.Tooltip,ESearchCase::CaseSensitive)
                || (SelectedEventScope?(FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(Q.Text)||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(Q.Text)||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(Q.Friendly)||FTextInspector::GetFlags(P->PinFriendlyName)!=FTextInspector::GetFlags(Q.Friendly)):(!P->DefaultTextValue.IdenticalTo(Q.Text)||!P->PinFriendlyName.IdenticalTo(Q.Friendly)))
                || P->DefaultObject!=Q.Object || P->ParentPin!=Q.Parent || P->SubPins!=Q.Children || P->LinkedTo!=Q.Links || PinFlags(P)!=Q.Flags || P->SourceIndex!=Q.SourceIndex) return false;
        }
        return true;
    }
};

TSharedRef<FJsonObject> OwnSource(const TSharedRef<FJsonObject>& Data)
{
    auto J=MakeShared<FJsonObject>();
    for (const TCHAR* Key:{TEXT("requested_function_class"),TEXT("compiled_function_path"),TEXT("skeleton_function_path"),TEXT("compiled_function_flags_string"),TEXT("skeleton_function_flags_string"),TEXT("function_member_guid"),TEXT("function_graph_guid"),TEXT("callable_kind"),TEXT("compiled_signature_parameters"),TEXT("compiled_signature_parameter_count"),TEXT("compiled_signature_matches_skeleton"),TEXT("custom_event_node_path"),TEXT("custom_event_node_flags_string"),TEXT("own_custom_event_source"),TEXT("own_custom_event_rpc_authoring"),TEXT("own_custom_event_rpc_flags_match"),TEXT("authored_rpc_flags_string"),TEXT("rpc_execution_verified")})
    { const auto V=Data->TryGetField(Key); if (V.IsValid()) J->SetField(Key,V); }
    return J;
}
TSharedRef<FJsonObject> OwnReceipt(const FResult& Value)
{
    auto J=MakeShared<FJsonObject>(); J->SetBoolField(TEXT("success"),Value.bSuccess); J->SetBoolField(TEXT("mutation_attempted"),Value.bMutationAttempted);
    J->SetBoolField(TEXT("change_state_known"),Value.bChangeStateKnown); J->SetField(TEXT("changed"),Value.bChangeStateKnown?TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(Value.bChanged)):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
    J->SetBoolField(TEXT("postcondition_passed"),Value.bPostconditionPassed); J->SetStringField(TEXT("stage"),Value.Stage); J->SetStringField(TEXT("error_code"),Value.ErrorCode); J->SetStringField(TEXT("error"),Value.Error); J->SetObjectField(TEXT("data"),Value.Data); return J;
}
// MY consumes only copied signature JSON from the existing public own inspector.
// AddCall independently resolves and validates the current function and defaults.
// Original FindPin searched inputs exact/case-insensitively; non-input/unknown keys ignore.
bool OwnDefaults(const TSharedPtr<FJsonValue>& Value,const TSharedRef<FJsonObject>& Source,
    const TSharedRef<FJsonObject>& Out,const TSharedRef<FJsonObject>& Adapter,FResult& R,bool Native=false)
{
    Adapter->SetBoolField(TEXT("nonobject_params_ignored"),Value->Type!=EJson::Object&&Value->Type!=EJson::Null);
    TArray<TSharedPtr<FJsonValue>> Rows;Adapter->SetArrayField(TEXT("parameter_adaptation"),Rows);
    if(Value->Type!=EJson::Object)return true;
    if(Value->AsObject()->Values.Num()>64)return Fail(R,TEXT("function_node_parameter_budget"),TEXT("At most 64 own parameter keys are admitted before the core writer."));
    const TArray<TSharedPtr<FJsonValue>>* Parameters=nullptr;bool Matches=false;FString FlagText,FunctionPath;uint64 Flags=0;
    if(!Source->TryGetArrayField(Native?TEXT("native_signature_parameters"):TEXT("compiled_signature_parameters"),Parameters)||!Parameters||Parameters->Num()>128
        ||!Source->TryGetBoolField(Native?TEXT("native_signature_captured"):TEXT("compiled_signature_matches_skeleton"),Matches)||!Matches
        ||!Source->TryGetStringField(Native?TEXT("native_function_flags_string"):TEXT("compiled_function_flags_string"),FlagText)||!LexTryParseString(Flags,*FlagText)
        ||!Source->TryGetStringField(Native?TEXT("native_function_path"):TEXT("compiled_function_path"),FunctionPath))
        return Fail(R,TEXT("function_node_own_defaults_source_unavailable"),TEXT("The existing copied compiled/current own signature is unavailable; no writer dispatched."));
    TMap<FName,TSharedPtr<FJsonObject>> Inputs;
    for(const auto& Item:*Parameters)
    {
        if(!Item.IsValid()||Item->Type!=EJson::Object)return Fail(R,TEXT("function_node_own_defaults_source_unavailable"),TEXT("Copied own parameter row is unavailable."));
        const auto P=Item->AsObject();FString Name;bool Return=false,OutParm=false,Reference=false;
        if(!P.IsValid()||!P->TryGetStringField(TEXT("name"),Name)||!P->TryGetBoolField(TEXT("is_return"),Return)
            ||!P->TryGetBoolField(TEXT("is_out"),OutParm)||!P->TryGetBoolField(TEXT("is_reference"),Reference))
            return Fail(R,TEXT("function_node_own_defaults_source_unavailable"),TEXT("Copied own parameter name/direction is unavailable."));
        if(!Return&&(!OutParm||Reference))Inputs.Add(FName(*Name),P);
    }
    TSet<FName> PreparedNames;
    for(const auto& Pair:Value->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView());const auto V=Pair.Value;auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("requested_key"),Key);Row->SetField(TEXT("requested_value"),V);Row->SetBoolField(TEXT("default_prepared"),false);
        Rows.Add(MakeShared<FJsonValueObject>(Row));Adapter->SetArrayField(TEXT("parameter_adaptation"),Rows);
        const auto Reject=[&](const TCHAR* Condition,const FString& Expected,const FString& Actual)
        {
            Row->SetStringField(TEXT("disposition"),Condition);
            if(!Adapter->HasField(TEXT("own_default_first_failure")))
            {
                auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("condition"),Condition);D->SetStringField(TEXT("object"),FunctionPath+TEXT(":")+Key);
                D->SetStringField(TEXT("expected"),Expected);D->SetStringField(TEXT("actual"),Actual);D->SetStringField(TEXT("scope"),TEXT("Copied signature/request only; no failed-object lookup or writer."));
                Adapter->SetObjectField(TEXT("own_default_first_failure"),D);
            }
            return Fail(R,TEXT("function_node_parameter_unsupported"),TEXT("Own raw-default adaptation is outside the finite checked value-kind contract; copied first failure retained, no core writer."));
        };
        if(!V.IsValid())return Reject(TEXT("invalid_parameter_json"),TEXT("valid JSON value"),TEXT("invalid JSON carrier"));
        // Stock self/execute inputs are not authored by this value adapter. Pure calls have no execute input.
        const bool Physical=Key.Equals(TEXT("self"),ESearchCase::IgnoreCase)
            ||((Flags&FUNC_BlueprintPure)==0&&Key.Equals(TEXT("execute"),ESearchCase::IgnoreCase));
        // Unknown keys may exceed FName storage or contain escaped NUL; legacy FString lookup simply did not match.
        const auto* Found=Key.Len()<NAME_SIZE&&!HasNul(Key)?Inputs.Find(FName(*Key)):nullptr;
        if(!Physical&&!Found){Row->SetStringField(TEXT("disposition"),TEXT("ignored_no_input_pin"));continue;}
        if(V->Type==EJson::Null||V->Type==EJson::Object)
        {Row->SetStringField(TEXT("disposition"),TEXT("ignored_original_value_kind"));continue;}
        if(Physical)
        {
            if(V->Type==EJson::Number||V->Type==EJson::Boolean)
                return Reject(TEXT("original_physical_raw_default_bypass_unsupported"),TEXT("typed self/exec wire; nonnumeric original values ignore"),TEXT("numeric/bool raw DefaultValue write"));
            Row->SetStringField(TEXT("disposition"),TEXT("ignored_original_physical_pin_value_kind"));continue;
        }
        const auto Parameter=*Found;const TSharedPtr<FJsonObject>* Type=nullptr;const TSharedPtr<FJsonObject>* CurrentType=nullptr;
        FString Name,Category,Subcategory,ObjectPath,Container;bool Ref=false,Const=false,Array=false,PinRef=false;
        if(!Parameter->TryGetStringField(TEXT("name"),Name)||!Parameter->TryGetBoolField(TEXT("is_reference"),Ref)
            ||!Parameter->TryGetBoolField(TEXT("is_const"),Const)||!Parameter->TryGetObjectField(Native?TEXT("native_pin_type"):TEXT("compiled_pin_type"),Type)||!Type||!Type->IsValid()
            ||!Parameter->TryGetObjectField(Native?TEXT("native_pin_type"):TEXT("current_pin_type"),CurrentType)||!CurrentType||!CurrentType->IsValid()
            ||!Text(Type->ToSharedRef()).Equals(Text(CurrentType->ToSharedRef()),ESearchCase::CaseSensitive)
            ||!(*Type)->TryGetStringField(TEXT("category"),Category)||!(*Type)->TryGetStringField(TEXT("subcategory"),Subcategory)
            ||!(*Type)->TryGetStringField(TEXT("subcategory_object"),ObjectPath)||!(*Type)->TryGetStringField(TEXT("container_type"),Container)
            ||!(*Type)->TryGetBoolField(TEXT("is_array"),Array)||!(*Type)->TryGetBoolField(TEXT("is_reference"),PinRef))
            return Reject(TEXT("copied_parameter_type_unavailable"),TEXT("matching copied compiled/current scalar descriptor"),TEXT("combined copied descriptor mismatch"));
        Row->SetStringField(TEXT("canonical_pin"),Name);Row->SetObjectField(TEXT("source_parameter"),Parameter);
        const bool Vector=Category==TEXT("struct")&&ObjectPath==TEXT("/Script/CoreUObject.Vector");
        // Original arrays assign only exact Vector triples. Other arrays and lengths ignore, even for unsupported input types.
        if(V->Type==EJson::Array&&(!Vector||V->AsArray().Num()!=3))
        {Row->SetStringField(TEXT("disposition"),TEXT("ignored_original_array_shape_or_category"));continue;}
        // Native non-scalar/container descriptors are intentionally not recursively
        // decoded here; refuse string writes instead of misclassifying an Inner pin.
        if(Native&&Category==TEXT("unsupported_native")&&V->Type==EJson::String)
            return Reject(TEXT("native_descriptor_default_pending"),TEXT("supported by-value scalar or exact Vector"),TEXT("native non-scalar/class/container input string"));
        // Original string dispatch has no Name/String/PC_Real assignment branch. Do not broaden it.
        if(V->Type==EJson::String&&Category!=TEXT("int")&&Category!=TEXT("bool")&&Category!=TEXT("float")&&Category!=TEXT("class"))
        {Row->SetStringField(TEXT("disposition"),TEXT("ignored_original_string_pin_category"));continue;}
        if(Ref||PinRef||Array||Container!=TEXT("none")||(Vector&&Const))
            return Reject(TEXT("own_reference_container_default_unsupported"),TEXT("by-value scalar input; nonconst exact Vector"),Text(Parameter.ToSharedRef()));
        TSharedPtr<FJsonValue> Converted;FString Literal,Comparison=TEXT("exact_text");
        if(Vector&&V->Type==EJson::Array)
        {
            float XYZ[3];TArray<TSharedPtr<FJsonValue>> Conversions;
            for(int32 I=0;I<3;++I)
            {
                double Number=0;const auto Part=V->AsArray()[I];const bool Parsed=Part.IsValid()&&Part->TryGetNumber(Number);if(!Parsed)Number=0;
                if(!FMath::IsFinite(Number)||FMath::Abs(Number)>MAX_flt)return Reject(TEXT("nonfinite_float32_vector_unsupported"),TEXT("finite float32 Vector projection"),TEXT("array element exceeds finite float32"));
                XYZ[I]=static_cast<float>(Number);auto C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("numeric_conversion_succeeded"),Parsed);C->SetBoolField(TEXT("original_as_number_zero_fallback"),!Parsed);C->SetNumberField(TEXT("float32_value"),double(XYZ[I]));Conversions.Add(MakeShared<FJsonValueObject>(C));
            }
            Literal=FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"),XYZ[0],XYZ[1],XYZ[2]);Converted=MakeShared<FJsonValueString>(Literal);Row->SetArrayField(TEXT("vector_element_adaptation"),Conversions);
            Row->SetStringField(TEXT("disposition"),TEXT("prepared_original_float32_vector_six_decimal_literal"));
        }
        else if(Category==TEXT("int")&&(V->Type==EJson::Number||V->Type==EJson::String))
        {
            double Number=0;const bool Parsed=V->TryGetNumber(Number);if(!Parsed)Number=0;
            const double Rounded=FMath::FloorToDouble(Number+0.5);
            if(!FMath::IsFinite(Number)||!FMath::IsFinite(Rounded)||Rounded<MIN_int32||Rounded>MAX_int32)
                return Reject(TEXT("own_int_conversion_overflow_unsupported"),TEXT("finite original RoundToInt within int32"),TEXT("nonfinite or rounded int32 overflow"));
            const int32 Int=static_cast<int32>(FMath::RoundToInt(Number));Literal=FString::FromInt(Int);Converted=MakeShared<FJsonValueNumber>(Int);
            Row->SetBoolField(TEXT("numeric_conversion_succeeded"),Parsed);Row->SetBoolField(TEXT("original_as_number_zero_fallback"),!Parsed);Row->SetStringField(TEXT("disposition"),TEXT("prepared_original_round_to_int"));
        }
        else if(Category==TEXT("bool")&&(V->Type==EJson::Boolean||V->Type==EJson::String))
        {
            const bool Bool=V->AsBool();Literal=Bool?TEXT("true"):TEXT("false");Converted=MakeShared<FJsonValueBoolean>(Bool);Row->SetStringField(TEXT("disposition"),V->Type==EJson::String?TEXT("prepared_original_string_as_bool"):TEXT("prepared_original_bool"));
        }
        else if((Category==TEXT("real")&&(Subcategory==TEXT("float")||Subcategory==TEXT("double"))||Category==TEXT("name")||Category==TEXT("string"))&&V->Type==EJson::Number)
        {
            const double Number=V->AsNumber();if(!FMath::IsFinite(Number)||FMath::Abs(Number)>MAX_flt)return Reject(TEXT("own_float32_projection_unsupported"),TEXT("finite float32 original numeric projection"),TEXT("nonfinite or float32 overflow"));
            const float Original=static_cast<float>(Number);Literal=FString::SanitizeFloat(Original);Row->SetNumberField(TEXT("original_float32_value"),double(Original));
            if(Category==TEXT("real"))
            {
                const double Numeric=FCString::Atod(*Literal);if(!FMath::IsFinite(Numeric)||FMath::Abs(Numeric)>9007199254740991.0)return Reject(TEXT("own_checked_numeric_bound_unsupported"),TEXT("finite checked numeric magnitude <= 2^53-1"),Literal);
                Converted=MakeShared<FJsonValueNumber>(Numeric);Comparison=Subcategory==TEXT("float")?TEXT("float32_value"):TEXT("double_value");Row->SetNumberField(TEXT("prepared_numeric_value"),Numeric);
            }
            else Converted=MakeShared<FJsonValueString>(Literal);
            Row->SetStringField(TEXT("disposition"),TEXT("prepared_original_float32_numeric_literal"));
        }
        else if((Category==TEXT("name")||Category==TEXT("string"))&&V->Type==EJson::Boolean)
        {Literal=V->AsBool()?TEXT("true"):TEXT("false");Converted=MakeShared<FJsonValueString>(Literal);Row->SetStringField(TEXT("disposition"),TEXT("prepared_original_bool_text_literal"));}
        else return Reject(TEXT("original_raw_default_bypass_unsupported"),TEXT("finite original scalar/Vector value-kind accepted by checked AddCall"),TEXT("unsupported raw DefaultValue bypass/category/value kind"));
        if(PreparedNames.Contains(FName(*Name)))return Reject(TEXT("case_equivalent_prepared_duplicate_unsupported"),TEXT("one prepared value per case-insensitive input name"),Name);
        PreparedNames.Add(FName(*Name));Out->SetField(Name,Converted);Row->SetStringField(TEXT("original_literal_text"),Literal);Row->SetStringField(TEXT("comparison"),Comparison);
        Row->SetBoolField(TEXT("literal_text_identity_promised"),Comparison==TEXT("exact_text"));Row->SetField(TEXT("prepared_value"),Converted);Row->SetBoolField(TEXT("default_prepared"),true);
    }
    return true;
}
bool OwnDefaultsApplied(FResult& R,const TSharedRef<FJsonObject>& Adapter,bool Native=false)
{
    const TSharedPtr<FJsonObject>* Applied=nullptr;bool Verified=R.Data->TryGetObjectField(TEXT("param_defaults_applied"),Applied)&&Applied&&Applied->IsValid();int32 Count=0;
    const auto RecordFailure=[&](const FString& Pin,const TCHAR* Condition,const FString& Expected,const FString& Actual)
    {
        if(Adapter->HasField(TEXT("own_default_first_failure")))return;
        FString FunctionPath;Adapter->GetObjectField(Native?TEXT("native_library_source_before"):TEXT("own_source_before"))->TryGetStringField(Native?TEXT("native_function_path"):TEXT("compiled_function_path"),FunctionPath);
        auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("condition"),Condition);D->SetStringField(TEXT("object"),FunctionPath+(Pin.IsEmpty()?FString():TEXT(":")+Pin));
        D->SetStringField(TEXT("expected"),Expected);D->SetStringField(TEXT("actual"),Actual);D->SetStringField(TEXT("scope"),TEXT("Copied actual core return and prepared defaults only; no failed-object lookup."));Adapter->SetObjectField(TEXT("own_default_first_failure"),D);
    };
    for(const auto& Item:Adapter->GetArrayField(TEXT("parameter_adaptation")))
    {
        const auto Row=Item->AsObject();if(!Row->GetBoolField(TEXT("default_prepared")))continue;++Count;
        FString Actual;const bool Found=Verified&&(*Applied)->TryGetStringField(Row->GetStringField(TEXT("canonical_pin")),Actual);Row->SetBoolField(TEXT("actual_core_default_observed"),Found);
        if(!Found){RecordFailure(Row->GetStringField(TEXT("canonical_pin")),TEXT("core_default_missing"),Row->GetStringField(TEXT("original_literal_text")),TEXT("not present in copied actual core defaults"));Verified=false;continue;}Row->SetStringField(TEXT("actual_pin_literal_text"),Actual);
        const FString Original=Row->GetStringField(TEXT("original_literal_text"));Row->SetBoolField(TEXT("literal_text_matches_original"),Actual.Equals(Original,ESearchCase::CaseSensitive));
        const FString Comparison=Row->GetStringField(TEXT("comparison"));bool Same=false;
        if(Comparison==TEXT("exact_text"))Same=Actual.Equals(Original,ESearchCase::CaseSensitive);
        else
        {
            double ActualNumber=0;const bool Parsed=FDefaultValueHelper::ParseDouble(Actual,ActualNumber);const double Expected=Row->GetNumberField(TEXT("prepared_numeric_value"));
            Same=Parsed&&FMath::IsFinite(ActualNumber)&&(Comparison==TEXT("float32_value")?static_cast<float>(ActualNumber)==static_cast<float>(Expected):ActualNumber==Expected);
        }
        Row->SetBoolField(TEXT("actual_checked_value_matches_prepared"),Same);
        if(!Same)RecordFailure(Row->GetStringField(TEXT("canonical_pin")),TEXT("core_default_value_mismatch"),Comparison+TEXT(":")+Original,Actual);Verified&=Same;
    }
    const bool CountMatches=Applied&&Applied->IsValid()&&(*Applied)->Values.Num()==Count;
    if(!CountMatches)RecordFailure(FString(),TEXT("core_default_count_mismatch"),FString::FromInt(Count),Applied&&Applied->IsValid()?FString::FromInt((*Applied)->Values.Num()):TEXT("copied defaults unavailable"));
    Verified&=CountMatches;Adapter->SetBoolField(TEXT("own_defaults_postcondition_verified"),Verified);
    if(!Verified)return Fail(R,TEXT("function_node_adapter_postcondition_failed"),TEXT("Copied actual core defaults differ from original own value-kind adaptation; actual prefix retained without another SDK query or retry."));
    return true;
}


// Original short-name native-library selection was independent of the checked
// add-call tool. Reuse its existing writer, but adapt the original JSON value
// kinds before dispatch. No function is executed, no CDO is requested and no
// source class/asset is loaded by this adapter.
UClass* NativeLibrary(const FString& Target)
{
    if(Target==TEXT("KismetMathLibrary"))return UKismetMathLibrary::StaticClass();
    if(Target==TEXT("KismetStringLibrary"))return UKismetStringLibrary::StaticClass();
    if(Target==TEXT("KismetSystemLibrary"))return UKismetSystemLibrary::StaticClass();
    if(Target==TEXT("KismetTextLibrary"))return UKismetTextLibrary::StaticClass();
    return nullptr;
}
struct FOriginalNativeLibrarySource
{
    struct FField
    {
        FProperty* Pointer=nullptr;FFieldClass* Class=nullptr;FName Name;EPropertyFlags Flags=CPF_None;int32 ArrayDim=0;
        UScriptStruct* Struct=nullptr;UClass* ObjectClass=nullptr;UClass* MetaClass=nullptr;
        FProperty* ChildA=nullptr;FProperty* ChildB=nullptr;bool Resolved=true;
    };
    TStrongObjectPtr<UClass> Owner;TStrongObjectPtr<UFunction> Function;TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UScriptStruct> Vector;
    TArray<FField> Fields;FString ClassPath,FunctionPath,CanonicalName;EClassFlags ClassFlags=CLASS_None;
    EFunctionFlags FunctionFlags=FUNC_None;EObjectFlags OwnerFlags=RF_NoFlags,FunctionObjectFlags=RF_NoFlags,PackageObjectFlags=RF_NoFlags;
    uint32 PackageFlags=0;bool Captured=false;
    bool Roots() const
    {
        return IsValid(Owner.Get())&&IsValid(Function.Get())&&IsValid(Package.Get())&&IsValid(Vector.Get())
            &&Owner->GetPathName()==ClassPath&&Owner->GetPackage()==Package.Get()&&Owner->GetOuter()==Package.Get()
            &&Owner->GetClass()==UClass::StaticClass()&&Owner->ClassFlags==ClassFlags&&Owner->GetFlags()==OwnerFlags
            &&Owner->HasAllClassFlags(CLASS_Native)&&Package->GetName()==TEXT("/Script/Engine")
            &&Package->GetFlags()==PackageObjectFlags&&uint32(Package->GetPackageFlags())==PackageFlags
            &&Function->GetOuter()==Owner.Get()&&Function->GetClass()==UFunction::StaticClass()
            &&Function->GetFlags()==FunctionObjectFlags&&Function->GetPathName()==FunctionPath
            &&Function->FunctionFlags==FunctionFlags&&Owner->FindFunctionByName(FName(*CanonicalName))==Function.Get();
    }
    static FField Read(FProperty* P)
    {
        FField V;V.Pointer=P;V.Class=P->GetClass();V.Name=P->GetFName();V.Flags=P->GetPropertyFlags();V.ArrayDim=P->ArrayDim;
        if(const auto* S=CastField<FStructProperty>(P)){V.Resolved&=S->Struct.IsResolved();if(V.Resolved)V.Struct=S->Struct.Get();}
        if(const auto* O=CastField<FObjectPropertyBase>(P)){V.Resolved&=O->PropertyClass.IsResolved();if(V.Resolved)V.ObjectClass=O->PropertyClass.Get();}
        if(const auto* C=CastField<FClassProperty>(P)){V.Resolved&=C->MetaClass.IsResolved();if(V.Resolved)V.MetaClass=C->MetaClass.Get();}
        if(const auto* A=CastField<FArrayProperty>(P))V.ChildA=A->Inner;
        if(const auto* S=CastField<FSetProperty>(P))V.ChildA=S->ElementProp;
        if(const auto* M=CastField<FMapProperty>(P)){V.ChildA=M->KeyProp;V.ChildB=M->ValueProp;}
        // No enum/UClass/UStruct resolution or recursive Inner traversal is
        // performed for unsupported parameter categories. AddCall owns its
        // independently selected full native signature validation.
        return V;
    }
    bool Current() const
    {
        if(!Captured||!Roots())return false;
        // Compare the complete current root inventory before dereferencing any
        // captured FProperty. Holding a UFunction alone does not hold its fields.
        TArray<FProperty*> CurrentFields;
        for(TFieldIterator<FProperty> It(Function.Get());It;++It){if(CurrentFields.Num()>=128)return false;CurrentFields.Add(*It);}
        if(CurrentFields.Num()!=Fields.Num())return false;
        for(int32 I=0;I<Fields.Num();++I)if(CurrentFields[I]!=Fields[I].Pointer)return false;
        for(const auto& Before:Fields)
        {
            const auto* P=Before.Pointer;
            if(P->GetOwnerStruct()!=Function.Get())return false;
            const auto Now=Read(Before.Pointer);
            if(!Now.Resolved||Now.Class!=Before.Class||Now.Name!=Before.Name||Now.Flags!=Before.Flags||Now.ArrayDim!=Before.ArrayDim
                ||Now.Struct!=Before.Struct||Now.ObjectClass!=Before.ObjectClass||Now.MetaClass!=Before.MetaClass
                ||Now.ChildA!=Before.ChildA||Now.ChildB!=Before.ChildB)return false;
        }
        return Roots();
    }
    bool Capture(UClass* C,const FString& RequestedName,const TSharedRef<FJsonObject>& Json)
    {
        if(!IsValid(C)||RequestedName.IsEmpty()||!Bounded(RequestedName,128))return false;
        Owner.Reset(C);Package.Reset(C->GetPackage());Vector.Reset(TBaseStructure<FVector>::Get());
        Function.Reset(C->FindFunctionByName(FName(*RequestedName)));
        if(!IsValid(Function.Get())||!IsValid(Package.Get())||!IsValid(Vector.Get())||Function->GetOuter()!=C
            ||!Function->HasAllFunctionFlags(FUNC_Native|FUNC_Public|FUNC_Static|FUNC_BlueprintCallable)
            ||Function->HasAnyFunctionFlags(FUNC_EditorOnly|FUNC_Private|FUNC_Protected|FUNC_Delegate)
            ||Package->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))return false;
        ClassPath=C->GetPathName();FunctionPath=Function->GetPathName();CanonicalName=Function->GetName();
        ClassFlags=C->ClassFlags;FunctionFlags=Function->FunctionFlags;OwnerFlags=C->GetFlags();FunctionObjectFlags=Function->GetFlags();
        PackageObjectFlags=Package->GetFlags();PackageFlags=uint32(Package->GetPackageFlags());
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(TFieldIterator<FProperty> It(Function.Get());It;++It)
        {
            FProperty* P=*It;if(Fields.Num()>=128||P->GetOwnerStruct()!=Function.Get())return false;
            const auto V=Read(P);if(!V.Resolved)return false;Fields.Add(V);
            if(!P->HasAnyPropertyFlags(CPF_Parm))continue;
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),V.Name.ToString());
            Row->SetBoolField(TEXT("is_return"),P->HasAnyPropertyFlags(CPF_ReturnParm));Row->SetBoolField(TEXT("is_out"),P->HasAnyPropertyFlags(CPF_OutParm));
            Row->SetBoolField(TEXT("is_reference"),P->HasAnyPropertyFlags(CPF_ReferenceParm));Row->SetBoolField(TEXT("is_const"),P->HasAnyPropertyFlags(CPF_ConstParm));
            auto Type=MakeShared<FJsonObject>();FString Category=TEXT("unsupported_native"),Subcategory,ObjectPath;
            const bool Container=CastField<FArrayProperty>(P)||CastField<FSetProperty>(P)||CastField<FMapProperty>(P);
            // These exact public descriptors map directly to installed K2
            // GetPropertyCategoryInfo; all compound/enum/class defaults remain
            // pending instead of running an unbounded recursive schema query.
            if(!Container&&V.ArrayDim==1)
            {
                if(CastField<FIntProperty>(P))Category=TEXT("int");
                else if(CastField<FBoolProperty>(P))Category=TEXT("bool");
                else if(CastField<FFloatProperty>(P)){Category=TEXT("real");Subcategory=TEXT("float");}
                else if(CastField<FDoubleProperty>(P)){Category=TEXT("real");Subcategory=TEXT("double");}
                else if(CastField<FNameProperty>(P))Category=TEXT("name");
                else if(CastField<FStrProperty>(P))Category=TEXT("string");
                else if(V.Struct==Vector.Get()){Category=TEXT("struct");ObjectPath=TEXT("/Script/CoreUObject.Vector");}
            }
            Type->SetStringField(TEXT("category"),Category);Type->SetStringField(TEXT("subcategory"),Subcategory);
            Type->SetStringField(TEXT("subcategory_object"),ObjectPath);Type->SetStringField(TEXT("container_type"),Container||V.ArrayDim!=1?TEXT("unsupported"):TEXT("none"));
            Type->SetBoolField(TEXT("is_array"),CastField<FArrayProperty>(P)!=nullptr||V.ArrayDim!=1);
            Type->SetBoolField(TEXT("is_reference"),P->HasAnyPropertyFlags(CPF_ReferenceParm));Row->SetObjectField(TEXT("native_pin_type"),Type);
            Row->SetStringField(TEXT("property_flags"),LexToString(uint64(V.Flags)));Row->SetStringField(TEXT("field_class"),V.Class->GetName());Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Captured=true;if(!Current())return false;
        Json->SetBoolField(TEXT("native_signature_captured"),true);Json->SetArrayField(TEXT("native_signature_parameters"),Rows);
        Json->SetStringField(TEXT("native_function_flags_string"),LexToString(uint32(FunctionFlags)));Json->SetStringField(TEXT("native_function_path"),FunctionPath);
        Json->SetStringField(TEXT("native_function_name"),CanonicalName);Json->SetStringField(TEXT("native_class_path"),ClassPath);
        return true;
    }
};
FResult NativeLibraryAdd(const FString& BlueprintPath,const FString& Target,const FString& Name,const FString& ParamsJson,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool CreateGraph,FResult R,const TSharedRef<FJsonObject>& Adapter)
{
    Adapter->SetStringField(TEXT("scope"),TEXT("Original KismetMathLibrary/KismetStringLibrary/KismetSystemLibrary/KismetTextLibrary short-name static native selection and original case-insensitive function name; finite original scalar/Vector defaults/ignores, independently validated existing AddCall catalogue, including its bounded missing-function creation or existing-graph True reuse when requested. No broad class inference/fallback, raw invalid-default bypass, recursive/enum/class/ref/container default expansion or runtime invocation."));
    Adapter->SetStringField(TEXT("target_resolution"),TEXT("original_known_native_library_short_name"));
    Adapter->SetBoolField(TEXT("shared_original_signature_default_adapter"),true);
    Adapter->SetBoolField(TEXT("create_graph_if_missing_forwarded_to_shared_call"),CreateGraph);
    if(CreateGraph)Adapter->SetStringField(TEXT("graph_creation_scope"),TEXT("Existing shared AddCall resolves and reuses an existing selected graph or creates one missing explicitly named own direct native Actor/ActorComponent/SceneComponent function for its already-supported runtime static primitive signature/native Make-Break profile. The shared writer retains graph/default cleanup/partial and structural receipts without explicit compile/save. Source/default preflight remains before creation; original invalid-source create-before-error effects, broader owners/world-context/generated/ref/container signatures and runtime use remain incomplete; usage validation pending."));
    if(!IsInGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||GIsRequestingExit||UE::IsSavingPackage()||!IsValid(GEditor))
    {Fail(R,TEXT("function_node_editor_state_invalid"),TEXT("Safe current Editor game thread outside saving/GC required."));return R;}
    TWeakObjectPtr<UEditorEngine> Editor=GEditor;TWeakObjectPtr<UWorld> World=GEditor->GetEditorWorldContext().World();
    auto Context=[&](){return IsInGameThread()&&!IsGarbageCollecting()&&!IsEngineExitRequested()&&!GIsRequestingExit&&!UE::IsSavingPackage()
        &&IsValid(GEditor)&&Editor.Get()==GEditor&&!GEditor->GetPIEWorldContext()&&!GEditor->PlayWorld
        &&World.IsValid()&&IsValid(World.Get())&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();};
    if(!Context()){Fail(R,TEXT("function_node_editor_state_invalid"),TEXT("Current Editor world outside PIE required."));return R;}
    TSharedPtr<FJsonValue> Params,Coordinates;auto Defaults=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> PositionValues;
    if(!Parse(ParamsJson,Params,R)||!Parse(PositionJson,Coordinates,R)||!Position(Coordinates,PositionValues,Adapter,R))return R;
    Adapter->SetField(TEXT("requested_params"),Params);
    FOriginalNativeLibrarySource Source;auto SourceJson=MakeShared<FJsonObject>();Adapter->SetObjectField(TEXT("native_library_source_before"),SourceJson);
    Adapter->SetNumberField(TEXT("function_source_observation_calls"),1);
    if(!Source.Capture(NativeLibrary(Target),Name,SourceJson)||!Context())
    {Fail(R,TEXT("function_node_source_unsupported"),TEXT("Exact native static library declaration is unavailable; no transaction or core writer dispatched."));return R;}
    if(!OwnDefaults(Params,SourceJson,Defaults,Adapter,R,true))return R;
    Adapter->SetObjectField(TEXT("prepared_defaults"),Defaults);
    const auto* PriorUndo=GUndo;bool Returned=false,EndpointCaptured=false;FOwnEndpoint Endpoint;
    {
        Adapter->SetNumberField(TEXT("outer_transaction_calls"),1);
        FScopedTransaction Transaction(NSLOCTEXT("Cubeless","OriginalFunctionNode","Add Blueprint Function Node"));
        if(!Context()||!Source.Current())
        {R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Outer transaction changed native source/context; actual prefix retained, no core writer."));return R;}
        Adapter->SetNumberField(TEXT("core_delegate_calls"),1);
        R=Function::AddCall(BlueprintPath,Source.ClassPath,Source.CanonicalName,Text(Defaults),Text(PositionValues),GraphName,GraphId,GraphType,CreateGraph,false,false,false,false,false);
        Returned=true;Adapter->SetNumberField(TEXT("core_delegate_returns"),1);Adapter->SetObjectField(TEXT("core_receipt_at_return"),OwnReceipt(R));
        auto Data=MakeShared<FJsonObject>();for(const auto& P:R.Data->Values)Data->SetField(FString(P.Key.ToView()),P.Value);R.Data=Data;
        R.Data->SetObjectField(TEXT("original_function_node_adapter"),Adapter);
        if(R.bSuccess&&R.bChangeStateKnown&&R.bPostconditionPassed&&Context()&&Source.Current())
        {
            const FString ObjectPath=BlueprintPath.Contains(TEXT("."))?BlueprintPath:BlueprintPath+TEXT(".")+FPackageName::GetShortName(BlueprintPath);
            UBlueprint* LoadedTarget=FindObject<UBlueprint>(nullptr,*ObjectPath);
            if(Context()&&Source.Current())EndpointCaptured=Endpoint.Capture(LoadedTarget,Context)&&Context()&&Source.Current();
        }
    }
    Adapter->SetBoolField(TEXT("outer_transaction_scope_exited"),true);
    if(!Returned||!R.bSuccess||!R.bChangeStateKnown||!R.bPostconditionPassed)
    {R.bChangeStateKnown=false;Adapter->SetBoolField(TEXT("failed_core_engine_followup_skipped"),true);return R;}
    // Source inventory is checked only at real transaction/core SDK boundaries,
    // not repeatedly inside pure JSON conversion. Never follow a failed core
    // with another source/target engine lookup or replay.
    const bool Current=EndpointCaptured&&Context()&&GUndo==PriorUndo&&Source.Current()&&Endpoint.Current()&&Context()&&Source.Current();
    Adapter->SetBoolField(TEXT("native_library_source_current_after_transaction"),Current);
    FString ActualClass,ActualName,ActualPath;double X=0,Y=0;bool Static=false;
    const bool Identity=R.Data->TryGetStringField(TEXT("function_class"),ActualClass)&&ActualClass==Source.ClassPath
        &&R.Data->TryGetStringField(TEXT("function_name"),ActualName)&&ActualName==Source.CanonicalName
        &&R.Data->TryGetStringField(TEXT("function_path"),ActualPath)&&ActualPath==Source.FunctionPath
        &&R.Data->TryGetBoolField(TEXT("is_static"),Static)&&Static
        &&R.Data->TryGetNumberField(TEXT("x"),X)&&X==PositionValues[0]->AsNumber()
        &&R.Data->TryGetNumberField(TEXT("y"),Y)&&Y==PositionValues[1]->AsNumber();
    Adapter->SetBoolField(TEXT("native_library_core_identity_verified"),Identity);
    const bool Values=OwnDefaultsApplied(R,Adapter,true);
    Adapter->SetBoolField(TEXT("native_library_original_defaults_verified"),Values);
    Adapter->SetStringField(TEXT("endpoint_observation_scope"),TEXT("Existing AddCall owns complete source/target/node/default lifecycle proof. Adapter checks held native declaration/root-descriptor identity across transaction/core boundaries and copied actual returned function/position/defaults. Physical target endpoint across EndTransaction reuses the existing own endpoint snapshot; whole CDO/global callback effects, runtime invocation and full original fallback are not verified."));
    if(!Current||!Identity||!Values)
    {if(!Current)R.bChangeStateKnown=false;Fail(R,TEXT("function_node_adapter_postcondition_failed"),TEXT("Actual native library core/transaction/default endpoint differs; complete actual prefix retained without retry or rollback."));}
    return R;
}

FResult OwnAdd(const FString& BlueprintPath,const FString& Target,const FString& Name,const FString& ParamsJson,const FString& PositionJson,
    const FString& GraphName,const FString& GraphId,const FString& GraphType,bool CreateGraph,FResult R,const TSharedRef<FJsonObject>& Adapter)
{
    Adapter->SetStringField(TEXT("scope"),TEXT("Original self/empty target, already-loaded normal Blueprint and existing compiled own nonoverride function or exact own primitive custom event including the admitted RPC profile; finite original value-kind defaults and original ignores. Shared AddCall independently owns current function/GUID/signature/physical node checks. No arbitrary failed-target fallback, inherited/override/wider-RPC/event-struct/ref/container/graph creation, invalid raw default bypass, reference/container/other struct default expansion or runtime invocation."));
    Adapter->SetStringField(TEXT("target_resolution"),Target.IsEmpty()?TEXT("original_empty_target_own_class"):TEXT("original_self_target_own_class_fallback"));
    if (!Bounded(BlueprintPath,1024) || !BlueprintPath.StartsWith(TEXT("/Game/")) || !Bounded(Name,128) || Name.IsEmpty())
    { Fail(R,TEXT("function_node_own_selector_invalid"),TEXT("An exact /Game Blueprint object path and bounded own function name are required.")); return R; }
    if (CreateGraph) { Fail(R,TEXT("function_node_graph_creation_unsupported"),TEXT("Own raw calls require an existing graph; original create-before-validation effects remain incomplete.")); return R; }
    TSharedPtr<FJsonValue> Params,Coordinates; TArray<TSharedPtr<FJsonValue>> PositionValues;
    if (!Parse(ParamsJson,Params,R)||!Parse(PositionJson,Coordinates,R)||!Position(Coordinates,PositionValues,Adapter,R)) return R;
    Adapter->SetField(TEXT("requested_params"),Params);
    if (!IsEditorGameThread() || IsGarbageCollecting() || IsEngineExitRequested() || GIsRequestingExit || UE::IsSavingPackage() || !IsValid(GEditor) || IsPlaySessionActive())
    { Fail(R,TEXT("function_node_editor_state_invalid"),TEXT("Idle Editor GT outside PIE/GC/save/exit is required before own lookup.")); return R; }
    TStrongObjectPtr<UEditorEngine> Editor(GEditor); TStrongObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());
    auto Context=[&]() { return IsEditorGameThread()&&!IsGarbageCollecting()&&!IsEngineExitRequested()&&!GIsRequestingExit&&!UE::IsSavingPackage()
        &&Editor.IsValid()&&World.IsValid()&&GEditor==Editor.Get()&&!IsPlaySessionActive()&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get(); };
    if (!Context()) { Fail(R,TEXT("function_node_editor_state_invalid"),TEXT("Current Editor world is unavailable.")); return R; }
    const FString ObjectPath=BlueprintPath.Contains(TEXT("."))?BlueprintPath:BlueprintPath+TEXT(".")+FPackageName::GetShortName(BlueprintPath);
    if (!FPackageName::IsValidObjectPath(ObjectPath)) { Fail(R,TEXT("function_node_own_selector_invalid"),TEXT("Own target must be an exact Blueprint package/object path."));return R; }
    Adapter->SetStringField(TEXT("resolved_blueprint_path"),ObjectPath);
    UBlueprint* Found=FindObject<UBlueprint>(nullptr,*ObjectPath); TStrongObjectPtr<UBlueprint> BP(Found);
    if (!Context()) { R.bChangeStateKnown=false; Fail(R,TEXT("function_node_own_context_changed"),TEXT("Own target lookup changed Editor context; no next SDK or writer.")); return R; }
    if (!FOwnEndpoint::Loaded(Found)||Found->GetClass()!=UBlueprint::StaticClass()||!Found->GetPathName().Equals(ObjectPath,ESearchCase::CaseSensitive)||Found->BlueprintType!=BPTYPE_Normal
        || Found->bBeingCompiled||Found->bQueuedForCompilation||Found->bIsRegeneratingOnLoad||Found->bSuppressStructurallyModified||!Found->GeneratedClass.GetGCPtr().IsResolved())
    { Fail(R,TEXT("function_node_own_loaded_target_required"),TEXT("Exact already-loaded idle normal UBlueprint is required; no target/class/CDO load or creation was dispatched.")); return R; }
    UClass* Class=Found->GeneratedClass.GetGCPtr(); TStrongObjectPtr<UClass> Generated(Class);
    if (!FOwnEndpoint::Loaded(Class)||!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy!=Found)
    { Fail(R,TEXT("function_node_own_class_unavailable"),TEXT("Current generated class must belong to the held Blueprint.")); return R; }
    const FString ClassPath=Class->GetPathName(); FOwnEndpoint BeforeRead;
    if (!BeforeRead.Capture(Found,Context,FName(*Name))||!Context()) { R.bChangeStateKnown=false; Fail(R,TEXT("function_node_own_read_unavailable"),TEXT("Own read scope is unavailable before signature inspection.")); return R; }
    Adapter->SetNumberField(TEXT("function_source_observation_calls"),1);
    FResult Source=Function::InspectCompiledFunction(ObjectPath,ClassPath,Name); Adapter->SetObjectField(TEXT("own_source_inspection_before"),OwnReceipt(Source));
    if (!Context()||!BeforeRead.Current()) { R.bChangeStateKnown=false; Fail(R,TEXT("function_node_own_read_changed"),TEXT("Signature query changed or obscured the captured target; no transaction/writer.")); return R; }
    if (!Source.bSuccess||!Source.bChangeStateKnown||Source.bChanged||!Source.bPostconditionPassed)
    { R=Source; auto Data=MakeShared<FJsonObject>(); for(const auto& P:R.Data->Values)Data->SetField(FString(P.Key.ToView()),P.Value);R.Data=Data;R.Data->SetObjectField(TEXT("original_function_node_adapter"),Adapter);return R; }
    FString Kind,Guid; if (!Source.Data->TryGetStringField(TEXT("callable_kind"),Kind)||(Kind!=TEXT("function")&&Kind!=TEXT("custom_event"))||!Source.Data->TryGetStringField(TEXT("function_member_guid"),Guid)||Guid.IsEmpty())
    { Fail(R,TEXT("function_node_own_function_required"),TEXT("An existing compiled own function or exact own primitive custom event including the admitted RPC profile is required; override/inherited fallback remains pending.")); return R; }
    if(Kind==TEXT("custom_event"))
    {
        const TArray<TSharedPtr<FJsonValue>>* Parameters=nullptr;
        if(!Source.Data->TryGetArrayField(TEXT("compiled_signature_parameters"),Parameters)||!Parameters){Fail(R,TEXT("function_node_own_event_source_unavailable"),TEXT("Copied current event parameter inventory is unavailable."));return R;}
        if(!Parameters->IsEmpty())
        {
            const TSharedPtr<FJsonObject>* Proof=nullptr;bool Primitive=false,Current=false;
            if(!Source.Data->TryGetObjectField(TEXT("own_custom_event_source"),Proof)||!Proof||!Proof->IsValid()
                ||!(*Proof)->TryGetBoolField(TEXT("by_value_primitive_event"),Primitive)||!Primitive||!(*Proof)->TryGetBoolField(TEXT("current_verified"),Current)||!Current)
            {Fail(R,TEXT("function_node_own_event_source_unavailable"),TEXT("The current event declaration/output and compiled input proof is unavailable; AddCall will independently revalidate before authoring."));return R;}
        }
    }
    const auto SourceProof=OwnSource(Source.Data); Adapter->SetObjectField(TEXT("own_source_before"),SourceProof);
    auto PreparedDefaults=MakeShared<FJsonObject>();
    if(!OwnDefaults(Params,Source.Data,PreparedDefaults,Adapter,R))return R;
    Adapter->SetObjectField(TEXT("prepared_defaults"),PreparedDefaults);
    const auto* PriorUndo=GUndo; FOwnEndpoint BeforeEnd; bool CoreReturned=false,EndpointCaptured=false;
    {
        Adapter->SetNumberField(TEXT("outer_transaction_calls"),1); FScopedTransaction Transaction(NSLOCTEXT("Cubeless","OriginalFunctionNode","Add Blueprint Function Node"));
        if (!Context()||!BeforeRead.Current()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_own_context_changed"),TEXT("Transaction entry changed the captured target; no core writer dispatched.")); }
        else
        {
            Adapter->SetNumberField(TEXT("core_delegate_calls"),1);
            R=Function::AddCall(BlueprintPath,ClassPath,Name,Text(PreparedDefaults),Text(PositionValues),GraphName,GraphId,GraphType,false,false,false,false,false,false);
            CoreReturned=true;Adapter->SetNumberField(TEXT("core_delegate_returns"),1);Adapter->SetObjectField(TEXT("core_receipt_at_return"),OwnReceipt(R));
            auto Data=MakeShared<FJsonObject>();for(const auto& P:R.Data->Values)Data->SetField(FString(P.Key.ToView()),P.Value);R.Data=Data;R.Data->SetObjectField(TEXT("original_function_node_adapter"),Adapter);
            const bool DefaultsVerified=R.bSuccess&&R.bChangeStateKnown&&R.bPostconditionPassed&&OwnDefaultsApplied(R,Adapter);
            bool Self=false;FString ActualGuid;const bool CoreIdentity=R.Data->TryGetBoolField(TEXT("self_context"),Self)&&Self&&R.Data->TryGetStringField(TEXT("function_member_guid"),ActualGuid)&&ActualGuid==Guid;
            // The core validates any SDK/structural effects. No pre-core class pointer
            // is imposed as a new invariant across its complete authoring operation.
            if (R.bSuccess&&R.bChangeStateKnown&&R.bPostconditionPassed&&CoreIdentity&&DefaultsVerified&&Context()) EndpointCaptured=BeforeEnd.Capture(BP.Get(),Context,FName(*Name))&&Context();
            Adapter->SetBoolField(TEXT("own_core_identity_verified"),CoreIdentity);
        }
    }
    Adapter->SetBoolField(TEXT("outer_transaction_scope_exited"),true);
    if (!CoreReturned||!R.bSuccess||!R.bChangeStateKnown||!R.bPostconditionPassed)
    {
        R.bChangeStateKnown=false;R.bSuccess=R.bPostconditionPassed=false;Adapter->SetBoolField(TEXT("failed_core_engine_followup_skipped"),true);
        if(R.ErrorCode.IsEmpty())Fail(R,TEXT("function_node_own_endpoint_unverified"),TEXT("No successful known core endpoint; actual prefix preserved without follow-up SDK/retry."));return R;
    }
    if (!EndpointCaptured||!Context()||GUndo!=PriorUndo||!BeforeEnd.Current())
    { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_own_endpoint_changed"),TEXT("Outer transaction changed or obscured source/target physical scope; core prefix retained without further SDK."));return R; }
    Adapter->SetNumberField(TEXT("function_source_observation_calls"),2);
    FResult AfterSource=Function::InspectCompiledFunction(ObjectPath,ClassPath,Name);Adapter->SetObjectField(TEXT("own_source_inspection_after"),OwnReceipt(AfterSource));
    const bool Current=Context()&&GUndo==PriorUndo&&BeforeEnd.Current();const auto SourceAfter=OwnSource(AfterSource.Data);Adapter->SetObjectField(TEXT("own_source_after"),SourceAfter);
    const bool Same=Current&&AfterSource.bSuccess&&AfterSource.bChangeStateKnown&&!AfterSource.bChanged&&AfterSource.bPostconditionPassed&&Text(SourceProof).Equals(Text(SourceAfter),ESearchCase::CaseSensitive);
    Adapter->SetBoolField(TEXT("post_transaction_observation_complete"),Current);Adapter->SetBoolField(TEXT("post_transaction_selected_scope_preserved"),Same);Adapter->SetBoolField(TEXT("postcondition_verified"),Same);
    Adapter->SetStringField(TEXT("endpoint_observation_scope"),TEXT("Existing public own signature query, target class/graph/node identities and complete physical pin type/default/flags/member/edge snapshots across read and EndTransaction only. Whole CDO/global SDK/disk effects and runtime invocation are unverified. Source receipts are diagnostics, not approval tokens; AddCall independently resolves the own source."));
    if (!Same) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_own_readback_unverified"),TEXT("Own signature/current endpoint differs after core/transaction; complete actual prefix retained, no retry or rollback.")); }
    return R;
}
}

Function::FResult Add(const FString& BlueprintPath, const FString& Target, const FString& FunctionName,
    const FString& ParamsJson, const FString& PositionJson, const FString& GraphName,
    const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing)
{
    Function::FResult R; auto Adapter = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("original_function_node_adapter"), Adapter);
    Adapter->SetStringField(TEXT("requested_target"), Target); Adapter->SetStringField(TEXT("requested_function_name"), FunctionName);
    Adapter->SetStringField(TEXT("requested_blueprint"), BlueprintPath); Adapter->SetStringField(TEXT("requested_graph_name"), GraphName);
    Adapter->SetStringField(TEXT("requested_graph_id"), GraphId); Adapter->SetStringField(TEXT("requested_graph_type"), GraphType);
    Adapter->SetBoolField(TEXT("create_graph_if_missing"), bCreateGraphIfMissing);
    Adapter->SetNumberField(TEXT("outer_transaction_calls"), 0); Adapter->SetNumberField(TEXT("core_delegate_calls"), 0); Adapter->SetNumberField(TEXT("core_delegate_returns"), 0);
    Adapter->SetNumberField(TEXT("function_source_observation_calls"), 0);
    Adapter->SetNumberField(TEXT("explicit_compile_calls"), 0); Adapter->SetNumberField(TEXT("explicit_save_calls"), 0);
    Adapter->SetBoolField(TEXT("runtime_invocation_verified"), false); Adapter->SetBoolField(TEXT("full_original_contract_verified"), false);
    Adapter->SetStringField(TEXT("scope"), TEXT("Actual native Engine GameplayStatics.GetPlayerController/GetPlayerPawn/GetPlayerCharacter in an existing owned runtime Blueprint instance event/function graph; original PlayerIndex rounding/unknown-key ignores and float32 position conversion adapted, validated AddCall core retained. No broad class inference, graph creation, raw invalid-default bypass, runtime player object availability or original mixed Bird pipeline claim."));
    if (Target.IsEmpty() || Target.Equals(TEXT("self"), ESearchCase::IgnoreCase))
    { return OwnAdd(BlueprintPath, Target, FunctionName, ParamsJson, PositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing, R, Adapter); }
    if (Target == TEXT("PrimitiveComponent") || Target == TEXT("UPrimitiveComponent"))
    { return PhysicsAdd(BlueprintPath, Target, FunctionName, ParamsJson, PositionJson, GraphName, GraphId, GraphType, bCreateGraphIfMissing, R, Adapter); }
    if(Target==TEXT("KismetMathLibrary")||Target==TEXT("KismetStringLibrary")||Target==TEXT("KismetSystemLibrary")||Target==TEXT("KismetTextLibrary"))
    { return NativeLibraryAdd(BlueprintPath,Target,FunctionName,ParamsJson,PositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing,R,Adapter); }
    const bool PlayerObject=FunctionName.Equals(TEXT("GetPlayerPawn"),ESearchCase::IgnoreCase)||FunctionName.Equals(TEXT("GetPlayerCharacter"),ESearchCase::IgnoreCase);
    const TCHAR* ActorFunction=Target!=TEXT("Actor")?nullptr:FunctionName.Equals(TEXT("K2_GetActorLocation"),ESearchCase::IgnoreCase)?TEXT("K2_GetActorLocation"):FunctionName.Equals(TEXT("K2_GetActorRotation"),ESearchCase::IgnoreCase)?TEXT("K2_GetActorRotation"):FunctionName.Equals(TEXT("GetActorScale3D"),ESearchCase::IgnoreCase)?TEXT("GetActorScale3D"):nullptr;
    const bool ActorGetter=ActorFunction!=nullptr;
    TWeakObjectPtr<UEditorEngine> CapturedEditor;TWeakObjectPtr<UWorld> CapturedWorld;
    auto Context=[&]()
    {
        if(!PlayerObject&&!ActorGetter) { return true; }
        if(!IsInGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||GIsRequestingExit||UE::IsSavingPackage()||!GEditor||GEditor!=CapturedEditor.Get()||!IsValid(GEditor)) { return false; }
        return !GEditor->GetPIEWorldContext()&&!GEditor->PlayWorld&&CapturedWorld.IsValid()&&IsValid(CapturedWorld.Get())&&!CapturedWorld->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==CapturedWorld.Get();
    };
    if(PlayerObject||ActorGetter)
    {
        if(!IsInGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||GIsRequestingExit||UE::IsSavingPackage()||!IsValid(GEditor)) { Fail(R,TEXT("function_node_editor_state_invalid"),TEXT("Safe Editor GT required before native getter resolution."));return R; }
        CapturedEditor=GEditor;CapturedWorld=GEditor->GetEditorWorldContext().World();if(!Context()) { Fail(R,TEXT("function_node_editor_state_invalid"),TEXT("Current Editor world outside PIE required."));return R; }
    }
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("function_node_editor_state_invalid"), TEXT("Editor game thread outside PIE/SIE required.")); return R; }
    if (!Bounded(Target, 128) || !Bounded(FunctionName, 128) || FunctionName.IsEmpty()
        || (Target != TEXT("UGameplayStatics") && Target != TEXT("GameplayStatics") && Target != TEXT("Actor")))
    { Fail(R, TEXT("function_node_target_unsupported"), TEXT("Use original UGameplayStatics/GameplayStatics or Actor class spelling for the finite native getters; broad original inference/fallback remains outside this slice.")); return R; }
    if (bCreateGraphIfMissing)
    { Fail(R, TEXT("function_node_graph_creation_unsupported"), TEXT("Select an existing graph; original create-before-function-validation effects remain outside this slice.")); return R; }
    if(Target==TEXT("Actor"))
    {
        Adapter->SetStringField(TEXT("scope"),TEXT("Original Actor short-name class target and exact native K2_GetActorLocation/K2_GetActorRotation/GetActorScale3D; object-self value-kind ignores and existing float32 position adaptation retained. Shared audited AddCall owns full source/binding/physical pin/default/lifetime proof; no runtime getter execution or broad self/component inference."));
        if(!ActorGetter) { Fail(R,TEXT("function_node_source_unsupported"),TEXT("Only the three exact native Actor transform getters are admitted for this original Actor target."));return R; }
        TSharedPtr<FJsonValue> ActorParams,ActorCoordinates;auto ActorPreparedDefaults=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> ActorPreparedPosition;
        if(!Parse(ParamsJson,ActorParams,R)||!Parse(PositionJson,ActorCoordinates,R)||!ActorDefaults(ActorParams,Adapter,R)||!Position(ActorCoordinates,ActorPreparedPosition,Adapter,R)) { return R; }
        Adapter->SetObjectField(TEXT("prepared_defaults"),ActorPreparedDefaults);Adapter->SetBoolField(TEXT("function_source_observation_delegated_to_core"),true);
        Adapter->SetNumberField(TEXT("outer_transaction_calls"),1);
        FScopedTransaction ActorTransaction(NSLOCTEXT("Cubeless","OriginalFunctionNode","Add Blueprint Function Node"));
        if(!Context()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Outer transaction changed captured Editor/world; no core writer dispatched."));return R; }
        Adapter->SetNumberField(TEXT("core_delegate_calls"),1);
        R=Function::AddCall(BlueprintPath,TEXT("/Script/Engine.Actor"),ActorFunction,Text(ActorPreparedDefaults),Text(ActorPreparedPosition),GraphName,GraphId,GraphType,false,false,false,false,false,false);
        // Adopt the entire actual result before any adapter authority/health check.
        Adapter->SetNumberField(TEXT("core_delegate_returns"),1);R.Data->SetObjectField(TEXT("original_function_node_adapter"),Adapter);
        Adapter->SetBoolField(TEXT("core_success"),R.bSuccess);Adapter->SetBoolField(TEXT("core_change_state_known"),R.bChangeStateKnown);Adapter->SetBoolField(TEXT("core_mutation_attempted"),R.bMutationAttempted);Adapter->SetStringField(TEXT("core_stage"),R.Stage);Adapter->SetStringField(TEXT("core_error_code"),R.ErrorCode);Adapter->SetStringField(TEXT("core_error"),R.Error);Adapter->SetBoolField(TEXT("core_postcondition_passed"),R.bPostconditionPassed);
        Adapter->SetField(TEXT("core_changed"),R.bChangeStateKnown?TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(R.bChanged)):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
        Adapter->SetStringField(TEXT("core_effect_count_scope"),TEXT("Delegate/outer transaction only; source observers and node work are performed by unchanged shared AddCall. Runtime/global SDK effects are not independently measured."));
        if(!R.bSuccess||!R.bChangeStateKnown) { return R; }
        if(!Context()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Core returned after context loss; actual partial prefix retained without further source SDK."));return R; }
        FString ActorActualClass,ActorActualFunction,ActorActualPath,ActorActualNodeClass,ActorNodeId;double ActorX=0,ActorY=0;bool ActorStatic=true,ActorPure=false,ActorTyped=false,ActorSourceStable=false,ActorObserved=false;
        const FString ActorExpectedPath=FString(TEXT("/Script/Engine.Actor:"))+ActorFunction;
        const bool ActorVerified=R.bPostconditionPassed
            &&R.Data->TryGetStringField(TEXT("function_class"),ActorActualClass)&&ActorActualClass==TEXT("/Script/Engine.Actor")
            &&R.Data->TryGetStringField(TEXT("function_name"),ActorActualFunction)&&ActorActualFunction==ActorFunction
            &&R.Data->TryGetStringField(TEXT("function_path"),ActorActualPath)&&ActorActualPath==ActorExpectedPath
            &&R.Data->TryGetStringField(TEXT("node_class"),ActorActualNodeClass)&&ActorActualNodeClass==TEXT("/Script/BlueprintGraph.K2Node_CallFunction")
            &&R.Data->TryGetStringField(TEXT("node_id"),ActorNodeId)&&!ActorNodeId.IsEmpty()
            &&R.Data->TryGetNumberField(TEXT("x"),ActorX)&&ActorX==ActorPreparedPosition[0]->AsNumber()
            &&R.Data->TryGetNumberField(TEXT("y"),ActorY)&&ActorY==ActorPreparedPosition[1]->AsNumber()
            &&R.Data->TryGetBoolField(TEXT("is_static"),ActorStatic)&&!ActorStatic&&R.Data->TryGetBoolField(TEXT("is_pure"),ActorPure)&&ActorPure
            &&R.Data->TryGetBoolField(TEXT("actor_getter_typed_result_verified"),ActorTyped)&&ActorTyped
            &&R.Data->TryGetBoolField(TEXT("actor_getter_source_observed_unchanged"),ActorSourceStable)&&ActorSourceStable
            &&R.Data->TryGetBoolField(TEXT("actor_getter_observation_complete"),ActorObserved)&&ActorObserved;
        Adapter->SetBoolField(TEXT("function_source_preserved"),ActorSourceStable);Adapter->SetBoolField(TEXT("postcondition_verified"),ActorVerified);
        if(!ActorVerified) { R.Stage=TEXT("verification");Fail(R,TEXT("function_node_adapter_postcondition_failed"),TEXT("Actual shared Actor getter receipt differs; partial/unknown/core diagnostics retained without retry, rollback or additional SDK.")); }
        return R;
    }
    UClass* Owner = UGameplayStatics::StaticClass(); UFunction* F = Owner->FindFunctionByName(FName(*FunctionName));
    auto BeforeSource = MakeShared<FJsonObject>(); Adapter->SetObjectField(TEXT("function_source_before"), BeforeSource);
    Adapter->SetNumberField(TEXT("function_source_observation_calls"), 1);
    if (!Source(Owner, F, BeforeSource))
    { Fail(R, TEXT("function_node_source_unsupported"), TEXT("Actual native declaring class/function/signature does not match GetPlayerController/GetPlayerPawn/GetPlayerCharacter.")); return R; }
    if(!Context()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Getter source observation changed Editor/world context; no transaction or core writer was dispatched."));return R; }
    TSharedPtr<FJsonValue> Params, Coordinates;
    if (!Parse(ParamsJson, Params, R) || !Parse(PositionJson, Coordinates, R)) { return R; }
    auto PreparedDefaults = MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> PreparedPosition;
    if (!Defaults(Params, PreparedDefaults, Adapter, R) || !Position(Coordinates, PreparedPosition, Adapter, R)) { return R; }
    Adapter->SetObjectField(TEXT("prepared_defaults"), PreparedDefaults);
    // Original command dispatcher has one outer transaction; core owns its explicit BP/graph Modify.
    Adapter->SetNumberField(TEXT("outer_transaction_calls"), 1);
    FScopedTransaction Transaction(NSLOCTEXT("Cubeless", "OriginalFunctionNode", "Add Blueprint Function Node"));
    if(!Context()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Outer transaction callback changed the captured context; no core writer was dispatched."));return R; }
    Adapter->SetNumberField(TEXT("core_delegate_calls"), 1);
    R = Function::AddCall(BlueprintPath, Owner->GetPathName(), F->GetName(), Text(PreparedDefaults), Text(PreparedPosition),
        GraphName, GraphId, GraphType, false, false, false, false, false, false);
    // Retain the entire actual core receipt, including any partial/unknown prefix, before checking it.
    Adapter->SetNumberField(TEXT("core_delegate_returns"), 1); R.Data->SetObjectField(TEXT("original_function_node_adapter"), Adapter);
    Adapter->SetBoolField(TEXT("core_success"), R.bSuccess); Adapter->SetBoolField(TEXT("core_change_state_known"), R.bChangeStateKnown);
    Adapter->SetBoolField(TEXT("core_mutation_attempted"), R.bMutationAttempted); Adapter->SetStringField(TEXT("core_stage"), R.Stage);
    Adapter->SetStringField(TEXT("core_error_code"), R.ErrorCode); Adapter->SetStringField(TEXT("core_error"), R.Error);
    Adapter->SetBoolField(TEXT("core_postcondition_passed"), R.bPostconditionPassed);
    Adapter->SetField(TEXT("core_changed"), R.bChangeStateKnown ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(R.bChanged)) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
    Adapter->SetStringField(TEXT("core_effect_count_scope"), TEXT("Only delegate/outer transaction calls are instrumented here. Core BP.Modify, graph.Modify, creator/finalize, per-default schema setter and MarkModified are source-derived work, not independently counted callbacks; implicit load/cache/notification effects are unmeasured."));
    if(PlayerObject&&(!R.bSuccess||!R.bChangeStateKnown)) { return R; }
    if(!Context()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Core returned after context loss; actual partial prefix retained without further source SDK."));return R; }
    auto AfterSource = MakeShared<FJsonObject>(); Adapter->SetObjectField(TEXT("function_source_after"), AfterSource);
    Adapter->SetNumberField(TEXT("function_source_observation_calls"), 2);
    const bool SourceAfter = Source(Owner, F, AfterSource);
    if(!Context()) { R.bChangeStateKnown=false;Fail(R,TEXT("function_node_context_changed"),TEXT("Source readback returned after context loss; actual core receipt retained without further SDK."));return R; }
    const bool SourcePreserved = SourceAfter && Text(BeforeSource) == Text(AfterSource);
    Adapter->SetBoolField(TEXT("function_source_preserved"), SourcePreserved);
    if (!R.bSuccess) { return R; } // Never mask or resume a failed core operation.
    FString ActualClass, ActualFunction, ActualPath, ActualNodeClass, NodeId; double X = 0, Y = 0; bool Static = false, Pure = false;
    bool Verified = SourcePreserved && R.bChangeStateKnown && R.bPostconditionPassed
        && R.Data->TryGetStringField(TEXT("function_class"), ActualClass) && ActualClass == Owner->GetPathName()
        && R.Data->TryGetStringField(TEXT("function_name"), ActualFunction) && ActualFunction == F->GetName()
        && R.Data->TryGetStringField(TEXT("function_path"), ActualPath) && ActualPath == F->GetPathName()
        && R.Data->TryGetStringField(TEXT("node_class"), ActualNodeClass) && ActualNodeClass == TEXT("/Script/BlueprintGraph.K2Node_CallFunction")
        && R.Data->TryGetStringField(TEXT("node_id"), NodeId) && !NodeId.IsEmpty()
        && R.Data->TryGetNumberField(TEXT("x"), X) && X == PreparedPosition[0]->AsNumber()
        && R.Data->TryGetNumberField(TEXT("y"), Y) && Y == PreparedPosition[1]->AsNumber()
        && R.Data->TryGetBoolField(TEXT("is_static"), Static) && Static && R.Data->TryGetBoolField(TEXT("is_pure"), Pure) && Pure;
    if (PreparedDefaults->HasField(TEXT("PlayerIndex")))
    {
        const TSharedPtr<FJsonObject>* Applied = nullptr; FString ActualIndex;
        Verified &= R.Data->TryGetObjectField(TEXT("param_defaults_applied"), Applied) && Applied && Applied->IsValid()
            && (*Applied)->TryGetStringField(TEXT("PlayerIndex"), ActualIndex)
            && ActualIndex == FString::FromInt(static_cast<int32>(PreparedDefaults->GetNumberField(TEXT("PlayerIndex"))));
    }
    Adapter->SetBoolField(TEXT("postcondition_verified"), Verified);
    if (!Verified)
    {
        R.Stage = TEXT("verification");
        if (!SourceAfter) { R.bChangeStateKnown = false; }
        else if (!SourcePreserved) { R.bChanged = true; }
        Fail(R, TEXT("function_node_adapter_postcondition_failed"), TEXT("Actual core/function source/node/default result differs; raw core observations and work prefix are retained. No retry or rollback was performed."));
    }
    return R;
}
}
