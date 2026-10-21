// CZ original typed Select(CallFunction)7 and actual SwitchEnum8; no private Select-node rewrite.
#include "Compatibility/UE58/CubelessBlueprintSelectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/UserDefinedEnum.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
#include "K2Node_SwitchEnum.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <cfloat>

namespace Cubeless::UE58::BlueprintSelection
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("shared_add_call_calls"),TEXT("shared_add_call_returns"),TEXT("enum_load_calls"),TEXT("enum_load_returns"),
        TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("node_allocation_calls"),TEXT("set_enum_calls"),TEXT("graph_add_node_calls"),
        TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("destroy_node_calls"),TEXT("destroy_node_returns"),
        TEXT("mark_blueprint_modified_calls"),TEXT("legacy_node_metadata_observation_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("position_scope"),TEXT("Original null/default origin and finite first-two float32 coordinates projected by checked int32 truncation; bounded legacy string/ignored shape fallback. No unbounded float-to-int32 conversion."));
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Existing shared graph resolution/capture/call creator and public Engine node APIs. Original no explicit compile/save; node CDO/cache/default object, graph signals and MarkModified skeleton/cache effects are implicit, not zero or full runtime/cook proof."));
    R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
}
FString NormalizeType(FString Type)
{ Type=Type.ToLower();Type.TrimStartAndEndInline();Type.ReplaceInline(TEXT(" "),TEXT(""));Type.ReplaceInline(TEXT("_"),TEXT(""));Type.ReplaceInline(TEXT("-"),TEXT(""));return Type; }
FName SelectFunction(const FString& Type)
{
    const FString T=NormalizeType(Type);
    if(T==TEXT("string")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectString); }
    if(T==TEXT("text")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectText); }
    if(T==TEXT("name")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectName); }
    if(T==TEXT("int")||T==TEXT("integer")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectInt); }
    if(T==TEXT("float")||T==TEXT("double")||T==TEXT("real")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectFloat); }
    if(T==TEXT("vector")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectVector); }
    if(T==TEXT("rotator")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectRotator); }
    if(T==TEXT("color")||T==TEXT("linearcolor")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectColor); }
    if(T==TEXT("transform")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectTransform); }
    if(T==TEXT("object")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectObject); }
    if(T==TEXT("class")) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,SelectClass); }return NAME_None;
}
bool Position(const FString& Json,int32 (&XY)[2],FString& Projected,FResult& R)
{
    TSharedPtr<FJsonValue> V;if(!Function::Pin::ParseJson(Json,V,R)) { return false; }
    double Values[2]={0,0};
    if(V->Type==EJson::Array&&V->AsArray().Num()>=2)
    { for(int32 I=0;I<2;++I) { V->AsArray()[I]->TryGetNumber(Values[I]); } }
    else if(V->Type==EJson::String)
    {
        FString S=V->AsString();S.TrimStartAndEndInline();S.RemoveFromStart(TEXT("["));S.RemoveFromEnd(TEXT("]"));S.RemoveFromStart(TEXT("("));S.RemoveFromEnd(TEXT(")"));
        TArray<FString> Parts;S.ParseIntoArray(Parts,TEXT(","),true);if(Parts.Num()>=2) { Values[0]=FCString::Atof(*Parts[0].TrimStartAndEnd());Values[1]=FCString::Atof(*Parts[1].TrimStartAndEnd()); }
    }
    TArray<TSharedPtr<FJsonValue>> Original,FloatRows,IntRows;
    for(int32 I=0;I<2;++I)
    {
        if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>FLT_MAX) { return Fail(R,TEXT("selection_position_invalid"),TEXT("Finite float32 position required before graph writer.")); }
        const float F=static_cast<float>(Values[I]);const double D=static_cast<double>(F);
        if(!FMath::IsFinite(F)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)) { return Fail(R,TEXT("selection_position_invalid"),TEXT("Original float32 position exceeds safe int32 assignment budget.")); }
        XY[I]=static_cast<int32>(F);Original.Add(MakeShared<FJsonValueNumber>(Values[I]));FloatRows.Add(MakeShared<FJsonValueNumber>(D));IntRows.Add(MakeShared<FJsonValueNumber>(XY[I]));
    }
    R.Data->SetField(TEXT("requested_position"),V);R.Data->SetArrayField(TEXT("position_numeric_prefix"),Original);R.Data->SetArrayField(TEXT("position_float32"),FloatRows);R.Data->SetArrayField(TEXT("position_int32"),IntRows);
    Projected=FString::Printf(TEXT("[%d,%d]"),XY[0],XY[1]);return true;
}
TSharedRef<FJsonObject> ResultJson(const FResult& R)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),R.bSuccess);J->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);
    if(R.bChangeStateKnown) { J->SetBoolField(TEXT("changed"),R.bChanged); }else { J->SetField(TEXT("changed"),MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("postcondition_passed"),R.bPostconditionPassed);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetObjectField(TEXT("data"),R.Data);return J;
}
bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
TSharedRef<FJsonObject> EnumSnapshot(UEnum* Enum)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Enum->GetPathName());J->SetStringField(TEXT("class"),Enum->GetClass()->GetPathName());J->SetBoolField(TEXT("package_dirty"),Enum->GetOutermost()->IsDirty());J->SetNumberField(TEXT("object_flags"),static_cast<uint32>(Enum->GetFlags()));J->SetNumberField(TEXT("num_enums"),Enum->NumEnums());
    TArray<TSharedPtr<FJsonValue>> Entries;
    for(int32 I=0;I<Enum->NumEnums();++I)
    { auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),Enum->GetNameStringByIndex(I));V->SetNumberField(TEXT("value"),Enum->GetValueByIndex(I));V->SetStringField(TEXT("value_decimal"),LexToString(Enum->GetValueByIndex(I)));V->SetStringField(TEXT("display_name"),Enum->GetDisplayNameTextByIndex(I).ToString());V->SetBoolField(TEXT("hidden"),Enum->HasMetaData(TEXT("Hidden"),I));V->SetBoolField(TEXT("spacer"),Enum->HasMetaData(TEXT("Spacer"),I));Entries.Add(MakeShared<FJsonValueObject>(V)); }
    J->SetArrayField(TEXT("entries"),Entries);return J;
}
// IH: source UserDefinedEnum assets are consumed read-only. This concrete local
// scope is intentionally TU-local; native enum and other node routes are unchanged.
FString EnumAssetText(const TSharedRef<FJsonObject>& J)
{ FString S;auto W=TJsonWriterFactory<>::Create(&S);FJsonSerializer::Serialize(J,W);return S; }
bool EnumAssetGlobals()
{ return IsEditorGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage(); }
bool EnumAssetDisk(UPackage* P,TSharedRef<FJsonObject>& J)
{
    const FString Package=P->GetName();FString File;
    if(!FPackageName::TryConvertLongPackageNameToFilename(Package,File,FPackageName::GetAssetPackageExtension())) { return false; }
    const int64 Size=IFileManager::Get().FileSize(*File);J->SetBoolField(TEXT("exists"),Size>=0);J->SetStringField(TEXT("file"),File);
    if(Size<0) { return true; }if(Size>16*1024*1024) { return false; }
    TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size) { return false; }
    uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);J->SetNumberField(TEXT("bytes"),Size);J->SetStringField(TEXT("sha1"),BytesToHex(Digest,FSHA1::DigestSize));return true;
}
struct FEnumAssetScope
{
    bool Active=false,Attached=false,GuidFixed=false,EnumAssigned=false;
    TStrongObjectPtr<UEnum> Source;TStrongObjectPtr<UPackage> Package;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    UObject* SourceOuter=nullptr;UClass* SourceClass=nullptr;FString SourcePath;EObjectFlags SourceFlags=RF_NoFlags;bool SourceDirty=false;
    TSharedPtr<FJsonObject> Before,PublicBefore,PublicCurrent;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEdGraphNode> Node;TStrongObjectPtr<UPackage> TargetPackageOwner;
    UObject *BlueprintOuter=nullptr,*GraphOuter=nullptr;UClass *BlueprintClass=nullptr,*GraphClass=nullptr,*NodeClass=nullptr;
    FString BlueprintPath,GraphPath,NodePath;UPackage* TargetPackage=nullptr;FGuid GraphGuid,NodeGuid;
    bool Bad(FResult& R,const TCHAR* Code,const TCHAR* Why)
    { if(!R.ErrorCode.IsEmpty()&&R.ErrorCode!=Code) { R.Data->SetStringField(TEXT("enum_asset_preceding_error_code"),R.ErrorCode);R.Data->SetStringField(TEXT("enum_asset_preceding_error"),R.Error); }R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("enum_asset_observation_complete"),false);return Fail(R,Code,Why); }
    bool Context() const
    { return EnumAssetGlobals()&&GEditor==Editor.Get()&&!IsPlaySessionActive()&&World.IsValid()&&GEditor->GetEditorWorldContext().World()==World.Get()&&!World->bIsTearingDown; }
    bool Identity(FResult& R)
    {
        if(!Active) { return true; }
        if(!Context()) { return Bad(R,TEXT("enum_asset_context_changed"),TEXT("Captured Editor/world or GT/PIE/GC/exit/save admission changed; no next node SDK or cleanup.")); }
        UEnum* E=Source.Get();
        if(!Loaded(E)||!Loaded(Package.Get())||!Loaded(SourceClass)||!Loaded(SourceOuter)||E->GetClass()!=SourceClass||E->GetOuter()!=SourceOuter||E->GetPackage()!=Package.Get()||E->GetPathName()!=SourcePath||E->GetFlags()!=SourceFlags||Package->IsDirty()!=SourceDirty)
        { return Bad(R,TEXT("enum_asset_identity_changed"),TEXT("Read-only UserDefinedEnum identity/package/dirty/flags changed; no source replacement or retry.")); }
        return true;
    }
    bool Target(FResult& R)
    {
        if(!Active||!Blueprint.IsValid()) { return true; }
        UBlueprint* B=Blueprint.Get();UEdGraph* G=Graph.Get();
        if(!Context()||!Loaded(B)||!Loaded(G)||!Loaded(TargetPackage)||!Loaded(BlueprintOuter)||!Loaded(GraphOuter)
            ||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPackage()!=TargetPackage||B->GetPathName()!=BlueprintPath
            ||G->GetClass()!=GraphClass||G->GetOuter()!=GraphOuter||G->GetPackage()!=TargetPackage||G->GetPathName()!=GraphPath||G->GraphGuid!=GraphGuid
            ||B->ParentClass!=AActor::StaticClass()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=B)
        { return Bad(R,TEXT("enum_asset_target_role_changed"),TEXT("Selected Blueprint/physical graph identity or routing changed across a callback; no stale target SDK.")); }
        if(Node.IsValid())
        {
            auto* N=Node.Get();int32 MembershipCount=0;for(const auto& It:G->Nodes) { if(It.Get()==N) { ++MembershipCount; } }
            if(!Loaded(N)||N->GetClass()!=NodeClass||N->GetOuter()!=G||N->GetPackage()!=TargetPackage||N->GetPathName()!=NodePath||MembershipCount!=(Attached?1:0)||(GuidFixed&&N->NodeGuid!=NodeGuid))
            { return Bad(R,TEXT("enum_asset_node_role_changed"),TEXT("Created node identity or planned graph membership changed; no stale node/pin observation or cleanup.")); }
        }
        if(Node.IsValid()&&EnumAssigned)
        {
            const auto* P=FindFProperty<FObjectPropertyBase>(NodeClass,TEXT("Enum"));
            if(!P||P->ArrayDim!=1||P->GetObjectPropertyValue_InContainer(Node.Get())!=Source.Get())
            { return Bad(R,TEXT("enum_asset_node_enum_role_changed"),TEXT("Created node no longer references the exact read-only enum source; no foreign enum is read or adopted.")); }
        }
        return true;
    }
    bool Snapshot(FResult& R,TSharedRef<FJsonObject>& J)
    {
        if(!Identity(R)) { return false; }auto* E=Source.Get();auto Pub=MakeShared<FJsonObject>();J->SetObjectField(TEXT("public_enum"),Pub);
        Pub->SetStringField(TEXT("path"),E->GetPathName());Pub->SetStringField(TEXT("class"),E->GetClass()->GetPathName());Pub->SetBoolField(TEXT("package_dirty"),Package->IsDirty());Pub->SetNumberField(TEXT("object_flags"),static_cast<uint32>(E->GetFlags()));Pub->SetNumberField(TEXT("num_enums"),E->NumEnums());
        if(E->NumEnums()<2||E->NumEnums()>257) { return Bad(R,TEXT("enum_asset_entries_changed"),TEXT("Bounded UserDefinedEnum entry count changed or became invalid.")); }
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(int32 I=0;I<E->NumEnums();++I)
        {
            auto Row=MakeShared<FJsonObject>();const FString Name=E->GetNameStringByIndex(I);const int64 Value=E->GetValueByIndex(I);const FString Display=E->GetDisplayNameTextByIndex(I).ToString();
            if(!Identity(R)) { return false; }if(Name.Len()>512||Display.Len()>4096) { return Bad(R,TEXT("enum_asset_text_bound"),TEXT("Enum names/display values exceed the bounded source observation.")); }
            Row->SetStringField(TEXT("name"),Name);Row->SetNumberField(TEXT("value"),Value);Row->SetStringField(TEXT("value_decimal"),LexToString(Value));Row->SetStringField(TEXT("display_name"),Display);
            Row->SetBoolField(TEXT("hidden"),E->HasMetaData(TEXT("Hidden"),I));Row->SetBoolField(TEXT("spacer"),E->HasMetaData(TEXT("Spacer"),I));if(!Identity(R)) { return false; }Entries.Add(MakeShared<FJsonValueObject>(Row));
        }
        Pub->SetArrayField(TEXT("entries"),Entries);J->SetNumberField(TEXT("cpp_form"),static_cast<int32>(E->GetCppForm()));J->SetNumberField(TEXT("underlying_type"),static_cast<int32>(E->GetUnderlyingType()));
        auto Stored=MakeShared<FJsonObject>();J->SetObjectField(TEXT("stored_UserDefinedEnum_fields"),Stored);
        for(const FName Field:{FName(TEXT("UniqueNameIndex")),FName(TEXT("EnumDescription")),FName(TEXT("DisplayNameMap"))})
        {
            auto* P=FindFProperty<FProperty>(E->GetClass(),Field);FString Value;if(!P||P->ArrayDim!=1||!P->ExportText_Direct(Value,P->ContainerPtrToValuePtr<void>(E),P->ContainerPtrToValuePtr<void>(E),E,PPF_None)||!Identity(R)) { return false; }
            if(Value.Len()>262144) { return Bad(R,TEXT("enum_asset_property_bound"),TEXT("Stored UserDefinedEnum field exceeds the bounded source observation.")); }Stored->SetStringField(Field.ToString(),Value);
        }
        // Public metadata read may preload deprecated package metadata internally;
        // guard its return before reading the map. No metadata setter is called.
        const TMap<FName,FString>* Meta=FMetaData::GetMapForObject(E);if(!Identity(R)) { return false; }
        auto Metadata=MakeShared<FJsonObject>();J->SetObjectField(TEXT("metadata"),Metadata);
        if(Meta)
        {
            if(Meta->Num()>2048) { return Bad(R,TEXT("enum_asset_metadata_bound"),TEXT("Enum metadata exceeds the finite bound.")); }
            for(const auto& Pair:*Meta) { if(Pair.Key.ToString().Len()>512||Pair.Value.Len()>16384) { return Bad(R,TEXT("enum_asset_metadata_bound"),TEXT("Enum metadata text exceeds the finite bound.")); }Metadata->SetStringField(Pair.Key.ToString(),Pair.Value); }
        }
        auto D=MakeShared<FJsonObject>();J->SetObjectField(TEXT("disk"),D);if(!EnumAssetDisk(Package.Get(),D)||!Identity(R)) { return false; }
        if(EnumAssetText(J).Len()>1048576) { return Bad(R,TEXT("enum_asset_observation_bound"),TEXT("Complete enum source snapshot exceeds1Mi UTF16 characters; no incomplete snapshot is certified.")); }
        return true;
    }
    bool Capture(UEnum* E,FResult& R)
    {
        if(E->GetClass()==UEnum::StaticClass()) { return true; }
        if(E->GetClass()!=UUserDefinedEnum::StaticClass()) { return true; }Active=true;
        if(!EnumAssetGlobals()||IsPlaySessionActive()) { return Bad(R,TEXT("enum_asset_editor_required"),TEXT("Safe Editor context required for a UserDefinedEnum source.")); }
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();Source.Reset(E);Package.Reset(E->GetPackage());SourceOuter=E->GetOuter();SourceClass=E->GetClass();SourcePath=E->GetPathName();SourceFlags=E->GetFlags();SourceDirty=Package->IsDirty();
        auto J=MakeShared<FJsonObject>();if(!Snapshot(R,J)) { if(R.Error.IsEmpty()) { Bad(R,TEXT("enum_asset_observation_incomplete"),TEXT("Complete stored enum source observation is unavailable; no target writer dispatched.")); }return false; }Before=J;PublicBefore=J->GetObjectField(TEXT("public_enum"));PublicCurrent=PublicBefore;
        const FString Encoded=EnumAssetText(J);FTCHARToUTF8 Utf8(*Encoded);uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Utf8.Get(),Utf8.Length(),Digest);
        R.Data->SetBoolField(TEXT("enum_asset_read_only"),true);R.Data->SetStringField(TEXT("enum_asset_authoring_sha1"),BytesToHex(Digest,FSHA1::DigestSize));R.Data->SetNumberField(TEXT("enum_asset_snapshot_utf16_chars"),Encoded.Len());R.Data->SetBoolField(TEXT("enum_asset_observation_complete"),true);R.Data->SetBoolField(TEXT("enum_asset_global_getter_purity_unverified"),true);return true;
    }
    bool Check(FResult& R)
    {
        if(!Active) { return true; }if(!Identity(R)) { return false; }auto J=MakeShared<FJsonObject>();
        if(!Snapshot(R,J)) { if(R.Error.IsEmpty()) { Bad(R,TEXT("enum_asset_observation_incomplete"),TEXT("Current stored enum source observation is incomplete; no next writer/cleanup/retry.")); }return false; }
        if(!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(J))) { return Bad(R,TEXT("enum_asset_source_changed"),TEXT("Read-only enum entries/stored fields/metadata/disk differ; no next writer/cleanup/retry.")); }
        PublicCurrent=J->GetObjectField(TEXT("public_enum"));return Target(R);
    }
    bool SetTarget(UBlueprint* B,UEdGraph* G,FResult& R)
    {
        if(!Active) { return true; }if(!Check(R)||!Loaded(B)||!Loaded(G)||B->GetPackage()==Package.Get()) { return Bad(R,TEXT("enum_asset_target_unsupported"),TEXT("Current target in a distinct Blueprint package required for read-only source protection.")); }
        Blueprint.Reset(B);Graph.Reset(G);BlueprintOuter=B->GetOuter();GraphOuter=G->GetOuter();BlueprintClass=B->GetClass();GraphClass=G->GetClass();BlueprintPath=B->GetPathName();GraphPath=G->GetPathName();TargetPackage=B->GetPackage();TargetPackageOwner.Reset(TargetPackage);GraphGuid=G->GraphGuid;return Check(R);
    }
    bool SetNode(UEdGraphNode* N,FResult& R)
    {
        if(!Active) { return true; }if(!Identity(R)||!Target(R)||!Loaded(N)||N->GetOuter()!=Graph.Get()) { return Bad(R,TEXT("enum_asset_node_unavailable"),TEXT("New node return cannot be adopted in current owned graph.")); }
        Node.Reset(N);NodeClass=N->GetClass();NodePath=N->GetPathName();return Check(R);
    }
};
bool AssetEnumNodeFields(UEdGraphNode* N,FEnumAssetScope& Scope,FResult& R)
{
    if(!Scope.Check(R)) { return false; }Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),N->GetName());R.Data->SetStringField(TEXT("class"),N->GetClass()->GetName());
    const FString Title=N->GetNodeTitle(ENodeTitleType::FullTitle).ToString();if(!Scope.Check(R)) { return false; }
    R.Data->SetStringField(TEXT("title"),Title);R.Data->SetStringField(TEXT("node_comment"),N->NodeComment);R.Data->SetBoolField(TEXT("comment_bubble_visible"),N->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),N->bCommentBubblePinned);return true;
}
UEnum* LoadEnumAsset(const FString& Path,FResult& R)
{
    if(!EnumAssetGlobals()||IsPlaySessionActive()) { Fail(R,TEXT("selection_enum_asset_context_required"),TEXT("Safe Editor context required before loading a read-only enum source."));return nullptr; }
    TWeakObjectPtr<UEditorEngine> Editor=GEditor;TWeakObjectPtr<UWorld> World=GEditor->GetEditorWorldContext().World();
    if(!World.IsValid()||World->bIsTearingDown) { Fail(R,TEXT("selection_enum_asset_context_required"),TEXT("Current Editor world required."));return nullptr; }
    Count(R,TEXT("enum_load_calls"));UEnum* E=LoadObject<UEnum>(nullptr,*Path);Count(R,TEXT("enum_load_returns"));R.Data->SetBoolField(TEXT("enum_asset_load_effects_unverified"),true);
    if(!EnumAssetGlobals()||GEditor!=Editor.Get()||IsPlaySessionActive()||!World.IsValid()||GEditor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown)
    { R.bChangeStateKnown=false;Fail(R,TEXT("selection_enum_asset_context_changed"),TEXT("Enum load returned after context change; returned UObject was not observed/adopted."));return nullptr; }
    if(!Loaded(E)||E->GetClass()!=UUserDefinedEnum::StaticClass()||E->GetPathName()!=Path||E->GetOuter()!=E->GetPackage()||E->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||!E->HasAnyFlags(RF_Public)||E->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly)||E->NumEnums()<2||E->NumEnums()>257)
    { Fail(R,TEXT("selection_enum_asset_unsupported"),TEXT("Exact current runtime UserDefinedEnum asset with bounded byte entries required; custom/redirected/transient variants remain unsupported."));return nullptr; }
    for(int32 I=0;I<E->NumEnums()-1;++I) { if(E->GetValueByIndex(I)<0||E->GetValueByIndex(I)>255||E->GetNameStringByIndex(I).Len()>512) { Fail(R,TEXT("selection_enum_byte_scope_pending"),TEXT("Enum asset entries must retain the original byte-pin scope."));return nullptr; } }
    return E;
}
bool AssetSwitchPins(UEdGraphNode* N,const FGuid& SelectionId,const FGuid& DefaultId,bool HasDefault,UEdGraphPin*& Selection,UEdGraphPin*& Default,FEnumAssetScope& Scope,FResult& R)
{
    if(!Scope.Check(R)) { return false; }if(N->Pins.Num()>512) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Current created switch pin array exceeds its finite observation bound.")); }Selection=nullptr;Default=nullptr;TArray<FString> CurrentCases;TSet<FGuid> CurrentIds;
    if(!SelectionId.IsValid()||(HasDefault&&!DefaultId.IsValid())) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Original switch selection/default pin ID is invalid.")); }
    for(UEdGraphPin* P:N->Pins)
    {
        if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||CurrentIds.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Current switch owning pin array no longer has its original fresh physical roles.")); }CurrentIds.Add(P->PinId);
        if(P->PinName==FName(TEXT("Default"))&&P->Direction==EGPD_Output&&(!HasDefault||P->PinId!=DefaultId)) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Unexpected default pin appeared during a schema callback.")); }
        if(P->Direction==EGPD_Output&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&P->PinName!=FName(TEXT("Default"))) { CurrentCases.Add(P->PinName.ToString()); }
        if(P->PinId==SelectionId)
        {
            if(Selection||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinName!=FName(TEXT("Selection"))||P->Direction!=EGPD_Input||P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Byte||P->PinType.PinSubCategoryObject.Get()!=Scope.Source.Get()||P->bOrphanedPin)
            { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Current switch selection pin no longer has its exact original ID/owner/enum role.")); }Selection=P;
        }
        if(HasDefault&&P->PinId==DefaultId)
        {
            if(Default||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinName!=FName(TEXT("Default"))||P->Direction!=EGPD_Output||P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec||P->bOrphanedPin)
            { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Current switch default pin no longer has its exact original ID/owner/exec role.")); }Default=P;
        }
    }
    if(!Selection||(HasDefault&&!Default)) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Original switch pin role is absent after a schema callback.")); }
    const auto& OriginalCases=R.Data->GetArrayField(TEXT("actual_case_names"));if(CurrentCases.Num()!=OriginalCases.Num()) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Switch case count changed during a schema callback.")); }
    for(int32 I=0;I<CurrentCases.Num();++I) { if(CurrentCases[I]!=OriginalCases[I]->AsString()) { return Scope.Bad(R,TEXT("enum_asset_switch_pin_changed"),TEXT("Switch case order/name changed during a schema callback.")); } }return true;
}
UEnum* ResolveEnum(const FString& Input,FResult& R)
{
    if(!Function::Pin::SafeName(Input)) { Fail(R,TEXT("selection_enum_invalid"),TEXT("Bounded original enum_type required."));return nullptr; }
    const FString Reference=Input.TrimStartAndEnd().Replace(TEXT("\\"),TEXT("/")).ToLower();
    for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")})
    { if(Reference.Contains(Token)) { Fail(R,TEXT("selection_enum_dependency_forbidden"),TEXT("Original enum dependency guard refused an editor authoring bridge reference."));return nullptr; } }
    FString AssetPath=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();AssetPath.TrimQuotesInline();AssetPath.ReplaceInline(TEXT("\\"),TEXT("/"));
    if(AssetPath.StartsWith(TEXT("/Game/"))||AssetPath.StartsWith(TEXT("/Engine/")))
    {
        if(!AssetPath.Contains(TEXT("."))) { const FString Short=FPackageName::GetShortName(AssetPath);AssetPath+=TEXT(".")+Short; }
        if(AssetPath.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(AssetPath)) { Fail(R,TEXT("selection_enum_asset_path_invalid"),TEXT("Exact original runtime enum asset object path required."));return nullptr; }
        return LoadEnumAsset(AssetPath,R);
    }
    FString Path=Input;Path.TrimStartAndEndInline();Path.TrimQuotesInline();if(!Path.Contains(TEXT("/"))&&!Path.Contains(TEXT("."))) { Path=TEXT("/Script/Engine.")+Path; }
    if(!Path.StartsWith(TEXT("/Script/Engine."))&&!Path.StartsWith(TEXT("/Script/CoreUObject."))) { Fail(R,TEXT("selection_enum_scope_pending"),TEXT("Finite native Engine/CoreUObject enum selected; original asset/custom-module/slow-find variants remain valid residuals."));return nullptr; }
    Count(R,TEXT("enum_load_calls"));auto* Enum=LoadObject<UEnum>(nullptr,*Path);Count(R,TEXT("enum_load_returns"));
    // UHT deliberately marks native enums RF_Transient; this is not a transient user asset.
    if(!Loaded(Enum)||Enum->GetClass()!=UEnum::StaticClass()||Enum->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||Enum->NumEnums()<2||Enum->NumEnums()>257)
    { Fail(R,TEXT("selection_enum_not_found_or_scope"),TEXT("Loaded bounded native runtime enum with switchable entries required; no implicit editor/user-enum mutation."));return nullptr; }
    const FString ActualPackage=Enum->GetOutermost()->GetName();
    if(!ActualPackage.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!ActualPackage.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive))
    { Fail(R,TEXT("selection_enum_scope_pending"),TEXT("Resolved enum must be owned by runtime Engine/CoreUObject; redirected editor/custom enum variants remain incomplete."));return nullptr; }
    for(int32 I=0;I<Enum->NumEnums()-1;++I)
    { if(Enum->GetValueByIndex(I)<0||Enum->GetValueByIndex(I)>255||Enum->GetNameStringByIndex(I).Len()>512) { Fail(R,TEXT("selection_enum_byte_scope_pending"),TEXT("Current byte selection requires bounded unsigned byte enum cases; original wider/negative variants remain incomplete."));return nullptr; } }
    return Enum;
}
bool ExistingNodesPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After)
{
    const auto& A=Before->GetArrayField(TEXT("nodes"));const auto& B=After->GetArrayField(TEXT("nodes"));
    if(B.Num()!=A.Num()+1) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}
bool GraphRowsPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const FString& Selected)
{
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(A.Num()!=B.Num()) { return false; }int32 SelectedCount=0;
    for(int32 I=0;I<A.Num();++I)
    { const auto Expected=MakeShared<FJsonObject>(*A[I]->AsObject());if(Expected->GetStringField(TEXT("object_path")).Equals(Selected,ESearchCase::CaseSensitive))
      { ++SelectedCount;Expected->SetNumberField(TEXT("node_count"),Expected->GetNumberField(TEXT("node_count"))+1); }
      if(!Equal(MakeShared<FJsonValueObject>(Expected),B[I])) { return false; } }return SelectedCount==1;
}
void LegacyNodeFields(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());
    R.Data->SetStringField(TEXT("title"),Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);
    R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
}
}

Function::FResult AddSelect(const FString& Path,const FString& Type,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create)
{
    FResult Context;Init(Context);Context.Data->SetStringField(TEXT("value_type"),Type);const FName FunctionName=SelectFunction(Type);
    if(FunctionName.IsNone()) { Fail(Context,TEXT("selection_type_unsupported"),TEXT("Unsupported select type: ")+Type);return Context; }
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,Context)) { return Context; }
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(Context,TEXT("selection_editor_invalid"),TEXT("Idle Editor game thread required."));return Context; }
    Count(Context,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","AddTypedSelect","Add Typed Blueprint Select"));
    Count(Context,TEXT("shared_add_call_calls"));FResult R=Function::AddCall(Path,TEXT("/Script/Engine.KismetMathLibrary"),FunctionName.ToString(),TEXT("{}"),PositionText,Name,Id,GraphType,Create,false,false,false,false,false);Count(Context,TEXT("shared_add_call_returns"));
    for(const auto& P:Context.Data->Values) { R.Data->SetField(P.Key,P.Value); }
    R.Data->SetStringField(TEXT("normalized_value_type"),NormalizeType(Type));R.Data->SetStringField(TEXT("mapped_function_name"),FunctionName.ToString());R.Data->SetStringField(TEXT("legacy_node_kind"),TEXT("select"));R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_CallFunction"));
    R.Data->SetBoolField(TEXT("alternate_UK2Node_Select_used"),false);R.Data->SetBoolField(TEXT("shared_add_call_admission_authoritative"),true);
    FString NodePath;if(R.Data->TryGetStringField(TEXT("object_path"),NodePath)) { if(auto* Node=FindObject<UEdGraphNode>(nullptr,*NodePath);Loaded(Node)) { LegacyNodeFields(Node,R); } }
    return R;
}

Function::FResult AddSwitchEnum(const FString& Path,const FString& EnumType,bool HasDefault,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_enum_type"),EnumType);R.Data->SetBoolField(TEXT("has_default_pin"),HasDefault);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("selection_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }
    auto* Enum=ResolveEnum(EnumType,R);if(!Enum) { return R; }const TStrongObjectPtr<UEnum> KeepEnum(Enum);FEnumAssetScope Asset;if(!Asset.Capture(Enum,R)) { return R; }const auto EnumBefore=Asset.Active?Asset.PublicBefore.ToSharedRef():EnumSnapshot(Enum);R.Data->SetObjectField(TEXT("enum_before"),EnumBefore);
    TArray<TSharedPtr<FJsonValue>> Cases;for(int32 I=0;I<Enum->NumEnums()-1;++I)
    { if(Enum->HasMetaData(TEXT("Hidden"),I)||Enum->HasMetaData(TEXT("Spacer"),I)) { continue; }auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("name"),Enum->GetNameStringByIndex(I));Entry->SetNumberField(TEXT("value"),Enum->GetValueByIndex(I));Entry->SetStringField(TEXT("display_name"),Enum->GetDisplayNameTextByIndex(I).ToString());Cases.Add(MakeShared<FJsonValueObject>(Entry)); }
    R.Data->SetArrayField(TEXT("enum_entries"),Cases);if(!Asset.Check(R)) { return R; }if(Cases.IsEmpty()) { Fail(R,TEXT("selection_enum_no_cases"),TEXT("Enum has no switchable values."));return R; }
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,R)) { return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","AddSwitchEnum","Add Blueprint Switch Enum"));
    if(!Asset.Check(R)) { return R; }R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto Resolved=Function::Resolve(Path,Name,Id,GraphType,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(Resolved));
    R.bMutationAttempted=Resolved.bMutationAttempted;R.bChanged=Resolved.bChanged;R.bChangeStateKnown=Resolved.bChangeStateKnown;
    if(!Asset.Check(R)) { return R; }
    if(!Resolved.bSuccess||!Resolved.bPostconditionPassed) { Fail(R,Resolved.ErrorCode.IsEmpty()?TEXT("selection_graph_unverified"):*Resolved.ErrorCode,Resolved.Error);return R; }
    auto* BP=FindObject<UBlueprint>(nullptr,*Resolved.Data->GetStringField(TEXT("blueprint")));const auto Graph=Resolved.Data->GetObjectField(TEXT("graph"));auto* G=FindObject<UEdGraph>(nullptr,*Graph->GetStringField(TEXT("object_path")));
    if(!Loaded(BP)||!Loaded(G)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||BP->ParentClass!=AActor::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated||!G->bEditable
        ||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||(Graph->GetStringField(TEXT("graph_type"))!=TEXT("function")&&Graph->GetStringField(TEXT("graph_type"))!=TEXT("event"))||G->Nodes.Num()>=2048)
    { Fail(R,TEXT("selection_blueprint_scope_pending"),TEXT("Finite current direct native Actor Blueprint owned editable function/event K2 graph required; other valid variants remain residual."));return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid GraphGuid=G->GraphGuid;if(!Asset.SetTarget(BP,G,R)) { return R; }
    const auto Before=Resolved.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);
    R.Stage=TEXT("create_switch_enum");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_SwitchEnum>(G);
    if(!Asset.Identity(R)||!Asset.Target(R)) { return R; }
    if(!Node) { Fail(R,TEXT("selection_allocation_failed"),TEXT("Failed to create switch enum node; resolver prefix retained."));return R; }const TStrongObjectPtr<UK2Node_SwitchEnum> KeepNode(Node);if(!Asset.SetNode(Node,R)) { return R; }
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());Count(R,TEXT("set_enum_calls"));Asset.EnumAssigned=Asset.Active;Node->SetEnum(Enum);if(!Asset.Check(R)) { return R; }Node->bHasDefaultPin=HasDefault;Node->NodePosX=XY[0];Node->NodePosY=XY[1];
    Asset.Attached=true;Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);if(!Asset.Check(R)) { return R; }
    Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();if(!Asset.Check(R)) { return R; }Asset.NodeGuid=Node->NodeGuid;Asset.GuidFixed=true;
    Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();if(!Asset.Check(R)) { return R; }
    Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();if(!Asset.Check(R)) { return R; }
    Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode();if(!Asset.Check(R)) { return R; }
    int32 CaseCount=0;TArray<TSharedPtr<FJsonValue>> ActualNames;for(const auto* Pin:Node->Pins) { if(Pin&&Pin->Direction==EGPD_Output&&Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Pin->PinName!=FName(TEXT("Default"))) { ++CaseCount;ActualNames.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString())); } }
    R.Data->SetNumberField(TEXT("case_count"),CaseCount);R.Data->SetArrayField(TEXT("actual_case_names"),ActualNames);R.Data->SetStringField(TEXT("node_id"),Node->NodeGuid.ToString(EGuidFormats::Digits));R.Data->SetStringField(TEXT("enum_type"),Enum->GetPathName());
    const bool CountOK=CaseCount==Cases.Num();R.Data->SetBoolField(TEXT("original_case_count_postcondition"),CountOK);
    if(!CountOK) { if(!Asset.Check(R)) { return R; }Asset.Attached=false;Count(R,TEXT("destroy_node_calls"));Node->DestroyNode();Count(R,TEXT("destroy_node_returns"));if(!Asset.Check(R)) { return R; }R.Data->SetBoolField(TEXT("created_node_retired"),!G->Nodes.Contains(Node)); }
    else { if(!Asset.Check(R)) { return R; }Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!Asset.Check(R)) { return R; } }
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto Observed=Function::Resolve(BP->GetPathName(),TEXT(""),GraphGuid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(Observed));if(!Asset.Check(R)) { return R; }R.Data->SetObjectField(TEXT("enum_after"),Asset.Active?Asset.PublicCurrent.ToSharedRef():EnumSnapshot(Enum));
    if(!Observed.bSuccess||!Observed.bChangeStateKnown||!Observed.bPostconditionPassed) { Fail(R,TEXT("selection_after_incomplete"),TEXT("Shared current graph read incomplete; all created/retired/modified prefix retained."));return R; }
    const auto After=Observed.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),After);R.bChanged=Resolved.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After))||!Equal(MakeShared<FJsonValueObject>(EnumBefore),MakeShared<FJsonValueObject>(R.Data->GetObjectField(TEXT("enum_after"))));R.bChangeStateKnown=true;
    if(!CountOK) { Fail(R,TEXT("selection_enum_case_count_mismatch"),TEXT("Original Switch enum case-count postcondition failed; actual retired-node prefix retained, no retry."));return R; }
    const TSharedPtr<FJsonValue>* NodeRow=After->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(NodeRow) { const auto Current=(*NodeRow)->AsObject();R.Data->SetObjectField(TEXT("node"),Current);for(const auto& P:Current->Values) { R.Data->SetField(P.Key,P.Value); }if(Asset.Active) { if(!AssetEnumNodeFields(Node,Asset,R)) { return R; } }else { LegacyNodeFields(Node,R); }if(!Asset.Check(R)) { return R; } }
    auto* Selection=Node->FindPin(TEXT("Selection"),EGPD_Input);auto* Default=Node->FindPin(TEXT("Default"),EGPD_Output);const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());
    FString ExpectedDefault;TObjectPtr<UObject> ExpectedObject=nullptr;FText ExpectedText;bool DefaultProjectionOK=false;
    R.Data->SetNumberField(TEXT("selection_default_projection_calls"),0);
    if(Selection&&Selection->PinType.PinCategory==UEdGraphSchema_K2::PC_Byte&&Selection->PinType.PinSubCategoryObject.Get()==Enum)
    {
        R.Data->SetNumberField(TEXT("selection_default_projection_calls"),1);
        const FGuid ProjectionSelectionId=Selection->PinId,ProjectionDefaultId=Default?Default->PinId:FGuid();const bool ProjectionHadDefault=Default!=nullptr;
        Schema->GetPinDefaultValuesFromString(Selection->PinType,Node,Selection->AutogeneratedDefaultValue,ExpectedDefault,ExpectedObject,ExpectedText,false);
        if(!Asset.Check(R)) { return R; }
        if(Asset.Active&&!AssetSwitchPins(Node,ProjectionSelectionId,ProjectionDefaultId,ProjectionHadDefault,Selection,Default,Asset,R)) { return R; }
        DefaultProjectionOK=Selection->AutogeneratedDefaultValue.IsEmpty()&&Selection->DefaultValue.Equals(ExpectedDefault,ESearchCase::CaseSensitive)
            &&Selection->DefaultObject==ExpectedObject&&Selection->DefaultTextValue.EqualTo(ExpectedText);
    }
    R.Data->SetStringField(TEXT("selection_autogenerated_default_value"),Selection?Selection->AutogeneratedDefaultValue:FString());
    R.Data->SetStringField(TEXT("selection_current_default_value"),Selection?Selection->DefaultValue:FString());
    R.Data->SetStringField(TEXT("selection_projected_default_value"),ExpectedDefault);
    R.Data->SetBoolField(TEXT("selection_default_projection_verified"),DefaultProjectionOK);
    // Native byte enums deliberately use an empty autogenerated default; the public
    // schema projects it to the first current enum entry without changing the pin.
    FString FinalDefaultError;bool FinalDefaultObserved=false;
    if(Asset.Active&&Selection&&Selection->PinType.PinCategory==UEdGraphSchema_K2::PC_Byte&&Selection->PinType.PinSubCategoryObject.Get()==Enum&&!Selection->PinType.IsContainer()&&!Selection->bOrphanedPin&&Selection->LinkedTo.IsEmpty()&&Selection->DefaultObject==nullptr&&Selection->DefaultTextValue.IsEmpty()&&DefaultProjectionOK)
    {
        const FGuid SelectionId=Selection->PinId,DefaultId=Default?Default->PinId:FGuid();const bool HadDefault=Default!=nullptr;FinalDefaultError=Schema->IsCurrentPinDefaultValid(Selection);
        if(!AssetSwitchPins(Node,SelectionId,DefaultId,HadDefault,Selection,Default,Asset,R)) { return R; }
        DefaultProjectionOK=Selection->AutogeneratedDefaultValue.IsEmpty()&&Selection->DefaultValue.Equals(ExpectedDefault,ESearchCase::CaseSensitive)&&Selection->DefaultObject==ExpectedObject&&Selection->DefaultTextValue.EqualTo(ExpectedText);FinalDefaultObserved=true;
    }
    bool PinsOK=Selection&&Selection->PinType.PinCategory==UEdGraphSchema_K2::PC_Byte&&Selection->PinType.PinSubCategoryObject.Get()==Enum&&!Selection->PinType.IsContainer()&&!Selection->bOrphanedPin&&Selection->LinkedTo.IsEmpty()
        &&Selection->DefaultObject==nullptr&&Selection->DefaultTextValue.IsEmpty()&&DefaultProjectionOK&&(Asset.Active?(FinalDefaultObserved&&FinalDefaultError.IsEmpty()):Schema->IsCurrentPinDefaultValid(Selection).IsEmpty())
        &&((HasDefault&&Default&&Default->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)||(!HasDefault&&!Default));
    for(int32 I=0;I<Cases.Num();++I) { PinsOK&=I<ActualNames.Num()&&ActualNames[I]->AsString().Equals(Cases[I]->AsObject()->GetStringField(TEXT("name")),ESearchCase::CaseSensitive); }
    R.Data->SetBoolField(TEXT("enum_selection_and_case_pins_verified"),PinsOK);R.Data->SetBoolField(TEXT("enum_source_preserved"),Equal(MakeShared<FJsonValueObject>(EnumBefore),MakeShared<FJsonValueObject>(R.Data->GetObjectField(TEXT("enum_after")))));
    R.Data->SetBoolField(TEXT("existing_nodes_preserved"),ExistingNodesPreserved(Before,After));R.Data->SetBoolField(TEXT("graph_rows_preserved"),GraphRowsPreserved(Before,After,G->GetPathName()));
    if(!NodeRow||!Loaded(Node)||Node->Enum.Get()!=Enum||Node->bHasDefaultPin!=HasDefault||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||G->GraphGuid!=GraphGuid||!G->Nodes.Contains(Node)||G->Nodes.Num()!=Before->GetArrayField(TEXT("nodes")).Num()+1||!PinsOK
        ||!R.Data->GetBoolField(TEXT("enum_source_preserved"))||!R.Data->GetBoolField(TEXT("existing_nodes_preserved"))||!R.Data->GetBoolField(TEXT("graph_rows_preserved")))
    { Fail(R,TEXT("selection_node_postcondition_failed"),TEXT("Current typed switch/enum/default/position/old-node proof failed; actual prefix retained without cleanup or retry."));return R; }
    if(!Asset.Check(R)) { return R; }R.Data->SetObjectField(TEXT("graph"),Observed.Data->GetObjectField(TEXT("graph")));R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
