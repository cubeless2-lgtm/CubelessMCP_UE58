// Dedicated legacy actor-property and focus contracts. Editor-only; no transport or gameplay dependencies.
#include "Compatibility/UE58/CubelessActorPropertyFocusApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "IAssetViewport.h"
#include "ILevelEditor.h"
#include "Kismet/GameplayStatics.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Math/Color.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "SLevelViewport.h"
#include "Templates/PointerVariants.h"
#include "UnrealClient.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtr.h"
#include <cmath>

namespace Cubeless::UE58::ActorPropertyFocus
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxActors=1000000,MaxClients=65536;
bool Running=false;
bool Live(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Finite(const FVector& V) { return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z); }
bool Finite(const FRotator& R) { return FMath::IsFinite(R.Pitch)&&FMath::IsFinite(R.Yaw)&&FMath::IsFinite(R.Roll); }
bool SafeText(const FString& S) { if(S.Len()>MaxJsonChars) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }return true; }
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& K) { return FCrc::StrCrc32(*K); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||!SafeText(Text)) { return false; }auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        { const FString Key(Reader->GetIdentifier());if(Keys.Last().Contains(Key)) { return false; }Keys.Last().Add(Key); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()||Values[0]->Type!=EJson::Object) { return false; }Out=Values[0]->AsObject();return Out.IsValid();
}
TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> RotationJson(const FRotator& R)
{ return {MakeShared<FJsonValueNumber>(R.Pitch),MakeShared<FJsonValueNumber>(R.Yaw),MakeShared<FJsonValueNumber>(R.Roll)}; }
FString JsonValue(const TSharedPtr<FJsonValue>& V)
{ auto O=MakeShared<FJsonObject>();O->SetField(TEXT("value"),V);FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S; }
TArray<FString> DirtyPackages()
{ TArray<UPackage*> Packages;FEditorFileUtils::GetDirtyPackages(Packages);TArray<FString> Names;for(UPackage* P:Packages) { if(Live(P)) { Names.AddUnique(P->GetName()); } }Names.Sort();return Names; }
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> Out;for(const FString& S:Values) { Out.Add(MakeShared<FJsonValueString>(S)); }return Out; }
bool Dependency(const FString& Value)
{
    FString S=FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();S.TrimQuotesInline();
    for(const TCHAR* Token:{TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("/Script/CubelessControlRigTools"),TEXT("CubelessControlRigTools"),TEXT("CubelessControlRigToolsEditor"),TEXT("CubelessMCP_UE58"),TEXT("CubelessMCPUE58"),TEXT("/Script/ModelContextProtocol"),TEXT("/Script/ToolsetRegistry")})
    { if(S.Contains(Token,ESearchCase::IgnoreCase)) { return true; } }return false;
}
bool SafeReference(UObject* O)
{
    if(!O) { return true; }if(!Live(O)||Dependency(O->GetPathName())) { return false; }
    UClass* C=Cast<UClass>(O);if(!C) { C=O->GetClass(); }int32 Depth=0;
    while(C) { if(++Depth>256||!Live(C)||Dependency(C->GetPathName())) { return false; }C=C->GetSuperClass(); }return true;
}
TSharedPtr<FJsonValue> PropertyValue(FProperty* P,void* A)
{
    if(auto* B=CastField<FBoolProperty>(P)) { return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(A)); }
    if(auto* I=CastField<FIntProperty>(P)) { return MakeShared<FJsonValueNumber>(I->GetPropertyValue(A)); }
    if(auto* F=CastField<FFloatProperty>(P)) { const float N=F->GetPropertyValue(A);if(FMath::IsFinite(N)) { return MakeShared<FJsonValueNumber>(N); }return {}; }
    if(auto* S=CastField<FStrProperty>(P)) { return MakeShared<FJsonValueString>(S->GetPropertyValue(A)); }
    if(auto* S=CastField<FStructProperty>(P))
    {
        if(S->Struct!=TBaseStructure<FLinearColor>::Get()) { return {}; }const FLinearColor C=*static_cast<const FLinearColor*>(A);
        if(!FMath::IsFinite(C.R)||!FMath::IsFinite(C.G)||!FMath::IsFinite(C.B)||!FMath::IsFinite(C.A)) { return {}; }
        return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(C.R),MakeShared<FJsonValueNumber>(C.G),MakeShared<FJsonValueNumber>(C.B),MakeShared<FJsonValueNumber>(C.A)});
    }
    if(auto* O=CastField<FObjectPropertyBase>(P)) { UObject* V=O->GetObjectPropertyValue(A);if(V&&!Live(V)) { return {}; }if(V) { return MakeShared<FJsonValueString>(V->GetPathName()); }return MakeShared<FJsonValueNull>(); }
    if(auto* B=CastField<FByteProperty>(P)) { return MakeShared<FJsonValueNumber>(B->GetPropertyValue(A)); }
    if(auto* E=CastField<FEnumProperty>(P)) { auto* N=E->GetUnderlyingProperty();if(!N) { return {}; }return MakeShared<FJsonValueString>(N->GetNumericPropertyValueToString(A)); }
    return {};
}
struct FWork
{
    FResult Result;FString Command,FailedStage,PropertyName;TSharedPtr<FJsonObject> Params;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AActor> Actor;TWeakObjectPtr<UClass> ActorClass;TWeakObjectPtr<UPackage> Package;
    FProperty* OriginalProperty=nullptr;TSharedPtr<FJsonValue> Before,Expected;bool PropertyCaptured=false,FieldWritten=false,DirtyKnown=false,DirtyBefore=false,DirtyCaptured=false;TArray<FString> DirtyBeforeNames;
    FWork(const TCHAR* Name):Command(Name)
    {
        Result.Compiled=false;Legacy->SetBoolField(TEXT("success"),false);Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);Result.Data->SetObjectField(TEXT("postconditions"),Checks);Result.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);
        for(const TCHAR* K:{TEXT("automatic_retry_permitted"),TEXT("atomic_rollback_available"),TEXT("explicit_modify_attempted"),TEXT("explicit_pre_edit_change_attempted"),TEXT("explicit_post_edit_change_attempted"),TEXT("explicit_mark_package_dirty_attempted"),TEXT("explicit_compile_attempted"),TEXT("explicit_save_attempted"),TEXT("explicit_transaction_attempted"),TEXT("render_completion_verified"),TEXT("complete_global_side_effect_inventory")}) { Result.Data->SetBoolField(K,false); }
        Result.Data->SetStringField(TEXT("scope"),TEXT("Original GWorld/GetName first-match actor scalar/reflection9 and active/fallback level viewport focus contracts, guarded to the Editor world and no PIE. No generic ImportPropertyWithNotify, label selector, actor bounds framing, UI automation, render wait, save or rollback."));
        Result.Data->SetStringField(TEXT("retry_risk"),TEXT("A reference load can initialize objects before failure; a direct property or camera write can remain after later readback/lifetime/report failure. No uncertain request is automatically replayed or restored."));
        for(const TCHAR* K:{TEXT("actor_enumeration_calls"),TEXT("actor_enumeration_returns"),TEXT("dirty_snapshot_calls"),TEXT("dirty_snapshot_returns"),TEXT("object_load_calls"),TEXT("object_load_returns"),TEXT("field_write_calls"),TEXT("field_write_returns"),TEXT("property_read_calls"),TEXT("property_read_returns"),TEXT("actor_details_calls"),TEXT("actor_details_returns"),TEXT("active_viewport_queries"),TEXT("set_view_location_calls"),TEXT("set_view_location_returns"),TEXT("set_view_rotation_calls"),TEXT("set_view_rotation_returns"),TEXT("client_invalidate_calls"),TEXT("client_invalidate_returns"),TEXT("Modify_calls"),TEXT("PreEditChange_calls"),TEXT("PostEditChange_calls"),TEXT("MarkPackageDirty_calls"),TEXT("compile_calls"),TEXT("save_calls"),TEXT("begin_transaction_calls"),TEXT("wrapper_wait_calls")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* Key) { double N=0;Calls->TryGetNumberField(Key,N);Calls->SetNumberField(Key,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error)
    { if(Result.Error.IsEmpty()) { FailedStage=Result.Stage;Result.ErrorCode=FString(TEXT("actor_property_focus_"))+Code;Result.Error=Error; }Result.bSuccess=false;Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false; }
    bool Request(const FString& Text)
    { Result.Stage=TEXT("parse_params");if(!Parse(Text,Params)) { return Fail(TEXT("params_invalid"),TEXT("This dedicated actor/focus API requires one bounded JSON params object")); }Result.Data->SetStringField(TEXT("params_json"),Text);Result.Data->SetObjectField(TEXT("params"),Params.ToSharedRef());return true; }
    bool Enter()
    {
        Result.Stage=TEXT("editor_admission");if(!IsEditorGameThread()||!Live(GEditor)) { return Fail(TEXT("editor_required"),TEXT("Requires the Editor game thread")); }if(Running) { return Fail(TEXT("reentrant_call"),TEXT("Another actor/property focus operation is active")); }
        if(IsPlaySessionActive()) { return Fail(TEXT("PIE_disabled"),TEXT("Actor property and focus operations are disabled during PIE/SIE")); }Editor=GEditor;Result.bChangeStateKnown=false;
        UWorld* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||GWorld!=W||W->WorldType!=EWorldType::Editor) { return Fail(TEXT("GWorld_ambiguous"),TEXT("Original GWorld selection requires the current Editor world")); }World=W;Prefix->SetStringField(TEXT("world_before"),W->GetPathName());Count(TEXT("dirty_snapshot_calls"));DirtyBeforeNames=DirtyPackages();Count(TEXT("dirty_snapshot_returns"));DirtyCaptured=true;Prefix->SetArrayField(TEXT("dirty_packages_before"),Strings(DirtyBeforeNames));return true;
    }
    bool AdmitWorld()
    {
        if(!IsEditorGameThread()||!Live(Editor.Get())||GEditor!=Editor.Get()||IsPlaySessionActive()||!Live(World.Get())) { return Fail(TEXT("editor_world_changed"),TEXT("Editor/world lifetime or PIE state changed during the operation")); }
        Result.bChangeStateKnown=false;if(GEditor->GetEditorWorldContext().World()!=World.Get()||GWorld!=World.Get()) { return Fail(TEXT("GWorld_changed"),TEXT("The original captured GWorld is no longer the current Editor world")); }return true;
    }
    bool FindActor(const FString& Name)
    {
        Result.Stage=TEXT("find_actor");if(!SafeText(Name)) { return Fail(TEXT("actor_name_invalid"),TEXT("Actor name exceeds the text bound or contains a NUL")); }if(!AdmitWorld()) { return false; }
        Count(TEXT("actor_enumeration_calls"));TArray<AActor*> Actors;UGameplayStatics::GetAllActorsOfClass(GWorld,AActor::StaticClass(),Actors);Count(TEXT("actor_enumeration_returns"));Prefix->SetNumberField(TEXT("actor_enumeration_count"),Actors.Num());
        if(!AdmitWorld()) { return false; }if(Actors.Num()>MaxActors) { return Fail(TEXT("actor_limit"),TEXT("Actor enumeration exceeds the bounded inspection limit")); }
        for(AActor* A:Actors) { if(Live(A)&&A->GetName()==Name) { Actor=A;break; } }
        if(!Live(Actor.Get())) { return Fail(TEXT("actor_not_found"),FString::Printf(TEXT("Actor not found: %s"),*Name)); }ActorClass=Actor->GetClass();Package=Actor->GetOutermost();Prefix->SetStringField(TEXT("actor_path"),Actor->GetPathName());Prefix->SetStringField(TEXT("actor_class"),ActorClass->GetPathName());Prefix->SetNumberField(TEXT("actor_object_id"),Actor->GetUniqueID());
        DirtyKnown=Live(Package.Get());if(DirtyKnown) { DirtyBefore=Package->IsDirty();Prefix->SetStringField(TEXT("package"),Package->GetName());Prefix->SetBoolField(TEXT("dirty_before"),DirtyBefore); }return true;
    }
    bool AdmitActor()
    {
        if(!AdmitWorld()) { return false; }if(!Live(Actor.Get())||!Live(ActorClass.Get())||Actor->GetWorld()!=World.Get()||Actor->GetClass()!=ActorClass.Get()) { return Fail(TEXT("actor_lifetime_changed"),TEXT("Actor/class lifetime or world changed during the operation")); }return true;
    }
    FProperty* Property()
    {
        if(!AdmitActor()) { return nullptr; }auto* P=Actor->GetClass()->FindPropertyByName(*PropertyName);
        if(!P||P!=OriginalProperty) { Fail(TEXT("property_lifetime_changed"),TEXT("Captured property changed or disappeared during the operation"));return nullptr; }return P;
    }
    void Write(const TCHAR* Stage,const TCHAR* Counter)
    { Result.Stage=Stage;Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Counter); }
    TSharedPtr<FJsonValue> Read(FProperty* P)
    { Count(TEXT("property_read_calls"));auto V=PropertyValue(P,P->ContainerPtrToValuePtr<void>(Actor.Get()));Count(TEXT("property_read_returns"));return V; }
    FResult Finish()
    {
        if(PropertyCaptured)
        {
            const FString PriorStage=Result.Stage;Result.Stage=TEXT("observe_property_after");FProperty* P=Property();auto After=P?Read(P):TSharedPtr<FJsonValue>();Prefix->SetBoolField(TEXT("property_after_known"),After.IsValid());
            if(After.IsValid())
            {
                Result.Data->SetField(TEXT("property_after"),After);if(Before.IsValid()&&!JsonValue(Before).Equals(JsonValue(After),ESearchCase::CaseSensitive)) { Result.bChanged=true; }
                if(FieldWritten&&Expected.IsValid()) { const bool Equal=JsonValue(Expected).Equals(JsonValue(After),ESearchCase::CaseSensitive);Checks->SetBoolField(TEXT("property_matches_expected"),Equal);if(!Equal) { Fail(TEXT("property_readback_mismatch"),TEXT("The actual property differs from the requested converted value after the write")); } }
            }
            else if(FieldWritten&&Result.Error.IsEmpty()) { Fail(TEXT("property_readback_unavailable"),TEXT("The property write returned but its final value could not be observed")); }
            if(Result.Error.IsEmpty()) { Result.Stage=PriorStage; }
        }
        if(IsEditorGameThread()&&GEditor==Editor.Get())
        {
            Prefix->SetBoolField(TEXT("actor_still_live"),Live(Actor.Get()));Prefix->SetBoolField(TEXT("original_package_still_live"),Live(Package.Get()));
            if(DirtyKnown&&Live(Package.Get())) { const bool After=Package->IsDirty();Prefix->SetBoolField(TEXT("dirty_after"),After);if(DirtyBefore!=After) { Result.bChanged=true; } }
            Result.bChangeStateKnown=false;UWorld* Current=GEditor->GetEditorWorldContext().World();if(Live(Current)) { Prefix->SetStringField(TEXT("world_after"),Current->GetPathName()); }
            if(DirtyCaptured) { Count(TEXT("dirty_snapshot_calls"));const auto After=DirtyPackages();Count(TEXT("dirty_snapshot_returns"));Prefix->SetArrayField(TEXT("dirty_packages_after"),Strings(After));if(After!=DirtyBeforeNames) { Result.bChanged=true; } }
        }
        Result.Data->SetStringField(TEXT("failed_stage"),FailedStage);Checks->SetBoolField(TEXT("explicit_notify_compile_save_not_attempted"),true);Result.bSuccess=Result.Error.IsEmpty();Result.bPostconditionPassed=Result.bSuccess;if(Result.bSuccess) { Result.Stage=TEXT("complete"); }else { Result.Stage=FailedStage; }return Result;
    }
};
enum class EKind:uint8 { Bool,Int,Float,String,LinearColor,Class,Object,Byte,Enum };
struct FPrepared
{
    EKind Kind=EKind::Bool;bool Bool=false;int32 Int=0;float Float=0;FString String;FLinearColor Color=FLinearColor::Black;uint8 Byte=0;int64 Enum=0;TStrongObjectPtr<UObject> Reference;
};
bool FloatValue(FWork& W,double Number,float& Out)
{ if(!FMath::IsFinite(Number)||Number>MAX_flt||Number<-MAX_flt) { return W.Fail(TEXT("numeric_range"),TEXT("Value must remain finite and representable as the original float32 type")); }Out=static_cast<float>(Number);return true; }
bool IntegerValue(FWork& W,double Number,double Minimum,double ExclusiveMaximum,int64& Out)
{ if(!FMath::IsFinite(Number)) { return W.Fail(TEXT("numeric_range"),TEXT("Nonfinite integer conversion is not supported")); }const double N=std::trunc(Number);if(N<Minimum||N>=ExclusiveMaximum) { return W.Fail(TEXT("numeric_range"),TEXT("Integer conversion is outside its defined legacy cast range")); }Out=static_cast<int64>(N);return true; }
bool EnumValue(FWork& W,const TSharedPtr<FJsonValue>& Value,UEnum* Enum,bool Byte,int64& Out)
{
    if(Value->Type==EJson::Number) { return IntegerValue(W,Value->AsNumber(),Byte?0.0:-9223372036854775808.0,Byte?256.0:9223372036854775808.0,Out); }
    if(Value->Type!=EJson::String) { return false; }FString Name=Value->AsString();
    if(Name.IsNumeric()) { Out=Byte?static_cast<int64>(static_cast<uint8>(FCString::Atoi(*Name))):FCString::Atoi64(*Name);return true; }
    if(Name.Contains(TEXT("::"))) { Name.Split(TEXT("::"),nullptr,&Name); }Out=Enum->GetValueByNameString(Name);if(Out==INDEX_NONE) { Out=Enum->GetValueByNameString(Value->AsString()); }
    if(Out==INDEX_NONE) { return W.Fail(TEXT("enum_name_not_found"),FString::Printf(TEXT("Could not find enum value for '%s'"),*Name)); }return true;
}
bool Prepare(FWork& W,FProperty* P,const TSharedPtr<FJsonValue>& V,FPrepared& Out)
{
    W.Result.Stage=TEXT("convert_property_value");
    if(CastField<FBoolProperty>(P)) { Out.Kind=EKind::Bool;Out.Bool=V->AsBool();W.Expected=MakeShared<FJsonValueBoolean>(Out.Bool); }
    else if(CastField<FIntProperty>(P)) { Out.Kind=EKind::Int;int64 N=0;if(!IntegerValue(W,V->AsNumber(),-2147483648.0,2147483648.0,N)) { return false; }Out.Int=static_cast<int32>(N);W.Expected=MakeShared<FJsonValueNumber>(Out.Int); }
    else if(CastField<FFloatProperty>(P)) { Out.Kind=EKind::Float;if(!FloatValue(W,V->AsNumber(),Out.Float)) { return false; }W.Expected=MakeShared<FJsonValueNumber>(Out.Float); }
    else if(CastField<FStrProperty>(P)) { Out.Kind=EKind::String;Out.String=V->AsString();W.Expected=MakeShared<FJsonValueString>(Out.String); }
    else if(auto* S=CastField<FStructProperty>(P))
    {
        if(S->Struct!=TBaseStructure<FLinearColor>::Get()) { return W.Fail(TEXT("property_type_unsupported"),FString::Printf(TEXT("Unsupported property type: %s for property %s"),*P->GetClass()->GetName(),*W.PropertyName)); }
        Out.Kind=EKind::LinearColor;if(V->Type!=EJson::Array) { return W.Fail(TEXT("color_array_required"),FString::Printf(TEXT("LinearColor property %s requires an array [r,g,b,a]"),*W.PropertyName)); }const auto& A=V->AsArray();if(A.Num()!=4) { return W.Fail(TEXT("color_size"),FString::Printf(TEXT("LinearColor property %s requires 4 values, got %d"),*W.PropertyName,A.Num())); }
        float C[4];for(int32 I=0;I<4;++I) { if(!A[I].IsValid()) { return W.Fail(TEXT("color_value_invalid"),TEXT("LinearColor JSON value is unavailable")); }if(!FloatValue(W,A[I]->AsNumber(),C[I])) { return false; } }Out.Color=FLinearColor(C[0],C[1],C[2],C[3]);W.Expected=MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(C[0]),MakeShared<FJsonValueNumber>(C[1]),MakeShared<FJsonValueNumber>(C[2]),MakeShared<FJsonValueNumber>(C[3])});
    }
    else if(auto* O=CastField<FObjectPropertyBase>(P))
    {
        auto* C=CastField<FClassProperty>(P);Out.Kind=C?EKind::Class:EKind::Object;const FString Label=C?TEXT("Class"):TEXT("Object");
        if(V->Type!=EJson::String) { return W.Fail(TEXT("reference_string_required"),FString::Printf(TEXT("%s property %s requires %s path string"),*Label,*W.PropertyName,C?TEXT("a class"):TEXT("an object"))); }
        const FString Path=V->AsString();if(!SafeText(Path)) { return W.Fail(TEXT("reference_path_invalid"),TEXT("Reference path exceeds the text bound or contains a NUL")); }
        if(!Path.IsEmpty()&&Dependency(Path)) { return W.Fail(TEXT("dependency_reference"),FString::Printf(TEXT("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."),*W.PropertyName,*Path)); }
        if(!Path.IsEmpty())
        {
            UClass* LoadClass=C?UClass::StaticClass():O->PropertyClass.Get();if(!Live(LoadClass)) { return W.Fail(TEXT("reference_class_invalid"),TEXT("Property reference class is unavailable before load")); }
            W.Prefix->SetStringField(TEXT("requested_reference_path"),Path);W.Write(TEXT("load_property_reference"),TEXT("object_load_calls"));Out.Reference.Reset(StaticLoadObject(LoadClass,nullptr,FStringView(Path)));W.Count(TEXT("object_load_returns"));W.Prefix->SetBoolField(TEXT("reference_load_returned_object"),Out.Reference.IsValid());
            P=W.Property();if(!P) { return false; }O=CastField<FObjectPropertyBase>(P);C=CastField<FClassProperty>(P);if(!O) { return W.Fail(TEXT("reference_property_changed"),TEXT("Reference property changed during load")); }
            if(!Live(Out.Reference.Get())) { return W.Fail(TEXT("reference_load_failed"),(C?FString::Printf(TEXT("Could not load class for property %s from path: %s"),*W.PropertyName,*Path):FString::Printf(TEXT("Could not load object for property %s from path: %s"),*W.PropertyName,*Path))); }
            if(C)
            {
                UClass* Loaded=Cast<UClass>(Out.Reference.Get());if(!Loaded) { return W.Fail(TEXT("reference_load_failed"),FString::Printf(TEXT("Could not load class for property %s from path: %s"),*W.PropertyName,*Path)); }
                if(C->MetaClass&&!Loaded->IsChildOf(C->MetaClass)) { return W.Fail(TEXT("reference_type_mismatch"),FString::Printf(TEXT("Class %s is not a child of required metaclass %s for property %s"),*Loaded->GetPathName(),*C->MetaClass->GetPathName(),*W.PropertyName)); }
            }
            else if(O->PropertyClass&&!Out.Reference->IsA(O->PropertyClass)) { return W.Fail(TEXT("reference_type_mismatch"),FString::Printf(TEXT("Object %s is not of required class %s for property %s"),*Out.Reference->GetPathName(),*O->PropertyClass->GetPathName(),*W.PropertyName)); }
            if(!SafeReference(Out.Reference.Get())) { return W.Fail(TEXT("dependency_reference"),TEXT("Loaded reference or class ancestry depends on an MCP-only Editor implementation")); }W.Prefix->SetStringField(TEXT("resolved_reference_path"),Out.Reference->GetPathName());
        }
        if(Out.Reference.IsValid()) { W.Expected=MakeShared<FJsonValueString>(Out.Reference->GetPathName()); }else { W.Expected=MakeShared<FJsonValueNull>(); }
    }
    else if(auto* B=CastField<FByteProperty>(P))
    {
        Out.Kind=EKind::Byte;int64 N=0;UEnum* E=B->GetIntPropertyEnum();
        if(E) { if(!EnumValue(W,V,E,true,N)) { if(!W.Result.Error.IsEmpty()) { return false; }return W.Fail(TEXT("property_type_unsupported"),FString::Printf(TEXT("Unsupported property type: %s for property %s"),*P->GetClass()->GetName(),*W.PropertyName)); } }
        else if(!IntegerValue(W,V->AsNumber(),0,256,N)) { return false; }Out.Byte=static_cast<uint8>(N);W.Expected=MakeShared<FJsonValueNumber>(Out.Byte);
    }
    else if(auto* E=CastField<FEnumProperty>(P))
    {
        Out.Kind=EKind::Enum;if(!E->GetEnum()||!E->GetUnderlyingProperty()||!EnumValue(W,V,E->GetEnum(),false,Out.Enum)) { if(!W.Result.Error.IsEmpty()) { return false; }return W.Fail(TEXT("property_type_unsupported"),FString::Printf(TEXT("Unsupported property type: %s for property %s"),*P->GetClass()->GetName(),*W.PropertyName)); }
        // SetIntPropertyValue truncates to the actual underlying type. Compute expected via an isolated, initialized property value.
        FNumericProperty* N=E->GetUnderlyingProperty();TArray<uint8> Scratch;Scratch.SetNumZeroed(N->GetSize()+N->GetMinAlignment());void* A=Align(Scratch.GetData(),N->GetMinAlignment());N->InitializeValue(A);N->SetIntPropertyValue(A,Out.Enum);W.Expected=MakeShared<FJsonValueString>(N->GetNumericPropertyValueToString(A));N->DestroyValue(A);
    }
    else { return W.Fail(TEXT("property_type_unsupported"),FString::Printf(TEXT("Unsupported property type: %s for property %s"),*P->GetClass()->GetName(),*W.PropertyName)); }
    W.Result.Data->SetField(TEXT("property_expected"),W.Expected);return true;
}
bool Apply(FWork& W,FPrepared& V)
{
    W.Result.Stage=TEXT("property_write_admission");FProperty* P=W.Property();if(!P) { return false; }const auto Current=W.Read(P);if(!Current.IsValid()||!JsonValue(Current).Equals(JsonValue(W.Before),ESearchCase::CaseSensitive)) { return W.Fail(TEXT("property_changed_before_write"),TEXT("Property changed while converting/loading the input; no field write was attempted")); }
    void* A=P->ContainerPtrToValuePtr<void>(W.Actor.Get());W.Write(TEXT("write_property"),TEXT("field_write_calls"));
    switch(V.Kind)
    {
    case EKind::Bool:CastFieldChecked<FBoolProperty>(P)->SetPropertyValue(A,V.Bool);break;
    case EKind::Int:CastFieldChecked<FIntProperty>(P)->SetPropertyValue_InContainer(W.Actor.Get(),V.Int);break;
    case EKind::Float:CastFieldChecked<FFloatProperty>(P)->SetPropertyValue(A,V.Float);break;
    case EKind::String:CastFieldChecked<FStrProperty>(P)->SetPropertyValue(A,V.String);break;
    case EKind::LinearColor:CastFieldChecked<FStructProperty>(P)->CopySingleValue(A,&V.Color);break;
    case EKind::Class:case EKind::Object:CastFieldChecked<FObjectPropertyBase>(P)->SetObjectPropertyValue(A,V.Reference.Get());break;
    case EKind::Byte:CastFieldChecked<FByteProperty>(P)->SetPropertyValue(A,V.Byte);break;
    case EKind::Enum:CastFieldChecked<FEnumProperty>(P)->GetUnderlyingProperty()->SetIntPropertyValue(A,V.Enum);break;
    }W.Count(TEXT("field_write_returns"));W.FieldWritten=true;W.Prefix->SetBoolField(TEXT("field_write_returned"),true);return true;
}
struct FView
{
    FLevelEditorViewportClient* Client=nullptr;FViewport* Viewport=nullptr;TStrongPtrVariant<FViewportClient> ClientPin;TSharedPtr<IAssetViewport> AssetOwner;TSharedPtr<SLevelViewport> SlateOwner;FString Source;
    FVector BeforeLocation=FVector::ZeroVector,ExpectedLocation=FVector::ZeroVector;FRotator BeforeRotation=FRotator::ZeroRotator,ExpectedRotation=FRotator::ZeroRotator;bool HasBefore=false;
    bool Admit(FWork& W)
    {
        if(!W.AdmitWorld()||!Client) { return false; }const bool Registered=GEditor->GetLevelViewportClients().Contains(Client);
        if(!Registered)
        {
            if(!SlateOwner.IsValid()||!FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))||&SlateOwner->GetLevelViewportClient()!=Client) { return W.Fail(TEXT("viewport_lifetime_changed"),TEXT("Captured level viewport client left its validated owner")); }
        }
        if(ClientPin.IsValid()&&ClientPin.Get()!=Client) { return W.Fail(TEXT("viewport_client_changed"),TEXT("Captured strong client identity changed")); }
        if(Client->Viewport!=Viewport||(Viewport&&Viewport->GetClient()!=Client)) { return W.Fail(TEXT("viewport_surface_changed"),TEXT("Captured viewport/client surface changed")); }
        W.Result.bChangeStateKnown=false;if(Client->GetWorld()!=W.World.Get()) { return W.Fail(TEXT("viewport_world_changed"),TEXT("Captured level viewport no longer observes the selected Editor world")); }return true;
    }
    bool Capture(FWork& W)
    {
        W.Result.Stage=TEXT("resolve_focus_viewport");if(!W.AdmitWorld()) { return false; }W.Count(TEXT("active_viewport_queries"));FViewport* Active=GEditor->GetActiveViewport();const auto Clients=GEditor->GetLevelViewportClients();
        if(Clients.Num()>MaxClients) { return W.Fail(TEXT("viewport_limit"),TEXT("Level viewport client count exceeds the bounded inspection limit")); }
        auto Current=[&]()->bool { if(GCurrentLevelEditingViewportClient&&Clients.Contains(GCurrentLevelEditingViewportClient)) { Client=GCurrentLevelEditingViewportClient;Source=TEXT("current_level_client");return true; }if(GCurrentLevelEditingViewportClient) { W.Prefix->SetBoolField(TEXT("unregistered_current_client_skipped"),true); }return false; };
        auto FirstRegistered=[&]()->bool { if(!W.AdmitWorld()) { return false; }const auto CurrentClients=GEditor->GetLevelViewportClients();if(CurrentClients.Num()>MaxClients) { W.Fail(TEXT("viewport_limit"),TEXT("Level viewport client count exceeds the bounded inspection limit"));return false; }for(auto* C:CurrentClients) { if(C&&C->Viewport) { Client=C;Source=TEXT("first_registered_level_viewport");return true; } }return false; };
        if(!Active) { if(!Current()) { FirstRegistered(); } }
        else
        {
            for(auto* C:Clients) { if(C&&C->Viewport==Active) { Client=C;Source=TEXT("active_viewport_level_client_match");break; } }
            if(!Client) { Current(); }
            if(!Client&&FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
            {
                auto& Module=FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));AssetOwner=Module.GetFirstActiveViewport();
                if(AssetOwner.IsValid()) { FEditorViewportClient* Base=&AssetOwner->GetAssetViewportClient();for(auto* C:Clients) { if(C&&static_cast<FEditorViewportClient*>(C)==Base) { Client=C;Source=TEXT("first_active_asset_viewport_validated_level_client");break; } }if(!Client) { W.Prefix->SetBoolField(TEXT("untyped_asset_viewport_cast_skipped"),true); } }
                if(!Client) { const auto Editor=Module.GetFirstLevelEditor();if(Editor.IsValid()) { const auto Views=Editor->GetViewports();if(Views.Num()>MaxClients) { return W.Fail(TEXT("viewport_limit"),TEXT("Slate level viewport count exceeds the bounded inspection limit")); }for(const auto& View:Views) { if(View.IsValid()) { SlateOwner=View;Client=&View->GetLevelViewportClient();Source=TEXT("first_typed_slate_level_viewport");break; } } } }
            }
            if(!Client) { FirstRegistered(); }
        }
        if(!Client) { return W.Fail(TEXT("viewport_missing"),TEXT("Failed to get active viewport")); }if(!W.AdmitWorld()) { return false; }
        if(!GEditor->GetLevelViewportClients().Contains(Client)&&(!SlateOwner.IsValid()||!FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))||&SlateOwner->GetLevelViewportClient()!=Client)) { return W.Fail(TEXT("viewport_lifetime_changed"),TEXT("Selected level client left its owner before its viewport could be captured")); }
        Viewport=Client->Viewport;if(Viewport) { ClientPin=Viewport->GetClientPtr(); }
        W.Prefix->SetStringField(TEXT("viewport_selection_source"),Source);W.Prefix->SetBoolField(TEXT("client_strong_handle_present"),ClientPin.IsValid());W.Prefix->SetBoolField(TEXT("viewport_present"),Viewport!=nullptr);if(!Admit(W)) { return false; }
        BeforeLocation=Client->GetViewLocation();BeforeRotation=Client->GetViewRotation();if(!Finite(BeforeLocation)||!Finite(BeforeRotation)) { return W.Fail(TEXT("viewport_before_nonfinite"),TEXT("Captured viewport transform is nonfinite")); }HasBefore=true;W.Prefix->SetArrayField(TEXT("view_location_before"),VectorJson(BeforeLocation));W.Prefix->SetArrayField(TEXT("view_rotation_before"),RotationJson(BeforeRotation));return true;
    }
    bool Observe(FWork& W,const TCHAR* Stage)
    {
        W.Result.Stage=Stage;if(!Admit(W)) { return false; }const FVector L=Client->GetViewLocation();const FRotator R=Client->GetViewRotation();if(!Finite(L)||!Finite(R)) { return W.Fail(TEXT("viewport_after_nonfinite"),TEXT("Captured viewport transform became nonfinite")); }
        auto J=MakeShared<FJsonObject>();J->SetArrayField(TEXT("location"),VectorJson(L));J->SetArrayField(TEXT("rotation"),RotationJson(R));W.Prefix->SetObjectField(Stage,J);if(HasBefore&&(L!=BeforeLocation||R!=BeforeRotation)) { W.Result.bChanged=true; }return true;
    }
};
bool LegacyVector(FWork& W,const TCHAR* Field,FVector& V)
{
    V=FVector::ZeroVector;const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(W.Params->TryGetArrayField(Field,A)&&A&&A->Num()>=3)
    { float F[3];for(int32 I=0;I<3;++I) { if(!(*A)[I].IsValid()) { return W.Fail(TEXT("vector_value_invalid"),TEXT("Vector JSON value is unavailable")); }if(!FloatValue(W,(*A)[I]->AsNumber(),F[I])) { return false; } }V=FVector(F[0],F[1],F[2]); }return true;
}
} // namespace

Variable::FResult SetProperty(const FString& ParamsJson)
{
    FWork W(TEXT("set_actor_property"));if(!W.Request(ParamsJson)) { return W.Finish(); }
    FString Name;if(!W.Params->TryGetStringField(TEXT("name"),Name)) { W.Fail(TEXT("name_missing"),TEXT("Missing 'name' parameter"));return W.Finish(); }
    if(!W.Enter()) { return W.Finish(); }TGuardValue<bool> RunningGuard(Running,true);if(!W.FindActor(Name)) { return W.Finish(); }
    if(!W.Params->TryGetStringField(TEXT("property_name"),W.PropertyName)) { W.Fail(TEXT("property_name_missing"),TEXT("Missing 'property_name' parameter"));return W.Finish(); }if(!SafeText(W.PropertyName)) { W.Fail(TEXT("property_name_invalid"),TEXT("Property name exceeds the text bound or contains a NUL"));return W.Finish(); }
    if(!W.Params->HasField(TEXT("property_value"))) { W.Fail(TEXT("property_value_missing"),TEXT("Missing 'property_value' parameter"));return W.Finish(); }const auto Value=W.Params->Values.FindRef(TEXT("property_value"));if(!Value.IsValid()) { W.Fail(TEXT("property_value_invalid"),TEXT("Invalid JSON property value"));return W.Finish(); }
    W.Result.Stage=TEXT("find_property");if(!W.AdmitActor()) { return W.Finish(); }W.OriginalProperty=W.Actor->GetClass()->FindPropertyByName(*W.PropertyName);if(!W.OriginalProperty) { W.Fail(TEXT("property_not_found"),FString::Printf(TEXT("Property not found: %s"),*W.PropertyName));return W.Finish(); }
    W.Prefix->SetStringField(TEXT("property_class"),W.OriginalProperty->GetClass()->GetName());W.Prefix->SetNumberField(TEXT("property_array_index"),0);W.Before=W.Read(W.OriginalProperty);W.PropertyCaptured=true;if(W.Before.IsValid()) { W.Result.Data->SetField(TEXT("property_before"),W.Before); }
    FPrepared Prepared;if(!Prepare(W,W.OriginalProperty,Value,Prepared)) { return W.Finish(); }if(!W.Before.IsValid()) { W.Fail(TEXT("property_before_unavailable"),TEXT("Supported property has no finite/live before projection; no field write was attempted"));return W.Finish(); }
    if(!Apply(W,Prepared)) { return W.Finish(); }W.Result.Stage=TEXT("actor_details");if(!W.AdmitActor()) { return W.Finish(); }W.Count(TEXT("actor_details_calls"));
    auto Details=MakeShared<FJsonObject>();Details->SetStringField(TEXT("name"),W.Actor->GetName());Details->SetStringField(TEXT("class"),W.Actor->GetClass()->GetName());const FVector Location=W.Actor->GetActorLocation(),Scale=W.Actor->GetActorScale3D();const FRotator Rotation=W.Actor->GetActorRotation();
    if(!Finite(Location)||!Finite(Scale)||!Finite(Rotation)) { W.Fail(TEXT("actor_details_nonfinite"),TEXT("Property write returned but actor detail transform is nonfinite"));return W.Finish(); }Details->SetArrayField(TEXT("location"),VectorJson(Location));Details->SetArrayField(TEXT("rotation"),RotationJson(Rotation));Details->SetArrayField(TEXT("scale"),VectorJson(Scale));W.Count(TEXT("actor_details_returns"));W.Legacy->SetStringField(TEXT("actor"),Name);W.Legacy->SetStringField(TEXT("property"),W.PropertyName);W.Legacy->SetBoolField(TEXT("success"),true);W.Legacy->SetObjectField(TEXT("actor_details"),Details);return W.Finish();
}

Variable::FResult Focus(const FString& ParamsJson)
{
    FWork W(TEXT("focus_viewport"));if(!W.Request(ParamsJson)||!W.Enter()) { return W.Finish(); }TGuardValue<bool> RunningGuard(Running,true);
    FString Target;const bool HasTarget=W.Params->TryGetStringField(TEXT("target"),Target),HasLocation=W.Params->HasField(TEXT("location")),HasRotation=W.Params->HasField(TEXT("orientation"));FVector Location,Angles;
    if(!LegacyVector(W,TEXT("location"),Location)||!LegacyVector(W,TEXT("orientation"),Angles)) { return W.Finish(); }float Distance=1000;if(W.Params->HasField(TEXT("distance"))&&!FloatValue(W,W.Params->GetNumberField(TEXT("distance")),Distance)) { return W.Finish(); }
    FView View;if(!View.Capture(W)) { return W.Finish(); }if(HasTarget) { if(!W.FindActor(Target)||!W.AdmitActor()) { return W.Finish(); }Location=W.Actor->GetActorLocation(); }
    else if(!HasLocation) { W.Fail(TEXT("target_missing"),TEXT("Either 'target' or 'location' must be provided"));return W.Finish(); }
    View.ExpectedLocation=Location-FVector(Distance,0,0);View.ExpectedRotation=HasRotation?FRotator(Angles.X,Angles.Y,Angles.Z):View.BeforeRotation;
    if(!Finite(View.ExpectedLocation)||!Finite(View.ExpectedRotation)) { W.Fail(TEXT("focus_transform_nonfinite"),TEXT("Focus transform must remain finite after original float32 coercion and -X offset"));return W.Finish(); }
    W.Result.Data->SetArrayField(TEXT("view_location_expected"),VectorJson(View.ExpectedLocation));W.Result.Data->SetArrayField(TEXT("view_rotation_expected"),RotationJson(View.ExpectedRotation));W.Result.Data->SetBoolField(TEXT("orientation_supplied"),HasRotation);W.Result.Data->SetNumberField(TEXT("effective_distance_float32"),Distance);
    W.Result.Stage=TEXT("set_view_location");if(!View.Admit(W)) { return W.Finish(); }W.Write(TEXT("set_view_location"),TEXT("set_view_location_calls"));View.Client->SetViewLocation(View.ExpectedLocation);W.Count(TEXT("set_view_location_returns"));if(!View.Observe(W,TEXT("view_after_location"))) { return W.Finish(); }
    if(HasRotation) { W.Result.Stage=TEXT("set_view_rotation");if(!View.Admit(W)) { return W.Finish(); }W.Write(TEXT("set_view_rotation"),TEXT("set_view_rotation_calls"));View.Client->SetViewRotation(View.ExpectedRotation);W.Count(TEXT("set_view_rotation_returns"));if(!View.Observe(W,TEXT("view_after_rotation"))) { return W.Finish(); } }
    W.Result.Stage=TEXT("invalidate_viewport");if(!View.Admit(W)) { return W.Finish(); }W.Write(TEXT("invalidate_viewport"),TEXT("client_invalidate_calls"));View.Client->Invalidate();W.Count(TEXT("client_invalidate_returns"));if(!View.Observe(W,TEXT("view_after_invalidate"))) { return W.Finish(); }
    const FVector ActualLocation=View.Client->GetViewLocation();const FRotator ActualRotation=View.Client->GetViewRotation();const bool LocationMatches=ActualLocation.Equals(View.ExpectedLocation,1e-6),RotationMatches=FMath::Abs(ActualRotation.Pitch-View.ExpectedRotation.Pitch)<=1e-6&&FMath::Abs(ActualRotation.Yaw-View.ExpectedRotation.Yaw)<=1e-6&&FMath::Abs(ActualRotation.Roll-View.ExpectedRotation.Roll)<=1e-6;
    W.Checks->SetBoolField(TEXT("location_matches_expected"),LocationMatches);W.Checks->SetBoolField(TEXT("rotation_matches_expected_or_preserved"),RotationMatches);W.Checks->SetBoolField(TEXT("render_completion_not_claimed"),true);if(!LocationMatches||!RotationMatches) { W.Fail(TEXT("focus_readback_mismatch"),TEXT("Actual viewport transform differs from the original -X focus or optional rotation contract"));return W.Finish(); }W.Legacy->SetBoolField(TEXT("success"),true);return W.Finish();
}
} // namespace Cubeless::UE58::ActorPropertyFocus
