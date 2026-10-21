// DD original EnumLiteral8 / IsValid7 / SwitchInt10. Engine nodes only; no explicit compile/save.
#include "Compatibility/UE58/CubelessBlueprintBranchingApi.h"
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
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_SwitchInteger.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintBranching
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeToken(const FString& S) { if(S.Len()>512) { return false; }for(int32 I=0;I<S.Len();++I) { if(!S[I]) { return false; } }return true; }
FString NormalizeTypeName(const FString& TypeName)
{
    FString Normalized = TypeName.ToLower();
    Normalized.TrimStartAndEndInline();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    return Normalized;
}
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("operation_mapping_calls"),TEXT("enum_load_calls"),TEXT("enum_load_returns"),TEXT("enum_observation_calls"),TEXT("enum_default_resolution_calls"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("mapped_function_lookup_calls"),TEXT("mapped_function_lookup_returns"),TEXT("signature_observation_calls"),TEXT("signature_parameter_observation_calls"),TEXT("node_allocation_calls"),TEXT("set_from_function_calls"),TEXT("enum_assignment_calls"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("reconstruct_calls"),TEXT("add_case_pin_calls"),TEXT("add_case_pin_returns"),TEXT("try_set_default_calls"),TEXT("try_set_default_returns"),TEXT("default_validation_calls"),TEXT("default_match_calls"),TEXT("pin_default_changed_calls"),TEXT("pin_default_changed_returns"),TEXT("destroy_node_calls"),TEXT("destroy_node_returns"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original direct Engine node creation. Shared graph resolver may create a requested function graph/refresh skeleton; default setters and MarkModified can affect status/cache/dirty/CDO/children. Counters describe direct calls, not SDK internal callbacks. No explicit fullcompile/save, global-zero or atomic rollback claim."));
    R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
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
        if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>FLT_MAX) { return Fail(R,TEXT("value_position_invalid"),TEXT("Finite float32 position required before graph writer.")); }
        const float F=static_cast<float>(Values[I]);const double D=static_cast<double>(F);
        if(!FMath::IsFinite(F)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)) { return Fail(R,TEXT("value_position_invalid"),TEXT("Original float32 position exceeds safe int32 assignment budget.")); }
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

bool ExistingNodesPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,int32 Delta)
{
    const auto& A=Before->GetArrayField(TEXT("nodes"));const auto& B=After->GetArrayField(TEXT("nodes"));
    if(B.Num()!=A.Num()+Delta) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}

bool GraphRowsPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const FString& Selected,int32 Delta)
{
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(A.Num()!=B.Num()) { return false; }int32 SelectedCount=0;
    for(int32 I=0;I<A.Num();++I)
    { const auto Expected=MakeShared<FJsonObject>(*A[I]->AsObject());if(Expected->GetStringField(TEXT("object_path")).Equals(Selected,ESearchCase::CaseSensitive))
      { ++SelectedCount;Expected->SetNumberField(TEXT("node_count"),Expected->GetNumberField(TEXT("node_count"))+Delta); }
      if(!Equal(MakeShared<FJsonValueObject>(Expected),B[I])) { return false; } }return SelectedCount==1;
}

void LegacyNodeFields(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());
    R.Data->SetStringField(TEXT("title"),Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);
    R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
}
bool ActorScope(UBlueprint* BP,UEdGraph* G,const TSharedPtr<FJsonObject>& Graph,FResult& R)
{
    if(!Loaded(BP)||!Loaded(G)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||BP->ParentClass!=AActor::StaticClass()
        ||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified
        ||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()
        ||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()
        ||(Graph->GetStringField(TEXT("graph_type"))!=TEXT("function")&&Graph->GetStringField(TEXT("graph_type"))!=TEXT("event"))||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("value_blueprint_scope_pending"),TEXT("Finite idle direct native Actor Blueprint with an owned editable function/event K2 graph required; wider original owners remain residual.")); }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("value_external_instances_pending"),TEXT("Loaded external instances/derived Blueprints can receive MarkModified effects; finite scope refused."));
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
bool PlaceAssetEnumNode(UEdGraph* G,UEdGraphNode* N,const int32 (&XY)[2],FEnumAssetScope& Scope,FResult& R)
{
    R.Data->SetStringField(TEXT("created_node_path"),N->GetPathName());N->NodePosX=XY[0];N->NodePosY=XY[1];
    Scope.Attached=true;Count(R,TEXT("graph_add_node_calls"));G->AddNode(N,true);if(!Scope.Check(R)) { return false; }
    Count(R,TEXT("new_guid_calls"));N->CreateNewGuid();if(!Scope.Check(R)) { return false; }Scope.NodeGuid=N->NodeGuid;Scope.GuidFixed=true;
    Count(R,TEXT("post_placed_calls"));N->PostPlacedNewNode();if(!Scope.Check(R)) { return false; }
    Count(R,TEXT("allocate_pins_calls"));N->AllocateDefaultPins();if(!Scope.Check(R)) { return false; }
    Count(R,TEXT("reconstruct_calls"));N->ReconstructNode();if(!Scope.Check(R)) { return false; }
    R.Data->SetStringField(TEXT("created_node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));return true;
}
bool FreshPins(UEdGraphNode* Node);
UEdGraphPin* AssetEnumPin(UEdGraphNode* N,const FGuid& Id,FEnumAssetScope& Scope,FResult& R,EEdGraphPinDirection Direction=EGPD_Input)
{
    if(!Scope.Check(R)) { return nullptr; }if(N->Pins.Num()!=2||!FreshPins(N)) { Scope.Bad(R,TEXT("enum_asset_pin_role_changed"),TEXT("Created literal no longer has the original two fresh unlinked pins."));return nullptr; }if(!Id.IsValid()) { Scope.Bad(R,TEXT("enum_asset_pin_role_changed"),TEXT("Selected pin has no valid persistent ID before the next SDK call."));return nullptr; }UEdGraphPin* Found=nullptr;
    for(UEdGraphPin* P:N->Pins) { if(P&&P->PinId==Id) { if(Found||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->Direction!=Direction||P->PinName!=(Direction==EGPD_Input?UK2Node_EnumLiteral::GetEnumInputPinName():UEdGraphSchema_K2::PN_ReturnValue)||P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Byte||P->PinType.PinSubCategoryObject.Get()!=Scope.Source.Get()||P->PinType.IsContainer()||P->bOrphanedPin||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty()) { Scope.Bad(R,TEXT("enum_asset_pin_role_changed"),TEXT("Current pin ownership/unique ID changed after a callback."));return nullptr; }Found=P; } }
    if(!Found) { Scope.Bad(R,TEXT("enum_asset_pin_role_changed"),TEXT("Original selected pin ID is absent after callback; stale pin was not read.")); }return Found;
}
bool HiddenEnum(const UEnum* Enum,int32 Index)
{
    return !Enum||!Enum->IsValidEnumValue(Enum->GetValueByIndex(Index))||Enum->HasMetaData(TEXT("Hidden"),Index)||Enum->HasMetaData(TEXT("Spacer"),Index);
}
bool EnumDefault(const TSharedPtr<FJsonValue>& V,UEnum* Enum,FString& Text,int32& Index,FResult& R)
{
    Count(R,TEXT("enum_default_resolution_calls"));Index=INDEX_NONE;R.Data->SetBoolField(TEXT("enum_value_supplied"),V.IsValid());R.Data->SetBoolField(TEXT("value_json_omission_sentinel_used"),!V.IsValid());
    if(V.IsValid()) { R.Data->SetField(TEXT("requested_enum_value"),V); }else { R.Data->SetField(TEXT("requested_enum_value"),MakeShared<FJsonValueNull>()); }
    if(!V.IsValid())
    { for(int32 I=0;I<Enum->NumEnums();++I) { if(!HiddenEnum(Enum,I)) { Index=I;break; } } }
    else if(V->Type==EJson::String)
    {
        FString S=V->AsString();S.TrimStartAndEndInline();S.TrimQuotesInline();
        if(S.IsEmpty()) { return Fail(R,TEXT("branching_enum_value_invalid"),TEXT("Enum value cannot be empty.")); }
        Index=Enum->GetIndexByNameString(S);
        if(Index==INDEX_NONE&&!S.Contains(TEXT("::"))) { Index=Enum->GetIndexByNameString(FString::Printf(TEXT("%s::%s"),*Enum->GetName(),*S)); }
        if(Index==INDEX_NONE) { Index=Enum->GetIndexByNameString(Enum->GenerateFullEnumName(*S)); }
    }
    else if(V->Type==EJson::Number)
    {
        const double D=V->AsNumber();
        if(!FMath::IsFinite(D)||FMath::Abs(D)>9007199254740991.0) { return Fail(R,TEXT("branching_enum_numeric_scope_pending"),TEXT("Finite exact-double-safe integer enum value required before original int64 cast; larger original variants remain incomplete.")); }
        const int64 I=static_cast<int64>(D);
        if(!FMath::IsNearlyEqual(D,static_cast<double>(I))) { return Fail(R,TEXT("branching_enum_value_invalid"),TEXT("Enum numeric default must be an integer value.")); }
        Index=Enum->GetIndexByValue(I);
    }
    else { return Fail(R,TEXT("branching_enum_value_invalid"),TEXT("Enum default must be a string enum entry or integer enum value.")); }
    if(Index==INDEX_NONE) { return Fail(R,TEXT("branching_enum_value_invalid"),TEXT("Enum value not found, or enum has no visible values.")); }
    if(HiddenEnum(Enum,Index)) { return Fail(R,TEXT("branching_enum_value_invalid"),TEXT("Enum value is hidden or spacer.")); }
    if(Enum->GetValueByIndex(Index)<0||Enum->GetValueByIndex(Index)>255) { return Fail(R,TEXT("branching_enum_byte_scope_pending"),TEXT("Current selected enum literal must fit the Engine byte pin; wider original values remain incomplete.")); }
    Text=Enum->GetNameStringByIndex(Index);R.Data->SetStringField(TEXT("default_value"),Text);R.Data->SetNumberField(TEXT("default_index"),Index);
    TArray<TSharedPtr<FJsonValue>> Entries;
    for(int32 I=0;I<Enum->NumEnums();++I)
    { if(HiddenEnum(Enum,I)) { continue; }auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Enum->GetNameStringByIndex(I));J->SetNumberField(TEXT("value"),static_cast<double>(Enum->GetValueByIndex(I)));J->SetStringField(TEXT("value_decimal"),LexToString(Enum->GetValueByIndex(I)));J->SetStringField(TEXT("display_name"),Enum->GetDisplayNameTextByIndex(I).ToString());Entries.Add(MakeShared<FJsonValueObject>(J)); }
    R.Data->SetArrayField(TEXT("enum_entries"),Entries);return true;
}
bool IntegerCase(const TSharedPtr<FJsonValue>& V,int32& Out,FResult& R)
{
    if(!V.IsValid()||V->Type!=EJson::Number) { return Fail(R,TEXT("branching_switch_cases_invalid"),TEXT("'case_values' entries must be integers.")); }
    const double D=V->AsNumber();
    if(!FMath::IsFinite(D)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)) { return Fail(R,TEXT("branching_switch_numeric_scope_pending"),TEXT("Finite safe int32 case required before rounding.")); }
    const int64 Rounded=FMath::RoundToInt(D);
    if(Rounded<MIN_int32||Rounded>MAX_int32||!FMath::IsNearlyEqual(D,static_cast<double>(Rounded))) { return Fail(R,TEXT("branching_switch_cases_invalid"),TEXT("'case_values' entries must be integers.")); }
    Out=static_cast<int32>(Rounded);return true;
}
bool SwitchCases(const FString& Json,int32& Start,int32& CountCases,FResult& R)
{
    TSharedPtr<FJsonValue> V;if(!Json.IsEmpty()&&!Function::Pin::ParseJson(Json,V,R)) { return false; }R.Data->SetBoolField(TEXT("case_values_supplied"),V.IsValid());R.Data->SetBoolField(TEXT("case_values_json_omission_sentinel_used"),!V.IsValid());
    if(V.IsValid()) { R.Data->SetField(TEXT("requested_case_values"),V); }else { R.Data->SetField(TEXT("requested_case_values"),MakeShared<FJsonValueNull>()); }
    if(V.IsValid())
    {
        if(V->Type!=EJson::Array) { return Fail(R,TEXT("branching_switch_cases_invalid"),TEXT("'case_values' must be an array.")); }
        const auto& A=V->AsArray();if(A.IsEmpty()) { return Fail(R,TEXT("branching_switch_cases_invalid"),TEXT("'case_values' must contain at least one integer case.")); }
        TSet<int32> Seen;int32 Previous=0;
        for(int32 I=0;I<A.Num();++I)
        {
            int32 N=0;if(!IntegerCase(A[I],N,R)) { return false; }
            if(Seen.Contains(N)) { return Fail(R,TEXT("branching_switch_cases_invalid"),FString::Printf(TEXT("Duplicate switch int case: %d"),N)); }Seen.Add(N);
            if(I==0) { Start=N; }else if(static_cast<int64>(N)!=static_cast<int64>(Previous)+1) { return Fail(R,TEXT("branching_switch_cases_invalid"),TEXT("Sparse switch int cases are not supported; use ascending contiguous case values.")); }Previous=N;
        }
        CountCases=A.Num();
    }
    if(CountCases<=0||CountCases>128) { return Fail(R,TEXT("branching_switch_cases_invalid"),CountCases<=0?TEXT("'case_count' must be greater than zero."):TEXT("'case_count' must be 128 or less.")); }
    if(static_cast<int64>(Start)+CountCases>MAX_int32) { return Fail(R,TEXT("branching_switch_numeric_scope_pending"),TEXT("Start plus case count must remain safe for SDK postincrements; extreme original endpoints remain incomplete.")); }
    R.Data->SetNumberField(TEXT("start_index"),Start);R.Data->SetNumberField(TEXT("case_count"),CountCases);TArray<TSharedPtr<FJsonValue>> Plan;
    for(int32 I=0;I<CountCases;++I) { Plan.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%lld"),static_cast<int64>(Start)+I))); }R.Data->SetArrayField(TEXT("expected_case_names"),Plan);return true;
}
bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedPtr<FJsonObject>& Before,FResult& R,FEnumAssetScope* Asset=nullptr,const FString* ValidityPosition=nullptr)
{
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));
    R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;
    if(Asset&&!Asset->Check(R)) { return false; }
    if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("branching_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));
    if(ValidityPosition&&Loaded(BP)&&BP->BlueprintType==BPTYPE_FunctionLibrary)
    {
        const TStrongObjectPtr<UBlueprint> Hold(BP);auto Body=Function::AddCall(Path,TEXT("/Script/Engine.KismetSystemLibrary"),TEXT("IsValid"),TEXT("{}"),*ValidityPosition,Name,Id,Type,false,false,false,false,false,false);
        bool EnteredLoad=false;S.Data->TryGetBoolField(TEXT("loaded_during_operation"),EnteredLoad);Body.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));Body.Data->SetBoolField(TEXT("prior_graph_resolution_loaded"),EnteredLoad);Body.bMutationAttempted|=S.bMutationAttempted;Body.bChanged|=S.bChanged;Body.bChangeStateKnown=Body.bChangeStateKnown&&S.bChangeStateKnown&&(!EnteredLoad||Body.bSuccess);Body.Data->SetStringField(TEXT("value_type"),TEXT("object"));Body.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_CallFunction"));Body.Data->SetStringField(TEXT("mapped_function_name"),TEXT("IsValid"));R=MoveTemp(Body);return false;
    }
    const auto J=S.Data->GetObjectField(TEXT("graph"));G=FindObject<UEdGraph>(nullptr,*J->GetStringField(TEXT("object_path")));
    if(!ActorScope(BP,G,J,R)) { return false; }Before=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());return true;
}
void Place(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const int32 (&XY)[2],bool Reconstruct,FResult& R)
{
    R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());Node->NodePosX=XY[0];Node->NodePosY=XY[1];
    Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();
    if(Reconstruct) { Count(R,TEXT("reconstruct_calls"));Node->ReconstructNode(); }R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
}
void Mark(UBlueprint* BP,FResult& R)
{
    R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_blueprint_modified_returns"));
}
bool After(UBlueprint* BP,UEdGraph* G,UEdGraphNode* Node,const FGuid& Guid,const TSharedPtr<FJsonObject>& Before,int32 Delta,FResult& R,FEnumAssetScope* Asset=nullptr)
{
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(BP->GetPathName(),TEXT(""),Guid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(S));
    if(Asset&&!Asset->Check(R)) { return false; }
    if(!S.bSuccess||!S.bChangeStateKnown||!S.bPostconditionPassed) { return Fail(R,TEXT("branching_after_incomplete"),TEXT("Current graph observation did not close; actual mutation/cleanup prefix retained, no retry.")); }
    const auto A=S.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),A);R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;
    const auto* Row=A->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& V){return V->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Row) { R.Data->SetObjectField(TEXT("node"),(*Row)->AsObject());for(const auto& P:(*Row)->AsObject()->Values) { R.Data->SetField(P.Key,P.Value); }if(Asset) { if(!AssetEnumNodeFields(Node,*Asset,R)) { return false; } }else { LegacyNodeFields(Node,R); } }
    const bool Old=ExistingNodesPreserved(Before,A,Delta),Rows=GraphRowsPreserved(Before,A,G->GetPathName(),Delta);R.Data->SetBoolField(TEXT("existing_nodes_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetObjectField(TEXT("graph"),S.Data->GetObjectField(TEXT("graph")));
    return (Old&&Rows&&G->GraphGuid==Guid&&(Delta==0?!Row:Row!=nullptr))||Fail(R,TEXT("branching_graph_postcondition_failed"),TEXT("Selected node/old graph preservation differs; actual current state retained, no rollback claimed."));
}
bool FreshPins(UEdGraphNode* Node)
{
    TSet<FGuid> Ids;
    for(const auto* P:Node->Pins)
    { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return false; }Ids.Add(P->PinId); }
    return true;
}
struct FSignature { TMap<FName,FEdGraphPinType> Inputs,Outputs;TSharedRef<FJsonObject> Json=MakeShared<FJsonObject>(); };
bool ValidSignature(UFunction* F,bool Class,const UEdGraphSchema_K2* Schema,FSignature& Out,FResult& R,const TCHAR* Key)
{
    Count(R,TEXT("signature_observation_calls"));R.Data->SetObjectField(Key,Out.Json);UClass* Owner=UKismetSystemLibrary::StaticClass();
    if(!Loaded(Owner)||!Loaded(F)||F->GetFName()!=(Class?FName(TEXT("IsValidClass")):FName(TEXT("IsValid")))||F->GetOuter()!=Owner||Owner->ClassGeneratedBy!=nullptr||!Owner->HasAnyClassFlags(CLASS_Native)||Owner->GetOutermost()->GetName()!=TEXT("/Script/Engine")||!F->HasAllFunctionFlags(FUNC_Public|FUNC_Static|FUNC_BlueprintCallable|FUNC_BlueprintPure)
        ||F->HasAnyFunctionFlags(FUNC_Private|FUNC_Protected|FUNC_EditorOnly|FUNC_Delegate|FUNC_Net)||F->HasMetaData(TEXT("Latent"))||F->HasMetaData(TEXT("DeprecatedFunction"))||F->GetBoolMetaData(TEXT("BlueprintInternalUseOnly")))
    { return Fail(R,TEXT("branching_is_valid_signature_pending"),TEXT("Exact public static pure native Engine IsValid/IsValidClass required.")); }
    Out.Json->SetStringField(TEXT("function"),F->GetPathName());Out.Json->SetStringField(TEXT("owner"),Owner->GetPathName());Out.Json->SetStringField(TEXT("function_flags"),LexToString(static_cast<uint32>(F->FunctionFlags)));Out.Json->SetStringField(TEXT("owner_class_flags"),LexToString(static_cast<uint32>(Owner->GetClassFlags())));Out.Json->SetStringField(TEXT("owner_object_flags"),LexToString(static_cast<uint32>(Owner->GetFlags())));Out.Json->SetBoolField(TEXT("package_dirty"),Owner->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> Rows;
    for(TFieldIterator<FProperty> It(F);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        Count(R,TEXT("signature_parameter_observation_calls"));const bool Return=It->HasAnyPropertyFlags(CPF_ReturnParm);const auto* C=CastField<FClassProperty>(*It);const auto* O=CastField<FObjectProperty>(*It);FEdGraphPinType T;
        const bool TypeOK=Return?CastField<FBoolProperty>(*It)!=nullptr:Class?(C&&C->PropertyClass==UClass::StaticClass()&&C->MetaClass==UObject::StaticClass()):(O&&!C&&O->PropertyClass==UObject::StaticClass());
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),It->GetName());J->SetStringField(TEXT("property_class"),It->GetClass()->GetName());J->SetStringField(TEXT("property_flags"),LexToString(static_cast<uint64>(It->GetPropertyFlags())));Rows.Add(MakeShared<FJsonValueObject>(J));Out.Json->SetArrayField(TEXT("parameters"),Rows);
        if(!TypeOK||It->GetOwnerStruct()!=F||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_ReferenceParm)||(!Return&&It->HasAnyPropertyFlags(CPF_OutParm))||!Schema->ConvertPropertyToPinType(*It,T)||T.IsContainer()||T.bIsReference||T.bIsWeakPointer||It->GetFName()!=(Return?FName(TEXT("ReturnValue")):Class?FName(TEXT("Class")):FName(TEXT("Object"))))
        { return Fail(R,TEXT("branching_is_valid_signature_pending"),TEXT("Current IsValid parameter identity/type/layout differs.")); }
        J->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(T));if(Return) { Out.Outputs.Add(It->GetFName(),T); }else { Out.Inputs.Add(It->GetFName(),T); }
    }
    return (Out.Inputs.Num()==1&&Out.Outputs.Num()==1)||Fail(R,TEXT("branching_is_valid_signature_pending"),TEXT("Exactly one typed input and bool return required."));
}
bool Pins(UK2Node_CallFunction* Node,UClass* Owner,const FSignature& Sig,const UEdGraphSchema_K2* Schema)
{
    if(!Loaded(Node)||!Node->IsNodePure()||Node->Pins.Num()!=Sig.Inputs.Num()+Sig.Outputs.Num()+1) { return false; }
    TSet<FGuid> Ids;
    for(const UEdGraphPin* P:Node->Pins)
    { if(!P||P->WasTrashed()||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->GetOwningNodeUnchecked()!=Node||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return false; }Ids.Add(P->PinId); }
    for(const auto& Pair:Sig.Inputs)
    { const auto* P=Node->FindPin(Pair.Key,EGPD_Input);if(!P||P->PinType!=Pair.Value||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bDefaultValueIsIgnored||!Schema->IsCurrentPinDefaultValid(P).IsEmpty()) { return false; } }
    for(const auto& Pair:Sig.Outputs)
    { const auto* P=Node->FindPin(Pair.Key,EGPD_Output);if(!P||P->PinType!=Pair.Value||P->bHidden||P->bNotConnectable||P->bDefaultValueIsIgnored) { return false; } }
    const auto* Self=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
    return Self&&Self->bHidden&&Self->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Self->PinType.PinSubCategoryObject.Get()==Owner
        &&!Self->PinType.IsContainer()&&!Self->PinType.bIsReference&&(!Self->DefaultObject||Self->DefaultObject->GetClass()==Owner);
}
}
Function::FResult AddEnumLiteral(const FString& Path,const FString& EnumType,const FString& ValueJson,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_enum_type"),EnumType);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_EnumLiteral"));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("branching_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddEnumLiteral","Add Blueprint Enum Literal"));
    auto* Enum=ResolveEnum(EnumType,R);if(!Enum) { return R; }const TStrongObjectPtr<UEnum> KeepEnum(Enum);FEnumAssetScope Asset;if(!Asset.Capture(Enum,R)) { return R; }
    Count(R,TEXT("enum_observation_calls"));const auto E=Asset.Active?Asset.PublicBefore.ToSharedRef():EnumSnapshot(Enum);R.Data->SetObjectField(TEXT("enum_before"),E);
    TSharedPtr<FJsonValue> V;if(!ValueJson.IsEmpty()&&!Function::Pin::ParseJson(ValueJson,V,R)) { return R; }FString Default;int32 Index=INDEX_NONE;if(!EnumDefault(V,Enum,Default,Index,R)||!Asset.Check(R)) { return R; }
    int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Asset.Check(R)||!Graph(Path,Name,Id,Type,Create,BP,G,Before,R,Asset.Active?&Asset:nullptr)||!Asset.Check(R)||!Asset.SetTarget(BP,G,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());
    R.Stage=TEXT("create_enum_literal");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_EnumLiteral>(G);
    if(!Asset.Identity(R)||!Asset.Target(R)) { return R; }
    if(!Node) { Fail(R,TEXT("branching_allocation_failed"),TEXT("Enum node allocation returned null; attempted prefix retained."));return R; }const TStrongObjectPtr<UK2Node_EnumLiteral> KeepNode(Node);if(!Asset.SetNode(Node,R)) { return R; }
    Count(R,TEXT("enum_assignment_calls"));Asset.EnumAssigned=Asset.Active;Node->Enum=Enum;UEdGraphNode* Base=Node;
    if(Asset.Active) { if(!PlaceAssetEnumNode(G,Base,XY,Asset,R)) { return R; } }else { Place(BP,G,Base,XY,true,R); }
    UEdGraphPin* Input=Base->FindPin(UK2Node_EnumLiteral::GetEnumInputPinName(),EGPD_Input);bool Accepted=Input!=nullptr;R.Data->SetBoolField(TEXT("enum_input_pin_found"),Accepted);
    if(Asset.Active&&Input) { Input=AssetEnumPin(Base,Input->PinId,Asset,R);if(!Input) { return R; } }
    if(!Accepted) { Fail(R,TEXT("branching_enum_pin_missing"),TEXT("Failed to resolve enum literal input pin.")); }
    else
    {
        R.Stage=TEXT("apply_enum_default");const FGuid InputId=Input->PinId;Count(R,TEXT("try_set_default_calls"));Schema->TrySetDefaultValue(*Input,Default);Count(R,TEXT("try_set_default_returns"));
        if(Asset.Active) { Input=AssetEnumPin(Base,InputId,Asset,R);if(!Input) { return R; } }R.Data->SetStringField(TEXT("returned_default_value"),Input->DefaultValue);
        Count(R,TEXT("default_validation_calls"));const FString Error=Schema->IsCurrentPinDefaultValid(Input);
        if(Asset.Active) { Input=AssetEnumPin(Base,InputId,Asset,R);if(!Input) { return R; } }Count(R,TEXT("default_match_calls"));const int32 Actual=Enum->GetIndexByNameString(Input->DefaultValue);R.Data->SetStringField(TEXT("default_validation_error"),Error);R.Data->SetNumberField(TEXT("returned_default_index"),Actual);
        Accepted=Error.IsEmpty()&&Actual==Index;if(!Accepted) { Fail(R,TEXT("branching_enum_default_invalid"),Error.IsEmpty()?TEXT("Requested enum literal default was not accepted."):Error); }
        else { Count(R,TEXT("pin_default_changed_calls"));Base->PinDefaultValueChanged(Input);Count(R,TEXT("pin_default_changed_returns"));if(!Asset.Check(R)) { return R; } }
    }
    R.Data->SetBoolField(TEXT("enum_value_accepted"),Accepted);const FString ErrorCode=R.ErrorCode,Error=R.Error;
    if(!Accepted) { R.Stage=TEXT("retire_invalid_enum_literal");R.Data->SetStringField(TEXT("original_enum_error_code"),ErrorCode);R.Data->SetStringField(TEXT("original_enum_error"),Error);if(!Asset.Check(R)) { return R; }Asset.Attached=false;Count(R,TEXT("destroy_node_calls"));Base->DestroyNode();Count(R,TEXT("destroy_node_returns"));if(!Asset.Check(R)) { return R; }R.Data->SetBoolField(TEXT("created_node_retired"),!G->Nodes.Contains(Node)); }else { if(!Asset.Check(R)) { return R; }Mark(BP,R);if(!Asset.Check(R)) { return R; } }
    const bool Observed=After(BP,G,Base,Guid,Before,Accepted?1:0,R,Asset.Active?&Asset:nullptr);if(!Asset.Check(R)) { return R; }Count(R,TEXT("enum_observation_calls"));const auto EA=Asset.Active?Asset.PublicCurrent.ToSharedRef():EnumSnapshot(Enum);R.Data->SetObjectField(TEXT("enum_after"),EA);const bool Same=Equal(MakeShared<FJsonValueObject>(E),MakeShared<FJsonValueObject>(EA));R.Data->SetBoolField(TEXT("enum_source_preserved"),Same);R.bChanged=R.bChanged||!Same;
    if(!Observed) { return R; }if(!Accepted)
    { const bool Retired=!G->Nodes.Contains(Node)&&Same;R.Data->SetBoolField(TEXT("retired_node_graph_postcondition"),Retired);if(!Retired) { R.bChangeStateKnown=false;Fail(R,TEXT("branching_enum_cleanup_incomplete"),TEXT("New-node retirement/source proof did not close; no atomic restoration claimed.")); }else { Fail(R,*ErrorCode,Error); }return R; }
    const auto* Output=Base->FindPin(UEdGraphSchema_K2::PN_ReturnValue,EGPD_Output);Input=Base->FindPin(UK2Node_EnumLiteral::GetEnumInputPinName(),EGPD_Input);
    FString FinalDefaultError;bool FinalDefaultObserved=false;
    if(Asset.Active&&FreshPins(Base)&&Base->Pins.Num()==2&&Input&&Output&&Input->PinType==Output->PinType&&Input->PinType.PinCategory==UEdGraphSchema_K2::PC_Byte&&Input->PinType.PinSubCategoryObject.Get()==Enum&&!Input->PinType.IsContainer()&&!Input->PinType.bIsReference)
    {
        const FGuid InputId=Input->PinId,OutputId=Output->PinId;FinalDefaultError=Schema->IsCurrentPinDefaultValid(Input);
        Input=AssetEnumPin(Base,InputId,Asset,R);if(!Input) { return R; }Output=AssetEnumPin(Base,OutputId,Asset,R,EGPD_Output);if(!Output) { return R; }FinalDefaultObserved=true;
    }
    const bool PinsOK=FreshPins(Base)&&Base->Pins.Num()==2&&Input&&Output&&Input->PinType==Output->PinType&&Input->PinType.PinCategory==UEdGraphSchema_K2::PC_Byte&&Input->PinType.PinSubCategoryObject.Get()==Enum&&!Input->PinType.IsContainer()&&!Input->PinType.bIsReference&&(Asset.Active?(FinalDefaultObserved&&FinalDefaultError.IsEmpty()):Schema->IsCurrentPinDefaultValid(Input).IsEmpty())&&Enum->GetIndexByNameString(Input->DefaultValue)==Index;
    R.Data->SetBoolField(TEXT("enum_literal_pin_default_verified"),PinsOK);R.Data->SetStringField(TEXT("enum_type"),Enum->GetPathName());
    if(!Loaded(Node)||Node->GetClass()!=UK2Node_EnumLiteral::StaticClass()||Node->Enum.Get()!=Enum||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!PinsOK||!Same)
    { Fail(R,TEXT("branching_enum_postcondition_failed"),TEXT("Current enum node/default/pin/source identity differs; accepted prefix retained without cleanup/retry."));return R; }
    if(!Asset.Check(R)) { return R; }R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult AddIsValid(const FString& Path,const FString& ValueType,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("value_type"),ValueType);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_CallFunction"));
    if(!IsEditorGameThread()||IsPlaySessionActive()||!SafeToken(ValueType)) { Fail(R,TEXT("branching_editor_or_type_invalid"),TEXT("Idle Editor and bounded type without interior NUL required."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddIsValid","Add Blueprint Is Valid"));Count(R,TEXT("operation_mapping_calls"));const FString Normal=NormalizeTypeName(ValueType);const bool Class=Normal==TEXT("class");
    if(!Normal.IsEmpty()&&Normal!=TEXT("object")&&!Class) { Fail(R,TEXT("branching_is_valid_type_unsupported"),TEXT("Unsupported is_valid value type: ")+ValueType);return R; }
    int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R,nullptr,Class?nullptr:&PositionJson)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());UClass* Owner=UKismetSystemLibrary::StaticClass();const TStrongObjectPtr<UClass> KeepOwner(Owner);
    const FName Fn=Class?GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary,IsValidClass):GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary,IsValid);Count(R,TEXT("mapped_function_lookup_calls"));auto* F=Owner->FindFunctionByName(Fn);Count(R,TEXT("mapped_function_lookup_returns"));R.Data->SetStringField(TEXT("mapped_function_name"),Fn.ToString());
    if(!Loaded(F)) { Fail(R,TEXT("branching_is_valid_function_unavailable"),TEXT("Mapped IsValid UFunction unavailable; resolver prefix retained."));return R; }const TStrongObjectPtr<UFunction> KeepFunction(F);FSignature Sig;if(!ValidSignature(F,Class,Schema,Sig,R,TEXT("native_signature_before"))) { return R; }
    FText Reason;if(!Schema->CanFunctionBeUsedInGraph(Owner,F,G,UEdGraphSchema_K2::FT_Pure|UEdGraphSchema_K2::FT_Imperative|UEdGraphSchema_K2::FT_Const,false,&Reason)) { Fail(R,TEXT("branching_is_valid_graph_rejected"),Reason.ToString());return R; }
    R.Stage=TEXT("create_is_valid");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_CallFunction>(G);
    if(!Node) { Fail(R,TEXT("branching_allocation_failed"),TEXT("IsValid call-node allocation returned null."));return R; }const TStrongObjectPtr<UK2Node_CallFunction> KeepNode(Node);Count(R,TEXT("set_from_function_calls"));Node->SetFromFunction(F);Place(BP,G,Node,XY,false,R);Mark(BP,R);
    const bool Observed=After(BP,G,Node,Guid,Before,1,R);FSignature SA;const bool SignatureOK=ValidSignature(F,Class,Schema,SA,R,TEXT("native_signature_after"));const bool Same=SignatureOK&&Equal(MakeShared<FJsonValueObject>(Sig.Json),MakeShared<FJsonValueObject>(SA.Json));R.Data->SetBoolField(TEXT("native_signature_preserved"),Same);R.bChanged=R.bChanged||!Same;if(!SignatureOK) { R.bChangeStateKnown=false; }if(!Observed||!SignatureOK) { return R; }
    const bool Identity=Loaded(Node)&&Node->GetClass()==UK2Node_CallFunction::StaticClass()&&Node->GetTargetFunction()==F&&Node->FunctionReference.GetMemberName()==Fn&&Node->FunctionReference.GetMemberParentClass()==Owner&&!Node->FunctionReference.IsSelfContext()&&!Node->FunctionReference.GetMemberGuid().IsValid()&&G->Nodes.Contains(Node)&&Node->NodeGuid.IsValid()&&Node->NodePosX==XY[0]&&Node->NodePosY==XY[1];
    const bool PinOK=Pins(Node,Owner,Sig,Schema);R.Data->SetBoolField(TEXT("function_member_identity_verified"),Identity);R.Data->SetBoolField(TEXT("is_valid_pin_shape_verified"),PinOK);
    if(!Identity||!PinOK||!Same) { Fail(R,TEXT("branching_is_valid_postcondition_failed"),TEXT("Current IsValid function/pin/source/position proof failed; actual accepted prefix retained, no rollback."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
Function::FResult AddSwitchInt(const FString& Path,int32 Start,int32 Cases,const FString& CaseValuesJson,bool HasDefault,const FString& PositionJson,const FString& Name,const FString& Id,const FString& Type,bool Create)
{
    FResult R;Init(R);R.Data->SetNumberField(TEXT("requested_start_index"),Start);R.Data->SetNumberField(TEXT("requested_case_count"),Cases);R.Data->SetBoolField(TEXT("has_default_pin"),HasDefault);R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_SwitchInteger"));
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("branching_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddSwitchInt","Add Blueprint Switch Int"));if(!SwitchCases(CaseValuesJson,Start,Cases,R)) { return R; }
    int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedPtr<FJsonObject> Before;if(!Graph(Path,Name,Id,Type,Create,BP,G,Before,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid Guid=G->GraphGuid;const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());
    R.Stage=TEXT("create_switch_int");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_SwitchInteger>(G);
    if(!Node) { Fail(R,TEXT("branching_allocation_failed"),TEXT("Switch integer allocation returned null."));return R; }const TStrongObjectPtr<UK2Node_SwitchInteger> KeepNode(Node);Node->StartIndex=Start;Node->bHasDefaultPin=HasDefault;UEdGraphNode* Base=Node;Place(BP,G,Base,XY,false,R);UK2Node_Switch* Switch=Node;
    for(int32 I=0;I<Cases;++I) { Count(R,TEXT("add_case_pin_calls"));Switch->AddPinToSwitchNode();Count(R,TEXT("add_case_pin_returns")); }Count(R,TEXT("reconstruct_calls"));Base->ReconstructNode();Mark(BP,R);
    if(!After(BP,G,Base,Guid,Before,1,R)) { return R; }TArray<TSharedPtr<FJsonValue>> Actual;
    for(const auto* P:Base->Pins) { if(P&&P->Direction==EGPD_Output&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&P->PinName!=FName(TEXT("Default"))) { Actual.Add(MakeShared<FJsonValueString>(P->PinName.ToString())); } }R.Data->SetArrayField(TEXT("actual_case_names"),Actual);R.Data->SetNumberField(TEXT("actual_case_count"),Actual.Num());
    const auto* Selection=Switch->GetSelectionPin();const auto* Default=Switch->GetDefaultPin();const auto* Exec=Base->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);const auto* FnPin=Switch->GetFunctionPin();
    const bool FunctionOK=Switch->FunctionName==FName(TEXT("NotEqual_IntInt"))&&Switch->FunctionClass.Get()==UKismetMathLibrary::StaticClass()&&FnPin&&FnPin->Direction==EGPD_Input&&FnPin->PinName==Switch->FunctionName&&FnPin->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&FnPin->PinType.PinSubCategoryObject.Get()==UKismetMathLibrary::StaticClass()&&FnPin->bHidden&&FnPin->bNotConnectable&&FnPin->bDefaultValueIsReadOnly&&FnPin->DefaultObject&&FnPin->DefaultObject->GetClass()==UKismetMathLibrary::StaticClass();
    R.Data->SetBoolField(TEXT("switch_engine_function_pin_verified"),FunctionOK);
    bool PinsOK=FreshPins(Base)&&Base->Pins.Num()==Cases+3+(HasDefault?1:0)&&FunctionOK&&Selection&&Exec&&Selection->Direction==EGPD_Input&&Selection->PinType.PinCategory==UEdGraphSchema_K2::PC_Int&&!Selection->PinType.IsContainer()&&!Selection->PinType.bIsReference&&Schema->IsCurrentPinDefaultValid(Selection).IsEmpty()&&Exec->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&((HasDefault&&Default&&Default->Direction==EGPD_Output&&Default->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)||(!HasDefault&&!Default))&&Actual.Num()==Cases;
    for(int32 I=0;I<Actual.Num()&&I<Cases;++I) { PinsOK=PinsOK&&Actual[I]->AsString().Equals(R.Data->GetArrayField(TEXT("expected_case_names"))[I]->AsString(),ESearchCase::CaseSensitive); }R.Data->SetBoolField(TEXT("switch_integer_pin_plan_verified"),PinsOK);
    if(!Loaded(Node)||Node->GetClass()!=UK2Node_SwitchInteger::StaticClass()||Node->StartIndex!=Start||Node->bHasDefaultPin!=HasDefault||!G->Nodes.Contains(Node)||!Node->NodeGuid.IsValid()||Node->NodePosX!=XY[0]||Node->NodePosY!=XY[1]||!PinsOK)
    { Fail(R,TEXT("branching_switch_postcondition_failed"),TEXT("Current switch case/Selection/Default/position/identity proof failed; original accepted node remains, no new rollback."));return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
