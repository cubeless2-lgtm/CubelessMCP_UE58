// Original blank native UEnvQuery creation; no query execution or editor-graph authoring.
#include "Compatibility/UE58/CubelessEnvQueryApi.h"
#include "Compatibility/UE58/CubelessBehaviorTreeApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_BlueprintBase.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::EnvQuery
{
namespace
{
constexpr int32 MaxNodes = 1000, MaxProperties = 128, MaxText = 262144, MaxPackages = 4096;
constexpr int64 MaxDiskBytes = 16 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = false; R.Code = Code; R.Error = Error; return false; }
FString Encode(const FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); R.Data->SetBoolField(TEXT("changed"), R.bChanged); }
    else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); R.Data->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown); J->SetBoolField(TEXT("postcondition_passed"), R.bPost);
    J->SetBoolField(TEXT("compile_requested"), false); J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false);
    R.Data->SetBoolField(TEXT("save_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("save_asset_requested"), R.bSaveRequested); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
void Init(FResult& R, const TCHAR* Operation, bool bSave)
{
    R.Operation = Operation; R.bSaveRequested = bSave;
    for (const TCHAR* K : { TEXT("asset_load_dispatched"), TEXT("implicit_postload_possible"), TEXT("asset_registry_publication_dispatched"),
        TEXT("asset_allocation_attempted"), TEXT("package_fully_load_dispatched"), TEXT("implicit_package_load_possible"), TEXT("created"), TEXT("asset_saved"), TEXT("graph_creation_attempted"),
        TEXT("option_creation_attempted"), TEXT("runtime_execution_verified"), TEXT("query_execution_dispatched"), TEXT("implicit_engine_work_measured") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetNumberField(TEXT("asset_load_count"), 0); R.Data->SetNumberField(TEXT("save_attempt_count"), 0); R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: public CoreUObject/AIModule/AssetRegistry/EditorAssetLibrary; no EQS Editor factory, official Toolset delegate or ServerRPC"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Fresh exact native UEnvQuery only; intrinsic PostInitProperties assigns QueryName, explicit asset resolution may run PostLoad. No options, generator/test authoring, editor graph creation, query execution or compile. Allocation/publication/save prefixes are retained without rollback or replay."));
    R.Data->SetStringField(TEXT("inspection_scope"), TEXT("Bounded exact native query/options/editor graph summary after initial resolution, including admitted loaded Blueprint generator and runtime native generator/test source chains. Custom/generated node values are not exported. Summary is not full nonblank dependency independence, generator/test correctness, graph-authoring or runtime proof."));
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("path"), O ? O->GetPathName() : TEXT("")); J->SetStringField(TEXT("class"), O ? O->GetClass()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("outer"), O && O->GetOuter() ? O->GetOuter()->GetPathName() : TEXT(""));
    J->SetStringField(TEXT("object_flags"), O ? FString::Printf(TEXT("%llu"), static_cast<uint64>(O->GetFlags())) : TEXT("0")); return J;
}
bool Native(UClass* C, const TCHAR* Package, bool bAbstract = false)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
    && (bAbstract || !C->HasAnyClassFlags(CLASS_Abstract)) && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)
    && C->GetOutermost()->GetName() == Package && !C->GetOutermost()->IsDirty(); }
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J,const TFunction<bool()>& Current={},FResult* Result=nullptr)
{
    if((Current&&!Current())||!Loaded(O))return false;
    TStrongObjectPtr<UObject> Keep;TStrongObjectPtr<UClass> KeepClass;UObject* ActualOuter=O->GetOuter();
    if(Current){Keep.Reset(O);KeepClass.Reset(O->GetClass());}
    auto DescriptorLost=[&]()
    {
        if(Result){Result->bKnown=false;Result->Data->SetBoolField(TEXT("eqs_followup_sdk_stopped"),true);if(!Result->Data->HasField(TEXT("eqs_first_mismatch"))){auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("stage"),Result->Stage);D->SetStringField(TEXT("object"),Loaded(O)?O->GetPathName():TEXT("retired exporter receiver"));D->SetStringField(TEXT("property"),TEXT("exporter_receiver_or_descriptor_routing"));D->SetStringField(TEXT("expected"),TEXT("held receiver/actual owner/class and copied field inventory"));D->SetStringField(TEXT("actual"),TEXT("changed at exporter return"));Result->Data->SetObjectField(TEXT("eqs_first_mismatch"),D);}Fail(*Result,TEXT("eqs_inspection_current_changed"),TEXT("EQS exporter receiver/descriptor changed; no followup exporter or retry."));}return false;
    };
    // WU: only the newly admitted query profile uses copied descriptor routing.
    // Generated/custom node properties are not exported by the summary path.
    struct FReadField{FProperty* P;FFieldVariant Owner;const FFieldClass* Class;FName Name;int32 Dim;};
    TArray<FReadField> Fields;
    if(Current){for(TFieldIterator<FProperty> It(O->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It){if(Fields.Num()>=MaxProperties)return false;Fields.Add({*It,It->GetOwnerVariant(),It->GetClass(),It->GetFName(),It->ArrayDim});}}
    auto FieldsCurrent=[&]()
    {
        if(!Current)return true;if(!Current())return false;
        if(!Loaded(O)||!Loaded(KeepClass.Get())||O->GetClass()!=KeepClass.Get()||O->GetOuter()!=ActualOuter)return DescriptorLost();
        if(O->HasAnyFlags(RF_ClassDefaultObject))
        {
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto CDO=KeepClass->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!CDO.IsResolved()||CDO!=O)return DescriptorLost();
        }
        int32 I=0;for(TFieldIterator<FProperty> It(O->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It,++I){if(!Fields.IsValidIndex(I)||*It!=Fields[I].P)return DescriptorLost();const auto& F=Fields[I];if(It->GetOwnerVariant()!=F.Owner||It->GetClass()!=F.Class||It->GetFName()!=F.Name||It->ArrayDim!=F.Dim)return DescriptorLost();}return I==Fields.Num()||DescriptorLost();
    };
    int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; } if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            if(!FieldsCurrent())return false;FString S; const void* V = P->ContainerPtrToValuePtr<void>(O, I);const bool Exported=P->ExportText_Direct(S,V,V,O,PPF_None);if(!FieldsCurrent()||!Exported)return false;
            Chars += S.Len(); if (Chars > MaxText) { return false; } Values.Add(MakeShared<FJsonValueString>(S));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName());
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(P->GetPropertyFlags()))); Row->SetNumberField(TEXT("array_dim"), P->ArrayDim); Row->SetArrayField(TEXT("values"), Values); J->SetObjectField(P->GetName(), Row);
    }
    return true;
}
bool NativeSource(TSharedRef<FJsonObject>& J,const TFunction<bool()>& Current={},FResult* Result=nullptr)
{
    if(Current&&!Current())return false;UClass* C = UEnvQuery::StaticClass(); const auto Id = Identity(C); for (const auto& P : Id->Values) { J->SetField(P.Key, P.Value); } J->SetBoolField(TEXT("native_identity_verified"), false);
    if (!Native(C, TEXT("/Script/AIModule")) || C->GetPathName() != TEXT("/Script/AIModule.EnvQuery") || C->IsEditorOnly()) { return false; }
    J->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(C->GetClassFlags()))); J->SetBoolField(TEXT("package_dirty"), C->GetOutermost()->IsDirty());
    UObject* CDO = C->GetDefaultObject(false);if(Current&&!Current())return false; J->SetBoolField(TEXT("cdo_exists"), CDO != nullptr);
    TStrongObjectPtr<UObject> KeepCDO;if(Current&&CDO){if(!Loaded(CDO)||CDO->GetClass()!=C)return false;KeepCDO.Reset(CDO);}
    if (CDO)
    {
        J->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); auto P = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("existing_cdo_properties"), P);
        if (!Loaded(CDO) || CDO->GetClass() != C || CDO->GetOuter() != C->GetOuter() || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, {}, P,Current,Result)) { return false; }
    }
    if(Current&&!Current())return false;J->SetBoolField(TEXT("native_identity_verified"), true); return true;
}
bool ProtectSource(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, bool bAllowLifecycleCDO, bool& bNewCDO)
{
    auto Expected = MakeShared<FJsonObject>(); for (const auto& P : Before->Values) { Expected->SetField(P.Key, P.Value); } bNewCDO = false;
    if (!Before->GetBoolField(TEXT("cdo_exists")) && After->GetBoolField(TEXT("cdo_exists")))
    {
        if (!bAllowLifecycleCDO) { return false; } Expected->SetBoolField(TEXT("cdo_exists"), true); Expected->SetObjectField(TEXT("existing_cdo"), After->GetObjectField(TEXT("existing_cdo")));
        Expected->SetObjectField(TEXT("existing_cdo_properties"), After->GetObjectField(TEXT("existing_cdo_properties"))); bNewCDO = true;
    }
    return Text(Expected) == Text(After);
}
bool Disk(const FString& PackageName, TSharedRef<FJsonObject>& J)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    J->SetStringField(TEXT("path"), Filename); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size));
    if (Size < 0) { J->SetStringField(TEXT("sha1"), TEXT("")); return true; } TArray<uint8> Bytes;
    if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Excluded, TSharedRef<FJsonObject>& J)
{
    int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Excluded) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetPathName(), It->IsDirty()); } return true;
}
bool ExactPath(const FString& Input, FString& Path, FResult& R)
{
    Path = FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd(); Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetLongPackageAssetName(Path); } FString PackageName, ObjectName;
    if (Path.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(Path) || !Path.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        || ObjectName != FPackageName::GetLongPackageAssetName(PackageName) || !(PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/Engine/"))))
    { return Fail(R, TEXT("eqs_asset_path_invalid"), TEXT("Use a bounded exact ordinary /Game or /Engine asset object path or package; other mounts/subobjects remain unsupported.")); } return true;
}
bool EditorState(FResult& R)
{ return (IsEditorGameThread() && !IsPlaySessionActive()) || Fail(R, TEXT("eqs_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); }
UEnvQuery* Resolve(const FString& Path, FResult& R)
{
    UObject* O = FindObject<UObject>(nullptr, *Path);
    if (!O) { R.Data->SetBoolField(TEXT("asset_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_postload_possible"), true); R.Data->SetNumberField(TEXT("asset_load_count"), 1); R.Data->SetStringField(TEXT("last_explicit_load_path"), Path); O = LoadObject<UObject>(nullptr, *Path); }
    auto* Q = Cast<UEnvQuery>(O);
    if (!Loaded(Q) || Q->GetClass() != UEnvQuery::StaticClass() || Q->GetPathName() != Path || Q->GetOuter() != Q->GetOutermost()
        || Q->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject) || Q->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor))
    { Fail(R, TEXT("eqs_native_asset_required"), TEXT("Requires an ordinary readable exact native UEnvQuery; custom/generated/transient query variants remain incomplete.")); return nullptr; } return Q;
}
bool QueryState(UEnvQuery* Q, TSharedRef<FJsonObject>& J,TFunction<bool()>* OutCurrent=nullptr,FResult* Result=nullptr)
{
    if (!Loaded(Q) || Q->GetClass() != UEnvQuery::StaticClass() || Q->GetOuter() != Q->GetOutermost() || Q->GetOptions().Num() > MaxNodes) { return false; }
    TFunction<bool()> Current;
    // Existing AIModule-only summaries retain their established exporter path.
    // New source classes need only the original structural summary, not arbitrary
    // generated/custom property exporters or generator/test virtual execution.
    bool ExtendedSources=false;
    for(UEnvQueryOption* O:Q->GetOptions())if(Loaded(O))
    {
        if(O->Generator.IsResolved()&&Loaded(O->Generator.Get())&&!Native(O->Generator->GetClass(),TEXT("/Script/AIModule")))ExtendedSources=true;
        for(const auto& Slot:O->Tests)if(Slot.IsResolved()&&Loaded(Slot.Get())&&!Native(Slot->GetClass(),TEXT("/Script/AIModule")))ExtendedSources=true;
    }
    if(ExtendedSources)
    {
        struct FReadObject{TStrongObjectPtr<UObject> Hold,Outer;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path;EObjectFlags Flags;bool Dirty;};
        struct FReadClass{UClass* Class;UClass* Super;UObject* GeneratedBy;UObject* CDO;EClassFlags Flags;};
        struct FReadBlueprint{UBlueprint* BP;UClass* Generated;UClass* Skeleton;UClass* Parent;UClass* SkeletonSuper;UObject* SkeletonCDO;EBlueprintStatus Status;EBlueprintType Type;};
        struct FReadOption{UEnvQueryOption* Option;UEnvQueryGenerator* Generator;TArray<TObjectPtr<UEnvQueryTest>> Tests;};
        auto Objects=MakeShared<TArray<FReadObject>>();auto Classes=MakeShared<TArray<FReadClass>>();auto Blueprints=MakeShared<TArray<FReadBlueprint>>();auto Options=MakeShared<TArray<FReadOption>>();
        auto AddObject=[&](UObject* O)
        {
            if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter())))return false;
            for(const auto& Old:*Objects)if(Old.Hold.Get()==O)return true;if(Objects->Num()>=MaxNodes*8)return false;
            FReadObject Role;Role.Hold.Reset(O);Role.Outer.Reset(O->GetOuter());Role.Class.Reset(O->GetClass());Role.Package.Reset(O->GetPackage());Role.Path=O->GetPathName();Role.Flags=O->GetFlags()&EObjectFlags(RF_Public|RF_Standalone|RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient);Role.Dirty=O->GetPackage()->IsDirty();Objects->Add(MoveTemp(Role));return true;
        };
        // Nullable CDO handles are read without GetDefaultObject/creation; actual
        // source/skeleton owners are captured rather than assumed to be a class.
        auto ExistingCDO=[](UClass* C,UObject*& Out)
        {
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto Handle=C->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(!Handle.IsResolved())return false;Out=Handle.Get();return !Out||(Loaded(Out)&&Out->GetClass()==C&&Out->HasAnyFlags(RF_ClassDefaultObject));
        };
        auto AddSourceChain=[&](UClass* First,bool Generator)
        {
            if(!Loaded(First)||First->HasAnyClassFlags(CLASS_Abstract)||!First->IsChildOf(Generator?UEnvQueryGenerator::StaticClass():UEnvQueryTest::StaticClass()))return false;
            const bool BlueprintGenerator=Generator&&First->IsChildOf(UEnvQueryGenerator_BlueprintBase::StaticClass());
            int32 Depth=0;TSet<UClass*> Seen;
            for(UClass* C=First;C;C=C->GetSuperClass())
            {
                if(++Depth>32||Seen.Contains(C)||!Loaded(C)||C->IsEditorOnly()||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||!C->ClassGeneratedBy.IsResolved())return false;Seen.Add(C);
                bool Already=false;for(const auto& Old:*Classes)Already|=Old.Class==C;if(Already)continue;
                UObject* CDO=nullptr;if(!ExistingCDO(C,CDO)||!AddObject(C)||(CDO&&!AddObject(CDO)))return false;
                if(C->HasAnyClassFlags(CLASS_Native))
                {
                    if(C->GetClass()!=UClass::StaticClass()||!C->GetPackage()->GetName().StartsWith(TEXT("/Script/"))||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||C->GetPackage()->IsDirty()||C->ClassGeneratedBy!=nullptr)return false;
                }
                else
                {
                    if(!BlueprintGenerator||C->GetClass()!=UBlueprintGeneratedClass::StaticClass())return false;
                    UObject* Source=C->ClassGeneratedBy.Get();if(!Loaded(Source))return false;UBlueprint* BP=Cast<UBlueprint>(Source);
                    if(!Loaded(BP)||BP->GetClass()!=UBlueprint::StaticClass()||BP->BlueprintType!=BPTYPE_Normal||BP->Status==BS_BeingCreated||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=C||BP->ParentClass.GetGCPtr()!=C->GetSuperClass())return false;
                    UClass* Skeleton=BP->SkeletonGeneratedClass.GetGCPtr().Get();UClass* Parent=BP->ParentClass.GetGCPtr().Get();UObject* SkeletonCDO=nullptr;
                    if(!Loaded(Skeleton)||!Loaded(Parent)||!Skeleton->ClassGeneratedBy.IsResolved()||Skeleton->ClassGeneratedBy!=BP||!ExistingCDO(Skeleton,SkeletonCDO)||!AddObject(BP)||!AddObject(Skeleton)||!AddObject(Parent)||(SkeletonCDO&&!AddObject(SkeletonCDO)))return false;
                    Blueprints->Add({BP,C,Skeleton,Parent,Skeleton->GetSuperClass(),SkeletonCDO,BP->Status.GetValue(),BP->BlueprintType.GetValue()});
                }
                Classes->Add({C,C->GetSuperClass(),C->ClassGeneratedBy.Get(),CDO,C->GetClassFlags()&EClassFlags(CLASS_Native|CLASS_CompiledFromBlueprint|CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface|CLASS_DefaultToInstanced|CLASS_EditInlineNew)});
            }
            return true;
        };
        if(!Loaded(GEditor)||!AddObject(GEditor))return false;UEditorEngine* Editor=GEditor;UWorld* World=Editor->GetEditorWorldContext().World();if(!Loaded(World)||!AddObject(World)||!AddObject(Q)||!Q->EdGraph.IsResolved())return false;
        const auto OptionSlots=Q->GetOptionsMutable();UEdGraph* Graph=Q->EdGraph.Get();TArray<TObjectPtr<UEdGraphNode>> Nodes;TObjectPtr<UClass> GraphSchema;
        for(const auto& Slot:OptionSlots)
        {
            if(!Slot.IsResolved())return false;UEnvQueryOption* O=Slot.Get();if(!Loaded(O)||O->GetClass()!=UEnvQueryOption::StaticClass()||O->GetTypedOuter<UEnvQuery>()!=Q||!AddObject(O)||!O->Generator.IsResolved()||O->Tests.Num()>MaxNodes)return false;
            UEnvQueryGenerator* G=O->Generator.Get();if(G&&(!Loaded(G)||G->GetTypedOuter<UEnvQuery>()!=Q||!AddObject(G)||!AddSourceChain(G->GetClass(),true)))return false;
            for(const auto& T:O->Tests)if(!T.IsResolved()||!Loaded(T.Get())||T->GetTypedOuter<UEnvQuery>()!=Q||!AddObject(T.Get())||!AddSourceChain(T->GetClass(),false))return false;
            Options->Add({O,G,O->Tests});
        }
        if(Graph)
        {
            if(!Loaded(Graph)||Graph->GetOuter()!=Q||!Native(Graph->GetClass(),TEXT("/Script/EnvironmentQueryEditor"))||Graph->Nodes.Num()>MaxNodes||!Graph->Schema.GetGCPtr().IsResolved()||!AddObject(Graph))return false;
            GraphSchema=Graph->Schema.GetGCPtr();if(GraphSchema&&!AddObject(GraphSchema.Get()))return false;Nodes=Graph->Nodes;
            for(const auto& N:Nodes)if(!N.IsResolved()||!Loaded(N.Get())||N->GetOuter()!=Graph||!Native(N->GetClass(),TEXT("/Script/EnvironmentQueryEditor"))||!AddObject(N.Get()))return false;
        }
        const FName QueryName=Q->GetQueryName();const bool Strip=Q->bStripFromClientBuilds!=0;
        auto Lost=MakeShared<bool>(false);
        Current=[Objects,Classes,Blueprints,Options,OptionSlots,Nodes,GraphSchema,Graph,Q,Editor,World,QueryName,Strip,ExistingCDO,Lost,Result]()
        {
            if(*Lost)return false;
            auto Bad=[&](const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual)
            {
                *Lost=true;if(Result){Result->bKnown=false;Result->bPost=false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Result->Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);if(!Result->Data->HasField(TEXT("eqs_first_mismatch")))Result->Data->SetObjectField(TEXT("eqs_first_mismatch"),J);Result->Data->SetBoolField(TEXT("eqs_followup_sdk_stopped"),true);Fail(*Result,TEXT("eqs_inspection_current_changed"),TEXT("EQS query/source role changed at an exporter return; no followup SDK, rollback or retry."));}return false;
            };
            if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage()||GEditor!=Editor||!Loaded(Editor)||!Loaded(World)||Editor->GetEditorWorldContext().World()!=World||World->bIsTearingDown)return Bad(TEXT("Editor"),TEXT("context"),TEXT("admitted Editor/world outside PIE/GC/save"),TEXT("changed or unavailable"));
            if(Result&&Result->Data->HasField(TEXT("eqs_followup_sdk_stopped"))){*Lost=true;return false;}
            for(const auto& R:*Objects){UObject* O=R.Hold.Get();if(!Loaded(O)||!Loaded(R.Class.Get())||!Loaded(R.Package.Get())||(R.Outer.IsValid()&&!Loaded(R.Outer.Get()))||O->GetClass()!=R.Class.Get()||O->GetOuter()!=R.Outer.Get()||O->GetPackage()!=R.Package.Get()||O->GetPathName()!=R.Path||(O->GetFlags()&EObjectFlags(RF_Public|RF_Standalone|RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient))!=R.Flags||R.Package->IsDirty()!=R.Dirty)return Bad(R.Path,TEXT("identity_owner_class_package_dirty"),TEXT("admitted actual source role"),TEXT("different or retired"));}
            for(const auto& R:*Classes){UClass* C=R.Class;UObject* CDO=nullptr;if(C->GetSuperClass()!=R.Super||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=R.GeneratedBy||!ExistingCDO(C,CDO)||CDO!=R.CDO||(C->GetClassFlags()&EClassFlags(CLASS_Native|CLASS_CompiledFromBlueprint|CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface|CLASS_DefaultToInstanced|CLASS_EditInlineNew))!=R.Flags)return Bad(C->GetPathName(),TEXT("class_chain_generated_source_existing_cdo"),TEXT("admitted chain/source/nullable CDO"),TEXT("changed"));}
            for(const auto& R:*Blueprints){UBlueprint* BP=R.BP;UObject* CDO=nullptr;if(BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||BP->Status!=R.Status||BP->BlueprintType!=R.Type||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=R.Generated||BP->SkeletonGeneratedClass.GetGCPtr()!=R.Skeleton||BP->ParentClass.GetGCPtr()!=R.Parent||!R.Skeleton->ClassGeneratedBy.IsResolved()||R.Skeleton->ClassGeneratedBy!=BP||R.Skeleton->GetSuperClass()!=R.SkeletonSuper||!ExistingCDO(R.Skeleton,CDO)||CDO!=R.SkeletonCDO)return Bad(BP->GetPathName(),TEXT("blueprint_generated_skeleton_parent"),TEXT("admitted idle actual compiler source roles"),TEXT("changed"));}
            const auto& Actual=Q->GetOptionsMutable();if(Actual.Num()!=OptionSlots.Num()||!Q->EdGraph.IsResolved()||Q->EdGraph!=Graph||Q->GetQueryName()!=QueryName||(Q->bStripFromClientBuilds!=0)!=Strip)return Bad(Q->GetPathName(),TEXT("query_option_graph_summary"),TEXT("admitted option/graph/name/strip roles"),TEXT("changed"));
            for(int32 I=0;I<Actual.Num();++I){const auto& R=(*Options)[I];if(!Actual[I].IsResolved()||Actual[I]!=OptionSlots[I]||!R.Option->Generator.IsResolved()||R.Option->Generator!=R.Generator||R.Option->Tests.Num()!=R.Tests.Num())return Bad(Q->GetPathName(),TEXT("option_generator_test_slots"),TEXT("admitted ordered option/node slots"),TEXT("changed"));for(int32 T=0;T<R.Tests.Num();++T)if(!R.Option->Tests[T].IsResolved()||R.Option->Tests[T]!=R.Tests[T])return Bad(R.Option->GetPathName(),TEXT("test_slot"),LexToString(T),TEXT("different or unresolved"));}
            if(Graph){if(!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr()!=GraphSchema||Graph->Nodes.Num()!=Nodes.Num())return Bad(Graph->GetPathName(),TEXT("graph_schema_node_slots"),TEXT("admitted schema/node inventory"),TEXT("changed"));for(int32 I=0;I<Nodes.Num();++I)if(!Graph->Nodes[I].IsResolved()||Graph->Nodes[I]!=Nodes[I])return Bad(Graph->GetPathName(),TEXT("graph_node_slot"),LexToString(I),TEXT("different or unresolved"));}
            return true;
        };
        if(OutCurrent)*OutCurrent=Current;if(!Current())return false;
    }
    J = Identity(Q); J->SetStringField(TEXT("asset_type"), TEXT("eqs")); J->SetStringField(TEXT("asset_path"), Q->GetPathName()); J->SetStringField(TEXT("query_name"), Q->GetQueryName().ToString());
    J->SetBoolField(TEXT("package_dirty"), Q->GetOutermost()->IsDirty()); J->SetBoolField(TEXT("strip_from_client_builds"), Q->bStripFromClientBuilds != 0); J->SetNumberField(TEXT("option_count"), Q->GetOptions().Num());
    if(Current){J->SetBoolField(TEXT("extended_node_source_summary"),true);J->SetStringField(TEXT("node_source_observation_scope"),TEXT("Actual query/option/node/class-chain and current BP generated/skeleton/parent/existing nullable CDO identity roles only; generated/custom node property values, source graphs and CDO authored values are not sampled. No node virtual execution, compiler work, CDO creation or added loader."));}
    auto P = MakeShared<FJsonObject>(); if (!Properties(Q, {}, P,Current,Result)) { return false; } J->SetObjectField(TEXT("query_properties"), P);
    TArray<TSharedPtr<FJsonValue>> Options; TSet<const UObject*> Seen; int32 ObjectCount = 0, RepeatedGenerators = 0, RepeatedTests = 0;
    for (UEnvQueryOption* Option : Q->GetOptions())
    {
        if (!Loaded(Option) || Option->GetClass() != UEnvQueryOption::StaticClass() || Option->GetTypedOuter<UEnvQuery>() != Q || Seen.Contains(Option) || ++ObjectCount > MaxNodes) { return false; } Seen.Add(Option);
        auto Row = Identity(Option); auto OP = MakeShared<FJsonObject>(); if (!Properties(Option, {}, OP,Current,Result)) { return false; } Row->SetObjectField(TEXT("properties"), OP);
        if (Option->Generator)
        {
            UEnvQueryGenerator* G = Option->Generator;
            // EnvQueryOption owns ordinary reflected references, not query-global unique roles.
            // EnvQueryManager copies the generator independently for every ordered option.
            if (!Loaded(G) || (!Native(G->GetClass(), TEXT("/Script/AIModule"))&&!Current) || G->GetTypedOuter<UEnvQuery>() != Q || ++ObjectCount > MaxNodes) { return false; }
            if (Seen.Contains(G)) { ++RepeatedGenerators; } Seen.Add(G);
            auto GP = MakeShared<FJsonObject>(); auto GR = Identity(G);
            if(Native(G->GetClass(),TEXT("/Script/AIModule"))){if(!Properties(G,{},GP,Current,Result))return false;}
            else{GR->SetBoolField(TEXT("reflected_properties_sampled"),false);GR->SetStringField(TEXT("property_scope"),TEXT("Original structural summary plus admitted source identity/class chain; custom/generated exporters and generator execution are not dispatched."));}
            GR->SetObjectField(TEXT("properties"), GP); Row->SetObjectField(TEXT("generator"), GR);
        }
        else { Row->SetObjectField(TEXT("generator"), MakeShared<FJsonObject>()); }
        if (Option->Tests.Num() > MaxNodes) { return false; } TArray<TSharedPtr<FJsonValue>> Tests;
        for (UEnvQueryTest* T : Option->Tests)
        {
            // A repeated same-query test stays an ordered occurrence; no execution,
            // sorting, TestOrder rewrite or reference deduplication is performed here.
            if (!Loaded(T) || (!Native(T->GetClass(), TEXT("/Script/AIModule"))&&!Current) || T->GetTypedOuter<UEnvQuery>() != Q || ++ObjectCount > MaxNodes) { return false; }
            if (Seen.Contains(T)) { ++RepeatedTests; } Seen.Add(T);
            auto TP = MakeShared<FJsonObject>(); auto TR = Identity(T);
            if(Native(T->GetClass(),TEXT("/Script/AIModule"))){if(!Properties(T,{},TP,Current,Result))return false;}
            else{TR->SetBoolField(TEXT("reflected_properties_sampled"),false);TR->SetStringField(TEXT("property_scope"),TEXT("Original structural summary plus admitted native source identity/class chain; custom exporters and test execution are not dispatched."));}
            TR->SetObjectField(TEXT("properties"), TP); Tests.Add(MakeShared<FJsonValueObject>(TR));
        }
        Row->SetArrayField(TEXT("tests"), Tests); Options.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("options"), Options); J->SetNumberField(TEXT("repeated_generator_reference_count"), RepeatedGenerators); J->SetNumberField(TEXT("repeated_test_reference_count"), RepeatedTests);
    J->SetStringField(TEXT("option_node_reference_scope"), TEXT("Ordered same-query generator/test occurrences may share an existing object. Exact loaded type/source/owner and occurrence budgets remain checked; option and editor-graph node roles retain their existing uniqueness checks. Structural inspection does not certify query execution or complete nonblank dependency independence."));
    auto Graph = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("editor_graph_present"), Q->EdGraph != nullptr); J->SetNumberField(TEXT("editor_graph_node_count"), Q->EdGraph ? Q->EdGraph->Nodes.Num() : 0);
    if (Q->EdGraph)
    {
        UEdGraph* G = Q->EdGraph;
        if (!Loaded(G) || G->GetOuter() != Q || !Native(G->GetClass(), TEXT("/Script/EnvironmentQueryEditor")) || G->Nodes.Num() > MaxNodes) { return false; }
        Graph = Identity(G); auto GP = MakeShared<FJsonObject>(); if (!Properties(G, {}, GP,Current,Result)) { return false; } Graph->SetObjectField(TEXT("properties"), GP); TArray<TSharedPtr<FJsonValue>> Nodes;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!Loaded(N) || N->GetOuter() != G || !Native(N->GetClass(), TEXT("/Script/EnvironmentQueryEditor")) || Seen.Contains(N)) { return false; } Seen.Add(N);
            auto NR = Identity(N); NR->SetStringField(TEXT("node_guid"), N->NodeGuid.ToString()); auto NP = MakeShared<FJsonObject>(); if (!Properties(N, {}, NP,Current,Result)) { return false; } NR->SetObjectField(TEXT("properties"), NP); Nodes.Add(MakeShared<FJsonValueObject>(NR));
        }
        Graph->SetArrayField(TEXT("nodes"), Nodes);
    }
    if(Current&&!Current())return false;J->SetObjectField(TEXT("editor_graph"), Graph); J->SetBoolField(TEXT("observations_complete"), true); return true;
}
struct FObservation
{ TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();TFunction<bool()> Current; };
bool Observe(UEnvQuery* Q, FObservation& O,FResult* Result=nullptr)
{ return QueryState(Q,O.State,&O.Current,Result)&&NativeSource(O.Source,O.Current,Result)&&(!O.Current||O.Current())&&Disk(Q->GetOutermost()->GetName(),O.DiskState)&&Dirty(Q->GetOutermost()->GetName(),O.OtherDirty)&&(!O.Current||O.Current()); }
TSharedRef<FJsonObject> Observation(const FObservation& O)
{ auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_source"), O.Source); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); return J; }
void Publish(FResult& R, const FObservation& O, const TCHAR* Label)
{
    R.Data->SetObjectField(Label, Observation(O)); for (const auto& P : O.State->Values) { R.Data->SetField(P.Key, P.Value); }
    const bool Blank = O.State->GetNumberField(TEXT("option_count")) == 0 && !O.State->GetBoolField(TEXT("editor_graph_present"));
    R.Data->SetBoolField(TEXT("readback_confirmed"), true); R.Data->SetBoolField(TEXT("blank_native_dependencies_verified"), Blank);
    R.Data->SetBoolField(TEXT("complete_nonblank_dependency_audit"), false); R.Data->SetBoolField(TEXT("graph_authoring_verified"), false);
    TArray<TSharedPtr<FJsonValue>> Issues; if (!Blank) { Issues.Add(MakeShared<FJsonValueString>(TEXT("Nonblank option/provider/editor-graph dependency independence is not fully audited by this finite summary."))); }
    const bool Persisted = O.DiskState->GetBoolField(TEXT("exists")) && O.DiskState->GetNumberField(TEXT("bytes")) > 0;
    if (R.Operation == TEXT("inspect_ai_asset_structure") && !Persisted) { Issues.Add(MakeShared<FJsonValueString>(TEXT("Query file is not persisted; in-memory summary remains available."))); }
    R.Data->SetBoolField(TEXT("persistence_verified"), Persisted); R.Data->SetArrayField(TEXT("audit_issues"), Issues); R.Data->SetBoolField(TEXT("mcp_dependency_audit"), Issues.IsEmpty());
    R.Data->SetArrayField(TEXT("mcp_dependency_audit_issues"), Issues); R.Data->SetBoolField(TEXT("mcp_dependency_audit_pass"), Issues.IsEmpty());
    R.Data->SetStringField(TEXT("dependency_audit_scope"), TEXT("Exact native blank query has no option/graph references; creation uses in-memory admission before optional save, inspection additionally reports absent persistence. Nonblank dependency inventories remain incomplete."));
}
bool Retained(UEnvQuery* Q, const FObservation& Before, FObservation& After, FResult& R, bool bAllowCDO)
{
    if(Before.Current&&!Before.Current())return false;
    if (!Observe(Q,After,&R)) { R.bKnown=false;R.bPost=false;if(R.Code.IsEmpty())Fail(R,TEXT("eqs_retained_state_unobservable"),TEXT("Retained target/source/disk state is incomplete; do not replay allocation or save."));return false; } if(Before.Current&&!Before.Current())return false;Publish(R, After, TEXT("after"));
    R.bChanged |= Text(Before.State) != Text(After.State); bool NewCDO = false;
    const bool SourceOK = ProtectSource(Before.Source, After.Source, bAllowCDO, NewCDO), DirtyOK = Text(Before.OtherDirty) == Text(After.OtherDirty), DiskOK = Text(Before.DiskState) == Text(After.DiskState);
    R.Data->SetBoolField(TEXT("existing_native_defaults_preserved"), SourceOK); R.Data->SetBoolField(TEXT("implicit_native_cdo_creation_observed"), NewCDO || (R.Data->HasField(TEXT("implicit_native_cdo_creation_observed")) && R.Data->GetBoolField(TEXT("implicit_native_cdo_creation_observed")))); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), DirtyOK); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), DiskOK);
    if (!SourceOK || !DirtyOK || !DiskOK) { R.bPost = false; return Fail(R, TEXT("eqs_preservation_failed"), TEXT("Actual target retained but protected native defaults/other dirty states/disk changed; save suppressed.")); } return true;
}
bool Save(UEnvQuery* Q, const FObservation& Before, FResult& R)
{
    if (!R.bSaveRequested) { return true; } R.Stage = TEXT("save"); R.bSaveAttempted = true;
    auto Call = MakeShared<FJsonObject>(); Call->SetNumberField(TEXT("attempt_index"), 1); Call->SetBoolField(TEXT("package_dirty_before"), Q->GetOutermost()->IsDirty());
    const bool Saved = UEditorAssetLibrary::SaveLoadedAsset(Q, false); Call->SetBoolField(TEXT("success"), Saved); Call->SetBoolField(TEXT("package_dirty_after"), Q->GetOutermost()->IsDirty());
    R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Call) }); R.Data->SetNumberField(TEXT("save_attempt_count"), 1);
    FObservation After; if (!Observe(Q, After)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("eqs_save_state_unobservable"), TEXT("Save prefix retained; current RAM/source/disk state is incomplete and must not be replayed.")); } Publish(R, After, TEXT("after_save"));
    if (!Saved) { R.bKnown = false; R.bPost = false; R.Data->SetBoolField(TEXT("save_disk_outcome_known"), false); return Fail(R, TEXT("eqs_save_failed"), TEXT("The one exact save returned false; disk transaction uncertified, no save retry or cleanup occurred.")); }
    auto Expected = MakeShared<FJsonObject>(); for (const auto& P : Before.State->Values) { Expected->SetField(P.Key, P.Value); }
    const FString Flags = Before.State->GetStringField(TEXT("object_flags")); const uint64 Old = FCString::Strtoui64(*Flags, nullptr, 10);
    if (Flags != FString::Printf(TEXT("%llu"), Old)) { R.bKnown = false; R.bPost = false; return Fail(R, TEXT("eqs_save_flags_unobservable"), TEXT("Saved bytes retained; before-save identity flag text was noncanonical.")); }
    Expected->SetStringField(TEXT("object_flags"), FString::Printf(TEXT("%llu"), Old | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))); Expected->SetBoolField(TEXT("package_dirty"), false);
    bool NewCDO = false; const bool SourceOK = ProtectSource(Before.Source, After.Source, false, NewCDO) && Text(Before.OtherDirty) == Text(After.OtherDirty), StateOK = Text(Expected) == Text(After.State);
    R.bSaved = !Q->GetOutermost()->IsDirty() && After.DiskState->GetBoolField(TEXT("exists")) && After.DiskState->GetNumberField(TEXT("bytes")) > 0;
    R.Data->SetBoolField(TEXT("asset_saved"), R.bSaved); R.Data->SetBoolField(TEXT("save_state_preserved"), StateOK); R.Data->SetBoolField(TEXT("save_source_preserved"), SourceOK); R.Data->SetBoolField(TEXT("save_disk_outcome_known"), true);
    R.Data->SetStringField(TEXT("save_state_comparison_scope"), TEXT("Only exact audited query object flags predict before OR RF_WasLoaded|RF_LoadCompleted, and selected package dirty=false. Query properties/options/graph/native defaults/other dirty states remain exact; raw snapshots are retained."));
    if (!R.bSaved || !SourceOK || !StateOK) { R.bPost = false; return Fail(R, TEXT("eqs_save_postcondition_failed"), TEXT("Exact save returned true but clean file/state/source preservation was not certified; saved bytes and actual snapshots retained without replay.")); } return true;
}
}
FString CreateAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset)
{
    FResult R; Init(R, TEXT("create_ai_eqs_query"), bSaveAsset); if (!EditorState(R)) { return Encode(R); }
    FString Root = AssetRoot.TrimStartAndEnd(); Root.TrimQuotesInline(); Root.ReplaceInline(TEXT("\\"), TEXT("/")); while (Root.EndsWith(TEXT("/"))) { Root.LeftChopInline(1); } FText Reason;
    if (AssetName.IsEmpty() || AssetName.Len() >= NAME_SIZE || FName(*AssetName).IsNone() || !FName::IsValidXName(AssetName, INVALID_OBJECTNAME_CHARACTERS, &Reason)
        || !(Root == TEXT("/Game") || Root.StartsWith(TEXT("/Game/"))) || !FPackageName::IsValidLongPackageName(Root))
    { Fail(R, TEXT("eqs_create_path_invalid"), TEXT("Use a valid bounded simple asset_name and /Game asset_root; existing destinations are never overwritten.")); return Encode(R); }
    const FString PackageName = Root + TEXT("/") + AssetName, Path = PackageName + TEXT(".") + AssetName; auto SourceBefore = MakeShared<FJsonObject>(), DiskBefore = MakeShared<FJsonObject>(), DirtyBefore = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("native_source_before"), SourceBefore); R.Data->SetObjectField(TEXT("disk_before"), DiskBefore); R.Data->SetObjectField(TEXT("other_package_dirty_before"), DirtyBefore);
    if (Path.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(Path) || !NativeSource(SourceBefore) || !Disk(PackageName, DiskBefore) || !Dirty(PackageName, DirtyBefore))
    { Fail(R, TEXT("eqs_native_source_unobservable"), TEXT("Exact native class/default/destination observations incomplete before allocation; partial rows retained.")); return Encode(R); }
    if (FindPackage(nullptr, *PackageName) || FindObject<UObject>(nullptr, *Path) || DiskBefore->GetBoolField(TEXT("exists")) || FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path)).IsValid())
    { Fail(R, TEXT("eqs_target_exists"), TEXT("Destination exists; no replacement/allocation/save attempted.")); return Encode(R); }
    R.Stage = TEXT("create_asset"); R.bAttempted = true; R.Data->SetBoolField(TEXT("asset_allocation_attempted"), true);
    UPackage* Package = CreatePackage(*PackageName); TStrongObjectPtr<UPackage> KeepPackage(Package);
    if (Package) { R.Data->SetBoolField(TEXT("package_fully_load_dispatched"), true); R.Data->SetBoolField(TEXT("implicit_package_load_possible"), true); Package->FullyLoad(); }
    auto* Q = Package ? NewObject<UEnvQuery>(Package, UEnvQuery::StaticClass(), *AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr; TStrongObjectPtr<UEnvQuery> Keep(Q); R.bChanged = Package != nullptr;
    if (!Loaded(Q)) { R.bKnown = false; Fail(R, TEXT("eqs_create_failed"), TEXT("Allocation prefix retained; no atomic cleanup/rollback/retry claimed.")); return Encode(R); }
    FObservation Factory; if (!Observe(Q, Factory)) { R.bKnown = false; Fail(R, TEXT("eqs_factory_state_unobservable"), TEXT("Allocated native query retained but factory/source state incomplete.")); return Encode(R); } Publish(R, Factory, TEXT("factory_default_state"));
    bool NewCDO = false; if (!ProtectSource(SourceBefore, Factory.Source, true, NewCDO) || Text(DiskBefore) != Text(Factory.DiskState) || Text(DirtyBefore) != Text(Factory.OtherDirty)
        || Q->GetPathName() != Path || Q->GetQueryName() != FName(*AssetName) || !Q->GetOptions().IsEmpty() || Q->EdGraph)
    { Fail(R, TEXT("eqs_factory_postcondition_failed"), TEXT("Allocated prefix retained but blank native factory identity/options/graph/source/defaults were not certified; publication/save suppressed.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("implicit_native_cdo_creation_observed"), NewCDO);
    Q->MarkPackageDirty(); Package->MarkPackageDirty(); R.Data->SetBoolField(TEXT("asset_registry_publication_dispatched"), true); FAssetRegistryModule::AssetCreated(Q);
    FObservation After; if (!Retained(Q, Factory, After, R, false)) { return Encode(R); }
    R.Data->SetBoolField(TEXT("created"), true); R.bChanged = true; R.bPost = true; if (!Save(Q, After, R)) { return Encode(R); } R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
FString InspectAsset(const FString& AssetPath)
{
    FResult R; Init(R, TEXT("inspect_ai_asset_structure"), false); FString Path; if (!EditorState(R) || !ExactPath(AssetPath, Path, R)) { return Encode(R); }
    // Preserve the existing BT/Blackboard dispatcher, including its loader diagnostics. No non-EQS resolution or CDO warming here.
    UObject* Existing = FindObject<UObject>(nullptr, *Path);
    if (Existing && Existing->GetClass()->GetPathName() != TEXT("/Script/AIModule.EnvQuery")) { return BehaviorTree::InspectAsset(AssetPath); }
    if (!Existing)
    {
        const FAssetData Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
        if (!Asset.IsValid() || Asset.AssetClassPath.ToString() != TEXT("/Script/AIModule.EnvQuery")) { return BehaviorTree::InspectAsset(AssetPath); }
    }
    UEnvQuery* Q = Resolve(Path, R); TStrongObjectPtr<UEnvQuery> Keep(Q); if (!Q) { return Encode(R); } FObservation Before, After;
    if (!Observe(Q,Before,&R)) { if(R.Code.IsEmpty())Fail(R,TEXT("eqs_inspection_unobservable"),TEXT("Bounded query summary failed; unsupported query subclasses, unresolved/nonowned node references, nonstandard generated sources and custom editor graphs remain incomplete."));return Encode(R); } Publish(R, Before, TEXT("before"));
    if (!Retained(Q, Before, After, R, false)) { return Encode(R); }
    if (R.bChanged) { Fail(R, TEXT("eqs_inspection_state_changed"), TEXT("Repeated native summary changed after initial resolution; no explicit graph/option/cache/query/write/save was dispatched.")); return Encode(R); }
    R.Data->SetStringField(TEXT("read_only_claim_scope"),TEXT("No explicit query/generator/test execution, property writer, compiler, CDO creation or save after initial resolution. New node profiles expose structural/source roles only; authored generator/CDO values and full nonblank dependency independence are not certified."));
    R.Data->SetBoolField(TEXT("read_only_after_initial_resolution"), true); R.bPost = true; R.bSuccess = true; R.Stage = TEXT("complete"); return Encode(R);
}
}
