#include "Compatibility/UE58/CubelessMapApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "Kismet/BlueprintSetLibrary.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EngineVersion.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Cubeless::UE58::Map
{
namespace
{
using Function::FResult;
constexpr const TCHAR* LibraryPath=TEXT("/Script/Engine.BlueprintMapLibrary");
constexpr const TCHAR* DefaultPath=TEXT("/Script/Engine.Default__BlueprintMapLibrary");
constexpr const TCHAR* SetLibraryPath=TEXT("/Script/Engine.BlueprintSetLibrary");
constexpr const TCHAR* SetDefaultPath=TEXT("/Script/Engine.Default__BlueprintSetLibrary");
const TCHAR* SetOperation(const FString& Name)
{struct FRow{const TCHAR* Name;const TCHAR* Op;};static constexpr FRow Rows[]={{TEXT("Set_Add"),TEXT("add")},{TEXT("Set_Remove"),TEXT("remove")},{TEXT("Set_Contains"),TEXT("contains")},{TEXT("Set_Length"),TEXT("length")},{TEXT("Set_ToArray"),TEXT("toarray")},{TEXT("Set_Union"),TEXT("union")},{TEXT("Set_Intersection"),TEXT("intersection")},{TEXT("Set_Difference"),TEXT("difference")}};for(const auto& R:Rows)if(Name.Equals(R.Name,ESearchCase::CaseSensitive))return R.Op;return nullptr;}
constexpr const TCHAR* VectorPath=TEXT("/Script/CoreUObject.Vector");
bool Loaded(const UObject* O)
{return IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);}
bool Ready()
{return IsEditorGameThread()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&Loaded(GEditor)&&!IsPlaySessionActive();}
bool Equal(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}
bool SameType(const FEdGraphPinType& A,const FEdGraphPinType& B)
{return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
uint32 PinFlags(const UEdGraphPin* P)
{return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bAdvancedView)<<4)|(uint32(P->bDeprecated)<<5)|(uint32(P->bDisplayAsMutableRef)<<6)|(uint32(P->bAllowFriendlyName)<<7)|(uint32(P->bOrphanedPin)<<8)|(uint32(P->ShouldSavePinIfOrphaned())<<9);}
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold;UObject* Outer;UClass* Class;UPackage* Package;FString Path;EObjectFlags Flags;bool PackageRole,Dirty,MutableDirty;uint32 PackageFlags=0;
    explicit FIdentity(UObject* O,bool Mutable=false):Hold(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetPackage()),Path(O->GetPathName()),Flags(O->GetFlags()),PackageRole(O->IsA<UPackage>()),Dirty(false),MutableDirty(Mutable)
    {if(PackageRole){const auto* P=static_cast<const UPackage*>(O);Dirty=P->IsDirty();PackageFlags=P->GetPackageFlags();}}
    bool Current()const
    {const auto* O=Hold.Get();if(!Loaded(O)||O->GetClass()!=Class||O->GetOuter()!=Outer||O->GetPackage()!=Package||!Equal(O->GetPathName(),Path)||O->GetFlags()!=Flags)return false;if(PackageRole){const auto* P=static_cast<const UPackage*>(O);if(P->GetPackageFlags()!=PackageFlags||(!MutableDirty&&P->IsDirty()!=Dirty))return false;}return true;}
};
struct FField
{
    FProperty* Address;FFieldClass* Class;FName Name;uint64 Flags;int32 Dim,Offset,Size;UStruct* Owner;const FProperty* OwnerProperty;
    explicit FField(FProperty* P):Address(P),Class(P->GetClass()),Name(P->GetFName()),Flags(uint64(P->GetPropertyFlags())),Dim(P->ArrayDim),Offset(P->GetOffset_ForInternal()),Size(P->GetSize()),Owner(P->GetOwnerStruct()),OwnerProperty(P->GetOwnerProperty()){}
    bool Current()const
    {const auto* P=Address;return P&&P->GetClass()==Class&&P->GetFName()==Name&&uint64(P->GetPropertyFlags())==Flags&&P->ArrayDim==Dim&&P->GetOffset_ForInternal()==Offset&&P->GetSize()==Size&&P->GetOwnerStruct()==Owner&&P->GetOwnerProperty()==OwnerProperty&&!P->HasSetterOrGetter();}
};
struct FPin
{
    UEdGraphPin* Address;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;uint32 Flags;FString Default,Auto;FText Text;TObjectPtr<UObject> Object;
    TStrongObjectPtr<UObject> ObjectHold;UEdGraphPin* Parent;TArray<UEdGraphPin*> Children,Links;
    explicit FPin(UEdGraphPin* P):Address(P),Id(P->PinId),Name(P->PinName),Direction(P->Direction),Type(P->PinType),Flags(PinFlags(P)),Default(P->DefaultValue),Auto(P->AutogeneratedDefaultValue),Text(P->DefaultTextValue),Object(P->DefaultObject),Parent(P->ParentPin),Children(P->SubPins),Links(P->LinkedTo){}
    bool Current(const UEdGraphNode* N)const
    {const auto* P=Address;return P&&!P->WasTrashed()&&P->GetOwningNodeUnchecked()==N&&P->PinId==Id&&P->PinName==Name&&P->Direction==Direction&&SameType(P->PinType,Type)&&PinFlags(P)==Flags&&P->DefaultObject.IsResolved()&&P->DefaultObject==Object&&Equal(P->DefaultValue,Default)&&Equal(P->AutogeneratedDefaultValue,Auto)&&P->DefaultTextValue.EqualTo(Text)&&P->ParentPin==Parent&&P->SubPins==Children&&P->LinkedTo==Links;}
};
struct FOldNode
{
    TStrongObjectPtr<UEdGraphNode> Hold;FString Path;UClass* Class;UObject* Outer;EObjectFlags Flags;FGuid Guid;int32 X,Y,Width,Height;FString Comment;
    bool Bubble,Pinned,Call=false,Variable=false;FMemberReference Member;TArray<TSharedPtr<FPin>> Pins;
    explicit FOldNode(UEdGraphNode* N):Hold(N),Path(N->GetPathName()),Class(N->GetClass()),Outer(N->GetOuter()),Flags(N->GetFlags()),Guid(N->NodeGuid),X(N->NodePosX),Y(N->NodePosY),Width(N->NodeWidth),Height(N->NodeHeight),Comment(N->NodeComment),Bubble(N->bCommentBubbleVisible),Pinned(N->bCommentBubblePinned)
    {if(const auto* C=Cast<UK2Node_CallFunction>(N)){Call=true;Member=C->FunctionReference;}else if(const auto* V=Cast<UK2Node_Variable>(N)){Variable=true;Member=V->VariableReference;}}
    bool Current()const
    {const auto* N=Hold.Get();if(!Loaded(N)||N->GetClass()!=Class||N->GetOuter()!=Outer||!Equal(N->GetPathName(),Path)||N->GetFlags()!=Flags||N->NodeGuid!=Guid||N->NodePosX!=X||N->NodePosY!=Y||N->NodeWidth!=Width||N->NodeHeight!=Height||!Equal(N->NodeComment,Comment)||N->bCommentBubbleVisible!=Bubble||N->bCommentBubblePinned!=Pinned||N->Pins.Num()!=Pins.Num())return false;
        if(Call){const auto& M=static_cast<const UK2Node_CallFunction*>(N)->FunctionReference;if(!M.IsSameReference(Member)||M.IsDeprecated()!=Member.IsDeprecated())return false;}
        if(Variable){const auto& M=static_cast<const UK2Node_Variable*>(N)->VariableReference;if(!M.IsSameReference(Member)||M.IsDeprecated()!=Member.IsDeprecated())return false;}
        for(int32 I=0;I<Pins.Num();++I)if(N->Pins[I]!=Pins[I]->Address||!Pins[I]->Current(N))return false;return true;}
};
struct FOldGraph
{
    TStrongObjectPtr<UEdGraph> Hold;FString Path;UObject* Outer;UClass* Class;EObjectFlags Flags;FGuid Guid;TObjectPtr<UClass> Schema;bool Editable;
    TArray<TObjectPtr<UEdGraph>> Children;TArray<TSharedPtr<FOldNode>> Nodes;
    explicit FOldGraph(UEdGraph* G):Hold(G),Path(G->GetPathName()),Outer(G->GetOuter()),Class(G->GetClass()),Flags(G->GetFlags()),Guid(G->GraphGuid),Schema(nullptr),Editable(G->bEditable),Children(G->SubGraphs)
    {auto H=G->Schema;if(H.GetGCPtr().IsResolved())Schema=H.GetGCPtr();}
};
// Concrete lifecycle for the exact String -> double/Vector Map8 cohorts.
struct FMapScope
{
    bool SetCall=false;FSetProperty* SetParameter=nullptr;FProperty* SetElement=nullptr;
    TArray<TPair<FSetProperty*,FProperty*>> AdditionalSets;
    bool BinarySet()const{return SetCall&&(Operation==TEXT("union")||Operation==TEXT("intersection")||Operation==TEXT("difference"));}
    const TCHAR* SourcePath()const{return SetCall?SetLibraryPath:LibraryPath;}const TCHAR* SourceDefaultPath()const{return SetCall?SetDefaultPath:DefaultPath;}
    FResult& R;bool LiteralMode=false;int32 PendingPin=INDEX_NONE;TSharedPtr<FPin> PendingExpected;bool Neutral=false,PresetLibrary=false,PresetEntryFixed=false;TStrongObjectPtr<UK2Node_FunctionEntry> PresetEntry;FMemberReference PresetEntryReference;uint32 PresetEntryFlags=0;
    bool DoubleValue=false,Stopped=false,LastCurrent=false,SourceFixed=false,ClassesObserved=false,ClassesFixed=false,TargetFixed=false,InventoryFixed=false,Attached=false,MemberFixed=false,GuidFixed=false,PinsFixed=false;
    FString Operation,FunctionName,NodePath;int32 X=0,Y=0;FGuid GraphGuid,NodeGuid;EObjectFlags NodeFlags=RF_NoFlags;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;decltype(GUndo) Undo=nullptr;
    TArray<TSharedPtr<FIdentity>> Identities;
    TStrongObjectPtr<UClass> Library,SchemaClass,NodeClass,Parent,Generated,Skeleton;
    TObjectPtr<UClass> ParentHandle,GeneratedHandle,SkeletonHandle;
    TArray<UClass*> ParentChain;TArray<uint32> ParentFlags;TArray<int32> ParentSizes;
    TStrongObjectPtr<UObject> LibraryCDO,SchemaCDO,GeneratedCDO,SkeletonCDO;
    TStrongObjectPtr<UScriptStruct> Vector;TStrongObjectPtr<UFunction> Native;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UK2Node_CallFunction> Node;UPackage* TargetPackage=nullptr;
    FMemberReference Member;TArray<TSharedPtr<FPin>> Pins;
    TArray<FProperty*> Parameters,VectorFields;TArray<TSharedPtr<FField>> Fields;FMapProperty* MapParameter=nullptr;FProperty *Key=nullptr,*Value=nullptr;FArrayProperty* ArrayParameter=nullptr;FProperty* ArrayInner=nullptr;
    uint32 FunctionFlags=0,ClassFlags=0,SchemaFlags=0,NodeClassFlags=0,VectorFlags=0;int32 FunctionSize=0,VectorSize=0,VectorAlign=0,NodeClassSize=0;
    TArray<TPair<FName,FString>> Metadata;TArray<bool> MetadataPresent;
    TSet<const UEdGraphPin*> OldPinAddresses;TArray<TSharedPtr<FOldGraph>> OldGraphs;TArray<TObjectPtr<UEdGraph>> Functions,Macros,Ubergraphs,Events,Delegates;
    TArray<FBPVariableDescription> Variables,GeneratedVariables;TArray<FBPInterfaceDescription> Interfaces;EBlueprintStatus Status=BS_Unknown;
    explicit FMapScope(FResult& Reply):R(Reply){}
    bool Reject(const TCHAR* Code,const TCHAR* Why,bool Unknown=false)
    {if(!Stopped){const bool KnownPrefix=R.bMutationAttempted&&!Unknown&&LastCurrent&&Attached&&PinsFixed&&Node.IsValid();Stopped=true;R.ErrorCode=Code;R.Error=Why;R.bSuccess=R.bPostconditionPassed=false;if(Unknown||R.bMutationAttempted)R.bChangeStateKnown=KnownPrefix;if(KnownPrefix){R.bChanged=true;R.Data->SetObjectField(TEXT("after"),TargetJson());R.Data->SetBoolField(TEXT("created"),true);R.Data->SetBoolField(TEXT("known_retained_wildcard_prefix"),!LiteralMode);if(LiteralMode){R.Data->SetBoolField(TEXT("known_retained_typed_map_prefix"),true);
        auto Prefix=MakeShared<FJsonObject>();for(const auto& P:Pins){auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("pin_id"),P->Id.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("raw_default"),P->Default);Q->SetStringField(TEXT("autogenerated_default_value"),P->Auto);Q->SetStringField(TEXT("category"),P->Type.PinCategory.ToString());Q->SetStringField(TEXT("subcategory"),P->Type.PinSubCategory.ToString());Prefix->SetObjectField(P->Name.ToString(),Q);}R.Data->SetObjectField(TEXT("map_literal_known_pin_prefix"),Prefix);R.Data->SetStringField(TEXT("map_literal_known_pin_prefix_scope"),TEXT("Copied last complete current pin snapshots; no failure-time SDK or retired-pointer dereference."));}}R.Data->SetStringField(TEXT("failed_boundary"),R.Stage);}return false;}
    bool Keep(UObject* O,bool Mutable=false)
    {if(!Loaded(O))return Reject(TEXT("map_role_unavailable"),TEXT("Required already-loaded role unavailable."));for(const auto& I:Identities)if(I->Hold.Get()==O)return I->Current()||Reject(TEXT("map_identity_changed"),TEXT("A held identity changed."),true);Identities.Add(MakeShared<FIdentity>(O,Mutable));return true;}
    bool Context()
    {if(Stopped)return false;if(!Ready()||GEditor!=Editor.Get()||!Loaded(Editor.Get())||!Loaded(World.Get())||World->bIsTearingDown)return Reject(TEXT("map_editor_context_changed"),TEXT("Captured Editor/world changed; no next SDK or cleanup."),true);
        UWorld* Current=Editor->GetEditorWorldContext().World();
        if(!Ready()||GEditor!=Editor.Get()||!Loaded(Editor.Get())||!Loaded(World.Get())||World->bIsTearingDown||Current!=World.Get())return Reject(TEXT("map_editor_context_changed"),TEXT("Editor/world changed across tracked world getter."),true);return true;}
    bool Begin()
    {if(!Ready())return Reject(TEXT("map_editor_state_invalid"),TEXT("Idle loaded Editor GT required before lookup."));Editor.Reset(GEditor);if(!Keep(Editor.Get()))return false;UWorld* W=Editor->GetEditorWorldContext().World();if(!Ready()||GEditor!=Editor.Get()||!Loaded(W))return Reject(TEXT("map_editor_context_changed"),TEXT("Initial world role changed."),true);World.Reset(W);if(!Keep(W)||!Context())return false;Undo=GUndo;return true;}
    bool CDOCurrent(const UClass* C,const UObject* D)const
    {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto H=C->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        return H.IsResolved()&&H==D&&(!D||(Loaded(D)&&D->GetClass()==C));
    }
    bool PresetCurrent()const
    {
        if(!PresetLibrary)return true;auto* B=Blueprint.Get();
        if((!DoubleValue&&!Neutral)||!Loaded(B)||B->BlueprintType!=BPTYPE_FunctionLibrary||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=UBlueprintFunctionLibrary::StaticClass()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=nullptr||!B->NewVariables.IsEmpty()||!B->GeneratedVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->CachedUDSDependencies.IsEmpty())return false;
        if(!PresetEntryFixed)return true;auto* E=PresetEntry.Get();auto* G=Graph.Get();if(!Loaded(G)||G->GetFName()!=TEXT("PresetLoader")||G->GetOuter()!=B||!B->FunctionGraphs.Contains(G)||!Loaded(E)||E->GetClass()!=UK2Node_FunctionEntry::StaticClass()||E->GetOuter()!=G||!G->Nodes.Contains(E)||!E->NodeGuid.IsValid()||!E->FunctionReference.IsSameReference(PresetEntryReference)||E->FunctionReference.IsDeprecated()||E->GetExtraFlags()!=PresetEntryFlags||!(PresetEntryFlags&FUNC_Static))return false;
        int32 Count=0;for(UEdGraphPin* P:E->Pins)if(P&&P->PinName==TEXT("__WorldContext")){FEdGraphPinType Expected;Expected.PinCategory=UEdGraphSchema_K2::PC_Object;Expected.PinSubCategoryObject=UObject::StaticClass();if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=E||!P->PinId.IsValid()||P->Direction!=EGPD_Output||!SameType(P->PinType,Expected)||!P->bHidden||P->bOrphanedPin||P->bDeprecated||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return false;++Count;}return Count==1;
    }
    bool Raw();
    bool PriorCurrent()const;
    bool Source();bool SetSource();
    bool Target(const FString& Path,const FString& Name,const FString& Id,const FString& Kind);
    bool FreezePins();bool SetFreezePins();
    bool PinReceipt();
    bool UntypedCallReceipt();
    TArray<TSharedPtr<FJsonValue>> LiteralCalls;
    void LiteralCall(const TCHAR* Call,bool Returned){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("call"),Call);J->SetBoolField(TEXT("attempted"),true);J->SetBoolField(TEXT("returned"),Returned);LiteralCalls.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("map_literal_actual_call_prefix"),LiteralCalls);}
    bool LiteralTypes();
    bool LiteralDefaults(const TSharedRef<FJsonObject>& Defaults);
    bool LiteralTransition(int32 Index,const TSharedPtr<FPin>& Expected,const FString& Literal,bool Auto);
    TSharedRef<FJsonObject> LiteralObservation(int32 Index,const FString& Requested,const FString& SchemaLiteral,bool Match,bool Valid);
    bool ExpectedType(const FProperty* P,const FEdGraphPinType& Type)const;
    TSharedRef<FJsonObject> TypeJson(const FEdGraphPinType& Type)const;
    TSharedRef<FJsonObject> SourceJson()const;
    TSharedRef<FJsonObject> TargetJson()const;
    FString Kind(const UEdGraph* G)const
    {const auto* B=Blueprint.Get();if(B->FunctionGraphs.Contains(G))return TEXT("function");if(B->MacroGraphs.Contains(G))return TEXT("macro");if(B->UbergraphPages.Contains(G)||B->EventGraphs.Contains(G))return TEXT("event");return TEXT("unknown");}
    UObject* ExistingCDO(UClass* C,const FString& Path)
    {if(!Raw())return nullptr;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto H=C->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        if(!H.IsResolved())return nullptr;UObject* D=StaticFindObject(nullptr,C->GetPackage(),*FPackageName::ObjectPathToObjectName(Path));
        if(!Raw()||!Loaded(D)||H!=D||!CDOCurrent(C,D)||!Equal(D->GetPathName(),Path)||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return nullptr;return Keep(D)?D:nullptr;}
    bool AfterModify(bool Returned,EObjectFlags Before)
    {R.Data->SetBoolField(R.Stage+TEXT("_returned"),Returned);
        // Only the selected package's actual normal Undo transactional addition
        // at this Modify return; no generic role flag mask or save transition.
        if(Ready()&&GEditor==Editor.Get()&&Loaded(TargetPackage)&&GUndo==Undo&&Undo&&Returned&&TargetPackage->IsDirty()&&TargetPackage->GetFlags()==(Before|RF_Transactional))
            for(const auto& I:Identities)if(I->Hold.Get()==TargetPackage&&I->Flags==Before){I->Flags=TargetPackage->GetFlags();R.Data->SetBoolField(TEXT("map_target_package_transactional_addition_accepted"),Before!=I->Flags);}
        return Raw();}
    bool Adopt(UK2Node_CallFunction* Returned)
    {R.Data->SetNumberField(TEXT("node_allocation_returns"),1);R.Data->SetBoolField(TEXT("node_allocation_returned_nonnull"),Returned!=nullptr);R.Data->SetBoolField(TEXT("returned_node_identity_observed"),false);
        if(IsEditorGameThread()&&!IsGarbageCollecting()&&Loaded(Returned)){Node.Reset(Returned);NodePath=Returned->GetPathName();NodeFlags=Returned->GetFlags();auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),NodePath);J->SetStringField(TEXT("class_path"),Returned->GetClass()->GetPathName());J->SetStringField(TEXT("outer_path"),Returned->GetOuter()?Returned->GetOuter()->GetPathName():FString());J->SetStringField(TEXT("package_path"),Returned->GetPackage()->GetName());J->SetStringField(TEXT("object_flags_string"),LexToString(uint32(NodeFlags)));R.Data->SetObjectField(TEXT("returned_node_identity"),J);R.Data->SetBoolField(TEXT("returned_node_identity_observed"),true);}
        if(!Raw())return false;return Node.IsValid()||Reject(TEXT("map_node_allocation_failed"),TEXT("No safe returned node identity; allocation prefix retained."),true);}
};

bool SameVariables(const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B)
{
    if(A.Num()!=B.Num())return false;
    for(int32 I=0;I<A.Num();++I){const auto& X=A[I];const auto& Y=B[I];
        if(X.VarName!=Y.VarName||X.VarGuid!=Y.VarGuid||!SameType(X.VarType,Y.VarType)||X.PropertyFlags!=Y.PropertyFlags||!Equal(X.FriendlyName,Y.FriendlyName)||!X.Category.IdenticalTo(Y.Category)||!Equal(X.DefaultValue,Y.DefaultValue)||X.RepNotifyFunc!=Y.RepNotifyFunc||X.ReplicationCondition!=Y.ReplicationCondition||X.MetaDataArray.Num()!=Y.MetaDataArray.Num())return false;
        for(int32 M=0;M<X.MetaDataArray.Num();++M)if(X.MetaDataArray[M].DataKey!=Y.MetaDataArray[M].DataKey||!Equal(X.MetaDataArray[M].DataValue,Y.MetaDataArray[M].DataValue))return false;
    }return true;
}
bool FMapScope::PriorCurrent()const
{
    const auto* B=Blueprint.Get();
    if(B->FunctionGraphs!=Functions||B->MacroGraphs!=Macros||B->UbergraphPages!=Ubergraphs||B->EventGraphs!=Events||B->DelegateSignatureGraphs!=Delegates||!SameVariables(B->NewVariables,Variables)||!SameVariables(B->GeneratedVariables,GeneratedVariables)||B->ImplementedInterfaces.Num()!=Interfaces.Num())return false;
    for(int32 I=0;I<Interfaces.Num();++I)if(B->ImplementedInterfaces[I].Interface!=Interfaces[I].Interface||B->ImplementedInterfaces[I].Graphs!=Interfaces[I].Graphs)return false;
    for(const auto& Old:OldGraphs){const auto* G=Old->Hold.Get();auto H=G->Schema;
        if(!Loaded(G)||!Equal(G->GetPathName(),Old->Path)||G->GetOuter()!=Old->Outer||G->GetClass()!=Old->Class||G->GetFlags()!=Old->Flags||G->GraphGuid!=Old->Guid||!H.GetGCPtr().IsResolved()||H.GetGCPtr()!=Old->Schema||G->bEditable!=Old->Editable||G->SubGraphs!=Old->Children||G->Nodes.Num()!=Old->Nodes.Num()+(G==Graph.Get()&&Attached?1:0))return false;
        for(int32 I=0;I<Old->Nodes.Num();++I)if(!G->Nodes[I].IsResolved()||G->Nodes[I]!=Old->Nodes[I]->Hold.Get()||!Old->Nodes[I]->Current())return false;
    }return true;
}
bool FMapScope::Raw()
{
    LastCurrent=false;
    if(!Context())return false;
    if(GUndo!=Undo)return Reject(TEXT("map_transaction_context_changed"),TEXT("Active Undo changed across the node-only operation."),true);
    for(const auto& I:Identities)if(!I->Current())return Reject(TEXT("map_identity_changed"),TEXT("Held identity/class/package state changed."),true);
    if(SourceFixed){
        if(uint32(SchemaClass->ClassFlags)!=SchemaFlags||uint32(NodeClass->ClassFlags)!=NodeClassFlags||NodeClass->GetStructureSize()!=NodeClassSize||((!DoubleValue&&!Neutral)&&uint32(Vector->StructFlags)!=VectorFlags))return Reject(TEXT("map_native_class_changed"),TEXT("Native schema/stock node class flags or layout changed."),true);
        if(Native->GetOuterUClass()!=Library.Get()||Native->GetFName()!=FName(*FunctionName)||uint32(Native->FunctionFlags)!=FunctionFlags||Native->GetStructureSize()!=FunctionSize||Native->NumParms!=Parameters.Num()||uint32(Library->ClassFlags)!=ClassFlags||!CDOCurrent(Library.Get(),LibraryCDO.Get())||!CDOCurrent(SchemaClass.Get(),SchemaCDO.Get())||((!DoubleValue&&!Neutral)&&(Vector->GetStructureSize()!=VectorSize||Vector->GetMinAlignment()!=VectorAlign))||(!Library->HasMetaData(TEXT("BlueprintThreadSafe"))||!Library->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty()))return Reject(TEXT("map_native_source_changed"),TEXT("Native function/class/CDO/layout changed."),true);
        TArray<FProperty*> Current;for(TFieldIterator<FProperty> It(Native.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It)Current.Add(*It);
        if(SetCall){if(Current!=Parameters)return Reject(TEXT("set_native_source_changed"),TEXT("Ordered native root inventory changed."),true);for(const auto& F:Fields)if(Parameters.Contains(F->Address)&&!F->Current())return Reject(TEXT("set_native_source_changed"),TEXT("Root descriptor changed before nested slot access."),true);}
        if(SetCall)for(const auto& E:AdditionalSets)if(E.Key->ElementProp!=E.Value)return Reject(TEXT("set_native_source_changed"),TEXT("Additional Set root/nested slot changed."),true);
        if(Current!=Parameters||(SetCall?(!SetParameter||SetParameter->ElementProp!=SetElement||(ArrayParameter&&ArrayParameter->Inner!=ArrayInner)):(MapParameter->KeyProp!=Key||MapParameter->ValueProp!=Value||(ArrayParameter&&(ArrayParameter->Inner!=ArrayInner||ArrayInner->GetOwner<FProperty>()!=ArrayParameter||Key->GetOwner<FProperty>()!=MapParameter||Value->GetOwner<FProperty>()!=MapParameter)))))return Reject(TEXT("map_native_source_changed"),TEXT("Ordered native property inventory changed."),true);
        int32 Index=0;if((!DoubleValue&&!Neutral))for(TFieldIterator<FProperty> It(Vector.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It){if(!VectorFields.IsValidIndex(Index)||*It!=VectorFields[Index++])return Reject(TEXT("map_vector_source_changed"),TEXT("Vector field inventory changed."),true);}
        if(Index!=VectorFields.Num())return Reject(TEXT("map_vector_source_changed"),TEXT("Vector field count changed."),true);
        for(const auto& F:Fields)if(!F->Current())return Reject(TEXT("map_native_source_changed"),TEXT("A captured function or Vector property changed."),true);
        for(int32 I=0;I<Metadata.Num();++I)if(Native->HasMetaData(Metadata[I].Key)!=MetadataPresent[I]||!Equal(Native->GetMetaData(Metadata[I].Key),Metadata[I].Value))return Reject(TEXT("map_native_metadata_changed"),TEXT("Audited native metadata changed."),true);
    }
    if(PresetLibrary&&!PresetCurrent())return Reject(TEXT("map_preset_loader_context_changed"),TEXT("Exact static member0 PresetLoader local Map context or automatic Entry WorldContext changed."),true);
    if(ClassesObserved){auto P=Blueprint->ParentClass;auto G=Blueprint->GeneratedClass;auto S=Blueprint->SkeletonGeneratedClass;if(!P.GetGCPtr().IsResolved()||!G.GetGCPtr().IsResolved()||!S.GetGCPtr().IsResolved()||P.GetGCPtr()!=ParentHandle||G.GetGCPtr()!=GeneratedHandle||S.GetGCPtr()!=SkeletonHandle||Blueprint->Status!=Status||Blueprint->bBeingCompiled||Blueprint->bQueuedForCompilation||Blueprint->bIsRegeneratingOnLoad||Blueprint->bSuppressStructurallyModified)return Reject(TEXT("map_target_class_changed"),TEXT("Captured class handles/status changed during target adoption."),true);}
    if(ClassesFixed){auto* B=Blueprint.Get();auto P=B->ParentClass;auto G=B->GeneratedClass;auto S=B->SkeletonGeneratedClass;
        if(!P.GetGCPtr().IsResolved()||!G.GetGCPtr().IsResolved()||!S.GetGCPtr().IsResolved()||P.GetGCPtr()!=Parent.Get()||G.GetGCPtr()!=Generated.Get()||S.GetGCPtr()!=Skeleton.Get()||B->BlueprintType!=(PresetLibrary?BPTYPE_FunctionLibrary:BPTYPE_Normal)||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||B->Status!=Status)return Reject(TEXT("map_target_class_changed"),TEXT("Captured Blueprint class/status roles changed before another SDK."),true);
        int32 Index=0;for(auto It=Parent->GetSuperStructIterator();It;++It){if(!ParentChain.IsValidIndex(Index)||*It!=ParentChain[Index]||uint32(ParentChain[Index]->ClassFlags)!=ParentFlags[Index]||ParentChain[Index]->GetStructureSize()!=ParentSizes[Index])return Reject(TEXT("map_parent_source_changed"),TEXT("Native parent hierarchy/layout changed."),true);++Index;}if(Index!=ParentChain.Num())return Reject(TEXT("map_parent_source_changed"),TEXT("Native parent hierarchy count changed."),true);
    }
    if(TargetFixed){auto* B=Blueprint.Get();auto P=B->ParentClass;auto G=B->GeneratedClass;auto S=B->SkeletonGeneratedClass;auto Schema=Graph->Schema;
        if(!P.GetGCPtr().IsResolved()||!G.GetGCPtr().IsResolved()||!S.GetGCPtr().IsResolved()||P.GetGCPtr()!=Parent.Get()||G.GetGCPtr()!=Generated.Get()||S.GetGCPtr()!=Skeleton.Get()||!CDOCurrent(Generated.Get(),GeneratedCDO.Get())||!CDOCurrent(Skeleton.Get(),SkeletonCDO.Get())||B->BlueprintType!=(PresetLibrary?BPTYPE_FunctionLibrary:BPTYPE_Normal)||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified||B->Status!=Status||Graph->GetOuter()!=B||Graph->GraphGuid!=GraphGuid||!Graph->bEditable||!Schema.GetGCPtr().IsResolved()||Schema.GetGCPtr()!=SchemaClass.Get())return Reject(TEXT("map_target_role_changed"),TEXT("Blueprint/class/CDO/graph/schema roles changed."),true);
    }
    if(InventoryFixed&&!PriorCurrent())return Reject(TEXT("map_prior_state_changed"),TEXT("Existing graph/node/pin/reference/declarations changed."),true);
    if(Node.IsValid()){const auto* N=Node.Get();int32 Count=0;for(const auto& E:Graph->Nodes){if(!E.IsResolved())return Reject(TEXT("map_node_inventory_unresolved"),TEXT("Unresolved graph node inventory."),true);if(E==N)++Count;}
        if(!Loaded(N)||N->GetClass()!=UK2Node_CallFunction::StaticClass()||N->GetOuter()!=Graph.Get()||N->GetPackage()!=TargetPackage||!Equal(N->GetPathName(),NodePath)||N->GetFlags()!=NodeFlags||Count!=(Attached?1:0)||(GuidFixed&&(N->NodeGuid!=NodeGuid||N->NodePosX!=X||N->NodePosY!=Y)))return Reject(TEXT("map_created_node_changed"),TEXT("Returned node identity/membership/position changed."),true);
        if(MemberFixed&&(!N->FunctionReference.IsSameReference(Member)||N->FunctionReference.IsDeprecated()||N->IsNodePure()!=(Operation==TEXT("find")||Operation==TEXT("contains")||Operation==TEXT("length"))))return Reject(TEXT("map_function_reference_changed"),TEXT("Exact native binding changed."),true);
        if(PinsFixed){if(N->Pins.Num()!=Pins.Num())return Reject(TEXT("map_created_pins_changed"),TEXT("Physical pin count changed."),true);for(int32 I=0;I<Pins.Num();++I)if(N->Pins[I]!=Pins[I]->Address||(!Pins[I]->Current(N)&&!(LiteralMode&&PendingPin==I&&PendingExpected.IsValid()&&PendingExpected->Current(N)))){
            if(LiteralMode&&!R.Data->HasField(TEXT("map_literal_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),TEXT("created_pin_whole_role"));J->SetStringField(TEXT("pin_name"),Pins[I]->Name.ToString());J->SetStringField(TEXT("expected_pin_id"),Pins[I]->Id.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("expected_actual_observed"),false);J->SetStringField(TEXT("diagnostic_scope"),TEXT("Compound physical identity/type/default/link guard failed; no retired pin is dereferenced for a per-property diagnostic."));R.Data->SetObjectField(TEXT("map_literal_first_mismatch"),J);}
            return Reject(TEXT("map_created_pins_changed"),TEXT("Frozen physical pin/ref/terminal/default/link changed."),true);
        }}
    }LastCurrent=true;return true;
}
bool FMapScope::ExpectedType(const FProperty* P,const FEdGraphPinType& Type)const
{
    FEdGraphPinType E;E.PinCategory=P->HasAnyPropertyFlags(CPF_ReturnParm)?(Operation==TEXT("length")?UEdGraphSchema_K2::PC_Int:UEdGraphSchema_K2::PC_Boolean):UEdGraphSchema_K2::PC_Wildcard;
    E.bIsReference=P->HasAllPropertyFlags(CPF_OutParm|CPF_ReferenceParm);E.bIsConst=P->HasAnyPropertyFlags(CPF_ConstParm);
    if(SetCall&&(P==SetParameter||AdditionalSets.ContainsByPredicate([P](const auto& E){return E.Key==P;})))E.ContainerType=EPinContainerType::Set;
    if(P==MapParameter){E.ContainerType=EPinContainerType::Map;E.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Wildcard;}if(P==ArrayParameter)E.ContainerType=EPinContainerType::Array;return SameType(Type,E);
}
// QT: exact Set5 native source within the existing Map node lifecycle.
bool FMapScope::SetSource()
{
 R.Stage=TEXT("set_native_source");Library.Reset(FindObject<UClass>(nullptr,TEXT("/Script/Engine.BlueprintSetLibrary")));if(!Raw()||!Keep(Library.Get()))return false;
 SchemaClass.Reset(FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.EdGraphSchema_K2")));if(!Raw()||!Keep(SchemaClass.Get()))return false;NodeClass.Reset(FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_CallFunction")));if(!Raw()||!Keep(NodeClass.Get()))return false;
 if(Library.Get()!=UBlueprintSetLibrary::StaticClass()||Library->GetClass()!=UClass::StaticClass()||!Library->HasAnyClassFlags(CLASS_Native)||Library->HasAnyClassFlags(CLASS_Abstract|CLASS_Interface|CLASS_Deprecated|CLASS_NewerVersionExists)||SchemaClass.Get()!=UEdGraphSchema_K2::StaticClass()||NodeClass.Get()!=UK2Node_CallFunction::StaticClass())return Reject(TEXT("set_native_class_unsupported"),TEXT("Exact loaded native Set library, K2 schema and stock CallFunction required."));
 for(UClass* C:{Library.Get(),SchemaClass.Get(),NodeClass.Get()})if(!Keep(C->GetPackage())||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||C->GetPackage()->IsDirty())return Reject(TEXT("set_native_package_changed"),TEXT("Existing clean compiled-in native packages required."));
 SchemaFlags=uint32(SchemaClass->ClassFlags);NodeClassFlags=uint32(NodeClass->ClassFlags);NodeClassSize=NodeClass->GetStructureSize();
 LibraryCDO.Reset(ExistingCDO(Library.Get(),TEXT("/Script/Engine.Default__BlueprintSetLibrary")));SchemaCDO.Reset(ExistingCDO(SchemaClass.Get(),TEXT("/Script/BlueprintGraph.Default__EdGraphSchema_K2")));if(!LibraryCDO.IsValid()||!SchemaCDO.IsValid())return Reject(TEXT("set_native_CDO_unavailable"),TEXT("Existing resolved SetLibrary/schema CDO required; no hidden warm."));
 UFunction* Returned=Library->FindFunctionByName(FName(*FunctionName));TStrongObjectPtr<UFunction> Immediate(Returned);if(!Raw()||!Keep(Returned))return false;Native=MoveTemp(Immediate);
 const bool Pure=Operation==TEXT("contains")||Operation==TEXT("length");
 if(Native->GetOuterUClass()!=Library.Get()||Native->GetFName()!=FName(*FunctionName)||!Native->HasAllFunctionFlags(FUNC_Native|FUNC_Static|FUNC_Public|FUNC_BlueprintCallable)||Native->HasAnyFunctionFlags(FUNC_BlueprintPure)!=Pure||Native->HasAnyFunctionFlags(FUNC_EditorOnly|FUNC_Delegate|FUNC_MulticastDelegate))return Reject(TEXT("set_native_function_unsupported"),TEXT("Exact declared runtime Set5 signature required."));
 FunctionFlags=uint32(Native->FunctionFlags);ClassFlags=uint32(Library->ClassFlags);FunctionSize=Native->GetStructureSize();
 const bool Binary=BinarySet();
 const TCHAR* SetGroup=Binary?TEXT("A|B|Result"):Operation==TEXT("add")?TEXT("TargetSet|NewItem"):Operation==TEXT("remove")?TEXT("TargetSet|Item"):Operation==TEXT("contains")?TEXT("TargetSet|ItemToFind"):Operation==TEXT("toarray")?TEXT("A|Result"):TEXT("TargetSet");
 const TCHAR* RefTerm=Operation==TEXT("add")?TEXT("NewItem"):Operation==TEXT("remove")?TEXT("Item"):Operation==TEXT("contains")?TEXT("ItemToFind"):TEXT("");
 for(const TCHAR* Name:{TEXT("Category"),TEXT("CustomThunk"),TEXT("SetParam"),TEXT("AutoCreateRefTerm"),TEXT("BlueprintThreadSafe"),TEXT("DisplayName"),TEXT("CompactNodeTitle"),TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")}){const FName MetadataKey(Name);const FString V=Native->GetMetaData(MetadataKey);if(!Raw())return false;const bool Present=Native->HasMetaData(MetadataKey);if(!Raw())return false;Metadata.Emplace(MetadataKey,V);MetadataPresent.Add(Present);}
 if(!Equal(Native->GetMetaData(TEXT("Category")),TEXT("Utilities|Set"))||!Equal(Native->GetMetaData(TEXT("CustomThunk")),TEXT("true"))||!Equal(Native->GetMetaData(TEXT("SetParam")),SetGroup)||!Equal(Native->GetMetaData(TEXT("AutoCreateRefTerm")),RefTerm))return Reject(TEXT("set_native_metadata_unsupported"),TEXT("Installed SetParam/AutoCreateRefTerm group differs."));
 for(const TCHAR* Name:{TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")})if(Native->HasMetaData(Name))return Reject(TEXT("set_native_metadata_unsupported"),TEXT("Special callable/ScriptName remapping is outside exact Set5."));
 TArray<FName> Expected{FName((Binary||Operation==TEXT("toarray"))?TEXT("A"):TEXT("TargetSet"))};if(Binary){Expected.Add(TEXT("B"));Expected.Add(TEXT("Result"));}if(*RefTerm)Expected.Add(FName(RefTerm));if(Operation==TEXT("remove")||Operation==TEXT("contains")||Operation==TEXT("length"))Expected.Add(TEXT("ReturnValue"));if(Operation==TEXT("toarray"))Expected.Add(TEXT("Result"));
 for(TFieldIterator<FProperty> It(Native.Get(),EFieldIterationFlags::IncludeDeprecated);It;++It){FProperty* P=*It;const int32 I=Parameters.Num();if(!Expected.IsValidIndex(I)||P->GetFName()!=Expected[I]||P->GetOwnerStruct()!=Native.Get()||P->GetOwnerProperty()!=P||P->ArrayDim!=1||!P->HasAnyPropertyFlags(CPF_Parm)||P->HasSetterOrGetter()||P->GetOffset_ForInternal()<0||P->GetOffset_ForInternal()+P->GetSize()>FunctionSize)return Reject(TEXT("set_native_property_role"),TEXT("Exact directly-owned ordered native parameter inventory required."));Parameters.Add(P);Fields.Add(MakeShared<FField>(P));
  if(I==0){SetParameter=CastField<FSetProperty>(P);if(!SetParameter)return Reject(TEXT("set_native_property_role"),TEXT("First parameter must be SetProperty."));SetElement=SetParameter->ElementProp;}
  else if(Binary){auto* Set=CastField<FSetProperty>(P);if(!Set||Set->GetClass()!=FSetProperty::StaticClass())return Reject(TEXT("set_native_property_role"),TEXT("Binary Set B/Result must be exact own SetProperty."));AdditionalSets.Emplace(Set,Set->ElementProp);}
  else if(Operation==TEXT("toarray")){ArrayParameter=CastField<FArrayProperty>(P);if(!ArrayParameter)return Reject(TEXT("set_native_property_role"),TEXT("ToArray result must be ArrayProperty."));ArrayInner=ArrayParameter->Inner;}
  else if(P->HasAnyPropertyFlags(CPF_ReturnParm)){const bool Length=Operation==TEXT("length");if(P->GetClass()!=(Length?FIntProperty::StaticClass():FBoolProperty::StaticClass())||P->GetSize()!=(Length?sizeof(int32):sizeof(bool)))return Reject(TEXT("set_native_return_role"),TEXT("Native int Length/bool return required."));}
  else if(P->GetClass()!=FIntProperty::StaticClass()||P->GetSize()!=sizeof(int32))return Reject(TEXT("set_native_item_role"),TEXT("Native placeholder item is int32, not an already-typed user value."));
 }
 if(Parameters.Num()!=Expected.Num()||Native->NumParms!=Parameters.Num()||!SetParameter||!SetElement)return Reject(TEXT("set_native_parameter_count"),TEXT("Complete exact Set5 native placeholder signature required."));
 TArray<TPair<FProperty*,FProperty*>> Nested{{SetParameter,SetElement},{ArrayParameter,ArrayInner}};for(const auto& E:AdditionalSets)Nested.Emplace(E.Key,E.Value);
 for(auto Pair:Nested)if(Pair.Key){auto* Child=Pair.Value;if(!Child||Child->GetClass()!=FIntProperty::StaticClass()||Child->GetOwner<FProperty>()!=Pair.Key||Child->GetOwnerProperty()!=Pair.Key||Child->GetOwnerStruct()!=Native.Get()||Child->ArrayDim!=1||Child->GetSize()!=sizeof(int32)||Child->HasSetterOrGetter())return Reject(TEXT("set_native_element_role"),TEXT("Exact owned int32 Set.ElementProp/Array.Inner placeholder required."));Fields.Add(MakeShared<FField>(Child));}
 if(Binary&&AdditionalSets.Num()!=2)return Reject(TEXT("set_native_parameter_count"),TEXT("Binary Set requires A/B/Result and three owned ElementProp descriptors."));
 SourceFixed=true;if(!Raw())return false;for(FProperty* P:Parameters){FEdGraphPinType T;const bool Converted=static_cast<const UEdGraphSchema_K2*>(SchemaCDO.Get())->ConvertPropertyToPinType(P,T);if(!Raw())return false;if(!Converted||!ExpectedType(P,T))return Reject(TEXT("set_native_schema_type"),TEXT("Native placeholder-to-wildcard schema projection differs."));}
 R.Data->SetBoolField(TEXT("native_set_contract_verified"),true);R.Data->SetObjectField(TEXT("native_set_source_before"),SourceJson());return Raw();
}


bool FMapScope::Source()
{
    if(SetCall)return SetSource();
    R.Stage=TEXT("map_native_source");
    Library.Reset(FindObject<UClass>(nullptr,LibraryPath));if(!Raw()||!Keep(Library.Get()))return false;
    SchemaClass.Reset(FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.EdGraphSchema_K2")));if(!Raw()||!Keep(SchemaClass.Get()))return false;
    if((!DoubleValue&&!Neutral)){Vector.Reset(FindObject<UScriptStruct>(nullptr,VectorPath));if(!Raw()||!Keep(Vector.Get()))return false;}
    NodeClass.Reset(FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_CallFunction")));if(!Raw()||!Keep(NodeClass.Get())||!Keep(NodeClass->GetPackage()))return false;
    SchemaFlags=uint32(SchemaClass->ClassFlags);NodeClassFlags=uint32(NodeClass->ClassFlags);NodeClassSize=NodeClass->GetStructureSize();
    if(NodeClass.Get()!=UK2Node_CallFunction::StaticClass()||!NodeClass->HasAnyClassFlags(CLASS_Native)||NodeClass->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists))return Reject(TEXT("map_native_source_unsupported"),TEXT("Exact already-loaded allocatable stock CallFunction class required."));
    for(UObject* O:{static_cast<UObject*>(Library.Get()),static_cast<UObject*>(SchemaClass.Get()),static_cast<UObject*>(Vector.Get())})if(O&&!Keep(O->GetPackage()))return false;
    if(Library.Get()!=UBlueprintMapLibrary::StaticClass()||SchemaClass.Get()!=UEdGraphSchema_K2::StaticClass()||((!DoubleValue&&!Neutral)&&Vector.Get()!=TBaseStructure<FVector>::Get())||Library->GetClass()!=UClass::StaticClass()||!Equal(Library->GetPathName(),LibraryPath)||!Library->HasAnyClassFlags(CLASS_Native)||Library->HasAnyClassFlags(CLASS_Interface|CLASS_Deprecated|CLASS_NewerVersionExists)||Library->GetPackage()->GetName()!=TEXT("/Script/Engine")||Library->GetPackage()->IsDirty()||Library->GetPackage()->GetPackageFlags()!=uint32(PKG_CompiledIn)||SchemaClass->GetPackage()->GetName()!=TEXT("/Script/BlueprintGraph")||((!DoubleValue&&!Neutral)&&(!Equal(Vector->GetPathName(),VectorPath)||Vector->GetPackage()->GetName()!=TEXT("/Script/CoreUObject")||Vector->GetPackage()->IsDirty()||Vector->GetStructureSize()!=sizeof(FVector)||Vector->GetMinAlignment()!=alignof(FVector)||(Vector->StructFlags&STRUCT_Native)==0))||!SchemaClass->HasAnyClassFlags(CLASS_Native))return Reject(TEXT("map_native_source_unsupported"),TEXT("Exact loaded native Map library/K2 schema/Vector source required."));
    if(!Raw())return false;
    LibraryCDO.Reset(ExistingCDO(Library.Get(),DefaultPath));SchemaCDO.Reset(ExistingCDO(SchemaClass.Get(),TEXT("/Script/BlueprintGraph.Default__EdGraphSchema_K2")));
    if(!Loaded(LibraryCDO.Get())||!Loaded(SchemaCDO.Get()))return Reject(TEXT("map_native_default_unavailable"),TEXT("Already-created resolved native CDOs required; no creation/load."));
    Native.Reset(Library->FindFunctionByName(FName(*FunctionName)));if(!Raw()||!Keep(Native.Get()))return false;
    if(Native->GetOuterUClass()!=Library.Get()||Native->GetFName()!=FName(*FunctionName))return Reject(TEXT("map_native_function_missing"),TEXT("Exact declared function required; no inherited/remapped resolution."));
    FunctionFlags=uint32(Native->FunctionFlags);ClassFlags=uint32(Library->ClassFlags);FunctionSize=Native->GetStructureSize();VectorSize=Neutral?0:DoubleValue?sizeof(double):Vector->GetStructureSize();VectorAlign=Neutral?0:DoubleValue?alignof(double):Vector->GetMinAlignment();VectorFlags=(DoubleValue||Neutral)?0U:uint32(Vector->StructFlags);
    const uint32 Expected=uint32(FUNC_Final|FUNC_RequiredAPI|FUNC_Native|FUNC_Static|FUNC_Public|FUNC_HasOutParms|FUNC_BlueprintCallable)|((Operation==TEXT("find")||Operation==TEXT("contains")||Operation==TEXT("length"))?uint32(FUNC_BlueprintPure):0U);
    R.Data->SetStringField(TEXT("native_function_flags_expected_string"),LexToString(Expected));R.Data->SetStringField(TEXT("native_function_flags_actual_string"),LexToString(FunctionFlags));
    if(FunctionFlags!=Expected)return Reject(TEXT("map_native_signature_mismatch"),TEXT("SDK symbolic function flags differ; no node created."));
    for(const TCHAR* Name:{TEXT("Category"),TEXT("CustomThunk"),TEXT("MapParam"),TEXT("MapKeyParam"),TEXT("MapValueParam"),TEXT("AutoCreateRefTerm"),TEXT("BlueprintThreadSafe"),TEXT("DisplayName"),TEXT("CompactNodeTitle"),TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")}){const FName K(Name);Metadata.Emplace(K,Native->GetMetaData(K));MetadataPresent.Add(Native->HasMetaData(K));}
    const bool Length=Operation==TEXT("length"),Clear=Operation==TEXT("clear"),Find=Operation==TEXT("find"),Contains=Operation==TEXT("contains"),KeyOnly=Contains||Operation==TEXT("remove"),Keys=Operation==TEXT("keys"),ArrayResult=Keys||Operation==TEXT("values");
    if(Length){
        if(!Equal(Native->GetMetaData(TEXT("DisplayName")),TEXT("Length"))||!Equal(Native->GetMetaData(TEXT("CompactNodeTitle")),TEXT("LENGTH"))||!Equal(Native->GetMetaData(TEXT("Category")),TEXT("Utilities|Map"))||!Equal(Native->GetMetaData(TEXT("CustomThunk")),TEXT("true"))||!Equal(Native->GetMetaData(TEXT("MapParam")),TEXT("TargetMap"))||Native->HasMetaData(TEXT("MapKeyParam"))||Native->HasMetaData(TEXT("MapValueParam"))||Native->HasMetaData(TEXT("AutoCreateRefTerm"))||!Native->HasMetaData(TEXT("BlueprintThreadSafe"))||!Native->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return Reject(TEXT("map_native_metadata_mismatch"),TEXT("Exact key/value-free pure Map Length metadata differs."));
        const FName Keywords(TEXT("Keywords"));Metadata.Emplace(Keywords,Native->GetMetaData(Keywords));MetadataPresent.Add(Native->HasMetaData(Keywords));if(!Native->HasMetaData(Keywords)||!Equal(Native->GetMetaData(Keywords),TEXT("num size count")))return Reject(TEXT("map_native_metadata_mismatch"),TEXT("Installed Map Length keyword metadata differs."));
    }
    else if(ArrayResult){
        const TCHAR* Output=Keys?TEXT("Keys"):TEXT("Values");
        if(!Equal(Native->GetMetaData(TEXT("DisplayName")),Output)||!Equal(Native->GetMetaData(TEXT("CompactNodeTitle")),Keys?TEXT("KEYS"):TEXT("VALUES"))||!Equal(Native->GetMetaData(TEXT("Category")),TEXT("Utilities|Map"))||!Equal(Native->GetMetaData(TEXT("CustomThunk")),TEXT("true"))||!Equal(Native->GetMetaData(TEXT("MapParam")),TEXT("TargetMap"))||Native->HasMetaData(TEXT("MapKeyParam"))!=Keys||Native->HasMetaData(TEXT("MapValueParam"))==Keys||!Equal(Native->GetMetaData(Keys?TEXT("MapKeyParam"):TEXT("MapValueParam")),Output)||!Equal(Native->GetMetaData(TEXT("AutoCreateRefTerm")),Output)||Native->HasMetaData(TEXT("BlueprintThreadSafe"))||!Library->HasMetaData(TEXT("BlueprintThreadSafe"))||!Library->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return Reject(TEXT("map_native_metadata_mismatch"),TEXT("Exact array-output Map metadata differs."));
    }
    else if(KeyOnly){
        if(!Equal(Native->GetMetaData(TEXT("DisplayName")),Contains?TEXT("Contains"):TEXT("Remove"))||!Equal(Native->GetMetaData(TEXT("CompactNodeTitle")),Contains?TEXT("CONTAINS"):TEXT("REMOVE"))||!Equal(Native->GetMetaData(TEXT("Category")),TEXT("Utilities|Map"))||!Equal(Native->GetMetaData(TEXT("CustomThunk")),TEXT("true"))||!Equal(Native->GetMetaData(TEXT("MapParam")),TEXT("TargetMap"))||!Equal(Native->GetMetaData(TEXT("MapKeyParam")),TEXT("Key"))||Native->HasMetaData(TEXT("MapValueParam"))||!Equal(Native->GetMetaData(TEXT("AutoCreateRefTerm")),TEXT("Key"))||Native->HasMetaData(TEXT("BlueprintThreadSafe"))!=Contains||(Contains&&!Native->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())||!Library->HasMetaData(TEXT("BlueprintThreadSafe"))||!Library->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())return Reject(TEXT("map_native_metadata_mismatch"),TEXT("Exact key-only Map metadata differs."));
    }
    else if(Native->GetMetaData(TEXT("DisplayName"))!=(Clear?TEXT("Clear"):Find?TEXT("Find"):TEXT("Add"))||Native->GetMetaData(TEXT("CompactNodeTitle"))!=(Clear?TEXT("CLEAR"):Find?TEXT("FIND"):TEXT("ADD"))||Native->GetMetaData(TEXT("Category"))!=TEXT("Utilities|Map")||Native->GetMetaData(TEXT("CustomThunk"))!=TEXT("true")||Native->GetMetaData(TEXT("MapParam"))!=TEXT("TargetMap")||Native->HasMetaData(TEXT("MapKeyParam"))==Clear||Native->HasMetaData(TEXT("MapValueParam"))==Clear||(!Clear&&(Native->GetMetaData(TEXT("MapKeyParam"))!=TEXT("Key")||Native->GetMetaData(TEXT("MapValueParam"))!=TEXT("Value")||Native->GetMetaData(TEXT("AutoCreateRefTerm"))!=TEXT("Key, Value")))||(Clear&&Native->HasMetaData(TEXT("AutoCreateRefTerm")))||Native->HasMetaData(TEXT("BlueprintThreadSafe"))!=Find||(Find&&!Native->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty())||(!Library->HasMetaData(TEXT("BlueprintThreadSafe"))||!Library->GetMetaData(TEXT("BlueprintThreadSafe")).IsEmpty()))return Reject(TEXT("map_native_metadata_mismatch"),TEXT("Exact Map/category/wildcard/ref-term metadata differs."));
    for(const TCHAR* Name:{TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("BlueprintInternalUseOnly"),TEXT("WorldContext"),TEXT("ScriptName")})if(Native->HasMetaData(Name))return Reject(TEXT("map_native_metadata_mismatch"),TEXT("Unexpected native execution/lookup metadata."));
    for(TFieldIterator<FProperty> It(Native.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It){if(Parameters.Num()>=4||!It->HasAnyPropertyFlags(CPF_Parm))return Reject(TEXT("map_native_signature_mismatch"),TEXT("Unexpected declared field."));Parameters.Add(*It);Fields.Add(MakeShared<FField>(*It));}
    if(Parameters.Num()!=((ArrayResult||Length)?2:Clear?1:Find?4:3)||Native->NumParms!=Parameters.Num())return Reject(TEXT("map_native_signature_mismatch"),TEXT("Ordered parameter count differs."));
    constexpr uint64 DirectionBits=uint64(CPF_Parm|CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm|CPF_ReturnParm);
    for(int32 I=0;I<Parameters.Num();++I){auto* P=Parameters[I];const bool Return=(Length&&I==1)||(Find&&I==3)||(KeyOnly&&I==2),Out=(Find&&I==2)||(ArrayResult&&I==1);const FName Name(Return?TEXT("ReturnValue"):ArrayResult&&I==1?(Keys?TEXT("Keys"):TEXT("Values")):I==0?TEXT("TargetMap"):I==1?TEXT("Key"):TEXT("Value"));
        const uint64 ExpectedBits=uint64(CPF_Parm|CPF_OutParm)|(Return?uint64(CPF_ReturnParm):Out?0ULL:uint64(CPF_ReferenceParm|CPF_ConstParm));
        if(P->GetOwnerStruct()!=Native.Get()||((KeyOnly||ArrayResult||Length)&&P->GetOwner<FProperty>()!=nullptr)||P->GetFName()!=Name||P->ArrayDim!=1||P->HasSetterOrGetter()||(uint64(P->GetPropertyFlags())&DirectionBits)!=ExpectedBits||P->HasAnyPropertyFlags(CPF_EditorOnly|CPF_AdvancedDisplay|CPF_RequiredParm)||P->GetOffset_ForInternal()<0||P->GetSize()<=0||P->GetOffset_ForInternal()+P->GetSize()>FunctionSize||Native->HasMetaData(FName(*(TEXT("CPP_Default_")+P->GetName()))))return Reject(TEXT("map_native_signature_mismatch"),TEXT("Ordered property owner/name/direction/default/layout differs."));
        if(I==0){MapParameter=CastField<FMapProperty>(P);if(!MapParameter||P->GetClass()!=FMapProperty::StaticClass())return Reject(TEXT("map_native_signature_mismatch"),TEXT("Exact Map placeholder property required."));Key=MapParameter->KeyProp;Value=MapParameter->ValueProp;
            for(FProperty* Child:{Key,Value}){if(!Child||Child->GetClass()!=FIntProperty::StaticClass()||Child->ArrayDim!=1||Child->GetOwnerProperty()!=MapParameter||((KeyOnly||ArrayResult||Length)&&Child->GetOwner<FProperty>()!=MapParameter)||Child->GetSize()!=sizeof(int32)||Child->HasSetterOrGetter())return Reject(TEXT("map_native_signature_mismatch"),TEXT("Exact int32 Map placeholder children required."));Fields.Add(MakeShared<FField>(Child));}}
        else if(ArrayResult){
            if(P->GetClass()!=FArrayProperty::StaticClass())return Reject(TEXT("map_native_signature_mismatch"),TEXT("Exact array output property required."));ArrayParameter=static_cast<FArrayProperty*>(P);ArrayInner=ArrayParameter->Inner;
            if(!ArrayInner||ArrayInner->GetClass()!=FIntProperty::StaticClass()||ArrayInner->GetOwner<FProperty>()!=ArrayParameter||ArrayInner->GetOwnerProperty()!=ArrayParameter||ArrayInner->GetOwnerStruct()!=Native.Get()||ArrayInner->ArrayDim!=1||ArrayInner->GetSize()!=sizeof(int32)||ArrayInner->GetOffset_ForInternal()<0||ArrayInner->HasSetterOrGetter())return Reject(TEXT("map_native_signature_mismatch"),TEXT("Exact directly-owned int32 array placeholder Inner required."));Fields.Add(MakeShared<FField>(ArrayInner));
        }
        else if(P->GetClass()!=((Return&&!Length)?FBoolProperty::StaticClass():FIntProperty::StaticClass())||((KeyOnly||Length)&&P->GetSize()!=((Return&&!Length)?sizeof(bool):sizeof(int32))))return Reject(TEXT("map_native_signature_mismatch"),TEXT("Scalar placeholder/return class differs."));
    }
    const FName Axes[]={TEXT("X"),TEXT("Y"),TEXT("Z")};const int32 Offsets[]={int32(STRUCT_OFFSET(FVector,X)),int32(STRUCT_OFFSET(FVector,Y)),int32(STRUCT_OFFSET(FVector,Z))};
    if((!DoubleValue&&!Neutral))for(TFieldIterator<FProperty> It(Vector.Get(),EFieldIteratorFlags::ExcludeSuper);It;++It){const int32 I=VectorFields.Num();auto* P=*It;if(I>=3||P->GetClass()!=FDoubleProperty::StaticClass()||P->GetFName()!=Axes[I]||P->ArrayDim!=1||P->GetOwnerStruct()!=Vector.Get()||P->GetSize()!=sizeof(double)||P->GetOffset_ForInternal()!=Offsets[I]||P->HasSetterOrGetter())return Reject(TEXT("map_vector_layout_mismatch"),TEXT("Exact native three-double Vector fields required."));VectorFields.Add(P);Fields.Add(MakeShared<FField>(P));}
    if(VectorFields.Num()!=((DoubleValue||Neutral)?0:3))return Reject(TEXT("map_vector_layout_mismatch"),TEXT("Native Vector fields missing."));
    SourceFixed=true;if(!Raw())return false;
    for(FProperty* P:Parameters){FEdGraphPinType Type;const bool Converted=static_cast<const UEdGraphSchema_K2*>(SchemaCDO.Get())->ConvertPropertyToPinType(P,Type);if(!Raw())return false;if(!Converted||!ExpectedType(P,Type))return Reject(TEXT("map_native_pin_type_mismatch"),TEXT("Schema initial wildcard/ref/terminal type differs."));}
    R.Data->SetBoolField(TEXT("native_map_contract_verified"),true);R.Data->SetBoolField(TEXT("map_prior_numeric_capture_verified"),false);
    R.Data->SetStringField(TEXT("map_source_guard_basis"),TEXT("Installed UE5.8.3 symbolic function flags, exact ordered placeholder semantic layout/metadata and frozen full observed property flags. Prior numeric runtime capture not claimed."));
    R.Data->SetObjectField(TEXT("native_map_source_before"),SourceJson());return Raw();
}

bool FMapScope::Target(const FString& Input,const FString& Name,const FString& Id,const FString& RequestedKind)
{
    R.Stage=TEXT("map_target");if(!Raw())return false;
    if(!Function::Pin::SafeName(Input)||!Input.StartsWith(TEXT("/Game/"))||Input.Contains(TEXT(":"))||Input.Contains(TEXT(".."))||Input.Contains(TEXT("\\")))return Reject(TEXT("map_blueprint_path_invalid"),TEXT("Exact bounded Game Blueprint object/package path required."));
    const FString Path=Input.Contains(TEXT("."))?Input:Input+TEXT(".")+FPackageName::GetShortName(Input);
    Blueprint.Reset(FindObject<UBlueprint>(nullptr,*Path));if(!Raw()||!Keep(Blueprint.Get()))return false;
    PresetLibrary=Blueprint->BlueprintType==BPTYPE_FunctionLibrary;
    if(!Equal(Blueprint->GetPathName(),Path)||(PresetLibrary?!PresetCurrent():Blueprint->BlueprintType!=BPTYPE_Normal)||Blueprint->bBeingCompiled||Blueprint->bQueuedForCompilation||Blueprint->bIsRegeneratingOnLoad||Blueprint->bSuppressStructurallyModified||Blueprint->Status==BS_BeingCreated)return Reject(TEXT("map_blueprint_state_unsupported"),TEXT("Loaded idle normal Blueprint with current skeleton required."));
    TargetPackage=Blueprint->GetPackage();if(!Keep(TargetPackage,true)||TargetPackage->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor|PKG_CompiledIn))return Reject(TEXT("map_blueprint_state_unsupported"),TEXT("Persistent Game Blueprint package required."));
    auto PH=Blueprint->ParentClass;auto GH=Blueprint->GeneratedClass;auto SH=Blueprint->SkeletonGeneratedClass;
    if(!PH.GetGCPtr().IsResolved()||!GH.GetGCPtr().IsResolved()||!SH.GetGCPtr().IsResolved())return Reject(TEXT("map_blueprint_state_unsupported"),TEXT("Resolved current generated/skeleton classes required; compile explicitly if absent."));
    ParentHandle=PH.GetGCPtr();GeneratedHandle=GH.GetGCPtr();SkeletonHandle=SH.GetGCPtr();Status=Blueprint->Status;ClassesObserved=true;
    UClass* ParentRaw=ParentHandle.Get();if(IsEditorGameThread()&&!IsGarbageCollecting()&&Loaded(ParentRaw))Parent.Reset(ParentRaw);if(!Raw()||!Keep(Parent.Get()))return false;
    UClass* GeneratedRaw=GeneratedHandle.Get();if(IsEditorGameThread()&&!IsGarbageCollecting()&&Loaded(GeneratedRaw))Generated.Reset(GeneratedRaw);if(!Raw()||!Keep(Generated.Get()))return false;
    UClass* SkeletonRaw=SkeletonHandle.Get();if(IsEditorGameThread()&&!IsGarbageCollecting()&&Loaded(SkeletonRaw))Skeleton.Reset(SkeletonRaw);if(!Raw()||!Keep(Skeleton.Get()))return false;
    for(auto It=Parent->GetSuperStructIterator();It;++It){UClass* C=Cast<UClass>(*It);if(!Keep(C)||!Keep(C->GetPackage())||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Interface|CLASS_Deprecated|CLASS_NewerVersionExists)||C->IsEditorOnly()||(C->GetPackage()->GetName()!=TEXT("/Script/Engine")&&C->GetPackage()->GetName()!=TEXT("/Script/CoreUObject")))return Reject(TEXT("map_parent_unsupported"),TEXT("Loaded native Engine/CoreUObject parent chain required."));ParentChain.Add(C);ParentFlags.Add(uint32(C->ClassFlags));ParentSizes.Add(C->GetStructureSize());}
    Status=Blueprint->Status;ClassesFixed=true;if(!Raw())return false;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const auto GC=Generated->ClassDefaultObject;const auto SC=Skeleton->ClassDefaultObject;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    if(!GC.IsResolved()||!SC.IsResolved())return Reject(TEXT("map_blueprint_state_unsupported"),TEXT("Target CDO handles must already be resolved."));
    UObject* GD=nullptr;UObject* SD=nullptr;
    if(PresetLibrary)
    {
        const auto CaptureExisting=[&](UClass* C,const TObjectPtr<UObject>& Handle,UObject*& ResultCDO)
        {
            if(Handle==nullptr){ResultCDO=nullptr;return Raw();}
            const FString CurrentCDOPath=Handle.GetPathName();if(!Raw())return false;
            ResultCDO=StaticFindObject(nullptr,nullptr,*CurrentCDOPath);if(!Raw())return false;
            return Loaded(ResultCDO)&&Handle==ResultCDO&&ResultCDO->GetClass()==C&&ResultCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)&&Keep(ResultCDO)&&Raw();
        };
        if(!CaptureExisting(Generated.Get(),GC,GD)||!CaptureExisting(Skeleton.Get(),SC,SD))return Reject(TEXT("map_blueprint_state_unsupported"),TEXT("Current existing BFL CDO handle/identity changed; no CDO created or values sampled."));
    }
    else
    {
        GD=StaticFindObject(nullptr,TargetPackage,*(TEXT("Default__")+Generated->GetName()));if(!Raw())return false;
        SD=StaticFindObject(nullptr,TargetPackage,*(TEXT("Default__")+Skeleton->GetName()));if(!Raw())return false;
        if(GC!=GD||SC!=SD||!Loaded(GD)||!Loaded(SD)||!Keep(GD)||!Keep(SD))return Reject(TEXT("map_blueprint_state_unsupported"),TEXT("Exact already-created target CDO roles required; values are not read."));
    }
    GeneratedCDO.Reset(GD);SkeletonCDO.Reset(SD);Status=Blueprint->Status;
    if(!Function::Pin::SafeName(Name,true)||!Function::Pin::SafeName(Id,true)||!Function::Pin::SafeName(RequestedKind,true))return Reject(TEXT("map_graph_selector_invalid"),TEXT("Bounded graph selectors required."));
    FGuid Wanted;if(!Id.IsEmpty()&&(!FGuid::Parse(Id,Wanted)||!Wanted.IsValid()))return Reject(TEXT("map_graph_selector_invalid"),TEXT("Nonzero graph GUID required."));
    FString K=RequestedKind.ToLower();if(K==TEXT("eventgraph")||K==TEXT("event_graph")||K==TEXT("ubergraph")||K==TEXT("uber_graph"))K=TEXT("event");else if(K==TEXT("functiongraph")||K==TEXT("function_graph"))K=TEXT("function");else if(K==TEXT("macrograph")||K==TEXT("macro_graph"))K=TEXT("macro");
    if(K.IsEmpty()&&Name.IsEmpty()&&Id.IsEmpty())K=TEXT("event");
    if(!K.IsEmpty()&&K!=TEXT("any")&&K!=TEXT("event")&&K!=TEXT("function")&&K!=TEXT("macro"))return Reject(TEXT("map_graph_type_unsupported"),TEXT("Existing event/function/macro graph required."));
    TArray<UEdGraph*> All;Blueprint->GetAllGraphs(All);if(!Raw())return false;if(All.Num()>256)return Reject(TEXT("map_graph_budget_exceeded"),TEXT("At most 256 owned graphs."));
    for(UEdGraph* G:All){if(!Loaded(G))return Reject(TEXT("map_graph_state_unsupported"),TEXT("All owned graph roles must be loaded."));
        if(G->GetOuter()!=Blueprint.Get()||(!Name.IsEmpty()&&!G->GetName().Equals(Name,ESearchCase::IgnoreCase))||(!Id.IsEmpty()&&G->GraphGuid!=Wanted)||(!K.IsEmpty()&&K!=TEXT("any")&&Kind(G)!=K))continue;
        if(Graph.IsValid())return Reject(TEXT("map_graph_ambiguous"),TEXT("Select one owned graph by name/GUID."));Graph.Reset(G);}
    if(!Graph.IsValid()||!Graph->GraphGuid.IsValid()||!Graph->bEditable||Graph->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||Graph->Nodes.Num()>=2048||Kind(Graph.Get())==TEXT("unknown"))return Reject(TEXT("map_graph_state_unsupported"),TEXT("Direct owned idle K2 graph unavailable."));
    auto SchemaHandle=Graph->Schema;if(!SchemaHandle.GetGCPtr().IsResolved()||SchemaHandle.GetGCPtr()!=SchemaClass.Get())return Reject(TEXT("map_graph_state_unsupported"),TEXT("Exact standard K2 schema required."));
    if(!Keep(Graph.Get()))return false;GraphGuid=Graph->GraphGuid;
    if(PresetLibrary)
    {
        if(Graph->GetFName()!=TEXT("PresetLoader")||Kind(Graph.Get())!=TEXT("function"))return Reject(TEXT("map_preset_loader_graph_unsupported"),TEXT("Function-library Map nodes are limited to the exact own PresetLoader function; no general BFL target."));
        const auto EntryInventory=Graph->Nodes;for(const auto& Handle:EntryInventory){if(!Handle.IsResolved())return Reject(TEXT("map_preset_loader_entry_unavailable"),TEXT("Resolved own function node inventory required."));UEdGraphNode* Candidate=Handle.Get();if(!Raw()||Graph->Nodes!=EntryInventory)return false;if(Candidate&&Candidate->GetClass()==UK2Node_FunctionEntry::StaticClass()){if(PresetEntry.IsValid())return Reject(TEXT("map_preset_loader_entry_unavailable"),TEXT("One current own Entry required."));PresetEntry.Reset(static_cast<UK2Node_FunctionEntry*>(Candidate));if(!Keep(PresetEntry.Get())||!Raw())return false;}}
        if(!PresetEntry.IsValid())return Reject(TEXT("map_preset_loader_entry_unavailable"),TEXT("One current own Entry required."));PresetEntryReference=PresetEntry->FunctionReference;PresetEntryFlags=PresetEntry->GetExtraFlags();FMemberReference ExpectedEntry;ExpectedEntry.SetExternalMember(Graph->GetFName(),nullptr,PresetEntryReference.GetMemberGuid());
        if(!PresetEntryReference.IsSameReference(ExpectedEntry)||PresetEntryReference.IsSelfContext()||PresetEntryReference.IsDeprecated()||!PresetEntry->CustomGeneratedFunctionName.IsNone()||!(PresetEntryFlags&FUNC_Static))return Reject(TEXT("map_preset_loader_entry_unsupported"),TEXT("Exact static own external-null Entry binding required."));PresetEntryFixed=true;
        R.Data->SetStringField(TEXT("function_library_profile"),TEXT("preset_loader"));R.Data->SetStringField(TEXT("target_context"),TEXT("preset_loader_library"));R.Data->SetBoolField(TEXT("authored_world_context_edge_assumed"),false);R.Data->SetBoolField(TEXT("target_generated_cdo_available"),GD!=nullptr);R.Data->SetBoolField(TEXT("target_skeleton_cdo_available"),SD!=nullptr);
    }
    TargetFixed=true;if(!Raw())return false;if(PresetLibrary)R.Data->SetBoolField(TEXT("automatic_entry_world_context_current_verified"),true);
    if(Neutral?!Native->HasAnyFunctionFlags(FUNC_BlueprintPure):(Operation!=TEXT("find")&&Operation!=TEXT("length"))){const bool Allowed=static_cast<const UEdGraphSchema_K2*>(SchemaCDO.Get())->DoesGraphSupportImpureFunctions(Graph.Get());if(!Raw())return false;if(!Allowed)return Reject(TEXT("map_graph_operation_rejected"),TEXT("Graph does not support imperative Map nodes."));}
    Functions=Blueprint->FunctionGraphs;Macros=Blueprint->MacroGraphs;Ubergraphs=Blueprint->UbergraphPages;Events=Blueprint->EventGraphs;Delegates=Blueprint->DelegateSignatureGraphs;
    Variables=Blueprint->NewVariables;GeneratedVariables=Blueprint->GeneratedVariables;Interfaces=Blueprint->ImplementedInterfaces;
    int32 TotalNodes=0,TotalPins=0;TSet<FGuid> GraphIds;
    for(UEdGraph* G:All){if(!Loaded(G)||!G->GraphGuid.IsValid()||GraphIds.Contains(G->GraphGuid)||G->Nodes.Num()>2048)return Reject(TEXT("map_prior_state_unobservable"),TEXT("Owned graph identities/budget invalid."));GraphIds.Add(G->GraphGuid);
        auto Old=MakeShared<FOldGraph>(G);if(!Keep(G))return false;TSet<FGuid> NodeIds;
        // Copy wrappers before tracked reads so no array iterator survives them.
        const auto Entries=G->Nodes;
        for(const auto& Entry:Entries){if(++TotalNodes>8192||!Entry.IsResolved())return Reject(TEXT("map_prior_state_unobservable"),TEXT("Node inventory unresolved or over budget."));
            UEdGraphNode* N=Entry.Get();if(!Raw())return false;if(G->Nodes!=Entries)return Reject(TEXT("map_prior_state_changed"),TEXT("Node inventory changed during observation."),true);
            if(!Loaded(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid()||NodeIds.Contains(N->NodeGuid)||N->Pins.Num()>512)return Reject(TEXT("map_prior_state_unobservable"),TEXT("Existing node identity invalid."));NodeIds.Add(N->NodeGuid);
            auto Saved=MakeShared<FOldNode>(N);TSet<FGuid> PinIds;
            for(UEdGraphPin* P:N->Pins){if(++TotalPins>16384||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||PinIds.Contains(P->PinId)||!P->DefaultObject.IsResolved())return Reject(TEXT("map_prior_state_unobservable"),TEXT("Existing physical pin identity unavailable."));PinIds.Add(P->PinId);OldPinAddresses.Add(P);Saved->Pins.Add(MakeShared<FPin>(P));}
            Old->Nodes.Add(Saved);
        }OldGraphs.Add(Old);
    }
    InventoryFixed=true;if(!Raw())return false;
    // A tracked default getter is fenced before reading any next pin/object.
    for(const auto& Old:OldGraphs)for(const auto& Saved:Old->Nodes)for(const auto& P:Saved->Pins){
        UObject* D=P->Address->DefaultObject.Get();TStrongObjectPtr<UObject> Held(Ready()&&Loaded(D)?D:nullptr);if(!Raw())return false;
        if(D){if(!Loaded(D)||!Keep(D))return false;P->ObjectHold.Reset(D);}
        for(UEdGraphPin* Other:P->Links)if(!Other||!OldPinAddresses.Contains(Other)||Other->WasTrashed()||!Other->LinkedTo.Contains(P->Address))return Reject(TEXT("map_prior_state_unobservable"),TEXT("Existing physical links must be reciprocal."));
    }
    R.Data->SetStringField(TEXT("resolved_blueprint_path"),Path);R.Data->SetBoolField(TEXT("loaded_during_operation"),false);R.Data->SetObjectField(TEXT("before"),TargetJson());return Raw();
}
TSharedRef<FJsonObject> FMapScope::TypeJson(const FEdGraphPinType& T)const
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("category"),T.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());
    UObject* P=T.PinSubCategoryObject.Get();J->SetStringField(TEXT("type_object"),Loaded(P)?P->GetPathName():FString());J->SetNumberField(TEXT("container_type"),int32(T.ContainerType));
    J->SetBoolField(TEXT("is_reference"),T.bIsReference);J->SetBoolField(TEXT("is_const"),T.bIsConst);J->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);J->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("serialize_as_single_precision_float"),T.bSerializeAsSinglePrecisionFloat);
    auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());V->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());UObject* O=T.PinValueType.TerminalSubCategoryObject.Get();V->SetStringField(TEXT("type_object"),Loaded(O)?O->GetPathName():FString());V->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);V->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);V->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);J->SetObjectField(TEXT("terminal"),V);return J;
}
TSharedRef<FJsonObject> FMapScope::SourceJson()const
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("class_path"),SourcePath());J->SetStringField(TEXT("function_path"),Native->GetPathName());J->SetStringField(TEXT("function_flags_string"),LexToString(FunctionFlags));J->SetStringField(TEXT("class_flags_string"),LexToString(ClassFlags));J->SetStringField(TEXT("existing_cdo_path"),SourceDefaultPath());J->SetBoolField(TEXT("cdo_creation_requested"),false);J->SetBoolField(TEXT("cdo_values_sampled"),false);
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& F:Fields){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),F->Name.ToString());Row->SetStringField(TEXT("field_class"),F->Class->GetName());Row->SetStringField(TEXT("property_flags_string"),LexToString(F->Flags));Row->SetNumberField(TEXT("array_dim"),F->Dim);Row->SetNumberField(TEXT("offset"),F->Offset);Row->SetNumberField(TEXT("size"),F->Size);Rows.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("fields"),Rows);
    auto M=MakeShared<FJsonObject>();for(int32 I=0;I<Metadata.Num();++I){auto Row=MakeShared<FJsonObject>();Row->SetBoolField(TEXT("present"),MetadataPresent[I]);Row->SetStringField(TEXT("value"),Metadata[I].Value);M->SetObjectField(Metadata[I].Key.ToString(),Row);}J->SetObjectField(TEXT("metadata"),M);
    if(ArrayParameter){auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("parameter_name"),ArrayParameter->GetName());A->SetStringField(TEXT("property_path"),ArrayParameter->GetPathName());A->SetStringField(TEXT("property_class"),TEXT("ArrayProperty"));A->SetStringField(TEXT("inner_path"),ArrayInner->GetPathName());A->SetStringField(TEXT("inner_class"),TEXT("IntProperty"));A->SetBoolField(TEXT("direct_owner_and_current_slot_verified"),true);A->SetBoolField(TEXT("native_int_placeholder_is_typed_output"),false);J->SetObjectField(TEXT("array_output_source"),A);}
    if(!Neutral){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("struct_path"),DoubleValue?TEXT(""):VectorPath);if(DoubleValue){V->SetStringField(TEXT("category"),TEXT("real"));V->SetStringField(TEXT("subcategory"),TEXT("double"));V->SetBoolField(TEXT("scalar_property_required_after_link"),true);}V->SetNumberField(TEXT("size"),VectorSize);V->SetNumberField(TEXT("alignment"),VectorAlign);V->SetBoolField(TEXT("runtime_values_sampled"),false);J->SetObjectField(TEXT("requested_value_source"),V);}return J;
}
TSharedRef<FJsonObject> FMapScope::TargetJson()const
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("blueprint_path"),Blueprint->GetPathName());J->SetStringField(TEXT("graph_path"),Graph->GetPathName());J->SetStringField(TEXT("graph_id"),GraphGuid.ToString(EGuidFormats::Digits));J->SetBoolField(TEXT("dirty"),TargetPackage->IsDirty());J->SetNumberField(TEXT("blueprint_status"),int32(Blueprint->Status));
    int32 Nodes=0,PinCount=0;for(const auto& G:OldGraphs){Nodes+=G->Hold->Nodes.Num();for(const auto& N:G->Nodes)PinCount+=N->Hold->Pins.Num();}if(Attached&&Node.IsValid())PinCount+=Node->Pins.Num();J->SetNumberField(TEXT("owned_graph_count"),OldGraphs.Num());J->SetNumberField(TEXT("node_count"),Nodes);J->SetNumberField(TEXT("pin_count"),PinCount);J->SetNumberField(TEXT("own_variable_count"),Variables.Num());J->SetNumberField(TEXT("generated_variable_count"),GeneratedVariables.Num());return J;
}
bool FMapScope::FreezePins()
{
    if(SetCall)return SetFreezePins();
    if(!Raw())return false;const auto* N=Node.Get();const bool Pure=Operation==TEXT("find")||Operation==TEXT("contains")||Operation==TEXT("length");
    if(N->Pins.Num()!=Parameters.Num()+1+(Pure?0:2)||N->IsNodePure()!=Pure)return Reject(TEXT("map_allocated_node_shape_mismatch"),TEXT("Stock Map pin count/purity differs."));
    TSet<FGuid> Ids;for(UEdGraphPin* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||P->bDeprecated||!P->LinkedTo.IsEmpty()||!P->DefaultObject.IsResolved())return Reject(TEXT("map_allocated_node_shape_mismatch"),TEXT("Initial physical whole pin role invalid."));Ids.Add(P->PinId);Pins.Add(MakeShared<FPin>(P));}
    PinsFixed=true;if(!Raw()||!PinReceipt())return false;
    for(FProperty* Field:Parameters){const bool Input=!Field->HasAnyPropertyFlags(CPF_ReturnParm)&&(!Field->HasAnyPropertyFlags(CPF_OutParm)||Field->HasAnyPropertyFlags(CPF_ReferenceParm));const auto* P=Node->FindPin(Field->GetFName(),Input?EGPD_Input:EGPD_Output);
        if(!P||!ExpectedType(Field,P->PinType)||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bAdvancedView||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return Reject(TEXT("map_allocated_node_shape_mismatch"),TEXT("Stock wildcard direction/ref/const/terminal differs."));
        const bool Ignored=Field->HasAllPropertyFlags(CPF_ConstParm|CPF_ReferenceParm)&&(!Native->HasMetaData(TEXT("AutoCreateRefTerm"))||P->PinType.IsContainer());
        if(P->bDefaultValueIsIgnored!=Ignored)return Reject(TEXT("map_allocated_node_shape_mismatch"),TEXT("AutoCreateRefTerm/container ignored-default condition differs."));
        const FString ExpectedDefault=Field->HasAnyPropertyFlags(CPF_ReturnParm)?(Operation==TEXT("length")?TEXT("0"):TEXT("false")):TEXT("");
        if(!Equal(P->DefaultValue,ExpectedDefault)||!Equal(P->AutogeneratedDefaultValue,ExpectedDefault))return Reject(TEXT("map_allocated_node_default_mismatch"),TEXT("Initial wildcard defaults must be empty; bool ReturnValue uses false and Length int ReturnValue uses zero."));
    }
    FEdGraphPinType SelfType;SelfType.PinCategory=UEdGraphSchema_K2::PC_Object;SelfType.PinSubCategoryObject=Library.Get();const auto* Self=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
    if(!Self||!SameType(Self->PinType,SelfType)||!Self->bHidden||Self->DefaultObject!=LibraryCDO.Get()||!Self->LinkedTo.IsEmpty())return Reject(TEXT("map_allocated_node_shape_mismatch"),TEXT("Exact hidden static library self/CDO differs."));
    if(!Pure){FEdGraphPinType Exec;Exec.PinCategory=UEdGraphSchema_K2::PC_Exec;for(const FName Name:{UEdGraphSchema_K2::PN_Execute,UEdGraphSchema_K2::PN_Then}){const auto* P=Node->FindPin(Name,Name==UEdGraphSchema_K2::PN_Execute?EGPD_Input:EGPD_Output);if(!P||!SameType(P->PinType,Exec))return Reject(TEXT("map_allocated_node_shape_mismatch"),TEXT("Imperative exec pair differs."));}}
    return Raw();
}
bool FMapScope::SetFreezePins()
{
    if(!Raw())return false;const auto* N=Node.Get();const bool Pure=Operation==TEXT("find")||Operation==TEXT("contains")||Operation==TEXT("length");
    if(N->Pins.Num()!=Parameters.Num()+1+(Pure?0:2)||N->IsNodePure()!=Pure)return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("Stock Map pin count/purity differs."));
    TSet<FGuid> Ids;for(UEdGraphPin* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||P->bDeprecated||!P->LinkedTo.IsEmpty()||!P->DefaultObject.IsResolved())return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("Initial physical whole pin role invalid."));Ids.Add(P->PinId);Pins.Add(MakeShared<FPin>(P));}
    PinsFixed=true;if(!Raw()||!PinReceipt())return false;
    for(FProperty* Field:Parameters){const bool Input=!Field->HasAnyPropertyFlags(CPF_ReturnParm)&&(!Field->HasAnyPropertyFlags(CPF_OutParm)||Field->HasAnyPropertyFlags(CPF_ReferenceParm));auto* P=Node->FindPin(Field->GetFName(),Input?EGPD_Input:EGPD_Output);if(!P)return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("Missing selected parameter pin."));const FText CopiedText=P->DefaultTextValue;const bool EmptyText=CopiedText.IsEmpty();if(!Raw())return false;P=Node->FindPin(Field->GetFName(),Input?EGPD_Input:EGPD_Output);
        if(!P||!ExpectedType(Field,P->PinType)||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bAdvancedView||P->DefaultObject!=nullptr||!EmptyText)return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("Stock wildcard direction/ref/const/terminal differs."));
        const bool Ignored=Field->HasAllPropertyFlags(CPF_ConstParm|CPF_ReferenceParm)&&(!Native->HasMetaData(TEXT("AutoCreateRefTerm"))||P->PinType.IsContainer());
        if(P->bDefaultValueIsIgnored!=Ignored)return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("AutoCreateRefTerm/container ignored-default condition differs."));
        const FString ExpectedDefault=Field->HasAnyPropertyFlags(CPF_ReturnParm)?(Operation==TEXT("length")?TEXT("0"):TEXT("false")):TEXT("");
        if(!Equal(P->DefaultValue,ExpectedDefault)||!Equal(P->AutogeneratedDefaultValue,ExpectedDefault))return Reject(TEXT("set_allocated_node_default_mismatch"),TEXT("Initial wildcard defaults must be empty; bool ReturnValue uses false and Length int ReturnValue uses zero."));
    }
    FEdGraphPinType SelfType;SelfType.PinCategory=UEdGraphSchema_K2::PC_Object;SelfType.PinSubCategoryObject=Library.Get();const auto* Self=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
    if(!Self||!SameType(Self->PinType,SelfType)||!Self->bHidden||Self->DefaultObject!=LibraryCDO.Get()||!Self->LinkedTo.IsEmpty())return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("Exact hidden static library self/CDO differs."));
    if(!Pure){FEdGraphPinType Exec;Exec.PinCategory=UEdGraphSchema_K2::PC_Exec;for(const FName Name:{UEdGraphSchema_K2::PN_Execute,UEdGraphSchema_K2::PN_Then}){const auto* P=Node->FindPin(Name,Name==UEdGraphSchema_K2::PN_Execute?EGPD_Input:EGPD_Output);if(!P||!SameType(P->PinType,Exec))return Reject(TEXT("set_allocated_node_shape_mismatch"),TEXT("Imperative exec pair differs."));}}
    return Raw();
}
bool FMapScope::PinReceipt()
{
    if(!Raw())return false;TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& P:Pins){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->Name.ToString());J->SetStringField(TEXT("pin_id"),P->Id.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetObjectField(TEXT("pin_type"),TypeJson(P->Type));J->SetNumberField(TEXT("flags"),P->Flags);J->SetStringField(TEXT("default_value"),P->Default);J->SetStringField(TEXT("autogenerated_default_value"),P->Auto);const FText CopiedText=P->Text;const bool TextEmpty=CopiedText.IsEmpty();if(SetCall&&!Raw())return false;J->SetBoolField(TEXT("default_text_empty"),TextEmpty);J->SetStringField(TEXT("default_object_path"),P->Name==UEdGraphSchema_K2::PN_Self?SourceDefaultPath():TEXT(""));J->SetBoolField(TEXT("whole_current_verified"),true);J->SetStringField(TEXT("owner_path"),NodePath);Rows.Add(MakeShared<FJsonValueObject>(J));}
    if(!Raw())return false;R.Data->SetArrayField(SetCall?TEXT("set_initial_pin_shapes"):TEXT("map_initial_pin_shapes"),Rows);return true;
}

// OA: preserve the UE56 Any conversion before the stock schema validates it.
// The only structure admitted here remains the existing native Vector cohort.
bool MapLiteralText(const TSharedPtr<FJsonValue>& V,bool IsVector,FString& Out)
{
    if(!V)return false;Out.Reset();
    if(IsVector&&(V->Type==EJson::Array||V->Type==EJson::Object)){
        double A[3]={0,0,0};
        if(V->Type==EJson::Array){const auto& Values=V->AsArray();for(int32 I=0;I<3;++I)if(Values.IsValidIndex(I)&&Values[I]->Type==EJson::Number)A[I]=Values[I]->AsNumber();}
        else {const auto O=V->AsObject();if(!O)return false;O->TryGetNumberField(TEXT("x"),A[0]);O->TryGetNumberField(TEXT("y"),A[1]);O->TryGetNumberField(TEXT("z"),A[2]);}
        if(!FMath::IsFinite(A[0])||!FMath::IsFinite(A[1])||!FMath::IsFinite(A[2]))return false;
        Out=FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),*FString::SanitizeFloat(A[0]),*FString::SanitizeFloat(A[1]),*FString::SanitizeFloat(A[2]));
    }
    else if(V->Type==EJson::String)Out=V->AsString();
    else if(V->Type==EJson::Number){if(!FMath::IsFinite(V->AsNumber()))return false;Out=FString::SanitizeFloat(V->AsNumber());}
    else if(V->Type==EJson::Boolean)Out=V->AsBool()?TEXT("true"):TEXT("false");
    else if(V->Type==EJson::Array||V->Type==EJson::Object){if(!FJsonSerializer::Serialize(V,TEXT(""),TJsonWriterFactory<>::Create(&Out)))return false;}
    else if(V->Type!=EJson::Null)return false;
    return Out.Len()<=65536;
}
bool FMapScope::LiteralTransition(int32 Index,const TSharedPtr<FPin>& Expected,const FString& Literal,bool Auto)
{
    if(!Raw()||!Pins.IsValidIndex(Index)||!Expected.IsValid()||Expected->Address!=Pins[Index]->Address)return false;
    // One exact planned default/Auto/type result or the unchanged before result
    // is observable. All other source, node and pin properties remain frozen.
    PendingPin=Index;PendingExpected=Expected;LastCurrent=false;R.bChangeStateKnown=false;
    UEdGraphPin* P=Pins[Index]->Address;const auto* Schema=static_cast<const UEdGraphSchema_K2*>(SchemaCDO.Get());
    const TCHAR* Call=Auto?TEXT("SetPinAutogeneratedDefaultValueBasedOnType"):TEXT("TrySetDefaultValue");LiteralCall(Call,false);
    if(Auto)Schema->SetPinAutogeneratedDefaultValueBasedOnType(P);else Schema->TrySetDefaultValue(*P,Literal,false);LiteralCall(Call,true);
    if(!Raw())return false;
    const bool Applied=Expected->Current(Node.Get());
    Pins[Index]=MakeShared<FPin>(Pins[Index]->Address);PendingExpected.Reset();PendingPin=INDEX_NONE;
    if(!Raw())return false;
    if(!Applied)return Reject(TEXT("map_literal_sdk_not_applied"),TEXT("Schema retained the exact previous pin default instead of the requested result; created typed prefix remains, without rollback or retry."));
    return true;
}
bool FMapScope::LiteralTypes()
{
    if(!LiteralMode||Neutral||!Raw())return false;
    R.Stage=TEXT("map_literal_specialize");
    // Preserve the independently verified original stock wildcard allocation.
    R.Data->SetArrayField(TEXT("map_allocated_pin_shapes"),R.Data->GetArrayField(TEXT("map_initial_pin_shapes")));
    const auto* Schema=static_cast<const UEdGraphSchema_K2*>(SchemaCDO.Get());
    for(int32 I=0;I<Pins.Num();++I){const auto Pin=Pins[I];if(Pin->Type.PinCategory!=UEdGraphSchema_K2::PC_Wildcard)continue;
        FEdGraphPinType Typed=Pin->Type;
        const bool Map=Pin->Name==TEXT("TargetMap"),KeyPin=Pin->Name==TEXT("Key");
        Typed.PinCategory=Map||KeyPin?UEdGraphSchema_K2::PC_String:DoubleValue?UEdGraphSchema_K2::PC_Real:UEdGraphSchema_K2::PC_Struct;
        Typed.PinSubCategory=(!Map&&!KeyPin&&DoubleValue)?UEdGraphSchema_K2::PC_Double:NAME_None;
        Typed.PinSubCategoryObject=(!Map&&!KeyPin&&!DoubleValue)?Vector.Get():nullptr;
        if(Map){Typed.PinValueType.TerminalCategory=DoubleValue?UEdGraphSchema_K2::PC_Real:UEdGraphSchema_K2::PC_Struct;Typed.PinValueType.TerminalSubCategory=DoubleValue?UEdGraphSchema_K2::PC_Double:NAME_None;Typed.PinValueType.TerminalSubCategoryObject=DoubleValue?nullptr:Vector.Get();}
        // Plain finite specialization retains stock const/ref/container/flags.
        Pin->Type=Typed;Pin->Address->PinType=Typed;if(!Raw())return false;
        if(Map)continue;
        const FString Auto=KeyPin?FString():DoubleValue?TEXT("0.0"):TEXT("0, 0, 0");
        FString RawDefault;TObjectPtr<UObject> Object=nullptr;FText Text;
        LiteralCall(TEXT("GetPinDefaultValuesFromString_Auto"),false);Schema->GetPinDefaultValuesFromString(Typed,Node.Get(),Auto,RawDefault,Object,Text,false);LiteralCall(TEXT("GetPinDefaultValuesFromString_Auto"),true);if(!Raw())return false;
        if(Object!=nullptr||!Text.IsEmpty())return Reject(TEXT("map_literal_conversion_unexpected"),TEXT("Finite String/Double/Vector stock Auto conversion returned an object or Text role."));
        auto Expected=MakeShared<FPin>(Pin->Address);Expected->Default=RawDefault;Expected->Auto=Auto;Expected->Text=Text;Expected->Type.bSerializeAsSinglePrecisionFloat=false;
        R.Stage=TEXT("map_literal_stock_auto");if(!LiteralTransition(I,Expected,Auto,true))return false;
    }
    R.Data->SetBoolField(TEXT("map_literal_profile_seeded"),true);R.Data->SetBoolField(TEXT("map_typed_specialization_verified"),true);return PinReceipt()&&Raw();
}
TSharedRef<FJsonObject> FMapScope::LiteralObservation(int32 Index,const FString& Requested,const FString& SchemaLiteral,bool Match,bool Valid)
{
    auto J=MakeShared<FJsonObject>();if(!Raw()||!Pins.IsValidIndex(Index))return J;
    const auto Role=Pins[Index];const FText Text=Role->Text;LiteralCall(TEXT("CopiedDefaultText_ToString"),false);const FString TextString=Text.ToString();LiteralCall(TEXT("CopiedDefaultText_ToString"),true);if(!Raw())return J;
    LiteralCall(TEXT("GetDefaultAsString"),false);const FString DefaultString=Role->Address->GetDefaultAsString();LiteralCall(TEXT("GetDefaultAsString"),true);if(!Raw())return J;
    J->SetStringField(TEXT("requested_literal"),Requested);J->SetStringField(TEXT("schema_literal"),SchemaLiteral);J->SetStringField(TEXT("raw_default"),Role->Default);J->SetStringField(TEXT("default_text"),TextString);J->SetStringField(TEXT("default_as_string"),DefaultString);J->SetStringField(TEXT("autogenerated_default_value"),Role->Auto);J->SetBoolField(TEXT("schema_match"),Match);J->SetBoolField(TEXT("current_default_valid"),Valid);J->SetStringField(TEXT("pin_id"),Role->Id.ToString(EGuidFormats::Digits));J->SetObjectField(TEXT("pin_type"),TypeJson(Role->Type));J->SetBoolField(TEXT("whole_current_verified"),true);return J;
}
bool FMapScope::LiteralDefaults(const TSharedRef<FJsonObject>& Defaults)
{
    if(!LiteralTypes())return false;
    const auto* Schema=static_cast<const UEdGraphSchema_K2*>(SchemaCDO.Get());
    const auto Applied=MakeShared<FJsonObject>(),Observed=MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("map_literal_defaults_applied"),Applied);R.Data->SetObjectField(TEXT("map_literal_default_observations"),Observed);
    // Canonical Key then Value order is independent of JSON object ordering.
    for(const TCHAR* Name:{TEXT("Key"),TEXT("Value")}){
        const auto V=Defaults->TryGetField(Name);if(!V.IsValid())continue;
        int32 Index=INDEX_NONE;for(int32 I=0;I<Pins.Num();++I)if(Pins[I]->Name==Name&&Pins[I]->Direction==EGPD_Input){Index=I;break;}
        if(Index==INDEX_NONE||Pins[Index]->Type.IsContainer()||(Pins[Index]->Flags&(1U<<2))||Pins[Index]->Address->bDefaultValueIsIgnored)return Reject(TEXT("map_literal_pin_unavailable"),TEXT("Exact visible nonignored scalar Key/Value input required."));
        FString Literal;if(!MapLiteralText(V,FString(Name)==TEXT("Value")&&!DoubleValue,Literal))return Reject(TEXT("map_literal_conversion_failed"),TEXT("Original bounded Any conversion failed; typed node prefix retained."));
        const FEdGraphPinType Type=Pins[Index]->Type;FString RawDefault;TObjectPtr<UObject> Object=nullptr;FText Text;
        R.Stage=TEXT("map_literal_schema_conversion");LiteralCall(TEXT("GetPinDefaultValuesFromString_Literal"),false);Schema->GetPinDefaultValuesFromString(Type,Node.Get(),Literal,RawDefault,Object,Text,false);LiteralCall(TEXT("GetPinDefaultValuesFromString_Literal"),true);if(!Raw())return false;
        if(Object!=nullptr||!Text.IsEmpty())return Reject(TEXT("map_literal_conversion_unexpected"),TEXT("Finite String/Double/Vector conversion returned an object or Text role."));
        UEdGraphPin* P=Pins[Index]->Address;
        LiteralCall(TEXT("IsPinDefaultValid"),false);const FString Validation=Schema->IsPinDefaultValid(P,RawDefault,nullptr,Text);LiteralCall(TEXT("IsPinDefaultValid"),true);if(!Raw())return false;
        if(!Validation.IsEmpty()){R.Data->SetStringField(TEXT("map_literal_schema_error"),Validation);R.Data->SetStringField(TEXT("map_literal_failed_pin"),Name);return Reject(TEXT("map_literal_engine_rejected"),TEXT("Schema rejected the supplied literal before its setter; created typed prefix remains."));}
        auto Expected=MakeShared<FPin>(Pins[Index]->Address);Expected->Default=RawDefault;Expected->Text=Text;Expected->Type.bSerializeAsSinglePrecisionFloat=false;
        R.Stage=TEXT("map_literal_apply");R.Data->SetStringField(TEXT("map_literal_active_pin"),Name);if(!LiteralTransition(Index,Expected,Literal,false))return false;
        P=Pins[Index]->Address;LiteralCall(TEXT("DoesDefaultValueMatch"),false);const bool Match=Schema->DoesDefaultValueMatch(*P,Literal);LiteralCall(TEXT("DoesDefaultValueMatch"),true);if(!Raw())return false;
        P=Pins[Index]->Address;LiteralCall(TEXT("IsCurrentPinDefaultValid"),false);const FString CurrentError=Schema->IsCurrentPinDefaultValid(P);LiteralCall(TEXT("IsCurrentPinDefaultValid"),true);if(!Raw())return false;
        Applied->SetStringField(Name,Pins[Index]->Default);const auto Observation=LiteralObservation(Index,Literal,Literal,Match,CurrentError.IsEmpty());if(!Raw())return false;Observed->SetObjectField(Name,Observation);
        if(!Match||!CurrentError.IsEmpty()){R.Data->SetStringField(TEXT("map_literal_failed_pin"),Name);R.Data->SetStringField(TEXT("map_literal_schema_error"),CurrentError);return Reject(TEXT("map_literal_readback_mismatch"),TEXT("Actual schema readback rejected the retained literal; no rollback or retry."));}
    }
    R.Data->SetBoolField(TEXT("map_literal_defaults_verified"),true);return PinReceipt()&&Raw();
}


bool FMapScope::UntypedCallReceipt()
{
    if(!Neutral||!PinsFixed||!Raw())return false;
    const FString Comment=Node->NodeComment;const bool Bubble=Node->bCommentBubbleVisible,Pinned=Node->bCommentBubblePinned;
    const auto Current=[&]()
    {
        if(!Raw())return false;
        if(!Equal(Node->NodeComment,Comment)||Node->bCommentBubbleVisible!=Bubble||Node->bCommentBubblePinned!=Pinned){
            R.Data->SetStringField(TEXT("first_failure_object"),NodePath);R.Data->SetStringField(TEXT("first_failure_expected"),TEXT("captured node comment/bubble fields unchanged"));R.Data->SetStringField(TEXT("first_failure_actual"),TEXT("captured plain-field equality=false"));
            return Reject(TEXT("map_call_observation_changed"),TEXT("Created node metadata changed across the receipt query."),true);}
        return true;
    };
    // Query copied FText only; close the callback boundary before following the
    // node or the next saved pin. The scope still owns every source/target role.
    const FText Title=Node->GetNodeTitle(ENodeTitleType::FullTitle);if(!Current())return false;
    const FString TitleString=Title.ToString();if(!Current())return false;
    const FString Id=NodeGuid.ToString(EGuidFormats::Digits),Name=Node->GetName(),ClassName=NodeClass->GetName();
    auto Flat=MakeShared<FJsonObject>();Flat->SetStringField(TEXT("node_id"),Id);Flat->SetStringField(TEXT("node_guid"),Id);Flat->SetStringField(TEXT("object_path"),NodePath);Flat->SetStringField(TEXT("node_class"),NodeClass->GetPathName());Flat->SetStringField(TEXT("node_name"),Name);Flat->SetStringField(TEXT("node_type"),ClassName);
    Flat->SetStringField(TEXT("name"),Name);Flat->SetStringField(TEXT("class"),ClassName);Flat->SetStringField(TEXT("title"),TitleString);Flat->SetNumberField(TEXT("x"),X);Flat->SetNumberField(TEXT("y"),Y);Flat->SetStringField(TEXT("node_comment"),Comment);Flat->SetBoolField(TEXT("comment_bubble_visible"),Bubble);Flat->SetBoolField(TEXT("comment_bubble_pinned"),Pinned);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& P:Pins){
        const auto& T=P->Type;
        // FreezePins has independently verified the only object-bearing pin:
        // exact hidden library self. Read the copied type handles shallowly.
        const FString TypeObject=P->Name==UEdGraphSchema_K2::PN_Self?FString(SourcePath()):FString();
        auto Type=MakeShared<FJsonObject>();Type->SetStringField(TEXT("category"),T.PinCategory.ToString());Type->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());Type->SetStringField(TEXT("type_object"),TypeObject);Type->SetStringField(TEXT("subcategory_object"),TypeObject);Type->SetNumberField(TEXT("container_type"),int32(T.ContainerType));Type->SetBoolField(TEXT("is_array"),T.ContainerType==EPinContainerType::Array);Type->SetBoolField(TEXT("is_reference"),T.bIsReference);Type->SetBoolField(TEXT("is_const"),T.bIsConst);Type->SetBoolField(TEXT("is_weak_pointer"),T.bIsWeakPointer);Type->SetBoolField(TEXT("is_uobject_wrapper"),T.bIsUObjectWrapper);Type->SetBoolField(TEXT("serialize_as_single_precision_float"),T.bSerializeAsSinglePrecisionFloat);
        auto Terminal=MakeShared<FJsonObject>();Terminal->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());Terminal->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());Terminal->SetStringField(TEXT("type_object"),TEXT(""));Terminal->SetBoolField(TEXT("is_const"),T.PinValueType.bTerminalIsConst);Terminal->SetBoolField(TEXT("is_weak_pointer"),T.PinValueType.bTerminalIsWeakPointer);Terminal->SetBoolField(TEXT("is_uobject_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);Type->SetObjectField(TEXT("terminal"),Terminal);
        const FText DefaultText=P->Text;const FString DefaultTextString=DefaultText.ToString();if(!Current())return false;
        auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("name"),P->Name.ToString());Q->SetStringField(TEXT("pin_id"),P->Id.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("direction"),P->Direction==EGPD_Input?TEXT("input"):TEXT("output"));Q->SetStringField(TEXT("category"),T.PinCategory.ToString());Q->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());Q->SetBoolField(TEXT("is_array"),T.ContainerType==EPinContainerType::Array);Q->SetStringField(TEXT("subcategory_object"),TypeObject);Q->SetObjectField(TEXT("pin_type"),Type);Q->SetNumberField(TEXT("flags"),P->Flags);Q->SetBoolField(TEXT("orphaned"),(P->Flags&(1U<<8))!=0);Q->SetStringField(TEXT("default_value"),P->Default);Q->SetStringField(TEXT("autogenerated_default_value"),P->Auto);Q->SetStringField(TEXT("default_object"),P->Name==UEdGraphSchema_K2::PN_Self?SourceDefaultPath():TEXT(""));Q->SetStringField(TEXT("default_text"),DefaultTextString);Q->SetArrayField(TEXT("linked_to"),TArray<TSharedPtr<FJsonValue>>());Rows.Add(MakeShared<FJsonValueObject>(Q));
    }
    if(!Current())return false;Flat->SetArrayField(TEXT("pins"),Rows);
    auto GraphRow=MakeShared<FJsonObject>();GraphRow->SetStringField(TEXT("graph_id"),GraphGuid.ToString(EGuidFormats::Digits));GraphRow->SetStringField(TEXT("graph_name"),Graph->GetName());GraphRow->SetStringField(TEXT("graph_type"),Kind(Graph.Get()));GraphRow->SetStringField(TEXT("object_path"),Graph->GetPathName());GraphRow->SetStringField(TEXT("schema"),SchemaClass->GetName());GraphRow->SetNumberField(TEXT("node_count"),Graph->Nodes.Num());
    Flat->SetObjectField(TEXT("graph"),GraphRow);Flat->SetStringField(TEXT("blueprint"),Blueprint->GetPathName());Flat->SetStringField(TEXT("function_name"),FunctionName);Flat->SetStringField(TEXT("function_class"),SourcePath());Flat->SetStringField(TEXT("function_path"),Native->GetPathName());Flat->SetBoolField(TEXT("is_static"),(FunctionFlags&FUNC_Static)!=0);Flat->SetBoolField(TEXT("is_pure"),(FunctionFlags&FUNC_BlueprintPure)!=0);Flat->SetBoolField(TEXT("is_latent"),false);Flat->SetObjectField(TEXT("param_defaults_applied"),MakeShared<FJsonObject>());Flat->SetBoolField(TEXT("flat_node_observation_complete"),true);Flat->SetStringField(TEXT("node_creation_api"),TEXT("public NewObject/AddNode/SetFromFunction/CreateNewGuid/PostPlacedNewNode/AllocateDefaultPins with concrete callback barriers"));Flat->SetBoolField(TEXT("implicit_skeleton_compile_possible"),false);
    if(!Current())return false;for(const auto& Pair:Flat->Values)R.Data->SetField(Pair.Key,Pair.Value);return true;
}
}

static Function::FResult AddNodeInternal(const FString& BlueprintPath,const FString& Operation,const FString& KeyType,const FString& ValueType,
    const FString& ParamDefaultsJson,const FString& NodePositionJson,const FString& GraphName,const FString& GraphId,
    const FString& GraphType,bool bCreateGraphIfMissing,bool Neutral)
{
    FResult R;FMapScope C(R);C.Neutral=Neutral;C.Operation=Operation;C.DoubleValue=ValueType==TEXT("double");C.FunctionName=Operation==TEXT("clear")?TEXT("Map_Clear"):Operation==TEXT("find")?TEXT("Map_Find"):Operation==TEXT("contains")?TEXT("Map_Contains"):Operation==TEXT("remove")?TEXT("Map_Remove"):Operation==TEXT("keys")?TEXT("Map_Keys"):Operation==TEXT("values")?TEXT("Map_Values"):Operation==TEXT("length")?TEXT("Map_Length"):TEXT("Map_Add");
    if(Operation==TEXT("length"))R.Data->SetBoolField(TEXT("native_map_length"),true);
    if(!Neutral&&ValueType==TEXT("vector")&&(Operation==TEXT("contains")||Operation==TEXT("remove")||Operation==TEXT("keys")||Operation==TEXT("values")))R.Data->SetBoolField(TEXT("native_vector_map_operation_parity"),true);
    if(C.Neutral){R.Data->SetBoolField(TEXT("native_untyped_map_call"),true);R.Data->SetBoolField(TEXT("map_type_request_specified"),false);}
    else R.Data->SetBoolField(C.DoubleValue?TEXT("native_map_string_double"):TEXT("native_map_string_vector"),true);R.Data->SetStringField(TEXT("map_operation"),Operation);R.Data->SetBoolField(TEXT("specialization_pending"),true);
    R.Data->SetBoolField(TEXT("node_only_contract"),true);R.Data->SetBoolField(TEXT("implicit_data_wiring"),false);R.Data->SetBoolField(TEXT("map_target_requires_connection"),true);
    R.Data->SetBoolField(TEXT("map_key_requires_connection"),Operation==TEXT("find")||Operation==TEXT("add")||Operation==TEXT("contains")||Operation==TEXT("remove"));R.Data->SetBoolField(TEXT("map_value_requires_connection"),Operation==TEXT("add"));
    R.Data->SetBoolField(TEXT("map_literal_defaults_supported"),false);R.Data->SetBoolField(TEXT("map_typed_specialization_verified"),false);R.Data->SetBoolField(TEXT("map_runtime_execution_verified"),false);
    R.Data->SetBoolField(TEXT("transaction_created"),false);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),false);R.Data->SetBoolField(TEXT("full_original_history_pipeline_verified"),false);
    if(Operation!=TEXT("clear")&&Operation!=TEXT("find")&&Operation!=TEXT("add")&&Operation!=TEXT("contains")&&Operation!=TEXT("remove")&&Operation!=TEXT("keys")&&Operation!=TEXT("values")&&Operation!=TEXT("length")){C.Reject(TEXT("map_operation_unsupported"),TEXT("Exact operations: clear/find/add/contains/remove/keys/values/length for existing String-double/Vector cohorts."));return R;}
    if(!C.Neutral&&(KeyType!=TEXT("string")||(ValueType!=TEXT("vector")&&ValueType!=TEXT("double")))){C.Reject(TEXT("map_type_unsupported"),TEXT("Only exact String -> native Vector or scalar double cohorts; float and arbitrary value types remain unsupported."));return R;}
    if(bCreateGraphIfMissing){C.Reject(TEXT("map_graph_creation_unsupported"),TEXT("Create an owned graph separately; no implicit graph creation."));return R;}
    TSharedPtr<FJsonValue> Defaults,Position;
    if(!Function::Pin::ParseJson(ParamDefaultsJson,Defaults,R,!C.Neutral&&(Operation==TEXT("find")||Operation==TEXT("add")||Operation==TEXT("contains")||Operation==TEXT("remove")))||!Function::Pin::ParseJson(NodePositionJson,Position,R))return R;
    if(C.Neutral&&Defaults->Type==EJson::Null)Defaults=MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
    if(Defaults->Type!=EJson::Object){C.Reject(TEXT("map_literal_defaults_unsupported"),TEXT("Defaults require an object; only typed scalar Key/Value literals are admitted."));return R;}
    auto CanonicalDefaults=MakeShared<FJsonObject>();
    for(const auto& Pair:Defaults->AsObject()->Values){const FString Requested(Pair.Key.ToView());const bool Key=Requested.Equals(TEXT("Key"),ESearchCase::IgnoreCase),Value=Requested.Equals(TEXT("Value"),ESearchCase::IgnoreCase);
        const FString Name=Key?TEXT("Key"):TEXT("Value");
        if(C.Neutral||(!Key&&!Value)||(Key&&(Operation!=TEXT("find")&&Operation!=TEXT("add")&&Operation!=TEXT("contains")&&Operation!=TEXT("remove")))||(Value&&Operation!=TEXT("add"))||CanonicalDefaults->HasField(Name)){C.Reject(TEXT("map_literal_defaults_unsupported"),TEXT("Only unique case-insensitive Key(find/add/contains/remove) and Value(add) are admitted in an existing typed cohort; TargetMap, output, neutral wildcard and unknown literals remain unsupported."));return R;}
        CanonicalDefaults->SetField(Name,Pair.Value);
    }
    C.LiteralMode=!CanonicalDefaults->Values.IsEmpty();
    if(C.LiteralMode){R.Data->SetBoolField(TEXT("native_typed_map_literal_defaults"),true);R.Data->SetBoolField(TEXT("typed_unlinked_map_profile"),true);R.Data->SetBoolField(TEXT("map_literal_defaults_supported"),true);R.Data->SetBoolField(TEXT("map_key_requires_connection"),!CanonicalDefaults->HasField(TEXT("Key")));R.Data->SetBoolField(TEXT("map_value_requires_connection"),Operation==TEXT("add")&&!CanonicalDefaults->HasField(TEXT("Value")));R.Data->SetBoolField(TEXT("map_literal_runtime_invocation_verified"),false);R.Data->SetBoolField(TEXT("atomic_rollback_provided"),false);R.Data->SetObjectField(TEXT("map_literal_defaults_applied"),MakeShared<FJsonObject>());R.Data->SetObjectField(TEXT("map_literal_default_observations"),MakeShared<FJsonObject>());}
    if(Position->Type!=EJson::Array||Position->AsArray().Num()!=2){C.Reject(TEXT("map_position_invalid"),TEXT("Position must contain two bounded integers."));return R;}
    int32 Coords[2];for(int32 I=0;I<2;++I){const auto& V=Position->AsArray()[I];if(V->Type!=EJson::Number||!FMath::IsFinite(V->AsNumber())||FMath::TruncToDouble(V->AsNumber())!=V->AsNumber()||(C.Neutral?(V->AsNumber()<MIN_int32||V->AsNumber()>MAX_int32):FMath::Abs(V->AsNumber())>1000000.0)){C.Reject(TEXT("map_position_invalid"),C.Neutral?TEXT("Position requires exact int32 coordinates."):TEXT("Position requires exact integers within +/-1000000."));return R;}Coords[I]=int32(V->AsNumber());}C.X=Coords[0];C.Y=Coords[1];
    const auto& Version=FEngineVersion::Current();if(Version.GetMajor()!=5||Version.GetMinor()!=8||Version.GetPatch()!=3){C.Reject(TEXT("map_version_unsupported"),TEXT("Only installed UE5.8.3 supported."));return R;}
    if(!C.Begin()||!C.Source()||!C.Target(BlueprintPath,GraphName,GraphId,GraphType))return R;
    if(!C.Neutral){FEdGraphPinType Requested;Requested.PinCategory=UEdGraphSchema_K2::PC_String;Requested.ContainerType=EPinContainerType::Map;Requested.PinValueType.TerminalCategory=C.DoubleValue?UEdGraphSchema_K2::PC_Real:UEdGraphSchema_K2::PC_Struct;Requested.PinValueType.TerminalSubCategory=C.DoubleValue?UEdGraphSchema_K2::PC_Double:NAME_None;Requested.PinValueType.TerminalSubCategoryObject=C.DoubleValue?nullptr:C.Vector.Get();R.Data->SetObjectField(TEXT("map_requested_type"),C.TypeJson(Requested));}
    R.Data->SetStringField(TEXT("function_class"),LibraryPath);R.Data->SetStringField(TEXT("function_name"),C.FunctionName);
    R.Stage=TEXT("map_blueprint_modify");R.bMutationAttempted=true;R.bChangeStateKnown=false;
    EObjectFlags PackageFlags=C.TargetPackage->GetFlags();const bool BPModified=C.Blueprint->Modify();if(!C.AfterModify(BPModified,PackageFlags))return R;
    R.Stage=TEXT("map_graph_modify");PackageFlags=C.TargetPackage->GetFlags();const bool GraphModified=C.Graph->Modify();if(!C.AfterModify(GraphModified,PackageFlags))return R;
    R.Stage=TEXT("map_allocate");R.Data->SetNumberField(TEXT("node_allocation_calls"),1);
    UK2Node_CallFunction* Returned=NewObject<UK2Node_CallFunction>(C.Graph.Get(),NAME_None,RF_Transactional);if(!C.Adopt(Returned))return R;
    R.Stage=TEXT("map_attach");C.Attached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(C.Node.Get(),false,false);R.Data->SetNumberField(TEXT("graph_add_node_returns"),1);if(!C.Raw())return R;
    R.Stage=TEXT("map_set_from_function");C.Member.SetExternalMember(FName(*C.FunctionName),C.Library.Get());if(!C.Raw())return R;C.MemberFixed=true;
    R.Data->SetNumberField(TEXT("set_from_function_calls"),1);C.Node->SetFromFunction(C.Native.Get());R.Data->SetNumberField(TEXT("set_from_function_returns"),1);if(!C.Raw())return R;
    C.Node->NodePosX=C.X;C.Node->NodePosY=C.Y;
    R.Stage=TEXT("map_create_guid");R.Data->SetNumberField(TEXT("create_guid_calls"),1);C.Node->CreateNewGuid();R.Data->SetNumberField(TEXT("create_guid_returns"),1);
    if(IsEditorGameThread()&&!IsGarbageCollecting()&&Loaded(C.Node.Get())&&C.Node->GetOuter()==C.Graph.Get()&&Equal(C.Node->GetPathName(),C.NodePath)){
        C.NodeGuid=C.Node->NodeGuid;C.GuidFixed=true;R.Data->SetStringField(TEXT("created_node_guid"),C.NodeGuid.ToString(EGuidFormats::Digits));R.Data->SetBoolField(TEXT("created_node_guid_observed"),true);}
    if(!C.Raw())return R;if(!C.GuidFixed||!C.NodeGuid.IsValid()){C.Reject(TEXT("map_created_guid_invalid"),TEXT("New GUID not safely observed; no cleanup/retry."));return R;}
    R.Stage=TEXT("map_post_placed");R.Data->SetNumberField(TEXT("post_placed_calls"),1);C.Node->PostPlacedNewNode();R.Data->SetNumberField(TEXT("post_placed_returns"),1);if(!C.Raw())return R;
    if(!C.Node->Pins.IsEmpty()){C.Reject(TEXT("map_unexpected_preallocated_pins"),TEXT("PostPlaced unexpectedly allocated pins; retained node is not repaired."));return R;}
    R.Stage=TEXT("map_allocate_pins");R.Data->SetNumberField(TEXT("allocate_pins_calls"),1);C.Node->AllocateDefaultPins();R.Data->SetNumberField(TEXT("allocate_pins_returns"),1);if(!C.Raw()||!C.FreezePins())return R;
    if(C.LiteralMode&&!C.LiteralDefaults(CanonicalDefaults))return R;
    // Existing empty-default calls retain the exact original wildcard allocation.
    // No Conform call, synthetic link or fake default. Actual
    // Map producer wiring is a separately requested SDK/schema action.
    R.Stage=TEXT("map_mark_modified");C.Status=BS_Dirty;R.Data->SetNumberField(TEXT("mark_modified_calls"),1);FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint.Get());R.Data->SetNumberField(TEXT("mark_modified_returns"),1);if(!C.Raw())return R;
    R.Stage=TEXT("map_verification");if(!C.Raw())return R;
    R.Data->SetObjectField(TEXT("native_map_source_after"),C.SourceJson());R.Data->SetBoolField(TEXT("native_map_source_observed_unchanged"),true);R.Data->SetObjectField(TEXT("after"),C.TargetJson());
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),C.NodePath);J->SetStringField(TEXT("node_id"),C.NodePath);J->SetStringField(TEXT("node_guid"),C.NodeGuid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("node_class_path"),TEXT("/Script/BlueprintGraph.K2Node_CallFunction"));J->SetStringField(TEXT("function_owner_path"),LibraryPath);J->SetStringField(TEXT("function_name"),C.FunctionName);R.Data->SetObjectField(TEXT("node"),J);
    R.Data->SetBoolField(TEXT("created"),true);R.Data->SetBoolField(TEXT("allocated_native_wildcard_shape_verified"),true);R.Data->SetBoolField(TEXT("existing_nodes_and_declarations_preserved"),true);R.Data->SetBoolField(TEXT("transaction_endpoint_observed"),true);
    R.Data->SetStringField(TEXT("change_observation_scope"),TEXT("Bounded held source identity/metadata/layout, target Blueprint/class/CDO metadata, owned graph/node/whole-pin/reference/declaration identities. No CDO/runtime Map values, global/private callback state, disk purity or runtime effect proof."));
    if(C.Neutral&&!C.UntypedCallReceipt())return R;
    if(!C.Raw())return R;if(Operation==TEXT("length")||(!C.Neutral&&!C.DoubleValue&&(Operation==TEXT("contains")||Operation==TEXT("remove")||Operation==TEXT("keys")||Operation==TEXT("values"))))R.Data->SetBoolField(TEXT("map_operation_parity_verified"),true);R.bSuccess=R.bPostconditionPassed=R.bChangeStateKnown=R.bChanged=true;R.Stage=TEXT("complete");return R;
}
namespace
{
const TCHAR* UntypedOperation(const FString& FunctionName)
{
    struct FRow{const TCHAR* Function;const TCHAR* Operation;};
    static constexpr FRow Rows[]={{TEXT("Map_Clear"),TEXT("clear")},{TEXT("Map_Find"),TEXT("find")},{TEXT("Map_Add"),TEXT("add")},{TEXT("Map_Contains"),TEXT("contains")},{TEXT("Map_Remove"),TEXT("remove")},{TEXT("Map_Keys"),TEXT("keys")},{TEXT("Map_Values"),TEXT("values")},{TEXT("Map_Length"),TEXT("length")}};
    for(const auto& Row:Rows)if(FunctionName.Equals(Row.Function,ESearchCase::CaseSensitive))return Row.Operation;return nullptr;
}
}
namespace {
static Function::FResult AddSetCall(const FString& BlueprintPath,const FString& FunctionName,const FString& DefaultsJson,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
 FResult R;FMapScope C(R);C.SetCall=true;C.Neutral=true;const TCHAR* Op=SetOperation(FunctionName);if(!Op){C.Reject(TEXT("set_function_unsupported"),TEXT("Exact admitted native Set operations only."));return R;}C.Operation=Op;C.FunctionName=FunctionName;
 TSharedPtr<FJsonValue> Defaults,Position;if(!Function::Pin::ParseJson(DefaultsJson,Defaults,R)||!Function::Pin::ParseJson(PositionJson,Position,R))return R;if(Defaults->Type==EJson::Null)Defaults=MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
 if(Defaults->Type!=EJson::Object||!Defaults->AsObject()->Values.IsEmpty()){C.Reject(TEXT("set_literal_defaults_pending"),TEXT("Set5 generic wildcard admission first supports empty param_defaults only; literals remain incomplete."));return R;}
 if(Position->Type!=EJson::Array||Position->AsArray().Num()!=2){C.Reject(TEXT("set_position_invalid"),TEXT("Exact int32 pair required."));return R;}int32 XY[2];for(int32 I=0;I<2;++I){const auto& V=Position->AsArray()[I];const double N=V->Type==EJson::Number?V->AsNumber():MAX_dbl;if(V->Type!=EJson::Number||!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32||FMath::TruncToDouble(N)!=N){C.Reject(TEXT("set_position_invalid"),TEXT("Exact int32 pair required."));return R;}XY[I]=static_cast<int32>(N);}C.X=XY[0];C.Y=XY[1];
 if(!C.Begin()||!C.Source()||!C.Target(BlueprintPath,GraphName,GraphId,GraphType))return R;
 if(C.PresetLibrary||C.Blueprint->GetClass()!=UBlueprint::StaticClass()||C.Parent.Get()!=AActor::StaticClass()||!C.Variables.IsEmpty()||!C.Interfaces.IsEmpty()){C.Reject(TEXT("set_host_pending"),TEXT("First Set5 cohort uses an existing own normal direct Actor graph, with own variables/interfaces empty; broader hosts remain incomplete."));return R;}
 R.Data->SetBoolField(TEXT("native_untyped_set_call"),true);R.Data->SetBoolField(TEXT("binary_set_operation"),C.BinarySet());R.Data->SetBoolField(TEXT("node_only_contract"),true);R.Data->SetBoolField(TEXT("set_type_request_specified"),false);R.Data->SetBoolField(TEXT("implicit_data_wiring"),false);R.Data->SetBoolField(TEXT("set_target_requires_connection"),true);R.Data->SetBoolField(TEXT("set_runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"),false);R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("explicit_save_requested"),false);
    R.Data->SetStringField(TEXT("function_class"),C.SourcePath());R.Data->SetStringField(TEXT("function_name"),C.FunctionName);
    R.Stage=TEXT("set_blueprint_modify");R.bMutationAttempted=true;R.bChangeStateKnown=false;
    EObjectFlags PackageFlags=C.TargetPackage->GetFlags();const bool BPModified=C.Blueprint->Modify();if(!C.AfterModify(BPModified,PackageFlags))return R;
    R.Stage=TEXT("set_graph_modify");PackageFlags=C.TargetPackage->GetFlags();const bool GraphModified=C.Graph->Modify();if(!C.AfterModify(GraphModified,PackageFlags))return R;
    R.Stage=TEXT("set_allocate");R.Data->SetNumberField(TEXT("node_allocation_calls"),1);
    UK2Node_CallFunction* Returned=NewObject<UK2Node_CallFunction>(C.Graph.Get(),NAME_None,RF_Transactional);if(!C.Adopt(Returned))return R;
    R.Stage=TEXT("set_attach");C.Attached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(C.Node.Get(),false,false);R.Data->SetNumberField(TEXT("graph_add_node_returns"),1);if(!C.Raw())return R;
    R.Stage=TEXT("set_set_from_function");C.Member.SetExternalMember(FName(*C.FunctionName),C.Library.Get());if(!C.Raw())return R;C.MemberFixed=true;
    R.Data->SetNumberField(TEXT("set_from_function_calls"),1);C.Node->SetFromFunction(C.Native.Get());R.Data->SetNumberField(TEXT("set_from_function_returns"),1);if(!C.Raw())return R;
    C.Node->NodePosX=C.X;C.Node->NodePosY=C.Y;
    R.Stage=TEXT("set_create_guid");R.Data->SetNumberField(TEXT("create_guid_calls"),1);C.Node->CreateNewGuid();R.Data->SetNumberField(TEXT("create_guid_returns"),1);
    if(IsEditorGameThread()&&!IsGarbageCollecting()&&Loaded(C.Node.Get())&&C.Node->GetOuter()==C.Graph.Get()&&Equal(C.Node->GetPathName(),C.NodePath)){
        C.NodeGuid=C.Node->NodeGuid;C.GuidFixed=true;R.Data->SetStringField(TEXT("created_node_guid"),C.NodeGuid.ToString(EGuidFormats::Digits));R.Data->SetBoolField(TEXT("created_node_guid_observed"),true);}
    if(!C.Raw())return R;if(!C.GuidFixed||!C.NodeGuid.IsValid()){C.Reject(TEXT("set_created_guid_invalid"),TEXT("New GUID not safely observed; no cleanup/retry."));return R;}
    R.Stage=TEXT("set_post_placed");R.Data->SetNumberField(TEXT("post_placed_calls"),1);C.Node->PostPlacedNewNode();R.Data->SetNumberField(TEXT("post_placed_returns"),1);if(!C.Raw())return R;
    if(!C.Node->Pins.IsEmpty()){C.Reject(TEXT("set_unexpected_preallocated_pins"),TEXT("PostPlaced unexpectedly allocated pins; retained node is not repaired."));return R;}
    R.Stage=TEXT("set_allocate_pins");R.Data->SetNumberField(TEXT("allocate_pins_calls"),1);C.Node->AllocateDefaultPins();R.Data->SetNumberField(TEXT("allocate_pins_returns"),1);if(!C.Raw()||!C.FreezePins())return R;

    // Existing empty-default calls retain the exact original wildcard allocation.
    // No Conform call, synthetic link or fake default. Actual
    // Map producer wiring is a separately requested SDK/schema action.
    R.Stage=TEXT("set_mark_modified");C.Status=BS_Dirty;R.Data->SetNumberField(TEXT("mark_modified_calls"),1);FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint.Get());R.Data->SetNumberField(TEXT("mark_modified_returns"),1);if(!C.Raw())return R;
    R.Stage=TEXT("set_verification");if(!C.Raw())return R;
    R.Data->SetObjectField(TEXT("native_set_source_after"),C.SourceJson());R.Data->SetBoolField(TEXT("native_set_source_observed_unchanged"),true);R.Data->SetObjectField(TEXT("after"),C.TargetJson());
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),C.NodePath);J->SetStringField(TEXT("node_id"),C.NodePath);J->SetStringField(TEXT("node_guid"),C.NodeGuid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("node_class_path"),TEXT("/Script/BlueprintGraph.K2Node_CallFunction"));J->SetStringField(TEXT("function_owner_path"),C.SourcePath());J->SetStringField(TEXT("function_name"),C.FunctionName);R.Data->SetObjectField(TEXT("node"),J);
    R.Data->SetBoolField(TEXT("created"),true);R.Data->SetBoolField(TEXT("allocated_native_wildcard_shape_verified"),true);R.Data->SetBoolField(TEXT("existing_nodes_and_declarations_preserved"),true);R.Data->SetBoolField(TEXT("transaction_endpoint_observed"),true);
    R.Data->SetStringField(TEXT("change_observation_scope"),TEXT("Bounded held source identity/metadata/layout, target Blueprint/class/CDO metadata, owned graph/node/whole-pin/reference/declaration identities. No CDO/runtime Map values, global/private callback state, disk purity or runtime effect proof."));
    if(C.Neutral&&!C.UntypedCallReceipt())return R;
    if(!C.Raw())return R;if(C.Operation==TEXT("length")||(!C.Neutral&&!C.DoubleValue&&(C.Operation==TEXT("contains")||C.Operation==TEXT("remove")||C.Operation==TEXT("keys")||C.Operation==TEXT("values"))))R.Data->SetBoolField(TEXT("set_operation_parity_verified"),true);R.bSuccess=R.bPostconditionPassed=R.bChangeStateKnown=R.bChanged=true;R.Stage=TEXT("complete");return R;

}

}
bool SupportsUntypedCall(const FString& FunctionClass,const FString& FunctionName)
{return (FunctionClass.Equals(LibraryPath,ESearchCase::CaseSensitive)&&UntypedOperation(FunctionName)!=nullptr)||(FunctionClass.Equals(SetLibraryPath,ESearchCase::CaseSensitive)&&SetOperation(FunctionName)!=nullptr);}
Function::FResult AddUntypedCall(const FString& BlueprintPath,const FString& FunctionName,const FString& ParamDefaultsJson,
    const FString& NodePositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    if(SetOperation(FunctionName))return AddSetCall(BlueprintPath,FunctionName,ParamDefaultsJson,NodePositionJson,GraphName,GraphId,GraphType);
    const TCHAR* Operation=UntypedOperation(FunctionName);if(!Operation){FResult R;FMapScope C(R);C.Reject(TEXT("map_operation_unsupported"),TEXT("Only exact native Map8 functions admit the untyped checked-call route."));return R;}
    return AddNodeInternal(BlueprintPath,Operation,FString(),FString(),ParamDefaultsJson,NodePositionJson,GraphName,GraphId,GraphType,false,true);
}
Function::FResult AddNode(const FString& BlueprintPath,const FString& Operation,const FString& KeyType,const FString& ValueType,
    const FString& ParamDefaultsJson,const FString& NodePositionJson,const FString& GraphName,const FString& GraphId,
    const FString& GraphType,bool bCreateGraphIfMissing)
{return AddNodeInternal(BlueprintPath,Operation,KeyType,ValueType,ParamDefaultsJson,NodePositionJson,GraphName,GraphId,GraphType,bCreateGraphIfMissing,false);}
}




