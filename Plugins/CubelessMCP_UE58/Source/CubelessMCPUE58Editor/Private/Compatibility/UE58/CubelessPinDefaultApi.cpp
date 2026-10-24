// UE56 set_blueprint_pin_default via public schema APIs and exact readback.
#include "Compatibility/UE58/CubelessPinDefaultApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Editor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeSet.h"
#include "K2Node_MakeMap.h"
#include "K2Node_Select.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::PinDefault
{
namespace
{
namespace Pin = Function::Pin;
bool Fail(Function::FResult& R, const TCHAR* Code, const TCHAR* Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
FString Text(const TSharedRef<FJsonObject>& J)
{ FString Out; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out)); return Out; }
TSharedRef<FJsonObject> ObservePin(const UEdGraphPin* P)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("name"), P->PinName.ToString());
    J->SetStringField(TEXT("pin_id"), P->PinId.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("direction"), P->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    J->SetObjectField(TEXT("pin_type"), Pin::Describe(P->PinType));
    FString TypeText; FEdGraphPinType::StaticStruct()->ExportText(TypeText, &P->PinType, &P->PinType, nullptr, PPF_None, nullptr);
    J->SetStringField(TEXT("pin_type_text"), TypeText); J->SetStringField(TEXT("default_value"), P->DefaultValue);
    J->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString());
    J->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : FString());
    J->SetBoolField(TEXT("hidden"), P->bHidden); J->SetBoolField(TEXT("default_ignored"), P->bDefaultValueIsIgnored);
    TArray<TSharedPtr<FJsonValue>> Links;
    for (const UEdGraphPin* Other : P->LinkedTo)
    {
        auto Link = MakeShared<FJsonObject>(); Link->SetStringField(TEXT("node_path"), Other->GetOwningNode()->GetPathName());
        Link->SetStringField(TEXT("pin_id"), Other->PinId.ToString(EGuidFormats::Digits)); Link->SetStringField(TEXT("pin_name"), Other->PinName.ToString());
        Links.Add(MakeShared<FJsonValueObject>(Link));
    }
    J->SetArrayField(TEXT("linked_to"), Links); return J;
}
bool CaptureNode(const UK2Node* Node, const TSharedRef<FJsonObject>& J)
{
    if (!IsValid(Node) || !Node->NodeGuid.IsValid() || Node->Pins.Num() > 4096) { return false; }
    J->SetStringField(TEXT("object_path"), Node->GetPathName()); J->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); J->SetNumberField(TEXT("x"), Node->NodePosX); J->SetNumberField(TEXT("y"), Node->NodePosY);
    TArray<TSharedPtr<FJsonValue>> Pins; TSet<FGuid> Seen;
    for (const UEdGraphPin* P : Node->Pins)
    {
        if (!P || P->WasTrashed() || !P->PinId.IsValid() || Seen.Contains(P->PinId)) { return false; } Seen.Add(P->PinId);
        for (const UEdGraphPin* Other : P->LinkedTo)
        { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(P))) { return false; } }
        Pins.Add(MakeShared<FJsonValueObject>(ObservePin(P)));
    }
    J->SetArrayField(TEXT("pins"), Pins); return true;
}
// LV: one exact native setter input; global primitive/enum parsing stays unchanged.
bool TextureSetterClass(const UK2Node* Node,FString& Path)
{
    if(!IsValid(Node)||Node->GetClass()!=UK2Node_VariableSet::StaticClass())return false;
    const auto* V=static_cast<const UK2Node_VariableSet*>(Node);const FName Name=V->VariableReference.GetMemberName();
    UClass* C=(Name==TEXT("AddressX")||Name==TEXT("AddressY"))?UTextureRenderTarget2D::StaticClass():(Name==TEXT("Filter")||Name==TEXT("LODGroup"))?UTexture::StaticClass():nullptr;
    if(!C)return false;FMemberReference E;E.SetExternalMember(Name,C);if(!V->VariableReference.IsSameReference(E)||V->VariableReference.IsDeprecated())return false;
    Path=C==UTexture::StaticClass()?TEXT("/Script/Engine.Texture"):TEXT("/Script/Engine.TextureRenderTarget2D");return true;
}
Function::FResult SetTextureEnum(UEdGraph* G,UBlueprint* BP,UK2Node* Node,const FString& ClassPath,const FString& PinName,const FString& ValueJson,const TSharedRef<FJsonObject>& Identities)
{
    Function::FResult R;R.Data->SetBoolField(TEXT("native_texture_enum_pin_default"),true);R.Data->SetObjectField(TEXT("graph_identity_before"),Identities);
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    TStrongObjectPtr<UBlueprint> KeepBP(BP);TStrongObjectPtr<UEdGraph> KeepGraph(G);TStrongObjectPtr<UK2Node> KeepNode(Node);TStrongObjectPtr<UEditorEngine> Editor(GEditor);TStrongObjectPtr<UWorld> World(GWorld);auto* Undo=GUndo;
    const FString GraphPath=G->GetPathName(),NodePath=Node->GetPathName();const FGuid Guid=Node->NodeGuid;const FMemberReference Reference=static_cast<UK2Node_VariableSet*>(Node)->VariableReference;const FName Variable=Reference.GetMemberName();
    auto Stop=[&](const TCHAR* M){R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;Fail(R,TEXT("pin_default_texture_scope_changed"),M);return false;};
    const auto ParentHandle=BP->ParentClass.GetGCPtr(),GeneratedHandle=BP->GeneratedClass.GetGCPtr(),SkeletonHandle=BP->SkeletonGeneratedClass.GetGCPtr(),GraphSchemaHandle=G->Schema.GetGCPtr();
    UClass* SchemaClass=UEdGraphSchema_K2::StaticClass();
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const auto SchemaCDOHandle=SchemaClass->ClassDefaultObject;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    TArray<TStrongObjectPtr<UObject>> ClassHolds;ClassHolds.Emplace(SchemaClass);
    const auto ClassFromHandle=[&](const TObjectPtr<UClass>& H)->UClass*{if(!H.IsResolved()||H==nullptr)return nullptr;UClass* C=FindObject<UClass>(nullptr,*H.GetPathName());if(!Loaded(C)||H!=C)return nullptr;ClassHolds.Emplace(C);return C;};
    UClass* Parent=ClassFromHandle(ParentHandle);UClass* Generated=ClassFromHandle(GeneratedHandle);UClass* Skeleton=ClassFromHandle(SkeletonHandle);
    UObject* SchemaObject=SchemaCDOHandle.IsResolved()&&SchemaCDOHandle!=nullptr?FindObject<UObject>(nullptr,*SchemaCDOHandle.GetPathName()):nullptr;
    if(Loaded(SchemaObject))ClassHolds.Emplace(SchemaObject);
    if(!Parent||!Generated||!Skeleton||!GraphSchemaHandle.IsResolved()||GraphSchemaHandle!=SchemaClass||!SchemaCDOHandle.IsResolved()||SchemaCDOHandle!=SchemaObject||!Loaded(SchemaObject)||SchemaObject->GetClass()!=SchemaClass||!SchemaObject->HasAnyFlags(RF_ClassDefaultObject)||!SchemaObject->GetPathName().Equals(TEXT("/Script/BlueprintGraph.Default__EdGraphSchema_K2"),ESearchCase::CaseSensitive)){Stop(TEXT("Existing exact target classes and schema CDO required before enum-default SDK."));return R;}
    struct FRole{TStrongObjectPtr<UObject> Held;UClass* Class;UObject* Outer;UPackage* Package;FString Path;EObjectFlags Flags;UClass* NativeClass;uint32 ClassFlags;int32 Size;};TArray<FRole> Roles;
    const auto HoldRole=[&](UObject* O,UClass* K){Roles.Add({TStrongObjectPtr<UObject>(O),O->GetClass(),O->GetOuter(),O->GetPackage(),O->GetPathName(),O->GetFlags(),K,K?uint32(K->GetClassFlags()):0,K?K->GetStructureSize():0});};
    HoldRole(Parent,Parent);HoldRole(Generated,Generated);HoldRole(Skeleton,Skeleton);HoldRole(SchemaClass,SchemaClass);HoldRole(SchemaObject,nullptr);const auto* Schema=static_cast<const UEdGraphSchema_K2*>(SchemaObject);
    auto Basic=[&](){
        if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||!Loaded(World.Get())||GWorld!=World.Get()||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||GUndo!=Undo||!Loaded(BP)||!Loaded(G)||!Loaded(Node)||Node->GetClass()!=UK2Node_VariableSet::StaticClass()||Node->GetOuter()!=G||Node->NodeGuid!=Guid||!Node->GetPathName().Equals(NodePath,ESearchCase::CaseSensitive)||!G->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||!G->Nodes.Contains(Node)||!static_cast<UK2Node_VariableSet*>(Node)->VariableReference.IsSameReference(Reference)||static_cast<UK2Node_VariableSet*>(Node)->VariableReference.IsDeprecated())return false;
        for(const auto& A:Roles){const UObject* O=A.Held.Get();if(!Loaded(O)||O->GetClass()!=A.Class||O->GetOuter()!=A.Outer||O->GetPackage()!=A.Package||!O->GetPathName().Equals(A.Path,ESearchCase::CaseSensitive)||O->GetFlags()!=A.Flags||(A.NativeClass&&(uint32(A.NativeClass->GetClassFlags())!=A.ClassFlags||A.NativeClass->GetStructureSize()!=A.Size)))return false;}
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto CurrentCDO=SchemaClass->ClassDefaultObject;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        return BP->ParentClass.GetGCPtr().IsResolved()&&BP->ParentClass.GetGCPtr()==Parent&&BP->GeneratedClass.GetGCPtr().IsResolved()&&BP->GeneratedClass.GetGCPtr()==Generated&&BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&BP->SkeletonGeneratedClass.GetGCPtr()==Skeleton&&G->Schema.GetGCPtr().IsResolved()&&G->Schema.GetGCPtr()==SchemaClass&&CurrentCDO.IsResolved()&&CurrentCDO==SchemaObject;
    };
    if(!Basic()){Stop(TEXT("Exact idle held native setter roles required."));return R;}
    if(FName(*PinName)!=Variable){Fail(R,TEXT("pin_default_texture_input_unsupported"),TEXT("Only the exact property value input is admitted; output/self/other native inputs remain unsupported."));return R;}
    TSharedPtr<FJsonObject> Baseline,BeforeRow,LastRow;FGuid ValueId;bool AllowDefault=false;
    auto Normalize=[&](const TSharedRef<FJsonObject>& Row,TSharedPtr<FJsonObject>& Copy)->bool
    {if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(Row)),Copy)||!Copy.IsValid())return false;int32 Count=0;for(const auto& V:Copy->GetArrayField(TEXT("pins"))){auto P=V->AsObject();if(P->GetStringField(TEXT("id"))==ValueId.ToString(EGuidFormats::Digits)&&P->GetStringField(TEXT("name")).Equals(Variable.ToString(),ESearchCase::CaseSensitive)&&P->GetStringField(TEXT("direction"))==TEXT("input")){P->RemoveField(TEXT("default"));++Count;}}return Count==1;};
    auto Current=[&]()->bool
    {
        if(!Basic())return Stop(TEXT("Editor/Undo/held setter role changed; no further SDK."));
        auto Plan=Graph::ReadExternalMemberVariablePlan(GraphPath,Variable.ToString(),true,ClassPath);R.Data->SetObjectField(TEXT("native_texture_last_plan"),Plan);
        if(!Plan->GetBoolField(TEXT("success"))||!Basic())return Stop(TEXT("Native source/current plan failed; actual prefix retained without another SDK or retry."));
        if(Plan->GetStringField(TEXT("member_scope"))!=TEXT("external_native_texture_enum_setter"))return Stop(TEXT("Native member scope differs."));
        if(Baseline.IsValid())
        {
            for(const TCHAR* K:{TEXT("property_path"),TEXT("expected_pin_type_text"),TEXT("expected_self_pin_type_text"),TEXT("enum_path"),TEXT("target_class_path"),TEXT("declaring_class_path")})if(!Plan->GetStringField(K).Equals(Baseline->GetStringField(K),ESearchCase::CaseSensitive))return Stop(TEXT("Captured native identity/type changed."));
            if(!Text(Plan->GetObjectField(TEXT("native_source_before")).ToSharedRef()).Equals(Text(Baseline->GetObjectField(TEXT("native_source_before")).ToSharedRef()),ESearchCase::CaseSensitive))return Stop(TEXT("Native enum/property/source metadata changed."));
        }
        TSharedPtr<FJsonObject> Row;int32 Count=0;for(const auto& V:Plan->GetArrayField(TEXT("native_texture_member_nodes")))if(V->AsObject()->GetStringField(TEXT("object_path")).Equals(NodePath,ESearchCase::CaseSensitive)){Row=V->AsObject();++Count;}
        if(Count!=1||!Row.IsValid())return Stop(TEXT("Actual held setter has no unique current role."));
        if(BeforeRow.IsValid())
        {
            if(AllowDefault){TSharedPtr<FJsonObject> A,B;if(!Normalize(BeforeRow.ToSharedRef(),A)||!Normalize(Row.ToSharedRef(),B)||!Text(A.ToSharedRef()).Equals(Text(B.ToSharedRef()),ESearchCase::CaseSensitive))return Stop(TEXT("Non-default setter role/pin/link/identity changed."));}
            else if(!Text(BeforeRow.ToSharedRef()).Equals(Text(Row.ToSharedRef()),ESearchCase::CaseSensitive))return Stop(TEXT("Setter changed before the selected default writer."));
        }
        if(!Baseline.IsValid())Baseline=Plan;LastRow=Row;return true;
    };
    if(!Current())return R;
    TSharedPtr<FJsonObject> ValueRow;for(const auto& V:LastRow->GetArrayField(TEXT("pins"))){auto P=V->AsObject();if(P->GetStringField(TEXT("name")).Equals(Variable.ToString(),ESearchCase::CaseSensitive)&&P->GetStringField(TEXT("direction"))==TEXT("input")){if(ValueRow.IsValid()){Fail(R,TEXT("pin_default_ambiguous"),TEXT("Native input is ambiguous."));return R;}ValueRow=P;}}
    if(!ValueRow.IsValid()||!ValueRow->GetArrayField(TEXT("links")).IsEmpty()||!FGuid::ParseExact(ValueRow->GetStringField(TEXT("id")),EGuidFormats::Digits,ValueId)){Fail(R,TEXT("pin_default_input_unsupported"),TEXT("The exact native enum value input must be unlinked and have a persistent ID."));return R;}BeforeRow=LastRow;
    auto Acquire=[&]()->UEdGraphPin*{UEdGraphPin* P=nullptr;for(auto* Q:Node->Pins)if(Q&&Q->PinId==ValueId){if(P)return nullptr;P=Q;}return P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==Node&&P->PinName==Variable&&P->Direction==EGPD_Input&&!P->bHidden&&!P->bOrphanedPin&&!P->bDeprecated&&!P->bDefaultValueIsIgnored&&!P->bDefaultValueIsReadOnly&&!P->bNotConnectable&&!P->ParentPin&&P->SubPins.IsEmpty()&&P->LinkedTo.IsEmpty()?P:nullptr;};
    TSharedPtr<FJsonValue> Requested;FString Token;if(!Pin::ParseJson(ValueJson,Requested,R))return R;
    if(!Requested.IsValid()||Requested->Type!=EJson::String||!Pin::SafeName(Requested->AsString())){Fail(R,TEXT("pin_default_texture_enum_invalid"),TEXT("Provide one exact canonical enum entry name; numeric/null/object defaults remain unsupported."));return R;}Token=Requested->AsString();
    int32 Allowed=0;for(const auto& V:Baseline->GetObjectField(TEXT("native_source_before"))->GetArrayField(TEXT("enum_entries"))){auto E=V->AsObject();if(E->GetStringField(TEXT("name")).Equals(Token,ESearchCase::CaseSensitive)&&!E->GetBoolField(TEXT("hidden"))&&!E->GetBoolField(TEXT("sentinel"))){const int64 N=FCString::Atoi64(*E->GetStringField(TEXT("value_string")));if(N>=0&&N<=255)++Allowed;}}
    if(Allowed!=1){Fail(R,TEXT("pin_default_texture_enum_invalid"),TEXT("Token must exactly name one defined visible non-MAX entry of this property enum."));return R;}
    auto PinJson=[&](const TSharedRef<FJsonObject>& P)->TSharedRef<FJsonObject>
    {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->GetStringField(TEXT("name")));J->SetStringField(TEXT("pin_id"),P->GetStringField(TEXT("id")));J->SetStringField(TEXT("direction"),P->GetStringField(TEXT("direction")));auto T=P->GetObjectField(TEXT("type"));auto PT=MakeShared<FJsonObject>();PT->SetStringField(TEXT("category"),T->GetStringField(TEXT("category")));PT->SetStringField(TEXT("subcategory"),T->GetStringField(TEXT("subcategory")));PT->SetStringField(TEXT("subcategory_object"),T->GetStringField(TEXT("type_object")));PT->SetBoolField(TEXT("is_array"),T->GetIntegerField(TEXT("container_type"))==1);PT->SetBoolField(TEXT("is_reference"),T->GetBoolField(TEXT("is_reference")));J->SetObjectField(TEXT("pin_type"),PT);J->SetStringField(TEXT("pin_type_text"),P->GetStringField(TEXT("type_text")));J->SetStringField(TEXT("default_value"),P->GetStringField(TEXT("default")));J->SetStringField(TEXT("default_text"),P->GetStringField(TEXT("default_text")));J->SetStringField(TEXT("default_object"),P->GetStringField(TEXT("default_object")));const uint32 F=uint32(P->GetIntegerField(TEXT("flags")));J->SetBoolField(TEXT("hidden"),(F&1)!=0);J->SetBoolField(TEXT("default_ignored"),(F&8)!=0);J->SetArrayField(TEXT("linked_to"),P->GetArrayField(TEXT("links")));return J;};
    auto NodeJson=[&](const TSharedRef<FJsonObject>& Row)->TSharedRef<FJsonObject>{auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),NodePath);J->SetStringField(TEXT("node_id"),Row->GetStringField(TEXT("node_guid")));J->SetStringField(TEXT("node_class"),Row->GetStringField(TEXT("node_class_path")));J->SetNumberField(TEXT("x"),Row->GetNumberField(TEXT("x")));J->SetNumberField(TEXT("y"),Row->GetNumberField(TEXT("y")));TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& V:Row->GetArrayField(TEXT("pins")))Pins.Add(MakeShared<FJsonValueObject>(PinJson(V->AsObject().ToSharedRef())));J->SetArrayField(TEXT("pins"),Pins);return J;};
    const bool DirtyBefore=BP->GetPackage()->IsDirty();auto Before=NodeJson(BeforeRow.ToSharedRef());R.Data->SetObjectField(TEXT("before"),Before);R.Data->SetObjectField(TEXT("previous"),PinJson(ValueRow.ToSharedRef()));R.Data->SetField(TEXT("requested_value"),Requested);R.Data->SetStringField(TEXT("requested_default_value"),Token);R.Data->SetBoolField(TEXT("dirty_before"),DirtyBefore);R.Data->SetStringField(TEXT("object_path"),NodePath);R.Data->SetStringField(TEXT("node_id"),Guid.ToString(EGuidFormats::Digits));auto GI=MakeShared<FJsonObject>();GI->SetStringField(TEXT("path"),GraphPath);GI->SetStringField(TEXT("name"),G->GetName());GI->SetStringField(TEXT("guid"),G->GraphGuid.ToString(EGuidFormats::Digits));R.Data->SetObjectField(TEXT("graph"),GI);
    if(!Current())return R;auto* Selected=Acquire();if(!Selected){Stop(TEXT("Current selected input unavailable."));return R;}
    FString Canonical;TObjectPtr<UObject> Object;FText Localized;Schema->GetPinDefaultValuesFromString(Selected->PinType,Node,Token,Canonical,Object,Localized,false);R.Data->SetBoolField(TEXT("native_texture_canonical_parse_returned"),true);if(!Current())return R;Selected=Acquire();if(!Selected){Stop(TEXT("Input changed after canonical parse."));return R;}
    if(!Object.IsResolved()||Object!=nullptr||!Localized.IsEmpty()){Fail(R,TEXT("pin_default_value_rejected"),TEXT("Native enum defaults must not resolve an object or localized text."));return R;}
    const FString Error=Schema->IsPinDefaultValid(Selected,Canonical,nullptr,Localized);if(!Current())return R;if(!Error.IsEmpty()){Fail(R,TEXT("pin_default_value_rejected"),TEXT("K2 schema rejected the native enum token."));return R;}
    Selected=Acquire();if(!Selected){Stop(TEXT("Input changed before comparison."));return R;}const bool WasMatching=Schema->DoesDefaultValueMatch(*Selected,Token);if(!Current())return R;
    if(!WasMatching)
    {
        R.Stage=TEXT("set_pin_default");R.bMutationAttempted=true;R.bChangeStateKnown=false;
        R.Data->SetNumberField(TEXT("native_texture_modify_attempted"),1);const bool B=BP->Modify();R.Data->SetBoolField(TEXT("native_texture_bp_modify_returned"),B);if(!Current())return R;
        R.Data->SetNumberField(TEXT("native_texture_modify_attempted"),2);const bool D=G->Modify();R.Data->SetBoolField(TEXT("native_texture_graph_modify_returned"),D);if(!Current())return R;
        R.Data->SetNumberField(TEXT("native_texture_modify_attempted"),3);const bool N=Node->Modify();R.Data->SetBoolField(TEXT("native_texture_node_modify_returned"),N);if(!Current())return R;
        Selected=Acquire();if(!Selected){Stop(TEXT("Input changed after Modify."));return R;}R.Data->SetBoolField(TEXT("native_texture_default_sdk_attempted"),true);AllowDefault=true;Schema->TrySetDefaultValue(*Selected,Token,true);R.Data->SetBoolField(TEXT("native_texture_default_sdk_returned"),true);if(!Current())return R;
    }
    Selected=Acquire();if(!Selected){Stop(TEXT("Input unavailable after default SDK."));return R;}const bool Matches=Schema->DoesDefaultValueMatch(*Selected,Token);if(!Current())return R;Selected=Acquire();if(!Selected){Stop(TEXT("Input unavailable after match callback."));return R;}const FString Validation=Schema->IsCurrentPinDefaultValid(Selected);if(!Current())return R;
    auto AfterIds=Graph::ReadNodeIdentities(GraphPath);R.Data->SetObjectField(TEXT("graph_identity_after"),AfterIds);if(!AfterIds->GetBoolField(TEXT("success"))){Stop(TEXT("Final graph identity observation failed; no further SDK."));return R;}if(!Current())return R;auto A=MakeShared<FJsonObject>(),B=MakeShared<FJsonObject>();A->SetArrayField(TEXT("nodes"),Identities->GetArrayField(TEXT("nodes")));B->SetArrayField(TEXT("nodes"),AfterIds->GetArrayField(TEXT("nodes")));const bool Ids=AfterIds->GetBoolField(TEXT("success"))&&Text(A).Equals(Text(B),ESearchCase::CaseSensitive)&&Identities->GetIntegerField(TEXT("total_node_count"))==AfterIds->GetIntegerField(TEXT("total_node_count"));if(!Ids){Stop(TEXT("Graph identities changed during enum default authoring."));return R;}
    auto After=NodeJson(LastRow.ToSharedRef());R.Data->SetObjectField(TEXT("after"),After);R.Data->SetBoolField(TEXT("dirty_after"),BP->GetPackage()->IsDirty());R.bChangeStateKnown=true;R.bChanged=!Text(Before).Equals(Text(After),ESearchCase::CaseSensitive)||BP->GetPackage()->IsDirty()!=DirtyBefore;
    for(const auto& V:LastRow->GetArrayField(TEXT("pins")))if(V->AsObject()->GetStringField(TEXT("id"))==ValueId.ToString(EGuidFormats::Digits))R.Data->SetObjectField(TEXT("pin"),PinJson(V->AsObject().ToSharedRef()));
    const bool Applied=Matches&&Validation.IsEmpty();R.Data->SetBoolField(TEXT("requested_value_applied"),Applied);R.Data->SetBoolField(TEXT("already_matching"),WasMatching);R.Data->SetBoolField(TEXT("other_node_pins_preserved"),true);R.Data->SetBoolField(TEXT("graph_identities_preserved"),Ids);R.Data->SetBoolField(TEXT("legacy_float_serialization_flag_cleared"),false);R.Data->SetBoolField(TEXT("native_texture_runtime_property_written"),false);R.Data->SetStringField(TEXT("change_observation_scope"),TEXT("exact native setter physical pins, current native property/source roles, graph identities and target dirty state; Python verifies the complete graph delta"));
    if(!Applied){Fail(R,TEXT("pin_default_postcondition_failed"),TEXT("Native enum default did not match; actual prefix retained without retry."));return R;}R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
// LV NativeTextureDefault_End
}
Function::FResult Set(const FString& GraphPath, const FString& NodePath, const FString& PinName,
    const FString& ValueJson, const FString& Direction)
{
    Function::FResult R;
    // First lookup cannot yet classify its node: block unsafe lookup contexts before dispatch.
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())
    {Fail(R,TEXT("pin_default_editor_state_invalid"),TEXT("Idle game-thread authoring outside import/GC/save/exit required before lookup."));return R;}
    if (!Pin::SafeName(GraphPath) || !Pin::SafeName(NodePath) || !Pin::SafeName(PinName)
        || (!Direction.IsEmpty() && !Direction.Equals(TEXT("input"), ESearchCase::IgnoreCase)))
    { Fail(R, TEXT("pin_default_selector_unsupported"), TEXT("Use exact bounded graph/node/pin selectors and input direction; output defaults remain pending.")); return R; }
    const auto Identities = Graph::ReadNodeIdentities(GraphPath); R.Data->SetObjectField(TEXT("graph_identity_before"), Identities);
    if (!Identities->GetBoolField(TEXT("success"))) { Fail(R, TEXT("pin_default_graph_invalid"), TEXT("Loaded graph failed the common editor identity guard.")); return R; }
    UEdGraph* G = FindObject<UEdGraph>(nullptr, *GraphPath); UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraph(G);
    UK2Node* Node = FindObject<UK2Node>(nullptr, *NodePath);
    FString TextureClass;if(IsValid(G)&&IsValid(BP)&&TextureSetterClass(Node,TextureClass))return SetTextureEnum(G,BP,Node,TextureClass,PinName,ValueJson,Identities);
    if (!IsValid(BP) || !IsValid(Node) || Node->GetPathName() != NodePath || Node->GetGraph() != G || !G->Nodes.Contains(Node)
        || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass() || BP->BlueprintType != BPTYPE_Normal
        || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bIsRegeneratingOnLoad || BP->bSuppressStructurallyModified)
    { Fail(R, TEXT("pin_default_scope_unsupported"), TEXT("Requires an exact loaded node on an idle normal Blueprint's standard K2 graph.")); return R; }
    // AAS: these six concrete authoring nodes inherit the empty
    // UEdGraphNode::PinDefaultValueChanged (EdGraphNode.h827). Installed
    // MakeContainer/StructMemberSet/StructMemberGet/StructOperation and their
    // concrete classes do not replace that callback. Do not admit subclasses:
    // custom callbacks, pin topology/type changes and node creation are separate.
    const UClass* NodeClass=Node->GetClass();
    const bool PassiveDefault=NodeClass==UK2Node_MakeArray::StaticClass()||NodeClass==UK2Node_MakeSet::StaticClass()
        ||NodeClass==UK2Node_MakeMap::StaticClass()||NodeClass==UK2Node_Select::StaticClass()
        ||NodeClass==UK2Node_MakeStruct::StaticClass()||NodeClass==UK2Node_BreakStruct::StaticClass();
    if (NodeClass != UK2Node_VariableSet::StaticClass() && NodeClass != UK2Node_CallFunction::StaticClass()
        && NodeClass != UK2Node_IfThenElse::StaticClass()&&!PassiveDefault)
    { Fail(R, TEXT("pin_default_node_pending"), TEXT("Exact standard VariableSet/CallFunction/Branch or passive MakeArray/MakeSet/MakeMap/Select/MakeStruct/BreakStruct required; other node callbacks remain pending.")); return R; }
    if(PassiveDefault)
    {
        R.Data->SetBoolField(TEXT("stock_passive_pin_default"),true);
        R.Data->SetStringField(TEXT("stock_passive_pin_default_scope"),TEXT("Existing typed unlinked visible input defaults on exact stock MakeArray/MakeSet/MakeMap/Select/MakeStruct/BreakStruct. Existing common primitive/null-object conversion and exact saved-clean runtime Vector/Rotator/Transform scalar converter, schema validation, idempotent matching, exact selected pin/readback and other-pin/whole-graph preservation apply. No node/type creation, topology or array resize, object/class loading or arbitrary struct source expansion. Stock BreakStruct whole const-reference inputs still require a connection under both original and current schema; their rejection is preserved, while eligible existing split scalar leaves use the common input path. Wider original malformed/short-array/coercion variants remain incomplete. Explicit compile/save0; actual per-node authoring and persistence usage validation pending."));
    }
    const auto Parent = InspectBlueprintParent(BP->ParentClass ? BP->ParentClass->GetPathName() : FString());
    R.Data->SetObjectField(TEXT("parent_inspection"), Parent);
    if (!Parent->GetBoolField(TEXT("success"))) { Fail(R, TEXT("pin_default_parent_unsupported"), TEXT("Blueprint parent failed the runtime-class guard.")); return R; }
    UEdGraphPin* Selected = nullptr;
    for (UEdGraphPin* P : Node->Pins)
    {
        if (P && P->PinName.ToString() == PinName && (Direction.IsEmpty() || P->Direction == EGPD_Input))
        { if (Selected) { Fail(R, TEXT("pin_default_ambiguous"), TEXT("Pin name is ambiguous; explicitly select input direction.")); return R; } Selected = P; }
    }
    if (!Selected || Selected->Direction != EGPD_Input || Selected->WasTrashed() || Selected->bOrphanedPin
        || Selected->bHidden || Selected->bDefaultValueIsIgnored || Selected->PinName == UEdGraphSchema_K2::PN_Self || !Selected->LinkedTo.IsEmpty())
    { Fail(R, TEXT("pin_default_input_unsupported"), TEXT("Requires one visible non-orphaned unlinked input whose default is used; self/hidden/ignored/connected inputs are not modified.")); return R; }
    TSharedPtr<FJsonValue> Requested; FString RequestedText;
    if (!Pin::ParseJson(ValueJson, Requested, R)) { return R; }
    // Reuse the established exact runtime value-struct converter. The original
    // CommandsShared.h530 names these Vector/Rotator/Transform value families.
    // Installed BreakStruct199 creates its whole input as const-reference. A
    // conversion-only copy may describe its value, but the actual pin flags stay
    // untouched and the schema still requires an incoming connection (UE58
    // Schema2991, same UE56 Schema3028). This is an intentional rejection, not
    // evidence that whole-struct BreakStruct default editing is implemented.
    FEdGraphPinType ConversionType=Selected->PinType;bool BreakValueView=false;
    if(PassiveDefault&&NodeClass==UK2Node_BreakStruct::StaticClass()&&ConversionType.bIsConst&&ConversionType.bIsReference)
    {
        auto* Break=static_cast<UK2Node_BreakStruct*>(Node);auto View=ConversionType;View.bIsConst=false;View.bIsReference=false;
        if(Pin::IsScalarValueStruct(View)&&Break->StructType.IsResolved()
            &&Break->StructType.Get()==View.PinSubCategoryObject.Get()&&Selected->PinName==Break->StructType->GetFName())
        {ConversionType=View;BreakValueView=true;}
    }
    const bool PassiveValueStruct=PassiveDefault&&Pin::IsScalarValueStruct(ConversionType);
    if(BreakValueView)
    {
        R.Data->SetBoolField(TEXT("break_struct_conversion_view_only"),true);
        R.Data->SetBoolField(TEXT("actual_const_reference_flags_preserved"),Selected->PinType.bIsConst&&Selected->PinType.bIsReference);
    }
    if(PassiveValueStruct)
    {
        if(!Pin::ScalarValueDefaultText(ConversionType,Requested,RequestedText,R))return R;
        R.Data->SetObjectField(TEXT("passive_value_struct_source_before"),Pin::ScalarValueStructSource(Selected->PinType));
    }
    else if(!Pin::DefaultText(Selected->PinType,Requested,RequestedText,R))return R;
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    FString CanonicalText; TObjectPtr<UObject> CanonicalObject = nullptr; FText CanonicalLocalized;
    Schema->GetPinDefaultValuesFromString(Selected->PinType, Node, RequestedText, CanonicalText, CanonicalObject, CanonicalLocalized, false);
    const FString SchemaError=CanonicalObject?FString():Schema->IsPinDefaultValid(Selected,CanonicalText,CanonicalObject,CanonicalLocalized);
    if (CanonicalObject || !SchemaError.IsEmpty())
    {
        R.Data->SetStringField(TEXT("schema_default_validation_error"),SchemaError);
        R.Data->SetBoolField(TEXT("stock_break_struct_const_reference_rejected"),BreakValueView&&!SchemaError.IsEmpty());
        Fail(R, TEXT("pin_default_value_rejected"), TEXT("The public schema rejected this typed default before modification; concrete object literals are not loaded. A stock BreakStruct whole input retains its SDK const-reference connection requirement.")); return R;
    }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP); const TStrongObjectPtr<UK2Node> KeepNode(Node);
    const TStrongObjectPtr<UEdGraph> KeepGraph(G);
    const TStrongObjectPtr<UObject> KeepValueStruct(PassiveValueStruct?Selected->PinType.PinSubCategoryObject.Get():nullptr);
    auto Before = MakeShared<FJsonObject>(); if (!CaptureNode(Node, Before))
    { Fail(R, TEXT("pin_default_observation_incomplete"), TEXT("Node pins could not be captured within the 4096-pin bound.")); return R; }
    const bool DirtyBefore = BP->GetOutermost()->IsDirty(); const FGuid SelectedId = Selected->PinId;
    const bool LegacyFloatBefore = Selected->PinType.bSerializeAsSinglePrecisionFloat;
    FEdGraphPinType ExpectedType = Selected->PinType;
    const FGuid GraphGuid=G->GraphGuid,NodeGuid=Node->NodeGuid;
    auto PassiveCurrent=[&]()
    {
        if(!PassiveDefault)return true;
        UEdGraphPin* Found=nullptr;int32 Matches=0;
        const bool Context=IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge
            &&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();
        const bool Owner=Context&&IsValid(BP)&&IsValid(G)&&IsValid(Node)&&Node->GetClass()==NodeClass
            &&Node->GetPathName()==NodePath&&G->GetPathName()==GraphPath&&Node->GetOuter()==G
            &&G->GetOuter()==BP&&G->Nodes.Contains(Node)&&G->GraphGuid==GraphGuid&&Node->NodeGuid==NodeGuid
            &&!BP->bBeingCompiled&&!BP->bQueuedForCompilation&&!BP->bIsRegeneratingOnLoad&&!BP->bSuppressStructurallyModified;
        if(Owner)for(auto* P:Node->Pins)if(P&&P->PinId==SelectedId){Found=P;++Matches;}
        if(Owner&&Matches==1&&Found==Selected&&!Found->WasTrashed()&&!Found->bOrphanedPin
            &&Found->GetOwningNodeUnchecked()==Node&&Found->PinName.ToString()==PinName&&Found->Direction==EGPD_Input
            &&Found->PinType==ExpectedType&&!Found->bHidden&&!Found->bDefaultValueIsIgnored&&Found->LinkedTo.IsEmpty())
        {
            if(!PassiveValueStruct)return true;
            const auto Source=Pin::ScalarValueStructSource(Found->PinType);R.Data->SetObjectField(TEXT("passive_value_struct_source_current"),Source);
            if(Found->PinType.PinSubCategoryObject.Get()==KeepValueStruct.Get()&&Pin::IsScalarValueStruct(Found->PinType)
                &&Text(Source)==Text(R.Data->GetObjectField(TEXT("passive_value_struct_source_before")).ToSharedRef()))return true;
        }
        R.bChangeStateKnown=false;
        R.Data->SetStringField(TEXT("first_mismatch"),Owner?TEXT("selected_passive_input_identity_type_or_role"):TEXT("passive_node_graph_or_editor_owner"));
        R.Data->SetStringField(TEXT("expected"),TEXT("same current stock node/graph and unlinked typed input at the next SDK boundary"));
        R.Data->SetStringField(TEXT("actual"),Owner?FString::Printf(TEXT("matching_pin_ids=%d; same_pointer=%s"),Matches,Found==Selected?TEXT("true"):TEXT("false")):TEXT("owner/context changed"));
        return Fail(R,TEXT("pin_default_passive_current_lost"),TEXT("Current passive input/owner changed at an engine boundary; actual prefix retained without following callback, cleanup, save or retry."));
    };
    if(!PassiveCurrent())return R;
    R.Data->SetObjectField(TEXT("before"), Before); R.Data->SetObjectField(TEXT("previous"), ObservePin(Selected));
    R.Data->SetField(TEXT("requested_value"), Requested); R.Data->SetStringField(TEXT("requested_default_value"), RequestedText);
    R.Data->SetBoolField(TEXT("dirty_before"), DirtyBefore); R.Data->SetStringField(TEXT("object_path"), NodePath);
    R.Data->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    auto GraphInfo = MakeShared<FJsonObject>(); GraphInfo->SetStringField(TEXT("path"), GraphPath); GraphInfo->SetStringField(TEXT("name"), G->GetName());
    GraphInfo->SetStringField(TEXT("guid"), G->GraphGuid.ToString(EGuidFormats::Digits)); R.Data->SetObjectField(TEXT("graph"), GraphInfo);
    const bool WasMatching = Schema->DoesDefaultValueMatch(*Selected, RequestedText) && Schema->IsCurrentPinDefaultValid(Selected).IsEmpty();
    if (!WasMatching)
    {
        R.Stage = TEXT("set_pin_default"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
        BP->Modify();if(!PassiveCurrent())return R;
        G->Modify();if(!PassiveCurrent())return R;
        Node->Modify();if(!PassiveCurrent())return R;
        // This API already emits PinDefaultValueChanged exactly once. Do not
        // repeat the legacy explicit callback or attempt an incomplete rollback.
        Schema->TrySetDefaultValue(*Selected, RequestedText, true);
        if (ExpectedType.PinCategory == UEdGraphSchema_K2::PC_Real) { ExpectedType.bSerializeAsSinglePrecisionFloat = false; }
        if(!PassiveCurrent())return R;
    }
    auto After = MakeShared<FJsonObject>(); const auto AfterIds = Graph::ReadNodeIdentities(GraphPath);
    R.Data->SetObjectField(TEXT("after"), After); R.Data->SetObjectField(TEXT("graph_identity_after"), AfterIds);
    const bool Known = CaptureNode(Node, After) && AfterIds->GetBoolField(TEXT("success"));
    R.bChangeStateKnown = Known; R.bChanged = Known && (Text(Before) != Text(After) || BP->GetOutermost()->IsDirty() != DirtyBefore);
    R.Data->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty());
    UEdGraphPin* Actual = nullptr; for (UEdGraphPin* P : Node->Pins) { if (P && P->PinId == SelectedId) { Actual = P; break; } }
    auto BeforeIdentityRows = MakeShared<FJsonObject>(); auto AfterIdentityRows = MakeShared<FJsonObject>();
    BeforeIdentityRows->SetArrayField(TEXT("nodes"), Identities->GetArrayField(TEXT("nodes")));
    if (Known) { AfterIdentityRows->SetArrayField(TEXT("nodes"), AfterIds->GetArrayField(TEXT("nodes"))); }
    const bool IdentityPreserved = Known && Node->GetGraph() == G && G->Nodes.Contains(Node)
        && Node->GetPathName() == NodePath && Before->GetStringField(TEXT("node_id")) == Node->NodeGuid.ToString(EGuidFormats::Digits)
        && Before->GetIntegerField(TEXT("x")) == Node->NodePosX && Before->GetIntegerField(TEXT("y")) == Node->NodePosY
        && Text(BeforeIdentityRows) == Text(AfterIdentityRows)
        && Identities->GetIntegerField(TEXT("total_node_count")) == AfterIds->GetIntegerField(TEXT("total_node_count"));
    R.bChanged |= Known && !IdentityPreserved;
    const bool Applied = IdentityPreserved && Actual && Actual->PinName.ToString() == PinName && Actual->Direction == EGPD_Input
        && Actual->PinType == ExpectedType && Actual->LinkedTo.IsEmpty()
        && Schema->DoesDefaultValueMatch(*Actual, RequestedText) && Schema->IsCurrentPinDefaultValid(Actual).IsEmpty();
    if (Actual && Known) { R.Data->SetObjectField(TEXT("pin"), ObservePin(Actual)); }
    bool OtherPinsPreserved = Known && Before->GetArrayField(TEXT("pins")).Num() == After->GetArrayField(TEXT("pins")).Num();
    if (OtherPinsPreserved)
    {
        for (const auto& Value : Before->GetArrayField(TEXT("pins")))
        {
            const auto Old = Value->AsObject(); if (Old->GetStringField(TEXT("pin_id")) == SelectedId.ToString(EGuidFormats::Digits)) { continue; }
            const UEdGraphPin* Current = nullptr;
            for (const UEdGraphPin* P : Node->Pins) { if (P && P->PinId.ToString(EGuidFormats::Digits) == Old->GetStringField(TEXT("pin_id"))) { Current = P; break; } }
            OtherPinsPreserved &= Current && Text(ObservePin(Current)) == Text(Old.ToSharedRef());
        }
    }
    R.Data->SetBoolField(TEXT("other_node_pins_preserved"), OtherPinsPreserved);
    R.Data->SetBoolField(TEXT("graph_identities_preserved"), IdentityPreserved);
    R.Data->SetBoolField(TEXT("legacy_float_serialization_flag_cleared"), Known && Actual && LegacyFloatBefore
        && Actual->PinType.PinCategory == UEdGraphSchema_K2::PC_Real && !Actual->PinType.bSerializeAsSinglePrecisionFloat);
    R.Data->SetBoolField(TEXT("requested_value_applied"), Applied); R.Data->SetBoolField(TEXT("already_matching"), WasMatching);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("selected node pins and target dirty state; Python independently verifies complete graph delta"));
    if (!Applied || !OtherPinsPreserved)
    { R.Stage = TEXT("verification"); Fail(R, TEXT("pin_default_postcondition_failed"), TEXT("Requested default/readback or other node pins differ; actual partial observations retained, no rollback or retry performed.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
