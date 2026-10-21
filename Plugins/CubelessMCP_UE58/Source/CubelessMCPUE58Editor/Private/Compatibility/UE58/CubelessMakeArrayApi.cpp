#include "Compatibility/UE58/CubelessMakeArrayApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/UserDefinedEnum.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/PropertyOptional.h"
#include "UObject/FieldPathProperty.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EngineVersion.h"
#include "Misc/DefaultValueHelper.h"
#include "Internationalization/Text.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::MakeArray
{
namespace
{
using FResult = Function::FResult;
bool FullyLoaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects); }
FString Text(const TSharedRef<FJsonObject>& Object)
{ FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out; }
FString Path(const UObject* Object) { return Object ? Object->GetPathName() : FString(); }
bool Error(FResult& R, const TCHAR* Code, const FString& Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
FString Normalize(FString Value)
{ Value.TrimStartAndEndInline(); Value.ToLowerInline(); Value.ReplaceInline(TEXT(" "), TEXT("")); Value.ReplaceInline(TEXT("_"), TEXT("")); Value.ReplaceInline(TEXT("-"), TEXT("")); return Value; }
bool ExactInt32(const TSharedPtr<FJsonValue>& Value, int32& Out)
{
    if (!Value.IsValid() || Value->Type != EJson::Number) { return false; }
    const double Number = Value->AsNumber();
    if (!FMath::IsFinite(Number) || Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32)
        || FMath::TruncToDouble(Number) != Number) { return false; }
    Out = static_cast<int32>(Number); return true;
}


// IK: concrete read-only UserDefinedEnum scope. Primitive/Vector routes keep their existing path.
bool EnumContext()
{ return IsEditorGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage(); }
bool EnumLoaded(const UObject* O)
{ return FullyLoaded(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed); }
bool EnumEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!EnumEqual(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& Pair:X->Values) { const auto* V=Y->Values.Find(Pair.Key);if(!V||!EnumEqual(Pair.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
struct FEnumArrayScope
{
    Function::FResult* Reply=nullptr;bool Active=false,Stopped=false,Attached=false,GuidFixed=false,PinsFixed=false,LoadEntered=false,FunctionAssigned=false;
    TStrongObjectPtr<UEnum> Source;TStrongObjectPtr<UPackage> SourcePackage;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    UObject* SourceOuter=nullptr;UClass* SourceClass=nullptr;FString SourcePath;EObjectFlags SourceFlags=RF_NoFlags;bool SourceDirty=false;uint32 SourcePackageFlags=0;EObjectFlags SourcePackageObjectFlags=RF_NoFlags;
    TSharedPtr<FJsonObject> Before;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEdGraphNode> Node;
    TStrongObjectPtr<UPackage> TargetPackage;TStrongObjectPtr<UClass> Parent,Generated,Skeleton;TStrongObjectPtr<const UEdGraphSchema_K2> Schema;
    UObject *BlueprintOuter=nullptr,*GraphOuter=nullptr;UClass *BlueprintClass=nullptr,*GraphClass=nullptr,*NodeClass=nullptr;
    FString BlueprintPath,GraphPath,NodePath;FGuid GraphGuid,NodeGuid;
    struct FPinRole { UEdGraphPin* Address=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_MAX;FEdGraphPinType Type;uint32 Flags=0;UObject* DefaultObject=nullptr; };
    TArray<FPinRole> Roles;
    TStrongObjectPtr<UClass> FunctionOwner;TStrongObjectPtr<UFunction> NativeFunction;TStrongObjectPtr<UObject> FunctionCDO;
    bool Bad(const TCHAR* Code,const TCHAR* Why)
    {
        if(Stopped) { return false; }Stopped=true;auto& R=*Reply;R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=!LoadEntered&&!R.bMutationAttempted;
        if(!R.ErrorCode.IsEmpty()&&R.ErrorCode!=Code) { R.Data->SetStringField(TEXT("enum_preceding_error_code"),R.ErrorCode); }
        R.ErrorCode=Code;R.Error=Why;R.Data->SetBoolField(TEXT("enum_source_observed_unchanged"),false);
        R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);return false;
    }
    bool Context() const
    { return EnumContext()&&GEditor==Editor.Get()&&!IsPlaySessionActive()&&World.IsValid()&&GEditor->GetEditorWorldContext().World()==World.Get()&&!World->bIsTearingDown; }
    bool Identity()
    {
        if(Stopped) { return false; }if(!Active) { return true; }
        if(!Context()) { return Bad(TEXT("array_enum_context_changed"),TEXT("Captured Editor/world or GT/PIE/GC/exit/save admission changed; no next SDK or cleanup.")); }
        auto* E=Source.Get();
        if(!EnumLoaded(E)||!EnumLoaded(SourcePackage.Get())||!EnumLoaded(SourceOuter)||!EnumLoaded(SourceClass)
            ||E->GetClass()!=SourceClass||E->GetOuter()!=SourceOuter||E->GetPackage()!=SourcePackage.Get()||E->GetPathName()!=SourcePath||E->GetFlags()!=SourceFlags||SourcePackage->IsDirty()!=SourceDirty||uint32(SourcePackage->GetPackageFlags())!=SourcePackageFlags||SourcePackage->GetFlags()!=SourcePackageObjectFlags)
        { return Bad(TEXT("array_enum_identity_changed"),TEXT("Read-only enum source identity/package/flags/dirty changed; no replacement source is adopted.")); }
        return true;
    }
    static uint32 PinFlags(const UEdGraphPin* P)
    { return uint32(P->bHidden)|(uint32(P->bNotConnectable)<<1)|(uint32(P->bDefaultValueIsReadOnly)<<2)|(uint32(P->bDefaultValueIsIgnored)<<3)|(uint32(P->bOrphanedPin)<<4)|(uint32(P->bAdvancedView)<<5); }
    bool Target()
    {
        if(!Blueprint.IsValid()) { return true; }auto* B=Blueprint.Get();auto* G=Graph.Get();
        if(!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(TargetPackage.Get())||!EnumLoaded(BlueprintOuter)||!EnumLoaded(GraphOuter)
            ||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPackage()!=TargetPackage.Get()||B->GetPathName()!=BlueprintPath
            ||B->ParentClass.Get()!=Parent.Get()||B->GeneratedClass.Get()!=Generated.Get()||B->SkeletonGeneratedClass.Get()!=Skeleton.Get()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||G->GetClass()!=GraphClass||G->GetOuter()!=GraphOuter||G->GetPackage()!=TargetPackage.Get()||G->GetPathName()!=GraphPath||G->GraphGuid!=GraphGuid
            ||TargetPackage->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_CompiledIn|PKG_PlayInEditor)||!G->bEditable||G->GetSchema()!=Schema.Get()||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=B)
        { return Bad(TEXT("array_enum_target_changed"),TEXT("Selected Blueprint/owned graph/schema role changed; no stale target SDK or cleanup.")); }
        if(FunctionOwner.IsValid()&&(!EnumLoaded(FunctionOwner.Get())||!EnumLoaded(NativeFunction.Get())||!EnumLoaded(FunctionCDO.Get())||NativeFunction->GetOuterUClass()!=FunctionOwner.Get()||FunctionOwner->GetDefaultObject(false)!=FunctionCDO.Get()))
        { return Bad(TEXT("array_enum_native_function_changed"),TEXT("Captured native function/library/CDO role changed; no next SDK.")); }
        if(Node.IsValid())
        {
            auto* N=Node.Get();int32 Count=0;for(const auto& It:G->Nodes) { if(It.Get()==N) { ++Count; } }
            if(!EnumLoaded(N)||N->GetClass()!=NodeClass||N->GetOuter()!=G||N->GetPackage()!=TargetPackage.Get()||N->GetPathName()!=NodePath||Count!=(Attached?1:0)||(GuidFixed&&N->NodeGuid!=NodeGuid))
            { return Bad(TEXT("array_enum_node_changed"),TEXT("Created native node identity/membership changed; retained partial state is not repaired.")); }
            if(FunctionAssigned)
            {
                const auto* Call=Cast<UK2Node_CallFunction>(N);
                if(!Call||Call->FunctionReference.GetMemberParentClass()!=FunctionOwner.Get()||Call->FunctionReference.GetMemberName()!=NativeFunction->GetFName()||Call->FunctionReference.IsSelfContext()||Call->FunctionReference.GetMemberGuid().IsValid())
                { return Bad(TEXT("array_enum_function_binding_changed"),TEXT("Created call node no longer has the selected exact native function binding.")); }
            }
            if(PinsFixed)
            {
                if(N->Pins.Num()!=Roles.Num()) { return Bad(TEXT("array_enum_pin_changed"),TEXT("Current physical pin count differs from the allocation receipt.")); }
                for(int32 I=0;I<Roles.Num();++I)
                {
                    const auto& Role=Roles[I];UEdGraphPin* P=N->Pins[I];
                    if(P!=Role.Address||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=Role.Id||P->PinName!=Role.Name||P->Direction!=Role.Direction
                        ||P->PinType!=Role.Type||P->PinType.bIsUObjectWrapper!=Role.Type.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat!=Role.Type.bSerializeAsSinglePrecisionFloat||P->PinType.PinValueType.bTerminalIsUObjectWrapper!=Role.Type.PinValueType.bTerminalIsUObjectWrapper||PinFlags(P)!=Role.Flags||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()||P->DefaultObject.Get()!=Role.DefaultObject||(Role.DefaultObject&&!EnumLoaded(Role.DefaultObject))||!P->DefaultTextValue.IsEmpty())
                    { return Bad(TEXT("array_enum_pin_changed"),TEXT("Original physical pin ID/owner/type/flags or unlinked default role changed across a callback.")); }
                }
            }
        }
        return true;
    }
    bool Snapshot(TSharedRef<FJsonObject>& J)
    {
        if(!Identity()) { return false; }auto* E=Source.Get();
        J->SetStringField(TEXT("path"),SourcePath);J->SetStringField(TEXT("class"),SourceClass->GetPathName());J->SetBoolField(TEXT("dirty"),SourceDirty);J->SetNumberField(TEXT("flags"),uint32(SourceFlags));J->SetNumberField(TEXT("package_flags"),SourcePackageFlags);J->SetNumberField(TEXT("package_object_flags"),uint32(SourcePackageObjectFlags));
        if(E->NumEnums()<2||E->NumEnums()>257) { return Bad(TEXT("array_enum_entries_invalid"),TEXT("Bounded byte UserDefinedEnum entries required.")); }
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(int32 I=0;I<E->NumEnums();++I)
        {
            auto Row=MakeShared<FJsonObject>();const FString Name=E->GetNameStringByIndex(I);const int64 Value=E->GetValueByIndex(I);const FString Display=E->GetDisplayNameTextByIndex(I).ToString();
            if(!Identity()) { return false; }
            if(Name.Len()>512||Display.Len()>4096||(I<E->NumEnums()-1&&(Value<0||Value>255))) { return Bad(TEXT("array_enum_entries_invalid"),TEXT("Enum byte entries/text exceed the finite source scope.")); }
            Row->SetStringField(TEXT("name"),Name);Row->SetStringField(TEXT("full_name"),E->GetNameByIndex(I).ToString());Row->SetStringField(TEXT("value"),LexToString(Value));Row->SetStringField(TEXT("display"),Display);
            Row->SetBoolField(TEXT("hidden"),E->HasMetaData(TEXT("Hidden"),I));Row->SetBoolField(TEXT("spacer"),E->HasMetaData(TEXT("Spacer"),I));if(!Identity()) { return false; }Entries.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetArrayField(TEXT("entries"),Entries);J->SetNumberField(TEXT("cpp_form"),int32(E->GetCppForm()));J->SetNumberField(TEXT("underlying"),int32(E->GetUnderlyingType()));
        for(const FName Field:{FName(TEXT("UniqueNameIndex")),FName(TEXT("EnumDescription")),FName(TEXT("DisplayNameMap"))})
        {
            auto* P=FindFProperty<FProperty>(E->GetClass(),Field);FString Value;
            if(!P||P->ArrayDim!=1||!P->ExportText_Direct(Value,P->ContainerPtrToValuePtr<void>(E),P->ContainerPtrToValuePtr<void>(E),E,PPF_None)||!Identity()) { return Bad(TEXT("array_enum_stored_fields_incomplete"),TEXT("Complete stored UserDefinedEnum fields unavailable.")); }
            if(Value.Len()>262144) { return Bad(TEXT("array_enum_source_bound"),TEXT("Stored enum source field exceeds finite observation bound.")); }J->SetStringField(Field.ToString(),Value);
        }
        const TMap<FName,FString>* Meta=FMetaData::GetMapForObject(E);if(!Identity()) { return false; }auto M=MakeShared<FJsonObject>();
        if(Meta)
        {
            if(Meta->Num()>2048) { return Bad(TEXT("array_enum_source_bound"),TEXT("Enum source metadata exceeds finite observation bound.")); }
            for(const auto& Pair:*Meta) { if(Pair.Key.ToString().Len()>512||Pair.Value.Len()>16384) { return Bad(TEXT("array_enum_source_bound"),TEXT("Enum metadata text exceeds bound.")); }M->SetStringField(Pair.Key.ToString(),Pair.Value); }
        }
        J->SetObjectField(TEXT("metadata"),M);FString File;
        if(!FPackageName::TryConvertLongPackageNameToFilename(SourcePackage->GetName(),File,FPackageName::GetAssetPackageExtension())) { return Bad(TEXT("array_enum_disk_incomplete"),TEXT("Enum source package filename unavailable.")); }
        const int64 Size=IFileManager::Get().FileSize(*File);J->SetBoolField(TEXT("disk_exists"),Size>=0);J->SetStringField(TEXT("disk_path"),File);
        if(Size>=0)
        {
            if(Size>16*1024*1024) { return Bad(TEXT("array_enum_disk_bound"),TEXT("Enum source disk package exceeds16MiB bound.")); }
            TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size) { return Bad(TEXT("array_enum_disk_incomplete"),TEXT("Enum source disk read incomplete.")); }
            uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);J->SetNumberField(TEXT("disk_bytes"),Size);J->SetStringField(TEXT("disk_sha1"),BytesToHex(Digest,FSHA1::DigestSize));
        }
        if(!Identity()||Text(J).Len()>1048576) { return Bad(TEXT("array_enum_source_bound"),TEXT("Complete enum source snapshot exceeds1Mi UTF16 bound or context changed.")); }return true;
    }
    bool Check()
    {
        if(Stopped) { return false; }if(!Active) { return true; }if(!Identity()||!Target()) { return false; }
        auto Now=MakeShared<FJsonObject>();if(!Snapshot(Now)) { return false; }
        if(!EnumEqual(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(Now))) { return Bad(TEXT("array_enum_source_changed"),TEXT("Read-only enum entries/stored fields/metadata/disk changed; no next SDK, rollback or retry.")); }
        Reply->Data->SetBoolField(TEXT("enum_source_observed_unchanged"),true);return Target();
    }
    bool Load(const FString& Input,Function::FResult& R)
    {
        Reply=&R;Active=true;
        if(!EnumContext()||IsPlaySessionActive()) { return Bad(TEXT("array_enum_editor_required"),TEXT("Safe Editor GT context required before enum asset load.")); }
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!World.IsValid()||World->bIsTearingDown) { return Bad(TEXT("array_enum_editor_required"),TEXT("Current Editor world required.")); }
        FString P=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();P.TrimQuotesInline();P.ReplaceInline(TEXT("\\"),TEXT("/"));
        if(!Function::Pin::SafeName(Input)||(!P.StartsWith(TEXT("/Game/"))&&!P.StartsWith(TEXT("/Engine/")))) { return Bad(TEXT("array_enum_descriptor_pending"),TEXT("This enum extension consumes exact Game/Engine UserDefinedEnum assets; native/custom-module/slow-find variants remain pending.")); }
        const FString Low=P.ToLower();for(const TCHAR* Token:{TEXT("cubelessmcp"),TEXT("cubelesscontrolrigtools"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal")}) { if(Low.Contains(Token)) { return Bad(TEXT("array_enum_dependency_forbidden"),TEXT("Enum asset dependency points to authoring bridge content.")); } }
        if(!P.Contains(TEXT("."))) { P+=TEXT(".")+FPackageName::GetShortName(P); }
        if(P.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(P)) { return Bad(TEXT("array_enum_path_invalid"),TEXT("Raw and expanded exact enum asset object path must fit NAME_SIZE.")); }
        LoadEntered=true;R.Data->SetNumberField(TEXT("enum_load_calls"),1);UEnum* E=LoadObject<UEnum>(nullptr,*P);R.Data->SetNumberField(TEXT("enum_load_returns"),1);R.Data->SetBoolField(TEXT("enum_load_global_effects_unverified"),true);
        if(!Context()) { return Bad(TEXT("array_enum_context_changed"),TEXT("Enum load returned after context loss; return UObject was not observed/adopted.")); }
        if(!EnumLoaded(E)||E->GetClass()!=UUserDefinedEnum::StaticClass()||E->GetPathName()!=P||E->GetOuter()!=E->GetPackage()||!E->HasAnyFlags(RF_Public)
            ||E->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||E->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))
        { return Bad(TEXT("array_enum_source_unsupported"),TEXT("Exact runtime public UserDefinedEnum asset required; redirected/custom/transient source is not substituted.")); }
        Source.Reset(E);SourcePackage.Reset(E->GetPackage());SourceOuter=E->GetOuter();SourceClass=E->GetClass();SourcePath=P;SourceFlags=E->GetFlags();SourceDirty=SourcePackage->IsDirty();SourcePackageFlags=uint32(SourcePackage->GetPackageFlags());SourcePackageObjectFlags=SourcePackage->GetFlags();
        auto J=MakeShared<FJsonObject>();if(!Snapshot(J)) { return false; }Before=J;R.Data->SetStringField(TEXT("enum_asset_path"),P);R.Data->SetObjectField(TEXT("enum_source_before"),J);
        R.Data->SetBoolField(TEXT("enum_source_read_only"),true);R.Data->SetBoolField(TEXT("enum_source_initial_dirty"),SourceDirty);return Check();
    }
    bool SetTarget(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S)
    {
        if(!Check()||!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(S)||B->GetPackage()==SourcePackage.Get()) { return Bad(TEXT("array_enum_target_unsupported"),TEXT("A current directly owned Blueprint graph in a distinct source package is required.")); }
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);TargetPackage.Reset(B->GetPackage());Parent.Reset(B->ParentClass.Get());Generated.Reset(B->GeneratedClass.Get());Skeleton.Reset(B->SkeletonGeneratedClass.Get());BlueprintOuter=B->GetOuter();GraphOuter=G->GetOuter();BlueprintClass=B->GetClass();GraphClass=G->GetClass();BlueprintPath=B->GetPathName();GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;return Check();
    }
    bool SetNode(UEdGraphNode* N,UClass* ExpectedClass)
    {
        // NewObject has returned, but the node is not yet owned by Graph.Nodes.
        // No source/metadata/display SDK runs before the exact return is strongly held.
        if(!Identity()) { return false; }
        if(!EnumLoaded(N)||N->GetClass()!=ExpectedClass||N->GetOuter()!=Graph.Get()||N->GetPackage()!=TargetPackage.Get())
        { return Bad(TEXT("array_enum_node_invalid"),TEXT("NewObject return does not have the expected exact native class/owned graph role.")); }
        Node.Reset(N);NodeClass=ExpectedClass;NodePath=N->GetPathName();return Check();
    }
    bool FreezePins()
    {
        if(!Check()||!Node.IsValid()||Node->Pins.Num()>512) { return Bad(TEXT("array_enum_pin_invalid"),TEXT("New allocation has no bounded physical pin array.")); }
        TSet<FGuid> Ids;for(UEdGraphPin* P:Node->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node.Get()||!P->PinId.IsValid()||Ids.Contains(P->PinId)) { return Bad(TEXT("array_enum_pin_invalid"),TEXT("Allocated physical pin ID/owner is invalid.")); }
            Ids.Add(P->PinId);Roles.Add({P,P->PinId,P->PinName,P->Direction,P->PinType,PinFlags(P),P->DefaultObject.Get()});
        }
        PinsFixed=true;return Check();
    }
    bool ExpectType(UEdGraphPin* P,const FEdGraphPinType& Type)
    {
        if(!Check()) { return false; }for(auto& Role:Roles) { if(Role.Address==P) { Role.Type=Type;return true; } }return Bad(TEXT("array_enum_pin_invalid"),TEXT("Writer pin is not an original allocated physical pin."));
    }
    bool Value(const TSharedPtr<FJsonValue>& V,FString& Out)
    {
        if(!Check()) { return false; }int32 Index=INDEX_NONE;UEnum* E=Source.Get();
        if(V.IsValid()&&V->Type==EJson::String)
        {
            FString Name=V->AsString().TrimStartAndEnd();Name.TrimQuotesInline();if(Name.IsEmpty()||Name.Len()>512) { return Bad(TEXT("array_enum_value_invalid"),TEXT("Enum value must be a bounded nonempty entry name or integral numeric value.")); }
            Index=E->GetIndexByNameString(Name);if(Index==INDEX_NONE&&!Name.Contains(TEXT("::"))) { Index=E->GetIndexByNameString(E->GetName()+TEXT("::")+Name); }
            if(Index==INDEX_NONE) { const FString Full=E->GenerateFullEnumName(*Name);if(!Check()) { return false; }Index=E->GetIndexByNameString(Full); }
        }
        else if(V.IsValid()&&V->Type==EJson::Number)
        {
            const double D=V->AsNumber();if(!FMath::IsFinite(D)||D<0||D>255||FMath::TruncToDouble(D)!=D) { return Bad(TEXT("array_enum_value_invalid"),TEXT("Enum numeric default must be an exact byte-valued entry; no truncation.")); }Index=E->GetIndexByValue(int64(D));
        }
        if(!Check()) { return false; }
        if(Index==INDEX_NONE||Index>=E->NumEnums()-1||E->HasMetaData(TEXT("Hidden"),Index)||E->HasMetaData(TEXT("Spacer"),Index)) { return Bad(TEXT("array_enum_value_invalid"),TEXT("Enum entry is missing, hidden, spacer, MAX or is not a string/integer default.")); }
        Out=E->GetNameStringByIndex(Index);return Check();
    }
};
// IN: original Vector literal conversion; no object/struct type catalog expansion.
bool VectorLiteral(const TSharedPtr<FJsonValue>& Input,FString& Out,FResult& R)
{
    if(!Input) { return Error(R,TEXT("make_array_vector_literal_invalid"),TEXT("A Vector value needs a finite array, lowercase axis object, or schema-valid literal string.")); }
    if(Input->Type==EJson::String)
    {
        const FString Literal=Input->AsString();FVector Parsed;bool Safe=!Literal.IsEmpty()&&Literal.Len()<=65536;for(TCHAR Unit:Literal) { Safe&=Unit!=0; }
        if(Safe&&FDefaultValueHelper::ParseVector(Literal,Parsed)&&!Parsed.ContainsNaN()) { Out=Literal;return true; }
        return Error(R,TEXT("make_array_vector_literal_invalid"),TEXT("Vector literal string is not finite and schema-valid."));
    }
    double Axes[3]={0,0,0};
    if(Input->Type==EJson::Array)
    {
        const auto& Values=Input->AsArray();
        // Original GetPinDefaultStringForType reads the first three values and defaults missing axes to zero.
        for(int32 I=0;I<FMath::Min(Values.Num(),3);++I) { if(!Values[I]||Values[I]->Type!=EJson::Number||!FMath::IsFinite(Values[I]->AsNumber())) { return Error(R,TEXT("make_array_vector_literal_invalid"),TEXT("Provided Vector axes must be finite JSON numbers; legacy malformed-number coercions remain pending.")); }Axes[I]=Values[I]->AsNumber(); }
    }
    else if(Input->Type==EJson::Object)
    {
        const auto Object=Input->AsObject();const TCHAR* Names[3]={TEXT("x"),TEXT("y"),TEXT("z")};if(!Object) { return false; }
        for(int32 I=0;I<3;++I) { const auto Value=Object->TryGetField(Names[I]);if(!Value) { continue; }if(Value->Type!=EJson::Number||!FMath::IsFinite(Value->AsNumber())) { return Error(R,TEXT("make_array_vector_literal_invalid"),TEXT("Provided lowercase Vector axes must be finite JSON numbers; missing axes default to zero.")); }Axes[I]=Value->AsNumber(); }
    }
    else { return Error(R,TEXT("make_array_vector_literal_invalid"),TEXT("Vector values do not coerce null/bool/scalar into a struct literal.")); }
    Out=FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),*FString::SanitizeFloat(Axes[0]),*FString::SanitizeFloat(Axes[1]),*FString::SanitizeFloat(Axes[2]));return true;
}

// IN: callback admission only for the newly accepted Vector literal/operation routes.
struct FVectorArrayScope
{
    FResult* Reply=nullptr;bool Stopped=false,Attached=false,GuidFixed=false,PinsFixed=false;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UEdGraphNode> Node;
    TStrongObjectPtr<const UEdGraphSchema_K2> Schema;TStrongObjectPtr<UScriptStruct> Vector;TStrongObjectPtr<UClass> Parent,Generated,Skeleton,NodeClass;
    UObject *BlueprintOuter=nullptr,*VectorOuter=nullptr,*GeneratedOuter=nullptr,*SkeletonOuter=nullptr;UClass *BlueprintClass=nullptr,*VectorClass=nullptr,*GeneratedClass=nullptr,*SkeletonClass=nullptr;
    FString BlueprintPath,GraphPath,VectorPath,NodePath,GeneratedPath,SkeletonPath;FGuid GraphGuid,NodeGuid;EObjectFlags VectorFlags=RF_NoFlags,GeneratedFlags=RF_NoFlags,SkeletonFlags=RF_NoFlags;
    bool VectorDirty=false,FunctionAssigned=false;TArray<FEnumArrayScope::FPinRole> Roles;
    TStrongObjectPtr<UClass> FunctionOwner;TStrongObjectPtr<UFunction> Function;TStrongObjectPtr<UObject> FunctionDefault;
    bool Bad(const TCHAR* Code,const TCHAR* Why) { if(!Stopped) { if(!Reply->ErrorCode.IsEmpty()&&Reply->ErrorCode!=Code) { Reply->Data->SetStringField(TEXT("vector_preceding_error_code"),Reply->ErrorCode);Reply->Data->SetStringField(TEXT("vector_preceding_error"),Reply->Error); }Stopped=true;Reply->bSuccess=Reply->bPostconditionPassed=false;Reply->bChangeStateKnown=false;Reply->ErrorCode=Code;Reply->Error=Why;Reply->Data->SetBoolField(TEXT("vector_callback_admission_failed"),true); }return false; }
    bool Context()
    {
        if(Stopped) { return false; }
        if(!EnumContext()||GEditor!=Editor.Get()||IsPlaySessionActive()||!World.IsValid()||GEditor->GetEditorWorldContext().World()!=World.Get()||World->bIsTearingDown) { return Bad(TEXT("array_vector_context_changed"),TEXT("Captured Editor/world admission changed; no next Vector SDK or cleanup.")); }return true;
    }
    bool Check()
    {
        if(!Context()) { return false; }
        UBlueprint* B=Blueprint.Get();UEdGraph* G=Graph.Get();
        if(!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(Vector.Get())||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPathName()!=BlueprintPath||B->ParentClass.Get()!=Parent.Get()
            ||B->GeneratedClass.Get()!=Generated.Get()||B->SkeletonGeneratedClass.Get()!=Skeleton.Get()||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||!EnumLoaded(Generated.Get())||!EnumLoaded(Skeleton.Get())||Generated->GetClass()!=GeneratedClass||Skeleton->GetClass()!=SkeletonClass||Generated->GetOuter()!=GeneratedOuter||Skeleton->GetOuter()!=SkeletonOuter||Generated->GetPathName()!=GeneratedPath||Skeleton->GetPathName()!=SkeletonPath||Generated->GetFlags()!=GeneratedFlags||Skeleton->GetFlags()!=SkeletonFlags
            ||G->GetOuter()!=B||G->GetPathName()!=GraphPath||G->GraphGuid!=GraphGuid||G->GetPackage()!=B->GetPackage()||!G->bEditable||G->GetSchema()!=Schema.Get()
            ||Vector->GetClass()!=VectorClass||Vector->GetOuter()!=VectorOuter||Vector->GetPathName()!=VectorPath||Vector->GetFlags()!=VectorFlags||Vector->GetPackage()->IsDirty()!=VectorDirty||Vector->GetStructureSize()!=sizeof(FVector))
        { return Bad(TEXT("array_vector_target_changed"),TEXT("Selected Blueprint/graph/current classes or exact native Vector identity changed; no next SDK.")); }
        if(Node.IsValid())
        {
            UEdGraphNode* N=Node.Get();int32 Count=0;for(const auto& Current:G->Nodes) { Count+=Current.Get()==N?1:0; }
            if(!EnumLoaded(N)||N->GetClass()!=NodeClass.Get()||N->GetOuter()!=G||N->GetPackage()!=B->GetPackage()||N->GetPathName()!=NodePath||Count!=(Attached?1:0)||(GuidFixed&&N->NodeGuid!=NodeGuid)) { return Bad(TEXT("array_vector_node_changed"),TEXT("Fresh Vector node no longer has its exact current identity/ownership.")); }
            if(FunctionAssigned)
            {
                auto* Call=Cast<UK2Node_CallFunction>(N);if(!Call||!EnumLoaded(FunctionOwner.Get())||!EnumLoaded(Function.Get())||Function->GetOuterUClass()!=FunctionOwner.Get()||FunctionOwner->GetDefaultObject(false)!=FunctionDefault.Get()
                    ||Call->FunctionReference.GetMemberParentClass()!=FunctionOwner.Get()||Call->FunctionReference.GetMemberName()!=Function->GetFName()||Call->FunctionReference.GetMemberGuid().IsValid()||Call->FunctionReference.IsSelfContext()) { return Bad(TEXT("array_vector_function_changed"),TEXT("Fresh Vector array node no longer targets the exact captured native array function.")); }
            }
            if(PinsFixed)
            {
                if(N->Pins.Num()!=Roles.Num()) { return Bad(TEXT("array_vector_pins_changed"),TEXT("Fresh Vector physical pin array changed.")); }
                for(int32 I=0;I<Roles.Num();++I) { const auto& Role=Roles[I];const UEdGraphPin* P=N->Pins[I];if(P!=Role.Address||!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||P->PinId!=Role.Id||P->PinName!=Role.Name||P->Direction!=Role.Direction||P->PinType!=Role.Type||FEnumArrayScope::PinFlags(P)!=Role.Flags||P->DefaultObject!=Role.DefaultObject||!P->DefaultTextValue.IsEmpty()||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return Bad(TEXT("array_vector_pins_changed"),TEXT("Captured Vector physical pin ID/type/flags/links changed during callback.")); } }
            }
        }
        return true;
    }
    bool Init(UBlueprint* B,UEdGraph* G,const UEdGraphSchema_K2* S,UScriptStruct* V,FResult& R)
    {
        Reply=&R;if(!EnumContext()||IsPlaySessionActive()) { return Bad(TEXT("array_vector_editor_required"),TEXT("Safe Editor GT context required before new Vector node route.")); }Editor=GEditor;World=GEditor->GetEditorWorldContext().World();
        if(!EnumLoaded(B)||!EnumLoaded(G)||!EnumLoaded(V)||!EnumLoaded(B->GeneratedClass.Get())||!EnumLoaded(B->SkeletonGeneratedClass.Get())) { return Bad(TEXT("array_vector_target_changed"),TEXT("Current loaded Blueprint/graph/native Vector required.")); }
        Blueprint.Reset(B);Graph.Reset(G);Schema.Reset(S);Vector.Reset(V);Parent.Reset(B->ParentClass.Get());Generated.Reset(B->GeneratedClass.Get());Skeleton.Reset(B->SkeletonGeneratedClass.Get());
        BlueprintClass=B->GetClass();BlueprintOuter=B->GetOuter();BlueprintPath=B->GetPathName();GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;
        VectorClass=V->GetClass();VectorOuter=V->GetOuter();VectorPath=V->GetPathName();VectorFlags=V->GetFlags();VectorDirty=V->GetPackage()->IsDirty();
        GeneratedClass=Generated->GetClass();SkeletonClass=Skeleton->GetClass();GeneratedOuter=Generated->GetOuter();SkeletonOuter=Skeleton->GetOuter();GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();GeneratedFlags=Generated->GetFlags();SkeletonFlags=Skeleton->GetFlags();return Check();
    }
    bool SetNode(UEdGraphNode* N,UClass* Class)
    {
        if(!Context()) { return false; }if(!EnumLoaded(Blueprint.Get())||!EnumLoaded(Graph.Get())||!EnumLoaded(N)||N->GetClass()!=Class||N->GetOuter()!=Graph.Get()||N->GetPackage()!=Blueprint->GetPackage()) { return Bad(TEXT("array_vector_node_changed"),TEXT("Public Vector allocation return cannot be adopted as the exact fresh node.")); }
        Node.Reset(N);NodeClass.Reset(Class);NodePath=N->GetPathName();return Check();
    }
    bool FreezePins()
    {
        if(!Check()||!Node.IsValid()||Node->Pins.Num()>512) { return false; }Roles.Reset();TSet<FGuid> Ids;
        for(UEdGraphPin* P:Node->Pins) { if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=Node.Get()||!P->PinId.IsValid()||Ids.Contains(P->PinId)) { return Bad(TEXT("array_vector_pins_changed"),TEXT("Fresh Vector physical pins have invalid IDs/ownership.")); }Ids.Add(P->PinId);FEnumArrayScope::FPinRole Role;Role.Address=P;Role.Id=P->PinId;Role.Name=P->PinName;Role.Direction=P->Direction;Role.Type=P->PinType;Role.Flags=FEnumArrayScope::PinFlags(P);Role.DefaultObject=P->DefaultObject;Roles.Add(MoveTemp(Role)); }PinsFixed=true;return Check();
    }
    bool ExpectType(UEdGraphPin* P,const FEdGraphPinType& Type) { if(!Check()) { return false; }for(auto& Role:Roles) { if(Role.Address==P) { Role.Type=Type;return true; } }return Bad(TEXT("array_vector_pins_changed"),TEXT("Type write does not select a captured fresh Vector pin.")); }
};

struct FDescriptorSource;
struct FContext
{
    TSharedPtr<Function::FPresetLoaderNameArrayScope> NameScope;
    TSharedPtr<FEnumArrayScope> EnumScope;
    TSharedPtr<FVectorArrayScope> VectorScope;
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    const UEdGraphSchema_K2* Schema = nullptr;
    UClass* NodeClass = nullptr;
    int32 X = 0, Y = 0, InputCount = 1;
    bool bCountSupplied = false, bValuesSupplied = false;
    FString ElementType;
    FString DescriptorPath;TSharedPtr<FDescriptorSource> DescriptorSource;
    TStrongObjectPtr<UClass> DescriptorParent,DescriptorGenerated,DescriptorSkeleton;bool DescriptorTargetBound=false;
    TArray<TSharedPtr<FJsonValue>> DescriptorRawValues;TArray<bool> DescriptorObjectInputs;
    TArray<TStrongObjectPtr<UObject>> DescriptorExpectedObjects;
    // OL: read-only target fence exists only around actual source/default loads.
    bool DescriptorLoadTargetReady=false;
    TStrongObjectPtr<UEditorEngine> DescriptorLoadEditor;TStrongObjectPtr<UWorld> DescriptorLoadWorld;
    TStrongObjectPtr<UPackage> DescriptorLoadPackage;decltype(GUndo) DescriptorLoadUndo=nullptr;
    UObject* DescriptorLoadOuter=nullptr;UClass* DescriptorLoadClass=nullptr;FString DescriptorLoadTargetPath;
    EObjectFlags DescriptorLoadTargetFlags=RF_NoFlags,DescriptorLoadPackageObjectFlags=RF_NoFlags;
    uint32 DescriptorLoadPackageFlags=0;bool DescriptorLoadDirty=false;
    struct FLoadClassRole{TStrongObjectPtr<UClass> Hold;UClass* Class=nullptr;UObject* Outer=nullptr;FString Path;EObjectFlags Flags=RF_NoFlags;uint32 ClassFlags=0;TStrongObjectPtr<UObject> Default;UClass* DefaultClass=nullptr;UObject* DefaultOuter=nullptr;FString DefaultPath;EObjectFlags DefaultFlags=RF_NoFlags;};
    TArray<FLoadClassRole> DescriptorLoadClasses;TSharedPtr<FJsonObject> DescriptorLoadBaseline;
    TArray<TSharedPtr<FJsonValue>> DescriptorLoadPrefix;
    FEdGraphPinType ElementPinType;
    struct FExtendedPin{UEdGraphPin* Address;FGuid Id;FName Name;EEdGraphPinDirection Direction;FEdGraphPinType Type;};
    struct FExtendedNode{TStrongObjectPtr<UEdGraphNode> Hold;UEdGraph* Graph;int32 Index;FGuid Guid;TArray<FExtendedPin> Pins;};
    struct FExtendedGraph{TStrongObjectPtr<UEdGraph> Hold;int32 NodeCount;};
    TArray<FExtendedNode> ExtendedOld;TArray<FExtendedGraph> ExtendedGraphs;TArray<FExtendedPin> ExtendedPins;
    TStrongObjectPtr<UBlueprint> ExtendedBP;TStrongObjectPtr<UEdGraph> ExtendedGraph;
    TStrongObjectPtr<UClass> ExtendedClass;TStrongObjectPtr<const UEdGraphSchema_K2> ExtendedSchema;
    TStrongObjectPtr<UK2Node_MakeArray> ExtendedCreated;FResult* ExtendedResult=nullptr;
    bool ExtendedBound=false,ExtendedAttached=false,ExtendedPinsFixed=false;
    TArray<FString> ExtendedSchemaValues;
    TArray<FString> Values, DefaultsBeforeValues;
    TSharedRef<FJsonObject> Before = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> NativeBefore = MakeShared<FJsonObject>();
};
// JP: original Shared.h GetPinDefaultStringForType/GetPinDefaultString for
// name/string values only. Literal text is not an identifier; keep empty/space/
// Unicode/control characters admitted by the existing bounded JSON parser.
bool TextLiteral(const TSharedPtr<FJsonValue>& Value,bool Name,FString& Out,FResult& R)
{
    Out.Reset();
    if(!Value){return Error(R,TEXT("make_array_text_literal_invalid"),TEXT("A parsed bounded JSON value is required."));}
    if(Value->Type==EJson::String){Out=Value->AsString();}
    else if(Value->Type==EJson::Boolean){Out=Value->AsBool()?TEXT("true"):TEXT("false");}
    else if(Value->Type==EJson::Number){Out=FString::SanitizeFloat(Value->AsNumber());}
    else if(Value->Type==EJson::Array||Value->Type==EJson::Object)
    {if(!FJsonSerializer::Serialize(Value,TEXT(""),TJsonWriterFactory<>::Create(&Out))){return Error(R,TEXT("make_array_text_literal_invalid"),TEXT("Original JSON fallback serialization failed before allocation."));}}
    else if(Value->Type!=EJson::Null){return Error(R,TEXT("make_array_text_literal_invalid"),TEXT("Unsupported JSON notation for original text conversion."));}
    // SDK FNameProperty imports by FName(Buffer); this bound prevents its fatal
    // NAME_SIZE path. It does not impose SafeName/Identifier on literal values.
    if(Out.Len()>65536||(Name&&Out.Len()>=NAME_SIZE))
    {return Error(R,TEXT("make_array_text_literal_bound"),TEXT("Converted string exceeds65536 UTF16 units or name reaches the native NAME_SIZE limit."));}
    return true;
}

bool ExtendedNumberLiteral(const TSharedPtr<FJsonValue>& V,const FEdGraphPinType& Type,FString& Out,FResult& R)
{
    if(V&&V->Type==EJson::Number)
    {
        const double Number=V->AsNumber();
        if(!FMath::IsFinite(Number))return Error(R,TEXT("make_array_numeric_literal_invalid"),TEXT("Numeric default must be finite; no engine mutation dispatched."));
        if(Type.PinCategory==UEdGraphSchema_K2::PC_Int64)
        {
            if(Number<-9223372036854775808.0||Number>=9223372036854775808.0)return Error(R,TEXT("make_array_int64_numeric_range"),TEXT("JSON double cannot be converted safely to int64; use an exact int64 string within SDK bounds."));
            Out=FString::Printf(TEXT("%lld"),static_cast<int64>(Number));return true;
        }
    }
    return TextLiteral(V,false,Out,R);
}
bool ExtendedPrimitive(const FContext& C)
{return C.ElementType==TEXT("int64")||C.ElementType==TEXT("float")||C.ElementType==TEXT("double")||C.ElementType==TEXT("text");}
// OC: loaded descriptor source observation is request-local. No default buffer
// addresses, CDO creation, loading, compile, save or runtime evaluation is used.
struct FDescriptorSource
{
    struct FObjectRole
    {
        TStrongObjectPtr<UObject> Hold,OuterHold;TStrongObjectPtr<UClass> ClassHold;TStrongObjectPtr<UPackage> Package;
        UObject* Outer=nullptr;UClass* Class=nullptr;FString Name,PackageName;
        EObjectFlags Flags=RF_NoFlags;EObjectFlags PackageObjectFlags=RF_NoFlags;uint32 PackageFlags=0;bool Dirty=false;
        UStruct* Super=nullptr;TStrongObjectPtr<UClass> Within;int32 Size=0,Alignment=0;uint32 TypeFlags=0;
        TStrongObjectPtr<UObject> ExistingCDO;UClass* CDOClass=nullptr;UObject* CDOOuter=nullptr;EObjectFlags CDOFlags=RF_NoFlags;bool CDOObserved=false,FieldsObserved=false,FieldsCaptured=false,MetadataCaptured=false;TMap<FName,FString> Metadata;
        TArray<FProperty*> DirectFields;
    };
    struct FFieldRole
    {
        FProperty* Address=nullptr;FFieldVariant Owner;FFieldClass* Class=nullptr;FName Name;
        uint64 Flags=0;int32 Dim=0,Offset=0,Size=0;TMap<FName,FString> Metadata;
        FProperty *Inner=nullptr,*Key=nullptr,*Value=nullptr;FFieldClass* FieldPathClass=nullptr;FString FieldPathClassName;TStrongObjectPtr<UObject> Binding1,Binding2;bool BindingsCaptured=false;
    };
    struct FFileRole{FString PackageName,Filename;bool Exists=false;int64 Bytes=-1;FString SHA1;};
    TArray<FObjectRole> Objects;TArray<FFieldRole> Fields;TArray<FFileRole> Files;
    TStrongObjectPtr<UStruct> Type;TArray<TStrongObjectPtr<UObject>> DefaultObjects;
    FString Kind;bool Ready=false,MetadataReady=false,BindingsReady=false;
};
bool Descriptor(const FContext& C){return C.ElementType==TEXT("object")||C.ElementType==TEXT("class")||C.ElementType==TEXT("rotator")||C.ElementType==TEXT("transform")||C.ElementType==TEXT("struct");}
bool TrackedMakeArray(const FContext& C){return ExtendedPrimitive(C)||Descriptor(C);}
bool DescriptorPlain(FContext& C,FResult& R);
bool DescriptorCurrent(FContext& C,FResult& R);
bool ExtendedCurrentPlain(FContext& C,FResult& R,const UK2Node_MakeArray* Node);
bool Capture(FContext& C,const TSharedRef<FJsonObject>& Out);
bool DescriptorLoadTargetCurrent(FContext& C,FResult& R);
bool DescriptorBad(FContext& C,FResult& R,const TCHAR* Property,const FString& Expected,const FString& Actual)
{
    if(!R.Data->HasField(TEXT("descriptor_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("expected_actual_observed"),false);J->SetStringField(TEXT("diagnostic_kind"),TEXT("compound_guard_or_copied_operands"));R.Data->SetObjectField(TEXT("descriptor_first_mismatch"),J);}
    R.Data->SetBoolField(TEXT("descriptor_followup_sdk_stopped"),true);R.Data->SetBoolField(TEXT("make_array_descriptor_source_observed_unchanged"),false);R.bChangeStateKnown=false;
    return Error(R,TEXT("make_array_descriptor_current_changed"),TEXT("Descriptor/target current guard failed; retained actual prefix, no rollback, retry or followup SDK."));
}
bool DescriptorFile(const FString& PackageName,FDescriptorSource::FFileRole& Out)
{
    Out.PackageName=PackageName;Out.Exists=FPackageName::DoesPackageExist(PackageName,&Out.Filename);if(!Out.Exists)Out.Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
    Out.Bytes=Out.Exists?IFileManager::Get().FileSize(*Out.Filename):-1;
    if(Out.Exists){TArray<uint8> Bytes;if(Out.Bytes<0||Out.Bytes>268435456||!FFileHelper::LoadFileToArray(Bytes,*Out.Filename))return false;FSHAHash Hash;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash.Hash);Out.SHA1=Hash.ToString();}return true;
}
bool DescriptorRuntime(const UObject* O)
{
    if(!EnumLoaded(O)||O->IsEditorOnly()||!EnumLoaded(O->GetPackage())||O->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))return false;
    const FString P=O->GetPackage()->GetName();if(!(P==TEXT("/Script/CoreUObject")||P==TEXT("/Script/Engine")||P.StartsWith(TEXT("/Game/"))))return false;
    // The asset package alone cannot authorize an Editor/plugin-derived class.
    for(const UClass* K=O->GetClass();K;K=K->GetSuperClass()){if(!EnumLoaded(K)||K->IsEditorOnly())return false;const FString M=K->GetPackage()->GetName();if(M!=TEXT("/Script/CoreUObject")&&M!=TEXT("/Script/Engine")&&!M.StartsWith(TEXT("/Game/")))return false;}return true;
}
bool DescriptorAddObject(FContext& C,FResult& R,UObject* O,bool WithFields)
{
    auto& S=*C.DescriptorSource;
    for(const auto& E:S.Objects)if(E.Hold.Get()==O)return true;
    if(!DescriptorRuntime(O)||O->GetPackage()==C.Blueprint->GetPackage()||S.Objects.Num()>=128)return Error(R,TEXT("make_array_descriptor_source_pending"),TEXT("This resolved-source slice requires a separate runtime CoreUObject/Engine or Game source package within the observation bound. Other modules, aliases, unloaded Engine paths and same-target source remain incomplete."));
    FDescriptorSource::FObjectRole E;E.Hold.Reset(O);E.Package.Reset(O->GetPackage());E.Outer=O->GetOuter();E.OuterHold.Reset(E.Outer);E.Class=O->GetClass();E.ClassHold.Reset(E.Class);E.Name=O->GetPathName();E.PackageName=E.Package->GetName();E.Flags=O->GetFlags();E.PackageObjectFlags=E.Package->GetFlags();E.PackageFlags=E.Package->GetPackageFlags();E.Dirty=E.Package->IsDirty();
    if(auto* K=Cast<UClass>(O)){if(!K->ClassWithin.IsResolved()||!K->ClassDefaultObject.IsResolved())return Error(R,TEXT("make_array_descriptor_cdo_pending"),TEXT("Existing CDO handle is unresolved; this slice does not resolve/create it."));E.Within.Reset(K->ClassWithin.Get());E.ExistingCDO.Reset(K->ClassDefaultObject.Get());E.CDOObserved=true;if(E.ExistingCDO.IsValid()){if(!EnumLoaded(E.ExistingCDO.Get()))return Error(R,TEXT("make_array_descriptor_cdo_pending"),TEXT("Existing CDO is not current/loaded; no CDO creation or load requested."));E.CDOClass=E.ExistingCDO->GetClass();E.CDOOuter=E.ExistingCDO->GetOuter();E.CDOFlags=E.ExistingCDO->GetFlags();}E.TypeFlags=uint32(K->GetClassFlags());if(K->HasAnyClassFlags(CLASS_NewerVersionExists|CLASS_Deprecated))return Error(R,TEXT("make_array_descriptor_class_pending"),TEXT("Retired/deprecated class source is not admitted."));}
    if(auto* U=Cast<UStruct>(O)){E.Super=U->GetSuperStruct();E.Size=U->GetStructureSize();E.Alignment=U->GetMinAlignment();if(auto* SS=Cast<UScriptStruct>(U))E.TypeFlags=uint32(SS->StructFlags);E.FieldsObserved=WithFields;if(WithFields)for(TFieldIterator<FProperty> It(U,EFieldIteratorFlags::ExcludeSuper);It;++It){if(E.DirectFields.Num()>=2048)return Error(R,TEXT("make_array_descriptor_source_bound"),TEXT("Direct field observation exceeds 2048."));E.DirectFields.Add(*It);}}
    const bool IsGame=E.PackageName.StartsWith(TEXT("/Game/"));bool FileKnown=false;for(const auto& F:S.Files)FileKnown|=F.PackageName==E.PackageName;
    if(IsGame&&!FileKnown){FDescriptorSource::FFileRole F;if(!DescriptorFile(E.PackageName,F))return Error(R,TEXT("make_array_descriptor_source_file_unobservable"),TEXT("Source file snapshot could not be obtained before mutation."));S.Files.Add(MoveTemp(F));}
    S.Objects.Add(MoveTemp(E));return true;
}
bool DescriptorAddField(FContext& C,FResult& R,FProperty* P,FFieldVariant Owner,int32 Depth)
{
    auto& S=*C.DescriptorSource;if(!P||Depth>16||S.Fields.Num()>=4096)return Error(R,TEXT("make_array_descriptor_field_bound"),TEXT("Source field hierarchy exceeds the bounded descriptor observation."));
    if(P->GetOwnerVariant()!=Owner)return DescriptorBad(C,R,TEXT("field_owner"),FString::Printf(TEXT("%p"),Owner.GetRawPointer()),FString::Printf(TEXT("%p"),P->GetOwnerVariant().GetRawPointer()));
    FDescriptorSource::FFieldRole E;E.Address=P;E.Owner=Owner;E.Class=P->GetClass();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Dim=P->ArrayDim;E.Offset=P->GetOffset_ForInternal();E.Size=P->GetSize();
    if(const auto* M=P->GetMetaDataMap()){if(M->Num()>256)return Error(R,TEXT("make_array_descriptor_metadata_bound"),TEXT("Field metadata exceeds256 entries."));E.Metadata=*M;}
    if(auto* A=CastField<FOptionalProperty>(P))E.Inner=A->GetValueProperty();
    if(auto* A=CastField<FFieldPathProperty>(P)){E.FieldPathClass=A->PropertyClass;if(E.FieldPathClass&&!FFieldClass::GetAllFieldClasses().Contains(E.FieldPathClass))return Error(R,TEXT("make_array_fieldpath_class_unlinked_pending"),TEXT("FieldPath PropertyClass is not a registered linked FFieldClass; no unheld class-name dereference."));E.FieldPathClassName=E.FieldPathClass?E.FieldPathClass->GetName():FString();}
    if(auto* A=CastField<FEnumProperty>(P))E.Inner=A->GetUnderlyingProperty();
    if(auto* A=CastField<FArrayProperty>(P))E.Inner=A->Inner;
    if(auto* A=CastField<FSetProperty>(P))E.Inner=A->ElementProp;
    if(auto* A=CastField<FMapProperty>(P)){E.Key=A->KeyProp;E.Value=A->ValueProp;}
    const auto Inner=E.Inner,Key=E.Key,Value=E.Value;S.Fields.Add(MoveTemp(E));
    if(Inner&&!DescriptorAddField(C,R,Inner,FFieldVariant(P),Depth+1))return false;
    if(Key&&!DescriptorAddField(C,R,Key,FFieldVariant(P),Depth+1))return false;
    return !Value||DescriptorAddField(C,R,Value,FFieldVariant(P),Depth+1);
}
bool DescriptorPlain(FContext& C,FResult& R)
{
    if(!Descriptor(C)||!C.DescriptorSource.IsValid()||!C.DescriptorSource->Ready)return true;
    if(R.Data->HasField(TEXT("descriptor_followup_sdk_stopped")))return false;
    auto& S=*C.DescriptorSource;
    if(!ExtendedCurrentPlain(C,R,C.ExtendedCreated.Get()))return false;
    for(const auto& E:S.Objects)
    {
        UObject* O=E.Hold.Get();if(!EnumLoaded(O)||O->GetClass()!=E.Class||O->GetOuter()!=E.Outer||O->GetFlags()!=E.Flags||O->GetPathName()!=E.Name||O->GetPackage()!=E.Package.Get()||!EnumLoaded(E.Package.Get())||E.Package->GetFlags()!=E.PackageObjectFlags||E.Package->GetPackageFlags()!=E.PackageFlags||E.Package->IsDirty()!=E.Dirty)return DescriptorBad(C,R,TEXT("held_source_identity_package"),E.Name,TEXT("compound identity/flags/package mismatch"));
        if(auto* U=Cast<UStruct>(O)){if(U->GetSuperStruct()!=E.Super||U->GetStructureSize()!=E.Size||U->GetMinAlignment()!=E.Alignment)return DescriptorBad(C,R,TEXT("struct_layout_super"),E.Name,TEXT("compound layout/super mismatch"));if(E.FieldsObserved){TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(U,EFieldIteratorFlags::ExcludeSuper);It;++It)Actual.Add(*It);if(Actual!=E.DirectFields)return DescriptorBad(C,R,TEXT("direct_field_membership"),E.Name,TEXT("different direct field address inventory"));}}
        if(auto* K=Cast<UClass>(O)){if(uint32(K->GetClassFlags())!=E.TypeFlags||!K->ClassWithin.IsResolved()||K->ClassWithin.Get()!=E.Within.Get()||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=E.ExistingCDO.Get())return DescriptorBad(C,R,TEXT("class_flags_existing_cdo"),E.Name,TEXT("different class flags/CDO handle"));}
        if(E.ExistingCDO.IsValid()&&(!EnumLoaded(E.ExistingCDO.Get())||E.ExistingCDO->GetClass()!=E.CDOClass||E.ExistingCDO->GetOuter()!=E.CDOOuter||E.ExistingCDO->GetFlags()!=E.CDOFlags))return DescriptorBad(C,R,TEXT("existing_cdo_identity"),E.Name,TEXT("compound existing CDO class/outer/flags mismatch"));
        if(auto* SS=Cast<UScriptStruct>(O))if(uint32(SS->StructFlags)!=E.TypeFlags)return DescriptorBad(C,R,TEXT("struct_flags"),LexToString(E.TypeFlags),LexToString(uint32(SS->StructFlags)));
    }
    // Owners/direct-membership are closed above before any retained FField use.
    for(const auto& E:S.Fields)
    {
        FProperty* P=E.Address;if(P->GetOwnerVariant()!=E.Owner||P->GetClass()!=E.Class||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->ArrayDim!=E.Dim||P->GetOffset_ForInternal()!=E.Offset||P->GetSize()!=E.Size)return DescriptorBad(C,R,TEXT("field_identity_layout"),E.Name.ToString(),TEXT("compound owner/type/layout mismatch"));
        if(auto* A=CastField<FOptionalProperty>(P))if(A->GetValueProperty()!=E.Inner)return DescriptorBad(C,R,TEXT("optional_value_property"),E.Name.ToString(),TEXT("different optional value field"));
        if(auto* A=CastField<FFieldPathProperty>(P)){if(A->PropertyClass!=E.FieldPathClass||(E.FieldPathClass&&(!FFieldClass::GetAllFieldClasses().Contains(E.FieldPathClass)||E.FieldPathClass->GetName()!=E.FieldPathClassName)))return DescriptorBad(C,R,TEXT("field_path_property_class"),E.FieldPathClassName,TEXT("different or unregistered FFieldClass constraint"));}
        if(auto* A=CastField<FEnumProperty>(P))if(A->GetUnderlyingProperty()!=E.Inner)return DescriptorBad(C,R,TEXT("enum_underlying"),E.Name.ToString(),TEXT("different underlying field"));
        if(auto* A=CastField<FArrayProperty>(P))if(A->Inner!=E.Inner)return DescriptorBad(C,R,TEXT("array_inner"),E.Name.ToString(),TEXT("different Inner"));
        if(auto* A=CastField<FSetProperty>(P))if(A->ElementProp!=E.Inner)return DescriptorBad(C,R,TEXT("set_element"),E.Name.ToString(),TEXT("different element"));
        if(auto* A=CastField<FMapProperty>(P))if(A->KeyProp!=E.Key||A->ValueProp!=E.Value)return DescriptorBad(C,R,TEXT("map_key_value"),E.Name.ToString(),TEXT("different Key/Value"));
        const auto* M=P->GetMetaDataMap();if((M?M->Num():0)!=E.Metadata.Num())return DescriptorBad(C,R,TEXT("field_metadata"),E.Name.ToString(),TEXT("different count"));for(const auto& V:E.Metadata){const FString* Actual=M?M->Find(V.Key):nullptr;if(!Actual||*Actual!=V.Value)return DescriptorBad(C,R,TEXT("field_metadata_value"),V.Value,Actual?*Actual:TEXT("missing"));}
    }
    return true;
}
// Public reflected binding getters may resolve an ObjectHandle. Strong-hold
// each returned object, then close plain physical/source membership before the
// next field access. This is separate from metadata Current to avoid recursion.
bool DescriptorFieldBinding(FContext& C,FResult& R,FProperty* P,TStrongObjectPtr<UObject>& One,TStrongObjectPtr<UObject>& Two)
{
    if(!DescriptorPlain(C,R))return false;
    if(auto* ClassField=CastField<FClassProperty>(P)){One.Reset(ClassField->PropertyClass.Get());if(!DescriptorPlain(C,R))return false;Two.Reset(ClassField->MetaClass.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* SoftClassField=CastField<FSoftClassProperty>(P)){One.Reset(SoftClassField->PropertyClass.Get());if(!DescriptorPlain(C,R))return false;Two.Reset(SoftClassField->MetaClass.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* InterfaceField=CastField<FInterfaceProperty>(P)){One.Reset(InterfaceField->InterfaceClass.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* DelegateField=CastField<FDelegateProperty>(P)){One.Reset(DelegateField->SignatureFunction.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* MulticastField=CastField<FMulticastDelegateProperty>(P)){One.Reset(MulticastField->SignatureFunction.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* ObjectField=CastField<FObjectPropertyBase>(P)){One.Reset(ObjectField->PropertyClass.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* StructField=CastField<FStructProperty>(P)){One.Reset(StructField->Struct.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* ByteField=CastField<FByteProperty>(P)){One.Reset(ByteField->Enum.Get());if(!DescriptorPlain(C,R))return false;}
    else if(auto* EnumField=CastField<FEnumProperty>(P)){One.Reset(EnumField->GetEnum());if(!DescriptorPlain(C,R))return false;}
    return true;
}
bool DescriptorCurrent(FContext& C,FResult& R)
{
    if(!Descriptor(C)||!C.DescriptorSource.IsValid()||!C.DescriptorSource->Ready)return true;
    if(!DescriptorPlain(C,R))return false;auto& S=*C.DescriptorSource;
    for(const auto& E:S.Objects)
    {
        const auto* M=FMetaData::GetMapForObject(E.Hold.Get());if(!DescriptorPlain(C,R))return false;TMap<FName,FString> Copy;if(M){if(M->Num()>256)return DescriptorBad(C,R,TEXT("source_metadata_bound"),TEXT("<=256"),LexToString(M->Num()));Copy=*M;}
        if(!DescriptorPlain(C,R))return false;
        if(S.MetadataReady&&E.MetadataCaptured){if(Copy.Num()!=E.Metadata.Num())return DescriptorBad(C,R,TEXT("source_metadata"),E.Name,TEXT("different count"));for(const auto& V:E.Metadata){const FString* Actual=Copy.Find(V.Key);if(!Actual||*Actual!=V.Value)return DescriptorBad(C,R,TEXT("source_metadata_value"),V.Value,Actual?*Actual:TEXT("missing"));}}
    }
    if(S.BindingsReady)for(const auto& E:S.Fields){TStrongObjectPtr<UObject> One,Two;if(!DescriptorFieldBinding(C,R,E.Address,One,Two))return false;if(One.Get()!=E.Binding1.Get()||Two.Get()!=E.Binding2.Get())return DescriptorBad(C,R,TEXT("field_type_binding"),E.Name.ToString(),TEXT("different resolved type binding"));}
    // Disk checks run only at source bind/final commit observation, not every
    // scratch/pin SDK return. Actual objects/fields/metadata stay guarded above.
    return true;
}
TSharedRef<FJsonObject> DescriptorState(FContext& C)
{
    auto& S=*C.DescriptorSource;auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("descriptor_kind"),C.ElementType);J->SetStringField(TEXT("type_path"),S.Type->GetPathName());J->SetBoolField(TEXT("default_values_sampled"),false);
    TArray<TSharedPtr<FJsonValue>> Roots,Fields,Files;
    auto Meta=[](const TMap<FName,FString>& M){auto V=MakeShared<FJsonObject>();for(const auto& P:M)V->SetStringField(P.Key.ToString(),P.Value);return V;};
    for(const auto& E:S.Objects){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("path"),E.Name);O->SetStringField(TEXT("class_path"),Path(E.Class));O->SetStringField(TEXT("outer_path"),Path(E.Outer));O->SetStringField(TEXT("object_flags_string"),LexToString(uint32(E.Flags)));O->SetStringField(TEXT("package_path"),E.PackageName);O->SetStringField(TEXT("package_flags_string"),LexToString(E.PackageFlags));O->SetBoolField(TEXT("package_dirty"),E.Dirty);O->SetStringField(TEXT("super_path"),Path(E.Super));O->SetStringField(TEXT("within_path"),Path(E.Within.Get()));O->SetNumberField(TEXT("structure_size"),E.Size);O->SetNumberField(TEXT("alignment"),E.Alignment);O->SetBoolField(TEXT("class_layout_observed"),Cast<UClass>(E.Hold.Get())!=nullptr);O->SetStringField(TEXT("class_flags_string"),LexToString(Cast<UClass>(E.Hold.Get())?E.TypeFlags:0));O->SetBoolField(TEXT("struct_layout_observed"),Cast<UScriptStruct>(E.Hold.Get())!=nullptr);O->SetStringField(TEXT("struct_flags_string"),LexToString(Cast<UScriptStruct>(E.Hold.Get())?E.TypeFlags:0));auto D=MakeShared<FJsonObject>();D->SetBoolField(TEXT("exists"),E.ExistingCDO.IsValid());D->SetStringField(TEXT("path"),Path(E.ExistingCDO.Get()));D->SetStringField(TEXT("class_path"),E.ExistingCDO.IsValid()?Path(E.ExistingCDO->GetClass()):FString());D->SetStringField(TEXT("outer_path"),E.ExistingCDO.IsValid()?Path(E.ExistingCDO->GetOuter()):FString());D->SetStringField(TEXT("object_flags_string"),E.ExistingCDO.IsValid()?LexToString(uint32(E.ExistingCDO->GetFlags())):TEXT("0"));O->SetObjectField(TEXT("existing_cdo"),D);O->SetObjectField(TEXT("metadata"),Meta(E.Metadata));Roots.Add(MakeShared<FJsonValueObject>(O));}
    for(const auto& E:S.Fields){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("path"),E.Address->GetPathName());O->SetStringField(TEXT("name"),E.Name.ToString());O->SetStringField(TEXT("field_class"),E.Class->GetName());O->SetStringField(TEXT("owner_path"),E.Owner.GetPathName());O->SetStringField(TEXT("owner_kind"),E.Owner.IsUObject()?TEXT("uobject"):TEXT("field"));O->SetStringField(TEXT("flags_string"),LexToString(E.Flags));O->SetNumberField(TEXT("array_dim"),E.Dim);O->SetNumberField(TEXT("offset"),E.Offset);O->SetNumberField(TEXT("size"),E.Size);O->SetStringField(TEXT("field_path_class"),E.FieldPathClassName);O->SetStringField(TEXT("binding_path"),Path(E.Binding1.Get()));O->SetStringField(TEXT("secondary_binding_path"),Path(E.Binding2.Get()));O->SetObjectField(TEXT("metadata"),Meta(E.Metadata));Fields.Add(MakeShared<FJsonValueObject>(O));}
    for(const auto& E:S.Files){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("package_path"),E.PackageName);O->SetStringField(TEXT("filename"),E.Filename);O->SetBoolField(TEXT("exists"),E.Exists);O->SetNumberField(TEXT("bytes"),E.Bytes);O->SetStringField(TEXT("sha1"),E.SHA1);Files.Add(MakeShared<FJsonValueObject>(O));}
    J->SetArrayField(TEXT("roots"),Roots);J->SetArrayField(TEXT("fields"),Fields);J->SetArrayField(TEXT("files"),Files);return J;
}
// An explicit Load is a mutation-capable callback boundary even though this
// operation never saves or edits the source asset. Keep this phase local and
// preserve loaded-only behavior when FindObject already resolves the request.
bool DescriptorLoadTargetCurrent(FContext& C,FResult& R)
{
    if(!C.DescriptorLoadTargetReady)return true;
    if(!EnumContext()||IsPlaySessionActive()||!EnumLoaded(C.DescriptorLoadEditor.Get())||GEditor!=C.DescriptorLoadEditor.Get()||!EnumLoaded(C.DescriptorLoadWorld.Get())||GEditor->GetEditorWorldContext().World()!=C.DescriptorLoadWorld.Get()||C.DescriptorLoadWorld->bIsTearingDown||GUndo!=C.DescriptorLoadUndo)
        return DescriptorBad(C,R,TEXT("load_target_context"),TEXT("captured Editor/world/transaction"),TEXT("compound context mismatch"));
    auto* B=C.ExtendedBP.Get();auto* Package=C.DescriptorLoadPackage.Get();
    if(!EnumLoaded(B)||!EnumLoaded(Package)||B->GetClass()!=C.DescriptorLoadClass||B->GetOuter()!=C.DescriptorLoadOuter||B->GetPathName()!=C.DescriptorLoadTargetPath||B->GetFlags()!=C.DescriptorLoadTargetFlags||B->GetPackage()!=Package||Package->GetFlags()!=C.DescriptorLoadPackageObjectFlags||Package->GetPackageFlags()!=C.DescriptorLoadPackageFlags||Package->IsDirty()!=C.DescriptorLoadDirty)
        return DescriptorBad(C,R,TEXT("load_target_identity_package"),C.DescriptorLoadTargetPath,TEXT("compound target/package mismatch"));
    if(!B->ParentClass.GetGCPtr().IsResolved()||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=C.DescriptorParent.Get()||B->GeneratedClass.GetGCPtr()!=C.DescriptorGenerated.Get()||B->SkeletonGeneratedClass.GetGCPtr()!=C.DescriptorSkeleton.Get())
        return DescriptorBad(C,R,TEXT("load_target_class_slots"),TEXT("captured resolved parent/generated/skeleton"),TEXT("different or unresolved current slot"));
    for(const auto& Role:C.DescriptorLoadClasses)
    {
        auto* K=Role.Hold.Get();if(!EnumLoaded(K)||K->GetClass()!=Role.Class||K->GetOuter()!=Role.Outer||K->GetPathName()!=Role.Path||K->GetFlags()!=Role.Flags||uint32(K->GetClassFlags())!=Role.ClassFlags)return DescriptorBad(C,R,TEXT("load_target_class_role"),Role.Path,TEXT("compound held class mismatch"));
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto DefaultHandle=K->ClassDefaultObject;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        if(!DefaultHandle.IsResolved()||DefaultHandle!=Role.Default.Get())return DescriptorBad(C,R,TEXT("load_target_existing_cdo_slot"),Role.Path,TEXT("different or unresolved existing CDO slot"));
        if(Role.Default.IsValid()){auto* D=Role.Default.Get();if(!EnumLoaded(D)||D->GetClass()!=Role.DefaultClass||D->GetOuter()!=Role.DefaultOuter||D->GetPathName()!=Role.DefaultPath||D->GetFlags()!=Role.DefaultFlags)return DescriptorBad(C,R,TEXT("load_target_existing_cdo_identity"),Role.DefaultPath,TEXT("compound held CDO mismatch"));}
    }
    return true;
}
bool BeginDescriptorLoadTarget(FContext& C,FResult& R)
{
    if(C.DescriptorLoadTargetReady)return ExtendedCurrentPlain(C,R,nullptr);
    if(!EnumContext()||IsPlaySessionActive()||!C.DescriptorTargetBound)return DescriptorBad(C,R,TEXT("load_target_preflight_context"),TEXT("bound target in safe Editor context"),TEXT("context or bound target unavailable"));
    if(!ExtendedCurrentPlain(C,R,nullptr))return false;
    C.DescriptorLoadEditor.Reset(GEditor);C.DescriptorLoadWorld.Reset(GEditor->GetEditorWorldContext().World());C.DescriptorLoadUndo=GUndo;
    if(!EnumLoaded(C.DescriptorLoadWorld.Get())||C.DescriptorLoadWorld->bIsTearingDown)return Error(R,TEXT("make_array_load_context_unavailable"),TEXT("Existing Editor world required before source load."));
    auto* B=C.ExtendedBP.Get();C.DescriptorLoadPackage.Reset(B->GetPackage());C.DescriptorLoadClass=B->GetClass();C.DescriptorLoadOuter=B->GetOuter();C.DescriptorLoadTargetPath=B->GetPathName();C.DescriptorLoadTargetFlags=B->GetFlags();
    C.DescriptorLoadPackageObjectFlags=C.DescriptorLoadPackage->GetFlags();C.DescriptorLoadPackageFlags=C.DescriptorLoadPackage->GetPackageFlags();C.DescriptorLoadDirty=C.DescriptorLoadPackage->IsDirty();
    for(UClass* K:{C.DescriptorParent.Get(),C.DescriptorGenerated.Get(),C.DescriptorSkeleton.Get()})
    {
        if(!K)continue;if(!EnumLoaded(K))return Error(R,TEXT("make_array_load_target_class_unavailable"),TEXT("Captured target classes must be loaded before source load."));
        FContext::FLoadClassRole Role;Role.Hold.Reset(K);Role.Class=K->GetClass();Role.Outer=K->GetOuter();Role.Path=K->GetPathName();Role.Flags=K->GetFlags();Role.ClassFlags=uint32(K->GetClassFlags());
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        const auto DefaultHandle=K->ClassDefaultObject;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
        if(!DefaultHandle.IsResolved())return Error(R,TEXT("make_array_load_target_cdo_unresolved"),TEXT("An unresolved existing target CDO handle is not resolved/created by this load slice."));
        Role.Default.Reset(DefaultHandle.Get());if(Role.Default.IsValid()){auto* D=Role.Default.Get();if(!EnumLoaded(D))return Error(R,TEXT("make_array_load_target_cdo_unavailable"),TEXT("Existing target CDO is not loaded."));Role.DefaultClass=D->GetClass();Role.DefaultOuter=D->GetOuter();Role.DefaultPath=D->GetPathName();Role.DefaultFlags=D->GetFlags();}C.DescriptorLoadClasses.Add(MoveTemp(Role));
    }
    C.DescriptorLoadTargetReady=true;
    auto Before=MakeShared<FJsonObject>();if(!Capture(C,Before)||!ExtendedCurrentPlain(C,R,nullptr))return DescriptorBad(C,R,TEXT("load_target_snapshot_before"),TEXT("complete current target snapshot"),TEXT("unobservable before source load"));
    C.DescriptorLoadBaseline=Before;R.Data->SetObjectField(TEXT("make_array_target_before_source_load"),Before);return true;
}
bool LoadDescriptorExactPath(FContext& C,FResult& R,const FString& Requested,UClass* ExpectedClass,const TCHAR* RoleName,TStrongObjectPtr<UObject>& Returned)
{
    if(!Requested.StartsWith(TEXT("/Game/"))||!Function::Pin::SafeName(Requested)||Requested.Contains(TEXT(":"))||Requested.Contains(TEXT("'"))||Requested.Contains(TEXT("\\"))||!FPackageName::IsValidObjectPath(Requested))
        return Error(R,TEXT("make_array_exact_path_load_pending"),TEXT("This load extension requires an exact full Game object path; short aliases, unloaded Engine sources and custom modules remain incomplete."));
    const FString PackagePath=FPackageName::ObjectPathToPackageName(Requested);
    if(PackagePath==C.Blueprint->GetPackage()->GetName())return Error(R,TEXT("make_array_same_target_source_pending"),TEXT("Source/default load from the target Blueprint package remains outside this slice."));
    if(!BeginDescriptorLoadTarget(C,R))return false;
    FDescriptorSource::FFileRole FileBefore;if(!DescriptorFile(PackagePath,FileBefore)||!ExtendedCurrentPlain(C,R,nullptr)||!DescriptorCurrent(C,R))return DescriptorBad(C,R,TEXT("load_source_file_before"),PackagePath,TEXT("file/current observation unavailable before load"));
    auto Step=MakeShared<FJsonObject>();Step->SetStringField(TEXT("role"),RoleName);Step->SetStringField(TEXT("requested_path"),Requested);Step->SetStringField(TEXT("stage"),TEXT("load_entered"));Step->SetBoolField(TEXT("find_object_observed_missing"),true);Step->SetBoolField(TEXT("attempted"),true);Step->SetBoolField(TEXT("returned"),false);Step->SetBoolField(TEXT("returned_strong_hold_completed"),false);Step->SetBoolField(TEXT("source_bind_completed"),false);Step->SetBoolField(TEXT("returned_nonnull"),false);Step->SetBoolField(TEXT("target_current_verified"),false);Step->SetBoolField(TEXT("source_file_unchanged_verified"),false);C.DescriptorLoadPrefix.Add(MakeShared<FJsonValueObject>(Step));R.Data->SetArrayField(TEXT("make_array_descriptor_load_prefix"),C.DescriptorLoadPrefix);
    R.Data->SetBoolField(TEXT("native_make_array_exact_path_resolution"),true);R.Data->SetBoolField(TEXT("descriptor_load_global_effects_unverified"),true);R.Data->SetBoolField(TEXT("descriptor_automatic_retry_safe"),false);
    R.Data->SetNumberField(TEXT("descriptor_load_calls"),C.DescriptorLoadPrefix.Num());
    UObject* Raw=StaticLoadObject(ExpectedClass,nullptr,*Requested);
    Returned.Reset(Raw); // first operation on the returned raw role: strong adoption, no introspection first
    Step->SetBoolField(TEXT("returned"),true);Step->SetBoolField(TEXT("returned_nonnull"),Raw!=nullptr);Step->SetBoolField(TEXT("returned_strong_hold_completed"),Raw!=nullptr&&Returned.Get()==Raw);Step->SetStringField(TEXT("stage"),TEXT("load_returned"));R.Data->SetNumberField(TEXT("descriptor_load_returns"),C.DescriptorLoadPrefix.Num());
    if(!ExtendedCurrentPlain(C,R,nullptr))return false;
    if(!Raw){R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("descriptor_followup_sdk_stopped"),true);return Error(R,TEXT("make_array_exact_path_load_failed"),TEXT("Explicit source/default load returned null; global effects are unverified and no subsequent SDK/retry is dispatched."));}
    auto After=MakeShared<FJsonObject>();if(!Capture(C,After)||!ExtendedCurrentPlain(C,R,nullptr)||!DescriptorCurrent(C,R))return DescriptorBad(C,R,TEXT("load_target_snapshot_after"),TEXT("complete current target/source snapshot"),TEXT("unobservable after explicit load"));
    R.Data->SetObjectField(TEXT("make_array_target_after_source_load"),After);
    if(Text(C.DescriptorLoadBaseline.ToSharedRef())!=Text(After))return DescriptorBad(C,R,TEXT("load_target_snapshot_changed"),TEXT("copied pre-load whole target DTO"),TEXT("different post-load whole target DTO"));
    Step->SetBoolField(TEXT("target_current_verified"),true);
    FDescriptorSource::FFileRole FileAfter;if(!DescriptorFile(PackagePath,FileAfter)||!ExtendedCurrentPlain(C,R,nullptr)||!DescriptorCurrent(C,R))return DescriptorBad(C,R,TEXT("load_source_file_after"),PackagePath,TEXT("file/current observation unavailable after load"));
    if(FileAfter.Exists!=FileBefore.Exists||FileAfter.Bytes!=FileBefore.Bytes||FileAfter.SHA1!=FileBefore.SHA1)return DescriptorBad(C,R,TEXT("load_source_file_changed"),FileBefore.SHA1,FileAfter.SHA1);
    Step->SetBoolField(TEXT("source_file_unchanged_verified"),true);
    if(!EnumLoaded(Returned.Get())||Returned->GetPathName()!=Requested){R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("descriptor_followup_sdk_stopped"),true);return Error(R,TEXT("make_array_exact_path_load_result_mismatch"),TEXT("Exact load did not return the requested live object path; redirected/retired sources remain incomplete and are not retried."));}
    Step->SetStringField(TEXT("resolved_path"),Returned->GetPathName());Step->SetStringField(TEXT("stage"),TEXT("target_verified_source_binding_pending"));return true;
}
bool BindDescriptor(FContext& C,FResult& R)
{
    if(!Descriptor(C))return true;
    const auto Parent=C.Blueprint->ParentClass.GetGCPtr(),Generated=C.Blueprint->GeneratedClass.GetGCPtr(),Skeleton=C.Blueprint->SkeletonGeneratedClass.GetGCPtr();
    if(!Parent.IsResolved()||!Generated.IsResolved()||!Skeleton.IsResolved())return Error(R,TEXT("make_array_descriptor_target_classes_pending"),TEXT("Current target parent/generated/skeleton handles must already be resolved before source/default inspection."));
    C.DescriptorParent.Reset(Parent);C.DescriptorGenerated.Reset(Generated);C.DescriptorSkeleton.Reset(Skeleton);C.DescriptorTargetBound=true;
    if(!ExtendedCurrentPlain(C,R,nullptr))return false;
    C.DescriptorSource=MakeShared<FDescriptorSource>();auto& S=*C.DescriptorSource;S.Kind=C.ElementType;
    UObject* Found=nullptr;
    if(C.ElementType==TEXT("rotator"))Found=TBaseStructure<FRotator>::Get();
    else if(C.ElementType==TEXT("transform"))Found=TBaseStructure<FTransform>::Get();
    else if(C.ElementType==TEXT("object")||C.ElementType==TEXT("class"))Found=C.DescriptorPath.IsEmpty()?static_cast<UObject*>(UObject::StaticClass()):FindObject<UClass>(nullptr,*C.DescriptorPath);
    else Found=FindObject<UScriptStruct>(nullptr,*C.DescriptorPath);
    TStrongObjectPtr<UObject> LoadedSource;
    if(!Found&&(C.ElementType==TEXT("object")||C.ElementType==TEXT("class")||C.ElementType==TEXT("struct")))
    {if(!LoadDescriptorExactPath(C,R,C.DescriptorPath,C.ElementType==TEXT("struct")?UScriptStruct::StaticClass():UClass::StaticClass(),TEXT("descriptor_type"),LoadedSource))return false;Found=LoadedSource.Get();}
    TStrongObjectPtr<UObject> ReturnedSource(Found);if(!ExtendedCurrentPlain(C,R,nullptr))return false;
    UStruct* Type=Cast<UStruct>(Found);if(!Type||!DescriptorRuntime(Type))return Error(R,TEXT("make_array_descriptor_load_pending"),TEXT("Use an exact runtime class/ScriptStruct descriptor. Exact full Game paths may be explicitly loaded; original short aliases, unloaded Engine types and custom native modules remain incomplete."));
    S.Type.Reset(Type);C.ElementPinType.PinCategory=C.ElementType==TEXT("object")?UEdGraphSchema_K2::PC_Object:C.ElementType==TEXT("class")?UEdGraphSchema_K2::PC_Class:UEdGraphSchema_K2::PC_Struct;C.ElementPinType.PinSubCategoryObject=Type;
    for(UStruct* U=Type;U;U=U->GetSuperStruct())if(!DescriptorAddObject(C,R,U,C.ElementType!=TEXT("object")&&C.ElementType!=TEXT("class")))return false;
    // Whole direct membership for every captured root is established before
    // dereferencing nested fields; actual field owners follow SDK factories.
    S.Ready=true;if(!DescriptorPlain(C,R))return false;
    for(int32 I=0;I<S.Objects.Num();++I){const auto Copy=S.Objects[I].DirectFields;UStruct* Owner=Cast<UStruct>(S.Objects[I].Hold.Get());for(auto* P:Copy)if(!DescriptorAddField(C,R,P,FFieldVariant(Owner),0))return false;S.Objects[I].FieldsCaptured=true;}
    S.Ready=true;if(!DescriptorPlain(C,R))return false;
    // Resolve actual property constraints only after a complete physical
    // baseline exists. Nested script structs receive their own field snapshot;
    // Object/Class constraints receive hierarchy/identity, not member scans.
    for(int32 I=0;I<S.Fields.Num();++I)
    {
        TStrongObjectPtr<UObject> One,Two;FProperty* P=S.Fields[I].Address;if(!DescriptorFieldBinding(C,R,P,One,Two))return false;
        S.Fields[I].Binding1=MoveTemp(One);S.Fields[I].Binding2=MoveTemp(Two);S.Fields[I].BindingsCaptured=true;
        UObject* First=S.Fields[I].Binding1.Get();UObject* Second=S.Fields[I].Binding2.Get();
        for(UObject* O:{First,Second})if(O){if(!DescriptorAddObject(C,R,O,Cast<UScriptStruct>(O)!=nullptr))return false;}
        for(int32 J=0;J<S.Objects.Num();++J)if(!S.Objects[J].FieldsCaptured){const auto Copy=S.Objects[J].DirectFields;UStruct* Owner=Cast<UStruct>(S.Objects[J].Hold.Get());for(auto* Q:Copy)if(!DescriptorAddField(C,R,Q,FFieldVariant(Owner),0))return false;S.Objects[J].FieldsCaptured=true;}
        if(!DescriptorPlain(C,R))return false;
    }
    S.BindingsReady=true;
    for(int32 I=0;I<S.Objects.Num();++I){auto* O=S.Objects[I].Hold.Get();const auto* M=FMetaData::GetMapForObject(O);if(!DescriptorPlain(C,R))return false;TMap<FName,FString> Copy;if(M){if(M->Num()>256)return Error(R,TEXT("make_array_descriptor_metadata_bound"),TEXT("Root metadata exceeds256 entries."));Copy=*M;}if(!DescriptorPlain(C,R))return false;S.Objects[I].Metadata=MoveTemp(Copy);S.Objects[I].MetadataCaptured=true;}
    S.MetadataReady=true;if(!DescriptorCurrent(C,R))return false;
    for(int32 I=0;I<C.DescriptorRawValues.Num();++I)
    {
        UObject* Object=nullptr;const auto& V=C.DescriptorRawValues[I];const bool ObjectInput=(C.ElementType==TEXT("object")||C.ElementType==TEXT("class"))&&V->Type==EJson::String;
        if(ObjectInput){const FString P=V->AsString();if(P.IsEmpty()||P.Equals(TEXT("None"),ESearchCase::IgnoreCase))return Error(R,TEXT("make_array_default_object_not_found"),TEXT("Original string object/class defaults require a resolved nonnull object/class; empty/None strings are not null clearing requests."));{if(!P.StartsWith(TEXT("/"))||P.Contains(TEXT("'")))return Error(R,TEXT("make_array_default_object_load_pending"),TEXT("Original object/class aliases remain incomplete; use an exact loaded object/class path or an exact full Game path for explicit loading."));Object=C.ElementType==TEXT("class")?static_cast<UObject*>(FindObject<UClass>(nullptr,*P)):FindObject<UObject>(nullptr,*P);TStrongObjectPtr<UObject> LoadedDefault;if(!Object){if(!LoadDescriptorExactPath(C,R,P,C.ElementType==TEXT("class")?UClass::StaticClass():UObject::StaticClass(),TEXT("default_object"),LoadedDefault))return false;Object=LoadedDefault.Get();}TStrongObjectPtr<UObject> ReturnedDefault(Object);if(!DescriptorCurrent(C,R))return false;if(!DescriptorRuntime(Object))return Error(R,TEXT("make_array_default_object_load_pending"),TEXT("Referenced default object/class is not admitted resolved runtime data; inspect the retained explicit-load prefix when present."));if(C.ElementType==TEXT("class")?!Cast<UClass>(Object)->IsChildOf(CastChecked<UClass>(Type)):!Object->IsA(CastChecked<UClass>(Type)))return Error(R,TEXT("make_array_default_object_type_mismatch"),TEXT("Loaded default object/class does not satisfy the actual descriptor class constraint."));if(!DescriptorAddObject(C,R,Object,false))return false;}}
        S.DefaultObjects.Emplace(Object);C.DescriptorObjectInputs.Add(ObjectInput);
    }
    // New loaded object roles receive metadata before the final bind receipt.
    for(auto& E:S.Objects){if(E.MetadataCaptured)continue;const auto* M=FMetaData::GetMapForObject(E.Hold.Get());if(!DescriptorPlain(C,R))return false;TMap<FName,FString> Copy;if(M){if(M->Num()>256)return Error(R,TEXT("make_array_descriptor_metadata_bound"),TEXT("Root metadata exceeds256 entries."));Copy=*M;}if(!DescriptorPlain(C,R))return false;E.Metadata=MoveTemp(Copy);E.MetadataCaptured=true;}
    if(!DescriptorCurrent(C,R))return false;
    R.Data->SetObjectField(TEXT("element_pin_type"),Function::Pin::Describe(C.ElementPinType));R.Data->SetObjectField(TEXT("make_array_descriptor_source_before"),DescriptorState(C));
    R.Data->SetStringField(TEXT("descriptor_source_scope"),TEXT("Request-local resolved runtime source identity, direct/nested field layout/metadata, package/file and existing CDO identity; baseline follows any explicit exact Game source/default load. No deep CDO/UDS default values or global loader/transitive effects sampled. Short aliases, unloaded Engine sources and custom native modules remain incomplete."));
    // All explicit source loads and their read-only target comparisons are closed.
    // The normal node writer may now intentionally dirty/modify its target.
    for(const auto& Value:C.DescriptorLoadPrefix){auto Step=Value->AsObject();Step->SetBoolField(TEXT("source_bind_completed"),true);Step->SetStringField(TEXT("stage"),TEXT("complete"));}
    C.DescriptorLoadTargetReady=false;return true;
}
bool FinishDescriptor(FContext& C,FResult& R)
{
    if(!Descriptor(C))return true;if(!DescriptorCurrent(C,R))return false;
    for(const auto& F:C.DescriptorSource->Files){FDescriptorSource::FFileRole Actual;if(!DescriptorFile(F.PackageName,Actual)||Actual.Exists!=F.Exists||Actual.Bytes!=F.Bytes||Actual.SHA1!=F.SHA1)return DescriptorBad(C,R,TEXT("source_file"),F.SHA1,Actual.SHA1);}
    const auto After=DescriptorState(C);R.Data->SetObjectField(TEXT("make_array_descriptor_source_after"),After);R.Data->SetBoolField(TEXT("make_array_descriptor_source_observed_unchanged"),true);return DescriptorCurrent(C,R);
}

bool ExtendedStop(FContext& C,FResult& R,const TCHAR* Property,const FString& Expected,const FString& Actual)
{
    if(!R.Data->HasField(TEXT("extended_primitive_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("expected_actual_observed"),false);J->SetStringField(TEXT("diagnostic_kind"),TEXT("compound_guard_or_copied_comparison_operands"));J->SetStringField(TEXT("diagnostic_scope"),TEXT("Guard failure summary or comparison operands; not independent per-property actual readback. Count/address/GUID operands are retained where sampled."));R.Data->SetObjectField(TEXT("extended_primitive_first_mismatch"),J);}
    R.Data->SetBoolField(TEXT("extended_primitive_followup_sdk_stopped"),true);R.bChangeStateKnown=false;return Error(R,TEXT("array_extended_primitive_current_changed"),TEXT("Current target/source/pin identity changed at an SDK return; partial prefix retained without rollback, retry or followup SDK."));
}
bool ExtendedCurrentPlain(FContext& C,FResult& R,const UK2Node_MakeArray* Node)
{
    if(!TrackedMakeArray(C)||!C.ExtendedBound)return true;
    if(R.Data->HasField(TEXT("extended_primitive_followup_sdk_stopped"))||R.Data->HasField(TEXT("descriptor_followup_sdk_stopped")))return false;
    if(!DescriptorLoadTargetCurrent(C,R))return false;
    auto Bad=[&](const TCHAR* K,const FString& E,const FString& A){return ExtendedStop(C,R,K,E,A);};
    if(!EnumContext()||!EnumLoaded(C.ExtendedBP.Get())||C.Blueprint!=C.ExtendedBP.Get()||!EnumLoaded(C.ExtendedGraph.Get())||C.Graph!=C.ExtendedGraph.Get()||C.Graph->GetOuter()!=C.Blueprint||C.Graph->Schema.Get()!=C.Schema->GetClass()||C.Blueprint->bBeingCompiled||C.Blueprint->bQueuedForCompilation||C.Blueprint->bIsRegeneratingOnLoad)return Bad(TEXT("target_current"),TEXT("held idle Blueprint/direct graph/schema"),TEXT("different, retired or busy"));
    if(!EnumLoaded(C.ExtendedClass.Get())||C.NodeClass!=C.ExtendedClass.Get()||!EnumLoaded(C.ExtendedSchema.Get())||C.Schema!=C.ExtendedSchema.Get())return Bad(TEXT("source_class_schema_current"),TEXT("held loaded MakeArray native class and existing schema"),TEXT("different or retired"));
    if(Descriptor(C)&&C.DescriptorTargetBound&&(C.Blueprint->ParentClass.GetGCPtr()!=C.DescriptorParent.Get()||C.Blueprint->GeneratedClass.GetGCPtr()!=C.DescriptorGenerated.Get()||C.Blueprint->SkeletonGeneratedClass.GetGCPtr()!=C.DescriptorSkeleton.Get()))return DescriptorBad(C,R,TEXT("target_class_roles"),TEXT("held parent/generated/skeleton"),TEXT("different class handle"));
    for(const auto& G:C.ExtendedGraphs){if(!EnumLoaded(G.Hold.Get()))return Bad(TEXT("observed_graph_current"),TEXT("held complete-capture graph"),TEXT("retired"));const int32 ExpectedCount=G.NodeCount+(G.Hold.Get()==C.Graph&&C.ExtendedAttached?1:0);if(G.Hold->Nodes.Num()!=ExpectedCount)return Bad(TEXT("graph_node_count"),LexToString(ExpectedCount),LexToString(G.Hold->Nodes.Num()));}
    auto PinsCurrent=[&](const UEdGraphNode* N,const TArray<FContext::FExtendedPin>& Roles)
    {
        if(N->Pins.Num()!=Roles.Num())return Bad(TEXT("pin_count"),LexToString(Roles.Num()),LexToString(N->Pins.Num()));
        for(int32 I=0;I<Roles.Num();++I){const auto& Role=Roles[I];const UEdGraphPin* P=N->Pins[I];if(P!=Role.Address||!P||P->WasTrashed())return Bad(TEXT("pin_address"),FString::Printf(TEXT("%p"),Role.Address),FString::Printf(TEXT("%p"),P));if(P->GetOwningNodeUnchecked()!=N||P->PinId!=Role.Id||P->PinName!=Role.Name||P->Direction!=Role.Direction||!(P->PinType==Role.Type&&P->PinType.bIsUObjectWrapper==Role.Type.bIsUObjectWrapper&&P->PinType.bSerializeAsSinglePrecisionFloat==Role.Type.bSerializeAsSinglePrecisionFloat&&P->PinType.PinValueType.bTerminalIsUObjectWrapper==Role.Type.PinValueType.bTerminalIsUObjectWrapper))return Bad(TEXT("pin_role"),Role.Id.ToString(EGuidFormats::Digits),P->PinId.ToString(EGuidFormats::Digits));}return true;
    };
    for(const auto& Old:C.ExtendedOld){const auto* N=Old.Graph->Nodes[Old.Index].Get();if(N!=Old.Hold.Get()||!EnumLoaded(Old.Hold.Get())||N->NodeGuid!=Old.Guid||N->GetOuter()!=Old.Graph)return Bad(TEXT("prior_node_identity"),FString::Printf(TEXT("%p"),Old.Hold.Get()),FString::Printf(TEXT("%p"),N));if(!PinsCurrent(N,Old.Pins))return false;}
    if(C.ExtendedCreated.IsValid()){const auto* N=C.ExtendedCreated.Get();if(Node!=N||!EnumLoaded(N)||N->GetOuter()!=C.Graph||(C.ExtendedAttached&&C.Graph->Nodes.Last()!=N))return Bad(TEXT("created_node_identity"),FString::Printf(TEXT("%p"),N),FString::Printf(TEXT("%p"),Node));if(C.ExtendedPinsFixed&&!PinsCurrent(N,C.ExtendedPins))return false;}
    return true;
}
bool ExtendedCurrent(FContext& C,FResult& R,const UK2Node_MakeArray* Node)
{return ExtendedCurrentPlain(C,R,Node)&&(!Descriptor(C)||DescriptorCurrent(C,R));}
bool ExtendedObservedCurrent(FContext* C)
{return !C||!C->ExtendedBound||ExtendedCurrent(*C,*C->ExtendedResult,C->ExtendedCreated.Get());}
bool ExtendedBind(FContext& C,FResult& R)
{
    if(!TrackedMakeArray(C))return true;
    if(!EnumLoaded(C.NodeClass)||!EnumLoaded(C.Schema))return Error(R,TEXT("make_array_native_class_unavailable"),TEXT("Loaded native MakeArray class and current schema are required before mutation; no source load requested."));
    C.ExtendedBP.Reset(C.Blueprint);C.ExtendedGraph.Reset(C.Graph);C.ExtendedClass.Reset(C.NodeClass);C.ExtendedSchema.Reset(C.Schema);
    C.ExtendedResult=&R;
    TArray<UEdGraph*> Graphs;C.Blueprint->GetAllGraphs(Graphs);if(Graphs.Num()>256)return ExtendedStop(C,R,TEXT("observed_graph_capture"),TEXT("existing bounded complete-capture graphs"),TEXT("over bound"));
    int32 CapturedNodes=0,CapturedPins=0;
    for(auto* Graph:Graphs){if(!EnumLoaded(Graph))return ExtendedStop(C,R,TEXT("observed_graph_capture"),TEXT("loaded complete-capture graph"),TEXT("retired/null"));if(Graph->Nodes.Num()>2048||(CapturedNodes+=Graph->Nodes.Num())>8192)return Error(R,TEXT("make_array_before_state_unobservable"),TEXT("Existing complete-capture node bound exceeded before mutation."));FContext::FExtendedGraph G;G.Hold.Reset(Graph);G.NodeCount=Graph->Nodes.Num();C.ExtendedGraphs.Add(MoveTemp(G));for(int32 Index=0;Index<Graph->Nodes.Num();++Index){const auto& Node=Graph->Nodes[Index];if(!EnumLoaded(Node.Get()))return ExtendedStop(C,R,TEXT("prior_node_capture"),TEXT("loaded current node"),TEXT("retired/null"));if(Node->Pins.Num()>512||(CapturedPins+=Node->Pins.Num())>16384)return Error(R,TEXT("make_array_before_state_unobservable"),TEXT("Existing complete-capture pin bound exceeded before mutation."));FContext::FExtendedNode Old;Old.Hold.Reset(Node.Get());Old.Graph=Graph;Old.Index=Index;Old.Guid=Node->NodeGuid;for(auto* Pin:Node->Pins){if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node.Get())return ExtendedStop(C,R,TEXT("prior_pin_capture"),TEXT("node-owned current pin"),TEXT("retired/different owner"));Old.Pins.Add({Pin,Pin->PinId,Pin->PinName,Pin->Direction,Pin->PinType});}C.ExtendedOld.Add(MoveTemp(Old));}}
    C.ExtendedBound=true;return ExtendedCurrent(C,R,nullptr);
}
bool ExtendedFreeze(FContext& C,FResult& R,UK2Node_MakeArray* Node)
{
    if(!ExtendedCurrent(C,R,Node))return false;
    if(!Node||Node->Pins.Num()>129)return ExtendedStop(C,R,TEXT("new_pin_capture"),TEXT("bounded actual stock pins"),TEXT("null/over bound"));
    for(auto* Pin:Node->Pins){if(!Pin||Pin->WasTrashed()||Pin->GetOwningNodeUnchecked()!=Node||!Pin->PinId.IsValid())return ExtendedStop(C,R,TEXT("new_pin_capture"),TEXT("stock node-owned current pin/GUID"),TEXT("retired/different/invalid"));C.ExtendedPins.Add({Pin,Pin->PinId,Pin->PinName,Pin->Direction,Pin->PinType});}
    C.ExtendedPinsFixed=true;return ExtendedCurrent(C,R,Node);
}

// Original Shared.h typed Vector/Rotator/Transform conversion, retaining
// string/number/bool/null/JSON fallback for other descriptor values.
bool DescriptorLiteral(const TSharedPtr<FJsonValue>& V,const FString& Kind,FString& Out,FResult& R)
{
    if(!V)return Error(R,TEXT("make_array_descriptor_value_invalid"),TEXT("Parsed JSON value is required."));
    if((Kind==TEXT("rotator")||Kind==TEXT("vector"))&&(V->Type==EJson::Array||V->Type==EJson::Object))
    {
        double A=0,B=0,D=0;const bool Rot=Kind==TEXT("rotator");
        if(V->Type==EJson::Array){const auto& Items=V->AsArray();A=Items.IsValidIndex(0)?Items[0]->AsNumber():0;B=Items.IsValidIndex(1)?Items[1]->AsNumber():0;D=Items.IsValidIndex(2)?Items[2]->AsNumber():0;}
        else{const auto O=V->AsObject();O->TryGetNumberField(Rot?TEXT("pitch"):TEXT("x"),A);O->TryGetNumberField(Rot?TEXT("yaw"):TEXT("y"),B);O->TryGetNumberField(Rot?TEXT("roll"):TEXT("z"),D);}
        Out=Rot?FString::Printf(TEXT("%s,%s,%s"),*FString::SanitizeFloat(A),*FString::SanitizeFloat(B),*FString::SanitizeFloat(D)):FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),*FString::SanitizeFloat(A),*FString::SanitizeFloat(B),*FString::SanitizeFloat(D));return true;
    }
    if(Kind==TEXT("transform")&&V->Type==EJson::Object)
    {
        const auto O=V->AsObject();const auto L=O->HasField(TEXT("location"))?O->TryGetField(TEXT("location")):O->TryGetField(TEXT("translation"));const auto T=O->TryGetField(TEXT("rotation"));const auto S=O->HasField(TEXT("scale"))?O->TryGetField(TEXT("scale")):O->TryGetField(TEXT("scale3d"));
        FString LS=TEXT("(X=0,Y=0,Z=0)"),RS=TEXT("(Pitch=0,Yaw=0,Roll=0)"),SS=TEXT("(X=1,Y=1,Z=1)");
        if(L.IsValid()&&!DescriptorLiteral(L,TEXT("vector"),LS,R))return false;if(T.IsValid()&&!DescriptorLiteral(T,TEXT("rotator"),RS,R))return false;if(S.IsValid()&&!DescriptorLiteral(S,TEXT("vector"),SS,R))return false;
        FVector Location=FVector::ZeroVector,Scale=FVector::OneVector;FRotator Rotation=FRotator::ZeroRotator;
        if(!FDefaultValueHelper::ParseVector(LS,Location)||!FDefaultValueHelper::ParseRotator(RS,Rotation)||!FDefaultValueHelper::ParseVector(SS,Scale)||Location.ContainsNaN()||Rotation.ContainsNaN()||Scale.ContainsNaN())return Error(R,TEXT("make_array_transform_component_invalid"),TEXT("Transform location/rotation/scale must parse to finite component values; no silent zero fallback."));
        Out=FTransform(Rotation,Location,Scale).ToString();return true;
    }
    return TextLiteral(V,false,Out,R);
}
bool DescriptorDefaultMatch(FContext& C,const UK2Node_MakeArray* Node,int32 Index)
{
    if(!ExtendedObservedCurrent(&C)||!C.ExtendedSchemaValues.IsValidIndex(Index))return false;
    UEdGraphPin* P=Node->Pins[Index+1];UObject* Expected=C.DescriptorExpectedObjects.IsValidIndex(Index)?C.DescriptorExpectedObjects[Index].Get():nullptr;
    if(P->DefaultObject!=Expected)return false;
    const bool Match=C.Schema->DoesDefaultValueMatch(*P,C.ExtendedSchemaValues[Index]);if(!ExtendedObservedCurrent(&C))return false;
    P=Node->Pins[Index+1];const FString ErrorText=C.Schema->IsCurrentPinDefaultValid(P);if(!ExtendedObservedCurrent(&C))return false;return Match&&ErrorText.IsEmpty();
}
bool ApplyDescriptorValues(FContext& C,FResult& R,UK2Node_MakeArray* Node,const TSharedRef<FJsonObject>& Applied,const TSharedRef<FJsonObject>& Observations)
{
    C.DescriptorExpectedObjects.SetNum(C.InputCount);TArray<TSharedPtr<FJsonValue>> Steps;R.Data->SetArrayField(TEXT("descriptor_value_steps"),Steps);
    for(int32 Index=0;Index<C.Values.Num();++Index)
    {
        if(!ExtendedObservedCurrent(&C))return false;
        UEdGraphPin* P=Node->Pins[Index+1];const FString Label=P->PinName.ToString();const FEdGraphPinType ActualType=P->PinType;FString Raw;TObjectPtr<UObject> ExpectedObject=nullptr;FText ExpectedText;
        auto Step=MakeShared<FJsonObject>();Step->SetNumberField(TEXT("index"),Index);Step->SetStringField(TEXT("pin_name"),Label);Step->SetStringField(TEXT("stage"),TEXT("convert"));Step->SetBoolField(TEXT("assignment_attempted"),false);Step->SetBoolField(TEXT("assignment_returned"),false);Steps.Add(MakeShared<FJsonValueObject>(Step));R.Data->SetArrayField(TEXT("descriptor_value_steps"),Steps);
        const bool ObjectSetter=C.DescriptorObjectInputs[Index];FString SchemaLiteral=C.Values[Index];
        // Keep requested_literal unchanged. K2 validates Rotator pin strings with
        // IsStringValidVector, while ParseRotator also accepts legacy P=/Pitch=.
        if(C.ElementType==TEXT("rotator")){FRotator Parsed;if(FDefaultValueHelper::ParseRotator(SchemaLiteral,Parsed)){if(Parsed.ContainsNaN())return Error(R,TEXT("make_array_rotator_nonfinite"),TEXT("Rotator literal must parse to finite pitch/yaw/roll."));SchemaLiteral=FString::Printf(TEXT("%s,%s,%s"),*FString::SanitizeFloat(Parsed.Pitch),*FString::SanitizeFloat(Parsed.Yaw),*FString::SanitizeFloat(Parsed.Roll));}}
        if(ObjectSetter){ExpectedObject=C.DescriptorSource->DefaultObjects[Index].Get();SchemaLiteral=Path(ExpectedObject.Get());}
        else
        {
            C.Schema->GetPinDefaultValuesFromString(ActualType,Node,SchemaLiteral,Raw,ExpectedObject,ExpectedText,false);
            // A schema parser may resolve embedded references. Hold its actual
            // return before the current barrier; do not assume callback-free.
            TStrongObjectPtr<UObject> Returned(ExpectedObject.Get());if(!ExtendedObservedCurrent(&C))return false;
            if(Returned.IsValid()&&!DescriptorRuntime(Returned.Get()))return Error(R,TEXT("make_array_descriptor_default_dependency_pending"),TEXT("Schema conversion returned a non-runtime default reference; retained node/prefix requires inspection."));
        }
        C.DescriptorExpectedObjects[Index].Reset(ExpectedObject.Get());
        Step->SetStringField(TEXT("stage"),TEXT("validate"));P=Node->Pins[Index+1];const FString Validation=C.Schema->IsPinDefaultValid(P,ObjectSetter?FString():Raw,ExpectedObject.Get(),ExpectedText);if(!ExtendedObservedCurrent(&C))return false;
        if(!Validation.IsEmpty()){R.Data->SetStringField(TEXT("descriptor_default_engine_error"),Validation);return Error(R,TEXT("make_array_engine_default_rejected"),TEXT("Engine rejected descriptor value on retained input: ")+Label);}
        P=Node->Pins[Index+1];
        Step->SetStringField(TEXT("stage"),TEXT("assignment"));Step->SetStringField(TEXT("assignment_kind"),ObjectSetter?TEXT("TrySetDefaultObject"):TEXT("TrySetDefaultValue"));Step->SetBoolField(TEXT("assignment_attempted"),true);
        if(ObjectSetter)C.Schema->TrySetDefaultObject(*P,ExpectedObject.Get(),false);
        else C.Schema->TrySetDefaultValue(*P,SchemaLiteral,false);
        Step->SetBoolField(TEXT("assignment_returned"),true);if(!ExtendedObservedCurrent(&C))return false;P=Node->Pins[Index+1];
        Step->SetStringField(TEXT("stage"),TEXT("readback"));const bool Match=C.Schema->DoesDefaultValueMatch(*P,SchemaLiteral);if(!ExtendedObservedCurrent(&C))return false;P=Node->Pins[Index+1];
        const FString CurrentError=C.Schema->IsCurrentPinDefaultValid(P);if(!ExtendedObservedCurrent(&C))return false;P=Node->Pins[Index+1];
        if(!Match||!CurrentError.IsEmpty()||P->DefaultObject!=ExpectedObject.Get()){R.Data->SetStringField(TEXT("descriptor_default_engine_error"),CurrentError);return Error(R,TEXT("make_array_default_readback_mismatch"),TEXT("Actual descriptor default differs on retained input: ")+Label);}
        const FString ActualRaw=P->DefaultValue,ObjectPath=Path(P->DefaultObject);const FText TextCopy=P->DefaultTextValue;const FGuid Id=P->PinId;const FEdGraphPinType TypeCopy=P->PinType;
        const FString TextDisplay=TextCopy.ToString();if(!ExtendedObservedCurrent(&C))return false;P=Node->Pins[Index+1];const FString AsString=P->GetDefaultAsString();if(!ExtendedObservedCurrent(&C))return false;
        C.ExtendedSchemaValues[Index]=SchemaLiteral;
        const FString AppliedValue=ObjectSetter?ObjectPath:ActualRaw;Applied->SetStringField(Label,AppliedValue);
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("requested_literal"),C.Values[Index]);O->SetStringField(TEXT("schema_literal"),SchemaLiteral);O->SetStringField(TEXT("raw_default"),ActualRaw);O->SetStringField(TEXT("default_object"),ObjectPath);O->SetStringField(TEXT("default_text"),TextDisplay);O->SetStringField(TEXT("default_as_string"),AsString);O->SetStringField(TEXT("applied_value"),AppliedValue);O->SetStringField(TEXT("applied_kind"),ObjectSetter?TEXT("object_path"):TEXT("raw_default"));O->SetBoolField(TEXT("schema_match"),Match);O->SetBoolField(TEXT("current_default_valid"),CurrentError.IsEmpty());O->SetStringField(TEXT("pin_id"),Id.ToString(EGuidFormats::Digits));O->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(TypeCopy));if(!ExtendedObservedCurrent(&C))return false;Observations->SetObjectField(Label,O);
        Step->SetStringField(TEXT("stage"),TEXT("complete"));Step->SetBoolField(TEXT("readback_verified"),true);R.Data->SetNumberField(TEXT("values_applied_count"),Applied->Values.Num());
    }
    return true;
}

bool Arguments(FContext& C, FResult& R, const FString& ElementType, const FString& InputCountJson,
    const FString& ValuesJson, const FString& TypeObject, const FString& EnumType,
    const FString& PositionJson, bool bCreate)
{
    if (bCreate) { return Error(R, TEXT("make_array_graph_creation_unsupported"), TEXT("Select an existing graph; node-only MakeArray does not create one.")); }
    C.ElementType = Normalize(ElementType);
    if (C.ElementType == TEXT("integer")) { C.ElementType = TEXT("int"); }
    if(C.ElementType==TEXT("integer64"))C.ElementType=TEXT("int64");
    if(C.ElementType==TEXT("single"))C.ElementType=TEXT("float");
    if(C.ElementType==TEXT("real"))C.ElementType=TEXT("double");
    if (C.ElementType == TEXT("boolean")) { C.ElementType = TEXT("bool"); }
    if(C.ElementType==TEXT("byteenum")) { C.ElementType=TEXT("enum"); }
    if(C.ElementType==TEXT("scriptstruct"))C.ElementType=TEXT("struct");
    if(Descriptor(C)){C.DescriptorPath=TypeObject.TrimStartAndEnd();if(C.ElementType==TEXT("struct")&&C.DescriptorPath.IsEmpty())return Error(R,TEXT("make_array_struct_type_object_required"),TEXT("struct/scriptstruct requires the original type_object argument."));}
    if(C.ElementType==TEXT("enum"))
    {
        C.EnumScope=MakeShared<FEnumArrayScope>();
        if(!C.EnumScope->Load(TypeObject.TrimStartAndEnd().IsEmpty()?EnumType:TypeObject,R)) { return false; }
        C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_Byte;C.ElementPinType.PinSubCategoryObject=C.EnumScope->Source.Get();
    }
    else if(!Descriptor(C)&&(!TypeObject.IsEmpty()||!EnumType.IsEmpty()))
    { return Error(R,TEXT("make_array_type_descriptor_unsupported"),TEXT("Object/enum descriptors apply only to the enum element route; other unsupported original types remain pending.")); }
    if (C.ElementType != TEXT("int") && C.ElementType != TEXT("bool") && C.ElementType != TEXT("vector") && C.ElementType != TEXT("enum") && C.ElementType != TEXT("name") && C.ElementType != TEXT("string") && !ExtendedPrimitive(C)&&!Descriptor(C))
    { return Error(R, TEXT("make_array_element_type_unsupported"), TEXT("Original primitive8/enum/Vector and loaded runtime object/class/Rotator/Transform/ScriptStruct descriptors are admitted; remaining loaders and dynamic variants remain incomplete.")); }
    if (C.ElementType == TEXT("vector"))
    {
        C.ElementPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        C.ElementPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if(C.ElementType==TEXT("int64")){C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_Int64;}
    else if(C.ElementType==TEXT("float")||C.ElementType==TEXT("double")){C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_Real;C.ElementPinType.PinSubCategory=C.ElementType==TEXT("float")?UEdGraphSchema_K2::PC_Float:UEdGraphSchema_K2::PC_Double;}
    else if(C.ElementType==TEXT("text")){C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_Text;}
    else if(C.ElementType==TEXT("name")){C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_Name;}
    else if(C.ElementType==TEXT("string")){C.ElementPinType.PinCategory=UEdGraphSchema_K2::PC_String;}
    else if(C.ElementType!=TEXT("enum")&&!Descriptor(C)) { C.ElementPinType.PinCategory = C.ElementType == TEXT("int") ? UEdGraphSchema_K2::PC_Int : UEdGraphSchema_K2::PC_Boolean; }
    TSharedPtr<FJsonValue> Value;
    if (!Function::Pin::ParseJson(ValuesJson, Value, R,TrackedMakeArray(C))) { return false; }
    C.bValuesSupplied = Value->Type != EJson::Null;
    if (C.bValuesSupplied)
    {
        if (Value->Type != EJson::Array || Value->AsArray().Num() > 128)
        { return Error(R, TEXT("make_array_values_invalid"), TEXT("values must be omitted/null or a list of at most 128 exact typed values.")); }
        C.InputCount = Value->AsArray().Num();
        for (const auto& Item : Value->AsArray())
        {
            int32 Number = 0;
            if(Descriptor(C)){FString Literal;if(!DescriptorLiteral(Item,C.ElementType,Literal,R))return false;C.Values.Add(Literal);C.DescriptorRawValues.Add(Item);}
            else if (C.ElementType == TEXT("int"))
            {
                if (!ExactInt32(Item, Number))
                { return Error(R, TEXT("make_array_value_invalid"), TEXT("Every int element must be exact int32; bool, null, fraction, string and out-of-range values fail before allocation.")); }
                C.Values.Add(LexToString(Number));
            }
            else if(C.ElementType==TEXT("int64")||C.ElementType==TEXT("float")||C.ElementType==TEXT("double"))
            {FString Literal;if(!ExtendedNumberLiteral(Item,C.ElementPinType,Literal,R))return false;C.Values.Add(Literal);}
            else if(C.ElementType==TEXT("name")||C.ElementType==TEXT("string")||C.ElementType==TEXT("text"))
            {FString Literal;if(!TextLiteral(Item,C.ElementType==TEXT("name"),Literal,R)){return false;}C.Values.Add(Literal);}
            else if(C.ElementType==TEXT("vector"))
            { FString Literal;if(!VectorLiteral(Item,Literal,R)) { return false; }C.Values.Add(Literal); }
            else if(C.EnumScope.IsValid())
            { FString EnumValue;if(!C.EnumScope->Value(Item,EnumValue)) { return false; }C.Values.Add(EnumValue); }
            else
            {
                if (!Item.IsValid() || Item->Type != EJson::Boolean)
                { return Error(R, TEXT("make_array_value_invalid"), TEXT("Every bool element must be an exact JSON boolean; no numeric/string/null coercion.")); }
                C.Values.Add(Item->AsBool() ? TEXT("true") : TEXT("false"));
            }
        }
    }
    if (!Function::Pin::ParseJson(InputCountJson, Value, R)) { return false; }
    C.bCountSupplied = Value->Type != EJson::Null;
    if (C.bCountSupplied && !ExactInt32(Value, C.InputCount))
    { return Error(R, TEXT("make_array_input_count_invalid"), TEXT("input_count must be omitted/null or exact int32, not bool/fraction.")); }
    if (C.InputCount < 0 || C.InputCount > 128 || C.Values.Num() > C.InputCount)
    { return Error(R, TEXT("make_array_input_count_invalid"), TEXT("input_count must be 0..128 and cannot be smaller than the supplied values list.")); }
    if (!Function::Pin::ParseJson(PositionJson, Value, R)) { return false; }
    if (Value->Type != EJson::Null && (Value->Type != EJson::Array || Value->AsArray().Num() != 2
        || !ExactInt32(Value->AsArray()[0], C.X) || !ExactInt32(Value->AsArray()[1], C.Y)
        || C.X < -1000000 || C.X > 1000000 || C.Y < -1000000 || C.Y > 1000000))
    { return Error(R, TEXT("make_array_position_invalid"), TEXT("node_position must be omitted/null or two exact integer coordinates within +/-1000000.")); }
    if(ExtendedPrimitive(C)){R.Data->SetBoolField(TEXT("native_extended_primitive_make_array"),true);R.Data->SetBoolField(TEXT("extended_primitive_runtime_invocation_verified"),false);R.Data->SetStringField(TEXT("int64_numeric_precision_scope"),TEXT("Original number is upstream double then finite/range-gated truncating int64 conversion; exact digits require a literal string. No runtime integer overflow/evaluation guarantee."));}
    if(Descriptor(C)){R.Data->SetBoolField(TEXT("native_descriptor_make_array"),true);R.Data->SetBoolField(TEXT("descriptor_runtime_invocation_verified"),false);}
    R.Data->SetStringField(TEXT("element_type"), C.ElementType);
    R.Data->SetObjectField(TEXT("element_pin_type"), Function::Pin::Describe(C.ElementPinType));
    R.Data->SetNumberField(TEXT("input_count"), C.InputCount);
    R.Data->SetBoolField(TEXT("vector_inputs_require_connections_for_nonzero_values"), false);
    R.Data->SetBoolField(TEXT("vector_literal_defaults_supported"), C.ElementType==TEXT("vector"));
    R.Data->SetBoolField(TEXT("input_count_supplied"), C.bCountSupplied);
    R.Data->SetBoolField(TEXT("values_supplied"), C.bValuesSupplied);
    R.Data->SetNumberField(TEXT("values_requested_count"), C.Values.Num()); return true;
}

FString GraphKind(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (Blueprint->FunctionGraphs.Contains(Graph)) { return TEXT("function"); }
    if (Blueprint->MacroGraphs.Contains(Graph)) { return TEXT("macro"); }
    if (Blueprint->UbergraphPages.Contains(Graph) || Blueprint->EventGraphs.Contains(Graph)) { return TEXT("event"); }
    return TEXT("unknown");
}

TSharedRef<FJsonObject> GraphDescription(const FContext& C)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("graph_name"), C.Graph->GetName());
    Row->SetStringField(TEXT("graph_id"), C.Graph->GraphGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("graph_type"), GraphKind(C.Blueprint, C.Graph));
    Row->SetStringField(TEXT("object_path"), C.Graph->GetPathName()); Row->SetStringField(TEXT("schema"), C.Schema->GetClass()->GetPathName());
    Row->SetNumberField(TEXT("node_count"), C.Graph->Nodes.Num()); return Row;
}

bool ResolveTarget(FContext& C, FResult& R, const FString& Path, const FString& GraphName, const FString& GraphId, const FString& GraphType)
{
    if (!Function::Pin::SafeName(Path) || !Path.StartsWith(TEXT("/Game/")) || Path.Contains(TEXT(":")))
    { return Error(R, TEXT("make_array_blueprint_path_invalid"), TEXT("Use an exact bounded Game Blueprint asset or package path.")); }
    const FString AssetPath = Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
    if (!Function::Pin::SafeName(AssetPath)) { return Error(R, TEXT("make_array_blueprint_path_invalid"), TEXT("Completed asset path exceeds the bound.")); }
    C.Blueprint = FindObject<UBlueprint>(nullptr, *AssetPath);
    if (!FullyLoaded(C.Blueprint)) { return Error(R, TEXT("make_array_blueprint_not_loaded"), TEXT("Load the exact Blueprint through the official asset tool first; this operation never loads it implicitly.")); }
    UBlueprint* BP = C.Blueprint;
    if(C.Blueprint->BlueprintType==BPTYPE_FunctionLibrary)
    {
        if(!(C.ElementType==TEXT("name"))) {return Error(R,TEXT("make_array_blueprint_state_unsupported"),TEXT("Only Name MakeArray and Name Array_Length are supported in the exact member-free PresetLoader BFL."));}
        C.NameScope=Function::BeginPresetLoaderNameArray(R,C.Blueprint,GraphName,GraphId,GraphType,false,C.Graph,C.Schema);
        if(!C.NameScope.IsValid())return false;
        R.Data->SetObjectField(TEXT("graph"),GraphDescription(C));R.Data->SetStringField(TEXT("resolved_blueprint_path"),C.Blueprint->GetPathName());return C.NameScope->Check();
    }
    if (BP->GetPathName() != AssetPath || BP->BlueprintType != BPTYPE_Normal || !FullyLoaded(BP->ParentClass) || !BP->SkeletonGeneratedClass
        || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad
        || BP->Status == BS_BeingCreated || BP->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Error(R, TEXT("make_array_blueprint_state_unsupported"), TEXT("Requires an already-loaded normal idle runtime Blueprint with a current skeleton.")); }
    for (const UClass* Parent = BP->ParentClass; Parent; Parent = Parent->GetSuperClass())
    {
        if (!FullyLoaded(Parent) || !Parent->HasAnyClassFlags(CLASS_Native) || Parent->IsEditorOnly()
            || Parent->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Interface)
            || (Parent->GetOutermost()->GetName() != TEXT("/Script/Engine") && Parent->GetOutermost()->GetName() != TEXT("/Script/CoreUObject")))
        { return Error(R, TEXT("make_array_blueprint_parent_unsupported"), TEXT("This cohort requires an exact loaded native Engine/CoreUObject runtime parent chain.")); }
    }
    TArray<FString> External;
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != BP && BP->GeneratedClass && It->ParentClass && It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.AddUnique(It->GetPathName()); } }
    if (BP->GeneratedClass)
    {
        TArray<UObject*> Instances; GetObjectsOfClass(BP->GeneratedClass, Instances, true, RF_NoFlags);
        for (UObject* Instance : Instances) { if (IsValid(Instance) && Instance->GetOutermost() != BP->GetOutermost()) { External.AddUnique(Instance->GetPathName()); } }
    }
    External.Sort(); TArray<TSharedPtr<FJsonValue>> Rows; for (const FString& Value : External) { Rows.Add(MakeShared<FJsonValueString>(Value)); }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"), Rows);
    if (!External.IsEmpty()) { return Error(R, TEXT("make_array_external_instances_unsupported"), TEXT("Loaded external instances or derived Blueprints are outside this mutation slice.")); }
    if (!Function::Pin::SafeName(GraphName, true) || !Function::Pin::SafeName(GraphId, true) || !Function::Pin::SafeName(GraphType, true))
    { return Error(R, TEXT("make_array_graph_selector_invalid"), TEXT("Graph selectors must be bounded safe strings.")); }
    FGuid RequestedGuid; if (!GraphId.IsEmpty() && (!FGuid::Parse(GraphId, RequestedGuid) || !RequestedGuid.IsValid()))
    { return Error(R, TEXT("make_array_graph_selector_invalid"), TEXT("graph_id must be a valid nonzero GUID.")); }
    FString Kind = Normalize(GraphType);
    if (Kind == TEXT("eventgraph") || Kind == TEXT("ubergraph")) { Kind = TEXT("event"); }
    else if (Kind == TEXT("functiongraph")) { Kind = TEXT("function"); }
    else if (Kind == TEXT("macrograph")) { Kind = TEXT("macro"); }
    if (!Kind.IsEmpty() && Kind != TEXT("any") && Kind != TEXT("event") && Kind != TEXT("function") && Kind != TEXT("macro"))
    { return Error(R, TEXT("make_array_graph_type_unsupported"), TEXT("Initial selectors support existing direct event/function/macro K2 graphs only.")); }
    if (GraphName.IsEmpty() && GraphId.IsEmpty() && Kind.IsEmpty()) { Kind = TEXT("event"); }
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    if (Graphs.Num() > 256) { return Error(R, TEXT("make_array_graph_budget_exceeded"), TEXT("Graph selection exceeds 256 owned graphs.")); }
    for (UEdGraph* Graph : Graphs)
    {
        if (!IsValid(Graph) || (!GraphName.IsEmpty() && !Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
            || (!GraphId.IsEmpty() && Graph->GraphGuid != RequestedGuid) || (!Kind.IsEmpty() && Kind != TEXT("any") && GraphKind(BP, Graph) != Kind)) { continue; }
        if (C.Graph) { return Error(R, TEXT("make_array_graph_ambiguous"), TEXT("Graph selectors match multiple graphs; provide an exact name or GUID.")); } C.Graph = Graph;
    }
    if (!C.Graph) { return Error(R, TEXT("make_array_graph_not_found"), TEXT("No matching existing graph; nothing was created.")); }
    UClass* SchemaClass = C.Graph->Schema.Get();
    C.Schema = FullyLoaded(SchemaClass) && SchemaClass->GetPathName() == TEXT("/Script/BlueprintGraph.EdGraphSchema_K2")
        ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if (!FullyLoaded(C.Graph) || C.Graph->GetOuter() != BP || !C.Graph->GraphGuid.IsValid() || GraphKind(BP, C.Graph) == TEXT("unknown")
        || !FullyLoaded(C.Schema) || C.Schema->GetClass() != SchemaClass || C.Graph->Nodes.Num() >= 2048)
    { return Error(R, TEXT("make_array_graph_state_unsupported"), TEXT("Requires a directly owned standard K2 graph with valid GUID and room below 2048 nodes.")); }
    R.Data->SetObjectField(TEXT("graph"), GraphDescription(C)); R.Data->SetStringField(TEXT("resolved_blueprint_path"), BP->GetPathName());
    R.Data->SetBoolField(TEXT("loaded_during_operation"), false); R.Data->SetBoolField(TEXT("schema_cdo_already_loaded"), true); return true;
}

FString TypeText(const FEdGraphPinType& Type)
{ FString Out; FEdGraphPinType::StaticStruct()->ExportText(Out, &Type, &Type, nullptr, PPF_None, nullptr); return Out; }

TSharedRef<FJsonObject> NodeState(const UEdGraphNode* Node,FEnumArrayScope* Scope=nullptr,FVectorArrayScope* VectorScope=nullptr,Function::FPresetLoaderNameArrayScope* NameScope=nullptr,FContext* Extended=nullptr)
{
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("object_path"), Node->GetPathName());
    Row->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::Digits)); Row->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::Digits));
    Row->SetStringField(TEXT("node_class"), Node->GetClass()->GetPathName()); Row->SetStringField(TEXT("comment"), Node->NodeComment);
    Row->SetNumberField(TEXT("x"), Node->NodePosX); Row->SetNumberField(TEXT("y"), Node->NodePosY);
    const auto Enabled=Node->GetDesiredEnabledState();if((NameScope&&!NameScope->Check())||(Scope&&!Scope->Check())||(VectorScope&&!VectorScope->Check())||!ExtendedObservedCurrent(Extended)) { return Row; }
    const bool Ghost=Node->IsAutomaticallyPlacedGhostNode();if((NameScope&&!NameScope->Check())||(Scope&&!Scope->Check())||(VectorScope&&!VectorScope->Check())||!ExtendedObservedCurrent(Extended)) { return Row; }
    Row->SetNumberField(TEXT("enabled_state"),static_cast<int32>(Enabled));Row->SetBoolField(TEXT("ghost"),Ghost);
    TArray<TSharedPtr<FJsonValue>> Pins;
    const auto PinCopies=Extended?Node->Pins:TArray<UEdGraphPin*>();const auto& PinRows=Extended?PinCopies:Node->Pins;
    for (const UEdGraphPin* Pin : PinRows)
    {
        if (!Pin || Pin->WasTrashed()) { Row->SetBoolField(TEXT("pin_observation_incomplete"), true); continue; }
        auto P = MakeShared<FJsonObject>(); P->SetStringField(TEXT("name"), Pin->PinName.ToString()); P->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::Digits));
        P->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        const FEdGraphPinType CopiedPinType=Pin->PinType;auto Type = Function::Pin::Describe(Extended?CopiedPinType:Pin->PinType);if(!ExtendedObservedCurrent(Extended))return Row;if(NameScope&&!NameScope->Check())return Row; Type->SetBoolField(TEXT("is_const"), Pin->PinType.bIsConst); P->SetObjectField(TEXT("pin_type"), Type);
        P->SetStringField(TEXT("pin_type_text"), TypeText(Extended?CopiedPinType:Pin->PinType));if(!ExtendedObservedCurrent(Extended))return Row;if(NameScope&&!NameScope->Check())return Row; P->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        P->SetStringField(TEXT("autogenerated_default_value"), Pin->AutogeneratedDefaultValue); P->SetStringField(TEXT("default_object"), NameScope?(Pin->DefaultObject==nullptr?FString():Pin->DefaultObject.GetPathName()):Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString());
        if(NameScope&&!NameScope->Check())return Row;const FText CopiedText=Pin->DefaultTextValue;const FString ObservedText=CopiedText.ToString();if(!ExtendedObservedCurrent(Extended))return Row;P->SetStringField(TEXT("default_text"), ObservedText); P->SetBoolField(TEXT("hidden"), Pin->bHidden); P->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
        P->SetBoolField(TEXT("default_value_is_ignored"), Pin->bDefaultValueIsIgnored); P->SetBoolField(TEXT("default_value_is_read_only"), Pin->bDefaultValueIsReadOnly);
        P->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable); P->SetBoolField(TEXT("advanced_view"), Pin->bAdvancedView);
        P->SetBoolField(TEXT("display_as_mutable_ref"), Pin->bDisplayAsMutableRef);
        if (Pin->ParentPin && Pin->ParentPin->WasTrashed()) { P->SetBoolField(TEXT("parent_pin_observation_incomplete"), true); }
        P->SetStringField(TEXT("parent_pin_id"), Pin->ParentPin && !Pin->ParentPin->WasTrashed() ? Pin->ParentPin->PinId.ToString(EGuidFormats::Digits) : FString());
        TArray<TSharedPtr<FJsonValue>> Children;
        for (const UEdGraphPin* Child : Pin->SubPins)
        {
            if (!Child || Child->WasTrashed()) { P->SetBoolField(TEXT("sub_pin_observation_incomplete"), true); Children.Add(MakeShared<FJsonValueString>(FString())); }
            else { Children.Add(MakeShared<FJsonValueString>(Child->PinId.ToString(EGuidFormats::Digits))); }
        }
        P->SetArrayField(TEXT("sub_pin_ids"), Children); TArray<FString> Edges;
        for (const UEdGraphPin* Other : Pin->LinkedTo)
        {
            if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked())) { P->SetBoolField(TEXT("link_observation_incomplete"), true); continue; }
            Edges.Add(Other->GetOwningNodeUnchecked()->GetPathName() + TEXT(":") + Other->PinName.ToString());
        }
        Edges.Sort();
        TArray<TSharedPtr<FJsonValue>> Links; for (const FString& Edge : Edges) { Links.Add(MakeShared<FJsonValueString>(Edge)); } P->SetArrayField(TEXT("links"), Links);
        Pins.Add(MakeShared<FJsonValueObject>(P));
    }
    Row->SetArrayField(TEXT("pins"), Pins);
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    {
        Row->SetStringField(TEXT("function_name"), Call->FunctionReference.GetMemberName().ToString()); const UClass* Owner = Call->FunctionReference.GetMemberParentClass();TStrongObjectPtr<const UClass> ReturnedOwner;if(Extended){if(!ExtendedObservedCurrent(Extended))return Row;if(Owner){if(!EnumLoaded(Owner)){ExtendedStop(*Extended,*Extended->ExtendedResult,TEXT("member_parent_return"),TEXT("loaded parent class"),TEXT("retired"));return Row;}ReturnedOwner.Reset(Owner);}if(!ExtendedObservedCurrent(Extended))return Row;}
        if(NameScope&&!NameScope->Check())return Row;Row->SetStringField(TEXT("function_member_parent_path"), Owner ? Owner->GetPathName() : FString());
        Row->SetStringField(TEXT("function_member_guid"), Call->FunctionReference.GetMemberGuid().ToString(EGuidFormats::Digits)); Row->SetBoolField(TEXT("function_self_context"), Call->FunctionReference.IsSelfContext());
    }
    if (const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node))
    {
        if(Extended){UBlueprint* Source=Macro->GetSourceBlueprint();if(!ExtendedObservedCurrent(Extended))return Row;TStrongObjectPtr<UBlueprint> HoldSource;if(Source){if(!EnumLoaded(Source)){ExtendedStop(*Extended,*Extended->ExtendedResult,TEXT("macro_source_return"),TEXT("loaded resolved source Blueprint"),TEXT("retired"));return Row;}HoldSource.Reset(Source);}if(!ExtendedObservedCurrent(Extended))return Row;Row->SetStringField(TEXT("macro_blueprint_path"),Path(Source));}else{Row->SetStringField(TEXT("macro_blueprint_path"), Path(Macro->GetSourceBlueprint()));}
        if(NameScope&&!NameScope->Check())return Row;Row->SetStringField(TEXT("resolved_wildcard_type_text"), TypeText(Macro->ResolvedWildcardType));if(!ExtendedObservedCurrent(Extended))return Row;
    }
    if (const UK2Node_MakeArray* Array = Cast<UK2Node_MakeArray>(Node))
    { Row->SetNumberField(TEXT("input_count"), Array->NumInputs); }
    return Row;
}

bool Capture(FContext& C, const TSharedRef<FJsonObject>& Out)
{
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return false; }
    if (!IsValid(C.Blueprint) || !IsValid(C.Graph) || C.Graph->GetOuter() != C.Blueprint) { return false; }
    Out->SetStringField(TEXT("blueprint_path"), C.Blueprint->GetPathName()); Out->SetStringField(TEXT("parent_class"), C.Blueprint->ParentClass ? C.Blueprint->ParentClass->GetPathName() : FString());
    if(C.NameScope.IsValid()&&!C.NameScope->Check())return false;Out->SetBoolField(TEXT("dirty"), C.Blueprint->GetOutermost()->IsDirty()); Out->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(C.Blueprint->Status.GetValue()));
    TArray<TSharedPtr<FJsonValue>> Variables;
    const auto VariableCopies=TrackedMakeArray(C)?C.Blueprint->NewVariables:TArray<FBPVariableDescription>();const auto& VariableRows=TrackedMakeArray(C)?VariableCopies:C.Blueprint->NewVariables;
    for (const FBPVariableDescription& Variable : VariableRows)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), Variable.VarName.ToString()); Row->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits));
        FString Declaration; FBPVariableDescription::StaticStruct()->ExportText(Declaration, &Variable, &Variable, C.Blueprint, PPF_None, nullptr);
        if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return false; }
        Row->SetStringField(TEXT("declaration_text"), Declaration);
        Row->SetStringField(TEXT("type"), TypeText(Variable.VarType));if(!ExtendedObservedCurrent(&C))return false; Row->SetStringField(TEXT("flags_string"), LexToString(Variable.PropertyFlags)); Row->SetStringField(TEXT("stored_default"), Variable.DefaultValue);
        Row->SetStringField(TEXT("category"), Variable.Category.ToString());if(!ExtendedObservedCurrent(&C))return false; TArray<TSharedPtr<FJsonValue>> Metadata;
        for (const FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
        { auto M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("key"), Entry.DataKey.ToString()); M->SetStringField(TEXT("value"), Entry.DataValue); Metadata.Add(MakeShared<FJsonValueObject>(M)); }
        Row->SetArrayField(TEXT("metadata"), Metadata); Variables.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("variables"), Variables); TArray<TSharedPtr<FJsonValue>> Interfaces;
    for (const FBPInterfaceDescription& Interface : C.Blueprint->ImplementedInterfaces)
    {
        if (!IsValid(Interface.Interface)) { return false; } auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("class_path"), Interface.Interface->GetPathName());
        TArray<TSharedPtr<FJsonValue>> Rows; for (const UEdGraph* Graph : Interface.Graphs) { if (!IsValid(Graph)) { return false; } Rows.Add(MakeShared<FJsonValueString>(Graph->GetPathName())); }
        Row->SetArrayField(TEXT("graphs"), Rows); Interfaces.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("interfaces"), Interfaces); TArray<UEdGraph*> Graphs; C.Blueprint->GetAllGraphs(Graphs);if(C.NameScope.IsValid()&&!C.NameScope->Check())return false;
    if (Graphs.Num() > 256 || !Graphs.Contains(C.Graph)) { return false; }
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<FGuid> GraphIds; int32 TotalPins = 0, TotalNodes = 0;
    for (const UEdGraph* Graph : Graphs)
    {
        if (!IsValid(Graph) || !Graph->GraphGuid.IsValid() || GraphIds.Contains(Graph->GraphGuid) || Graph->Nodes.Num() > 2048 || (TotalNodes += Graph->Nodes.Num()) > 8192) { return false; }
        GraphIds.Add(Graph->GraphGuid); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), Graph->GetPathName()); Row->SetStringField(TEXT("guid"), Graph->GraphGuid.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("name"), Graph->GetName()); Row->SetStringField(TEXT("type"), GraphKind(C.Blueprint, Graph)); TArray<TSharedPtr<FJsonValue>> Nodes; TSet<FGuid> NodeIds;
        const auto NodeCopies=TrackedMakeArray(C)?Graph->Nodes:TArray<TObjectPtr<UEdGraphNode>>();const auto& NodeRows=TrackedMakeArray(C)?NodeCopies:Graph->Nodes;
        for (const UEdGraphNode* Node : NodeRows)
        {
            if (!IsValid(Node) || !Node->NodeGuid.IsValid() || NodeIds.Contains(Node->NodeGuid) || Node->Pins.Num() > 512) { return false; } NodeIds.Add(Node->NodeGuid);
            TSet<FGuid> PinIds;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (++TotalPins > 16384 || !Pin || Pin->WasTrashed() || !Pin->PinId.IsValid() || PinIds.Contains(Pin->PinId)) { return false; } PinIds.Add(Pin->PinId);
                for (const UEdGraphPin* Other : Pin->LinkedTo)
                { if (!Other || Other->WasTrashed() || !IsValid(Other->GetOwningNodeUnchecked()) || !Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(Pin))) { return false; } }
            }
            auto Observed=NodeState(Node,C.EnumScope.Get(),C.VectorScope.Get(),C.NameScope.Get(),TrackedMakeArray(C)&&C.ExtendedBound?&C:nullptr);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return false; }
            Nodes.Add(MakeShared<FJsonValueObject>(Observed));
        }
        Row->SetArrayField(TEXT("nodes"), Nodes); Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("graphs"), Rows); return true;
}

bool PriorPreserved(const FContext& C, const TSharedRef<FJsonObject>& After, const UEdGraphNode* Created)
{
    if (!IsValid(Created) || Created->GetGraph() != C.Graph) { return false; }
    auto Expected = MakeShared<FJsonObject>(); auto Actual = MakeShared<FJsonObject>();
    for (const auto& Pair : C.Before->Values) { const FString Key(Pair.Key.ToView()); if (Key != TEXT("dirty") && Key != TEXT("blueprint_status")) { Expected->SetField(Key, Pair.Value); } }
    for (const auto& Pair : After->Values) { const FString Key(Pair.Key.ToView()); if (Key != TEXT("dirty") && Key != TEXT("blueprint_status")) { Actual->SetField(Key, Pair.Value); } }
    TArray<TSharedPtr<FJsonValue>> Graphs; int32 Removed = 0;
    for (const auto& Value : After->GetArrayField(TEXT("graphs")))
    {
        auto Graph = MakeShared<FJsonObject>(); const auto Original = Value->AsObject();
        for (const auto& Pair : Original->Values) { Graph->SetField(FString(Pair.Key.ToView()), Pair.Value); }
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const auto& Node : Original->GetArrayField(TEXT("nodes")))
        {
            if (Node->AsObject()->GetStringField(TEXT("object_path")) == Created->GetPathName()) { ++Removed; continue; } Nodes.Add(Node);
        }
        Graph->SetArrayField(TEXT("nodes"), Nodes); Graphs.Add(MakeShared<FJsonValueObject>(Graph));
    }
    Actual->SetArrayField(TEXT("graphs"), Graphs); return Removed == 1 && Text(Expected) == Text(Actual);
}

TSharedRef<FJsonObject> NativeState(FContext& C)
{
    auto Row = MakeShared<FJsonObject>(); bool Complete = FullyLoaded(C.NodeClass)
        && C.NodeClass->GetPathName() == TEXT("/Script/BlueprintGraph.K2Node_MakeArray")
        && C.NodeClass->HasAnyClassFlags(CLASS_Native)
        && !C.NodeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        && (C.NameScope.IsValid()?C.NodeClass==UK2Node_MakeArray::StaticClass():C.NodeClass->IsChildOf(UK2Node_MakeArray::StaticClass()))
        && C.NodeClass->GetOutermost()->GetName() == TEXT("/Script/BlueprintGraph")
        && !C.NodeClass->GetOutermost()->IsDirty() && FullyLoaded(C.Schema);
    Row->SetBoolField(TEXT("observations_complete"), Complete);
    Row->SetStringField(TEXT("node_class"), Path(C.NodeClass));
    Row->SetStringField(TEXT("schema_class"), C.Schema ? Path(C.Schema->GetClass()) : FString());
    Row->SetStringField(TEXT("schema_cdo"), Path(C.Schema));
    if (Complete)
    {
        Row->SetStringField(TEXT("class_flags_string"), LexToString(static_cast<uint32>(C.NodeClass->GetClassFlags())));
        Row->SetStringField(TEXT("class_super"), Path(C.NodeClass->GetSuperClass()));if(C.NameScope.IsValid()&&!C.NameScope->Check())return Row;
        Row->SetStringField(TEXT("class_package"), C.NodeClass->GetOutermost()->GetName());
        Row->SetBoolField(TEXT("class_package_dirty"), C.NodeClass->GetOutermost()->IsDirty());
        Row->SetStringField(TEXT("class_package_flags_string"), LexToString(static_cast<uint32>(C.NodeClass->GetOutermost()->GetPackageFlags())));
    }
    if (C.ElementType == TEXT("vector"))
    {
        const UScriptStruct* Vector = Cast<UScriptStruct>(C.ElementPinType.PinSubCategoryObject.Get());
        const bool VectorComplete = FullyLoaded(Vector) && Vector == TBaseStructure<FVector>::Get()
            && Vector->GetPathName() == TEXT("/Script/CoreUObject.Vector")
            && Vector->GetStructureSize() == sizeof(FVector) && !Vector->IsEditorOnly()
            && Vector->GetOutermost()->GetName() == TEXT("/Script/CoreUObject")
            && !Vector->GetOutermost()->IsDirty()
            && !Vector->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
        Row->SetBoolField(TEXT("native_vector_struct_observed"), VectorComplete);
        Row->SetStringField(TEXT("native_vector_struct_path"), Path(Vector));
        if (VectorComplete)
        {
            Row->SetNumberField(TEXT("native_vector_struct_size"), Vector->GetStructureSize());
            Row->SetBoolField(TEXT("native_vector_package_dirty"), Vector->GetOutermost()->IsDirty());
            Row->SetStringField(TEXT("native_vector_package_flags_string"), LexToString(static_cast<uint32>(Vector->GetOutermost()->GetPackageFlags())));
        }
        Row->SetBoolField(TEXT("observations_complete"), Complete && VectorComplete);
    }
    return Row;
}
FEdGraphPinType PinType(const FContext& C, bool bOutput, bool bTyped)
{
    FEdGraphPinType Type = bTyped ? C.ElementPinType : FEdGraphPinType();
    if (!bTyped) { Type.PinCategory = UEdGraphSchema_K2::PC_Wildcard; }
    if (bOutput) { Type.ContainerType = EPinContainerType::Array; } return Type;
}
bool Shape(const FContext& C, const UK2Node_MakeArray* Node, bool bTyped)
{
    if (!IsValid(Node) || Node->GetClass() != C.NodeClass || Node->GetGraph() != C.Graph
        || !C.Graph->Nodes.Contains(Node) || !Node->NodeGuid.IsValid() || Node->NumInputs != C.InputCount
        || Node->NodePosX != C.X || Node->NodePosY != C.Y || Node->Pins.Num() != C.InputCount + 1) { return false; }
    TSet<FGuid> Ids;
    for (int32 Index = 0; Index < Node->Pins.Num(); ++Index)
    {
        const UEdGraphPin* Pin = Node->Pins[Index]; const bool bOutput = Index == 0;
        const FName Name = bOutput ? FName(TEXT("Array")) : FName(*FString::Printf(TEXT("[%d]"), Index - 1));
        FString ScopedActual,ScopedExpected;if(C.NameScope.IsValid()){if(!C.NameScope->Check())return false;const auto Copied=Pin->PinType;ScopedActual=TypeText(Copied);if(!C.NameScope->Check())return false;ScopedExpected=TypeText(PinType(C,bOutput,bTyped));if(!C.NameScope->Check())return false;}
        FString ExtendedActual,ExtendedExpected;
        if(TrackedMakeArray(C)){if(!ExtendedObservedCurrent(const_cast<FContext*>(&C)))return false;if(!Pin||Pin->WasTrashed())return false;const FEdGraphPinType Actual=Pin->PinType,Expected=PinType(C,bOutput,bTyped);ExtendedActual=TypeText(Actual);if(!ExtendedObservedCurrent(const_cast<FContext*>(&C)))return false;ExtendedExpected=TypeText(Expected);if(!ExtendedObservedCurrent(const_cast<FContext*>(&C)))return false;}
        if (!Pin || Pin->WasTrashed() || Pin->GetOwningNodeUnchecked() != Node || !Pin->PinId.IsValid()
            || Ids.Contains(Pin->PinId) || Pin->PinName != Name || Pin->Direction != (bOutput ? EGPD_Output : EGPD_Input)
            || (TrackedMakeArray(C)?ExtendedActual!=ExtendedExpected:C.NameScope.IsValid()?ScopedActual!=ScopedExpected:TypeText(Pin->PinType) != TypeText(PinType(C, bOutput, bTyped))) || Pin->ParentPin || !Pin->SubPins.IsEmpty()
            || !Pin->LinkedTo.IsEmpty() || Pin->bHidden || Pin->bNotConnectable || Pin->bDefaultValueIsReadOnly
            || Pin->bDefaultValueIsIgnored || Pin->bOrphanedPin || ((!Descriptor(C)||bOutput)&&Pin->DefaultObject) || (Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Text&&!Pin->PinType.IsContainer()?!Pin->DefaultValue.IsEmpty():!Pin->DefaultTextValue.IsEmpty())) { return false; }
        Ids.Add(Pin->PinId);
    }
    return Node->GetOutputPin() == Node->Pins[0];
}
bool DefaultsMatch(FContext& C, const UK2Node_MakeArray* Node)
{
    if (!IsValid(Node) || Node->Pins.Num() != C.InputCount + 1 || C.DefaultsBeforeValues.Num() != C.InputCount) { return false; }
    for (int32 Index = 0; Index < C.InputCount; ++Index)
    {
        if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return false; }
        const UEdGraphPin* Pin = Node->Pins[Index + 1];
        const FString& Expected = TrackedMakeArray(C)?C.ExtendedSchemaValues[Index]:C.Values.IsValidIndex(Index) ? C.Values[Index] : C.DefaultsBeforeValues[Index];
        if(Descriptor(C)){if(!DescriptorDefaultMatch(C,Node,Index))return false;continue;}
        if(!Pin||Pin->WasTrashed()||Pin->DefaultObject||(Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Text?!Pin->DefaultValue.IsEmpty():!Pin->DefaultTextValue.IsEmpty())) { return false; }
        const bool Match=C.Schema->DoesDefaultValueMatch(*Pin,Expected);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return false; }Pin=Node->Pins[Index+1];
        const FString Validation=C.Schema->IsCurrentPinDefaultValid(Pin);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return false; }
        if(!Match||!Validation.IsEmpty()) { return false; }
    }
    return true;
}
void Finish(FContext& C, FResult& R, UK2Node_MakeArray* Node, bool Good)
{
    if (!R.ErrorCode.IsEmpty()) { R.Data->SetStringField(TEXT("failed_mutation_stage"), R.Stage); }
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return; }
    R.Stage = TEXT("verification"); auto After = MakeShared<FJsonObject>();
    const bool TargetKnown = Capture(C, After);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return; }const auto NativeAfter = NativeState(C);if(C.NameScope.IsValid()&&!C.NameScope->Check())return;if(C.VectorScope.IsValid()&&!C.VectorScope->Check()) { return; }
    const bool NativeKnown = NativeAfter->GetBoolField(TEXT("observations_complete"));
    R.Data->SetObjectField(TEXT("after"), After); R.Data->SetObjectField(TEXT("native_source_after"), NativeAfter);
    R.bChangeStateKnown = TargetKnown && NativeKnown;
    R.bChanged = (TargetKnown && Text(C.Before) != Text(After)) || (NativeKnown && Text(C.NativeBefore) != Text(NativeAfter));
    const bool Prior = TargetKnown && PriorPreserved(C, After, Node);
    const bool NativePreserved = NativeKnown && Text(C.NativeBefore) == Text(NativeAfter);
    bool Declarations = false;
    if (TargetKnown)
    {
        auto Expected = MakeShared<FJsonObject>(); auto Actual = MakeShared<FJsonObject>();
        for (const TCHAR* Key : {TEXT("variables"), TEXT("interfaces")})
        { Expected->SetArrayField(Key, C.Before->GetArrayField(Key)); Actual->SetArrayField(Key, After->GetArrayField(Key)); }
        Declarations = Text(Expected) == Text(Actual);
    }
    R.Data->SetBoolField(TEXT("existing_nodes_preserved"), Prior);
    R.Data->SetBoolField(TEXT("declarations_preserved"), Declarations);
    R.Data->SetBoolField(TEXT("native_source_observed_unchanged"), NativePreserved);
    R.Data->SetBoolField(TEXT("created"), IsValid(Node) && C.Graph->Nodes.Contains(Node));
    if (IsValid(Node))
    {
        const auto State = NodeState(Node,C.EnumScope.Get(),C.VectorScope.Get(),C.NameScope.Get(),TrackedMakeArray(C)&&C.ExtendedBound?&C:nullptr);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return; }R.Data->SetObjectField(TEXT("node"), State);
        for (const TCHAR* Key : {TEXT("node_id"), TEXT("node_guid"), TEXT("object_path"), TEXT("node_class")}) { R.Data->SetStringField(Key, State->GetStringField(Key)); }
        R.Data->SetArrayField(TEXT("pins"), State->GetArrayField(TEXT("pins"))); R.Data->SetObjectField(TEXT("graph"), GraphDescription(C));
    }
    const bool Typed = Shape(C, Node, true), Defaults = DefaultsMatch(C, Node) && (!Descriptor(C) || R.Data->GetIntegerField(TEXT("values_applied_count")) == C.Values.Num());
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return; }
    R.Data->SetBoolField(TEXT("typed_node_shape_verified"), Typed);
    R.Data->SetBoolField(TEXT("requested_defaults_verified"), Defaults);
    const bool DescriptorPreserved=FinishDescriptor(C,R);if(!DescriptorPreserved)return;
    const bool Verified = Good && R.bChangeStateKnown && Prior && NativePreserved && Declarations && Typed && Defaults&&DescriptorPreserved;
    R.Data->SetBoolField(TEXT("node_only_verified"), Verified);
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("bounded owned graphs/full pin/default/link and variable/interface identities plus dirty/status; loaded native class/schema metadata. Arbitrary Editor callbacks/disk writes are not fully observed."));
    R.Data->SetStringField(TEXT("partial_failure_policy"), TEXT("retain created node and applied defaults, report exact known/unknown state; no DestroyNode/rollback/compile/save/automatic retry"));
    if (!Verified)
    {
        if (R.ErrorCode.IsEmpty()) { Error(R, TEXT("make_array_postcondition_failed"), TEXT("Typed node/default shape or prior/native preservation failed; inspect the retained partial state before a continuation.")); }
        return;
    }
    if(C.NameScope.IsValid()){auto Context=C.NameScope->Context();if(!Context.IsValid()||!C.NameScope->Check())return;R.Data->SetObjectField(TEXT("bfl_name_array_context_after"),Context);R.Data->SetBoolField(TEXT("name_array_target_current_verified"),true);}
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete");
}
}

Function::FResult AddNode(const FString& BlueprintPath, const FString& ElementType,
    const FString& InputCountJson, const FString& ValuesJson, const FString& TypeObject,
    const FString& EnumType, const FString& NodePositionJson, const FString& GraphName,
    const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing)
{
    FResult R; FContext C; const auto& Version = FEngineVersion::Current();
    R.Data->SetBoolField(TEXT("node_only_contract"), true);
    R.Data->SetBoolField(TEXT("implicit_data_wiring"), false);
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_requested"), false);
    R.Data->SetBoolField(TEXT("explicit_compile_requested"), false);
    R.Data->SetBoolField(TEXT("explicit_save_requested"), false);
    R.Data->SetBoolField(TEXT("full_legacy_make_array_parity"), false);
    R.Data->SetBoolField(TEXT("wildcard_reconstruction_persistence_verified"), false);
    R.Data->SetStringField(TEXT("requested_element_type"), ElementType);
    R.Data->SetStringField(TEXT("requested_type_object"), TypeObject);
    R.Data->SetStringField(TEXT("requested_enum_type"), EnumType);
    R.Data->SetStringField(TEXT("requested_graph_name"), GraphName);
    R.Data->SetStringField(TEXT("requested_graph_id"), GraphId);
    R.Data->SetStringField(TEXT("requested_graph_type"), GraphType);
    R.Data->SetBoolField(TEXT("create_graph_if_missing"), bCreateGraphIfMissing);
    R.Data->SetStringField(TEXT("count_precedence"), TEXT("omitted count/values -> 1; supplied list sets count; explicit count overrides; fewer values retain native typed autogenerated defaults; explicit []/0 stays empty"));
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Error(R, TEXT("make_array_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8 || Version.GetPatch() != 3)
    { Error(R, TEXT("make_array_version_unsupported"), TEXT("Only installed UE 5.8.3 is implemented.")); return R; }
    if (!Arguments(C, R, ElementType, InputCountJson, ValuesJson, TypeObject, EnumType, NodePositionJson, bCreateGraphIfMissing)
        || !ResolveTarget(C, R, BlueprintPath, GraphName, GraphId, GraphType)) { return R; }
    if(C.EnumScope.IsValid()&&!C.EnumScope->SetTarget(C.Blueprint,C.Graph,C.Schema)) { return R; }
    if(C.ElementType==TEXT("vector")&&!C.Values.IsEmpty()) { C.VectorScope=MakeShared<FVectorArrayScope>();if(!C.VectorScope->Init(C.Blueprint,C.Graph,C.Schema,Cast<UScriptStruct>(C.ElementPinType.PinSubCategoryObject.Get()),R)) { return R; } }
    C.NodeClass = C.NameScope.IsValid()?UK2Node_MakeArray::StaticClass():FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.K2Node_MakeArray"));if(C.NameScope.IsValid()&&!C.NameScope->Check())return R;
    if(!ExtendedBind(C,R)||!BindDescriptor(C,R))return R;
    C.NativeBefore = NativeState(C);if(C.NameScope.IsValid()&&!C.NameScope->Check())return R;if(C.VectorScope.IsValid()&&!C.VectorScope->Check()) { return R; }const auto RepeatedNative = NativeState(C);if(C.NameScope.IsValid()&&!C.NameScope->Check())return R;if(C.VectorScope.IsValid()&&!C.VectorScope->Check()) { return R; }
    if (!C.NativeBefore->GetBoolField(TEXT("observations_complete")) || Text(C.NativeBefore) != Text(RepeatedNative))
    { Error(R, TEXT("make_array_native_class_unavailable"), C.DescriptorLoadPrefix.IsEmpty()?TEXT("Exact loaded native MakeArray class/schema metadata is absent/incomplete or changed; no allocation/load attempted."):TEXT("Native MakeArray class/schema metadata is absent/incomplete or changed after recorded source/default loading; node allocation was not attempted and the actual load prefix/global-effects limitation is retained.")); return R; }
    R.Data->SetObjectField(TEXT("native_source_before"), C.NativeBefore);
    R.Data->SetStringField(TEXT("native_contract_basis"), TEXT("installed UE58 K2Node_MakeContainer public NumInputs/allocation/GetOutputPin/schema fields; observed constructor proof remains pending"));
    if (!Capture(C, C.Before))
    { if((C.NameScope.IsValid())||(C.EnumScope.IsValid()&&C.EnumScope->Stopped)||(C.VectorScope.IsValid()&&C.VectorScope->Stopped)) { return R; }Error(R, TEXT("make_array_before_state_unobservable"), TEXT("Bounded existing graph/declaration/pin state incomplete; no mutation dispatched.")); return R; }
    R.Data->SetObjectField(TEXT("before"), C.Before);
    TStrongObjectPtr<UBlueprint> KeepTarget(C.Blueprint);
    UK2Node_MakeArray* Node=nullptr;
    R.Stage=TEXT("create_make_array_node");R.bMutationAttempted=true;
    C.Blueprint->Modify();if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
    C.Graph->Modify();if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
    if(C.NameScope.IsValid())
    {
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);
        Node=NewObject<UK2Node_MakeArray>(C.Graph,NAME_None,RF_Transactional);
        R.Data->SetNumberField(TEXT("node_allocation_returns"),1);
        if(!C.NameScope->Adopt(Node)||!C.NameScope->Place(C.X,C.Y,C.InputCount))return R;
        if(!C.NameScope->Attach())return R;
        R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(Node,false,false);
        R.Data->SetNumberField(TEXT("graph_add_node_returns"),1);R.bChanged=true;if(!C.NameScope->Check())return R;
        
        Node->CreateNewGuid();if(!C.NameScope->FixGuid())return R;
        Node->PostPlacedNewNode();if(!C.NameScope->Check())return R;
        if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!C.NameScope->Check())return R;}
        if(!C.NameScope->Freeze())return R;
    }
    else if(C.EnumScope.IsValid())
    {
        UClass* AllocationClass=UK2Node_MakeArray::StaticClass();if(!C.EnumScope->Check()) { return R; }
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);
        Node=NewObject<UK2Node_MakeArray>(C.Graph,NAME_None,RF_Transactional);
        R.Data->SetNumberField(TEXT("node_allocation_returns"),1);R.Data->SetBoolField(TEXT("node_allocation_returned_nonnull"),Node!=nullptr);
        if(!C.EnumScope->SetNode(Node,AllocationClass)) { return R; }
        if(C.Graph->HasAnyFlags(RF_Transient)) { Node->SetFlags(RF_Transient); }
        C.EnumScope->Attached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);
        C.Graph->AddNode(Node,false,false);if(!C.EnumScope->Check()) { return R; }
        Node->NumInputs=C.InputCount;Node->NodePosX=C.X;Node->NodePosY=C.Y;
        Node->CreateNewGuid();if(!C.EnumScope->Check()) { return R; }
        C.EnumScope->NodeGuid=Node->NodeGuid;C.EnumScope->GuidFixed=true;
        Node->PostPlacedNewNode();if(!C.EnumScope->Check()) { return R; }
        if(Node->Pins.IsEmpty()) { Node->AllocateDefaultPins();if(!C.EnumScope->Check()) { return R; } }
        if(!C.EnumScope->FreezePins()) { return R; }
    }
    else if(C.VectorScope.IsValid())
    {
        UClass* AllocationClass=UK2Node_MakeArray::StaticClass();if(!C.VectorScope->Check()) { return R; }
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);Node=NewObject<UK2Node_MakeArray>(C.Graph,NAME_None,RF_Transactional);
        R.Data->SetNumberField(TEXT("node_allocation_returns"),1);R.Data->SetBoolField(TEXT("node_allocation_returned_nonnull"),Node!=nullptr);
        if(!C.VectorScope->SetNode(Node,AllocationClass)) { return R; }if(C.Graph->HasAnyFlags(RF_Transient)) { Node->SetFlags(RF_Transient); }
        C.VectorScope->Attached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(Node,false,false);if(!C.VectorScope->Check()) { return R; }
        Node->NumInputs=C.InputCount;Node->NodePosX=C.X;Node->NodePosY=C.Y;Node->CreateNewGuid();if(!C.VectorScope->Check()) { return R; }
        C.VectorScope->NodeGuid=Node->NodeGuid;C.VectorScope->GuidFixed=true;Node->PostPlacedNewNode();if(!C.VectorScope->Check()) { return R; }
        if(Node->Pins.IsEmpty()) { Node->AllocateDefaultPins();if(!C.VectorScope->Check()) { return R; } }if(!C.VectorScope->FreezePins()) { return R; }
    }
    else if(TrackedMakeArray(C))
    {
        R.Data->SetNumberField(TEXT("node_allocation_calls"),1);Node=NewObject<UK2Node_MakeArray>(C.Graph,NAME_None,RF_Transactional);R.Data->SetNumberField(TEXT("node_allocation_returns"),1);C.ExtendedCreated.Reset(Node);if(!Node||!ExtendedObservedCurrent(&C))return R;
        if(C.Graph->HasAnyFlags(RF_Transient))Node->SetFlags(RF_Transient);
        C.ExtendedAttached=true;R.Data->SetNumberField(TEXT("graph_add_node_calls"),1);C.Graph->AddNode(Node,false,false);R.Data->SetNumberField(TEXT("graph_add_node_returns"),1);R.bChanged=true;if(!ExtendedObservedCurrent(&C))return R;
        Node->NumInputs=C.InputCount;Node->NodePosX=C.X;Node->NodePosY=C.Y;Node->CreateNewGuid();if(!ExtendedObservedCurrent(&C))return R;
        Node->PostPlacedNewNode();if(!ExtendedObservedCurrent(&C))return R;if(Node->Pins.IsEmpty()){Node->AllocateDefaultPins();if(!ExtendedObservedCurrent(&C))return R;}
        if(!ExtendedFreeze(C,R,Node))return R;
    }
    else
    {
        FGraphNodeCreator<UK2Node_MakeArray> Creator(*C.Graph);Node=Creator.CreateNode(false);
        Node->NumInputs=C.InputCount;Node->NodePosX=C.X;Node->NodePosY=C.Y;Creator.Finalize();
    }
    bool Good = Shape(C, Node, false);
    R.Data->SetBoolField(TEXT("allocated_native_shape_verified"), Good);
    R.Data->SetObjectField(TEXT("allocated_node_before_defaults"), NodeState(Node,C.EnumScope.Get(),C.VectorScope.Get(),C.NameScope.Get(),TrackedMakeArray(C)&&C.ExtendedBound?&C:nullptr));
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
    if (!Good) { Error(R, TEXT("make_array_allocated_shape_mismatch"), TEXT("Native allocated wildcard output/inputs differ from exact public allocation source; retained node is not repaired.")); }
    auto Applied = MakeShared<FJsonObject>();auto ValueObservations=MakeShared<FJsonObject>();if(TrackedMakeArray(C)){R.Data->SetObjectField(TEXT("values_applied"),Applied);R.Data->SetObjectField(TEXT("value_default_observations"),ValueObservations);}
    if (Good)
    {
        R.Stage = TEXT("specialize_make_array_pins");
        for (int32 PinIndex=0;PinIndex<Node->Pins.Num();++PinIndex)
        {
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
            UEdGraphPin* Pin=Node->Pins[PinIndex];
            FEdGraphPinType Expected=Pin->PinType;Expected.PinCategory=C.ElementPinType.PinCategory;Expected.PinSubCategory=C.ElementPinType.PinSubCategory;Expected.PinSubCategoryObject=C.ElementPinType.PinSubCategoryObject;
            if(C.EnumScope.IsValid()&&!C.EnumScope->ExpectType(Pin,Expected)) { return R; }
            if(C.VectorScope.IsValid()&&!C.VectorScope->ExpectType(Pin,Expected)) { return R; }
            if(C.NameScope.IsValid()&&!C.NameScope->ExpectType(Pin,Expected))return R;
            if(TrackedMakeArray(C)){for(auto& Role:C.ExtendedPins)if(Role.Address==Pin)Role.Type=Expected;}
            Pin->PinType.PinCategory = C.ElementPinType.PinCategory;
            Pin->PinType.PinSubCategory = C.ElementPinType.PinSubCategory;
            Pin->PinType.PinSubCategoryObject = C.ElementPinType.PinSubCategoryObject;
            if (Pin->Direction == EGPD_Input)
            {
                if(C.NameScope.IsValid()&&!C.NameScope->ExpectDefault(Pin,TEXT("None"),TEXT("None")))return R;
                C.Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
                if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
                Pin=Node->Pins[PinIndex];C.DefaultsBeforeValues.Add(Pin->DefaultValue);if(TrackedMakeArray(C)){const FString ActualDefault=Pin->GetDefaultAsString();if(!ExtendedObservedCurrent(&C))return R;C.ExtendedSchemaValues.Add(ActualDefault);}
            }
        }
        R.Data->SetObjectField(TEXT("typed_node_before_values"), NodeState(Node,C.EnumScope.Get(),C.VectorScope.Get(),C.NameScope.Get(),TrackedMakeArray(C)&&C.ExtendedBound?&C:nullptr));
        if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
        Good = Shape(C, Node, true);
        if (!Good) { Error(R, TEXT("make_array_specialization_mismatch"), TEXT("Primitive specialization changed an unexpected native pin shape; retained state requires inspection.")); }
    }
    if (Good)
    {
        R.Stage = TEXT("apply_make_array_values");
        if(Descriptor(C)){Good=ApplyDescriptorValues(C,R,Node,Applied,ValueObservations);}
        else for (int32 Index = 0; Index < C.Values.Num(); ++Index)
        {
            UEdGraphPin* Pin = Node->Pins[Index + 1];
            FString SchemaLiteral=C.Values[Index];FText ValidationText;
            if(C.ElementType==TEXT("text"))
            {
                FString ExpectedRaw;TObjectPtr<UObject> ExpectedObject=nullptr;FText ExpectedText;const FEdGraphPinType ActualType=Pin->PinType;
                C.Schema->GetPinDefaultValuesFromString(ActualType,Node,C.Values[Index],ExpectedRaw,ExpectedObject,ExpectedText,false);if(!ExtendedObservedCurrent(&C))return R;
                if(ExpectedObject||!ExpectedRaw.IsEmpty()){Good=false;Error(R,TEXT("make_array_text_conversion_invalid"),TEXT("Stock Text conversion returned a non-Text cell."));break;}
                ValidationText=ExpectedText;SchemaLiteral.Reset();FTextStringHelper::WriteToBuffer(SchemaLiteral,ExpectedText);if(!ExtendedObservedCurrent(&C))return R;Pin=Node->Pins[Index+1];
            }
            if(ExtendedPrimitive(C))C.ExtendedSchemaValues[Index]=SchemaLiteral;
            const FString Validation=C.Schema->IsPinDefaultValid(Pin,C.ElementType==TEXT("text")?FString():SchemaLiteral,nullptr,ValidationText);
            if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }Pin=Node->Pins[Index+1];
            if (!Validation.IsEmpty())
            { Good = false; Error(R, TEXT("make_array_engine_default_rejected"), TEXT("Engine rejected a strict typed value on the retained new input: ") + Pin->PinName.ToString()); break; }
            if(C.NameScope.IsValid()&&!C.NameScope->ExpectDefault(Pin,C.Values[Index],Pin->AutogeneratedDefaultValue))return R;
            C.Schema->TrySetDefaultValue(*Pin,SchemaLiteral,false);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }Pin=Node->Pins[Index+1];
            if(C.NameScope.IsValid()||C.EnumScope.IsValid()||C.VectorScope.IsValid()) { Node->PinDefaultValueChanged(Pin);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }Pin=Node->Pins[Index+1]; }
            const bool Match=C.Schema->DoesDefaultValueMatch(*Pin,SchemaLiteral);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }Pin=Node->Pins[Index+1];
            const FString CurrentError=C.Schema->IsCurrentPinDefaultValid(Pin);if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }Pin=Node->Pins[Index+1];
            if (!Match||!CurrentError.IsEmpty()||Pin->DefaultObject||(C.ElementType==TEXT("text")?!Pin->DefaultValue.IsEmpty():!Pin->DefaultTextValue.IsEmpty()))
            { Good = false; Error(R, TEXT("make_array_default_readback_mismatch"), TEXT("Actual retained input default differs from the requested value: ") + Pin->PinName.ToString()); break; }
            Applied->SetStringField(Pin->PinName.ToString(), Pin->DefaultValue);
            if(ExtendedPrimitive(C))
            {
                const FString PinName=Pin->PinName.ToString(),Raw=Pin->DefaultValue;const FGuid Id=Pin->PinId;const FEdGraphPinType Type=Pin->PinType;const FText TextValue=Pin->DefaultTextValue;
                const FString Display=TextValue.ToString();if(!ExtendedObservedCurrent(&C))return R;Pin=Node->Pins[Index+1];const FString DefaultAsString=Pin->GetDefaultAsString();if(!ExtendedObservedCurrent(&C))return R;
                auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("requested_literal"),C.Values[Index]);O->SetStringField(TEXT("schema_literal"),SchemaLiteral);O->SetStringField(TEXT("raw_default"),Raw);O->SetStringField(TEXT("default_text"),Display);O->SetStringField(TEXT("default_as_string"),DefaultAsString);O->SetBoolField(TEXT("schema_match"),Match);O->SetBoolField(TEXT("current_default_valid"),CurrentError.IsEmpty());O->SetStringField(TEXT("pin_id"),Id.ToString(EGuidFormats::Digits));O->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(Type));if(!ExtendedObservedCurrent(&C))return R;ValueObservations->SetObjectField(PinName,O);
            }
        }
    }
    R.Data->SetObjectField(TEXT("values_applied"), Applied);
    R.Data->SetNumberField(TEXT("values_applied_count"), Applied->Values.Num());
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
    if(IsValid(Node)&&C.Graph->Nodes.Contains(Node)) { FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint); }
    if((C.NameScope.IsValid()&&!C.NameScope->Check())||(C.EnumScope.IsValid()&&!C.EnumScope->Check())||(C.VectorScope.IsValid()&&!C.VectorScope->Check())||!ExtendedObservedCurrent(&C)) { return R; }
    R.Data->SetStringField(TEXT("runtime_reference_scope"), C.EnumScope.IsValid()?TEXT("exact read-only runtime UserDefinedEnum byte pins and native MakeArray; full source entries/stored fields/metadata/disk preserved; compile/cold/cook proof separate"):TEXT("native Engine collection element and compiler CreateArray only; no Cubeless runtime type/function/content introduced; compile/cold/cook proof is separate"));
    Finish(C, R, Node, Good); return R;
}
}
