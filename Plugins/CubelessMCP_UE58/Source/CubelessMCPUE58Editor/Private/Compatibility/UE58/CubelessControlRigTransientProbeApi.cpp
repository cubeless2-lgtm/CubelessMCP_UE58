#include "Compatibility/UE58/CubelessControlRigTransientProbeApi.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigAssetReference.h"
#include "Rigs/RigHierarchy.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "EulerTransform.h"
#include "Editor.h"
#include "CoreGlobals.h"
#include "Dom/JsonValue.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/FieldIterator.h"
#include "UObject/Package.h"

namespace Cubeless::UE58::ControlRigTransientProbe
{
namespace
{
// Authoring section for the single API.cpp target. Not an additional module/source target.
using FResult = Variable::FResult;
using FValue = TSharedPtr<FJsonValue>;
constexpr int32 MaxCarrierUnits = 1 << 20;
constexpr int32 MaxSourceRows = 65536;
constexpr int32 MaxCases = 256;
constexpr int32 MaxSamples = 4096;
constexpr int32 MaxEvents = 256;
constexpr int64 MaxWork = 1048576;

bool Live(UObject* Object)
{
    return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_BeginDestroyed | RF_FinishDestroyed);
}

bool TextInput(const FString& Text)
{
    if (Text.Len() > MaxCarrierUnits) { return false; }
    for (TCHAR Ch : Text) { if (Ch == 0) { return false; } }
    return true;
}

bool Decode(const FString& Text, FValue& Out, FString& Error)
{
    if (!TextInput(Text)) { Error = TEXT("JSON carrier exceeds 1Mi UTF16 units or contains NUL"); return false; }
    int32 Depth = 0; bool Quoted = false, Escaped = false;
    for (TCHAR Ch : Text)
    {
        if (Quoted)
        {
            if (Escaped) { Escaped = false; }
            else if (Ch == TCHAR(92)) { Escaped = true; }
            else if (Ch == TCHAR(34)) { Quoted = false; }
        }
        else if (Ch == TCHAR(34)) { Quoted = true; }
        else if (Ch == TEXT('[') || Ch == TEXT('{')) { if (++Depth > 64) { Error = TEXT("JSON carrier exceeds nesting64"); return false; } }
        else if (Ch == TEXT(']') || Ch == TEXT('}')) { if (--Depth < 0) { Error = TEXT("JSON carrier has unbalanced containers"); return false; } }
    }
    if (Quoted || Depth != 0) { Error = TEXT("JSON carrier has an incomplete string or container"); return false; }
    TArray<FValue> Envelope;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Text + TEXT("]")), Envelope, FJsonSerializer::EFlags::None)
        || Envelope.Num() != 1 || !Envelope[0].IsValid())
    { Error = TEXT("JSON carrier must encode exactly one value"); return false; }
    Out = Envelope[0];
    TArray<FValue> Pending {Out}; int32 Slots = 0;
    while (!Pending.IsEmpty())
    {
        const FValue Value = Pending.Pop(EAllowShrinking::No);
        if (!Value.IsValid() || ++Slots > MaxSourceRows) { Error = TEXT("JSON carrier exceeds the finite value-slot boundary"); return false; }
        if (Value->Type == EJson::String && !TextInput(Value->AsString())) { Error = TEXT("Decoded JSON text contains NUL or exceeds its bound"); return false; }
        if (Value->Type == EJson::Number && !FMath::IsFinite(Value->AsNumber())) { Error = TEXT("JSON requires finite numbers"); return false; }
        if (Value->Type == EJson::Array) { Pending.Append(Value->AsArray()); }
        if (Value->Type == EJson::Object)
        {
            for (const auto& Pair : Value->AsObject()->Values)
            {
                if (!TextInput(FString(Pair.Key.ToView()))) { Error = TEXT("Decoded JSON key is invalid"); return false; }
                Pending.Add(Pair.Value);
            }
        }
    }
    return true;
}

TArray<TPair<FString, FValue>> ObjectRows(const TSharedPtr<FJsonObject>& Object)
{
    TArray<TPair<FString, FValue>> Rows;
    if (Object.IsValid())
    { for (const auto& Pair : Object->Values) { Rows.Emplace(FString(Pair.Key.ToView()), Pair.Value); } }
    return Rows;
}

FString GetStringParam(const TSharedPtr<FJsonObject>& Object, const FString& Key, const FString& Default = FString())
{
    FString Text;
    return Object.IsValid() && Object->TryGetStringField(Key, Text) ? Text : Default;
}

struct FIdentity
{
    TStrongObjectPtr<UObject> Object, Class, Outer, Package;
    FString Path, ClassPath, OuterPath, PackagePath;
    explicit FIdentity(UObject* InObject) : Object(InObject), Class(InObject ? InObject->GetClass() : nullptr),
        Outer(InObject ? InObject->GetOuter() : nullptr), Package(InObject ? InObject->GetPackage() : nullptr)
    {
        if (InObject)
        {
            Path = InObject->GetPathName(); ClassPath = Class->GetPathName();
            if (Outer.IsValid()) { OuterPath = Outer->GetPathName(); }
            if (Package.IsValid()) { PackagePath = Package->GetPathName(); }
        }
    }
    bool Current() const
    {
        return Live(Object.Get()) && Live(Class.Get()) && Live(Package.Get())
            && Object->GetClass() == Class.Get() && Object->GetOuter() == Outer.Get() && Object->GetPackage() == Package.Get()
            && Object->GetPathName() == Path && Class->GetPathName() == ClassPath && Package->GetPathName() == PackagePath
            && (!Outer.IsValid() || (Live(Outer.Get()) && Outer->GetPathName() == OuterPath));
    }
};

struct FRun
{
    FResult Result;
    TSharedPtr<FIdentity> Asset, RigClass, Rig, Hierarchy;
    TStrongObjectPtr<UControlRigBlueprint> Blueprint;
    UClass* Class = nullptr;
    UControlRig* Instance = nullptr;
    URigHierarchy* Tree = nullptr;
    int64 Work = 0;
    uint32 SafeTopology = 0;
    bool HasSafeTopology = false;
    bool Failed = false;
    FString Failure;

    void Count(const TCHAR* Key, int32 Amount = 1)
    { double Value = 0; Result.Data->TryGetNumberField(Key, Value); Result.Data->SetNumberField(Key, Value + Amount); }
    bool Bad(const FString& Error)
    { Failed = true; Failure = Error; return false; }
    bool Step(int64 Amount = 1)
    { Work += Amount; return Work <= MaxWork || Bad(TEXT("Transient probe exceeded its finite work budget")); }
    bool Current()
    {
        if (!IsInGameThread() || !GEditor || IsEngineExitRequested()) { return Bad(TEXT("Live editor game thread required")); }
        if (Asset.IsValid() && !Asset->Current()) { return Bad(TEXT("ControlRig source identity/package changed during a callback")); }
        if (RigClass.IsValid() && (!RigClass->Current() || !Class || Class->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Abstract)))
        { return Bad(TEXT("Captured ControlRig generated class changed or became unusable")); }
        if (Class && (!Live(Class->ClassWithin.Get()) || !GetTransientPackage()->IsA(Class->ClassWithin.Get())))
        { return Bad(TEXT("ControlRig ClassWithin no longer permits the owned TransientPackage Outer")); }
        if (Blueprint.IsValid() && Blueprint->GeneratedClass.Get() != Class)
        { return Bad(TEXT("Legacy generated-class binding changed during opaque SDK work")); }
        if (Rig.IsValid() && (!Rig->Current() || !Instance || Instance->GetClass() != Class
            || Instance->GetOuter() != GetTransientPackage() || Instance->GetPackage() != GetTransientPackage()))
        { return Bad(TEXT("Captured ControlRig instance is no longer the owned transient instance")); }
        if (Hierarchy.IsValid() && (!Hierarchy->Current() || !Instance || Instance->GetHierarchy() != Tree
            || Tree->GetOuter() != Instance || Tree->GetPackage() != GetTransientPackage()))
        { return Bad(TEXT("Captured hierarchy identity/package/instance owner changed during a callback")); }
        return Step();
    }
    void Opaque(const TCHAR* Key)
    { Result.bChangeStateKnown = false; Count(Key); }
    void TransientWork(const TCHAR* Key)
    {
        Result.bMutationAttempted = true; Opaque(Key);
        Result.Data->SetBoolField(TEXT("transient_work_attempted"), true);
    }
    bool SafeHierarchy()
    {
        if (!Current() || !Tree) { return false; }
        const uint32 Version = Tree->GetTopologyVersion();
        if (HasSafeTopology && SafeTopology == Version) { return true; }
        if (Tree->Num() > MaxSourceRows) { return Bad(TEXT("Transient hierarchy exceeds the finite element boundary")); }
        const TArray<FRigElementKey> Keys = Tree->GetAllKeys(false);
        if (!Current() || Tree->GetTopologyVersion() != Version || Keys.Num() != Tree->Num())
        { return Bad(TEXT("Hierarchy slots changed while capturing its bounded parent inventory")); }
        TMap<FRigElementKey, TArray<FRigElementKey>> Parents;
        for (const FRigElementKey& Key : Keys)
        {
            const TArray<FRigElementKey> Row = Tree->GetParents(Key, false);
            if (!Current() || Tree->GetTopologyVersion() != Version || !Step(Row.Num() + 1)) { return false; }
            Parents.Add(Key, Row);
        }
        struct FFrame { FRigElementKey Key; int32 Next = 0; };
        TMap<FRigElementKey, uint8> Color;
        TMap<FRigElementKey, int32> DepthByKey;
        for (const FRigElementKey& Start : Keys)
        {
            if (Color.FindRef(Start) == 2) { continue; }
            TArray<FFrame> Stack; Stack.Add({Start, 0}); Color.Add(Start, 1);
            while (!Stack.IsEmpty())
            {
                if (!Step() || Stack.Num() > 64) { return Bad(TEXT("Hierarchy parent graph exceeds depth64")); }
                FFrame& Frame = Stack.Last(); const auto* Row = Parents.Find(Frame.Key);
                if (!Row) { return Bad(TEXT("Hierarchy parent references a missing inventory element")); }
                if (Frame.Next >= Row->Num())
                {
                    int32 LongestDepth = 1;
                    for (const FRigElementKey& Parent : *Row)
                    {
                        if (!Step()) { return false; }
                        const int32* ParentDepth = DepthByKey.Find(Parent);
                        if (!ParentDepth) { return Bad(TEXT("Hierarchy parent depth was not resolved after bounded traversal")); }
                        LongestDepth = FMath::Max(LongestDepth, 1 + *ParentDepth);
                        if (LongestDepth > 64) { return Bad(TEXT("Hierarchy parent graph exceeds depth64")); }
                    }
                    DepthByKey.Add(Frame.Key, LongestDepth);
                    Color.Add(Frame.Key, 2); Stack.Pop(EAllowShrinking::No); continue;
                }
                const FRigElementKey Parent = (*Row)[Frame.Next++];
                const uint8 State = Color.FindRef(Parent);
                if (State == 1) { return Bad(TEXT("Hierarchy contains a parent cycle before recursive transform work")); }
                if (State == 0) { Color.Add(Parent, 1); Stack.Add({Parent, 0}); }
            }
        }
        if (!Current() || Tree->GetTopologyVersion() != Version) { return Bad(TEXT("Hierarchy topology changed during its safety inventory")); }
        HasSafeTopology = true; SafeTopology = Version; return true;
    }
};

FString NormalizeLoadPath(const FString& Path)
{
    FString Normalized = FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();
    Normalized.TrimQuotesInline();
    if ((Normalized.StartsWith(TEXT("/Game/")) || Normalized.StartsWith(TEXT("/Engine/"))) && !Normalized.Contains(TEXT(".")))
    { Normalized = Normalized + TEXT(".") + FPackageName::GetShortName(Normalized); }
    return Normalized;
}

bool ResolveAndCreate(FRun& Run, const FString& Path, const FString& ClassPath, FString& AssetPath, FString& ResolvedClass)
{
    Run.Asset.Reset(); Run.RigClass.Reset(); Run.Rig.Reset(); Run.Hierarchy.Reset(); Run.Blueprint.Reset();
    Run.Class = nullptr; Run.Instance = nullptr; Run.Tree = nullptr; Run.HasSafeTopology = false;
    UObject* Source = nullptr;
    if (!ClassPath.TrimStartAndEnd().IsEmpty())
    {
        Run.Opaque(TEXT("class_load_calls"));
        Run.Class = LoadObject<UClass>(nullptr, *NormalizeLoadPath(ClassPath));
        if (!Run.Class)
        { Run.Opaque(TEXT("class_find_calls")); Run.Class = FindFirstObject<UClass>(*ClassPath); }
        if (!Run.Class)
        { Run.Opaque(TEXT("class_load_calls")); Run.Class = LoadObject<UClass>(nullptr, *(TEXT("/Script/Engine.") + ClassPath)); }
        if (!Live(Run.Class) || !Run.Class->IsChildOf(UControlRig::StaticClass()))
        { return Run.Bad(FString::Printf(TEXT("ControlRig class not found or invalid: %s"), *ClassPath)); }
        Source = Run.Class;
    }
    else if (!Path.TrimStartAndEnd().IsEmpty())
    {
        Run.Opaque(TEXT("asset_load_calls")); Source = LoadObject<UObject>(nullptr, *NormalizeLoadPath(Path));
        if (!Live(Source)) { return Run.Bad(FString::Printf(TEXT("ControlRig asset/class not found or invalid: %s"), *Path)); }
        Run.Asset = MakeShared<FIdentity>(Source);
        if (UControlRigBlueprint* Legacy = Cast<UControlRigBlueprint>(Source))
        {
            if (Legacy->GetClass() != UControlRigBlueprint::StaticClass())
            { return Run.Bad(TEXT("Custom ControlRigBlueprint subclasses are unsupported by the transient public construction adaptation; their overridden creation/reference behavior is not bypassed")); }
            Run.Blueprint.Reset(Legacy); Run.Opaque(TEXT("asset_reference_calls"));
            const FControlRigAssetStrongReference Ref = Legacy->GetControlRigAssetReference();
            if (!Run.Asset->Current()) { return Run.Bad(TEXT("ControlRig source changed during public asset-reference callback")); }
            Run.Class = Ref.GetBlueprintClass().Get(); AssetPath = Source->GetPathName();
        }
        else
        {
            Run.Class = Cast<UClass>(Source); if (!Run.Class) { Run.Class = Source->GetClass(); }
            if (!Run.Class->IsChildOf(UControlRig::StaticClass()))
            { return Run.Bad(TEXT("Only the legacy Blueprint/generated ControlRig class representation is supported by the original property contract; new runtime-asset variable storage is not approximated")); }
            AssetPath = Source->GetPathName();
        }
    }
    else { return Run.Bad(TEXT("Missing 'control_rig_path' or 'control_rig_class' parameter")); }
    if (!Live(Run.Class) || !Run.Class->IsChildOf(UControlRig::StaticClass()) || Run.Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists))
    { return Run.Bad(TEXT("Resolved ControlRig class is missing, abstract or superseded")); }
    if (!Live(Run.Class->ClassWithin.Get()) || !GetTransientPackage()->IsA(Run.Class->ClassWithin.Get()))
    { return Run.Bad(TEXT("Resolved ControlRig ClassWithin is incompatible with the required TransientPackage Outer; no instance was created")); }
    Run.RigClass = MakeShared<FIdentity>(Run.Class); ResolvedClass = Run.Class->GetPathName();
    if (!Run.Asset.IsValid()) { Run.Asset = MakeShared<FIdentity>(Source); }
    Run.Result.Data->SetStringField(TEXT("captured_source_package"), Run.Asset->PackagePath);
    Run.Result.Data->SetBoolField(TEXT("captured_source_dirty_after_load"), Source->GetPackage()->IsDirty());
    if (!Run.Current()) { return false; }
    if (Run.Blueprint.IsValid())
    {
        // UE58 CreateControlRig's helper uses the editor asset as Outer. Keep
        // its public preparation order, but create an RF_Transient package-owned
        // instance from the outset; do not create/rename an asset-owned child.
        Run.Count(TEXT("legacy_create_route_adapted_calls"));
        Run.Result.bCompileRequested = true; Run.Result.bCompileAttempted = true;
        Run.Result.bMutationAttempted = true;
        Run.Opaque(TEXT("vm_recompile_check_calls"));
        Run.Blueprint->RecompileVMIfRequired();
        if (!Run.Current()) { return false; }
        Run.TransientWork(TEXT("transient_create_attempts"));
        Run.Instance = NewObject<UControlRig>(GetTransientPackage(), Run.Class, NAME_None, RF_Transient);
    }
    else
    { Run.TransientWork(TEXT("transient_create_attempts")); Run.Instance = NewObject<UControlRig>(GetTransientPackage(), Run.Class, NAME_None, RF_Transient); }
    if (!Live(Run.Instance)) { return Run.Bad(TEXT("Failed to create the transient ControlRig instance")); }
    Run.Count(TEXT("transient_create_returned_count"));
    Run.Result.bChanged = true; Run.Result.Data->SetBoolField(TEXT("observed_explicit_transient_change"), true);
    Run.Rig = MakeShared<FIdentity>(Run.Instance);
    if (!Run.Current()) { return false; }
    if (Run.Blueprint.IsValid())
    {
        Run.TransientWork(TEXT("compatibility_prepare_initialize_calls"));
        Run.Instance->Initialize(true);
    }
    return Run.Current();
}


// Original UE56 Shared.h 1959-2139; pure-data parser/serializer contract.
TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Vector)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
    return Values;
}

TSharedPtr<FJsonValue> VectorToJsonValue(const FVector& Vector)
{
    return MakeShared<FJsonValueArray>(VectorToJsonArray(Vector));
}

bool IsFiniteControlRigProbeFloatRange(const double Value)
{
    return FMath::IsFinite(Value) &&
        FMath::Abs(Value) <= static_cast<double>(TNumericLimits<float>::Max());
}

bool TryParseControlRigProbeName(
    const FString& RequestedName,
    const FString& FieldPath,
    FName& OutName,
    FString& OutError,
    const bool bAllowNone = false)
{
    OutError.Reset();
    const FString NormalizedName = RequestedName.TrimStartAndEnd();
    if (NormalizedName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), *FieldPath);
        return false;
    }
    if (NormalizedName.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' is too long for an Unreal name (maximum %d characters)"),
            *FieldPath,
            NAME_SIZE - 1);
        return false;
    }

    OutName = FName(*NormalizedName);
    if (!bAllowNone && OutName.IsNone())
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' name '%s': name resolves to None"),
            *FieldPath,
            *RequestedName);
        return false;
    }
    return true;
}

bool TryGetNumberFromObjectField(const TSharedPtr<FJsonObject>& Object, const FString& LowerName, const FString& UpperName, double& OutValue)
{
    return Object.IsValid() &&
        (Object->TryGetNumberField(LowerName, OutValue) || Object->TryGetNumberField(UpperName, OutValue)) &&
        IsFiniteControlRigProbeFloatRange(OutValue);
}

bool JsonValueToVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
{
    if (!Value.IsValid())
    {
        return false;
    }

    if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        if (Values.Num() < 3)
        {
            return false;
        }
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!Values[0].IsValid() || Values[0]->Type != EJson::Number || !Values[0]->TryGetNumber(X) ||
            !Values[1].IsValid() || Values[1]->Type != EJson::Number || !Values[1]->TryGetNumber(Y) ||
            !Values[2].IsValid() || Values[2]->Type != EJson::Number || !Values[2]->TryGetNumber(Z) ||
            !IsFiniteControlRigProbeFloatRange(X) ||
            !IsFiniteControlRigProbeFloatRange(Y) ||
            !IsFiniteControlRigProbeFloatRange(Z))
        {
            return false;
        }
        OutVector = FVector(X, Y, Z);
        return true;
    }

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!TryGetNumberFromObjectField(Object, TEXT("x"), TEXT("X"), X) ||
            !TryGetNumberFromObjectField(Object, TEXT("y"), TEXT("Y"), Y) ||
            !TryGetNumberFromObjectField(Object, TEXT("z"), TEXT("Z"), Z))
        {
            return false;
        }
        OutVector = FVector(X, Y, Z);
        return true;
    }

    return false;
}

bool JsonValueToRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator)
{
    FVector Vector;
    if (JsonValueToVector(Value, Vector))
    {
        OutRotator = FRotator(Vector.X, Vector.Y, Vector.Z);
        return true;
    }
    return false;
}

bool JsonValueToTransform(const TSharedPtr<FJsonValue>& Value, FTransform& OutTransform)
{
    if (!Value.IsValid() || Value->Type != EJson::Object)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Object = Value->AsObject();
    if (!Object.IsValid())
    {
        return false;
    }

    FVector Translation = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;
    bool bHasAnyField = false;

    const TSharedPtr<FJsonValue> TranslationValue = Object->TryGetField(TEXT("translation")).IsValid()
        ? Object->TryGetField(TEXT("translation"))
        : Object->TryGetField(TEXT("location"));
    if (TranslationValue.IsValid())
    {
        if (!JsonValueToVector(TranslationValue, Translation))
        {
            return false;
        }
        bHasAnyField = true;
    }

    if (const TSharedPtr<FJsonValue> RotationValue = Object->TryGetField(TEXT("rotation")))
    {
        if (!JsonValueToRotator(RotationValue, Rotation))
        {
            return false;
        }
        bHasAnyField = true;
    }

    const TSharedPtr<FJsonValue> ScaleValue = Object->TryGetField(TEXT("scale")).IsValid()
        ? Object->TryGetField(TEXT("scale"))
        : Object->TryGetField(TEXT("scale3d"));
    if (ScaleValue.IsValid())
    {
        if (!JsonValueToVector(ScaleValue, Scale))
        {
            return false;
        }
        bHasAnyField = true;
    }

    if (!bHasAnyField)
    {
        return false;
    }

    OutTransform = FTransform(Rotation, Translation, Scale);
    return true;
}

// Original UE56 Shared.h 4751-4884; parser and read-only hierarchy resolver contract.
FString RigElementTypeToString(ERigElementType Type)
{
    switch (Type)
    {
    case ERigElementType::Bone:
        return TEXT("Bone");
    case ERigElementType::Null:
        return TEXT("Null");
    case ERigElementType::Control:
        return TEXT("Control");
    case ERigElementType::Curve:
        return TEXT("Curve");
    case ERigElementType::Reference:
        return TEXT("Reference");
    case ERigElementType::Connector:
        return TEXT("Connector");
    case ERigElementType::Socket:
        return TEXT("Socket");
    default:
        return TEXT("None");
    }
}

bool ParseRigElementType(const FString& RawValue, ERigElementType& OutType)
{
    const FString Value = RawValue.TrimStartAndEnd().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT(""));
    if (Value.Equals(TEXT("Bone"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Bone;
        return true;
    }
    if (Value.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Null;
        return true;
    }
    if (Value.Equals(TEXT("Control"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Control;
        return true;
    }
    if (Value.Equals(TEXT("Curve"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Curve;
        return true;
    }
    if (Value.Equals(TEXT("Reference"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Reference;
        return true;
    }
    if (Value.Equals(TEXT("Connector"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Connector;
        return true;
    }
    if (Value.Equals(TEXT("Socket"), ESearchCase::IgnoreCase))
    {
        OutType = ERigElementType::Socket;
        return true;
    }
    return false;
}

bool ResolveRigElementKey(
    URigHierarchy* Hierarchy,
    const FString& ElementSpec,
    FRigElementKey& OutKey,
    FString* OutError = nullptr)
{
    if (OutError)
    {
        OutError->Reset();
    }
    if (!Hierarchy)
    {
        if (OutError)
        {
            *OutError = TEXT("ControlRig hierarchy is null");
        }
        return false;
    }

    FString TypeText;
    FString NameText = ElementSpec;
    const bool bHasExplicitType = ElementSpec.Split(TEXT(":"), &TypeText, &NameText);

    FName ElementName = NAME_None;
    FString NameError;
    if (!TryParseControlRigProbeName(NameText, TEXT("sample element name"), ElementName, NameError))
    {
        if (OutError)
        {
            *OutError = NameError;
        }
        return false;
    }

    if (bHasExplicitType)
    {
        ERigElementType ParsedType = ERigElementType::None;
        if (ParseRigElementType(TypeText, ParsedType))
        {
            const FRigElementKey TypedKey(ElementName, ParsedType);
            if (Hierarchy->Contains(TypedKey))
            {
                OutKey = TypedKey;
                return true;
            }
        }
    }

    static const ERigElementType ProbeTypes[] = {
        ERigElementType::Bone,
        ERigElementType::Control,
        ERigElementType::Null,
        ERigElementType::Reference,
        ERigElementType::Connector,
        ERigElementType::Socket,
        ERigElementType::Curve
    };

    for (const ERigElementType Type : ProbeTypes)
    {
        const FRigElementKey Candidate(ElementName, Type);
        if (Hierarchy->Contains(Candidate))
        {
            OutKey = Candidate;
            return true;
        }
    }

    return false;
}

// Original UE56 Shared.h 5009-5425; pure-data parser/serializer contract.
TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetArrayField(TEXT("translation"), VectorToJsonArray(Transform.GetLocation()));

    const FQuat Rotation = Transform.GetRotation();
    TArray<TSharedPtr<FJsonValue>> QuatValues;
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.X));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.Y));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.Z));
    QuatValues.Add(MakeShared<FJsonValueNumber>(Rotation.W));
    Object->SetArrayField(TEXT("rotation"), QuatValues);

    const FRotator Rotator = Rotation.Rotator();
    TArray<TSharedPtr<FJsonValue>> RotatorValues;
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Pitch));
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Yaw));
    RotatorValues.Add(MakeShared<FJsonValueNumber>(Rotator.Roll));
    Object->SetArrayField(TEXT("rotation_euler"), RotatorValues);

    Object->SetArrayField(TEXT("scale"), VectorToJsonArray(Transform.GetScale3D()));
    return Object;
}

struct FControlRigDirectProbeCase
{
    FString Name;
    TMap<FString, TSharedPtr<FJsonValue>> PropertyValues;
    TMap<FName, double> CurveValues;
    TMap<FName, TSharedPtr<FJsonValue>> ControlValues;
    TArray<FString> InputErrors;
};

void AddProbeProperty(FControlRigDirectProbeCase& ProbeCase, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value)
{
    FName ValidatedPropertyName = NAME_None;
    FString NameError;
    if (!TryParseControlRigProbeName(PropertyName, TEXT("property name"), ValidatedPropertyName, NameError))
    {
        ProbeCase.InputErrors.Add(NameError);
        return;
    }
    if (!Value.IsValid())
    {
        ProbeCase.InputErrors.Add(FString::Printf(
            TEXT("Property '%s' has no value"),
            *PropertyName));
        return;
    }
    ProbeCase.PropertyValues.Add(ValidatedPropertyName.ToString(), Value);
}

FControlRigDirectProbeCase MakeDefaultControlRigProbeCase(
    const FString& Name,
    bool bShouldTrace,
    const FVector& InteractionLocation,
    double IKBlendL,
    double IKBlendInteract,
    const FString& ShouldTracePropertyName,
    const FString& InteractionLocationPropertyName)
{
    FControlRigDirectProbeCase ProbeCase;
    ProbeCase.Name = Name;
    AddProbeProperty(ProbeCase, ShouldTracePropertyName, MakeShared<FJsonValueBoolean>(bShouldTrace));
    AddProbeProperty(ProbeCase, InteractionLocationPropertyName, VectorToJsonValue(InteractionLocation));
    ProbeCase.CurveValues.Add(FName(TEXT("IKBlend_l")), IKBlendL);
    ProbeCase.CurveValues.Add(FName(TEXT("IK_blend_interact")), IKBlendInteract);
    return ProbeCase;
}

TArray<FControlRigDirectProbeCase> BuildDefaultControlRigProbeCases(
    const FString& ShouldTracePropertyName,
    const FString& InteractionLocationPropertyName)
{
    TArray<FControlRigDirectProbeCase> Cases;
    Cases.Add(MakeDefaultControlRigProbeCase(TEXT("baseline_false_curves0"), false, FVector::ZeroVector, 0.0, 0.0, ShouldTracePropertyName, InteractionLocationPropertyName));
    Cases.Add(MakeDefaultControlRigProbeCase(TEXT("trace_true_curves0"), true, FVector::ZeroVector, 0.0, 0.0, ShouldTracePropertyName, InteractionLocationPropertyName));
    Cases.Add(MakeDefaultControlRigProbeCase(TEXT("trace_true_ikblend_l1"), true, FVector::ZeroVector, 1.0, 0.0, ShouldTracePropertyName, InteractionLocationPropertyName));
    Cases.Add(MakeDefaultControlRigProbeCase(TEXT("trace_true_interact_curve1_zero_loc"), true, FVector::ZeroVector, 0.0, 1.0, ShouldTracePropertyName, InteractionLocationPropertyName));
    Cases.Add(MakeDefaultControlRigProbeCase(TEXT("trace_true_all_curves1_high_loc"), true, FVector(0.0, 0.0, 120.0), 1.0, 1.0, ShouldTracePropertyName, InteractionLocationPropertyName));
    Cases.Add(MakeDefaultControlRigProbeCase(TEXT("trace_true_all_curves1_side_loc"), true, FVector(80.0, -40.0, 80.0), 1.0, 1.0, ShouldTracePropertyName, InteractionLocationPropertyName));
    return Cases;
}

void AddJsonObjectFieldsAsProperties(FControlRigDirectProbeCase& ProbeCase, const TSharedPtr<FJsonObject>& Object)
{
    if (!Object.IsValid())
    {
        return;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ObjectRows(Object))
    {
        AddProbeProperty(ProbeCase, Pair.Key, Pair.Value);
    }
}

void AddJsonObjectFieldsAsControlValues(
    FControlRigDirectProbeCase& ProbeCase,
    const TSharedPtr<FJsonObject>& Object,
    const FString& FieldPath)
{
    if (!Object.IsValid())
    {
        return;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ObjectRows(Object))
    {
        FName ControlName = NAME_None;
        FString NameError;
        if (!TryParseControlRigProbeName(
                Pair.Key,
                FString::Printf(TEXT("%s.%s"), *FieldPath, *Pair.Key),
                ControlName,
                NameError))
        {
            ProbeCase.InputErrors.Add(NameError);
            continue;
        }
        if (!Pair.Value.IsValid())
        {
            ProbeCase.InputErrors.Add(FString::Printf(
                TEXT("'%s.%s' has no value"),
                *FieldPath,
                *Pair.Key));
            continue;
        }
        ProbeCase.ControlValues.Add(ControlName, Pair.Value);
    }
}

void AddJsonObjectFieldsAsCurveValues(
    FControlRigDirectProbeCase& ProbeCase,
    const TSharedPtr<FJsonObject>& Object,
    const FString& FieldPath)
{
    if (!Object.IsValid())
    {
        return;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ObjectRows(Object))
    {
        FName CurveName = NAME_None;
        FString NameError;
        if (!TryParseControlRigProbeName(
                Pair.Key,
                FString::Printf(TEXT("%s.%s"), *FieldPath, *Pair.Key),
                CurveName,
                NameError))
        {
            ProbeCase.InputErrors.Add(NameError);
            continue;
        }

        double CurveValue = 0.0;
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number ||
            !Pair.Value->TryGetNumber(CurveValue) ||
            !IsFiniteControlRigProbeFloatRange(CurveValue))
        {
            ProbeCase.InputErrors.Add(FString::Printf(
                TEXT("'%s.%s' requires a finite number within float range"),
                *FieldPath,
                *Pair.Key));
            continue;
        }
        ProbeCase.CurveValues.Add(CurveName, CurveValue);
    }
}

TArray<FControlRigDirectProbeCase> ParseControlRigProbeCases(
    const TSharedPtr<FJsonObject>& Params,
    const FString& ShouldTracePropertyName,
    const FString& InteractionLocationPropertyName)
{
    if (!Params.IsValid())
    {
        return BuildDefaultControlRigProbeCases(ShouldTracePropertyName, InteractionLocationPropertyName);
    }

    const TArray<TSharedPtr<FJsonValue>>* CaseValues = nullptr;
    if (!Params->TryGetArrayField(TEXT("cases"), CaseValues) || !CaseValues || CaseValues->Num() == 0)
    {
        TArray<FControlRigDirectProbeCase> DefaultCases = BuildDefaultControlRigProbeCases(
            ShouldTracePropertyName,
            InteractionLocationPropertyName);
        const TSharedPtr<FJsonObject>* ControlsObject = nullptr;
        const bool bHasControlValues = Params->HasField(TEXT("control_values"));
        const bool bControlValuesAreObject = Params->TryGetObjectField(TEXT("control_values"), ControlsObject)
            && ControlsObject
            && ControlsObject->IsValid();
        if (bControlValuesAreObject)
        {
            for (FControlRigDirectProbeCase& DefaultCase : DefaultCases)
            {
                AddJsonObjectFieldsAsControlValues(DefaultCase, *ControlsObject, TEXT("control_values"));
            }
        }
        else if (bHasControlValues)
        {
            for (FControlRigDirectProbeCase& DefaultCase : DefaultCases)
            {
                DefaultCase.InputErrors.Add(TEXT("'control_values' must be an object"));
            }
        }
        return DefaultCases;
    }

    const TSharedPtr<FJsonObject>* DefaultControlsObject = nullptr;
    const bool bHasDefaultControlValues = Params->HasField(TEXT("control_values"));
    const bool bDefaultControlValuesAreObject = Params->TryGetObjectField(
        TEXT("control_values"),
        DefaultControlsObject) && DefaultControlsObject && DefaultControlsObject->IsValid();

    TArray<FControlRigDirectProbeCase> Cases;
    for (int32 CaseIndex = 0; CaseIndex < CaseValues->Num(); ++CaseIndex)
    {
        const TSharedPtr<FJsonValue>& CaseValue = (*CaseValues)[CaseIndex];
        if (!CaseValue.IsValid() || CaseValue->Type != EJson::Object)
        {
            continue;
        }

        const TSharedPtr<FJsonObject> CaseObject = CaseValue->AsObject();
        if (!CaseObject.IsValid())
        {
            continue;
        }

        FControlRigDirectProbeCase ProbeCase;
        ProbeCase.Name = GetStringParam(CaseObject, TEXT("name"), FString::Printf(TEXT("case_%d"), CaseIndex));

        if (bDefaultControlValuesAreObject)
        {
            AddJsonObjectFieldsAsControlValues(ProbeCase, *DefaultControlsObject, TEXT("control_values"));
        }
        else if (bHasDefaultControlValues)
        {
            ProbeCase.InputErrors.Add(TEXT("'control_values' must be an object"));
        }

        const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
        if (CaseObject->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject)
        {
            AddJsonObjectFieldsAsProperties(ProbeCase, *PropertiesObject);
        }

        const TSharedPtr<FJsonObject>* ControlsObject = nullptr;
        const bool bHasCaseControlValues = CaseObject->HasField(TEXT("control_values"));
        if (CaseObject->TryGetObjectField(TEXT("control_values"), ControlsObject)
            && ControlsObject
            && ControlsObject->IsValid())
        {
            AddJsonObjectFieldsAsControlValues(
                ProbeCase,
                *ControlsObject,
                FString::Printf(TEXT("cases[%d].control_values"), CaseIndex));
        }
        else if (bHasCaseControlValues)
        {
            ProbeCase.InputErrors.Add(FString::Printf(
                TEXT("'cases[%d].control_values' must be an object"),
                CaseIndex));
        }

        if (const TSharedPtr<FJsonValue> ShouldTraceValue = CaseObject->TryGetField(TEXT("should_trace")))
        {
            AddProbeProperty(ProbeCase, ShouldTracePropertyName, ShouldTraceValue);
        }

        TSharedPtr<FJsonValue> LocationValue = CaseObject->TryGetField(TEXT("interaction_world_location"));
        if (!LocationValue.IsValid())
        {
            LocationValue = CaseObject->TryGetField(TEXT("loc"));
        }
        if (LocationValue.IsValid())
        {
            AddProbeProperty(ProbeCase, InteractionLocationPropertyName, LocationValue);
        }

        const TSharedPtr<FJsonObject>* CurvesObject = nullptr;
        if (CaseObject->TryGetObjectField(TEXT("curves"), CurvesObject) && CurvesObject && CurvesObject->IsValid())
        {
            AddJsonObjectFieldsAsCurveValues(
                ProbeCase,
                *CurvesObject,
                FString::Printf(TEXT("cases[%d].curves"), CaseIndex));
        }

        Cases.Add(ProbeCase);
    }

    if (Cases.Num() > 0)
    {
        return Cases;
    }

    TArray<FControlRigDirectProbeCase> DefaultCases = BuildDefaultControlRigProbeCases(
        ShouldTracePropertyName,
        InteractionLocationPropertyName);
    if (bDefaultControlValuesAreObject)
    {
        for (FControlRigDirectProbeCase& DefaultCase : DefaultCases)
        {
            AddJsonObjectFieldsAsControlValues(DefaultCase, *DefaultControlsObject, TEXT("control_values"));
        }
    }
    else if (bHasDefaultControlValues)
    {
        for (FControlRigDirectProbeCase& DefaultCase : DefaultCases)
        {
            DefaultCase.InputErrors.Add(TEXT("'control_values' must be an object"));
        }
    }
    return DefaultCases;
}

FControlRigDirectProbeCase BuildDefaultControlRigPrePostCase(
    const FString& ShouldTracePropertyName,
    const FString& InteractionLocationPropertyName)
{
    return MakeDefaultControlRigProbeCase(
        TEXT("forced_driver_side"),
        true,
        FVector(80.0, -40.0, 80.0),
        1.0,
        1.0,
        ShouldTracePropertyName,
        InteractionLocationPropertyName);
}

TArray<FControlRigDirectProbeCase> ParseControlRigPrePostCases(
    const TSharedPtr<FJsonObject>& Params,
    const FString& ShouldTracePropertyName,
    const FString& InteractionLocationPropertyName)
{
    if (!Params.IsValid())
    {
        TArray<FControlRigDirectProbeCase> Cases;
        Cases.Add(BuildDefaultControlRigPrePostCase(ShouldTracePropertyName, InteractionLocationPropertyName));
        return Cases;
    }

    const TArray<TSharedPtr<FJsonValue>>* CaseValues = nullptr;
    if (Params->TryGetArrayField(TEXT("cases"), CaseValues) && CaseValues && CaseValues->Num() > 0)
    {
        return ParseControlRigProbeCases(Params, ShouldTracePropertyName, InteractionLocationPropertyName);
    }

    FControlRigDirectProbeCase ProbeCase;
    ProbeCase.Name = GetStringParam(Params, TEXT("name"), TEXT("forced_driver_side"));

    const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObject) && PropertiesObject)
    {
        AddJsonObjectFieldsAsProperties(ProbeCase, *PropertiesObject);
    }
    if (Params->TryGetObjectField(TEXT("input_defaults"), PropertiesObject) && PropertiesObject)
    {
        AddJsonObjectFieldsAsProperties(ProbeCase, *PropertiesObject);
    }

    const TSharedPtr<FJsonObject>* ControlsObject = nullptr;
    const bool bHasControlValues = Params->HasField(TEXT("control_values"));
    if (Params->TryGetObjectField(TEXT("control_values"), ControlsObject)
        && ControlsObject
        && ControlsObject->IsValid())
    {
        AddJsonObjectFieldsAsControlValues(ProbeCase, *ControlsObject, TEXT("control_values"));
    }
    else if (bHasControlValues)
    {
        ProbeCase.InputErrors.Add(TEXT("'control_values' must be an object"));
    }

    if (const TSharedPtr<FJsonValue> ShouldTraceValue = Params->TryGetField(TEXT("should_trace")))
    {
        AddProbeProperty(ProbeCase, ShouldTracePropertyName, ShouldTraceValue);
    }
    else if (!ProbeCase.PropertyValues.Contains(ShouldTracePropertyName))
    {
        AddProbeProperty(ProbeCase, ShouldTracePropertyName, MakeShared<FJsonValueBoolean>(true));
    }

    TSharedPtr<FJsonValue> LocationValue = Params->TryGetField(TEXT("interaction_world_location"));
    if (!LocationValue.IsValid())
    {
        LocationValue = Params->TryGetField(TEXT("loc"));
    }
    if (LocationValue.IsValid())
    {
        AddProbeProperty(ProbeCase, InteractionLocationPropertyName, LocationValue);
    }
    else if (!ProbeCase.PropertyValues.Contains(InteractionLocationPropertyName))
    {
        AddProbeProperty(ProbeCase, InteractionLocationPropertyName, VectorToJsonValue(FVector(80.0, -40.0, 80.0)));
    }

    const TSharedPtr<FJsonObject>* CurvesObject = nullptr;
    if ((Params->TryGetObjectField(TEXT("curve_values"), CurvesObject) || Params->TryGetObjectField(TEXT("curves"), CurvesObject)) && CurvesObject && CurvesObject->IsValid())
    {
        AddJsonObjectFieldsAsCurveValues(ProbeCase, *CurvesObject, TEXT("curve_values"));
    }

    if (!ProbeCase.CurveValues.Contains(FName(TEXT("IKBlend_l"))))
    {
        ProbeCase.CurveValues.Add(FName(TEXT("IKBlend_l")), 1.0);
    }
    if (!ProbeCase.CurveValues.Contains(FName(TEXT("IK_blend_interact"))))
    {
        ProbeCase.CurveValues.Add(FName(TEXT("IK_blend_interact")), 1.0);
    }

    TArray<FControlRigDirectProbeCase> Cases;
    Cases.Add(ProbeCase);
    return Cases;
}

// Original UE56 Shared.h 5561-5628; pure-data parser/serializer contract.
bool ParseControlRigProbeEventNames(
    const TArray<FString>& RequestedEvents,
    TArray<FName>& OutEventNames,
    FString& OutError)
{
    OutEventNames.Reset();
    OutError.Reset();
    OutEventNames.Reserve(RequestedEvents.Num());
    for (int32 EventIndex = 0; EventIndex < RequestedEvents.Num(); ++EventIndex)
    {
        FName EventName = NAME_None;
        if (!TryParseControlRigProbeName(
                RequestedEvents[EventIndex],
                FString::Printf(TEXT("execute_events[%d]"), EventIndex),
                EventName,
                OutError))
        {
            OutEventNames.Reset();
            return false;
        }
        OutEventNames.Add(EventName);
    }
    return true;
}

TArray<FString> GetStringArrayParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, const TArray<FString>& Defaults)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(FieldName, Values) || !Values || Values->Num() == 0)
    {
        return Defaults;
    }

    TArray<FString> Result;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        if (!Value.IsValid())
        {
            continue;
        }
        if (Value->Type == EJson::String)
        {
            Result.Add(Value->AsString());
        }
        else if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (Object.IsValid())
            {
                FString Name;
                if (Object->TryGetStringField(TEXT("name"), Name))
                {
                    FString Type;
                    if (Object->TryGetStringField(TEXT("type"), Type) && !Type.IsEmpty())
                    {
                        Result.Add(FString::Printf(TEXT("%s:%s"), *Type, *Name));
                    }
                    else
                    {
                        Result.Add(Name);
                    }
                }
            }
        }
    }

    return Result.Num() > 0 ? Result : Defaults;
}

// Original UE56 Shared.h 5678-5709; pure-data parser/serializer contract.
TSharedPtr<FJsonObject> TransformDeltaToJsonObject(const FTransform& PreTransform, const FTransform& PostTransform)
{
    const FVector TranslationDelta = PostTransform.GetLocation() - PreTransform.GetLocation();
    const FVector ScaleDelta = PostTransform.GetScale3D() - PreTransform.GetScale3D();
    const FQuat PreRotation = PreTransform.GetRotation().GetNormalized();
    const FQuat PostRotation = PostTransform.GetRotation().GetNormalized();

    TSharedPtr<FJsonObject> DeltaObject = MakeShared<FJsonObject>();
    DeltaObject->SetArrayField(TEXT("translation_delta"), VectorToJsonArray(TranslationDelta));
    DeltaObject->SetNumberField(TEXT("translation_distance"), TranslationDelta.Size());
    DeltaObject->SetNumberField(TEXT("rotation_delta_degrees"), FMath::RadiansToDegrees(PreRotation.AngularDistance(PostRotation)));
    DeltaObject->SetArrayField(TEXT("scale_delta"), VectorToJsonArray(ScaleDelta));
    return DeltaObject;
}

TSharedPtr<FJsonObject> BuildRigPoseDeltaObject(
    const TMap<FString, FTransform>& PreTransforms,
    const TMap<FString, FTransform>& PostTransforms)
{
    TSharedPtr<FJsonObject> DeltaObject = MakeShared<FJsonObject>();
    for (const TPair<FString, FTransform>& Pair : PostTransforms)
    {
        const FTransform* PreTransform = PreTransforms.Find(Pair.Key);
        if (!PreTransform)
        {
            continue;
        }

        DeltaObject->SetObjectField(Pair.Key, TransformDeltaToJsonObject(*PreTransform, Pair.Value));
    }
    return DeltaObject;
}

TSharedPtr<FJsonValue> EchoProperty(const FProperty* Property, const void* Memory, int32 Depth = 0)
{
    if (!Property || !Memory || Depth > 3) { return MakeShared<FJsonValueNull>(); }
    if (const FBoolProperty* Typed = CastField<FBoolProperty>(Property)) { return MakeShared<FJsonValueBoolean>(Typed->GetPropertyValue(Memory)); }
    if (const FByteProperty* Typed = CastField<FByteProperty>(Property))
    {
        const uint8 Value = Typed->GetPropertyValue(Memory);
        if (Typed->Enum)
        {
            auto Object = MakeShared<FJsonObject>(); Object->SetNumberField(TEXT("value"), Value);
            Object->SetStringField(TEXT("name"), Typed->Enum->GetNameStringByValue(Value));
            Object->SetStringField(TEXT("enum_type"), Typed->Enum->GetName()); return MakeShared<FJsonValueObject>(Object);
        }
        return MakeShared<FJsonValueNumber>(Value);
    }
    if (const FNumericProperty* Typed = CastField<FNumericProperty>(Property))
    {
        if (Typed->IsFloatingPoint()) { return MakeShared<FJsonValueNumber>(Typed->GetFloatingPointPropertyValue(Memory)); }
        const int64 Value = CastField<FUInt64Property>(Typed) || CastField<FUInt32Property>(Typed) || CastField<FUInt16Property>(Typed)
            ? static_cast<int64>(Typed->GetUnsignedIntPropertyValue(Memory)) : Typed->GetSignedIntPropertyValue(Memory);
        return MakeShared<FJsonValueNumber>(static_cast<double>(Value));
    }
    if (const FNameProperty* Typed = CastField<FNameProperty>(Property)) { return MakeShared<FJsonValueString>(Typed->GetPropertyValue(Memory).ToString()); }
    if (const FStrProperty* Typed = CastField<FStrProperty>(Property)) { return MakeShared<FJsonValueString>(Typed->GetPropertyValue(Memory)); }
    if (const FStructProperty* Typed = CastField<FStructProperty>(Property))
    {
        auto Object = MakeShared<FJsonObject>(); Object->SetStringField(TEXT("_struct_type"), Typed->Struct->GetName());
        if (Depth == 3)
        {
            FString Export; Property->ExportTextItem_Direct(Export, Memory, nullptr, nullptr, PPF_None);
            Object->SetStringField(TEXT("_export_text"), Export);
        }
        else
        {
            // Only the accepted fixed Core math structs reach this path, not
            // arbitrary user structs/containers or an unbounded public exporter.
            int32 Fields = 0;
            for (TFieldIterator<FProperty> It(Typed->Struct, EFieldIterationFlags::IncludeSuper); It && ++Fields <= 64; ++It)
            { Object->SetField(It->GetName(), EchoProperty(*It, It->ContainerPtrToValuePtr<void>(Memory), Depth + 1)); }
        }
        return MakeShared<FJsonValueObject>(Object);
    }
    return MakeShared<FJsonValueNull>();
}

bool ApplyProperty(FRun& Run, const FString& Name, const FValue& Value, FValue& Echo, FString& Error)
{
    FName Key;
    if (!TryParseControlRigProbeName(Name, TEXT("property name"), Key, Error)) { return false; }
    if (!Run.Current()) { Error = Run.Failure; return false; }
    FProperty* Property = Run.Class->FindPropertyByName(Key);
    if (!Property) { Error = FString::Printf(TEXT("Property '%s' not found on %s"), *Name, *Run.Class->GetName()); return false; }
    TStrongObjectPtr<UStruct> Owner(Property->GetOwnerStruct());
    const FIdentity OwnerIdentity(Owner.Get());
    const int32 ArrayDim = Property->ArrayDim; const int32 ElementSize = Property->GetElementSize();
    const int32 Offset = Property->GetOffset_ForInternal();
    auto Current = [&]() -> bool
    {
        if (!Run.Current()) { return false; }
        return OwnerIdentity.Current() && Run.Class->FindPropertyByName(Key) == Property
            && Property->GetOwnerStruct() == Owner.Get() && Property->ArrayDim == ArrayDim && Property->GetElementSize() == ElementSize
            && Property->GetOffset_ForInternal() == Offset
            || Run.Bad(TEXT("Captured property owner/definition/storage offset changed during access"));
    };
    if (!Current() || !Value.IsValid()) { Error = TEXT("Property definition or value became unavailable"); return false; }
    void* Memory = Property->ContainerPtrToValuePtr<void>(Run.Instance);
    if (!Memory) { Error = FString::Printf(TEXT("Could not resolve property memory for '%s'"), *Name); return false; }
    bool Applied = false;
    if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        if (Value->Type != EJson::Boolean) { Error = FString::Printf(TEXT("Property '%s' expects a boolean"), *Name); return false; }
        Run.TransientWork(TEXT("property_set_attempts")); BoolProperty->SetPropertyValue(Memory, Value->AsBool()); Applied = true;
    }
    else if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        if (Value->Type != EJson::Number) { Error = FString::Printf(TEXT("Property '%s' expects a number"), *Name); return false; }
        const double Number = Value->AsNumber();
        if (!IsFiniteControlRigProbeFloatRange(Number)) { Error = FString::Printf(TEXT("Property '%s' requires a finite number within float range"), *Name); return false; }
        const bool Floating = NumericProperty->IsFloatingPoint();
        if (!Current()) { Error = Run.Failure; return false; }
        if (!Floating && !NumericProperty->CanHoldValue(Number))
        { Error = FString::Printf(TEXT("Property '%s' cannot represent the requested integral value"), *Name); return false; }
        if (!Current()) { Error = Run.Failure; return false; }
        Memory = Property->ContainerPtrToValuePtr<void>(Run.Instance);
        Run.TransientWork(TEXT("property_set_attempts"));
        if (Floating) { NumericProperty->SetFloatingPointPropertyValue(Memory, Number); }
        else if (CastField<FUInt64Property>(NumericProperty) || CastField<FUInt32Property>(NumericProperty) || CastField<FUInt16Property>(NumericProperty))
        { NumericProperty->SetIntPropertyValue(Memory, static_cast<uint64>(Number)); }
        else { NumericProperty->SetIntPropertyValue(Memory, static_cast<int64>(Number)); }
        Applied = true;
    }
    else if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
    {
        FName Parsed;
        if (Value->Type != EJson::String) { Error = FString::Printf(TEXT("Property '%s' expects a string/name"), *Name); return false; }
        if (!TryParseControlRigProbeName(Value->AsString(), FString::Printf(TEXT("property '%s' name value"), *Name), Parsed, Error)) { return false; }
        Run.TransientWork(TEXT("property_set_attempts")); NameProperty->SetPropertyValue(Memory, Parsed); Applied = true;
    }
    else if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
    {
        if (Value->Type != EJson::String) { Error = FString::Printf(TEXT("Property '%s' expects a string"), *Name); return false; }
        Run.TransientWork(TEXT("property_set_attempts")); StringProperty->SetPropertyValue(Memory, Value->AsString()); Applied = true;
    }
    else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        if (StructProperty->Struct == TBaseStructure<FVector>::Get())
        {
            FVector Parsed;
            if (!JsonValueToVector(Value, Parsed)) { Error = FString::Printf(TEXT("Property '%s' expects a vector array/object"), *Name); return false; }
            Run.TransientWork(TEXT("property_set_attempts")); *static_cast<FVector*>(Memory) = Parsed; Applied = true;
        }
        else if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
        {
            FRotator Parsed;
            if (!JsonValueToRotator(Value, Parsed)) { Error = FString::Printf(TEXT("Property '%s' expects a rotator array/object"), *Name); return false; }
            Run.TransientWork(TEXT("property_set_attempts")); *static_cast<FRotator*>(Memory) = Parsed; Applied = true;
        }
        else if (StructProperty->Struct == TBaseStructure<FTransform>::Get())
        {
            FTransform Parsed;
            if (!JsonValueToTransform(Value, Parsed)) { Error = FString::Printf(TEXT("Property '%s' expects a transform object"), *Name); return false; }
            Run.TransientWork(TEXT("property_set_attempts")); *static_cast<FTransform*>(Memory) = Parsed; Applied = true;
        }
    }
    if (!Applied) { Error = FString::Printf(TEXT("Unsupported property type for '%s': %s"), *Name, *Property->GetClass()->GetName()); return false; }
    if (!Current()) { Error = Run.Failure; return false; }
    // Resolve memory again after virtual numeric access. No pre-callback address
    // or field iterator is reused for readback after an ownership change.
    Echo = EchoProperty(Property, Property->ContainerPtrToValuePtr<void>(Run.Instance));
    if (!Current()) { Error = Run.Failure; return false; }
    Run.Count(TEXT("property_set_successes")); return true;
}

bool ControlValue(FRun& Run, FName Name, const FValue* Input, FValue& Output, FString& Error)
{
    if (!Run.SafeHierarchy()) { Error = Run.Failure; return false; }
    const FRigElementKey Key(Name, ERigElementType::Control);
    FRigControlElement* Element = Run.Tree->Find<FRigControlElement>(Key);
    if (!Element) { Error = FString::Printf(TEXT("Control '%s' was not found in the transient ControlRig hierarchy"), *Name.ToString()); return false; }
    const ERigControlType Type = Element->Settings.ControlType; const ERigControlAxis Axis = Element->Settings.PrimaryAxis;
    const uint32 Version = Run.Tree->GetTopologyVersion();
    auto Current = [&]() -> bool
    {
        if (!Run.Current() || Run.Tree->GetTopologyVersion() != Version || Run.Tree->Find<FRigControlElement>(Key) != Element)
        { return Run.Bad(TEXT("Control element topology/identity changed during value access")); }
        return Element->Settings.ControlType == Type && Element->Settings.PrimaryAxis == Axis
            || Run.Bad(TEXT("Control element type/axis changed during value access"));
    };
    if (Type != ERigControlType::Float && Type != ERigControlType::EulerTransform)
    {
        Error = FString::Printf(TEXT("Control '%s' has unsupported direct-probe type '%s'; only Float and EulerTransform are supported"),
            *Name.ToString(), *StaticEnum<ERigControlType>()->GetNameStringByValue(static_cast<int64>(Type))); return false;
    }
    if (Input)
    {
        FRigControlValue Value;
        if (Type == ERigControlType::Float)
        {
            double Number = 0;
            if (!Input->IsValid() || (*Input)->Type != EJson::Number || !(*Input)->TryGetNumber(Number) || !IsFiniteControlRigProbeFloatRange(Number))
            { Error = FString::Printf(TEXT("Control '%s' expects a finite numeric value within float range"), *Name.ToString()); return false; }
            Value = FRigControlValue::Make<float>(static_cast<float>(Number));
        }
        else
        {
            FTransform Transform;
            if (!JsonValueToTransform(*Input, Transform) || Transform.ContainsNaN())
            { Error = FString::Printf(TEXT("Control '%s' expects a finite transform object within float range"), *Name.ToString()); return false; }
            Value = URigHierarchy::MakeControlValueFromEulerTransform(FEulerTransform(Transform));
        }
        Run.TransientWork(TEXT("control_set_attempts")); Run.Tree->SetControlValue(Key, Value, ERigControlValueType::Current, false, false);
        if (!Current()) { Error = Run.Failure; return false; }
        Run.Count(TEXT("control_set_successes"));
    }
    const FRigControlValue Copy = Run.Tree->GetControlValue(Key, ERigControlValueType::Current);
    if (!Current()) { Error = Run.Failure; return false; }
    if (Type == ERigControlType::Float)
    {
        const float Number = Copy.Get<float>();
        if (!FMath::IsFinite(Number)) { Error = FString::Printf(TEXT("Control '%s' produced a non-finite float readback"), *Name.ToString()); return false; }
        Output = MakeShared<FJsonValueNumber>(Number);
    }
    else
    {
        const FTransform Transform = Copy.GetAsTransform(Type, Axis);
        if (Transform.ContainsNaN()) { Error = FString::Printf(TEXT("Control '%s' produced a non-finite transform readback"), *Name.ToString()); return false; }
        Output = MakeShared<FJsonValueObject>(TransformToJsonObject(Transform));
    }
    return true;
}

bool CurveValue(FRun& Run, FName Name, const double* Input, double& Output, FString& Error)
{
    if (!Run.SafeHierarchy()) { Error = Run.Failure; return false; }
    const FRigElementKey Key(Name, ERigElementType::Curve);
    if (!Run.Tree->Contains(Key)) { Error = FString::Printf(TEXT("Curve '%s' not found in ControlRig hierarchy"), *Name.ToString()); return false; }
    const uint32 Version = Run.Tree->GetTopologyVersion();
    if (Input) { Run.TransientWork(TEXT("curve_set_attempts")); Run.Tree->SetCurveValue(Key, static_cast<float>(*Input), false); }
    if (!Run.Current() || Run.Tree->GetTopologyVersion() != Version || !Run.Tree->Contains(Key))
    { Error = TEXT("Curve topology/membership changed during value access"); return Run.Bad(Error); }
    Output = Run.Tree->GetCurveValue(Key);
    if (!Run.Current() || !FMath::IsFinite(Output)) { Error = TEXT("Curve readback became invalid or non-finite"); return false; }
    if (Input) { Run.Count(TEXT("curve_set_successes")); }
    return true;
}

TSharedPtr<FJsonObject> Sample(FRun& Run, const TArray<FString>& Specs, TMap<FString, FTransform>& Transforms)
{
    auto Pose = MakeShared<FJsonObject>(); Transforms.Reset();
    for (const FString& Spec : Specs)
    {
        if (!Run.SafeHierarchy()) { break; }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("requested"), Spec);
        FRigElementKey Key; FString Error;
        const bool Found = ResolveRigElementKey(Run.Tree, Spec, Key, &Error);
        if (!Run.Current()) { break; }
        if (!Found)
        {
            Row->SetBoolField(TEXT("valid"), false);
            Row->SetStringField(TEXT("error"), Error.IsEmpty() ? FString::Printf(TEXT("Rig element not found: %s"), *Spec) : Error);
            Pose->SetObjectField(Spec, Row); continue;
        }
        Row->SetBoolField(TEXT("valid"), true); Row->SetStringField(TEXT("name"), Key.Name.ToString());
        Row->SetStringField(TEXT("type"), RigElementTypeToString(Key.Type));
        if (Key.Type == ERigElementType::Curve)
        {
            const float Value = Run.Tree->GetCurveValue(Key);
            if (!Run.Current()) { break; }
            if (!FMath::IsFinite(Value)) { Run.Bad(TEXT("Sampled curve is non-finite")); break; }
            Row->SetNumberField(TEXT("curve_value"), Value); Row->SetStringField(TEXT("note"), TEXT("Curve elements do not expose a global transform"));
        }
        else
        {
            const FTransform Transform = Run.Tree->GetGlobalTransform(Key, false);
            if (!Run.Current()) { break; }
            if (Transform.ContainsNaN()) { Run.Bad(TEXT("Sampled global transform is non-finite")); break; }
            Row->SetObjectField(TEXT("global"), TransformToJsonObject(Transform)); Transforms.Add(Key.Name.ToString(), Transform);
        }
        Pose->SetObjectField(Key.Name.ToString(), Row); Run.Count(TEXT("sampled_element_count"));
    }
    return Pose;
}


struct FRequest
{
    FString Path, ClassPath, InputDefaults, Curves, Cases, Samples, Controls, Events, TraceProperty, LocationProperty;
    bool PrePost = false;
};
bool ProbeActive = false;

FResult Perform(const FRequest& Request)
{
    FRun Run; FResult& Result = Run.Result;
    const FString Method = Request.PrePost ? TEXT("sample_controlrig_pre_post_runtime_pose") : TEXT("controlrig_direct_gate_probe");
    Result.Stage = TEXT("preflight_transient_probe");
    auto Params = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Cases, Errors, Warnings;
    Result.Data->SetBoolField(TEXT("success"), false);
    Result.Data->SetBoolField(TEXT("partial"), false);
    Result.Data->SetArrayField(TEXT("cases"), Cases); Result.Data->SetArrayField(TEXT("errors"), Errors); Result.Data->SetArrayField(TEXT("warnings"), Warnings);
    Result.Data->SetStringField(TEXT("operation"), Method);
    Result.Data->SetBoolField(TEXT("legacy_read_only_is_total_SDK_purity_proof"), false);
    Result.Data->SetBoolField(TEXT("legacy_asset_modified_false_is_unchanged_asset_proof"), false);
    Result.Data->SetBoolField(TEXT("transient_work_attempted"), false); Result.Data->SetBoolField(TEXT("observed_explicit_transient_change"), false);
    Result.Data->SetBoolField(TEXT("asset_save_requested"), false);
    Result.Data->SetBoolField(TEXT("atomic_rollback"), false); Result.Data->SetBoolField(TEXT("automatic_retry"), false);
    Result.Data->SetBoolField(TEXT("legacy_create_route_adapted_transient_outer"), true);
    Result.Data->SetStringField(TEXT("compile_observation_scope"), TEXT("public RecompileVMIfRequired check requested/dispatched for legacy Blueprint creation; actual compiler execution/completion is unobserved"));
    Result.Data->SetField(TEXT("actual_compiler_invocation_count"), MakeShared<FJsonValueNull>());
    Result.Data->SetField(TEXT("actual_compiler_completion"), MakeShared<FJsonValueNull>());
    for (const TCHAR* Key : {TEXT("asset_load_calls"), TEXT("class_load_calls"), TEXT("class_find_calls"), TEXT("asset_reference_calls"),
        TEXT("legacy_blueprint_CreateControlRig_direct_calls"), TEXT("legacy_create_route_adapted_calls"), TEXT("vm_recompile_check_calls"),
        TEXT("transient_create_attempts"), TEXT("transient_create_returned_count"), TEXT("compatibility_prepare_initialize_calls"),
        TEXT("handler_initialize_calls"), TEXT("property_set_attempts"), TEXT("property_set_successes"), TEXT("control_set_attempts"),
        TEXT("control_set_successes"), TEXT("curve_set_attempts"), TEXT("curve_set_successes"), TEXT("execute_calls"),
        TEXT("execute_true_returns"), TEXT("execute_false_returns"), TEXT("sampled_element_count"), TEXT("explicit_save_calls")})
    { Result.Data->SetNumberField(Key, 0); }
    auto Fail = [&](const FString& Code, const FString& Message) -> FResult
    {
        Result.bSuccess = false; Result.bPostconditionPassed = false; Result.ErrorCode = Code; Result.Error = Message;
        Result.Data->SetBoolField(TEXT("success"), false);
        if (Run.Asset.IsValid() && Live(Run.Asset->Package.Get()))
        {
            Result.Data->SetBoolField(TEXT("captured_source_dirty_after"), Cast<UPackage>(Run.Asset->Package.Get())->IsDirty());
            Result.Data->SetBoolField(TEXT("captured_source_identity_current"), Run.Asset->Current());
            Result.Data->SetStringField(TEXT("dirty_observation_scope"), TEXT("captured source package after load, including failure; not proof about all SDK side effects or a replaced current package"));
        }
        Result.Data->SetBoolField(TEXT("partial"), Result.bMutationAttempted || !Cases.IsEmpty());
        Result.Data->SetBoolField(TEXT("change_state_known"), Result.bChangeStateKnown);
        Result.Data->SetBoolField(TEXT("observed_explicit_transient_change"), Result.bChanged);
        return Result;
    };
    if (!IsInGameThread() || !GEditor || IsEngineExitRequested() || ProbeActive)
    { return Fail(TEXT("controlrig_transient_probe_unavailable"), TEXT("Live non-reentrant editor game-thread dispatch is required")); }
    TGuardValue<bool> ProbeScope(ProbeActive, true);
    if (!TextInput(Request.Path) || !TextInput(Request.ClassPath) || !TextInput(Request.TraceProperty) || !TextInput(Request.LocationProperty)
        || NormalizeLoadPath(Request.Path).Len() >= NAME_SIZE || NormalizeLoadPath(Request.ClassPath).Len() >= NAME_SIZE
        || Request.ClassPath.Len() + 15 >= NAME_SIZE)
    { return Fail(TEXT("controlrig_transient_probe_input_invalid"), TEXT("Ordinary text is invalid or exceeds the bounded path/name-loading boundary")); }
    Params->SetStringField(TEXT("should_trace_property"), Request.TraceProperty);
    Params->SetStringField(TEXT("interaction_location_property"), Request.LocationProperty);
    struct FCarrier { const TCHAR* Key; const FString* Text; EJson Kind; };
    TArray<FCarrier> Carriers {{TEXT("cases"), &Request.Cases, EJson::Array}, {TEXT("sample_elements"), &Request.Samples, EJson::Array},
        {TEXT("control_values"), &Request.Controls, EJson::Object}, {TEXT("execute_events"), &Request.Events, EJson::Array}};
    if (Request.PrePost) { Carriers.Add({TEXT("input_defaults"), &Request.InputDefaults, EJson::Object}); Carriers.Add({TEXT("curve_values"), &Request.Curves, EJson::Object}); }
    for (const FCarrier& Carrier : Carriers)
    {
        FValue Value; FString Error;
        if (!Decode(*Carrier.Text, Value, Error)) { return Fail(TEXT("controlrig_transient_probe_input_invalid"), FString(Carrier.Key) + TEXT(": ") + Error); }
        if (Value->Type != EJson::Null && Value->Type != Carrier.Kind)
        { return Fail(TEXT("controlrig_transient_probe_input_invalid"), FString(Carrier.Key) + TEXT(" requires its original nullable array/object shape")); }
        if (Value->Type != EJson::Null) { Params->SetField(Carrier.Key, Value); }
        else if (FString(Carrier.Key) == TEXT("input_defaults"))
        {
            auto Defaults = MakeShared<FJsonObject>(); Defaults->SetBoolField(TEXT("ShouldDoIKTrace"), true);
            Defaults->SetField(TEXT("InteractionWorldLocation"), VectorToJsonValue(FVector(80, -40, 80)));
            Params->SetObjectField(Carrier.Key, Defaults);
        }
        else if (FString(Carrier.Key) == TEXT("curve_values"))
        {
            auto Defaults = MakeShared<FJsonObject>(); Defaults->SetNumberField(TEXT("IK_blend_interact"), 1.0); Defaults->SetNumberField(TEXT("IKBlend_l"), 1.0);
            Params->SetObjectField(Carrier.Key, Defaults);
        }
        // All other nulls are omitted, matching the original public wrapper.
    }
    const TArray<FValue>* RawCases = nullptr; const TArray<FValue>* RawSamples = nullptr; const TArray<FValue>* RawEvents = nullptr;
    Params->TryGetArrayField(TEXT("cases"), RawCases); Params->TryGetArrayField(TEXT("sample_elements"), RawSamples); Params->TryGetArrayField(TEXT("execute_events"), RawEvents);
    const TSharedPtr<FJsonObject>* RawControls = nullptr; Params->TryGetObjectField(TEXT("control_values"), RawControls);
    const int64 CaseRows = RawCases && !RawCases->IsEmpty() ? RawCases->Num() : Request.PrePost ? 1 : 6;
    if ((RawCases && RawCases->Num() > MaxCases) || (RawSamples && RawSamples->Num() > MaxSamples)
        || (RawEvents && RawEvents->Num() > MaxEvents)
        || (RawControls && RawControls->IsValid() && int64((*RawControls)->Values.Num()) * FMath::Max<int64>(CaseRows, 6) > MaxSourceRows))
    { return Fail(TEXT("controlrig_transient_probe_budget_exceeded"), TEXT("Raw case/element/event/shared-control projection exceeds the finite boundary before parser expansion")); }
    const auto ProbeCases = Request.PrePost ? ParseControlRigPrePostCases(Params, Request.TraceProperty, Request.LocationProperty)
        : ParseControlRigProbeCases(Params, Request.TraceProperty, Request.LocationProperty);
    const TArray<FString> DefaultSamples {TEXT("pelvis"), TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("IK_foot_L"), TEXT("IK_foot_R"), TEXT("head_ctrl")};
    const TArray<FString> DefaultEvents = Request.PrePost ? TArray<FString>{TEXT("Forwards Solve")}
        : TArray<FString>{TEXT("Construction"), TEXT("Forwards Solve"), TEXT("Post Forwards Solve")};
    const TArray<FString> Samples = GetStringArrayParam(Params, TEXT("sample_elements"), DefaultSamples);
    const TArray<FString> Events = GetStringArrayParam(Params, TEXT("execute_events"), DefaultEvents);
    TArray<FName> EventNames; FString EventError;
    if (!ParseControlRigProbeEventNames(Events, EventNames, EventError)) { return Fail(TEXT("controlrig_transient_probe_events_invalid"), EventError); }
    if (ProbeCases.Num() > MaxCases || Samples.Num() > MaxSamples || Events.Num() > MaxEvents
        || int64(ProbeCases.Num()) * (Samples.Num() + Events.Num() + 1) > MaxSourceRows)
    { return Fail(TEXT("controlrig_transient_probe_budget_exceeded"), TEXT("Case/sample/event work exceeds the finite probe boundary; no SDK work dispatched")); }
    const int64 CarrierUnits = int64(Request.Cases.Len()) + Request.Samples.Len() + Request.Controls.Len() + Request.Events.Len()
        + Request.Path.Len() + Request.ClassPath.Len() + Request.TraceProperty.Len() + Request.LocationProperty.Len()
        + (Request.PrePost ? int64(Request.InputDefaults.Len()) + Request.Curves.Len() : 0);
    const int64 ReportProjection = int64(ProbeCases.Num()) * (CarrierUnits * 8 + int64(Samples.Num()) * 4096 + int64(Events.Num()) * 4096 + 65536);
    if (ReportProjection > (int64(64) << 20))
    { return Fail(TEXT("controlrig_transient_probe_budget_exceeded"), TEXT("Conservative repeated-case report projection exceeds 64Mi UTF16 units before SDK work")); }
    Result.Data->SetNumberField(TEXT("report_projection_units"), static_cast<double>(ReportProjection));
    TArray<FValue> SampleRows, EventRows;
    for (const FString& Name : Samples) { SampleRows.Add(MakeShared<FJsonValueString>(Name)); }
    for (const FString& Name : Events) { EventRows.Add(MakeShared<FJsonValueString>(Name)); }
    Result.Data->SetArrayField(TEXT("sample_elements"), SampleRows); Result.Data->SetArrayField(TEXT("execute_events"), EventRows);
    Result.Data->SetStringField(TEXT("should_trace_property"), Request.TraceProperty);
    Result.Data->SetStringField(TEXT("interaction_location_property"), Request.LocationProperty);
    Result.Data->SetBoolField(TEXT("read_only"), true); Result.Data->SetBoolField(TEXT("asset_modified"), false); Result.Data->SetBoolField(TEXT("saves_assets"), false);
    if (Request.PrePost)
    {
        Result.Data->SetBoolField(TEXT("runtime_graph_prepost"), false); Result.Data->SetBoolField(TEXT("same_instance_prepost"), true);
        Result.Data->SetStringField(TEXT("runtime_source"), TEXT("direct_transient_controlrig"));
        Result.Data->SetStringField(TEXT("runtime_note"), TEXT("Samples a transient ControlRig hierarchy before and after execute events. It does not instrument the compiled AnimGraph node stack."));
    }
    FString ResolvedAsset, ResolvedClass, BaselineName;
    TMap<FString, FTransform> Baseline;
    for (const FControlRigDirectProbeCase& ProbeCase : ProbeCases)
    {
        Result.Stage = TEXT("create_transient_controlrig");
        FString CaseAsset, CaseClass;
        if (!ResolveAndCreate(Run, Request.Path, Request.ClassPath, CaseAsset, CaseClass)) { break; }
        if (ResolvedAsset.IsEmpty()) { ResolvedAsset = CaseAsset; }
        if (ResolvedClass.IsEmpty()) { ResolvedClass = CaseClass; }
        auto Case = MakeShared<FJsonObject>(); Case->SetStringField(TEXT("name"), ProbeCase.Name); Case->SetStringField(TEXT("control_rig_class"), CaseClass);
        TArray<FValue> CaseErrors, CaseWarnings, Executions;
        auto Properties = MakeShared<FJsonObject>(), PropertyEcho = MakeShared<FJsonObject>();
        auto Controls = MakeShared<FJsonObject>(), ControlAfterSet = MakeShared<FJsonObject>(), ControlAfterExecute = MakeShared<FJsonObject>();
        auto Curves = MakeShared<FJsonObject>(), CurveBefore = MakeShared<FJsonObject>(), CurveAfter = MakeShared<FJsonObject>();
        Case->SetObjectField(TEXT("property_inputs"), Properties); Case->SetObjectField(TEXT("property_echo"), PropertyEcho);
        Case->SetObjectField(TEXT("control_inputs"), Controls); Case->SetObjectField(TEXT("control_echo_after_set"), ControlAfterSet); Case->SetObjectField(TEXT("control_echo_after_execute"), ControlAfterExecute);
        Case->SetObjectField(TEXT("curve_inputs"), Curves); Case->SetObjectField(Request.PrePost ? TEXT("curve_echo_before_execute") : TEXT("curve_echo_after_set"), CurveBefore);
        Case->SetObjectField(TEXT("curve_echo_after_execute"), CurveAfter);
        Cases.Add(MakeShared<FJsonValueObject>(Case)); Result.Data->SetArrayField(TEXT("cases"), Cases);
        auto Error = [&](const FString& Text)
        { CaseErrors.Add(MakeShared<FJsonValueString>(Text)); Errors.Add(MakeShared<FJsonValueString>(ProbeCase.Name + TEXT(": ") + Text)); Case->SetArrayField(TEXT("errors"), CaseErrors); Result.Data->SetArrayField(TEXT("errors"), Errors); };
        auto Warning = [&](const FString& Text)
        { CaseWarnings.Add(MakeShared<FJsonValueString>(Text)); Warnings.Add(MakeShared<FJsonValueString>(ProbeCase.Name + TEXT(": ") + Text)); Case->SetArrayField(TEXT("warnings"), CaseWarnings); Result.Data->SetArrayField(TEXT("warnings"), Warnings); };
        for (const FString& InputError : ProbeCase.InputErrors) { Error(InputError); }
        Result.Stage = TEXT("initialize_transient_controlrig");
        Run.TransientWork(TEXT("handler_initialize_calls")); Run.Instance->Initialize(true);
        if (!Run.Current()) { Error(Run.Failure); break; }
        Run.Tree = Run.Instance->GetHierarchy();
        if (!Live(Run.Tree)) { Error(TEXT("ControlRig hierarchy is null after Initialize")); continue; }
        Run.Hierarchy = MakeShared<FIdentity>(Run.Tree);
        if (!Run.SafeHierarchy()) { Error(Run.Failure); break; }
        Result.Stage = TEXT("apply_transient_probe_inputs");
        for (const auto& Pair : ProbeCase.PropertyValues)
        {
            Properties->SetField(Pair.Key, Pair.Value); FString Text; FValue Echo;
            if (ApplyProperty(Run, Pair.Key, Pair.Value, Echo, Text)) { PropertyEcho->SetField(Pair.Key, Echo); }
            else { Error(Text); }
            if (Run.Failed) { break; }
        }
        TSet<FName> AppliedControls, AppliedCurves;
        for (const auto& Pair : ProbeCase.ControlValues)
        {
            if (Run.Failed) { break; } Controls->SetField(Pair.Key.ToString(), Pair.Value); FString Text; FValue Echo;
            if (ControlValue(Run, Pair.Key, &Pair.Value, Echo, Text)) { AppliedControls.Add(Pair.Key); ControlAfterSet->SetField(Pair.Key.ToString(), Echo); }
            else { Error(Text); }
        }
        for (const auto& Pair : ProbeCase.CurveValues)
        {
            if (Run.Failed) { break; } Curves->SetNumberField(Pair.Key.ToString(), Pair.Value); FString Text; double Echo = 0;
            if (CurveValue(Run, Pair.Key, &Pair.Value, Echo, Text)) { AppliedCurves.Add(Pair.Key); CurveBefore->SetNumberField(Pair.Key.ToString(), Echo); }
            else { Error(Text); }
        }
        TMap<FString, FTransform> Before, After;
        if (Request.PrePost && !Run.Failed) { Case->SetObjectField(TEXT("pre_pose"), Sample(Run, Samples, Before)); }
        bool CanExecute = !Run.Failed;
        Result.Stage = TEXT("execute_transient_controlrig_events");
        for (int32 Index = 0; Index < Events.Num(); ++Index)
        {
            if (Run.Failed) { break; }
            auto Event = MakeShared<FJsonObject>(); Event->SetStringField(TEXT("event"), Events[Index]);
            Executions.Add(MakeShared<FJsonValueObject>(Event)); Case->SetArrayField(TEXT("execute_results"), Executions);
            if (!CanExecute)
            { Event->SetBoolField(TEXT("success"), false); Event->SetBoolField(TEXT("skipped"), true); Event->SetStringField(TEXT("skip_reason"), TEXT("A validated control or curve input could not be reapplied after Construction")); continue; }
            if (!Run.SafeHierarchy()) { Error(Run.Failure); break; }
            Run.TransientWork(TEXT("execute_calls"));
            const bool Executed = Run.Instance->Execute(EventNames[Index]);
            Event->SetBoolField(TEXT("success"), Executed); Run.Count(Executed ? TEXT("execute_true_returns") : TEXT("execute_false_returns"));
            if (!Run.Current()) { Error(Run.Failure); break; }
            if (!Executed)
            { const FString Text = FString::Printf(TEXT("ControlRig Execute returned false for event '%s'"), *Events[Index]); Event->SetStringField(TEXT("warning"), Text); Warning(Text); }
            if (EventNames[Index] == FRigUnit_PrepareForExecution::EventName)
            {
                bool Reapplied = true;
                for (const auto& Pair : ProbeCase.CurveValues)
                {
                    if (Run.Failed) { break; } if (!AppliedCurves.Contains(Pair.Key)) { continue; }
                    FString Text; double Echo = 0;
                    if (CurveValue(Run, Pair.Key, &Pair.Value, Echo, Text)) { CurveBefore->SetNumberField(Pair.Key.ToString(), Echo); }
                    else
                    { AppliedCurves.Remove(Pair.Key); CurveBefore->RemoveField(Pair.Key.ToString()); Reapplied = false; Error(FString::Printf(TEXT("Curve '%s' disappeared after the Construction event"), *Pair.Key.ToString())); }
                }
                for (const auto& Pair : ProbeCase.ControlValues)
                {
                    if (Run.Failed) { break; } if (!AppliedControls.Contains(Pair.Key)) { continue; }
                    FString Text; FValue Echo;
                    if (ControlValue(Run, Pair.Key, &Pair.Value, Echo, Text)) { ControlAfterSet->SetField(Pair.Key.ToString(), Echo); }
                    else
                    { AppliedControls.Remove(Pair.Key); ControlAfterSet->RemoveField(Pair.Key.ToString()); Reapplied = false; Error(TEXT("Failed to reapply a ControlRig input after Construction: ") + Text); }
                }
                Event->SetBoolField(TEXT("input_reapply_success"), Reapplied); CanExecute = Reapplied && !Run.Failed;
            }
        }
        Result.Stage = TEXT("sample_transient_controlrig_results");
        for (FName Name : AppliedControls)
        {
            if (Run.Failed) { break; } FString Text; FValue Echo;
            if (ControlValue(Run, Name, nullptr, Echo, Text)) { ControlAfterExecute->SetField(Name.ToString(), Echo); } else { Error(Text); }
        }
        for (const auto& Pair : ProbeCase.CurveValues)
        {
            if (Run.Failed) { break; }
            if (!Run.SafeHierarchy()) { break; }
            if (!Run.Tree->Contains(FRigElementKey(Pair.Key, ERigElementType::Curve))) { continue; }
            FString Text; double Echo = 0;
            if (CurveValue(Run, Pair.Key, nullptr, Echo, Text)) { CurveAfter->SetNumberField(Pair.Key.ToString(), Echo); } else { Error(Text); }
        }
        if (!Run.Failed)
        {
            const auto Pose = Sample(Run, Samples, After);
            if (Request.PrePost) { Case->SetObjectField(TEXT("post_pose"), Pose); Case->SetObjectField(TEXT("deltas"), BuildRigPoseDeltaObject(Before, After)); }
            else
            {
                Case->SetObjectField(TEXT("transforms"), Pose);
                if (Baseline.IsEmpty() && !After.IsEmpty()) { Baseline = After; BaselineName = ProbeCase.Name; }
                auto Delta = MakeShared<FJsonObject>();
                for (const auto& Pair : After)
                {
                    if (const FTransform* Original = Baseline.Find(Pair.Key))
                    {
                        const FVector Difference = Pair.Value.GetLocation() - Original->GetLocation(); auto Row = MakeShared<FJsonObject>();
                        Row->SetArrayField(TEXT("delta"), VectorToJsonArray(Difference)); Row->SetNumberField(TEXT("distance"), Difference.Size()); Delta->SetObjectField(Pair.Key, Row);
                    }
                }
                Case->SetObjectField(TEXT("delta_from_baseline"), Delta);
            }
        }
        if (Run.Failed && !Run.Failure.IsEmpty()) { Error(Run.Failure); }
        Case->SetArrayField(TEXT("errors"), CaseErrors); Case->SetArrayField(TEXT("warnings"), CaseWarnings); Case->SetArrayField(TEXT("execute_results"), Executions);
        Case->SetBoolField(TEXT("success"), CaseErrors.IsEmpty() && !Run.Failed);
        if (Run.Asset.IsValid() && Live(Run.Asset->Package.Get()))
        {
            Result.Data->SetBoolField(TEXT("captured_source_dirty_after"), Cast<UPackage>(Run.Asset->Package.Get())->IsDirty());
            Result.Data->SetStringField(TEXT("dirty_observation_scope"), TEXT("captured source package after load; not proof about all SDK side effects or a changed current asset package"));
        }
        if (Run.Failed) { break; }
    }
    Result.Data->SetStringField(TEXT("control_rig_asset"), ResolvedAsset); Result.Data->SetStringField(TEXT("control_rig_class"), ResolvedClass);
    if (!Request.PrePost) { Result.Data->SetStringField(TEXT("baseline_case"), BaselineName); }
    Result.Data->SetNumberField(TEXT("case_count"), Cases.Num());
    Result.Data->SetNumberField(TEXT("requested_case_count"), ProbeCases.Num()); Result.Data->SetBoolField(TEXT("cases_complete"), Cases.Num() == ProbeCases.Num() && !Run.Failed);
    Result.Data->SetBoolField(TEXT("total_SDK_effects_observed"), false);
    Result.Data->SetBoolField(TEXT("change_state_known"), Result.bChangeStateKnown);
    if (Run.Failed)
    { return Fail(TEXT("controlrig_transient_probe_state_or_execution_failed"), Run.Failure); }
    if (!Errors.IsEmpty()) { return Fail(TEXT("controlrig_transient_probe_case_errors"), TEXT("Transient cases reported errors; completed outputs and all observed work remain in the prefix")); }
    Result.bSuccess = true; Result.bPostconditionPassed = true; Result.Stage = TEXT("complete_transient_controlrig_probe");
    Result.Data->SetBoolField(TEXT("success"), true);
    return Result;
}

} // anonymous namespace

Variable::FResult Direct(const FString& ControlRigPath, const FString& ControlRigClass,
    const FString& CasesJson, const FString& SampleElementsJson, const FString& ControlValuesJson,
    const FString& ExecuteEventsJson, const FString& ShouldTraceProperty, const FString& InteractionLocationProperty)
{
    FRequest Request;
    Request.Path = ControlRigPath; Request.ClassPath = ControlRigClass;
    Request.Cases = CasesJson; Request.Samples = SampleElementsJson; Request.Controls = ControlValuesJson;
    Request.Events = ExecuteEventsJson; Request.TraceProperty = ShouldTraceProperty; Request.LocationProperty = InteractionLocationProperty;
    return Perform(Request);
}

Variable::FResult PrePost(const FString& ControlRigPath, const FString& ControlRigClass,
    const FString& InputDefaultsJson, const FString& CurveValuesJson, const FString& CasesJson,
    const FString& SampleElementsJson, const FString& ControlValuesJson, const FString& ExecuteEventsJson,
    const FString& ShouldTraceProperty, const FString& InteractionLocationProperty)
{
    FRequest Request; Request.PrePost = true;
    Request.Path = ControlRigPath; Request.ClassPath = ControlRigClass;
    Request.InputDefaults = InputDefaultsJson; Request.Curves = CurveValuesJson;
    Request.Cases = CasesJson; Request.Samples = SampleElementsJson; Request.Controls = ControlValuesJson;
    Request.Events = ExecuteEventsJson; Request.TraceProperty = ShouldTraceProperty; Request.LocationProperty = InteractionLocationProperty;
    return Perform(Request);
}
} // namespace Cubeless::UE58::ControlRigTransientProbe
