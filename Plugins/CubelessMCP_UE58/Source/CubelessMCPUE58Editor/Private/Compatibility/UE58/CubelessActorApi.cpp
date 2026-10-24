// UE58 adaptation of the six named UE56 editor Actor commands.
// Original route/behavior evidence: editor-actor-port-plan-K.md. No bridge code.
#include "Compatibility/UE58/CubelessActorApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Algo/Count.h"

#include "Camera/CameraActor.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Level.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/TargetPoint.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Editor/TransBuffer.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Selection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Subsystems/EditorElementSubsystem.h"
#include "Layers/LayersSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::Actor
{
// ACJ-v2: NewlyCreated alone is not a complete on-disk-existence test. The
// regular successful SavePackage path clears it, while autosave/procedural paths
// differ (SavePackage2.cpp3597..3615). Observe the actual mounted .umap instead;
// this is a file-backed editing admission, never a claim of save success or
// memory/disk content equality. Dirty memory is legitimate for the original
// Actor batch workflow, exactly as it is for a normal-flag saved map.
struct FFileBackedNewMapWitness
{
    TStrongObjectPtr<UPackage> Package;FString PackageName,Filename;
    bool Initialized=false,NewMap=false;
    TSharedRef<FJsonObject> Receipt=MakeShared<FJsonObject>();
    bool Current(UWorld* World,ULevel* Level)
    {
        if(!IsValid(World)||!IsValid(Level))return false;
        UPackage* P=Level->GetOutermost();if(!IsValid(P)||P->HasAnyPackageFlags(PKG_EditorOnly))return false;
        const uint32 Flags=uint32(P->GetPackageFlags());const uint32 NewMapFlags=uint32(PKG_ContainsMap|PKG_NewlyCreated);
        if(Initialized)
        {
            if(!NewMap)return (Flags&uint32(PKG_NewlyCreated))==0; // unchanged original normal-flag admission
            Receipt->SetBoolField(TEXT("current_dirty"),P->IsDirty());Receipt->SetNumberField(TEXT("current_package_flags"),Flags);
            return P==Package.Get()&&P->GetClass()==UPackage::StaticClass()&&P->GetName().Equals(PackageName,ESearchCase::CaseSensitive)&&World->GetOutermost()==P&&Level->OwningWorld==World&&World->PersistentLevel.IsResolved()&&World->PersistentLevel.Get()==Level&&World->GetCurrentLevel()==Level&&(Flags==NewMapFlags||Flags==uint32(PKG_ContainsMap));
        }
        Receipt->SetBoolField(TEXT("newly_created_present"),(Flags&uint32(PKG_NewlyCreated))!=0);
        if((Flags&uint32(PKG_NewlyCreated))==0){Initialized=true;return true;}
        // No lazy resolving, package load, flag clearing, dirty clearing, save,
        // repair, global cache or source/instance baseline rebinding occurs.
        const FString Name=P->GetName();FString DiskFile;
        const bool Shape=Flags==NewMapFlags&&P->GetClass()==UPackage::StaticClass()&&World->GetOutermost()==P&&Level->OwningWorld==World&&World->PersistentLevel.IsResolved()&&World->PersistentLevel.Get()==Level&&World->GetCurrentLevel()==Level&&Name.StartsWith(TEXT("/Game/"))&&Name.Len()<NAME_SIZE&&FPackageName::IsValidLongPackageName(Name,false);
        const bool Exists=Shape&&FPackageName::DoesPackageExist(Name,&DiskFile,false);
        const bool MapExtension=Exists&&FPaths::GetExtension(DiskFile,true).Equals(FPackageName::GetMapPackageExtension(),ESearchCase::IgnoreCase);
        const int64 Bytes=MapExtension?IFileManager::Get().FileSize(*DiskFile):-1;
        Receipt->SetStringField(TEXT("package"),Name);Receipt->SetNumberField(TEXT("initial_package_flags"),Flags);Receipt->SetBoolField(TEXT("initial_dirty"),P->IsDirty());Receipt->SetBoolField(TEXT("exact_game_map_shape"),Shape);Receipt->SetBoolField(TEXT("disk_exists"),Exists);Receipt->SetBoolField(TEXT("map_extension"),MapExtension);Receipt->SetStringField(TEXT("disk_filename"),DiskFile);Receipt->SetNumberField(TEXT("disk_file_size"),double(Bytes));Receipt->SetBoolField(TEXT("memory_disk_content_equality_claimed"),false);Receipt->SetBoolField(TEXT("save_success_inferred"),false);Receipt->SetBoolField(TEXT("dirty_memory_allowed"),true);
        if(!Shape||!MapExtension||Bytes<=0){Receipt->SetBoolField(TEXT("admitted"),false);return false;}
        Package.Reset(P);PackageName=Name;Filename=DiskFile;Initialized=true;NewMap=true;Receipt->SetBoolField(TEXT("admitted"),true);return true;
    }
};
// QA: a separate concrete lease, stored behind the existing operation-local scope.
// It admits only a direct Actor Blueprint with one stock SCS Scene root (plus its optional stock editor visualization sprite) and no
// authored nonprimitive variable/interface or nonempty lifecycle bytecode. No BP script runs
// are inferred from a transform result; opaque Engine callbacks stay opaque.
struct FBlueprintActorMutation
{
    FFileBackedNewMapWitness NewMapWitness;
    TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<ULevel> Level;TStrongObjectPtr<AActor> Actor;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UClass> Class,Skeleton;
    TStrongObjectPtr<AActor> CDO;TStrongObjectPtr<USimpleConstructionScript> SCS;TStrongObjectPtr<USCS_Node> SCSNode;TStrongObjectPtr<USceneComponent> Template,Root;
    TArray<TStrongObjectPtr<UObject>> Holds;TArray<TStrongObjectPtr<USceneComponent>> RetiredRoots;
    FString SourcePath,ClassPath,SkeletonPath,ActorPath,RootName,TemplatePath,File,FileSHA;FGuid VariableGuid;FName VariableName;
    UObject* TemplateOuter=nullptr;EObjectFlags ClassFlags=RF_NoFlags,SkeletonFlags=RF_NoFlags,TemplateFlags=RF_NoFlags;EClassFlags ClassTypeFlags=CLASS_None,SkeletonTypeFlags=CLASS_None;
    FTransform BeforeTransform=FTransform::Identity,ExpectedTransform=FTransform::Identity,TemplateTransform=FTransform::Identity;
    EComponentMobility::Type Mobility=EComponentMobility::Static;bool Visible=true,Hidden=false;
    TArray<FName> ActorTags,RootTags,TemplateTags;bool Stopped=false,Retired=false,Reacquiring=false,Dispatched=false;
    FString FailureCode,FailureStage;TSharedRef<FJsonObject> Data=MakeShared<FJsonObject>();
    FObjectProperty* ComponentProperty=nullptr;FObjectProperty* SkeletonProperty=nullptr;
    struct FField{FProperty* P;UStruct* Owner;FFieldClass* Class;FName Name;uint64 Flags;int32 Dim,Offset,Size;UObject* Binding;};TArray<FField> Fields,SkeletonFields;
    // ACJ: declaration, CDO backing values and instance overrides are separate
    // baselines. No construction, getter/exporter, compile or CDO creation here.
    struct FOwnScalar
    {
        FBPVariableDescription Declaration;FProperty* Generated=nullptr;FProperty* SkeletonProperty=nullptr;
        uint8 GeneratedBoolSize=0,GeneratedBoolOffset=0,GeneratedBoolByteMask=0,GeneratedBoolFieldMask=0;
        uint8 SkeletonBoolSize=0,SkeletonBoolOffset=0,SkeletonBoolByteMask=0,SkeletonBoolFieldMask=0;
        FString CDOValue,InstanceValue;FText CDOText,InstanceText;bool ResetByConstruction=false;
    };
    TArray<FOwnScalar> OwnScalars;bool OwnScalarsReady=false,OwnEditorTransformDispatched=false;
    struct FOther{TStrongObjectPtr<AActor> Actor;TStrongObjectPtr<USceneComponent> Root;UClass* Class;UObject* Outer;FString Path,Label;FTransform Transform;TArray<FName> Tags;bool Selected;};TArray<FOther> Others;
    struct FGraph{TStrongObjectPtr<UEdGraph> Graph;FGuid Guid;UObject* Outer;UClass* Schema;TArray<UEdGraphNode*> Nodes;};
    struct FPin{UEdGraphPin* P;FGuid Guid;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;FString Raw,Auto;FText Text;UObject* Default;TArray<UEdGraphPin*> Links;uint32 Flags;};
    struct FNode{TStrongObjectPtr<UEdGraphNode> Node;UEdGraph* Graph;UClass* Class;FGuid Guid;FString Path,Comment;int32 X,Y;TArray<FPin> Pins;};TArray<FGraph> Graphs;TArray<FNode> Nodes;TArray<UEdGraph*> GraphInventory;
    TArray<TObjectPtr<UEdGraph>> Functions,Macros,Events,Delegates;
    struct FFunction{FName Name;TStrongObjectPtr<UFunction> Function;UClass* Owner;EFunctionFlags Flags;TArray<uint8> Script;};TArray<FFunction> Lifecycle;
    bool Bound=false;EObjectFlags CDOFlags=RF_NoFlags;UObject* CDOOuter=nullptr;FString CDOPath;
    TStrongObjectPtr<USceneComponent> CDORoot;FTransform CDOTransform=FTransform::Identity;TArray<FName> CDOTags;
    EComponentMobility::Type TemplateMobility=EComponentMobility::Static;bool TemplateVisible=true,TemplateHidden=false;
    FObjectProperty* NativeRootProperty=nullptr;
    TStrongObjectPtr<UBillboardComponent> Sprite;TArray<TStrongObjectPtr<UBillboardComponent>> RetiredSprites;
    bool SpritePresent=false,RootVisualize=false,TemplateVisualize=false;FTransform SpriteRelative=FTransform::Identity;TArray<FName> SpriteTags;
    // UW: delete-only source/component projection. No exporter, construction,
    // compile, accessor invocation, reflected scratch destruction or retry.
    bool AuthoredDelete=false,AuthoredReady=false;FObjectProperty* AuthoredParentProperty=nullptr;
    struct FUWField
    {
        FProperty* Property=nullptr;UStruct* Owner=nullptr;FFieldClass* Kind=nullptr;
        FName Name;uint64 Flags=0;int32 Dim=0,Offset=0,Size=0;
        FProperty* Inner=nullptr;FProperty* Value=nullptr;FProperty* RootProperty=nullptr;
        UStruct* Structure=nullptr;UClass* ObjectClass=nullptr;
        uint8 BoolSize=0,BoolOffset=0,BoolByteMask=0,BoolFieldMask=0;bool Accessor=false;
    };
    struct FUWType{UStruct* Type=nullptr;UClass* Class=nullptr;UObject* Outer=nullptr;UStruct* Super=nullptr;int32 Size=0;uint32 ClassFlags=0;TArray<FProperty*> Fields;};
    struct FUWReference{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;};
    struct FUWScene
    {
        TStrongObjectPtr<USceneComponent> Object;UClass* Class=nullptr;UObject* Outer=nullptr;
        FName Name,Socket;EObjectFlags Flags=RF_NoFlags;EComponentCreationMethod Creation=EComponentCreationMethod::Native;
        USceneComponent* Parent=nullptr;TArray<USceneComponent*> Children;
        FTransform Relative=FTransform::Identity,World=FTransform::Identity;
        FString Path,Values;bool Registered=false;
    };
    struct FUWSCS
    {
        TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<USceneComponent> Template;
        UClass* Class=nullptr;FName Name,Socket,ParentName,ParentOwner;FGuid Guid;
        bool NativeParent=false;FText Category;TArray<USCS_Node*> Children;TArray<TPair<FName,FString>> Metadata;
        FObjectProperty* Generated=nullptr;FObjectProperty* Skeleton=nullptr;USceneComponent* Instance=nullptr;
        FUWScene Source;
    };
    TArray<FUWSCS> AuthoredNodes;TArray<USCS_Node*> AuthoredAll,AuthoredRoots;
    TArray<FUWScene> AuthoredComponents;TArray<FUWField> AuthoredFields;TArray<FUWType> AuthoredTypes;
    TArray<FUWReference> AuthoredReferences;TArray<FText> AuthoredTextHolds;

    bool UWFail(const TCHAR* Stage,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual)
    {return Fail(Stage,TEXT("actor_authored_scs_delete_pending_or_changed"),Object,Property,Expected,Actual,true);}
    bool UWParent(USceneComponent* C,USceneComponent*& Parent)
    {
        Parent=nullptr;auto* P=AuthoredParentProperty;
        if(!Live(C)||!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=USceneComponent::StaticClass()||P->GetOwnerProperty()!=P||P->GetFName()!=FName(TEXT("AttachParent"))||P->ArrayDim!=1||P->GetSize()!=sizeof(TObjectPtr<UObject>)||!P->PropertyClass.IsResolved()||P->PropertyClass.Get()!=USceneComponent::StaticClass()||!UWField(P,!AuthoredReady))return false;
        const auto* Slot=P->ContainerPtrToValuePtr<TObjectPtr<UObject>>(C);if(!Slot||!Slot->IsResolved())return false;
        UObject* Value=Slot->Get();if(Value&&(!Live(Value)||!Value->IsA<USceneComponent>()))return false;Parent=static_cast<USceneComponent*>(Value);return true;
    }
    bool UWType(UStruct* T,bool Capture)
    {
        if(!Live(T)||!Live(T->GetClass())||!T->GetPackage()->GetName().StartsWith(TEXT("/Script/")))return false;
        TArray<FProperty*> FieldsNow;
        for(TFieldIterator<FProperty> It(T,EFieldIterationFlags::IncludeSuper);It;++It)
        {if(FieldsNow.Num()>=512)return false;FieldsNow.Add(*It);}
        const int32 Index=AuthoredTypes.IndexOfByPredicate([&](const FUWType& E){return E.Type==T;});
        if(Index!=INDEX_NONE)
        {const auto& E=AuthoredTypes[Index];return E.Class==T->GetClass()&&E.Outer==T->GetOuter()&&E.Super==T->GetSuperStruct()&&E.Size==T->GetPropertiesSize()&&E.ClassFlags==(T->IsA<UClass>()?uint32(static_cast<UClass*>(T)->GetClassFlags()):0)&&E.Fields==FieldsNow;}
        if(!Capture)return false;Holds.Emplace(T);Holds.Emplace(T->GetClass());Holds.Emplace(T->GetOuter());Holds.Emplace(T->GetPackage());
        FUWType E;E.Type=T;E.Class=T->GetClass();E.Outer=T->GetOuter();E.Super=T->GetSuperStruct();E.Size=T->GetPropertiesSize();E.ClassFlags=T->IsA<UClass>()?uint32(static_cast<UClass*>(T)->GetClassFlags()):0;E.Fields=MoveTemp(FieldsNow);AuthoredTypes.Add(MoveTemp(E));return true;
    }
    bool UWField(FProperty* P,bool Capture)
    {
        if(!P||!Live(P->GetOwnerStruct()))return false;
        // A pure backing-value traversal has no callbacks. Close full owner
        // inventories once at the source barrier rather than once per scalar.
        if(!AuthoredTypes.ContainsByPredicate([&](const FUWType& E){return E.Type==P->GetOwnerStruct();})&&(!Capture||!UWType(P->GetOwnerStruct(),true)))return false;
        const FUWType* Type=AuthoredTypes.FindByPredicate([&](const FUWType& E){return E.Type==P->GetOwnerStruct();});
        if(!Type||!Type->Fields.Contains(P->GetOwnerProperty()))return false;
        FUWField F;F.Property=P;F.Owner=P->GetOwnerStruct();F.Kind=P->GetClass();F.Name=P->GetFName();F.Flags=uint64(P->GetPropertyFlags());
        const bool Known=F.Kind==FBoolProperty::StaticClass()||F.Kind==FInt8Property::StaticClass()||F.Kind==FInt16Property::StaticClass()||F.Kind==FIntProperty::StaticClass()||F.Kind==FInt64Property::StaticClass()||F.Kind==FByteProperty::StaticClass()||F.Kind==FUInt16Property::StaticClass()||F.Kind==FUInt32Property::StaticClass()||F.Kind==FUInt64Property::StaticClass()||F.Kind==FFloatProperty::StaticClass()||F.Kind==FDoubleProperty::StaticClass()||F.Kind==FNameProperty::StaticClass()||F.Kind==FStrProperty::StaticClass()||F.Kind==FTextProperty::StaticClass()||F.Kind==FObjectProperty::StaticClass()||F.Kind==FClassProperty::StaticClass()||F.Kind==FEnumProperty::StaticClass()||F.Kind==FStructProperty::StaticClass()||F.Kind==FArrayProperty::StaticClass()||F.Kind==FSetProperty::StaticClass()||F.Kind==FMapProperty::StaticClass();
        if(!Known)return false;
        F.Dim=P->ArrayDim;F.Offset=P->GetOffset_ForInternal();F.Size=P->GetSize();F.RootProperty=P->GetOwnerProperty();F.Accessor=P->HasSetterOrGetter();
        if(F.Dim<1||F.Dim>64||F.Offset<0||F.Size<=0||F.Size>1024*1024)return false;
        if(F.Kind==FArrayProperty::StaticClass())F.Inner=static_cast<FArrayProperty*>(P)->Inner;
        else if(F.Kind==FSetProperty::StaticClass())F.Inner=static_cast<FSetProperty*>(P)->ElementProp;
        else if(F.Kind==FMapProperty::StaticClass()){F.Inner=static_cast<FMapProperty*>(P)->KeyProp;F.Value=static_cast<FMapProperty*>(P)->ValueProp;}
        else if(F.Kind==FEnumProperty::StaticClass())F.Inner=static_cast<FEnumProperty*>(P)->GetUnderlyingProperty();
        else if(F.Kind==FStructProperty::StaticClass()){auto* S=static_cast<FStructProperty*>(P);if(!S->Struct.IsResolved())return false;F.Structure=S->Struct.Get();}
        else if(F.Kind==FObjectProperty::StaticClass()||F.Kind==FClassProperty::StaticClass())
        {auto* O=static_cast<FObjectProperty*>(P);if(!O->PropertyClass.IsResolved())return false;F.ObjectClass=O->PropertyClass.Get();}
        else if(F.Kind==FBoolProperty::StaticClass())
        {auto* B=static_cast<FBoolProperty*>(P);F.BoolSize=B->GetBoolFieldSize();F.BoolOffset=B->GetByteOffset();F.BoolByteMask=B->GetByteMask();F.BoolFieldMask=B->GetFieldMask();}
        const int32 Index=AuthoredFields.IndexOfByPredicate([&](const FUWField& E){return E.Property==P;});
        if(Index!=INDEX_NONE)
        {
            const auto& E=AuthoredFields[Index];
            return E.Owner==F.Owner&&E.Kind==F.Kind&&E.Name==F.Name&&E.Flags==F.Flags&&E.Dim==F.Dim&&E.Offset==F.Offset&&E.Size==F.Size&&E.RootProperty==F.RootProperty&&E.Accessor==F.Accessor
                &&E.Inner==F.Inner&&E.Value==F.Value&&E.Structure==F.Structure&&E.ObjectClass==F.ObjectClass&&E.BoolSize==F.BoolSize&&E.BoolOffset==F.BoolOffset&&E.BoolByteMask==F.BoolByteMask&&E.BoolFieldMask==F.BoolFieldMask;
        }
        if(!Capture||AuthoredFields.Num()>=4096)return false;
        if(F.Structure)Holds.Emplace(F.Structure);if(F.ObjectClass)Holds.Emplace(F.ObjectClass);AuthoredFields.Add(F);return true;
    }
    bool UWRef(UObject* O,bool Capture,FString& Out)
    {
        if(!O){Out+=TEXT("null;");return true;}
        if(!Live(O)||!Live(O->GetClass())||!Live(O->GetPackage()))return false;
        // Components/templates and arbitrary owned default subobjects are not
        // recursively exported. Only external runtime references are admitted.
        const FString PackageName=O->GetPackage()->GetName();
        if(!(PackageName.StartsWith(TEXT("/Engine/"))||PackageName.StartsWith(TEXT("/Game/"))||PackageName==TEXT("/Script/Engine")||PackageName==TEXT("/Script/CoreUObject")||PackageName==TEXT("/Script/PhysicsCore")))return false;
        if(O==Blueprint.Get()||O==Class.Get()||O==Actor.Get()||O->IsIn(Blueprint.Get())||O->IsIn(Class.Get())||O->IsIn(Actor.Get()))return false;
        const int32 Index=AuthoredReferences.IndexOfByPredicate([&](const FUWReference& E){return E.Object.Get()==O;});
        if(Index==INDEX_NONE)
        {
            if(!Capture||AuthoredReferences.Num()>=512)return false;
            FUWReference E;E.Object.Reset(O);E.Class=O->GetClass();E.Outer=O->GetOuter();E.Package=O->GetPackage();E.Path=O->GetPathName();Holds.Emplace(E.Class);AuthoredReferences.Add(MoveTemp(E));
        }
        Out+=FString::Printf(TEXT("ref:%p;"),O);return true;
    }
    static void UWToken(FString& Out,const FString& V){Out+=LexToString(V.Len())+TEXT(":")+V+TEXT(";");}
    bool UWValue(FProperty* P,const void* Address,bool Capture,FString& Out,int32 Depth=0)
    {
        if(Depth>16||!Address||!UWField(P,Capture)||Out.Len()>1024*1024)return false;
        FFieldClass* K=P->GetClass();
        if(K==FBoolProperty::StaticClass()){Out+=static_cast<FBoolProperty*>(P)->GetPropertyValue(Address)?TEXT("1;"):TEXT("0;");return true;}
        if(K==FInt8Property::StaticClass()||K==FInt16Property::StaticClass()||K==FIntProperty::StaticClass()||K==FInt64Property::StaticClass())
        {Out+=LexToString(static_cast<FNumericProperty*>(P)->GetSignedIntPropertyValue(Address))+TEXT(";");return true;}
        if(K==FByteProperty::StaticClass()||K==FUInt16Property::StaticClass()||K==FUInt32Property::StaticClass()||K==FUInt64Property::StaticClass())
        {Out+=LexToString(static_cast<FNumericProperty*>(P)->GetUnsignedIntPropertyValue(Address))+TEXT(";");return true;}
        if(K==FFloatProperty::StaticClass()){Out+=BytesToHex(static_cast<const uint8*>(Address),sizeof(float))+TEXT(";");return true;}
        if(K==FDoubleProperty::StaticClass()){Out+=BytesToHex(static_cast<const uint8*>(Address),sizeof(double))+TEXT(";");return true;}
        if(K==FNameProperty::StaticClass()){const auto& N=*static_cast<const FName*>(Address);UWToken(Out,N.ToString());return true;}
        if(K==FStrProperty::StaticClass()){const auto& S=*static_cast<const FString*>(Address);if(S.Len()>65536)return false;UWToken(Out,S);return true;}
        if(K==FTextProperty::StaticClass())
        {const auto& T=*static_cast<const FText*>(Address);if(AuthoredTransform&&UXLogicalText)return UXText(T,Out);if(Capture||(AuthoredTransform&&UXHoldText))AuthoredTextHolds.Add(T);Out+=FString::Printf(TEXT("%p:%u;"),FTextInspector::GetSharedDataId(T),FTextInspector::GetFlags(T));return true;}
        if(K==FObjectProperty::StaticClass()||K==FClassProperty::StaticClass())
        {const auto* Slot=static_cast<const TObjectPtr<UObject>*>(Address);return Slot->IsResolved()&&UWRef(Slot->Get(),Capture,Out);}
        if(K==FEnumProperty::StaticClass())return UWValue(static_cast<FEnumProperty*>(P)->GetUnderlyingProperty(),Address,Capture,Out,Depth+1);
        if(K==FStructProperty::StaticClass())
        {
            auto* T=static_cast<FStructProperty*>(P)->Struct.Get();if(!AuthoredTypes.ContainsByPredicate([&](const FUWType& E){return E.Type==T;})&&(!Capture||!UWType(T,true)))return false;
            const auto* E=AuthoredTypes.FindByPredicate([&](const FUWType& R){return R.Type==T;});const TArray<FProperty*> FieldsNow=E->Fields;
            if(FieldsNow.IsEmpty())return false;
            for(FProperty* Child:FieldsNow)
            {
                if(Child->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;
                if(!UWField(Child,Capture)||Child->GetOffset_ForInternal()+Child->GetSize()>T->GetStructureSize())return false;
                for(int32 I=0;I<Child->ArrayDim;++I)if(!UWValue(Child,Child->ContainerPtrToValuePtr<void>(Address,I),Capture,Out,Depth+1))return false;
            }return true;
        }
        if(K==FArrayProperty::StaticClass())
        {auto* A=static_cast<FArrayProperty*>(P);if(!UWField(A->Inner,Capture))return false;FScriptArrayHelper H(A,Address);if(H.Num()>256)return false;Out+=LexToString(H.Num())+TEXT("[");for(int32 I=0;I<H.Num();++I)if(!UWValue(A->Inner,H.GetRawPtr(I),Capture,Out,Depth+1))return false;Out+=TEXT("]");return true;}
        if(K==FSetProperty::StaticClass())
        {auto* A=static_cast<FSetProperty*>(P);if(!UWField(A->ElementProp,Capture))return false;FScriptSetHelper H(A,Address);if(H.Num()>256||H.GetMaxIndex()>1024)return false;Out+=LexToString(H.Num())+TEXT("{");for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)&&!UWValue(A->ElementProp,H.GetElementPtr(I),Capture,Out,Depth+1))return false;Out+=TEXT("}");return true;}
        if(K==FMapProperty::StaticClass())
        {auto* A=static_cast<FMapProperty*>(P);if(!UWField(A->KeyProp,Capture)||!UWField(A->ValueProp,Capture))return false;FScriptMapHelper H(A,Address);if(H.Num()>256||H.GetMaxIndex()>1024)return false;Out+=LexToString(H.Num())+TEXT("{");for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)&&(!UWValue(A->KeyProp,H.GetKeyPtr(I),Capture,Out,Depth+1)||!UWValue(A->ValueProp,H.GetValuePtr(I),Capture,Out,Depth+1)))return false;Out+=TEXT("}");return true;}
        return false; // Opaque/native unreflected payload is original-valid pending.
    }
    bool UWValues(USceneComponent* C,bool Capture,FString& Out)
    {
        if(!Live(C)||(!AuthoredTypes.ContainsByPredicate([&](const FUWType& E){return E.Type==C->GetClass();})&&(!Capture||!UWType(C->GetClass(),true))))return false;
        const auto* Type=AuthoredTypes.FindByPredicate([&](const FUWType& E){return E.Type==C->GetClass();});const TArray<FProperty*> FieldsNow=Type->Fields;
        for(FProperty* P:FieldsNow)
        {
            if(!P->HasAnyPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;
            // Only the actual instance transform triplet is a planned value.
            // Source template values and all other instance overrides stay exact.
            if(AuthoredTransform&&C->GetOuter()==Actor.Get()&&P->GetOwnerStruct()==USceneComponent::StaticClass()&&(P->GetFName()==FName(TEXT("RelativeLocation"))||P->GetFName()==FName(TEXT("RelativeRotation"))||P->GetFName()==FName(TEXT("RelativeScale3D"))))continue;
            if(!UWField(P,Capture)||P->GetOffset_ForInternal()+P->GetSize()>C->GetClass()->GetPropertiesSize())return false;
            UWToken(Out,P->GetPathName());
            for(int32 I=0;I<P->ArrayDim;++I)if(!UWValue(P,P->ContainerPtrToValuePtr<void>(C,I),Capture,Out))return UWFail(TEXT("authored_values"),C->GetPathName(),TEXT("editable_field"),TEXT("bounded direct reflected native value; opaque shapes remain valid pending"),P->GetPathName());
        }return true;
    }
    bool UWSceneCapture(USceneComponent* C,FUWScene& E,bool Source)
    {
        if(!Live(C)||!Live(C->GetClass())||!C->GetClass()->HasAnyClassFlags(CLASS_Native)||!C->GetClass()->ClassGeneratedBy.IsResolved()||C->GetClass()->ClassGeneratedBy.Get()!=nullptr||C->GetClass()->GetPackage()->GetName()!=TEXT("/Script/Engine"))return false;
        if(!UWParent(C,E.Parent))return false;
        E.Object.Reset(C);E.Class=C->GetClass();Holds.Emplace(E.Class);E.Outer=C->GetOuter();E.Name=C->GetFName();E.Path=C->GetPathName();E.Flags=C->GetFlags();E.Creation=C->CreationMethod;E.Socket=C->GetAttachSocketName();
        for(const auto& Child:C->GetAttachChildren()){if(!Child.IsResolved())return false;E.Children.Add(Child.Get());}
        E.Relative=C->GetRelativeTransform();E.World=C->GetComponentTransform();E.Registered=C->IsRegistered();
        if(Source&&(C->GetOwner()||C->GetWorld()||E.Parent||!E.Children.IsEmpty()||E.Registered))return false;
        return UWValues(C,true,E.Values);
    }
    bool UWSceneCurrent(const FUWScene& E,bool Source,const TCHAR* Stage)
    {
        auto* C=E.Object.Get();if(!Live(C)||C->GetClass()!=E.Class||C->GetOuter()!=E.Outer)return UWFail(Stage,E.Path,TEXT("component_identity"),TEXT("captured live class/outer"),TEXT("changed or unavailable"));
        USceneComponent* Parent=nullptr;if(!UWParent(C,Parent)||C->GetFName()!=E.Name||!C->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||((AuthoredTransform&&!Source)?!UXFlags(C,E.Flags,Stage):C->GetFlags()!=E.Flags)||C->CreationMethod!=E.Creation||C->GetAttachSocketName()!=E.Socket||Parent!=E.Parent||C->GetAttachChildren().Num()!=E.Children.Num()||!C->GetRelativeTransform().Equals(E.Relative,(AuthoredTransform&&!Source)?1.e-4:0.0)||C->IsRegistered()!=E.Registered)return UWFail(Stage,E.Path,TEXT("component_role"),TEXT("captured identity/attachment/relative/registration"),TEXT("changed or unavailable"));
        for(int32 I=0;I<E.Children.Num();++I)if(!C->GetAttachChildren()[I].IsResolved()||C->GetAttachChildren()[I].Get()!=E.Children[I])return UWFail(Stage,E.Path,TEXT("ordered_children"),TEXT("captured"),TEXT("changed"));
        if(Source?(C->GetOwner()!=nullptr||C->GetWorld()!=nullptr):(C->GetOwner()!=Actor.Get()||C->GetWorld()!=World.Get()||!C->GetComponentTransform().Equals(E.World,AuthoredTransform?1.e-4:0.0)))return UWFail(Stage,E.Path,TEXT("owner/world"),TEXT("captured role"),TEXT("changed"));
        if(AuthoredTransform&&!Source&&!UXComponentReady(C))return UWFail(Stage,E.Path,TEXT("simulation/weld"),TEXT("none"),TEXT("changed"));
        FString Values;if(!UWValues(C,false,Values)||Values!=E.Values)return UWFail(Stage,E.Path,TEXT("reflected_editable_values"),TEXT("captured direct values"),TEXT("changed or unsupported"));return true;
    }
    bool CaptureAuthoredDelete()
    {
        if(SCS->GetAllNodes().Num()<1||SCS->GetAllNodes().Num()>64||SCS->GetRootNodes().Num()!=1)return UWFail(TEXT("bind_authored_scs"),SourcePath,TEXT("SCS count"),TEXT("1..64 nodes, one own scene root"),LexToString(SCS->GetAllNodes().Num()));
        AuthoredAll=SCS->GetAllNodes();AuthoredRoots=SCS->GetRootNodes();TSet<USCS_Node*> Unique;TSet<FGuid> Guids;TSet<FName> Names;
        for(USCS_Node* N:AuthoredAll)
        {
            if(!Live(N)||N->GetClass()!=USCS_Node::StaticClass()||N->GetOuter()!=SCS.Get()||Unique.Contains(N)||!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved())return false;
            Unique.Add(N);FUWSCS E;E.Node.Reset(N);E.Class=N->ComponentClass.Get();E.Template.Reset(Cast<USceneComponent>(N->ComponentTemplate.Get()));E.Name=N->GetVariableName();E.Guid=N->VariableGuid;E.Socket=N->AttachToName;E.ParentName=N->ParentComponentOrVariableName;E.ParentOwner=N->ParentComponentOwnerClassName;E.NativeParent=N->bIsParentComponentNative;
            if(!Live(E.Class)||!E.Class->HasAnyClassFlags(CLASS_Native)||!E.Class->ClassGeneratedBy.IsResolved()||E.Class->ClassGeneratedBy.Get()!=nullptr||!Live(E.Template.Get())||E.Template->GetClass()!=E.Class||E.Template->GetOuter()!=Class.Get()||!E.Guid.IsValid()||E.Name.IsNone()||Guids.Contains(E.Guid)||Names.Contains(E.Name)||E.NativeParent||!E.ParentName.IsNone()||!E.ParentOwner.IsNone())return UWFail(TEXT("bind_authored_scs"),SourcePath,TEXT("own native Scene SCS role"),TEXT("closed own tree/native runtime component/template+GUID"),E.Name.ToString());
            Holds.Emplace(E.Class);Guids.Add(E.Guid);Names.Add(E.Name);
            for(const auto& H:N->ChildNodes){if(!H.IsResolved())return false;E.Children.Add(H.Get());}
            E.Category=N->CategoryName;for(const auto& M:N->MetaDataArray)E.Metadata.Add({M.DataKey,M.DataValue});
            for(const auto& F:Fields)if(F.Name==E.Name&&F.Class==FObjectProperty::StaticClass())E.Generated=static_cast<FObjectProperty*>(F.P);
            for(const auto& F:SkeletonFields)if(F.Name==E.Name&&F.Class==FObjectProperty::StaticClass())E.Skeleton=static_cast<FObjectProperty*>(F.P);
            if(!E.Generated||!E.Skeleton||E.Generated->ArrayDim!=1||E.Skeleton->ArrayDim!=1||!E.Generated->PropertyClass.IsResolved()||E.Generated->PropertyClass.Get()!=E.Class||!E.Skeleton->PropertyClass.IsResolved()||E.Skeleton->PropertyClass.Get()!=E.Class)return false;
            const auto* Slot=E.Generated->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Actor.Get());if(!Slot||!Slot->IsResolved())return false;
            E.Instance=Cast<USceneComponent>(Slot->Get());if(!Live(E.Instance)||E.Instance->GetClass()!=E.Class||E.Instance->GetOwner()!=Actor.Get()||E.Instance->GetOuter()!=Actor.Get()||E.Instance->CreationMethod!=EComponentCreationMethod::SimpleConstructionScript)return false;
            if(!UWSceneCapture(E.Template.Get(),E.Source,true))return false;AuthoredNodes.Add(MoveTemp(E));
        }
        TMap<USCS_Node*,int32> Parents;
        for(const auto& E:AuthoredNodes)for(USCS_Node* Child:E.Children){if(!Unique.Contains(Child)||Child==E.Node.Get())return false;Parents.FindOrAdd(Child)++;}
        if(!Unique.Contains(AuthoredRoots[0])||Parents.Contains(AuthoredRoots[0]))return false;
        for(USCS_Node* N:AuthoredAll)if(N!=AuthoredRoots[0]&&Parents.FindRef(N)!=1)return false;
        TArray<USCS_Node*> Pending={AuthoredRoots[0]};TSet<USCS_Node*> Visited;
        while(!Pending.IsEmpty()){USCS_Node* N=Pending.Pop(EAllowShrinking::No);if(Visited.Contains(N))return false;Visited.Add(N);const auto* E=AuthoredNodes.FindByPredicate([&](const FUWSCS& Row){return Row.Node.Get()==N;});if(!E)return false;Pending.Append(E->Children);}
        if(Visited.Num()!=AuthoredNodes.Num())return false;
        const auto* Top=AuthoredNodes.FindByPredicate([&](const FUWSCS& E){return E.Node.Get()==AuthoredRoots[0];});if(!Top||Top->Instance!=Root.Get())return false;
        TInlineComponentArray<UActorComponent*> ComponentsNow(Actor.Get());if(ComponentsNow.Num()>128)return false;
        for(UActorComponent* C:ComponentsNow)
        {
            if(!Live(C)||!C->IsA<USceneComponent>())return UWFail(TEXT("bind_authored_instance"),ActorPath,TEXT("owned_component"),TEXT("native Scene-derived SCS or exact stock editor visualization"),GetPathNameSafe(C));
            auto* Scene=static_cast<USceneComponent*>(C);const bool Authored=AuthoredNodes.ContainsByPredicate([&](const FUWSCS& E){return E.Instance==Scene;});
            USceneComponent* Parent=nullptr;if(!UWParent(Scene,Parent))return false;
            if(!Authored&&(Scene->GetClass()!=UBillboardComponent::StaticClass()||!Scene->IsVisualizationComponent()||!Scene->HasAllFlags(RF_Transient|RF_TextExportTransient)||!Parent||!AuthoredNodes.ContainsByPredicate([&](const FUWSCS& E){return E.Instance==Parent;})||!Scene->GetAttachChildren().IsEmpty()))return false;
            FUWScene E;if(!UWSceneCapture(Scene,E,false))return false;AuthoredComponents.Add(MoveTemp(E));
        }
        for(const auto& E:AuthoredNodes)
        {
            if(!AuthoredComponents.ContainsByPredicate([&](const FUWScene& R){return R.Object.Get()==E.Instance;}))return false;
            for(USCS_Node* Child:E.Children){const auto* Row=AuthoredNodes.FindByPredicate([&](const FUWSCS& R){return R.Node.Get()==Child;});USceneComponent* Parent=nullptr;if(!Row||!UWParent(Row->Instance,Parent)||Parent!=E.Instance||Row->Instance->GetAttachSocketName()!=Row->Socket)return false;}
            if(E.Instance==Root.Get()){USceneComponent* Parent=nullptr;if(!UWParent(E.Instance,Parent)||Parent)return false;}
        }
        for(const auto& E:AuthoredComponents)for(USceneComponent* Child:E.Children){const auto* Row=AuthoredComponents.FindByPredicate([&](const FUWScene& R){return R.Object.Get()==Child;});if(!Row||Row->Parent!=E.Object.Get())return false;}
        AuthoredReady=true;Data->SetBoolField(TEXT("authored_scs_delete_scope"),true);Data->SetNumberField(TEXT("authored_scs_nodes"),AuthoredNodes.Num());Data->SetNumberField(TEXT("instance_components"),AuthoredComponents.Num());
        Data->SetStringField(TEXT("authored_value_scope"),TEXT("Direct reflected nontransient editable native fields; layout/child descriptors and external reference identities held. No exporter/accessor/native serializer; unreflected/cache/native callback/global effects not proved."));return true;
    }
    bool AuthoredSourceCurrent(const TCHAR* Stage)
    {
        if(!AuthoredReady)return UWFail(Stage,SourcePath,TEXT("authored admission"),TEXT("complete before callbacks"),TEXT("incomplete"));
        if(!Live(SCS.Get())||SCS->GetClass()!=SCSClass||SCS->GetOuter()!=SCSOuter||SCS->GetAllNodes()!=AuthoredAll||SCS->GetRootNodes()!=AuthoredRoots)return UWFail(Stage,SourcePath,TEXT("SCS inventory"),TEXT("captured"),TEXT("changed"));
        for(const auto& E:AuthoredNodes)
        {
            auto* N=E.Node.Get();if(!Live(N)||N->GetClass()!=USCS_Node::StaticClass()||N->GetOuter()!=SCS.Get()||N->VariableGuid!=E.Guid||N->GetVariableName()!=E.Name||N->AttachToName!=E.Socket||N->ParentComponentOrVariableName!=E.ParentName||N->ParentComponentOwnerClassName!=E.ParentOwner||N->bIsParentComponentNative!=E.NativeParent||!N->ComponentClass.IsResolved()||N->ComponentClass.Get()!=E.Class||!N->ComponentTemplate.IsResolved()||N->ComponentTemplate.Get()!=E.Template.Get()||N->ChildNodes.Num()!=E.Children.Num()||N->MetaDataArray.Num()!=E.Metadata.Num()||FTextInspector::GetSharedDataId(N->CategoryName)!=FTextInspector::GetSharedDataId(E.Category)||FTextInspector::GetFlags(N->CategoryName)!=FTextInspector::GetFlags(E.Category))return UWFail(Stage,SourcePath,TEXT("SCS node/GUID/template"),TEXT("captured"),E.Name.ToString());
            for(int32 I=0;I<E.Children.Num();++I)if(!N->ChildNodes[I].IsResolved()||N->ChildNodes[I].Get()!=E.Children[I])return UWFail(Stage,SourcePath,TEXT("SCS ordered children"),TEXT("captured"),E.Name.ToString());
            for(int32 I=0;I<E.Metadata.Num();++I)if(N->MetaDataArray[I].DataKey!=E.Metadata[I].Key||N->MetaDataArray[I].DataValue!=E.Metadata[I].Value)return UWFail(Stage,SourcePath,TEXT("SCS authored metadata"),TEXT("captured"),E.Name.ToString());
        }
        for(const auto& E:AuthoredTypes)if(!UWType(E.Type,false))return UWFail(Stage,SourcePath,TEXT("native descriptor inventory"),TEXT("captured"),TEXT("changed"));
        for(const auto& E:AuthoredFields)if(!UWField(E.Property,false))return UWFail(Stage,SourcePath,TEXT("native descriptor/layout/child"),TEXT("captured"),TEXT("changed"));
        for(const auto& E:AuthoredReferences){auto* O=E.Object.Get();if(!Live(O)||O->GetClass()!=E.Class||O->GetOuter()!=E.Outer||O->GetPackage()!=E.Package||!O->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive))return UWFail(Stage,E.Path,TEXT("external runtime reference"),TEXT("captured"),TEXT("changed"));}
        for(const auto& E:AuthoredNodes)if(!UWSceneCurrent(E.Source,true,Stage))return false;
        return true;
    }
    bool AuthoredInstanceCurrent(const TCHAR* Stage)
    {
        // Called only before accepted retirement. Afterwards InstanceCurrent
        // returns from its source/survivor branch without touching this closure.
        if(!RootSlot(Actor.Get(),Root.Get())||!Actor->GetActorTransform().Equals((AuthoredTransform&&Dispatched)?ExpectedTransform:BeforeTransform,AuthoredTransform?1.e-4:0.0))return UWFail(Stage,ActorPath,TEXT("root/world transform"),TEXT("captured"),TEXT("changed"));
        TInlineComponentArray<UActorComponent*> ComponentsNow(Actor.Get());if(ComponentsNow.Num()!=AuthoredComponents.Num())return UWFail(Stage,ActorPath,TEXT("component count"),LexToString(AuthoredComponents.Num()),LexToString(ComponentsNow.Num()));
        for(const auto& E:AuthoredComponents)if(!ComponentsNow.Contains(E.Object.Get()))return UWFail(Stage,ActorPath,TEXT("component membership"),TEXT("captured"),TEXT("changed"));
        for(const auto& E:AuthoredNodes){const auto* Slot=E.Generated->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Actor.Get());if(!Slot||!Slot->IsResolved()||Slot->Get()!=E.Instance)return UWFail(Stage,ActorPath,TEXT("compiled object binding"),E.Name.ToString(),TEXT("changed"));}
        for(const auto& E:AuthoredComponents)if(!UWSceneCurrent(E,false,Stage))return false;return true;
    }

    // UX shares UW source proof, but instance transforms and reconstruction are
    // planned transitions. Retired component addresses are never receivers.
    bool AuthoredTransform=false,UXReacquiring=false,UXPlanned=false;
    bool UXLogicalText=false,UXHoldText=false;TArray<FString> UXLogicalValues;
    TArray<int32> UXParents;TArray<FTransform> UXWorlds,UXRelatives;
    TArray<bool> UXAbsoluteLocation,UXAbsoluteRotation,UXAbsoluteScale;TArray<TStrongObjectPtr<USceneComponent>> UXRetired;
    TArray<UObject*> UXRetiredAddresses;
    EObjectFlags UXActorFlags=RF_NoFlags;
    TStrongObjectPtr<UTransBuffer> UXTransactor;const FTransaction* UXOwnTransaction=nullptr;
    int32 UXTransactionIndex=INDEX_NONE,UXTransactionDepth=0;FGuid UXTransactionId,UXOperationId;FString UXTransactionContext;bool UXTransactionReady=false;
    bool AuthoredHierarchy()const{return AuthoredDelete||AuthoredTransform;}
    bool UXText(const FText& Text,FString& Out)
    {
        // Only native empty or identified Base history is representable by this
        // callback-free source/id projection. Reject tables before GetTextId:
        // their history can resolve/load a table. Unidentified/dynamic history
        // remains original-valid pending before any writer is dispatched.
        const FText Copy=Text;
        const bool Empty=FTextInspector::GetSharedDataId(Copy)==FTextInspector::GetSharedDataId(FText::GetEmpty());
        if(Copy.IsFromStringTable())return false;
        const auto Id=FTextInspector::GetTextId(Copy);if(!Empty&&Id.IsEmpty())return false;
        const FString* Source=Empty?nullptr:FTextInspector::GetSourceString(Copy);
        const auto Namespace=FTextInspector::GetNamespace(Copy),Key=FTextInspector::GetKey(Copy);
        auto Token=[&](const FString& V){if(V.Len()>65536)return false;for(TCHAR C:V)if(C==0)return false;UWToken(Out,V);return true;};
        return Token(Empty?TEXT("native_empty"):TEXT("identified_base"))&&Token(LexToString(FTextInspector::GetFlags(Copy)))&&Token(Source?*Source:TEXT(""))&&Token(Namespace.IsSet()?TEXT("set:")+Namespace.GetValue():TEXT("unset"))&&Token(Key.IsSet()?TEXT("set:")+Key.GetValue():TEXT("unset"));
    }
    static bool UXScale(const FVector& S){return !S.ContainsNaN();}
    bool UXFlags(UObject* O,EObjectFlags Expected,const TCHAR* Stage)
    {
        const EObjectFlags Actual=O->GetFlags();
        if(Actual!=Expected)return UWFail(Stage,O->GetPathName(),TEXT("object flags"),LexToString(uint32(Expected)),LexToString(uint32(Actual)));
        return true;
    }
    bool UXComponentReady(USceneComponent* C)
    {
        if(!Live(C))return false;
        if(auto* Primitive=Cast<UPrimitiveComponent>(C))return !Primitive->BodyInstance.bSimulatePhysics&&Primitive->BodyInstance.WeldParent==nullptr;
        return true;
    }
    bool PlanAuthoredTransform(const FTransform& Desired)
    {
        if(!AuthoredTransform)return true;
        if(!Check(TEXT("before_authored_transform_plan")))return false;
        if(!UXScale(BeforeTransform.GetScale3D())||!UXScale(Desired.GetScale3D()))return UWFail(TEXT("plan_authored_transform"),ActorPath,TEXT("root scale"),TEXT("finite per-axis scale, including nonuniform, negative and zero axes"),Desired.GetScale3D().ToString());
        UXParents.Reset();UXWorlds.Reset();UXRelatives.Reset();UXAbsoluteLocation.Reset();UXAbsoluteRotation.Reset();UXAbsoluteScale.Reset();UXLogicalValues.Reset();UXActorFlags=Actor->GetFlags();
        for(const auto& E:AuthoredComponents)
        {
            auto* C=E.Object.Get();const bool Visual=!AuthoredNodes.ContainsByPredicate([&](const FUWSCS& N){return N.Instance==C;});
            if(!UXComponentReady(C)||!E.Socket.IsNone()||!UXScale(E.Relative.GetScale3D()))return UWFail(TEXT("plan_authored_transform"),E.Path,TEXT("transform role"),TEXT("no socket/simulation/weld; finite local scale3 and captured per-channel absolute flags"),TEXT("wider original-valid transform role pending"));
            const int32 Parent=E.Parent?AuthoredComponents.IndexOfByPredicate([&](const FUWScene& R){return R.Object.Get()==E.Parent;}):INDEX_NONE;
            if((C==Root.Get())!=(Parent==INDEX_NONE))return UWFail(TEXT("plan_authored_transform"),E.Path,TEXT("closed parent role"),TEXT("one own root"),TEXT("changed"));
            UXParents.Add(Parent);UXRelatives.Add(C==Root.Get()?Desired:E.Relative);UXWorlds.Add(FTransform::Identity);UXAbsoluteLocation.Add(C->IsUsingAbsoluteLocation());UXAbsoluteRotation.Add(C->IsUsingAbsoluteRotation());UXAbsoluteScale.Add(C->IsUsingAbsoluteScale());
            FString Logical;{TGuardValue<bool> Mode(UXLogicalText,true);if(!UWValues(C,false,Logical))return UWFail(TEXT("plan_authored_transform"),E.Path,TEXT("fresh value projection"),TEXT("native empty or identified Base FText; wider history remains valid pending"),TEXT("unrepresentable authored value"));}UXLogicalValues.Add(MoveTemp(Logical));
            if(Visual)
            {
                int32 Count=0;for(const auto& Other:AuthoredComponents)if(Other.Parent==E.Parent&&Other.Class==UBillboardComponent::StaticClass()&&!AuthoredNodes.ContainsByPredicate([&](const FUWSCS& N){return N.Instance==Other.Object.Get();}))++Count;
                if(Count!=1)return UWFail(TEXT("plan_authored_transform"),E.Path,TEXT("visualization role"),TEXT("one stock Billboard per authored parent"),LexToString(Count));
            }
        }
        TSet<int32> Done;
        for(int32 Pass=0;Pass<AuthoredComponents.Num();++Pass)
            for(int32 I=0;I<AuthoredComponents.Num();++I)if(!Done.Contains(I)&&(UXParents[I]==INDEX_NONE||Done.Contains(UXParents[I])))
            {
                // SceneComponent.cpp CalcNewComponentToWorld_GeneralCase uses this
                // same relative * parent operation, including FTransform's signed
                // scale matrix path. Zero axes do not require an inverse here.
                UXWorlds[I]=UXParents[I]==INDEX_NONE?Desired:UXRelatives[I]*UXWorlds[UXParents[I]];
                FTransform BeforeWorld=UXParents[I]==INDEX_NONE?BeforeTransform:AuthoredComponents[I].Relative*AuthoredComponents[UXParents[I]].World;
                // AAX: SceneComponent.cpp727 copies each absolute channel from
                // NewRelativeTransform after relative*parent (same UE56.cpp696).
                // These are authored modes, not a promise that every child
                // follows the actor. Preserve all modes/relative values and
                // project both before and after with the installed SDK order.
                if(UXParents[I]!=INDEX_NONE)
                {
                    if(UXAbsoluteLocation[I]){UXWorlds[I].CopyTranslation(UXRelatives[I]);BeforeWorld.CopyTranslation(AuthoredComponents[I].Relative);}
                    if(UXAbsoluteRotation[I]){UXWorlds[I].CopyRotation(UXRelatives[I]);BeforeWorld.CopyRotation(AuthoredComponents[I].Relative);}
                    if(UXAbsoluteScale[I]){UXWorlds[I].CopyScale3D(UXRelatives[I]);BeforeWorld.CopyScale3D(AuthoredComponents[I].Relative);}
                }
                if(BeforeWorld.ContainsNaN()||!BeforeWorld.Equals(AuthoredComponents[I].World,1.e-4))return UWFail(TEXT("plan_authored_transform"),AuthoredComponents[I].Path,TEXT("captured relative->world"),BeforeWorld.ToString(),AuthoredComponents[I].World.ToString());
                if(UXWorlds[I].ContainsNaN())return UWFail(TEXT("plan_authored_transform"),AuthoredComponents[I].Path,TEXT("planned relative->world"),TEXT("finite Engine FTransform projection"),UXWorlds[I].ToString());Done.Add(I);
            }
        if(Done.Num()!=AuthoredComponents.Num())return UWFail(TEXT("plan_authored_transform"),ActorPath,TEXT("parent closure"),TEXT("acyclic closed tree"),TEXT("incomplete"));
        UXPlanned=true;Data->SetBoolField(TEXT("authored_scs_transform_scope"),true);Data->SetBoolField(TEXT("authored_scs_delete_scope"),false);Data->SetNumberField(TEXT("transform_epsilon"),1.e-4);
        Data->SetStringField(TEXT("scope"),TEXT("Own bounded native Engine Scene SCS transform with finite general per-axis scale, including nonuniform, negative and zero axes; source/template/GUID unchanged; captured instance editable overrides and child relative values preserved. Root desired and child world use Engine FTransform relative*parent semantics, including signed-scale matrix handling and the installed per-channel absolute translation/rotation/scale projection for authored or stock visualization children. Each captured absolute flag and authored relative value is preserved; absolute channels need not follow actor movement. No arbitrary affine/shear exactness or attached-world inverse is claimed. Component roles retired before setter and reacquired by fresh compiled binding/parent. Original direct setter vs existing editor movement remain distinct. Absolute-mode combinations, editor reconstruction and persistence usage remain validation pending. Wider callbacks, physics/socket/custom/inherited behavior remain pending."));return Check(TEXT("authored_transform_plan_complete"));
    }
    bool UXTransactionCurrent()const
    {
        if(!UXTransactionReady)return true;
        if(!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Live(Editor.Get())||GEditor!=Editor.Get()||!Live(UXTransactor.Get())||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=UXTransactor.Get()||UXTransactor->GetClass()!=UTransBuffer::StaticClass()||UXTransactor->ActiveCount!=UXTransactionDepth||UXTransactor->UndoCount||UXTransactor->UndoBuffer.IsEmpty()||&UXTransactor->UndoBuffer.Last().Get()!=UXOwnTransaction)return false;
        const auto C=UXTransactor->UndoBuffer.Last()->GetContext();return C.TransactionId==UXTransactionId&&C.OperationId==UXOperationId&&C.Context==UXTransactionContext;
    }
    bool UXBeginTransaction()
    {
        const bool Can=Editor->CanTransact();Data->SetBoolField(TEXT("editor_can_transact"),Can);if(!Check(TEXT("after_authored_can_transact")))return false;
        if(!Can||GIsTransacting)return true;
        if(!Editor->Trans.IsResolved())return Fail(TEXT("authored_transaction_slot"),TEXT("actor_authored_transaction_unavailable"));
        auto* T=Editor->Trans.Get();if(!Live(T)||T->GetClass()!=UTransBuffer::StaticClass())return Fail(TEXT("authored_transaction_class"),TEXT("actor_authored_transaction_class_pending"));UXTransactor.Reset(static_cast<UTransBuffer*>(T));
        const int32 Before=UXTransactor->ActiveCount;if(Before<0||Before==MAX_int32||(Before&&(UXTransactor->UndoCount||UXTransactor->UndoBuffer.IsEmpty())))return Fail(TEXT("authored_transaction_depth"),TEXT("actor_authored_transaction_unavailable"));
        const FTransaction* Prior=Before?&UXTransactor->UndoBuffer.Last().Get():nullptr;FGuid PriorId,PriorOperation;
        if(Prior){const auto C=Prior->GetContext();PriorId=C.TransactionId;PriorOperation=C.OperationId;UXTransactionContext=C.Context;}else UXTransactionContext=TEXT("CubelessUX_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Count(TEXT("transaction_begin_calls"));UXTransactionIndex=Editor->BeginTransaction(*UXTransactionContext,FText::FromString(TEXT("Cubeless Blueprint Actor Transform")),nullptr);Data->SetNumberField(TEXT("transaction_begin_index"),UXTransactionIndex);
        if(!Context(TEXT("after_authored_transaction_begin"))||!Live(UXTransactor.Get())||UXTransactor->GetClass()!=UTransBuffer::StaticClass()||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=UXTransactor.Get()||UXTransactionIndex!=Before||UXTransactor->ActiveCount!=Before+1||UXTransactor->UndoCount||UXTransactor->UndoBuffer.IsEmpty())return Fail(TEXT("after_authored_transaction_begin"),TEXT("actor_authored_transaction_changed"));
        UXOwnTransaction=&UXTransactor->UndoBuffer.Last().Get();const auto C=UXOwnTransaction->GetContext();if(C.Context!=UXTransactionContext||(Prior&&(UXOwnTransaction!=Prior||C.TransactionId!=PriorId||C.OperationId!=PriorOperation)))return Fail(TEXT("after_authored_transaction_identity"),TEXT("actor_authored_transaction_changed"));
        UXTransactionId=C.TransactionId;UXOperationId=C.OperationId;UXTransactionDepth=Before+1;UXTransactionReady=true;return Check(TEXT("after_authored_transaction"));
    }
    bool UXEndTransaction()
    {
        if(UXTransactionIndex==INDEX_NONE)return !Stopped;
        if(!UXTransactionReady||!UXTransactionCurrent()){Data->SetBoolField(TEXT("transaction_end_skipped_unknown"),true);return Fail(TEXT("before_authored_transaction_end"),TEXT("actor_authored_transaction_unknown"));}
        const int32 Before=UXTransactionIndex;Count(TEXT("transaction_end_calls"));const int32 Returned=Editor->EndTransaction();Data->SetNumberField(TEXT("transaction_end_return"),Returned);UXTransactionIndex=INDEX_NONE;UXTransactionReady=false;
        // Only the sealed own transaction can be closed after another role was
        // lost. No actor/component/source read is made on that failure path.
        if(!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Live(Editor.Get())||GEditor!=Editor.Get()||!Live(UXTransactor.Get())||UXTransactor->GetClass()!=UTransBuffer::StaticClass()||!Editor->Trans.IsResolved()||Editor->Trans.Get()!=UXTransactor.Get()||Returned!=Before+1||UXTransactor->ActiveCount!=Before)return Fail(TEXT("after_authored_transaction_end"),TEXT("actor_authored_transaction_changed"));
        if(Before){if(UXTransactor->UndoBuffer.IsEmpty()||&UXTransactor->UndoBuffer.Last().Get()!=UXOwnTransaction)return Fail(TEXT("after_authored_transaction_parent"),TEXT("actor_authored_transaction_changed"));const auto C=UXTransactor->UndoBuffer.Last()->GetContext();if(C.TransactionId!=UXTransactionId||C.OperationId!=UXOperationId||C.Context!=UXTransactionContext)return Fail(TEXT("after_authored_transaction_parent"),TEXT("actor_authored_transaction_changed"));}
        return !Stopped&&Check(TEXT("after_authored_transaction_cleanup"));
    }
    bool ReacquireAuthoredTransform(const TCHAR* Stage,bool OriginalDirect)
    {
        if(!InstanceCurrent(Stage))return false; // actor/source/survivor only while UXReacquiring
        TInlineComponentArray<UActorComponent*> Inventory(Actor.Get());TArray<TStrongObjectPtr<USceneComponent>> Fresh;
        for(UActorComponent* C:Inventory){if(C)Holds.Emplace(C);}
        if(!InstanceCurrent(Stage))return false;
        if(Inventory.Num()!=AuthoredComponents.Num())return UWFail(Stage,ActorPath,TEXT("returned component count"),LexToString(AuthoredComponents.Num()),LexToString(Inventory.Num()));
        for(UActorComponent* C:Inventory)if(!Live(C)||!C->IsA<USceneComponent>()||C->GetOwner()!=Actor.Get()||C->GetOuter()!=Actor.Get())return Fail(Stage,TEXT("actor_authored_returned_inventory_role_changed"));
        Fresh.SetNum(AuthoredComponents.Num());TSet<USceneComponent*> Used;
        for(const auto& N:AuthoredNodes)
        {
            const auto* Slot=N.Generated->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Actor.Get());if(!Slot||!Slot->IsResolved())return Fail(Stage,TEXT("actor_authored_returned_member_unresolved"));UObject* Value=Slot->Get();
            if(!Inventory.Contains(Value)||!Live(Value)||Value->GetClass()!=N.Class)return Fail(Stage,TEXT("actor_authored_returned_member_role_changed"));
            auto* C=static_cast<USceneComponent*>(Value);const int32 I=AuthoredComponents.IndexOfByPredicate([&](const FUWScene& E){return E.Object.Get()==N.Instance;});if(I==INDEX_NONE||Used.Contains(C))return Fail(Stage,TEXT("actor_authored_returned_member_duplicate"));
            Fresh[I].Reset(C);Used.Add(C);
        }
        for(int32 I=0;I<AuthoredComponents.Num();++I)if(!Fresh[I].IsValid())
        {
            const int32 Parent=UXParents[I];if(Parent==INDEX_NONE||!Fresh[Parent].IsValid())return Fail(Stage,TEXT("actor_authored_returned_visualization_parent"));
            for(UActorComponent* C:Inventory)if(!Used.Contains(static_cast<USceneComponent*>(C)))
            {
                if(!Live(C)||C->GetClass()!=UBillboardComponent::StaticClass())continue;auto* S=static_cast<USceneComponent*>(C);USceneComponent* P=nullptr;if(!UWParent(S,P))return Fail(Stage,TEXT("actor_authored_returned_parent_unavailable"));
                if(P==Fresh[Parent].Get()){if(Fresh[I].IsValid()||!S->IsVisualizationComponent()||!S->HasAllFlags(RF_Transient|RF_TextExportTransient))return Fail(Stage,TEXT("actor_authored_returned_visualization_changed"));Fresh[I].Reset(S);}
            }
            if(!Fresh[I].IsValid())return Fail(Stage,TEXT("actor_authored_returned_visualization_missing"));Used.Add(Fresh[I].Get());
        }
        TArray<FUWScene> Next;TArray<TSharedPtr<FJsonValue>> Receipts;
        for(int32 I=0;I<AuthoredComponents.Num();++I)
        {
            const auto& E=AuthoredComponents[I];auto* C=Fresh[I].Get();const bool Same=C==E.Object.Get();const FUWSCS* Source=AuthoredNodes.FindByPredicate([&](const FUWSCS& N){return N.Instance==E.Object.Get();});
            if(OriginalDirect&&!Same)return UWFail(Stage,E.Path,TEXT("direct setter component"),TEXT("same physical component"),TEXT("changed"));
            if(!Live(C)||C->GetClass()!=E.Class||C->GetOuter()!=Actor.Get()||C->GetOwner()!=Actor.Get()||C->GetWorld()!=World.Get()||C->CreationMethod!=E.Creation||C->GetAttachSocketName()!=E.Socket||C->IsRegistered()!=E.Registered||!UXComponentReady(C))return UWFail(Stage,E.Path,TEXT("returned component role"),TEXT("captured logical role"),TEXT("changed"));
            const int32 ExpectedAbsolute=(UXAbsoluteLocation[I]?1:0)|(UXAbsoluteRotation[I]?2:0)|(UXAbsoluteScale[I]?4:0);
            const int32 ActualAbsolute=(C->IsUsingAbsoluteLocation()?1:0)|(C->IsUsingAbsoluteRotation()?2:0)|(C->IsUsingAbsoluteScale()?4:0);
            if(ActualAbsolute!=ExpectedAbsolute)return UWFail(Stage,E.Path,TEXT("returned absolute channels (location=1,rotation=2,scale=4)"),LexToString(ExpectedAbsolute),LexToString(ActualAbsolute));
            EObjectFlags ExpectedFlags=E.Flags;
            if(!Same)ExpectedFlags=Source?EObjectFlags(Source->Source.Flags&~(RF_ArchetypeObject|RF_Transactional|RF_WasLoaded|RF_Public|RF_InheritableComponentTemplate)):EObjectFlags((Fresh[UXParents[I]]->GetFlags()&RF_Transactional)|RF_Transient|RF_TextExportTransient);
            if(C->GetFlags()!=ExpectedFlags||(Source&&C->GetFName()!=E.Name))return UWFail(Stage,E.Path,TEXT("returned name/flags"),LexToString(uint32(ExpectedFlags)),LexToString(uint32(C->GetFlags())));
            USceneComponent* Parent=nullptr;if(!UWParent(C,Parent)||Parent!=(UXParents[I]==INDEX_NONE?nullptr:Fresh[UXParents[I]].Get()))return Fail(Stage,TEXT("actor_authored_returned_attachment_changed"));
            const auto& Children=C->GetAttachChildren();if(Children.Num()!=E.Children.Num())return Fail(Stage,TEXT("actor_authored_returned_children_changed"));
            for(int32 J=0;J<E.Children.Num();++J){const int32 Child=AuthoredComponents.IndexOfByPredicate([&](const FUWScene& R){return R.Object.Get()==E.Children[J];});if(Child==INDEX_NONE||!Children[J].IsResolved()||Children[J].Get()!=Fresh[Child].Get())return Fail(Stage,TEXT("actor_authored_returned_children_changed"));}
            if(!C->GetRelativeTransform().Equals(UXRelatives[I],1.e-4)||!C->GetComponentTransform().Equals(UXWorlds[I],1.e-4))return UWFail(Stage,E.Path,TEXT("returned relative/world"),UXWorlds[I].ToString(),C->GetComponentTransform().ToString());
            if(!Same)
            {
                FString Logical;TGuardValue<bool> Mode(UXLogicalText,true);if(!UWValues(C,false,Logical)||Logical!=UXLogicalValues[I])return UWFail(Stage,E.Path,TEXT("fresh logical editable overrides"),TEXT("captured non-transform values and FText source/id/flags"),TEXT("changed or unrepresentable"));
            }
            FString Values;{TGuardValue<bool> HoldText(UXHoldText,!Same);if(!UWValues(C,false,Values)||(Same&&Values!=E.Values))return UWFail(Stage,E.Path,TEXT("instance editable overrides"),TEXT("captured non-transform values"),TEXT("changed"));}
            FUWScene R;R.Object.Reset(C);R.Class=E.Class;R.Outer=Actor.Get();R.Name=C->GetFName();R.Socket=E.Socket;R.Flags=ExpectedFlags;R.Creation=E.Creation;R.Parent=Parent;for(const auto& H:Children)R.Children.Add(H.Get());R.Relative=UXRelatives[I];R.World=UXWorlds[I];R.Path=C->GetPathName();R.Values=MoveTemp(Values);R.Registered=E.Registered;Next.Add(MoveTemp(R));
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("before_path"),E.Path);J->SetStringField(TEXT("after_path"),C->GetPathName());J->SetBoolField(TEXT("physical_component_changed"),!Same);J->SetNumberField(TEXT("expected_absolute_channels"),ExpectedAbsolute);J->SetNumberField(TEXT("actual_absolute_channels"),ActualAbsolute);J->SetStringField(TEXT("absolute_channel_bits"),TEXT("location=1,rotation=2,scale=4"));J->SetStringField(TEXT("expected_world"),UXWorlds[I].ToString());J->SetStringField(TEXT("actual_world"),C->GetComponentTransform().ToString());Receipts.Add(MakeShared<FJsonValueObject>(J));
        }
        const int32 RootIndex=AuthoredComponents.IndexOfByPredicate([&](const FUWScene& E){return E.Object.Get()==Root.Get();});if(RootIndex==INDEX_NONE||!RootSlot(Actor.Get(),Fresh[RootIndex].Get()))return Fail(Stage,TEXT("actor_authored_returned_root_changed"));
        for(auto& N:AuthoredNodes){const int32 I=AuthoredComponents.IndexOfByPredicate([&](const FUWScene& E){return E.Object.Get()==N.Instance;});N.Instance=Fresh[I].Get();}
        Root.Reset(Fresh[RootIndex].Get());AuthoredComponents=MoveTemp(Next);UXReacquiring=false;Data->SetArrayField(TEXT("transform_component_reacquisition"),Receipts);return Check(Stage);
    }
    bool ExecuteAuthoredTransform(const FTransform& Desired,bool NoOp,bool OriginalDirect)
    {
        if(!UXPlanned||!Check(TEXT("before_authored_transform")))return false;if(NoOp&&!OriginalDirect){Data->SetBoolField(TEXT("no_op_writer_skipped"),true);return true;}
        bool Accepted=false;
        auto Apply=[&]() -> bool
        {
            if(!OriginalDirect)
            {
                if(!UXBeginTransaction())return false;
                Count(TEXT("world_modify_calls"));World->Modify(false);if(!Check(TEXT("after_authored_world_modify")))return false;
                Count(TEXT("level_modify_calls"));Level->Modify(false);if(!Check(TEXT("after_authored_level_modify")))return false;
                Count(TEXT("actor_modify_calls"));Actor->Modify(false);if(!Check(TEXT("after_authored_actor_modify")))return false;
                Count(TEXT("root_modify_calls"));Root->Modify(false);if(!Check(TEXT("after_authored_root_modify")))return false;
            }
            TStrongObjectPtr<UEditorActorSubsystem> Subsystem;
            if(!OriginalDirect){Subsystem.Reset(Editor->GetEditorSubsystem<UEditorActorSubsystem>());if(!Check(TEXT("after_authored_subsystem"))||!Live(Subsystem.Get()))return Fail(TEXT("after_authored_subsystem"),TEXT("actor_authored_subsystem_unavailable"));}
            ExpectedTransform=Desired;Dispatched=true;OwnEditorTransformDispatched=!OriginalDirect;UXReacquiring=true;
            for(const auto& E:AuthoredComponents){UXRetired.Emplace(E.Object.Get());UXRetiredAddresses.Add(E.Object.Get());}
            Count(TEXT("transform_calls"));Data->SetStringField(TEXT("transform_writer"),OriginalDirect?TEXT("AActor::SetActorTransform original direct"):TEXT("EditorActorSubsystem::SetActorTransform existing editor movement"));
            Accepted=OriginalDirect?Actor->SetActorTransform(Desired):Subsystem->SetActorTransform(Actor.Get(),Desired);Data->SetBoolField(TEXT("transform_returned"),true);Data->SetBoolField(TEXT("transform_accepted"),Accepted);
            if(!Accepted){Data->SetBoolField(TEXT("transform_false_readback_suppressed"),true);return Fail(TEXT("after_authored_transform_false"),TEXT("actor_blueprint_transform_outcome_unknown"));}
            if(!ReacquireAuthoredTransform(TEXT("after_authored_transform"),OriginalDirect))return false;
            if(!OriginalDirect){Count(TEXT("mark_level_dirty_calls"));Level->MarkPackageDirty();if(!Check(TEXT("after_authored_transform_dirty")))return false;}
            return true;
        };
        Apply();if(!OriginalDirect&&!UXEndTransaction())return Accepted;return Accepted;
    }

    bool VisualizationCurrent()const
    {
        TInlineComponentArray<UActorComponent*> Components(Actor.Get());
        if(Components.Num()!=1+(SpritePresent?1:0)||!Components.Contains(Root.Get())||Sprite.IsValid()!=SpritePresent)return false;
        const auto& Children=Root->GetAttachChildren();if(Children.Num()!=(SpritePresent?1:0))return false;
        if(!SpritePresent)return true;if(!RootVisualize)return false;
        auto* V=Sprite.Get();
        if(!Components.Contains(V)||!Children[0].IsResolved()||Children[0].Get()!=V||!Live(V)||V->GetClass()!=UBillboardComponent::StaticClass()||V->GetOwner()!=Actor.Get()||V->GetOuter()!=Actor.Get()||V->GetWorld()!=World.Get()||!V->IsVisualizationComponent()||!V->IsEditorOnly()||!V->HasAllFlags(RF_Transient|RF_TextExportTransient)||V->Mobility!=EComponentMobility::Movable||V->CreationMethod!=Root->CreationMethod||V->GetAttachParent()!=Root.Get()||!V->GetAttachChildren().IsEmpty()||!V->GetAttachSocketName().IsNone()||V->IsUsingAbsoluteLocation()||V->IsUsingAbsoluteRotation()||!V->IsUsingAbsoluteScale()||!V->GetRelativeTransform().Equals(SpriteRelative,0.0)||V->ComponentTags!=SpriteTags)return false;
        return true;
    }
    bool BindVisualization()
    {
        // Source OnRegister may create one stock editor-only sprite. This is not
        // a blanket exemption for user components or editor-only attachments.
        TInlineComponentArray<UActorComponent*> Components(Actor.Get());
        for(UActorComponent* C:Components){if(C)Holds.Emplace(C);}
        for(UActorComponent* C:Components)if(C!=Root.Get())
        {
            if(Sprite.IsValid()||!Live(C)||C->GetClass()!=UBillboardComponent::StaticClass())return Fail(TEXT("bind_visualization"),TEXT("actor_blueprint_component_inventory_changed"),ActorPath,TEXT("component_count"),TEXT("root plus at most one stock visualization Billboard"),LexToString(Components.Num()),true);
            Sprite.Reset(static_cast<UBillboardComponent*>(C));
        }
        SpritePresent=Sprite.IsValid();if(SpritePresent){SpriteRelative=Sprite->GetRelativeTransform();SpriteTags=Sprite->ComponentTags;}
        Data->SetBoolField(TEXT("editor_visualization_present"),SpritePresent);Data->SetBoolField(TEXT("private_sprite_slot_read"),false);Data->SetBoolField(TEXT("root_visualize_component"),RootVisualize);Data->SetBoolField(TEXT("source_template_visualize_component"),TemplateVisualize);
        return true;
    }
    bool RootRoleCurrent(const TCHAR* Stage,AActor* A)
    {
        auto Observation=MakeShared<FJsonObject>();auto Expected=MakeShared<FJsonObject>();auto Actual=MakeShared<FJsonObject>();FString First;
        auto B=[&](const TCHAR* K,bool E,bool V){Expected->SetBoolField(K,E);Actual->SetBoolField(K,V);if(E!=V&&First.IsEmpty())First=K;};
        auto N=[&](const TCHAR* K,int32 E,int32 V){Expected->SetNumberField(K,E);Actual->SetNumberField(K,V);if(E!=V&&First.IsEmpty())First=K;};
        auto S=[&](const TCHAR* K,const FString& E,const FString& V){Expected->SetStringField(K,E);Actual->SetStringField(K,V);if(!E.Equals(V,ESearchCase::CaseSensitive)&&First.IsEmpty())First=K;};
        B(TEXT("held_root_live"),true,Live(Root.Get()));B(TEXT("actor_root_slot_matches"),true,RootSlot(A,Root.Get()));
        // Do not inspect the root after a liveness or binding failure merely to
        // make diagnostics richer. All remaining operands below are observed.
        if(First.IsEmpty())
        {
            auto* R=Root.Get();S(TEXT("class_path"),USceneComponent::StaticClass()->GetPathName(),R->GetClass()->GetPathName());
            if(First.IsEmpty())
            {
            S(TEXT("outer_path"),ActorPath,GetPathNameSafe(R->GetOuter()));S(TEXT("owner_path"),ActorPath,GetPathNameSafe(R->GetOwner()));S(TEXT("world_path"),World->GetPathName(),GetPathNameSafe(R->GetWorld()));S(TEXT("name"),RootName,R->GetName());
            N(TEXT("creation_method"),int32(EComponentCreationMethod::SimpleConstructionScript),int32(R->CreationMethod));N(TEXT("mobility"),int32(Mobility),int32(R->Mobility));B(TEXT("visible"),Visible,R->IsVisible());B(TEXT("hidden_in_game"),Hidden,R->bHiddenInGame);B(TEXT("visualize_component"),RootVisualize,R->bVisualizeComponent);
            B(TEXT("component_tags_equal"),true,R->ComponentTags==RootTags);B(TEXT("parent_is_null"),true,R->GetAttachParent()==nullptr);N(TEXT("direct_child_count"),SpritePresent?1:0,R->GetAttachChildren().Num());B(TEXT("socket_none"),true,R->GetAttachSocketName().IsNone());B(TEXT("absolute_location"),false,R->IsUsingAbsoluteLocation());B(TEXT("absolute_rotation"),false,R->IsUsingAbsoluteRotation());B(TEXT("absolute_scale"),false,R->IsUsingAbsoluteScale());
            TInlineComponentArray<UActorComponent*> Components(A);N(TEXT("actor_component_count"),1+(SpritePresent?1:0),Components.Num());
            B(TEXT("sprite_present"),SpritePresent,Sprite.IsValid());
            if(SpritePresent)
            {
                auto* V=Sprite.Get();B(TEXT("sprite_live"),true,Live(V));
                if(Live(V))
                {
                    S(TEXT("sprite_class"),UBillboardComponent::StaticClass()->GetPathName(),V->GetClass()->GetPathName());
                    if(V->GetClass()==UBillboardComponent::StaticClass())
                    {
                    S(TEXT("sprite_owner"),ActorPath,GetPathNameSafe(V->GetOwner()));S(TEXT("sprite_outer"),ActorPath,GetPathNameSafe(V->GetOuter()));S(TEXT("sprite_world"),World->GetPathName(),GetPathNameSafe(V->GetWorld()));
                    B(TEXT("sprite_visualization"),true,V->IsVisualizationComponent());B(TEXT("sprite_editor_only"),true,V->IsEditorOnly());B(TEXT("sprite_transient_export_flags"),true,V->HasAllFlags(RF_Transient|RF_TextExportTransient));Actual->SetNumberField(TEXT("sprite_flags"),double(uint32(V->GetFlags())));N(TEXT("sprite_required_flag_bits"),int32(RF_Transient|RF_TextExportTransient),int32(V->GetFlags()&(RF_Transient|RF_TextExportTransient)));N(TEXT("sprite_mobility"),int32(EComponentMobility::Movable),int32(V->Mobility));N(TEXT("sprite_creation_method"),int32(R->CreationMethod),int32(V->CreationMethod));B(TEXT("sprite_attached_to_root"),true,V->GetAttachParent()==R);N(TEXT("sprite_child_count"),0,V->GetAttachChildren().Num());B(TEXT("sprite_socket_none"),true,V->GetAttachSocketName().IsNone());B(TEXT("sprite_absolute_location"),false,V->IsUsingAbsoluteLocation());B(TEXT("sprite_absolute_rotation"),false,V->IsUsingAbsoluteRotation());B(TEXT("sprite_absolute_scale"),true,V->IsUsingAbsoluteScale());S(TEXT("sprite_relative"),SpriteRelative.ToString(),V->GetRelativeTransform().ToString());B(TEXT("sprite_tags_equal"),true,V->ComponentTags==SpriteTags);
                    }
                }
            }
            B(TEXT("stock_visualization_role"),true,VisualizationCurrent());
            }
        }
        Observation->SetStringField(TEXT("stage"),Stage);Observation->SetObjectField(TEXT("expected"),Expected);Observation->SetObjectField(TEXT("actual"),Actual);Observation->SetStringField(TEXT("first_mismatch"),First);Data->SetObjectField(TEXT("root_role_observation"),Observation);
        if(!First.IsEmpty())return Fail(Stage,TEXT("actor_blueprint_root_role_changed"),ActorPath+TEXT(".")+RootName,First,ToJson(Expected),ToJson(Actual),true);
        return true;
    }
    UObject* SCSOuter=nullptr;UClass* SCSClass=nullptr;UClass* NodeClass=nullptr;UObject* NodeOuter=nullptr;
    FStructProperty* GeneratedFrame=nullptr;FStructProperty* SkeletonFrame=nullptr;
    AActor* TemplateOwner=nullptr;UWorld* TemplateWorld=nullptr;
    bool RootSlot(const AActor* A,const USceneComponent* Expected)const
    {
        if(!NativeRootProperty||NativeRootProperty->GetOwnerStruct()!=AActor::StaticClass()||NativeRootProperty->GetFName()!=FName(TEXT("RootComponent"))||NativeRootProperty->GetOwnerProperty()!=NativeRootProperty||NativeRootProperty->ArrayDim!=1||NativeRootProperty->GetSize()!=sizeof(TObjectPtr<UObject>)||NativeRootProperty->HasSetterOrGetter()||!NativeRootProperty->PropertyClass.IsResolved()||NativeRootProperty->PropertyClass.Get()!=USceneComponent::StaticClass())return false;
        const auto* Slot=NativeRootProperty->ContainerPtrToValuePtr<TObjectPtr<UObject>>(A);return Slot&&Slot->IsResolved()&&Slot->Get()==Expected;
    }
    static bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Fail(const TCHAR* Stage,const TCHAR* Code,const FString& Object=FString(),const FString& Property=FString(),const FString& Expected=FString(),const FString& Actual=FString(),bool Observed=false)
    {
        if(!Stopped){FailureCode=Code;FailureStage=Stage;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("code"),Code);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetBoolField(TEXT("individual_operands_observed"),Observed);if(Observed){J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);}Data->SetObjectField(TEXT("first_current_failure"),J);}Stopped=true;Data->SetBoolField(TEXT("scope_safe"),false);Data->SetBoolField(TEXT("followup_sdk_stopped"),true);return false;
    }
    void Count(const TCHAR* K){double N=0;Data->TryGetNumberField(K,N);Data->SetNumberField(K,N+1);}
    bool Context(const TCHAR* Stage)
    {
        Data->SetObjectField(TEXT("newly_created_map_admission"),NewMapWitness.Receipt);
        if(AuthoredHierarchy())
        {
            if(Stopped)return false;
            if(!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Live(Editor.Get())||GEditor!=Editor.Get()||!Live(World.Get())||!Live(Level.Get()))return Fail(Stage,TEXT("actor_authored_delete_phase_or_roles_changed"));
            if(!Editor->PlayWorld.IsResolved()||Editor->PlayWorld.Get()!=nullptr||Editor->GetPIEWorldContext()||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||Editor->GetEditorWorldContext().World()!=World.Get()||!World->PersistentLevel.IsResolved()||World->PersistentLevel.Get()!=Level.Get()||World->GetCurrentLevel()!=Level.Get()||Level->OwningWorld!=World.Get()||World->GetLevels().Num()!=1||World->IsPartitionedWorld()||Level->IsUsingExternalActors()||FLevelUtils::IsLevelLocked(Level.Get())||!NewMapWitness.Current(World.Get(),Level.Get()))return Fail(Stage,TEXT("actor_authored_delete_context_changed"));
            if(AuthoredTransform&&!UXTransactionCurrent())return Fail(Stage,TEXT("actor_authored_transaction_changed"));
            return true;
        }
        if(Stopped)return false;if(!IsInGameThread()||!Live(Editor.Get())||GEditor!=Editor.Get()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Live(World.Get())||!Live(Level.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get()||!World->PersistentLevel.IsResolved()||World->PersistentLevel.Get()!=Level.Get()||World->GetCurrentLevel()!=Level.Get()||Level->OwningWorld!=World.Get()||World->GetLevels().Num()!=1||World->IsPartitionedWorld()||Level->IsUsingExternalActors()||FLevelUtils::IsLevelLocked(Level.Get())||!NewMapWitness.Current(World.Get(),Level.Get()))return Fail(Stage,TEXT("actor_blueprint_context_changed"));return true;
    }
    bool FieldInventory(UClass* C,TArray<FField>& Out)
    {
        for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){FProperty* P=*It;if(Out.Num()>=64||P->GetOwnerStruct()!=C||P->GetOwnerProperty()!=P||P->HasSetterOrGetter())return false;UObject* Binding=nullptr;if(auto* O=CastField<FObjectProperty>(P)){if(!O->PropertyClass.IsResolved())return false;Binding=O->PropertyClass.Get();}else if(auto* S=CastField<FStructProperty>(P)){if(!S->Struct.IsResolved())return false;Binding=S->Struct.Get();}if(Binding)Holds.Emplace(Binding);Out.Add({P,C,P->GetClass(),P->GetFName(),uint64(P->GetPropertyFlags()),P->ArrayDim,P->GetOffset_ForInternal(),P->GetSize(),Binding});}return true;
    }
    bool FieldsCurrent(UClass* C,const TArray<FField>& Expected)
    {
        TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){if(Actual.Num()>=64)return false;Actual.Add(*It);}if(Actual.Num()!=Expected.Num())return false;
        for(int32 I=0;I<Expected.Num();++I){const auto& E=Expected[I];if(Actual[I]!=E.P)return false;auto* P=E.P;if(P->GetOwnerStruct()!=E.Owner||P->GetOwnerProperty()!=P||P->GetClass()!=E.Class||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->ArrayDim!=E.Dim||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size||P->HasSetterOrGetter())return false;if(auto* O=CastField<FObjectProperty>(P)){if(!O->PropertyClass.IsResolved()||O->PropertyClass.Get()!=E.Binding)return false;}else if(auto* S=CastField<FStructProperty>(P)){if(!S->Struct.IsResolved()||S->Struct.Get()!=E.Binding)return false;}}return true;
    }
    static bool OwnPinSame(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    static bool OwnDeclarationSame(const FBPVariableDescription& A,const FBPVariableDescription& B)
    {
        if(A.VarName!=B.VarName||A.VarGuid!=B.VarGuid||!OwnPinSame(A.VarType,B.VarType)||!A.FriendlyName.Equals(B.FriendlyName,ESearchCase::CaseSensitive)||FTextInspector::GetFlags(A.Category)!=FTextInspector::GetFlags(B.Category)||FTextInspector::GetSharedDataId(A.Category)!=FTextInspector::GetSharedDataId(B.Category)||A.PropertyFlags!=B.PropertyFlags||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||!A.DefaultValue.Equals(B.DefaultValue,ESearchCase::CaseSensitive)||A.MetaDataArray.Num()!=B.MetaDataArray.Num())return false;
        for(int32 I=0;I<A.MetaDataArray.Num();++I)if(A.MetaDataArray[I].DataKey!=B.MetaDataArray[I].DataKey||!A.MetaDataArray[I].DataValue.Equals(B.MetaDataArray[I].DataValue,ESearchCase::CaseSensitive))return false;
        return true;
    }
    static bool OwnPrimitive(const FEdGraphPinType& T,FProperty* P)
    {
        // Match each actual compiled field, retaining independent captured owners.
        if(!P||P->ArrayDim!=1||P->HasSetterOrGetter()||T.ContainerType!=EPinContainerType::None||T.bIsReference||T.bIsConst||T.bIsWeakPointer||T.bIsUObjectWrapper||!T.PinSubCategoryObject.IsExplicitlyNull()||!T.PinValueType.TerminalSubCategoryObject.IsExplicitlyNull()||!T.PinValueType.TerminalCategory.IsNone()||!T.PinValueType.TerminalSubCategory.IsNone()||T.PinValueType.bTerminalIsConst||T.PinValueType.bTerminalIsWeakPointer||T.PinValueType.bTerminalIsUObjectWrapper)return false;
        const auto K=P->GetClass();const FName C=T.PinCategory,S=T.PinSubCategory;
        if(C==UEdGraphSchema_K2::PC_Real)return (S==UEdGraphSchema_K2::PC_Float&&K==FFloatProperty::StaticClass())||(S==UEdGraphSchema_K2::PC_Double&&K==FDoubleProperty::StaticClass());
        if(!S.IsNone())return false;
        return (C==UEdGraphSchema_K2::PC_Boolean&&K==FBoolProperty::StaticClass())||(C==UEdGraphSchema_K2::PC_Int&&K==FIntProperty::StaticClass())||(C==UEdGraphSchema_K2::PC_Int64&&K==FInt64Property::StaticClass())||(C==UEdGraphSchema_K2::PC_Name&&K==FNameProperty::StaticClass())||(C==UEdGraphSchema_K2::PC_String&&K==FStrProperty::StaticClass())||(C==UEdGraphSchema_K2::PC_Text&&K==FTextProperty::StaticClass());
    }
    static bool OwnBool(FProperty* P,uint8& Size,uint8& Offset,uint8& ByteMask,uint8& FieldMask,bool Capture)
    {
        if(P->GetClass()!=FBoolProperty::StaticClass())return true;const auto* B=static_cast<FBoolProperty*>(P);
        if(Capture){Size=B->GetBoolFieldSize();Offset=B->GetByteOffset();ByteMask=B->GetByteMask();FieldMask=B->GetFieldMask();return true;}
        return Size==B->GetBoolFieldSize()&&Offset==B->GetByteOffset()&&ByteMask==B->GetByteMask()&&FieldMask==B->GetFieldMask();
    }
    static bool OwnValue(FProperty* P,const UObject* Container,FString& Out,FText* HoldText=nullptr)
    {
        const void* Address=P->ContainerPtrToValuePtr<void>(Container);if(!Address)return false;const auto K=P->GetClass();
        if(K==FBoolProperty::StaticClass())Out=static_cast<FBoolProperty*>(P)->GetPropertyValue(Address)?TEXT("true"):TEXT("false");
        else if(K==FIntProperty::StaticClass()||K==FInt64Property::StaticClass())Out=LexToString(static_cast<FNumericProperty*>(P)->GetSignedIntPropertyValue(Address));
        else if(K==FFloatProperty::StaticClass())Out=BytesToHex(static_cast<const uint8*>(Address),sizeof(float));
        else if(K==FDoubleProperty::StaticClass())Out=BytesToHex(static_cast<const uint8*>(Address),sizeof(double));
        else if(K==FNameProperty::StaticClass())Out=static_cast<const FName*>(Address)->ToString();
        else if(K==FStrProperty::StaticClass()){const auto& V=*static_cast<const FString*>(Address);if(V.Len()>65536)return false;Out=V;}
        else if(K==FTextProperty::StaticClass()){const auto& V=*static_cast<const FText*>(Address);if(HoldText)*HoldText=V;Out=FString::Printf(TEXT("%p:%u"),FTextInspector::GetSharedDataId(V),FTextInspector::GetFlags(V));}
        else return false;return true;
    }
    bool CaptureOwnScalars()
    {
        const auto* B=Blueprint.Get();if(B->NewVariables.Num()>32)return false;TSet<FName> Names;TSet<FGuid> Guids;
        for(const auto& D:B->NewVariables)
        {
            if(D.VarName.IsNone()||!D.VarGuid.IsValid()||Names.Contains(D.VarName)||Guids.Contains(D.VarGuid)||D.MetaDataArray.Num()>256||D.DefaultValue.Len()>65536||D.FriendlyName.Len()>65536)return false;
            Names.Add(D.VarName);Guids.Add(D.VarGuid);
            const auto* G=Fields.FindByPredicate([&](const FField& E){return E.Name==D.VarName;});const auto* S=SkeletonFields.FindByPredicate([&](const FField& E){return E.Name==D.VarName;});
            if(!G||!S||!OwnPrimitive(D.VarType,G->P)||!OwnPrimitive(D.VarType,S->P)||G->Offset<0||G->Size<=0||G->Offset>Class->GetPropertiesSize()-G->Size||S->Offset<0||S->Size<=0||S->Offset>Skeleton->GetPropertiesSize()-S->Size)return false;
            FOwnScalar E;E.Declaration=D;E.Generated=G->P;E.SkeletonProperty=S->P;
            if(!OwnBool(E.Generated,E.GeneratedBoolSize,E.GeneratedBoolOffset,E.GeneratedBoolByteMask,E.GeneratedBoolFieldMask,true)||!OwnBool(E.SkeletonProperty,E.SkeletonBoolSize,E.SkeletonBoolOffset,E.SkeletonBoolByteMask,E.SkeletonBoolFieldMask,true)||!OwnValue(E.Generated,CDO.Get(),E.CDOValue,&E.CDOText)||!OwnValue(E.Generated,Actor.Get(),E.InstanceValue,&E.InstanceText))return false;
            // ActorConstruction.cpp98..112: writable BP / not instance-editable
            // scalar reset. This lease already fixes the world to Editor, not PIE.
            const uint64 F=G->Flags;E.ResetByConstruction=(F&CPF_BlueprintVisible)!=0&&(F&CPF_BlueprintReadOnly)==0&&((F&CPF_DisableEditOnInstance)!=0||(F&CPF_Edit)==0);
            OwnScalars.Add(MoveTemp(E));
        }
        OwnScalarsReady=true;Data->SetNumberField(TEXT("own_primitive_variable_count"),OwnScalars.Num());Data->SetBoolField(TEXT("own_cdo_instance_values_observed_separately"),true);
        Data->SetStringField(TEXT("own_primitive_variable_scope"),TEXT("Own bool/int32/int64/float/double/Name/String/Text scalar declarations <=32, independent generated/skeleton descriptors and separate CDO/instance backing values. Editor movement alone may observe the exact ActorConstruction reset-to-CDO flag rule; direct setter/delete do not admit this reset. Arrays/struct/object/enum/inherited variables and nonempty lifecycle bytecode remain incomplete."));return true;
    }
    bool OwnSourceCurrent(const TCHAR* Stage)
    {
        if(!OwnScalarsReady||Blueprint->NewVariables.Num()!=OwnScalars.Num())return Fail(Stage,TEXT("actor_blueprint_own_variable_inventory_changed"));
        for(int32 I=0;I<OwnScalars.Num();++I)
        {
            auto& E=OwnScalars[I];
            // Complete root inventories/descriptors were compared by SourceCurrent
            // before these borrowed property addresses or value cells are read.
            if(!OwnDeclarationSame(Blueprint->NewVariables[I],E.Declaration)||!OwnBool(E.Generated,E.GeneratedBoolSize,E.GeneratedBoolOffset,E.GeneratedBoolByteMask,E.GeneratedBoolFieldMask,false)||!OwnBool(E.SkeletonProperty,E.SkeletonBoolSize,E.SkeletonBoolOffset,E.SkeletonBoolByteMask,E.SkeletonBoolFieldMask,false))return Fail(Stage,TEXT("actor_blueprint_own_variable_declaration_changed"),SourcePath,E.Declaration.VarName.ToString());
            FString Actual;if(!OwnValue(E.Generated,CDO.Get(),Actual)||!Actual.Equals(E.CDOValue,ESearchCase::CaseSensitive))return Fail(Stage,TEXT("actor_blueprint_own_variable_cdo_changed"),CDOPath,E.Declaration.VarName.ToString(),E.CDOValue,Actual,true);
        }return true;
    }
    bool OwnInstanceCurrent(const TCHAR* Stage)
    {
        // Accepted-delete retirement returns before this live actor reader.
        TArray<TSharedPtr<FJsonValue>> Resets;
        for(const auto& E:OwnScalars)
        {
            FString Actual;if(!OwnValue(E.Generated,Actor.Get(),Actual))return Fail(Stage,TEXT("actor_blueprint_own_variable_instance_unobservable"),ActorPath,E.Declaration.VarName.ToString());
            if(Actual.Equals(E.InstanceValue,ESearchCase::CaseSensitive))continue;
            if(!OwnEditorTransformDispatched||!E.ResetByConstruction||!Actual.Equals(E.CDOValue,ESearchCase::CaseSensitive))return Fail(Stage,TEXT("actor_blueprint_own_variable_instance_changed"),ActorPath,E.Declaration.VarName.ToString(),E.InstanceValue,Actual,true);
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("variable"),E.Declaration.VarName.ToString());J->SetStringField(TEXT("before"),E.InstanceValue);J->SetStringField(TEXT("actual"),Actual);J->SetStringField(TEXT("transition"),TEXT("editor_construction_writable_non_instance_editable_to_preserved_cdo"));Resets.Add(MakeShared<FJsonValueObject>(J));
        }Data->SetArrayField(TEXT("own_primitive_observed_construction_resets"),Resets);return true;
    }
    bool SourceCurrent(const TCHAR* Stage)
    {
        if(!Context(Stage))return false;auto* B=Blueprint.Get();
        if(!Live(B)||B->GetClass()!=UBlueprint::StaticClass()||B->GetPackage()!=Package.Get()||B->GetOuter()!=Package.Get()||Package->IsDirty()||B->Status!=BS_UpToDate||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->BlueprintType!=BPTYPE_Normal||B->NewVariables.Num()>32||!B->ImplementedInterfaces.IsEmpty()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=AActor::StaticClass()||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Class.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=Skeleton.Get()||!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript.Get()!=SCS.Get()||!B->GetPathName().Equals(SourcePath,ESearchCase::CaseSensitive))return Fail(Stage,TEXT("actor_blueprint_source_changed"));
        for(UClass* C:{Class.Get(),Skeleton.Get()})if(!Live(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||C->GetOuter()!=Package.Get()||C->GetSuperClass()!=AActor::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=B)return Fail(Stage,TEXT("actor_blueprint_source_class_changed"));
        const auto* GeneratedBP=static_cast<UBlueprintGeneratedClass*>(Class.Get());const auto* SkeletonBP=static_cast<UBlueprintGeneratedClass*>(Skeleton.Get());
        if(GeneratedBP->UberGraphFramePointerProperty!=GeneratedFrame||SkeletonBP->UberGraphFramePointerProperty!=SkeletonFrame||!GeneratedBP->SimpleConstructionScript.IsResolved()||GeneratedBP->SimpleConstructionScript.Get()!=SCS.Get())return Fail(Stage,TEXT("actor_blueprint_compiler_source_binding_changed"));
        if(!GeneratedBP->DynamicBindingObjects.IsEmpty()||!GeneratedBP->ComponentTemplates.IsEmpty()||!GeneratedBP->Timelines.IsEmpty()||!B->ComponentTemplates.IsEmpty()||!B->Timelines.IsEmpty())return Fail(Stage,TEXT("actor_blueprint_source_callbacks_changed"));
        if(Class->GetFlags()!=ClassFlags||Skeleton->GetFlags()!=SkeletonFlags||Class->GetClassFlags()!=ClassTypeFlags||Skeleton->GetClassFlags()!=SkeletonTypeFlags||!Class->GetPathName().Equals(ClassPath,ESearchCase::CaseSensitive)||!Skeleton->GetPathName().Equals(SkeletonPath,ESearchCase::CaseSensitive)||!Class->ClassDefaultObject.IsResolved()||Class->ClassDefaultObject.Get()!=CDO.Get()||!Live(CDO.Get())||CDO->GetClass()!=Class.Get()||CDO->GetPackage()!=Package.Get()||CDO->GetOuter()!=CDOOuter||CDO->GetFlags()!=CDOFlags||!CDO->GetPathName().Equals(CDOPath,ESearchCase::CaseSensitive)||!RootSlot(CDO.Get(),CDORoot.Get())||(CDORoot.IsValid()&&!Live(CDORoot.Get()))||!CDO->GetActorTransform().Equals(CDOTransform,0.0)||CDO->Tags!=CDOTags||!FieldsCurrent(Class.Get(),Fields)||!FieldsCurrent(Skeleton.Get(),SkeletonFields))return Fail(Stage,TEXT("actor_blueprint_source_signature_changed"));
        if(!OwnSourceCurrent(Stage))return false;
        if(AuthoredHierarchy()){if(!AuthoredSourceCurrent(Stage))return false;}
        else
        {
        if(!Live(SCS.Get())||SCS->GetClass()!=SCSClass||SCS->GetOuter()!=SCSOuter||SCS->GetAllNodes().Num()!=1||SCS->GetAllNodes()[0]!=SCSNode.Get()||SCS->GetRootNodes().Num()!=1||SCS->GetRootNodes()[0]!=SCSNode.Get()||!Live(SCSNode.Get())||SCSNode->GetClass()!=NodeClass||SCSNode->GetOuter()!=NodeOuter||SCSNode->VariableGuid!=VariableGuid||SCSNode->GetVariableName()!=VariableName||!SCSNode->ComponentClass.IsResolved()||SCSNode->ComponentClass.Get()!=USceneComponent::StaticClass()||!SCSNode->ComponentTemplate.IsResolved()||SCSNode->ComponentTemplate.Get()!=Template.Get()||!SCSNode->GetChildNodes().IsEmpty())return Fail(Stage,TEXT("actor_blueprint_scs_changed"));
        if(!Live(Template.Get())||Template->GetClass()!=USceneComponent::StaticClass()||Template->GetOuter()!=TemplateOuter||Template->GetOwner()!=TemplateOwner||Template->GetWorld()!=TemplateWorld||Template->GetPackage()!=Package.Get()||Template->GetFlags()!=TemplateFlags||!Template->GetPathName().Equals(TemplatePath,ESearchCase::CaseSensitive)||!Template->GetRelativeTransform().Equals(TemplateTransform,0.0)||Template->ComponentTags!=TemplateTags||Template->Mobility!=TemplateMobility||Template->IsVisible()!=TemplateVisible||Template->bHiddenInGame!=TemplateHidden||Template->bVisualizeComponent!=TemplateVisualize||Template->GetAttachParent()||!Template->GetAttachChildren().IsEmpty()||Template->IsRegistered())return Fail(Stage,TEXT("actor_blueprint_template_changed"));
        }
        for(const auto* Category:{&B->FunctionGraphs,&B->MacroGraphs,&B->UbergraphPages,&B->DelegateSignatureGraphs})for(const auto& H:*Category)if(!H.IsResolved())return Fail(Stage,TEXT("actor_blueprint_source_graph_slot_unresolved"));
        if(B->FunctionGraphs!=Functions||B->MacroGraphs!=Macros||B->UbergraphPages!=Events||B->DelegateSignatureGraphs!=Delegates)return Fail(Stage,TEXT("actor_blueprint_source_graphs_changed"));
        for(const auto& E:Graphs){auto* G=E.Graph.Get();if(!Live(G)||G->GetClass()!=UEdGraph::StaticClass()||G->GetOuter()!=E.Outer||G->GraphGuid!=E.Guid||!G->SubGraphs.IsEmpty()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr().Get()!=E.Schema||G->Nodes.Num()!=E.Nodes.Num())return Fail(Stage,TEXT("actor_blueprint_source_graph_changed"));for(int32 I=0;I<E.Nodes.Num();++I)if(!G->Nodes[I].IsResolved()||G->Nodes[I].Get()!=E.Nodes[I])return Fail(Stage,TEXT("actor_blueprint_source_nodes_changed"));}
        for(const auto& E:Nodes){auto* N=E.Node.Get();if(!Live(N)||N->GetClass()!=E.Class||N->GetOuter()!=E.Graph||N->NodeGuid!=E.Guid||N->NodePosX!=E.X||N->NodePosY!=E.Y||!N->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||!N->NodeComment.Equals(E.Comment,ESearchCase::CaseSensitive)||N->Pins.Num()!=E.Pins.Num())return Fail(Stage,TEXT("actor_blueprint_source_node_changed"));for(int32 I=0;I<E.Pins.Num();++I){const auto& P=E.Pins[I];const auto* A=N->Pins[I];if(A!=P.P||!A||A->WasTrashed()||A->GetOwningNodeUnchecked()!=N||A->PinId!=P.Guid||A->PinName!=P.Name||A->Direction!=P.Direction||A->PinType!=P.Type||A->PinType.bIsUObjectWrapper!=P.Type.bIsUObjectWrapper||A->PinType.bSerializeAsSinglePrecisionFloat!=P.Type.bSerializeAsSinglePrecisionFloat||A->PinType.PinValueType.bTerminalIsUObjectWrapper!=P.Type.PinValueType.bTerminalIsUObjectWrapper||!A->DefaultValue.Equals(P.Raw,ESearchCase::CaseSensitive)||!A->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive)||!A->DefaultObject.IsResolved()||A->DefaultObject.Get()!=P.Default||A->LinkedTo!=P.Links||FTextInspector::GetFlags(A->DefaultTextValue)!=P.Flags||FTextInspector::GetSharedDataId(A->DefaultTextValue)!=FTextInspector::GetSharedDataId(P.Text))return Fail(Stage,TEXT("actor_blueprint_source_pin_changed"));}}
        return true;
    }
    bool InstanceCurrent(const TCHAR* Stage)
    {
        if(!SourceCurrent(Stage))return false;
        // Level slots are inspected by address first. Accepted deletion permits
        // only selected absence; no field of the retired selected actor/root is read.
        int32 TargetCount=0;TSet<AActor*> Present;for(const auto& H:Level->Actors){if(!H.IsResolved())return Fail(Stage,TEXT("actor_blueprint_level_slot_unresolved"));AActor* A=H.Get();if(A){if(Present.Contains(A))return Fail(Stage,TEXT("actor_blueprint_level_duplicate"));Present.Add(A);if(A==Actor.Get())++TargetCount;}}
        if(TargetCount!=(Retired?0:1)||Present.Num()!=Others.Num()+(Retired?0:1))return Fail(Stage,TEXT("actor_blueprint_level_membership_changed"));
        for(const auto& E:Others){AActor* A=E.Actor.Get();if(!Present.Contains(A)||!Live(A)||A->GetClass()!=E.Class||A->GetOuter()!=E.Outer||A->GetLevel()!=Level.Get()||!A->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||!RootSlot(A,E.Root.Get())||(E.Root.IsValid()&&!Live(E.Root.Get()))||!A->GetActorTransform().Equals(E.Transform,0.0)||A->Tags!=E.Tags||!A->GetActorLabel(false).Equals(E.Label,ESearchCase::CaseSensitive)||A->IsSelectedInEditor()!=E.Selected)return Fail(Stage,TEXT("actor_blueprint_unselected_changed"),E.Path,TEXT("captured basic actor state"));}
        if(Retired)return true;AActor* A=Actor.Get();if(!Live(A)||A->GetClass()!=Class.Get()||A->GetOuter()!=Level.Get()||A->GetLevel()!=Level.Get()||!A->GetPathName().Equals(ActorPath,ESearchCase::CaseSensitive)||A->Tags!=ActorTags||A->GetOwner()||A->IsTemplate()||A->IsChildActor()||A->IsPackageExternal()||A->HasAnyFlags(RF_Transient)||A->bIsEditorOnlyActor||!A->IsEditable()||A->IsLockLocation())return Fail(Stage,TEXT("actor_blueprint_instance_changed"));
        if(!OwnInstanceCurrent(Stage))return false;
        if(AuthoredTransform)
        {
            if(!UXFlags(A,UXActorFlags,Stage))return UWFail(Stage,ActorPath,TEXT("actor flags"),LexToString(uint32(UXActorFlags)),LexToString(uint32(A->GetFlags())));
            if(UXReacquiring)return true;
        }
        if(AuthoredHierarchy())return AuthoredInstanceCurrent(Stage);
        if(Reacquiring)return true;
        if(!RootRoleCurrent(Stage,A))return false;
        const auto* BoundSlot=ComponentProperty->ContainerPtrToValuePtr<TObjectPtr<UObject>>(A);if(!BoundSlot||!BoundSlot->IsResolved()||BoundSlot->Get()!=Root.Get())return Fail(Stage,TEXT("actor_blueprint_instance_property_binding_changed"));
        // RootRoleCurrent verifies the exact root plus optional Engine-created editor sprite inventory.
        const FTransform Actual=A->GetActorTransform();const FTransform Relative=Root->GetRelativeTransform();const FTransform& Expected=Dispatched?ExpectedTransform:BeforeTransform;if(!Actual.Equals(Expected,Dispatched?0.01:0.0)||!Relative.Equals(Expected,Dispatched?0.01:0.0))return Fail(Stage,TEXT("actor_blueprint_transform_changed"),ActorPath,TEXT("unattached root world/relative"),Expected.ToString(),Actual.ToString(),true);
        return true;
    }
    bool Disk(const TCHAR* Stage,FString& Hash)
    {
        if(!InstanceCurrent(Stage))return false;const int64 Size=IFileManager::Get().FileSize(*File);
        if(Size<=0||Size>64*1024*1024)return Fail(Stage,TEXT("actor_blueprint_source_file_unavailable"));
        TArray<uint8> Bytes;const bool Read=FFileHelper::LoadFileToArray(Bytes,*File);
        if(!InstanceCurrent(Stage)||!Read||Bytes.Num()!=Size)return Fail(Stage,TEXT("actor_blueprint_source_file_changed"));
        FSHAHash Digest;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest.Hash);Hash=Digest.ToString();return true;
    }
    bool Check(const TCHAR* Stage)
    {
        if(!InstanceCurrent(Stage))return false;
        for(const auto& F:Lifecycle)
        {
            // Lookup is an observation, not part of the pure current predicate.
            TStrongObjectPtr<UFunction> Actual(Class->FindFunctionByName(F.Name));
            if(!InstanceCurrent(Stage))return false;
            if(Actual.Get()!=F.Function.Get()||(Actual.IsValid()&&(!Live(Actual.Get())||Actual->GetOuter()!=F.Owner||Actual->FunctionFlags!=F.Flags||Actual->Script!=F.Script)))return Fail(Stage,TEXT("actor_blueprint_lifecycle_changed"),ClassPath,F.Name.ToString());
        }
        FString Now;if(!Disk(Stage,Now))return false;if(!Now.Equals(FileSHA,ESearchCase::CaseSensitive))return Fail(Stage,TEXT("actor_blueprint_source_file_changed"),File,TEXT("SHA1"),FileSHA,Now,true);
        Data->SetBoolField(TEXT("scope_safe"),true);Data->SetStringField(TEXT("last_current_stage"),Stage);return true;
    }
    bool CaptureGraphs()
    {
        auto* B=Blueprint.Get();Functions=B->FunctionGraphs;Macros=B->MacroGraphs;Events=B->UbergraphPages;Delegates=B->DelegateSignatureGraphs;
        // Only direct category graphs are admitted. Check the absent child slots
        // before any recursive SDK traversal could follow callback-created cycles.
        for(const auto* Category:{&Functions,&Macros,&Events,&Delegates})for(const auto& H:*Category){if(!H.IsResolved()||!Live(H.Get())||!H->SubGraphs.IsEmpty())return Fail(TEXT("bind_source_graphs"),TEXT("actor_blueprint_graph_unavailable"));GraphInventory.Add(H.Get());}
        if(GraphInventory.Num()>16)return Fail(TEXT("bind_source_graphs"),TEXT("actor_blueprint_graph_budget"));
        TSet<UEdGraph*> Unique;int32 PinCount=0;
        for(UEdGraph* G:GraphInventory)
        {
            if(!Live(G)||G->GetClass()!=UEdGraph::StaticClass()||Unique.Contains(G)||G->GetOuter()!=B||!G->SubGraphs.IsEmpty()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr().Get()!=UEdGraphSchema_K2::StaticClass()||G->Nodes.Num()>64)return Fail(TEXT("bind_source_graphs"),TEXT("actor_blueprint_graph_profile_pending"));
            Unique.Add(G);FGraph Row;Row.Graph.Reset(G);Row.Guid=G->GraphGuid;Row.Outer=G->GetOuter();Row.Schema=G->Schema.GetGCPtr().Get();Holds.Emplace(Row.Schema);
            for(const auto& H:G->Nodes){if(!H.IsResolved()||!Live(H.Get())||H->GetOuter()!=G)return Fail(TEXT("bind_source_nodes"),TEXT("actor_blueprint_graph_nodes_unavailable"));Row.Nodes.Add(H.Get());}Graphs.Add(MoveTemp(Row));
        }
        for(const auto& G:Graphs)for(UEdGraphNode* N:G.Nodes)
        {
            FNode Row;Row.Node.Reset(N);Row.Graph=G.Graph.Get();Row.Class=N->GetClass();Holds.Emplace(Row.Class);Row.Guid=N->NodeGuid;Row.Path=N->GetPathName();Row.Comment=N->NodeComment;Row.X=N->NodePosX;Row.Y=N->NodePosY;
            if(N->Pins.Num()>32)return Fail(TEXT("bind_source_pins"),TEXT("actor_blueprint_pin_budget"));
            for(UEdGraphPin* P:N->Pins)
            {
                if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->DefaultObject.IsResolved()||++PinCount>512)return Fail(TEXT("bind_source_pins"),TEXT("actor_blueprint_pin_unavailable"));
                if(P->DefaultObject.Get())Holds.Emplace(P->DefaultObject.Get());
                Row.Pins.Add({P,P->PinId,P->PinName,P->Direction,P->PinType,P->DefaultValue,P->AutogeneratedDefaultValue,P->DefaultTextValue,P->DefaultObject.Get(),P->LinkedTo,FTextInspector::GetFlags(P->DefaultTextValue)});
            }Nodes.Add(MoveTemp(Row));
        }return true;
    }
    bool Bind(const FState& Before,bool InAuthoredDelete=false,bool InAuthoredTransform=false)
    {
        AuthoredDelete=InAuthoredDelete;AuthoredTransform=InAuthoredTransform;
        Data->SetBoolField(TEXT("native_blueprint_actor_scope"),true);Data->SetBoolField(TEXT("admission_complete"),false);
        Data->SetStringField(TEXT("scope"),TEXT("Loaded saved-clean direct Actor Blueprint; one stock SCS Scene root plus optional exact native editor visualization Billboard; bounded own primitive scalar variables; interfaces 0; absent or empty UCS/Destroyed/EndPlay bytecode. Captured source/graph/pin/basic-other-actor state; logical Scene role after public editor transform; accepted delete retires selected fields. Opaque callback internals/runtime/GC/full inherited-property proof remain incomplete."));
        Editor=GEditor;World.Reset(Before.World.Get());Level.Reset(Before.Level.Get());Actor.Reset(Before.Target.Get());NativeRootProperty=FindFProperty<FObjectProperty>(AActor::StaticClass(),FName(TEXT("RootComponent")));
        if(!Context(TEXT("bind_context"))||!Live(Actor.Get()))return false;
        auto* A=Actor.Get();Class.Reset(A->GetClass());
        if(!Live(Class.Get())||Class->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Class->ClassGeneratedBy.IsResolved())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_class_pending"));
        UObject* Owner=Class->ClassGeneratedBy.Get();Holds.Emplace(Owner);Blueprint.Reset(Cast<UBlueprint>(Owner));
        if(!Live(Blueprint.Get())||Blueprint->GetClass()!=UBlueprint::StaticClass())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_source_pending"));
        auto* B=Blueprint.Get();Package.Reset(B->GetPackage());SourcePath=B->GetPathName();ClassPath=Class->GetPathName();Data->SetStringField(TEXT("source_path"),SourcePath);
        if(!Live(Package.Get())||!Package->GetName().StartsWith(TEXT("/Game/"))||Package->IsDirty()||B->GetOuter()!=Package.Get()||B->BlueprintType!=BPTYPE_Normal||B->Status!=BS_UpToDate||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->NewVariables.Num()>32||!B->ImplementedInterfaces.IsEmpty()||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=AActor::StaticClass()||!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Class.Get()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_source_profile_pending"));
        Skeleton.Reset(B->SkeletonGeneratedClass.GetGCPtr().Get());SCS.Reset(B->SimpleConstructionScript.Get());
        const auto* GeneratedBP=static_cast<UBlueprintGeneratedClass*>(Class.Get());
        if(!GeneratedBP->DynamicBindingObjects.IsEmpty()||!GeneratedBP->ComponentTemplates.IsEmpty()||!GeneratedBP->Timelines.IsEmpty()||!B->ComponentTemplates.IsEmpty()||!B->Timelines.IsEmpty())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_dynamic_bindings_pending"));
        if(!Live(Skeleton.Get())||Skeleton->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!Live(SCS.Get())||SCS->GetAllNodes().IsEmpty()||SCS->GetRootNodes().Num()!=1||(!AuthoredHierarchy()&&(SCS->GetAllNodes().Num()!=1||SCS->GetAllNodes()[0]!=SCS->GetRootNodes()[0])))return Fail(TEXT("bind_source"),TEXT("actor_blueprint_scs_profile_pending"));
        if(AuthoredTransform&&SCS->GetAllNodes().Num()==1&&SCS->GetRootNodes()[0]==SCS->GetAllNodes()[0])
        {
            auto* N=SCS->GetRootNodes()[0];if(Live(N)&&N->ComponentClass.IsResolved()&&N->ComponentClass.Get()==USceneComponent::StaticClass())AuthoredTransform=false;
        }
        SCSNode.Reset(SCS->GetRootNodes()[0]);
        if(!Live(SCSNode.Get())||!SCSNode->ComponentClass.IsResolved()||(!AuthoredHierarchy()&&SCSNode->ComponentClass.Get()!=USceneComponent::StaticClass())||!SCSNode->ComponentTemplate.IsResolved()||(!AuthoredHierarchy()&&!SCSNode->GetChildNodes().IsEmpty()))return Fail(TEXT("bind_source"),TEXT("actor_blueprint_scs_profile_pending"));
        Template.Reset(Cast<USceneComponent>(SCSNode->ComponentTemplate.Get()));
        if(!Live(Template.Get())||!Class->ClassDefaultObject.IsResolved())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_template_unavailable"));
        CDO.Reset(Cast<AActor>(Class->ClassDefaultObject.Get()));if(!Live(CDO.Get()))return Fail(TEXT("bind_source"),TEXT("actor_blueprint_cdo_unavailable"));
        SCSOuter=SCS->GetOuter();SCSClass=SCS->GetClass();NodeOuter=SCSNode->GetOuter();NodeClass=SCSNode->GetClass();Holds.Emplace(SCSOuter);Holds.Emplace(SCSClass);Holds.Emplace(NodeClass);
        if(SCSClass!=USimpleConstructionScript::StaticClass()||NodeClass!=USCS_Node::StaticClass()||NodeOuter!=SCS.Get())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_scs_class_pending"));
        GeneratedFrame=static_cast<UBlueprintGeneratedClass*>(Class.Get())->UberGraphFramePointerProperty;SkeletonFrame=static_cast<UBlueprintGeneratedClass*>(Skeleton.Get())->UberGraphFramePointerProperty;
        TemplateOwner=Template->GetOwner();TemplateWorld=Template->GetWorld();if(TemplateOwner||TemplateWorld)return Fail(TEXT("bind_source"),TEXT("actor_blueprint_template_context_pending"));
        CDOFlags=CDO->GetFlags();CDOOuter=CDO->GetOuter();CDOPath=CDO->GetPathName();CDORoot.Reset(CDO->GetRootComponent());if(CDORoot.IsValid()&&!Live(CDORoot.Get()))return Fail(TEXT("bind_source"),TEXT("actor_blueprint_cdo_root_unavailable"));CDOTransform=CDO->GetActorTransform();CDOTags=CDO->Tags;
        ClassFlags=Class->GetFlags();SkeletonFlags=Skeleton->GetFlags();ClassTypeFlags=Class->GetClassFlags();SkeletonTypeFlags=Skeleton->GetClassFlags();SkeletonPath=Skeleton->GetPathName();
        TemplateOuter=Template->GetOuter();TemplateFlags=Template->GetFlags();TemplatePath=Template->GetPathName();TemplateTransform=Template->GetRelativeTransform();TemplateTags=Template->ComponentTags;TemplateMobility=Template->Mobility;TemplateVisible=Template->IsVisible();TemplateHidden=Template->bHiddenInGame;TemplateVisualize=Template->bVisualizeComponent;
        VariableGuid=SCSNode->VariableGuid;VariableName=SCSNode->GetVariableName();
        if(!VariableGuid.IsValid()||VariableName.IsNone()||!FieldInventory(Class.Get(),Fields)||!FieldInventory(Skeleton.Get(),SkeletonFields)||!CaptureGraphs())return Fail(TEXT("bind_source"),TEXT("actor_blueprint_source_inventory_pending"));
        for(const auto& Entry: {TPair<FStructProperty*,const TArray<FField>*>(GeneratedFrame,&Fields),TPair<FStructProperty*,const TArray<FField>*>(SkeletonFrame,&SkeletonFields)})if(Entry.Key&&!Entry.Value->ContainsByPredicate([&](const FField& F){return F.P==Entry.Key;}))return Fail(TEXT("bind_source"),TEXT("actor_blueprint_frame_binding_pending"));
        Data->SetBoolField(TEXT("generated_frame_present"),GeneratedFrame!=nullptr);Data->SetBoolField(TEXT("skeleton_frame_present"),SkeletonFrame!=nullptr);Data->SetBoolField(TEXT("frame_default_buffer_read"),false);
        for(const auto& E:Fields)if(E.Name==VariableName)ComponentProperty=CastField<FObjectProperty>(E.P);
        for(const auto& E:SkeletonFields)if(E.Name==VariableName)SkeletonProperty=CastField<FObjectProperty>(E.P);
        if(!ComponentProperty||!SkeletonProperty||ComponentProperty->ArrayDim!=1||ComponentProperty->GetSize()!=sizeof(TObjectPtr<UObject>)||!ComponentProperty->PropertyClass.IsResolved()||ComponentProperty->PropertyClass.Get()!=(AuthoredHierarchy()?Template->GetClass():USceneComponent::StaticClass())||!SkeletonProperty->PropertyClass.IsResolved()||SkeletonProperty->PropertyClass.Get()!=(AuthoredHierarchy()?Template->GetClass():USceneComponent::StaticClass()))return Fail(TEXT("bind_source"),TEXT("actor_blueprint_component_binding_pending"));
        Root.Reset(A->GetRootComponent());
        if(AuthoredHierarchy())
        {
            AuthoredParentProperty=FindFProperty<FObjectProperty>(USceneComponent::StaticClass(),FName(TEXT("AttachParent")));USceneComponent* Parent=nullptr;
            if(!UWParent(Root.Get(),Parent)||Parent)return Fail(TEXT("bind_authored_root_parent"),TEXT("actor_authored_scs_root_parent_pending"));
        }
        if(!Live(Root.Get())||A->GetOwner()||A->GetLevel()!=Level.Get()||A->GetClass()!=Class.Get()||A->IsTemplate()||A->IsPackageExternal()||A->HasAnyFlags(RF_Transient)||A->bIsEditorOnlyActor||(!AuthoredHierarchy()&&A->GetAttachParentActor()))return Fail(TEXT("bind_instance"),TEXT("actor_blueprint_instance_profile_pending"));
        ActorPath=A->GetPathName();ActorTags=A->Tags;RootName=Root->GetName();RootTags=Root->ComponentTags;Mobility=Root->Mobility;Visible=Root->IsVisible();Hidden=Root->bHiddenInGame;BeforeTransform=A->GetActorTransform();ExpectedTransform=BeforeTransform;RootVisualize=Root->bVisualizeComponent;if(AuthoredTransform)UXActorFlags=A->GetFlags();
        if(AuthoredHierarchy()){if(!CaptureAuthoredDelete())return Fail(TEXT("bind_authored_delete"),TEXT("actor_authored_scs_delete_profile_pending"));}
        else if(!BindVisualization())return false;
        for(const auto& H:Level->Actors)
        {
            if(!H.IsResolved())return Fail(TEXT("bind_other_actors"),TEXT("actor_blueprint_level_slot_unresolved"));AActor* Other=H.Get();if(!Other||Other==A)continue;
            if(!Live(Other)||Others.Num()>=10000)return Fail(TEXT("bind_other_actors"),TEXT("actor_blueprint_other_actor_unavailable"));
            FOther Row;Row.Actor.Reset(Other);Row.Root.Reset(Other->GetRootComponent());Row.Class=Other->GetClass();Holds.Emplace(Row.Class);Row.Outer=Other->GetOuter();Row.Path=Other->GetPathName();Row.Label=Other->GetActorLabel(false);Row.Transform=Other->GetActorTransform();Row.Tags=Other->Tags;Row.Selected=Other->IsSelectedInEditor();Others.Add(MoveTemp(Row));
        }
        if(!CaptureOwnScalars())return Fail(TEXT("bind_own_variables"),TEXT("actor_blueprint_own_primitive_variables_pending"),SourcePath,TEXT("own scalar declarations/CDO/instance values"));
        Bound=true;if(!InstanceCurrent(TEXT("bind_complete_roles")))return false;
        TArray<TSharedPtr<FJsonValue>> Rows;bool LifecycleSafe=true;
        for(const FName Name:{FName(TEXT("UserConstructionScript")),FName(TEXT("ReceiveDestroyed")),FName(TEXT("ReceiveEndPlay"))})
        {
            TStrongObjectPtr<UFunction> F(Class->FindFunctionByName(Name));if(!InstanceCurrent(TEXT("after_lifecycle_lookup")))return false;
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Name.ToString());J->SetBoolField(TEXT("exists"),F.IsValid());J->SetBoolField(TEXT("owner_observed"),F.IsValid());
            FFunction Row;Row.Name=Name;Row.Owner=nullptr;Row.Flags=FUNC_None;
            if(F.IsValid()){if(!Live(F.Get())||!F->GetOuterUClass())return Fail(TEXT("bind_lifecycle"),TEXT("actor_blueprint_lifecycle_unavailable"));Row.Owner=F->GetOuterUClass();Holds.Emplace(Row.Owner);Row.Flags=F->FunctionFlags;Row.Script=F->Script;Row.Function.Reset(F.Get());J->SetStringField(TEXT("function_path"),F->GetPathName());J->SetStringField(TEXT("owner_path"),Row.Owner->GetPathName());J->SetNumberField(TEXT("script_size"),Row.Script.Num());J->SetNumberField(TEXT("function_flags"),double(uint32(Row.Flags)));LifecycleSafe&=Row.Script.IsEmpty();}else J->SetNumberField(TEXT("script_size"),0);
            J->SetBoolField(TEXT("absent_or_empty_bytecode"),!F.IsValid()||Row.Script.IsEmpty());Lifecycle.Add(MoveTemp(Row));Rows.Add(MakeShared<FJsonValueObject>(J));
        }
        Data->SetArrayField(TEXT("lifecycle_functions"),Rows);Data->SetBoolField(TEXT("lifecycle_profile_passed"),LifecycleSafe);
        if(!LifecycleSafe)return Fail(TEXT("bind_lifecycle"),TEXT("actor_blueprint_lifecycle_bytecode_pending"),SourcePath,TEXT("UCS/ReceiveDestroyed/ReceiveEndPlay"));
        File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());if(!Disk(TEXT("bind_source_file"),FileSHA))return false;
        Data->SetStringField(TEXT("source_file_sha1"),FileSHA);Data->SetStringField(TEXT("source_class_path"),ClassPath);Data->SetStringField(TEXT("source_skeleton_path"),SkeletonPath);Data->SetStringField(TEXT("scs_variable_guid"),VariableGuid.ToString());Data->SetStringField(TEXT("scs_variable_name"),VariableName.ToString());Data->SetStringField(TEXT("target_path"),ActorPath);Data->SetStringField(TEXT("root_path_before"),Root->GetPathName());
        const auto Parent=Cubeless::UE58::InspectBlueprintParent(ClassPath);if(!Check(TEXT("after_runtime_parent_audit")))return false;Data->SetObjectField(TEXT("source_runtime_parent_audit"),Parent);bool ParentOK=false;if(!Parent->TryGetBoolField(TEXT("success"),ParentOK)||!ParentOK)return Fail(TEXT("bind_dependency"),TEXT("actor_blueprint_runtime_parent_rejected"));
        const auto Dependencies=Cubeless::UE58::ReadPackageDependencies({Package->GetName()},true,256);if(!Check(TEXT("after_source_dependency_audit")))return false;Data->SetObjectField(TEXT("source_dependency_audit"),Dependencies);bool Passed=false;if(!Dependencies->TryGetBoolField(TEXT("passed"),Passed)||!Passed)return Fail(TEXT("bind_dependency"),TEXT("actor_blueprint_dependencies_rejected"));
        const TSet<FString> NativePackages={TEXT("/Script/Engine"),TEXT("/Script/CoreUObject"),TEXT("/Script/PhysicsCore"),TEXT("/Script/MeshDescription"),TEXT("/Script/StaticMeshDescription"),TEXT("/Script/NavigationSystem")};
        for(const auto& Edge:Dependencies->GetArrayField(TEXT("edges"))){const FString Name=Edge->AsObject()->GetStringField(TEXT("to"));if(!Name.StartsWith(TEXT("/Game/"))&&!Name.StartsWith(TEXT("/Engine/"))&&!NativePackages.Contains(Name))return Fail(TEXT("bind_dependency"),TEXT("actor_blueprint_dependency_module_pending"),Name);}
        if(AuthoredDelete)Data->SetStringField(TEXT("scope"),TEXT("Delete-only loaded saved-clean direct Actor BP; own closed 1..64 native Engine Scene-derived SCS tree, GUID/member binding and reflected editable template/instance values. Existing source graph/file/lifecycle and survivor checks. Accepted DestroyActor retires whole selected actor/component closure; False is unknown with no selected readback. Native unreflected/cache/global callback effects and wider original-valid shapes remain incomplete."));
        Data->SetBoolField(TEXT("admission_complete"),true);return Check(TEXT("bind_complete"));
    }
    bool ReacquireRoot(const TCHAR* Stage)
    {
        // The previous root may already have been renamed/destroyed by PostEditMove.
        // Its address remains held but is never inspected after dispatch.
        if(!InstanceCurrent(Stage))return false;
        TStrongObjectPtr<USceneComponent> Returned(Actor->GetRootComponent());if(!InstanceCurrent(Stage))return false;
        if(!Live(Returned.Get()))return Fail(Stage,TEXT("actor_blueprint_reconstructed_root_unavailable"));
        const auto* Slot=ComponentProperty->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Actor.Get());
        if(!Slot||!Slot->IsResolved()||Slot->Get()!=Returned.Get())return Fail(Stage,TEXT("actor_blueprint_reconstructed_binding_changed"));
        Data->SetBoolField(TEXT("root_physical_identity_changed"),Returned.Get()!=Root.Get());Data->SetStringField(TEXT("root_path_after"),Returned->GetPathName());
        TInlineComponentArray<UActorComponent*> Components(Actor.Get());for(UActorComponent* C:Components){if(C)Holds.Emplace(C);}
        if(!InstanceCurrent(Stage))return false;TStrongObjectPtr<UBillboardComponent> ReturnedSprite;
        for(UActorComponent* C:Components)if(C!=Returned.Get())
        {
            if(ReturnedSprite.IsValid()||!Live(C)||C->GetClass()!=UBillboardComponent::StaticClass())return Fail(Stage,TEXT("actor_blueprint_reconstructed_components_changed"),ActorPath,TEXT("component_count"),LexToString(1+(SpritePresent?1:0)),LexToString(Components.Num()),true);
            ReturnedSprite.Reset(static_cast<UBillboardComponent*>(C));
        }
        if(ReturnedSprite.IsValid()!=SpritePresent)return Fail(Stage,TEXT("actor_blueprint_visualization_presence_changed"));
        RetiredRoots.Emplace(Root.Get());RetiredSprites.Emplace(Sprite.Get());Root.Reset(Returned.Get());Sprite.Reset(ReturnedSprite.Get());Reacquiring=false;return Check(Stage);
    }
    bool Execute(EOperation Operation,const FTransform& Desired,bool NoOp,bool OriginalDirect=false)
    {
        if(AuthoredTransform)return ExecuteAuthoredTransform(Desired,NoOp,OriginalDirect);
        if(!Check(TEXT("before_blueprint_execute")))return false;if(NoOp&&!OriginalDirect){Data->SetBoolField(TEXT("no_op_writer_skipped"),true);return true;}
        if(OriginalDirect)
        {
            if(Operation!=EOperation::SetTransform)return Fail(TEXT("original_blueprint_operation"),TEXT("actor_original_operation_invalid"));
            ExpectedTransform=Desired;Dispatched=true;Count(TEXT("transform_calls"));Data->SetStringField(TEXT("transform_writer"),TEXT("AActor::SetActorTransform original direct"));
            const bool Accepted=Actor->SetActorTransform(Desired);Data->SetBoolField(TEXT("transform_returned"),true);Data->SetBoolField(TEXT("transform_accepted"),Accepted);
            Check(TEXT("after_original_blueprint_transform"));return Accepted;
        }
        TStrongObjectPtr<UEditorActorSubsystem> Subsystem(GEditor->GetEditorSubsystem<UEditorActorSubsystem>());if(!Check(TEXT("after_blueprint_subsystem"))||!Live(Subsystem.Get()))return Fail(TEXT("after_blueprint_subsystem"),TEXT("actor_blueprint_subsystem_unavailable"));
        if(Operation==EOperation::Delete)
        {
            Count(TEXT("destroy_calls"));const bool Accepted=Subsystem->DestroyActor(Actor.Get());Data->SetBoolField(TEXT("destroy_returned"),true);Data->SetBoolField(TEXT("destroy_accepted"),Accepted);
            if(!Accepted){Data->SetBoolField(TEXT("destroy_false_readback_suppressed"),true);return Fail(TEXT("after_blueprint_destroy_false"),TEXT("actor_blueprint_delete_outcome_unknown"));}
            Retired=true;Data->SetBoolField(TEXT("retired_after_accepted_destroy"),true);if(!Check(TEXT("after_blueprint_destroy")))return true;
            Count(TEXT("mark_level_dirty_calls"));Level->MarkPackageDirty();Check(TEXT("after_blueprint_delete_dirty"));return true;
        }
        const FScopedTransaction Transaction(FText::FromString(TEXT("Cubeless Blueprint Actor Transform")));if(!Check(TEXT("after_blueprint_transaction")))return false;
        Count(TEXT("world_modify_calls"));World->Modify(false);if(!Check(TEXT("after_blueprint_world_modify")))return false;
        Count(TEXT("level_modify_calls"));Level->Modify(false);if(!Check(TEXT("after_blueprint_level_modify")))return false;
        Count(TEXT("actor_modify_calls"));Actor->Modify(false);if(!Check(TEXT("after_blueprint_actor_modify")))return false;
        Count(TEXT("root_modify_calls"));Root->Modify(false);if(!Check(TEXT("after_blueprint_root_modify")))return false;
        ExpectedTransform=Desired;Dispatched=true;OwnEditorTransformDispatched=true;Reacquiring=true;Count(TEXT("transform_calls"));
        const bool Accepted=Subsystem->SetActorTransform(Actor.Get(),Desired);Data->SetBoolField(TEXT("transform_returned"),true);Data->SetBoolField(TEXT("transform_accepted"),Accepted);
        if(!Accepted){Data->SetBoolField(TEXT("transform_false_readback_suppressed"),true);return Fail(TEXT("after_blueprint_transform_false"),TEXT("actor_blueprint_transform_outcome_unknown"));}
        if(!ReacquireRoot(TEXT("after_blueprint_transform")))return true;
        Count(TEXT("mark_level_dirty_calls"));Level->MarkPackageDirty();Check(TEXT("after_blueprint_transform_dirty"));return true;
    }
};

// RB: one native parent deletion, with independently held surviving native children.
// Keep the existing public EditorActorSubsystem writer; do not synthesize detach calls.
struct FParentActorDeleteMutation
{
    FFileBackedNewMapWitness NewMapWitness;
    TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<ULevel> Level;
    TStrongObjectPtr<AActor> Parent;TStrongObjectPtr<USceneComponent> ParentRoot;
    TStrongObjectPtr<UEditorActorSubsystem> Subsystem;TStrongObjectPtr<USelection> Selection;TStrongObjectPtr<ULayersSubsystem> Layers;
    TSharedRef<FJsonObject> Data=MakeShared<FJsonObject>();FString FailureCode,FailureStage,ParentPath;
    bool Stopped=false,Rejected=false,Bound=false,Retired=false,Entered=false;int32 ParentIndex=INDEX_NONE;
    struct FActorRole{TStrongObjectPtr<AActor> Actor;TStrongObjectPtr<USceneComponent> Root;UClass* Class=nullptr;FString Path;FTransform Transform;TArray<FName> Tags;bool Selected=false;};
    struct FSceneRole{TStrongObjectPtr<USceneComponent> Scene;AActor* Owner=nullptr;UClass* Class=nullptr;UObject* Outer=nullptr;FString Path;USceneComponent* Parent=nullptr;TArray<USceneComponent*> Children;FName Socket;FTransform Relative,World;TArray<FName> Tags;EComponentMobility::Type Mobility=EComponentMobility::Static;bool Registered=false,AbsoluteLocation=false,AbsoluteRotation=false,AbsoluteScale=false;};
    struct FComponentRole{TStrongObjectPtr<UActorComponent> Component;TStrongObjectPtr<UWorld> ComponentWorld;AActor* Owner=nullptr;UClass* Class=nullptr;UObject* Outer=nullptr;FString Path;EComponentCreationMethod Method=EComponentCreationMethod::Native;};
    struct FLevelRole{TStrongObjectPtr<AActor> Actor;AActor* Owner=nullptr;bool Selected=false;};
    TArray<FActorRole> Actors;TArray<FComponentRole> Components;TArray<FSceneRole> Scenes;
    TArray<AActor*> LevelActors;TArray<FLevelRole> OtherRoles;TArray<TStrongObjectPtr<UObject>> Holds;
    static bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    static bool Plain(UClass* C){return C==ATargetPoint::StaticClass()||C==AStaticMeshActor::StaticClass()||C==APointLight::StaticClass()||C==ASpotLight::StaticClass()||C==ADirectionalLight::StaticClass()||C==ACameraActor::StaticClass();}
    static bool HasChildren(AActor* A){TArray<AActor*> Values;A->GetAttachedActors(Values);return !Values.IsEmpty();}
    static bool Scale(const FVector& S){return !S.ContainsNaN()&&S.X>0&&S.X<=1.0e12&&S.X==S.Y&&S.Y==S.Z;}
    bool Bad(const TCHAR* Stage,const TCHAR* Code,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual,bool Unknown=true)
    {
        if(!Stopped&&!Rejected){FailureCode=Code;FailureStage=Stage;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);Data->SetObjectField(TEXT("first_parent_delete_mismatch"),J);}
        Stopped|=Unknown;Rejected|=!Unknown;Data->SetBoolField(TEXT("scope_safe"),false);Data->SetBoolField(TEXT("boundary_state_unknown"),Stopped);return false;
    }
    bool Context(const TCHAR* Stage)
    {
        Data->SetObjectField(TEXT("newly_created_map_admission"),NewMapWitness.Receipt);
        if(Stopped)return false;
        if(!IsInGameThread()||!GEditor||GEditor!=Editor.Get()||!Live(Editor.Get())||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Live(World.Get())||!Live(Level.Get())||World->bIsTearingDown||World->WorldType!=EWorldType::Editor)
            return Bad(Stage,TEXT("actor_parent_delete_context_lost"),ParentPath,TEXT("editor_context"),TEXT("captured idle live Editor"),TEXT("unavailable"));
        if(GEditor->GetEditorWorldContext().World()!=World.Get()||World->PersistentLevel.Get()!=Level.Get()||World->GetCurrentLevel()!=Level.Get()||Level->OwningWorld!=World.Get()||World->GetLevels().Num()!=1||World->IsPartitionedWorld()||Level->IsUsingExternalActors()||FLevelUtils::IsLevelLocked(Level.Get())||!NewMapWitness.Current(World.Get(),Level.Get()))
            return Bad(Stage,TEXT("actor_parent_delete_level_changed"),ParentPath,TEXT("level_role"),TEXT("same saved unlocked persistent level"),TEXT("changed"));
        return true;
    }
    bool Check(const TCHAR* Stage)
    {
        if(Rejected)return true; // Initial unsupported admission: copied metadata only, no observer.
        if(!Context(Stage))return false;if(!Bound)return true;
        if(Level->Actors.Num()!=LevelActors.Num())return Bad(Stage,TEXT("actor_parent_delete_level_membership"),ParentPath,TEXT("level_actor_slots"),LexToString(LevelActors.Num()),LexToString(Level->Actors.Num()));
        for(int32 I=0;I<LevelActors.Num();++I){if(!Level->Actors[I].IsResolved()||Level->Actors[I].Get()!=((Retired&&I==ParentIndex)?nullptr:LevelActors[I]))return Bad(Stage,TEXT("actor_parent_delete_level_membership"),ParentPath,TEXT("level_actor_slot"),LexToString(I),TEXT("changed or unresolved"));}
        for(const auto& E:OtherRoles){if(!Live(E.Actor.Get())||E.Actor->GetLevel()!=Level.Get()||E.Actor->GetOwner()!=E.Owner||E.Actor->IsSelectedInEditor()!=E.Selected)return Bad(Stage,TEXT("actor_parent_delete_other_actor_changed"),ParentPath,TEXT("other_actor_owner_selection"),TEXT("captured live roles"),TEXT("changed"));}
        for(int32 I=0;I<Actors.Num();++I)
        {
            if(Retired&&I==0)continue;const auto& E=Actors[I];AActor* A=E.Actor.Get();
            if(!Live(A)||A->GetClass()!=E.Class||A->GetOuter()!=Level.Get()||A->GetLevel()!=Level.Get()||A->GetRootComponent()!=E.Root.Get()||!Live(E.Root.Get())||!A->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive)||A->Tags!=E.Tags||A->GetOwner()||A->IsChildActor()||A->IsTemplate()||A->IsPackageExternal()||A->HasAnyFlags(RF_Transient)||A->bIsEditorOnlyActor||!A->IsEditable()||A->AActor::IsLockLocation()||A->IsSelectedInEditor()!=E.Selected||!A->GetActorTransform().Equals(E.Transform,Retired?0.01:0.0))
                return Bad(Stage,TEXT("actor_parent_delete_actor_role_changed"),E.Path,TEXT("actor_identity_transform"),E.Transform.ToString(),TEXT("changed/unavailable"));
            if(!Retired&&I==0&&A->OnDestroyed.IsBound())return Bad(Stage,TEXT("actor_parent_delete_callback_changed"),E.Path,TEXT("OnDestroyed"),TEXT("unbound native actor"),TEXT("bound"));
            TInlineComponentArray<UActorComponent*> Actual(A);int32 Expected=0;for(const auto& C:Components)if(C.Owner==A){++Expected;if(!Actual.Contains(C.Component.Get()))return Bad(Stage,TEXT("actor_parent_delete_components_changed"),E.Path,TEXT("component_membership"),TEXT("same held native components"),TEXT("missing"));}
            if(Actual.Num()!=Expected)return Bad(Stage,TEXT("actor_parent_delete_components_changed"),E.Path,TEXT("component_count"),LexToString(Expected),LexToString(Actual.Num()));
            TArray<AActor*> Attached;A->GetAttachedActors(Attached);if(I==0){if(Attached.Num()!=Actors.Num()-1)return Bad(Stage,TEXT("actor_parent_delete_children_changed"),E.Path,TEXT("attached_actor_count"),LexToString(Actors.Num()-1),LexToString(Attached.Num()));for(int32 J=1;J<Actors.Num();++J)if(!Attached.Contains(Actors[J].Actor.Get()))return Bad(Stage,TEXT("actor_parent_delete_children_changed"),E.Path,TEXT("attached_actor_membership"),Actors[J].Path,TEXT("missing"));}
            else if(!Attached.IsEmpty())return Bad(Stage,TEXT("actor_parent_delete_children_changed"),E.Path,TEXT("child_actor_count"),TEXT("0"),LexToString(Attached.Num()));
        }
        for(const auto& E:Components){if(Retired&&E.Owner==Parent.Get())continue;auto* C=E.Component.Get();if(!Live(C)||C->GetOwner()!=E.Owner||C->GetClass()!=E.Class||C->GetOuter()!=E.Outer||C->CreationMethod!=E.Method||(E.ComponentWorld.Get()&&!Live(E.ComponentWorld.Get()))||C->GetWorld()!=E.ComponentWorld.Get()||!C->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive))return Bad(Stage,TEXT("actor_parent_delete_component_role_changed"),E.Path,TEXT("component_identity_owner"),TEXT("captured"),TEXT("changed"));}
        for(const auto& E:Scenes)
        {
            if(Retired&&E.Owner==Parent.Get())continue;auto* C=E.Scene.Get();if(!Live(C))return Bad(Stage,TEXT("actor_parent_delete_scene_unavailable"),E.Path,TEXT("live"),TEXT("true"),TEXT("false"));
            bool ChildRoot=false;for(int32 I=1;I<Actors.Num();++I)ChildRoot|=Actors[I].Root.Get()==C;
            const auto ExpectedParent=Retired&&ChildRoot?nullptr:E.Parent;const FTransform& ExpectedRelative=Retired&&ChildRoot?E.World:E.Relative;
            if(C->GetAttachParent()!=ExpectedParent||C->GetAttachSocketName()!=E.Socket||C->Mobility!=E.Mobility||C->IsRegistered()!=E.Registered||C->ComponentTags!=E.Tags||C->IsUsingAbsoluteLocation()!=E.AbsoluteLocation||C->IsUsingAbsoluteRotation()!=E.AbsoluteRotation||C->IsUsingAbsoluteScale()!=E.AbsoluteScale)
                return Bad(Stage,TEXT("actor_parent_delete_scene_role_changed"),E.Path,TEXT("attachment_socket_mobility_tags"),Retired&&ChildRoot?TEXT("detached root; other roles captured"):TEXT("captured"),TEXT("changed"));
            if(!C->GetRelativeTransform().Equals(ExpectedRelative,Retired?0.01:0.0)||!C->GetComponentTransform().Equals(E.World,Retired?0.01:0.0))return Bad(Stage,TEXT("actor_parent_delete_keep_world_failed"),E.Path,TEXT("relative/world_transform"),ExpectedRelative.ToString()+TEXT(" / ")+E.World.ToString(),C->GetRelativeTransform().ToString()+TEXT(" / ")+C->GetComponentTransform().ToString());
            const auto& Children=C->GetAttachChildren();if(Children.Num()!=E.Children.Num())return Bad(Stage,TEXT("actor_parent_delete_scene_children_changed"),E.Path,TEXT("internal_children_count"),LexToString(E.Children.Num()),LexToString(Children.Num()));for(int32 I=0;I<Children.Num();++I)if(!Children[I].IsResolved()||Children[I].Get()!=E.Children[I])return Bad(Stage,TEXT("actor_parent_delete_scene_children_changed"),E.Path,TEXT("internal_child_slot"),LexToString(I),TEXT("changed/unresolved"));
            if(auto* P=Cast<UPrimitiveComponent>(C)){if(P->IsSimulatingPhysics()||P->IsWelded())return Bad(Stage,TEXT("actor_parent_delete_physics_pending"),E.Path,TEXT("simulated_or_welded"),TEXT("false"),TEXT("true"));}
        }
        Data->SetBoolField(TEXT("children_current_verified"),true);Data->SetBoolField(TEXT("children_detached_keep_world_verified"),Retired);return Context(Stage);
    }
    bool Bind(const FState& Before)
    {
        Editor=GEditor;World.Reset(Before.World.Get());Level.Reset(Before.Level.Get());Parent.Reset(Before.Target.Get());if(!Live(Parent.Get()))return Bad(TEXT("bind"),TEXT("actor_parent_delete_unavailable"),TEXT(""),TEXT("target"),TEXT("live native actor"),TEXT("unavailable"));ParentPath=Parent->GetPathName();
        Data->SetBoolField(TEXT("parent_actor_delete_contract"),true);Data->SetBoolField(TEXT("destroy_entered"),false);Data->SetBoolField(TEXT("explicit_compile_requested"),false);Data->SetBoolField(TEXT("explicit_save_requested"),false);Before.Snapshot->SetObjectField(TEXT("parent_actor_delete_preflight"),Data);
        if(!Context(TEXT("bind_context")))return false;
        TArray<AActor*> Children;Parent->GetAttachedActors(Children);for(AActor* A:Children)if(A)Holds.Emplace(A);
        if(!Plain(Parent->GetClass())||Children.Num()<1||Children.Num()>8||Parent->GetOwner()||Parent->GetAttachParentActor()||Parent->OnDestroyed.IsBound())return Bad(TEXT("bind_profile"),TEXT("actor_parent_delete_profile_pending"),ParentPath,TEXT("plain_parent_children"),TEXT("unowned unbound native parent; 1..8 direct native children"),LexToString(Children.Num()),false);
        ParentRoot.Reset(Parent->GetRootComponent());if(!Live(ParentRoot.Get()))return Bad(TEXT("bind_root"),TEXT("actor_parent_delete_root_unavailable"),ParentPath,TEXT("root"),TEXT("live"),TEXT("unavailable"));
        if(!Parent->GetActorTransform().Equals(Before.TargetTransform,0.0)||!Scale(Before.TargetTransform.GetScale3D()))return Bad(TEXT("bind_profile"),TEXT("actor_parent_delete_scale_pending"),ParentPath,TEXT("transform_scale"),TEXT("captured positive uniform bounded scale"),Parent->GetActorTransform().ToString(),false);
        TSet<AActor*> Unique;Unique.Add(Parent.Get());TArray<AActor*> All{Parent.Get()};
        for(AActor* A:Children){if(!Live(A)||Unique.Contains(A)||A->GetClass()!=AStaticMeshActor::StaticClass()||A->GetOwner()||A->IsSelectedInEditor()||A->GetLevel()!=Level.Get()||A->GetOuter()!=Level.Get()||A->IsChildActor()||A->IsTemplate()||A->IsPackageExternal()||A->HasAnyFlags(RF_Transient)||A->bIsEditorOnlyActor||!A->IsEditable()||A->AActor::IsLockLocation()||!Scale(A->GetActorScale3D()))return Bad(TEXT("bind_child"),TEXT("actor_parent_delete_child_pending"),ParentPath,TEXT("child_actor"),TEXT("unselected unowned same-level exact StaticMeshActor with positive uniform scale"),TEXT("outside first profile"),false);Unique.Add(A);All.Add(A);}
        for(int32 I=0;I<All.Num();++I){auto* A=All[I];auto* Root=A->GetRootComponent();if(!Live(Root)||Root->GetOwner()!=A||Root->GetAttachParent()!=(I==0?nullptr:ParentRoot.Get())||!Root->GetAttachSocketName().IsNone()||Root->IsUsingAbsoluteLocation()||Root->IsUsingAbsoluteRotation()||Root->IsUsingAbsoluteScale()||(I>0&&Root->GetClass()!=UStaticMeshComponent::StaticClass()))return Bad(TEXT("bind_root"),TEXT("actor_parent_delete_root_profile_pending"),ParentPath,TEXT("root_attachment"),TEXT("direct root; no socket/absolute flags"),TEXT("outside first profile"),false);TArray<AActor*> Grandchildren;A->GetAttachedActors(Grandchildren);if(I>0&&!Grandchildren.IsEmpty())return Bad(TEXT("bind_child"),TEXT("actor_parent_delete_deeper_tree_pending"),A->GetPathName(),TEXT("attached_child_count"),TEXT("0"),LexToString(Grandchildren.Num()),false);FActorRole E;E.Actor.Reset(A);E.Root.Reset(Root);E.Class=A->GetClass();E.Path=A->GetPathName();E.Transform=A->GetActorTransform();E.Tags=A->Tags;E.Selected=A->IsSelectedInEditor();Actors.Add(MoveTemp(E));}
        for(auto* A:All){TInlineComponentArray<UActorComponent*> Values(A);if(Values.Num()<1||Values.Num()>64)return Bad(TEXT("bind_components"),TEXT("actor_parent_delete_component_budget"),A->GetPathName(),TEXT("component_count"),TEXT("1..64"),LexToString(Values.Num()),false);for(auto* C:Values){if(!Live(C)||!Live(C->GetClass())||C->GetOwner()!=A||!C->IsIn(A)||C->CreationMethod!=EComponentCreationMethod::Native||!C->GetClass()->HasAnyClassFlags(CLASS_Native)||!C->GetClass()->ClassGeneratedBy.IsResolved()||C->GetClass()->ClassGeneratedBy.Get()!=nullptr||C->GetClass()->GetPackage()->GetName()!=TEXT("/Script/Engine"))return Bad(TEXT("bind_components"),TEXT("actor_parent_delete_component_pending"),A->GetPathName(),TEXT("native_component"),TEXT("owned exact Engine native role"),TEXT("outside profile"),false);FComponentRole E;E.Component.Reset(C);E.ComponentWorld.Reset(C->GetWorld());if(E.ComponentWorld.Get()&&!Live(E.ComponentWorld.Get()))return Bad(TEXT("bind_components"),TEXT("actor_parent_delete_component_world_unavailable"),C->GetPathName(),TEXT("component_world"),TEXT("captured nullable live role"),TEXT("unavailable"));E.Owner=A;E.Class=C->GetClass();E.Outer=C->GetOuter();E.Path=C->GetPathName();E.Method=C->CreationMethod;Components.Add(MoveTemp(E));Holds.Emplace(C->GetClass());}}
        TSet<USceneComponent*> Known;for(const auto& C:Components)if(auto* S=Cast<USceneComponent>(C.Component.Get()))Known.Add(S);
        for(const auto& C:Components)if(auto* S=Cast<USceneComponent>(C.Component.Get())){FSceneRole E;E.Scene.Reset(S);E.Owner=C.Owner;E.Class=C.Class;E.Outer=C.Outer;E.Path=C.Path;E.Parent=S->GetAttachParent();E.Socket=S->GetAttachSocketName();E.Relative=S->GetRelativeTransform();E.World=S->GetComponentTransform();E.Tags=S->ComponentTags;E.Mobility=S->Mobility;E.Registered=S->IsRegistered();E.AbsoluteLocation=S->IsUsingAbsoluteLocation();E.AbsoluteRotation=S->IsUsingAbsoluteRotation();E.AbsoluteScale=S->IsUsingAbsoluteScale();if(E.Relative.ContainsNaN()||E.World.ContainsNaN()||(E.Parent&&!Known.Contains(E.Parent)))return Bad(TEXT("bind_scene"),TEXT("actor_parent_delete_scene_pending"),E.Path,TEXT("scene_relation"),TEXT("finite captured own tree"),TEXT("outside profile"),false);for(const auto& Child:S->GetAttachChildren()){if(!Child.IsResolved()||!Known.Contains(Child.Get()))return Bad(TEXT("bind_scene"),TEXT("actor_parent_delete_scene_pending"),E.Path,TEXT("child_membership"),TEXT("resolved captured components only"),TEXT("outside profile"),false);E.Children.Add(Child.Get());}if(auto* P=Cast<UPrimitiveComponent>(S))if(P->IsSimulatingPhysics()||P->IsWelded())return Bad(TEXT("bind_physics"),TEXT("actor_parent_delete_physics_pending"),E.Path,TEXT("simulated_or_welded"),TEXT("false"),TEXT("true"),false);Scenes.Add(MoveTemp(E));}
        if(Level->Actors.Num()>10000)return Bad(TEXT("bind_level"),TEXT("actor_parent_delete_level_budget"),ParentPath,TEXT("level_actor_slots"),TEXT("<=10000"),LexToString(Level->Actors.Num()),false);
        for(int32 I=0;I<Level->Actors.Num();++I){const auto& Slot=Level->Actors[I];if(!Slot.IsResolved())return Bad(TEXT("bind_level"),TEXT("actor_parent_delete_level_unresolved"),ParentPath,TEXT("actor_slot"),TEXT("resolved"),LexToString(I));auto* A=Slot.Get();LevelActors.Add(A);if(A==Parent.Get()){if(ParentIndex!=INDEX_NONE)return Bad(TEXT("bind_level"),TEXT("actor_parent_delete_parent_duplicate"),ParentPath,TEXT("parent_slots"),TEXT("1"),TEXT("duplicate"));ParentIndex=I;}if(A&&A!=Parent.Get()){if(!Live(A)||A->GetOwner()==Parent.Get())return Bad(TEXT("bind_level"),TEXT("actor_parent_delete_owner_pending"),ParentPath,TEXT("other_actor_owner"),TEXT("live; not owned by removed parent"),TEXT("outside profile"),false);FLevelRole E;E.Actor.Reset(A);E.Owner=A->GetOwner();E.Selected=A->IsSelectedInEditor();OtherRoles.Add(MoveTemp(E));}}
        if(ParentIndex==INDEX_NONE)return Bad(TEXT("bind_level"),TEXT("actor_parent_delete_parent_missing"),ParentPath,TEXT("parent_membership"),TEXT("one current slot"),TEXT("missing"));Bound=true;if(!Check(TEXT("bind_complete")))return false;
        Subsystem.Reset(GEditor->GetEditorSubsystem<UEditorActorSubsystem>());if(!Check(TEXT("subsystem_return")))return false;Selection.Reset(GEditor->GetSelectedActors());if(!Check(TEXT("selection_return")))return false;Layers.Reset(GEditor->GetEditorSubsystem<ULayersSubsystem>());if(!Check(TEXT("layers_return")))return false;if(!Live(Subsystem.Get())||!Live(Selection.Get())||!Live(Layers.Get()))return Bad(TEXT("bind_subsystems"),TEXT("actor_parent_delete_subsystem_unavailable"),ParentPath,TEXT("editor_subsystems"),TEXT("existing live actor/selection/layers"),TEXT("unavailable"));
        Data->SetNumberField(TEXT("surviving_child_count"),Actors.Num()-1);TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=1;I<Actors.Num();++I){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor_path"),Actors[I].Path);J->SetStringField(TEXT("world_before"),Actors[I].Transform.ToString());J->SetStringField(TEXT("planned_parent_after"),TEXT(""));J->SetStringField(TEXT("planned_relative_after"),Actors[I].Transform.ToString());Rows.Add(MakeShared<FJsonValueObject>(J));}Data->SetArrayField(TEXT("children_plan"),Rows);Data->SetBoolField(TEXT("scope_safe"),true);Data->SetStringField(TEXT("writer"),TEXT("existing UEditorActorSubsystem::DestroyActor; no synthetic detach"));return Check(TEXT("publish_plan"));
    }
    bool Execute()
    {
        if(!Check(TEXT("before_parent_destroy")))return false;
        if(!Live(Subsystem.Get())||!Live(Selection.Get())||!Live(Layers.Get())||GEditor->GetSelectedActors()!=Selection.Get())return Bad(TEXT("before_parent_destroy"),TEXT("actor_parent_delete_subsystem_changed"),ParentPath,TEXT("held_subsystems"),TEXT("captured live subsystem/selection"),TEXT("changed"));
        Entered=true;Data->SetBoolField(TEXT("destroy_entered"),true);Data->SetNumberField(TEXT("destroy_calls"),1);
        const bool Accepted=Subsystem->DestroyActor(Parent.Get());Data->SetBoolField(TEXT("destroy_returned"),true);Data->SetBoolField(TEXT("destroy_accepted"),Accepted);
        Retired=true;Data->SetBoolField(TEXT("selected_getters_retired"),true);
        if(!Accepted)return Bad(TEXT("parent_destroy_false"),TEXT("actor_parent_delete_outcome_unknown"),ParentPath,TEXT("destroy_return"),TEXT("accepted known boundary"),TEXT("false; opaque selection/layer/delegate prefix"));
        if(!Check(TEXT("after_parent_destroy")))return true;
        TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=1;I<Actors.Num();++I){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor_path"),Actors[I].Path);J->SetStringField(TEXT("world_after"),Actors[I].Actor->GetActorTransform().ToString());J->SetStringField(TEXT("relative_after"),Actors[I].Root->GetRelativeTransform().ToString());J->SetStringField(TEXT("attach_parent_after"),TEXT(""));Rows.Add(MakeShared<FJsonValueObject>(J));}Data->SetArrayField(TEXT("children_after"),Rows);
        Data->SetBoolField(TEXT("mark_level_dirty_entered"),true);Level->MarkPackageDirty();Data->SetBoolField(TEXT("mark_level_dirty_returned"),true);Check(TEXT("after_parent_delete_dirty"));return true;
    }
};

// IY: concrete state for the two existing mutations of exact native TargetPoint.
// Strong holds are operation-local. Deletion retires the actor/component identities;
// no getter on those retired objects is used after an accepted DestroyActor.
// UB: one concrete native StaticMeshActor hierarchy transform lease. No detach/delete writer.
struct FParentActorTransformMutation
{
    FFileBackedNewMapWitness NewMapWitness;
    using FComponentRole=FParentActorDeleteMutation::FComponentRole;
    using FSceneRole=FParentActorDeleteMutation::FSceneRole;
    TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<ULevel> Level;
    TStrongObjectPtr<UEditorActorSubsystem> Subsystem;TStrongObjectPtr<UEditorElementSubsystem> Elements;
    TSharedRef<FJsonObject> Data=MakeShared<FJsonObject>();FString FailureCode,FailureStage;
    bool Stopped=false,Rejected=false,Bound=false,Dispatched=false,Planned=false;
    struct FIdentity{TStrongObjectPtr<UObject> Object,Class,Outer,Package;FString Path;};TArray<FIdentity> Identities;
    struct FField{FObjectProperty* Object=nullptr;UClass* Owner=nullptr;FName Name;int32 Offset=0,Size=0;uint64 Flags=0;UClass* ValueClass=nullptr;};FField RootField,MeshField;
    struct FAsset{TStrongObjectPtr<UObject> Object;EObjectFlags Flags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false;};TArray<FAsset> Assets;
    struct FNode
    {
        TStrongObjectPtr<AActor> Actor;FComponentRole Component;FSceneRole Scene;int32 ParentIndex=INDEX_NONE,Depth=0;
        FString Path,Label;TArray<FName> Tags;AActor* Owner=nullptr;bool Selected=false,Collision=true;
        FTransform Before=FTransform::Identity,Expected=FTransform::Identity;
        TSharedPtr<FJsonObject> Authored;
    };TArray<FNode> Nodes;
    struct FOther{TStrongObjectPtr<AActor> Actor;TStrongObjectPtr<USceneComponent> Root;AActor* Owner=nullptr;FString Path,Label;FTransform Transform;TArray<FName> Tags;bool Selected=false;};TArray<FOther> Others;
    TArray<AActor*> LevelActors;
    static bool Live(const UObject* O){return FParentActorDeleteMutation::Live(O);}
    static bool Scale(const FVector& S){return FParentActorDeleteMutation::Scale(S);}
    bool Bad(const TCHAR* Stage,const TCHAR* Code,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual,bool Unknown=true)
    {
        if(!Stopped&&!Rejected){FailureCode=Code;FailureStage=Stage;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);Data->SetObjectField(TEXT("first_parent_transform_mismatch"),J);}
        Stopped|=Unknown;Rejected|=!Unknown;Data->SetBoolField(TEXT("scope_safe"),false);Data->SetBoolField(TEXT("boundary_state_unknown"),Stopped);return false;
    }
    bool Context(const TCHAR* Stage)
    {
        Data->SetObjectField(TEXT("newly_created_map_admission"),NewMapWitness.Receipt);
        if(Stopped||Rejected)return false;
        if(!IsInGameThread()||!GEditor||GEditor!=Editor.Get()||!Live(Editor.Get())||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Live(World.Get())||!Live(Level.Get())||World->bIsTearingDown||World->WorldType!=EWorldType::Editor)
            return Bad(Stage,TEXT("actor_parent_transform_context_lost"),TEXT(""),TEXT("editor_context"),TEXT("same idle live Editor/world"),TEXT("unavailable"));
        if(GEditor->GetEditorWorldContext().World()!=World.Get()||World->PersistentLevel.Get()!=Level.Get()||World->GetCurrentLevel()!=Level.Get()||Level->OwningWorld!=World.Get()||World->GetLevels().Num()!=1||World->IsPartitionedWorld()||Level->IsUsingExternalActors()||FLevelUtils::IsLevelLocked(Level.Get())||!NewMapWitness.Current(World.Get(),Level.Get()))
            return Bad(Stage,TEXT("actor_parent_transform_level_changed"),TEXT(""),TEXT("level_role"),TEXT("same saved unlocked persistent level"),TEXT("changed"));return true;
    }
    bool Pin(UObject* O)
    {
        if(!Live(O)||!Live(O->GetClass())||(O->GetOuter()&&!Live(O->GetOuter()))||!Live(O->GetPackage()))return Bad(TEXT("bind_identity"),TEXT("actor_parent_transform_object_unavailable"),TEXT(""),TEXT("live_object"),TEXT("live held role"),TEXT("unavailable"));
        if(Identities.ContainsByPredicate([&](const FIdentity& E){return E.Object.Get()==O;}))return true;
        if(Identities.Num()>=32768)return Bad(TEXT("bind_identity"),TEXT("actor_parent_transform_identity_budget"),TEXT(""),TEXT("identity_count"),TEXT("<=32768"),TEXT("larger"),false);
        FIdentity E;E.Object.Reset(O);E.Class.Reset(O->GetClass());E.Outer.Reset(O->GetOuter());E.Package.Reset(O->GetPackage());E.Path=O->GetPathName();Identities.Add(MoveTemp(E));return true;
    }
    bool IdentitiesCurrent(const TCHAR* Stage)
    {
        for(const auto& E:Identities){auto* O=E.Object.Get();if(!Live(O)||!Live(E.Class.Get())||(E.Outer.Get()&&!Live(E.Outer.Get()))||!Live(E.Package.Get())||O->GetClass()!=E.Class.Get()||O->GetOuter()!=E.Outer.Get()||O->GetPackage()!=E.Package.Get()||!O->GetPathName().Equals(E.Path,ESearchCase::CaseSensitive))return Bad(Stage,TEXT("actor_parent_transform_identity_changed"),E.Path,TEXT("class_outer_package_path"),TEXT("captured live held role"),TEXT("changed/unavailable"));}return true;
    }
    bool BindField(FField& E,UClass* Owner,const TCHAR* Name,UClass* ValueClass)
    {
        E.Owner=Owner;E.Name=Name;E.ValueClass=ValueClass;E.Object=FindFProperty<FObjectProperty>(Owner,Name);
        if(!Pin(Owner)||!Pin(ValueClass)||!E.Object||E.Object->GetClass()!=FObjectProperty::StaticClass())return Bad(TEXT("bind_field"),TEXT("actor_parent_transform_field_pending"),TEXT(""),Name,TEXT("existing native object field"),TEXT("missing"),false);
        E.Offset=E.Object->GetOffset_ForInternal();E.Size=E.Object->GetSize();E.Flags=uint64(E.Object->GetPropertyFlags());return FieldCurrent(E,TEXT("bind_field"));
    }
    bool FieldCurrent(const FField& E,const TCHAR* Stage)
    {
        FProperty* Fresh=FindFProperty<FProperty>(E.Owner,E.Name);
        if(Fresh!=E.Object||!Fresh||Fresh->GetClass()!=FObjectProperty::StaticClass())return Bad(Stage,TEXT("actor_parent_transform_field_changed"),TEXT("native_descriptor"),TEXT("field_membership_class"),TEXT("captured exact object field"),TEXT("changed"));
        if(E.Object->GetOwnerStruct()!=E.Owner||E.Object->GetOwnerProperty()!=E.Object||E.Object->ArrayDim!=1||E.Object->GetOffset_ForInternal()!=E.Offset||E.Object->GetSize()!=E.Size||E.Size!=sizeof(TObjectPtr<UObject>)||uint64(E.Object->GetPropertyFlags())!=E.Flags||E.Object->HasSetterOrGetter()||!E.Object->PropertyClass.IsResolved()||E.Object->PropertyClass.Get()!=E.ValueClass)
            return Bad(Stage,TEXT("actor_parent_transform_field_changed"),TEXT("native_descriptor"),TEXT("owner_layout_class_slot"),TEXT("captured native no-accessor layout"),TEXT("changed"));return true;
    }
    bool Ref(const FField& E,const UObject* Owner,UObject*& Value,const TCHAR* Stage)
    {
        if(!FieldCurrent(E,Stage))return false;const auto* Slot=E.Object->GetObjectPtrPropertyValuePtr(E.Object->ContainerPtrToValuePtr<void>(Owner));
        if(!Slot||!Slot->IsResolved())return Bad(Stage,TEXT("actor_parent_transform_unresolved_slot"),TEXT("held_object"),TEXT("native_reference"),TEXT("resolved nullable pointer; no load"),TEXT("unresolved"));Value=Slot->Get();return true;
    }
    bool Asset(UObject* O,const TCHAR* Stage)
    {
        if(!O)return true;if(!Pin(O))return false;const UClass* K=O->GetClass();
        if((K!=UStaticMesh::StaticClass()&&K!=UMaterial::StaticClass()&&K!=UMaterialInstanceConstant::StaticClass())||!O->IsAsset()||O->GetOuter()!=O->GetPackage()||O->GetPackage()->IsDirty())return Bad(Stage,TEXT("actor_parent_transform_asset_pending"),O->GetPathName(),TEXT("source_role"),TEXT("loaded saved-clean exact native mesh/material asset"),TEXT("other/dirty role"),!Bound?false:true);
        if(const auto* Old=Assets.FindByPredicate([&](const FAsset& E){return E.Object.Get()==O;})){if(O->GetFlags()!=Old->Flags||uint32(O->GetPackage()->GetPackageFlags())!=Old->PackageFlags||O->GetPackage()->IsDirty()!=Old->Dirty)return Bad(Stage,TEXT("actor_parent_transform_asset_changed"),O->GetPathName(),TEXT("flags_package_dirty"),TEXT("captured source role"),TEXT("changed"));}
        else{if(Bound||Assets.Num()>=64)return Bad(Stage,TEXT("actor_parent_transform_asset_changed"),O->GetPathName(),TEXT("source_inventory"),TEXT("captured <=64 source assets"),TEXT("new/larger"));FAsset E;E.Object.Reset(O);E.Flags=O->GetFlags();E.PackageFlags=uint32(O->GetPackage()->GetPackageFlags());E.Dirty=O->GetPackage()->IsDirty();Assets.Add(MoveTemp(E));}return true;
    }
    bool Authored(FNode& E,TSharedPtr<FJsonObject>& Out,const TCHAR* Stage)
    {
        auto* R=static_cast<UStaticMeshComponent*>(E.Component.Component.Get());UObject* Mesh=nullptr;if(!Ref(MeshField,R,Mesh,Stage))return false;TStrongObjectPtr<UObject> ReturnedMesh(Mesh);if(Mesh&&!Asset(Mesh,Stage))return false;
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("mesh"),Mesh?Mesh->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Materials;
        if(R->OverrideMaterials.Num()>32)return Bad(Stage,TEXT("actor_parent_transform_material_budget"),E.Path,TEXT("override_materials"),TEXT("<=32"),LexToString(R->OverrideMaterials.Num()),!Bound?false:true);
        for(const auto& Slot:R->OverrideMaterials){if(!Slot.IsResolved())return Bad(Stage,TEXT("actor_parent_transform_unresolved_slot"),E.Path,TEXT("override_material"),TEXT("resolved nullable material"),TEXT("unresolved"));auto* M=Slot.Get();TStrongObjectPtr<UObject> ReturnedMaterial(M);if(M&&!Asset(M,Stage))return false;Materials.Add(MakeShared<FJsonValueString>(M?M->GetPathName():FString()));}
        J->SetArrayField(TEXT("override_materials"),Materials);J->SetNumberField(TEXT("mobility"),int32(R->Mobility));J->SetBoolField(TEXT("registered"),R->IsRegistered());J->SetBoolField(TEXT("active"),R->IsActive());J->SetBoolField(TEXT("visible"),R->IsVisible());J->SetBoolField(TEXT("hidden"),R->bHiddenInGame);J->SetBoolField(TEXT("overlap"),R->GetGenerateOverlapEvents());
        J->SetNumberField(TEXT("authored_collision"),int32(R->BodyInstance.GetCollisionEnabled(false)));J->SetNumberField(TEXT("effective_collision"),int32(R->GetCollisionEnabled()));J->SetStringField(TEXT("collision_profile"),R->GetCollisionProfileName().ToString());J->SetNumberField(TEXT("object_type"),int32(R->GetCollisionObjectType()));
        const FCollisionResponseContainer& Responses=R->GetCollisionResponseToChannels();TArray<TSharedPtr<FJsonValue>> Channels;for(int32 I=0;I<int32(UE_ARRAY_COUNT(Responses.EnumArray));++I)Channels.Add(MakeShared<FJsonValueNumber>(int32(Responses.GetResponse(static_cast<ECollisionChannel>(I)))));J->SetArrayField(TEXT("responses"),Channels);
        if(R->IsSimulatingPhysics()||R->IsWelded()||R->IsDeferringMovementUpdates())return Bad(Stage,TEXT("actor_parent_transform_physics_pending"),E.Path,TEXT("simulating_welded_deferred"),TEXT("false"),TEXT("true"),!Bound?false:true);Out=J;return true;
    }
    bool Check(const TCHAR* Stage)
    {
        if(Rejected)return true; // Unsupported first-scope input: metadata only, no later observer.
        if(!Context(Stage)||!IdentitiesCurrent(Stage))return false;if(!Bound)return true;
        if(Level->Actors.Num()!=LevelActors.Num())return Bad(Stage,TEXT("actor_parent_transform_level_membership"),TEXT("level"),TEXT("actor_slots"),LexToString(LevelActors.Num()),LexToString(Level->Actors.Num()));
        for(int32 I=0;I<LevelActors.Num();++I)if(!Level->Actors[I].IsResolved()||Level->Actors[I].Get()!=LevelActors[I])return Bad(Stage,TEXT("actor_parent_transform_level_membership"),TEXT("level"),TEXT("actor_slot"),LexToString(I),TEXT("changed/unresolved"));
        for(auto& E:Nodes)
        {
            auto* A=E.Actor.Get();auto* R=E.Scene.Scene.Get();UObject* ActualRoot=nullptr;if(!Ref(RootField,A,ActualRoot,Stage))return false;
            if(A->GetClass()!=AStaticMeshActor::StaticClass()||A->GetLevel()!=Level.Get()||A->GetOuter()!=Level.Get()||ActualRoot!=R||A->GetOwner()!=E.Owner||A->IsChildActor()||A->IsTemplate()||A->IsPackageExternal()||A->HasAnyFlags(RF_Transient)||A->bIsEditorOnlyActor||!A->IsEditable()||A->AActor::IsLockLocation()||A->Tags!=E.Tags||A->IsSelectedInEditor()!=E.Selected||!A->GetActorLabel(false).Equals(E.Label,ESearchCase::CaseSensitive)||A->GetActorEnableCollision()!=E.Collision)return Bad(Stage,TEXT("actor_parent_transform_actor_changed"),E.Path,TEXT("actor_role_authored"),TEXT("captured same-level native actor/owner/basic fields"),TEXT("changed"));
            TInlineComponentArray<UActorComponent*> Parts(A);if(Parts.Num()!=1||Parts[0]!=R)return Bad(Stage,TEXT("actor_parent_transform_component_membership"),E.Path,TEXT("components"),TEXT("one captured native static-mesh root"),LexToString(Parts.Num()));
            if(R->GetClass()!=UStaticMeshComponent::StaticClass()||R->GetOwner()!=A||R->GetOuter()!=E.Component.Outer||R->GetWorld()!=E.Component.ComponentWorld.Get()||R->CreationMethod!=E.Component.Method)return Bad(Stage,TEXT("actor_parent_transform_root_changed"),E.Path,TEXT("component_owner_world_method"),TEXT("captured component role"),TEXT("changed"));
            if(R->GetAttachParent()!=E.Scene.Parent||R->GetAttachSocketName()!=E.Scene.Socket||R->IsUsingAbsoluteLocation()!=E.Scene.AbsoluteLocation||R->IsUsingAbsoluteRotation()!=E.Scene.AbsoluteRotation||R->IsUsingAbsoluteScale()!=E.Scene.AbsoluteScale||R->ComponentTags!=E.Scene.Tags)return Bad(Stage,TEXT("actor_parent_transform_attachment_changed"),E.Path,TEXT("parent_socket_absolute_tags"),TEXT("captured authored relation"),TEXT("changed"));
            const auto& Children=R->GetAttachChildren();if(Children.Num()!=E.Scene.Children.Num())return Bad(Stage,TEXT("actor_parent_transform_attachment_changed"),E.Path,TEXT("ordered_child_count"),LexToString(E.Scene.Children.Num()),LexToString(Children.Num()));for(int32 I=0;I<Children.Num();++I)if(!Children[I].IsResolved()||Children[I].Get()!=E.Scene.Children[I])return Bad(Stage,TEXT("actor_parent_transform_attachment_changed"),E.Path,TEXT("ordered_child_slot"),LexToString(I),TEXT("changed/unresolved"));
            const FTransform& Expected=Dispatched?E.Expected:E.Before;const FTransform& Relative=Dispatched&&E.ParentIndex==INDEX_NONE?E.Expected:E.Scene.Relative;
            const FTransform ActualWorld=R->GetComponentTransform(),ActualRelative=R->GetRelativeTransform();
            if(!ActualWorld.Equals(Expected,Dispatched?0.01:0.0)||!ActualRelative.Equals(Relative,E.ParentIndex==INDEX_NONE&&Dispatched?0.01:0.0))return Bad(Stage,TEXT("actor_parent_transform_projection_failed"),E.Path,TEXT("world_relative"),Expected.ToString()+TEXT(" / ")+Relative.ToString(),ActualWorld.ToString()+TEXT(" / ")+ActualRelative.ToString());
            TSharedPtr<FJsonObject> Fresh;if(!Authored(E,Fresh,Stage))return false;if(!ToJson(Fresh).Equals(ToJson(E.Authored),ESearchCase::CaseSensitive))return Bad(Stage,TEXT("actor_parent_transform_authored_changed"),E.Path,TEXT("mesh_material_primitive_fields"),ToJson(E.Authored),ToJson(Fresh));
        }
        for(const auto& E:Others){auto* A=E.Actor.Get();UObject* ActualRoot=nullptr;if(!Ref(RootField,A,ActualRoot,Stage))return false;if(A->GetLevel()!=Level.Get()||ActualRoot!=E.Root.Get()||A->GetOwner()!=E.Owner||A->Tags!=E.Tags||A->IsSelectedInEditor()!=E.Selected||!A->GetActorLabel(false).Equals(E.Label,ESearchCase::CaseSensitive)||!A->GetActorTransform().Equals(E.Transform,0.0))return Bad(Stage,TEXT("actor_parent_transform_unrelated_changed"),E.Path,TEXT("basic_actor_fields"),TEXT("captured unrelated role"),TEXT("changed"));}
        Data->SetBoolField(TEXT("authored_relative_and_attachment_preserved"),true);Data->SetBoolField(TEXT("planned_world_transforms_verified"),true);return Context(Stage);
    }
    bool Bind(const FState& Before)
    {
        Editor=GEditor;World.Reset(Before.World.Get());Level.Reset(Before.Level.Get());Data->SetBoolField(TEXT("parent_actor_transform_contract"),true);Data->SetBoolField(TEXT("explicit_compile_requested"),false);Data->SetBoolField(TEXT("explicit_save_requested"),false);Data->SetNumberField(TEXT("transform_calls"),0);Data->SetBoolField(TEXT("typed_element_true_is_not_transform_predicate"),true);Before.Snapshot->SetObjectField(TEXT("parent_actor_transform_preflight"),Data);
        if(!Context(TEXT("bind_context"))||!Pin(World.Get())||!Pin(Level.Get())||!BindField(RootField,AActor::StaticClass(),TEXT("RootComponent"),USceneComponent::StaticClass())||!BindField(MeshField,UStaticMeshComponent::StaticClass(),TEXT("StaticMesh"),UStaticMesh::StaticClass()))return false;
        AActor* Initial=Before.Target.Get();if(!Live(Initial)||!Pin(Initial))return Bad(TEXT("bind"),TEXT("actor_parent_transform_target_unavailable"),TEXT(""),TEXT("target"),TEXT("live native actor"),TEXT("unavailable"));FNode First;First.Actor.Reset(Initial);Nodes.Add(MoveTemp(First));TSet<AActor*> Seen;Seen.Add(Initial);
        for(int32 I=0;I<Nodes.Num();++I)
        {
            auto* A=Nodes[I].Actor.Get();if(!Check(TEXT("bind_node_context")))return false;
            if(A->GetClass()!=AStaticMeshActor::StaticClass()||A->GetOwner()||A->GetLevel()!=Level.Get()||A->GetOuter()!=Level.Get()||A->IsChildActor()||A->IsTemplate()||A->IsPackageExternal()||A->HasAnyFlags(RF_Transient)||A->bIsEditorOnlyActor||!A->IsEditable()||A->AActor::IsLockLocation())return Bad(TEXT("bind_node"),TEXT("actor_parent_transform_profile_pending"),A->GetPathName(),TEXT("native_actor"),TEXT("unowned same-level exact StaticMeshActor"),TEXT("valid wider role incomplete"),false);
            UObject* RawRoot=nullptr;if(!Ref(RootField,A,RawRoot,TEXT("bind_root")))return false;TStrongObjectPtr<UObject> ReturnedRoot(RawRoot);
            if(!Live(RawRoot)||RawRoot->GetClass()!=UStaticMeshComponent::StaticClass()||!Pin(RawRoot))return Bad(TEXT("bind_root"),TEXT("actor_parent_transform_root_pending"),A->GetPathName(),TEXT("root"),TEXT("live exact StaticMeshComponent"),TEXT("valid wider root incomplete"),false);
            auto* R=static_cast<UStaticMeshComponent*>(RawRoot);TInlineComponentArray<UActorComponent*> Parts(A);if(Parts.Num()!=1||Parts[0]!=R||R->GetOwner()!=A||R->GetOuter()!=A||R->GetWorld()!=World.Get()||R->CreationMethod!=EComponentCreationMethod::Native||!R->IsRegistered())return Bad(TEXT("bind_root"),TEXT("actor_parent_transform_components_pending"),A->GetPathName(),TEXT("root_membership"),TEXT("one owned registered native static-mesh root"),TEXT("valid wider component profile incomplete"),false);
            auto& E=Nodes[I];E.Path=A->GetPathName();E.Label=A->GetActorLabel(false);E.Tags=A->Tags;E.Owner=A->GetOwner();E.Selected=A->IsSelectedInEditor();E.Collision=A->GetActorEnableCollision();E.Before=A->GetActorTransform();E.Expected=E.Before;
            E.Component.Component.Reset(R);E.Component.ComponentWorld.Reset(R->GetWorld());E.Component.Owner=A;E.Component.Class=R->GetClass();E.Component.Outer=R->GetOuter();E.Component.Path=R->GetPathName();E.Component.Method=R->CreationMethod;
            E.Scene.Scene.Reset(R);E.Scene.Owner=A;E.Scene.Class=R->GetClass();E.Scene.Outer=R->GetOuter();E.Scene.Path=R->GetPathName();E.Scene.Parent=R->GetAttachParent();E.Scene.Socket=R->GetAttachSocketName();E.Scene.Relative=R->GetRelativeTransform();E.Scene.World=R->GetComponentTransform();E.Scene.Tags=R->ComponentTags;E.Scene.AbsoluteLocation=R->IsUsingAbsoluteLocation();E.Scene.AbsoluteRotation=R->IsUsingAbsoluteRotation();E.Scene.AbsoluteScale=R->IsUsingAbsoluteScale();
            USceneComponent* ExpectedParent=E.ParentIndex==INDEX_NONE?nullptr:Nodes[E.ParentIndex].Scene.Scene.Get();
            if(E.Scene.Parent!=ExpectedParent||!E.Scene.Socket.IsNone()||E.Scene.AbsoluteLocation||E.Scene.AbsoluteRotation||E.Scene.AbsoluteScale||E.Before.ContainsNaN()||E.Scene.Relative.ContainsNaN()||!Scale(E.Before.GetScale3D())||!Scale(E.Scene.Relative.GetScale3D())||!E.Before.Equals(E.Scene.World,0.0))return Bad(TEXT("bind_scene"),TEXT("actor_parent_transform_relation_pending"),E.Path,TEXT("attachment_scale"),TEXT("closed no-socket/no-absolute rooted tree; finite positive uniform scale"),TEXT("valid wider relation incomplete"),false);
            if(!Authored(E,E.Authored,TEXT("bind_authored")))return false;
            const int32 ChildDepth=E.Depth+1;const FString ParentNodePath=E.Path;TArray<AActor*> Children;A->GetAttachedActors(Children,true,false);for(auto* Child:Children)if(Child&&!Pin(Child))return false;
            for(auto* Child:Children){if(!Live(Child)||Seen.Contains(Child)||Nodes.Num()>=32||ChildDepth>8)return Bad(TEXT("bind_children"),TEXT("actor_parent_transform_tree_pending"),ParentNodePath,TEXT("closed_acyclic_bound"),TEXT("unique tree <=32 actors/depth8"),TEXT("cycle/duplicate/larger/unavailable"),false);Seen.Add(Child);FNode N;N.Actor.Reset(Child);N.ParentIndex=I;N.Depth=ChildDepth;Nodes.Add(MoveTemp(N));}
        }
        if(Nodes.Num()<2)return Bad(TEXT("bind_children"),TEXT("actor_parent_transform_tree_pending"),Nodes[0].Path,TEXT("descendants"),TEXT("nonempty tree for this profile"),TEXT("empty"),false);
        TSet<USceneComponent*> Roots;for(const auto& E:Nodes)Roots.Add(E.Scene.Scene.Get());
        for(auto& E:Nodes){for(const auto& Slot:E.Scene.Scene->GetAttachChildren()){if(!Slot.IsResolved()||!Roots.Contains(Slot.Get()))return Bad(TEXT("bind_children"),TEXT("actor_parent_transform_external_child_pending"),E.Path,TEXT("scene_child"),TEXT("resolved captured actor roots only"),TEXT("external/unresolved"),false);E.Scene.Children.Add(Slot.Get());}for(int32 J=0;J<Nodes.Num();++J)if(Nodes[J].ParentIndex!=INDEX_NONE&&Nodes[Nodes[J].ParentIndex].Scene.Scene.Get()==E.Scene.Scene.Get()&&Algo::Count(E.Scene.Children,Nodes[J].Scene.Scene.Get())!=1)return Bad(TEXT("bind_children"),TEXT("actor_parent_transform_reciprocal_pending"),E.Path,TEXT("child_root_membership"),TEXT("exactly one reciprocal child root"),TEXT("missing/duplicate"),false);}
        if(Level->Actors.Num()>10000)return Bad(TEXT("bind_level"),TEXT("actor_parent_transform_level_budget"),TEXT("level"),TEXT("actor_slots"),TEXT("<=10000"),TEXT("larger"),false);
        for(const auto& Slot:Level->Actors){if(!Slot.IsResolved())return Bad(TEXT("bind_level"),TEXT("actor_parent_transform_level_unresolved"),TEXT("level"),TEXT("actor_slot"),TEXT("resolved nullable pointer"),TEXT("unresolved"));AActor* A=Slot.Get();LevelActors.Add(A);if(!A)continue;if(!Pin(A))return false;if(Seen.Contains(A))continue;FOther E;E.Actor.Reset(A);E.Path=A->GetPathName();E.Owner=A->GetOwner();UObject* RawRoot=nullptr;if(!Ref(RootField,A,RawRoot,TEXT("bind_other_root")))return false;TStrongObjectPtr<UObject> ReturnedRoot(RawRoot);if(RawRoot&&!Pin(RawRoot))return false;E.Root.Reset(static_cast<USceneComponent*>(RawRoot));E.Transform=A->GetActorTransform();E.Tags=A->Tags;E.Label=A->GetActorLabel(false);E.Selected=A->IsSelectedInEditor();Others.Add(MoveTemp(E));}
        for(const auto& E:Nodes)if(Algo::Count(LevelActors,E.Actor.Get())!=1)return Bad(TEXT("bind_level"),TEXT("actor_parent_transform_member_pending"),E.Path,TEXT("level_membership"),TEXT("one captured actor slot"),TEXT("missing/duplicate"),false);
        Bound=true;if(!Check(TEXT("bind_complete")))return false;Subsystem.Reset(GEditor->GetEditorSubsystem<UEditorActorSubsystem>());if(!Check(TEXT("subsystem_return")))return false;Elements.Reset(GEditor->GetEditorSubsystem<UEditorElementSubsystem>());if(!Check(TEXT("elements_return")))return false;if(!Live(Subsystem.Get())||!Live(Elements.Get())||!Pin(Subsystem.Get())||!Pin(Elements.Get()))return Bad(TEXT("bind_subsystems"),TEXT("actor_parent_transform_subsystem_unavailable"),Nodes[0].Path,TEXT("subsystems"),TEXT("held live actor/element subsystems"),TEXT("unavailable"));
        Data->SetNumberField(TEXT("captured_descendant_count"),Nodes.Num()-1);Data->SetBoolField(TEXT("scope_safe"),true);Data->SetStringField(TEXT("remaining"),TEXT("Exact native StaticMeshActor closed tree/own static-mesh root1, positive uniform bounded scales, no sockets/absolute overrides/simulation/weld/deferred movement. Other original-valid native/Blueprint/custom roots, component shapes, physics/scales/hierarchies remain incomplete. Captured fields/assets/roles only; opaque global callback/RAM/physics-cache purity and runtime are unproved."));return Check(TEXT("publish_scope"));
    }
    bool Plan(const FTransform& Desired)
    {
        if(!Check(TEXT("plan_current")))return false;if(!Scale(Desired.GetScale3D()))return Bad(TEXT("plan"),TEXT("actor_parent_transform_requested_scale_pending"),Nodes[0].Path,TEXT("requested_scale"),TEXT("positive uniform bounded scale"),TEXT("valid wider scale incomplete"),false);
        TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=0;I<Nodes.Num();++I){auto& E=Nodes[I];E.Expected=I==0?Desired:E.Scene.Relative*Nodes[E.ParentIndex].Expected;if(E.Expected.ContainsNaN()||!Scale(E.Expected.GetScale3D()))return Bad(TEXT("plan"),TEXT("actor_parent_transform_world_plan_pending"),E.Path,TEXT("planned_world"),TEXT("finite positive uniform bounded projection"),TEXT("outside finite projection"),false);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor_path"),E.Path);J->SetNumberField(TEXT("parent_index"),E.ParentIndex);J->SetStringField(TEXT("world_before"),E.Before.ToString());J->SetStringField(TEXT("planned_world"),E.Expected.ToString());J->SetStringField(TEXT("authored_relative_before"),E.Scene.Relative.ToString());Rows.Add(MakeShared<FJsonValueObject>(J));}Data->SetArrayField(TEXT("hierarchy_plan"),Rows);Planned=true;return Check(TEXT("plan_published"));
    }
    void Count(const TCHAR* Key){double N=0;Data->TryGetNumberField(Key,N);Data->SetNumberField(Key,N+1);}
    bool Execute(const FTransform& Desired,bool NoOp,bool OriginalDirect=false)
    {
        if(!Planned||!Check(TEXT("before_parent_transform")))return false;if(NoOp&&!OriginalDirect){Data->SetBoolField(TEXT("no_op"),true);return true;}
        if(OriginalDirect)
        {
            Count(TEXT("transform_calls"));Data->SetBoolField(TEXT("transform_entered"),true);Data->SetStringField(TEXT("transform_writer"),TEXT("AActor::SetActorTransform original direct"));Dispatched=true;
            const bool Accepted=Nodes[0].Actor->SetActorTransform(Desired);Data->SetBoolField(TEXT("transform_returned"),true);Data->SetBoolField(TEXT("transform_accepted"),Accepted);
            Check(TEXT("after_original_parent_transform"));return Accepted;
        }
        const FScopedTransaction Transaction(FText::FromString(TEXT("Cubeless set_actor_transform")));if(!Check(TEXT("after_transaction_begin")))return false;
        Count(TEXT("world_modify_calls"));World->Modify(false);if(!Check(TEXT("after_world_modify")))return false;Count(TEXT("level_modify_calls"));Level->Modify(false);if(!Check(TEXT("after_level_modify")))return false;
        Count(TEXT("actor_modify_calls"));Nodes[0].Actor->Modify(false);if(!Check(TEXT("after_actor_modify")))return false;Count(TEXT("root_modify_calls"));Nodes[0].Scene.Scene->Modify(false);if(!Check(TEXT("after_root_modify")))return false;
        if(!Live(Subsystem.Get())||!Live(Elements.Get())||GEditor->GetEditorSubsystem<UEditorActorSubsystem>()!=Subsystem.Get()||GEditor->GetEditorSubsystem<UEditorElementSubsystem>()!=Elements.Get()||!Check(TEXT("before_transform_subsystem")))return Bad(TEXT("before_transform_subsystem"),TEXT("actor_parent_transform_subsystem_changed"),Nodes[0].Path,TEXT("subsystem"),TEXT("captured live subsystem"),TEXT("changed"));
        Count(TEXT("transform_calls"));Data->SetBoolField(TEXT("transform_entered"),true);Dispatched=true;
        const bool Accepted=Subsystem->SetActorTransform(Nodes[0].Actor.Get(),Desired);Data->SetBoolField(TEXT("transform_returned"),true);Data->SetBoolField(TEXT("transform_accepted"),Accepted);
        if(!Accepted)return Bad(TEXT("transform_return_false"),TEXT("actor_parent_transform_outcome_unknown"),Nodes[0].Path,TEXT("SDK_return"),TEXT("accepted boundary"),TEXT("false with opaque movement/delegate prefix; no further getter"));
        if(!Check(TEXT("after_parent_transform")))return Accepted;TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& E:Nodes){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("actor_path"),E.Path);J->SetStringField(TEXT("world_after"),E.Scene.Scene->GetComponentTransform().ToString());J->SetStringField(TEXT("relative_after"),E.Scene.Scene->GetRelativeTransform().ToString());Rows.Add(MakeShared<FJsonValueObject>(J));}Data->SetArrayField(TEXT("hierarchy_after"),Rows);
        Count(TEXT("mark_level_dirty_calls"));Level->MarkPackageDirty();Check(TEXT("after_parent_transform_dirty"));return Accepted;
    }
    bool DescendantPath(const FString& Path)const{for(int32 I=1;I<Nodes.Num();++I)if(Nodes[I].Path.Equals(Path,ESearchCase::CaseSensitive))return true;return false;}
};

struct FTargetPointMutation
{
    FFileBackedNewMapWitness NewMapWitness;
    TSharedPtr<FParentActorTransformMutation> ParentTransform;
    TSharedPtr<FBlueprintActorMutation> BlueprintActor;
    TSharedPtr<FParentActorDeleteMutation> ParentDelete;
    TWeakObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TStrongObjectPtr<ULevel> Level;TStrongObjectPtr<AActor> Target;TStrongObjectPtr<USceneComponent> Root;
    struct FIdentity{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;bool Retires=false;};
    struct FComponent{TStrongObjectPtr<UActorComponent> Object;USceneComponent* AttachParent=nullptr;};
    TArray<FIdentity> Identities;TArray<FComponent> Components;FTransform BeforeTransform=FTransform::Identity;bool Stopped=false,Retired=false,TransformDispatched=false;
    // PO transform and PQ leaf-delete opt-ins; the ordinary unattached path is unchanged.
    bool AttachedMode=false,AttachedDeleteMode=false;
    bool OriginalNativeMode=false;UClass* OriginalNativeClass=nullptr; // UC reuses held roles for existing native leaf profiles only.
    TArray<USceneComponent*> ParentChildrenAfterDelete;
    TStrongObjectPtr<AActor> ParentActor;TStrongObjectPtr<USceneComponent> ParentRoot;
    FString ParentPath,ParentRootPath,TargetPath;
    struct FAttachedScene
    {
        TStrongObjectPtr<USceneComponent> Scene;USceneComponent* Parent=nullptr;
        TArray<USceneComponent*> Children;FName Socket;FString Path;FTransform Relative,World;
        bool ParentOwned=false,AbsLocation=false,AbsRotation=false,AbsScale=false;
        EComponentMobility::Type Mobility=EComponentMobility::Static;
    };
    TArray<FAttachedScene> AttachedScenes;
    TArray<UActorComponent*> ParentComponents;TMap<UObject*,EObjectFlags> ParentFlags;
    FTransform ParentWorld=FTransform::Identity,ParentRelative=FTransform::Identity,TargetRelative=FTransform::Identity;
    FString FailureCode,FailureStage;TSharedRef<FJsonObject> Data=MakeShared<FJsonObject>();
    static bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Fail(const TCHAR* Stage,const TCHAR* Code)
    {if(!Stopped){FailureStage=Stage;FailureCode=Code;Data->SetStringField(TEXT("first_failure_stage"),Stage);Data->SetStringField(TEXT("first_failure_code"),Code);}Stopped=true;Data->SetBoolField(TEXT("scope_safe"),false);return false;}
    void Count(const TCHAR* K){double N=0;Data->TryGetNumberField(K,N);Data->SetNumberField(K,N+1);}
    bool Context(const TCHAR* Stage)
    {
        Data->SetObjectField(TEXT("newly_created_map_admission"),NewMapWitness.Receipt);
        if(Stopped){return false;}if(!IsInGameThread()||!GEditor||GEditor!=Editor.Get()||!Live(Editor.Get())||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()
            ||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||!Live(World.Get())||!Live(Level.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown)
        {return Fail(Stage,TEXT("actor_target_point_context_changed"));}
        if(GEditor->GetEditorWorldContext().World()!=World.Get()||World->PersistentLevel.Get()!=Level.Get()||World->GetCurrentLevel()!=Level.Get()||Level->OwningWorld!=World.Get())
        {return Fail(Stage,TEXT("actor_target_point_world_role_changed"));}return true;
    }
    bool Pin(UObject* O,bool Retires)
    {
        if(!Live(O)||!Live(O->GetClass())||(O->GetOuter()&&!Live(O->GetOuter()))||!Live(O->GetPackage())){return Fail(TEXT("capture_roles"),TEXT("actor_target_point_identity_invalid"));}
        FIdentity I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Retires=Retires;Identities.Add(MoveTemp(I));return true;
    }

    static bool OrdinaryScale(const FVector& S)
    {
        // Additional finite attached-profile boundary, not a generic UE limitation.
        return !S.ContainsNaN()&&S.X>0.0&&S.X<=1.0e12&&S.X==S.Y&&S.Y==S.Z;
    }
    static TSharedRef<FJsonObject> TransformState(const FTransform& T)
    {
        auto J=MakeShared<FJsonObject>();const auto L=T.GetLocation(),S=T.GetScale3D();const auto Q=T.GetRotation();
        J->SetArrayField(TEXT("translation"),{MakeShared<FJsonValueNumber>(L.X),MakeShared<FJsonValueNumber>(L.Y),MakeShared<FJsonValueNumber>(L.Z)});
        J->SetArrayField(TEXT("rotation_quaternion"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});
        J->SetArrayField(TEXT("scale"),{MakeShared<FJsonValueNumber>(S.X),MakeShared<FJsonValueNumber>(S.Y),MakeShared<FJsonValueNumber>(S.Z)});return J;
    }
    bool AttachmentFail(const TCHAR* Stage,const TCHAR* Code,const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual,bool Observed=true)
    {
        if(!Data->HasField(TEXT("first_attachment_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);
            J->SetBoolField(TEXT("operands_observed"),Observed);
            if(Observed){J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);}
            else{J->SetField(TEXT("expected"),MakeShared<FJsonValueNull>());J->SetField(TEXT("actual"),MakeShared<FJsonValueNull>());}
            Data->SetObjectField(TEXT("first_attachment_mismatch"),J);
        }
        return Fail(Stage,Code);
    }
    bool BindAttachment()
    {
        // Every returned attachment role is held before another receiver is read.
        ParentRoot.Reset(Root->GetAttachParent());if(!Context(TEXT("capture_attach_parent")))return false;
        if(!Live(ParentRoot.Get())||ParentRoot->GetClass()!=USceneComponent::StaticClass())
            return AttachmentFail(TEXT("capture_attach_parent"),TEXT("actor_attached_parent_root_unsupported"),TargetPath,TEXT("root.attach_parent"),TEXT("exact native SceneComponent"),TEXT("unavailable/other"),false);
        ParentActor.Reset(ParentRoot->GetOwner());if(!Context(TEXT("capture_attach_owner")))return false;
        if(!Live(ParentActor.Get())||ParentActor->GetClass()!=ATargetPoint::StaticClass()||ParentActor.Get()==Target.Get()
            ||ParentActor->GetLevel()!=Level.Get()||ParentActor->GetOuter()!=Level.Get()||ParentActor->GetRootComponent()!=ParentRoot.Get())
            return AttachmentFail(TEXT("capture_attach_owner"),TEXT("actor_attached_parent_unsupported"),TargetPath,TEXT("attach_parent.owner"),TEXT("distinct exact TargetPoint in same level"),TEXT("other/unavailable"),false);
        ParentPath=ParentActor->GetPathName();ParentRootPath=ParentRoot->GetPathName();
        if(!Pin(ParentActor.Get(),false)||!Pin(ParentRoot.Get(),false))return false;
        ParentFlags.Add(ParentActor.Get(),ParentActor->GetFlags());ParentFlags.Add(ParentRoot.Get(),ParentRoot->GetFlags());
        if(ParentActor->IsTemplate()||ParentActor->bIsEditorOnlyActor||ParentActor->IsPackageExternal()
            ||ParentActor->HasAnyFlags(RF_Transient)||!ParentActor->IsEditable()||ParentActor->IsChildActor()||ParentActor->AActor::IsLockLocation()
            ||ParentRoot->GetAttachParent()||!ParentRoot->GetAttachSocketName().IsNone()
            ||Root->IsUsingAbsoluteLocation()||Root->IsUsingAbsoluteRotation()||Root->IsUsingAbsoluteScale()||!Root->GetAttachSocketName().IsNone()
            ||ParentRoot->IsUsingAbsoluteLocation()||ParentRoot->IsUsingAbsoluteRotation()||ParentRoot->IsUsingAbsoluteScale())
            return AttachmentFail(TEXT("capture_attachment"),TEXT("actor_attached_profile_unsupported"),TargetPath,TEXT("attachment_profile"),TEXT("unattached ordinary parent; no socket/absolute flags"),TEXT("outside finite profile"),false);
        ParentWorld=ParentRoot->GetComponentTransform();ParentRelative=ParentRoot->GetRelativeTransform();TargetRelative=Root->GetRelativeTransform();
        if(ParentWorld.ContainsNaN()||ParentRelative.ContainsNaN()||TargetRelative.ContainsNaN()
            ||!OrdinaryScale(ParentWorld.GetScale3D())||!OrdinaryScale(BeforeTransform.GetScale3D()))
            return AttachmentFail(TEXT("capture_attachment"),TEXT("actor_attached_scale_profile_unsupported"),TargetPath,TEXT("world_scale"),TEXT("positive uniform bounded scale"),TEXT("outside tested attached profile"),false);
        TArray<AActor*> Children;ParentActor->GetAttachedActors(Children);
        if(Children.Num()!=1||Children[0]!=Target.Get())
            return AttachmentFail(TEXT("capture_attachment"),TEXT("actor_attached_parent_children_unsupported"),ParentPath,TEXT("attached_actor_count"),TEXT("one selected TargetPoint"),LexToString(Children.Num()));
        Children.Reset();Target->GetAttachedActors(Children);
        if(!Children.IsEmpty())return AttachmentFail(TEXT("capture_attachment"),TEXT("actor_attached_target_children_unsupported"),TargetPath,TEXT("attached_actor_count"),TEXT("0"),LexToString(Children.Num()));
        TInlineComponentArray<UActorComponent*> PC(ParentActor.Get());
        if(PC.Num()>256)return Fail(TEXT("capture_attachment"),TEXT("actor_attached_parent_component_budget"));
        for(UActorComponent* C:PC)
        {
            if(!Live(C)||C->GetOwner()!=ParentActor.Get()||!C->IsIn(ParentActor.Get())||!Live(C->GetClass())
                ||!C->GetClass()->HasAnyClassFlags(CLASS_Native)||C->GetClass()->ClassGeneratedBy
                ||!C->GetClass()->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)||!Pin(C,false))
                return Fail(TEXT("capture_attachment"),TEXT("actor_attached_parent_component_unsupported"));
            ParentComponents.Add(C);ParentFlags.Add(C,C->GetFlags());
        }
        auto CaptureScene=[&](UActorComponent* C,bool IsParent)->bool
        {
            auto* S=Cast<USceneComponent>(C);if(!S)return true;
            FAttachedScene Role;Role.Scene.Reset(S);Role.Parent=S->GetAttachParent();Role.Children=S->GetAttachChildren();
            Role.Path=S->GetPathName();Role.Socket=S->GetAttachSocketName();Role.Relative=S->GetRelativeTransform();Role.World=S->GetComponentTransform();Role.ParentOwned=IsParent;
            Role.AbsLocation=S->IsUsingAbsoluteLocation();Role.AbsRotation=S->IsUsingAbsoluteRotation();Role.AbsScale=S->IsUsingAbsoluteScale();Role.Mobility=S->Mobility;
            if(Role.Relative.ContainsNaN()||Role.World.ContainsNaN())return false;
            if(IsParent)
            {
                if(Role.Parent&&!ParentComponents.Contains(Role.Parent))return false;
                for(USceneComponent* Child:Role.Children)if(Child!=Root.Get()&&!ParentComponents.Contains(Child))return false;
            }
            AttachedScenes.Add(MoveTemp(Role));return true;
        };
        for(auto* C:ParentComponents)if(!CaptureScene(C,true))return Fail(TEXT("capture_attachment"),TEXT("actor_attached_parent_components_unsupported"));
        TInlineComponentArray<UActorComponent*> TC(Target.Get());for(auto* C:TC)if(!CaptureScene(C,false))return Fail(TEXT("capture_attachment"),TEXT("actor_attached_target_components_unsupported"));
        if(AttachedDeleteMode)
        {
            // SDK SetOwner(nullptr) would mutate another ownership relation, outside this attached-only slice.
            if(Target->GetOwner())return AttachmentFail(TEXT("capture_delete_attachment"),TEXT("actor_attached_delete_owner_unsupported"),TargetPath,TEXT("actor_owner"),TEXT("None"),TEXT("additional ownership relation"),false);
            int32 SelectedMembership=0;
            for(const auto& Role:AttachedScenes)if(Role.ParentOwned)
            {
                for(USceneComponent* C:Role.Children)if(C==Root.Get())
                {
                    if(Role.Scene.Get()!=ParentRoot.Get())return AttachmentFail(TEXT("capture_delete_attachment"),TEXT("actor_attached_delete_membership_unsupported"),Role.Path,TEXT("AttachChildren"),TEXT("selected root only under captured parent root"),TEXT("different"),false);
                    ++SelectedMembership;
                }
                if(Role.Scene.Get()==ParentRoot.Get())ParentChildrenAfterDelete=Role.Children;
            }
            if(SelectedMembership!=1||ParentChildrenAfterDelete.RemoveSingle(Root.Get())!=1)return AttachmentFail(TEXT("capture_delete_attachment"),TEXT("actor_attached_delete_membership_unsupported"),ParentRootPath,TEXT("selected_child_membership"),TEXT("1"),LexToString(SelectedMembership));
            Data->SetStringField(TEXT("planned_parent_attach_children_address_order"),PointerList(ParentChildrenAfterDelete));
            Data->SetBoolField(TEXT("attached_target_point_delete"),true);Data->SetNumberField(TEXT("destroy_calls"),0);
            Data->SetBoolField(TEXT("SDK_internal_callback_atomicity_proven"),false);Data->SetBoolField(TEXT("retired_target_getter_dispatched"),false);
        }
        auto Before=MakeShared<FJsonObject>();Before->SetStringField(TEXT("parent_actor"),ParentPath);Before->SetStringField(TEXT("parent_root"),ParentRootPath);
        Before->SetStringField(TEXT("target_actor"),TargetPath);Before->SetObjectField(TEXT("parent_world"),TransformState(ParentWorld));Before->SetObjectField(TEXT("parent_relative"),TransformState(ParentRelative));
        Before->SetObjectField(TEXT("target_world"),TransformState(BeforeTransform));Before->SetObjectField(TEXT("target_relative"),TransformState(TargetRelative));
        TArray<TSharedPtr<FJsonValue>> Paths;for(USceneComponent* C:ParentRoot->GetAttachChildren())Paths.Add(MakeShared<FJsonValueString>(C->GetPathName()));Before->SetArrayField(TEXT("parent_root_attach_children"),Paths);
        Data->SetObjectField(TEXT("attachment_before"),Before);Data->SetBoolField(TEXT("attached_target_point_world_transform"),!AttachedDeleteMode);
        Data->SetStringField(TEXT("attached_profile_remaining"),TEXT("Only exact native TargetPoint parent/target, same saved persistent level, parent unattached with selected target as sole attached actor, target child0, socketNone and absoluteFalse roots; positive uniform bounded world scales. Other valid native parents, siblings, nonuniform/negative/zero scales, sockets, absolute flags, Blueprint/custom callbacks remain incomplete. Engine computes world-to-relative; no reparent/delete/runtime expansion."));
        if(AttachedDeleteMode)Data->SetStringField(TEXT("attached_profile_remaining"),TEXT("PQ exact native TargetPoint parent/leaf target in the existing PO finite profile: same saved persistent level, parent unattached, sole attached actor target, target child0/OwnerNone, no root socket/absolute flags, positive uniform bounded before scales. Delete retires selected roles and removes exactly its root pointer from the parent's captured ordered children. Only parent identities/basic actor observations/components/flags/captured scene fields are protected; unobserved properties, other parents/ownership/subtrees/scales, runtime, internal delegate atomicity and GC completion remain incomplete. No explicit compile/save or automatic retry."));
        return true;
    }
    static FString PointerList(const TArray<USceneComponent*>& Values)
    {
        FString S;for(auto* V:Values){if(!S.IsEmpty())S+=TEXT(",");S+=LexToString(reinterpret_cast<UPTRINT>(V));}return S;
    }
    FString HeldPath(UObject* O)const{for(const auto& I:Identities)if(I.Object.Get()==O)return I.Path;return ParentPath;}
    bool AttachmentCurrent(const TCHAR* Stage)
    {
        if(!AttachedMode)return true;
        // Identity roles have already been closed by Check before any receiver use.
        if((!Retired&&(Target->GetRootComponent()!=Root.Get()||Root->GetAttachParent()!=ParentRoot.Get()))
            ||ParentActor->GetRootComponent()!=ParentRoot.Get()||ParentRoot->GetOwner()!=ParentActor.Get()
            ||ParentActor->GetLevel()!=Level.Get()||ParentActor->GetOuter()!=Level.Get()||ParentRoot->GetAttachParent())
            return AttachmentFail(Stage,TEXT("actor_attached_binding_changed"),TargetPath,TEXT("attachment_roles"),TEXT("captured exact parent/root/level"),TEXT("different"),false);
        for(const auto& Pair:ParentFlags)if(Pair.Key->GetFlags()!=Pair.Value)
            return AttachmentFail(Stage,TEXT("actor_attached_parent_flags_changed"),HeldPath(Pair.Key),TEXT("captured_source.object_flags"),LexToString(uint32(Pair.Value)),LexToString(uint32(Pair.Key->GetFlags())));
        if(ParentActor->IsTemplate()||ParentActor->bIsEditorOnlyActor||ParentActor->IsPackageExternal()
            ||ParentActor->HasAnyFlags(RF_Transient)||!ParentActor->IsEditable()||ParentActor->IsChildActor()||ParentActor->AActor::IsLockLocation())
            return AttachmentFail(Stage,TEXT("actor_attached_parent_eligibility_changed"),ParentPath,TEXT("parent_mutation_eligibility"),TEXT("captured ordinary editable native parent"),TEXT("different"),false);
        TInlineComponentArray<UActorComponent*> PC(ParentActor.Get());
        if(PC.Num()!=ParentComponents.Num())return AttachmentFail(Stage,TEXT("actor_attached_parent_components_changed"),ParentPath,TEXT("owned_component_count"),LexToString(ParentComponents.Num()),LexToString(PC.Num()));
        for(auto* C:ParentComponents)if(!PC.Contains(C)||C->GetOwner()!=ParentActor.Get())return AttachmentFail(Stage,TEXT("actor_attached_parent_components_changed"),ParentPath,TEXT("owned_component_membership"),TEXT("captured"),TEXT("different"),false);
        const FTransform CurrentParent=ParentRoot->GetComponentTransform(),CurrentRelative=ParentRoot->GetRelativeTransform();
        if(!CurrentParent.Equals(ParentWorld,0.0))return AttachmentFail(Stage,TEXT("actor_attached_parent_transform_changed"),ParentPath,TEXT("world_transform"),ParentWorld.ToString(),CurrentParent.ToString());
        if(!CurrentRelative.Equals(ParentRelative,0.0))return AttachmentFail(Stage,TEXT("actor_attached_parent_transform_changed"),ParentPath,TEXT("relative_transform"),ParentRelative.ToString(),CurrentRelative.ToString());
        if(!Retired)
        {
            TArray<AActor*> Actors;ParentActor->GetAttachedActors(Actors);
            if(Actors.Num()!=1||Actors[0]!=Target.Get())return AttachmentFail(Stage,TEXT("actor_attached_actor_membership_changed"),ParentPath,TEXT("attached_actor_count"),TEXT("1 exact selected target"),LexToString(Actors.Num()));
            Actors.Reset();Target->GetAttachedActors(Actors);if(!Actors.IsEmpty())return AttachmentFail(Stage,TEXT("actor_attached_actor_membership_changed"),TargetPath,TEXT("attached_actor_count"),TEXT("0"),LexToString(Actors.Num()));
            if(AttachedDeleteMode&&Target->GetOwner())return AttachmentFail(Stage,TEXT("actor_attached_delete_owner_changed"),TargetPath,TEXT("actor_owner"),TEXT("None"),TEXT("different"),false);
        }
        // After accepted destroy, do not call GetAttachedActors: it can dereference a stale target child.
        // The surviving component pointer inventories below establish exact removal without target getters.
        for(const auto& Role:AttachedScenes)
        {
            if(Retired&&!Role.ParentOwned)continue;
            USceneComponent* C=Role.Scene.Get();
            if(!Live(C))return AttachmentFail(Stage,TEXT("actor_attached_scene_relation_changed"),Role.Path,TEXT("live"),TEXT("true"),TEXT("false"));
            const auto CurrentComponentParent=C->GetAttachParent();TArray<USceneComponent*> CurrentChildren;
            for(const TObjectPtr<USceneComponent>& Slot:C->GetAttachChildren())
            {
                if(AttachedDeleteMode&&!Slot.IsResolved())return AttachmentFail(Stage,TEXT("actor_attached_child_slot_unresolved"),Role.Path,TEXT("AttachChildren.resolved"),TEXT("resolved held pointers"),TEXT("unresolved"),false);
                CurrentChildren.Add(Slot.Get());
            }
            const TArray<USceneComponent*>& ExpectedChildren=Retired&&C==ParentRoot.Get()?ParentChildrenAfterDelete:Role.Children;
            if(CurrentComponentParent!=Role.Parent)return AttachmentFail(Stage,TEXT("actor_attached_scene_relation_changed"),Role.Path,TEXT("AttachParent.address"),LexToString(reinterpret_cast<UPTRINT>(Role.Parent)),LexToString(reinterpret_cast<UPTRINT>(CurrentComponentParent)));
            if(CurrentChildren!=ExpectedChildren)return AttachmentFail(Stage,TEXT("actor_attached_scene_relation_changed"),Role.Path,TEXT("AttachChildren.address_order"),PointerList(ExpectedChildren),PointerList(CurrentChildren));
            const FName Socket=C->GetAttachSocketName();
            if(Socket!=Role.Socket)return AttachmentFail(Stage,TEXT("actor_attached_scene_relation_changed"),Role.Path,TEXT("AttachSocketName"),Role.Socket.ToString(),Socket.ToString());
            const int32 ExpectedAbs=(Role.AbsLocation?1:0)|(Role.AbsRotation?2:0)|(Role.AbsScale?4:0);
            const int32 ActualAbs=(C->IsUsingAbsoluteLocation()?1:0)|(C->IsUsingAbsoluteRotation()?2:0)|(C->IsUsingAbsoluteScale()?4:0);
            if(ExpectedAbs!=ActualAbs)return AttachmentFail(Stage,TEXT("actor_attached_scene_relation_changed"),Role.Path,TEXT("absolute_location_rotation_scale_bits"),LexToString(ExpectedAbs),LexToString(ActualAbs));
            if(C->Mobility!=Role.Mobility)return AttachmentFail(Stage,TEXT("actor_attached_scene_relation_changed"),Role.Path,TEXT("Mobility"),LexToString(int32(Role.Mobility)),LexToString(int32(C->Mobility)));
            const FTransform Relative=C->GetRelativeTransform(),WorldNow=C->GetComponentTransform();
            if(Relative.ContainsNaN()||WorldNow.ContainsNaN())return AttachmentFail(Stage,TEXT("actor_attached_nonfinite_current"),Role.Path,TEXT("scene_transform_finite"),TEXT("true"),TEXT("false"));
            if(C!=Root.Get()&&!Relative.Equals(Role.Relative,0.0))
                return AttachmentFail(Stage,TEXT("actor_attached_nonselected_relative_changed"),Role.Path,TEXT("scene.relative_transform"),Role.Relative.ToString(),Relative.ToString());
            if(Role.ParentOwned&&!WorldNow.Equals(Role.World,0.0))
                return AttachmentFail(Stage,TEXT("actor_attached_parent_component_world_changed"),Role.Path,TEXT("scene.world_transform"),Role.World.ToString(),WorldNow.ToString());
        }
        if(Retired)
        {
            auto After=MakeShared<FJsonObject>();After->SetObjectField(TEXT("parent_world"),TransformState(CurrentParent));After->SetObjectField(TEXT("parent_relative"),TransformState(CurrentRelative));After->SetStringField(TEXT("parent_root_attach_children_address_order"),PointerList(ParentChildrenAfterDelete));
            Data->SetObjectField(TEXT("attachment_after"),After);Data->SetBoolField(TEXT("parent_selected_child_removed"),true);Data->SetBoolField(TEXT("attachment_relations_preserved"),true);Data->SetBoolField(TEXT("post_destroy_attached_actor_enumeration_dispatched"),false);return true;
        }
        const FTransform Relative=Root->GetRelativeTransform(),WorldNow=Root->GetComponentTransform();
        // Forward composition uses public FTransform math only; no handwritten
        // inverse/world-to-relative algorithm and no private SceneComponent call.
        const FTransform Composed=Relative*ParentWorld;
        if(!Composed.Equals(WorldNow,0.01))
            return AttachmentFail(Stage,TEXT("actor_attached_transform_relation_changed"),TargetPath,TEXT("relative_parent_world_composition"),Composed.ToString(),WorldNow.ToString());
        if(!TransformDispatched&&!Relative.Equals(TargetRelative,0.0))
            return AttachmentFail(Stage,TEXT("actor_attached_relative_changed_before_dispatch"),TargetPath,TEXT("relative_transform"),TargetRelative.ToString(),Relative.ToString());
        auto After=MakeShared<FJsonObject>();After->SetObjectField(TEXT("parent_world"),TransformState(CurrentParent));After->SetObjectField(TEXT("parent_relative"),TransformState(CurrentRelative));
        After->SetObjectField(TEXT("target_world"),TransformState(WorldNow));After->SetObjectField(TEXT("target_relative"),TransformState(Relative));
        Data->SetObjectField(TEXT("attachment_after"),After);Data->SetBoolField(TEXT("attachment_relations_preserved"),true);return true;
    }

    bool Bind(const FState& Before,bool Attached=false,bool AttachedDelete=false,bool OriginalNative=false)
    {
        Editor=GEditor;World.Reset(Before.World.Get());Level.Reset(Before.Level.Get());if(!Context(TEXT("capture_context"))){return false;}
        AttachedMode=Attached;AttachedDeleteMode=Attached&&AttachedDelete;OriginalNativeMode=OriginalNative;
        if(AttachedMode)
        {
            Target.Reset(Before.Target.Get());if(!Live(Target.Get()))return Fail(TEXT("capture_roles"),TEXT("actor_attached_target_unavailable"));
            Root.Reset(Target->GetRootComponent());if(!Context(TEXT("capture_root_return")))return false;
        }
        AActor* A=Before.Target.Get();if(!Live(A)||(OriginalNativeMode?!FParentActorDeleteMutation::Plain(A->GetClass()):A->GetClass()!=ATargetPoint::StaticClass())||A->GetLevel()!=Level.Get()||A->GetOuter()!=Level.Get()||!Live(A->GetRootComponent())||(!OriginalNativeMode&&A->GetRootComponent()->GetClass()!=USceneComponent::StaticClass())
            ||!A->GetActorTransform().Equals(Before.TargetTransform,0.0)){return Fail(TEXT("capture_roles"),TEXT("actor_target_point_current_role_invalid"));}
        Target.Reset(A);Root.Reset(A->GetRootComponent());BeforeTransform=Before.TargetTransform;OriginalNativeClass=OriginalNativeMode?A->GetClass():nullptr;
        if(AttachedMode){TargetPath=A->GetPathName();if(!BindAttachment())return false;}
        if(!Pin(A,true)||!Pin(Root.Get(),true)||!Pin(A->GetClass(),false)||!Pin(World.Get(),false)||!Pin(Level.Get(),false)||!Pin(Level->GetPackage(),false)){return false;}
        TInlineComponentArray<UActorComponent*> Current(A);if(Current.Num()>256){return Fail(TEXT("capture_components"),TEXT("actor_target_point_component_budget"));}
        for(UActorComponent* C:Current)
        {
            if(!Live(C)||C->GetOwner()!=A||!C->IsIn(A)||!Live(C->GetClass())||!C->GetClass()->HasAnyClassFlags(CLASS_Native)||C->GetClass()->ClassGeneratedBy||!C->GetClass()->GetPackage()->GetName().Equals(TEXT("/Script/Engine"),ESearchCase::CaseSensitive)||!Pin(C,true))
            {return Fail(TEXT("capture_components"),TEXT("actor_target_point_component_role_invalid"));}
            FComponent R;R.Object.Reset(C);if(auto* Scene=Cast<USceneComponent>(C)){R.AttachParent=Scene->GetAttachParent();}Components.Add(MoveTemp(R));
        }
        for(const auto& C:Components){if(C.AttachParent&&!Current.Contains(C.AttachParent)&&!(AttachedMode&&C.Object.Get()==Root.Get()&&C.AttachParent==ParentRoot.Get())){return Fail(TEXT("capture_components"),TEXT("actor_target_point_component_parent_outside_target"));}}
        Data->SetStringField(TEXT("scope"),TEXT("Exact native TargetPoint transform/delete; current Editor/world/level/actor/root/native components and basic unselected actor observations. No spawn type expansion, explicit compile/save, arbitrary component fields/global callback or runtime proof."));
        Data->SetStringField(TEXT("target_path"),A->GetPathName());Data->SetNumberField(TEXT("component_count"),Components.Num());Data->SetBoolField(TEXT("scope_safe"),true);Data->SetBoolField(TEXT("retired_after_accepted_destroy"),false);return Check(TEXT("capture_complete"));
    }
    bool Check(const TCHAR* Stage)
    {
        if(ParentTransform){const bool OK=ParentTransform->Check(Stage);if(!OK){FailureCode=ParentTransform->FailureCode;FailureStage=ParentTransform->FailureStage;Stopped=true;}return OK;}
        if(ParentDelete){const bool OK=ParentDelete->Check(Stage);if(!OK){FailureCode=ParentDelete->FailureCode;FailureStage=ParentDelete->FailureStage;Stopped=true;}return OK;}
        if(BlueprintActor){const bool OK=BlueprintActor->Check(Stage);if(!OK){FailureCode=BlueprintActor->FailureCode;FailureStage=BlueprintActor->FailureStage;Stopped=true;}return OK;}
        if(!Context(Stage)){return false;}
        for(const auto& I:Identities)
        {
            if(Retired&&I.Retires){continue;}UObject* O=I.Object.Get();if(!Live(O)||!Live(I.Class)||(I.Outer&&!Live(I.Outer))||!Live(I.Package)||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive))
            {if(AttachedMode)return AttachmentFail(Stage,TEXT("actor_target_point_identity_changed"),I.Path,TEXT("held_class_outer_package_path"),TEXT("captured role"),TEXT("unavailable/different"),false);return Fail(Stage,TEXT("actor_target_point_identity_changed"));}
        }
        // Same native eligibility as Prepare, repeated after each returned callback.
        // Exact ATargetPoint uses AActor's public editor-only bit; no virtual IsEditorOnly call.
        if(World->GetLevels().Num()!=1||World->IsPartitionedWorld()||Level->IsUsingExternalActors()||FLevelUtils::IsLevelLocked(Level.Get())
            ||!NewMapWitness.Current(World.Get(),Level.Get()))
        {return Fail(Stage,TEXT("actor_target_point_level_eligibility_changed"));}
        if(Retired){return AttachedDeleteMode?AttachmentCurrent(Stage):true;}
        if(Target->GetClass()!=(OriginalNativeMode?OriginalNativeClass:ATargetPoint::StaticClass())||Target->GetLevel()!=Level.Get()||Target->GetRootComponent()!=Root.Get()||Root->GetAttachParent()!=(AttachedMode?ParentRoot.Get():nullptr))
        {return Fail(Stage,TEXT("actor_target_point_actor_role_changed"));}
        TInlineComponentArray<UActorComponent*> Current(Target.Get());if(Current.Num()!=Components.Num()){return Fail(Stage,TEXT("actor_target_point_component_set_changed"));}
        for(const auto& C:Components){if(!Current.Contains(C.Object.Get())||C.Object->GetOwner()!=Target.Get()){return Fail(Stage,TEXT("actor_target_point_component_owner_changed"));}if(auto* Scene=Cast<USceneComponent>(C.Object.Get())){if(Scene->GetAttachParent()!=C.AttachParent){return Fail(Stage,TEXT("actor_target_point_component_parent_changed"));}}}
        for(const auto& C:Components){if(auto* Scene=Cast<USceneComponent>(C.Object.Get())){const auto& Children=Scene->GetAttachChildren();if(Children.Num()>Components.Num()){return Fail(Stage,TEXT("actor_target_point_attached_component_changed"));}for(USceneComponent* Child:Children){if(!Child||!Current.Contains(Child)){return Fail(Stage,TEXT("actor_target_point_attached_component_changed"));}}}}
        if(Target->IsTemplate()||Target->bIsEditorOnlyActor||Target->IsPackageExternal()||Target->HasAnyFlags(RF_Transient)||!Target->IsEditable()||Target->IsChildActor()||Target->AActor::IsLockLocation())
        {return Fail(Stage,TEXT("actor_target_point_actor_eligibility_changed"));}
        if(!AttachmentCurrent(Stage))return false;
        if(!TransformDispatched&&!Target->GetActorTransform().Equals(BeforeTransform,0.0)){return Fail(Stage,TEXT("actor_target_point_transform_changed_before_dispatch"));}return true;
    }
};
namespace
{
bool TargetPointMutationRunning=false;
TArray<TSharedPtr<FJsonValue>> Triple(double X, double Y, double Z)
{
    return { MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y), MakeShared<FJsonValueNumber>(Z) };
}

TSharedPtr<FJsonObject> SnapshotActor(AActor* Value)
{
    auto Result = MakeShared<FJsonObject>();
    const FVector Location = Value->GetActorLocation();
    const FRotator Rotation = Value->GetActorRotation();
    const FVector Scale = Value->GetActorScale3D();
    Result->SetStringField(TEXT("name"), Value->GetName());
    Result->SetStringField(TEXT("class"), Value->GetClass()->GetName());
    Result->SetArrayField(TEXT("location"), Triple(Location.X, Location.Y, Location.Z));
    Result->SetArrayField(TEXT("rotation"), Triple(Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
    Result->SetArrayField(TEXT("scale"), Triple(Scale.X, Scale.Y, Scale.Z));
    Result->SetStringField(TEXT("actor_path"), Value->GetPathName());
    Result->SetStringField(TEXT("class_path"), Value->GetClass()->GetPathName());
    // false avoids manufacturing a missing editor label during a read.
    Result->SetStringField(TEXT("label"), Value->GetActorLabel(false));
    Result->SetStringField(TEXT("level_path"), Value->GetLevel()->GetOutermost()->GetName());
    Result->SetStringField(TEXT("package_path"), Value->GetPackage()->GetName());
    Result->SetBoolField(TEXT("external_package"), Value->IsPackageExternal());
    Result->SetBoolField(TEXT("selected"), Value->IsSelectedInEditor());
    Result->SetBoolField(TEXT("editor_only"), Value->IsEditorOnly());
    Result->SetBoolField(TEXT("has_root_component"), Value->GetRootComponent() != nullptr);
    Result->SetBoolField(TEXT("transform_finite"), !Value->GetActorTransform().ContainsNaN());
    return Result;
}

bool ParseTriple(const FString& Json, FVector& Out, bool& bPresent, FString& Error)
{
    bPresent = false;
    if (Json.Len() > 1024) { Error = TEXT("Transform JSON exceeds 1024 UTF-16 code units."); return false; }
    TArray<TSharedPtr<FJsonValue>> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Json + TEXT("]")), Root) || Root.Num() != 1 || !Root[0].IsValid())
    {
        Error = TEXT("Transform input must be one JSON null or an array of exactly three finite numbers."); return false;
    }
    if (Root[0]->Type == EJson::Null) { return true; }
    if (Root[0]->Type != EJson::Array || Root[0]->AsArray().Num() != 3) { Error = TEXT("Transform input must have exactly three numbers."); return false; }
    double Values[3];
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const auto& Item = Root[0]->AsArray()[Index];
        if (!Item.IsValid() || Item->Type != EJson::Number || !FMath::IsFinite(Item->AsNumber())) { Error = TEXT("Transform components must be finite JSON numbers, not booleans."); return false; }
        Values[Index] = Item->AsNumber();
        // Actor component/rendering APIs include float-backed state. Extremely
        // large finite doubles are not safe merely because FTransform is LWC.
        if (FMath::Abs(Values[Index]) > 1.0e12) { Error = TEXT("Transform component magnitude exceeds the supported 1e12 bound."); return false; }
    }
    Out = FVector(Values[0], Values[1], Values[2]);
    bPresent = true;
    return true;
}

// UC mirrors the original typed public array parser. JSON null means the
// optional Python field was omitted. Trailing array cells are not interpreted.
bool ParseOriginalTriple(const FString& Json,FVector& Out,bool& bPresent,FString& Error)
{
    bPresent=false;
    if(Json.Len()>16*1024*1024){Error=TEXT("Original transform carrier exceeds the existing bounded request size; wider valid inputs remain incomplete.");return false;}
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Json+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()){Error=TEXT("Original transform carrier must contain one JSON value.");return false;}
    if(Values[0]->Type==EJson::Null)return true;
    if(Values[0]->Type!=EJson::Array){Error=TEXT("Original typed transform input must be an array or null; malformed direct-native coercions remain incomplete.");return false;}
    bPresent=true;Out=FVector::ZeroVector;const auto& Cells=Values[0]->AsArray();
    if(Cells.Num()<3)return true;
    double Converted[3];
    for(int32 Index=0;Index<3;++Index)
    {
        if(!Cells[Index].IsValid()||Cells[Index]->Type!=EJson::Number){Error=TEXT("Original typed transform components must be numbers; other native AsNumber coercions remain incomplete.");return false;}
        const double Value=Cells[Index]->AsNumber();
        if(!FMath::IsFinite(Value)||FMath::Abs(Value)>1.0e12){Error=TEXT("Original component is outside the retained finite current safety bound; wider values remain incomplete.");return false;}
        const float Narrowed=static_cast<float>(Value);
        if(!FMath::IsFinite(Narrowed)){Error=TEXT("Original float32 component is nonfinite; wider values remain incomplete.");return false;}
        Converted[Index]=double(Narrowed);
    }
    Out=FVector(Converted[0],Converted[1],Converted[2]);return true;
}

UClass* SupportedClass(const FString& Type)
{
    if (Type.Equals(TEXT("StaticMeshActor"), ESearchCase::IgnoreCase)) { return AStaticMeshActor::StaticClass(); }
    if (Type.Equals(TEXT("PointLight"), ESearchCase::IgnoreCase)) { return APointLight::StaticClass(); }
    if (Type.Equals(TEXT("SpotLight"), ESearchCase::IgnoreCase)) { return ASpotLight::StaticClass(); }
    if (Type.Equals(TEXT("DirectionalLight"), ESearchCase::IgnoreCase)) { return ADirectionalLight::StaticClass(); }
    if (Type.Equals(TEXT("CameraActor"), ESearchCase::IgnoreCase)) { return ACameraActor::StaticClass(); }
    return nullptr;
}

bool IsSupportedClass(UClass* Type)
{
    return Type == AStaticMeshActor::StaticClass() || Type == APointLight::StaticClass() || Type == ASpotLight::StaticClass()
        || Type == ADirectionalLight::StaticClass() || Type == ACameraActor::StaticClass();
}

bool SetError(FString& Code, FString& Error, const TCHAR* InCode, const FString& Message)
{
    Code = InCode; Error = Message; return false;
}

UClass* ResolveSavedActorBlueprint(const FString& Path, bool& bLoadAttempted, FString& Code, FString& Error)
{
    auto Reject = [&](const TCHAR* InCode, const FString& Message) -> UClass*
    { SetError(Code, Error, InCode, Message); return nullptr; };
    if (Path.Len() >= NAME_SIZE || !Path.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(Path))
    { return Reject(TEXT("blueprint_path_invalid"), TEXT("Provide an exact /Game Blueprint asset object path shorter than NAME_SIZE.")); }
    const FString PackageName = FPackageName::ObjectPathToPackageName(Path);
    if (!FPackageName::DoesPackageExist(PackageName))
    { return Reject(TEXT("blueprint_not_saved"), TEXT("Compile and explicitly save the Blueprint before spawning it.")); }
    bLoadAttempted = true;
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
    if (!Blueprint || Blueprint->BlueprintType != BPTYPE_Normal || !Blueprint->GeneratedClass
        || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass())
        || Blueprint->GeneratedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    { return Reject(TEXT("blueprint_actor_class_unsupported"), TEXT("Asset must be a normal Actor Blueprint with an instantiable generated class.")); }
    if (Blueprint->GetOutermost()->IsDirty() || (Blueprint->Status != BS_UpToDate && Blueprint->Status != BS_UpToDateWithWarnings))
    { return Reject(TEXT("blueprint_compile_save_required"), TEXT("The Blueprint must be compiled and clean on disk. No explicit compile/save is dispatched; loading may perform engine PostLoad work.")); }
    const auto Parent = Cubeless::UE58::InspectBlueprintParent(Blueprint->GeneratedClass->GetPathName());
    if (!Parent->GetBoolField(TEXT("success")))
    { return Reject(TEXT("blueprint_runtime_parent_rejected"), Parent->GetStringField(TEXT("error"))); }
    for (const UClass* Class = Blueprint->GeneratedClass; Class; Class = Class->GetSuperClass())
    {
        const FString Package = Class->GetOutermost()->GetName();
        if (Class->HasAnyClassFlags(CLASS_Native) && Package != TEXT("/Script/Engine") && Package != TEXT("/Script/CoreUObject"))
        { return Reject(TEXT("blueprint_native_parent_unsupported"), TEXT("Native parents outside audited Engine/CoreUObject classes remain unsupported in this slice.")); }
    }
    const auto Dependencies = Cubeless::UE58::ReadPackageDependencies({PackageName}, true, 256);
    bool bPassed = false;
    if (!Dependencies->TryGetBoolField(TEXT("passed"), bPassed) || !bPassed)
    { return Reject(TEXT("blueprint_runtime_dependencies_rejected"), TEXT("Saved recursive dependency inspection failed, is dirty/incomplete, or references editor tooling. Inspect ReadPackageDependencies; limit 256 packages.")); }
    // Engine Cube contains a runtime UNavCollision (Runtime/NavigationSystem).
    const TSet<FString> NativePackages = {TEXT("/Script/Engine"), TEXT("/Script/CoreUObject"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
    for (const auto& Edge : Dependencies->GetArrayField(TEXT("edges")))
    {
        const FString Package = Edge->AsObject()->GetStringField(TEXT("to"));
        if (!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/")) && !NativePackages.Contains(Package))
        { return Reject(TEXT("blueprint_dependency_module_unsupported"), TEXT("Unaudited module or plugin mount in saved dependencies: ") + Package); }
    }
    return Blueprint->GeneratedClass;
}

void AddWorldIdentity(const FState& State, const TSharedPtr<FJsonObject>& Result)
{
    Result->SetStringField(TEXT("editor_world_path"), State.World.IsValid() ? State.World->GetPathName() : FString());
    Result->SetStringField(TEXT("persistent_level_path"), State.LevelPath);
    Result->SetStringField(TEXT("actor_scope"), TEXT("loaded editor-world actors; unloaded World Partition descriptors are not enumerated"));
}
}

FString OperationName(EOperation Operation)
{
    switch (Operation)
    {
    case EOperation::List: return TEXT("get_actors_in_level");
    case EOperation::Find: return TEXT("find_actors_by_name");
    case EOperation::Inspect: return TEXT("get_actor_properties");
    case EOperation::Spawn: return TEXT("spawn_actor");
    case EOperation::Delete: return TEXT("delete_actor");
    case EOperation::SetTransform: return TEXT("set_actor_transform");
    }
    return TEXT("unknown");
}

bool IsMutation(EOperation Operation)
{
    return Operation == EOperation::Spawn || Operation == EOperation::Delete || Operation == EOperation::SetTransform;
}

FString ToJson(const TSharedPtr<FJsonObject>& Value)
{
    FString Result;
    FJsonSerializer::Serialize(Value.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}

bool Capture(const FRequest& Request, FState& Out, FString& Code, FString& Error)
{
    if (!IsInGameThread() || !GEditor) { return SetError(Code, Error, TEXT("editor_game_thread_required"), TEXT("Actor APIs require the editor game thread.")); }
    if (GEditor->PlayWorld || GEditor->GetPIEWorldContext()) { return SetError(Code, Error, TEXT("play_session_active"), TEXT("Actor tools operate only on the editor world with PIE/SIE stopped.")); }
    if (Request.MaxActors < 1 || Request.MaxActors > 10000) { return SetError(Code, Error, TEXT("invalid_budget"), TEXT("max_actors must be in 1..10000.")); }
    if (Request.Name.Len() >= NAME_SIZE || Request.Pattern.Len() > 4096) { return SetError(Code, Error, TEXT("actor_selector_invalid"), TEXT("Object name must be shorter than NAME_SIZE and pattern must be at most 4096 UTF-16 units.")); }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    ULevel* Level = World ? World->PersistentLevel.Get() : nullptr;
    if (!World || World->WorldType != EWorldType::Editor || !Level) { return SetError(Code, Error, TEXT("editor_world_unavailable"), TEXT("No persistent editor world/level is available.")); }
    Out.World = World; Out.Level = Level; Out.LevelPath = Level->GetOutermost()->GetName();
    Out.bDirty = Level->GetOutermost()->IsDirty();
    if (!Request.ExpectedLevelPath.IsEmpty() && Request.ExpectedLevelPath != Out.LevelPath)
    {
        return SetError(Code, Error, TEXT("editor_level_mismatch"), TEXT("Persistent level differs from expected_level_path; this observation cannot identify the expected level."));
    }
    TArray<AActor*> Actors;
    // Explicitly include loaded inactive levels; no outliner/label filtering.
    for (TActorIterator<AActor> It(World, AActor::StaticClass(), EActorIteratorFlags::SkipPendingKill); It; ++It)
    {
        ++Out.ActorVisits;
        if (Actors.Num() >= Request.MaxActors) { return SetError(Code, Error, TEXT("actor_inspection_budget_exceeded"), TEXT("Loaded actor count exceeds the inspection budget; this enumeration is incomplete.")); }
        Actors.Add(*It);
    }
    Actors.Sort([](const AActor& A, const AActor& B) { return A.GetPathName() < B.GetPathName(); });
    TArray<TSharedPtr<FJsonValue>> Rows, Selected;
    int32 Matches = 0;
    for (AActor* Value : Actors)
    {
        if (Value->GetActorTransform().ContainsNaN())
        {
            return SetError(Code, Error, TEXT("actor_observation_nonfinite"), TEXT("Loaded actor transform is non-finite; refusing an invalid JSON snapshot: ") + Value->GetPathName());
        }
        auto Row = SnapshotActor(Value);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        Out.ActorsByPath.Add(Value->GetPathName(), Row);
        if (Value->IsSelectedInEditor()) { Selected.Add(MakeShared<FJsonValueString>(Value->GetPathName())); }
        if (!Request.Name.IsEmpty() && Value->GetName().Equals(Request.Name, ESearchCase::IgnoreCase)) { Out.CaseInsensitiveNameMatches.Add(Value->GetPathName()); }
        if (!Request.Name.IsEmpty() && Value->GetName().Equals(Request.Name, ESearchCase::CaseSensitive))
        {
            ++Matches; Out.Target = Value; Out.TargetSnapshot = Row; Out.TargetTransform = Value->GetActorTransform();
        }
    }
    Out.Snapshot = MakeShared<FJsonObject>();
    AddWorldIdentity(Out, Out.Snapshot);
    Out.Snapshot->SetArrayField(TEXT("actors"), Rows);
    Out.Snapshot->SetArrayField(TEXT("selected_actor_paths"), Selected);
    Out.Snapshot->SetNumberField(TEXT("actor_count"), Rows.Num());
    Out.Snapshot->SetBoolField(TEXT("enumeration_complete"), true);
    Out.Snapshot->SetBoolField(TEXT("dirty"), Out.bDirty);
    Out.Snapshot->SetBoolField(TEXT("world_partition"), World->IsPartitionedWorld());
    Out.Snapshot->SetBoolField(TEXT("external_actors"), Level->IsUsingExternalActors());
    // Keep the legacy target-existence contract, with world observations added.
    if (!Request.Name.IsEmpty())
    {
        Out.Snapshot->SetBoolField(TEXT("exists"), Out.TargetSnapshot.IsValid());
        if (Out.TargetSnapshot.IsValid()) { Out.Snapshot->SetObjectField(TEXT("actor"), Out.TargetSnapshot); }
    }
    if (Matches > 1) { return SetError(Code, Error, TEXT("actor_name_ambiguous"), TEXT("Multiple loaded actors have the exact object name; target identity is ambiguous.")); }
    return true;
}

bool PrepareMutation(const FRequest& Request, const FState& Before, FPreparedMutation& Out, FString& Code, FString& Error)
{
    UWorld* World = Before.World.Get(); ULevel* Level = Before.Level.Get();
    if(Request.bOriginalTransformContract&&Request.Operation!=EOperation::SetTransform)return SetError(Code,Error,TEXT("actor_original_operation_invalid"),TEXT("Original opt-in is specific to SetTransform."));
    if(TargetPointMutationRunning){return SetError(Code,Error,TEXT("actor_target_point_mutation_busy"),TEXT("A TargetPoint mutation callback is active; no nested actor writer."));}
    // QA2: copied admission operands are attached to the already-published
    // before snapshot only on refusal. No writer or failure reclassification.
    const bool HasWorld=World!=nullptr,HasLevel=Level!=nullptr;
    const bool CurrentMatches=HasWorld&&World->GetCurrentLevel()==Level;
    const int32 LevelCount=HasWorld?World->GetLevels().Num():0;
    const bool Partitioned=HasWorld&&World->IsPartitionedWorld(),External=HasLevel&&Level->IsUsingExternalActors(),Locked=HasLevel&&FLevelUtils::IsLevelLocked(Level);
    const uint32 LevelPackageFlags=HasLevel?uint32(Level->GetOutermost()->GetPackageFlags()):0;
    const bool GamePath=Before.LevelPath.StartsWith(TEXT("/Game/")),BoundedPath=Before.LevelPath.Len()<NAME_SIZE;
    const bool DiskExists=FPackageName::DoesPackageExist(Before.LevelPath);
    FFileBackedNewMapWitness NewMapWitness;const bool PackageFlagsAllowed=HasWorld&&HasLevel&&NewMapWitness.Current(World,Level);
    if(Before.Snapshot.IsValid())Before.Snapshot->SetObjectField(TEXT("newly_created_map_admission"),NewMapWitness.Receipt);
    if(!HasWorld||!HasLevel||!CurrentMatches||LevelCount!=1||Partitioned||External||Locked||!GamePath||!BoundedPath||!PackageFlagsAllowed||!DiskExists)
    {
        if(Before.Snapshot.IsValid())
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),TEXT("prepare_mutation_level"));J->SetBoolField(TEXT("operands_observed"),true);J->SetStringField(TEXT("world_path"),GetPathNameSafe(World));J->SetStringField(TEXT("level_path"),GetPathNameSafe(Level));J->SetStringField(TEXT("current_level_path"),HasWorld?GetPathNameSafe(World->GetCurrentLevel()):FString());J->SetStringField(TEXT("persistent_level_path"),HasWorld?GetPathNameSafe(World->PersistentLevel.Get()):FString());J->SetStringField(TEXT("input_level_package"),Before.LevelPath);J->SetBoolField(TEXT("world_present"),HasWorld);J->SetBoolField(TEXT("level_present"),HasLevel);J->SetBoolField(TEXT("current_matches"),CurrentMatches);J->SetNumberField(TEXT("level_count"),LevelCount);J->SetNumberField(TEXT("package_flags"),double(LevelPackageFlags));J->SetNumberField(TEXT("forbidden_package_flags"),double(uint32(PKG_NewlyCreated|PKG_EditorOnly)));J->SetBoolField(TEXT("package_flags_allowed"),PackageFlagsAllowed);J->SetBoolField(TEXT("disk_exists"),DiskExists);J->SetBoolField(TEXT("locked"),Locked);J->SetBoolField(TEXT("world_partition"),Partitioned);J->SetBoolField(TEXT("external_actors"),External);J->SetBoolField(TEXT("game_path"),GamePath);J->SetBoolField(TEXT("bounded_path"),BoundedPath);J->SetBoolField(TEXT("writer_entered"),false);
            Before.Snapshot->SetObjectField(TEXT("mutation_level_admission"),J);
        }
        return SetError(Code, Error, TEXT("actor_level_unsupported"), TEXT("Mutations require one saved /Game persistent level, current= persistent, unlocked, non-World-Partition and without external actors/streaming levels."));
    }
    FText NameReason;
    for (TCHAR Character : Request.Name)
    {
        if (Character < 32 || Character == 127)
        {
            return SetError(Code, Error, TEXT("actor_name_invalid"), TEXT("Object names cannot contain embedded NUL or control characters."));
        }
    }
    if (Request.Name.IsEmpty() || Request.Name.Equals(TEXT("None"), ESearchCase::IgnoreCase) || Request.Name.Len() >= NAME_SIZE
        || !FName::IsValidXName(Request.Name, INVALID_OBJECTNAME_CHARACTERS, &NameReason))
    {
        return SetError(Code, Error, TEXT("actor_name_invalid"), TEXT("Provide a nonempty valid exact UObject name shorter than NAME_SIZE; 'None' is reserved."));
    }
    AActor* Target = Before.Target.Get();
    if (Request.Operation == EOperation::Spawn)
    {
        if (Before.ActorsByPath.Num() >= Request.MaxActors) { return SetError(Code, Error, TEXT("actor_inspection_budget_exceeded"), TEXT("Spawning one actor would exceed the independent readback budget; no mutation was dispatched.")); }
        if (!Before.CaseInsensitiveNameMatches.IsEmpty()) { return SetError(Code, Error, TEXT("actor_name_conflict"), TEXT("Actor object name already exists (UObject names are case-insensitive for creation).")); }
        const FName ExistingName(*Request.Name, FNAME_Find);
        if (!ExistingName.IsNone() && StaticFindObjectFast(UObject::StaticClass(), Level, ExistingName)) { return SetError(Code, Error, TEXT("actor_name_conflict"), TEXT("A non-Actor object already owns the requested level object name.")); }
        if (!Request.BlueprintPath.IsEmpty())
        {
            Out.SpawnClass = ResolveSavedActorBlueprint(Request.BlueprintPath, Out.bBlueprintLoadAttempted, Code, Error);
            if (!Out.SpawnClass) { return false; }
        }
        else
        {
            Out.SpawnClass = SupportedClass(Request.Type);
            if (!Out.SpawnClass) { return SetError(Code, Error, TEXT("actor_class_unsupported"), TEXT("Supported exact native types: StaticMeshActor, PointLight, SpotLight, DirectionalLight, CameraActor (case-insensitive spelling).")); }
        }
        Out.DesiredTransform = FTransform::Identity;
    }
    else
    {
        if (!Target) { return SetError(Code, Error, TEXT("actor_not_found"), TEXT("No actor has the exact object name; actor labels are not selectors.")); }
        if((Request.Operation==EOperation::SetTransform||Request.Operation==EOperation::Delete)&&Target->GetClass()->GetClass()==UBlueprintGeneratedClass::StaticClass())
        {
            Out.TargetPoint=MakeShared<FTargetPointMutation>();Out.TargetPoint->BlueprintActor=MakeShared<FBlueprintActorMutation>();Out.TargetPoint->Data=Out.TargetPoint->BlueprintActor->Data;
            if(!Out.TargetPoint->BlueprintActor->Bind(Before,Request.Operation==EOperation::Delete,Request.Operation==EOperation::SetTransform)){Code=Out.TargetPoint->BlueprintActor->FailureCode;Error=TEXT("Blueprint Actor source/profile/current admission failed before explicit writer; copied lifecycle and first-current diagnostics retained.");return false;}
            Out.DesiredTransform=Before.TargetTransform;
            TStrongObjectPtr<UEditorActorSubsystem> Subsystem(GEditor->GetEditorSubsystem<UEditorActorSubsystem>());if(!Out.TargetPoint->Check(TEXT("after_blueprint_prepare_subsystem")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Blueprint current lost before writer."));
            if(!IsValid(Subsystem.Get()))return SetError(Code,Error,TEXT("actor_subsystem_unavailable"),TEXT("Blueprint mutation requires EditorActorSubsystem."));
            if(Request.Operation==EOperation::Delete)
            {
                TStrongObjectPtr<USelection> Selection(GEditor->GetSelectedActors());if(!Out.TargetPoint->Check(TEXT("after_blueprint_prepare_selection")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Blueprint current lost before writer."));
                TStrongObjectPtr<ULayersSubsystem> Layers(GEditor->GetEditorSubsystem<ULayersSubsystem>());if(!Out.TargetPoint->Check(TEXT("after_blueprint_prepare_layers")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Blueprint current lost before writer."));
                if(!IsValid(Selection.Get())||!IsValid(Layers.Get()))return SetError(Code,Error,TEXT("actor_subsystem_unavailable"),TEXT("Blueprint delete requires existing selection and Layers subsystems."));
            }
            else
            {
                TStrongObjectPtr<UEditorElementSubsystem> Elements(GEditor->GetEditorSubsystem<UEditorElementSubsystem>());if(!Out.TargetPoint->Check(TEXT("after_blueprint_prepare_elements")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Blueprint current lost before writer."));
                if(!IsValid(Elements.Get()))return SetError(Code,Error,TEXT("actor_subsystem_unavailable"),TEXT("Blueprint transform requires existing typed-element subsystem."));
            }
        }
        else
        {
        const bool AttachedTargetPoint=(Request.Operation==EOperation::SetTransform||Request.Operation==EOperation::Delete)&&Target->GetClass()==ATargetPoint::StaticClass()
            &&Target->GetRootComponent()&&Target->GetRootComponent()->GetAttachParent();
        if(Request.Operation==EOperation::SetTransform&&Target->GetClass()==AStaticMeshActor::StaticClass()&&!Target->GetAttachParentActor()&&FParentActorDeleteMutation::HasChildren(Target))
        {
            Out.TargetPoint=MakeShared<FTargetPointMutation>();Out.TargetPoint->ParentTransform=MakeShared<FParentActorTransformMutation>();Out.TargetPoint->Data=Out.TargetPoint->ParentTransform->Data;
            if(!Out.TargetPoint->ParentTransform->Bind(Before)){Code=Out.TargetPoint->ParentTransform->FailureCode;Error=TEXT("Parent transform admission/current failed before explicit writer; copied hierarchy and first-current diagnostics retained.");return false;}
            Out.DesiredTransform=Before.TargetTransform;
        }
        else if(Request.Operation==EOperation::Delete&&FParentActorDeleteMutation::Plain(Target->GetClass())&&!Target->GetAttachParentActor()&&FParentActorDeleteMutation::HasChildren(Target))
        {
            Out.TargetPoint=MakeShared<FTargetPointMutation>();Out.TargetPoint->ParentDelete=MakeShared<FParentActorDeleteMutation>();Out.TargetPoint->Data=Out.TargetPoint->ParentDelete->Data;
            if(!Out.TargetPoint->ParentDelete->Bind(Before)){Code=Out.TargetPoint->ParentDelete->FailureCode;Error=TEXT("Parent deletion admission/current failed; copied operands retained. No delete/retry was dispatched.");return false;}
            Out.DesiredTransform=Before.TargetTransform;
        }
        else if(AttachedTargetPoint)
        {
            Out.TargetPoint=MakeShared<FTargetPointMutation>();
            if(!Out.TargetPoint->Bind(Before,true,Request.Operation==EOperation::Delete)){Code=Out.TargetPoint->FailureCode;Error=TEXT("Attached TargetPoint source/context/profile admission failed before explicit mutation.");return false;}
            Out.DesiredTransform=Before.TargetTransform;
            TStrongObjectPtr<UEditorActorSubsystem> ActorSubsystem(GEditor->GetEditorSubsystem<UEditorActorSubsystem>());
            if(!Out.TargetPoint->Check(TEXT("after_attached_actor_subsystem")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Attached current lost before writer."));
            if(Request.Operation==EOperation::Delete)
            {
                TStrongObjectPtr<USelection> Selection(GEditor->GetSelectedActors());if(!Out.TargetPoint->Check(TEXT("after_attached_delete_selection")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Attached delete current lost before writer."));
                TStrongObjectPtr<ULayersSubsystem> Layers(GEditor->GetEditorSubsystem<ULayersSubsystem>());if(!Out.TargetPoint->Check(TEXT("after_attached_delete_layers")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Attached delete current lost before writer."));
                if(!IsValid(ActorSubsystem.Get())||!IsValid(Selection.Get())||!IsValid(Layers.Get()))return SetError(Code,Error,TEXT("actor_subsystem_unavailable"),TEXT("Attached deletion requires existing Editor actor/selection/Layers subsystems."));
            }
            else
            {
                TStrongObjectPtr<UEditorElementSubsystem> ElementSubsystem(GEditor->GetEditorSubsystem<UEditorElementSubsystem>());
                if(!Out.TargetPoint->Check(TEXT("after_attached_element_subsystem")))return SetError(Code,Error,*Out.TargetPoint->FailureCode,TEXT("Attached current lost before writer."));
                if(!IsValid(ActorSubsystem.Get())||!IsValid(ElementSubsystem.Get()))return SetError(Code,Error,TEXT("actor_subsystem_unavailable"),TEXT("Attached transform requires existing Editor actor/typed element subsystems."));
            }
        }
        else
        {
        if ((!IsSupportedClass(Target->GetClass())&&Target->GetClass()!=ATargetPoint::StaticClass()) || Target->GetLevel() != Level || Target->IsTemplate() || Target->IsEditorOnly()
            || Target->IsPackageExternal() || Target->HasAnyFlags(RF_Transient) || !Target->IsEditable()
            || Target->IsLockLocation() || !Target->GetRootComponent() || Target->IsChildActor() || Target->GetAttachParentActor())
        {
            return SetError(Code, Error, TEXT("actor_target_unsupported"), TEXT("Target must be an admitted exact native spawn class or native TargetPoint in the supported level, editable, unlocked, rooted, non-transient and unattached; Blueprint actors remain incomplete."));
        }
        TArray<AActor*> Attached;
        Target->GetAttachedActors(Attached);
        if (!Attached.IsEmpty()) { return SetError(Code, Error, TEXT("actor_target_unsupported"), TEXT("Actors with attached child actors are not supported by this mutation slice.")); }
        TInlineComponentArray<UActorComponent*> Components(Target);
        for (const UActorComponent* Component : Components)
        {
            if (Component && Component->GetClass()->GetOutermost()->GetName() != TEXT("/Script/Engine"))
            {
                return SetError(Code, Error, TEXT("actor_component_unsupported"), TEXT("Mutation requires Engine native components; authored or plugin component callbacks are outside this slice."));
            }
        }
        Out.DesiredTransform = Before.TargetTransform;
        if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) { return SetError(Code, Error, TEXT("actor_subsystem_unavailable"), TEXT("EditorActorSubsystem is unavailable.")); }
        if (Request.Operation == EOperation::Delete && (!GEditor->GetSelectedActors() || !GEditor->GetEditorSubsystem<ULayersSubsystem>()))
        { return SetError(Code, Error, TEXT("actor_subsystem_unavailable"), TEXT("Deletion requires editor selection and Layers subsystems.")); }
        if (Request.Operation == EOperation::SetTransform && !GEditor->GetEditorSubsystem<UEditorElementSubsystem>())
        { return SetError(Code, Error, TEXT("actor_subsystem_unavailable"), TEXT("Transform requires the editor typed-element subsystem.")); }
        }
        }
    }
    if (Request.Operation != EOperation::Delete)
    {
        FVector Value; bool bPresent = false;
        const auto ParseTransform=Request.bOriginalTransformContract?ParseOriginalTriple:ParseTriple;
        if (!ParseTransform(Request.LocationJson, Value, bPresent, Error)) { Code = TEXT("actor_transform_invalid"); return false; }
        if (bPresent) { Out.DesiredTransform.SetLocation(Value); }
        if (!ParseTransform(Request.RotationJson, Value, bPresent, Error)) { Code = TEXT("actor_transform_invalid"); return false; }
        if (bPresent) { Out.DesiredTransform.SetRotation(FRotator(Value.X, Value.Y, Value.Z).Quaternion()); }
        if (!ParseTransform(Request.ScaleJson, Value, bPresent, Error)) { Code = TEXT("actor_transform_invalid"); return false; }
        if (bPresent) { Out.DesiredTransform.SetScale3D(Value); }
        if (Out.DesiredTransform.ContainsNaN()) { return SetError(Code, Error, TEXT("actor_transform_invalid"), TEXT("Requested transform is non-finite after conversion.")); }
        Out.bNoOp = Request.Operation == EOperation::SetTransform && !Request.bOriginalTransformContract && Before.TargetTransform.Equals(Out.DesiredTransform, 0.0);
    }
    if(Out.TargetPoint&&Out.TargetPoint->AttachedMode&&Request.Operation!=EOperation::Delete)
    {
        if(!Out.TargetPoint->Check(TEXT("after_attached_transform_parse"))){Code=Out.TargetPoint->FailureCode;Error=TEXT("Attached target/source current changed while parsing the request.");return false;}
        if(!FTargetPointMutation::OrdinaryScale(Out.DesiredTransform.GetScale3D()))
            return SetError(Code,Error,TEXT("actor_attached_scale_profile_unsupported"),TEXT("New attached-only profile has positive uniform bounded requested world scale; broader valid Engine scale variants remain incomplete."));
    }
    if(Out.TargetPoint&&Out.TargetPoint->BlueprintActor&&!Out.TargetPoint->BlueprintActor->PlanAuthoredTransform(Out.DesiredTransform)){Code=Out.TargetPoint->BlueprintActor->FailureCode;Error=TEXT("Authored SCS transform plan/current failed before writer; wider original-valid hierarchy/scale variants remain incomplete.");return false;}
    if(Out.TargetPoint&&Out.TargetPoint->ParentTransform&&!Out.TargetPoint->ParentTransform->Plan(Out.DesiredTransform)){Code=Out.TargetPoint->ParentTransform->FailureCode;Error=TEXT("Parent transform plan/current failed before explicit writer; broader valid hierarchy/scale profiles remain incomplete.");return false;}
    if(Request.bOriginalTransformContract&&!Out.TargetPoint)
    {
        Out.TargetPoint=MakeShared<FTargetPointMutation>();
        if(!Out.TargetPoint->Bind(Before,false,false,true)){Code=Out.TargetPoint->FailureCode;Error=TEXT("Original direct setter requires the existing captured native component/level roles; wider valid profiles remain incomplete.");return false;}
    }
    if(Target&&Target->GetClass()==ATargetPoint::StaticClass()&&!Out.TargetPoint){Out.TargetPoint=MakeShared<FTargetPointMutation>();if(!Out.TargetPoint->Bind(Before)){Code=Out.TargetPoint->FailureCode;Error=TEXT("TargetPoint source/context admission failed before explicit actor mutation.");return false;}}
    return true;
}

bool ExecuteMutation(const FRequest& Request, const FState& Before, const FPreparedMutation& Prepared)
{
    UWorld* World = Before.World.Get(); ULevel* Level = Before.Level.Get(); AActor* Target = Before.Target.Get();
    if (!World || !Level) { return false; }
    if(Prepared.TargetPoint&&Prepared.TargetPoint->ParentTransform)
    {
        TGuardValue<bool> Lease(TargetPointMutationRunning,true);return Prepared.TargetPoint->ParentTransform->Execute(Prepared.DesiredTransform,Prepared.bNoOp,Request.bOriginalTransformContract);
    }
    if(Prepared.TargetPoint&&Prepared.TargetPoint->ParentDelete)
    {
        TGuardValue<bool> Lease(TargetPointMutationRunning,true);return Prepared.TargetPoint->ParentDelete->Execute();
    }
    if(Prepared.TargetPoint&&Prepared.TargetPoint->BlueprintActor)
    {
        TGuardValue<bool> Lease(TargetPointMutationRunning,true);return Prepared.TargetPoint->BlueprintActor->Execute(Request.Operation,Prepared.DesiredTransform,Prepared.bNoOp,Request.bOriginalTransformContract);
    }
    if(Prepared.TargetPoint)
    {
        auto& Scope=*Prepared.TargetPoint;TGuardValue<bool> Lease(TargetPointMutationRunning,true);if(!Scope.Check(TEXT("before_execute"))){return false;}
        if(Prepared.bNoOp){return true;}
        if(Request.Operation==EOperation::Delete)
        {
            auto* Subsystem=GEditor->GetEditorSubsystem<UEditorActorSubsystem>();TStrongObjectPtr<UEditorActorSubsystem> AttachedDeleteSubsystem(Scope.AttachedDeleteMode?Subsystem:nullptr);if(!Scope.Check(TEXT("after_delete_subsystem"))||!IsValid(Subsystem)){return false;}
            Scope.Count(TEXT("destroy_calls"));const bool Accepted=Subsystem->DestroyActor(Target);Scope.Data->SetBoolField(TEXT("destroy_returned"),true);Scope.Data->SetBoolField(TEXT("destroy_accepted"),Accepted);
            if(Accepted){Scope.Retired=true;Scope.Data->SetBoolField(TEXT("retired_after_accepted_destroy"),true);}
            else if(Scope.AttachedDeleteMode)
            {
                // False can follow selection/layers or opaque callback work. No target reread to guess a rollback.
                Scope.Data->SetBoolField(TEXT("destroy_false_readback_suppressed"),true);Scope.Fail(TEXT("after_attached_destroy_false"),TEXT("actor_attached_delete_outcome_unknown"));return false;
            }
            if(!Scope.Check(TEXT("after_destroy"))){return Accepted;}
            if(Accepted){Scope.Count(TEXT("mark_level_dirty_calls"));Level->MarkPackageDirty();Scope.Check(TEXT("after_delete_dirty"));}return Accepted;
        }
        if(Request.bOriginalTransformContract)
        {
            Scope.TransformDispatched=true;Scope.Count(TEXT("transform_calls"));Scope.Data->SetStringField(TEXT("transform_writer"),TEXT("AActor::SetActorTransform original direct"));
            const bool Accepted=Target->SetActorTransform(Prepared.DesiredTransform);Scope.Data->SetBoolField(TEXT("transform_returned"),true);Scope.Data->SetBoolField(TEXT("transform_accepted"),Accepted);
            Scope.Check(TEXT("after_original_transform"));return Accepted;
        }
        const FScopedTransaction Transaction(FText::FromString(TEXT("Cubeless ")+OperationName(Request.Operation)));if(!Scope.Check(TEXT("after_transaction_begin"))){return false;}
        Scope.Count(TEXT("world_modify_calls"));World->Modify(false);if(!Scope.Check(TEXT("after_world_modify"))){return false;}
        Scope.Count(TEXT("level_modify_calls"));Level->Modify(false);if(!Scope.Check(TEXT("after_level_modify"))){return false;}
        Scope.Count(TEXT("actor_modify_calls"));Target->Modify(false);if(!Scope.Check(TEXT("after_actor_modify"))){return false;}
        Scope.Count(TEXT("root_modify_calls"));Scope.Root->Modify(false);if(!Scope.Check(TEXT("after_root_modify"))){return false;}
        auto* Subsystem=GEditor->GetEditorSubsystem<UEditorActorSubsystem>();TStrongObjectPtr<UEditorActorSubsystem> AttachedSubsystem(Scope.AttachedMode?Subsystem:nullptr);if(!Scope.Check(TEXT("after_transform_subsystem"))||!IsValid(Subsystem)){return false;}
        Scope.TransformDispatched=true;Scope.Count(TEXT("transform_calls"));const bool Accepted=Subsystem->SetActorTransform(Target,Prepared.DesiredTransform);Scope.Data->SetBoolField(TEXT("transform_returned"),true);Scope.Data->SetBoolField(TEXT("transform_accepted"),Accepted);
        if(!Scope.Check(TEXT("after_transform"))){return Accepted;}
        if(!Target->GetActorTransform().Equals(Before.TargetTransform,0.0)){Scope.Count(TEXT("mark_level_dirty_calls"));Level->MarkPackageDirty();Scope.Check(TEXT("after_transform_dirty"));}return Accepted;
    }
    if (Prepared.bNoOp) { return true; }
    if (Request.Operation == EOperation::Delete)
    {
        const bool bAccepted = GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DestroyActor(Target);
        if (bAccepted && Before.Level.IsValid()) { Before.Level->MarkPackageDirty(); }
        return bAccepted;
    }
    const FScopedTransaction Transaction(FText::FromString(TEXT("Cubeless ") + OperationName(Request.Operation)));
    World->Modify(false); Level->Modify(false);
    if (Request.Operation == EOperation::Spawn)
    {
        FActorSpawnParameters Parameters;
        Parameters.Name = FName(*Request.Name);
        Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Required_ReturnNull;
        Parameters.OverrideLevel = Level;
        Parameters.ObjectFlags |= RF_Transactional;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Parameters.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
        Parameters.bCreateActorPackage = false;
        AActor* Created = World->SpawnActor(Prepared.SpawnClass, &Prepared.DesiredTransform, Parameters);
        if (Created && Before.Level.IsValid()) { Before.Level->MarkPackageDirty(); }
        return Created != nullptr;
    }
    if (!Target) { return false; }
    Target->Modify(false);
    Target->GetRootComponent()->Modify(false);
    const bool bAccepted = GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->SetActorTransform(Target, Prepared.DesiredTransform);
    if (Before.Target.IsValid() && Before.Level.IsValid() && !Before.Target->GetActorTransform().Equals(Before.TargetTransform, 0.0)) { Before.Level->MarkPackageDirty(); }
    return bAccepted;
}

bool CheckTargetPointScope(const FPreparedMutation& Prepared,FString& Code,FString& Error)
{
    if(!Prepared.TargetPoint){return true;}if(Prepared.TargetPoint->Check(TEXT("operation_boundary"))){return true;}
    Code=Prepared.TargetPoint->FailureCode;Error=TEXT("TargetPoint lifetime/role guard failed; actual returned Modify/transform/delete prefix retained, no further observation or retry.");return false;
}
TSharedPtr<FJsonObject> TargetPointDiagnostics(const FPreparedMutation& Prepared)
{return Prepared.TargetPoint?TSharedPtr<FJsonObject>(Prepared.TargetPoint->Data):TSharedPtr<FJsonObject>();}

bool VerifyMutation(const FRequest& Request, const FState& Before, const FState& After, const FPreparedMutation& Prepared, bool bApiAccepted)
{
    if(Prepared.TargetPoint)
    {
        if(!Prepared.TargetPoint->Check(TEXT("verify_current"))){return false;}bool Preserved=true;const FString Selected=Before.TargetSnapshot->GetStringField(TEXT("actor_path"));
        for(const auto& P:Before.ActorsByPath)
        {
            if(P.Key.Equals(Selected,ESearchCase::CaseSensitive))continue;const auto* Q=After.ActorsByPath.Find(P.Key);
            bool Same=Q&&ToJson(P.Value).Equals(ToJson(*Q),ESearchCase::CaseSensitive);
            if(Q&&Prepared.TargetPoint->ParentTransform&&Prepared.TargetPoint->ParentTransform->DescendantPath(P.Key))
            {
                // Only these captured descendants have planned SDK world propagation; Check already closes their authored relative/attachment and actual world values.
                auto E=MakeShared<FJsonObject>();E->Values=P.Value->Values;auto A=MakeShared<FJsonObject>();A->Values=(*Q)->Values;
                for(const TCHAR* Key:{TEXT("location"),TEXT("rotation"),TEXT("scale")}){E->RemoveField(Key);A->RemoveField(Key);}
                Same=ToJson(E).Equals(ToJson(A),ESearchCase::CaseSensitive);
            }
            if(Q&&Prepared.TargetPoint->ParentDelete)
            {
                bool Survivor=false;const auto& Roles=Prepared.TargetPoint->ParentDelete->Actors;for(int32 I=1;I<Roles.Num();++I)Survivor|=Roles[I].Path.Equals(P.Key,ESearchCase::CaseSensitive);
                if(Survivor)
                {
                    // Native KeepWorld detachment may normalize Euler/quaternion storage.
                    // Current already verifies held child world/relative roles at 0.01; retain every other basic cell literally.
                    auto E=MakeShared<FJsonObject>();E->Values=P.Value->Values;auto A=MakeShared<FJsonObject>();A->Values=(*Q)->Values;
                    for(const TCHAR* Key:{TEXT("location"),TEXT("rotation"),TEXT("scale")}){E->RemoveField(Key);A->RemoveField(Key);}
                    Same=ToJson(E).Equals(ToJson(A),ESearchCase::CaseSensitive);
                }
            }
            Preserved&=Same;
            if(!Same&&(Prepared.TargetPoint->AttachedDeleteMode||Prepared.TargetPoint->ParentDelete||Prepared.TargetPoint->ParentTransform)&&!Prepared.TargetPoint->Data->HasField(TEXT("first_unselected_actor_mismatch")))
            {
                auto Detail=MakeShared<FJsonObject>();Detail->SetStringField(TEXT("stage"),TEXT("verify_unselected_actor"));Detail->SetStringField(TEXT("object"),P.Key);Detail->SetStringField(TEXT("property"),TEXT("copied_basic_actor_snapshot"));Detail->SetStringField(TEXT("expected"),ToJson(P.Value));if(Q)Detail->SetStringField(TEXT("actual"),ToJson(*Q));else Detail->SetField(TEXT("actual"),MakeShared<FJsonValueNull>());Prepared.TargetPoint->Data->SetObjectField(TEXT("first_unselected_actor_mismatch"),Detail);
            }
        }
        for(const auto& P:After.ActorsByPath){if(P.Key.Equals(Selected,ESearchCase::CaseSensitive)){continue;}Preserved&=Before.ActorsByPath.Contains(P.Key);}
        Prepared.TargetPoint->Data->SetBoolField(TEXT("unselected_actor_basic_state_preserved"),Preserved);if(!Preserved){return false;}
    }
    if (!bApiAccepted || Before.World != After.World || Before.Level != After.Level) { return false; }
    if (Request.Operation == EOperation::Delete) { return Before.TargetSnapshot.IsValid() && !After.Target.IsValid(); }
    AActor* Target = After.Target.Get();
    if (!Target || Target->GetLevel() != After.Level.Get() || !After.TargetTransform.Equals(Prepared.DesiredTransform, 0.01)) { return false; }
    return Request.Operation == EOperation::Spawn ? Target->GetClass() == Prepared.SpawnClass : Target == Before.Target.Get();
}

TSharedPtr<FJsonObject> ReadPayload(const FRequest& Request, const FState& State)
{
    auto Result = MakeShared<FJsonObject>();
    if (Request.Operation == EOperation::List || Request.Operation == EOperation::Find)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& Value : State.Snapshot->GetArrayField(TEXT("actors")))
        {
            if (Request.Operation == EOperation::List || Value->AsObject()->GetStringField(TEXT("name")).Contains(Request.Pattern, ESearchCase::IgnoreCase)) { Rows.Add(Value); }
        }
        Result->SetArrayField(TEXT("actors"), Rows);
        Result->SetNumberField(TEXT("actor_count"), Rows.Num());
        Result->SetBoolField(TEXT("enumeration_complete"), true);
        Result->SetStringField(TEXT("name_matching"), TEXT("case-insensitive substring of UObject name; empty pattern matches all"));
    }
    else if (State.TargetSnapshot.IsValid()) { Result->Values = State.TargetSnapshot->Values; }
    AddWorldIdentity(State, Result);
    return Result;
}

TSharedPtr<FJsonObject> DescribeChanges(const FState& Before, const FState& After)
{
    auto Result = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Created, Removed, Updated;
    TArray<FString> Paths;
    After.ActorsByPath.GetKeys(Paths); Paths.Sort();
    for (const FString& Path : Paths)
    {
        const auto* Old = Before.ActorsByPath.Find(Path);
        if (!Old) { Created.Add(MakeShared<FJsonValueString>(Path)); }
        else if (ToJson(*Old) != ToJson(After.ActorsByPath[Path])) { Updated.Add(MakeShared<FJsonValueString>(Path)); }
    }
    Before.ActorsByPath.GetKeys(Paths); Paths.Sort();
    for (const FString& Path : Paths) { if (!After.ActorsByPath.Contains(Path)) { Removed.Add(MakeShared<FJsonValueString>(Path)); } }
    Result->SetArrayField(TEXT("created_actor_paths"), Created);
    Result->SetArrayField(TEXT("removed_actor_paths"), Removed);
    Result->SetArrayField(TEXT("updated_actor_paths"), Updated);
    Result->SetNumberField(TEXT("created_count"), Created.Num());
    Result->SetNumberField(TEXT("removed_count"), Removed.Num());
    Result->SetNumberField(TEXT("updated_count"), Updated.Num());
    Result->SetNumberField(TEXT("affected_count"), Created.Num() + Removed.Num() + Updated.Num());
    Result->SetBoolField(TEXT("package_dirty_changed"), Before.bDirty != After.bDirty);
    Result->SetStringField(TEXT("scope"), TEXT("loaded actor identity/basic-property/selection observations; not arbitrary UObject or delegate side-effect audit"));
    return Result;
}
}
