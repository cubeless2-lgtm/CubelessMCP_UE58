#include "Testing/CubelessNiagaraCollectionFixtureLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraParameterStore.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

FString UCubelessNiagaraCollectionFixtureLibrary::SeedMissingVec4Declarations(const FString& CollectionPath)
{
    auto Result=MakeShared<FJsonObject>();
    bool Attempted=false;int32 ExpectedDeclared=0,ExpectedStore=0,DispatchCount=0;
    const TCHAR* Stage=TEXT("preflight");FString Error;
    auto Encode=[&](bool Success)
    {
        Result->SetBoolField(TEXT("success"),Success);Result->SetStringField(TEXT("stage"),Stage);
        Result->SetStringField(TEXT("error"),Error);Result->SetStringField(TEXT("collection_path"),CollectionPath);
        Result->SetBoolField(TEXT("mutation_attempted"),Attempted);Result->SetBoolField(TEXT("change_state_known"),Success||!Attempted);
        Result->SetBoolField(TEXT("explicit_save_attempted"),false);Result->SetBoolField(TEXT("explicit_compile_attempted"),false);
        Result->SetBoolField(TEXT("atomic_rollback_available"),false);Result->SetBoolField(TEXT("automatic_retry_permitted"),false);
        Result->SetNumberField(TEXT("SDK_dispatch_count"),DispatchCount);Result->SetNumberField(TEXT("expected_declared_vec4_count"),ExpectedDeclared);
        Result->SetBoolField(TEXT("SDK_global_purity_unverified"),Attempted);
        Result->SetStringField(TEXT("scope"),TEXT("Development-only fixture: new empty exact Engine NPC, three declared Vec4 variables and intentionally missing default-store entries, public Engine APIs only. No Toolset, save, compiler or automatic retry."));
        FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));return Text;
    };
    auto Live=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    if(CollectionPath.Len()>512||!FPackageName::IsValidLongPackageName(CollectionPath)||
       !(CollectionPath.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/CubelessTests/GG_"),ESearchCase::CaseSensitive)||CollectionPath.StartsWith(TEXT("/Game/_MCP_Temp/NiagaraGenerated/CubelessTests/GH_"),ESearchCase::CaseSensitive)))
    {Error=TEXT("Exact owned GG/GH fixture package root required.");return Encode(false);}
    const FString Asset=FPackageName::GetLongPackageAssetName(CollectionPath);
    if(Asset!=TEXT("NPC_Source")&&Asset!=TEXT("NPC_Target")&&Asset!=TEXT("NPC_Partial"))
    {Error=TEXT("Only the three fixed fixture NPC basenames are admitted.");return Encode(false);}
    if(!IsInGameThread()||!GEditor||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested()||GExitPurge)
    {Error=TEXT("Stopped live Editor game thread without GC/save/exit required.");return Encode(false);}
    const FString ObjectPath=CollectionPath+TEXT(".")+Asset;
    UNiagaraParameterCollection* Collection=FindObject<UNiagaraParameterCollection>(nullptr,*ObjectPath);
    UNiagaraParameterCollectionInstance* Instance=Collection?Collection->GetDefaultInstance():nullptr;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    if(!Live(World)||World->bIsTearingDown||!Live(Collection)||Collection->GetClass()!=UNiagaraParameterCollection::StaticClass()||Collection->GetPathName()!=ObjectPath||Collection->GetOutermost()->GetName()!=CollectionPath||!Live(Instance)||Instance->GetClass()!=UNiagaraParameterCollectionInstance::StaticClass()||Instance->GetParent()!=Collection||Instance->GetOuter()!=Collection||Instance->GetOutermost()!=Collection->GetOutermost())
    {Error=TEXT("Already-loaded exact Engine collection/default-instance ownership required; no load or replacement fallback.");return Encode(false);}
    TStrongObjectPtr<UNiagaraParameterCollection> HoldCollection(Collection);
    TStrongObjectPtr<UNiagaraParameterCollectionInstance> HoldInstance(Instance);
    TStrongObjectPtr<UPackage> HoldPackage(Collection->GetOutermost());
    const FString InstancePath=Instance->GetPathName();const FString Namespace=Collection->GetFullNamespaceName().ToString();
    const FString Names[]={Namespace+TEXT("ValueA"),Namespace+TEXT("ValueB"),Namespace+TEXT("ValueBad")};
    auto Guard=[&]()
    {
        if(!IsInGameThread()||!GEditor||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested()||GExitPurge||GEditor->GetEditorWorldContext().World()!=World||!Live(World)||World->bIsTearingDown||!Live(Collection)||!Live(Instance)||!Live(HoldPackage.Get())||Collection->GetClass()!=UNiagaraParameterCollection::StaticClass()||Collection->GetPathName()!=ObjectPath||Collection->GetOuter()!=HoldPackage.Get()||Collection->GetOutermost()!=HoldPackage.Get()||HoldPackage->GetName()!=CollectionPath||Collection->GetDefaultInstance()!=Instance||Instance->GetClass()!=UNiagaraParameterCollectionInstance::StaticClass()||Instance->GetParent()!=Collection||Instance->GetOuter()!=Collection||Instance->GetPathName()!=InstancePath||Instance->GetOutermost()!=HoldPackage.Get()||Collection->GetFullNamespaceName().ToString()!=Namespace)
        {Error=TEXT("Captured fixture identity/world/namespace changed; no further dispatch or retry.");return false;}
        const TArray<FNiagaraVariable>& Declared=Collection->GetParameters();TArray<FNiagaraVariable> Stored;Instance->GetParameterStore().GetParameters(Stored);
        if(Declared.Num()!=ExpectedDeclared||Stored.Num()!=ExpectedStore){Error=TEXT("Unexpected declaration/store membership; partial fixture retained.");return false;}
        for(int32 I=0;I<ExpectedDeclared;++I){if(Declared[I].GetName().ToString()!=Names[I]||Declared[I].GetType()!=FNiagaraTypeDefinition::GetVec4Def()){Error=TEXT("Unexpected declared fixture type/name/order.");return false;}}
        return true;
    };
    if(!Guard()){return Encode(false);}
    Stage=TEXT("collection_modify");Attempted=true;++DispatchCount;Collection->Modify();if(!Guard()){return Encode(false);}
    Stage=TEXT("instance_modify");++DispatchCount;Instance->Modify();if(!Guard()){return Encode(false);}
    for(int32 I=0;I<3;++I)
    {
        const FNiagaraVariable Variable(FNiagaraTypeDefinition::GetVec4Def(),FName(*Names[I]));
        Stage=TEXT("add_declared_parameter");++DispatchCount;const int32 Added=Collection->AddParameter(Variable);++ExpectedDeclared;ExpectedStore=1;
        if(!Guard()||Added!=I){if(Error.IsEmpty())Error=TEXT("AddParameter did not return the expected new declaration index.");return Encode(false);}
        Stage=TEXT("remove_only_new_default_store_entry");++DispatchCount;const bool Removed=Instance->GetParameterStore().RemoveParameter(Variable);ExpectedStore=0;
        if(!Guard()||!Removed){if(Error.IsEmpty())Error=TEXT("New default-store entry was not removed; no retry.");return Encode(false);}
    }
    Stage=TEXT("mark_owned_package_dirty");++DispatchCount;Collection->MarkPackageDirty();if(!Guard()){return Encode(false);}
    Result->SetBoolField(TEXT("dirty_after"),HoldPackage->IsDirty());Result->SetNumberField(TEXT("missing_default_store_count"),3);
    Stage=TEXT("complete");return Encode(true);
}
