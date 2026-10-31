#include "Compatibility/UE58/CubelessActorInterfaceApi.h"
#include "Dom/JsonObject.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Interface.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::ActorInterface
{
FString AttachEventInterface(UObject* Object, UObject* InterfaceObject, bool Execute)
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    R->SetBoolField(TEXT("mutation_attempted"), false);
    R->SetBoolField(TEXT("changed"), false);
    R->SetBoolField(TEXT("change_state_known"), true);
    R->SetBoolField(TEXT("saved"), false);
    R->SetBoolField(TEXT("save_attempted"), false);
    R->SetBoolField(TEXT("explicit_compile_attempted"), false);
    R->SetBoolField(TEXT("atomic_rollback"), false);
    R->SetBoolField(TEXT("automatic_retry"), false);
    R->SetBoolField(TEXT("whole_authored_graph_values_verified"), false);
    R->SetStringField(TEXT("stage"), TEXT("preflight"));
    auto Encode = [&]() { FString S; const auto W = TJsonWriterFactory<>::Create(&S); FJsonSerializer::Serialize(R, W); return S; };
    auto Fail = [&](const TCHAR* Code, const TCHAR* Message) { R->SetStringField(TEXT("error_code"), Code); R->SetStringField(TEXT("error"), Message); return Encode(); };
    if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
        return Fail(TEXT("editor_idle_required"), TEXT("Run on the idle Editor game thread, outside PIE/SIE."));
    auto* BP = Cast<UBlueprint>(Object);
    auto* Source = Cast<UBlueprint>(InterfaceObject);
    if (!IsValid(BP) || BP->GetClass() != UBlueprint::StaticClass() || BP->BlueprintType != BPTYPE_Normal ||
        !IsValid(BP->ParentClass) || !BP->ParentClass->IsChildOf(AActor::StaticClass()) || BP->bBeingCompiled || BP->Status == BS_Error)
        return Fail(TEXT("actor_blueprint_required"), TEXT("An existing normal Actor/Character Blueprint without active compile/errors is required."));
    if (!IsValid(Source) || Source == BP || Source->GetClass() != UBlueprint::StaticClass() || Source->BlueprintType != BPTYPE_Interface ||
        Source->ParentClass != UInterface::StaticClass() || Source->bBeingCompiled || Source->Status != BS_UpToDate || Source->GetPackage()->IsDirty() ||
        !IsValid(Source->GeneratedClass) || Source->GeneratedClass->ClassGeneratedBy != Source)
        return Fail(TEXT("compiled_clean_interface_required"), TEXT("An already loaded, compiled, saved direct Blueprint Interface is required."));
    if (!BP->GetPackage()->GetName().StartsWith(TEXT("/Game/")) || !Source->GetPackage()->GetName().StartsWith(TEXT("/Game/")))
        return Fail(TEXT("game_asset_required"), TEXT("Explicit project /Game assets only; no engine/plugin source edits."));
    UClass* Interface = Source->GeneratedClass;
    TStrongObjectPtr<UBlueprint> HoldTarget(BP), HoldSource(Source);
    TStrongObjectPtr<UClass> HoldInterface(Interface), HoldParent(BP->ParentClass);
    const TWeakObjectPtr<UWorld> World = GEditor->GetEditorWorldContext().World();
    const FString TargetPath = BP->GetPathName(), SourcePath = Source->GetPathName();
    R->SetStringField(TEXT("target"), TargetPath); R->SetStringField(TEXT("interface"), Interface->GetPathName());
    int32 Methods = 0;
    for (TFieldIterator<UFunction> It(Interface, EFieldIterationFlags::None); It; ++It)
    {
        ++Methods;
        if (Methods > 16 || !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(*It) || It->GetReturnProperty() ||
            It->HasAnyFunctionFlags(FUNC_Net | FUNC_Static | FUNC_Delegate))
            return Fail(TEXT("event_interface_required"), TEXT("Bounded event-shape interfaces only; output/function/RPC interfaces are not handled by this adapter."));
        int32 Params = 0;
        for (TFieldIterator<FProperty> P(*It, EFieldIterationFlags::None); P; ++P)
        {
            if (!P->HasAnyPropertyFlags(CPF_Parm)) continue;
            if (++Params > 8 || P->HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm | CPF_ReturnParm))
                return Fail(TEXT("parameter_profile_required"), TEXT("At most eight by-value input parameters; no output/reference/container signature."));
            const auto* O = CastField<FObjectPropertyBase>(*P);
            const bool NativeObject = O && (O->PropertyClass == USkeletalMeshComponent::StaticClass() || O->PropertyClass == UAnimSequenceBase::StaticClass());
            if (!NativeObject && !CastField<FBoolProperty>(*P) && !CastField<FFloatProperty>(*P) && !CastField<FDoubleProperty>(*P) &&
                !CastField<FIntProperty>(*P) && !CastField<FStrProperty>(*P) && !CastField<FNameProperty>(*P))
                return Fail(TEXT("parameter_type_not_supported"), TEXT("Primitive inputs or native Engine SkeletalMeshComponent/AnimSequenceBase only; no MCP/project runtime class dependencies."));
        }
        if (BP->GeneratedClass && BP->GeneratedClass->FindFunctionByName(It->GetFName()) && !BP->GeneratedClass->ImplementsInterface(Interface))
            return Fail(TEXT("function_name_conflict"), TEXT("Existing same-name target/inherited function would conflict with the interface."));
    }
    if (Methods == 0) return Fail(TEXT("empty_interface"), TEXT("The source interface must declare at least one event."));
    auto Present = [&]() { return BP->ImplementedInterfaces.ContainsByPredicate([&](const FBPInterfaceDescription& I) { return I.Interface == Interface; }); };
    const bool Before = Present();
    R->SetBoolField(TEXT("already_present"), Before);
    if (!Execute || Before)
    {
        R->SetBoolField(TEXT("success"), true); R->SetBoolField(TEXT("interface_declaration_present"), Before);
        R->SetStringField(TEXT("stage"), Before ? TEXT("already_declared") : TEXT("preflight_passed")); return Encode();
    }
    TArray<UEdGraph*> BeforeGraphs; BP->GetAllGraphs(BeforeGraphs);
    TArray<FGuid> VariableIds; TArray<FName> VariableNames;
    for (const auto& V : BP->NewVariables) { VariableIds.Add(V.VarGuid); VariableNames.Add(V.VarName); }
    auto Current = [&]() { return IsValid(BP) && IsValid(Source) && BP->GetPathName() == TargetPath && Source->GetPathName() == SourcePath &&
        BP->BlueprintType == BPTYPE_Normal && !BP->bBeingCompiled && BP->Status != BS_Error &&
        BP->ParentClass == HoldParent.Get() && Source->BlueprintType == BPTYPE_Interface && Source->ParentClass == UInterface::StaticClass() &&
        Source->GeneratedClass == HoldInterface.Get() && !Source->bBeingCompiled &&
        Source->Status == BS_UpToDate && !Source->GetPackage()->IsDirty() && GEditor && !GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor &&
        World.IsValid() && GEditor->GetEditorWorldContext().World() == World.Get(); };
    R->SetBoolField(TEXT("mutation_attempted"), true);
    R->SetField(TEXT("changed"), MakeShared<FJsonValueNull>());
    R->SetBoolField(TEXT("change_state_known"), false);
    R->SetStringField(TEXT("stage"), TEXT("modify_target"));
    BP->Modify();
    if (!Current()) return Fail(TEXT("context_changed_after_modify"), TEXT("Context changed after Modify; no interface writer dispatched. Inspect actual state; no rollback/retry."));
    R->SetStringField(TEXT("stage"), TEXT("implement_interface"));
    const bool EngineReturn = FBlueprintEditorUtils::ImplementNewInterface(BP, Interface->GetClassPathName());
    R->SetBoolField(TEXT("engine_return"), EngineReturn);
    R->SetBoolField(TEXT("implicit_structural_compile_requested"), EngineReturn);
    if (!Current()) return Fail(TEXT("context_changed_after_implement"), TEXT("Context changed after engine attachment. Inspect the target and preserve partial changes."));
    const bool After = Present(); R->SetBoolField(TEXT("interface_declaration_present"), After);
    R->SetBoolField(TEXT("interface_declaration_changed"), Before != After); R->SetBoolField(TEXT("dirty"), BP->GetPackage()->IsDirty());
    TArray<UEdGraph*> AfterGraphs; BP->GetAllGraphs(AfterGraphs);
    bool InventorySame = BeforeGraphs == AfterGraphs && VariableIds.Num() == BP->NewVariables.Num();
    for (int32 I = 0; InventorySame && I < VariableIds.Num(); ++I)
        InventorySame = VariableIds[I] == BP->NewVariables[I].VarGuid && VariableNames[I] == BP->NewVariables[I].VarName;
    R->SetBoolField(TEXT("existing_graph_and_variable_inventory_preserved"), InventorySame);
    if (!InventorySame) return Fail(TEXT("authored_inventory_changed"), TEXT("Existing graph/variable inventory changed; keep actual interface/dirty result, no rollback/retry."));
    if (!EngineReturn || !After) return Fail(TEXT("interface_attachment_failed"), TEXT("Engine return or observed source declaration failed; preserve actual state."));
    R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("stage"), TEXT("interface_declared_compile_save_required"));
    R->SetStringField(TEXT("scope"), TEXT("Source declaration only; caller explicitly compiles/saves and verifies authored behavior. No full callback/graph-value preservation claim."));
    return Encode();
}
}
