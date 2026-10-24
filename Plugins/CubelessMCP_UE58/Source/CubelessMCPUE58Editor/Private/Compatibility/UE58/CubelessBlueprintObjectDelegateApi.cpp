// DL native delegate bind, pure Create Event and connected interface Message; Editor only.
#include "Compatibility/UE58/CubelessBlueprintObjectDelegateApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include <cfloat>
namespace Cubeless::UE58::BlueprintObjectDelegate
{
namespace
{
using FResult=Function::FResult;
namespace Pin=Function::Pin;
constexpr int32 MaxGraphs=256,MaxNodes=2048;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }

bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }

bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }

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

TSharedRef<FJsonObject> Source(const UStruct* Struct,FResult& R)
{
    Count(R,TEXT("source_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Struct->GetPathName());J->SetStringField(TEXT("class"),Struct->GetClass()->GetPathName());J->SetStringField(TEXT("object_flags"),LexToString(static_cast<uint32>(Struct->GetFlags())));J->SetStringField(TEXT("super"),GetPathNameSafe(Struct->GetSuperStruct()));J->SetStringField(TEXT("package"),Struct->GetOutermost()->GetName());J->SetStringField(TEXT("package_flags"),LexToString(static_cast<uint32>(Struct->GetOutermost()->GetPackageFlags())));J->SetBoolField(TEXT("package_dirty"),Struct->GetOutermost()->IsDirty());
    if(const UClass* Class=Cast<UClass>(Struct)) { J->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(Class->GetClassFlags())));J->SetStringField(TEXT("generated_by"),GetPathNameSafe(Class->ClassGeneratedBy)); }
    if(const UScriptStruct* Script=Cast<UScriptStruct>(Struct)) { J->SetStringField(TEXT("struct_flags"),LexToString(static_cast<uint32>(Script->StructFlags))); }
    return J;
}


// ADG: saved generated Actor ancestry is an opt-in, not a replacement for the
// existing native/UMG path. Hold original source roles before calling the shared
// inspector. Only admission/final use its full dependency/disk observations;
// SDK callback fences below compare held identity/current state without loading
// classes, creating CDOs or freezing compiler/serialization caches.
struct FGeneratedBindingParent
{
    struct FRole
    {
        TStrongObjectPtr<UObject> Object;
        UObject* Outer=nullptr; UClass* Class=nullptr; UPackage* Package=nullptr;
        FString Path; EObjectFlags Flags=RF_NoFlags;
    };
    struct FAncestor
    {
        UBlueprint* Blueprint=nullptr; UClass* Generated=nullptr; UClass* Skeleton=nullptr;
        UClass* Parent=nullptr; UClass* SkeletonParent=nullptr;
        UObject* CDO=nullptr; UObject* SkeletonCDO=nullptr; USimpleConstructionScript* SCS=nullptr;
        EBlueprintStatus Status=BS_Unknown; EClassFlags ClassFlags=CLASS_None; EClassFlags SkeletonFlags=CLASS_None;
    };
    UBlueprint* Target=nullptr; UClass* Parent=nullptr;
    TStrongObjectPtr<UEdGraph> GraphHold;
    TStrongObjectPtr<UEditorEngine> EditorHold;
    UObject* GraphOuter=nullptr;UClass* GraphClass=nullptr;UPackage* GraphPackage=nullptr;
    UClass* GraphSchemaClass=nullptr;FString GraphPath;FGuid GraphGuid;
    TArray<FRole> Roles; TArray<FAncestor> Ancestors;
    TSharedPtr<FJsonObject> Before;
    bool Active=false;bool Stopped=false;

    bool Mismatch(FResult& R,const TCHAR* Boundary,const FString& Object,const TCHAR* Field,const FString& Expected,const FString& Actual)
    {
        Stopped=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("boundary"),Boundary);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);
        if(!R.Data->HasField(TEXT("generated_binding_parent_first_mismatch")))R.Data->SetObjectField(TEXT("generated_binding_parent_first_mismatch"),J);
        R.bChangeStateKnown=false;
        return Fail(R,TEXT("binding_generated_parent_changed"),TEXT("Original generated Actor parent roles changed at an SDK boundary. Actual operation prefix retained; no cleanup, compile/save or retry follows this mismatch."));
    }
    bool Keep(UObject* O)
    {
        if(!Loaded(O))return false;
        for(const auto& E:Roles)if(E.Object.Get()==O)return true;
        FRole E;E.Object.Reset(O);E.Outer=O->GetOuter();E.Class=O->GetClass();E.Package=O->GetOutermost();E.Path=O->GetPathName();E.Flags=O->GetFlags();Roles.Add(MoveTemp(E));return true;
    }
    template<class T,class U> static bool Same(const TObjectPtr<T>& Slot,U* Expected)
    { return Slot.IsResolved()&&Slot.GetHandle()==TObjectPtr<T>(Expected).GetHandle(); }
    // Public resolved path metadata does not invoke tracked OnHandleRead. Hold
    // the exact resident object before any later SDK observer can reenter.
    template<class T> bool HoldSlot(const TObjectPtr<T>& Slot,T*& Out)
    {
        Out=nullptr;if(!Slot.IsResolved())return false;
        if(Same(Slot,static_cast<T*>(nullptr)))return true;
        const FString Path=Slot.GetPathName();auto* Found=StaticFindObject(nullptr,nullptr,*Path);
        if(!Loaded(Found)||!Found->IsA<T>()||!Same(Slot,static_cast<T*>(Found))||!Keep(Found))return false;
        Out=static_cast<T*>(Found);return true;
    }
    bool Super(UClass* C,UClass*& Out)
    {
        Out=nullptr;if(!Loaded(C))return false;
        static FObjectProperty* const P=FindFProperty<FObjectProperty>(UStruct::StaticClass(),TEXT("SuperStruct"));
        if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=UStruct::StaticClass()||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->ElementSize!=sizeof(TObjectPtr<UStruct>)||P->GetSize()!=sizeof(TObjectPtr<UStruct>)||P->GetMinAlignment()!=alignof(TObjectPtr<UStruct>)||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>UStruct::StaticClass()->GetPropertiesSize()||P->HasSetterOrGetter()||!Same(P->PropertyClass,UStruct::StaticClass()))return false;
        const TObjectPtr<UObject> Slot=P->GetObjectPtrPropertyValue(P->ContainerPtrToValuePtr<void>(C));
        if(!Slot.IsResolved())return false;if(Same(Slot,static_cast<UObject*>(nullptr)))return true;
        auto* Found=StaticFindObject(nullptr,nullptr,*Slot.GetPathName());
        if(!Loaded(Found)||!Found->IsA<UClass>()||!Same(Slot,Found))return false;
        Out=static_cast<UClass*>(Found);return true;
    }
    bool Bind(UBlueprint* BP,UEdGraph* G,FResult& R)
    {
        Target=BP;
        if(!Loaded(BP)||!Loaded(G)||!HoldSlot(BP->ParentClass.GetGCPtr(),Parent)||!Loaded(Parent))return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Current resolved target/parent identities required before source inspection; no implicit source resolution."));
        Active=!Parent->HasAnyClassFlags(CLASS_Native);if(!Active)return true;
        GraphHold.Reset(G);EditorHold.Reset(GEditor);GraphOuter=G->GetOuter();GraphClass=G->GetClass();GraphPackage=G->GetOutermost();GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;
        if(!Loaded(GEditor)||!HoldSlot(G->Schema.GetGCPtr(),GraphSchemaClass)||!Loaded(GraphSchemaClass)||!Keep(BP))return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Current target ownership required before source inspection."));
        TSet<UClass*> Seen;
        for(UClass* C=Parent;C&&!C->HasAnyClassFlags(CLASS_Native);)
        {
            if(!Loaded(C)||!Cast<UBlueprintGeneratedClass>(C)||Seen.Contains(C)||Seen.Num()>=128||!Keep(C))return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Finite resident generated Actor ancestry required; broader generated owners remain incomplete."));
            Seen.Add(C);UObject* Generator=nullptr;if(!HoldSlot(C->ClassGeneratedBy,Generator))return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Current resolved generator role required before shared source inspection."));
            UBlueprint* B=Cast<UBlueprint>(Generator);if(!Loaded(B)||B==Target)return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Existing distinct ancestor Blueprint role required."));
            FAncestor A;A.Blueprint=B;A.Generated=C;
            UClass* ActualGenerated=nullptr;
            if(!HoldSlot(B->GeneratedClass.GetGCPtr(),ActualGenerated)||ActualGenerated!=C||!HoldSlot(B->SkeletonGeneratedClass.GetGCPtr(),A.Skeleton)||!Loaded(A.Skeleton)||!HoldSlot(B->ParentClass.GetGCPtr(),A.Parent)||!Loaded(A.Parent)||!HoldSlot(C->ClassDefaultObject,A.CDO)||!HoldSlot(B->SimpleConstructionScript,A.SCS))return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Current generated/skeleton/parent and resolved nullable CDO/SCS roles required; none are created."));
            UObject* SkeletonGenerator=nullptr;UClass* GeneratedSuper=nullptr;
            if(!HoldSlot(A.Skeleton->ClassGeneratedBy,SkeletonGenerator)||SkeletonGenerator!=B||!HoldSlot(A.Skeleton->ClassDefaultObject,A.SkeletonCDO)||!Super(A.Skeleton,A.SkeletonParent)||(A.SkeletonParent&&!Keep(A.SkeletonParent))||!Super(C,GeneratedSuper)||GeneratedSuper!=A.Parent)return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Current existing skeleton generator, actual superclass and nullable CDO roles required. Skeleton superclass is not assumed equal to generated superclass."));
            if(B->GetOutermost()->IsDirty()||B->BlueprintType!=BPTYPE_Normal||(B->Status!=BS_UpToDate&&B->Status!=BS_UpToDateWithWarnings)||B->bBeingCompiled||B->bQueuedForCompilation||B->bSuppressStructurallyModified||B->bIsRegeneratingOnLoad)return Fail(R,TEXT("binding_generated_parent_scope_pending"),TEXT("Saved clean normally compiled idle generated Actor ancestry required; unsupported initial state is not a callback mismatch."));
            A.Status=B->Status;A.ClassFlags=C->GetClassFlags();A.SkeletonFlags=A.Skeleton->GetClassFlags();Ancestors.Add(A);C=A.Parent;
        }
        if(Ancestors.IsEmpty())return Fail(R,TEXT("binding_generated_parent_roles_pending"),TEXT("Actual generated source ancestry must be captured before shared inspection."));
        if(!Current(R,TEXT("before_parent_admission_inspection")))return false;
        Before=GeneratedComponent::InspectSavedActorParent(Parent);R.Data->SetObjectField(TEXT("generated_binding_parent_before"),Before);
        if(!Current(R,TEXT("after_parent_admission_inspection")))return false;
        if(!Before->GetBoolField(TEXT("success")))return Fail(R,TEXT("binding_generated_parent_scope_pending"),TEXT("Shared saved runtime Actor parent admission refused; graph-resolution prefix retained and broader generated variants remain incomplete."));
        const bool ActorParent=Parent->IsChildOf(AActor::StaticClass());if(!Current(R,TEXT("after_actor_parent_classification")))return false;if(!ActorParent)return Fail(R,TEXT("binding_generated_parent_scope_pending"),TEXT("Generated Actor ancestry only; generated UObject/Component/Widget owners remain incomplete."));
        const auto& Rows=Before->GetArrayField(TEXT("generated_parents"));
        if(Rows.Num()!=Ancestors.Num())return Mismatch(R,TEXT("parent_admission_roles"),Parent->GetPathName(),TEXT("generated_parent_count"),LexToString(Ancestors.Num()),LexToString(Rows.Num()));
        for(int32 I=0;I<Ancestors.Num();++I)
        {
            const auto Row=Rows[I]->AsObject();const auto& A=Ancestors[I];
            if(!Row->GetStringField(TEXT("blueprint_path")).Equals(A.Blueprint->GetPathName(),ESearchCase::CaseSensitive)||!Row->GetStringField(TEXT("class_path")).Equals(A.Generated->GetPathName(),ESearchCase::CaseSensitive))return Mismatch(R,TEXT("parent_admission_roles"),A.Generated->GetPathName(),TEXT("inspector_role_identity"),A.Blueprint->GetPathName()+TEXT("|")+A.Generated->GetPathName(),Row->GetStringField(TEXT("blueprint_path"))+TEXT("|")+Row->GetStringField(TEXT("class_path")));
        }
        R.Data->SetBoolField(TEXT("generated_binding_parent_profile"),true);R.Data->SetBoolField(TEXT("generated_binding_parent_authored_values_fully_observed"),false);
        R.Data->SetStringField(TEXT("generated_binding_parent_scope"),TEXT("Saved/runtime dependency source admission plus original held BP/generated/skeleton/actual parents/nullable existing CDO/SCS roles and current clean compiled state. Full authored CDO/SCS values, arbitrary callbacks, runtime binding/message execution and broader generated owners remain unverified."));
        return Current(R,TEXT("after_parent_admission"));
    }
    bool RolesCurrent(FResult& R,const TCHAR* Boundary)
    {
        if(!Active)return true;if(Stopped)return false;
        if(!IsEditorGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||GIsRequestingExit||UE::IsSavingPackage()||!Loaded(EditorHold.Get())||GEditor!=EditorHold.Get()||IsPlaySessionActive())return Mismatch(R,Boundary,GetPathNameSafe(Target),TEXT("EditorState"),TEXT("same_idle_editor_no_play_gc_save_exit"),TEXT("editor_unavailable_changed_or_busy"));
        if(!Loaded(Target)||!Loaded(GraphHold.Get())||!Same(Target->ParentClass.GetGCPtr(),Parent))return Mismatch(R,Boundary,GetPathNameSafe(Target),TEXT("ParentClass/GraphLifetime"),GetPathNameSafe(Parent),TEXT("unresolved_changed_or_unready"));
        if(Target->BlueprintType!=BPTYPE_Normal||Target->bBeingCompiled||Target->bQueuedForCompilation||Target->bSuppressStructurallyModified||Target->bIsRegeneratingOnLoad||Target->Status==BS_BeingCreated)return Mismatch(R,Boundary,Target->GetPathName(),TEXT("TargetAuthoringState"),TEXT("ordinary_idle_blueprint"),TEXT("nonordinary_compiling_queued_suppressed_or_regenerating"));
        UEdGraph* G=GraphHold.Get();
        if(G->GetOuter()!=GraphOuter||G->GetClass()!=GraphClass||G->GetOutermost()!=GraphPackage||!G->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||G->GraphGuid!=GraphGuid||!G->bEditable||!Same(G->Schema.GetGCPtr(),GraphSchemaClass))return Mismatch(R,Boundary,GraphPath,TEXT("GraphOwnerIdentitySchema"),TEXT("original_outer_class_package_path_guid_schema_and_editable"),G->GetPathName()+TEXT("|")+GetPathNameSafe(G->GetOuter())+TEXT("|")+GetPathNameSafe(G->GetClass()));
        for(const auto& E:Roles)
        {
            UObject* O=E.Object.Get();
            if(!Loaded(O))return Mismatch(R,Boundary,E.Path,TEXT("lifetime"),TEXT("live"),TEXT("unready_or_destroyed"));
            if(O->GetOuter()!=E.Outer)return Mismatch(R,Boundary,E.Path,TEXT("Outer"),GetPathNameSafe(E.Outer),GetPathNameSafe(O->GetOuter()));
            if(O->GetClass()!=E.Class)return Mismatch(R,Boundary,E.Path,TEXT("Class"),GetPathNameSafe(E.Class),GetPathNameSafe(O->GetClass()));
            if(O->GetOutermost()!=E.Package)return Mismatch(R,Boundary,E.Path,TEXT("Package"),GetPathNameSafe(E.Package),GetPathNameSafe(O->GetOutermost()));
            if(!O->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive))return Mismatch(R,Boundary,E.Path,TEXT("Path"),E.Path,O->GetPathName());
            if(O!=Target&&O->GetFlags()!=E.Flags)return Mismatch(R,Boundary,E.Path,TEXT("ObjectFlags"),LexToString(uint32(E.Flags)),LexToString(uint32(O->GetFlags())));
        }
        for(const auto& A:Ancestors)
        {
            UBlueprint* B=A.Blueprint;UClass* C=A.Generated;
            if(!Same(B->GeneratedClass.GetGCPtr(),C))return Mismatch(R,Boundary,B->GetPathName(),TEXT("GeneratedClass"),GetPathNameSafe(C),TEXT("unresolved_or_changed"));
            if(!Same(B->SkeletonGeneratedClass.GetGCPtr(),A.Skeleton))return Mismatch(R,Boundary,B->GetPathName(),TEXT("SkeletonGeneratedClass"),GetPathNameSafe(A.Skeleton),TEXT("unresolved_or_changed"));
            if(!Same(B->ParentClass.GetGCPtr(),A.Parent))return Mismatch(R,Boundary,B->GetPathName(),TEXT("ParentClass"),GetPathNameSafe(A.Parent),TEXT("unresolved_or_changed"));
            if(!Same(B->SimpleConstructionScript,A.SCS))return Mismatch(R,Boundary,B->GetPathName(),TEXT("SimpleConstructionScript"),GetPathNameSafe(A.SCS),TEXT("unresolved_or_changed"));
            if(!Same(C->ClassDefaultObject,A.CDO))return Mismatch(R,Boundary,C->GetPathName(),TEXT("ClassDefaultObject"),GetPathNameSafe(A.CDO),TEXT("unresolved_or_changed"));
            if(!Same(C->ClassGeneratedBy,B)||!Same(A.Skeleton->ClassGeneratedBy,B))return Mismatch(R,Boundary,C->GetPathName(),TEXT("ClassGeneratedBy"),B->GetPathName(),TEXT("unresolved_or_changed"));
            if(!Same(A.Skeleton->ClassDefaultObject,A.SkeletonCDO))return Mismatch(R,Boundary,A.Skeleton->GetPathName(),TEXT("ClassDefaultObject"),GetPathNameSafe(A.SkeletonCDO),TEXT("unresolved_or_changed"));
            if(C->GetClassFlags()!=A.ClassFlags||A.Skeleton->GetClassFlags()!=A.SkeletonFlags)return Mismatch(R,Boundary,C->GetPathName(),TEXT("ClassFlags"),LexToString(uint32(A.ClassFlags))+TEXT("|")+LexToString(uint32(A.SkeletonFlags)),LexToString(uint32(C->GetClassFlags()))+TEXT("|")+LexToString(uint32(A.Skeleton->GetClassFlags())));
            UClass* GeneratedSuper=nullptr;UClass* SkeletonSuper=nullptr;
            if(!Super(C,GeneratedSuper)||GeneratedSuper!=A.Parent)return Mismatch(R,Boundary,C->GetPathName(),TEXT("SuperClass"),GetPathNameSafe(A.Parent),GetPathNameSafe(GeneratedSuper));
            if(!Super(A.Skeleton,SkeletonSuper)||SkeletonSuper!=A.SkeletonParent)return Mismatch(R,Boundary,A.Skeleton->GetPathName(),TEXT("SuperClass"),GetPathNameSafe(A.SkeletonParent),GetPathNameSafe(SkeletonSuper));
            if(B->GetOutermost()->IsDirty())return Mismatch(R,Boundary,B->GetPathName(),TEXT("PackageDirty"),TEXT("false"),TEXT("true"));
            if(B->Status!=A.Status)return Mismatch(R,Boundary,B->GetPathName(),TEXT("Status"),LexToString(uint8(A.Status)),LexToString(uint8(B->Status.GetValue())));
            if(B->bBeingCompiled||B->bQueuedForCompilation||B->bSuppressStructurallyModified||B->bIsRegeneratingOnLoad)return Mismatch(R,Boundary,B->GetPathName(),TEXT("CompileRegeneration"),TEXT("idle"),TEXT("compiling_queued_suppressed_or_regenerating"));
        }
        return true;
    }
    TArray<TObjectPtr<UEdGraph>> DirectGraphSlots() const
    {
        // Copy public slot handles only; no tracked graph reads or new baseline.
        // Separators preserve collection/interface boundaries as well as order.
        TArray<TObjectPtr<UEdGraph>> Slots;
        Slots.Append(Target->FunctionGraphs);Slots.Add(nullptr);
        Slots.Append(Target->MacroGraphs);Slots.Add(nullptr);
        Slots.Append(Target->UbergraphPages);Slots.Add(nullptr);
        Slots.Append(Target->EventGraphs);Slots.Add(nullptr);
        Slots.Append(Target->DelegateSignatureGraphs);Slots.Add(nullptr);
        for(const auto& Interface:Target->ImplementedInterfaces){Slots.Append(Interface.Graphs);Slots.Add(nullptr);}
        return Slots;
    }
    bool Current(FResult& R,const TCHAR* Boundary)
    {
        if(!Active)return true;if(Stopped)return false;if(!RolesCurrent(R,Boundary))return false;
        // Keep existing ownership/membership semantics, including nested and
        // extension graphs. This only enumerates graph membership, not nodes,
        // pins, descriptors or disk. GetAllGraphs can call an extension observer;
        // reclose the original roles after it instead of adopting new baselines.
        if(FBlueprintEditorUtils::FindBlueprintForGraph(GraphHold.Get())!=Target)return Mismatch(R,Boundary,GraphPath,TEXT("GraphBlueprintOwner"),GetPathNameSafe(Target),TEXT("detached_or_other_owner"));
        const auto BeforeSlots=DirectGraphSlots();TArray<UEdGraph*> Owned;Count(R,TEXT("generated_binding_membership_calls"));Target->GetAllGraphs(Owned);
        if(!RolesCurrent(R,Boundary))return false;
        const auto AfterSlots=DirectGraphSlots();
        if(BeforeSlots.Num()!=AfterSlots.Num())return Mismatch(R,Boundary,Target->GetPathName(),TEXT("DirectGraphSlots.Num"),LexToString(BeforeSlots.Num()),LexToString(AfterSlots.Num()));
        for(int32 I=0;I<BeforeSlots.Num();++I)if(BeforeSlots[I].GetHandle()!=AfterSlots[I].GetHandle())return Mismatch(R,Boundary,Target->GetPathName(),*FString::Printf(TEXT("DirectGraphSlots[%d]"),I),BeforeSlots[I].GetPathName(),AfterSlots[I].GetPathName());
        if(Owned.Num()>64||!Owned.Contains(GraphHold.Get()))return Mismatch(R,Boundary,GraphPath,TEXT("GraphMembership"),TEXT("original_graph_in_finite_owned_inventory"),TEXT("detached_or_membership_budget_exceeded"));
        return true;
    }
    bool Finish(FResult& R)
    {
        if(!Active)return true;if(!Current(R,TEXT("before_parent_final_inspection")))return false;
        const auto After=GeneratedComponent::InspectSavedActorParent(Parent);R.Data->SetObjectField(TEXT("generated_binding_parent_after"),After);
        if(!Current(R,TEXT("after_parent_final_inspection")))return false;
        const bool Preserved=After->GetBoolField(TEXT("success"))&&Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));R.Data->SetBoolField(TEXT("generated_binding_parent_preserved"),Preserved);
        return Preserved||Mismatch(R,TEXT("parent_final_inspection"),Parent->GetPathName(),TEXT("saved_parent_inspection"),TEXT("original_admission_metadata"),TEXT("changed_or_refused"));
    }
};

bool Mark(UBlueprint* BP,FResult& R,FGeneratedBindingParent& Parent)
{
    if(!Parent.Current(R,TEXT("before_mark_modified")))return false;R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_blueprint_modified_returns"));return Parent.Current(R,TEXT("after_mark_modified"));
}

FString GraphType(UBlueprint* BP, UEdGraph* G)
{
    if (BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }
    if (BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }
    if (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G)) { return TEXT("event"); }
    if (BP->DelegateSignatureGraphs.Contains(G)) { return TEXT("delegate"); } return TEXT("unknown");
}

TSharedRef<FJsonObject> DescribeGraph(UBlueprint* BP, UEdGraph* G)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("graph_id"), G->GraphGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("graph_name"), G->GetName()); J->SetStringField(TEXT("graph_type"), GraphType(BP, G)); J->SetStringField(TEXT("object_path"), G->GetPathName());
    J->SetStringField(TEXT("schema"), G->GetSchema() ? G->GetSchema()->GetClass()->GetName() : TEXT("")); J->SetNumberField(TEXT("node_count"), G->Nodes.Num()); return J;
}

TSharedRef<FJsonObject> DescribeNode(UEdGraphNode* Node)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits)); J->SetStringField(TEXT("object_path"), Node->GetPathName());
    J->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); J->SetStringField(TEXT("node_name"), Node->GetName());
    J->SetNumberField(TEXT("x"), Node->NodePosX); J->SetNumberField(TEXT("y"), Node->NodePosY);
    TArray<TSharedPtr<FJsonValue>> Pins;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (!P) { continue; } auto Q = MakeShared<FJsonObject>(); Q->SetStringField(TEXT("name"), P->PinName.ToString());
        Q->SetStringField(TEXT("pin_id"), P->PinId.ToString(EGuidFormats::Digits)); Q->SetStringField(TEXT("direction"), P->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        Q->SetObjectField(TEXT("pin_type"), Pin::Describe(P->PinType)); Q->SetBoolField(TEXT("orphaned"), P->bOrphanedPin);
        Q->SetStringField(TEXT("default_value"), P->DefaultValue); Q->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : TEXT(""));
        Q->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString()); TArray<TSharedPtr<FJsonValue>> Links;
        for (UEdGraphPin* Other : P->LinkedTo)
        {
            if (!Other || !IsValid(Other->GetOwningNodeUnchecked())) { continue; } auto Link = MakeShared<FJsonObject>();
            Link->SetStringField(TEXT("node_id"), Other->GetOwningNode()->NodeGuid.ToString(EGuidFormats::Digits)); Link->SetStringField(TEXT("node_path"), Other->GetOwningNode()->GetPathName());
            Link->SetStringField(TEXT("pin_name"), Other->PinName.ToString()); Links.Add(MakeShared<FJsonValueObject>(Link));
        }
        Q->SetArrayField(TEXT("linked_to"), Links); Pins.Add(MakeShared<FJsonValueObject>(Q));
    }
    J->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    { const UFunction* F = Call->GetTargetFunction(); J->SetStringField(TEXT("function_path"), F ? F->GetPathName() : TEXT("")); }
    if (const UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node))
    {
        J->SetStringField(TEXT("custom_event_name"), Event->CustomFunctionName.ToString());
        J->SetBoolField(TEXT("call_in_editor"), Event->bCallInEditor); J->SetBoolField(TEXT("override_function"), Event->bOverrideFunction);
        J->SetBoolField(TEXT("deprecated"), Event->bIsDeprecated); J->SetNumberField(TEXT("function_flags"), Event->FunctionFlags);
        J->SetNumberField(TEXT("user_pin_count"), Event->UserDefinedPins.Num());
    }
    return J;
}

bool Capture(UBlueprint* BP, UEdGraph* G, TSharedRef<FJsonObject>& Out, FResult& R)
{
    Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("blueprint"), BP->GetPathName()); Out->SetBoolField(TEXT("dirty"), BP->GetOutermost()->IsDirty());
    Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(BP->Status.GetValue()));
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    if (Graphs.Num() > MaxGraphs) { return Fail(R, TEXT("function_graph_budget_exceeded"), TEXT("Blueprint graph observation exceeds 256 graphs.")); }
    TArray<TSharedPtr<FJsonValue>> GraphRows; TSet<FGuid> GraphGuids;
    for (UEdGraph* Candidate : Graphs)
    {
        if (!IsValid(Candidate) || !Candidate->GraphGuid.IsValid() || GraphGuids.Contains(Candidate->GraphGuid))
        { return Fail(R, TEXT("function_graph_identity_invalid"), TEXT("Graph identities are invalid or duplicated; no identities are repaired.")); }
        GraphGuids.Add(Candidate->GraphGuid); GraphRows.Add(MakeShared<FJsonValueObject>(DescribeGraph(BP, Candidate)));
    }
    Out->SetArrayField(TEXT("graphs"), GraphRows);
    if (G)
    {
        if (!Graphs.Contains(G) || G->Nodes.Num() > MaxNodes) { return Fail(R, TEXT("function_graph_state_invalid"), TEXT("Target graph is no longer owned by the Blueprint or exceeds 2048 nodes.")); }
        TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> NodeGuids;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!IsValid(N) || !N->NodeGuid.IsValid() || NodeGuids.Contains(N->NodeGuid) || N->Pins.Num() > 512)
            { return Fail(R, TEXT("function_node_identity_invalid"), TEXT("A graph node identity/pin budget is invalid; observation is incomplete.")); }
            NodeGuids.Add(N->NodeGuid); Nodes.Add(MakeShared<FJsonValueObject>(DescribeNode(N)));
        }
        Out->SetObjectField(TEXT("graph"), DescribeGraph(BP, G)); Out->SetArrayField(TEXT("nodes"), Nodes);
    }
    return true;
}

bool DispatcherName(const FString& Input,FString& Out,FResult& R)
{
    Out=Input.TrimStartAndEnd();bool Bounded=!Out.IsEmpty()&&Out.Len()<NAME_SIZE;for(int32 I=0;I<Out.Len();++I) { Bounded&=Out[I]!=0; }
    return (Bounded&&!FName(*Out).IsNone())||Fail(R,TEXT("dispatcher_name_invalid"),TEXT("Original trimmed dispatcher FName must be nonempty/nonNone and fit NAME_SIZE without interior NUL."));
}

TSharedRef<FJsonObject> Variables(UBlueprint* BP,FResult& R)
{
    Count(R,TEXT("variable_observation_calls"));auto J=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;bool Complete=BP->NewVariables.Num()<=256;
    if(Complete) { for(const auto& V:BP->NewVariables) { FString Text;FBPVariableDescription::StaticStruct()->ExportText(Text,&V,nullptr,BP,PPF_None,nullptr);if(Text.Len()>32768) { Complete=false;break; }auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),V.VarName.ToString());Q->SetStringField(TEXT("guid"),V.VarGuid.ToString());Q->SetStringField(TEXT("reflected_descriptor"),Text);Rows.Add(MakeShared<FJsonValueObject>(Q)); } }J->SetArrayField(TEXT("ordered_descriptors"),Rows);J->SetBoolField(TEXT("complete"),Complete);return J;
}

bool BlueprintState(UBlueprint* BP,TSharedRef<FJsonObject>& Out,FResult& R)
{
    Count(R,TEXT("blueprint_capture_calls"));Out=MakeShared<FJsonObject>();const auto V=Variables(BP,R);Out->SetObjectField(TEXT("variables"),V);if(!V->GetBoolField(TEXT("complete"))) { return Fail(R,TEXT("dispatcher_variable_observation_incomplete"),TEXT("Descriptor snapshot exceeds finite256/32768 bounds.")); }
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(Graphs.Num()>64) { return Fail(R,TEXT("dispatcher_graph_budget"),TEXT("Full selected owned graph preservation scope is64 graphs.")); }
    TArray<TSharedPtr<FJsonValue>> Rows;int32 Nodes=0;for(UEdGraph* G:Graphs)
    { if(!Loaded(G)||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||(Nodes+=G->Nodes.Num())>4096) { return Fail(R,TEXT("dispatcher_graph_observation_incomplete"),TEXT("Loaded exact K2 graphs within total4096-node scope required.")); }auto Q=MakeShared<FJsonObject>();Count(R,TEXT("graph_capture_calls"));if(!Capture(BP,G,Q,R)) { Out->SetArrayField(TEXT("graph_states"),Rows);return false; }Rows.Add(MakeShared<FJsonValueObject>(Q)); }Out->SetArrayField(TEXT("graph_states"),Rows);Out->SetBoolField(TEXT("dirty"),BP->GetOutermost()->IsDirty());Out->SetNumberField(TEXT("blueprint_status"),BP->Status.GetValue());return true;
}

bool OldVariables(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,int32 Delta)
{
    const auto& A=Before->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("ordered_descriptors"));const auto& B=After->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("ordered_descriptors"));if(B.Num()!=A.Num()+Delta) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}

bool ReferenceGuard(const FString& S)
{
    FString N=S.TrimStartAndEnd();N.TrimQuotesInline();N.ReplaceInline(TEXT("\\"),TEXT("/"));N=N.ToLower();for(const TCHAR* Token:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor")}) { if(N.Contains(Token)) { return false; } }return true;
}

TSharedRef<FJsonObject> PropertyJson(const FMulticastDelegateProperty* P,FResult& R)
{
    Count(R,TEXT("property_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("present"),P!=nullptr);if(P) { J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("field_class"),P->GetClass()->GetName());J->SetStringField(TEXT("owner_class"),GetPathNameSafe(P->GetOwnerClass()));J->SetStringField(TEXT("flags_uint64"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));J->SetStringField(TEXT("signature_function"),GetPathNameSafe(P->SignatureFunction));J->SetBoolField(TEXT("blueprint_callable"),P->HasAllPropertyFlags(CPF_BlueprintCallable));J->SetBoolField(TEXT("blueprint_assignable"),P->HasAllPropertyFlags(CPF_BlueprintAssignable)); }return J;
}

FString GetPinDirectionName(const UEdGraphPin* Pin)
{
    return Pin && Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input");
}

TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType)
{
    TSharedPtr<FJsonObject> TypeObject = MakeShared<FJsonObject>();
    TypeObject->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
    TypeObject->SetStringField(TEXT("subcategory"), PinType.PinSubCategory.ToString());
    TypeObject->SetBoolField(TEXT("is_array"), PinType.IsArray());
    if (PinType.PinSubCategoryObject.IsValid())
    {
        TypeObject->SetStringField(TEXT("subcategory_object"), PinType.PinSubCategoryObject->GetPathName());
    }
    return TypeObject;
}

TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    PinObject->SetStringField(TEXT("name"), Pin ? Pin->PinName.ToString() : FString());
    PinObject->SetStringField(TEXT("direction"), GetPinDirectionName(Pin));

    if (Pin)
    {
        PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
        PinObject->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
        PinObject->SetObjectField(TEXT("pin_type"), PinTypeToJson(Pin->PinType));
        PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);

        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            PinObject->SetStringField(TEXT("subcategory_object"), Pin->PinType.PinSubCategoryObject->GetPathName());
        }
        if (Pin->DefaultObject)
        {
            PinObject->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
        }

        TArray<TSharedPtr<FJsonValue>> LinkedPins;
        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if (!LinkedPin || !LinkedPin->GetOwningNode())
            {
                continue;
            }

            TSharedPtr<FJsonObject> LinkedPinObject = MakeShared<FJsonObject>();
            LinkedPinObject->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
            LinkedPinObject->SetStringField(TEXT("node_name"), LinkedPin->GetOwningNode()->GetName());
            LinkedPinObject->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
            LinkedPins.Add(MakeShared<FJsonValueObject>(LinkedPinObject));
        }
        PinObject->SetArrayField(TEXT("linked_to"), LinkedPins);
    }

    return PinObject;
}

TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!Node)
    {
        return NodeObject;
    }

    NodeObject->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    NodeObject->SetStringField(TEXT("name"), Node->GetName());
    NodeObject->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetStringField(TEXT("node_comment"), Node->NodeComment);
    NodeObject->SetBoolField(TEXT("comment_bubble_visible"), Node->bCommentBubbleVisible);
    NodeObject->SetBoolField(TEXT("comment_bubble_pinned"), Node->bCommentBubblePinned);

    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pins.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
        }
        NodeObject->SetArrayField(TEXT("pins"), Pins);
    }

    return NodeObject;
}

bool PinsMatchType(const FEdGraphPinType& A,const FEdGraphPinType& B)
{ return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper; }

TSharedRef<FJsonObject> Signature(UFunction* F,FResult& R)
{
    Count(R,TEXT("signature_observation_calls"));auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("loaded"),Loaded(F));J->SetStringField(TEXT("path"),GetPathNameSafe(F));bool Complete=Loaded(F);TArray<TSharedPtr<FJsonValue>> Rows;
    if(Complete) { J->SetStringField(TEXT("flags_uint32"),LexToString(static_cast<uint32>(F->FunctionFlags)));for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)
    { if(!It->HasAnyPropertyFlags(CPF_Parm)) { continue; }if(Rows.Num()>=256) { Complete=false;break; }FEdGraphPinType T;const bool Converted=GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(*It,T);auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),It->GetName());Q->SetStringField(TEXT("field_class"),It->GetClass()->GetName());Q->SetStringField(TEXT("flags_uint64"),LexToString(static_cast<uint64>(It->GetPropertyFlags())));Q->SetBoolField(TEXT("schema_converted"),Converted);if(Converted) { auto D=Function::Pin::Describe(T);D->SetBoolField(TEXT("is_const"),T.bIsConst);Q->SetObjectField(TEXT("pin_type"),D); }Rows.Add(MakeShared<FJsonValueObject>(Q));Complete&=Converted; } }
    J->SetArrayField(TEXT("parameters"),Rows);J->SetBoolField(TEXT("complete"),Complete);return J;
}

bool Structural(UBlueprint* BP,FResult& R,FGeneratedBindingParent& Parent)
{ if(!Parent.Current(R,TEXT("before_mark_structural")))return false;Count(R,TEXT("mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);Count(R,TEXT("mark_structural_returns"));return Parent.Current(R,TEXT("after_mark_structural")); }

void Init(FResult& R);

bool Scope(UBlueprint* BP,UEdGraph* G,FResult& R,bool GeneratedParent=false)
{
    if(!Loaded(BP)||!Loaded(G)||!Loaded(BP->ParentClass)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("binding_owner_scope_pending"),TEXT("Idle owned editable existing K2 graph required; nonordinary/compiling/locked owner variants remain tracked.")); }
    int32 Depth=0;TArray<TSharedPtr<FJsonValue>> Parents;for(UClass* C=GeneratedParent?nullptr:BP->ParentClass.GetGCPtr().Get();C;C=C->GetSuperClass())
    {
        const FString Package=C->GetOutermost()->GetName();if(++Depth>64||!Loaded(C)||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->IsEditorOnly()||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||(!Package.Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)&&!Package.Equals(TEXT("/Script/UMG"),ESearchCase::CaseSensitive)))
        { return Fail(R,TEXT("binding_parent_scope_pending"),TEXT("Loaded native runtime Engine/CoreUObject/UMG hierarchy required; valid WidgetBlueprint owners are admitted. Other/generated/custom/editor parents remain tracked.")); }Parents.Add(MakeShared<FJsonValueString>(C->GetPathName()));
    }
    R.Data->SetArrayField(TEXT("runtime_parent_hierarchy"),Parents);R.Data->SetStringField(TEXT("owner_scope"),GeneratedParent?TEXT("Owned runtime graph with saved clean generated Actor ancestry. Held source-role and saved runtime-dependency admission only; no full authored CDO/SCS/runtime execution proof."):TEXT("Owned runtime native-parent Actor/Object/UMG Widget graphs. No whole widget preview/world/CDO/SCS/children/cache effect proof."));return true;
}
bool Graph(const FString& Path,const FString& Name,const FString& Id,const FString& Type,bool Create,UBlueprint*& BP,UEdGraph*& G,TSharedRef<FJsonObject>& Before,FResult& R,FGeneratedBindingParent& Parent)
{
    Count(R,TEXT("shared_graph_resolve_calls"));const auto S=Function::Resolve(Path,Name,Id,Type,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(S));R.bMutationAttempted=S.bMutationAttempted;R.bChanged=S.bChanged;R.bChangeStateKnown=S.bChangeStateKnown;if(!S.bSuccess||!S.bPostconditionPassed) { return Fail(R,S.ErrorCode.IsEmpty()?TEXT("binding_graph_unverified"):*S.ErrorCode,S.Error); }
    BP=FindObject<UBlueprint>(nullptr,*S.Data->GetStringField(TEXT("blueprint")));G=FindObject<UEdGraph>(nullptr,*S.Data->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")));R.Data->SetStringField(TEXT("blueprint"),S.Data->GetStringField(TEXT("blueprint")));R.Data->SetObjectField(TEXT("before"),S.Data->GetObjectField(TEXT("after")));if(!Parent.Bind(BP,G,R)||!Scope(BP,G,R,Parent.Active)||!Parent.Current(R,TEXT("after_owner_scope"))) { return false; }const bool Complete=BlueprintState(BP,Before,R);if(!Parent.Current(R,TEXT("after_initial_blueprint_observation")))return false;R.Data->SetObjectField(Complete?TEXT("before_blueprint_state"):TEXT("incomplete_before_blueprint_state"),Before);return Complete;
}
bool Text(const FString& Input,FString& Out,bool Empty,FResult& R)
{
    Out=Input.TrimStartAndEnd();bool Safe=(Empty||!Out.IsEmpty())&&Out.Len()<=1024;for(int32 I=0;I<Out.Len();++I) { Safe&=Out[I]!=0; }return Safe||Fail(R,TEXT("binding_text_invalid"),TEXT("Bounded original trimmed string without interior NUL required."));
}
bool Name(const FString& Input,FString& Out,FResult& R)
{ return Text(Input,Out,false,R)&&(Out.Len()<NAME_SIZE||Fail(R,TEXT("binding_name_overflow"),TEXT("FName input exceeds safe NAME_SIZE."))); }
bool RuntimeClass(UClass* C,FResult& R)
{
    return (Loaded(C)&&!C->IsEditorOnly()&&!C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)&&ReferenceGuard(C->GetPathName())&&!C->GetOutermost()->GetName().Equals(TEXT("/Script/CubelessMCPUE58Editor"),ESearchCase::CaseSensitive))||Fail(R,TEXT("binding_source_class_pending"),TEXT("Current loaded runtime class without tooling dependency required; no new game C++ helper or forced module load."));
}
UClass* LoadOwner(const FString& Requested,bool Interface,FResult& R,FGeneratedBindingParent& Parent)
{
    FString TextValue;if(!Text(Requested,TextValue,false,R)||!ReferenceGuard(TextValue)) { Fail(R,TEXT("binding_class_reference_invalid"),TEXT("Original dependency reference/empty class guard refused."));return nullptr; }UClass* C=nullptr;
    auto Load=[&](const FString& P)->UClass* { if(!Parent.Current(R,TEXT("before_source_class_load")))return nullptr;Count(R,TEXT("source_class_load_calls"));auto* V=LoadObject<UClass>(nullptr,*P);Count(R,TEXT("source_class_load_returns"));const TStrongObjectPtr<UClass> Hold(V);if(!Parent.Current(R,TEXT("after_source_class_load")))return nullptr;R.Data->SetStringField(TEXT("last_source_class_load_returned"),GetPathNameSafe(V));return V; };
    if(!Interface) { Count(R,TEXT("source_class_lookup_calls"));C=FindObject<UClass>(nullptr,*TextValue);Count(R,TEXT("source_class_lookup_returns"));if(!C) { C=Load(TextValue); } }
    else
    {
        FString P=FPackageName::ExportTextPathToObjectPath(TextValue).TrimStartAndEnd();P.TrimQuotesInline();if((P.StartsWith(TEXT("/Game/"))||P.StartsWith(TEXT("/Engine/")))&&!P.Contains(TEXT("."))) { P+=TEXT(".")+FPackageName::GetShortName(P); }if(P.Len()>2048) { Fail(R,TEXT("binding_class_path_overflow"),TEXT("Normalized load path exceeds finite budget."));return nullptr; }
        C=Load(P);if(!Parent.Current(R,TEXT("after_first_interface_class_load")))return nullptr;if(!C) { Count(R,TEXT("source_class_lookup_calls"));C=FindFirstObject<UClass>(*TextValue);Count(R,TEXT("source_class_lookup_returns"));if(!Parent.Current(R,TEXT("after_interface_class_fallback_lookup")))return nullptr; }if(!C) { C=Load(TEXT("/Script/Engine.")+TextValue); }
        if(!C)
        {
            if(!Parent.Current(R,TEXT("before_source_blueprint_load")))return nullptr;Count(R,TEXT("source_blueprint_load_calls"));auto* B=LoadObject<UBlueprint>(nullptr,*P);Count(R,TEXT("source_blueprint_load_returns"));const TStrongObjectPtr<UBlueprint> Hold(B);if(!Parent.Current(R,TEXT("after_source_blueprint_load")))return nullptr;R.Data->SetStringField(TEXT("source_blueprint_load_returned"),GetPathNameSafe(B));if(Loaded(B)) { C=B->GeneratedClass?B->GeneratedClass.Get():B->SkeletonGeneratedClass.Get(); }if(!C&&!P.EndsWith(TEXT("_C"))) { C=Load(P+TEXT("_C")); }
        }
    }
    if(!Parent.Current(R,TEXT("after_source_class_resolution")))return nullptr;R.Data->SetStringField(TEXT("source_class_returned"),GetPathNameSafe(C));R.Data->SetStringField(TEXT("class_load_scope"),TEXT("Original class/Blueprint load paths retained. Loader/PostLoad/cache effects precede source snapshot and are not total-zero or fully observed."));
    if(!RuntimeClass(C,R)||(!Interface&&(!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy))||(Interface&&!C->HasAnyClassFlags(CLASS_Interface))) { if(R.Data->GetNumberField(TEXT("source_class_load_calls"))>0||R.Data->GetNumberField(TEXT("source_blueprint_load_calls"))>0) { R.bChangeStateKnown=false; }Fail(R,TEXT("binding_source_class_unavailable"),Interface?TEXT("Original interface requires CLASS_Interface and current class readiness."):TEXT("Original native delegate source requires current native runtime class."));return nullptr; }return C;
}
FMulticastDelegateProperty* NativeProperty(UClass* C,FName N,FResult& R)
{ Count(R,TEXT("property_lookup_calls"));FMulticastDelegateProperty* P=nullptr;for(TFieldIterator<FMulticastDelegateProperty> It(C,EFieldIteratorFlags::IncludeSuper);It;++It) { if(It->GetFName()==N) { P=*It;break; } }Count(R,TEXT("property_lookup_returns"));return P; }
UFunction* LookupFunction(UClass* C,const FString& N,FResult& R)
{
    Count(R,TEXT("function_lookup_calls"));UFunction* F=C->FindFunctionByName(FName(*N));if(!F) { TArray<UFunction*> Matches;for(TFieldIterator<UFunction> It(C,EFieldIteratorFlags::IncludeSuper);It;++It) { if(It->GetName().Equals(N,ESearchCase::IgnoreCase)) { Matches.Add(*It); } }if(Matches.Num()==1) { F=Matches[0]; } }Count(R,TEXT("function_lookup_returns"));if(!Loaded(F)) { Fail(R,TEXT("binding_function_unavailable"),TEXT("Original interface function lookup missing/ambiguous/not loaded."));return nullptr; }return F;
}
UClass* NodeClass(const TCHAR* Path,UClass* Super,FResult& R)
{
    Count(R,TEXT("node_class_lookup_calls"));auto* C=FindObject<UClass>(nullptr,Path);Count(R,TEXT("node_class_lookup_returns"));if(!Loaded(C)||C->GetSuperClass()!=Super||!C->HasAnyClassFlags(CLASS_Native)||C->ClassGeneratedBy||C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)) { Fail(R,TEXT("binding_node_class_unavailable"),TEXT("Exact loaded public Engine node class/base required; no private derived import or forced module load."));return nullptr; }return C;
}
UEdGraphPin* ResolvePin(UEdGraph* G,const FString& NodeId,const FString& Requested,const TCHAR* Default,EEdGraphPinDirection Direction,FResult& R)
{
    FString Id,PinName;if(!Text(NodeId,Id,false,R)||!Text(Requested,PinName,true,R)) { return nullptr; }if(PinName.IsEmpty()) { PinName=Default; }FGuid Guid;if(!FGuid::Parse(Id,Guid)||!Guid.IsValid()) { Fail(R,TEXT("binding_endpoint_id_invalid"),TEXT("Current endpoint requires valid node GUID; all current and legacy GUID string formats admitted."));return nullptr; }Count(R,TEXT("endpoint_node_lookup_calls"));UEdGraphNode* N=nullptr;int32 Matches=0;for(UEdGraphNode* Candidate:G->Nodes) { if(Candidate&&Candidate->NodeGuid==Guid) { N=Candidate;++Matches; } }Count(R,TEXT("endpoint_node_lookup_returns"));if(Matches!=1||!Loaded(N)) { Fail(R,TEXT("binding_endpoint_node_missing"),TEXT("Original endpoint node missing/ambiguous in selected graph."));return nullptr; }
    Count(R,TEXT("endpoint_pin_lookup_calls"));UEdGraphPin* P=nullptr;for(UEdGraphPin* Candidate:N->Pins) { if(Candidate&&Candidate->Direction==Direction&&Candidate->PinName.ToString().Equals(PinName,ESearchCase::IgnoreCase)) { P=Candidate;break; } }
    auto* VariableGetClass=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_VariableGet"));if(!P&&Direction==EGPD_Output&&VariableGetClass&&N->GetClass()->IsChildOf(VariableGetClass)) { for(UEdGraphPin* Candidate:N->Pins) { if(Candidate&&Candidate->Direction==EGPD_Output&&Candidate->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec) { P=Candidate;break; } } }Count(R,TEXT("endpoint_pin_lookup_returns"));
    if(!P||P->WasTrashed()||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()) { Fail(R,TEXT("binding_endpoint_pin_missing"),TEXT("Current directional endpoint unavailable; no pointer/default/pin repaired."));return nullptr; }R.Data->SetObjectField(TEXT("last_endpoint_pin"),PinToJson(P).ToSharedRef());return P;
}
TSharedRef<FJsonObject> WithoutLinks(const TSharedRef<FJsonObject>& N,const TSet<FGuid>& Allowed)
{
    auto J=MakeShared<FJsonObject>(*N);TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& V:N->GetArrayField(TEXT("pins"))) { auto Q=MakeShared<FJsonObject>(*V->AsObject());FGuid Id;FGuid::Parse(Q->GetStringField(TEXT("pin_id")),Id);if(Allowed.Contains(Id)) { Q->RemoveField(TEXT("linked_to")); }Pins.Add(MakeShared<FJsonValueObject>(Q)); }J->SetArrayField(TEXT("pins"),Pins);return J;
}
bool Observe(UBlueprint* BP,UEdGraph* G,UEdGraphNode* N,const TSharedRef<FJsonObject>& Before,const TSet<FGuid>& LinkPins,FResult& R)
{
    R.Stage=TEXT("after_observation");TSharedRef<FJsonObject> A=MakeShared<FJsonObject>();const bool Complete=BlueprintState(BP,A,R);R.Data->SetObjectField(Complete?TEXT("after_blueprint_state"):TEXT("incomplete_after_blueprint_state"),A);if(!Complete) { R.bChangeStateKnown=false;return false; }
    const auto& B=Before->GetArrayField(TEXT("graph_states"));const auto& C=A->GetArrayField(TEXT("graph_states"));bool Old=B.Num()==C.Num(),Rows=Old;TArray<TSharedPtr<FJsonValue>> NewNodes;int32 Delta=0;for(const auto& V:B) { const auto X=V->AsObject();if(X->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")).Equals(G->GetPathName(),ESearchCase::CaseSensitive)) { Delta=G->Nodes.Num()-X->GetArrayField(TEXT("nodes")).Num();break; } }
    if(Old) { for(int32 I=0;I<B.Num();++I)
    {
        const auto X=B[I]->AsObject(),Y=C[I]->AsObject();const bool Target=X->GetObjectField(TEXT("graph"))->GetStringField(TEXT("object_path")).Equals(G->GetPathName(),ESearchCase::CaseSensitive);const auto& NX=X->GetArrayField(TEXT("nodes"));const auto& NY=Y->GetArrayField(TEXT("nodes"));auto GX=MakeShared<FJsonObject>(*X->GetObjectField(TEXT("graph")));if(Target) { GX->SetNumberField(TEXT("node_count"),GX->GetNumberField(TEXT("node_count"))+Delta);R.Data->SetObjectField(TEXT("after"),Y);R.Data->SetObjectField(TEXT("graph"),DescribeGraph(BP,G)); }
        Rows&=Equal(MakeShared<FJsonValueObject>(GX),MakeShared<FJsonValueObject>(Y->GetObjectField(TEXT("graph"))));if(NY.Num()<NX.Num()||(!Target&&NY.Num()!=NX.Num())) { Old=false;continue; }
        for(int32 K=0;K<NX.Num();++K) { Old&=Equal(MakeShared<FJsonValueObject>(WithoutLinks(NX[K]->AsObject().ToSharedRef(),LinkPins)),MakeShared<FJsonValueObject>(WithoutLinks(NY[K]->AsObject().ToSharedRef(),LinkPins))); }for(int32 K=NX.Num();K<NY.Num();++K) { NewNodes.Add(NY[K]); }
    } }
    const bool Vars=OldVariables(Before,A,0);R.Data->SetBoolField(TEXT("old_nodes_except_endpoint_links_preserved"),Old);R.Data->SetBoolField(TEXT("graph_rows_preserved"),Rows);R.Data->SetBoolField(TEXT("old_variable_descriptors_preserved"),Vars);R.Data->SetArrayField(TEXT("new_nodes"),NewNodes);R.Data->SetNumberField(TEXT("observed_new_node_count"),NewNodes.Num());R.bChanged=R.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(A));R.bChangeStateKnown=true;if(Loaded(N)&&G->Nodes.Contains(N)) { R.Data->SetObjectField(TEXT("node"),DescribeNode(N));R.Data->SetStringField(TEXT("node_id"),N->NodeGuid.ToString()); }
    return (Old&&Rows&&Vars)||Fail(R,TEXT("binding_preservation_failed"),TEXT("Actual old node/default/type/source graph preservation differs beyond intended endpoint links. Prefix retained, no added rollback."));
}
bool LivePins(UEdGraphNode* N)
{
    TSet<FGuid> Ids;for(const UEdGraphPin* P:N->Pins) { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()) { return false; }Ids.Add(P->PinId);for(const UEdGraphPin* Q:P->LinkedTo) { if(!Q||Q->WasTrashed()||!Loaded(Q->GetOwningNodeUnchecked())||!Q->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } } }return true;
}
bool Place(UEdGraph* G,UEdGraphNode* N,const int32 (&XY)[2],bool Reconstruct,FResult& R,FGeneratedBindingParent& Parent)
{
    if(!Parent.Current(R,TEXT("before_place")))return false;
    N->NodePosX=XY[0];N->NodePosY=XY[1];Count(R,TEXT("transactional_flag_write_calls"));N->SetFlags(RF_Transactional);Count(R,TEXT("explicit_graph_modify_calls"));G->Modify();if(!Parent.Current(R,TEXT("after_graph_modify")))return false;
    Count(R,TEXT("graph_add_node_calls"));G->AddNode(N,true,false);if(!Parent.Current(R,TEXT("after_add_node")))return false;
    Count(R,TEXT("new_guid_calls"));N->CreateNewGuid();if(!Parent.Current(R,TEXT("after_create_guid")))return false;
    Count(R,TEXT("post_placed_calls"));N->PostPlacedNewNode();Count(R,TEXT("post_placed_returns"));if(!Parent.Current(R,TEXT("after_post_placed")))return false;
    Count(R,TEXT("allocate_pins_calls"));N->AllocateDefaultPins();if(!Parent.Current(R,TEXT("after_allocate_pins")))return false;
    if(Reconstruct) { Count(R,TEXT("reconstruct_calls"));N->ReconstructNode();if(!Parent.Current(R,TEXT("after_reconstruct")))return false; }
    R.Data->SetObjectField(TEXT("placed_node_prefix"),DescribeNode(N));R.Data->SetStringField(TEXT("created_node_path"),N->GetPathName());R.Data->SetStringField(TEXT("created_node_guid"),N->NodeGuid.ToString(EGuidFormats::Digits));return Parent.Current(R,TEXT("after_placed_description"));
}
bool Connect(const UEdGraphSchema_K2* Schema,UEdGraphPin* A,UEdGraphPin* B,FResult& R,FGeneratedBindingParent& Parent)
{
    if(!Parent.Current(R,TEXT("before_connection")))return false;if(!A||!B||A->WasTrashed()||B->WasTrashed()) { return Fail(R,TEXT("binding_connection_pin_unavailable"),TEXT("Invalid pin connection request.")); }Count(R,TEXT("can_create_connection_calls"));const FPinConnectionResponse Response=Schema->CanCreateConnection(A,B);if(!Parent.Current(R,TEXT("after_can_create_connection")))return false;auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("response"),Response.Response);J->SetStringField(TEXT("message"),Response.Message.ToString());J->SetObjectField(TEXT("source_before"),PinToJson(A).ToSharedRef());J->SetObjectField(TEXT("target_before"),PinToJson(B).ToSharedRef());R.Data->SetObjectField(TEXT("last_connection_response"),J);if(!Parent.Current(R,TEXT("after_connection_response_description")))return false;
    if(!Response.CanSafeConnect()&&Response.Response==CONNECT_RESPONSE_DISALLOW) { return Fail(R,TEXT("binding_connection_disallowed"),Response.Message.ToString()); }const FEdGraphPinReference SourceRef(A),TargetRef(B);Count(R,TEXT("try_create_connection_calls"));const bool Made=Schema->TryCreateConnection(A,B);Count(R,TEXT("try_create_connection_returns"));if(!Parent.Current(R,TEXT("after_try_create_connection")))return false;J->SetBoolField(TEXT("returned"),Made);auto* SourceAfter=SourceRef.Get();auto* TargetAfter=TargetRef.Get();J->SetBoolField(TEXT("source_pin_current"),SourceAfter!=nullptr);J->SetBoolField(TEXT("target_pin_current"),TargetAfter!=nullptr);if(SourceAfter) { J->SetObjectField(TEXT("source_after"),PinToJson(SourceAfter).ToSharedRef()); }if(TargetAfter) { J->SetObjectField(TEXT("target_after"),PinToJson(TargetAfter).ToSharedRef()); }if(!Parent.Current(R,TEXT("after_connection_description")))return false;return Made||Fail(R,TEXT("binding_connection_failed"),TEXT("K2 schema failed to create connection; actual returned prefix retained."));
}
void Legacy(UBlueprint* BP,UEdGraph* G,UEdGraphNode* N,FResult& R)
{
    auto J=NodeToJson(N,true);auto H=MakeShared<FJsonObject>(*DescribeGraph(BP,G));H->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());H->RemoveField(TEXT("object_path"));J->SetObjectField(TEXT("graph"),H);R.Data->SetObjectField(TEXT("legacy_result"),J);
}
FResult Cleanup(UBlueprint* BP,UEdGraph* G,UEdGraphNode* N,const TSharedRef<FJsonObject>& Before,const TSet<FGuid>& LinkPins,const FString& Code,const FString& Error,FResult& R,FGeneratedBindingParent& Parent)
{
    if(!Parent.Current(R,TEXT("before_cleanup")))return R;R.Stage=TEXT("destroy_new_node");R.Data->SetBoolField(TEXT("cleanup_attempted"),true);R.Data->SetObjectField(TEXT("new_node_before_destroy"),DescribeNode(N));Count(R,TEXT("destroy_node_calls"));N->DestroyNode();Count(R,TEXT("destroy_node_returns"));if(!Parent.Current(R,TEXT("after_cleanup_destroy")))return R;R.Data->SetBoolField(TEXT("new_node_detached_after_destroy"),!G->Nodes.Contains(N));R.Data->SetStringField(TEXT("retired_node_current_path"),GetPathNameSafe(N));R.bChanged=true;Observe(BP,G,nullptr,Before,LinkPins,R);if(!Parent.Current(R,TEXT("after_cleanup_observation")))return R;Fail(R,*Code,Error+TEXT(" Only original new-node Destroy performed; converter/endpoint/callback effects retained, not atomic restoration."));return R;
}

FResult NativeBind(const FString& Path,const FString& ClassPath,const FString& Requested,const FString& PositionJson,const FString& GN,const FString& GI,const FString& GT,bool Create)
{
    FResult R;Init(R);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("binding_editor_invalid"),TEXT("Idle Editor game thread required."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","NativeDelegateBind","Add Blueprint Native Delegate Bind"));FString Delegate,OwnerText;if(!Name(Requested,Delegate,R)||!Text(ClassPath,OwnerText,false,R)||!ReferenceGuard(OwnerText)) { Fail(R,TEXT("binding_native_input_invalid"),TEXT("Original native delegate name/owner dependency guard refused."));return R; }int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    FGeneratedBindingParent Parent;UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Graph(Path,GN,GI,GT,Create,BP,G,Before,R,Parent)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid GraphGuid=G->GraphGuid;auto* Owner=LoadOwner(OwnerText,false,R,Parent);if(!Parent.Current(R,TEXT("after_native_owner_load")))return R;if(!Owner) { return R; }const TStrongObjectPtr<UClass> KeepOwner(Owner);auto* P=NativeProperty(Owner,FName(*Delegate),R);R.Data->SetObjectField(TEXT("property_before"),PropertyJson(P,R));if(!P||!P->HasAllPropertyFlags(CPF_BlueprintAssignable)||!Loaded(P->SignatureFunction)) { Fail(R,TEXT("binding_native_property_unavailable"),TEXT("Original native bind requires BlueprintAssignable multicast property and current signature."));return R; }const TStrongObjectPtr<UFunction> KeepSignature(P->SignatureFunction);const auto SigBefore=Signature(P->SignatureFunction,R),OwnerBefore=Source(Owner,R);R.Data->SetObjectField(TEXT("signature_before"),SigBefore);R.Data->SetObjectField(TEXT("source_class_before"),OwnerBefore);
    auto* Class=NodeClass(TEXT("/Script/BlueprintGraph.K2Node_AddDelegate"),UK2Node_BaseMCDelegate::StaticClass(),R);if(!Class) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_before"),ClassBefore);if(!Parent.Current(R,TEXT("before_native_bind_allocation")))return R;R.Stage=TEXT("allocate_native_bind");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* N=NewObject<UK2Node_BaseMCDelegate>(G,Class);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node_BaseMCDelegate> KeepNode(N);if(!Parent.Current(R,TEXT("after_native_bind_allocation")))return R;R.Data->SetStringField(TEXT("allocated_node_path"),GetPathNameSafe(N));if(!N) { Fail(R,TEXT("binding_node_allocation_failed"),TEXT("Original native bind allocation returnedNull."));return R; }
    Count(R,TEXT("set_from_property_calls"));N->SetFromProperty(P,false,Owner);Count(R,TEXT("set_from_property_returns"));if(!Parent.Current(R,TEXT("after_set_from_property")))return R;R.Data->SetBoolField(TEXT("self_context"),false);UEdGraphNode* Base=N;Count(R,TEXT("compatibility_calls"));const bool Compatible=Base->IsCompatibleWithGraph(G);if(!Parent.Current(R,TEXT("after_native_graph_compatibility")))return R;R.Data->SetBoolField(TEXT("graph_compatible"),Compatible);if(!Compatible) { R.Data->SetObjectField(TEXT("allocated_node_prefix"),DescribeNode(Base));Fail(R,TEXT("binding_graph_incompatible"),TEXT("Original late compatibility refusal retains allocated external-reference node; no new cleanup."));return R; }
    if(!Place(G,Base,XY,true,R,Parent))return R;R.Stage=TEXT("mark_structural");if(!Structural(BP,R,Parent))return R;const bool Observed=Observe(BP,G,Base,Before,TSet<FGuid>(),R);if(!Parent.Current(R,TEXT("after_native_bind_observation")))return R;auto* Current=NativeProperty(Owner,FName(*Delegate),R);R.Data->SetObjectField(TEXT("property_after"),PropertyJson(Current,R));const auto SigAfter=Signature(Current?Current->SignatureFunction.Get():nullptr,R),OwnerAfter=Source(Owner,R),ClassAfter=Source(Class,R);R.Data->SetObjectField(TEXT("signature_after"),SigAfter);R.Data->SetObjectField(TEXT("source_class_after"),OwnerAfter);R.Data->SetObjectField(TEXT("node_class_after"),ClassAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(SigBefore),MakeShared<FJsonValueObject>(SigAfter))&&Equal(MakeShared<FJsonValueObject>(OwnerBefore),MakeShared<FJsonValueObject>(OwnerAfter))&&Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter));R.Data->SetBoolField(TEXT("source_preserved"),Sources);
    auto* Exec=Base->FindPin(UEdGraphSchema_K2::PN_Execute,EGPD_Input);auto* Then=Base->FindPin(UEdGraphSchema_K2::PN_Then,EGPD_Output);auto* Self=Base->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);auto* D=N->GetDelegatePin();FSimpleMemberReference Member;if(Current&&Current->SignatureFunction) { FMemberReference::FillSimpleMemberReference<UFunction>(Current->SignatureFunction,Member); }
    bool Pins=LivePins(Base)&&Base->Pins.Num()==4&&Exec&&Then&&Self&&D&&Exec->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Then->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Self->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Self->PinType.PinSubCategoryObject.Get()==Owner->GetAuthoritativeClass()&&D->Direction==EGPD_Input&&D->PinType.PinCategory==UEdGraphSchema_K2::PC_Delegate&&D->PinType.bIsReference&&D->PinType.bIsConst&&D->PinType.PinSubCategoryMemberReference==Member;for(const auto* Pin:Base->Pins) { Pins&=Pin->LinkedTo.IsEmpty(); }R.Data->SetBoolField(TEXT("native_bind_pins_verified"),Pins);R.Data->SetStringField(TEXT("delegate_name"),Delegate);R.Data->SetStringField(TEXT("delegate_owner_class"),Owner->GetPathName());R.Data->SetStringField(TEXT("signature_function"),GetPathNameSafe(Current?Current->SignatureFunction.Get():nullptr));Legacy(BP,G,Base,R);auto J=R.Data->GetObjectField(TEXT("legacy_result"));J->SetStringField(TEXT("delegate_name"),Delegate);J->SetStringField(TEXT("delegate_owner_class"),Owner->GetPathName());J->SetStringField(TEXT("signature_function"),GetPathNameSafe(Current?Current->SignatureFunction.Get():nullptr));
    if(!Parent.Current(R,TEXT("after_final_node_description")))return R;if(!Observed) { return R; }if(!Pins||!Sources||!Current||N->GetProperty()!=Current||N->GetDelegateSignature()!=Current->SignatureFunction||N->DelegateReference.IsSelfContext()||Base->GetClass()!=Class||Base->NodePosX!=XY[0]||Base->NodePosY!=XY[1]||!Base->HasAnyFlags(RF_Transactional)||G->GraphGuid!=GraphGuid||R.Data->GetNumberField(TEXT("observed_new_node_count"))!=1) { Fail(R,TEXT("binding_native_postcondition_failed"),TEXT("Actual native-owner external reference/pin/signature/position/source proof differs; prefix retained, no cleanup/replay."));return R; }if(!Parent.Finish(R))return R;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
FResult CreateDelegate(const FString& Path,const FString& Requested,const FString& ObjectId,const FString& TargetId,const FString& ObjectName,const FString& TargetName,const FString& PositionJson,const FString& GN,const FString& GI,const FString& GT,bool Create)
{
    FResult R;Init(R);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("binding_editor_invalid"),TEXT("Original Create Event refuses PIE/SIE and requires Editor game thread."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","CreateDelegate","Add Blueprint Create Event"));FString FunctionName;if(!Name(Requested,FunctionName,R)) { return R; }int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }FGeneratedBindingParent Parent;UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Graph(Path,GN,GI,GT,Create,BP,G,Before,R,Parent)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const auto* Schema=Cast<UEdGraphSchema_K2>(G->GetSchema());const FGuid GraphGuid=G->GraphGuid;
    auto* Object=ResolvePin(G,ObjectId,ObjectName,TEXT("ReturnValue"),EGPD_Output,R);if(!Object) { return R; }R.Data->SetObjectField(TEXT("object_source_before"),PinToJson(Object).ToSharedRef());auto* Target=ResolvePin(G,TargetId,TargetName,TEXT("Delegate"),EGPD_Input,R);if(!Target) { return R; }R.Data->SetObjectField(TEXT("delegate_target_before"),PinToJson(Target).ToSharedRef());const FEdGraphPinReference ObjectRef(Object),TargetRef(Target);const TStrongObjectPtr<UEdGraphNode> KeepObject(Object->GetOwningNode()),KeepTarget(Target->GetOwningNode());TSet<FGuid> LinkPins={Object->PinId,Target->PinId};
    auto* Class=NodeClass(TEXT("/Script/BlueprintGraph.K2Node_CreateDelegate"),UK2Node::StaticClass(),R);if(!Class) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_before"),ClassBefore);if(!Parent.Current(R,TEXT("before_create_delegate_allocation")))return R;R.Stage=TEXT("allocate_create_delegate");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* K=NewObject<UK2Node>(G,Class);Count(R,TEXT("node_allocation_returns"));auto* N=Cast<UK2Node_CreateDelegate>(K);const TStrongObjectPtr<UK2Node> KeepNode(K);if(!Parent.Current(R,TEXT("after_create_delegate_allocation")))return R;R.Data->SetStringField(TEXT("allocated_node_path"),GetPathNameSafe(K));if(!N) { Fail(R,TEXT("binding_node_allocation_failed"),TEXT("Create Event allocation returnedNull/type differs."));return R; }UEdGraphNode* Base=N;if(!Place(G,Base,XY,false,R,Parent))return R;
    R.Stage=TEXT("connect_object");if(!Connect(Schema,ObjectRef.Get(),N->GetObjectInPin(),R,Parent)) { const FString C=R.ErrorCode,E=R.Error;return Cleanup(BP,G,Base,Before,LinkPins,C,E,R,Parent); }R.Data->SetObjectField(TEXT("object_connection_returned"),MakeShared<FJsonObject>(*R.Data->GetObjectField(TEXT("last_connection_response"))));R.Data->SetObjectField(TEXT("after_object_connection_node"),DescribeNode(Base));R.Stage=TEXT("connect_delegate");if(!Connect(Schema,N->GetDelegateOutPin(),TargetRef.Get(),R,Parent)) { const FString C=R.ErrorCode,E=R.Error;return Cleanup(BP,G,Base,Before,LinkPins,C,E,R,Parent); }R.Data->SetObjectField(TEXT("delegate_connection_returned"),MakeShared<FJsonObject>(*R.Data->GetObjectField(TEXT("last_connection_response"))));
    R.Stage=TEXT("select_function_after_connections");Count(R,TEXT("set_function_calls"));N->SetFunction(FName(*FunctionName));Count(R,TEXT("set_function_returns"));if(!Parent.Current(R,TEXT("after_set_function")))return R;R.Data->SetStringField(TEXT("selected_function_before_handle"),N->GetFunctionName().ToString());Count(R,TEXT("handle_any_change_calls"));N->HandleAnyChange(true);Count(R,TEXT("handle_any_change_returns"));if(!Parent.Current(R,TEXT("after_handle_any_change")))return R;R.Data->SetStringField(TEXT("selected_function_after_handle"),N->GetFunctionName().ToString());R.Data->SetStringField(TEXT("selected_function_guid"),N->SelectedFunctionGuid.ToString());R.Data->SetObjectField(TEXT("after_handle_any_change_node"),DescribeNode(Base));if(!Mark(BP,R,Parent))return R;const bool Observed=Observe(BP,G,Base,Before,LinkPins,R);if(!Parent.Current(R,TEXT("after_connected_node_observation")))return R;Object=ObjectRef.Get();Target=TargetRef.Get();auto* In=N->GetObjectInPin();auto* Out=N->GetDelegateOutPin();auto* ScopeClass=N->GetScopeClass();auto* Sig=N->GetDelegateSignature();const TStrongObjectPtr<UClass> KeepScope(ScopeClass);const TStrongObjectPtr<UFunction> KeepSignature(Sig);R.Data->SetStringField(TEXT("scope_class"),GetPathNameSafe(ScopeClass));R.Data->SetObjectField(TEXT("current_delegate_signature"),Signature(Sig,R));R.Data->SetStringField(TEXT("function_name"),FunctionName);R.Data->SetBoolField(TEXT("runtime_bindability_verified"),false);R.Data->SetStringField(TEXT("binding_validation_scope"),TEXT("Original creator keeps function hint even when SDK internal IsValid clears GUID; no added early callable/signature-validity gate. Current graph/pure-node/connections/selection are verified; full compiler/runtime bindability separately unverified."));const auto ClassAfter=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_after"),ClassAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter));R.Data->SetBoolField(TEXT("source_preserved"),Sources);const bool Edges=Object&&Target&&In&&Out&&In->LinkedTo.Contains(Object)&&Object->LinkedTo.Contains(In)&&Out->LinkedTo.Contains(Target)&&Target->LinkedTo.Contains(Out);R.Data->SetBoolField(TEXT("both_delegate_edges_verified"),Edges);if(Object) { R.Data->SetObjectField(TEXT("object_source_after"),PinToJson(Object).ToSharedRef()); }if(Target) { R.Data->SetObjectField(TEXT("delegate_target_after"),PinToJson(Target).ToSharedRef()); }Legacy(BP,G,Base,R);R.Data->GetObjectField(TEXT("legacy_result"))->SetStringField(TEXT("function_name"),FunctionName);
    if(!Parent.Current(R,TEXT("after_final_node_description")))return R;if(!Observed) { return R; }if(!LivePins(Base)||Base->Pins.Num()!=2||!In||!Out||In->Direction!=EGPD_Input||In->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object||Out->Direction!=EGPD_Output||Out->PinType.PinCategory!=UEdGraphSchema_K2::PC_Delegate||!K->IsNodePure()||!Edges||!Sources||N->GetFunctionName()!=FName(*FunctionName)||Base->GetClass()!=Class||Base->NodePosX!=XY[0]||Base->NodePosY!=XY[1]||!Base->HasAnyFlags(RF_Transactional)||G->GraphGuid!=GraphGuid||R.Data->GetNumberField(TEXT("observed_new_node_count"))!=1) { Fail(R,TEXT("binding_create_delegate_postcondition_failed"),TEXT("Actual pure selection/typed endpoint/link/old-state proof differs; accepted construction/schema side effects retained without new success-boundary cleanup or replay."));return R; }if(!Parent.Finish(R))return R;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
FResult Message(const FString& Path,const FString& RequestedClass,const FString& RequestedFunction,const FString& TargetId,const FString& TargetName,const FString& PositionJson,const FString& GN,const FString& GI,const FString& GT,bool Create)
{
    FResult R;Init(R);if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("binding_editor_invalid"),TEXT("Original interface Message refuses PIE/SIE and requires Editor game thread."));return R; }Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","InterfaceMessage","Add Blueprint Interface Message"));FString FunctionName;if(!Name(RequestedFunction,FunctionName,R)) { return R; }int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }FGeneratedBindingParent Parent;UBlueprint* BP=nullptr;UEdGraph* G=nullptr;TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();if(!Graph(Path,GN,GI,GT,Create,BP,G,Before,R,Parent)) { return R; }const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const auto* Schema=Cast<UEdGraphSchema_K2>(G->GetSchema());const FGuid GraphGuid=G->GraphGuid;auto* Interface=LoadOwner(RequestedClass,true,R,Parent);if(!Parent.Current(R,TEXT("after_interface_load")))return R;if(!Interface) { return R; }const TStrongObjectPtr<UClass> KeepInterface(Interface);auto* F=LookupFunction(Interface,FunctionName,R);if(!Parent.Current(R,TEXT("after_interface_function_lookup")))return R;if(!F) { return R; }const TStrongObjectPtr<UFunction> KeepFunction(F);const auto FunctionBefore=Signature(F,R),InterfaceBefore=Source(Interface,R);R.Data->SetObjectField(TEXT("function_before"),FunctionBefore);R.Data->SetObjectField(TEXT("interface_before"),InterfaceBefore);
    auto* Class=NodeClass(TEXT("/Script/BlueprintGraph.K2Node_Message"),UK2Node_CallFunction::StaticClass(),R);if(!Class) { return R; }const TStrongObjectPtr<UClass> KeepClass(Class);const auto ClassBefore=Source(Class,R);R.Data->SetObjectField(TEXT("node_class_before"),ClassBefore);if(!Parent.Current(R,TEXT("before_message_allocation")))return R;R.Stage=TEXT("allocate_interface_message");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* N=NewObject<UK2Node_CallFunction>(G,Class);Count(R,TEXT("node_allocation_returns"));const TStrongObjectPtr<UK2Node_CallFunction> KeepNode(N);if(!Parent.Current(R,TEXT("after_message_allocation")))return R;R.Data->SetStringField(TEXT("allocated_node_path"),GetPathNameSafe(N));if(!N) { Fail(R,TEXT("binding_node_allocation_failed"),TEXT("Message allocation returnedNull."));return R; }Count(R,TEXT("function_reference_write_calls"));if(!Parent.Current(R,TEXT("before_message_function_reference")))return R;N->FunctionReference.SetFromField<UFunction>(F,false);if(!Parent.Current(R,TEXT("after_message_function_reference")))return R;UEdGraphNode* Base=N;if(!Place(G,Base,XY,true,R,Parent))return R;
    auto* Target=ResolvePin(G,TargetId,TargetName,TEXT("ReturnValue"),EGPD_Output,R);TSet<FGuid> LinkPins;if(!Target) { const FString C=R.ErrorCode,E=R.Error;return Cleanup(BP,G,Base,Before,LinkPins,C,E,R,Parent); }const TStrongObjectPtr<UEdGraphNode> KeepTarget(Target->GetOwningNode());const FEdGraphPinReference TargetRef(Target);LinkPins.Add(Target->PinId);R.Data->SetObjectField(TEXT("message_target_before"),PinToJson(Target).ToSharedRef());auto* Self=Schema->FindSelfPin(*N,EGPD_Input);if(!Self) { return Cleanup(BP,G,Base,Before,LinkPins,TEXT("binding_message_self_missing"),TEXT("Original Message self pin missing"),R,Parent); }
    R.Stage=TEXT("connect_message_target");if(!Connect(Schema,TargetRef.Get(),Self,R,Parent)) { const FString C=R.ErrorCode,E=R.Error;return Cleanup(BP,G,Base,Before,LinkPins,C,E,R,Parent); }R.Data->SetObjectField(TEXT("message_connection_returned"),MakeShared<FJsonObject>(*R.Data->GetObjectField(TEXT("last_connection_response"))));if(!Mark(BP,R,Parent))return R;const bool Observed=Observe(BP,G,Base,Before,LinkPins,R);if(!Parent.Current(R,TEXT("after_connected_node_observation")))return R;Target=TargetRef.Get();Self=Schema->FindSelfPin(*N,EGPD_Input);const auto FunctionAfter=Signature(F,R),InterfaceAfter=Source(Interface,R),ClassAfter=Source(Class,R);R.Data->SetObjectField(TEXT("function_after"),FunctionAfter);R.Data->SetObjectField(TEXT("interface_after"),InterfaceAfter);R.Data->SetObjectField(TEXT("node_class_after"),ClassAfter);const bool Sources=Equal(MakeShared<FJsonValueObject>(FunctionBefore),MakeShared<FJsonValueObject>(FunctionAfter))&&Equal(MakeShared<FJsonValueObject>(InterfaceBefore),MakeShared<FJsonValueObject>(InterfaceAfter))&&Equal(MakeShared<FJsonValueObject>(ClassBefore),MakeShared<FJsonValueObject>(ClassAfter));R.Data->SetBoolField(TEXT("source_preserved"),Sources);const bool Edge=Self&&Target&&Self->LinkedTo.Contains(Target)&&Target->LinkedTo.Contains(Self);R.Data->SetBoolField(TEXT("message_target_edge_verified"),Edge);if(Target) { R.Data->SetObjectField(TEXT("message_target_after"),PinToJson(Target).ToSharedRef()); }R.Data->SetStringField(TEXT("interface_class"),Interface->GetPathName());R.Data->SetStringField(TEXT("function_name"),F->GetName());R.Data->SetStringField(TEXT("function_path"),F->GetPathName());R.Data->SetBoolField(TEXT("interface_message_runtime_invoked"),false);Legacy(BP,G,Base,R);auto J=R.Data->GetObjectField(TEXT("legacy_result"));J->SetStringField(TEXT("interface_class"),Interface->GetPathName());J->SetStringField(TEXT("function_name"),F->GetName());J->SetStringField(TEXT("function_path"),F->GetPathName());
    if(!Parent.Current(R,TEXT("after_final_node_description")))return R;if(!Observed) { return R; }if(!LivePins(Base)||!Self||Self->PinType.PinCategory!=UEdGraphSchema_K2::PC_Object||Self->PinType.PinSubCategoryObject.Get()!=UObject::StaticClass()||!Self->bDefaultValueIsIgnored||static_cast<UK2Node*>(N)->IsNodePure()||!Edge||!Sources||N->GetTargetFunction()!=F||Base->GetClass()!=Class||Base->NodePosX!=XY[0]||Base->NodePosY!=XY[1]||!Base->HasAnyFlags(RF_Transactional)||G->GraphGuid!=GraphGuid||R.Data->GetNumberField(TEXT("observed_new_node_count"))!=1) { Fail(R,TEXT("binding_message_postcondition_failed"),TEXT("Actual interface function/Message self/target link/old graph/source proof differs; accepted prefix retained without new cleanup or replay."));return R; }if(!Parent.Finish(R))return R;R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}

void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("generated_binding_membership_calls"),TEXT("allocate_pins_calls"),TEXT("blueprint_capture_calls"),TEXT("can_create_connection_calls"),TEXT("compatibility_calls"),TEXT("destroy_node_calls"),TEXT("destroy_node_returns"),TEXT("endpoint_node_lookup_calls"),TEXT("endpoint_node_lookup_returns"),TEXT("endpoint_pin_lookup_calls"),TEXT("endpoint_pin_lookup_returns"),TEXT("explicit_graph_modify_calls"),TEXT("function_lookup_calls"),TEXT("function_lookup_returns"),TEXT("function_reference_write_calls"),TEXT("graph_add_node_calls"),TEXT("graph_capture_calls"),TEXT("handle_any_change_calls"),TEXT("handle_any_change_returns"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("mark_structural_calls"),TEXT("mark_structural_returns"),TEXT("new_guid_calls"),TEXT("node_allocation_calls"),TEXT("node_allocation_returns"),TEXT("node_class_lookup_calls"),TEXT("node_class_lookup_returns"),TEXT("outer_transaction_calls"),TEXT("post_placed_calls"),TEXT("post_placed_returns"),TEXT("property_lookup_calls"),TEXT("property_lookup_returns"),TEXT("property_observation_calls"),TEXT("reconstruct_calls"),TEXT("set_from_property_calls"),TEXT("set_from_property_returns"),TEXT("set_function_calls"),TEXT("set_function_returns"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("signature_observation_calls"),TEXT("source_blueprint_load_calls"),TEXT("source_blueprint_load_returns"),TEXT("source_class_load_calls"),TEXT("source_class_load_returns"),TEXT("source_class_lookup_calls"),TEXT("source_class_lookup_returns"),TEXT("source_observation_calls"),TEXT("transactional_flag_write_calls"),TEXT("try_create_connection_calls"),TEXT("try_create_connection_returns"),TEXT("variable_observation_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("automatic_retry_calls"),TEXT("explicit_blueprint_modify_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetBoolField(TEXT("cleanup_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);
    R.Data->SetStringField(TEXT("counter_scope"),TEXT("Direct calls/returns only. Structural may synchronously regenerate skeleton/CDO/cache/children. Schema connections can break links/convert/reconstruct/notify/Modify. HandleAnyChange(true) internally validates and MarkModified/Broadcast/Notify; final explicit MarkModified is separate. Explicit compiler/save0 is not total effects0."));
    R.Data->SetStringField(TEXT("preservation_scope"),TEXT("Ordered owned K2 graphs/nodes/pin IDs/types/defaults/member descriptor text; only intended endpoint linked_to fields may differ. Actual new/schema-converter/cleanup prefixes retained. Arbitrary CDO/SCS/world/widget preview/cache/children and full runtime binding/message execution remain unverified; no atomic restoration."));
}
}
Function::FResult AddNativeBind(const FString& P,const FString& C,const FString& D,const FString& X,const FString& N,const FString& I,const FString& T,bool G) { return NativeBind(P,C,D,X,N,I,T,G); }
Function::FResult AddCreateDelegate(const FString& P,const FString& F,const FString& O,const FString& D,const FString& OP,const FString& DP,const FString& X,const FString& N,const FString& I,const FString& T,bool G) { return CreateDelegate(P,F,O,D,OP,DP,X,N,I,T,G); }
Function::FResult AddMessage(const FString& P,const FString& C,const FString& F,const FString& O,const FString& OP,const FString& X,const FString& N,const FString& I,const FString& T,bool G) { return Message(P,C,F,O,OP,X,N,I,T,G); }
}
