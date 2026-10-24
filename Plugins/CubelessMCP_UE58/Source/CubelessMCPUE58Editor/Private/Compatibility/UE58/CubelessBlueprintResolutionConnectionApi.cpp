// HM: legacy Blueprint resolution and general K2 node connection. Editor-only.
#include "Compatibility/UE58/CubelessBlueprintResolutionConnectionApi.h"
#include "Compatibility/UE58/CubelessBlueprintTopologyApi.h"
#include "Compatibility/UE58/CubelessConnectionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EditorAssetLibrary.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Engine/MemberReference.h"
#include "Internationalization/Text.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::BlueprintResolutionConnection
{
namespace
{
using Variable::FResult;
bool Running=false;bool GraphPinsRunning=false;
bool GlobalSafe(){return IsInGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool TextSafe(const FString& S,int32 Limit){if(S.Len()>Limit)return false;for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
FString Json(const TSharedPtr<FJsonObject>& J){FString S;if(J)FJsonSerializer::Serialize(J.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
bool Finite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Slots)
{
    if(!V||Depth>64||++Slots>262144)return false;
    if(V->Type==EJson::Number)return FMath::IsFinite(V->AsNumber());
    if(V->Type==EJson::String)return TextSafe(V->AsString(),1048576);
    if(V->Type==EJson::Array){for(const auto& Entry:V->AsArray())if(!Finite(Entry,Depth+1,Slots))return false;}
    else if(V->Type==EJson::Object){if(!V->AsObject())return false;for(const auto& Entry:V->AsObject()->Values)if(!TextSafe(FString(Entry.Key),1048576)||!Finite(Entry.Value,Depth+1,Slots))return false;}
    return V->Type!=EJson::None;
}
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||!TextSafe(S,1048576))return false;
    int32 Depth=0;bool Quoted=false,Escaped=false;for(TCHAR C:S){if(Quoted){if(Escaped)Escaped=false;else if(C==TEXT('\\'))Escaped=true;else if(C==TEXT('"'))Quoted=false;}else if(C==TEXT('"'))Quoted=true;else if(C==TEXT('{')||C==TEXT('[')){if(++Depth>64)return false;}else if(C==TEXT('}')||C==TEXT(']')){if(--Depth<0)return false;}}
    if(Depth||Quoted||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)||!Out)return false;int32 Slots=0;return Finite(MakeShared<FJsonValueObject>(Out),0,Slots);
}
bool ForbiddenClassPath(const UObject* O)
{
    if(!O)return false;const FString P=O->GetPathName();for(const TCHAR* Token:{TEXT("CubelessMCP"),TEXT("cubeless_mcp"),TEXT("CubelessControlRigTools"),TEXT("ModelContextProtocol"),TEXT("ToolsetRegistry"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("UnrealMCP"),TEXT("PythonScriptPlugin"),TEXT("EnginePythonTypes"),TEXT("/Engine/Transient")})if(P.Contains(Token,ESearchCase::IgnoreCase))return true;return false;
}
struct FObjectIdentity
{
    TStrongObjectPtr<UObject> Hold;TWeakObjectPtr<UObject> Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Outermost,Package;FString Path;bool Retired=false;
    explicit FObjectIdentity(UObject* O):Hold(O),Outer(O->GetOuter()),Class(O->GetClass()),Outermost(O->GetOutermost()),Package(O->GetPackage()),Path(O->GetPathName()){}
    bool Exact()const{auto* O=Hold.Get();return Retired||(Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Outermost.Get()&&O->GetPackage()==Package.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive));}
};
// Pin objects are not UObjects. All SDK boundaries use value snapshots and fresh live membership.
struct FPinValue
{
    UEdGraphPin* Address=nullptr;TWeakObjectPtr<UEdGraphNode> Owner;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;
    FString Default,AutoDefault,Text;TWeakObjectPtr<UObject> DefaultObject;bool Orphan=false,NotConnectable=false,ReadOnly=false,Ignored=false;TArray<UEdGraphPin*> Links;
    TSharedPtr<FJsonObject> Legacy;
};
struct FNodeValue
{
    TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<FPinValue> Pins;FString Header,Plain;
};
struct FGraphValue
{
    TWeakObjectPtr<UEdGraph> Graph;FGuid Id;TWeakObjectPtr<UClass> SchemaClass;TArray<FNodeValue> Nodes;TArray<UEdGraph*> SubGraphs;FString Plain;
};
// QF concrete GraphPins lease. Original Connect writer is reused; this is not a new graph writer.
struct FGraphPinsLease
{
    struct FPin {UEdGraphPin* Address=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FText Text,Friendly;FString Raw,Auto,Display,FriendlyDisplay;UObject* Object=nullptr;TArray<UEdGraphPin*> Links,Children;UEdGraphPin* Parent=nullptr;uint32 Flags=0;};
    struct FNode {TStrongObjectPtr<UEdGraphNode> Hold;FGuid Id;FString Path,Comment;int32 X=0,Y=0;UClass* Class=nullptr;FMemberReference Ref;bool Call=false;TArray<FPin> Pins;};
    struct FGraph {TStrongObjectPtr<UEdGraph> Hold;FGuid Id;TArray<TObjectPtr<UEdGraph>> Sub;TArray<FNode> Nodes;};
    TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<UClass> Gen,Skel;TStrongObjectPtr<UObject> CDO;
    TArray<TStrongObjectPtr<UObject>> Holds,Retired;TArray<FGraph> Rows,Candidate;TArray<TArray<TObjectPtr<UEdGraph>>> Roles;TArray<UEdGraph*> All;FString BPPath,GenPath,SkelPath,CDOPath;
    TSharedPtr<FJsonObject> First;bool Stopped=false,Advancing=false,CompilerReturn=false,CandidateReady=false,Initialized=false,LinkPlanned=false,LinkDone=false,ExecRoute=false;
    UEdGraph* Selected=nullptr;FGuid SourceNodeId,TargetNodeId,SourceId,TargetId,OldNodeId,OldId;ECanCreateConnectionResponse Response=CONNECT_RESPONSE_MAKE;FString PlannedRaw,PlannedText;UObject* PlannedObject=nullptr;uint32 PlannedTextFlags=0;bool PlannedDefault=false,OldPlannedDefault=false;FString OldPlannedRaw,OldPlannedText;UObject* OldPlannedObject=nullptr;uint32 OldPlannedTextFlags=0;
    TMap<FGuid,TArray<FGuid>> PlannedLinks;TMap<FGuid,FGuid> PinOwners;
    bool Bad(const TCHAR* Field,const FString& Expected,const FString& Actual)
    {if(!Stopped){First=MakeShared<FJsonObject>();First->SetStringField(TEXT("property"),Field);First->SetStringField(TEXT("expected"),Expected);First->SetStringField(TEXT("actual"),Actual);First->SetStringField(TEXT("object_captured_path"),BPPath);First->SetBoolField(TEXT("later_conditions_unobserved"),true);}Stopped=true;return false;}
    static uint32 Flags(const UEdGraphPin* P){return uint32(P->bHidden)|(uint32(P->bOrphanedPin)<<1)|(uint32(P->bNotConnectable)<<2)|(uint32(P->bDefaultValueIsReadOnly)<<3)|(uint32(P->bDefaultValueIsIgnored)<<4)|(uint32(P->bAdvancedView)<<5)|(uint32(P->bDeprecated)<<6)|(uint32(P->bDisplayAsMutableRef)<<7)|(uint32(P->bAllowFriendlyName)<<8);}
    static bool Type(const FEdGraphPinType& A,const FEdGraphPinType& B){return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    bool Context()
    {if(Stopped)return false;if(!GlobalSafe()||!Live(BP.Get())||!Live(Editor.Get())||!Live(World.Get())||GEditor!=Editor.Get()||GEditor->GetEditorWorldContext().World()!=World.Get()||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||World->bIsTearingDown)return Bad(TEXT("editor_context"),TEXT("same idle live Editor/world/BP"),TEXT("changed"));return true;}
    TArray<TArray<TObjectPtr<UEdGraph>>> Categories()const
    {return {BP->FunctionGraphs,BP->MacroGraphs,BP->UbergraphPages,BP->DelegateSignatureGraphs};}
    bool Classes(bool Rebind)
    {
        if(!Context()||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("class_slot"),TEXT("resolved generated/skeleton"),TEXT("unavailable"));
        TStrongObjectPtr<UClass> G(BP->GeneratedClass.Get()),K(BP->SkeletonGeneratedClass.Get());
        for(UClass* C:{G.Get(),K.Get()})if(!Live(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=BP.Get()||C->GetOuter()!=BP->GetPackage()||C->GetPackage()!=BP->GetPackage()||C->GetSuperClass()!=AActor::StaticClass())return Bad(TEXT("generated_role"),TEXT("same own direct Actor generated classes"),TEXT("different"));
        if(!G->GetPathName().Equals(GenPath,ESearchCase::CaseSensitive)||!K->GetPathName().Equals(SkelPath,ESearchCase::CaseSensitive)||!G->ClassDefaultObject.IsResolved())return Bad(TEXT("class_path"),TEXT("captured generated/skeleton paths and resolved CDO"),TEXT("different"));
        TStrongObjectPtr<UObject> D(G->ClassDefaultObject.Get());
        if(D.IsValid()&&(!Live(D.Get())||D->GetClass()!=G.Get()||D->GetOuter()!=BP->GetPackage()||D->GetPackage()!=BP->GetPackage()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!D->GetPathName().Equals(CDOPath,ESearchCase::CaseSensitive)))return Bad(TEXT("CDO_role"),CDOPath,TEXT("different"));
        if(!Rebind)return (G.Get()==Gen.Get()&&K.Get()==Skel.Get()&&D.Get()==CDO.Get())||Bad(TEXT("class_identity"),TEXT("held current slots"),TEXT("changed"));
        if(Gen.IsValid())Retired.Emplace(Gen.Get());if(Skel.IsValid())Retired.Emplace(Skel.Get());if(CDO.IsValid())Retired.Emplace(CDO.Get());Gen=MoveTemp(G);Skel=MoveTemp(K);CDO=MoveTemp(D);return true;
    }
    bool Capture(TArray<FGraph>& Out)
    {
        if(!Context())return false;Out.Reset();int32 NodeCount=0,PinCount=0;TSet<UEdGraphPin*> Pins;TSet<FGuid> Ids;
        for(auto* G:All){if(!Live(G)||G->GetOuter()!=BP.Get()||G->GetClass()!=UEdGraph::StaticClass()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||!G->SubGraphs.IsEmpty())return Bad(TEXT("graph_role"),TEXT("own exact flat K2 graphs"),TEXT("different"));FGraph R;R.Hold.Reset(G);R.Id=G->GraphGuid;R.Sub=G->SubGraphs;
            for(const auto& Slot:G->Nodes){if(!Slot.IsResolved())return Bad(TEXT("node_slot"),TEXT("resolved"),TEXT("unresolved"));auto* N=Slot.Get();if(!Live(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid()||++NodeCount>2048)return Bad(TEXT("node_role"),TEXT("bounded current graph node"),TEXT("different"));FNode V;V.Hold.Reset(N);V.Id=N->NodeGuid;V.Path=N->GetPathName();V.Class=N->GetClass();V.Comment=N->NodeComment;V.X=N->NodePosX;V.Y=N->NodePosY;if(auto* Call=Cast<UK2Node_CallFunction>(N)){V.Call=true;V.Ref=Call->FunctionReference;}
                for(auto* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||Pins.Contains(P)||++PinCount>16384||!P->DefaultObject.IsResolved()||P->PinType.PinSubCategoryObject.IsStale()||P->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(TEXT("pin_membership"),TEXT("bounded resolved unique current pins"),TEXT("different"));Ids.Add(P->PinId);Pins.Add(P);FPin Q;Q.Address=P;Q.Id=P->PinId;Q.Persistent=P->PersistentGuid;Q.Name=P->PinName;Q.Direction=P->Direction;Q.Type=P->PinType;Q.Text=P->DefaultTextValue;Q.Friendly=P->PinFriendlyName;Q.Raw=P->DefaultValue;Q.Auto=P->AutogeneratedDefaultValue;Q.Object=P->DefaultObject.Get();Q.Links=P->LinkedTo;Q.Children=P->SubPins;Q.Parent=P->ParentPin;Q.Flags=Flags(P);for(UObject* O:{Q.Object,Q.Type.PinSubCategoryObject.Get(),Q.Type.PinValueType.TerminalSubCategoryObject.Get()})if(O){if(!Live(O))return Bad(TEXT("pin_object"),TEXT("loaded"),TEXT("unavailable"));Holds.Emplace(O);}V.Pins.Add(MoveTemp(Q));}R.Nodes.Add(MoveTemp(V));}Out.Add(MoveTemp(R));}
        for(const auto& G:Out)for(const auto& N:G.Nodes)for(const auto& P:N.Pins){for(auto* L:P.Links)if(!Pins.Contains(L)||!L->LinkedTo.Contains(P.Address))return Bad(TEXT("reciprocal_link"),TEXT("current same-inventory reciprocal"),TEXT("different"));if(P.Parent&&(!Pins.Contains(P.Parent)||!P.Parent->SubPins.Contains(P.Address)))return Bad(TEXT("split_parent"),TEXT("current reciprocal parent"),TEXT("different"));for(auto* C:P.Children)if(!Pins.Contains(C)||C->ParentPin!=P.Address)return Bad(TEXT("split_child"),TEXT("current reciprocal child"),TEXT("different"));}return true;
    }
    bool Pure(const TArray<FGraph>& Set)
    {
        for(const auto& G:Set){if(!Live(G.Hold.Get())||G.Hold->Nodes.Num()!=G.Nodes.Num()||G.Hold->GraphGuid!=G.Id||G.Hold->SubGraphs!=G.Sub||!G.Hold->Schema.GetGCPtr().IsResolved()||G.Hold->Schema.Get()!=UEdGraphSchema_K2::StaticClass())return Bad(TEXT("graph_current"),TEXT("captured graph slots"),TEXT("changed"));for(int32 I=0;I<G.Nodes.Num();++I){const auto& N=G.Nodes[I];if(!G.Hold->Nodes[I].IsResolved()||G.Hold->Nodes[I].Get()!=N.Hold.Get()||!Live(N.Hold.Get())||N.Hold->GetClass()!=N.Class||N.Hold->GetOuter()!=G.Hold.Get()||N.Hold->NodeGuid!=N.Id||!N.Hold->GetPathName().Equals(N.Path,ESearchCase::CaseSensitive)||N.Hold->NodePosX!=N.X||N.Hold->NodePosY!=N.Y||!N.Hold->NodeComment.Equals(N.Comment,ESearchCase::CaseSensitive)||N.Hold->Pins.Num()!=N.Pins.Num())return Bad(TEXT("node_current"),N.Id.ToString(),TEXT("changed"));if(N.Call&&!static_cast<UK2Node_CallFunction*>(N.Hold.Get())->FunctionReference.IsSameReference(N.Ref))return Bad(TEXT("function_reference"),N.Id.ToString(),TEXT("changed"));for(int32 J=0;J<N.Pins.Num();++J)if(N.Hold->Pins[J]!=N.Pins[J].Address)return Bad(TEXT("pin_slot_current"),N.Id.ToString(),TEXT("changed"));}
            for(const auto& N:G.Nodes)for(const auto& Q:N.Pins){const auto* P=Q.Address;if(P->WasTrashed()||P->PinId!=Q.Id||P->PersistentGuid!=Q.Persistent||P->PinName!=Q.Name||P->Direction!=Q.Direction||!Type(P->PinType,Q.Type)||!P->DefaultObject.IsResolved()||P->DefaultObject.Get()!=Q.Object||!P->DefaultValue.Equals(Q.Raw,ESearchCase::CaseSensitive)||!P->AutogeneratedDefaultValue.Equals(Q.Auto,ESearchCase::CaseSensitive)||FTextInspector::GetSharedDataId(P->DefaultTextValue)!=FTextInspector::GetSharedDataId(Q.Text)||FTextInspector::GetFlags(P->DefaultTextValue)!=FTextInspector::GetFlags(Q.Text)||FTextInspector::GetSharedDataId(P->PinFriendlyName)!=FTextInspector::GetSharedDataId(Q.Friendly)||FTextInspector::GetFlags(P->PinFriendlyName)!=FTextInspector::GetFlags(Q.Friendly)||Flags(P)!=Q.Flags||P->LinkedTo!=Q.Links||P->ParentPin!=Q.Parent||P->SubPins!=Q.Children)return Bad(TEXT("pin_current"),Q.Id.ToString(),TEXT("changed"));}}return true;
    }
    bool Texts(TArray<FGraph>& Set)
    {for(auto& G:Set)for(auto& N:G.Nodes)for(auto& Q:N.Pins){Q.Display=Q.Text.ToString();if(!Current())return false;Q.FriendlyDisplay=Q.Friendly.ToString();if(!Current())return false;}return true;}
    static FGuid PinId(UEdGraphPin* Address,const TArray<FGraph>& Set)
    {for(const auto& G:Set)for(const auto& N:G.Nodes)for(const auto& P:N.Pins)if(P.Address==Address)return P.Id;return FGuid();}
    static TArray<FGuid> ChildIds(const FPin& P,const TArray<FGraph>& Set)
    {TArray<FGuid> R;for(auto* C:P.Children)R.Add(PinId(C,Set));return R;}
    static TArray<FGuid> LinkIds(const FPin& P,const TArray<FGraph>& Set)
    {TArray<FGuid> R;for(auto* L:P.Links)for(const auto& G:Set)for(const auto& N:G.Nodes)for(const auto& V:N.Pins)if(V.Address==L)R.Add(V.Id);return R;}
    // YP: reuse the original concrete writer for an explicit-permission Engine
    // primitive autocast. This lease adds only its exact one-node/two-leg plan.
    bool ConversionRoute=false,ConversionReady=false;int32 ConversionSearchCalls=0,ConversionTypeCalls=0;
    TStrongObjectPtr<UClass> ConversionOwner;TStrongObjectPtr<UFunction> ConversionFunction;
    FMemberReference ConversionRef;EFunctionFlags ConversionFunctionFlags=FUNC_None;FString ConversionOwnerPath,ConversionFunctionPath;
    FEdGraphPinType ConversionSourceType,ConversionTargetType;FGuid ConversionNodeId,ConversionInputId,ConversionOutputId;
    struct FConversionField{FProperty* Property=nullptr;const FFieldClass* Kind=nullptr;FName Name;uint64 Flags=0;int32 Size=0,Offset=0,Dim=0;};
    TArray<FConversionField> ConversionFields;
    static bool ConversionType(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {auto X=A,Y=B;X.bIsReference=Y.bIsReference=false;X.bIsConst=Y.bIsConst=false;return Type(X,Y);}
    bool ConversionCurrent()
    {
        if(!ConversionReady)return true;
        if(!Live(ConversionOwner.Get())||ConversionOwner->GetClass()!=UClass::StaticClass()||!ConversionOwner->HasAnyClassFlags(CLASS_Native)||ConversionOwner->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated)
            ||!ConversionOwner->ClassGeneratedBy.IsResolved()||ConversionOwner->ClassGeneratedBy!=nullptr||ConversionOwner->GetOutermost()->GetName()!=TEXT("/Script/Engine")||!ConversionOwner->GetPathName().Equals(ConversionOwnerPath,ESearchCase::CaseSensitive)
            ||!Live(ConversionFunction.Get())||ConversionFunction->GetClass()!=UFunction::StaticClass()||ConversionFunction->GetOuter()!=ConversionOwner.Get()||ConversionFunction->FunctionFlags!=ConversionFunctionFlags||!ConversionFunction->GetPathName().Equals(ConversionFunctionPath,ESearchCase::CaseSensitive))
            return Bad(TEXT("autocast_native_source"),ConversionFunctionPath,TEXT("changed source function/class role"));
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(ConversionFunction.Get(),EFieldIterationFlags::None);It;++It){if(Now.Num()>=4)return Bad(TEXT("autocast_native_signature"),TEXT("two captured native parameters"),TEXT("over bound"));Now.Add(*It);}
        if(Now.Num()!=ConversionFields.Num())return Bad(TEXT("autocast_native_signature"),LexToString(ConversionFields.Num()),LexToString(Now.Num()));
        for(int32 I=0;I<Now.Num();++I){const auto& E=ConversionFields[I];if(Now[I]!=E.Property)return Bad(TEXT("autocast_native_field"),E.Name.ToString(),TEXT("different address"));auto* P=Now[I];if(P->GetOwnerStruct()!=ConversionFunction.Get()||P->GetClass()!=E.Kind||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->ArrayDim!=E.Dim)return Bad(TEXT("autocast_native_field"),E.Name.ToString(),TEXT("changed owner/flags/layout"));}
        return true;
    }
    bool BindConversion(UEdGraphPin* A,UEdGraphPin* B,const UEdGraphSchema_K2* Schema)
    {
        if(!Current())return false;ConversionSourceType=A->PinType;ConversionTargetType=B->PinType;
        ++ConversionSearchCalls;const auto Found=Schema->SearchForAutocastFunction(ConversionSourceType,ConversionTargetType);
        TStrongObjectPtr<UClass> ImmediateOwner(Found.IsSet()?Found->FunctionOwner:nullptr);
        if(!Current())return false;
        if(!Found.IsSet()||!Live(ImmediateOwner.Get())||ImmediateOwner->GetClass()!=UClass::StaticClass()||!ImmediateOwner->HasAnyClassFlags(CLASS_Native)||ImmediateOwner->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated)
            ||ImmediateOwner->GetOutermost()->GetName()!=TEXT("/Script/Engine")||!ImmediateOwner->ClassGeneratedBy.IsResolved()||ImmediateOwner->ClassGeneratedBy!=nullptr)
            return Bad(TEXT("autocast_lookup"),TEXT("existing native Engine primitive autocast"),TEXT("specialized/custom conversion remains pending"));
        ConversionOwner=MoveTemp(ImmediateOwner);ConversionOwnerPath=ConversionOwner->GetPathName();
        for(TFieldIterator<UFunction> It(ConversionOwner.Get(),EFieldIterationFlags::None);It;++It)if(It->GetFName()==Found->TargetFunction){ConversionFunction.Reset(*It);break;}
        if(!Live(ConversionFunction.Get())||ConversionFunction->GetClass()!=UFunction::StaticClass()||ConversionFunction->GetOuter()!=ConversionOwner.Get()||!ConversionFunction->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_BlueprintPure))return Bad(TEXT("autocast_function"),TEXT("direct native static pure function"),TEXT("unavailable"));
        ConversionFunctionPath=ConversionFunction->GetPathName();ConversionFunctionFlags=ConversionFunction->FunctionFlags;
        FProperty* Input=nullptr;FProperty* Output=nullptr;
        for(TFieldIterator<FProperty> It(ConversionFunction.Get(),EFieldIterationFlags::None);It;++It)
        {auto* P=*It;if(ConversionFields.Num()>=2||P->GetOwnerStruct()!=ConversionFunction.Get()||P->ArrayDim!=1||!P->HasAnyPropertyFlags(CPF_Parm)||P->HasSetterOrGetter())return Bad(TEXT("autocast_function_signature"),TEXT("one direct primitive input and return"),TEXT("pending"));
         if(P->HasAnyPropertyFlags(CPF_ReturnParm)){if(Output)return Bad(TEXT("autocast_return"),TEXT("one return"),TEXT("duplicate"));Output=P;}
         else{if(Input||(P->HasAnyPropertyFlags(CPF_OutParm)&&!P->HasAnyPropertyFlags(CPF_ConstParm)))return Bad(TEXT("autocast_input"),TEXT("one nonmutable input"),TEXT("pending"));Input=P;}
         FConversionField E;E.Property=P;E.Kind=P->GetClass();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;ConversionFields.Add(E);}
        if(!Input||!Output||ConversionFields.Num()!=2)return Bad(TEXT("autocast_function_signature"),TEXT("input plus return"),TEXT("missing"));
        ConversionReady=true;if(!Current())return false;
        ConversionRef.SetExternalMember(Found->TargetFunction,ConversionOwner.Get());if(!Current())return false;
        FEdGraphPinType InputType,OutputType;++ConversionTypeCalls;const bool InputOK=Schema->ConvertPropertyToPinType(Input,InputType);if(!Current())return false;
        ++ConversionTypeCalls;const bool OutputOK=Schema->ConvertPropertyToPinType(Output,OutputType);if(!Current())return false;
        if(!InputOK||!OutputOK||!ConversionType(ConversionSourceType,InputType)||!ConversionType(ConversionTargetType,OutputType))return Bad(TEXT("autocast_function_types"),TEXT("actual native input/return types match requested endpoints"),TEXT("different"));
        return true;
    }
    bool ConversionCandidate()
    {
        if(!ConversionRoute||LinkDone||CompilerReturn)return true;
        const FNode* NewNode=nullptr;const FPin* Source=nullptr;const FPin* Target=nullptr;
        for(int32 I=0;I<Rows.Num();++I)
        {
            const auto& Old=Rows[I];const auto& Now=Candidate[I];const bool IsSelected=Old.Hold.Get()==Selected;
            if(Now.Nodes.Num()!=Old.Nodes.Num()+(IsSelected?1:0))return Bad(TEXT("autocast_new_node_count"),TEXT("exactly one append in selected graph"),TEXT("different"));
            if(IsSelected)NewNode=&Now.Nodes.Last();
            for(const auto& Node:Now.Nodes)for(const auto& Pin:Node.Pins){if(Pin.Id==SourceId)Source=&Pin;if(Pin.Id==TargetId)Target=&Pin;}
        }
        if(!NewNode||!Source||!Target||NewNode->Class!=UK2Node_CallFunction::StaticClass()||!NewNode->Call||!NewNode->Ref.IsSameReference(ConversionRef)||!NewNode->Id.IsValid())return Bad(TEXT("autocast_new_node_role"),ConversionFunctionPath,TEXT("not the one exact predicted native CallFunction"));
        const FPin* Input=nullptr;const FPin* Output=nullptr;
        for(const auto& Pin:NewNode->Pins)
        {
            if(Pin.Name==UEdGraphSchema_K2::PN_Self&&Pin.Direction==EGPD_Input&&(Pin.Flags&1)&&Pin.Links.IsEmpty()&&Pin.Type.PinCategory==UEdGraphSchema_K2::PC_Object&&Pin.Type.PinSubCategoryObject.Get()==ConversionOwner.Get())continue;
            if(Pin.Parent||!Pin.Children.IsEmpty()||Pin.Type.ContainerType!=EPinContainerType::None)return Bad(TEXT("autocast_new_pin"),TEXT("whole primitive ports"),TEXT("pending"));
            if(Pin.Direction==EGPD_Input&&ConversionType(Pin.Type,ConversionSourceType)&&!Input)Input=&Pin;
            else if(Pin.Direction==EGPD_Output&&ConversionType(Pin.Type,ConversionTargetType)&&!Output)Output=&Pin;
            else return Bad(TEXT("autocast_new_pin"),TEXT("one input, one return, optional unlinked hidden self"),TEXT("unexpected port"));
        }
        if(!Input||!Output||Input->Links.Num()!=1||Input->Links[0]!=Source->Address||Output->Links.Num()!=1||Output->Links[0]!=Target->Address
            ||!Source->Links.Contains(Input->Address)||Target->Links.Num()!=1||Target->Links[0]!=Output->Address)
            return Bad(TEXT("autocast_two_legs"),TEXT("exact reciprocal source-input and return-target legs"),TEXT("different"));
        auto* SourcePlan=PlannedLinks.Find(SourceId);auto* TargetPlan=PlannedLinks.Find(TargetId);if(!SourcePlan||!TargetPlan||!TargetPlan->IsEmpty())return Bad(TEXT("autocast_endpoint_plan"),TEXT("captured source links and initially empty target"),TEXT("different"));
        SourcePlan->Add(Input->Id);TargetPlan->Add(Output->Id);
        for(const auto& Pin:NewNode->Pins){PlannedLinks.Add(Pin.Id,LinkIds(Pin,Candidate));PinOwners.Add(Pin.Id,NewNode->Id);}
        ConversionNodeId=NewNode->Id;ConversionInputId=Input->Id;ConversionOutputId=Output->Id;return true;
    }
    TSharedRef<FJsonObject> ConversionReceipt()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("native_source_observed"),ConversionReady);J->SetNumberField(TEXT("public_autocast_search_calls"),ConversionSearchCalls);J->SetNumberField(TEXT("public_property_type_calls"),ConversionTypeCalls);
        J->SetStringField(TEXT("native_function"),ConversionFunctionPath);J->SetStringField(TEXT("conversion_node_id"),ConversionNodeId.ToString());J->SetStringField(TEXT("conversion_input_pin_id"),ConversionInputId.ToString());J->SetStringField(TEXT("conversion_output_pin_id"),ConversionOutputId.ToString());
        J->SetBoolField(TEXT("SDK_internal_callback_atomicity_proven"),false);J->SetBoolField(TEXT("legacy_direct_link_success_overridden"),false);return J;
    }
    bool Advance()
    {
        // No old pin is accessed here; fresh physical inventory is fixed before any text observer.
        if(!Capture(Candidate))return false;CandidateReady=true;if(!Pure(Candidate)||!Texts(Candidate))return false;
        if(Candidate.Num()!=Rows.Num())return Bad(TEXT("graph_count"),LexToString(Rows.Num()),LexToString(Candidate.Num()));
        if(!ConversionCandidate())return false;
        for(int32 I=0;I<Rows.Num();++I){const auto& A=Rows[I];const auto& B=Candidate[I];if(A.Hold.Get()!=B.Hold.Get()||A.Id!=B.Id||A.Nodes.Num()+((ConversionRoute&&!LinkDone&&!CompilerReturn&&A.Hold.Get()==Selected)?1:0)!=B.Nodes.Num())return Bad(TEXT("graph_semantics"),TEXT("same graph/nodes"),TEXT("different"));for(int32 J=0;J<A.Nodes.Num();++J){const auto& N=A.Nodes[J];const auto& M=B.Nodes[J];if(N.Hold.Get()!=M.Hold.Get()||N.Id!=M.Id||N.Class!=M.Class||!N.Path.Equals(M.Path,ESearchCase::CaseSensitive)||N.X!=M.X||N.Y!=M.Y||!N.Comment.Equals(M.Comment,ESearchCase::CaseSensitive)||N.Call!=M.Call||(N.Call&&!N.Ref.IsSameReference(M.Ref))||N.Pins.Num()!=M.Pins.Num())return Bad(TEXT("node_semantics"),N.Id.ToString(),TEXT("different"));for(int32 K=0;K<N.Pins.Num();++K){const auto& P=N.Pins[K];const auto& Q=M.Pins[K];const auto Expected=(!CompilerReturn&&LinkPlanned)?PlannedLinks.FindRef(P.Id):LinkIds(P,Rows);const FSetCell* SetExpected=(!CompilerReturn&&!LinkDone&&LinkPlanned)?SetCells.Find(P.Id):nullptr;const bool ResetTarget=!CompilerReturn&&PlannedDefault&&P.Id==TargetId;const bool ResetOldExec=!CompilerReturn&&OldPlannedDefault&&P.Id==OldId;const FString& ExpectedRaw=SetExpected?SetExpected->Raw:ResetOldExec?OldPlannedRaw:ResetTarget?PlannedRaw:P.Raw;const FString& ExpectedText=SetExpected?SetExpected->Text:ResetOldExec?OldPlannedText:ResetTarget?PlannedText:P.Display;UObject* ExpectedObject=SetExpected?SetExpected->Object:ResetOldExec?OldPlannedObject:ResetTarget?PlannedObject:P.Object;const uint32 ExpectedTextFlags=SetExpected?SetExpected->TextFlags:ResetOldExec?OldPlannedTextFlags:ResetTarget?PlannedTextFlags:FTextInspector::GetFlags(P.Text);if(P.Id!=Q.Id||P.Persistent!=Q.Persistent||P.Name!=Q.Name||P.Direction!=Q.Direction||!Type(SetExpected?SetExpected->Type:P.Type,Q.Type)||ExpectedObject!=Q.Object||P.Flags!=Q.Flags||!ExpectedRaw.Equals(Q.Raw,ESearchCase::CaseSensitive)||!P.Auto.Equals(Q.Auto,ESearchCase::CaseSensitive)||!ExpectedText.Equals(Q.Display,ESearchCase::CaseSensitive)||!P.FriendlyDisplay.Equals(Q.FriendlyDisplay,ESearchCase::CaseSensitive)||ExpectedTextFlags!=FTextInspector::GetFlags(Q.Text)||FTextInspector::GetFlags(P.Friendly)!=FTextInspector::GetFlags(Q.Friendly)||PinId(P.Parent,Rows)!=PinId(Q.Parent,Candidate)||ChildIds(P,Rows)!=ChildIds(Q,Candidate)||LinkIds(Q,Candidate)!=Expected)return Bad(TEXT("pin_semantics"),P.Id.ToString(),TEXT("unexpected type/value/link transition"));}}}
        Rows=MoveTemp(Candidate);CandidateReady=false;Advancing=false;CompilerReturn=false;LinkDone|=LinkPlanned;return Current();
    }
    bool Current()
    {if(!Context())return false;if(!BP->GetPathName().Equals(BPPath,ESearchCase::CaseSensitive)||BP->BlueprintType!=BPTYPE_Normal||!BP->ParentClass.GetGCPtr().IsResolved()||BP->ParentClass.Get()!=AActor::StaticClass()||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||!BP->NewVariables.IsEmpty()||!BP->ImplementedInterfaces.IsEmpty()||!BP->Timelines.IsEmpty()||Categories()!=Roles)return Bad(TEXT("host_current"),TEXT("same idle direct Actor authored graph categories"),TEXT("changed"));if(!Classes(false)||!ArrayCurrent()||!SetCurrent()||!ConversionCurrent())return false;if(CandidateReady)return Pure(Candidate);if(Advancing)return Advance();return !Initialized||Pure(Rows);}
    // QI/AAU: concrete authored primitive Array -> typed TargetArray of the
    // original nine array operations. The schema writer and lease remain shared.
    struct FArrayField {FProperty* P=nullptr;const FFieldClass* Class=nullptr;FFieldVariant Owner;FName Name;uint64 Flags=0;int32 Size=0,Offset=0,Dim=0;FProperty* Inner=nullptr;const FFieldClass* InnerClass=nullptr;FFieldVariant InnerOwner;uint64 InnerFlags=0;int32 InnerSize=0,InnerOffset=0,InnerDim=0;};
    struct FArrayUser {FName Name;FEdGraphPinType Type;EEdGraphPinDirection Direction;FString Default;};
    bool ArrayRoute=false,ArrayReady=false;TStrongObjectPtr<UClass> ArrayLibrary;TStrongObjectPtr<UFunction> ArrayFunction;TStrongObjectPtr<UK2Node_FunctionEntry> ArrayEntry;TStrongObjectPtr<UK2Node_CallArrayFunction> ArrayCall;
    FEdGraphPinType ArrayOutputType,ArrayInputType,ArrayOldOutputType;FString ArraySourcePinName,ArrayInputPinName,ArrayOldPinName;bool ArraySchemaChecked=false,ArraySchemaCompatible=false;
    TArray<FArrayField> ArrayFields;TArray<FArrayUser> ArrayUsers;FMemberReference ArrayEntryReference;EFunctionFlags ArrayFunctionFlags=FUNC_None;FString ArrayLibraryPath,ArrayFunctionPath,ArrayParmMetadata,ArrayDependentMetadata,ArrayRefMetadata,ArrayInternalMetadata;FName ArrayName;TStrongObjectPtr<UObject> ArrayCDO;
    static bool ArrayFunctionName(FName N)
    {return N==TEXT("Array_Length")||N==TEXT("Array_Get")||N==TEXT("Array_Set")||N==TEXT("Array_Add")||N==TEXT("Array_AddUnique")||N==TEXT("Array_Insert")||N==TEXT("Array_Contains")||N==TEXT("Array_RemoveItem")||N==TEXT("Array_Clear");}
    static bool PrimitiveArray(const FEdGraphPinType& T)
    {return T.ContainerType==EPinContainerType::Array&&!T.bIsUObjectWrapper&&(T.PinCategory==UEdGraphSchema_K2::PC_Int||T.PinCategory==UEdGraphSchema_K2::PC_Int64||T.PinCategory==UEdGraphSchema_K2::PC_Real||T.PinCategory==UEdGraphSchema_K2::PC_Boolean||T.PinCategory==UEdGraphSchema_K2::PC_Name||T.PinCategory==UEdGraphSchema_K2::PC_String);}
    static bool ArrayElement(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {if(!PrimitiveArray(A)||!PrimitiveArray(B))return false;auto X=A,Y=B;X.bIsReference=Y.bIsReference=false;X.bIsConst=Y.bIsConst=false;return Type(X,Y);}
    bool ArrayCurrent()
    {
        if(!ArrayReady)return true;
        if(!Live(ArrayLibrary.Get())||ArrayLibrary->GetClass()!=UClass::StaticClass()||!ArrayLibrary->HasAnyClassFlags(CLASS_Native)||!ArrayLibrary->ClassGeneratedBy.IsResolved()||ArrayLibrary->ClassGeneratedBy.Get()!=nullptr||!ArrayLibrary->GetPathName().Equals(ArrayLibraryPath,ESearchCase::CaseSensitive)||!Live(ArrayFunction.Get())||ArrayFunction->GetClass()!=UFunction::StaticClass()||ArrayFunction->GetOuter()!=ArrayLibrary.Get()||ArrayFunction->GetFName()!=ArrayName||ArrayFunction->FunctionFlags!=ArrayFunctionFlags||!ArrayFunction->GetPathName().Equals(ArrayFunctionPath,ESearchCase::CaseSensitive))return Bad(TEXT("array_native_function_role"),ArrayFunctionPath,TEXT("different source role"));
        if(!ArrayLibrary->ClassDefaultObject.IsResolved()||ArrayLibrary->ClassDefaultObject.Get()!=ArrayCDO.Get()
            ||(ArrayCDO.IsValid()&&(!Live(ArrayCDO.Get())||ArrayCDO->GetClass()!=ArrayLibrary.Get()||!ArrayCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||ArrayCDO->GetPackage()!=ArrayLibrary->GetPackage())))return Bad(TEXT("array_native_CDO_role"),TEXT("captured nullable native library CDO identity"),TEXT("changed"));
        if(ArrayFunction->GetMetaData(TEXT("ArrayParm"))!=ArrayParmMetadata||ArrayFunction->GetMetaData(TEXT("ArrayTypeDependentParams"))!=ArrayDependentMetadata
            ||ArrayFunction->GetMetaData(TEXT("AutoCreateRefTerm"))!=ArrayRefMetadata||ArrayFunction->GetMetaData(TEXT("BlueprintInternalUseOnly"))!=ArrayInternalMetadata)return Bad(TEXT("array_native_metadata"),ArrayName.ToString(),TEXT("changed ArrayParm/dependent/ref/internal metadata"));
        TArray<FProperty*> Inventory;for(TFieldIterator<FProperty> It(ArrayFunction.Get(),EFieldIterationFlags::None);It;++It){if(Inventory.Num()>=16)return Bad(TEXT("array_native_fields"),LexToString(ArrayFields.Num()),TEXT("over budget"));Inventory.Add(*It);}
        if(Inventory.Num()!=ArrayFields.Num())return Bad(TEXT("array_native_field_count"),LexToString(ArrayFields.Num()),LexToString(Inventory.Num()));
        for(int32 I=0;I<ArrayFields.Num();++I)
        {
            const auto& E=ArrayFields[I];if(Inventory[I]!=E.P)return Bad(TEXT("array_native_field_membership"),E.Name.ToString(),TEXT("different field address"));auto* P=Inventory[I];
            if(P->GetClass()!=E.Class||P->GetOwnerVariant()!=E.Owner||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->ArrayDim!=E.Dim)return Bad(TEXT("array_native_field_layout"),E.Name.ToString(),TEXT("different owner/class/flags/layout"));
            if(E.Inner)
            {
                auto* A=static_cast<FArrayProperty*>(P);if(A->Inner!=E.Inner)return Bad(TEXT("array_native_inner_slot"),E.Name.ToString(),TEXT("different Inner"));auto* Inner=A->Inner;
                if(Inner->GetOwnerProperty()!=A||Inner->GetOwnerVariant()!=E.InnerOwner||Inner->GetClass()!=E.InnerClass||uint64(Inner->GetPropertyFlags())!=E.InnerFlags||Inner->GetSize()!=E.InnerSize||Inner->GetOffset_ForInternal()!=E.InnerOffset||Inner->ArrayDim!=E.InnerDim)return Bad(TEXT("array_native_inner_role"),E.Name.ToString(),TEXT("different actual Inner owner/class/flags/layout"));
            }
        }
        if(!Live(Selected)||Selected->GetOuter()!=BP.Get())return Bad(TEXT("array_graph_role"),TEXT("own selected function graph"),TEXT("unavailable"));
        bool FoundEntry=false,FoundCall=false;for(const auto& Slot:Selected->Nodes){if(!Slot.IsResolved())return Bad(TEXT("array_node_slot"),TEXT("resolved"),TEXT("unresolved"));FoundEntry|=Slot.Get()==ArrayEntry.Get();FoundCall|=Slot.Get()==ArrayCall.Get();}
        if(!FoundEntry||!FoundCall||!Live(ArrayEntry.Get())||ArrayEntry->GetClass()!=UK2Node_FunctionEntry::StaticClass()||ArrayEntry->GetOuter()!=Selected||!ArrayEntry->FunctionReference.IsSameReference(ArrayEntryReference)||!Live(ArrayCall.Get())||ArrayCall->GetClass()!=UK2Node_CallArrayFunction::StaticClass()||ArrayCall->GetOuter()!=Selected||ArrayCall->FunctionReference.GetMemberName()!=ArrayName||ArrayCall->FunctionReference.IsSelfContext())return Bad(TEXT("array_node_roles"),TEXT("same own authored Entry and native Array call"),TEXT("different"));
        if(ArrayEntry->UserDefinedPins.Num()!=ArrayUsers.Num())return Bad(TEXT("array_authored_parameter_count"),LexToString(ArrayUsers.Num()),LexToString(ArrayEntry->UserDefinedPins.Num()));
        for(int32 I=0;I<ArrayUsers.Num();++I){const auto& E=ArrayUsers[I];const auto& A=ArrayEntry->UserDefinedPins[I];if(!A.IsValid()||A->PinName!=E.Name||!Type(A->PinType,E.Type)||A->DesiredPinDirection!=E.Direction||!A->PinDefaultValue.Equals(E.Default,ESearchCase::CaseSensitive))return Bad(TEXT("array_authored_parameter"),E.Name.ToString(),TEXT("different name/type/direction/default"));}return true;
    }
    bool BindArray(UEdGraphPin* A,UEdGraphPin* B,const UEdGraphSchema_K2* Schema)
    {
        if(!Current())return false;
        auto* Entry=Cast<UK2Node_FunctionEntry>(A->GetOwningNodeUnchecked());auto* Call=Cast<UK2Node_CallArrayFunction>(B->GetOwningNodeUnchecked());
        if(!Entry||Entry->GetClass()!=UK2Node_FunctionEntry::StaticClass()||!Call||Call->GetClass()!=UK2Node_CallArrayFunction::StaticClass()||Entry->GetOuter()!=Selected||Call->GetOuter()!=Selected||!BP->FunctionGraphs.Contains(Selected)||B->PinName!=TEXT("TargetArray")||!ArrayFunctionName(Call->FunctionReference.GetMemberName())||Call->FunctionReference.IsSelfContext())return Bad(TEXT("array_endpoint_profile"),TEXT("own function authored Entry output -> typed original Array9.TargetArray"),TEXT("pending"));
        int32 Entries=0;for(const auto& G:Rows)if(G.Hold.Get()==Selected)for(const auto& N:G.Nodes)Entries+=N.Class==UK2Node_FunctionEntry::StaticClass();if(Entries!=1)return Bad(TEXT("array_entry_count"),TEXT("one own FunctionEntry"),LexToString(Entries));
        ArrayEntry.Reset(Entry);ArrayCall.Reset(Call);ArrayEntryReference=Entry->FunctionReference;ArrayName=Call->FunctionReference.GetMemberName();ArrayOutputType=A->PinType;ArrayInputType=B->PinType;ArraySourcePinName=A->PinName.ToString();ArrayInputPinName=B->PinName.ToString();
        for(const auto& User:Entry->UserDefinedPins){if(!User.IsValid()||ArrayUsers.Num()>=64||User->PinType.PinSubCategoryObject.IsStale()||User->PinType.PinValueType.TerminalSubCategoryObject.IsStale())return Bad(TEXT("array_authored_parameters"),TEXT("bounded current UserPinInfo roles"),TEXT("unavailable"));ArrayUsers.Add({User->PinName,User->PinType,User->DesiredPinDirection,User->PinDefaultValue});}
        const auto Authored=[&](const UEdGraphPin* P){for(const auto& U:ArrayUsers)if(U.Name==P->PinName&&U.Direction==EGPD_Output&&ArrayElement(U.Type,P->PinType))return true;return false;};
        if(!Authored(A))return Bad(TEXT("array_producer_parameter"),A->PinName.ToString(),TEXT("not an authored primitive Array input"));
        if(B->LinkedTo.Num()>1)return Bad(TEXT("array_old_source_count"),TEXT("zero or one current source"),LexToString(B->LinkedTo.Num()));
        if(B->LinkedTo.Num()==1){auto* Old=B->LinkedTo[0];if(Old->GetOwningNodeUnchecked()!=Entry||Old->Direction!=EGPD_Output||!Authored(Old)||!ArrayElement(Old->PinType,B->PinType))return Bad(TEXT("array_old_producer_parameter"),TEXT("same function authored same-element Array output"),TEXT("pending"));ArrayOldOutputType=Old->PinType;ArrayOldPinName=Old->PinName.ToString();}
        ArrayLibrary.Reset(FindObject<UClass>(nullptr,TEXT("/Script/Engine.KismetArrayLibrary")));if(!Live(ArrayLibrary.Get())||ArrayLibrary->GetClass()!=UClass::StaticClass()||!ArrayLibrary->HasAnyClassFlags(CLASS_Native)||!ArrayLibrary->ClassGeneratedBy.IsResolved()||ArrayLibrary->ClassGeneratedBy.Get()!=nullptr||!ArrayLibrary->ClassDefaultObject.IsResolved())return Bad(TEXT("array_library_unavailable"),TEXT("already loaded exact native KismetArrayLibrary"),TEXT("unavailable"));ArrayLibraryPath=ArrayLibrary->GetPathName();ArrayCDO.Reset(ArrayLibrary->ClassDefaultObject.Get());
        for(TFieldIterator<UFunction> It(ArrayLibrary.Get(),EFieldIterationFlags::None);It;++It)if(It->GetFName()==ArrayName){ArrayFunction.Reset(*It);break;}
        if(!Live(ArrayFunction.Get())||ArrayFunction->GetClass()!=UFunction::StaticClass()||ArrayFunction->GetOuter()!=ArrayLibrary.Get()||!ArrayFunction->HasAllFunctionFlags(FUNC_Native|FUNC_Static)||!ArrayFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable|FUNC_BlueprintPure))return Bad(TEXT("array_function_unavailable"),TEXT("direct loaded original Array9 UFunction"),TEXT("unavailable"));ArrayFunctionPath=ArrayFunction->GetPathName();ArrayFunctionFlags=ArrayFunction->FunctionFlags;
        const bool Length=ArrayName==TEXT("Array_Length"),Get=ArrayName==TEXT("Array_Get"),Set=ArrayName==TEXT("Array_Set"),
            Add=ArrayName==TEXT("Array_Add")||ArrayName==TEXT("Array_AddUnique"),Insert=ArrayName==TEXT("Array_Insert"),
            Contains=ArrayName==TEXT("Array_Contains"),Remove=ArrayName==TEXT("Array_RemoveItem");
        const FName Item=Add||Insert?FName(TEXT("NewItem")):Contains?FName(TEXT("ItemToFind")):Get||Set||Remove?FName(TEXT("Item")):NAME_None;
        const bool Return=Length||Add||Contains||Remove,Index=Get||Set||Insert;
        ArrayParmMetadata=TEXT("TargetArray");ArrayDependentMetadata=Item.IsNone()?FString():Item.ToString();
        ArrayRefMetadata=Get||Item.IsNone()?FString():Item.ToString();ArrayInternalMetadata=Get?TEXT("true"):TEXT("");
        if(ArrayFunction->GetMetaData(TEXT("ArrayParm"))!=ArrayParmMetadata||ArrayFunction->GetMetaData(TEXT("ArrayTypeDependentParams"))!=ArrayDependentMetadata
            ||ArrayFunction->GetMetaData(TEXT("AutoCreateRefTerm"))!=ArrayRefMetadata||ArrayFunction->GetMetaData(TEXT("BlueprintInternalUseOnly"))!=ArrayInternalMetadata)
            return Bad(TEXT("array_native_metadata"),ArrayName.ToString(),TEXT("actual installed Array9 metadata differs"));
        TSet<FName> Expected;Expected.Add(TEXT("TargetArray"));if(!Item.IsNone())Expected.Add(Item);if(Index)Expected.Add(TEXT("Index"));if(Set)Expected.Add(TEXT("bSizeToFit"));if(Return)Expected.Add(TEXT("ReturnValue"));
        TSet<FName> Seen;
        for(TFieldIterator<FProperty> It(ArrayFunction.Get(),EFieldIterationFlags::None);It;++It)
        {
            if(ArrayFields.Num()>=16)return Bad(TEXT("array_field_budget"),TEXT("bounded native function"),TEXT("too many"));FProperty* P=*It;const FName Name=P->GetFName();
            if(P->GetOwnerStruct()!=ArrayFunction.Get()||P->ArrayDim!=1||!P->HasAnyPropertyFlags(CPF_Parm)||!Expected.Contains(Name)||Seen.Contains(Name))return Bad(TEXT("array_field_owner"),TEXT("unique direct original Array9 parameter"),P->GetName());
            const bool IsReturn=Name==TEXT("ReturnValue"),IsGetOutput=Get&&Name==Item;
            if(P->HasAnyPropertyFlags(CPF_ReturnParm)!=IsReturn||(!IsReturn&&!IsGetOutput&&P->HasAnyPropertyFlags(CPF_OutParm)&&!P->HasAnyPropertyFlags(CPF_ReferenceParm))
                ||(IsGetOutput&&!P->HasAnyPropertyFlags(CPF_OutParm)))return Bad(TEXT("array_parameter_direction"),Name.ToString(),TEXT("different actual native input/output role"));
            FArrayField E;E.P=P;E.Class=P->GetClass();E.Owner=P->GetOwnerVariant();E.Name=Name;E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;
            if(Name==TEXT("TargetArray")&&P->GetClass()==FArrayProperty::StaticClass())
            {
                auto* Array=static_cast<FArrayProperty*>(P);auto* Inner=Array->Inner;
                if(!Inner||Inner->GetOwnerProperty()!=Array||Inner->GetClass()!=FIntProperty::StaticClass()||Inner->ArrayDim!=1)return Bad(TEXT("array_inner_prototype"),TEXT("actual owned native Int Inner; wildcard prototype is not selected element constraint"),TEXT("different"));
                E.Inner=Inner;E.InnerClass=Inner->GetClass();E.InnerOwner=Inner->GetOwnerVariant();E.InnerFlags=uint64(Inner->GetPropertyFlags());E.InnerSize=Inner->GetSize();E.InnerOffset=Inner->GetOffset_ForInternal();E.InnerDim=Inner->ArrayDim;
            }
            else if((Name==Item||Name==TEXT("Index")||(IsReturn&&!Contains&&!Remove))&&P->GetClass()==FIntProperty::StaticClass()){}
            else if((Name==TEXT("bSizeToFit")||(IsReturn&&(Contains||Remove)))&&P->GetClass()==FBoolProperty::StaticClass()){}
            else return Bad(TEXT("array_native_signature"),TEXT("original Array9 array/int/bool native prototype"),P->GetName());
            Seen.Add(Name);ArrayFields.Add(E);
        }
        if(Seen.Num()!=Expected.Num())return Bad(TEXT("array_native_signature"),LexToString(Expected.Num()),LexToString(Seen.Num()));
        // SDK CallArrayFunction84/306 propagates only category/subcategory/type
        // object. Since this route accepts an already typed target, every peer
        // must already carry the same element so the callback has no need to
        // reset peer defaults, recombine subpins or infer a new element family.
        if(!Item.IsNone())
        {
            const FPin* Dependent=nullptr;
            for(const auto& G:Rows)for(const auto& N:G.Nodes)if(N.Hold.Get()==Call)for(const auto& P:N.Pins)if(P.Name==Item)
            {if(Dependent)return Bad(TEXT("array_dependent_pin"),Item.ToString(),TEXT("ambiguous"));Dependent=&P;}
            if(!Dependent||Dependent->Parent||!Dependent->Children.IsEmpty()||Dependent->Type.ContainerType!=EPinContainerType::None
                ||Dependent->Type.PinCategory!=A->PinType.PinCategory||Dependent->Type.PinSubCategory!=A->PinType.PinSubCategory
                ||Dependent->Type.PinSubCategoryObject!=A->PinType.PinSubCategoryObject||Dependent->Type.bIsUObjectWrapper
                ||Dependent->Direction!=(Get?EGPD_Output:EGPD_Input))return Bad(TEXT("array_dependent_pin"),Item.ToString(),TEXT("current whole same-element typed item port required; wildcard inference/split or different-element peers remain incomplete"));
        }
        ArrayReady=true;if(!Current())return false;UFunction* Returned=Call->GetTargetFunction();TStrongObjectPtr<UFunction> Immediate(Returned);if(!Current())return false;if(Returned!=ArrayFunction.Get())return Bad(TEXT("array_call_binding"),ArrayFunctionPath,TEXT("different current target UFunction"));ArraySchemaCompatible=Schema->ArePinsCompatible(A,B,Gen.Get());ArraySchemaChecked=true;if(!Current())return false;return true;
    }
    TSharedRef<FJsonObject> ArrayReceipt()const
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("snapshot_stage"),TEXT("pre_writer_captured_source; final proof uses actual known/postcondition outcome"));J->SetBoolField(TEXT("source_observed"),ArrayReady);J->SetBoolField(TEXT("schema_compatibility_observed"),ArraySchemaChecked);if(!ArrayReady)return J;
        const auto PinType=[](const FEdGraphPinType& T,const FString& Name){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Name);O->SetStringField(TEXT("category"),T.PinCategory.ToString());O->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());O->SetStringField(TEXT("container"),TEXT("array"));O->SetBoolField(TEXT("is_reference"),T.bIsReference);O->SetBoolField(TEXT("is_const"),T.bIsConst);O->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);return O;};
        J->SetObjectField(TEXT("new_source_output"),PinType(ArrayOutputType,ArraySourcePinName));J->SetObjectField(TEXT("target_input"),PinType(ArrayInputType,ArrayInputPinName));if(!ArrayOldPinName.IsEmpty())J->SetObjectField(TEXT("old_source_output"),PinType(ArrayOldOutputType,ArrayOldPinName));
        J->SetStringField(TEXT("native_library"),ArrayLibraryPath);J->SetStringField(TEXT("native_function"),ArrayFunctionPath);J->SetStringField(TEXT("native_operation"),ArrayName.ToString());J->SetStringField(TEXT("native_array_parameter"),ArrayParmMetadata);J->SetStringField(TEXT("native_dependent_parameter"),ArrayDependentMetadata);J->SetStringField(TEXT("native_auto_ref_term"),ArrayRefMetadata);J->SetBoolField(TEXT("typed_peer_inference_required"),false);J->SetBoolField(TEXT("actual_Array9_usage_verified"),false);J->SetStringField(TEXT("native_function_flags"),LexToString(uint32(ArrayFunctionFlags)));J->SetBoolField(TEXT("are_pins_compatible"),ArraySchemaCompatible);J->SetBoolField(TEXT("endpoint_qualifiers_captured_individually"),true);
        for(const auto& E:ArrayFields)if(E.Inner){J->SetStringField(TEXT("native_array_property_flags"),LexToString(E.Flags));J->SetStringField(TEXT("native_inner_flags"),LexToString(E.InnerFlags));J->SetBoolField(TEXT("native_inner_owner_property_verified"),true);J->SetStringField(TEXT("native_inner_prototype"),TEXT("IntProperty; wildcard CustomThunk prototype, not selected element constraint"));}
        J->SetBoolField(TEXT("runtime_array_value_or_length_evaluated"),false);J->SetBoolField(TEXT("runtime_array_operation_evaluated"),false);return J;
    }
// QT concrete source and first SetParam typing. Native prototype descriptors are not graph endpoint qualifiers.
    bool SetRoute=false,SetReady=false;TStrongObjectPtr<UClass> SetLibrary;TStrongObjectPtr<UFunction> SetFunction;TStrongObjectPtr<UK2Node_FunctionEntry> SetEntry;TStrongObjectPtr<UK2Node_CallFunction> SetCall;
    TStrongObjectPtr<UObject> SetCDO;TArray<FArrayField> SetFields;TArray<FArrayUser> SetUsers;FMemberReference SetEntryRef;EFunctionFlags SetFunctionFlags=FUNC_None;FString SetFunctionPath,SetGroup,SetAutoRef;FName SetName;int32 SetEntryFlags=0;bool SetSchemaCompatible=false;FEdGraphPinType SetSourceType,SetTargetType;
    struct FSetCell {FEdGraphPinType Type;FString Raw,Text;UObject* Object=nullptr;uint32 TextFlags=0;};TMap<FGuid,FSetCell> SetCells;
    bool SetBinary=false,SetFollowup=false,BinaryGroupCaptured=false;TArray<FPin> BinaryGroup;
    static bool BinarySetName(FName N){return N==TEXT("Set_Union")||N==TEXT("Set_Intersection")||N==TEXT("Set_Difference");}
    static bool SetFunctionName(FName N){return BinarySetName(N)||N==TEXT("Set_Add")||N==TEXT("Set_Remove")||N==TEXT("Set_Contains")||N==TEXT("Set_Length")||N==TEXT("Set_ToArray");}
    bool SetCurrent()
    {
        if(!SetReady)return true;
        if(!Live(SetLibrary.Get())||SetLibrary->GetClass()!=UClass::StaticClass()||!SetLibrary->HasAnyClassFlags(CLASS_Native)||!SetLibrary->ClassGeneratedBy.IsResolved()||SetLibrary->ClassGeneratedBy.Get()!=nullptr||!SetLibrary->GetPathName().Equals(TEXT("/Script/Engine.BlueprintSetLibrary"),ESearchCase::CaseSensitive)||!SetLibrary->ClassDefaultObject.IsResolved()||SetLibrary->ClassDefaultObject.Get()!=SetCDO.Get()||!Live(SetFunction.Get())||SetFunction->GetClass()!=UFunction::StaticClass()||SetFunction->GetOuter()!=SetLibrary.Get()||SetFunction->GetFName()!=SetName||SetFunction->FunctionFlags!=SetFunctionFlags||!SetFunction->GetPathName().Equals(SetFunctionPath,ESearchCase::CaseSensitive))return Bad(TEXT("set_native_source"),SetFunctionPath,TEXT("changed library/function/CDO binding"));
        if(SetCDO.IsValid()&&(!Live(SetCDO.Get())||SetCDO->GetClass()!=SetLibrary.Get()||!SetCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||SetCDO->GetPackage()!=SetLibrary->GetPackage()))return Bad(TEXT("set_native_CDO"),TEXT("captured nullable native default role"),TEXT("changed"));
        TArray<FProperty*> Inventory;for(TFieldIterator<FProperty> It(SetFunction.Get(),EFieldIterationFlags::None);It;++It){if(Inventory.Num()>=16)return Bad(TEXT("set_native_fields"),TEXT("bounded actual parameters"),TEXT("too many"));Inventory.Add(*It);}if(Inventory.Num()!=SetFields.Num())return Bad(TEXT("set_native_fields"),LexToString(SetFields.Num()),LexToString(Inventory.Num()));
        for(int32 I=0;I<Inventory.Num();++I){const auto& E=SetFields[I];if(Inventory[I]!=E.P)return Bad(TEXT("set_native_field_membership"),E.Name.ToString(),TEXT("changed"));auto* P=Inventory[I];if(P->GetClass()!=E.Class||P->GetOwnerVariant()!=E.Owner||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->ArrayDim!=E.Dim)return Bad(TEXT("set_native_field_layout"),E.Name.ToString(),TEXT("changed"));if(E.Inner){FProperty* Child=E.Class==FSetProperty::StaticClass()?static_cast<FSetProperty*>(P)->ElementProp:static_cast<FArrayProperty*>(P)->Inner;if(Child!=E.Inner)return Bad(TEXT("set_native_nested_slot"),E.Name.ToString(),TEXT("changed ElementProp/Inner"));if(Child->GetOwnerProperty()!=P||Child->GetOwnerVariant()!=E.InnerOwner||Child->GetClass()!=E.InnerClass||uint64(Child->GetPropertyFlags())!=E.InnerFlags||Child->GetSize()!=E.InnerSize||Child->GetOffset_ForInternal()!=E.InnerOffset||Child->ArrayDim!=E.InnerDim)return Bad(TEXT("set_native_nested_layout"),E.Name.ToString(),TEXT("changed"));}}
        if(!SetFunction->GetMetaData(TEXT("SetParam")).Equals(SetGroup,ESearchCase::CaseSensitive)||!SetFunction->GetMetaData(TEXT("AutoCreateRefTerm")).Equals(SetAutoRef,ESearchCase::CaseSensitive))return Bad(TEXT("set_native_metadata"),SetGroup,TEXT("changed"));
        bool EntryFound=false,CallFound=false;for(const auto& Slot:Selected->Nodes){if(!Slot.IsResolved())return Bad(TEXT("set_node_slot"),TEXT("resolved"),TEXT("unresolved"));EntryFound|=Slot.Get()==SetEntry.Get();CallFound|=Slot.Get()==SetCall.Get();}
        if(!EntryFound||!CallFound||!Live(SetEntry.Get())||SetEntry->GetClass()!=UK2Node_FunctionEntry::StaticClass()||SetEntry->GetOuter()!=Selected||!SetEntry->FunctionReference.IsSameReference(SetEntryRef)||SetEntry->GetExtraFlags()!=SetEntryFlags||!Live(SetCall.Get())||SetCall->GetClass()!=UK2Node_CallFunction::StaticClass()||SetCall->GetOuter()!=Selected||SetEntry->UserDefinedPins.Num()!=SetUsers.Num())return Bad(TEXT("set_endpoint_nodes"),TEXT("same own authored Entry and exact CallFunction"),TEXT("changed"));
        for(int32 I=0;I<SetUsers.Num();++I){const auto& E=SetUsers[I];const auto& U=SetEntry->UserDefinedPins[I];if(!U.IsValid()||U->PinName!=E.Name||!Type(U->PinType,E.Type)||U->DesiredPinDirection!=E.Direction||!U->PinDefaultValue.Equals(E.Default,ESearchCase::CaseSensitive))return Bad(TEXT("set_authored_parameter"),E.Name.ToString(),TEXT("changed"));}return true;
    }
    bool BindSet(UEdGraphPin* A,UEdGraphPin* B,const UEdGraphSchema_K2* Schema)
    {
        if(!Current())return false;auto* Entry=Cast<UK2Node_FunctionEntry>(A->GetOwningNodeUnchecked());auto* Call=Cast<UK2Node_CallFunction>(B->GetOwningNodeUnchecked());if(Call&&BinarySetName(Call->FunctionReference.GetMemberName()))return BindBinarySet(A,B,Schema);FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;Int.ContainerType=EPinContainerType::Set;auto Source=A->PinType;Source.bIsReference=false;Source.bIsConst=false;
        if(!Entry||Entry->GetClass()!=UK2Node_FunctionEntry::StaticClass()||!Call||Call->GetClass()!=UK2Node_CallFunction::StaticClass()||Entry->GetOuter()!=Selected||Call->GetOuter()!=Selected||!BP->FunctionGraphs.Contains(Selected)||!Type(Source,Int)||B->PinType.ContainerType!=EPinContainerType::Set||B->PinType.PinCategory!=UEdGraphSchema_K2::PC_Wildcard||!B->LinkedTo.IsEmpty()||!SetFunctionName(Call->FunctionReference.GetMemberName())||Call->FunctionReference.IsSelfContext())return Bad(TEXT("set_first_make_profile"),TEXT("own int Set function input -> unlinked exact Set5 wildcard Set input"),TEXT("pending"));
        SetEntry.Reset(Entry);SetCall.Reset(Call);SetEntryRef=Entry->FunctionReference;SetEntryFlags=Entry->GetExtraFlags();SetName=Call->FunctionReference.GetMemberName();SetSourceType=A->PinType;SetTargetType=B->PinType;
        for(const auto& U:Entry->UserDefinedPins){if(!U.IsValid()||SetUsers.Num()>=64)return Bad(TEXT("set_authored_parameters"),TEXT("bounded valid UserPinInfo"),TEXT("unavailable"));SetUsers.Add({U->PinName,U->PinType,U->DesiredPinDirection,U->PinDefaultValue});}bool Authored=false;for(const auto& U:SetUsers){auto T=U.Type;T.bIsReference=false;T.bIsConst=false;if(U.Name==A->PinName&&U.Direction==EGPD_Output&&Type(T,Int))Authored=true;}if(!Authored)return Bad(TEXT("set_authored_producer"),A->PinName.ToString(),TEXT("not authored int Set"));
        SetLibrary.Reset(FindObject<UClass>(nullptr,TEXT("/Script/Engine.BlueprintSetLibrary")));if(!Live(SetLibrary.Get())||SetLibrary->GetClass()!=UClass::StaticClass()||!SetLibrary->ClassDefaultObject.IsResolved())return Bad(TEXT("set_native_library"),TEXT("already loaded native SetLibrary/resolved nullable CDO"),TEXT("unavailable"));SetCDO.Reset(SetLibrary->ClassDefaultObject.Get());
        for(TFieldIterator<UFunction> It(SetLibrary.Get(),EFieldIterationFlags::None);It;++It)if(It->GetFName()==SetName){SetFunction.Reset(*It);break;}if(!Live(SetFunction.Get())||SetFunction->GetClass()!=UFunction::StaticClass()||SetFunction->GetOuter()!=SetLibrary.Get()||!SetFunction->HasAllFunctionFlags(FUNC_Static|FUNC_Native)||!SetFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable|FUNC_BlueprintPure))return Bad(TEXT("set_native_function"),TEXT("direct loaded native Set5"),TEXT("unavailable"));SetFunctionPath=SetFunction->GetPathName();SetFunctionFlags=SetFunction->FunctionFlags;
        const bool ToArray=SetName==TEXT("Set_ToArray"),Length=SetName==TEXT("Set_Length");const FName SetPin=ToArray?TEXT("A"):TEXT("TargetSet");const FName Item=SetName==TEXT("Set_Add")?TEXT("NewItem"):SetName==TEXT("Set_Remove")?TEXT("Item"):TEXT("ItemToFind");SetGroup=ToArray?TEXT("A|Result"):Length?TEXT("TargetSet"):FString(TEXT("TargetSet|"))+Item.ToString();SetAutoRef=(ToArray||Length)?FString():Item.ToString();if(B->PinName!=SetPin)return Bad(TEXT("set_input_pin"),SetPin.ToString(),B->PinName.ToString());
        bool HasSet=false;for(TFieldIterator<FProperty> It(SetFunction.Get(),EFieldIterationFlags::None);It;++It){auto* P=*It;if(P->GetOwnerStruct()!=SetFunction.Get()||P->ArrayDim!=1||!P->HasAnyPropertyFlags(CPF_Parm)||SetFields.Num()>=8)return Bad(TEXT("set_native_signature"),TEXT("bounded direct native parameters"),TEXT("different"));FArrayField E;E.P=P;E.Class=P->GetClass();E.Owner=P->GetOwnerVariant();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;FProperty* Child=nullptr;
            if(E.Name==SetPin&&E.Class==FSetProperty::StaticClass()){Child=static_cast<FSetProperty*>(P)->ElementProp;if(!Child)return Bad(TEXT("set_native_element"),TEXT("nonnull owned ElementProp"),TEXT("null"));HasSet=true;}else if(ToArray&&E.Name==TEXT("Result")&&E.Class==FArrayProperty::StaticClass()){Child=static_cast<FArrayProperty*>(P)->Inner;if(!Child)return Bad(TEXT("set_native_inner"),TEXT("nonnull owned Inner"),TEXT("null"));}else if((E.Name==Item&&E.Class==FIntProperty::StaticClass())||(E.Name==TEXT("ReturnValue")&&(E.Class==FBoolProperty::StaticClass()||E.Class==FIntProperty::StaticClass()))){}else return Bad(TEXT("set_native_signature"),TEXT("Set5 actual int prototype"),E.Name.ToString());
            if(Child){if(Child->GetOwnerProperty()!=P||Child->GetClass()!=FIntProperty::StaticClass()||Child->ArrayDim!=1)return Bad(TEXT("set_native_element"),TEXT("owned Int ElementProp/Inner"),TEXT("different"));E.Inner=Child;E.InnerClass=Child->GetClass();E.InnerOwner=Child->GetOwnerVariant();E.InnerFlags=uint64(Child->GetPropertyFlags());E.InnerSize=Child->GetSize();E.InnerOffset=Child->GetOffset_ForInternal();E.InnerDim=Child->ArrayDim;}SetFields.Add(E);}
        if(!HasSet||SetFields.Num()!=(Length?2:SetName==TEXT("Set_Add")?2:ToArray?2:3))return Bad(TEXT("set_native_signature"),TEXT("Set parameter"),TEXT("missing"));SetReady=true;if(!Current())return false;UFunction* Returned=Call->GetTargetFunction();TStrongObjectPtr<UFunction> Immediate(Returned);if(!Current())return false;if(Returned!=SetFunction.Get())return Bad(TEXT("set_call_function_binding"),SetFunctionPath,TEXT("different actual target"));
        TArray<FName> GroupNames;GroupNames.Add(SetPin);if(!Length)GroupNames.Add(ToArray?FName(TEXT("Result")):Item);TArray<FPin> Group;for(const auto& G:Rows)for(const auto& N:G.Nodes)if(N.Hold.Get()==Call)for(const auto& P:N.Pins)if(GroupNames.Contains(P.Name))Group.Add(P);if(Group.Num()!=GroupNames.Num())return Bad(TEXT("set_group_pins"),SetGroup,TEXT("missing/duplicate group pins"));
        for(const auto& P:Group){if(!P.Links.IsEmpty()||P.Parent||!P.Children.IsEmpty()||P.Type.PinCategory!=UEdGraphSchema_K2::PC_Wildcard||P.Type.PinSubCategory!=NAME_None||P.Type.PinSubCategoryObject.IsValid()||P.Type.bIsUObjectWrapper||!P.Raw.IsEmpty()||!P.Auto.IsEmpty()||P.Object||!P.Display.IsEmpty())return Bad(TEXT("set_untyped_group"),P.Name.ToString(),TEXT("requires whole unlinked defaults-empty wildcard group"));const auto ExpectedContainer=P.Name==SetPin?EPinContainerType::Set:ToArray?EPinContainerType::Array:EPinContainerType::None;if(P.Type.ContainerType!=ExpectedContainer)return Bad(TEXT("set_group_container"),P.Name.ToString(),TEXT("different"));
            FSetCell E;E.Type=P.Type;E.Type.PinCategory=UEdGraphSchema_K2::PC_Int;E.Type.PinSubCategory=NAME_None;E.Type.PinSubCategoryObject=nullptr;E.Type.bIsUObjectWrapper=false;E.Raw=P.Raw;E.Object=P.Object;E.Text=P.Display;E.TextFlags=FTextInspector::GetFlags(P.Text);
            bool Valid=false;if(ExpectedContainer==EPinContainerType::None){Valid=Schema->DefaultValueSimpleValidation(E.Type,P.Name,P.Raw,TObjectPtr<UObject>(P.Object),P.Text);if(!Current())return false;}
            // Containers/non-auto-ref output arrays fail IsPinDefaultValid; input connection resets Auto as well.
            if(!Valid||P.Id==B->PinId){TObjectPtr<UObject> O=nullptr;FText T;FString Raw;Schema->GetPinDefaultValuesFromString(E.Type,Call,P.Auto,Raw,O,T,false);TStrongObjectPtr<UObject> Hold(O.Get());if(!Current())return false;const FString Display=T.ToString();if(!Current())return false;if(Hold.IsValid())Holds.Emplace(Hold.Get());E.Raw=Raw;E.Object=Hold.Get();E.Text=Display;E.TextFlags=FTextInspector::GetFlags(T);}SetCells.Add(P.Id,MoveTemp(E));}
        SetSchemaCompatible=Schema->ArePinsCompatible(A,B,Gen.Get());if(!Current())return false;return true;
    }
    // QY: three SetParam roots. Only first A typing and typed B MAKE are added.
    bool BindBinarySet(UEdGraphPin* SourcePin,UEdGraphPin* TargetPin,const UEdGraphSchema_K2* Schema)
    {
        if(!Current())return false;
        auto* Entry=Cast<UK2Node_FunctionEntry>(SourcePin->GetOwningNodeUnchecked());auto* Call=Cast<UK2Node_CallFunction>(TargetPin->GetOwningNodeUnchecked());FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;Int.ContainerType=EPinContainerType::Set;auto Source=SourcePin->PinType;Source.bIsReference=false;Source.bIsConst=false;
        if(!Entry||Entry->GetClass()!=UK2Node_FunctionEntry::StaticClass()||!Call||Call->GetClass()!=UK2Node_CallFunction::StaticClass()||Entry->GetOuter()!=Selected||Call->GetOuter()!=Selected||!BP->FunctionGraphs.Contains(Selected)||!Type(Source,Int)||!TargetPin->LinkedTo.IsEmpty()||TargetPin->ParentPin||!TargetPin->SubPins.IsEmpty()||!BinarySetName(Call->FunctionReference.GetMemberName())||Call->FunctionReference.IsSelfContext())return Bad(TEXT("binary_set_make_profile"),TEXT("own authored int Set producer into unlinked binary Set A or B input"),TEXT("pending"));
        const FName ProducerName=SourcePin->PinName,TargetName=TargetPin->PinName;const FGuid ProducerId=SourcePin->PinId,InputId=TargetPin->PinId;
        SetBinary=true;SetFollowup=TargetName==TEXT("B");if(TargetName!=TEXT("A")&&!SetFollowup)return Bad(TEXT("binary_set_input"),TEXT("A then B input only; Result consumer remains incomplete"),TargetName.ToString());
        SetEntry.Reset(Entry);SetCall.Reset(Call);SetEntryRef=Entry->FunctionReference;SetEntryFlags=Entry->GetExtraFlags();SetName=Call->FunctionReference.GetMemberName();SetSourceType=SourcePin->PinType;SetTargetType=TargetPin->PinType;SetGroup=TEXT("A|B|Result");SetAutoRef.Reset();
        for(const auto& U:Entry->UserDefinedPins){if(!U.IsValid()||SetUsers.Num()>=64)return Bad(TEXT("binary_set_authored_parameters"),TEXT("bounded valid UserPinInfo"),TEXT("unavailable"));SetUsers.Add({U->PinName,U->PinType,U->DesiredPinDirection,U->PinDefaultValue});}
        const auto Authored=[&](FName Name){for(const auto& U:SetUsers){auto T=U.Type;T.bIsReference=false;T.bIsConst=false;if(U.Name==Name&&U.Direction==EGPD_Output&&Type(T,Int))return true;}return false;};if(!Authored(ProducerName))return Bad(TEXT("binary_set_authored_producer"),ProducerName.ToString(),TEXT("not own int Set parameter"));
        SetLibrary.Reset(FindObject<UClass>(nullptr,TEXT("/Script/Engine.BlueprintSetLibrary")));if(!Live(SetLibrary.Get())||SetLibrary->GetClass()!=UClass::StaticClass()||!SetLibrary->ClassDefaultObject.IsResolved())return Bad(TEXT("binary_set_native_library"),TEXT("existing native SetLibrary and resolved nullable CDO"),TEXT("unavailable"));SetCDO.Reset(SetLibrary->ClassDefaultObject.Get());
        for(TFieldIterator<UFunction> It(SetLibrary.Get(),EFieldIterationFlags::None);It;++It)if(It->GetFName()==SetName){SetFunction.Reset(*It);break;}
        if(!Live(SetFunction.Get())||SetFunction->GetClass()!=UFunction::StaticClass()||SetFunction->GetOuter()!=SetLibrary.Get()||!SetFunction->HasAllFunctionFlags(FUNC_Static|FUNC_Native|FUNC_Public|FUNC_BlueprintCallable)||SetFunction->HasAnyFunctionFlags(FUNC_BlueprintPure|FUNC_Delegate|FUNC_MulticastDelegate|FUNC_EditorOnly))return Bad(TEXT("binary_set_native_function"),TEXT("exact native callable nonpure Set Union/Intersection/Difference"),TEXT("unavailable"));SetFunctionPath=SetFunction->GetPathName();SetFunctionFlags=SetFunction->FunctionFlags;
        const FName Names[3]={TEXT("A"),TEXT("B"),TEXT("Result")};for(TFieldIterator<FProperty> It(SetFunction.Get(),EFieldIterationFlags::None);It;++It){auto* P=*It;const int32 Index=SetFields.Num();if(Index>=3||P->GetClass()!=FSetProperty::StaticClass()||P->GetFName()!=Names[Index]||P->GetOwnerStruct()!=SetFunction.Get()||P->GetOwnerProperty()!=P||P->ArrayDim!=1||!P->HasAnyPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm)||P->HasSetterOrGetter()||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()+P->GetSize()>SetFunction->GetStructureSize())return Bad(TEXT("binary_set_native_signature"),TEXT("ordered direct A/B/Result FSetProperty roots"),P->GetName());
            FArrayField E;E.P=P;E.Class=P->GetClass();E.Owner=P->GetOwnerVariant();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();E.Dim=P->ArrayDim;auto* Child=static_cast<FSetProperty*>(P)->ElementProp;
            if(!Child||Child->GetClass()!=FIntProperty::StaticClass()||Child->GetOwnerProperty()!=P||Child->GetOwner<FProperty>()!=P||Child->GetOwnerStruct()!=SetFunction.Get()||Child->ArrayDim!=1||Child->GetSize()!=sizeof(int32)||Child->HasSetterOrGetter())return Bad(TEXT("binary_set_native_element"),E.Name.ToString(),TEXT("requires exact owned int placeholder ElementProp"));E.Inner=Child;E.InnerClass=Child->GetClass();E.InnerOwner=Child->GetOwnerVariant();E.InnerFlags=uint64(Child->GetPropertyFlags());E.InnerSize=Child->GetSize();E.InnerOffset=Child->GetOffset_ForInternal();E.InnerDim=Child->ArrayDim;SetFields.Add(E);}
        if(SetFields.Num()!=3||SetFunction->NumParms!=3||!SetFunction->GetMetaData(TEXT("CustomThunk")).Equals(TEXT("true"),ESearchCase::CaseSensitive)||SetFunction->HasMetaData(TEXT("AutoCreateRefTerm")))return Bad(TEXT("binary_set_native_metadata"),TEXT("three native roots; CustomThunk/SetParam=A|B|Result and no AutoCreateRefTerm"),TEXT("different"));
        SetReady=true;if(!Current())return false;UFunction* Returned=Call->GetTargetFunction();TStrongObjectPtr<UFunction> Immediate(Returned);if(!Current())return false;if(Returned!=SetFunction.Get())return Bad(TEXT("binary_set_call_binding"),SetFunctionPath,TEXT("different"));
        TArray<FPin> Group;for(const auto& G:Rows)for(const auto& N:G.Nodes)if(N.Hold.Get()==Call)for(const auto& P:N.Pins)if(P.Name==Names[0]||P.Name==Names[1]||P.Name==Names[2])Group.Add(P);if(Group.Num()!=3)return Bad(TEXT("binary_set_group"),SetGroup,TEXT("missing or duplicate group"));
        TSet<FName> GroupSeen;for(const auto& P:Group){if(GroupSeen.Contains(P.Name))return Bad(TEXT("binary_set_group"),TEXT("unique A/B/Result pin names"),P.Name.ToString());GroupSeen.Add(P.Name);int32 Index=INDEX_NONE;for(int32 I=0;I<3;++I)if(P.Name==Names[I])Index=I;if(Index==INDEX_NONE)return Bad(TEXT("binary_set_group"),TEXT("canonical group"),TEXT("different"));const auto& NativeField=SetFields[Index];FEdGraphPinType Expected=Int;Expected.PinCategory=SetFollowup?UEdGraphSchema_K2::PC_Int:UEdGraphSchema_K2::PC_Wildcard;Expected.bIsReference=(NativeField.Flags&(CPF_OutParm|CPF_ReferenceParm))==(CPF_OutParm|CPF_ReferenceParm);Expected.bIsConst=(NativeField.Flags&CPF_ConstParm)!=0;const bool Input=!(NativeField.Flags&CPF_OutParm)||(NativeField.Flags&CPF_ReferenceParm);const auto Direction=Input?EGPD_Input:EGPD_Output;
            if(P.Direction!=Direction||!Type(P.Type,Expected)||P.Parent||!P.Children.IsEmpty()||!P.Raw.IsEmpty()||!P.Auto.IsEmpty()||P.Object||!P.Display.IsEmpty())return Bad(TEXT("binary_set_group_shape"),P.Name.ToString(),TEXT("canonical whole empty-default wildcard or int Set endpoint preserving actual native qualifiers"));
            if(SetFollowup&&P.Name==TEXT("A")){if(P.Links.Num()!=1)return Bad(TEXT("binary_set_existing_A"),TEXT("one preserved authored int Set A edge"),TEXT("different count"));const FPin* Peer=nullptr;for(const auto& G:Rows)for(const auto& N:G.Nodes)if(N.Hold.Get()==Entry)for(const auto& Q:N.Pins)if(Q.Address==P.Links[0])Peer=&Q;if(!Peer)return Bad(TEXT("binary_set_existing_A"),TEXT("own same Entry output"),TEXT("different owner"));auto T=Peer->Type;T.bIsReference=false;T.bIsConst=false;if(Peer->Direction!=EGPD_Output||!Type(T,Int)||!Authored(Peer->Name))return Bad(TEXT("binary_set_existing_A"),TEXT("typed own authored int Set source"),TEXT("different"));}
            else if(!P.Links.IsEmpty())return Bad(TEXT("binary_set_group_links"),P.Name.ToString(),TEXT("unlinked except preserved A in followup B"));
            // Conform changes terminal3+wrapper only on first A. B MAKE keeps A/Result byte semantics.
            if(SetFollowup&&P.Name!=TEXT("B"))continue;FSetCell E;E.Type=P.Type;E.Type.PinCategory=UEdGraphSchema_K2::PC_Int;E.Type.PinSubCategory=NAME_None;E.Type.PinSubCategoryObject=nullptr;E.Type.bIsUObjectWrapper=false;E.Raw=P.Raw;E.Object=P.Object;E.Text=P.Display;E.TextFlags=FTextInspector::GetFlags(P.Text);
            TObjectPtr<UObject> Object=nullptr;FText Text;FString Raw;Schema->GetPinDefaultValuesFromString(E.Type,Call,P.Auto,Raw,Object,Text,false);TStrongObjectPtr<UObject> Held(Object.Get());if(!Current())return false;const FString Display=Text.ToString();if(!Current())return false;if(Held.IsValid())Holds.Emplace(Held.Get());E.Raw=Raw;E.Object=Held.Get();E.Text=Display;E.TextFlags=FTextInspector::GetFlags(Text);SetCells.Add(P.Id,MoveTemp(E));}
        BinaryGroup=Group;BinaryGroupCaptured=true;
        // Resolve fresh owning pins after observers. No stale raw endpoint survives a callback.
        SourcePin=Entry->FindPin(ProducerName,EGPD_Output);TargetPin=Call->FindPin(TargetName,EGPD_Input);if(!SourcePin||!TargetPin||SourcePin->PinId!=ProducerId||TargetPin->PinId!=InputId)return Bad(TEXT("binary_set_endpoint_current"),TEXT("same selected endpoint IDs"),TEXT("changed"));SetSchemaCompatible=Schema->ArePinsCompatible(SourcePin,TargetPin,Gen.Get());if(!Current())return false;return true;
    }

    TSharedRef<FJsonObject> SetReceipt()const
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("snapshot_stage"),TEXT("pre_writer_captured_source; final known/postcondition outcome is separate"));J->SetBoolField(TEXT("source_observed"),SetReady);J->SetBoolField(TEXT("endpoint_qualifiers_captured_individually"),SetReady);if(!SetReady)return J;J->SetBoolField(TEXT("are_pins_compatible_pre_writer"),SetSchemaCompatible);J->SetStringField(TEXT("native_function"),SetFunctionPath);J->SetStringField(TEXT("set_param_group"),SetGroup);J->SetBoolField(TEXT("binary_set_operation"),SetBinary);J->SetStringField(TEXT("typing_stage"),SetBinary?(SetFollowup?TEXT("typed_B_make_preserving_A"):TEXT("first_A_make")):TEXT("first_make"));J->SetBoolField(TEXT("native_prototype_element_owner_checked"),true);const auto Q=[](const FEdGraphPinType& T){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("container_type_native"),static_cast<int32>(T.ContainerType));O->SetStringField(TEXT("category_native"),T.PinCategory.ToString());O->SetBoolField(TEXT("is_reference"),T.bIsReference);O->SetBoolField(TEXT("is_const"),T.bIsConst);return O;};J->SetObjectField(TEXT("source_output"),Q(SetSourceType));J->SetObjectField(TEXT("target_input_before"),Q(SetTargetType));if(SetBinary){J->SetBoolField(TEXT("group_captured_before_writer"),BinaryGroupCaptured);TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& P:BinaryGroup){auto V=Q(P.Type);V->SetStringField(TEXT("name"),P.Name.ToString());V->SetStringField(TEXT("id"),P.Id.ToString());V->SetStringField(TEXT("direction"),P.Direction==EGPD_Input?TEXT("input"):TEXT("output"));V->SetNumberField(TEXT("existing_link_count"),P.Links.Num());Pins.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("set_param_group_before"),Pins);TArray<TSharedPtr<FJsonValue>> Fields;for(const auto& F:SetFields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),F.Name.ToString());V->SetStringField(TEXT("native_flags"),LexToString(F.Flags));V->SetStringField(TEXT("element_flags"),LexToString(F.InnerFlags));V->SetNumberField(TEXT("element_size"),F.InnerSize);Fields.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("native_set_roots_before"),Fields);}J->SetBoolField(TEXT("runtime_set_execution_verified"),false);return J;
    }

    bool Bind(UBlueprint* B,UEdGraph* G)
    {
        BP.Reset(B);Editor.Reset(GEditor);World.Reset(GEditor->GetEditorWorldContext().World());BPPath=B->GetPathName();Selected=G;
        if(!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=AActor::StaticClass()||B->BlueprintType!=BPTYPE_Normal||!B->NewVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->Timelines.IsEmpty()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("profile"),TEXT("loaded normal direct Actor without own variables/interfaces/timelines"),TEXT("pending"));
        Gen.Reset(B->GeneratedClass.Get());Skel.Reset(B->SkeletonGeneratedClass.Get());if(!Live(Gen.Get())||!Live(Skel.Get())||!Gen->ClassDefaultObject.IsResolved())return Bad(TEXT("initial_classes"),TEXT("loaded classes/resolved nullable CDO"),TEXT("unavailable"));CDO.Reset(Gen->ClassDefaultObject.Get());GenPath=Gen->GetPathName();SkelPath=Skel->GetPathName();CDOPath=CDO.IsValid()?CDO->GetPathName():FString();Roles=Categories();B->GetAllGraphs(All);for(auto* V:All)Holds.Emplace(V);if(!All.Contains(G)||All.Num()>64)return Bad(TEXT("graph_selection"),TEXT("bounded own existing graph"),TEXT("different"));if(!Current()||!Capture(Rows))return false;Initialized=true;if(!Texts(Rows))return false;return Current();
    }
    bool Plan(UEdGraphPin* A,UEdGraphPin* B,const FPinConnectionResponse& Decision,bool Allow,const UEdGraphSchema_K2* Schema)
    {
        if(!Current())return false;
        const auto Scalar=[](const FEdGraphPinType& T){return T.ContainerType==EPinContainerType::None&&!T.bIsReference&&!T.bIsUObjectWrapper&&(T.PinCategory==UEdGraphSchema_K2::PC_Int||T.PinCategory==UEdGraphSchema_K2::PC_Int64||T.PinCategory==UEdGraphSchema_K2::PC_Real||T.PinCategory==UEdGraphSchema_K2::PC_Boolean||T.PinCategory==UEdGraphSchema_K2::PC_Name||T.PinCategory==UEdGraphSchema_K2::PC_String);};
        const auto Exec=[](const FEdGraphPinType& T){return T.PinCategory==UEdGraphSchema_K2::PC_Exec&&T.ContainerType==EPinContainerType::None&&!T.bIsReference&&!T.bIsUObjectWrapper;};
        ConversionRoute=A&&B&&Allow&&Decision.Response==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE&&Scalar(A->PinType)&&Scalar(B->PinType)&&B->LinkedTo.IsEmpty();
        if(!A||!B||A->Direction!=EGPD_Output||B->Direction!=EGPD_Input||(!SetRoute&&!Scalar(A->PinType)&&!(ExecRoute&&Exec(A->PinType)&&Exec(B->PinType))&&!(ArrayRoute&&ArrayElement(A->PinType,B->PinType)))||(!ArrayRoute&&!SetRoute&&!ConversionRoute&&!Type(A->PinType,B->PinType))||!A->SubPins.IsEmpty()||!B->SubPins.IsEmpty()||A->ParentPin||B->ParentPin)return Bad(TEXT("whole_connection_profile"),TEXT("same whole admitted scalar, canonical exec or same-element primitive Array output/input"),TEXT("pending"));
        if(ConversionRoute&&!BindConversion(A,B,Schema))return false;
        if(ArrayRoute&&!BindArray(A,B,Schema))return false;
        if(SetRoute){if(!Allow||Decision.Response!=CONNECT_RESPONSE_MAKE||!B->LinkedTo.IsEmpty())return Bad(TEXT("set_first_make_only"),TEXT("original11 allow replacement True; first MAKE into unlinked wildcard SetParam"),TEXT("pending"));if(!BindSet(A,B,Schema))return false;}
        UEdGraphPin* OldReset=nullptr;const bool Already=A->LinkedTo.Contains(B)&&B->LinkedTo.Contains(A);Response=Decision.Response;
        const bool Replacement=Allow&&(ExecRoute?(Response==CONNECT_RESPONSE_BREAK_OTHERS_A&&A->LinkedTo.Num()==1):(Response==CONNECT_RESPONSE_BREAK_OTHERS_B&&B->LinkedTo.Num()==1));
        if(!Already&&Response!=CONNECT_RESPONSE_MAKE&&!Replacement&&!ConversionRoute)return Bad(TEXT("schema_profile"),ExecRoute?TEXT("MAKE or permitted BREAK_OTHERS_A output old1"):TEXT("MAKE or permitted BREAK_OTHERS_B input old1"),Connection::SchemaResponseName(Response));
        SourceNodeId=A->GetOwningNodeUnchecked()->NodeGuid;TargetNodeId=B->GetOwningNodeUnchecked()->NodeGuid;SourceId=A->PinId;TargetId=B->PinId;
        for(const auto& G:Rows)for(const auto& N:G.Nodes)for(const auto& P:N.Pins){PlannedLinks.Add(P.Id,LinkIds(P,Rows));PinOwners.Add(P.Id,N.Id);}
        if(!Already&&ExecRoute&&Replacement)
        {
            // Current inventory has already fixed every linked pin and reciprocal owner.
            // Retain every other predecessor of old/new inputs; only the selected output's old edge is removed.
            auto* Old=A->LinkedTo[0];if(Old==B||Old->Direction!=EGPD_Input||!Type(Old->PinType,A->PinType)||Old->GetOwningNodeUnchecked()->GetOuter()!=Selected)return Bad(TEXT("old_exec_target"),TEXT("distinct own same-graph canonical exec input"),TEXT("different"));
            OldNodeId=Old->GetOwningNodeUnchecked()->NodeGuid;OldId=Old->PinId;
            PlannedLinks.FindChecked(OldId).Remove(SourceId);PlannedLinks.FindChecked(SourceId).Reset();
            if(!PlannedLinks.FindChecked(OldId).IsEmpty())OldReset=Old;
        }
        else if(!Already&&!ExecRoute&&B->LinkedTo.Num()==1)
        {
            auto* Old=B->LinkedTo[0];if(Old==A||Old->Direction!=EGPD_Output||(ArrayRoute?!ArrayElement(Old->PinType,B->PinType):!Type(Old->PinType,B->PinType)))return Bad(TEXT("old_source"),TEXT("distinct same-element scalar/Array oldSource1"),TEXT("different"));OldNodeId=Old->GetOwningNodeUnchecked()->NodeGuid;OldId=Old->PinId;PlannedLinks.FindChecked(OldId).Remove(TargetId);PlannedLinks.FindChecked(TargetId).Reset();
        }
        if(!Already)
        {
            if(!ConversionRoute){PlannedLinks.FindChecked(SourceId).Add(TargetId);PlannedLinks.FindChecked(TargetId).Add(SourceId);}
            // BreakAllPinLinks notifies the old input too. A remaining predecessor resets that input to Auto.
            // Plan both actual affected input cells before the writer; unrelated cells remain fixed.
            const auto PlanInput=[&](UEdGraphPin* Input,bool IsOld)
            {
                const FEdGraphPinType T=Input->PinType;const FString Auto=Input->AutogeneratedDefaultValue;TStrongObjectPtr<UEdGraphNode> Owner(Input->GetOwningNodeUnchecked());TObjectPtr<UObject> ParsedObject=nullptr;FText ParsedText;FString ParsedRaw;
                Schema->GetPinDefaultValuesFromString(T,Owner.Get(),Auto,ParsedRaw,ParsedObject,ParsedText,false);TStrongObjectPtr<UObject> ImmediateDefault(ParsedObject.Get());if(!Current())return false;
                const FString Display=ParsedText.ToString();if(!Current())return false;if(ImmediateDefault.IsValid())Holds.Emplace(ImmediateDefault.Get());
                if(IsOld){OldPlannedRaw=ParsedRaw;OldPlannedObject=ImmediateDefault.Get();OldPlannedText=Display;OldPlannedTextFlags=FTextInspector::GetFlags(ParsedText);OldPlannedDefault=true;}
                else{PlannedRaw=ParsedRaw;PlannedObject=ImmediateDefault.Get();PlannedText=Display;PlannedTextFlags=FTextInspector::GetFlags(ParsedText);PlannedDefault=true;}return true;
            };
            if(!SetRoute&&(!PlanInput(B,false)||(OldReset&&!PlanInput(OldReset,true))))return false;
        }
        LinkPlanned=true;return true;
    }
};

struct FWork
{
    FGraphPinsLease* GraphPins=nullptr;FResult* Delivered=nullptr;FResult R;TSharedPtr<FJsonObject> Legacy=MakeShared<FJsonObject>();TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Prefix;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UBlueprint> BP;TWeakObjectPtr<UClass> Generated,Skeleton;TWeakObjectPtr<UEdGraph> Selected;
    TArray<TUniquePtr<FObjectIdentity>> Holds;TArray<TArray<UEdGraph*>> Roles;TArray<FGraphValue> Graphs;bool SDKEntered=false;bool AllowSkeletonTransition=false;bool AllowLinkRemoval=false;ECanCreateConnectionResponse PlannedResponse=CONNECT_RESPONSE_MAKE;bool DirtyBefore=false;int32 StatusBefore=0;bool CacheBefore=false;
    TWeakObjectPtr<UEdGraphNode> SourceNode,TargetNode;FGuid SourcePinId,TargetPinId;FString SourceName,TargetName;
    FWork():Editor(GlobalSafe()?GEditor:nullptr),World(GlobalSafe()?GEditor->GetEditorWorldContext().World():nullptr)
    {R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);}
    bool Fail(const TCHAR* Code,const FString& Error){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Error;}R.bSuccess=R.bPostconditionPassed=false;if(SDKEntered||R.bMutationAttempted)R.bChangeStateKnown=false;return false;}
    bool Base(const FString& Stage)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty())return false;if(GraphPins&&!GraphPins->Current())return Fail(TEXT("blueprint_graph_pins_current_unknown"),TEXT("GraphPins request-long roles changed; no follow-up SDK."));
        if(!GlobalSafe())return Fail(TEXT("blueprint_connection_context_unavailable"),TEXT("Safe Editor GT outside GC/exit/save required; no further object access or retry."));
        auto* CurrentWorld=GEditor->GetEditorWorldContext().World();if(Editor.Get()!=GEditor||!Live(CurrentWorld)||World.Get()!=CurrentWorld||CurrentWorld->bIsTearingDown)return Fail(TEXT("blueprint_connection_context_changed"),TEXT("Editor/world identity changed across callback."));
        if(GEditor->PlayWorld||GEditor->bIsSimulatingInEditor)return Fail(TEXT("blueprint_connection_play_context_unsupported"),TEXT("Graph writers require idle non-PIE Editor context."));
        for(const auto& Identity:Holds)if(!Identity->Exact())return Fail(TEXT("blueprint_connection_identity_changed"),TEXT("Captured object class/outer/effective package/path changed; plain prefix retained."));
        if(auto* B=BP.Get()){if(B->GeneratedClass!=Generated.Get()||(!AllowSkeletonTransition&&B->SkeletonGeneratedClass!=Skeleton.Get())||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad)return Fail(TEXT("blueprint_connection_blueprint_changed"),TEXT("Current Blueprint generated/skeleton class or compilation role changed."));}
        return true;
    }
    bool Pin(UObject* O){if(!Base(TEXT("pin_connection_object")))return false;if(!Live(O))return Fail(TEXT("blueprint_connection_object_unavailable"),TEXT("Expected current loaded object unavailable."));for(const auto& Identity:Holds)if(Identity->Hold.Get()==O&&!Identity->Retired)return true;if(Holds.Num()>=65536)return Fail(TEXT("blueprint_connection_object_bound"),TEXT("Captured object bound exceeded."));Holds.Add(MakeUnique<FObjectIdentity>(O));return true;}
    TArray<TArray<UEdGraph*>> CurrentRoles()const
    {
        TArray<TArray<UEdGraph*>> Result;auto Capture=[&](const auto& A){TArray<UEdGraph*> Row;for(const auto& Entry:A)Row.Add(Entry.Get());Result.Add(MoveTemp(Row));};auto* B=BP.Get();Capture(B->FunctionGraphs);Capture(B->MacroGraphs);Capture(B->UbergraphPages);Capture(B->EventGraphs);Capture(B->DelegateSignatureGraphs);for(const auto& Interface:B->ImplementedInterfaces)Capture(Interface.Graphs);return Result;
    }
    bool Bind(UBlueprint* B)
    {
        if(!Pin(B)||!Pin(B->GetOutermost()))return false;BP=B;Generated=B->GeneratedClass;Skeleton=B->SkeletonGeneratedClass;
        if(Generated.IsValid()&&!Pin(Generated.Get()))return false;if(Skeleton.IsValid()&&!Pin(Skeleton.Get()))return false;Roles=CurrentRoles();DirtyBefore=B->GetOutermost()->IsDirty();StatusBefore=static_cast<int32>(B->Status);CacheBefore=B->bCachedDependenciesUpToDate;return Base(TEXT("blueprint_connection_bound"));
    }
    void Count(const FString& Name){double N=0;Counts->TryGetNumberField(Name,N);Counts->SetNumberField(Name,N+1);}
    bool Event(const FString& Name,bool Writer=false)
    {
        if(!Admit(Name))return false;Count(Name);SDKEntered=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
        if(Writer){R.bMutationAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Name);Row->SetBoolField(TEXT("entered"),true);Row->SetBoolField(TEXT("returned"),false);Prefix.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("actual_mutation_prefix"),Prefix);}return true;
    }
    void Returned(){if(!Prefix.IsEmpty())Prefix.Last()->AsObject()->SetBoolField(TEXT("returned"),true);}
    bool Capture(UEdGraph* G,FGraphValue& Out)
    {
        if(!Base(TEXT("capture_connection_graph"))||!Pin(G))return false;auto* B=BP.Get();if(!G->IsIn(B)||G->GetOutermost()!=B->GetOutermost()||G->GetPackage()!=B->GetPackage())return Fail(TEXT("blueprint_connection_graph_owner_invalid"),TEXT("Graph must be contained by exact target Blueprint in the same outer/effective package."));
        Out.Graph=G;Out.Id=G->GraphGuid;Out.SchemaClass=G->Schema.Get();for(const auto& Sub:G->SubGraphs)Out.SubGraphs.Add(Sub.Get());if(G->Nodes.Num()>16384)return Fail(TEXT("blueprint_connection_graph_bound"),TEXT("Graph node bound exceeded."));
        auto Shape=MakeShared<FJsonObject>();Shape->SetStringField(TEXT("id"),G->GraphGuid.ToString());Shape->SetStringField(TEXT("schema"),G->Schema?G->Schema->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> NodeRows;TSet<const UEdGraphPin*> LivePins;int32 PinCount=0;
        for(const auto& Entry:G->Nodes){auto* N=Entry.Get();if(!Live(N)||N->GetGraph()!=G||N->GetOuter()!=G||N->GetPackage()!=B->GetPackage()||!Pin(N))return Fail(TEXT("blueprint_connection_node_owner_invalid"),TEXT("Live exact graph-owned node required before pin access."));for(auto* P:N->Pins){if(++PinCount>65536||!P||LivePins.Contains(P)||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N)return Fail(TEXT("blueprint_connection_pin_membership_invalid"),TEXT("Null/retired/foreign/duplicate pin in current node array."));LivePins.Add(P);}}
        int32 LinkCount=0;for(const auto& Entry:G->Nodes){auto* N=Entry.Get();FNodeValue NV;NV.Node=N;NV.Id=N->NodeGuid;auto NJ=MakeShared<FJsonObject>();NJ->SetStringField(TEXT("path"),N->GetPathName());NJ->SetStringField(TEXT("class"),N->GetClass()->GetPathName());NJ->SetStringField(TEXT("id"),N->NodeGuid.ToString());NJ->SetNumberField(TEXT("x"),N->NodePosX);NJ->SetNumberField(TEXT("y"),N->NodePosY);NJ->SetStringField(TEXT("comment"),N->NodeComment);if(auto* FunctionNode=Cast<UK2Node_CallFunction>(N)){const auto& Ref=FunctionNode->FunctionReference;NJ->SetStringField(TEXT("function_member"),Ref.GetMemberName().ToString());NJ->SetStringField(TEXT("function_guid"),Ref.GetMemberGuid().ToString());NJ->SetBoolField(TEXT("function_self"),Ref.IsSelfContext());NJ->SetStringField(TEXT("function_scope"),Ref.GetMemberScopeName());if(auto* ParentClass=Ref.GetMemberParentClass()){if(!Pin(ParentClass))return false;NJ->SetStringField(TEXT("function_parent_class"),ParentClass->GetPathName());}if(auto* ParentPackage=Ref.GetMemberParentPackage()){if(!Pin(ParentPackage))return false;NJ->SetStringField(TEXT("function_parent_package"),ParentPackage->GetPathName());}}NV.Header=Json(NJ);TArray<TSharedPtr<FJsonValue>> PinRows;
            for(auto* P:N->Pins){FPinValue PV;PV.Address=P;PV.Owner=N;PV.Id=P->PinId;PV.Persistent=P->PersistentGuid;PV.Name=P->PinName;PV.Direction=P->Direction;PV.Type=P->PinType;PV.Default=P->DefaultValue;PV.AutoDefault=P->AutogeneratedDefaultValue;PV.Text=P->DefaultTextValue.ToString();if(GraphPins&&!GraphPins->Current())return false;PV.DefaultObject=P->DefaultObject.Get();PV.Orphan=P->bOrphanedPin;PV.NotConnectable=P->bNotConnectable;PV.ReadOnly=P->bDefaultValueIsReadOnly;PV.Ignored=P->bDefaultValueIsIgnored;PV.Links=P->LinkedTo;
                for(UObject* Ref:{P->PinType.PinSubCategoryObject.Get(),P->PinType.PinValueType.TerminalSubCategoryObject.Get(),P->DefaultObject.Get()})if(Ref&&!Pin(Ref))return false;
                auto PJ=MakeShared<FJsonObject>();PJ->SetStringField(TEXT("id"),PV.Id.ToString());PJ->SetStringField(TEXT("persistent"),PV.Persistent.ToString());PJ->SetStringField(TEXT("name"),P->PinName.ToString());PJ->SetStringField(TEXT("direction"),P->Direction==EGPD_Output?TEXT("output"):TEXT("input"));PJ->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());PJ->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());PJ->SetBoolField(TEXT("is_array"),P->PinType.IsArray());auto PT=MakeShared<FJsonObject>();PT->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());PT->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());PT->SetBoolField(TEXT("is_array"),P->PinType.IsArray());if(auto* O=P->PinType.PinSubCategoryObject.Get()){PJ->SetStringField(TEXT("subcategory_object"),O->GetPathName());PT->SetStringField(TEXT("subcategory_object"),O->GetPathName());}PJ->SetObjectField(TEXT("pin_type"),PT);PJ->SetStringField(TEXT("default_value"),PV.Default);if(auto* O=P->DefaultObject.Get())PJ->SetStringField(TEXT("default_object"),O->GetPathName());
                TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P->LinkedTo){if(++LinkCount>262144||!LivePins.Contains(L))return Fail(TEXT("blueprint_connection_link_membership_invalid"),TEXT("Linked raw pin is outside current captured graph membership; it was not dereferenced."));auto* LN=L->GetOwningNodeUnchecked();if(!LN||!LN->Pins.Contains(L)||!L->LinkedTo.Contains(P))return Fail(TEXT("blueprint_connection_link_reciprocity_invalid"),TEXT("Current physical link is not reciprocal."));auto LJ=MakeShared<FJsonObject>();LJ->SetStringField(TEXT("node_id"),LN->NodeGuid.ToString());LJ->SetStringField(TEXT("node_name"),LN->GetName());LJ->SetStringField(TEXT("pin_name"),L->PinName.ToString());Links.Add(MakeShared<FJsonValueObject>(LJ));}PJ->SetArrayField(TEXT("linked_to"),Links);
                PV.Legacy=MakeShared<FJsonObject>(*PJ);PV.Legacy->RemoveField(TEXT("id"));PV.Legacy->RemoveField(TEXT("persistent"));PJ->SetStringField(TEXT("type_export"),TypeString(PV.Type));if(GraphPins&&!GraphPins->Current())return false;PJ->SetStringField(TEXT("autodefault"),PV.AutoDefault);PJ->SetStringField(TEXT("text"),PV.Text);PJ->SetBoolField(TEXT("orphan"),PV.Orphan);PJ->SetBoolField(TEXT("not_connectable"),PV.NotConnectable);PJ->SetBoolField(TEXT("readonly"),PV.ReadOnly);PJ->SetBoolField(TEXT("ignored"),PV.Ignored);PinRows.Add(MakeShared<FJsonValueObject>(PJ));NV.Pins.Add(MoveTemp(PV));}
            NJ->SetArrayField(TEXT("pins"),PinRows);NV.Plain=Json(NJ);NodeRows.Add(MakeShared<FJsonValueObject>(NJ));Out.Nodes.Add(MoveTemp(NV));}
        Shape->SetArrayField(TEXT("nodes"),NodeRows);Out.Plain=Json(Shape);if(Out.Plain.Len()>8*1024*1024)return Fail(TEXT("blueprint_connection_snapshot_bound"),TEXT("Unique graph snapshot exceeds bounded internal value budget."));return true;
    }
    static FString TypeString(const FEdGraphPinType& T)
    {FString S;FEdGraphPinType::StaticStruct()->ExportText(S,&T,nullptr,nullptr,PPF_None,nullptr);return S;}
    static bool SamePhysical(const FGraphValue& A,const FGraphValue& B)
    {if(A.Graph!=B.Graph||A.SubGraphs!=B.SubGraphs||A.Nodes.Num()!=B.Nodes.Num())return false;for(int32 I=0;I<A.Nodes.Num();++I){if(A.Nodes[I].Node!=B.Nodes[I].Node||A.Nodes[I].Pins.Num()!=B.Nodes[I].Pins.Num())return false;for(int32 J=0;J<A.Nodes[I].Pins.Num();++J)if(A.Nodes[I].Pins[J].Address!=B.Nodes[I].Pins[J].Address)return false;}return true;}
    bool PreserveSelected(const FGraphValue& Old,const FGraphValue& Current)
    {
        TMap<UEdGraphPin*,const FPinValue*> OldByAddress,NewByAddress;TMap<UEdGraphPin*,const FPinValue*> OldToCurrent,NewToOld;TSet<const UEdGraphNode*> OldNodes;
        if(Old.Id!=Current.Id||Old.SchemaClass!=Current.SchemaClass||Old.SubGraphs!=Current.SubGraphs)return Fail(TEXT("blueprint_connection_graph_identity_changed"),TEXT("Graph GUID/schema/subgraph roles changed during connection callback."));for(const auto& N:Old.Nodes){OldNodes.Add(N.Node.Get());for(const auto& P:N.Pins)OldByAddress.Add(P.Address,&P);}
        for(const auto& N:Current.Nodes)for(const auto& P:N.Pins)NewByAddress.Add(P.Address,&P);
        int32 LastNode=-1;
        for(const auto& ON:Old.Nodes)
        {
            const FNodeValue* NN=nullptr;int32 Found=-1;for(int32 I=0;I<Current.Nodes.Num();++I)if(Current.Nodes[I].Node==ON.Node&&Current.Nodes[I].Id==ON.Id){NN=&Current.Nodes[I];Found=I;break;}
            if(!NN||Found<=LastNode)return Fail(TEXT("blueprint_connection_existing_node_retired"),TEXT("K2 changed the identity/order of an existing node; no old pin was dereferenced or retried."));LastNode=Found;if(ON.Header!=NN->Header)return Fail(TEXT("blueprint_connection_unrelated_node_header_changed"),TEXT("Existing node position/comment/direct function binding changed during callback."));
            if(ON.Pins.Num()!=NN->Pins.Num())return Fail(TEXT("blueprint_connection_pin_shape_incomplete"),TEXT("Existing-node pin count changed beyond the supported exact-ID reconstruction; actual partial retained."));
            for(int32 I=0;I<ON.Pins.Num();++I)
            {
                const auto& OP=ON.Pins[I];const auto& NP=NN->Pins[I];const bool Endpoint=(ON.Node==SourceNode&&OP.Id==SourcePinId)||(ON.Node==TargetNode&&OP.Id==TargetPinId);
                if(OP.Id!=NP.Id||OP.Persistent!=NP.Persistent||!OP.Name.ToString().Equals(NP.Name.ToString(),ESearchCase::CaseSensitive)||OP.Direction!=NP.Direction||OP.Orphan!=NP.Orphan||OP.NotConnectable!=NP.NotConnectable||OP.ReadOnly!=NP.ReadOnly||OP.Ignored!=NP.Ignored)return Fail(TEXT("blueprint_connection_pin_identity_changed"),TEXT("Existing current pin ID/name/direction/persistent ID/flags changed outside exact supported reconstruction."));
                const bool TypeTransition=(OP.Type.PinCategory==UEdGraphSchema_K2::PC_Wildcard||NP.Type.PinCategory==UEdGraphSchema_K2::PC_Wildcard||((ON.Node==SourceNode||ON.Node==TargetNode)&&PlannedResponse==CONNECT_RESPONSE_MAKE_WITH_PROMOTION));
                if(OP.Type!=NP.Type&&!TypeTransition)return Fail(TEXT("blueprint_connection_unrelated_type_changed"),TEXT("Callback changed a non-wildcard/non-promotion pin type outside the requested endpoints."));
                if(!Endpoint&&!(GraphPins&&GraphPins->SetRoute&&GraphPins->SetCells.Contains(OP.Id))&&(OP.Default!=NP.Default||OP.AutoDefault!=NP.AutoDefault||OP.Text!=NP.Text||OP.DefaultObject!=NP.DefaultObject))return Fail(TEXT("blueprint_connection_unrelated_default_changed"),TEXT("Callback changed an unrelated pin default/object/text; actual partial retained."));
                OldToCurrent.Add(OP.Address,&NP);NewToOld.Add(NP.Address,&OP);
            }
        }
        auto IsEndpoint=[&](const FPinValue& P,bool Source){return P.Owner==(Source?SourceNode:TargetNode)&&P.Id==(Source?SourcePinId:TargetPinId);};
        auto MayRemove=[&](const FPinValue& P){return AllowLinkRemoval&&((IsEndpoint(P,true)&&(PlannedResponse==CONNECT_RESPONSE_BREAK_OTHERS_A||PlannedResponse==CONNECT_RESPONSE_BREAK_OTHERS_AB))||(IsEndpoint(P,false)&&(PlannedResponse==CONNECT_RESPONSE_BREAK_OTHERS_B||PlannedResponse==CONNECT_RESPONSE_BREAK_OTHERS_AB||PlannedResponse==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)));};
        for(const auto& ON:Old.Nodes)for(const auto& OP:ON.Pins)
        {
            const FPinValue* const* CurrentEntry=OldToCurrent.Find(OP.Address);if(!CurrentEntry)return Fail(TEXT("blueprint_connection_pin_mapping_incomplete"),TEXT("Exact old-to-current value mapping incomplete."));const auto& NP=**CurrentEntry;
            TArray<UEdGraphPin*> RetainedExpected,RetainedActual;
            for(auto* OldLink:OP.Links){const FPinValue* const* LinkedOld=OldByAddress.Find(OldLink);if(!LinkedOld)return Fail(TEXT("blueprint_connection_old_link_unknown"),TEXT("Old physical link was not in the captured value inventory."));if(MayRemove(OP)||MayRemove(**LinkedOld))continue;const FPinValue* const* Mapped=OldToCurrent.Find(OldLink);if(!Mapped)return Fail(TEXT("blueprint_connection_retained_link_lost"),TEXT("Retained link endpoint was retired."));RetainedExpected.Add((*Mapped)->Address);}
            for(auto* NewLink:NP.Links)
            {
                const FPinValue* const* LinkedNew=NewByAddress.Find(NewLink);if(!LinkedNew)return Fail(TEXT("blueprint_connection_current_link_unknown"),TEXT("Current link is outside fresh inventory."));
                const FPinValue* OldCounterpart=NewToOld.FindRef(NewLink);
                if(OldCounterpart&&OP.Links.Contains(OldCounterpart->Address)){if(!MayRemove(OP)&&!MayRemove(*OldCounterpart))RetainedActual.Add(NewLink);continue;}
                const bool Direct=(IsEndpoint(NP,true)&&IsEndpoint(**LinkedNew,false))||(IsEndpoint(NP,false)&&IsEndpoint(**LinkedNew,true));
                bool Conversion=false;if(PlannedResponse==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE&&!OldCounterpart){Conversion=!OldNodes.Contains((*LinkedNew)->Owner.Get())&&(IsEndpoint(NP,true)||IsEndpoint(NP,false));}
                if(!Direct&&!Conversion)return Fail(TEXT("blueprint_connection_unrelated_link_added"),TEXT("Callback added a link outside requested endpoints/new conversion node; actual partial retained."));
            }
            if(RetainedExpected!=RetainedActual)return Fail(TEXT("blueprint_connection_retained_links_changed"),TEXT("Unrelated existing ordered reciprocal links were removed/reordered by callback; no baseline re-adoption."));
        }
        return true;
    }
    bool Admit(const FString& Stage)
    {
        if(!Base(Stage))return false;if(BP.IsValid()&&Roles!=CurrentRoles())return Fail(TEXT("blueprint_connection_graph_roles_changed"),TEXT("Blueprint root/interface graph physical roles changed across callback."));
        for(const auto& Expected:Graphs){auto* G=Expected.Graph.Get();FGraphValue Current;if(!Capture(G,Current))return false;if(Current.Plain!=Expected.Plain||!SamePhysical(Expected,Current))return Fail(TEXT("blueprint_connection_topology_changed"),TEXT("Captured graph/pin/value/link state changed before the intended SDK transition."));}return true;
    }
    bool CaptureAll()
    {
        if(!Event(TEXT("GetAllGraphs")))return false;TArray<UEdGraph*> All;BP->GetAllGraphs(All);if(!Admit(TEXT("after_GetAllGraphs")))return false;if(All.Num()>16384)return Fail(TEXT("blueprint_connection_graph_bound"),TEXT("Blueprint graph count bound exceeded."));int64 TotalText=0;TArray<FGraphValue> Captured;for(auto* G:All){if(!G)continue;FGraphValue Row;if(!Capture(G,Row))return false;TotalText+=Row.Plain.Len();if(TotalText>16*1024*1024)return Fail(TEXT("blueprint_connection_snapshot_total_bound"),TEXT("Total internal graph value budget exceeded before writer."));Captured.Add(MoveTemp(Row));}Graphs=MoveTemp(Captured);return true;
    }
    bool After(const FString& Stage,bool AllowSelected=false)
    {
        if(!Base(FString(TEXT("after_"))+Stage))return false;if(Roles!=CurrentRoles())return Fail(TEXT("blueprint_connection_graph_roles_changed"),TEXT("Root/interface graph roles changed outside an exact declared graph append."));if(BP.IsValid()){auto* B=BP.Get();const bool Dirty=B->GetOutermost()->IsDirty();R.bChanged|=Dirty!=DirtyBefore||static_cast<int32>(B->Status)!=StatusBefore||B->bCachedDependenciesUpToDate!=CacheBefore;R.Data->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("package_dirty_after"),Dirty);}
        for(auto& Expected:Graphs){FGraphValue Current;if(!Capture(Expected.Graph.Get(),Current))return false;if(AllowSelected&&Expected.Graph==Selected){R.bChanged|=Current.Plain!=Expected.Plain||!SamePhysical(Expected,Current);if(!PreserveSelected(Expected,Current))return false;Expected=MoveTemp(Current);}else if(Current.Plain!=Expected.Plain||!SamePhysical(Expected,Current))return Fail(TEXT("blueprint_connection_unrelated_graph_changed"),TEXT("SDK changed a graph outside the selected transition; actual prefix retained."));}
        return true;
    }
    UEdGraphNode* FindNode(UEdGraph* G,const FString& Id){for(const auto& Entry:G->Nodes)if(Entry&&Entry->NodeGuid.ToString()==Id)return Entry.Get();return nullptr;}
    UEdGraphPin* FindPin(UEdGraphNode* N,const FString& Name,EEdGraphPinDirection Direction){for(auto* P:N->Pins)if(P&&P->PinName.ToString()==Name&&P->Direction==Direction)return P;for(auto* P:N->Pins)if(P&&P->PinName.ToString().Equals(Name,ESearchCase::IgnoreCase)&&P->Direction==Direction)return P;if(Direction==EGPD_Output&&Cast<UK2Node_VariableGet>(N))for(auto* P:N->Pins)if(P&&P->Direction==EGPD_Output&&P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec)return P;return nullptr;}
    UEdGraphPin* FindCurrentPin(UEdGraphNode* N,const FString& Name,EEdGraphPinDirection Direction){for(auto* P:N->Pins)if(P&&P->PinName.ToString().Equals(Name,ESearchCase::CaseSensitive)&&P->Direction==Direction)return P;return nullptr;}
    const FPinValue* Value(UEdGraphPin* P)const{for(const auto& G:Graphs)for(const auto& N:G.Nodes)for(const auto& V:N.Pins)if(V.Address==P)return &V;return nullptr;}
    bool CurrentPins(UEdGraphPin*& A,UEdGraphPin*& B)
    {
        if(!Admit(TEXT("reacquire_connection_pins")))return false;auto* G=Selected.Get();auto* SN=SourceNode.Get();auto* TN=TargetNode.Get();if(!Live(SN)||!Live(TN)||!G->Nodes.Contains(SN)||!G->Nodes.Contains(TN))return Fail(TEXT("blueprint_connection_selected_node_lost"),TEXT("Original selected node identities are not current members."));A=FindCurrentPin(SN,SourceName,EGPD_Output);B=FindCurrentPin(TN,TargetName,EGPD_Input);if(!A||!B||A->PinId!=SourcePinId||B->PinId!=TargetPinId)return Fail(TEXT("blueprint_connection_selected_pin_lost"),TEXT("Current endpoint name/direction/PinId differs; no replacement pin was silently adopted."));return true;
    }
    bool RuntimeRefs()
    {
        // Direct, typed references only. String/Text defaults are not object references.
        TArray<TWeakObjectPtr<UObject>> References;
        for(const auto& G:Graphs)for(const auto& N:G.Nodes){auto* Node=N.Node.Get();if(ForbiddenClassPath(Node->GetClass()))return Fail(TEXT("blueprint_connection_runtime_dependency_forbidden"),TEXT("Graph node class is an MCP dependency."));for(const auto& P:N.Pins)for(UObject* O:{P.Type.PinSubCategoryObject.Get(),P.Type.PinValueType.TerminalSubCategoryObject.Get(),P.DefaultObject.Get()})if(O)References.AddUnique(O);}
        for(const auto& Reference:References){auto* O=Reference.Get();if(!Admit(TEXT("runtime_reference_preflight")))return false;if(ForbiddenClassPath(O))return Fail(TEXT("blueprint_connection_runtime_dependency_forbidden"),TEXT("Typed pin object/class/default references MCP; string literals are unaffected."));if(!Event(TEXT("Reference_IsEditorOnly")))return false;const bool EditorOnly=O->IsEditorOnly();R.Data->SetBoolField(TEXT("last_reference_is_editor_only"),EditorOnly);if(!After(TEXT("Reference_IsEditorOnly")))return false;if(EditorOnly)return Fail(TEXT("blueprint_connection_editor_dependency_forbidden"),TEXT("Typed actual reference is Editor-only."));}return true;
    }
    FString Encode(const TCHAR* Operation)
    {
        R.Data->SetBoolField(TEXT("read_only_direct_API"),FString(Operation)==TEXT("resolve_blueprint"));
        R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),R.bChangeStateKnown);R.Data->SetArrayField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("explicit_compile_save_calls"),false);R.Data->SetBoolField(TEXT("do_not_retry"),R.bMutationAttempted||!R.bChangeStateKnown);
        if(!R.Error.IsEmpty()){Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error);}if(Delivered)*Delivered=R;return Operations::EncodeVariableResult(R,Operation);
    }
};
FString NormalizedType(FString S){S=S.ToLower();S.ReplaceInline(TEXT(" "),TEXT(""));S.ReplaceInline(TEXT("-"),TEXT("_"));if(S==TEXT("eventgraph")||S==TEXT("event_graph")||S==TEXT("ubergraph")||S==TEXT("uber_graph"))return TEXT("event");if(S==TEXT("functiongraph")||S==TEXT("function_graph"))return TEXT("function");if(S==TEXT("macrograph")||S==TEXT("macro_graph"))return TEXT("macro");return S;}
}
FString Resolve(const FString& ParamsJson)
{
    constexpr const TCHAR* Operation=TEXT("resolve_blueprint");TSharedPtr<FJsonObject> P;FString Name;
    if(!Parse(ParamsJson,P)){FWork W;W.Fail(TEXT("blueprint_resolution_invalid_params_json"),TEXT("Expected finite bounded JSON object."));return W.Encode(Operation);}
    if(!P->TryGetStringField(TEXT("blueprint_name"),Name)){FWork W;W.Fail(TEXT("blueprint_resolution_missing_name"),TEXT("Missing 'blueprint_name' parameter"));return W.Encode(Operation);}
    if(!GlobalSafe()||Running||GraphPinsRunning){FWork W;W.Fail(TEXT("blueprint_resolution_context_unavailable"),TEXT("Nonreentrant safe Editor context required."));return W.Encode(Operation);}TGuardValue<bool> Scope(Running,true);
    auto R=BlueprintTopology::ResolveBlueprintLegacy(Name);R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("operation_complete"),true);if(!R.Data->HasTypedField<EJson::Object>(TEXT("legacy_result"))){auto Error=MakeShared<FJsonObject>();Error->SetBoolField(TEXT("success"),false);Error->SetStringField(TEXT("error"),R.Error);R.Data->SetObjectField(TEXT("legacy_result"),Error);}if(!R.Error.IsEmpty()){auto Error=R.Data->GetObjectField(TEXT("legacy_result"));Error->SetBoolField(TEXT("success"),false);Error->SetStringField(TEXT("error"),R.Error);}return Operations::EncodeVariableResult(R,Operation);
}

namespace
{
bool RefreshSkeleton(FWork& W)
{
    if(!W.Base(TEXT("after_structural_graph_append")))return false;auto* B=W.BP.Get();UClass* Current=B->SkeletonGeneratedClass.Get();
    if(Current&&(!Live(Current)||Current->ClassGeneratedBy!=B||Current->GetOutermost()!=B->GetOutermost()||Current->GetPackage()!=B->GetPackage()))return W.Fail(TEXT("blueprint_connection_skeleton_owner_changed"),TEXT("Structural callback returned a foreign current skeleton class; no class was adopted."));
    W.Skeleton=Current;W.AllowSkeletonTransition=false;return !Current||W.Pin(Current);
}
void RetireSkeletonForStructuralSDK(FWork& W)
{
    UClass* Old=W.Skeleton.Get();TArray<TSharedPtr<FJsonValue>> Retired;
    if(Old)for(auto& Identity:W.Holds){if(Identity->Retired)continue;auto* O=Identity->Hold.Get();if(O==Old||O->IsIn(Old)){Identity->Retired=true;Retired.Add(MakeShared<FJsonValueString>(Identity->Path));}}
    W.AllowSkeletonTransition=true;W.R.Data->SetArrayField(TEXT("expected_structural_skeleton_retirements"),Retired);W.R.Data->SetBoolField(TEXT("SDK_structural_skeleton_compile_may_block"),true);
}
UEdGraph* CreateGraph(FWork& W,const FString& RequestedName,bool Function)
{
    auto* B=W.BP.Get();FName NewName=FName(TEXT("EventGraph"));
    if(Function){if(RequestedName.IsEmpty()){W.Fail(TEXT("blueprint_connection_graph_name_missing"),TEXT("Missing 'graph_name' parameter for function graph creation"));return nullptr;}if(!TextSafe(RequestedName,NAME_SIZE-1)){W.Fail(TEXT("blueprint_connection_graph_name_bound"),TEXT("Function graph FName exceeds NAME_SIZE."));return nullptr;}if(!W.Event(TEXT("FindUniqueKismetName")))return nullptr;NewName=FBlueprintEditorUtils::FindUniqueKismetName(B,RequestedName);if(!W.After(TEXT("FindUniqueKismetName")))return nullptr;}
    if(FindObject<UObject>(B,*NewName.ToString())){W.Fail(TEXT("blueprint_connection_graph_name_collision"),TEXT("A same-name object already occupies the new graph namespace; no overwrite or retry."));return nullptr;}W.R.Data->SetStringField(TEXT("requested_new_graph_name"),NewName.ToString());
    if(!W.Event(TEXT("CreateNewGraph"),true))return nullptr;UEdGraph* Created=FBlueprintEditorUtils::CreateNewGraph(B,NewName,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());W.Returned();W.R.Data->SetBoolField(TEXT("create_graph_returned_nonnull"),Created!=nullptr);W.R.bChanged|=Created!=nullptr;
    if(!W.After(TEXT("CreateNewGraph")))return nullptr;
    if(!Live(Created)||Created->GetOuter()!=B||Created->GetPackage()!=B->GetPackage()||Created->GetOutermost()!=B->GetOutermost()||Created->GetFName()!=NewName||Created->GetClass()!=UEdGraph::StaticClass()||Created->Schema.Get()!=UEdGraphSchema_K2::StaticClass()){W.Fail(TEXT("blueprint_connection_created_graph_identity_invalid"),TEXT("SDK did not return the exact newly owned named K2 graph; residual creation prefix retained."));return nullptr;}
    if(!W.Pin(Created))return nullptr;W.Selected=Created;W.R.Data->SetStringField(TEXT("created_graph_path"),Created->GetPathName());
    const TCHAR* AddStage=Function?TEXT("AddFunctionGraph"):TEXT("AddUbergraphPage");if(!W.Event(AddStage,true))return nullptr;
    // The public append performs skeleton-only compilation internally. Only that old skeleton role retires.
    RetireSkeletonForStructuralSDK(W);W.Roles[Function?0:2].Add(Created);
    if(Function)FBlueprintEditorUtils::AddFunctionGraph<UClass>(B,Created,true,nullptr);else FBlueprintEditorUtils::AddUbergraphPage(B,Created);W.Returned();
    if(!RefreshSkeleton(W)||!W.After(AddStage))return nullptr;
    if(Function){if(!W.Event(TEXT("MarkBlueprintAsStructurallyModified"),true))return nullptr;RetireSkeletonForStructuralSDK(W);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);W.Returned();if(!RefreshSkeleton(W)||!W.After(TEXT("MarkBlueprintAsStructurallyModified")))return nullptr;}
    FGraphValue Current;if(!W.Capture(Created,Current))return nullptr;W.Graphs.Add(MoveTemp(Current));return Created;
}
bool AuditFunctions(FWork& W)
{
    // Only actual direct UFunction references. No new transitive runtime auditor is introduced.
    TArray<TWeakObjectPtr<UK2Node_CallFunction>> Calls;for(const auto& G:W.Graphs)for(const auto& N:G.Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(N.Node.Get()))Calls.Add(Call);
    for(const auto& Weak:Calls){auto* Call=Weak.Get();if(!W.Event(TEXT("GetTargetFunction")))return false;UFunction* Function=Call->GetTargetFunction();TStrongObjectPtr<UFunction> ImmediateFunction(W.GraphPins?Function:nullptr);W.R.Data->SetBoolField(TEXT("last_function_getter_returned_nonnull"),Function!=nullptr);if(!W.After(TEXT("GetTargetFunction")))return false;if(Function){if(!W.Pin(Function))return false;if(ForbiddenClassPath(Function))return W.Fail(TEXT("blueprint_connection_runtime_function_forbidden"),TEXT("Actual callable UFunction belongs to an MCP dependency; no further writer is attempted."));if(!W.Event(TEXT("Function_IsEditorOnly")))return false;const bool EditorOnly=Function->IsEditorOnly();W.R.Data->SetBoolField(TEXT("last_function_is_editor_only"),EditorOnly);if(!W.After(TEXT("Function_IsEditorOnly")))return false;if(EditorOnly)return W.Fail(TEXT("blueprint_connection_editor_function_forbidden"),TEXT("Actual callable UFunction is Editor-only."));}}
    return true;
}
bool VerifyConnection(FWork& W,ECanCreateConnectionResponse Response,const TSet<const UEdGraphNode*>& BeforeNodes)
{
    UEdGraphPin* CurrentA=nullptr;UEdGraphPin* CurrentB=nullptr;if(!W.CurrentPins(CurrentA,CurrentB))return false;bool Connected=CurrentA->LinkedTo.Contains(CurrentB)&&CurrentB->LinkedTo.Contains(CurrentA);
    TArray<TSharedPtr<FJsonValue>> ConversionRows;
    if(!Connected&&Response==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)
    {
        // Exact one newly added conversion node must have both reciprocal endpoint legs.
        auto* G=W.Selected.Get();for(const auto& Entry:G->Nodes){auto* N=Entry.Get();if(BeforeNodes.Contains(N))continue;bool InputLeg=false,OutputLeg=false;for(auto* P:N->Pins){if(P->Direction==EGPD_Input&&P->LinkedTo.Contains(CurrentA)&&CurrentA->LinkedTo.Contains(P))InputLeg=true;if(P->Direction==EGPD_Output&&P->LinkedTo.Contains(CurrentB)&&CurrentB->LinkedTo.Contains(P))OutputLeg=true;}if(InputLeg&&OutputLeg){Connected=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("node_path"),N->GetPathName());Row->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());ConversionRows.Add(MakeShared<FJsonValueObject>(Row));}}
    }
    W.R.Data->SetArrayField(TEXT("actual_conversion_nodes"),ConversionRows);W.R.Data->SetBoolField(TEXT("typed_connection_readback_passed"),Connected);
    if(!Connected)return W.Fail(TEXT("blueprint_connection_readback_failed"),TEXT("K2 returned but exact current endpoint reciprocal connection/conversion legs were not observed. Actual partial topology retained; no rollback or retry."));return true;
}
}
FString ConnectConcrete(const FString& ParamsJson,FGraphPinsLease* GraphPins,FResult* Delivered)
{
    const TCHAR* Operation=GraphPins?TEXT("connect_blueprint_graph_pins"):TEXT("connect_blueprint_nodes");FWork W;W.GraphPins=GraphPins;W.Delivered=Delivered;TSharedPtr<FJsonObject> P;
    if(!Parse(ParamsJson,P)){W.Fail(TEXT("blueprint_connection_invalid_params_json"),TEXT("Expected finite bounded JSON object."));return W.Encode(Operation);}
    FString Name,SourceId,TargetId,SourcePinName,TargetPinName;struct FRequired{const TCHAR* Key;FString* Out;};for(const FRequired& Field:{FRequired{TEXT("blueprint_name"),&Name},FRequired{TEXT("source_node_id"),&SourceId},FRequired{TEXT("target_node_id"),&TargetId},FRequired{TEXT("source_pin"),&SourcePinName},FRequired{TEXT("target_pin"),&TargetPinName}})if(!P->TryGetStringField(Field.Key,*Field.Out)){W.Fail(TEXT("blueprint_connection_required_parameter"),FString::Printf(TEXT("Missing '%s' parameter"),Field.Key));return W.Encode(Operation);}
    if(!GlobalSafe()||Running||(GraphPinsRunning&&!GraphPins)){W.Fail(TEXT("blueprint_connection_context_unavailable"),TEXT("Nonreentrant safe Editor context required."));return W.Encode(Operation);}TGuardValue<bool> Scope(Running,true);
    for(const FString* S:{&SourceId,&TargetId,&SourcePinName,&TargetPinName})if(!TextSafe(*S,65536)){W.Fail(TEXT("blueprint_connection_selector_bound"),TEXT("Bounded node/pin selectors without embedded NUL required."));return W.Encode(Operation);}
    UBlueprint* B=BlueprintTopology::ResolveBlueprintForMetadata(Name,W.R);W.SDKEntered=W.R.Data->HasField(TEXT("SDK_global_purity_unverified"));if(auto* ExistingCounts=W.R.Data->Values.Find(TEXT("native_call_counts")))W.R.Data->SetField(TEXT("resolution_call_counts"),*ExistingCounts);W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetObjectField(TEXT("legacy_result"),W.Legacy);W.R.Compiled=false;
    if(!B||!W.R.Error.IsEmpty())return W.Encode(Operation);if(!W.Bind(B)||!W.CaptureAll())return W.Encode(Operation);
    auto Request=MakeShared<FJsonObject>();Request->SetStringField(TEXT("source_node_id"),SourceId);Request->SetStringField(TEXT("target_node_id"),TargetId);Request->SetStringField(TEXT("source_pin"),SourcePinName);Request->SetStringField(TEXT("target_pin"),TargetPinName);W.R.Data->SetObjectField(TEXT("connection_intent"),Request);
    FString GraphName,GraphId,GraphType;P->TryGetStringField(TEXT("graph_name"),GraphName);P->TryGetStringField(TEXT("graph_id"),GraphId);P->TryGetStringField(TEXT("graph_type"),GraphType);for(const FString* S:{&GraphName,&GraphId,&GraphType})if(!TextSafe(*S,65536)){W.Fail(TEXT("blueprint_connection_selector_bound"),TEXT("Bounded graph selectors without embedded NUL required."));return W.Encode(Operation);}
    bool Create=false;P->TryGetBoolField(TEXT("create_graph_if_missing"),Create);const FString Type=NormalizedType(GraphType);const bool HasSelector=!GraphId.IsEmpty()||!GraphName.IsEmpty()||!Type.IsEmpty();UEdGraph* G=nullptr;
    if(!HasSelector){for(const auto& Entry:B->UbergraphPages)if(Entry&&Entry->GetName().Contains(TEXT("EventGraph"))){G=Entry.Get();break;}if(!G)G=CreateGraph(W,TEXT("EventGraph"),false);}
    else{if(!W.Event(TEXT("FindGraphForMetadata")))return W.Encode(Operation);G=BlueprintTopology::FindGraphForMetadata(B,GraphId,GraphName,GraphType,W.R);if(!W.After(TEXT("FindGraphForMetadata")))return W.Encode(Operation);if(!G){if(!Create){W.Fail(TEXT("blueprint_connection_graph_not_found"),FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_id='%s', graph_type='%s'"),*GraphName,*GraphId,*GraphType));return W.Encode(Operation);}if(!GraphId.IsEmpty()){W.Fail(TEXT("blueprint_connection_graph_id_creation_unsupported"),TEXT("Cannot create a graph with an explicit graph_id"));return W.Encode(Operation);}if(Type==TEXT("function"))G=CreateGraph(W,GraphName,true);else if(Type==TEXT("event")&&GraphName.IsEmpty()){for(const auto& Entry:B->UbergraphPages)if(Entry&&Entry->GetName().Contains(TEXT("EventGraph"))){G=Entry.Get();break;}if(!G)G=CreateGraph(W,TEXT("EventGraph"),false);}else{W.Fail(TEXT("blueprint_connection_graph_creation_unsupported"),FString::Printf(TEXT("Creating graph_type '%s' is not supported by this command"),*GraphType));return W.Encode(Operation);}}}
    if(GraphPins&&G!=GraphPins->Selected){W.Fail(TEXT("blueprint_graph_pins_selected_graph_changed"),TEXT("Exact already-bound existing graph required."));return W.Encode(Operation);}if(!G||!W.Admit(TEXT("resolved_connection_graph")))return W.Encode(Operation);bool CapturedGraph=false;for(const auto& GraphValue:W.Graphs)if(GraphValue.Graph.Get()==G){CapturedGraph=true;break;}if(!CapturedGraph){W.Fail(TEXT("blueprint_connection_selected_graph_not_captured"),TEXT("Selected graph was not in the captured current Blueprint inventory."));return W.Encode(Operation);}W.Selected=G;auto* SN=W.FindNode(G,SourceId);auto* TN=W.FindNode(G,TargetId);if(!SN||!TN){W.Fail(TEXT("blueprint_connection_node_not_found"),TEXT("Source or target node not found"));return W.Encode(Operation);}auto* A=W.FindPin(SN,SourcePinName,EGPD_Output);if(!A){W.Fail(TEXT("blueprint_connection_source_pin_missing"),FString::Printf(TEXT("Source output pin not found: %s"),*SourcePinName));return W.Encode(Operation);}auto* Target=W.FindPin(TN,TargetPinName,EGPD_Input);if(!Target){W.Fail(TEXT("blueprint_connection_target_pin_missing"),FString::Printf(TEXT("Target input pin not found: %s"),*TargetPinName));return W.Encode(Operation);}
    W.SourceNode=SN;W.TargetNode=TN;W.SourceName=A->PinName.ToString();W.TargetName=Target->PinName.ToString();W.SourcePinId=A->PinId;W.TargetPinId=Target->PinId;
    if(!W.Event(TEXT("GetSchema")))return W.Encode(Operation);const auto* Schema=Cast<const UEdGraphSchema_K2>(G->GetSchema());if(!W.After(TEXT("GetSchema")))return W.Encode(Operation);if(!Schema){W.Fail(TEXT("blueprint_connection_k2_schema_missing"),TEXT("Failed to get K2 schema"));return W.Encode(Operation);}if(!W.Pin(const_cast<UEdGraphSchema_K2*>(Schema)))return W.Encode(Operation);
    if(!W.RuntimeRefs()||!AuditFunctions(W)||!W.CurrentPins(A,Target)||!W.Event(TEXT("CanCreateConnection")))return W.Encode(Operation);if(GraphPins){GraphPins->ExecRoute=A->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec&&Target->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec;GraphPins->ArrayRoute=A->PinType.ContainerType==EPinContainerType::Array&&Target->PinType.ContainerType==EPinContainerType::Array;GraphPins->SetRoute=A->PinType.ContainerType==EPinContainerType::Set&&Target->PinType.ContainerType==EPinContainerType::Set;}const FPinConnectionResponse Response=Schema->CanCreateConnection(A,Target);const FString ResponseName=Connection::SchemaResponseName(Response.Response),Message=Response.Message.ToString();W.R.Data->SetStringField(TEXT("schema_response"),ResponseName);W.R.Data->SetStringField(TEXT("schema_message"),Message);if(!W.After(TEXT("CanCreateConnection")))return W.Encode(Operation);
    bool Allow=false;P->TryGetBoolField(TEXT("allow_pin_link_replacement"),Allow);if(!(GraphPins&&A->LinkedTo.Contains(Target)&&Target->LinkedTo.Contains(A))&&!Response.CanSafeConnect()&&(!Allow||Response.Response==CONNECT_RESPONSE_DISALLOW)){W.Legacy->SetStringField(TEXT("schema_response"),ResponseName);W.Legacy->SetStringField(TEXT("schema_message"),Message);if(const auto* SourceValue=W.Value(A))W.Legacy->SetObjectField(TEXT("source_pin"),SourceValue->Legacy);if(const auto* TargetValue=W.Value(Target))W.Legacy->SetObjectField(TEXT("target_pin"),TargetValue->Legacy);W.Fail(TEXT("blueprint_connection_schema_rejected"),Message);return W.Encode(Operation);}
    if(GraphPins&&!GraphPins->Plan(A,Target,Response,Allow,Schema)){W.Fail(TEXT("blueprint_graph_pins_profile_pending"),TEXT("Only same-scalar or authored same-element Array input old1 replacement, canonical exec output old1 replacement, and MAKE/already are closed in this public11 route."));return W.Encode(Operation);}
    const bool Already=GraphPins&&A->LinkedTo.Contains(Target)&&Target->LinkedTo.Contains(A);if(GraphPins){W.R.Data->SetBoolField(TEXT("already_connected"),Already);auto Before=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("connected"),Already);Before->SetNumberField(TEXT("source_link_count"),A->LinkedTo.Num());Before->SetNumberField(TEXT("target_link_count"),Target->LinkedTo.Num());W.R.Data->SetObjectField(TEXT("before"),Before);}
    if(!Already)
    {
    W.PlannedResponse=Response.Response;W.AllowLinkRemoval=Allow;TSet<const UEdGraphNode*> BeforeNodes;for(const auto& Entry:G->Nodes)BeforeNodes.Add(Entry.Get());if(!W.CurrentPins(A,Target)||!W.Event(TEXT("TryCreateConnection"),true))return W.Encode(Operation);
    if(GraphPins)GraphPins->Advancing=true;
    const bool Connected=Schema->TryCreateConnection(A,Target);W.Returned();W.R.Data->SetBoolField(TEXT("try_create_connection_returned"),Connected);W.R.Data->SetBoolField(TEXT("SDK_may_have_marked_blueprint_modified"),Connected);
    // A and Target are deliberately not dereferenced after the SDK; schema callbacks may retire them.
    A=nullptr;Target=nullptr;if(!W.After(TEXT("TryCreateConnection"),true))return W.Encode(Operation);if(!Connected){W.Fail(TEXT("blueprint_connection_k2_create_failed"),TEXT("K2 schema failed to create connection"));return W.Encode(Operation);}
    W.AllowLinkRemoval=false;if(GraphPins){if(!W.Event(TEXT("NotifyGraphChanged"),true))return W.Encode(Operation);G->NotifyGraphChanged();W.Returned();if(!W.After(TEXT("NotifyGraphChanged")))return W.Encode(Operation);}
    if(!W.RuntimeRefs()||!AuditFunctions(W)||!VerifyConnection(W,Response.Response,BeforeNodes)||!W.Event(TEXT("MarkBlueprintAsModified"),true))return W.Encode(Operation);FBlueprintEditorUtils::MarkBlueprintAsModified(B);W.Returned();if(!W.After(TEXT("MarkBlueprintAsModified"),true)||!W.RuntimeRefs()||!AuditFunctions(W)||!VerifyConnection(W,Response.Response,BeforeNodes))return W.Encode(Operation);
    }
    if(!W.Event(TEXT("DescribeGraphMetadata")))return W.Encode(Operation);auto Graph=BlueprintTopology::DescribeGraphMetadata(B,G,W.R);if(!W.After(TEXT("DescribeGraphMetadata"))||!Graph)return W.Encode(Operation);
    W.Legacy=MakeShared<FJsonObject>();W.Legacy->SetStringField(TEXT("source_node_id"),SourceId);W.Legacy->SetStringField(TEXT("target_node_id"),TargetId);W.Legacy->SetStringField(TEXT("source_pin"),SourcePinName);W.Legacy->SetStringField(TEXT("target_pin"),TargetPinName);W.Legacy->SetBoolField(TEXT("connected"),true);W.Legacy->SetStringField(TEXT("schema_response"),ResponseName);W.Legacy->SetStringField(TEXT("schema_message"),Message);W.Legacy->SetObjectField(TEXT("graph"),Graph);
    if(!W.Admit(TEXT("final_connection_readback")))return W.Encode(Operation);W.R.bChanged|=B->GetOutermost()->IsDirty()!=W.DirtyBefore||static_cast<int32>(B->Status)!=W.StatusBefore||B->bCachedDependenciesUpToDate!=W.CacheBefore;W.R.Data->SetBoolField(TEXT("package_dirty_before"),W.DirtyBefore);W.R.Data->SetBoolField(TEXT("package_dirty_after"),B->GetOutermost()->IsDirty());W.R.bSuccess=W.R.bPostconditionPassed=true;W.R.bChangeStateKnown=true;W.R.Stage=TEXT("complete");W.R.Data->SetStringField(TEXT("scope"),TEXT("Original general K2 connection and metadata5; current exact endpoints/physical reciprocal legs and bounded loaded Blueprint graph projections observed. SDK internal callback atomicity, global cache/derived-class effects, runtime execution, persistence and rollback are not proved. Explicit compile/save0; graph-creation structural SDK can synchronously recompile the skeleton."));return W.Encode(Operation);
}
FString Connect(const FString& ParamsJson){return ConnectConcrete(ParamsJson,nullptr,nullptr);}
// QF uses the existing Connect concrete writer and then closes the original compile/save contract.
FString ConnectGraphPins(const FString& ParamsJson)
{
    constexpr const TCHAR* Operation=TEXT("connect_blueprint_graph_pins");FResult R;TSharedPtr<FJsonObject> P;FGraphPinsLease Lease;
    auto Encode=[&](){if(Lease.Stopped){R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=false;if(R.Error.IsEmpty()){R.ErrorCode=TEXT("blueprint_graph_pins_current_unknown");R.Error=TEXT("Captured graph/class current failed; actual prefix retained without follow-up SDK, cleanup or retry.");}if(Lease.First)R.Data->SetObjectField(TEXT("first_current_failure"),Lease.First);}R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),R.bChangeStateKnown);return Operations::EncodeVariableResult(R,Operation);};
    if(!Parse(ParamsJson,P)){R.ErrorCode=TEXT("blueprint_graph_pins_invalid_json");R.Error=TEXT("Finite bounded original parameter object required.");return Encode();}
    bool Compile=false,Save=false;P->TryGetBoolField(TEXT("compile"),Compile);if(!P->TryGetBoolField(TEXT("save_asset"),Save))P->TryGetBoolField(TEXT("save"),Save);R.bCompileRequested=Compile;R.bSaveRequested=Save;
    FString Name,GraphName,GraphId,GraphType;P->TryGetStringField(TEXT("blueprint_name"),Name);P->TryGetStringField(TEXT("graph_name"),GraphName);P->TryGetStringField(TEXT("graph_id"),GraphId);P->TryGetStringField(TEXT("graph_type"),GraphType);
    if(!GlobalSafe()||Running||GraphPinsRunning){R.ErrorCode=TEXT("blueprint_graph_pins_context_invalid");R.Error=TEXT("Idle nonreentrant Editor context required.");return Encode();}
    TGuardValue<bool> ActiveRequest(GraphPinsRunning,true);
    UBlueprint* B=BlueprintTopology::ResolveBlueprintForMetadata(Name,R);TStrongObjectPtr<UBlueprint> KeepBP(B);if(!Live(B)||!R.Error.IsEmpty())return Encode();
    UEdGraph* G=BlueprintTopology::FindGraphForMetadata(B,GraphId,GraphName,GraphType,R);TStrongObjectPtr<UEdGraph> KeepGraph(G);if(!Live(G)||!R.Error.IsEmpty())return Encode();
    if(!Lease.Bind(B,G))return Encode();if(Save&&!B->GetPackage()->IsFullyLoaded()){R.ErrorCode=TEXT("blueprint_graph_pins_save_package_pending");R.Error=TEXT("Requested save requires already fully loaded target package; no hidden save-time package loading.");return Encode();}
    // The original public request is never overwritten by the private effective arguments.
    auto Effective=MakeShared<FJsonObject>(*P);Effective->SetStringField(TEXT("graph_id"),G->GraphGuid.ToString());Effective->SetStringField(TEXT("graph_name"),G->GetName());Effective->SetBoolField(TEXT("create_graph_if_missing"),false);
    FString CoreText=ConnectConcrete(Json(Effective),&Lease,&R);(void)CoreText;R.bCompileRequested=Compile;R.bSaveRequested=Save;R.Data->SetBoolField(TEXT("original_compile_requested"),Compile);R.Data->SetBoolField(TEXT("original_save_requested"),Save);R.Data->SetBoolField(TEXT("same_scalar_graph_pins_contract"),!Lease.ExecRoute&&!Lease.ArrayRoute&&!Lease.SetRoute&&!Lease.ConversionRoute);R.Data->SetBoolField(TEXT("native_autocast_graph_pins_contract"),Lease.ConversionRoute);if(Lease.ConversionRoute)R.Data->SetObjectField(TEXT("native_autocast_source"),Lease.ConversionReceipt());R.Data->SetBoolField(TEXT("array_graph_pins_contract"),Lease.ArrayRoute);R.Data->SetBoolField(TEXT("set_graph_pins_contract"),Lease.SetRoute);if(Lease.SetRoute)R.Data->SetObjectField(TEXT("set_connection_source"),Lease.SetReceipt());if(Lease.ArrayRoute)R.Data->SetObjectField(TEXT("array_connection_source"),Lease.ArrayReceipt());R.Data->SetBoolField(TEXT("exec_graph_pins_contract"),Lease.ExecRoute);R.Data->SetStringField(TEXT("graph_pins_route"),Lease.SetRoute?(Lease.SetBinary?TEXT("set_binary_int_A_then_B_make"):TEXT("set_param_int_first_make")):Lease.ArrayRoute?TEXT("same_element_array"):Lease.ExecRoute?TEXT("exec"):Lease.ConversionRoute?TEXT("native_primitive_autocast"):TEXT("same_scalar"));R.Data->SetBoolField(TEXT("input_auto_reset_planned"),Lease.PlannedDefault);R.Data->SetBoolField(TEXT("old_exec_input_auto_reset_planned"),Lease.OldPlannedDefault);
    if(Lease.Stopped||!R.bSuccess||!R.bChangeStateKnown)return Encode();
    if(Lease.ExecRoute){R.Data->SetStringField(TEXT("old_target_node_id"),Lease.OldNodeId.ToString());R.Data->SetStringField(TEXT("old_target_pin_id"),Lease.OldId.ToString());R.Data->SetStringField(TEXT("source_pin_id"),Lease.SourceId.ToString());R.Data->SetStringField(TEXT("new_target_pin_id"),Lease.TargetId.ToString());}
    else{R.Data->SetStringField(TEXT("old_source_node_id"),Lease.OldNodeId.ToString());R.Data->SetStringField(TEXT("old_source_pin_id"),Lease.OldId.ToString());R.Data->SetStringField(TEXT("target_pin_id"),Lease.TargetId.ToString());R.Data->SetStringField(TEXT("new_source_pin_id"),Lease.SourceId.ToString());}
    auto Legacy=R.Data->GetObjectField(TEXT("legacy_result"));const bool Already=R.Data->GetBoolField(TEXT("already_connected"));Legacy->SetBoolField(TEXT("already_connected"),Already);Legacy->SetBoolField(TEXT("success"),true);Legacy->SetStringField(TEXT("schema"),UEdGraphSchema_K2::StaticClass()->GetPathName());
    R.bSuccess=R.bPostconditionPassed=false;bool CompilePassed=true;FString CompileError;
    if(Compile)
    {
        if(!Lease.Current())return Encode();R.Stage=TEXT("compile");R.bCompileAttempted=true;R.bMutationAttempted=true;R.bChangeStateKnown=false;
        auto Receipt=Cubeless::UE58::CompileBlueprint(B->GetPathName(),false,false);R.Data->SetObjectField(TEXT("compile_result"),Receipt);Legacy->SetObjectField(TEXT("compile_result"),Receipt);
        // This concrete compiler returns typed compiled/status, not a synthetic accepted/async success.
        if(!Receipt->TryGetBoolField(TEXT("compiled"),CompilePassed)){Lease.Bad(TEXT("compile_result"),TEXT("typed returned compiled outcome"),TEXT("missing"));return Encode();}R.Compiled=CompilePassed;
        bool Known=true,Unknown=false;Receipt->TryGetBoolField(TEXT("change_state_known"),Known);Receipt->TryGetBoolField(TEXT("outcome_unknown"),Unknown);if(!Known||Unknown){Lease.Bad(TEXT("compile_result"),TEXT("known returned compiler outcome"),TEXT("unknown"));return Encode();}
        // Hold and validate the returned class/CDO slots before any graph/text observation.
        if(!Lease.Classes(true))return Encode();Lease.CompilerReturn=true;Lease.Advancing=true;if(!Lease.Current())return Encode();R.bChangeStateKnown=true;
        if(!CompilePassed){Receipt->TryGetStringField(TEXT("error"),CompileError);if(CompileError.IsEmpty())CompileError=TEXT("Compiler reported failure; actual requested save is reported separately.");}
    }
    Legacy->SetBoolField(TEXT("compiled"),Compile&&CompilePassed);
    if(Save)
    {
        // Known compile failure still reaches the original requested save. Unknown never does.
        if(!Lease.Current())return Encode();R.Stage=TEXT("save");R.bSaveAttempted=true;R.bMutationAttempted=true;R.bChangeStateKnown=false;
        R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(B,false);R.Data->SetBoolField(TEXT("save_returned"),R.bSaved);Legacy->SetBoolField(TEXT("saved"),R.bSaved);
        if(!R.bSaved){R.ErrorCode=TEXT("blueprint_graph_pins_save_failed");R.Error=TEXT("SaveLoadedAsset returned false; actual connected/compiled prefix retained. No next SDK, cleanup or retry.");Lease.Bad(TEXT("save_return"),TEXT("true"),TEXT("false"));return Encode();}
        if(!Lease.Current())return Encode();R.bChangeStateKnown=true;
    }
    if(!Lease.Current())return Encode();Legacy->SetBoolField(TEXT("save_requested"),Save);Legacy->SetBoolField(TEXT("saved"),R.bSaved);Legacy->SetBoolField(TEXT("dirty_after"),B->GetPackage()->IsDirty());
    // Original public11 verifies a direct reciprocal endpoint link after its
    // requested compile/save. A valid converter chain does not change that
    // legacy failure contract or erase the actual retained mutation/save prefix.
    if(Lease.ConversionRoute)
    {
        const FGraphPinsLease::FPin* Source=nullptr;const FGraphPinsLease::FPin* Target=nullptr;
        for(const auto& Row:Lease.Rows)for(const auto& Node:Row.Nodes)for(const auto& Pin:Node.Pins){if(Pin.Id==Lease.SourceId)Source=&Pin;if(Pin.Id==Lease.TargetId)Target=&Pin;}
        const bool Direct=Source&&Target&&Source->Links.Contains(Target->Address)&&Target->Links.Contains(Source->Address);
        auto After=MakeShared<FJsonObject>();After->SetBoolField(TEXT("connected"),Direct);After->SetNumberField(TEXT("source_link_count"),Source?Source->Links.Num():0);After->SetNumberField(TEXT("target_link_count"),Target?Target->Links.Num():0);After->SetStringField(TEXT("graph_error"),TEXT(""));
        const bool LegacyPost=Direct&&CompilePassed&&(!Save||R.bSaved);
        Legacy->SetObjectField(TEXT("before"),R.Data->GetObjectField(TEXT("before")));Legacy->SetObjectField(TEXT("after"),After);Legacy->SetBoolField(TEXT("postcondition_verified"),LegacyPost);
        Legacy->SetStringField(TEXT("contract_version"),TEXT("mutation_result_v1"));Legacy->SetStringField(TEXT("mode"),TEXT("apply"));Legacy->SetBoolField(TEXT("dry_run"),false);Legacy->SetBoolField(TEXT("DRY_RUN"),false);
        Legacy->SetBoolField(TEXT("changed"),!Already);Legacy->SetBoolField(TEXT("mutation_performed"),!Already);
        auto Mutation=MakeShared<FJsonObject>();Mutation->SetBoolField(TEXT("attempted"),true);Mutation->SetBoolField(TEXT("performed"),!Already);Mutation->SetStringField(TEXT("state"),Already?TEXT("not_performed"):TEXT("performed"));Legacy->SetObjectField(TEXT("mutation"),Mutation);
        auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("affected_count"),1);Delta->SetNumberField(TEXT("created_count"),0);Delta->SetNumberField(TEXT("updated_count"),Already?0:1);Delta->SetNumberField(TEXT("removed_count"),0);Delta->SetNumberField(TEXT("skipped_count"),Already?1:0);Delta->SetNumberField(TEXT("failed_count"),LegacyPost?0:1);Legacy->SetObjectField(TEXT("delta"),Delta);
        auto Verification=MakeShared<FJsonObject>();Verification->SetStringField(TEXT("status"),LegacyPost?TEXT("passed"):TEXT("failed"));Verification->SetStringField(TEXT("method"),TEXT("blueprint_graph_pin_guid_relookup"));Verification->SetBoolField(TEXT("before_observed"),true);Verification->SetBoolField(TEXT("after_observed"),true);Verification->SetBoolField(TEXT("strict"),true);
        Verification->SetArrayField(TEXT("checks"),{MakeShared<FJsonValueString>(TEXT("source_pin_resolved")),MakeShared<FJsonValueString>(TEXT("target_pin_resolved")),MakeShared<FJsonValueString>(TEXT("reciprocal_link_observed"))});Legacy->SetObjectField(TEXT("verification"),Verification);Legacy->SetBoolField(TEXT("verified_success"),LegacyPost);Legacy->SetStringField(TEXT("success_semantics"),LegacyPost?TEXT("verified_postcondition"):TEXT("postcondition_failed"));
        R.Data->SetObjectField(TEXT("after"),After);R.Data->SetObjectField(TEXT("legacy_mutation_contract"),Legacy);R.Data->SetBoolField(TEXT("legacy_direct_reciprocal_link_observed"),Direct);R.Data->SetBoolField(TEXT("legacy_created_count_is_actual_conversion_allocation_count"),false);
        if(!Direct)
        {
            R.ErrorCode=TEXT("mutation_postcondition_failed");R.Error=TEXT("Original GraphPins requires a direct reciprocal endpoint link; the separately observed native converter has two legs. Actual modification, requested compile/save and typed chain remain retained; no rollback/retry.");
            R.Stage=TEXT("legacy_direct_link_postcondition_after_requested_save");Legacy->SetBoolField(TEXT("legacy_success"),true);Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error_code"),R.ErrorCode);Legacy->SetStringField(TEXT("error"),R.Error);R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=true;return Encode();
        }
    }
    R.Data->SetBoolField(TEXT("requested_compile_failed_but_save_attempted"),Compile&&!CompilePassed&&Save&&R.bSaveAttempted);R.Data->SetBoolField(TEXT("original_graph_semantics_preserved"),true);R.Data->SetBoolField(TEXT("explicit_compile_save_calls"),Compile||Save);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("runtime_evaluated"),false);
    if(!CompilePassed){R.ErrorCode=TEXT("blueprint_graph_pins_compile_failed");R.Error=CompileError;R.Stage=TEXT("compile_failed_after_requested_save");Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),CompileError);return Encode();}
    R.bSuccess=R.bPostconditionPassed=true;R.bChangeStateKnown=true;R.Stage=TEXT("complete");R.Data->SetStringField(TEXT("scope"),TEXT("Original GraphPins public11 via existing Connect schema writer: own direct Actor/flatK2 same-scalar MAKE, already-linked or permitted input oldSource1 replacement; canonical exec MAKE/already or permitted output oldTarget1 to newTarget1 replacement, retaining other input predecessors; same-element primitive authored function Array output -> already typed native Array9 TargetArray input (Length/Get/Set/Add/AddUnique/Insert/Contains/RemoveItem/Clear) MAKE/already or input oldSource1 replacement, with endpoint qualifiers and native prototype Inner roles preserved separately; exact graph/pin semantic preservation and selected compile/save receipts. Explicitly permitted native Engine primitive autocasts into an unlinked whole scalar input reuse the same writer and preserve the original final direct-link failure contract after actual requested compile/save. Array item peers must already carry the same primitive element; no wildcard inference, object/struct arrays, mixed/split peers or alternate producer expansion. Native prototype/metadata/nullable library CDO identities remain protected; peer defaults/types and every original input qualifier are preserved. New operation/element/replacement/compile/save behavior is usage validation pending. Callback internals, arbitrary CDO/SCS reflected values, other converters/promotions/new graph creation, runtime execution and full original parity remain incomplete. Existing separate routes are unchanged."));return Encode();
}

}
