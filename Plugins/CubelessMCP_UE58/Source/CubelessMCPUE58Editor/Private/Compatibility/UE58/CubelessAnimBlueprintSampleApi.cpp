#include "Compatibility/UE58/CubelessAnimBlueprintSampleApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessCompilerDiagnostics.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/BlueprintExtension.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "ScopedTransaction.h"

namespace Cubeless::UE58::AnimBlueprintSample
{
namespace
{
using Variable::FResult;
struct FContext
{
    FResult& Result;
    TArray<TStrongObjectPtr<UObject>> Held;
    TSet<const UObject*> HeldSet;
    TSet<const UEdGraphPin*> Pins;
    USkeleton* Skeleton=nullptr;
    USkeletalMesh* Mesh=nullptr;
    USkeletalMesh* PreviewMesh=nullptr;
    UClass* Parent=nullptr;
    UAnimBlueprint* Target=nullptr;
    FString ExpectedTargetPackage,SourceBefore;
    bool bSourceReady=false,bIncomplete=false,bUnprovedWriter=false;
    explicit FContext(FResult& InResult):Result(InResult) {}
};
FContext* Active=nullptr;
constexpr int32 MaxGraphs=4096,MaxNodes=4096,MaxPins=32768,MaxOutput=1024*1024;
bool Loaded(const UObject* Object)
{ return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeText(const FString& Text,int32 Limit)
{ if(Text.Len()>Limit) { return false; }for(int32 I=0;I<Text.Len();++I) { if(Text[I]==0) { return false; } }return true; }
void Count(const TCHAR* Key)
{ auto C=Active->Result.Data->GetObjectField(TEXT("native_call_counts"));double N=0;C->TryGetNumberField(Key,N);C->SetNumberField(Key,N+1); }
void Incomplete(const TCHAR* Error)
{ Active->bIncomplete=true;auto& R=Active->Result;R.bChangeStateKnown=false;R.ErrorCode=TEXT("anim_blueprint_sample_observation_incomplete");R.Error=Error;R.Data->SetBoolField(TEXT("observation_incomplete"),true); }
bool Hold(const UObject* Object)
{
    if(!Object) { return true; }
    if(!Loaded(Object)||!Loaded(Object->GetClass())) { Incomplete(TEXT("Current object or class is unavailable"));return false; }
    if(!Active->HeldSet.Contains(Object))
    { if(Active->HeldSet.Num()>=32768) { Incomplete(TEXT("Source reference budget exceeded"));return false; }Active->HeldSet.Add(Object);Active->Held.Emplace(const_cast<UObject*>(Object)); }
    return true;
}
void Writer(const TCHAR* Key)
{ auto& R=Active->Result;R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(Key); }
TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Error)
{ auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J; }
FString Serialize(const TSharedRef<FJsonObject>& Object)
{ FString Text;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Text));return Text; }
bool IsAllowedPostProcessVariantTargetPath(const FString& PackagePath);
bool GetJsonBoolFieldOrDefault(const TSharedPtr<FJsonObject>& Object,const FString& FieldName,bool bDefaultValue);

FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, const FString& DefaultValue = FString())
{
    if (!Params.IsValid())
    {
        return DefaultValue;
    }

    FString Value;
    return Params->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}

bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefaultValue = false)
{
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return bDefaultValue;
    }
    return Params->GetBoolField(FieldName);
}

FString NormalizeAssetObjectPathForLoad(const FString& AssetPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(AssetPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if ((NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Engine/"))) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}

FString NormalizePackagePathForPostProcessVariant(const FString& RawPath)
{
    FString PackagePath = FPackageName::ExportTextPathToObjectPath(RawPath).TrimStartAndEnd();
    PackagePath.TrimQuotesInline();
    PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));

    int32 DotIndex = INDEX_NONE;
    if (PackagePath.FindChar(TEXT('.'), DotIndex))
    {
        PackagePath.LeftInline(DotIndex);
    }

    while (PackagePath.Contains(TEXT("//")))
    {
        PackagePath.ReplaceInline(TEXT("//"), TEXT("/"));
    }

    while (PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath.LeftChopInline(1);
    }

    return PackagePath;
}

bool IsValidGamePackagePathForPostProcessVariant(const FString& PackagePath, FString& OutError)
{
    if (PackagePath.IsEmpty())
    {
        OutError = TEXT("Package path cannot be empty");
        return false;
    }
    if (!PackagePath.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive))
    {
        OutError = FString::Printf(TEXT("Expected a /Game package path, got: %s"), *PackagePath);
        return false;
    }
    if (PackagePath.Contains(TEXT(".")))
    {
        OutError = FString::Printf(TEXT("Expected a package path without object suffix, got: %s"), *PackagePath);
        return false;
    }
    return true;
}

bool IsAllowedPostProcessVariantTargetPath(const FString& PackagePath)
{
    return PackagePath.Equals(TEXT("/Game/_MCP_Sample"), ESearchCase::IgnoreCase)
        || PackagePath.StartsWith(TEXT("/Game/_MCP_Sample/"), ESearchCase::IgnoreCase);
}

bool IsAllowedAnimStateMachineSampleTargetPath(const FString& PackagePath)
{
    return IsAllowedPostProcessVariantTargetPath(PackagePath)
        || PackagePath.Equals(TEXT("/Game/_MCP_Temp"), ESearchCase::IgnoreCase)
        || PackagePath.StartsWith(TEXT("/Game/_MCP_Temp/"), ESearchCase::IgnoreCase);
}

bool GetJsonBoolFieldOrDefault(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool bDefaultValue)
{
    bool bValue = bDefaultValue;
    return Object.IsValid() && Object->TryGetBoolField(FieldName, bValue) ? bValue : bDefaultValue;
}

bool ApplySampleCompileSaveOutcome(
    const TSharedPtr<FJsonObject>& ResultObj,
    const TSharedPtr<FJsonObject>& CompileResult,
    bool bCompileRequested,
    bool bSaveRequested,
    bool bTargetSaved)
{
    if (!ResultObj.IsValid())
    {
        return false;
    }

    const bool bCompileSucceeded = !bCompileRequested || GetJsonBoolFieldOrDefault(CompileResult, TEXT("success"), false);
    const bool bSaveSucceeded = !bSaveRequested || bTargetSaved;
    const bool bOverallSuccess = bCompileSucceeded && bSaveSucceeded;

    ResultObj->SetBoolField(TEXT("compile_succeeded"), bCompileSucceeded);
    ResultObj->SetBoolField(TEXT("save_succeeded"), bSaveSucceeded);
    ResultObj->SetBoolField(TEXT("success"), bOverallSuccess);

    if (!bOverallSuccess)
    {
        TArray<TSharedPtr<FJsonValue>> CompletionErrorValues;
        if (!bCompileSucceeded)
        {
            CompletionErrorValues.Add(MakeShared<FJsonValueString>(TEXT("compile_failed")));
        }
        if (!bSaveSucceeded)
        {
            CompletionErrorValues.Add(MakeShared<FJsonValueString>(TEXT("save_failed")));
        }
        ResultObj->SetArrayField(TEXT("completion_errors"), CompletionErrorValues);
    }

    return bOverallSuccess;
}

bool RefSkeletonSafe(const FReferenceSkeleton& Ref)
{
    const auto& Info=Ref.GetRefBoneInfo();const auto& Pose=Ref.GetRefBonePose();
    if(Info.Num()>4096||Info.Num()!=Pose.Num()) { Incomplete(TEXT("Source bone info/pose arrays are not aligned or bounded"));return false; }
    for(int32 I=0;I<Info.Num();++I)
    { if(Info[I].ParentIndex<-1||Info[I].ParentIndex>=I||Pose[I].ContainsNaN()||Ref.FindBoneIndex(Info[I].Name)!=I)
      { Incomplete(TEXT("Source canonical bone name/parent/finite pose proof failed"));return false; } }
    return true;
}
bool MeshSafe(USkeletalMesh* Mesh)
{ if(!Mesh) { return true; }if(!Hold(Mesh)||Mesh->IsCompiling()||!Hold(Mesh->GetSkeleton())) { Incomplete(TEXT("Source mesh is unavailable or compiling"));return false; }return RefSkeletonSafe(Mesh->GetRefSkeleton()); }
bool BlueprintSafe(UBlueprint* Blueprint,TSharedPtr<FJsonObject>* Observation=nullptr)
{
    if(!Blueprint||!Hold(Blueprint)||!Hold(Blueprint->GetOutermost())||Blueprint->bBeingCompiled)
    { Incomplete(TEXT("Blueprint unavailable or already compiling"));return false; }
    if(!Hold(Blueprint->ParentClass.Get())) { return false; }
    for(UClass* Class:{Blueprint->GeneratedClass.Get(),Blueprint->SkeletonGeneratedClass.Get()})
    { if(Class&&(!Hold(Class)||Class->ClassGeneratedBy!=Blueprint)) { Incomplete(TEXT("Current Blueprint class ownership changed"));return false; } }
    const auto Extensions=Blueprint->GetExtensions();if(Extensions.Num()>MaxGraphs) { Incomplete(TEXT("Blueprint extension budget exceeded"));return false; }
    for(const UBlueprintExtension* Extension:Extensions)
    {
        if(!Extension) { continue; }if(!Hold(Extension)) { return false; }
        int32 Depth=0;for(UClass* Class=Extension->GetClass();Class&&Class!=UBlueprintExtension::StaticClass();Class=Class->GetSuperClass())
        { if(++Depth>32||!Hold(Class)||!Class->IsNative()) { Incomplete(TEXT("Unaudited Blueprint graph producer"));return false; }
          const FString Package=Class->GetOutermost()->GetName();if(Package!=TEXT("/Script/Engine")&&Package!=TEXT("/Script/AnimGraph")&&Package!=TEXT("/Script/UMGEditor"))
          { Incomplete(TEXT("Unaudited Blueprint extension graph producer"));return false; } }
    }
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Blueprint->GetPathName());J->SetStringField(TEXT("guid"),Blueprint->GetBlueprintGuid().ToString());
    J->SetStringField(TEXT("parent_class"),GetPathNameSafe(Blueprint->ParentClass.Get()));J->SetStringField(TEXT("generated_class"),GetPathNameSafe(Blueprint->GeneratedClass.Get()));
    J->SetStringField(TEXT("skeleton_class"),GetPathNameSafe(Blueprint->SkeletonGeneratedClass.Get()));J->SetBoolField(TEXT("dirty"),Blueprint->GetOutermost()->IsDirty());J->SetNumberField(TEXT("status"),static_cast<int32>(Blueprint->Status));
    TArray<TSharedPtr<FJsonValue>> GraphRows;TSet<const UEdGraph*> Path;int32 GraphCount=0,NodeCount=0,PinCount=0;
    TFunction<bool(UEdGraph*,int32)> Visit=[&](UEdGraph* Graph,int32 Depth)
    {
        if(!Graph||!Hold(Graph)||Graph->GetTypedOuter<UBlueprint>()!=Blueprint||Depth>32||++GraphCount>MaxGraphs||Path.Contains(Graph)||Graph->Nodes.Num()>MaxNodes||Graph->SubGraphs.Num()>MaxGraphs)
        { Incomplete(TEXT("Current graph tree ownership/depth/count failed"));return false; }
        Path.Add(Graph);auto G=MakeShared<FJsonObject>();G->SetStringField(TEXT("path"),Graph->GetPathName());G->SetStringField(TEXT("guid"),Graph->GraphGuid.ToString());TArray<TSharedPtr<FJsonValue>> NodeRows;
        for(UEdGraphNode* Node:Graph->Nodes)
        {
            if(!Node) { continue; }if(!Hold(Node)||Node->GetGraph()!=Graph||++NodeCount>MaxNodes||Node->Pins.Num()>MaxPins)
            { Incomplete(TEXT("Current graph node ownership/count failed"));return false; }
            auto N=MakeShared<FJsonObject>();N->SetStringField(TEXT("path"),Node->GetPathName());N->SetStringField(TEXT("class"),Node->GetClass()->GetPathName());N->SetStringField(TEXT("guid"),Node->NodeGuid.ToString());N->SetNumberField(TEXT("x"),Node->NodePosX);N->SetNumberField(TEXT("y"),Node->NodePosY);
            TArray<TSharedPtr<FJsonValue>> PinRows;
            for(const UEdGraphPin* Pin:Node->Pins)
            {
                if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node||++PinCount>MaxPins)
                { Incomplete(TEXT("Current primary pin ownership/count failed"));return false; }
                Active->Pins.Add(Pin);if(!SafeText(Pin->DefaultValue,65536)||!Hold(Pin->DefaultObject.Get())) { Incomplete(TEXT("Current pin default unavailable or unbounded"));return false; }
                auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("id"),Pin->PinId.ToString());P->SetStringField(TEXT("name"),Pin->PinName.ToString());P->SetNumberField(TEXT("direction"),static_cast<int32>(Pin->Direction));P->SetStringField(TEXT("default"),Pin->DefaultValue);P->SetStringField(TEXT("default_object"),GetPathNameSafe(Pin->DefaultObject.Get()));P->SetNumberField(TEXT("links"),Pin->LinkedTo.Num());PinRows.Add(MakeShared<FJsonValueObject>(P));
            }
            N->SetArrayField(TEXT("pins"),PinRows);NodeRows.Add(MakeShared<FJsonValueObject>(N));
        }
        G->SetArrayField(TEXT("nodes"),NodeRows);GraphRows.Add(MakeShared<FJsonValueObject>(G));
        for(UEdGraph* Child:Graph->SubGraphs) { if(!Visit(Child,Depth+1)) { return false; } }Path.Remove(Graph);return true;
    };
    auto Roots=[&](const TArray<TObjectPtr<UEdGraph>>& Graphs)
    { if(Graphs.Num()>MaxGraphs) { Incomplete(TEXT("Blueprint root graph budget exceeded"));return false; }for(UEdGraph* Graph:Graphs) { if(Graph&&!Visit(Graph,0)) { return false; } }return true; };
    if(Blueprint->ImplementedInterfaces.Num()>MaxGraphs) { Incomplete(TEXT("Blueprint interface graph budget exceeded"));return false; }
    if(!Roots(Blueprint->UbergraphPages)||!Roots(Blueprint->FunctionGraphs)||!Roots(Blueprint->MacroGraphs)||!Roots(Blueprint->DelegateSignatureGraphs)) { return false; }
    for(const auto& Interface:Blueprint->ImplementedInterfaces) { if(!Roots(Interface.Graphs)) { return false; } }
    J->SetArrayField(TEXT("graphs"),GraphRows);if(Observation) { *Observation=J; }return !Active->bIncomplete;
}
bool ParentSafe(UClass* Parent)
{
    if(!Parent||!Hold(Parent)) { return false; }TSet<const UClass*> Seen;int32 Depth=0;
    for(UClass* Class=Parent;Class;Class=Class->GetSuperClass())
    {
        if(++Depth>64||Seen.Contains(Class)||!Hold(Class)||!Hold(Class->GetOutermost())) { Incomplete(TEXT("Parent class lineage unavailable/cyclic/deep"));return false; }Seen.Add(Class);
        const FString Package=Class->GetOutermost()->GetName();if(Package.StartsWith(TEXT("/Script/UnrealMCP"))||Package.StartsWith(TEXT("/Script/CubelessMCPUE58Editor")))
        { Incomplete(TEXT("Editor bridge dependency is not an admitted sample runtime parent"));return false; }
        if(Class->ClassGeneratedBy)
        { auto* Owner=Cast<UBlueprint>(Class->ClassGeneratedBy);if(!Owner||!BlueprintSafe(Owner)||(Owner->GeneratedClass.Get()!=Class&&Owner->SkeletonGeneratedClass.Get()!=Class))
          { Incomplete(TEXT("Loaded parent generated owner/class identity required"));return false; } }
        else if(!Class->IsNative()) { Incomplete(TEXT("Nonnative parent without current generated owner"));return false; }
    }
    return Parent->IsChildOf(UAnimInstance::StaticClass());
}
bool SourceReady()
{
    auto& C=*Active;if(!C.bSourceReady) { return true; }
    if(!C.Skeleton||!Hold(C.Skeleton)||!RefSkeletonSafe(C.Skeleton->GetReferenceSkeleton())||!MeshSafe(C.Mesh)||!MeshSafe(C.PreviewMesh)||!ParentSafe(C.Parent)) { return false; }
    if((C.Mesh&&C.Mesh->GetSkeleton()!=C.Skeleton)||(C.PreviewMesh&&C.PreviewMesh->GetSkeleton()!=C.Skeleton))
    { Incomplete(TEXT("Source skeleton/mesh identity changed after callback"));return false; }return !C.bIncomplete;
}
FString SourceObservation()
{
    if(!SourceReady()) { return FString(); }auto& C=*Active;auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
    for(UObject* Object:{static_cast<UObject*>(C.Skeleton),static_cast<UObject*>(C.Mesh),static_cast<UObject*>(C.PreviewMesh)})
    { if(!Object) { continue; }if(!Hold(Object->GetOutermost())) { return FString(); }auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("path"),Object->GetPathName());O->SetStringField(TEXT("class"),Object->GetClass()->GetPathName());O->SetBoolField(TEXT("dirty"),Object->GetOutermost()->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(O)); }
    J->SetArrayField(TEXT("source_assets"),Rows);J->SetStringField(TEXT("skeleton_guid"),C.Skeleton->GetGuid().ToString());J->SetStringField(TEXT("parent_class"),C.Parent->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Bones;const auto& Ref=C.Skeleton->GetReferenceSkeleton();const auto& Info=Ref.GetRefBoneInfo();const auto& Poses=Ref.GetRefBonePose();
    for(int32 I=0;I<Info.Num();++I) { auto B=MakeShared<FJsonObject>();B->SetStringField(TEXT("name"),Info[I].Name.ToString());B->SetNumberField(TEXT("parent"),Info[I].ParentIndex);B->SetStringField(TEXT("pose"),Poses[I].ToString());Bones.Add(MakeShared<FJsonValueObject>(B)); }J->SetArrayField(TEXT("bones"),Bones);
    if(auto* Owner=Cast<UBlueprint>(C.Parent->ClassGeneratedBy)) { TSharedPtr<FJsonObject> Parent; if(!BlueprintSafe(Owner,&Parent)) { return FString(); }J->SetObjectField(TEXT("parent_blueprint"),Parent); }
    const FString Text=Serialize(J);if(Text.Len()>MaxOutput) { Incomplete(TEXT("Source observations exceed bounded carrier"));return FString(); }return Text;
}
bool PrepareSources(USkeleton* Skeleton,USkeletalMesh* Mesh,USkeletalMesh* PreviewMesh,UClass* Parent,const FString& Target)
{
    auto& C=*Active;C.Skeleton=Skeleton;C.Mesh=Mesh;C.PreviewMesh=PreviewMesh;C.Parent=Parent;C.ExpectedTargetPackage=Target;C.bSourceReady=true;
    if(!SourceReady()) { return false; }C.SourceBefore=SourceObservation();C.Result.Data->SetStringField(TEXT("source_before_json"),C.SourceBefore);return !C.SourceBefore.IsEmpty()&&!C.bIncomplete;
}
bool BeforeWriter(const FString& Target)
{
    if(Active->bIncomplete||!SourceReady()||!SafeText(Target,NAME_SIZE-1)||!SafeText(NormalizeAssetObjectPathForLoad(Target),NAME_SIZE-1)) { return false; }
    for(UObject* Source:{static_cast<UObject*>(Active->Skeleton),static_cast<UObject*>(Active->Mesh),static_cast<UObject*>(Active->PreviewMesh)})
    { if(Source&&Source->GetOutermost()->GetName().Equals(Target,ESearchCase::IgnoreCase)) { Incomplete(TEXT("Target/source package collision refused before writer"));return false; } }
    for(UClass* Class=Active->Parent;Class;Class=Class->GetSuperClass())
    { if(Class->ClassGeneratedBy&&Class->ClassGeneratedBy->GetOutermost()->GetName().Equals(Target,ESearchCase::IgnoreCase)) { Incomplete(TEXT("Target/parent source package collision refused before writer"));return false; } }
    const FString Current=SourceObservation();if(Current.IsEmpty()||!Current.Equals(Active->SourceBefore,ESearchCase::CaseSensitive)) { Incomplete(TEXT("Source observations changed before writer; accepted prefix retained"));return false; }return true;
}
bool TargetSafe(UAnimBlueprint* Blueprint)
{
    auto& C=*Active;if(!Blueprint||!BlueprintSafe(Blueprint)||Blueprint->GetOutermost()->GetName()!=C.ExpectedTargetPackage||Blueprint->ParentClass.Get()!=C.Parent||Blueprint->TargetSkeleton.Get()!=C.Skeleton||Blueprint->bIsTemplate)
    { Incomplete(TEXT("Current target package/parent/skeleton/template identity failed"));return false; }
    if(FindObject<UObject>(nullptr,*Blueprint->GetPathName())!=Blueprint) { Incomplete(TEXT("Current loaded target lookup disagrees"));return false; }
    for(UClass* Class:{Blueprint->GeneratedClass.Get(),Blueprint->SkeletonGeneratedClass.Get()})
    { if(Class&&!Class->IsChildOf(C.Parent)) { Incomplete(TEXT("Current generated target lineage changed"));return false; } }
    C.Target=Blueprint;return SourceReady();
}
USkeletalMesh* PreviewReadback(UAnimBlueprint* Blueprint)
{
    if(!TargetSafe(Blueprint)) { return nullptr; }auto* Property=FindFProperty<FSoftObjectProperty>(UAnimBlueprint::StaticClass(),TEXT("PreviewSkeletalMesh"));
    const int32 Size=Blueprint->GetClass()->GetPropertiesSize();
    if(!Property||Property->GetOwnerStruct()!=UAnimBlueprint::StaticClass()||Property->ArrayDim!=1||Property->PropertyClass!=USkeletalMesh::StaticClass()||Property->GetOffset_ForInternal()<0||Property->GetSize()!=sizeof(FSoftObjectPtr)||Property->GetOffset_ForInternal()>Size||Property->GetSize()>Size-Property->GetOffset_ForInternal())
    { Incomplete(TEXT("Current preview soft-property layout unavailable"));return nullptr; }
    const auto* Stored=Property->ContainerPtrToValuePtr<FSoftObjectPtr>(Blueprint);auto* Current=Cast<USkeletalMesh>(Stored->Get());
    if((!Stored->IsNull()&&!Current)||!Hold(Current)) { Incomplete(TEXT("Non-null unresolved current preview before SDK getter"));return nullptr; }
    Count(TEXT("preview_getter_calls"));auto* Result=Blueprint->GetPreviewMesh(false);
    if(!Hold(Result)||Result!=Current||Result!=Active->PreviewMesh) { Incomplete(TEXT("Current preview readback differs from admitted source"));return nullptr; }return Result;
}
UObject* LoadAssetObjectForRuntimeProbe(const FString& Path)
{
    const FString Normalized=NormalizeAssetObjectPathForLoad(Path);if(Normalized.IsEmpty()) { return nullptr; }
    if(!SafeText(Normalized,NAME_SIZE-1)) { Incomplete(TEXT("Asset load path exceeds FName/NUL scope"));return nullptr; }
    Count(TEXT("asset_load_calls"));UObject* Object=UEditorAssetLibrary::LoadAsset(Normalized);Count(TEXT("asset_load_returns"));
    if(!Object) { Count(TEXT("static_asset_load_calls"));Object=StaticLoadObject(UObject::StaticClass(),nullptr,*Normalized);Count(TEXT("static_asset_load_returns")); }
    return Hold(Object)?Object:nullptr;
}
UClass* LoadClassForPin(const FString& Path)
{
    const FString Normalized=NormalizeAssetObjectPathForLoad(Path);const FString EnginePath=FString::Printf(TEXT("/Script/Engine.%s"),*Path);
    if(!SafeText(Normalized,NAME_SIZE-1)||!SafeText(Path,NAME_SIZE-1)||!SafeText(EnginePath,NAME_SIZE-1)) { Incomplete(TEXT("Parent load path exceeds FName/NUL scope"));return nullptr; }
    Count(TEXT("parent_class_load_calls"));UClass* Class=LoadObject<UClass>(nullptr,*Normalized);Count(TEXT("parent_class_load_returns"));
    if(!Class) { Count(TEXT("parent_class_find_calls"));Class=FindFirstObject<UClass>(*Path); }
    if(!Class) { Count(TEXT("parent_class_load_calls"));Class=LoadObject<UClass>(nullptr,*EnginePath);Count(TEXT("parent_class_load_returns")); }
    return Hold(Class)?Class:nullptr;
}
bool Exists(const FString& Path)
{ if(!SafeText(NormalizeAssetObjectPathForLoad(Path),NAME_SIZE-1)) { Incomplete(TEXT("Target existence path exceeds FName bound"));return false; }Count(TEXT("asset_exists_calls"));return UEditorAssetLibrary::DoesAssetExist(Path); }
bool Delete(const FString& Path)
{
    if(!BeforeWriter(Path)) { return false; }Writer(TEXT("asset_delete_calls"));const bool Result=UEditorAssetLibrary::DeleteAsset(Path);Count(TEXT("asset_delete_returns"));Active->Result.bChanged|=Result;
    Active->Result.Data->SetBoolField(TEXT("delete_return"),Result);Active->bUnprovedWriter|=!Result;return Result;
}
void MakeDirectory(const FString& Path)
{
    if(!BeforeWriter(Active->ExpectedTargetPackage)||!SafeText(Path,NAME_SIZE-1)) { return; }Writer(TEXT("make_directory_calls"));const bool Result=UEditorAssetLibrary::MakeDirectory(Path);Active->Result.Data->SetBoolField(TEXT("make_directory_return"),Result);
}
UAnimBlueprintFactory* NewFactory()
{
    if(!BeforeWriter(Active->ExpectedTargetPackage)) { return nullptr; }Count(TEXT("factory_allocation_calls"));auto* Factory=Cast<UAnimBlueprintFactory>(NewObject<UFactory>(GetTransientPackage(),UAnimBlueprintFactory::StaticClass()));Count(TEXT("factory_allocation_returns"));
    if(!Factory||!Hold(Factory)) { Incomplete(TEXT("Current AnimBlueprint factory unavailable"));return nullptr; }return Factory;
}
UObject* CreateAsset(IAssetTools& Tools,const FString& Name,const FString& Directory,UClass* Class,UAnimBlueprintFactory* Factory)
{
    if(!Factory||!Hold(Factory)||!BeforeWriter(Active->ExpectedTargetPackage)||!SafeText(Name,NAME_SIZE-1)||!Hold(Class)||!ParentSafe(Active->Parent)) { return nullptr; }
    if(!FKismetEditorUtilities::CanCreateBlueprintOfClass(Active->Parent)) { Incomplete(TEXT("Current parent is not admitted by public Blueprint factory"));return nullptr; }
    if(Factory->ParentClass.Get()!=Active->Parent||Factory->TargetSkeleton.Get()!=Active->Skeleton||Factory->PreviewSkeletalMesh.Get()!=Active->PreviewMesh||Factory->BlueprintType!=BPTYPE_Normal||Factory->bTemplate) { Incomplete(TEXT("Actual factory fields differ from admitted original configuration"));return nullptr; }
    Writer(TEXT("asset_factory_calls"));Active->Result.Data->SetBoolField(TEXT("factory_implicit_blueprint_compile_possible"),true);
    UObject* Object=Tools.CreateAsset(Name,Directory,Class,Factory);Count(TEXT("asset_factory_returns"));auto* Blueprint=Cast<UAnimBlueprint>(Object);
    if(!Hold(Object)) { Active->bUnprovedWriter=true;return nullptr; }
    Active->Result.Data->SetStringField(TEXT("factory_return_path"),GetPathNameSafe(Object));if(Blueprint) { Active->Result.bChanged=true; }
    if(!Blueprint||!TargetSafe(Blueprint)) { Active->bUnprovedWriter=true;return nullptr; }return Object;
}
bool DiagnosticSafe(const TSharedRef<FTokenizedMessage>& Message,const FCompilerResultsLog& Log)
{
    auto ObjectSafe=[&](const UObject* Object)
    { if(!Object) { return true; }if(!Hold(Object)) { return false; }if(const auto* Node=Cast<UEdGraphNode>(Object))
      { auto* Graph=Node->GetGraph();if(!Graph||!Hold(Graph)||!Graph->Nodes.Contains(Node)) { return false; }const FString Package=Node->GetClass()->GetOutermost()->GetName();return Node->GetClass()->IsNative()&&(Package==TEXT("/Script/Engine")||Package==TEXT("/Script/AnimGraph")||Package==TEXT("/Script/BlueprintGraph")); }return true; };
    if(Message->GetMessageTokens().Num()>4096) { return false; }
    auto TokenSafe=[&](const IMessageToken& Token)
    { if(Token.GetType()==EMessageToken::EdGraph) { const auto& T=static_cast<const FEdGraphToken&>(Token);const auto* Pin=Log.FindSourcePin(T.GetPin());if(Pin&&(!Active->Pins.Contains(Pin)||!ObjectSafe(Pin->GetOwningNodeUnchecked()))) { return false; }return ObjectSafe(Log.FindSourceObject(T.GetGraphObject())); }
      if(Token.GetType()==EMessageToken::Object) { const auto& T=static_cast<const FUObjectToken&>(Token);return ObjectSafe(Log.FindSourceObject(T.GetObject().Get())); }return true; };
    for(const auto& Token:Message->GetMessageTokens()) { if(!TokenSafe(Token.Get())) { return false; } }if(const auto Link=Message->GetMessageLink()) { if(!TokenSafe(*Link)) { return false; } }return true;
}
TSharedPtr<FJsonObject> AnimNodeProbeCompileBlueprint(UBlueprint* Blueprint)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("requested"),true);
    auto* Target=Cast<UAnimBlueprint>(Blueprint);if(!Target||!TargetSafe(Target)||!BeforeWriter(Active->ExpectedTargetPackage)) { J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),TEXT("Current compile target unavailable"));return J; }
    auto& R=Active->Result;R.bCompileAttempted=true;R.Stage=TEXT("compile");Count(TEXT("full_compile_calls"));FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::None,&Log);Count(TEXT("full_compile_returns"));
    R.Compiled=Loaded(Blueprint->GeneratedClass.Get())&&Log.NumErrors==0&&(Blueprint->Status==BS_UpToDate||Blueprint->Status==BS_UpToDateWithWarnings);
    auto Actual=MakeShared<FJsonObject>();Actual->SetNumberField(TEXT("errors"),Log.NumErrors);Actual->SetNumberField(TEXT("warnings"),Log.NumWarnings);Actual->SetNumberField(TEXT("status"),static_cast<int32>(Blueprint->Status));Actual->SetBoolField(TEXT("healthy"),R.Compiled.GetValue());
    R.Data->SetObjectField(TEXT("compiler_result"),Actual);J->SetBoolField(TEXT("success"),Blueprint->GeneratedClass!=nullptr);
    R.Data->SetBoolField(TEXT("legacy_generated_class_compile_predicate"),Blueprint->GeneratedClass!=nullptr);
    Active->Pins.Reset();if(!TargetSafe(Target)) { return J; }TArray<TSharedPtr<FJsonValue>> Diagnostics;
    J->SetStringField(TEXT("blueprint"),Blueprint->GetPathName());J->SetStringField(TEXT("generated_class"),GetPathNameSafe(Blueprint->GeneratedClass.Get()));
    if(Log.Messages.Num()>4096) { Incomplete(TEXT("Compiler diagnostics exceed bounded observation"));return J; }
    for(const auto& Message:Log.Messages)
    { if(!DiagnosticSafe(Message,Log)) { Incomplete(TEXT("Compiler diagnostic source context unavailable"));break; }Diagnostics.Add(MakeShared<FJsonValueObject>(CompilerDiagnostic(Message,Log))); }
    Actual->SetArrayField(TEXT("diagnostics"),Diagnostics);return J;
}
bool Save(const FString& Path,bool OnlyIfDirty)
{
    auto* Target=Active->Target;if(!Target||!TargetSafe(Target)||!BeforeWriter(Path)) { return false; }
    const FString ObjectPath=NormalizeAssetObjectPathForLoad(Path);if(Target->GetPathName()!=ObjectPath||FindObject<UObject>(nullptr,*ObjectPath)!=Target)
    { Incomplete(TEXT("Original save path no longer resolves to exact current target"));return false; }
    auto& R=Active->Result;R.bSaveAttempted=true;R.Stage=TEXT("force_save");Count(TEXT("force_save_calls"));const bool Result=UEditorAssetLibrary::SaveAsset(Path,OnlyIfDirty);Count(TEXT("force_save_returns"));R.bSaved=Result;R.Data->SetBoolField(TEXT("actual_save_return"),Result);if(!Result) { Active->bUnprovedWriter=true;R.Data->SetBoolField(TEXT("save_failure_state_unverified"),true); }return Result;
}

TSharedPtr<FJsonObject> RunOriginal(const TSharedPtr<FJsonObject>& Params)
{
    FString TargetAnimBPPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_anim_bp_path")));
    if (TargetAnimBPPath.IsEmpty())
    {
        TargetAnimBPPath = NormalizePackagePathForPostProcessVariant(GetStringParam(Params, TEXT("target_blueprint_path")));
    }
    if (TargetAnimBPPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Missing 'target_anim_bp_path' parameter"));
    }

    FString PathError;
    if (!IsValidGamePackagePathForPostProcessVariant(TargetAnimBPPath, PathError))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Invalid target_anim_bp_path. %s"), *PathError));
    }
    if (!IsAllowedAnimStateMachineSampleTargetPath(TargetAnimBPPath))
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Refusing to create AnimBP sample outside /Game/_MCP_Sample or /Game/_MCP_Temp: %s"),
            *TargetAnimBPPath));
    }

    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false) || GetBoolParam(Params, TEXT("validate_only"), false);
    const bool bOverwriteExisting = GetBoolParam(Params, TEXT("overwrite_existing"), false);
    const bool bCompile = GetBoolParam(Params, TEXT("compile"), true);
    const bool bSave = GetBoolParam(Params, TEXT("save_asset"), GetBoolParam(Params, TEXT("save"), false));

    const FString SkeletonPath = GetStringParam(Params, TEXT("skeleton_path"));
    const FString SkeletalMeshPath = GetStringParam(Params, TEXT("skeletal_mesh_path"));
    const FString PreviewSkeletalMeshPath = GetStringParam(Params, TEXT("preview_skeletal_mesh_path"));
    if (SkeletonPath.IsEmpty() && SkeletalMeshPath.IsEmpty())
    {
        return CreateErrorResponse(TEXT("Provide either 'skeleton_path' or 'skeletal_mesh_path'"));
    }

    USkeletalMesh* SourceSkeletalMesh = nullptr;
    if (!SkeletalMeshPath.IsEmpty())
    {
        SourceSkeletalMesh = Cast<USkeletalMesh>(LoadAssetObjectForRuntimeProbe(SkeletalMeshPath));
        if (!SourceSkeletalMesh)
        {
            return CreateErrorResponse(FString::Printf(TEXT("SkeletalMesh not found: %s"), *SkeletalMeshPath));
        }
    }

    USkeleton* TargetSkeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        TargetSkeleton = Cast<USkeleton>(LoadAssetObjectForRuntimeProbe(SkeletonPath));
        if (!TargetSkeleton)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));
        }
    }
    else if (SourceSkeletalMesh)
    {
        TargetSkeleton = SourceSkeletalMesh->GetSkeleton();
    }

    if (!TargetSkeleton)
    {
        return CreateErrorResponse(TEXT("Failed to resolve target Skeleton"));
    }
    if(!Hold(TargetSkeleton)||!MeshSafe(SourceSkeletalMesh)) { return CreateErrorResponse(Active->Result.Error); }
    if (SourceSkeletalMesh && SourceSkeletalMesh->GetSkeleton() != TargetSkeleton)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("SkeletalMesh skeleton does not match explicit Skeleton. mesh_skeleton='%s' skeleton='%s'"),
            SourceSkeletalMesh->GetSkeleton() ? *SourceSkeletalMesh->GetSkeleton()->GetPathName() : TEXT("<none>"),
            *TargetSkeleton->GetPathName()));
    }

    USkeletalMesh* PreviewSkeletalMesh = SourceSkeletalMesh;
    if (!PreviewSkeletalMeshPath.IsEmpty())
    {
        PreviewSkeletalMesh = Cast<USkeletalMesh>(LoadAssetObjectForRuntimeProbe(PreviewSkeletalMeshPath));
        if (!PreviewSkeletalMesh)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Preview SkeletalMesh not found: %s"), *PreviewSkeletalMeshPath));
        }
    }
    if(!Hold(TargetSkeleton)||!MeshSafe(SourceSkeletalMesh)||!MeshSafe(PreviewSkeletalMesh)) { return CreateErrorResponse(Active->Result.Error); }
    if (PreviewSkeletalMesh && PreviewSkeletalMesh->GetSkeleton() != TargetSkeleton)
    {
        return CreateErrorResponse(FString::Printf(
            TEXT("Preview SkeletalMesh skeleton does not match target Skeleton. preview_skeleton='%s' skeleton='%s'"),
            PreviewSkeletalMesh->GetSkeleton() ? *PreviewSkeletalMesh->GetSkeleton()->GetPathName() : TEXT("<none>"),
            *TargetSkeleton->GetPathName()));
    }

    UClass* ParentAnimInstanceClass = UAnimInstance::StaticClass();
    FString ParentClassPath = GetStringParam(Params, TEXT("parent_class_path"), TEXT("/Script/Engine.AnimInstance"));
    if (!ParentClassPath.IsEmpty())
    {
        UClass* LoadedParentClass = LoadClassForPin(ParentClassPath);
        if (!LoadedParentClass)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Failed to load parent_class_path: %s"), *ParentClassPath));
        }
        if (!LoadedParentClass->IsChildOf(UAnimInstance::StaticClass()))
        {
            return CreateErrorResponse(FString::Printf(TEXT("parent_class_path is not a UAnimInstance subclass: %s"), *ParentClassPath));
        }
        ParentAnimInstanceClass = LoadedParentClass;
    }

    if(!PrepareSources(TargetSkeleton,SourceSkeletalMesh,PreviewSkeletalMesh,ParentAnimInstanceClass,TargetAnimBPPath)) { return CreateErrorResponse(Active->Result.Error); }
    const FString TargetDirectory = FPackageName::GetLongPackagePath(TargetAnimBPPath);
    const FString TargetAssetName = FPackageName::GetShortName(TargetAnimBPPath);
    const bool bTargetExists = Exists(TargetAnimBPPath);

    UPackage* SkeletonPackage = TargetSkeleton->GetOutermost();
    UPackage* MeshPackage = SourceSkeletalMesh ? SourceSkeletalMesh->GetOutermost() : nullptr;
    UPackage* PreviewMeshPackage = PreviewSkeletalMesh ? PreviewSkeletalMesh->GetOutermost() : nullptr;
    const bool bSkeletonDirtyBefore = SkeletonPackage && SkeletonPackage->IsDirty();
    const bool bMeshDirtyBefore = MeshPackage && MeshPackage->IsDirty();
    const bool bPreviewMeshDirtyBefore = PreviewMeshPackage && PreviewMeshPackage->IsDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("command"), TEXT("create_anim_blueprint_sample"));
    ResultObj->SetBoolField(TEXT("sample_only"), true);
    ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObj->SetBoolField(TEXT("source_modified"), false);
    ResultObj->SetBoolField(TEXT("source_assets_modified"), false);
    ResultObj->SetStringField(TEXT("target_anim_bp_path"), TargetAnimBPPath);
    ResultObj->SetStringField(TEXT("target_directory"), TargetDirectory);
    ResultObj->SetStringField(TEXT("target_asset_name"), TargetAssetName);
    ResultObj->SetBoolField(TEXT("target_exists"), bTargetExists);
    ResultObj->SetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);
    ResultObj->SetStringField(TEXT("skeleton_path"), TargetSkeleton->GetPathName());
    ResultObj->SetStringField(TEXT("skeletal_mesh_path"), SourceSkeletalMesh ? SourceSkeletalMesh->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("preview_skeletal_mesh_path"), PreviewSkeletalMesh ? PreviewSkeletalMesh->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("parent_class_path"), ParentAnimInstanceClass->GetPathName());
    ResultObj->SetBoolField(TEXT("compile_requested"), bCompile);
    ResultObj->SetBoolField(TEXT("save_requested"), bSave);
    ResultObj->SetBoolField(TEXT("skeleton_dirty_before"), bSkeletonDirtyBefore);
    ResultObj->SetBoolField(TEXT("skeletal_mesh_dirty_before"), bMeshDirtyBefore);
    ResultObj->SetBoolField(TEXT("preview_skeletal_mesh_dirty_before"), bPreviewMeshDirtyBefore);

    if (bDryRun)
    {
        ResultObj->SetBoolField(TEXT("created"), false);
        ResultObj->SetBoolField(TEXT("overwrote_existing"), false);
        ResultObj->SetBoolField(TEXT("target_asset_saved"), false);
        ResultObj->SetStringField(TEXT("runtime_note"), TEXT("Dry run only: no AnimBP was created, compiled, or saved."));
        return ResultObj;
    }

    if (bTargetExists)
    {
        if (!bOverwriteExisting)
        {
            TSharedPtr<FJsonObject> ErrorObj = CreateErrorResponse(FString::Printf(TEXT("Target AnimBP already exists: %s"), *TargetAnimBPPath));
            ErrorObj->SetBoolField(TEXT("target_exists"), true);
            ErrorObj->SetStringField(TEXT("target_anim_bp_path"), TargetAnimBPPath);
            ErrorObj->SetBoolField(TEXT("source_modified"), false);
            return ErrorObj;
        }
        if (!Delete(TargetAnimBPPath))
        {
            return CreateErrorResponse(FString::Printf(TEXT("Failed to delete existing target AnimBP: %s"), *TargetAnimBPPath));
        }
    }

    if (!TargetDirectory.IsEmpty())
    {
        MakeDirectory(TargetDirectory);
    }

    UAnimBlueprintFactory* Factory = NewFactory();
    if(!Factory) { return CreateErrorResponse(Active->Result.Error); }
    Factory->BlueprintType = BPTYPE_Normal;
    Factory->ParentClass = ParentAnimInstanceClass;
    Factory->TargetSkeleton = TargetSkeleton;
    Factory->PreviewSkeletalMesh = PreviewSkeletalMesh;
    Factory->bTemplate = false;

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* CreatedAsset = CreateAsset(AssetToolsModule.Get(),TargetAssetName,TargetDirectory,UAnimBlueprint::StaticClass(),Factory);
    UAnimBlueprint* CreatedAnimBlueprint = Cast<UAnimBlueprint>(CreatedAsset);
    if (!CreatedAnimBlueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to create AnimBlueprint sample: %s"), *TargetAnimBPPath));
    }

    TSharedPtr<FJsonObject> CompileResult = MakeShared<FJsonObject>();
    CompileResult->SetBoolField(TEXT("requested"), bCompile);
    if (bCompile)
    {
        CompileResult = AnimNodeProbeCompileBlueprint(CreatedAnimBlueprint);
    }

    bool bTargetSaved = false;
    if (bSave)
    {
        bTargetSaved = Save(TargetAnimBPPath, false);
    }

    if(!TargetSafe(CreatedAnimBlueprint)) { return CreateErrorResponse(Active->Result.Error); }
    ResultObj->SetBoolField(TEXT("created"), true);
    ResultObj->SetBoolField(TEXT("overwrote_existing"), bTargetExists && bOverwriteExisting);
    ResultObj->SetBoolField(TEXT("target_asset_saved"), bTargetSaved);
    ResultObj->SetStringField(TEXT("created_asset_path"), CreatedAnimBlueprint->GetPathName());
    ResultObj->SetStringField(TEXT("generated_class_path"), CreatedAnimBlueprint->GeneratedClass ? CreatedAnimBlueprint->GeneratedClass->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("target_skeleton_readback"), CreatedAnimBlueprint->TargetSkeleton ? CreatedAnimBlueprint->TargetSkeleton->GetPathName() : FString());
    USkeletalMesh* PreviewMeshReadback = PreviewReadback(CreatedAnimBlueprint);
    ResultObj->SetStringField(TEXT("preview_skeletal_mesh_readback"), PreviewMeshReadback ? PreviewMeshReadback->GetPathName() : FString());
    ResultObj->SetObjectField(TEXT("compile_result"), CompileResult);
    ResultObj->SetBoolField(TEXT("skeleton_dirty_after"), SkeletonPackage && SkeletonPackage->IsDirty());
    ResultObj->SetBoolField(TEXT("skeletal_mesh_dirty_after"), MeshPackage && MeshPackage->IsDirty());
    ResultObj->SetBoolField(TEXT("preview_skeletal_mesh_dirty_after"), PreviewMeshPackage && PreviewMeshPackage->IsDirty());
    ApplySampleCompileSaveOutcome(ResultObj, CompileResult, bCompile, bSave, bTargetSaved);
    return ResultObj;
}

FResult Finish(FContext& Context)
{
    auto& R=Context.Result;
    R.Data->SetBoolField(TEXT("observed_changed_prefix"),R.bChanged);
    R.Data->SetBoolField(TEXT("unproved_writer_return"),Context.bUnprovedWriter);
    if(Context.bIncomplete||Context.bUnprovedWriter) { R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false; }
    const FString Text=Serialize(R.Data);if(Text.Len()>MaxOutput)
    { R.Data->RemoveField(TEXT("source_before_json"));R.Data->RemoveField(TEXT("source_after_json"));
      if(Serialize(R.Data).Len()>MaxOutput) { const TSharedPtr<FJsonObject>* Compiler=nullptr;if(R.Data->TryGetObjectField(TEXT("compiler_result"),Compiler)&&Compiler&&Compiler->IsValid()) { (*Compiler)->RemoveField(TEXT("diagnostics")); } }
      R.Data->SetBoolField(TEXT("oversized_observation_omitted"),true);R.Data->SetBoolField(TEXT("observation_incomplete"),true);R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;R.ErrorCode=TEXT("anim_blueprint_sample_output_incomplete");R.Error=TEXT("Observation exceeds bounded result carrier; original raw/compiler/save prefix retained"); }
    return R;
}
FResult Execute(const TSharedRef<FJsonObject>& Params)
{
    FResult R;R.bCompileRequested=GetBoolParam(Params,TEXT("compile"),true);R.bSaveRequested=GetBoolParam(Params,TEXT("save_asset"),false);
    R.Data->SetStringField(TEXT("operation"),TEXT("create_anim_blueprint_sample"));R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("factory_implicit_blueprint_compile_possible"),false);
    R.Data->SetBoolField(TEXT("SDK_callback_cache_and_constructor_effects_qualified"),true);R.Data->SetBoolField(TEXT("runtime_or_cook_verified"),false);
    auto Counters=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("static_asset_load_calls"),TEXT("static_asset_load_returns"),TEXT("parent_class_load_calls"),TEXT("parent_class_load_returns"),TEXT("parent_class_find_calls"),TEXT("asset_exists_calls"),TEXT("asset_delete_calls"),TEXT("asset_delete_returns"),TEXT("make_directory_calls"),TEXT("factory_allocation_calls"),TEXT("factory_allocation_returns"),TEXT("asset_factory_calls"),TEXT("asset_factory_returns"),TEXT("full_compile_calls"),TEXT("full_compile_returns"),TEXT("force_save_calls"),TEXT("force_save_returns"),TEXT("preview_getter_calls")}) { Counters->SetNumberField(Key,0); }R.Data->SetObjectField(TEXT("native_call_counts"),Counters);
    FContext Context(R);
    if(!IsEditorGameThread()||IsPlaySessionActive()||Active)
    { R.ErrorCode=TEXT("anim_blueprint_sample_editor_invalid");R.Error=TEXT("Idle nonreentrant Editor game thread required");return R; }
    struct FScope { explicit FScope(FContext& InContext) { Active=&InContext; }~FScope() { Active=nullptr; } } Scope(Context);
    for(const TCHAR* Key:{TEXT("target_anim_bp_path"),TEXT("skeleton_path"),TEXT("skeletal_mesh_path"),TEXT("preview_skeletal_mesh_path"),TEXT("parent_class_path")})
    { if(!SafeText(GetStringParam(Params,Key),NAME_SIZE-1)) { R.ErrorCode=TEXT("anim_blueprint_sample_path_invalid");R.Error=TEXT("Input path exceeds actual FName/NUL bound");return Finish(Context); } }
    FScopedTransaction Transaction(FText::FromString(TEXT("create_anim_blueprint_sample")));R.Stage=TEXT("original_composite");
    const auto Legacy=RunOriginal(Params);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("legacy_result_returned"),true);
    bool SourceStable=true;
    if(Context.bSourceReady)
    { const FString After=SourceObservation();R.Data->SetStringField(TEXT("source_after_json"),After);SourceStable=!After.IsEmpty()&&After.Equals(Context.SourceBefore,ESearchCase::CaseSensitive);R.Data->SetBoolField(TEXT("source_observation_unchanged"),SourceStable);R.Data->SetBoolField(TEXT("source_observation_is_total_purity_proof"),false);if(!After.IsEmpty()&&!After.Equals(Context.SourceBefore,ESearchCase::CaseSensitive)) { R.bChanged=true; } }
    const bool Dry=GetBoolParam(Params,TEXT("dry_run"),false)||GetBoolParam(Params,TEXT("validate_only"),false);
    bool TargetCurrent=Dry;
    if(Context.Target)
    {
        TargetCurrent=TargetSafe(Context.Target);if(TargetCurrent) { PreviewReadback(Context.Target);TargetCurrent=!Context.bIncomplete; }
        R.Data->SetBoolField(TEXT("current_target_matches_requested_sources"),TargetCurrent);
        if(TargetCurrent)
        { R.Data->SetStringField(TEXT("current_target_path"),GetPathNameSafe(Context.Target));R.Data->SetStringField(TEXT("current_generated_class"),GetPathNameSafe(Context.Target->GeneratedClass.Get()));R.Data->SetNumberField(TEXT("current_blueprint_status"),static_cast<int32>(Context.Target->Status)); }
    }
    R.bChangeStateKnown=!Context.bIncomplete&&!Context.bUnprovedWriter;
    bool LegacySuccess=false;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);
    if(!LegacySuccess)
    { R.ErrorCode=TEXT("anim_blueprint_sample_original_failure");Legacy->TryGetStringField(TEXT("error"),R.Error);if(R.Error.IsEmpty()) { R.Error=TEXT("Original compile or save completion failed; accepted prefix retained"); }R.Data->SetBoolField(TEXT("late_failure_no_automatic_cleanup"),R.bMutationAttempted);return Finish(Context); }
    const bool CompileOk=!R.bCompileRequested||(R.bCompileAttempted&&R.Compiled.IsSet()&&R.Compiled.GetValue());
    const bool SaveOk=!R.bSaveRequested||(R.bSaveAttempted&&R.bSaved);
    R.bPostconditionPassed=R.bChangeStateKnown&&SourceStable&&TargetCurrent&&(Dry||(CompileOk&&SaveOk));
    if(!R.bPostconditionPassed)
    { R.ErrorCode=TEXT("anim_blueprint_sample_current_postcondition_failed");R.Error=TEXT("Current source/target/compiler/save proof failed; original mutation prefix retained");return Finish(Context); }
    R.bSuccess=true;R.Stage=Dry?TEXT("dry_run"):TEXT("complete");return Finish(Context);
}
}
Variable::FResult CreateSample(const FString& TargetAnimBPPath,const FString& SkeletonPath,const FString& SkeletalMeshPath,const FString& PreviewSkeletalMeshPath,const FString& ParentClassPath,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly)
{
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("target_anim_bp_path"),TargetAnimBPPath);Params->SetStringField(TEXT("skeleton_path"),SkeletonPath);Params->SetStringField(TEXT("skeletal_mesh_path"),SkeletalMeshPath);Params->SetStringField(TEXT("preview_skeletal_mesh_path"),PreviewSkeletalMeshPath);Params->SetStringField(TEXT("parent_class_path"),ParentClassPath);
    Params->SetBoolField(TEXT("overwrite_existing"),bOverwriteExisting);Params->SetBoolField(TEXT("compile"),bCompile);Params->SetBoolField(TEXT("save_asset"),bSaveAsset);Params->SetBoolField(TEXT("dry_run"),bDryRun);Params->SetBoolField(TEXT("validate_only"),bValidateOnly);return Execute(Params);
}
}
