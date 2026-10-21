#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectGlobals.h"
#include "EdGraphSchema_K2.h"
#include "InputCoreTypes.h"
#include "Misc/DefaultValueHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::Function::Pin
{
namespace
{
bool Fail(FResult& R, const TCHAR* Code, const TCHAR* Message) { R.ErrorCode = Code; R.Error = Message; return false; }
bool SafeString(const FString& S)
{
    if (S.Len() > 65536) { return false; }
    for (int32 I = 0; I < S.Len(); ++I)
    {
        const uint32 C = S[I]; if (!C) { return false; }
        if (C >= 0xD800 && C <= 0xDBFF) { if (++I >= S.Len() || S[I] < 0xDC00 || S[I] > 0xDFFF) { return false; } }
        else if (C >= 0xDC00 && C <= 0xDFFF) { return false; }
    }
    return true;
}
bool Bounded(const TSharedPtr<FJsonValue>& V, int32 Depth, int32& Count,bool bAllowFiniteDouble)
{
    if (!V || Depth > 8 || ++Count > 1024) { return false; }
    if (V->Type == EJson::Number) { return FMath::IsFinite(V->AsNumber()) && (bAllowFiniteDouble || FMath::Abs(V->AsNumber()) <= 9007199254740991.0); }
    if (V->Type == EJson::String) { return SafeString(V->AsString()); }
    if (V->Type == EJson::Array) { for (const auto& X : V->AsArray()) { if (!Bounded(X, Depth + 1, Count,bAllowFiniteDouble)) { return false; } } }
    if (V->Type == EJson::Object) { for (const auto& Pair : V->AsObject()->Values) { if (!SafeName(FString(Pair.Key.ToView())) || !Bounded(Pair.Value, Depth + 1, Count,bAllowFiniteDouble)) { return false; } } }
    return true;
}
}
bool SafeName(const FString& S, bool bEmpty)
{ if ((!bEmpty && S.IsEmpty()) || S.Len() >= NAME_SIZE || !SafeString(S)) { return false; } for (TCHAR C : S) { if (C < 32 || C == 127) { return false; } } return true; }
bool Identifier(const FString& S)
{
    if (S.IsEmpty() || S.Len() > 100 || S.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { return false; }
    for (int32 I = 0; I < S.Len(); ++I) { const TCHAR C = S[I]; if (!(C == '_' || (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (I > 0 && C >= '0' && C <= '9'))) { return false; } } return true;
}
bool ParseJson(const FString& Text, TSharedPtr<FJsonValue>& Value, FResult& R,bool bAllowFiniteDouble)
{
    if (Text.IsEmpty() || Text.Len() > 65536) { return Fail(R, TEXT("function_json_invalid"), TEXT("JSON text must contain 1..65536 UTF-16 characters.")); }
    const FString Wrapped = TEXT("[") + Text + TEXT("]"); auto Reader = TJsonReaderFactory<>::Create(Wrapped);
    TArray<bool> Objects; TArray<TSet<FString>> Keys; EJsonNotation Token;
    while (Reader->ReadNext(Token))
    {
        if (Token == EJsonNotation::Error) { return Fail(R, TEXT("function_json_invalid"), TEXT("Malformed JSON.")); }
        if (Token != EJsonNotation::ArrayEnd && Token != EJsonNotation::ObjectEnd && !Objects.IsEmpty() && Objects.Last())
        { const FString& Key = Reader->GetIdentifier(); if (Keys.Last().Contains(Key)) { return Fail(R, TEXT("function_json_invalid"), TEXT("Duplicate JSON key.")); } Keys.Last().Add(Key); }
        if (Token == EJsonNotation::ArrayStart || Token == EJsonNotation::ObjectStart)
        { Objects.Add(Token == EJsonNotation::ObjectStart); Keys.AddDefaulted(); if (Objects.Num() > 10) { return Fail(R, TEXT("function_json_invalid"), TEXT("JSON nesting limit exceeded.")); } }
        else if (Token == EJsonNotation::ArrayEnd || Token == EJsonNotation::ObjectEnd) { Objects.Pop(); Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values; int32 Count = 0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Wrapped), Values) || Values.Num() != 1 || !Bounded(Values[0], 0, Count,bAllowFiniteDouble))
    { return Fail(R, TEXT("function_json_invalid"), bAllowFiniteDouble?TEXT("Provide one bounded JSON value; depth<=8, values<=1024 and finite binary64 numeric values; exact large integers require strings."):TEXT("Provide one bounded JSON value; depth<=8, values<=1024, finite numeric values within JSON exact integer range.")); }
    Value = Values[0]; return true;
}
bool IsPrimitive(const FEdGraphPinType& T)
{
    if (T.IsContainer() || T.PinSubCategoryObject.IsValid() || T.bIsReference || T.bIsWeakPointer) { return false; }
    const FName C = T.PinCategory;
    return C == UEdGraphSchema_K2::PC_Boolean || C == UEdGraphSchema_K2::PC_Int || C == UEdGraphSchema_K2::PC_Int64
        || C == UEdGraphSchema_K2::PC_String || C == UEdGraphSchema_K2::PC_Name || C == UEdGraphSchema_K2::PC_Text
        || (C == UEdGraphSchema_K2::PC_Real && (T.PinSubCategory == UEdGraphSchema_K2::PC_Float || T.PinSubCategory == UEdGraphSchema_K2::PC_Double));
}
bool PrimitiveType(const FString& Name, FEdGraphPinType& T, FResult& R)
{
    const FString N = Name.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
    if (N == TEXT("bool") || N == TEXT("boolean")) { T.PinCategory = UEdGraphSchema_K2::PC_Boolean; }
    else if (N == TEXT("int") || N == TEXT("integer")) { T.PinCategory = UEdGraphSchema_K2::PC_Int; }
    else if (N == TEXT("int64") || N == TEXT("integer64")) { T.PinCategory = UEdGraphSchema_K2::PC_Int64; }
    else if (N == TEXT("float") || N == TEXT("single") || N == TEXT("double") || N == TEXT("real"))
    { T.PinCategory = UEdGraphSchema_K2::PC_Real; T.PinSubCategory = N == TEXT("float") || N == TEXT("single") ? UEdGraphSchema_K2::PC_Float : UEdGraphSchema_K2::PC_Double; }
    else if (N == TEXT("name")) { T.PinCategory = UEdGraphSchema_K2::PC_Name; }
    else if (N == TEXT("string")) { T.PinCategory = UEdGraphSchema_K2::PC_String; }
    else if (N == TEXT("text")) { T.PinCategory = UEdGraphSchema_K2::PC_Text; }
    else { return Fail(R, TEXT("function_parameter_type_pending"), TEXT("Supports scalar bool/int/int64/float/double/name/string/text or the separate exact Vector/Rotator/Transform parameter path; other composite/reference types remain pending.")); }
    return true;
}
TSharedRef<FJsonObject> ScalarValueStructSource(const FEdGraphPinType& T)
{
    auto J = MakeShared<FJsonObject>();
    const UScriptStruct* S = Cast<UScriptStruct>(T.PinSubCategoryObject.Get());
    const UPackage* P = IsValid(S) ? S->GetOutermost() : nullptr;
    int32 ExpectedSize = 0; FString ExpectedPath;
    if (IsValid(S) && S == TBaseStructure<FVector>::Get()) { ExpectedSize = sizeof(FVector); ExpectedPath = TEXT("/Script/CoreUObject.Vector"); }
    else if (IsValid(S) && S == TBaseStructure<FRotator>::Get()) { ExpectedSize = sizeof(FRotator); ExpectedPath = TEXT("/Script/CoreUObject.Rotator"); }
    else if (IsValid(S) && S == TBaseStructure<FTransform>::Get()) { ExpectedSize = sizeof(FTransform); ExpectedPath = TEXT("/Script/CoreUObject.Transform"); }
    const bool Complete = IsValid(S) && P;
    const bool Verified = Complete && ExpectedSize && S->GetPathName() == ExpectedPath
        && FindObject<UScriptStruct>(nullptr, *ExpectedPath) == S && S->GetStructureSize() == ExpectedSize
        && !S->IsEditorOnly() && !S->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && P->GetName() == TEXT("/Script/CoreUObject") && !P->IsDirty()
        && !P->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
    J->SetBoolField(TEXT("observations_complete"), Complete); J->SetBoolField(TEXT("native_identity_verified"), Verified);
    J->SetStringField(TEXT("struct_path"), Complete ? S->GetPathName() : FString());
    J->SetNumberField(TEXT("struct_size"), Complete ? S->GetStructureSize() : 0);
    J->SetStringField(TEXT("package_path"), Complete ? P->GetName() : FString());
    J->SetBoolField(TEXT("package_dirty"), Complete && P->IsDirty());
    J->SetStringField(TEXT("package_flags_string"), Complete ? LexToString(static_cast<uint32>(P->GetPackageFlags())) : FString());
    return J;
}
bool IsScalarValueStruct(const FEdGraphPinType& T)
{
    return T.PinCategory == UEdGraphSchema_K2::PC_Struct && T.PinSubCategory.IsNone() && !T.IsContainer()
        && !T.bIsReference && !T.bIsConst && !T.bIsWeakPointer && !T.bIsUObjectWrapper
        && ScalarValueStructSource(T)->GetBoolField(TEXT("native_identity_verified"));
}

namespace
{
bool NativeConstraintLoaded(const UObject* O)
{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool NativeConstraintClass(const UClass* C,bool bReferencedArray=false)
{
    if(!NativeConstraintLoaded(C)||C->GetClass()!=UClass::StaticClass()||C->ClassGeneratedBy||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||C->IsEditorOnly())return false;
    const bool Object=C==UObject::StaticClass(),Actor=C==AActor::StaticClass();
    const bool Component=bReferencedArray&&C==UActorComponent::StaticClass(),Child=bReferencedArray&&C==UChildActorComponent::StaticClass(),Skeletal=bReferencedArray&&C==USkeletalMeshComponent::StaticClass();
    if(!Object&&!Actor&&!Component&&!Child&&!Skeletal)return false;
    auto* P=C->GetPackage();const TCHAR* Path=Object?TEXT("/Script/CoreUObject.Object"):Actor?TEXT("/Script/Engine.Actor"):Component?TEXT("/Script/Engine.ActorComponent"):Child?TEXT("/Script/Engine.ChildActorComponent"):TEXT("/Script/Engine.SkeletalMeshComponent");
    return NativeConstraintLoaded(P)&&C->GetOuter()==P&&P->HasAnyPackageFlags(PKG_CompiledIn)&&!P->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)&&!P->IsDirty()
        &&C->GetPathName().Equals(Path,ESearchCase::CaseSensitive)&&FindObject<UClass>(nullptr,Path)==C
        &&C->GetSuperClass()==(Actor||Component?UObject::StaticClass():Child?USceneComponent::StaticClass():Skeletal?USkinnedMeshComponent::StaticClass():nullptr);
}
bool NativeConstraintPin(const FEdGraphPinType& T)
{
    if(T.PinCategory!=UEdGraphSchema_K2::PC_Object&&T.PinCategory!=UEdGraphSchema_K2::PC_Class)return false;
    auto* C=Cast<UClass>(T.PinSubCategoryObject.Get());FEdGraphPinType Expected;Expected.PinCategory=T.PinCategory;Expected.PinSubCategoryObject=C;
    return NativeConstraintClass(C)&&T==Expected&&!T.bIsUObjectWrapper&&!T.bSerializeAsSinglePrecisionFloat;
}
}
bool NativeClassParameterType(const FString& Name,const FString& TypeObject,FEdGraphPinType& Type,FResult& R,bool bReferencedArray)
{
    const FString N=Name.TrimStartAndEnd().ToLower();
    if(N!=TEXT("object")&&N!=TEXT("class"))return Fail(R,TEXT("function_class_descriptor_pending"),TEXT("Only object/class native constraints use this finite descriptor."));
    if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())return Fail(R,TEXT("function_class_editor_required"),TEXT("Idle safe Editor game thread required for native class metadata."));
    FString P=FPackageName::ExportTextPathToObjectPath(TypeObject).TrimStartAndEnd();P.TrimQuotesInline();
    if(P.IsEmpty()||P==TEXT("Object"))P=TEXT("/Script/CoreUObject.Object");else if(P==TEXT("Actor"))P=TEXT("/Script/Engine.Actor");
    const bool ComponentArray=bReferencedArray&&(P==TEXT("/Script/Engine.ActorComponent")||P==TEXT("/Script/Engine.ChildActorComponent")||P==TEXT("/Script/Engine.SkeletalMeshComponent"));
    if(!SafeName(TypeObject,true)||(!ComponentArray&&P!=TEXT("/Script/Engine.Actor")&&P!=TEXT("/Script/CoreUObject.Object")))return Fail(R,TEXT("function_class_descriptor_pending"),TEXT("Scalar constraints remain exact Object/Actor; referenced arrays additionally admit exact native ActorComponent/ChildActorComponent/SkeletalMeshComponent. Generated/plugin/asset classes and loading remain pending."));
    UClass* C=FindObject<UClass>(nullptr,*P);
    if(!NativeConstraintClass(C,bReferencedArray))return Fail(R,TEXT("function_class_source_unavailable"),TEXT("Exact clean compiled-in runtime native class identity is unavailable; no load/CDO warming or substitution."));
    Type=FEdGraphPinType();Type.PinCategory=N==TEXT("class")?UEdGraphSchema_K2::PC_Class:UEdGraphSchema_K2::PC_Object;Type.PinSubCategoryObject=C;
    R.Data->SetNumberField(TEXT("native_class_load_calls"),0);R.Data->SetBoolField(TEXT("native_class_cdo_creation_requested"),false);return true;
}
TSharedRef<FJsonObject> NativeClassSource(const FEdGraphPinType& Type)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),false);J->SetBoolField(TEXT("native_identity_verified"),false);
    if(!NativeConstraintPin(Type)||!IsEditorGameThread()||IsPlaySessionActive()||!IsValid(GEditor)||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage())return J;
    auto* Editor=GEditor;auto* World=GEditor->GetEditorWorldContext().World();
    if(!NativeConstraintLoaded(World)||World->bIsTearingDown)return J;
    const auto Context=[&](){return IsEditorGameThread()&&!IsPlaySessionActive()&&IsValid(GEditor)&&GEditor==Editor&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage()&&NativeConstraintLoaded(World)&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World;};
    UClass* Constraint=CastChecked<UClass>(Type.PinSubCategoryObject.Get());TArray<TSharedPtr<FJsonValue>> Rows;int32 Count=0,Chars=0;
    for(UClass* C=Constraint;C;C=C->GetSuperClass())
    {
        if(++Count>2||!Context()||!NativeConstraintClass(C))return J;
        auto* Package=C->GetPackage();auto* D=C->GetDefaultObject(false);auto* MetaClass=C->GetClass();auto* Outer=C->GetOuter();const auto Flags=C->GetFlags();const auto ClassFlags=C->GetClassFlags();const auto PackageFlags=Package->GetPackageFlags();const auto PackageObjectFlags=Package->GetFlags();
        if(D&&(!NativeConstraintLoaded(D)||D->GetClass()!=C||D->GetOuter()!=Package))return J;
        const auto CDOFlags=D?D->GetFlags():RF_NoFlags;auto* CDOOuter=D?D->GetOuter():nullptr;
        const auto Current=[&](){return Context()&&NativeConstraintClass(C)&&C->GetClass()==MetaClass&&C->GetOuter()==Outer&&C->GetFlags()==Flags&&C->GetClassFlags()==ClassFlags&&C->GetPackage()==Package&&Package->GetPackageFlags()==PackageFlags&&Package->GetFlags()==PackageObjectFlags&&C->GetDefaultObject(false)==D&&(!D||(NativeConstraintLoaded(D)&&D->GetClass()==C&&D->GetOuter()==CDOOuter&&D->GetPackage()==Package&&D->GetFlags()==CDOFlags&&D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)));};
        if(!Current())return J;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),C->GetPathName());Row->SetStringField(TEXT("class"),MetaClass->GetPathName());Row->SetStringField(TEXT("outer"),Outer->GetPathName());Row->SetStringField(TEXT("flags"),LexToString(uint32(Flags)));Row->SetStringField(TEXT("class_flags"),LexToString(uint32(ClassFlags)));Row->SetStringField(TEXT("package"),Package->GetPathName());Row->SetStringField(TEXT("package_flags"),LexToString(uint32(PackageFlags)));Row->SetStringField(TEXT("package_object_flags"),LexToString(uint32(PackageObjectFlags)));Row->SetBoolField(TEXT("dirty"),Package->IsDirty());Row->SetBoolField(TEXT("uasset_required"),false);
        auto Metadata=MakeShared<FJsonObject>();const auto* Map=FMetaData::GetMapForObject(C);if(!Current()||(Map&&Map->Num()>2048))return J;
        if(Map)for(const auto& P:*Map){Chars+=P.Key.ToString().Len()+P.Value.Len();if(P.Key.ToString().Len()>512||P.Value.Len()>65536||Chars>1048576)return J;Metadata->SetStringField(P.Key.ToString(),P.Value);}Row->SetObjectField(TEXT("metadata"),Metadata);
        auto Default=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("existing_cdo"),Default);Default->SetBoolField(TEXT("exists"),D!=nullptr);
        if(D)
        {
            Default->SetStringField(TEXT("path"),D->GetPathName());Default->SetStringField(TEXT("class"),C->GetPathName());Default->SetStringField(TEXT("outer"),CDOOuter?CDOOuter->GetPathName():TEXT(""));Default->SetStringField(TEXT("flags"),LexToString(uint32(CDOFlags)));
            auto Fields=MakeShared<FJsonObject>();Default->SetObjectField(TEXT("fields"),Fields);int32 Properties=0;
            for(TFieldIterator<FProperty> It(C,EFieldIteratorFlags::IncludeSuper);It;++It)
            {
                FProperty* P=*It;if(++Properties>256||P->ArrayDim<1||P->ArrayDim>128)return J;const FString Name=P->GetName();TArray<TSharedPtr<FJsonValue>> Values;
                for(int32 I=0;I<P->ArrayDim;++I){if(!Current())return J;FString V;const auto* Address=P->ContainerPtrToValuePtr<void>(D,I);if(!P->ExportText_Direct(V,Address,Address,D,PPF_None)||!Current())return J;Chars+=V.Len();if(Chars>1048576)return J;Values.Add(MakeShared<FJsonValueString>(V));}
                Fields->SetArrayField(Name,Values);
            }
        }
        if(!Current())return J;Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetStringField(TEXT("constraint"),Constraint->GetPathName());J->SetArrayField(TEXT("hierarchy"),Rows);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetBoolField(TEXT("observations_complete"),true);return J;
}
bool NativeClassDefaultText(const FEdGraphPinType& Type,const TSharedPtr<FJsonValue>& Value,FString& Text,FResult& R)
{
    if(!NativeConstraintPin(Type))return Fail(R,TEXT("function_class_source_unavailable"),TEXT("Exact native object/class constraint required for default admission."));
    if(Value&&(Value->Type==EJson::Null||(Value->Type==EJson::String&&(Value->AsString().IsEmpty()||Value->AsString()==TEXT("None"))))){Text.Reset();return true;}
    return Fail(R,TEXT("function_class_default_pending"),TEXT("Parameter defaults admit null/empty/None only; non-null instance/class asset defaults remain pending."));
}

bool ScalarParameterType(const FString& Name, FEdGraphPinType& T, FResult& R)
{
    const FString N = Name.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
    const TCHAR* Path = N == TEXT("vector") ? TEXT("/Script/CoreUObject.Vector")
        : N == TEXT("rotator") ? TEXT("/Script/CoreUObject.Rotator")
        : N == TEXT("transform") ? TEXT("/Script/CoreUObject.Transform") : nullptr;
    if (!Path) { return PrimitiveType(Name, T, R); }
    T.PinCategory = UEdGraphSchema_K2::PC_Struct; T.PinSubCategoryObject = FindObject<UScriptStruct>(nullptr, Path);
    if (!IsScalarValueStruct(T))
    { return Fail(R, TEXT("function_parameter_struct_unsupported"), TEXT("Requires the exact already loaded, clean runtime CoreUObject Vector/Rotator/Transform scalar value type.")); }
    return true;
}
bool ScalarValueDefaultText(const FEdGraphPinType& T, const TSharedPtr<FJsonValue>& V, FString& Text, FResult& R)
{
    if (!IsScalarValueStruct(T) || !V)
    { return Fail(R, TEXT("function_default_type_pending"), TEXT("Only exact scalar Vector/Rotator/Transform value defaults are accepted here.")); }
    const bool Rotor = T.PinSubCategoryObject.Get() == TBaseStructure<FRotator>::Get();
    const bool Transform = T.PinSubCategoryObject.Get() == TBaseStructure<FTransform>::Get();
    auto Triple = [](const TSharedPtr<FJsonValue>& Value, bool bRotor, double Out[3])
    {
        const TCHAR* Axes[3] = {bRotor ? TEXT("pitch") : TEXT("x"), bRotor ? TEXT("yaw") : TEXT("y"), bRotor ? TEXT("roll") : TEXT("z")};
        if (!Value) { return false; }
        if (Value->Type == EJson::Array)
        {
            const auto& A = Value->AsArray(); if (A.Num() != 3) { return false; }
            for (int32 I = 0; I < 3; ++I) { if (!A[I] || A[I]->Type != EJson::Number || !FMath::IsFinite(A[I]->AsNumber())) { return false; } Out[I] = A[I]->AsNumber(); }
            return true;
        }
        if (Value->Type == EJson::Object)
        {
            const auto O = Value->AsObject(); if (!O) { return false; }
            // The historical axis-object parser defaults omitted axes to zero;
            // an omitted whole Transform scale group still defaults to identity.
            Out[0] = Out[1] = Out[2] = 0;
            for (const auto& Pair : O->Values)
            {
                const FString Key(Pair.Key.ToView()); int32 Axis = INDEX_NONE;
                for (int32 I = 0; I < 3; ++I) { if (Key == Axes[I]) { Axis = I; break; } }
                if (Axis == INDEX_NONE || !Pair.Value || Pair.Value->Type != EJson::Number || !FMath::IsFinite(Pair.Value->AsNumber())) { return false; }
                Out[Axis] = Pair.Value->AsNumber();
            }
            return true;
        }
        return false;
    };
    if (V->Type == EJson::String)
    {
        const FString Literal = V->AsString(); bool Valid = false;
        if (Transform) { FTransform Parsed; Valid = Parsed.InitFromString(Literal) && Parsed.IsValid(); }
        else if (Rotor) { FRotator Parsed; Valid = FDefaultValueHelper::ParseRotator(Literal, Parsed) && !Parsed.ContainsNaN(); }
        else { FVector Parsed; Valid = FDefaultValueHelper::ParseVector(Literal, Parsed) && !Parsed.ContainsNaN(); }
        if (Valid) { Text = Literal; return true; }
    }
    else if (!Transform)
    {
        double XYZ[3] = {0, 0, 0};
        if (Triple(V, Rotor, XYZ)) { Text = FString::Printf(TEXT("%.17g,%.17g,%.17g"), XYZ[0], XYZ[1], XYZ[2]); return true; }
    }
    else if (V->Type == EJson::Object)
    {
        double Location[3] = {0, 0, 0}, Rotation[3] = {0, 0, 0}, Scale[3] = {1, 1, 1};
        const auto O = V->AsObject(); TSet<FString> Groups; bool Valid = O.IsValid();
        if (O) { for (const auto& Pair : O->Values)
        {
            FString Group(Pair.Key.ToView()); if (Group == TEXT("translation")) { Group = TEXT("location"); } else if (Group == TEXT("scale3d")) { Group = TEXT("scale"); }
            if (Groups.Contains(Group) || (Group != TEXT("location") && Group != TEXT("rotation") && Group != TEXT("scale"))) { Valid = false; break; }
            Groups.Add(Group); double* Target = Group == TEXT("location") ? Location : Group == TEXT("rotation") ? Rotation : Scale;
            if (!Triple(Pair.Value, Group == TEXT("rotation"), Target)) { Valid = false; break; }
        } }
        if (Valid)
        {
            const FTransform Parsed(FRotator(Rotation[0], Rotation[1], Rotation[2]), FVector(Location[0], Location[1], Location[2]), FVector(Scale[0], Scale[1], Scale[2]));
            if (Parsed.IsValid()) { Text = Parsed.ToString(); return true; }
        }
    }
    return Fail(R, TEXT("function_default_invalid"), TEXT("Use a finite three-number Vector/Rotator array, lowercase axis object, schema-valid literal, or Transform location/translation, rotation, scale/scale3d groups. Null, extra fields and partial arrays are unsupported."));
}
bool InputKeyDefaultText(const FEdGraphPinType& T, const TSharedPtr<FJsonValue>& V, FString& Text, FResult& R)
{
    if (T.PinCategory != UEdGraphSchema_K2::PC_Struct || !T.PinSubCategory.IsNone()
        || T.PinSubCategoryObject.Get() != FKey::StaticStruct() || T.IsContainer()
        || T.bIsReference || T.bIsConst || T.bIsWeakPointer || !V)
    { return Fail(R, TEXT("function_key_default_type_invalid"), TEXT("The audited Key default requires an exact scalar non-reference FKey input.")); }
    FString Name;
    if (V->Type == EJson::Object)
    {
        const auto Object = V->AsObject();
        if (Object->Values.Num() != 1 || !Object->TryGetStringField(TEXT("key_name"), Name))
        { return Fail(R, TEXT("function_key_default_invalid"), TEXT("Provide exactly {key_name: <digital key name>} or the strict legacy (KeyName=\"...\") literal.")); }
    }
    else if (V->Type == EJson::String)
    {
        const FString Literal = V->AsString();
        const FString Prefix = TEXT("(KeyName=\""); const FString Suffix = TEXT("\")");
        if (!Literal.StartsWith(Prefix, ESearchCase::CaseSensitive) || !Literal.EndsWith(Suffix, ESearchCase::CaseSensitive))
        { return Fail(R, TEXT("function_key_default_invalid"), TEXT("Only the exact legacy (KeyName=\"...\") spelling is accepted as a string FKey default.")); }
        Name = Literal.Mid(Prefix.Len(), Literal.Len() - Prefix.Len() - Suffix.Len());
    }
    else { return Fail(R, TEXT("function_key_default_invalid"), TEXT("A Key default must be a typed key_name object or a strict legacy literal, never null/number/bool.")); }
    // Limit to a single finite identifier; never feed caller-authored struct text
    // into an importer. The engine's exporter produces the canonical pin token.
    if (!Identifier(Name)) { return Fail(R, TEXT("function_key_default_invalid"), TEXT("A bounded digital key identifier without whitespace or additional fields is required.")); }
    const FKey Key{FName(*Name)};
    if (!Key.IsValid() || !Key.IsDigital() || Key.IsAnalog() || Key.IsDeprecated() || !Key.IsBindableInBlueprints())
    { return Fail(R, TEXT("function_key_default_unsupported"), TEXT("Unknown, analog, deprecated or non-Blueprint-bindable keys are rejected before node creation.")); }
    Text.Reset();
    if (!Key.ExportTextItem(Text, FKey(), nullptr, PPF_None, nullptr) || Text != Key.GetFName().ToString())
    { return Fail(R, TEXT("function_key_default_invalid"), TEXT("The Engine did not produce the expected canonical FKey pin token.")); }
    return true;
}
bool PrimitiveArrayDefaultText(const FEdGraphPinType& T,const TSharedPtr<FJsonValue>& V,FString& Text,FResult& R)
{
    FEdGraphPinType Element=T;Element.ContainerType=EPinContainerType::None;
    const bool NativeElement=IsScalarValueStruct(Element);
    if(!T.IsArray()||(!IsPrimitive(Element)&&!NativeElement)||!V)return Fail(R,TEXT("function_array_default_type_unsupported"),TEXT("Only admitted primitive or exact native Vector/Rotator/Transform Array declaration defaults are supported."));
    // OK: UE56 converts the descriptor's Any value to one declaration string.
    // ContainerType does not turn Vector/Rotator triples into per-element initialization.
    if(NativeElement)
    {
        const bool Rotator=Element.PinSubCategoryObject.Get()==TBaseStructure<FRotator>::Get();
        const bool Transform=Element.PinSubCategoryObject.Get()==TBaseStructure<FTransform>::Get();
        if(!Transform&&(V->Type==EJson::Array||V->Type==EJson::Object))
        {
            double Axes[3]={0.0,0.0,0.0};
            if(V->Type==EJson::Array)
            {
                const auto& Items=V->AsArray();
                for(int32 Axis=0;Axis<3;++Axis)if(Items.IsValidIndex(Axis)&&Items[Axis].IsValid())Axes[Axis]=Items[Axis]->AsNumber();
            }
            else
            {
                const auto Object=V->AsObject();
                Object->TryGetNumberField(Rotator?TEXT("pitch"):TEXT("x"),Axes[0]);
                Object->TryGetNumberField(Rotator?TEXT("yaw"):TEXT("y"),Axes[1]);
                Object->TryGetNumberField(Rotator?TEXT("roll"):TEXT("z"),Axes[2]);
            }
            if(!FMath::IsFinite(Axes[0])||!FMath::IsFinite(Axes[1])||!FMath::IsFinite(Axes[2]))return Fail(R,TEXT("function_array_default_invalid"),TEXT("Native triple declaration values must remain finite."));
            if(Rotator)Text=FString::Printf(TEXT("(Pitch=%s,Yaw=%s,Roll=%s)"),*FString::SanitizeFloat(Axes[0]),*FString::SanitizeFloat(Axes[1]),*FString::SanitizeFloat(Axes[2]));
            else Text=FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),*FString::SanitizeFloat(Axes[0]),*FString::SanitizeFloat(Axes[1]),*FString::SanitizeFloat(Axes[2]));
            return true;
        }
        if(Transform&&V->Type==EJson::Object)
        {
            const auto Object=V->AsObject();
            const auto LocationValue=Object->HasField(TEXT("location"))?Object->TryGetField(TEXT("location")):Object->TryGetField(TEXT("translation"));
            const auto RotationValue=Object->TryGetField(TEXT("rotation"));
            const auto ScaleValue=Object->HasField(TEXT("scale"))?Object->TryGetField(TEXT("scale")):Object->TryGetField(TEXT("scale3d"));
            FEdGraphPinType VectorArray=T;VectorArray.PinSubCategoryObject=TBaseStructure<FVector>::Get();
            FEdGraphPinType RotatorArray=T;RotatorArray.PinSubCategoryObject=TBaseStructure<FRotator>::Get();
            FString Location=TEXT("(X=0,Y=0,Z=0)"),Rotation=TEXT("(Pitch=0,Yaw=0,Roll=0)"),Scale=TEXT("(X=1,Y=1,Z=1)");
            if(LocationValue.IsValid()&&!PrimitiveArrayDefaultText(VectorArray,LocationValue,Location,R))return false;
            if(RotationValue.IsValid()&&!PrimitiveArrayDefaultText(RotatorArray,RotationValue,Rotation,R))return false;
            if(ScaleValue.IsValid()&&!PrimitiveArrayDefaultText(VectorArray,ScaleValue,Scale,R))return false;
            FVector ParsedLocation=FVector::ZeroVector;FRotator ParsedRotation=FRotator::ZeroRotator;FVector ParsedScale=FVector::OneVector;
            if(!FDefaultValueHelper::ParseVector(Location,ParsedLocation)||!FDefaultValueHelper::ParseRotator(Rotation,ParsedRotation)||!FDefaultValueHelper::ParseVector(Scale,ParsedScale)||ParsedLocation.ContainsNaN()||ParsedRotation.ContainsNaN()||ParsedScale.ContainsNaN())
                return Fail(R,TEXT("function_array_transform_component_invalid"),TEXT("Transform location/rotation/scale must parse to finite components; no silent default-component fallback."));
            Text=FTransform(ParsedRotation,ParsedLocation,ParsedScale).ToString();return true;
        }
    }
    // UE56 GetPinDefaultStringForTypeChecked requires a JSON boolean even for
    // Boolean arrays; do not silently project [] or null to an omitted default.
    if(T.PinCategory==UEdGraphSchema_K2::PC_Boolean&&V->Type!=EJson::Boolean)
        return Fail(R,TEXT("function_array_default_invalid"),TEXT("Boolean default values must be JSON booleans, including Boolean array declarations."));
    if(V->Type==EJson::String){Text=V->AsString();return true;}
    if(V->Type==EJson::Boolean){Text=V->AsBool()?TEXT("true"):TEXT("false");return true;}
    if(V->Type==EJson::Number)
    {
        const double N=V->AsNumber();
        Text=(T.PinCategory==UEdGraphSchema_K2::PC_Int||T.PinCategory==UEdGraphSchema_K2::PC_Int64)
            ?FString::Printf(TEXT("%lld"),static_cast<int64>(N)):FString::SanitizeFloat(N);return true;
    }
    if(V->Type==EJson::Array||V->Type==EJson::Object)
    {
        Text.Reset();const TSharedRef<TJsonWriter<>> Writer=TJsonWriterFactory<>::Create(&Text);
        if(!FJsonSerializer::Serialize(V,TEXT(""),Writer))return Fail(R,TEXT("function_array_default_invalid"),TEXT("The bounded JSON declaration default could not be serialized."));
        return true;
    }
    if(V->Type==EJson::Null){Text.Reset();return true;}
    return Fail(R,TEXT("function_array_default_invalid"),TEXT("Unsupported primitive array declaration JSON default."));
}
bool ScalarUdsDeclarationDefaultText(const TSharedPtr<FJsonValue>& V,FString& Text,FResult& R)
{
    if(!V)return Fail(R,TEXT("function_uds_default_invalid"),TEXT("Original bounded Any declaration value required."));
    if(V->Type==EJson::Null)Text.Reset();else if(V->Type==EJson::String)Text=V->AsString();else if(V->Type==EJson::Boolean)Text=V->AsBool()?TEXT("true"):TEXT("false");else if(V->Type==EJson::Number){if(!FMath::IsFinite(V->AsNumber()))return Fail(R,TEXT("function_uds_default_invalid"),TEXT("Finite numeric declaration text required."));Text=FString::SanitizeFloat(V->AsNumber());}else if(!FJsonSerializer::Serialize(V,TEXT(""),TJsonWriterFactory<>::Create(&Text)))return Fail(R,TEXT("function_uds_default_invalid"),TEXT("Original JSON fallback declaration text could not be serialized."));
    if(Text.Len()>1048576)return Fail(R,TEXT("function_uds_default_invalid"),TEXT("One bounded declaration string required; no array-element or UDS field import."));
    const FString Lower=Text.ToLower();for(const TCHAR* Token:{TEXT("cubelessmcp"),TEXT("cubelesscontrolrigtools"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal")})if(Lower.Contains(Token))return Fail(R,TEXT("function_uds_default_dependency"),TEXT("Original dependency-reference guard rejected the declaration text."));return true;
}
bool DefaultText(const FEdGraphPinType& T, const TSharedPtr<FJsonValue>& V, FString& Text, FResult& R)
{
    if (RuntimeObject::IsSupportedPin(T))
    {
        // K2 stores object "None" as empty text, null DefaultObject and empty
        // DefaultTextValue. DoesDefaultValueMatch compares its input directly;
        // use that canonical form so an existing null is an exact no-op.
        if (V && V->Type == EJson::Null) { Text.Reset(); return true; }
        return Fail(R, TEXT("function_object_default_pending"), TEXT("Audited runtime object pin defaults accept JSON null only; concrete object/class asset defaults remain pending."));
    }
    if (!IsPrimitive(T) || !V) { return Fail(R, TEXT("function_default_type_pending"), TEXT("Only supported scalar primitive pin defaults are available in this slice.")); }
    // UE56 forwards numeric JSON strings to the SDK. Admit bounded decimal
    // integers without a double roundtrip; keep the original literal for K2.
    if (V->Type == EJson::String && (T.PinCategory == UEdGraphSchema_K2::PC_Int || T.PinCategory == UEdGraphSchema_K2::PC_Int64 || T.PinCategory == UEdGraphSchema_K2::PC_Real))
    {
        const FString Raw=V->AsString();const FString S=Raw.TrimStartAndEnd();bool Valid=false;
        if(!S.IsEmpty() && S.Len()<=256)
        {
            if(T.PinCategory==UEdGraphSchema_K2::PC_Int || T.PinCategory==UEdGraphSchema_K2::PC_Int64)
            {
                int32 Start=0;const bool Negative=S[0]==TEXT('-');if(Negative||S[0]==TEXT('+'))Start=1;
                const uint64 Limit=T.PinCategory==UEdGraphSchema_K2::PC_Int?(Negative?uint64(2147483648ULL):uint64(2147483647ULL)):(Negative?uint64(9223372036854775808ULL):uint64(9223372036854775807ULL));
                uint64 Magnitude=0;Valid=Start<S.Len();
                for(int32 I=Start;Valid&&I<S.Len();++I)
                {
                    const TCHAR C=S[I];if(C<TEXT('0')||C>TEXT('9')){Valid=false;break;}
                    const uint64 Digit=uint64(C-TEXT('0'));if(Magnitude>(Limit-Digit)/10){Valid=false;break;}Magnitude=Magnitude*10+Digit;
                }
            }
            else
            {
                double N=0;Valid=FDefaultValueHelper::ParseDouble(S,N)&&FMath::IsFinite(N)&&(T.PinSubCategory==UEdGraphSchema_K2::PC_Double||FMath::Abs(N)<=MAX_flt);
            }
        }
        if(Valid){Text=Raw;return true;}
        return Fail(R,TEXT("function_default_invalid"),TEXT("Numeric string must fit a finite scalar or bounded decimal integer; non-decimal integer literals remain unsupported."));
    }
    if (T.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    { if (V->Type == EJson::Boolean) { Text = V->AsBool() ? TEXT("true") : TEXT("false"); return true; } }
    else if (T.PinCategory == UEdGraphSchema_K2::PC_Int || T.PinCategory == UEdGraphSchema_K2::PC_Int64)
    { if (V->Type == EJson::Number) { const double N = V->AsNumber(); if (FMath::IsFinite(N) && FMath::TruncToDouble(N) == N && N >= -9223372036854775808.0 && N < 9223372036854775808.0 && (T.PinCategory != UEdGraphSchema_K2::PC_Int || (N >= MIN_int32 && N <= MAX_int32))) { Text = LexToString(static_cast<int64>(N)); return true; } } }
    else if (T.PinCategory == UEdGraphSchema_K2::PC_Real)
    { if (V->Type == EJson::Number) { const double N = V->AsNumber(); if (FMath::IsFinite(N) && T.PinSubCategory == UEdGraphSchema_K2::PC_Double) { Text = FString::Printf(TEXT("%.17g"), N); return true; } if (FMath::IsFinite(N) && FMath::Abs(N) <= MAX_flt) { Text = FString::Printf(TEXT("%.9g"), static_cast<double>(static_cast<float>(N))); return true; } } }
    else if (V->Type == EJson::String && (T.PinCategory != UEdGraphSchema_K2::PC_Name || SafeName(V->AsString(), true))) { Text = V->AsString(); return true; }
    return Fail(R, TEXT("function_default_invalid"), TEXT("Default JSON value does not fit the declared primitive pin type or its bounds."));
}
TSharedRef<FJsonObject> Describe(const FEdGraphPinType& T)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("category"), T.PinCategory.ToString()); J->SetStringField(TEXT("subcategory"), T.PinSubCategory.ToString());
    J->SetBoolField(TEXT("is_array"), T.IsArray()); J->SetBoolField(TEXT("is_reference"), T.bIsReference);
    J->SetStringField(TEXT("subcategory_object"), T.PinSubCategoryObject.IsValid() ? T.PinSubCategoryObject->GetPathName() : TEXT("")); return J;
}
}
