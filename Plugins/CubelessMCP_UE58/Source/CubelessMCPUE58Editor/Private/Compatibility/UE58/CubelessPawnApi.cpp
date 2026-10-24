// CF: ordered UE56 Pawn defaults contract, using public UE58 Engine fields/setter.
#include "Compatibility/UE58/CubelessPawnApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Pawn
{
namespace
{
using Variable::FResult;
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
// YB source roles use the same public resolved-path/shallow-pointer approach as
// other retained readers; no tracked Get or source loading/default creation.
UObject* ResolvedSourcePointer(const TObjectPtr<UObject>& Ref)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
    if(!IsEditorGameThread()||IsPlaySessionActive()||!Ref.IsResolved()||Ref==nullptr)return nullptr;
    const FString Path=Ref.GetPathName();UObject* Found=StaticFindObject(nullptr,nullptr,*Path);
    return Loaded(Found)&&Loaded(Found->GetClass())&&Ref==Found?Found:nullptr;
#else
    return nullptr; // Wider source resolution is an explicit incomplete variant.
#endif
}
bool TryResolvedParentSuper(const UClass* Class,UClass*& Out)
{
    // UStruct::SuperStruct is a reflected object slot (Class.h). Public property
    // access copies its TObjectPtr without invoking tracked GetSuperClass().
    Out=nullptr;if(!IsValid(Class))return false;
    // The linked intrinsic CoreUObject descriptor is captured once; repeated
    // source-current checks must not traverse tracked native superclass slots.
    static FObjectProperty* const P=FindFProperty<FObjectProperty>(UStruct::StaticClass(),TEXT("SuperStruct"));
    if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=UStruct::StaticClass()
        ||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->ElementSize!=sizeof(TObjectPtr<UStruct>)
        ||P->GetSize()!=sizeof(TObjectPtr<UStruct>)||P->GetMinAlignment()!=alignof(TObjectPtr<UStruct>)
        ||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>UStruct::StaticClass()->GetPropertiesSize()
        ||P->HasSetterOrGetter()||!P->PropertyClass.IsResolved()||P->PropertyClass!=UStruct::StaticClass())return false;
    const TObjectPtr<UObject> Slot=P->GetObjectPtrPropertyValue(P->ContainerPtrToValuePtr<void>(Class));
    if(!Slot.IsResolved())return false;
    if(Slot==nullptr)return true;
    Out=Cast<UClass>(ResolvedSourcePointer(Slot));return Loaded(Out);
}
bool ResolvedParentSuperMatches(const UClass* Class,const UClass* Expected)
{ UClass* Actual=nullptr;return TryResolvedParentSuper(Class,Actual)&&Actual==Expected; }
bool ResolvedParentSuperMatchesSlot(const UClass* Class,const TObjectPtr<UClass>& Expected)
{ UClass* Actual=nullptr;return Expected.IsResolved()&&TryResolvedParentSuper(Class,Actual)&&Expected==Actual; }
bool HasNul(const FString& S) { for (int32 I = 0; I < S.Len(); ++I) { if (S[I] == TCHAR('\0')) { return true; } } return false; }
bool Clean(const FString& S)
{ for (const TCHAR* K : { TEXT("/Script/Cubeless"), TEXT("/Script/UnrealMCP"), TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/Engine/PythonTypes"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/") }) { if (S.Contains(K, ESearchCase::IgnoreCase)) { return false; } } return true; }
void Count(FResult& R, const TCHAR* Key) { R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error, bool Unknown = false)
{ R.bSuccess = false; R.bPostconditionPassed = false; R.ErrorCode = Code; R.Error = Error; if (Unknown) { R.bChangeStateKnown = false; } return false; }
void Init(FResult& R)
{
    for (const TCHAR* K : { TEXT("asset_load_count"), TEXT("observation_capture_calls"), TEXT("property_request_count"), TEXT("property_validation_failures"),
        TEXT("setter_attempt_count"), TEXT("setter_return_count"), TEXT("accepted_property_count"), TEXT("mark_blueprint_modified_calls"), TEXT("mark_blueprint_modified_returns"),
        TEXT("explicit_modify_calls"), TEXT("outer_transaction_calls"), TEXT("full_compile_attempts"), TEXT("save_attempt_count"), TEXT("explicit_cdo_creation_calls") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetArrayField(TEXT("asset_load_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetArrayField(TEXT("property_calls"), {});
    R.Data->SetObjectField(TEXT("results"), MakeShared<FJsonObject>());
    for (const TCHAR* K : { TEXT("observations_complete"), TEXT("unselected_state_preserved"), TEXT("source_preserved"), TEXT("disk_preserved"), TEXT("other_package_dirty_preserved"),
        TEXT("all_requested_properties_succeeded"), TEXT("partial_success"), TEXT("runtime_verified"), TEXT("cold_persistence_verified"), TEXT("implicit_engine_work_measured") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: native Pawn/Character defaults and public Blueprint editor callback; no ServerRPC or runtime Cubeless class."));
    R.Data->SetStringField(TEXT("finite_scope"), TEXT("Normal own Game Blueprint with a loaded native or admitted generated Pawn parent chain, existing current generated Pawn CDO, no loaded live/derived instances. Native/generated parent-chain identities, existing CDOs and reflected values are observed without source writes or default creation. Saved-clean loaded generated Pawn parent chains are admitted with readonly source identity/graph membership and existing CDO observation; cold/generated custom owners, global short-name search and unfinished class variants remain incomplete."));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("One original command-level transaction after finite admission, ordered five property applications, then MarkBlueprintAsModified once if any setter succeeded, including matching values. No explicit Modify/PostEdit/compile/save. Damage setter marks replication dirtiness; MarkBlueprintAsModified performs implicit graph/cache/post-construction-list/PostEdit/editor callbacks and dirty/status work. These are not claimed absent or counted as explicit fullcompile."));
    R.Data->SetStringField(TEXT("transaction_scope"), TEXT("Original HandleCommand opens one mutating-command transaction before handler admission. This finite adapter preserves one transaction for admitted ordered processing, including all-invalid property requests, while its known-zero preflight refusals and the original public no-properties shortcut do not open it."));
    R.Data->SetStringField(TEXT("observation_scope"), TEXT("Bounded nontransient reflected BP/CDO/owned graph/component/template fields, typed selected values, loaded native class/CDO fields, target disk and other package dirty flags after explicit asset resolution. Private caches/replication bytes, unloaded assets, runtime behavior and cold persistence are not observed."));
    R.Data->SetStringField(TEXT("legacy_result_scope"), TEXT("Terminal success is any accepted property, including partial failures and matching setters. Empty public request succeeds without Blueprint resolution; original direct native empty-request error remains a documented surface difference. No mutation_result_v1 existed on this route."));
}
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), Loaded(O)); J->SetStringField(TEXT("path"), Loaded(O) ? O->GetPathName() : FString());
    J->SetStringField(TEXT("class"), Loaded(O) ? O->GetClass()->GetPathName() : FString()); J->SetStringField(TEXT("outer"), Loaded(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString());
    J->SetStringField(TEXT("object_flags"), Loaded(O) ? LexToString(static_cast<uint64>(O->GetFlags())) : FString()); return J;
}
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, int32 Depth = 0)
{
    if (!A || !B || A->Type != B->Type || Depth > 32) { return false; }
    if (A->Type == EJson::Object) { const auto X = A->AsObject(), Y = B->AsObject(); if (!X || !Y || X->Values.Num() != Y->Values.Num()) { return false; } for (const auto& P : X->Values) { if (!Equal(P.Value, Y->TryGetField(P.Key.ToView()), Depth + 1)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I], Depth + 1)) { return false; } } return true; }
    return A->Type == EJson::String ? A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive) : FJsonValue::CompareEqual(*A, *B);
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B) { return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
const TCHAR* PropertyNames[] = { TEXT("AutoPossessPlayer"), TEXT("bUseControllerRotationYaw"), TEXT("bUseControllerRotationPitch"), TEXT("bUseControllerRotationRoll"), TEXT("bCanBeDamaged") };
TSharedRef<FJsonObject> Values(const APawn* P)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(PropertyNames[0], static_cast<uint8>(P->AutoPossessPlayer)); J->SetBoolField(PropertyNames[1], P->bUseControllerRotationYaw);
    J->SetBoolField(PropertyNames[2], P->bUseControllerRotationPitch); J->SetBoolField(PropertyNames[3], P->bUseControllerRotationRoll); J->SetBoolField(PropertyNames[4], P->CanBeDamaged()); return J;
}
bool Fields(UObject* O, bool Blueprint, bool SelectedCDO, const TSharedRef<FJsonObject>& Out, const TFunction<bool()>* Current = nullptr)
{
    // The new native-parent profile supplies Current; old direct profiles are unchanged.
    TStrongObjectPtr<UObject> ReceiverHold,ReceiverOuter;
    TStrongObjectPtr<UClass> ReceiverClass;TStrongObjectPtr<UPackage> ReceiverPackage;
    FString ReceiverPath;EObjectFlags ReceiverFlags=RF_NoFlags;
    struct FFieldRole
    {
        FProperty* Field=nullptr;UStruct* Owner=nullptr;FFieldClass* Class=nullptr;
        FName Name;int32 Offset=0,Size=0,Dim=0,Alignment=0;uint64 Flags=0;
        TArray<const void*> Addresses;
    };
    TArray<FFieldRole> FieldRoles;
    if(Current)
    {
        if(!(*Current)()||!Loaded(O)||!Loaded(O->GetClass()))return false;
        ReceiverHold.Reset(O);ReceiverClass.Reset(O->GetClass());ReceiverOuter.Reset(O->GetOuter());ReceiverPackage.Reset(O->GetPackage());ReceiverPath=O->GetPathName();ReceiverFlags=O->GetFlags();
        for(TFieldIterator<FProperty> It(ReceiverClass.Get(),EFieldIteratorFlags::IncludeSuper);It;++It)
        {
            FProperty* P=*It;if(FieldRoles.Num()>=2048||P->ArrayDim<1||P->ArrayDim>64||!Loaded(P->GetOwnerStruct()))return false;
            FFieldRole E;E.Field=P;E.Owner=P->GetOwnerStruct();E.Class=P->GetClass();E.Name=P->GetFName();E.Offset=P->GetOffset_ForInternal();E.Size=P->GetSize();E.Dim=P->ArrayDim;E.Alignment=P->GetMinAlignment();E.Flags=uint64(P->GetPropertyFlags());
            for(int32 I=0;I<E.Dim;++I)E.Addresses.Add(P->ContainerPtrToValuePtr<void>(O,I));FieldRoles.Add(MoveTemp(E));
        }
    }
    const auto ReceiverCurrent=[&]()
    {
        if(!Current)return true;if(!(*Current)())return false;
        auto* Receiver=ReceiverHold.Get();auto* Class=ReceiverClass.Get();auto* Outer=ReceiverOuter.Get();
        if(!Loaded(Receiver)||!Loaded(Class)||(Outer&&!Loaded(Outer))||!Loaded(ReceiverPackage.Get())||Receiver!=O||Receiver->GetClass()!=Class||Receiver->GetOuter()!=Outer||Receiver->GetPackage()!=ReceiverPackage.Get()||Receiver->GetPathName()!=ReceiverPath||Receiver->GetFlags()!=ReceiverFlags)return false;
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(Class,EFieldIteratorFlags::IncludeSuper);It;++It){if(Now.Num()>=2048)return false;Now.Add(*It);}
        if(Now.Num()!=FieldRoles.Num())return false;
        // Check fresh membership first; never dereference a retired descriptor.
        for(int32 I=0;I<Now.Num();++I)if(Now[I]!=FieldRoles[I].Field)return false;
        for(int32 I=0;I<Now.Num();++I)
        {
            const auto& E=FieldRoles[I];auto* P=Now[I];
            if(P->GetOwnerStruct()!=E.Owner||P->GetClass()!=E.Class||P->GetFName()!=E.Name||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->ArrayDim!=E.Dim||P->GetMinAlignment()!=E.Alignment||uint64(P->GetPropertyFlags())!=E.Flags)return false;
            for(int32 Slot=0;Slot<E.Dim;++Slot)if(P->ContainerPtrToValuePtr<void>(Receiver,Slot)!=E.Addresses[Slot])return false;
        }
        return true;
    };
    int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        if (!ReceiverCurrent()) { return false; }
        FProperty* P = *It; if (P->HasAnyPropertyFlags(CPF_Transient)) { continue; }
        if (Blueprint && (P->GetFName() == TEXT("Status") || P->GetFName() == TEXT("bCachedDependenciesUpToDate"))) { continue; }
        bool Selected = false; for (const TCHAR* Name : PropertyNames) { Selected |= SelectedCDO && P->GetFName() == Name; } if (Selected) { continue; }
        auto J = MakeShared<FJsonObject>(); Out->SetObjectField(P->GetName(), J); if (++Count > 2048 || P->ArrayDim < 1 || P->ArrayDim > 64 || !P->GetOwnerStruct()) { return false; }
        J->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); J->SetStringField(TEXT("property_class"), P->GetClass()->GetName()); J->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        TArray<TSharedPtr<FJsonValue>> Rows; J->SetArrayField(TEXT("values"), Rows);
        for (int32 I = 0; I < P->ArrayDim; ++I)
        {
            if (!ReceiverCurrent()) { return false; }
            const void* A = P->ContainerPtrToValuePtr<void>(O, I); FString S;
            const bool Exported = P->ExportText_Direct(S, A, A, O, PPF_None);
            if (!ReceiverCurrent()) { return false; }
            if (!Exported || S.Len() > 1048576 || Chars > 1048576 - S.Len() || !Clean(S)) { return false; }
            Chars += S.Len(); Rows.Add(MakeShared<FJsonValueString>(S)); J->SetArrayField(TEXT("values"), Rows);
        }
    } return true;
}
struct FContext
{
    TStrongObjectPtr<UBlueprint> Blueprint; TStrongObjectPtr<APawn> CDO; FString Path, Package;
    struct FNativeParent
    {
        TStrongObjectPtr<UClass> Class; TStrongObjectPtr<UObject> Default,ClassOuter,DefaultOuter; TStrongObjectPtr<UPackage> Package;
        UClass* Super=nullptr; FString Path,DefaultPath; EObjectFlags Flags=RF_NoFlags,DefaultFlags=RF_NoFlags;
        EClassFlags ClassFlags=CLASS_None; int32 Size=0,Alignment=0; uint32 PackageFlags=0; bool Dirty=false;
    };
    TArray<FNativeParent> NativeParents; TStrongObjectPtr<UClass> Generated;
    struct FSourceGraph
    {
        TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UClass> Class,Schema;
        FString Path;EObjectFlags Flags=RF_NoFlags;
        struct FNode{TStrongObjectPtr<UEdGraphNode> Node;TStrongObjectPtr<UClass> Class;FString Path;EObjectFlags Flags=RF_NoFlags;};
        TArray<FNode> Nodes;
    };
    struct FGeneratedParent
    {
        TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UClass> Generated,Skeleton,SkeletonSuper;
        FString Path,SkeletonPath;EObjectFlags Flags=RF_NoFlags,SkeletonFlags=RF_NoFlags;EBlueprintStatus Status=BS_Unknown;TArray<FSourceGraph> Graphs;
    };
    TArray<FGeneratedParent> GeneratedParents;
    bool ExtendedNativeParent=false,GeneratedParentProfile=false;
    bool ParentCurrent(FResult& R) const
    {
        if(!ExtendedNativeParent)return true;
        auto Mismatch=[&](const FString& Object,const TCHAR* Field,const FString& Expected,const FString& Actual)
        {
            if(!R.Data->HasField(TEXT("native_parent_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);R.Data->SetObjectField(TEXT("native_parent_first_mismatch"),J);}return false;
        };
        auto* B=Blueprint.Get();auto* P=CDO.Get();auto* G=Generated.Get();
        if(!IsEditorGameThread()||IsPlaySessionActive()||!Loaded(B)||!Loaded(P)||!Loaded(G)||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.Get()!=G||G->ClassGeneratedBy!=B||!G->ClassDefaultObject.IsResolved()||G->GetDefaultObject(false)!=P||P->GetClass()!=G||!B->ParentClass.GetGCPtr().IsResolved()||NativeParents.IsEmpty()||B->ParentClass.Get()!=NativeParents[0].Class.Get()||(GeneratedParentProfile?!ResolvedParentSuperMatches(G,NativeParents[0].Class.Get()):G->GetSuperClass()!=NativeParents[0].Class.Get())||B->GetPathName()!=Path||B->bBeingCompiled||B->bIsRegeneratingOnLoad)
            return Mismatch(Path,TEXT("target_class_CDO_context"),TEXT("admitted current Blueprint/class/CDO and native parent"),TEXT("identity, context or class relation changed"));
        for(int32 I=0;I<NativeParents.Num();++I)
        {
            const auto& E=NativeParents[I];auto* K=E.Class.Get();auto* D=E.Default.Get();auto* ParentPackage=E.Package.Get();
            if(!Loaded(K)||!Loaded(ParentPackage)||K->GetPackage()!=ParentPackage||!Loaded(E.ClassOuter.Get())||K->GetOuter()!=E.ClassOuter.Get()||K->GetFlags()!=E.Flags||K->GetClassFlags()!=E.ClassFlags||K->GetPathName()!=E.Path||(GeneratedParentProfile?!ResolvedParentSuperMatches(K,E.Super):K->GetSuperClass()!=E.Super)||K->GetPropertiesSize()!=E.Size||K->GetMinAlignment()!=E.Alignment||uint32(ParentPackage->GetPackageFlags())!=E.PackageFlags||ParentPackage->IsDirty()!=E.Dirty||!K->ClassDefaultObject.IsResolved()||(K->HasAnyClassFlags(CLASS_Native)?K->GetDefaultObject(false)!=D:K->ClassDefaultObject!=D)||(I+1<NativeParents.Num()&&E.Super!=NativeParents[I+1].Class.Get()))
                return Mismatch(E.Path,TEXT("native_parent_role"),TEXT("held native class/super/layout/package/existing-default slots"),TEXT("role changed"));
            if(D&&(!Loaded(D)||D->GetClass()!=K||D->GetPackage()!=ParentPackage||!Loaded(E.DefaultOuter.Get())||D->GetOuter()!=E.DefaultOuter.Get()||D->GetFlags()!=E.DefaultFlags||D->GetPathName()!=E.DefaultPath))return Mismatch(E.Path,TEXT("native_parent_CDO"),E.DefaultPath,TEXT("default identity/owner/class/flags changed"));
        }
        for(const auto& E:GeneratedParents)
        {
            auto* Source=E.Blueprint.Get();auto* SourceClass=E.Generated.Get();auto* Skeleton=E.Skeleton.Get();
            if(!Loaded(Source)||Source->GetClass()!=UBlueprint::StaticClass()||Source->GetOuter()!=Source->GetPackage()||Source->GetPathName()!=E.Path||Source->GetFlags()!=E.Flags||Source->BlueprintType!=BPTYPE_Normal||Source->Status!=E.Status||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad||Source->bSuppressStructurallyModified||Source->GetPackage()->IsDirty()||!Source->GeneratedClass.GetGCPtr().IsResolved()||Source->GeneratedClass.GetGCPtr()!=SourceClass||!Loaded(SourceClass)||!SourceClass->ClassGeneratedBy.IsResolved()||SourceClass->ClassGeneratedBy!=Source||!Source->ParentClass.GetGCPtr().IsResolved()||!ResolvedParentSuperMatchesSlot(SourceClass,Source->ParentClass.GetGCPtr())||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved()||Source->SkeletonGeneratedClass.GetGCPtr()!=Skeleton||!Loaded(Skeleton)||Skeleton->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Skeleton->GetOuter()!=Source->GetPackage()||Skeleton->GetPathName()!=E.SkeletonPath||Skeleton->GetFlags()!=E.SkeletonFlags||!Skeleton->ClassGeneratedBy.IsResolved()||Skeleton->ClassGeneratedBy!=Source||!ResolvedParentSuperMatches(Skeleton,E.SkeletonSuper.Get()))
                return Mismatch(E.Path,TEXT("generated_parent_source_role"),TEXT("held saved-clean source Blueprint/generated/skeleton/actual parent roles"),TEXT("source role changed"));
            TArray<UEdGraph*> Actual;Source->GetAllGraphs(Actual);if(!Loaded(Source)||!Loaded(SourceClass)||SourceClass->ClassGeneratedBy!=Source||Source->GeneratedClass.GetGCPtr()!=SourceClass||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad)return Mismatch(E.Path,TEXT("source_graph_enumeration_return"),TEXT("same loaded source/class/compiler role"),TEXT("source changed"));if(Actual.Num()!=E.Graphs.Num())return Mismatch(E.Path,TEXT("source_graph_membership"),LexToString(E.Graphs.Num()),LexToString(Actual.Num()));
            for(const auto& SourceGraph:E.Graphs)
            {
                auto* Graph=SourceGraph.Graph.Get();if(!Actual.Contains(Graph)||!Loaded(Graph)||!Loaded(SourceGraph.Class.Get())||Graph->GetClass()!=SourceGraph.Class.Get()||!Loaded(SourceGraph.Outer.Get())||Graph->GetOuter()!=SourceGraph.Outer.Get()||Graph->GetPackage()!=Source->GetPackage()||Graph->GetPathName()!=SourceGraph.Path||Graph->GetFlags()!=SourceGraph.Flags||!Graph->Schema.GetGCPtr().IsResolved()||Graph->Schema.GetGCPtr()!=SourceGraph.Schema.Get()||!Loaded(SourceGraph.Schema.Get())||Graph->Nodes.Num()!=SourceGraph.Nodes.Num())return Mismatch(E.Path,TEXT("source_graph_role"),SourceGraph.Path,TEXT("graph identity/owner/schema/node membership changed"));
                for(const auto& N:SourceGraph.Nodes){auto* Node=N.Node.Get();if(!Graph->Nodes.Contains(Node)||!Loaded(Node)||!Loaded(N.Class.Get())||Node->GetClass()!=N.Class.Get()||Node->GetOuter()!=Graph||Node->GetGraph()!=Graph||Node->GetPathName()!=N.Path||Node->GetFlags()!=N.Flags)return Mismatch(SourceGraph.Path,TEXT("source_node_role"),N.Path,TEXT("node identity/owner/membership changed"));}
            }
        }
        return true;
    }
};
struct FObservation { bool Complete = false; TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), Disk = MakeShared<FJsonObject>(), Other = MakeShared<FJsonObject>(); };
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Phase)
{
    O = FObservation{}; Count(R, TEXT("observation_capture_calls")); auto J = MakeShared<FJsonObject>(); R.Data->SetObjectField(Phase, J);
    J->SetBoolField(TEXT("complete"), false); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_source"), O.Source); J->SetObjectField(TEXT("disk"), O.Disk); J->SetObjectField(TEXT("other_package_dirty"), O.Other);
    UBlueprint* B = C.Blueprint.Get(); APawn* P = C.CDO.Get();
    if (!C.ParentCurrent(R)) { return false; }
    const TFunction<bool()> Current=[&]{return C.ParentCurrent(R);};
    if (!Loaded(B) || !Loaded(P) || !Loaded(B->GeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetDefaultObject(false) != P || P->GetClass() != B->GeneratedClass || B->GetPathName() != C.Path) { return false; }
    O.State->SetObjectField(TEXT("blueprint_identity"), Identity(B)); O.State->SetObjectField(TEXT("cdo_identity"), Identity(P)); O.State->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty());
    O.State->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(B->Status)); O.State->SetBoolField(TEXT("cached_dependencies_up_to_date"), B->bCachedDependenciesUpToDate); O.State->SetObjectField(TEXT("selected_values"), Values(P));
    auto Objects = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("unselected_owned_fields"), Objects); TArray<UObject*> Owned, Children;
    Owned.Add(B); Owned.Add(P); GetObjectsWithOuter(B, Children, EGetObjectsFlags::IncludeNestedObjects); Owned.Append(Children); Children.Reset(); GetObjectsWithOuter(P, Children, EGetObjectsFlags::IncludeNestedObjects); Owned.Append(Children);
    if (B->SimpleConstructionScript)
    {
        Owned.AddUnique(B->SimpleConstructionScript); for (USCS_Node* N : B->SimpleConstructionScript->GetAllNodes()) { if (!Loaded(N) || !Loaded(N->ComponentTemplate)) { return false; } Owned.AddUnique(N); Owned.AddUnique(N->ComponentTemplate); }
    }
    if (Owned.Num() > 512) { return false; }
    TArray<TStrongObjectPtr<UObject>> OwnedHolds;if(C.ExtendedNativeParent){for(UObject* X:Owned){if(!Loaded(X))return false;OwnedHolds.Emplace(X);}if(!C.ParentCurrent(R))return false;}
    for (UObject* X : Owned)
    {
        if (!Loaded(X) || X->GetOutermost() != B->GetOutermost() || !Clean(X->GetPathName()) || !Clean(X->GetClass()->GetPathName())) { return false; }
        auto Row = MakeShared<FJsonObject>(); Objects->SetObjectField(X->GetPathName(), Row); Row->SetObjectField(TEXT("identity"), Identity(X)); auto F = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("fields"), F);
        if (!Fields(X, X == B, X == P, F, C.ExtendedNativeParent ? &Current : nullptr)) { return false; }
    }
    TArray<UClass*> Classes = { AActor::StaticClass(), APawn::StaticClass() }; Classes.AddUnique(B->ParentClass);
    if(C.ExtendedNativeParent){Classes.Reset();for(const auto& E:C.NativeParents)Classes.Add(E.Class.Get());}
    for (UClass* Class : Classes)
    {
        if (!C.ParentCurrent(R) || !Loaded(Class) || (!C.ExtendedNativeParent && Class->GetOutermost()->GetName() != TEXT("/Script/Engine")) || (!Class->HasAnyClassFlags(CLASS_Native)&&!C.GeneratedParents.ContainsByPredicate([&](const FContext::FGeneratedParent& E){return E.Generated.Get()==Class;}))) { return false; }
        auto Row = Identity(Class); O.Source->SetObjectField(Class->GetPathName(), Row); Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint64>(Class->GetClassFlags()))); Row->SetBoolField(TEXT("package_dirty"), Class->GetOutermost()->IsDirty());
        UObject* Default = Class->HasAnyClassFlags(CLASS_Native)?Class->GetDefaultObject(false):ResolvedSourcePointer(Class->ClassDefaultObject);if(!C.ParentCurrent(R))return false; Row->SetBoolField(TEXT("existing_cdo_available"), Default != nullptr);
        if (Default) { if (!Loaded(Default)) { return false; } Row->SetObjectField(TEXT("cdo_identity"), Identity(Default)); auto F = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("cdo_fields"), F); if (!Fields(Default, false, false, F, C.ExtendedNativeParent ? &Current : nullptr)) { return false; } }
    }
    for(const auto& E:C.GeneratedParents)
    {
        auto SourceRow=MakeShared<FJsonObject>();O.Source->SetObjectField(E.Path,SourceRow);SourceRow->SetObjectField(TEXT("blueprint_identity"),Identity(E.Blueprint.Get()));SourceRow->SetObjectField(TEXT("skeleton_identity"),Identity(E.Skeleton.Get()));TArray<TSharedPtr<FJsonValue>> Graphs;for(const auto& G:E.Graphs){auto Row=Identity(G.Graph.Get());TArray<TSharedPtr<FJsonValue>> Nodes;for(const auto& N:G.Nodes)Nodes.Add(MakeShared<FJsonValueObject>(Identity(N.Node.Get())));Row->SetArrayField(TEXT("node_membership"),Nodes);Graphs.Add(MakeShared<FJsonValueObject>(Row));}SourceRow->SetArrayField(TEXT("graph_membership"),Graphs);
        FString File;if(!FPackageName::TryConvertLongPackageNameToFilename(E.Blueprint->GetPackage()->GetName(),File,FPackageName::GetAssetPackageExtension()))return false;File=FPaths::ConvertRelativePathToFull(File);const int64 Size=IFileManager::Get().FileSize(*File);if(Size<=0||Size>67108864)return false;TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size)return false;uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash);SourceRow->SetStringField(TEXT("source_file"),File);SourceRow->SetNumberField(TEXT("source_bytes"),Size);SourceRow->SetStringField(TEXT("source_sha1"),BytesToHex(Hash,FSHA1::DigestSize));
    }
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(C.Package, Filename, FPackageName::GetAssetPackageExtension())) { return false; } Filename = FPaths::ConvertRelativePathToFull(Filename);
    const int64 Size = IFileManager::Get().FileSize(*Filename); O.Disk->SetStringField(TEXT("path"), Filename); O.Disk->SetBoolField(TEXT("exists"), Size >= 0); O.Disk->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size)); O.Disk->SetStringField(TEXT("sha1"), FString());
    if (Size > 67108864) { return false; } if (Size >= 0) { TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; } uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); O.Disk->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); }
    int32 Packages = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == C.Package) { continue; } if (++Packages > 8192) { return false; } O.Other->SetBoolField(It->GetName(), It->IsDirty()); }
    if(!C.ParentCurrent(R))return false;O.Complete = true; J->SetBoolField(TEXT("complete"), true); return true;
}
bool Prepare(const FString& Requested, FContext& C, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("pawn_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); }
    if (Requested.IsEmpty() || Requested.Len() > 2048 || HasNul(Requested)) { return Fail(R, TEXT("pawn_blueprint_path_invalid"), TEXT("Bounded original Blueprint name or Game object/package path required.")); }
    C.Path = FPackageName::ExportTextPathToObjectPath(Requested).TrimStartAndEnd(); C.Path.TrimQuotesInline(); C.Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!C.Path.Contains(TEXT("/")) && !C.Path.Contains(TEXT("."))) { C.Path = TEXT("/Game/Blueprints/") + C.Path; }
    if (!C.Path.Contains(TEXT("."))) { C.Path += TEXT(".") + FPackageName::GetShortName(C.Path); } C.Package = FPackageName::ObjectPathToPackageName(C.Path);
    if (C.Path.Len() >= 1024 || !FPackageName::IsValidObjectPath(C.Path) || !C.Package.StartsWith(TEXT("/Game/")) || C.Path.Contains(TEXT(":")) || !Clean(C.Path)) { return Fail(R, TEXT("pawn_blueprint_path_invalid"), TEXT("Exact Game Blueprint identity required; global name search/level Blueprint resolution remains incomplete.")); }
    auto Call = MakeShared<FJsonObject>(); Call->SetStringField(TEXT("asset_path"), C.Path); Call->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::LoadAsset")); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false);
    R.Data->SetArrayField(TEXT("asset_load_calls"), { MakeShared<FJsonValueObject>(Call) }); Count(R, TEXT("asset_load_count")); R.Stage = TEXT("load_blueprint"); UObject* Asset = UEditorAssetLibrary::LoadAsset(C.Path); Call->SetBoolField(TEXT("returned"), true); Call->SetObjectField(TEXT("returned_object"), Identity(Asset)); C.Blueprint.Reset(Cast<UBlueprint>(Asset));
    UBlueprint* B = C.Blueprint.Get(); R.Data->SetStringField(TEXT("asset_path"), C.Path); R.Data->SetStringField(TEXT("change_accounting_scope"), TEXT("Selected authoring observations begin after LoadAsset; load/PostLoad and implicit library/class/default initialization are separately disclosed, not certified absent by changed=False."));
    if (!Loaded(B)) { return Fail(R, TEXT("pawn_blueprint_not_found"), FString::Printf(TEXT("Blueprint not found: %s"), *Requested), true); }
    if(B->ParentClass.GetGCPtr().IsResolved())
    { auto* Parent=Cast<UClass>(ResolvedSourcePointer(B->ParentClass.GetGCPtr()));C.GeneratedParentProfile=Loaded(Parent)&&!Parent->HasAnyClassFlags(CLASS_Native); }
    UClass* TargetSuper=nullptr;
    if(C.GeneratedParentProfile)
    {auto* TargetClass=Cast<UClass>(ResolvedSourcePointer(B->GeneratedClass.GetGCPtr()));if(!Loaded(TargetClass)||!TryResolvedParentSuper(TargetClass,TargetSuper))return Fail(R,TEXT("pawn_generated_superclass_unavailable"),TEXT("Resolved target superclass observation required; observation failure is not a null superclass."));}
    if (B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->GetOuter() != B->GetOutermost() || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor | PKG_EditorOnly) || (!B->ParentClass.GetGCPtr().IsResolved() || !Loaded(B->ParentClass.Get()) || !B->ParentClass->IsChildOf(APawn::StaticClass()))
        || !Loaded(B->GeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || (C.GeneratedParentProfile?TargetSuper!=B->ParentClass.GetGCPtr():B->GeneratedClass->GetSuperClass()!=B->ParentClass) || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || B->Status == BS_BeingCreated)
    { return Fail(R, TEXT("pawn_blueprint_scope_unsupported"), TEXT("Current normal Blueprint with loaded native Pawn parent required; generated-parent/level variants remain incomplete.")); }
    if(B->ParentClass.Get()!=APawn::StaticClass()&&B->ParentClass.Get()!=ACharacter::StaticClass()&&!B->GeneratedClass->ClassDefaultObject.IsResolved())return Fail(R,TEXT("pawn_cdo_unresolved"),TEXT("Already resolved existing target CDO required; no implicit default-slot resolution."));
    APawn* P = Cast<APawn>(B->GeneratedClass->GetDefaultObject(false)); C.CDO.Reset(P);
    if (!Loaded(P) || P->GetClass() != B->GeneratedClass || P->GetOuter() != B->GeneratedClass->GetOuter() || !P->HasAnyFlags(RF_ClassDefaultObject)) { return Fail(R, TEXT("pawn_cdo_unavailable"), TEXT("Existing current own generated Pawn CDO required; no default-object warm/create.")); }
    C.ExtendedNativeParent=B->ParentClass.Get()!=APawn::StaticClass()&&B->ParentClass.Get()!=ACharacter::StaticClass();
    if(C.ExtendedNativeParent)
    {
        C.Generated.Reset(B->GeneratedClass.Get());TSet<UClass*> Seen;bool ActorSeen=false;
        UClass* NextParent=nullptr;
        for(UClass* K=B->ParentClass.Get();K;K=C.GeneratedParentProfile?NextParent:K->GetSuperClass())
        {
            if(Seen.Contains(K)||C.NativeParents.Num()>=32||!Loaded(K)||!Clean(K->GetPathName())||!K->ClassDefaultObject.IsResolved()||!Loaded(K->GetPackage())||(K->HasAnyClassFlags(CLASS_Native)&&!K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)))return Fail(R,TEXT("pawn_native_parent_source_unavailable"),TEXT("Finite already-loaded native parent chain and resolved existing default slots required; no source loader/default creation."));
            Seen.Add(K);FContext::FNativeParent E;E.Class.Reset(K);E.ClassOuter.Reset(K->GetOuter());E.Default.Reset(K->HasAnyClassFlags(CLASS_Native)?K->GetDefaultObject(false):ResolvedSourcePointer(K->ClassDefaultObject));if(!Loaded(K)||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject!=E.Default.Get())return Fail(R,TEXT("pawn_parent_default_return_changed"),TEXT("Current held class/default role required immediately after source default observation."));E.Package.Reset(K->GetPackage());if(C.GeneratedParentProfile){if(!TryResolvedParentSuper(K,E.Super))return Fail(R,TEXT("pawn_generated_superclass_unavailable"),TEXT("Resolved parent superclass observation required; observation failure cannot end the ancestry chain."));NextParent=E.Super;}else E.Super=K->GetSuperClass();E.Path=K->GetPathName();E.Flags=K->GetFlags();E.ClassFlags=K->GetClassFlags();E.Size=K->GetPropertiesSize();E.Alignment=K->GetMinAlignment();E.PackageFlags=uint32(K->GetPackage()->GetPackageFlags());E.Dirty=K->GetPackage()->IsDirty();
            if(auto* D=E.Default.Get()){if(!Loaded(D)||D->GetClass()!=K||D->GetPackage()!=K->GetPackage()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Fail(R,TEXT("pawn_native_parent_default_unavailable"),TEXT("Existing native class-owned CDO role required; no default creation."));E.DefaultOuter.Reset(D->GetOuter());E.DefaultPath=D->GetPathName();E.DefaultFlags=D->GetFlags();}
            if(!K->HasAnyClassFlags(CLASS_Native))
            {
                if(K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||!K->ClassGeneratedBy.IsResolved())return Fail(R,TEXT("pawn_generated_parent_class_pending"),TEXT("Exact current generated parent class required; stale/custom class families remain incomplete."));
                auto* Source=Cast<UBlueprint>(ResolvedSourcePointer(K->ClassGeneratedBy));
                if(!Loaded(Source)||Source==B||Source->GetClass()!=UBlueprint::StaticClass()||Source->GetOuter()!=K->GetPackage()||Source->GetPackage()==B->GetPackage()||!Source->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||Source->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)||Source->GetPackage()->IsDirty()||(Source->Status!=BS_UpToDate&&Source->Status!=BS_UpToDateWithWarnings)||Source->BlueprintType!=BPTYPE_Normal||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad||Source->bSuppressStructurallyModified||!Source->GeneratedClass.GetGCPtr().IsResolved()||Source->GeneratedClass.GetGCPtr()!=K||!Source->ParentClass.GetGCPtr().IsResolved()||Source->ParentClass.GetGCPtr()!=E.Super||!Source->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Fail(R,TEXT("pawn_generated_parent_source_pending"),TEXT("Loaded saved-clean normal Game source Blueprint with actual current generated/parent/skeleton roles required."));
                auto* Skeleton=Cast<UClass>(ResolvedSourcePointer(Source->SkeletonGeneratedClass.GetGCPtr()));UClass* SkeletonSuper=nullptr;if(!Loaded(Skeleton)||!TryResolvedParentSuper(Skeleton,SkeletonSuper)||Skeleton->GetClass()!=UBlueprintGeneratedClass::StaticClass()||Skeleton->GetOuter()!=Source->GetPackage()||!Skeleton->ClassGeneratedBy.IsResolved()||Skeleton->ClassGeneratedBy!=Source||(SkeletonSuper&&!Loaded(SkeletonSuper)))return Fail(R,TEXT("pawn_generated_parent_skeleton_pending"),TEXT("Existing owned skeleton and its actual loaded superclass required; no skeleton creation/compile."));
                FContext::FGeneratedParent GP;GP.Blueprint.Reset(Source);GP.Generated.Reset(K);GP.Skeleton.Reset(Skeleton);GP.SkeletonSuper.Reset(SkeletonSuper);GP.Path=Source->GetPathName();GP.Flags=Source->GetFlags();GP.Status=Source->Status;GP.SkeletonPath=Skeleton->GetPathName();GP.SkeletonFlags=Skeleton->GetFlags();
                TArray<UEdGraph*> Graphs;Source->GetAllGraphs(Graphs);if(!Loaded(Source)||!Loaded(K)||K->ClassGeneratedBy!=Source||Source->GeneratedClass.GetGCPtr()!=K||Source->bBeingCompiled||Source->bQueuedForCompilation||Source->bIsRegeneratingOnLoad)return Fail(R,TEXT("pawn_generated_parent_enumeration_changed"),TEXT("Source identity/compiler role changed while GetAllGraphs returned; no source graph dereference."));if(Graphs.Num()>64)return Fail(R,TEXT("pawn_generated_parent_graph_bound"),TEXT("Source graph inventory exceeds64; no source graph is modified."));int32 Nodes=0;TSet<UEdGraph*> SeenGraphs;
                for(auto* Graph:Graphs)
                {
                    if(!Loaded(Graph)||SeenGraphs.Contains(Graph)||!Loaded(Graph->GetClass())||!Loaded(Graph->GetOuter())||Graph->GetPackage()!=Source->GetPackage()||!Graph->Schema.GetGCPtr().IsResolved()||!Loaded(ResolvedSourcePointer(Graph->Schema.GetGCPtr()))||(Nodes+=Graph->Nodes.Num())>2048)return Fail(R,TEXT("pawn_generated_parent_graph_pending"),TEXT("Bounded current source graph identity/owner/schema membership required."));SeenGraphs.Add(Graph);
                    FContext::FSourceGraph G;G.Graph.Reset(Graph);G.Outer.Reset(Graph->GetOuter());G.Class.Reset(Graph->GetClass());G.Schema.Reset(Cast<UClass>(ResolvedSourcePointer(Graph->Schema.GetGCPtr())));G.Path=Graph->GetPathName();G.Flags=Graph->GetFlags();TSet<UEdGraphNode*> SeenNodes;
                    for(const auto& Slot:Graph->Nodes){if(!Slot.IsResolved())return Fail(R,TEXT("pawn_generated_parent_node_unresolved"),TEXT("No source node handle is resolved by this API."));auto* Node=Cast<UEdGraphNode>(ResolvedSourcePointer(Slot));if(!Loaded(Node)||SeenNodes.Contains(Node)||!Loaded(Node->GetClass())||Node->GetOuter()!=Graph||Node->GetGraph()!=Graph)return Fail(R,TEXT("pawn_generated_parent_node_pending"),TEXT("Actual owned source node inventory required."));SeenNodes.Add(Node);FContext::FSourceGraph::FNode N;N.Node.Reset(Node);N.Class.Reset(Node->GetClass());N.Path=Node->GetPathName();N.Flags=Node->GetFlags();G.Nodes.Add(MoveTemp(N));}GP.Graphs.Add(MoveTemp(G));
                }
                C.GeneratedParents.Add(MoveTemp(GP));
            }
            C.NativeParents.Add(MoveTemp(E));if(K==AActor::StaticClass()){ActorSeen=true;break;}
        }
        if(!ActorSeen||!C.ParentCurrent(R))return Fail(R,TEXT("pawn_native_parent_chain_incomplete"),TEXT("Current native Pawn-to-Actor source chain required."));
        if(!C.GeneratedParents.IsEmpty())
        {
            TArray<FString> Packages;for(const auto& E:C.GeneratedParents)Packages.AddUnique(E.Blueprint->GetPackage()->GetName());if(!C.ParentCurrent(R))return Fail(R,TEXT("pawn_generated_parent_current_lost"),TEXT("Source role changed before dependency audit."),true);
            const auto Dependencies=ReadPackageDependencies(Packages,true,256);R.Data->SetObjectField(TEXT("generated_parent_dependencies"),Dependencies);if(!C.ParentCurrent(R))return Fail(R,TEXT("pawn_generated_parent_current_lost"),TEXT("Source role changed during dependency audit; no writer."),true);
            if(!Dependencies->GetBoolField(TEXT("success"))||!Dependencies->GetBoolField(TEXT("passed")))return Fail(R,TEXT("pawn_generated_parent_dependencies_pending"),TEXT("Existing saved source dependencies must be fully known and free of Cubeless/MCP tooling references."));
            R.Data->SetBoolField(TEXT("generated_parent_chain_admitted"),true);R.Data->SetNumberField(TEXT("generated_parent_source_count"),C.GeneratedParents.Num());R.Data->SetBoolField(TEXT("generated_parent_full_authored_semantics_observed"),false);R.Data->SetStringField(TEXT("generated_parent_scope"),TEXT("Loaded saved-clean normal Game generated Pawn parent chain and existing source CDOs; readonly BP/generated/skeleton/actual superclass/graph-node identity-membership, source package dirty/file and dependency observations. No source graph execution/compile/default creation/loader/save; full per-node semantics and cold/live/derived propagation remain incomplete."));
        }
        R.Data->SetBoolField(TEXT("native_parent_chain_admitted"),C.GeneratedParents.IsEmpty());R.Data->SetNumberField(TEXT("native_parent_chain_count"),C.NativeParents.Num()-C.GeneratedParents.Num());R.Data->SetNumberField(TEXT("parent_chain_count"),C.NativeParents.Num());R.Data->SetBoolField(TEXT("native_parent_source_default_creation"),false);
    }
    for (TObjectIterator<UBlueprintGeneratedClass> It; It; ++It) { if (Loaded(*It) && *It != B->GeneratedClass && *It != B->SkeletonGeneratedClass && It->IsChildOf(B->GeneratedClass)) { return Fail(R, TEXT("pawn_derived_class_unsupported"), TEXT("Loaded derived class propagation is outside the selected callback scope.")); } }
    for (TObjectIterator<APawn> It; It; ++It) { if (Loaded(*It) && !It->IsTemplate() && It->IsA(B->GeneratedClass)) { return Fail(R, TEXT("pawn_live_instance_unsupported"), TEXT("Loaded selected live Pawn instance is outside defaults-only scope.")); } }
    const auto Auto = FindFProperty<FByteProperty>(P->GetClass(), PropertyNames[0]);
    if (!Auto || Auto->GetOwnerStruct() != APawn::StaticClass() || !Auto->GetIntPropertyEnum() || Auto->GetIntPropertyEnum()->GetPathName() != TEXT("/Script/Engine.EAutoReceiveInput")) { return Fail(R, TEXT("pawn_property_layout_unsupported"), TEXT("Exact public native Pawn enum layout required.")); }
    for (int32 I = 1; I < 5; ++I) { const auto Flag = FindFProperty<FBoolProperty>(P->GetClass(), PropertyNames[I]); if (!Flag || Flag->ArrayDim != 1 || Flag->GetOwnerStruct() != (I == 4 ? AActor::StaticClass() : APawn::StaticClass())) { return Fail(R, TEXT("pawn_property_layout_unsupported"), TEXT("Exact native Pawn/Actor boolean field layout required.")); } }
    return true;
}
struct FInput { bool Present = false, Valid = true, Flag = false; uint8 Byte = 0; FString Error; };
FInput OptionalBool(const FString& Json)
{
    FInput I; TArray<TSharedPtr<FJsonValue>> Values;
    if (Json.Len() > 64 || HasNul(Json) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Values) || Values.Num() != 1 || !Values[0].IsValid()) { I.Present = true; I.Valid = false; I.Error = TEXT("Optional Pawn flag requires one JSON null or exact boolean"); return I; }
    I.Present = Values[0]->Type != EJson::Null; I.Valid = !I.Present || Values[0]->Type == EJson::Boolean;
    if (!I.Valid) { I.Error = TEXT("Optional Pawn flag requires one JSON null or exact boolean"); } else if (I.Present) { I.Flag = Values[0]->AsBool(); } return I;
}
bool ResolveAuto(const FString& Text, UEnum* Enum, uint8& Out, FString& Error)
{
    if (Text.Len() > 128 || HasNul(Text)) { Error = TEXT("AutoPossessPlayer enum string exceeds the finite input bound"); return false; }
    if (Text.IsNumeric())
    {
        double Number = 0; if (!LexTryParseString(Number, *Text) || !FMath::IsFinite(Number) || Number < MIN_int32 || Number > MAX_int32) { Error = TEXT("Numeric enum string must remain within the original int32 conversion budget"); return false; }
        Out = static_cast<uint8>(FCString::Atoi(*Text)); return true; // Preserve original safe numeric-string byte wrapping/truncation.
    }
    FString Short = Text; if (Short.Contains(TEXT("::"))) { Short.Split(TEXT("::"), nullptr, &Short); }
    int64 Value = Enum->GetValueByNameString(Short); if (Value == INDEX_NONE) { Value = Enum->GetValueByNameString(Text); }
    if (Value == INDEX_NONE || Value < 0 || Value > MAX_uint8) { Error = FString::Printf(TEXT("Could not find enum value for '%s'"), *Short); return false; } Out = static_cast<uint8>(Value); return true;
}
}
static Variable::FResult SetPropertiesImpl(const FString& Path, const FString& Auto, const FString& Yaw, const FString& Pitch, const FString& Roll, const FString& Damage, const FJsonObject* RawParams)
{
    FResult R; Init(R); R.Data->SetStringField(TEXT("blueprint"), Path); R.Data->SetStringField(TEXT("blueprint_name"), Path);
    FInput Inputs[5]; Inputs[0].Present = !Auto.IsEmpty(); Inputs[1] = OptionalBool(Yaw); Inputs[2] = OptionalBool(Pitch); Inputs[3] = OptionalBool(Roll); Inputs[4] = OptionalBool(Damage);
    if (RawParams)
    {
        Inputs[0].Present = RawParams->HasField(TEXT("auto_possess_player"));
        const TCHAR* Keys[] = { TEXT("use_controller_rotation_yaw"), TEXT("use_controller_rotation_pitch"), TEXT("use_controller_rotation_roll"), TEXT("can_be_damaged") };
        for (int32 I = 1; I < 5; ++I)
        {
            Inputs[I] = FInput{}; Inputs[I].Present = RawParams->HasField(Keys[I - 1]);
            const auto Value = RawParams->Values.FindRef(Keys[I - 1]);
            // Original BoolProperty uses AsBool: unsuccessful conversion still
            // applies false, including explicit null/array/object. No pre-abort.
            if (Inputs[I].Present && Value.IsValid()) { Value->TryGetBool(Inputs[I].Flag); }
        }
        R.Data->SetBoolField(TEXT("raw_native_entry"), true);
        R.Data->SetBoolField(TEXT("raw_bool_conversion_fallback_false_preserved"), true);
        R.Data->SetBoolField(TEXT("raw_full_legacy_scope_complete"), false);
        R.Data->SetStringField(TEXT("legacy_result_scope"), TEXT("Original raw HasField inputs including null, ordered per-property failure/any-success, and no-properties error after target admission. Existing finite source/CDO/byte bounds remain incomplete; no compile/save or replay."));
    }
    int32 Requested = 0; for (const auto& I : Inputs) { Requested += I.Present ? 1 : 0; } R.Data->SetNumberField(TEXT("property_request_count"), Requested);
    if (!Requested && !RawParams) { R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("no_properties"); R.Data->SetStringField(TEXT("message"), TEXT("No properties specified to set")); R.Data->SetBoolField(TEXT("legacy_success"), true); return R; }
    FContext C; if (!Prepare(Path, C, R)) { return R; }
    if (!Requested && RawParams) { Fail(R, TEXT("pawn_no_properties_specified"), TEXT("No properties specified to set")); return R; }
    FObservation Before, After;
    R.Stage = TEXT("before_observation"); if (!Capture(C, Before, R, TEXT("before_observation"))) { Fail(R, TEXT("pawn_before_unobservable"), TEXT("Bounded before prefix is incomplete; no property application."), true); return R; }
    auto Desired = Values(C.CDO.Get()); auto Results = R.Data->GetObjectField(TEXT("results")); TArray<TSharedPtr<FJsonValue>> Calls; int32 Accepted = 0, Failed = 0;
    const FScopedTransaction Outer(NSLOCTEXT("CubelessMCP", "PawnProperties", "MCP set_pawn_properties")); Count(R, TEXT("outer_transaction_calls"));
    for (int32 I = 0; I < 5; ++I)
    {
        if (!Inputs[I].Present) { continue; } auto Row = MakeShared<FJsonObject>(); Results->SetObjectField(PropertyNames[I], Row); Row->SetBoolField(TEXT("success"), false);
        auto Call = MakeShared<FJsonObject>(); Calls.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("property_calls"), Calls); Call->SetNumberField(TEXT("order"), I); Call->SetStringField(TEXT("property"), PropertyNames[I]); Call->SetBoolField(TEXT("setter_attempted"), false); Call->SetBoolField(TEXT("setter_returned"), false);
        if (!I)
        {
            UEnum* Enum = FindFProperty<FByteProperty>(C.CDO->GetClass(), PropertyNames[0])->GetIntPropertyEnum();
            if (!RawParams) { Inputs[I].Valid = ResolveAuto(Auto, Enum, Inputs[I].Byte, Inputs[I].Error); Call->SetStringField(TEXT("requested"), Auto); }
            else
            {
                const auto Value = RawParams->Values.FindRef(TEXT("auto_possess_player"));
                if (Value.IsValid()) { Call->SetField(TEXT("requested_raw"), Value); }
                if (Value.IsValid() && Value->Type == EJson::Number)
                {
                    const double Number = Value->AsNumber();
                    // C++ float-to-uint8 is defined only after truncation fits.
                    Inputs[I].Valid = FMath::IsFinite(Number) && Number > -1.0 && Number < 256.0;
                    if (Inputs[I].Valid) { Inputs[I].Byte = static_cast<uint8>(Number); }
                    else { Inputs[I].Error = TEXT("Raw enum numeric narrowing outside the finite defined byte range remains incomplete"); R.Data->SetBoolField(TEXT("raw_enum_platform_narrowing_pending"), true); }
                }
                else if (Value.IsValid() && Value->Type == EJson::String)
                {
                    const FString Text = Value->AsString();
                    if (Text.IsNumeric() && Text.Len() <= 128 && !HasNul(Text))
                    {
                        const double Number = FCString::Atod(*Text);
                        Inputs[I].Valid = FMath::IsFinite(Number) && Number >= MIN_int32 && Number <= MAX_int32;
                        if (Inputs[I].Valid) { Inputs[I].Byte = static_cast<uint8>(FCString::Atoi(*Text)); }
                        else { Inputs[I].Error = TEXT("Raw numeric enum string narrowing outside the finite int32 budget remains incomplete"); R.Data->SetBoolField(TEXT("raw_enum_platform_narrowing_pending"), true); }
                    }
                    else { Inputs[I].Valid = ResolveAuto(Text, Enum, Inputs[I].Byte, Inputs[I].Error); }
                }
                else { Inputs[I].Valid = false; Inputs[I].Error = TEXT("Unsupported property type: ByteProperty for property AutoPossessPlayer"); }
            }
        }
        else if (RawParams)
        {
            const TCHAR* Key = I == 1 ? TEXT("use_controller_rotation_yaw") : I == 2 ? TEXT("use_controller_rotation_pitch") : I == 3 ? TEXT("use_controller_rotation_roll") : TEXT("can_be_damaged");
            // Preserve actual input type/value; Named JSON arguments are only carriers here.
            Call->SetField(TEXT("requested_raw"), RawParams->Values.FindRef(Key));
        }
        else { Call->SetStringField(TEXT("requested_json"), I == 1 ? Yaw : I == 2 ? Pitch : I == 3 ? Roll : Damage); }
        if (!Inputs[I].Valid) { ++Failed; Count(R, TEXT("property_validation_failures")); Row->SetStringField(TEXT("error"), Inputs[I].Error); Call->SetStringField(TEXT("error"), Inputs[I].Error); continue; }
        if(!C.ParentCurrent(R)){Fail(R,TEXT("pawn_native_parent_current_lost"),TEXT("Current target/source lease lost before property application; actual prefix retained."),true);return R;}
        R.Stage = TEXT("set_properties"); R.bMutationAttempted = true; R.bChangeStateKnown = false; Call->SetBoolField(TEXT("setter_attempted"), true); Count(R, TEXT("setter_attempt_count"));
        if (I == 0) { C.CDO->AutoPossessPlayer = static_cast<EAutoReceiveInput::Type>(Inputs[I].Byte); Desired->SetNumberField(PropertyNames[I], Inputs[I].Byte); }
        else { const bool V = Inputs[I].Flag; if (I == 1) { C.CDO->bUseControllerRotationYaw = V; } else if (I == 2) { C.CDO->bUseControllerRotationPitch = V; } else if (I == 3) { C.CDO->bUseControllerRotationRoll = V; } else { C.CDO->SetCanBeDamaged(V); } Desired->SetBoolField(PropertyNames[I], V); }
        Call->SetBoolField(TEXT("setter_returned"), true); Count(R, TEXT("setter_return_count"));
        if(!C.ParentCurrent(R)){Fail(R,TEXT("pawn_native_parent_current_lost"),TEXT("Current target/source lease lost after setter return; actual prefix retained."),true);return R;} ++Accepted; Count(R, TEXT("accepted_property_count")); Row->SetBoolField(TEXT("success"), true); Call->SetObjectField(TEXT("selected_values_after_return"), Values(C.CDO.Get()));
    }
    R.Data->SetNumberField(TEXT("failed_property_count"), Failed); R.Data->SetBoolField(TEXT("legacy_success"), Accepted > 0); R.Data->SetBoolField(TEXT("partial_success"), Accepted > 0 && Failed > 0); R.Data->SetBoolField(TEXT("all_requested_properties_succeeded"), Failed == 0); R.Data->SetObjectField(TEXT("desired_selected_values"), Desired);
    if (Accepted)
    {
        R.Stage = TEXT("mark_blueprint_modified"); Count(R, TEXT("mark_blueprint_modified_calls")); FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint.Get()); Count(R, TEXT("mark_blueprint_modified_returns"));
    }
    R.Stage = TEXT("after_observation"); if (!Capture(C, After, R, TEXT("after_observation"))) { Fail(R, TEXT("pawn_after_unobservable"), TEXT("Actual ordered property/mark prefix retained; final bounded observation incomplete, no replay/rollback."), true); return R; }
    R.bChangeStateKnown = true; R.bChanged = !Equal(Before.State, After.State) || !Equal(Before.Source, After.Source) || !Equal(Before.Disk, After.Disk) || !Equal(Before.Other, After.Other);
    const bool State = Equal(Before.State->GetObjectField(TEXT("unselected_owned_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("unselected_owned_fields")).ToSharedRef())
        && Equal(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef()) && Equal(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef());
    const bool Source = Equal(Before.Source, After.Source), Disk = Equal(Before.Disk, After.Disk), Other = Equal(Before.Other, After.Other), Selected = Equal(Desired, After.State->GetObjectField(TEXT("selected_values")).ToSharedRef());
    const bool Callback = Accepted ? After.State->GetBoolField(TEXT("package_dirty")) && After.State->GetNumberField(TEXT("blueprint_status")) == BS_Dirty && !After.State->GetBoolField(TEXT("cached_dependencies_up_to_date"))
        : Before.State->GetBoolField(TEXT("package_dirty")) == After.State->GetBoolField(TEXT("package_dirty")) && Before.State->GetNumberField(TEXT("blueprint_status")) == After.State->GetNumberField(TEXT("blueprint_status")) && Before.State->GetBoolField(TEXT("cached_dependencies_up_to_date")) == After.State->GetBoolField(TEXT("cached_dependencies_up_to_date"));
    R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("disk_preserved"), Disk); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("accepted_values_verified"), Selected);
    R.Data->SetBoolField(TEXT("mark_modified_state_verified"), Callback);
    if (!(State && Source && Disk && Other && Selected && Callback)) { Fail(R, TEXT("pawn_preservation_failed"), TEXT("Complete actual property/callback prefix differs from accepted values or declared preserved scope; no rollback/compile/save.")); return R; }
    if (!Accepted) { Fail(R, TEXT("pawn_no_property_applied"), TEXT("All specified properties failed; original per-property errors retained.")); return R; }
    R.bPostconditionPassed = true; R.bSuccess = true; R.Stage = TEXT("complete"); return R;
}

Variable::FResult SetProperties(const FString& Path, const FString& Auto, const FString& Yaw, const FString& Pitch, const FString& Roll, const FString& Damage)
{
    return SetPropertiesImpl(Path, Auto, Yaw, Pitch, Roll, Damage, nullptr);
}
Variable::FResult SetPropertiesRaw(const FString& Path, const FString& ParamsJson)
{
    FResult Rejected; Init(Rejected); TSharedPtr<FJsonObject> Params;
    if (ParamsJson.Len() > 1048576 || HasNul(ParamsJson) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ParamsJson), Params) || !Params.IsValid())
    { Fail(Rejected, TEXT("pawn_raw_params_invalid"), TEXT("Original finite bounded raw Pawn params object required before target load")); return Rejected; }
    return SetPropertiesImpl(Path, FString(), TEXT("null"), TEXT("null"), TEXT("null"), TEXT("null"), Params.Get());
}
}
