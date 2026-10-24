// Adapted local-SCS inspection contracts from UE56 Blueprint commands.
// Source provenance and deliberate limits: native-component-read-N.md.
#include "Compatibility/UE58/CubelessComponentApi.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::Component
{
namespace
{
bool Reject(FString& Code, FString& Error, const TCHAR* NewCode, const FString& Message)
{
    Code = NewCode; Error = Message; return false;
}

TArray<TSharedPtr<FJsonValue>> Triple(double X, double Y, double Z)
{
    return { MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y), MakeShared<FJsonValueNumber>(Z) };
}

bool SafeSelector(const FString& Value)
{
    if (Value.Len() >= NAME_SIZE) { return false; }
    for (TCHAR Character : Value) { if (Character < 32 || Character == 127) { return false; } }
    return true;
}

bool IsSimpleExportType(const FProperty* Property)
{
    return Property && (CastField<FBoolProperty>(Property) || CastField<FNumericProperty>(Property)
        || CastField<FEnumProperty>(Property) || CastField<FNameProperty>(Property) || CastField<FStrProperty>(Property));
}

bool IsPlainContainerScalar(const FProperty* P)
{
    if(!P)return false;const FFieldClass* K=P->GetClass();
    return K==FBoolProperty::StaticClass()||K==FNameProperty::StaticClass()||K==FStrProperty::StaticClass()
        ||K==FEnumProperty::StaticClass()||(CastField<FNumericProperty>(P)&&K->GetSuperClass()==FNumericProperty::StaticClass());
}

// Map/Set text export reuses the existing source hold/budget for resolved hard
// object/class leaves. Soft/weak/custom/aggregate leaves stay explicit gaps.
bool IsPlainContainerReference(const FProperty* P)
{
    return P&&(P->GetClass()==FObjectProperty::StaticClass()||P->GetClass()==FClassProperty::StaticClass());
}

bool HasValidScalarMetadata(const FProperty* Property, FString& Error)
{
    if (!Property) { Error = TEXT("Property metadata is missing."); return false; }
    if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        if (!IsValid(Enum->GetEnum()) || !Enum->GetUnderlyingProperty() || !Enum->GetUnderlyingProperty()->IsInteger())
        { Error = TEXT("Enum metadata or its underlying integer property is missing; moved/deleted enum descriptors cannot be read safely."); return false; }
    }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        if (!IsValid(Object->PropertyClass)) { Error = TEXT("Object property class metadata is missing."); return false; }
        if (const FClassProperty* Class = CastField<FClassProperty>(Property); Class && !IsValid(Class->MetaClass))
        { Error = TEXT("Class property metaclass metadata is missing."); return false; }
        if (const FSoftClassProperty* Class = CastField<FSoftClassProperty>(Property); Class && !IsValid(Class->MetaClass))
        { Error = TEXT("Soft class property metaclass metadata is missing."); return false; }
    }
    return true;
}

// JI admits only two additional native struct text exports. This is a bounded
// preflight/return fence, not a replacement serializer or a physics-body copy.
bool LegacyStructTextType(const FProperty* P)
{
    const auto* S=CastField<FStructProperty>(P);if(!S||!IsValid(S->Struct))return false;
    const FName N=S->Struct->GetFName();
    return (N==TEXT("Color")&&S->Struct==TBaseStructure<FColor>::Get())||(N==TEXT("BodyInstance")&&S->Struct==FBodyInstance::StaticStruct());
}
struct FLegacyStructTextRead
{
    struct FHeld{TStrongObjectPtr<UObject> Object;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;bool Dirty=false;uint32 PackageFlags=0;bool IsStruct=false;EStructFlags StructFlags=STRUCT_NoFlags;int32 StructSize=0;UStruct* StructSuper=nullptr;};
    TArray<FHeld> Held;UBlueprint* BP=nullptr;USimpleConstructionScript* SCS=nullptr;USCS_Node* Node=nullptr;UActorComponent* Template=nullptr;UClass* Generated=nullptr;FGuid Guid;FName Name;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;bool Unsafe=false,HardReferenceContainer=false;int32 Count=0;int64 Budget=0;
    struct FContainerField {const FProperty* P=nullptr;FFieldVariant Owner;FFieldClass* Kind=nullptr;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0;const FProperty* Underlying=nullptr;UEnum* Enum=nullptr;UClass* DeclaredClass=nullptr;UClass* MetaClass=nullptr;TArray<TPair<FName,int64>> Entries;};
    const FProperty* ContainerProperty=nullptr;UStruct* ContainerOwner=nullptr;FName ContainerName;
    TArray<FContainerField> ContainerFields;
    bool Loaded(const UObject* O)const{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    bool Context()
    {if(Unsafe)return false;if(!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!GEditor||GEditor!=Editor.Get()||!World.IsValid()||World->bIsTearingDown||(HardReferenceContainer?(World->WorldType!=EWorldType::Editor||static_cast<UWorld*>(GWorld)!=World.Get()):GEditor->GetEditorWorldContext().World()!=World.Get())){Unsafe=true;return false;}return true;}
    // Same public resolved-handle path/equality admission as KI below. Never
    // use tracked TObjectPtr::Get() even for an already resolved hard pointer.
    UObject* FindResolved(const TObjectPtr<UObject>& Ref)
    {
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
        if(!Context()||!Ref.IsResolved()||Ref==nullptr)return nullptr;
        const FString Path=Ref.GetPathName();
        UObject* Found=StaticFindObject(nullptr,nullptr,*Path);
        return Ref==Found?Found:nullptr;
#else
        return nullptr; // No resolving/tracking fallback for this new profile.
#endif
    }
    bool Pin(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter()))){Unsafe=true;return false;}
        for(const auto& I:Held)if(I.Object.Get()==O)return true;
        FHeld I;I.Object.Reset(O);I.Class=O->GetClass();I.Outer=O->GetOuter();I.Package=O->GetPackage();I.Path=O->GetPathName();I.Flags=O->GetFlags();I.Dirty=I.Package->IsDirty();I.PackageFlags=I.Package->GetPackageFlags();if(auto* S=Cast<UScriptStruct>(O)){I.IsStruct=true;I.StructFlags=S->StructFlags;I.StructSize=S->GetStructureSize();I.StructSuper=S->GetSuperStruct();}Held.Add(MoveTemp(I));return true;
    }
    bool Check()
    {
        if(!Context())return false;
        for(const auto& I:Held){auto* O=I.Object.Get();if(!Loaded(O)||!Loaded(I.Class)||!Loaded(I.Package)||(I.Outer&&!Loaded(I.Outer))||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetFlags()!=I.Flags||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive)||I.Package->IsDirty()!=I.Dirty||I.Package->GetPackageFlags()!=I.PackageFlags){Unsafe=true;return false;}if(I.IsStruct){auto* S=Cast<UScriptStruct>(O);if(!S||S->StructFlags!=I.StructFlags||S->GetStructureSize()!=I.StructSize||S->GetSuperStruct()!=I.StructSuper){Unsafe=true;return false;}}}
        if(BP->SimpleConstructionScript!=SCS||(HardReferenceContainer?BP->GeneratedClass.GetGCPtr()!=Generated:BP->GeneratedClass!=Generated)||Node->GetOuter()!=SCS||!SCS->GetAllNodes().Contains(Node)||Node->ComponentTemplate!=Template||Node->ComponentClass!=Template->GetClass()||Node->VariableGuid!=Guid||Node->GetVariableName()!=Name){Unsafe=true;return false;}
        if(ContainerProperty)
        {
            if(!Loaded(ContainerOwner)||FindFProperty<FProperty>(ContainerOwner,ContainerName)!=ContainerProperty){Unsafe=true;return false;}
            const auto* Map=CastField<FMapProperty>(ContainerProperty);const auto* Set=CastField<FSetProperty>(ContainerProperty);
            if(ContainerFields.Num()!=(Map?3:2)||(!Map&&!Set)
                ||(Map&&(Map->KeyProp!=ContainerFields[1].P||Map->ValueProp!=ContainerFields[2].P))
                ||(Set&&Set->ElementProp!=ContainerFields[1].P)){Unsafe=true;return false;}
            for(const auto& F:ContainerFields)
            {
                const auto* P=F.P;
                if(P->GetOwnerVariant()!=F.Owner||P->GetClass()!=F.Kind||P->GetPropertyFlags()!=F.Flags
                    ||P->GetOffset_ForInternal()!=F.Offset||P->GetSize()!=F.Size||P->ArrayDim!=F.Dim||P->HasGetter()
                    ||P->GetOwnerProperty()!=ContainerProperty){Unsafe=true;return false;}
                UEnum* E=nullptr;
                if(const auto* Enum=CastField<FEnumProperty>(P)){if(Enum->GetUnderlyingProperty()!=F.Underlying||!F.Underlying||F.Underlying->GetOwnerProperty()!=ContainerProperty||F.Underlying->GetOwner<FProperty>()!=P||F.Underlying->ArrayDim!=1||F.Underlying->HasGetter()||!IsPlainContainerScalar(F.Underlying)){Unsafe=true;return false;}E=Enum->GetEnum();}
                else if(const auto* Byte=CastField<FByteProperty>(P))E=Byte->Enum;
                if(E!=F.Enum||(E&&(!Loaded(E)||E->NumEnums()!=F.Entries.Num()))){Unsafe=true;return false;}
                if(E)for(int32 I=0;I<F.Entries.Num();++I)if(E->GetNameByIndex(I)!=F.Entries[I].Key||E->GetValueByIndex(I)!=F.Entries[I].Value){Unsafe=true;return false;}
                if(IsPlainContainerReference(P))
                {
                    const auto* Object=CastFieldChecked<FObjectProperty>(P);
                    const auto* Class=CastField<FClassProperty>(P);
                    if(!Object->PropertyClass.IsResolved()||(Class&&!Class->MetaClass.IsResolved())
                        ||Object->PropertyClass!=F.DeclaredClass||!Loaded(F.DeclaredClass)
                        ||(Class&&Class->MetaClass!=F.MetaClass)||(!Class&&F.MetaClass)||(F.MetaClass&&!Loaded(F.MetaClass)))
                    {Unsafe=true;return false;}
                }
            }
        }
        return true;
    }
    bool Bind(UBlueprint* B,USCS_Node* N,UActorComponent* T)
    {
        // Only the new hard-reference profile uses KI's no-read GWorld/context
        // and public resolved-path lookup; legacy scalar/struct admission stays unchanged.
        Editor=GEditor;World=HardReferenceContainer?static_cast<UWorld*>(GWorld):(GEditor?GEditor->GetEditorWorldContext().World():nullptr);if(!Context())return false;
        BP=B;Node=N;Template=T;if(!Pin(B)||!Pin(N)||!Pin(T))return false;
        if(HardReferenceContainer){SCS=Cast<USimpleConstructionScript>(FindResolved(B->SimpleConstructionScript));Generated=Cast<UClass>(FindResolved(B->GeneratedClass.GetGCPtr()));}
        else{SCS=B->SimpleConstructionScript;Generated=B->GeneratedClass;}
        if(!Pin(SCS)||!Pin(Generated))return false;Guid=N->VariableGuid;Name=N->GetVariableName();return Check();
    }
    bool BindContainer(const FProperty* P,FString& Error)
    {
        if(!Check()||!P||P->ArrayDim!=1||P->HasGetter())return false;
        const auto* Map=CastField<FMapProperty>(P);const auto* Set=CastField<FSetProperty>(P);
        if((!Map&&!Set)||(Map&&P->GetClass()!=FMapProperty::StaticClass())||(Set&&P->GetClass()!=FSetProperty::StaticClass()))return false;
        TArray<const FProperty*> Fields{P};if(Map){Fields.Add(Map->KeyProp);Fields.Add(Map->ValueProp);}else Fields.Add(Set->ElementProp);
        UStruct* Owner=P->GetOwnerStruct();if(!Pin(Owner)||FindFProperty<FProperty>(Owner,P->GetFName())!=P)return false;
        TArray<FContainerField> Rows;
        for(const auto* F:Fields)
        {
            if(IsPlainContainerReference(F))
            {
                const auto* Object=CastFieldChecked<FObjectProperty>(F);const auto* Class=CastField<FClassProperty>(F);
                if(!Object->PropertyClass.IsResolved()||(Class&&!Class->MetaClass.IsResolved()))
                {Error=TEXT("Unresolved Map/Set object/class descriptor metadata is not loaded by this read API.");return false;}
            }
            if(!F||F->ArrayDim!=1||F->HasGetter()||F->GetOwnerProperty()!=P
                ||(F!=P&&((!IsPlainContainerScalar(F)&&!IsPlainContainerReference(F))||(!IsPlainContainerReference(F)&&!HasValidScalarMetadata(F,Error)))))
            {Error=TEXT("Map/Set exports admit direct scalar/name/string/enum or resolved hard object/class fields with actual container-owned descriptors; soft/weak/struct/nested leaves remain incomplete.");return false;}
            FContainerField R;R.P=F;R.Owner=F->GetOwnerVariant();R.Kind=F->GetClass();R.Flags=F->GetPropertyFlags();R.Offset=F->GetOffset_ForInternal();R.Size=F->GetSize();R.Dim=F->ArrayDim;
            if(const auto* Enum=CastField<FEnumProperty>(F)){R.Enum=Enum->GetEnum();R.Underlying=Enum->GetUnderlyingProperty();}
            else if(const auto* Byte=CastField<FByteProperty>(F))R.Enum=Byte->Enum;
            if(R.Underlying&&(R.Underlying->GetOwnerProperty()!=P||R.Underlying->GetOwner<FProperty>()!=F
                ||R.Underlying->ArrayDim!=1||R.Underlying->HasGetter()||!IsPlainContainerScalar(R.Underlying)))return false;
            if(IsPlainContainerReference(F))
            {
                const auto* Object=CastFieldChecked<FObjectProperty>(F);R.DeclaredClass=Cast<UClass>(FindResolved(Object->PropertyClass));
                if(!Check()||!Pin(R.DeclaredClass))return false;
                if(const auto* Class=CastField<FClassProperty>(F)){R.MetaClass=Cast<UClass>(FindResolved(Class->MetaClass));if(!Check()||!Pin(R.MetaClass))return false;}
            }
            if(R.Enum)
            {
                if(!Pin(R.Enum)||R.Enum->NumEnums()<1||R.Enum->NumEnums()>1024)return false;
                for(int32 I=0;I<R.Enum->NumEnums();++I)R.Entries.Add({R.Enum->GetNameByIndex(I),R.Enum->GetValueByIndex(I)});
            }
            Rows.Add(MoveTemp(R));
        }
        ContainerOwner=Owner;ContainerName=P->GetFName();ContainerFields=MoveTemp(Rows);ContainerProperty=P;return Check();
    }
    bool NativeStruct(UScriptStruct* S)
    {
        if(!Loaded(S)||!Loaded(S->GetPackage())||S->GetClass()!=UScriptStruct::StaticClass()||!(S->StructFlags&STRUCT_Native)||(S->StructFlags&STRUCT_ExportTextItemNative)||!S->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return false;
        return Pin(S);
    }
    bool ValueBudget(const FProperty* P,const void* A,int32 Depth)
    {
        if(!Check()||!P||!A||Depth>8||++Count>4096||P->ArrayDim<1||P->ArrayDim>64||P->HasGetter())return false;
        Budget+=static_cast<int64>(P->GetName().Len())*6+32;if(Budget>1048576)return false;
        if(const auto* S=CastField<FStructProperty>(P))
        {
            if(!NativeStruct(S->Struct))return false;
            for(TFieldIterator<FProperty> It(S->Struct);It;++It)if(It->ShouldPort(PPF_None))for(int32 I=0;I<It->ArrayDim;++I)if(!ValueBudget(*It,It->ContainerPtrToValuePtr<void>(A,I),Depth+1))return false;
        }
        else if(const auto* ArrayMeta=CastField<FArrayProperty>(P))
        {FScriptArrayHelper H(ArrayMeta,A);if(H.Num()>256)return false;for(int32 I=0;I<H.Num();++I)if(!ValueBudget(ArrayMeta->Inner,H.GetRawPtr(I),Depth+1))return false;}
        else if(const auto* ObjectMeta=CastField<FObjectProperty>(P))
        {
            const TObjectPtr<UObject> ObjectValue=ObjectMeta->GetObjectPtrPropertyValue(A);if(!ObjectValue.IsResolved())return false;
            if(UObject* R=ObjectValue.Get()){if(!Pin(R)||!Pin(R->GetClass()))return false;Budget+=(static_cast<int64>(R->GetPathName().Len())+R->GetClass()->GetPathName().Len())*6+32;}else Budget+=8;
        }
        else if(const auto* N=CastField<FNameProperty>(P)){Budget+=static_cast<int64>(N->GetPropertyValue(A).ToString().Len())*6+32;}
        else if(const auto* StringMeta=CastField<FStrProperty>(P)){Budget+=static_cast<int64>(StringMeta->GetPropertyValue(A).Len())*6+32;}
        else if(const auto* E=CastField<FEnumProperty>(P))
        {if(!Loaded(E->GetEnum())||!E->GetUnderlyingProperty()||E->GetEnum()->NumEnums()<1||E->GetEnum()->NumEnums()>1024||!Pin(E->GetEnum()))return false;int32 Longest=0;for(int32 I=0;I<E->GetEnum()->NumEnums();++I)Longest=FMath::Max(Longest,E->GetEnum()->GetNameByIndex(I).ToString().Len());Budget+=static_cast<int64>(Longest)*6+128;}
        else if(const auto* B=CastField<FByteProperty>(P);B&&B->Enum)
        {if(!Loaded(B->Enum)||B->Enum->NumEnums()<1||B->Enum->NumEnums()>1024||!Pin(B->Enum))return false;int32 Longest=0;for(int32 I=0;I<B->Enum->NumEnums();++I)Longest=FMath::Max(Longest,B->Enum->GetNameByIndex(I).ToString().Len());Budget+=static_cast<int64>(Longest)*6+128;}
        else if(CastField<FBoolProperty>(P)||CastField<FNumericProperty>(P)){Budget+=128;}
        else return false;
        return Budget<=1048576&&Check();
    }
};

// KI is deliberately local-SCS-only. The shared scope-free value reader below
// retains its old single-object/scalar-array behavior and grants no array token.
struct FAssetObjectArrayRead
{
    struct FHeld
    {
        TStrongObjectPtr<UObject> Object; UClass* Class=nullptr; UObject* Outer=nullptr; UPackage* Package=nullptr;
        FString Path; EObjectFlags Flags=RF_NoFlags; bool Dirty=false, FullyLoaded=false; uint32 PackageFlags=0;
        bool IsClass=false; EClassFlags ClassFlags=CLASS_None; int32 ClassSize=0;
    };
    TArray<FHeld> Held; TArray<UObject*> Slots;
    TStrongObjectPtr<UEditorEngine> Editor; TStrongObjectPtr<UWorld> World; decltype(GUndo) Undo=nullptr;
    UBlueprint* BP=nullptr; USimpleConstructionScript* SCS=nullptr; USCS_Node* Node=nullptr;
    UActorComponent* Template=nullptr; UClass* Generated=nullptr; UClass* TemplateClass=nullptr;
    UClass* OwnerClass=nullptr; UClass* DeclaredClass=nullptr;
    const FArrayProperty* Array=nullptr; const FObjectProperty* Inner=nullptr;
    FName PropertyName, VariableName; FGuid Guid; EBlueprintStatus Status=BS_Unknown;
    uint64 ArrayFlags=0, InnerFlags=0; int32 ArrayOffset=0, ArraySize=0, InnerSize=0;
    int64 Budget=2; bool Unsafe=false;
    TSharedRef<FJsonObject> Receipt=MakeShared<FJsonObject>();

    bool Loaded(const UObject* O) const
    { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
    bool Context()
    {
        if(Unsafe)return false;
        if(!IsInGameThread()||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()
            ||GIsImportingT3D||!Editor.IsValid()||!Loaded(Editor.Get())||GEditor!=Editor.Get()||GUndo!=Undo||!World.IsValid()||!Loaded(World.Get())||World->bIsTearingDown
            ||World->WorldType!=EWorldType::Editor||static_cast<UWorld*>(GWorld)!=World.Get()) { Unsafe=true; return false; }
        return true;
    }
    // Public UE58 Editor late-resolve path lookup avoids tracked Get(). The
    // pointer equality operator is a shallow no-read comparison in this SDK.
    // No internal handle layout/private raw accessor is consumed by the plugin.
    UObject* FindResolved(const TObjectPtr<UObject>& Ref)
    {
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
        if(!Context()||!Ref.IsResolved()||Ref==nullptr)return nullptr;
        const FString Path=Ref.GetPathName();
        UObject* Found=StaticFindObject(nullptr,nullptr,*Path);
        return Ref==Found?Found:nullptr;
#else
        // The audited public no-read path is absent; do not add a resolving or
        // tracking fallback in this editor-only migration implementation.
        return nullptr;
#endif
    }
    bool Pin(UObject* O)
    {
        if(!Context()||!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter()))) { Unsafe=true; return false; }
        for(const auto& I:Held)if(I.Object.Get()==O)return true;
        FHeld I; I.Object.Reset(O); I.Class=O->GetClass(); I.Outer=O->GetOuter(); I.Package=O->GetPackage();
        I.Path=O->GetPathName(); I.Flags=O->GetFlags(); I.Dirty=I.Package->IsDirty(); I.PackageFlags=I.Package->GetPackageFlags(); I.FullyLoaded=I.Package->IsFullyLoaded();
        if(auto* C=Cast<UClass>(O)){I.IsClass=true;I.ClassFlags=C->ClassFlags;I.ClassSize=C->GetStructureSize();}
        Held.Add(MoveTemp(I));return true;
    }
    bool NativeClass(UClass* C)
    {
        return Loaded(C)&&Loaded(C->GetPackage())&&C->HasAnyClassFlags(CLASS_Native)
            &&!C->HasAnyClassFlags(CLASS_CompiledFromBlueprint)&&C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)
            &&C->GetPackage()->GetName()==TEXT("/Script/Engine")&&Pin(C);
    }
    bool Check()
    {
        if(!Context())return false;
        for(const auto& I:Held)
        {
            UObject* O=I.Object.Get();
            if(!Loaded(O)||!Loaded(I.Class)||!Loaded(I.Package)||(I.Outer&&!Loaded(I.Outer))
                ||O->GetClass()!=I.Class||O->GetOuter()!=I.Outer||O->GetPackage()!=I.Package||O->GetFlags()!=I.Flags
                ||!O->GetPathName().Equals(I.Path,ESearchCase::CaseSensitive)||I.Package->IsDirty()!=I.Dirty
                ||I.Package->GetPackageFlags()!=I.PackageFlags||I.Package->IsFullyLoaded()!=I.FullyLoaded) { Unsafe=true;return false; }
            if(I.IsClass){auto* C=Cast<UClass>(O);if(!C||C->ClassFlags!=I.ClassFlags||C->GetStructureSize()!=I.ClassSize){Unsafe=true;return false;}}
        }
        if(BP->SimpleConstructionScript!=SCS||BP->GeneratedClass.GetGCPtr()!=Generated||BP->Status!=Status
            ||Node->GetOuter()!=SCS||!SCS->GetAllNodes().Contains(Node)||Node->ComponentTemplate!=Template
            ||Node->ComponentClass!=TemplateClass||Template->GetClass()!=TemplateClass
            ||Node->VariableGuid!=Guid||Node->GetVariableName()!=VariableName) { Unsafe=true;return false; }
        const FProperty* Current=nullptr;
        for(TFieldIterator<FProperty> It(OwnerClass,EFieldIteratorFlags::ExcludeSuper);It;++It)
            if(It->GetFName()==PropertyName){Current=*It;break;}
        if(Current!=Array||!Current||Current->GetClass()!=FArrayProperty::StaticClass()) { Unsafe=true;return false; }
        if(Array->Inner!=Inner||Array->GetOwnerClass()!=OwnerClass||Array->GetPropertyFlags()!=ArrayFlags
            ||Array->ArrayDim!=1||Array->GetOffset_ForInternal()!=ArrayOffset||Array->GetElementSize()!=ArraySize||Array->HasGetter()
            ||Inner->GetClass()!=FObjectProperty::StaticClass()||Inner->PropertyClass!=DeclaredClass
            ||Inner->ArrayDim!=1||Inner->GetElementSize()!=InnerSize||Inner->GetPropertyFlags()!=InnerFlags||Inner->HasGetter()) { Unsafe=true;return false; }
        FScriptArrayHelper H(Array,Array->ContainerPtrToValuePtr<void>(Template));
        if(H.Num()!=Slots.Num()) { Unsafe=true;return false; }
        for(int32 I=0;I<H.Num();++I)
        {
            const TObjectPtr<UObject> Value=Inner->GetObjectPtrPropertyValue(H.GetRawPtr(I));
            if(!Value.IsResolved()||Value!=Slots[I]) { Unsafe=true;return false; }
        }
        return true;
    }
    bool Bind(UBlueprint* B,USCS_Node* N,UActorComponent* T,const FArrayProperty* P,int32 MaxChars,FString& Error,const TSharedRef<FJsonObject>& Data)
    {
        Data->SetObjectField(TEXT("asset_object_array_read"),Receipt);
        Receipt->SetNumberField(TEXT("max_elements"),256);Receipt->SetNumberField(TEXT("explicit_reference_load_calls"),0);
        Receipt->SetBoolField(TEXT("preflight_complete"),false);Receipt->SetBoolField(TEXT("export_attempted"),false);
        Receipt->SetBoolField(TEXT("export_returned"),false);Receipt->SetBoolField(TEXT("readonly_fence_passed"),false);
        Receipt->SetBoolField(TEXT("arbitrary_asset_fields_verified"),false);Receipt->SetBoolField(TEXT("disk_sha_verified"),false);
        Receipt->SetStringField(TEXT("context_scope"),TEXT("same game-thread GWorld with Editor WorldType; no FWorldContext::World tracked getter"));
        Receipt->SetStringField(TEXT("class_scope"),TEXT("retained native identity/flags/size and exact finite declaration compatibility; superclass fields are unobserved"));
        Editor.Reset(GEditor);World.Reset(static_cast<UWorld*>(GWorld));Undo=GUndo;
        if(!Context()){Error=TEXT("Current editor array-read context is unavailable.");return false;}
        BP=B;Node=N;Template=T;Array=P;
        if(!Pin(B)||!Pin(N)||!Pin(T)){Error=TEXT("Current local array-read owner is unavailable.");return false;}
        UObject* SCSObject=FindResolved(B->SimpleConstructionScript);
        SCS=Loaded(SCSObject)&&SCSObject->GetClass()->GetPathName()==TEXT("/Script/Engine.SimpleConstructionScript")?static_cast<USimpleConstructionScript*>(SCSObject):nullptr;
        Generated=Cast<UClass>(FindResolved(B->GeneratedClass.GetGCPtr()));TemplateClass=T->GetClass();Status=B->Status;
        if(!Pin(SCS)||!Pin(Generated)||!Pin(TemplateClass)){Error=TEXT("Current local SCS/class roles are unavailable.");return false;}
        Guid=N->VariableGuid;VariableName=N->GetVariableName();PropertyName=P->GetFName();OwnerClass=P->GetOwnerClass();
        Inner=CastField<FObjectProperty>(P->Inner);
        constexpr EPropertyFlags InstancedFlags=CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_PersistentInstance|CPF_ExportObject;
        if(P->GetClass()!=FArrayProperty::StaticClass()||P->ArrayDim!=1||P->HasGetter()||!Inner
            ||Inner->GetClass()!=FObjectProperty::StaticClass()||Inner->ArrayDim!=1||Inner->HasGetter()
            ||P->HasAnyPropertyFlags(InstancedFlags)||Inner->HasAnyPropertyFlags(InstancedFlags)||!NativeClass(OwnerClass))
        {Error=TEXT("Only direct native Engine non-instanced hard-object material arrays are admitted; class/soft/weak/nested/custom fields remain unsupported.");return false;}
        DeclaredClass=Cast<UClass>(FindResolved(Inner->PropertyClass));
        if(!NativeClass(DeclaredClass)||(DeclaredClass->GetPathName()!=TEXT("/Script/Engine.MaterialInterface")
            &&DeclaredClass->GetPathName()!=TEXT("/Script/Engine.Material")&&DeclaredClass->GetPathName()!=TEXT("/Script/Engine.MaterialInstanceConstant")))
        {Error=TEXT("Array declaration must be an already-loaded exact native MaterialInterface, Material or MaterialInstanceConstant.");return false;}
        ArrayFlags=P->GetPropertyFlags();InnerFlags=Inner->GetPropertyFlags();ArrayOffset=P->GetOffset_ForInternal();ArraySize=P->GetElementSize();InnerSize=Inner->GetElementSize();
        FScriptArrayHelper H(P,P->ContainerPtrToValuePtr<void>(T));
        if(H.Num()>256){Error=TEXT("Asset-object arrays have at most 256 elements.");return false;}
        TArray<TSharedPtr<FJsonValue>> Observed;TSet<UObject*> AssetFilesObserved;
        for(int32 I=0;I<H.Num();++I)
        {
            const TObjectPtr<UObject> Ref=Inner->GetObjectPtrPropertyValue(H.GetRawPtr(I));
            if(!Ref.IsResolved()){Error=TEXT("An unresolved object handle would allow implicit reference loading; no exporter was called.");return false;}
            UObject* O=Ref==nullptr?nullptr:FindResolved(Ref);
            if(Ref!=O){Error=TEXT("An already-resolved reference could not be found at its exact loaded object path; no exporter was called.");return false;}
            Slots.Add(O);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),I);J->SetBoolField(TEXT("is_null"),O==nullptr);
            if(O)
            {
                if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetPackage())){Unsafe=true;Error=TEXT("An array asset reference is no longer fully loaded.");return false;}
                UClass* C=O->GetClass();UPackage* Package=O->GetPackage();const FString ClassPath=C->GetPathName(),PackageName=Package->GetName();
                if((ClassPath!=TEXT("/Script/Engine.Material")&&ClassPath!=TEXT("/Script/Engine.MaterialInstanceConstant"))
                    ||(DeclaredClass->GetPathName()==TEXT("/Script/Engine.Material")&&ClassPath!=TEXT("/Script/Engine.Material"))
                    ||(DeclaredClass->GetPathName()==TEXT("/Script/Engine.MaterialInstanceConstant")&&ClassPath!=TEXT("/Script/Engine.MaterialInstanceConstant"))
                    ||O->GetOuter()!=Package||!O->HasAllFlags(RF_Public|RF_Standalone)
                    ||O->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject|RF_DefaultSubObject)
                    ||Package->IsDirty()||!Package->IsFullyLoaded()||Package->HasAnyFlags(RF_Transient)||Package->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly)
                    ||(!PackageName.StartsWith(TEXT("/Game/"),ESearchCase::CaseSensitive)&&!PackageName.StartsWith(TEXT("/Engine/"),ESearchCase::CaseSensitive))
                    ||!FPackageName::IsValidObjectPath(O->GetPathName())||!NativeClass(C)||!Pin(O))
                {Error=TEXT("References must be null or saved-clean direct Game/Engine exact native Material or MaterialInstanceConstant assets; runtime/foreign/custom/instanced objects are unsupported.");return false;}
                AssetFilesObserved.Add(O);
                J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("class"),ClassPath);
                Budget+=(static_cast<int64>(O->GetPathName().Len())+ClassPath.Len()+6)*6+3;
            }
            else {J->SetField(TEXT("path"),MakeShared<FJsonValueNull>());Budget+=5;}
            Observed.Add(MakeShared<FJsonValueObject>(J));Receipt->SetArrayField(TEXT("ordered_references"),Observed);
            if(Budget>MaxChars){Error=TEXT("Asset-object array text may exceed the bounded export budget.");return false;}
        }
        Receipt->SetNumberField(TEXT("element_count"),Slots.Num());Receipt->SetNumberField(TEXT("preflight_chars"),Budget);
        if(!Check()){Error=TEXT("Current local array/source roles changed during preflight.");return false;}
        // Retain every reference and freeze the whole array before any filesystem
        // query. Recheck the complete scope after each query, not just context.
        for(UObject* O:AssetFilesObserved)
        {
            const bool Exists=FPackageName::DoesPackageExist(O->GetPackage()->GetName());
            if(!Check()){Error=TEXT("Current array/source roles changed during the package-existence query.");return false;}
            if(!Exists){Error=TEXT("A referenced material package has no existing package file.");return false;}
        }
        Receipt->SetBoolField(TEXT("preflight_complete"),true);return true;
    }
    bool Export(int32 MaxChars,TSharedPtr<FJsonValue>& Out,FString& Encoding,FString& Error)
    {
        if(!Check()){Error=TEXT("Current local array/source roles changed before export.");return false;}
        FString Text;Receipt->SetBoolField(TEXT("export_attempted"),true);
        // Exact original SDK contract. This exports strings, never refPath JSON.
        Array->ExportTextItem_Direct(Text,Array->ContainerPtrToValuePtr<void>(Template),nullptr,nullptr,PPF_None);
        Receipt->SetBoolField(TEXT("export_returned"),true);
        if(!Check()){Error=TEXT("Array export changed retained source/current identities or readonly package roles.");return false;}
        Receipt->SetBoolField(TEXT("readonly_fence_passed"),true);Receipt->SetBoolField(TEXT("export_owner_is_null"),true);
        if(Text.Len()>MaxChars){Error=TEXT("Property text exceeds max_value_chars.");return false;}
        Out=MakeShared<FJsonValueString>(Text);Encoding=TEXT("legacy_ue_export_text_bounded_asset_object_array");return true;
    }
};

bool ReadValue(const FProperty* Property, const void* Address, int32 MaxChars, TSharedPtr<FJsonValue>& Out, FString& Encoding, FString& Error, FLegacyStructTextRead* TextScope = nullptr)
{
    if (!HasValidScalarMetadata(Property, Error)) { return false; }
    Encoding = TEXT("legacy_typed_json");
    if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property)) { Out = MakeShared<FJsonValueBoolean>(Bool->GetPropertyValue(Address)); }
    else if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
    {
        // UE56 tests FNumericProperty before FByteProperty, so byte enums are
        // numeric in the observed legacy serializer; preserve that ordering.
        if (const FUInt64Property* Unsigned = CastField<FUInt64Property>(Property); Unsigned && Unsigned->GetPropertyValue(Address) > 9007199254740991ULL)
        { Error = TEXT("Integer cannot be represented exactly as JSON double; exact large-integer adapter is not implemented."); return false; }
        const double Value = Number->IsInteger() ? static_cast<double>(Number->GetSignedIntPropertyValue(Address)) : Number->GetFloatingPointPropertyValue(Address);
        if (!FMath::IsFinite(Value) || (Number->IsInteger() && FMath::Abs(Value) > 9007199254740991.0))
        { Error = TEXT("Nonfinite or inexact large integer property values are unsupported."); return false; }
        Out = MakeShared<FJsonValueNumber>(Value);
    }
    else if (const FStrProperty* String = CastField<FStrProperty>(Property)) { Out = MakeShared<FJsonValueString>(String->GetPropertyValue(Address)); }
    else if (const FNameProperty* Name = CastField<FNameProperty>(Property)) { Out = MakeShared<FJsonValueString>(Name->GetPropertyValue(Address).ToString()); }
    else if (const FTextProperty* Text = CastField<FTextProperty>(Property)) { Out = MakeShared<FJsonValueString>(Text->GetPropertyValue(Address).ToString()); }
    else if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
    {
        const int64 Value = Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address);
        Out = MakeShared<FJsonValueString>(Enum->GetEnum()->GetNameStringByValue(Value));
    }
    else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    {
        if (Struct->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector& Value = *static_cast<const FVector*>(Address);
            if (Value.ContainsNaN()) { Error = TEXT("Vector property is nonfinite."); return false; }
            Out = MakeShared<FJsonValueArray>(Triple(Value.X, Value.Y, Value.Z));
        }
        else if (Struct->Struct == TBaseStructure<FRotator>::Get())
        {
            const FRotator& Value = *static_cast<const FRotator*>(Address);
            if (Value.ContainsNaN()) { Error = TEXT("Rotator property is nonfinite."); return false; }
            Out = MakeShared<FJsonValueArray>(Triple(Value.Pitch, Value.Yaw, Value.Roll));
        }
        else if (Struct->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor& Value = *static_cast<const FLinearColor*>(Address);
            if (!FMath::IsFinite(Value.R) || !FMath::IsFinite(Value.G) || !FMath::IsFinite(Value.B) || !FMath::IsFinite(Value.A))
            { Error = TEXT("Linear color property is nonfinite."); return false; }
            Out = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(Value.R), MakeShared<FJsonValueNumber>(Value.G), MakeShared<FJsonValueNumber>(Value.B), MakeShared<FJsonValueNumber>(Value.A)});
        }
        else if(LegacyStructTextType(Property))
        {
            if(!TextScope||!TextScope->ValueBudget(Property,Address,0)){Error=TEXT("Native struct export preflight is incomplete or exceeds the bounded reflected-value budget; unresolved references/custom exporters remain unsupported.");return false;}
            FString Exported;
            // Exact UE56 context: defaults=nullptr, Owner=nullptr, PPF_None.
            // Do not pass the observed owner into the SDK or copy FBodyInstance.
            Property->ExportTextItem_Direct(Exported,Address,nullptr,nullptr,PPF_None);
            if(!TextScope->Check()){Error=TEXT("Native struct text export changed current object/package/dirty roles.");return false;}
            Out=MakeShared<FJsonValueString>(Exported);Encoding=TEXT("legacy_ue_export_text_native_struct");
        }
        else { Error = TEXT("This struct's arbitrary native export is not audited; only FVector/FRotator/FLinearColor and bounded native FColor/FBodyInstance text are supported in this slice."); return false; }
    }
    else if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    {
        const UObject* Value = Object->GetObjectPropertyValue(Address);
        Out = MakeShared<FJsonValueString>(IsValid(Value) ? Value->GetPathName() : FString());
    }
    else if(CastField<FMapProperty>(Property)||CastField<FSetProperty>(Property))
    {
        if(!TextScope||!TextScope->BindContainer(Property,Error))
        {if(Error.IsEmpty())Error=TEXT("Map/Set export requires retained local-SCS source and complete scalar/hard-reference descriptors, including empty containers.");return false;}
        const auto* Map=CastField<FMapProperty>(Property);const auto* Set=CastField<FSetProperty>(Property);
        int64 ApproximateChars=2;
        auto BudgetScalar=[&](const FProperty* P,const void* A)->bool
        {
            if(IsPlainContainerReference(P))
            {
                if(!TextScope->Check()){Error=TEXT("Hard-reference container source changed before pointer observation.");return false;}
                const auto* Object=CastFieldChecked<FObjectProperty>(P);
                const TObjectPtr<UObject> Value=Object->GetObjectPtrPropertyValue(A);
                if(!Value.IsResolved()){Error=TEXT("Unresolved Map/Set object handles are not loaded by this read API.");return false;}
                UObject* Referenced=Value==nullptr?nullptr:TextScope->FindResolved(Value);
                if(!TextScope->Check()||(Value!=nullptr&&!Referenced))
                {Error=TEXT("Resolved hard-reference value could not be found under the same current source; no tracked fallback.");return false;}
                const auto* Descriptor=TextScope->ContainerFields.FindByPredicate([&](const FLegacyStructTextRead::FContainerField& F){return F.P==P;});
                if(!Descriptor||!Descriptor->DeclaredClass){Error=TEXT("Retained hard-reference descriptor is unavailable.");return false;}
                int64 ReferenceChars=static_cast<int64>(P->GetName().Len())*6+32;
                if(Referenced)
                {
                    if(!TextScope->Pin(Referenced)||!TextScope->Pin(Referenced->GetClass())||!Referenced->IsA(Descriptor->DeclaredClass))
                    {Error=TEXT("Hard-reference container value/class/ownership is unavailable or mismatched.");return false;}
                    if(Descriptor->MetaClass)
                    {
                        const auto* ValueClass=Cast<UClass>(Referenced);
                        if(!ValueClass||!ValueClass->IsChildOf(Descriptor->MetaClass)){Error=TEXT("Container class value does not satisfy its current MetaClass.");return false;}
                    }
                    ReferenceChars+=(static_cast<int64>(Referenced->GetPathName().Len())+Referenced->GetClass()->GetPathName().Len())*6+32;
                }
                else ReferenceChars+=8;
                // Reuse the hold scope's object-path budget, without calling its
                // older struct-only ValueBudget branch that uses tracked Get().
                if(++TextScope->Count>4096||TextScope->Budget+ReferenceChars>1048576||!TextScope->Check())
                {Error=TEXT("Hard-reference Map/Set value exceeded retained source/budget admission.");return false;}
                TextScope->Budget+=ReferenceChars;
                ApproximateChars+=ReferenceChars;
                if(ApproximateChars>MaxChars){Error=TEXT("Map/Set text may exceed max_value_chars before export.");return false;}
                return true;
            }
            TSharedPtr<FJsonValue> V;FString ItemEncoding;
            if(!ReadValue(P,A,MaxChars,V,ItemEncoding,Error))return false;
            ApproximateChars+=V->Type==EJson::String?static_cast<int64>(V->AsString().Len())*6+32:128;
            if(ApproximateChars>MaxChars){Error=TEXT("Map/Set text may exceed max_value_chars before export.");return false;}return true;
        };
        int32 Count=0,Visited=0,MaxIndex=0;
        if(Map)
        {
            FScriptMapHelper H(Map,Address);Count=H.Num();MaxIndex=H.GetMaxIndex();
            if(Count<0||Count>256||MaxIndex<Count||MaxIndex>4096){Error=TEXT("Map export exceeds 256 live entries or 4096 sparse slots.");return false;}
            for(int32 I=0;I<MaxIndex;++I)if(H.IsValidIndex(I)){++Visited;if(!BudgetScalar(Map->KeyProp,H.GetKeyPtr(I))||!BudgetScalar(Map->ValueProp,H.GetValuePtr(I)))return false;}
        }
        else
        {
            FScriptSetHelper H(Set,Address);Count=H.Num();MaxIndex=H.GetMaxIndex();
            if(Count<0||Count>256||MaxIndex<Count||MaxIndex>4096){Error=TEXT("Set export exceeds 256 live entries or 4096 sparse slots.");return false;}
            for(int32 I=0;I<MaxIndex;++I)if(H.IsValidIndex(I)){++Visited;if(!BudgetScalar(Set->ElementProp,H.GetElementPtr(I)))return false;}
        }
        if(Visited!=Count||!TextScope->Check()){Error=TEXT("Map/Set sparse membership or retained source changed before export.");return false;}
        TextScope->Budget=ApproximateChars;
        FString Exported;Property->ExportTextItem_Direct(Exported,Address,nullptr,nullptr,PPF_None);
        if(!TextScope->Check()){Error=TEXT("Map/Set export lost retained source/current roles; no subsequent SDK observation.");return false;}
        Out=MakeShared<FJsonValueString>(Exported);
        const bool ReferenceLeaves=Map?(IsPlainContainerReference(Map->KeyProp)||IsPlainContainerReference(Map->ValueProp)):IsPlainContainerReference(Set->ElementProp);
        Encoding=ReferenceLeaves?(Map?TEXT("legacy_ue_export_text_bounded_hard_reference_map"):TEXT("legacy_ue_export_text_bounded_hard_reference_set")):(Map?TEXT("legacy_ue_export_text_bounded_scalar_map"):TEXT("legacy_ue_export_text_bounded_scalar_set"));
    }
    else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        // Validate inner metadata even for an empty array: GetCPPType and the
        // legacy exporter can otherwise assert on a moved/deleted enum class.
        if (!HasValidScalarMetadata(Array->Inner, Error)) { return false; }
        FScriptArrayHelper Helper(Array, Address);
        if (!IsSimpleExportType(Array->Inner) || Helper.Num() > 256) { Error = TEXT("Only arrays of up to 256 plain scalar/name/string/enum elements have a bounded legacy text export in this slice."); return false; }
        int64 ApproximateChars = 2;
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            TSharedPtr<FJsonValue> Item; FString ItemEncoding;
            if (!ReadValue(Array->Inner, Helper.GetRawPtr(Index), MaxChars, Item, ItemEncoding, Error)) { return false; }
            // Conservative bound for quoted/escaped string export. It rejects
            // some valid long arrays rather than running an unbounded exporter.
            ApproximateChars += Item->Type == EJson::String ? static_cast<int64>(Item->AsString().Len()) * 6 + 3 : 64;
            if (ApproximateChars > MaxChars) { Error = TEXT("Array text may exceed the bounded export budget."); return false; }
        }
        FString Exported;
        Property->ExportTextItem_Direct(Exported, Address, nullptr, nullptr, PPF_None);
        Out = MakeShared<FJsonValueString>(Exported);
        Encoding = TEXT("legacy_ue_export_text_bounded_scalar_array");
    }
    else { Error = TEXT("This property type requires an unaudited generic exporter and remains unsupported."); return false; }
    if (Out->Type == EJson::String && Out->AsString().Len() > MaxChars) { Error = TEXT("Property text exceeds max_value_chars."); return false; }
    return true;
}

struct FNodeVisit { USCS_Node* Node = nullptr; FString ParentName; };
}

bool ReadComponentValue(const FProperty* Property, const void* Address, int32 MaxChars,
    TSharedPtr<FJsonValue>& Out, FString& Encoding, FString& Error)
{
    return ReadValue(Property, Address, MaxChars, Out, Encoding, Error);
}

bool ReadLocalComponents(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName,
    bool bReadProperty, int32 MaxComponents, int32 MaxValueChars, FReadResult& Out, FString& Code, FString& Error)
{
    if (!IsInGameThread() || !GEditor) { return Reject(Code, Error, TEXT("editor_game_thread_required"), TEXT("Component inspection requires the editor game thread.")); }
    if (MaxComponents < 1 || MaxComponents > 10000 || MaxValueChars < 64 || MaxValueChars > 65536)
    { return Reject(Code, Error, TEXT("component_budget_invalid"), TEXT("max_components must be 1..10000 and max_value_chars 64..65536.")); }
    if (!SafeSelector(BlueprintPath) || !SafeSelector(ComponentName) || !SafeSelector(PropertyName) || (bReadProperty && (ComponentName.IsEmpty() || PropertyName.IsEmpty())))
    { return Reject(Code, Error, TEXT("component_selector_invalid"), TEXT("Selectors must be shorter than NAME_SIZE with no control characters; property read requires component and property names.")); }
    FString ObjectPath = BlueprintPath;
    if (!ObjectPath.StartsWith(TEXT("/")) || ObjectPath.Contains(TEXT(":")) || ObjectPath.Contains(TEXT("'")))
    { return Reject(Code, Error, TEXT("blueprint_path_invalid"), TEXT("Provide an exact Blueprint package/object path, not a short name, export-text reference or subobject path.")); }
    if (!ObjectPath.Contains(TEXT("."))) { ObjectPath += TEXT(".") + FPackageName::GetLongPackageAssetName(ObjectPath); }
    if (ObjectPath.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(ObjectPath))
    { return Reject(Code, Error, TEXT("blueprint_path_invalid"), TEXT("Blueprint object path is invalid or too long.")); }
    UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *ObjectPath);
    Out.bLoadedDuringRead = Blueprint == nullptr;
    if (!Blueprint) { Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath); }
    if (!Blueprint) { return Reject(Code, Error, TEXT("blueprint_not_found"), TEXT("No UBlueprint asset exists at the exact path.")); }
    Out.BlueprintPath = Blueprint->GetPathName();
    Out.bDirtyObserved = true;
    Out.bDirtyBefore = Blueprint->GetOutermost()->IsDirty();
    Out.bDirtyAfter = Out.bDirtyBefore;
    Out.StatusBefore = static_cast<int32>(Blueprint->Status);
    Out.StatusAfter = Out.StatusBefore;
    const TWeakObjectPtr<UBlueprint> ObservedBlueprint = Blueprint;bool SkipFinalDirtyObservation=false;
    ON_SCOPE_EXIT
    {
        if (!SkipFinalDirtyObservation && ObservedBlueprint.IsValid())
        {
            Out.bDirtyAfter = ObservedBlueprint->GetOutermost()->IsDirty();
            Out.StatusAfter = static_cast<int32>(ObservedBlueprint->Status);
            Out.bDirtyAfterObserved = true;
        }
    };
    const EBlueprintStatus StatusBefore = Blueprint->Status;
    auto& Data = Out.Data;
    Data->SetStringField(TEXT("blueprint_name"), BlueprintPath);
    Data->SetStringField(TEXT("blueprint_path"), Out.BlueprintPath);
    Data->SetStringField(TEXT("enumeration_scope"), TEXT("reachable nodes of this Blueprint's local SCS only"));
    Data->SetBoolField(TEXT("inherited_effective_defaults_verified"), false);
    Data->SetBoolField(TEXT("native_cdo_components_included"), false);
    Data->SetBoolField(TEXT("orphan_allnodes_entries_inspected"), false);
    Data->SetBoolField(TEXT("has_simple_construction_script"), Blueprint->SimpleConstructionScript != nullptr);
    TArray<TSharedPtr<FJsonValue>> Components;
    USCS_Node* Match = nullptr;
    if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
    {
        TArray<FNodeVisit> Pending;
        if (SCS->GetRootNodes().Num() > MaxComponents) { return Reject(Code, Error, TEXT("component_budget_exceeded"), TEXT("Local root node count exceeds the inspection budget.")); }
        for (USCS_Node* Root : SCS->GetRootNodes()) { Pending.Add({Root, FString()}); }
        TSet<USCS_Node*> Visited;
        TSet<FName> Names;
        while (!Pending.IsEmpty())
        {
            const FNodeVisit Current = Pending.Pop(EAllowShrinking::No);
            USCS_Node* Node = Current.Node;
            if (!IsValid(Node)) { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Local SCS contains an invalid node.")); }
            if (Node->GetOuter() != SCS || Node->GetVariableName().IsNone()) { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Node ownership/name is inconsistent with this Blueprint's local SCS.")); }
            if (Visited.Contains(Node) || Names.Contains(Node->GetVariableName())) { return Reject(Code, Error, TEXT("component_tree_ambiguous"), TEXT("Local SCS contains a repeated node/cycle or duplicate variable name.")); }
            if (Visited.Num() >= MaxComponents) { return Reject(Code, Error, TEXT("component_budget_exceeded"), TEXT("Local SCS node count exceeds the inspection budget.")); }
            Visited.Add(Node); Names.Add(Node->GetVariableName()); ++Out.NodeVisits;
            const FString Name = Node->GetVariableName().ToString();
            Out.LocalNodes.Add(Name, Node);
            if (Name == ComponentName) { Match = Node; }
            for (USCS_Node* Child : Node->GetChildNodes())
            {
                if (Pending.Num() + Visited.Num() >= MaxComponents) { return Reject(Code, Error, TEXT("component_budget_exceeded"), TEXT("Local SCS pending traversal exceeds the inspection budget.")); }
                Pending.Add({Child, Name});
            }
            if (!ComponentName.IsEmpty() && Name != ComponentName) { continue; }
            UActorComponent* Template = Node->ComponentTemplate;
            if (!IsValid(Template)) { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Local SCS node has no valid component template.")); }
            if (!IsValid(Node->ComponentClass) || !Template->IsA(Node->ComponentClass))
            { return Reject(Code, Error, TEXT("component_tree_invalid"), TEXT("Selected node class metadata does not match its component template.")); }
            auto Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("component_name"), Name);
            Row->SetStringField(TEXT("variable_name"), Name);
            Row->SetStringField(TEXT("node_guid"), Node->VariableGuid.ToString());
            Row->SetStringField(TEXT("component_class"), Node->ComponentClass ? Node->ComponentClass->GetName() : FString());
            Row->SetStringField(TEXT("component_class_path"), Node->ComponentClass ? Node->ComponentClass->GetPathName() : FString());
            Row->SetStringField(TEXT("template_name"), Template ? Template->GetName() : FString());
            Row->SetStringField(TEXT("template_path"), Template ? Template->GetPathName() : FString());
            Row->SetStringField(TEXT("parent_component_name"), Current.ParentName);
            if (const USceneComponent* Scene = Cast<USceneComponent>(Template))
            {
                const FVector Location = Scene->GetRelativeLocation(), Scale = Scene->GetRelativeScale3D();
                const FRotator Rotation = Scene->GetRelativeRotation();
                if (Location.ContainsNaN() || Scale.ContainsNaN() || Rotation.ContainsNaN()) { return Reject(Code, Error, TEXT("component_observation_nonfinite"), TEXT("Component relative transform is nonfinite.")); }
                auto Transform = MakeShared<FJsonObject>();
                Transform->SetArrayField(TEXT("location"), Triple(Location.X, Location.Y, Location.Z));
                Transform->SetArrayField(TEXT("rotation"), Triple(Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
                Transform->SetArrayField(TEXT("scale"), Triple(Scale.X, Scale.Y, Scale.Z));
                Row->SetObjectField(TEXT("relative_transform"), Transform);
            }
            if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Template))
            { Row->SetStringField(TEXT("static_mesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : FString()); }
            Components.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Components.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
    { return A->AsObject()->GetStringField(TEXT("variable_name")) < B->AsObject()->GetStringField(TEXT("variable_name")); });
    Data->SetArrayField(TEXT("components"), Components);
    Data->SetNumberField(TEXT("component_count"), Components.Num());
    Data->SetNumberField(TEXT("local_reachable_node_count"), Out.NodeVisits);
    if (bReadProperty)
    {
        if (!Match || !Match->ComponentTemplate) { return Reject(Code, Error, TEXT("component_not_found"), TEXT("No local reachable SCS component with that exact variable name and valid template was found.")); }
        UActorComponent* Template = Match->ComponentTemplate;
        FProperty* Property = nullptr;
        for (TFieldIterator<FProperty> It(Template->GetClass()); It; ++It)
        {
            if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase)) { Property = *It; break; }
        }
        if (!Property || Property->ArrayDim != 1) { return Reject(Code, Error, TEXT("component_property_not_found"), TEXT("Property was not found or is a fixed-size native array, which is unsupported.")); }
        TSharedPtr<FJsonValue> Value; FString Encoding;TUniquePtr<FLegacyStructTextRead> TextScope;TUniquePtr<FAssetObjectArrayRead> AssetArrayScope;
        const auto* ObjectArray=CastField<FArrayProperty>(Property);
        bool ReadOK=false;
        if(ObjectArray&&ObjectArray->Inner&&CastField<FObjectPropertyBase>(ObjectArray->Inner))
        {
            AssetArrayScope=MakeUnique<FAssetObjectArrayRead>();
            ReadOK=AssetArrayScope->Bind(Blueprint,Match,Template,ObjectArray,MaxValueChars,Error,Data.ToSharedRef())
                &&AssetArrayScope->Export(MaxValueChars,Value,Encoding,Error);
            if(AssetArrayScope->Unsafe){SkipFinalDirtyObservation=true;return Reject(Code,Error,TEXT("component_read_state_changed"),TEXT("Asset-object array read lost retained source/current roles; no further UObject observation."));}
        }
        else if(LegacyStructTextType(Property)||CastField<FMapProperty>(Property)||CastField<FSetProperty>(Property))
        {
            TextScope=MakeUnique<FLegacyStructTextRead>();
            const auto* Map=CastField<FMapProperty>(Property);const auto* Set=CastField<FSetProperty>(Property);
            TextScope->HardReferenceContainer=Map?(IsPlainContainerReference(Map->KeyProp)||IsPlainContainerReference(Map->ValueProp)):(Set&&IsPlainContainerReference(Set->ElementProp));
            if(!TextScope->Bind(Blueprint,Match,Template)){SkipFinalDirtyObservation=true;return Reject(Code,Error,TEXT("component_read_state_changed"),TEXT("Current local SCS text-read scope is unreadable."));}
        }
        if(!AssetArrayScope){ReadOK=ReadValue(Property, Property->ContainerPtrToValuePtr<void>(Template), MaxValueChars, Value, Encoding, Error,TextScope.Get());}
        if(TextScope&&(!TextScope->Check()||TextScope->Unsafe)){SkipFinalDirtyObservation=true;return Reject(Code,Error,TEXT("component_read_state_changed"),TEXT("Native text export lost its retained source/current identity or readonly package roles; no further observation."));}
        if(!ReadOK){Code=TEXT("component_property_type_unsupported");return false;}
        if(TextScope&&TextScope->ContainerProperty){Data->SetBoolField(TEXT("native_container_export_owner_is_null"),true);Data->SetBoolField(TEXT("native_container_scoped_identity_verified"),true);Data->SetNumberField(TEXT("native_container_export_preflight_chars"),TextScope->Budget);Data->SetNumberField(TEXT("native_container_live_entry_limit"),256);Data->SetNumberField(TEXT("native_container_sparse_slot_limit"),4096);}
        else if(TextScope){Data->SetBoolField(TEXT("native_struct_export_owner_is_null"),true);Data->SetBoolField(TEXT("native_struct_scoped_identity_verified"),true);Data->SetNumberField(TEXT("native_struct_export_preflight_chars"),TextScope->Budget);}
        Data->SetStringField(TEXT("component_name"), ComponentName);
        Data->SetStringField(TEXT("component_class"), Template->GetClass()->GetName());
        Data->SetStringField(TEXT("property_name"), PropertyName);
        Data->SetStringField(TEXT("property_type"), Property->GetCPPType());
        Data->SetStringField(TEXT("property_encoding"), Encoding);
        Data->SetField(TEXT("property_value"), Value);
    }
    Out.bDirtyAfter = Blueprint->GetOutermost()->IsDirty();
    Data->SetBoolField(TEXT("blueprint_status_unchanged"), StatusBefore == Blueprint->Status);
    if (Out.bDirtyAfter != Out.bDirtyBefore || StatusBefore != Blueprint->Status)
    { return Reject(Code, Error, TEXT("component_read_state_changed"), TEXT("Blueprint dirty/status observation changed during the read; inspect the asset before further work.")); }
    Data->SetBoolField(TEXT("success"), true);
    return true;
}
}
