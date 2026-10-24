// UE56 BlueprintNodeCommands Part1 variable contracts, adapted to public UE58 APIs.
// Transport, Python journals and public tool registration remain separate.
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/UserDefinedEnum.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "UObject/MetaData.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompilerMisc.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectArray.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "NiagaraSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Interfaces/IPluginManager.h"

namespace Cubeless::UE58::Variable
{
// PK: OQ bounded scalar UDS source roles, local declaration/Get/Set only.
// No source writes, default-buffer values, loader or generic executor.
struct FLocalScalarUdsSource::FState
{
 bool Stopped=false,Unknown=false,CurrentObservation=false;TFunction<bool()> TargetCurrent;TFunction<bool(const TCHAR*,const TCHAR*)> Failure;
 struct FHeldRole{TStrongObjectPtr<UObject> Object;UObject* Outer=nullptr;UClass* Class=nullptr;EObjectFlags Flags=RF_NoFlags;FString Path;};TArray<FHeldRole> Held;
 static bool Loaded(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
 bool Bad(const TCHAR* Code,const TCHAR* Why=TEXT("Captured scalar UDS source/current changed; no next SDK, cleanup or retry.")){if(Stopped)return false;Unknown|=CurrentObservation;Stopped=true;return Failure(Code,Why);}
 bool Hold(UObject* O){if(!Loaded(O))return Bad(TEXT("local_uds_source_hold_failed"));FHeldRole Role;Role.Object.Reset(O);Role.Outer=O->GetOuter();Role.Class=O->GetClass();Role.Flags=O->GetFlags();Role.Path=O->GetPathName();Held.Add(MoveTemp(Role));return true;}
 bool Fence(){if(Stopped)return false;if(!IsInGameThread()||!GEditor||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()){Unknown=true;return Bad(TEXT("local_uds_context_changed"));}if(!TargetCurrent()){Unknown=true;return Bad(TEXT("local_uds_target_changed"));}return UdsPlainCurrent();}
    bool UdsReady=false,UdsMetadataReady=false;
    TStrongObjectPtr<UUserDefinedStruct> UdsType;TStrongObjectPtr<UUserDefinedStructEditorData> UdsEditor;
    TStrongObjectPtr<UPackage> UdsPackage;FGuid UdsGuid;uint32 UdsFlags=0,UdsPackageFlags=0;int32 UdsSize=0,UdsAlignment=0;
    FString UdsPath,UdsEditorPath,UdsPackagePath,UdsEditorTooltip,UdsFilename,UdsSHA1;int64 UdsFileBytes=-1;
    TMap<FName,FString> UdsMetadata,UdsEditorMetadata,UdsEditorStoredMetadata;
    struct FUdsLengthField{const FProperty* Address=nullptr;FFieldClass* FieldClass=nullptr;FName InternalName,Category,SubCategory;FGuid Guid;FString FriendlyName,AuthoredName,Tooltip,SubtypePath;FEdGraphTerminalType Terminal;uint32 DescriptorBits=0;uint64 Flags=0;int32 Offset=0,Size=0;TMap<FName,FString> Metadata,DescriptorMetadata;};
    TArray<FUdsLengthField> UdsFields;
    static bool UdsMapSame(const TMap<FName,FString>& A,const TMap<FName,FString>& B)
    {if(A.Num()!=B.Num())return false;for(const auto& Pair:A){const FString* Actual=B.Find(Pair.Key);if(!Actual||!Actual->Equals(Pair.Value,ESearchCase::CaseSensitive))return false;}return true;}
    static uint32 UdsDescriptionBits(const FStructVariableDescription& D)
    {return (D.bInvalidMember?1U:0U)|(D.bDontEditOnInstance?2U:0U)|(D.bEnableSaveGame?4U:0U)|(D.bEnableMultiLineText?8U:0U)|(D.bEnable3dWidget?16U:0U);}
    bool UdsPlainCurrent()
    {
        if(Stopped)return false;if(!UdsReady)return true;TGuardValue<bool> Observing(CurrentObservation,true);
        for(const auto& Role:Held){const auto* O=Role.Object.Get();if(!Loaded(O)||O->GetOuter()!=Role.Outer||O->GetClass()!=Role.Class||O->GetFlags()!=Role.Flags||!O->GetPathName().Equals(Role.Path,ESearchCase::CaseSensitive))return Bad(TEXT("local_uds_held_identity"));}
        auto* UserType=UdsType.Get();auto* Data=UdsEditor.Get();
        if(!Loaded(UserType)||!Loaded(Data)||!Loaded(UdsPackage.Get())||UserType->GetClass()!=UUserDefinedStruct::StaticClass()||UserType->GetOuter()!=UdsPackage.Get()||!UserType->GetPathName().Equals(UdsPath,ESearchCase::CaseSensitive)||UserType->GetPackage()!=UdsPackage.Get()||UserType->Status!=UDSS_UpToDate||UserType->Guid!=UdsGuid||!UserType->PrimaryStruct.IsExplicitlyNull()||!UserType->EditorData.IsResolved()||UserType->EditorData!=Data||uint32(UserType->StructFlags)!=UdsFlags||UserType->GetStructureSize()!=UdsSize||UserType->GetMinAlignment()!=UdsAlignment||UserType->GetSuperStruct()!=nullptr||UdsPackage->GetPackageFlags()!=UdsPackageFlags||UdsPackage->IsDirty()||Data->GetClass()!=UUserDefinedStructEditorData::StaticClass()||Data->GetOuter()!=UserType||!Data->GetPathName().Equals(UdsEditorPath,ESearchCase::CaseSensitive))return Bad(TEXT("scalar_uds_length_source_role"));
        if(Data->VariablesDescriptions.Num()!=UdsFields.Num()||!Data->ToolTip.Equals(UdsEditorTooltip,ESearchCase::CaseSensitive)||!UdsMapSame(Data->MetaData,UdsEditorStoredMetadata))return Bad(TEXT("scalar_uds_length_editor_inventory"));
        int32 FieldIndex=0;
        for(TFieldIterator<FProperty> It(UserType,EFieldIterationFlags::None);It;++It,++FieldIndex)
        {
            if(!UdsFields.IsValidIndex(FieldIndex)||*It!=UdsFields[FieldIndex].Address)return Bad(TEXT("scalar_uds_length_field_membership"));
            const auto& FieldRole=UdsFields[FieldIndex];const auto* Physical=FieldRole.Address;
            if(Physical->GetOwnerVariant()!=UserType||Physical->GetClass()!=FieldRole.FieldClass||Physical->GetFName()!=FieldRole.InternalName||Physical->ArrayDim!=1||Physical->GetOffset_ForInternal()!=FieldRole.Offset||Physical->GetSize()!=FieldRole.Size||uint64(Physical->GetPropertyFlags())!=FieldRole.Flags)return Bad(TEXT("scalar_uds_length_field_identity"));
            const auto* Meta=Physical->GetMetaDataMap();if((Meta?Meta->Num():0)!=FieldRole.Metadata.Num()||(Meta&&!UdsMapSame(*Meta,FieldRole.Metadata)))return Bad(TEXT("scalar_uds_length_field_metadata"));
            const auto* Description=Data->VariablesDescriptions.FindByPredicate([&](const FStructVariableDescription& D){return D.VarName==FieldRole.InternalName;});
            if(!Description||Description->VarGuid!=FieldRole.Guid||!Description->FriendlyName.Equals(FieldRole.FriendlyName,ESearchCase::CaseSensitive)||Description->Category!=FieldRole.Category||Description->SubCategory!=FieldRole.SubCategory||Description->ContainerType!=EPinContainerType::None||!Description->SubCategoryObject.ToSoftObjectPath().ToString().Equals(FieldRole.SubtypePath,ESearchCase::CaseSensitive)||Description->PinValueType!=FieldRole.Terminal||Description->PinValueType.bTerminalIsUObjectWrapper!=FieldRole.Terminal.bTerminalIsUObjectWrapper||UdsDescriptionBits(*Description)!=FieldRole.DescriptorBits||!Description->ToolTip.Equals(FieldRole.Tooltip,ESearchCase::CaseSensitive)||!UdsMapSame(Description->MetaData,FieldRole.DescriptorMetadata))return Bad(TEXT("scalar_uds_length_authored_definition"));
        }
        return FieldIndex==UdsFields.Num()?true:Bad(TEXT("scalar_uds_length_field_count"));
    }
    bool UdsQueryCurrent(){return Fence();}
    bool UdsMetadataQuery(bool Capture)
    {
        for(int32 Index=0;Index<2;++Index)
        {
            UObject* Source=Index==0?static_cast<UObject*>(UdsType.Get()):static_cast<UObject*>(UdsEditor.Get());
            const auto* Returned=FMetaData::GetMapForObject(Source);TMap<FName,FString> Copy;if(Returned){if(Returned->Num()>256)return Bad(TEXT("scalar_uds_length_metadata_bound"));Copy=*Returned;}
            if(!UdsQueryCurrent())return false;
            auto& Expected=Index==0?UdsMetadata:UdsEditorMetadata;if(Capture)Expected=MoveTemp(Copy);else if(!UdsMapSame(Copy,Expected))return Bad(TEXT("scalar_uds_length_object_metadata"));
        }
        return true;
    }
    bool UdsAuthoredQuery(bool Capture)
    {
        if(!Fence())return false;
        const auto* Returned=FStructureEditorUtils::GetVarDescPtr(static_cast<const UUserDefinedStruct*>(UdsType.Get()));if(!Fence())return false;
        if(Returned!=&UdsEditor->VariablesDescriptions)return Bad(TEXT("scalar_uds_length_descriptor_return"));
        for(auto& FieldRole:UdsFields)
        {
            const FGuid ActualGuid=FStructureEditorUtils::GetGuidForProperty(FieldRole.Address);if(!Fence())return false;
            if(ActualGuid!=FieldRole.Guid)return Bad(TEXT("scalar_uds_length_authored_guid"));
            const FString Authored=FieldRole.Address->GetAuthoredName();if(!Fence())return false;
            if(Authored.IsEmpty()||(!Capture&&!Authored.Equals(FieldRole.AuthoredName,ESearchCase::CaseSensitive)))return Bad(TEXT("scalar_uds_length_authored_name"));if(Capture)FieldRole.AuthoredName=Authored;
        }
        return Fence();
    }
    bool UdsFileQuery(bool Capture)
    {
        TGuardValue<bool> Observing(CurrentObservation,!Capture);
        FString Filename;const bool Exists=FPackageName::DoesPackageExist(UdsPackagePath,&Filename);if(!Fence())return false;
        const int64 Bytes=Exists?IFileManager::Get().FileSize(*Filename):-1;
        if(!Exists||Bytes<0||Bytes>268435456)return Bad(TEXT("scalar_uds_length_saved_source_file"));
        TArray<uint8> Storage;const bool Read=FFileHelper::LoadFileToArray(Storage,*Filename);if(!Fence())return false;
        if(!Read||Storage.Num()!=Bytes)return Bad(TEXT("scalar_uds_length_source_file_read"));FSHAHash Hash;FSHA1::HashBuffer(Storage.GetData(),Storage.Num(),Hash.Hash);const FString Digest=Hash.ToString();
        if(Capture){UdsFilename=Filename;UdsFileBytes=Bytes;UdsSHA1=Digest;}else if(!Filename.Equals(UdsFilename,ESearchCase::CaseSensitive)||Bytes!=UdsFileBytes||!Digest.Equals(UdsSHA1,ESearchCase::CaseSensitive))return Bad(TEXT("scalar_uds_length_source_file_changed"));return true;
    }
    TSharedRef<FJsonObject> UdsState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("struct_path"),UdsPath);J->SetStringField(TEXT("struct_guid"),UdsGuid.ToString(EGuidFormats::Digits));J->SetNumberField(TEXT("status"),int32(UdsType->Status));J->SetNumberField(TEXT("struct_size"),UdsSize);J->SetNumberField(TEXT("struct_alignment"),UdsAlignment);J->SetStringField(TEXT("struct_flags_string"),LexToString(UdsFlags));J->SetStringField(TEXT("package_path"),UdsPackagePath);J->SetStringField(TEXT("package_flags_string"),LexToString(UdsPackageFlags));J->SetBoolField(TEXT("package_dirty"),false);J->SetStringField(TEXT("editor_data_path"),UdsEditorPath);
        for(const TCHAR* Flag:{TEXT("values_sampled"),TEXT("default_buffer_sampled"),TEXT("cdo_creation_requested"),TEXT("runtime_invocation_verified")})J->SetBoolField(Flag,false);
        auto Meta=[](const TMap<FName,FString>& M){auto O=MakeShared<FJsonObject>();for(const auto& Pair:M)O->SetStringField(Pair.Key.ToString(),Pair.Value);return O;};J->SetObjectField(TEXT("root_metadata"),Meta(UdsMetadata));J->SetObjectField(TEXT("editor_data_metadata"),Meta(UdsEditorMetadata));
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& FieldRole:UdsFields){auto F=MakeShared<FJsonObject>();F->SetStringField(TEXT("internal_name"),FieldRole.InternalName.ToString());F->SetStringField(TEXT("authored_name"),FieldRole.AuthoredName);F->SetStringField(TEXT("field_guid"),FieldRole.Guid.ToString(EGuidFormats::Digits));F->SetStringField(TEXT("class"),FieldRole.FieldClass->GetName());F->SetStringField(TEXT("owner_path"),UdsPath);F->SetStringField(TEXT("flags"),LexToString(FieldRole.Flags));F->SetNumberField(TEXT("offset"),FieldRole.Offset);F->SetNumberField(TEXT("size"),FieldRole.Size);F->SetNumberField(TEXT("array_dim"),1);F->SetStringField(TEXT("category"),FieldRole.Category.ToString());F->SetStringField(TEXT("subcategory"),FieldRole.SubCategory.ToString());F->SetObjectField(TEXT("metadata"),Meta(FieldRole.Metadata));Rows.Add(MakeShared<FJsonValueObject>(F));}J->SetArrayField(TEXT("fields"),Rows);
        auto File=MakeShared<FJsonObject>();File->SetStringField(TEXT("filename"),UdsFilename);File->SetBoolField(TEXT("exists"),true);File->SetNumberField(TEXT("bytes"),UdsFileBytes);File->SetStringField(TEXT("sha1"),UdsSHA1);J->SetObjectField(TEXT("file"),File);return J;
    }
    bool Load(const FString& Requested)
    {
        if(!Fence())return false;
        if(!Function::Pin::SafeName(Requested)||!Requested.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidObjectPath(Requested)||Requested.Contains(TEXT(":"))||Requested.Contains(TEXT("'")))return Bad(TEXT("array_scalar_uds_path_pending"),TEXT("This slice requires an exact full Game object path; loader, aliases, other ScriptStructs and nested/property families remain incomplete."));
        UScriptStruct* Raw=FindObject<UScriptStruct>(nullptr,*Requested);TStrongObjectPtr<UScriptStruct> Returned(Raw);if(!Fence())return false;
        if(!Loaded(Raw)||Raw->GetClass()!=UUserDefinedStruct::StaticClass())return Bad(TEXT("array_scalar_uds_not_loaded"),TEXT("An already-loaded exact UserDefinedStruct is required; use an explicit asset preload or existing MakeArray resolver first. No loader is dispatched here."));
        auto* UserType=static_cast<UUserDefinedStruct*>(Raw);UdsType.Reset(UserType);UdsPackage.Reset(UserType->GetPackage());
        if(!Loaded(UdsPackage.Get())||UserType->GetPathName()!=Requested||UserType->GetOuter()!=UdsPackage.Get()||UserType->Status!=UDSS_UpToDate||!UserType->Guid.IsValid()||!UserType->PrimaryStruct.IsExplicitlyNull()||!UserType->EditorData.IsResolved()||UserType->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||UserType->StructFlags&STRUCT_Native||UdsPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)||UdsPackage->IsDirty()||UserType->GetSuperStruct()!=nullptr)return Bad(TEXT("array_scalar_uds_source_pending"),TEXT("Requires saved-clean compiled primary runtime UDS without inherited fields; no source creation, repair, compilation or saving."));
        UObject* EditorRaw=UserType->EditorData.Get();TStrongObjectPtr<UObject> ReturnedEditor(EditorRaw);if(!Loaded(EditorRaw)||EditorRaw->GetClass()!=UUserDefinedStructEditorData::StaticClass()||EditorRaw->GetOuter()!=UserType)return Bad(TEXT("scalar_uds_length_editor_data"));UdsEditor.Reset(static_cast<UUserDefinedStructEditorData*>(EditorRaw));
        UdsPath=UserType->GetPathName();UdsEditorPath=EditorRaw->GetPathName();UdsPackagePath=UdsPackage->GetName();UdsGuid=UserType->Guid;UdsFlags=uint32(UserType->StructFlags);UdsPackageFlags=UdsPackage->GetPackageFlags();UdsSize=UserType->GetStructureSize();UdsAlignment=UserType->GetMinAlignment();UdsEditorTooltip=UdsEditor->ToolTip;UdsEditorStoredMetadata=UdsEditor->MetaData;
        if(UdsSize<1||UdsSize>65536||UdsAlignment<=0||UdsEditorStoredMetadata.Num()>256)return Bad(TEXT("array_scalar_uds_layout_bound"),TEXT("Compiled scalar UDS layout must fit 1..65536 bytes and bounded metadata."));
        for(UObject* O:{static_cast<UObject*>(UserType),EditorRaw,static_cast<UObject*>(UdsPackage.Get()),static_cast<UObject*>(UserType->GetClass()),static_cast<UObject*>(EditorRaw->GetClass())})if(!Hold(O))return false;
        TSet<FName> SeenNames;TSet<FGuid> SeenGuids;
        for(TFieldIterator<FProperty> It(UserType,EFieldIterationFlags::None);It;++It)
        {
            const auto* Physical=*It;auto* Kind=Physical->GetClass();FName Category,SubCategory;
            if(Kind==FStrProperty::StaticClass())Category=UEdGraphSchema_K2::PC_String;else if(Kind==FIntProperty::StaticClass())Category=UEdGraphSchema_K2::PC_Int;else if(Kind==FInt64Property::StaticClass())Category=UEdGraphSchema_K2::PC_Int64;else if(Kind==FFloatProperty::StaticClass()){Category=UEdGraphSchema_K2::PC_Real;SubCategory=UEdGraphSchema_K2::PC_Float;}else if(Kind==FDoubleProperty::StaticClass()){Category=UEdGraphSchema_K2::PC_Real;SubCategory=UEdGraphSchema_K2::PC_Double;}else if(Kind==FBoolProperty::StaticClass())Category=UEdGraphSchema_K2::PC_Boolean;else if(Kind==FNameProperty::StaticClass())Category=UEdGraphSchema_K2::PC_Name;else return Bad(TEXT("array_scalar_uds_field_pending"),TEXT("Only own FString/int32/int64/float/double/bool/FName scalar fields are admitted; Text, enum, nested/object/container/accessor families remain incomplete."));
            if(UdsFields.Num()>=32||Physical->GetOwnerVariant()!=UserType||Physical->ArrayDim!=1||Physical->HasSetterOrGetter()||!Physical->HasAnyPropertyFlags(CPF_Edit)||Physical->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_EditorOnly|CPF_InstancedReference|CPF_ContainsInstancedReference)||Physical->GetOffset_ForInternal()<0||Physical->GetSize()<=0||int64(Physical->GetOffset_ForInternal())+Physical->GetSize()>UdsSize)return Bad(TEXT("array_scalar_uds_field_pending"),TEXT("Requires 1..32 own editable scalar fields within the current compiled layout; no accessor/instanced/transient fields."));
            const auto* Description=UdsEditor->VariablesDescriptions.FindByPredicate([&](const FStructVariableDescription& D){return D.VarName==Physical->GetFName();});
            if(!Description||!Description->VarGuid.IsValid()||SeenNames.Contains(Description->VarName)||SeenGuids.Contains(Description->VarGuid)||Description->bInvalidMember||Description->ContainerType!=EPinContainerType::None||!Description->SubCategoryObject.IsNull()||Description->Category!=Category||Description->SubCategory!=SubCategory||Description->MetaData.Num()>256)return Bad(TEXT("array_scalar_uds_descriptor_pending"),TEXT("Current stock EditorData must map every direct scalar field to a unique authored GUID and matching scalar descriptor."));
            FUdsLengthField FieldRole;FieldRole.Address=Physical;FieldRole.FieldClass=Kind;FieldRole.InternalName=Physical->GetFName();FieldRole.Category=Category;FieldRole.SubCategory=SubCategory;FieldRole.Guid=Description->VarGuid;FieldRole.FriendlyName=Description->FriendlyName;FieldRole.Tooltip=Description->ToolTip;FieldRole.SubtypePath=Description->SubCategoryObject.ToSoftObjectPath().ToString();FieldRole.Terminal=Description->PinValueType;FieldRole.DescriptorBits=UdsDescriptionBits(*Description);FieldRole.DescriptorMetadata=Description->MetaData;FieldRole.Flags=uint64(Physical->GetPropertyFlags());FieldRole.Offset=Physical->GetOffset_ForInternal();FieldRole.Size=Physical->GetSize();if(const auto* M=Physical->GetMetaDataMap()){if(M->Num()>256)return Bad(TEXT("array_scalar_uds_metadata_bound"),TEXT("Field metadata exceeds bounded observation."));FieldRole.Metadata=*M;}SeenNames.Add(FieldRole.InternalName);SeenGuids.Add(FieldRole.Guid);UdsFields.Add(MoveTemp(FieldRole));
        }
        if(UdsFields.IsEmpty()||UdsFields.Num()!=UdsEditor->VariablesDescriptions.Num())return Bad(TEXT("array_scalar_uds_descriptor_pending"),TEXT("The entire current compiled scalar field and authored descriptor inventories must match."));
        UdsReady=true;if(!Fence()||!UdsMetadataQuery(true))return false;UdsMetadataReady=true;if(!UdsAuthoredQuery(true)||!UdsFileQuery(true))return false;return Fence();
    }
 bool Query(){TGuardValue<bool> Observing(CurrentObservation,true);return Fence()&&UdsMetadataReady&&UdsMetadataQuery(false)&&UdsAuthoredQuery(false)&&Fence();}
};
bool FLocalScalarUdsSource::Load(const FString& Requested,TFunction<bool()> Current,TFunction<bool(const TCHAR*,const TCHAR*)> Failure)
{if(State.IsValid())return false;State=MakeShared<FState>();State->TargetCurrent=MoveTemp(Current);State->Failure=MoveTemp(Failure);return State->Load(Requested);}
bool FLocalScalarUdsSource::PlainCurrent(){return !State.IsValid()||State->UdsPlainCurrent();}
bool FLocalScalarUdsSource::Query(){return State.IsValid()&&State->Query();}
bool FLocalScalarUdsSource::OutcomeUnknown()const{return State.IsValid()&&State->Unknown;}
bool FLocalScalarUdsSource::FinalFile(){return State.IsValid()&&State->Query()&&State->UdsFileQuery(false)&&State->Fence();}
UScriptStruct* FLocalScalarUdsSource::Type()const{return State.IsValid()?State->UdsType.Get():nullptr;}
UPackage* FLocalScalarUdsSource::Package()const{return State.IsValid()?State->UdsPackage.Get():nullptr;}
TSharedRef<FJsonObject> FLocalScalarUdsSource::Snapshot()const{return State->UdsState();}

namespace
{
constexpr int32 MaxVariables = 4096;
bool Fail(FResult& R, const TCHAR* Code, const FString& Message)
{ R.ErrorCode = Code; R.Error = Message; return false; }
bool SafeText(const FString& S, int32 Limit, bool bEmpty = true)
{
    if ((!bEmpty && S.IsEmpty()) || S.Len() > Limit) { return false; }
    for (int32 I = 0; I < S.Len(); ++I)
    {
        const uint32 C = S[I];
        if (C == 0 || (C >= 0xD800 && C <= 0xDBFF && (I + 1 == S.Len() || S[I + 1] < 0xDC00 || S[I + 1] > 0xDFFF))) { return false; }
        if (C >= 0xD800 && C <= 0xDBFF) { ++I; }
        else if (C >= 0xDC00 && C <= 0xDFFF) { return false; }
    }
    return true;
}
bool SafeName(const FString& S, bool bEmpty = false)
{
    if (!SafeText(S, NAME_SIZE - 1, bEmpty)) { return false; }
    for (TCHAR C : S) { if (C < 32 || C == 127) { return false; } }
    return true;
}
bool Identifier(const FString& S)
{
    if (S.IsEmpty() || S.Len() > 100 || S.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { return false; }
    for (int32 I = 0; I < S.Len(); ++I)
    { const TCHAR C = S[I]; if (!(C == '_' || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (I > 0 && C >= '0' && C <= '9'))) { return false; } }
    return true;
}
bool Bounded(const TSharedPtr<FJsonValue>& V, int32 Depth, int32& Count)
{
    if (!V || Depth > 8 || ++Count > 1024) { return false; }
    if (V->Type == EJson::Number) { return FMath::IsFinite(V->AsNumber()) && FMath::Abs(V->AsNumber()) <= 9007199254740991.0; }
    if (V->Type == EJson::String) { return SafeText(V->AsString(), 65536); }
    if (V->Type == EJson::Array) { for (const auto& X : V->AsArray()) { if (!Bounded(X, Depth + 1, Count)) { return false; } } }
    if (V->Type == EJson::Object) { for (const auto& X : V->AsObject()->Values) { if (!SafeName(FString(X.Key.ToView())) || !Bounded(X.Value, Depth + 1, Count)) { return false; } } }
    return true;
}
bool Parse(const FString& Text, TSharedPtr<FJsonValue>& Out, FResult& R)
{
    if (Text.IsEmpty() || Text.Len() > 65536) { return Fail(R, TEXT("variable_json_invalid"), TEXT("JSON must contain 1..65536 UTF-16 characters.")); }
    const FString Wrapped = TEXT("[") + Text + TEXT("]");
    auto Reader = TJsonReaderFactory<>::Create(Wrapped);
    TArray<TSet<FString>> Keys; TArray<bool> Objects; EJsonNotation Token;
    while (Reader->ReadNext(Token))
    {
        if (Token == EJsonNotation::Error) { return Fail(R, TEXT("variable_json_invalid"), TEXT("Malformed JSON.")); }
        if (Token != EJsonNotation::ObjectEnd && Token != EJsonNotation::ArrayEnd && !Objects.IsEmpty() && Objects.Last())
        { const FString& Key = Reader->GetIdentifier(); if (Keys.Last().Contains(Key)) { return Fail(R, TEXT("variable_json_invalid"), TEXT("Duplicate JSON object key.")); } Keys.Last().Add(Key); }
        if (Token == EJsonNotation::ObjectStart || Token == EJsonNotation::ArrayStart)
        { Objects.Add(Token == EJsonNotation::ObjectStart); Keys.AddDefaulted(); if (Objects.Num() > 10) { return Fail(R, TEXT("variable_json_invalid"), TEXT("JSON nesting limit exceeded.")); } }
        else if (Token == EJsonNotation::ObjectEnd || Token == EJsonNotation::ArrayEnd) { Objects.Pop(); Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values; int32 Count = 0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Wrapped), Values) || Values.Num() != 1 || !Bounded(Values[0], 0, Count))
    { return Fail(R, TEXT("variable_json_invalid"), TEXT("Provide one bounded JSON value; depth <=8, values <=1024, finite exact-range JSON numbers.")); }
    Out = Values[0]; return true;
}
TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> J; for (const FString& V : Values) { J.Add(MakeShared<FJsonValueString>(V)); } return J; }

bool RuntimeReference(UObject* Object, FResult& R)
{
    if (!IsValid(Object) || Object->IsEditorOnly() || Object->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
    { return Fail(R, TEXT("variable_reference_unsupported"), TEXT("Reference is missing or belongs to an editor/developer/uncooked object.")); }
    const FString Package = Object->GetOutermost()->GetName();
    if (Package == TEXT("/Script/Niagara") && Object->GetPathName() == TEXT("/Script/Niagara.NiagaraComponent"))
    {
        const UClass* Class = Cast<UClass>(Object);
        return (Class && RuntimeObject::IsSupportedClass(Class))
            || Fail(R, TEXT("variable_reference_unsupported"), TEXT("The exact NiagaraComponent native type failed the existing runtime-class guard."));
    }
    const TSet<FString> Scripts = {TEXT("/Script/CoreUObject"), TEXT("/Script/Engine"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
    if (Package.StartsWith(TEXT("/Script/")))
    { return Scripts.Contains(Package) || Fail(R, TEXT("variable_reference_unsupported"), TEXT("Native reference module is outside the audited runtime modules: ") + Package); }
    if ((!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/"))) || Object->HasAnyFlags(RF_Transient))
    { return Fail(R, TEXT("variable_reference_unsupported"), TEXT("Asset references require persistent /Game or /Engine assets.")); }
    auto Audit = ReadPackageDependencies({Package}, true, 4096);
    R.Data->SetObjectField(TEXT("last_reference_dependency_audit"), Audit);
    bool Passed = false;
    if (!Audit->TryGetBoolField(TEXT("passed"), Passed) || !Passed)
    { return Fail(R, TEXT("variable_reference_unverified"), TEXT("Referenced asset must have a complete clean saved dependency graph.")); }
    for (const auto& Edge : Audit->GetArrayField(TEXT("edges")))
    {
        const FString To = Edge->AsObject()->GetStringField(TEXT("to"));
        if (!To.StartsWith(TEXT("/Game/")) && !To.StartsWith(TEXT("/Engine/")) && !Scripts.Contains(To))
        { return Fail(R, TEXT("variable_reference_unsupported"), TEXT("Reference dependency uses an unaudited module or plugin: ") + To); }
    }
    return true;
}
bool PinType(const FString& Type, const FString& ObjectPath, bool bArray, FEdGraphPinType& Out, FResult& R)
{
    FString T = Type.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
    if (T == TEXT("bool") || T == TEXT("boolean")) { Out.PinCategory = UEdGraphSchema_K2::PC_Boolean; }
    else if (T == TEXT("int") || T == TEXT("integer")) { Out.PinCategory = UEdGraphSchema_K2::PC_Int; }
    else if (T == TEXT("int64") || T == TEXT("integer64")) { Out.PinCategory = UEdGraphSchema_K2::PC_Int64; }
    else if (T == TEXT("float") || T == TEXT("single") || T == TEXT("double") || T == TEXT("real"))
    { Out.PinCategory = UEdGraphSchema_K2::PC_Real; Out.PinSubCategory = (T == TEXT("float") || T == TEXT("single")) ? UEdGraphSchema_K2::PC_Float : UEdGraphSchema_K2::PC_Double; }
    else if (T == TEXT("string")) { Out.PinCategory = UEdGraphSchema_K2::PC_String; }
    else if (T == TEXT("name")) { Out.PinCategory = UEdGraphSchema_K2::PC_Name; }
    else if (T == TEXT("text")) { Out.PinCategory = UEdGraphSchema_K2::PC_Text; }
    else if (T == TEXT("vector") || T == TEXT("rotator") || T == TEXT("transform"))
    { Out.PinCategory = UEdGraphSchema_K2::PC_Struct; Out.PinSubCategoryObject = T == TEXT("vector") ? TBaseStructure<FVector>::Get() : T == TEXT("rotator") ? TBaseStructure<FRotator>::Get() : TBaseStructure<FTransform>::Get(); }
    else if (T == TEXT("object") || T == TEXT("class") || T == TEXT("struct") || T == TEXT("scriptstruct") || T == TEXT("enum") || T == TEXT("byteenum"))
    {
        if (!SafeName(ObjectPath, T == TEXT("object") || T == TEXT("class"))) { return Fail(R, TEXT("variable_type_invalid"), TEXT("This variable type requires an exact bounded type_object path.")); }
        if (T == TEXT("object"))
        {
            UClass* Class = RuntimeObject::FindSupportedClass(ObjectPath.IsEmpty() ? UObject::StaticClass()->GetPathName() : ObjectPath);
            if (!Class) { return Fail(R, TEXT("variable_object_type_pending"), TEXT("Object declarations currently admit exact runtime UObject/Actor/Controller/PlayerController/ActorComponent native classes; other object types remain pending.")); }
            Out.PinCategory = UEdGraphSchema_K2::PC_Object; Out.PinSubCategoryObject = Class;
            Out.ContainerType = bArray ? EPinContainerType::Array : EPinContainerType::None; return true;
        }
        UObject* O = ObjectPath.IsEmpty() ? UObject::StaticClass() : LoadObject<UObject>(nullptr, *ObjectPath);
        if (!RuntimeReference(O, R)) { return false; }
        if (T == TEXT("object") || T == TEXT("class"))
        {
            UClass* C = Cast<UClass>(O);
            if (!C) { return Fail(R, TEXT("variable_type_invalid"), TEXT("Object/class type_object must resolve to UClass.")); }
            for (UClass* Parent = C; Parent; Parent = Parent->GetSuperClass()) { if (!RuntimeReference(Parent, R)) { return false; } }
            Out.PinCategory = T == TEXT("object") ? UEdGraphSchema_K2::PC_Object : UEdGraphSchema_K2::PC_Class; Out.PinSubCategoryObject = C;
        }
        else if (T == TEXT("struct") || T == TEXT("scriptstruct"))
        { if (!Cast<UScriptStruct>(O)) { return Fail(R, TEXT("variable_type_invalid"), TEXT("Struct type_object must resolve to UScriptStruct.")); } Out.PinCategory = UEdGraphSchema_K2::PC_Struct; Out.PinSubCategoryObject = O; }
        else
        { if (!Cast<UEnum>(O)) { return Fail(R, TEXT("variable_type_invalid"), TEXT("Enum type_object must resolve to UEnum.")); } Out.PinCategory = UEdGraphSchema_K2::PC_Byte; Out.PinSubCategoryObject = O; }
    }
    else { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Unsupported type descriptor; delegate/interface/map/set/soft-reference types are not part of this variable slice.")); }
    Out.ContainerType = bArray ? EPinContainerType::Array : EPinContainerType::None; return true;
}
bool ValidateExistingPin(const FEdGraphPinType& T, FResult& R)
{
    if (T.ContainerType != EPinContainerType::None && T.ContainerType != EPinContainerType::Array)
    { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Map/set member defaults are not supported by this slice.")); }
    const FName C = T.PinCategory;
    if (C == UEdGraphSchema_K2::PC_Struct || C == UEdGraphSchema_K2::PC_Object || C == UEdGraphSchema_K2::PC_Class || C == UEdGraphSchema_K2::PC_Byte)
    { if (!T.PinSubCategoryObject.IsValid()) { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Complex variable type has no resolved runtime type object.")); } return RuntimeReference(T.PinSubCategoryObject.Get(), R); }
    if (C != UEdGraphSchema_K2::PC_Boolean && C != UEdGraphSchema_K2::PC_Int && C != UEdGraphSchema_K2::PC_Int64 && C != UEdGraphSchema_K2::PC_Real
        && C != UEdGraphSchema_K2::PC_String && C != UEdGraphSchema_K2::PC_Name && C != UEdGraphSchema_K2::PC_Text)
    { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Existing member type is outside the supported legacy variable descriptors.")); }
    return true;
}

// A transient reflected property gives the real member-default import/export grammar.
// CreatePropertyOnScope is public/exported and explicitly does not link the result.
struct FTypedDefault
{
    TStrongObjectPtr<UStruct> Scope;
    FProperty* Property = nullptr;
    void* Value = nullptr;
    FString Stored;
    ~FTypedDefault() { if (Value) { Property->DestroyValue(Value); FMemory::Free(Value); } }
    bool Create(const FEdGraphPinType& Type, UBlueprint* BP, FResult& R)
    {
        Scope.Reset(NewObject<UStruct>(GetTransientPackage(), NAME_None, RF_Transient));
        FCompilerResultsLog Log;
        Property = FKismetCompilerUtilities::CreatePropertyOnScope(Scope.Get(), TEXT("Value"), Type, BP->SkeletonGeneratedClass,
            CPF_None, GetDefault<UEdGraphSchema_K2>(), Log);
        if (!Property || Log.NumErrors) { return Fail(R, TEXT("variable_type_unsupported"), TEXT("Public Kismet property creation rejected the variable pin type.")); }
        Scope->AddCppProperty(Property); Scope->Bind(); Scope->StaticLink(true);
        Value = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment()); Property->InitializeValue(Value); return true;
    }
};
bool EnumNumber(UEnum* Enum, const TSharedPtr<FJsonValue>& V, int64& Number)
{
    if (!IsValid(Enum)) { return false; }
    for (int32 I = 0; I < Enum->NumEnums(); ++I)
    {
        if (Enum->GetNameStringByIndex(I).EndsWith(TEXT("_MAX"))) { continue; }
        if ((V->Type == EJson::String && (V->AsString() == Enum->GetNameStringByIndex(I) || V->AsString() == Enum->GetNameByIndex(I).ToString()))
            || (V->Type == EJson::Number && V->AsNumber() == static_cast<double>(Enum->GetValueByIndex(I))))
        { Number = Enum->GetValueByIndex(I); return true; }
    }
    return false;
}
bool Numeric(FNumericProperty* P, double N, void* Address)
{
    if (!FMath::IsFinite(N)) { return false; }
    if (P->IsInteger())
    {
        const bool Unsigned = CastField<FByteProperty>(P) || CastField<FUInt16Property>(P) || CastField<FUInt32Property>(P) || CastField<FUInt64Property>(P);
        const int32 Bits = P->GetElementSize() * 8;
        if (FMath::TruncToDouble(N) != N || FMath::Abs(N) > 9007199254740991.0 || N < (Unsigned ? 0.0 : -FMath::Pow(2., Bits - 1)) || N >= FMath::Pow(2., Unsigned ? Bits : Bits - 1)) { return false; }
        P->SetIntPropertyValue(Address, static_cast<uint64>(static_cast<int64>(N))); return true;
    }
    if (CastField<FFloatProperty>(P) && FMath::Abs(N) > MAX_flt) { return false; }
    P->SetFloatingPointPropertyValue(Address, N); return true;
}
bool Triple(const TSharedPtr<FJsonValue>& V, const TCHAR* const* Keys, double* Out)
{
    if (V->Type == EJson::Array)
    { if (V->AsArray().Num() != 3) { return false; } for (int32 I = 0; I < 3; ++I) { if (V->AsArray()[I]->Type != EJson::Number) { return false; } Out[I] = V->AsArray()[I]->AsNumber(); } }
    else if (V->Type == EJson::Object)
    {
        if (V->AsObject()->Values.Num() != 3) { return false; }
        for (int32 I = 0; I < 3; ++I)
        { const TSharedPtr<FJsonValue> X = V->AsObject()->TryGetField(Keys[I]); if (!X || X->Type != EJson::Number) { return false; } Out[I] = X->AsNumber(); }
    }
    else { return false; }
    for (int32 I = 0; I < 3; ++I) { if (!FMath::IsFinite(Out[I]) || FMath::Abs(Out[I]) > 1.e12) { return false; } } return true;
}
bool Fill(FProperty* P, void* Address, const TSharedPtr<FJsonValue>& V, FResult& R, int32 Depth = 0)
{
    if (!P || !V || Depth > 8 || P->ArrayDim != 1) { return Fail(R, TEXT("variable_default_unsupported"), TEXT("Unsupported property shape or nesting.")); }
    bool Ok = false;
    if (FArrayProperty* A = CastField<FArrayProperty>(P))
    {
        if (V->Type != EJson::Array || V->AsArray().Num() > 256) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Array defaults require a JSON array of at most 256 elements.")); }
        FScriptArrayHelper H(A, Address); H.Resize(V->AsArray().Num());
        for (int32 I = 0; I < H.Num(); ++I) { if (!Fill(A->Inner, H.GetRawPtr(I), V->AsArray()[I], R, Depth + 1)) { return false; } } return true;
    }
    if (FBoolProperty* BooleanProperty = CastField<FBoolProperty>(P)) { Ok = V->Type == EJson::Boolean; if (Ok) { BooleanProperty->SetPropertyValue(Address, V->AsBool()); } }
    else if (FEnumProperty* E = CastField<FEnumProperty>(P))
    { int64 Number = 0; Ok = E->GetUnderlyingProperty() && EnumNumber(E->GetEnum(), V, Number); if (Ok) { E->GetUnderlyingProperty()->SetIntPropertyValue(Address, Number); } }
    else if (FByteProperty* B = CastField<FByteProperty>(P); B && B->Enum)
    { int64 Number = 0; Ok = EnumNumber(B->Enum, V, Number) && Number >= 0 && Number <= 255; if (Ok) { B->SetPropertyValue(Address, static_cast<uint8>(Number)); } }
    else if (FNumericProperty* NumberProperty = CastField<FNumericProperty>(P)) { Ok = V->Type == EJson::Number && Numeric(NumberProperty, V->AsNumber(), Address); }
    else if (FStrProperty* StringProperty = CastField<FStrProperty>(P)) { Ok = V->Type == EJson::String; if (Ok) { StringProperty->SetPropertyValue(Address, V->AsString()); } }
    else if (FNameProperty* NameProperty = CastField<FNameProperty>(P)) { Ok = V->Type == EJson::String && SafeName(V->AsString(), true); if (Ok) { NameProperty->SetPropertyValue(Address, FName(*V->AsString())); } }
    else if (FTextProperty* T = CastField<FTextProperty>(P)) { Ok = V->Type == EJson::String; if (Ok) { T->SetPropertyValue(Address, FText::FromString(V->AsString())); } }
    else if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(P))
    {
        if (V->Type == EJson::Null) { O->SetObjectPropertyValue(Address, nullptr); return true; }
        if (V->Type != EJson::String || !SafeName(V->AsString()) || !V->AsString().StartsWith(TEXT("/"))) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Object defaults require an exact asset/class path or JSON null.")); }
        UObject* Object = LoadObject<UObject>(nullptr, *V->AsString());
        if (!RuntimeReference(Object, R)) { return false; }
        Ok = Object->IsA(O->PropertyClass);
        if (FClassProperty* C = CastField<FClassProperty>(P)) { UClass* Class = Cast<UClass>(Object); Ok = Class && Class->IsChildOf(C->MetaClass); }
        if (Ok) { O->SetObjectPropertyValue(Address, Object); }
    }
    else if (FStructProperty* S = CastField<FStructProperty>(P))
    {
        const TCHAR* XYZ[] = {TEXT("x"), TEXT("y"), TEXT("z")}; const TCHAR* PYR[] = {TEXT("pitch"), TEXT("yaw"), TEXT("roll")}; double N[3];
        if (S->Struct == TBaseStructure<FVector>::Get()) { Ok = Triple(V, XYZ, N); if (Ok) { *static_cast<FVector*>(Address) = FVector(N[0], N[1], N[2]); } }
        else if (S->Struct == TBaseStructure<FRotator>::Get()) { Ok = Triple(V, PYR, N); if (Ok) { *static_cast<FRotator*>(Address) = FRotator(N[0], N[1], N[2]); } }
        else if (S->Struct == TBaseStructure<FTransform>::Get())
        {
            if (V->Type != EJson::Object) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Transform default requires an object with location/translation, rotation and scale/scale3d.")); }
            FVector Location = FVector::ZeroVector, Scale = FVector::OneVector; FRotator Rotation = FRotator::ZeroRotator;
            TSet<FString> Groups;
            for (const auto& Pair : V->AsObject()->Values)
            {
                FString Key(Pair.Key.ToView()); const FString Group = Key == TEXT("translation") ? TEXT("location") : Key == TEXT("scale3d") ? TEXT("scale") : Key;
                if (Groups.Contains(Group) || (Group != TEXT("location") && Group != TEXT("rotation") && Group != TEXT("scale")) || !Triple(Pair.Value, Group == TEXT("rotation") ? PYR : XYZ, N))
                { return Fail(R, TEXT("variable_default_invalid"), TEXT("Invalid, unknown or duplicated Transform member.")); }
                Groups.Add(Group); if (Group == TEXT("location")) { Location = FVector(N[0], N[1], N[2]); } else if (Group == TEXT("scale")) { Scale = FVector(N[0], N[1], N[2]); } else { Rotation = FRotator(N[0], N[1], N[2]); }
            }
            *static_cast<FTransform*>(Address) = FTransform(Rotation, Location, Scale); return true;
        }
        else
        {
            if (!S->Struct || !RuntimeReference(S->Struct, R)) { return false; }
            if (V->Type != EJson::Object) { return Fail(R, TEXT("variable_default_unsupported"), TEXT("Other runtime struct defaults require explicit reflected member objects; raw import text is unsupported.")); }
            TSet<FProperty*> Assigned;
            for (const auto& Pair : V->AsObject()->Values)
            {
                FProperty* Member = nullptr; const FString Key(Pair.Key.ToView());
                for (TFieldIterator<FProperty> It(S->Struct); It; ++It) { if (It->GetName().Equals(Key, ESearchCase::IgnoreCase)) { Member = *It; break; } }
                if (!Member || Assigned.Contains(Member)) { return Fail(R, TEXT("variable_default_invalid"), TEXT("Unknown or case-duplicated struct field.")); }
                Assigned.Add(Member); if (!Fill(Member, Member->ContainerPtrToValuePtr<void>(Address), Pair.Value, R, Depth + 1)) { return false; }
            }
            return true;
        }
    }
    else { return Fail(R, TEXT("variable_default_unsupported"), TEXT("This reflected property is unsupported; no generic raw ImportText or arbitrary object creation is used.")); }
    return Ok || Fail(R, TEXT("variable_default_invalid"), TEXT("JSON value does not fit the requested reflected type or bounds."));
}
bool PrepareDefault(FTypedDefault& Typed, const FString& Json, FResult& R)
{
    TSharedPtr<FJsonValue> Value;
    if (!Parse(Json, Value, R) || !Fill(Typed.Property, Typed.Value, Value, R)) { return false; }
    if (!FBlueprintEditorUtils::PropertyValueToString_Direct(Typed.Property, static_cast<uint8*>(Typed.Value), Typed.Stored, Typed.Scope.Get()))
    { return Fail(R, TEXT("variable_default_unsupported"), TEXT("Member-default export failed.")); }
    void* Check = FMemory::Malloc(Typed.Property->GetSize(), Typed.Property->GetMinAlignment()); Typed.Property->InitializeValue(Check);
    const bool Imported = FBlueprintEditorUtils::PropertyValueFromString_Direct(Typed.Property, Typed.Stored, static_cast<uint8*>(Check), Typed.Scope.Get());
    const bool Same = Imported && Typed.Property->Identical(Typed.Value, Check, PPF_None);
    Typed.Property->DestroyValue(Check); FMemory::Free(Check);
    return Same || Fail(R, TEXT("variable_default_roundtrip_unsupported"), TEXT("UE member-default text cannot preserve this typed value exactly; no Blueprint mutation occurred."));
}
TSharedRef<FJsonObject> PinJson(const FEdGraphPinType& T)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("category"), T.PinCategory.ToString());
    J->SetStringField(TEXT("subcategory"), T.PinSubCategory.ToString()); J->SetBoolField(TEXT("is_array"), T.ContainerType == EPinContainerType::Array);
    J->SetStringField(TEXT("subcategory_object"), T.PinSubCategoryObject.IsValid() ? T.PinSubCategoryObject->GetPathName() : TEXT("")); return J;
}
TSharedPtr<FJsonValue> ReadTyped(FProperty* P, const void* Address, int32 Depth = 0)
{
    if (!P || !Address || Depth > 8 || P->ArrayDim != 1) { return nullptr; }
    if (FArrayProperty* A = CastField<FArrayProperty>(P))
    { FScriptArrayHelper H(A, Address); if (H.Num() > 256) { return nullptr; } TArray<TSharedPtr<FJsonValue>> Values;
      for (int32 I = 0; I < H.Num(); ++I) { auto V = ReadTyped(A->Inner, H.GetRawPtr(I), Depth + 1); if (!V) { return nullptr; } Values.Add(V); } return MakeShared<FJsonValueArray>(Values); }
    if (FBoolProperty* B = CastField<FBoolProperty>(P)) { return MakeShared<FJsonValueBoolean>(B->GetPropertyValue(Address)); }
    if (FEnumProperty* E = CastField<FEnumProperty>(P))
    { if (!E->GetEnum() || !E->GetUnderlyingProperty()) { return nullptr; } return MakeShared<FJsonValueNumber>(static_cast<double>(E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address))); }
    if (FNumericProperty* N = CastField<FNumericProperty>(P))
    { if (N->IsInteger()) { const int64 V = N->GetSignedIntPropertyValue(Address); if (V > 9007199254740991LL || V < -9007199254740991LL) { return MakeShared<FJsonValueString>(LexToString(V)); } return MakeShared<FJsonValueNumber>(static_cast<double>(V)); }
      const double V = N->GetFloatingPointPropertyValue(Address); if (!FMath::IsFinite(V)) { return nullptr; } return MakeShared<FJsonValueNumber>(V); }
    if (FStrProperty* S = CastField<FStrProperty>(P)) { const FString V = S->GetPropertyValue(Address); if (V.Len() > 65536) { return nullptr; } return MakeShared<FJsonValueString>(V); }
    if (FNameProperty* N = CastField<FNameProperty>(P)) { return MakeShared<FJsonValueString>(N->GetPropertyValue(Address).ToString()); }
    if (FTextProperty* T = CastField<FTextProperty>(P)) { const FString V = T->GetPropertyValue(Address).ToString(); if (V.Len() > 65536) { return nullptr; } return MakeShared<FJsonValueString>(V); }
    if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(P)) { const UObject* V = O->GetObjectPropertyValue(Address); if (V) { return MakeShared<FJsonValueString>(V->GetPathName()); } return MakeShared<FJsonValueNull>(); }
    if (FStructProperty* S = CastField<FStructProperty>(P))
    {
        if (!IsValid(S->Struct)) { return nullptr; } auto J = MakeShared<FJsonObject>(); int32 Count = 0;
        for (TFieldIterator<FProperty> It(S->Struct); It; ++It)
        { if (++Count > 64) { return nullptr; } auto V = ReadTyped(*It, It->ContainerPtrToValuePtr<void>(Address), Depth + 1); if (!V) { return nullptr; } J->SetField(It->GetName(), V); }
        return MakeShared<FJsonValueObject>(J);
    }
    return nullptr;
}
TSharedRef<FJsonObject> Metadata(const FBPVariableDescription& V)
{
    auto J = MakeShared<FJsonObject>(); for (const auto& E : V.MetaDataArray) { J->SetStringField(E.DataKey.ToString(), E.DataValue); } return J;
}
TSharedRef<FJsonObject> Row(UBlueprint* BP, const FBPVariableDescription& V, const TCHAR* Scope, int32 Index, TFunction<bool()> Fence = {})
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("scope"), Scope); J->SetNumberField(TEXT("index"), Index);
    J->SetStringField(TEXT("name"), V.VarName.ToString()); J->SetStringField(TEXT("guid"), V.VarGuid.ToString(EGuidFormats::Digits));
    J->SetStringField(TEXT("friendly_name"), V.FriendlyName); J->SetStringField(TEXT("category"), V.Category.ToString()); if(Fence&&!Fence())return J;
    J->SetStringField(TEXT("property_flags_string"), LexToString(V.PropertyFlags)); J->SetNumberField(TEXT("property_flags"), static_cast<double>(V.PropertyFlags));
    J->SetBoolField(TEXT("instance_editable"), !!(V.PropertyFlags & CPF_Edit) && !(V.PropertyFlags & CPF_DisableEditOnInstance)); J->SetBoolField(TEXT("blueprint_visible"), !!(V.PropertyFlags & CPF_BlueprintVisible));
    J->SetBoolField(TEXT("expose_on_spawn"), !!(V.PropertyFlags & CPF_ExposeOnSpawn));
    J->SetBoolField(TEXT("is_transient"), !!(V.PropertyFlags & CPF_Transient));
    J->SetObjectField(TEXT("pin_type"), PinJson(V.VarType)); J->SetObjectField(TEXT("metadata"), Metadata(V));
    J->SetStringField(TEXT("stored_default"), V.DefaultValue);
    UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
    FProperty* P = BP->GeneratedClass ? FindFProperty<FProperty>(BP->GeneratedClass, V.VarName) : nullptr;
    TSharedPtr<FJsonValue> Value = CDO && P ? ReadTyped(P, P->ContainerPtrToValuePtr<void>(CDO)) : nullptr;
    if(Fence&&!Fence())return J;
    J->SetBoolField(TEXT("default_value_available"), Value.IsValid()); J->SetBoolField(TEXT("default_value_current_compile"), BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings);
    J->SetStringField(TEXT("default_value_scope"), TEXT("existing_generated_cdo_without_creation; stored_default_is_separate_pending_descriptor_value"));
    J->SetBoolField(TEXT("generated_property_available"), P != nullptr);
    if (P)
    {
        J->SetStringField(TEXT("property_class"), P->GetClass()->GetName());
        J->SetStringField(TEXT("generated_property_flags_string"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        J->SetBoolField(TEXT("generated_is_transient"), P->HasAnyPropertyFlags(CPF_Transient));
    }
    J->SetBoolField(TEXT("generated_property_current_compile"), BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings);
    if (Value) { J->SetField(TEXT("default_value"), Value); }
    return J;
}
TSharedRef<FJsonObject> Capture(UBlueprint* BP,UObject* HeldCurrentCDO=nullptr,bool ProjectCurrentCDO=false,TFunction<bool()> Fence = {})
{
    if(Fence)
    {
        auto J=MakeShared<FJsonObject>();if(!Fence())return J;
        J->SetStringField(TEXT("blueprint_name"),BP->GetName());J->SetStringField(TEXT("asset_path"),BP->GetPathName());
        J->SetBoolField(TEXT("package_dirty"),BP->GetPackage()->IsDirty());J->SetNumberField(TEXT("blueprint_status"),int32(BP->Status.GetValue()));
        J->SetStringField(TEXT("generated_cdo_path"),HeldCurrentCDO?HeldCurrentCDO->GetPathName():FString());
        const auto NewCopy=BP->NewVariables,GeneratedCopy=BP->GeneratedVariables;const auto CategoriesCopy=BP->CategorySorting;
        TArray<TSharedPtr<FJsonValue>> New,Generated,Categories;
        for(int32 I=0;I<NewCopy.Num();++I){auto R=Row(BP,NewCopy[I],TEXT("new"),I,Fence);if(!Fence())return J;New.Add(MakeShared<FJsonValueObject>(R));}
        for(int32 I=0;I<GeneratedCopy.Num();++I){auto R=Row(BP,GeneratedCopy[I],TEXT("generated"),I,Fence);if(!Fence())return J;Generated.Add(MakeShared<FJsonValueObject>(R));}
        for(FName C:CategoriesCopy)Categories.Add(MakeShared<FJsonValueString>(C.ToString()));
        J->SetArrayField(TEXT("new_variables"),New);J->SetArrayField(TEXT("generated_variables"),Generated);J->SetArrayField(TEXT("category_sorting"),Categories);
        J->SetNumberField(TEXT("new_variable_count"),New.Num());J->SetNumberField(TEXT("generated_variable_count"),Generated.Num());J->SetNumberField(TEXT("category_count"),Categories.Num());
        return J;
    }
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("blueprint_name"), BP->GetName()); J->SetStringField(TEXT("asset_path"), BP->GetPathName());
    J->SetBoolField(TEXT("package_dirty"), BP->GetOutermost()->IsDirty()); J->SetNumberField(TEXT("blueprint_status"), static_cast<int32>(BP->Status.GetValue()));
    UObject* CDO = ProjectCurrentCDO?HeldCurrentCDO:(BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr);
    J->SetStringField(TEXT("generated_cdo_path"), CDO ? CDO->GetPathName() : TEXT(""));
    TArray<TSharedPtr<FJsonValue>> New, Generated, Categories;
    for (int32 I = 0; I < BP->NewVariables.Num(); ++I) { New.Add(MakeShared<FJsonValueObject>(Row(BP, BP->NewVariables[I], TEXT("new"), I))); }
    for (int32 I = 0; I < BP->GeneratedVariables.Num(); ++I) { Generated.Add(MakeShared<FJsonValueObject>(Row(BP, BP->GeneratedVariables[I], TEXT("generated"), I))); }
    for (const FName& C : BP->CategorySorting) { Categories.Add(MakeShared<FJsonValueString>(C.ToString())); }
    J->SetArrayField(TEXT("new_variables"), New); J->SetArrayField(TEXT("generated_variables"), Generated); J->SetArrayField(TEXT("category_sorting"), Categories);
    J->SetNumberField(TEXT("new_variable_count"), New.Num()); J->SetNumberField(TEXT("generated_variable_count"), Generated.Num()); J->SetNumberField(TEXT("category_count"), Categories.Num()); return J;
}
void Merge(FJsonObject& To, const FJsonObject& From) { for (const auto& P : From.Values) { To.SetField(FString(P.Key.ToView()), P.Value); } }
FString JsonText(const TSharedRef<FJsonObject>& J) { FString Text; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text)); return Text; }
void ObserveAfter(UBlueprint* BP, const TSharedRef<FJsonObject>& Before, FResult& R)
{
    auto After = Capture(BP); R.Data->SetObjectField(TEXT("after"), After);
    R.Data->SetStringField(TEXT("generated_cdo_path"), After->GetStringField(TEXT("generated_cdo_path")));
    R.Data->SetBoolField(TEXT("dirty_before"), Before->GetBoolField(TEXT("package_dirty"))); R.Data->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty());
    R.Data->SetBoolField(TEXT("package_dirty_before"), Before->GetBoolField(TEXT("package_dirty"))); R.Data->SetBoolField(TEXT("package_dirty_after"), BP->GetOutermost()->IsDirty());
    R.Data->SetBoolField(TEXT("dirty_state_unchanged"), Before->GetBoolField(TEXT("package_dirty")) == BP->GetOutermost()->IsDirty());
    R.bChanged = JsonText(Before) != JsonText(After); R.bChangeStateKnown = true;
    R.Data->SetStringField(TEXT("change_observation_scope"), TEXT("local_variable_descriptors_existing_generated_CDO_dirty_and_status; not arbitrary callback or unloaded asset state"));
}
UBlueprint* LoadBlueprint(const FString& Path, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("variable_editor_state_unsupported"), TEXT("Variable tools require the editor game thread outside PIE/SIE.")); return nullptr; }
    if (!SafeName(Path) || !Path.StartsWith(TEXT("/Game/"))) { Fail(R, TEXT("variable_blueprint_path_invalid"), TEXT("Provide an exact bounded /Game Blueprint asset/package path.")); return nullptr; }
    FString AssetPath = Path; if (!Path.Contains(TEXT("."))) { AssetPath += TEXT(".") + FPackageName::GetShortName(Path); }
    if (!SafeName(AssetPath)) { Fail(R, TEXT("variable_blueprint_path_invalid"), TEXT("The completed Blueprint object path exceeds the safe 1023-character name bound.")); return nullptr; }
    const bool Loaded = FindObject<UBlueprint>(nullptr, *AssetPath) != nullptr;
    R.Data->SetBoolField(TEXT("loaded_during_operation"), !Loaded); R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), !Loaded);
    R.Data->SetStringField(TEXT("load_scope"), TEXT("dirty/status baseline begins after asset load; implicit PostLoad effects are not measured"));
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!IsValid(BP)) { R.bChangeStateKnown = Loaded; Fail(R, TEXT("variable_blueprint_not_found"), TEXT("Blueprint load failed; inspect possible load effects before retry.")); return nullptr; }
    if (BP->NewVariables.Num() + BP->GeneratedVariables.Num() > MaxVariables)
    { Fail(R, TEXT("variable_budget_exceeded"), TEXT("Variable descriptor budget is 4096.")); return nullptr; }
    return BP;
}
bool MutationScope(UBlueprint* BP, FResult& R)
{
    if (BP->BlueprintType != BPTYPE_Normal || !BP->ParentClass || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->Status == BS_BeingCreated)
    { return Fail(R, TEXT("variable_blueprint_state_unsupported"), TEXT("Requires a normal idle Blueprint without suppressed or queued structural modification.")); }
    auto Parent = InspectBlueprintParent(BP->ParentClass->GetPathName()); R.Data->SetObjectField(TEXT("parent_validation"), Parent);
    if (!Parent->GetBoolField(TEXT("success"))) { return Fail(R, TEXT("variable_parent_unsupported"), TEXT("Blueprint runtime parent validation failed.")); }
    for (UClass* C = BP->ParentClass; C; C = C->GetSuperClass())
    { const FString Package = C->GetOutermost()->GetName(); if (C->HasAnyClassFlags(CLASS_Native) && Package != TEXT("/Script/Engine") && Package != TEXT("/Script/CoreUObject")) { return Fail(R, TEXT("variable_parent_unsupported"), TEXT("Native Blueprint parents outside Engine/CoreUObject are unaudited.")); } }
    // AddMemberVariable can rename loaded child members, while compilation can reinstance
    // external objects. Refuse those known effects before dispatching either operation.
    TArray<FString> External;
    for (TObjectIterator<UBlueprint> It; It; ++It)
    { if (*It != BP && It->ParentClass && BP->GeneratedClass && It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.AddUnique(It->GetPathName()); } }
    if (BP->GeneratedClass)
    {
        TArray<UObject*> Instances; GetObjectsOfClass(BP->GeneratedClass, Instances, true, RF_NoFlags);
        for (UObject* O : Instances) { if (IsValid(O) && O->GetOutermost() != BP->GetOutermost()) { External.AddUnique(O->GetPathName()); } }
    }
    External.Sort(); R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"), StringValues(External));
    return External.IsEmpty() || Fail(R, TEXT("variable_external_instances_unsupported"), TEXT("Loaded derived Blueprints or external instances can be affected by structural modification/compilation; inspect them before editing."));
}
FBPVariableDescription* FindLocal(UBlueprint* BP, const FString& Name, int32& Index, FResult& R)
{
    if (!SafeName(Name)) { Fail(R, TEXT("variable_name_invalid"), TEXT("Variable selector must contain 1..1023 safe UTF-16 characters.")); return nullptr; }
    Index = INDEX_NONE;
    for (int32 I = 0; I < BP->NewVariables.Num(); ++I)
    {
        if (!BP->NewVariables[I].VarName.ToString().Equals(Name, ESearchCase::IgnoreCase)) { continue; }
        if (Index != INDEX_NONE) { Fail(R, TEXT("variable_name_ambiguous"), TEXT("Multiple local descriptors match the requested variable.")); return nullptr; } Index = I;
    }
    if (Index == INDEX_NONE) { Fail(R, TEXT("variable_not_found"), TEXT("Default mutation requires a locally declared NewVariables member.")); return nullptr; }
    return &BP->NewVariables[Index];
}
bool CurrentMatches(UBlueprint* BP, const FBPVariableDescription& V, const FTypedDefault& Typed)
{
    if (!V.DefaultValue.IsEmpty()) { return V.DefaultValue == Typed.Stored; }
    UObject* CDO = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
    FProperty* P = BP->GeneratedClass ? FindFProperty<FProperty>(BP->GeneratedClass, V.VarName) : nullptr;
    return CDO && P && P->SameType(Typed.Property) && Typed.Property->Identical(Typed.Value, P->ContainerPtrToValuePtr<void>(CDO), PPF_None);
}
bool FinishRequested(UBlueprint* BP, const FString& Name, const FTypedDefault& Typed, FResult& R)
{
    if (R.bCompileRequested)
    {
        R.Stage = TEXT("compile"); R.bCompileAttempted = true; R.bMutationAttempted = true;
        auto C = CompileBlueprint(BP->GetPathName(), false, false); R.Data->SetObjectField(TEXT("compile"), C);
        bool Compiled = false; C->TryGetBoolField(TEXT("compiled"), Compiled); R.Compiled = Compiled;
        if (!Compiled) { return Fail(R, TEXT("variable_compile_failed"), TEXT("Variable state changed but compilation failed; inspect compiler diagnostics. Save was not attempted.")); }
    }
    R.Stage = TEXT("verification"); int32 Index = INDEX_NONE; FBPVariableDescription* V = FindLocal(BP, Name, Index, R);
    if (!V || !CurrentMatches(BP, *V, Typed)) { return Fail(R, TEXT("variable_default_postcondition_failed"), TEXT("Stored descriptor or generated CDO did not preserve the requested typed default.")); }
    if (R.bSaveRequested)
    {
        R.Stage = TEXT("save"); R.bSaveAttempted = true; R.bMutationAttempted = true; R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(BP, false);
        if (!R.bSaved) { return Fail(R, TEXT("variable_save_failed"), TEXT("Default readback passed but explicit save failed; inspect the dirty package before retry.")); }
    }
    return true;
}
struct FMetadataSuppressionScope
{
    UBlueprint* Blueprint;
    bool Previous;
    explicit FMetadataSuppressionScope(UBlueprint* BP) : Blueprint(BP), Previous(BP->bSuppressStructurallyModified) { Blueprint->bSuppressStructurallyModified = true; }
    ~FMetadataSuppressionScope() { Blueprint->bSuppressStructurallyModified = Previous; }
};
// KZ: feature-local scalar material/texture declarations. Generic runtime-object policy is unchanged.
TObjectPtr<UObject> MaterialVariableCDOHandle(const UClass* Class)
{
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const TObjectPtr<UObject> Handle=Class->ClassDefaultObject;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return Handle;
}
UClass* MaterialVariableClass(const FString& Path)
{
    if(Path==TEXT("/Script/Engine.MaterialInterface"))return UMaterialInterface::StaticClass();
    if(Path==TEXT("/Script/Engine.MaterialInstanceDynamic"))return UMaterialInstanceDynamic::StaticClass();
    if(Path==TEXT("/Script/Engine.Texture"))return UTexture::StaticClass();
    if(Path==TEXT("/Script/Engine.TextureRenderTarget2D"))return UTextureRenderTarget2D::StaticClass();
    if(Path==TEXT("/Script/Niagara.NiagaraSystem"))return UNiagaraSystem::StaticClass();
    return nullptr;
}
bool MaterialVariableRequest(const FString& Type,const FString& Path)
{
    const FString T=Type.ToLower().Replace(TEXT("_"),TEXT("")).Replace(TEXT("-"),TEXT("")).Replace(TEXT(" "),TEXT(""));
    return T==TEXT("object")&&(Path==TEXT("/Script/Engine.MaterialInterface")||Path==TEXT("/Script/Engine.MaterialInstanceDynamic")||Path==TEXT("/Script/Engine.Texture")||Path==TEXT("/Script/Niagara.NiagaraSystem")||Path==TEXT("/Script/Engine.TextureRenderTarget2D"));
}
// LP: this type is admitted only by the transient empty-array Add route below.
// The scalar/global RuntimeObject and generic PinType contracts are unchanged.
bool SkeletalArrayVariableRequest(const FString& Type,const FString& Path)
{
    const FString T=Type.ToLower().Replace(TEXT("_"),TEXT("")).Replace(TEXT("-"),TEXT("")).Replace(TEXT(" "),TEXT(""));
    return T==TEXT("object")&&Path==TEXT("/Script/Engine.SkeletalMeshComponent");
}
bool MaterialPinSame(const FEdGraphPinType& A,const FEdGraphPinType& B)
{return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
bool MaterialDescriptionSame(const FBPVariableDescription& A,const FBPVariableDescription& B)
{
    if(A.VarName!=B.VarName||A.VarGuid!=B.VarGuid||!MaterialPinSame(A.VarType,B.VarType)||A.FriendlyName!=B.FriendlyName||!A.Category.IdenticalTo(B.Category)
        ||A.PropertyFlags!=B.PropertyFlags||A.RepNotifyFunc!=B.RepNotifyFunc||A.ReplicationCondition!=B.ReplicationCondition||A.DefaultValue!=B.DefaultValue||A.MetaDataArray.Num()!=B.MetaDataArray.Num())return false;
    for(int32 I=0;I<A.MetaDataArray.Num();++I)if(A.MetaDataArray[I].DataKey!=B.MetaDataArray[I].DataKey||A.MetaDataArray[I].DataValue!=B.MetaDataArray[I].DataValue)return false;
    return true;
}
TSharedRef<FJsonObject> MaterialDeclarationRow(const FBPVariableDescription& V,int32 Index)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("scope"),TEXT("new"));J->SetNumberField(TEXT("index"),Index);J->SetStringField(TEXT("name"),V.VarName.ToString());
    J->SetStringField(TEXT("guid"),V.VarGuid.ToString(EGuidFormats::Digits));J->SetStringField(TEXT("friendly_name"),V.FriendlyName);J->SetStringField(TEXT("category"),V.Category.ToString());
    J->SetStringField(TEXT("property_flags_string"),LexToString(V.PropertyFlags));J->SetNumberField(TEXT("property_flags"),static_cast<double>(V.PropertyFlags));
    J->SetBoolField(TEXT("instance_editable"),!!(V.PropertyFlags&CPF_Edit)&&!(V.PropertyFlags&CPF_DisableEditOnInstance));J->SetBoolField(TEXT("blueprint_visible"),!!(V.PropertyFlags&CPF_BlueprintVisible));
    J->SetBoolField(TEXT("expose_on_spawn"),!!(V.PropertyFlags&CPF_ExposeOnSpawn));J->SetBoolField(TEXT("is_transient"),!!(V.PropertyFlags&CPF_Transient));J->SetObjectField(TEXT("pin_type"),PinJson(V.VarType));
    J->SetObjectField(TEXT("metadata"),Metadata(V));J->SetStringField(TEXT("stored_default"),V.DefaultValue);J->SetBoolField(TEXT("default_value_available"),false);
    J->SetStringField(TEXT("default_value_scope"),TEXT("pending_descriptor_only; selected generated CDO is separately verified after explicit compile; no unrelated runtime values sampled"));return J;
}
struct FMaterialVariableScope
{
    FResult& R;FName Name;TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UPackage> Package,NativePackage;TStrongObjectPtr<UEditorEngine> Editor;
    TStrongObjectPtr<UClass> Native,Parent,Skeleton,Generated;TStrongObjectPtr<const UObject> NativeCDO;
    TArray<TStrongObjectPtr<UClass>> RetiredClasses;TArray<FBPVariableDescription> Original,Expected,GeneratedDescriptions;TArray<FName> Categories;
    FString Path,PackagePath,NativePath,CDOPath;EObjectFlags BPFlags=RF_NoFlags,PackageFlags=RF_NoFlags,NativeFlags=RF_NoFlags,CDOFlags=RF_NoFlags;
    uint32 PackageBits=0,NativeBits=0;int32 NativeSize=0;decltype(GUndo) Undo=nullptr;EBlueprintStatus Status=BS_Unknown;bool Dirty=false,Stopped=false,NiagaraSystem=false,TextureRenderTarget2D=false,SkeletalArray=false,MapStringVector=false,MapStringDouble=false;
    TStrongObjectPtr<UScriptStruct> MapVector;EObjectFlags MapVectorFlags=RF_NoFlags,MapPackageObjectFlags=RF_NoFlags;uint32 MapStructFlags=0;int32 MapAlignment=0;
    struct FMapAxis{const FProperty* Field;FName Name;EPropertyFlags Flags;int32 Offset,Size;};TArray<FMapAxis> MapAxes;
    bool MapSourceCurrent()const
    {
        if(MapStringDouble)return Loaded(NativePackage.Get())&&NativePackage->GetName().Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)&&NativePackage->GetPackageFlags()==16U&&NativePackage->GetFlags()==MapPackageObjectFlags&&!NativePackage->IsDirty()&&!MapVector.IsValid()&&MapAxes.IsEmpty()&&MapAlignment==alignof(double);
        if(!Loaded(MapVector.Get())||!Loaded(NativePackage.Get())||MapVector->GetClass()!=UScriptStruct::StaticClass()||MapVector->GetOuter()!=NativePackage.Get()||MapVector->GetPackage()!=NativePackage.Get()
            ||MapVector->GetPathName()!=TEXT("/Script/CoreUObject.Vector")||MapVector->GetFlags()!=MapVectorFlags||uint32(MapVector->StructFlags)!=MapStructFlags||!(MapStructFlags&uint32(STRUCT_Native))
            ||MapVector->GetStructureSize()!=sizeof(FVector)||MapVector->GetMinAlignment()!=MapAlignment||MapAlignment!=alignof(FVector)
            ||NativePackage->GetName()!=TEXT("/Script/CoreUObject")||NativePackage->GetPackageFlags()!=16U||NativePackage->GetFlags()!=MapPackageObjectFlags||NativePackage->IsDirty()||MapAxes.Num()!=3)return false;
        auto Super=MapVector->GetSuperStructIterator();++Super;if(Super)return false;
        int32 Count=0;for(TFieldIterator<FProperty> It(MapVector.Get(),EFieldIterationFlags::None);It;++It)++Count;if(Count!=3)return false;
        for(const auto& F:MapAxes)if(MapVector->FindPropertyByName(F.Name)!=F.Field||F.Field->GetClass()!=FDoubleProperty::StaticClass()||F.Field->GetOwnerStruct()!=MapVector.Get()||F.Field->ArrayDim!=1
            ||F.Field->GetPropertyFlags()!=F.Flags||F.Field->GetOffset_ForInternal()!=F.Offset||F.Field->GetSize()!=F.Size)return false;
        return true;
    }
    TSharedRef<FJsonObject> MapSourceState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("struct_path"),NativePath);J->SetStringField(TEXT("type_path"),NativePath);J->SetStringField(TEXT("type_kind"),MapStringDouble?TEXT("string_double_map"):TEXT("string_vector_map"));
        J->SetStringField(TEXT("package_path"),TEXT("/Script/CoreUObject"));J->SetStringField(TEXT("package_flags_string"),TEXT("16"));J->SetBoolField(TEXT("package_dirty"),false);J->SetStringField(TEXT("type_flags_string"),LexToString(MapStructFlags));J->SetNumberField(TEXT("type_structure_size"),MapStringDouble?sizeof(double):sizeof(FVector));J->SetNumberField(TEXT("type_alignment"),MapAlignment);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& F:MapAxes){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),F.Name.ToString());Row->SetStringField(TEXT("class"),TEXT("DoubleProperty"));Row->SetStringField(TEXT("owner"),NativePath);Row->SetStringField(TEXT("flags"),LexToString(uint64(F.Flags)));Row->SetNumberField(TEXT("offset"),F.Offset);Row->SetNumberField(TEXT("size"),F.Size);Rows.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("native_fields"),Rows);J->SetBoolField(TEXT("runtime_map_values_sampled"),false);return J;
    }
    struct FSkeletalRole{TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;FString Path;EObjectFlags Flags;uint32 ClassFlags;int32 Size;};
    TArray<FSkeletalRole> SkeletalRoles;bool SkeletalSourceReady=false;
    bool SkeletalSourceCurrent()const
    {
        if(!SkeletalArray||!SkeletalSourceReady)return true;
        int32 Index=0;for(auto It=Native->GetSuperStructIterator();It;++It,++Index)
        {
            if(!SkeletalRoles.IsValidIndex(Index)||*It!=SkeletalRoles[Index].Class.Get())return false;
            const auto& V=SkeletalRoles[Index];const UClass* C=V.Class.Get();const UPackage* P=V.Package.Get();
            if(!Loaded(C)||!Loaded(P)||C->GetPackage()!=P||C->GetOuter()!=P||C->GetPathName()!=V.Path||C->GetFlags()!=V.Flags
                ||uint32(C->GetClassFlags())!=V.ClassFlags||C->GetStructureSize()!=V.Size||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr
                ||P->GetName()!=(Index==6?TEXT("/Script/CoreUObject"):TEXT("/Script/Engine"))||P->GetPackageFlags()!=16U||P->IsDirty())return false;
        }
        return Index==7&&SkeletalRoles.Num()==7;
    }
    bool BindSkeletalSource()
    {
        const TCHAR* Paths[]={TEXT("/Script/Engine.SkeletalMeshComponent"),TEXT("/Script/Engine.SkinnedMeshComponent"),TEXT("/Script/Engine.MeshComponent"),TEXT("/Script/Engine.PrimitiveComponent"),TEXT("/Script/Engine.SceneComponent"),TEXT("/Script/Engine.ActorComponent"),TEXT("/Script/CoreUObject.Object")};
        int32 Index=0;for(auto It=Native->GetSuperStructIterator();It;++It,++Index)
        {
            UClass* C=const_cast<UClass*>(Cast<UClass>(*It));UPackage* P=Loaded(C)?C->GetPackage():nullptr;
            if(Index>=UE_ARRAY_COUNT(Paths)||!Loaded(C)||!Loaded(P)||C->GetOuter()!=P||C->GetPathName()!=Paths[Index]
                ||!C->HasAnyClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)
                ||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=nullptr||C->IsEditorOnly()
                ||P->GetName()!=(Index==6?TEXT("/Script/CoreUObject"):TEXT("/Script/Engine"))||P->GetPackageFlags()!=16U||P->IsDirty())return false;
            FSkeletalRole V;V.Class.Reset(C);V.Package.Reset(P);V.Path=Paths[Index];V.Flags=C->GetFlags();V.ClassFlags=uint32(C->GetClassFlags());V.Size=C->GetStructureSize();SkeletalRoles.Add(MoveTemp(V));
        }
        SkeletalSourceReady=Index==UE_ARRAY_COUNT(Paths);return SkeletalSourceReady&&SkeletalSourceCurrent();
    }
    TSharedRef<FJsonObject> SkeletalSourceState()const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("class_path"),NativePath);
        J->SetBoolField(TEXT("existing_cdo_available"),NativeCDO.Get()!=nullptr);J->SetStringField(TEXT("existing_cdo_path"),CDOPath);J->SetStringField(TEXT("existing_cdo_flags_string"),LexToString(uint32(CDOFlags)));
        J->SetBoolField(TEXT("cdo_creation_requested"),false);J->SetBoolField(TEXT("cdo_values_sampled"),false);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:SkeletalRoles){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),V.Path);Row->SetStringField(TEXT("class_flags_string"),LexToString(V.ClassFlags));Row->SetNumberField(TEXT("class_size"),V.Size);Row->SetStringField(TEXT("package_path"),V.Package->GetName());Row->SetStringField(TEXT("package_flags_string"),TEXT("16"));Row->SetBoolField(TEXT("package_dirty"),false);Rows.Add(MakeShared<FJsonValueObject>(Row));}J->SetArrayField(TEXT("hierarchy"),Rows);return J;
    }
    explicit FMaterialVariableScope(FResult& In):R(In){}
    static bool Loaded(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
    static bool Globals(){return IsEditorGameThread()&&GEditor&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
    bool Stop(const TCHAR* Code,const TCHAR* Message)
    {
        Stopped=true;R.bChangeStateKnown=false;R.bSuccess=false;R.bPostconditionPassed=false;R.Data->SetBoolField(TEXT("native_resource_observation_incomplete"),true);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);return Fail(R,Code,Message);
    }
    // Plain earliest-wins identity diagnostics never follow an unapproved object or
    // dispatch another SDK query after the failed condition.
    bool IdentityMismatch(const TCHAR* Role,const FString& ObjectPath,const TCHAR* Property,const FString& ExpectedValue,const FString& ActualValue,bool Observed=true)
    {
        if(!R.Data->HasField(TEXT("native_resource_identity_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object_role"),Role);J->SetStringField(TEXT("object_path"),ObjectPath);
            J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),ExpectedValue);J->SetStringField(TEXT("actual"),ActualValue);J->SetBoolField(TEXT("actual_observed"),Observed);
            J->SetBoolField(TEXT("later_conditions_unobserved_after_failure"),true);J->SetStringField(TEXT("value_scope"),TEXT("captured held-role strings, raw scalar values or the exact evaluated comparison; no failed-object dereference for diagnostics"));
            R.Data->SetObjectField(TEXT("native_resource_identity_first_mismatch"),J);
        }
        return false;
    }
    bool TargetBase(bool SavedBoundary=false,uint32* ObservedPackageBits=nullptr)
    {
        if(!IsEditorGameThread())return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("IsEditorGameThread"),TEXT("true"),TEXT("false"));
        if(!GEditor)return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("GEditor_nonnull"),TEXT("true"),TEXT("false"));
        if(IsPlaySessionActive())return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("IsPlaySessionActive"),TEXT("false"),TEXT("true"));
        if(GIsImportingT3D)return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("GIsImportingT3D"),TEXT("false"),TEXT("true"));
        if(GExitPurge)return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("GExitPurge"),TEXT("false"),TEXT("true"));
        if(IsEngineExitRequested())return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("IsEngineExitRequested"),TEXT("false"),TEXT("true"));
        if(IsGarbageCollecting())return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("IsGarbageCollecting"),TEXT("false"),TEXT("true"));
        if(UE::IsSavingPackage())return IdentityMismatch(TEXT("editor_context"),TEXT(""),TEXT("UE::IsSavingPackage"),TEXT("false"),TEXT("true"));
        if(GEditor!=Editor.Get())return IdentityMismatch(TEXT("editor"),TEXT(""),TEXT("current_pointer"),LexToString(reinterpret_cast<UPTRINT>(Editor.Get())),LexToString(reinterpret_cast<UPTRINT>(GEditor)));
        if(!Loaded(Editor.Get()))return IdentityMismatch(TEXT("editor"),TEXT(""),TEXT("held_loaded"),TEXT("true"),TEXT("false"));
        if(GUndo!=Undo)return IdentityMismatch(TEXT("transaction"),TEXT(""),TEXT("GUndo"),LexToString(reinterpret_cast<UPTRINT>(Undo)),LexToString(reinterpret_cast<UPTRINT>(GUndo)));
        if(!Loaded(BP.Get()))return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("held_loaded"),TEXT("true"),TEXT("false"));
        if(!Loaded(Package.Get()))return IdentityMismatch(TEXT("target_package"),PackagePath,TEXT("held_loaded"),TEXT("true"),TEXT("false"));
        if(!Loaded(Parent.Get()))return IdentityMismatch(TEXT("target_parent"),TEXT(""),TEXT("held_loaded"),TEXT("true"),TEXT("false"));
        const FString ActualPath=BP->GetPathName();if(ActualPath!=Path)return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("path"),Path,ActualPath);
        const UPackage* ActualPackage=BP->GetOutermost();if(ActualPackage!=Package.Get())return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("outermost_pointer"),LexToString(reinterpret_cast<UPTRINT>(Package.Get())),LexToString(reinterpret_cast<UPTRINT>(ActualPackage)));
        const FString ActualPackagePath=Package->GetName();if(ActualPackagePath!=PackagePath)return IdentityMismatch(TEXT("target_package"),PackagePath,TEXT("name"),PackagePath,ActualPackagePath);
        const uint32 ActualBits=Package->GetPackageFlags();if(ObservedPackageBits)*ObservedPackageBits=ActualBits;
        const bool Qualified=SavedBoundary&&R.Stage==TEXT("save")&&R.bSaveAttempted&&R.bSaved;
        const bool ExactRemoval=Qualified&&(PackageBits&uint32(PKG_NewlyCreated))!=0&&ActualBits==(PackageBits&~uint32(PKG_NewlyCreated));
        if(SavedBoundary)
        {
            const auto Receipt=R.Data->GetObjectField(TEXT("native_resource_save_package_flags"));Receipt->SetBoolField(TEXT("actual_observed"),true);
            Receipt->SetStringField(TEXT("actual_after_bits_string"),LexToString(ActualBits));Receipt->SetStringField(TEXT("xor_bits_string"),LexToString(ActualBits^PackageBits));
            Receipt->SetBoolField(TEXT("unchanged"),ActualBits==PackageBits);Receipt->SetBoolField(TEXT("exact_newly_created_removal"),ExactRemoval);
        }
        if(ActualBits!=PackageBits&&!ExactRemoval)return IdentityMismatch(TEXT("target_package"),PackagePath,TEXT("package_flags_bits"),LexToString(PackageBits),LexToString(ActualBits));
        if(!BP->ParentClass.GetGCPtr().IsResolved())return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("parent_handle_resolved"),TEXT("true"),TEXT("false"));
        if(!((NiagaraSystem||TextureRenderTarget2D||SkeletalArray||MapStringVector)?BP->ParentClass.GetGCPtr()==Parent.Get():BP->ParentClass==Parent.Get()))return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("parent_equals_held_comparison"),TEXT("true"),TEXT("false"));
        const auto ActualType=BP->BlueprintType.GetValue();if(ActualType!=BPTYPE_Normal)return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("blueprint_type"),LexToString(int32(BPTYPE_Normal)),LexToString(int32(ActualType)));
        if(BP->bBeingCompiled)return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("bBeingCompiled"),TEXT("false"),TEXT("true"));
        if(BP->bQueuedForCompilation)return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("bQueuedForCompilation"),TEXT("false"),TEXT("true"));
        if(BP->bIsRegeneratingOnLoad)return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("bIsRegeneratingOnLoad"),TEXT("false"),TEXT("true"));
        if(BP->bSuppressStructurallyModified)return IdentityMismatch(TEXT("target_blueprint"),Path,TEXT("bSuppressStructurallyModified"),TEXT("false"),TEXT("true"));
        return true;
    }
    bool SourceBase()
    {
        if(!Loaded(Native.Get()))return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("held_loaded"),TEXT("true"),TEXT("false"));
        if(!Loaded(NativePackage.Get()))return IdentityMismatch(TEXT("native_package"),TEXT(""),TEXT("held_loaded"),TEXT("true"),TEXT("false"));
        const FString ActualPath=Native->GetPathName();if(ActualPath!=NativePath)return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("path"),NativePath,ActualPath);
        const UPackage* ActualPackage=Native->GetOutermost();if(ActualPackage!=NativePackage.Get())return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("outermost_pointer"),LexToString(reinterpret_cast<UPTRINT>(NativePackage.Get())),LexToString(reinterpret_cast<UPTRINT>(ActualPackage)));
        const FString ExpectedPackage=NiagaraSystem?TEXT("/Script/Niagara"):TEXT("/Script/Engine"),ActualPackageName=NativePackage->GetName();
        if(ActualPackageName!=ExpectedPackage)return IdentityMismatch(TEXT("native_package"),ExpectedPackage,TEXT("name"),ExpectedPackage,ActualPackageName);
        const uint32 ActualPackageBits=NativePackage->GetPackageFlags();if(ActualPackageBits!=16U)return IdentityMismatch(TEXT("native_package"),ExpectedPackage,TEXT("package_flags_bits"),TEXT("16"),LexToString(ActualPackageBits));
        if(NativePackage->IsDirty())return IdentityMismatch(TEXT("native_package"),ExpectedPackage,TEXT("dirty"),TEXT("false"),TEXT("true"));
        const EObjectFlags ActualFlags=Native->GetFlags();if(ActualFlags!=NativeFlags)return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("object_flags"),LexToString(uint32(NativeFlags)),LexToString(uint32(ActualFlags)));
        const uint32 ActualClassFlags=uint32(Native->GetClassFlags());if(ActualClassFlags!=NativeBits)return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("class_flags"),LexToString(NativeBits),LexToString(ActualClassFlags));
        const int32 ActualSize=Native->GetStructureSize();if(ActualSize!=NativeSize)return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("structure_size"),LexToString(NativeSize),LexToString(ActualSize));
        if(!Native->ClassGeneratedBy.IsResolved())return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("generated_by_handle_resolved"),TEXT("true"),TEXT("false"));
        if(Native->ClassGeneratedBy!=nullptr)return IdentityMismatch(TEXT("native_class"),NativePath,TEXT("generated_by_is_null_comparison"),TEXT("true"),TEXT("false"));
        if(SkeletalArray&&!SkeletalSourceCurrent())return IdentityMismatch(TEXT("skeletal_native_hierarchy"),NativePath,TEXT("captured_native_hierarchy_current"),TEXT("true"),TEXT("false"));
        return true;
    }
    bool Source(bool SavedBoundary=false)
    {
        if(Stopped)return false;
        if(MapStringVector){if(!TargetBase()||!MapSourceCurrent())return Stop(TEXT("variable_map_source_changed"),TEXT("Current own target or native FVector layout changed; no next SDK, Map value/default access or retry."));return true;}
        uint32 ObservedBits=PackageBits;
        if(SavedBoundary)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetBoolField(TEXT("saved_boundary_requested"),SavedBoundary);J->SetBoolField(TEXT("save_attempted"),R.bSaveAttempted);J->SetBoolField(TEXT("save_returned_true"),R.bSaved);
            J->SetBoolField(TEXT("qualified_successful_save"),R.Stage==TEXT("save")&&R.bSaveAttempted&&R.bSaved&&SavedBoundary);J->SetStringField(TEXT("expected_before_bits_string"),LexToString(PackageBits));J->SetStringField(TEXT("newly_created_bit_string"),LexToString(uint32(PKG_NewlyCreated)));
            J->SetBoolField(TEXT("expected_had_newly_created"),(PackageBits&uint32(PKG_NewlyCreated))!=0);J->SetBoolField(TEXT("actual_observed"),false);J->SetBoolField(TEXT("source_guard_passed"),false);J->SetBoolField(TEXT("admitted"),false);
            R.Data->SetObjectField(TEXT("native_resource_save_package_flags"),J);
        }
        if(!TargetBase(SavedBoundary,&ObservedBits)||!SourceBase())return Stop(TEXT("variable_material_scope_changed"),TEXT("Current target/native resource class roles changed; no next SDK, rollback or retry."));
        const auto Now=MaterialVariableCDOHandle(Native.Get());
        if(!TargetBase(SavedBoundary,&ObservedBits)||!SourceBase())return Stop(TEXT("variable_material_native_source_changed"),TEXT("Existing native CDO identity/metadata changed; no CDO creation or value sampling was requested."));
        bool CDOCurrent=true;
        if(!Now.IsResolved())CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("current_handle_resolved"),TEXT("true"),TEXT("false"));
        else if(Now!=NativeCDO.Get())CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("current_handle_equals_held_comparison"),TEXT("true"),TEXT("false"));
        else if(NativeCDO.Get())
        {
            if(!Loaded(NativeCDO.Get()))CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("held_loaded"),TEXT("true"),TEXT("false"));
            else if(NativeCDO->GetClass()!=Native.Get())CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("class_equals_held_native_comparison"),TEXT("true"),TEXT("false"));
            else
            {
                const FString ActualCDOPath=NativeCDO->GetPathName();
                if(ActualCDOPath!=CDOPath)CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("path"),CDOPath,ActualCDOPath);
                else {const EObjectFlags ActualCDOFlags=NativeCDO->GetFlags();if(ActualCDOFlags!=CDOFlags)CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("object_flags"),LexToString(uint32(CDOFlags)),LexToString(uint32(ActualCDOFlags)));}
            }
        }
        if(SkeletalArray&&CDOCurrent&&(!NativeCDO.Get()||NativeCDO->GetOuter()!=NativePackage.Get()||!NativeCDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)))CDOCurrent=IdentityMismatch(TEXT("native_cdo"),CDOPath,TEXT("skeletal_existing_cdo_role"),TEXT("true"),TEXT("false"));
        if(!CDOCurrent)return Stop(TEXT("variable_material_native_source_changed"),TEXT("Existing native CDO identity/metadata changed; no CDO creation or value sampling was requested."));
        if(SavedBoundary)
        {
            const auto Receipt=R.Data->GetObjectField(TEXT("native_resource_save_package_flags"));Receipt->SetBoolField(TEXT("source_guard_passed"),true);Receipt->SetBoolField(TEXT("admitted"),ObservedBits!=PackageBits);
            PackageBits=ObservedBits;Receipt->SetStringField(TEXT("expected_after_bits_string"),LexToString(PackageBits));
        }
        return true;
    }
    bool SameDescriptions(const TArray<FBPVariableDescription>& A,const TArray<FBPVariableDescription>& B)const
    {if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!MaterialDescriptionSame(A[I],B[I]))return false;return true;}
    bool Current()
    {
        if(!Source())return false;
        if(BP->GetFlags()!=BPFlags||Package->GetFlags()!=PackageFlags||BP->Status.GetValue()!=Status||Package->IsDirty()!=Dirty
            ||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->GeneratedClass.GetGCPtr().IsResolved()||((NiagaraSystem||TextureRenderTarget2D||SkeletalArray||MapStringVector)?(BP->SkeletonGeneratedClass.GetGCPtr()!=Skeleton.Get()||BP->GeneratedClass.GetGCPtr()!=Generated.Get()):(BP->SkeletonGeneratedClass!=Skeleton.Get()||BP->GeneratedClass!=Generated.Get()))
            ||!Loaded(Skeleton.Get())||!Loaded(Generated.Get())||!SameDescriptions(BP->NewVariables,Expected)||!SameDescriptions(BP->GeneratedVariables,GeneratedDescriptions)||BP->CategorySorting!=Categories)
            return Stop(TEXT("variable_material_target_changed"),TEXT("Current declaration/class/status/dirty scope changed outside the selected writer boundary."));
        return true;
    }
    bool Roles()
    {
        if(!TargetBase())return false;
        if(!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->GeneratedClass.GetGCPtr().IsResolved())return false;
        UClass* S=FindObject<UClass>(nullptr,*BP->SkeletonGeneratedClass.GetGCPtr().GetPathName());UClass* G=FindObject<UClass>(nullptr,*BP->GeneratedClass.GetGCPtr().GetPathName());
        if(!TargetBase()||!Loaded(S)||!Loaded(G)||((NiagaraSystem||TextureRenderTarget2D||SkeletalArray||MapStringVector)?(BP->SkeletonGeneratedClass.GetGCPtr()!=S||BP->GeneratedClass.GetGCPtr()!=G):(BP->SkeletonGeneratedClass!=S||BP->GeneratedClass!=G))||!S->ClassGeneratedBy.IsResolved()||S->ClassGeneratedBy!=BP.Get()||!G->ClassGeneratedBy.IsResolved()||G->ClassGeneratedBy!=BP.Get())return false;
        if(Skeleton.Get())RetiredClasses.Emplace(Skeleton.Get());if(Generated.Get())RetiredClasses.Emplace(Generated.Get());Skeleton.Reset(S);Generated.Reset(G);return true;
    }
    bool Begin(UBlueprint* Target,const FString& VariableName,UClass* Type,bool bSkeletalArray=false)
    {
        if(!Globals()||!Loaded(Target)||!Loaded(Type))return Fail(R,TEXT("variable_material_context_invalid"),TEXT("Exact loaded native resource type and idle Editor context required."));
        BP.Reset(Target);Editor.Reset(GEditor);Package.Reset(Target->GetOutermost());Native.Reset(Type);NativePackage.Reset(Type->GetOutermost());Name=FName(*VariableName);NiagaraSystem=Type==UNiagaraSystem::StaticClass();TextureRenderTarget2D=Type==UTextureRenderTarget2D::StaticClass();SkeletalArray=bSkeletalArray;
        if(!BP->ParentClass.GetGCPtr().IsResolved())return Fail(R,TEXT("variable_material_context_invalid"),TEXT("Current parent role must already be resolved."));
        Parent.Reset(FindObject<UClass>(nullptr,*BP->ParentClass.GetGCPtr().GetPathName()));Undo=GUndo;Path=BP->GetPathName();PackagePath=Package->GetName();PackageBits=Package->GetPackageFlags();
        NativePath=Native->GetPathName();NativeFlags=Native->GetFlags();NativeBits=Native->GetClassFlags();NativeSize=Native->GetStructureSize();
        if(!TargetBase()||!SourceBase()||!Native->HasAnyClassFlags(CLASS_Native)||Native->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||!Roles())
            return Fail(R,TEXT("variable_material_context_invalid"),TEXT("Existing target parent/generated classes and exact Engine native class must pass current runtime roles."));
        if(SkeletalArray&&(Type!=USkeletalMeshComponent::StaticClass()||!BindSkeletalSource()))return Fail(R,TEXT("variable_skeletal_array_source_unsupported"),TEXT("The exact already-loaded native SkeletalMeshComponent hierarchy is required; no native class or CDO is created."));
        const auto Handle=MaterialVariableCDOHandle(Native.Get());if(!Handle.IsResolved())return Fail(R,TEXT("variable_material_native_source_unavailable"),TEXT("Existing CDO handle must be resolved; no load or CDO creation."));
        const UObject* CDO=Handle==nullptr?nullptr:FindObject<UObject>(nullptr,*Handle.GetPathName());
        if(!TargetBase()||!SourceBase()||MaterialVariableCDOHandle(Native.Get())!=Handle||Handle!=CDO||(Handle!=nullptr&&!CDO))return Stop(TEXT("variable_material_scope_changed"),TEXT("Existing optional CDO lookup differs from the unchanged shallow native handle."));
        if(CDO&&(!Loaded(CDO)||CDO->GetClass()!=Native.Get()||!CDO->HasAnyFlags(RF_ClassDefaultObject)))return Fail(R,TEXT("variable_material_native_source_unavailable"),TEXT("Existing native CDO role is incomplete; no CDO creation."));
        NativeCDO.Reset(CDO);CDOPath=CDO?CDO->GetPathName():FString();CDOFlags=CDO?CDO->GetFlags():RF_NoFlags;
        if(SkeletalArray&&(!CDO||CDOPath!=TEXT("/Script/Engine.Default__SkeletalMeshComponent")||CDO->GetOuter()!=NativePackage.Get()||!CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)))return Fail(R,TEXT("variable_skeletal_array_source_unavailable"),TEXT("The exact existing native SkeletalMeshComponent CDO is required without creation or value sampling."));
        Original=Expected=BP->NewVariables;GeneratedDescriptions=BP->GeneratedVariables;Categories=BP->CategorySorting;BPFlags=BP->GetFlags();PackageFlags=Package->GetFlags();Status=BP->Status.GetValue();Dirty=Package->IsDirty();
        R.Data->SetBoolField(TEXT("native_resource_is_niagara_system"),NiagaraSystem);R.Data->SetBoolField(TEXT("native_resource_is_texture_render_target_2d"),TextureRenderTarget2D);R.Data->SetStringField(TEXT("native_resource_type_path"),NativePath);R.Data->SetBoolField(TEXT("native_resource_existing_cdo_available"),CDO!=nullptr);R.Data->SetStringField(TEXT("native_resource_existing_cdo_path"),CDOPath);
        R.Data->SetBoolField(TEXT("native_resource_cdo_creation_requested"),false);R.Data->SetBoolField(TEXT("native_resource_cdo_values_sampled"),false);
        if(SkeletalArray){R.Data->SetBoolField(TEXT("native_skeletal_array_declaration"),true);R.Data->SetObjectField(TEXT("native_skeletal_array_source_before"),SkeletalSourceState());R.Data->SetBoolField(TEXT("skeletal_array_nonempty_defaults_supported"),false);R.Data->SetBoolField(TEXT("skeletal_array_runtime_values_observed"),false);}
        R.Data->SetStringField(TEXT("change_observation_scope"),TEXT("all own/generated declaration descriptors, category order, current target class roles/status/dirty and selected generated default after explicit compile; native class/CDO identity metadata; no global callback purity"));return Current();
    }
    bool BeginMap(UBlueprint* Target,const FString& VariableName,bool DoubleValue=false)
    {
        MapStringVector=true;MapStringDouble=DoubleValue;if(!Globals()||!Loaded(Target))return Fail(R,TEXT("variable_map_context_invalid"),TEXT("An idle loaded own Blueprint target is required before exact native Map declaration."));
        BP.Reset(Target);Editor.Reset(GEditor);Package.Reset(Target->GetOutermost());Name=FName(*VariableName);Undo=GUndo;Path=BP->GetPathName();PackagePath=Package->GetName();PackageBits=Package->GetPackageFlags();
        if(!BP->ParentClass.GetGCPtr().IsResolved())return Fail(R,TEXT("variable_map_context_invalid"),TEXT("Current parent must already be resolved."));Parent.Reset(FindObject<UClass>(nullptr,*BP->ParentClass.GetGCPtr().GetPathName()));
        if(!MapStringDouble)
        {
        UScriptStruct* V=FindObject<UScriptStruct>(nullptr,TEXT("/Script/CoreUObject.Vector"));if(!TargetBase()||!Loaded(V)||V!=TBaseStructure<FVector>::Get())return Fail(R,TEXT("variable_map_native_source_unavailable"),TEXT("Exact already-loaded native CoreUObject.Vector is required without source load or CDO creation."));
        MapVector.Reset(V);NativePackage.Reset(V->GetPackage());NativePath=TEXT("/Script/CoreUObject.Vector");MapVectorFlags=V->GetFlags();MapStructFlags=uint32(V->StructFlags);MapAlignment=V->GetMinAlignment();if(!Loaded(NativePackage.Get()))return Fail(R,TEXT("variable_map_native_source_unavailable"),TEXT("Native Vector package is unavailable."));MapPackageObjectFlags=NativePackage->GetFlags();
        const TCHAR* Axes[]={TEXT("X"),TEXT("Y"),TEXT("Z")};const int32 Offsets[]={STRUCT_OFFSET(FVector,X),STRUCT_OFFSET(FVector,Y),STRUCT_OFFSET(FVector,Z)};
        for(int32 I=0;I<3;++I){const FProperty* F=V->FindPropertyByName(Axes[I]);if(!F||F->GetClass()!=FDoubleProperty::StaticClass()||F->GetOwnerStruct()!=V||F->ArrayDim!=1||F->GetOffset_ForInternal()!=Offsets[I]||F->GetSize()!=sizeof(double))return Fail(R,TEXT("variable_map_native_layout_unsupported"),TEXT("Native FVector must have its exact three double fields."));MapAxes.Add({F,F->GetFName(),F->GetPropertyFlags(),Offsets[I],F->GetSize()});}
        }
        else
        {
            NativePackage.Reset(FindPackage(nullptr,TEXT("/Script/CoreUObject")));if(!TargetBase()||!Loaded(NativePackage.Get()))return Fail(R,TEXT("variable_map_native_source_unavailable"),TEXT("Loaded CoreUObject scalar source package is required; no struct/class alias or loader is used."));
            NativePath=TEXT("");MapAlignment=alignof(double);MapPackageObjectFlags=NativePackage->GetFlags();
        }
        if(!MapSourceCurrent()||!Roles())return Fail(R,TEXT("variable_map_context_invalid"),TEXT("Current target classes/native layout differ before Map declaration."));
        Original=Expected=BP->NewVariables;GeneratedDescriptions=BP->GeneratedVariables;Categories=BP->CategorySorting;BPFlags=BP->GetFlags();PackageFlags=Package->GetFlags();Status=BP->Status.GetValue();Dirty=Package->IsDirty();
        R.Data->SetBoolField(MapStringDouble?TEXT("native_map_string_double_declaration"):TEXT("native_map_string_vector_declaration"),true);R.Data->SetObjectField(TEXT("native_map_source_before"),MapSourceState());R.Data->SetBoolField(TEXT("map_default_buffer_created"),false);R.Data->SetBoolField(TEXT("map_runtime_values_sampled"),false);
        R.Data->SetStringField(TEXT("change_observation_scope"),MapStringDouble?TEXT("Exact own transient String to scalar double Map declaration; full terminal/ref/const/wrapper/GUID/default/current target metadata; no float conversion, value buffer, local/output Map or runtime execution."):TEXT("Exact own String to native Vector Map declaration; all own/generated descriptors, selected GUID/full terminal type/default/metadata/transient flags, current target and native FVector layout; no Map value evaluation or global callback purity"));return Current();
    }
    FBPVariableDescription* Selected(){for(auto& V:BP->NewVariables)if(V.VarName==Name)return &V;return nullptr;}
    bool AcceptWriter(bool bAdded=false,bool bSaved=false,bool bNewSelected=false)
    {
        if(!Source(bSaved))return false;
        TArray<FBPVariableDescription> OldOthers,NowOthers;for(const auto& V:Original)if(V.VarName!=Name)OldOthers.Add(V);for(const auto& V:BP->NewVariables)if(V.VarName!=Name)NowOthers.Add(V);
        const uint32 BPDelta=uint32(BP->GetFlags())^uint32(BPFlags),PackageDelta=uint32(Package->GetFlags())^uint32(PackageFlags);
        const FBPVariableDescription* Previous=nullptr;for(const auto& V:Expected)if(V.VarName==Name)Previous=&V;
        // This receipt serializes copied descriptors and raw already-loaded roles before the original ordered checks.
        // After a failed condition only this plain JSON prefix is returned; no extra UObject query, export or SDK snapshot is dispatched.
        auto Validation=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("native_resource_writer_validation"),Validation);
        double ValidationCount=0;R.Data->TryGetNumberField(TEXT("native_resource_writer_validation_count"),ValidationCount);R.Data->SetNumberField(TEXT("native_resource_writer_validation_count"),ValidationCount+1);
        Validation->SetStringField(TEXT("object_path"),Path);Validation->SetStringField(TEXT("stage"),R.Stage);Validation->SetStringField(TEXT("first_failed_condition"),TEXT(""));
        Validation->SetBoolField(TEXT("later_conditions_unobserved_after_failure"),true);Validation->SetBoolField(TEXT("conditions_complete"),false);
        Validation->SetBoolField(TEXT("source_preflight_passed"),true);Validation->SetBoolField(TEXT("new_selected_admission"),bNewSelected);
        Validation->SetStringField(TEXT("bp_flags_expected"),LexToString(uint32(BPFlags)));Validation->SetStringField(TEXT("bp_flags_actual"),LexToString(uint32(BP->GetFlags())));
        Validation->SetStringField(TEXT("package_flags_expected"),LexToString(uint32(PackageFlags)));Validation->SetStringField(TEXT("package_flags_actual"),LexToString(uint32(Package->GetFlags())));
        Validation->SetNumberField(TEXT("descriptor_count_expected"),Original.Num()+(bAdded?1:0));Validation->SetNumberField(TEXT("descriptor_count_actual"),BP->NewVariables.Num());
        Validation->SetNumberField(TEXT("unselected_count_expected"),OldOthers.Num());Validation->SetNumberField(TEXT("unselected_count_actual"),NowOthers.Num());
        Validation->SetNumberField(TEXT("generated_count_expected"),GeneratedDescriptions.Num());Validation->SetNumberField(TEXT("generated_count_actual"),BP->GeneratedVariables.Num());
        Validation->SetBoolField(TEXT("dirty_expected"),Dirty);Validation->SetBoolField(TEXT("dirty_actual"),Package->IsDirty());
        // LF: successful native save marks the held UPackage UObject as loaded.
        // EObjectFlags are distinct from the earlier PKG_NewlyCreated package-bit receipt.
        const uint32 SavedObjectLoadFlags=uint32(RF_WasLoaded|RF_LoadCompleted);
        const uint32 ObservedPackageObjectFlags=uint32(PackageFlags)^PackageDelta;
        const bool SaveObjectFlagsQualified=bSaved&&R.Stage==TEXT("save")&&R.bSaveAttempted&&R.bSaved;
        const bool SavedObjectFlagsAddition=SaveObjectFlagsQualified&&PackageDelta==SavedObjectLoadFlags
            &&(uint32(PackageFlags)&SavedObjectLoadFlags)==0&&ObservedPackageObjectFlags==(uint32(PackageFlags)|SavedObjectLoadFlags);
        TSharedPtr<FJsonObject> SaveObjectFlagsReceipt;
        if(bSaved)
        {
            SaveObjectFlagsReceipt=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("native_resource_save_package_object_flags"),SaveObjectFlagsReceipt);
            SaveObjectFlagsReceipt->SetStringField(TEXT("stage"),R.Stage);SaveObjectFlagsReceipt->SetStringField(TEXT("object_path"),PackagePath);SaveObjectFlagsReceipt->SetStringField(TEXT("flag_kind"),TEXT("EObjectFlags of the held UPackage UObject, not EPackageFlags/PKG bits"));
            SaveObjectFlagsReceipt->SetBoolField(TEXT("saved_boundary_requested"),bSaved);SaveObjectFlagsReceipt->SetBoolField(TEXT("save_attempted"),R.bSaveAttempted);SaveObjectFlagsReceipt->SetBoolField(TEXT("save_returned_true"),R.bSaved);SaveObjectFlagsReceipt->SetBoolField(TEXT("qualified_successful_save"),SaveObjectFlagsQualified);
            SaveObjectFlagsReceipt->SetStringField(TEXT("expected_before_flags_string"),LexToString(uint32(PackageFlags)));SaveObjectFlagsReceipt->SetStringField(TEXT("actual_after_flags_string"),LexToString(ObservedPackageObjectFlags));SaveObjectFlagsReceipt->SetStringField(TEXT("xor_flags_string"),LexToString(PackageDelta));
            SaveObjectFlagsReceipt->SetStringField(TEXT("was_loaded_flag_string"),LexToString(uint32(RF_WasLoaded)));SaveObjectFlagsReceipt->SetStringField(TEXT("load_completed_flag_string"),LexToString(uint32(RF_LoadCompleted)));SaveObjectFlagsReceipt->SetStringField(TEXT("allowed_addition_mask_string"),LexToString(SavedObjectLoadFlags));
            SaveObjectFlagsReceipt->SetBoolField(TEXT("actual_observed"),true);SaveObjectFlagsReceipt->SetBoolField(TEXT("unchanged"),PackageDelta==0);SaveObjectFlagsReceipt->SetBoolField(TEXT("exact_missing_pair_added"),SavedObjectFlagsAddition);SaveObjectFlagsReceipt->SetBoolField(TEXT("current_checks_passed"),false);SaveObjectFlagsReceipt->SetBoolField(TEXT("admitted"),false);
        }
        const auto PlainDescription=[](const FBPVariableDescription& D)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),D.VarName.ToString());J->SetStringField(TEXT("guid"),D.VarGuid.ToString(EGuidFormats::Digits));
            J->SetStringField(TEXT("category_text"),D.Category.ToString());J->SetStringField(TEXT("friendly_name"),D.FriendlyName);J->SetStringField(TEXT("stored_default"),D.DefaultValue);
            J->SetStringField(TEXT("property_flags_string"),LexToString(D.PropertyFlags));J->SetStringField(TEXT("rep_notify"),D.RepNotifyFunc.ToString());J->SetNumberField(TEXT("replication_condition"),int32(D.ReplicationCondition.GetValue()));
            J->SetStringField(TEXT("pin_category"),D.VarType.PinCategory.ToString());J->SetStringField(TEXT("pin_subcategory"),D.VarType.PinSubCategory.ToString());J->SetNumberField(TEXT("pin_container"),int32(D.VarType.ContainerType));
            J->SetBoolField(TEXT("pin_is_reference"),D.VarType.bIsReference);J->SetBoolField(TEXT("pin_is_const"),D.VarType.bIsConst);J->SetBoolField(TEXT("pin_is_weak"),D.VarType.bIsWeakPointer);
            J->SetBoolField(TEXT("pin_is_uobject_wrapper"),D.VarType.bIsUObjectWrapper);J->SetBoolField(TEXT("pin_single_precision"),D.VarType.bSerializeAsSinglePrecisionFloat);J->SetBoolField(TEXT("pin_terminal_wrapper"),D.VarType.PinValueType.bTerminalIsUObjectWrapper);
            TArray<TSharedPtr<FJsonValue>> Entries;for(const auto& E:D.MetaDataArray){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("key"),E.DataKey.ToString());M->SetStringField(TEXT("value"),E.DataValue);Entries.Add(MakeShared<FJsonValueObject>(M));}J->SetArrayField(TEXT("metadata_entries"),Entries);return J;
        };
        FBPVariableDescription* ActualSelected=Selected();Validation->SetBoolField(TEXT("selected_expected_available"),Previous!=nullptr);Validation->SetBoolField(TEXT("selected_actual_available"),ActualSelected!=nullptr);
        if(Previous){const FBPVariableDescription Copy=*Previous;Validation->SetObjectField(TEXT("selected_expected"),PlainDescription(Copy));}
        if(ActualSelected){const FBPVariableDescription Copy=*ActualSelected;Validation->SetObjectField(TEXT("selected_actual"),PlainDescription(Copy));}
        auto Conditions=MakeShared<FJsonObject>();Validation->SetObjectField(TEXT("conditions"),Conditions);TArray<TSharedPtr<FJsonValue>> Evaluated;
        const auto Test=[&](const TCHAR* Condition,bool Passed)
        {
            Conditions->SetBoolField(Condition,Passed);Evaluated.Add(MakeShared<FJsonValueString>(Condition));Validation->SetArrayField(TEXT("evaluated_conditions"),Evaluated);
            if(!Passed){Validation->SetStringField(TEXT("first_failed_condition"),Condition);Validation->SetBoolField(TEXT("expected"),true);Validation->SetBoolField(TEXT("actual"),false);return false;}return true;
        };
        if(!Test(TEXT("unselected_descriptors_preserved"),SameDescriptions(OldOthers,NowOthers))
            ||!Test(TEXT("generated_descriptors_preserved"),SameDescriptions(BP->GeneratedVariables,GeneratedDescriptions))
            ||!Test(TEXT("category_order_preserved"),BP->CategorySorting==Categories)
            ||!Test(TEXT("selected_descriptor_present"),Selected()!=nullptr)
            ||!Test(TEXT("selected_description_preserved"),bNewSelected||(Previous&&MaterialDescriptionSame(*Previous,*Selected())))
            ||!Test(TEXT("descriptor_count_matches"),BP->NewVariables.Num()==Original.Num()+(bAdded?1:0))
            ||!Test(TEXT("blueprint_flags_transition_allowed"),!BPDelta||(BPDelta==uint32(RF_Transactional)&&BP->HasAnyFlags(RF_Transactional)))
            ||!Test(TEXT("package_flags_transition_allowed"),!PackageDelta||(PackageDelta==uint32(RF_Transactional)&&Package->HasAnyFlags(RF_Transactional))||SavedObjectFlagsAddition)
            ||!Test(TEXT("dirty_transition_allowed"),bSaved||!Dirty||Package->IsDirty())
            ||!Test(TEXT("current_class_roles"),Roles()))
            return Stop(TEXT("variable_material_writer_scope_changed"),TEXT("Selected writer changed unselected declarations/current roles or unexpected flags; actual prefix remains and no retry is safe."));
        Validation->SetBoolField(TEXT("later_conditions_unobserved_after_failure"),false);Validation->SetBoolField(TEXT("conditions_complete"),true);
        Expected=BP->NewVariables;BPFlags=BP->GetFlags();PackageFlags=Package->GetFlags();Status=BP->Status.GetValue();Dirty=Package->IsDirty();const bool CurrentPassed=Current();
        if(SaveObjectFlagsReceipt.IsValid()){SaveObjectFlagsReceipt->SetBoolField(TEXT("current_checks_passed"),CurrentPassed);SaveObjectFlagsReceipt->SetBoolField(TEXT("admitted"),CurrentPassed&&SavedObjectFlagsAddition);}
        return CurrentPassed;
    }
    TSharedRef<FJsonObject> Snapshot()const
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("blueprint_name"),BP->GetName());J->SetStringField(TEXT("asset_path"),Path);J->SetBoolField(TEXT("package_dirty"),Package->IsDirty());J->SetNumberField(TEXT("blueprint_status"),int32(BP->Status.GetValue()));
        TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=0;I<BP->NewVariables.Num();++I)Rows.Add(MakeShared<FJsonValueObject>(MaterialDeclarationRow(BP->NewVariables[I],I)));
        J->SetArrayField(TEXT("new_variables"),Rows);J->SetNumberField(TEXT("new_variable_count"),Rows.Num());J->SetNumberField(TEXT("generated_variable_count"),BP->GeneratedVariables.Num());return J;
    }
    void Observed(const TSharedRef<FJsonObject>& Before)
    {
        if(!Current())return;auto After=Snapshot();R.Data->SetObjectField(TEXT("after"),After);R.bChanged=JsonText(Before)!=JsonText(After);R.bChangeStateKnown=true;
        R.Data->SetBoolField(TEXT("dirty_before"),Before->GetBoolField(TEXT("package_dirty")));R.Data->SetBoolField(TEXT("dirty_after"),Dirty);R.Data->SetBoolField(TEXT("package_dirty_before"),Before->GetBoolField(TEXT("package_dirty")));R.Data->SetBoolField(TEXT("package_dirty_after"),Dirty);
        R.Data->SetBoolField(TEXT("native_resource_source_identity_verified"),true);
        if(MapStringVector){const auto AfterSource=MapSourceState();R.Data->SetObjectField(TEXT("native_map_source_after"),AfterSource);R.Data->SetBoolField(TEXT("native_map_source_observed_unchanged"),FJsonValue::CompareEqual(FJsonValueObject(R.Data->GetObjectField(TEXT("native_map_source_before"))),FJsonValueObject(AfterSource)));}
        if(SkeletalArray){const auto AfterSource=SkeletalSourceState();R.Data->SetObjectField(TEXT("native_skeletal_array_source_after"),AfterSource);R.Data->SetBoolField(TEXT("native_skeletal_array_source_observed_unchanged"),FJsonValue::CompareEqual(FJsonValueObject(R.Data->GetObjectField(TEXT("native_skeletal_array_source_before"))),FJsonValueObject(AfterSource)));}
    }
};
struct FMaterialVariableDefault
{
    FMaterialVariableScope& S;TStrongObjectPtr<UStruct> Owner;TStrongObjectPtr<UObject> Reference;TStrongObjectPtr<UClass> ReferenceClass;TStrongObjectPtr<UPackage> ReferencePackage;FString ReferencePath;EObjectFlags ReferenceFlags=RF_NoFlags;uint32 ReferencePackageFlags=0;bool ReferenceDirty=false;FObjectProperty* Property=nullptr;void* Value=nullptr;void* CheckValue=nullptr;
    FString Stored;EObjectFlags OwnerFlags=RF_NoFlags;EPropertyFlags Flags=CPF_None;int32 Size=0,Offset=0;bool Ready=false;
    explicit FMaterialVariableDefault(FMaterialVariableScope& Scope):S(Scope){}
    // Only a verified exact FObjectProperty with CPF_NoDestructor is allocated. Freeing POD storage never revisits a stale property.
    ~FMaterialVariableDefault(){FMemory::Free(CheckValue);FMemory::Free(Value);}
    bool Current()
    {
        if(!S.Current())return false;
        if(Reference.Get()&&(!FMaterialVariableScope::Loaded(Reference.Get())||!FMaterialVariableScope::Loaded(ReferenceClass.Get())||!FMaterialVariableScope::Loaded(ReferencePackage.Get())
            ||Reference->GetPathName()!=ReferencePath||Reference->GetClass()!=ReferenceClass.Get()||Reference->GetFlags()!=ReferenceFlags||Reference->GetOutermost()!=ReferencePackage.Get()
            ||ReferencePackage->GetPackageFlags()!=ReferencePackageFlags||ReferencePackage->IsDirty()!=ReferenceDirty))
            return S.Stop(TEXT("variable_material_reference_changed"),TEXT("The held resource default object/class/package metadata changed; no stale property or next SDK."));
        if(!FMaterialVariableScope::Loaded(Owner.Get())||Owner->GetFlags()!=OwnerFlags||!Property||Property->GetOwnerStruct()!=Owner.Get()||Property->GetClass()!=FObjectProperty::StaticClass()
            ||Property->ArrayDim!=1||!Property->PropertyClass.IsResolved()||Property->PropertyClass!=S.Native.Get()||Property->GetPropertyFlags()!=Flags||Property->GetSize()!=Size||Property->GetOffset_ForInternal()!=Offset)
            return S.Stop(TEXT("variable_material_scratch_changed"),TEXT("Private typed object-default property/layout changed; no stale virtual cleanup or next SDK."));
        return true;
    }
    bool Create(const FEdGraphPinType& Pin)
    {
        if(!S.Current())return false;Owner.Reset(NewObject<UStruct>(GetTransientPackage(),NAME_None,RF_Transient));S.R.Data->SetBoolField(TEXT("native_resource_scratch_returned"),Owner.IsValid());
        if(!S.Current()||!FMaterialVariableScope::Loaded(Owner.Get()))return false;OwnerFlags=Owner->GetFlags();
        const auto* Schema=GetDefault<UEdGraphSchema_K2>();if(!S.Current()||!FMaterialVariableScope::Loaded(Owner.Get()))return false;
        FCompilerResultsLog Log;FProperty* Returned=FKismetCompilerUtilities::CreatePropertyOnScope(Owner.Get(),TEXT("Value"),Pin,S.Skeleton.Get(),CPF_None,Schema,Log);
        S.R.Data->SetBoolField(TEXT("native_resource_scratch_property_returned"),Returned!=nullptr);if(!S.Current()||!FMaterialVariableScope::Loaded(Owner.Get()))return false;
        Property=CastField<FObjectProperty>(Returned);if(!Property||Log.NumErrors||Property->GetClass()!=FObjectProperty::StaticClass()||Property->GetOwnerStruct()!=Owner.Get()||!Property->PropertyClass.IsResolved()||Property->PropertyClass!=S.Native.Get())
            return Fail(S.R,TEXT("variable_type_unsupported"),TEXT("Public Kismet did not create the exact scalar object default property."));
        Owner->AddCppProperty(Property);if(!S.Current())return false;Owner->Bind();if(!S.Current())return false;Owner->StaticLink(true);if(!S.Current())return false;
        if(!Property->HasAllPropertyFlags(CPF_NoDestructor|CPF_ZeroConstructor)||Property->ArrayDim!=1||Property->GetOwnerStruct()!=Owner.Get()||!Property->PropertyClass.IsResolved()||Property->PropertyClass!=S.Native.Get())
            return Fail(S.R,TEXT("variable_material_scratch_unsupported"),TEXT("Only zero-constructible destructor-free scalar object storage is supported."));
        Flags=Property->GetPropertyFlags();Size=Property->GetSize();Offset=Property->GetOffset_ForInternal();if(Size!=sizeof(TObjectPtr<UObject>)||Offset!=0)return Fail(S.R,TEXT("variable_material_scratch_unsupported"),TEXT("Unexpected scalar object storage layout."));
        Value=FMemory::Malloc(Size,Property->GetMinAlignment());Property->InitializeValue(Value);Ready=true;return Current();
    }
    // LC: a persistent exact System asset reference only. No component SetAsset,
    // DI duplication, asset compile, instance creation or simulation is requested.
    bool NiagaraSystemReference(UObject* Object)
    {
        if(!Current())return false;
        if(!(IsValid(Object)&&Object->GetClass()==UNiagaraSystem::StaticClass()&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)))
            return Fail(S.R,TEXT("variable_reference_unsupported"),TEXT("Niagara default requires a loaded exact UNiagaraSystem asset; subclass/CDO/archetype/transient references are unsupported."));
        UPackage* Package=Object->GetPackage();const FString PackageName=Package->GetName();
        if(Object->GetOuter()!=Package||Object->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||Package->IsDirty()
            ||Package->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)
            ||(!PackageName.StartsWith(TEXT("/Game/"))&&!PackageName.StartsWith(TEXT("/Engine/"))))
            return Fail(S.R,TEXT("variable_reference_unsupported"),TEXT("System defaults require a saved clean persistent /Game or /Engine asset root; no plugin mount or runtime object fallback."));
        const bool EditorOnly=Object->IsEditorOnly();if(!Current())return false;
        if(EditorOnly)return Fail(S.R,TEXT("variable_reference_unsupported"),TEXT("Editor-only Niagara asset references are unsupported."));
        auto Audit=ReadPackageDependencies({PackageName},true,4096);if(!Current())return false;S.R.Data->SetObjectField(TEXT("last_reference_dependency_audit"),Audit);
        bool Passed=false;if(!Audit->TryGetBoolField(TEXT("passed"),Passed)||!Passed)return Fail(S.R,TEXT("variable_reference_unverified"),TEXT("System default requires a complete clean saved dependency graph."));
        // Keep the complete all-reference audit above. Only this exact System default
        // classifies saved Registry Package edges; non-Game provenance is never a runtime allowance.
        auto Classification=MakeShared<FJsonObject>();S.R.Data->SetObjectField(TEXT("niagara_system_dependency_classification"),Classification);
        Classification->SetBoolField(TEXT("complete"),false);Classification->SetBoolField(TEXT("edge_classification_complete"),false);Classification->SetBoolField(TEXT("passed"),false);Classification->SetBoolField(TEXT("packaging_verified"),false);
        Classification->SetStringField(TEXT("scope"),TEXT("saved on-disk AssetRegistry Package edge properties and root Game-reachable metadata; not cook/runtime validation"));
        TArray<TSharedPtr<FJsonValue>> RawEdges,RawGameEdges,RawEditorEdges,AssetRows,RuntimeRows;TMap<FString,TSet<FString>> ExpectedEdges,GameEdges;
        TMap<FString,TArray<FAssetData>> AssetsByPackage;TSet<FString> Packages;Packages.Add(PackageName);
        const auto Flush=[&](){Classification->SetArrayField(TEXT("raw_edges"),RawEdges);Classification->SetArrayField(TEXT("game_edges"),RawGameEdges);Classification->SetArrayField(TEXT("editor_only_edges"),RawEditorEdges);Classification->SetArrayField(TEXT("on_disk_assets"),AssetRows);Classification->SetArrayField(TEXT("runtime_reachable_packages"),RuntimeRows);};
        const auto Reject=[&](const FString& Why,bool Unsupported=false){Flush();Classification->SetStringField(TEXT("failure"),Why);return Fail(S.R,Unsupported?TEXT("variable_reference_unsupported"):TEXT("variable_reference_unverified"),Why);};
        for(const auto& Edge:Audit->GetArrayField(TEXT("edges")))
        {
            const auto E=Edge->AsObject();const FString From=E->GetStringField(TEXT("from")),To=E->GetStringField(TEXT("to"));
            ExpectedEdges.FindOrAdd(From).Add(To);Packages.Add(From);if(!To.StartsWith(TEXT("/Script/")))Packages.Add(To);
        }
        if(Packages.Num()>4096)return Reject(TEXT("System metadata classification exceeds the existing 4096-package audit bound."));
        bool HasNiagaraContent=false;for(const auto& Name:Packages)
        {if(Name.StartsWith(TEXT("/Niagara/")))HasNiagaraContent=true;else if(!Name.StartsWith(TEXT("/Game/"))&&!Name.StartsWith(TEXT("/Engine/")))return Reject(TEXT("Unaudited System dependency content mount: ")+Name,true);}
        if(HasNiagaraContent)
        {
            const TSharedPtr<IPlugin> Plugin=IPluginManager::Get().FindPlugin(TEXT("Niagara"));if(!Current())return false;
            if(!Plugin.IsValid())return Reject(TEXT("Engine Niagara content plugin metadata is unavailable."));
            const bool Enabled=Plugin->IsEnabled(),Mounted=Plugin->IsMounted(),Content=Plugin->CanContainContent();
            const EPluginLoadedFrom Origin=Plugin->GetLoadedFrom();const FString Mount=Plugin->GetMountedAssetPath();const FPluginDescriptor Descriptor=Plugin->GetDescriptor();if(!Current())return false;
            bool RuntimeNiagara=false,RuntimeCore=false;for(const auto& Module:Descriptor.Modules)
            {if(Module.Name.ToString()==TEXT("Niagara")&&Module.Type==EHostType::Runtime)RuntimeNiagara=true;if(Module.Name.ToString()==TEXT("NiagaraCore")&&Module.Type==EHostType::Runtime)RuntimeCore=true;}
            auto P=MakeShared<FJsonObject>();P->SetBoolField(TEXT("enabled"),Enabled);P->SetBoolField(TEXT("mounted"),Mounted);P->SetBoolField(TEXT("can_contain_content"),Content);
            P->SetBoolField(TEXT("engine_plugin"),Origin==EPluginLoadedFrom::Engine);P->SetStringField(TEXT("mount"),Mount);P->SetBoolField(TEXT("niagara_module_runtime"),RuntimeNiagara);P->SetBoolField(TEXT("niagara_core_module_runtime"),RuntimeCore);
            Classification->SetObjectField(TEXT("niagara_plugin"),P);
            if(!Enabled||!Mounted||!Content||Origin!=EPluginLoadedFrom::Engine||Mount!=TEXT("/Niagara/")||!RuntimeNiagara||!RuntimeCore)
                return Reject(TEXT("The exact mounted Engine Niagara content/runtime module roles are unverified."));
        }
        FAssetRegistryModule* RegistryModule=FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));if(!Current())return false;
        if(!RegistryModule)return Reject(TEXT("The existing AssetRegistry module is unavailable; no module/class/asset load fallback."));
        IAssetRegistry& Registry=RegistryModule->Get();const bool Loading=Registry.IsLoadingAssets();if(!Current())return false;
        if(Loading)return Reject(TEXT("AssetRegistry discovery is in progress; System dependency classification is incomplete."));
        TArray<FString> Ordered=Packages.Array();Ordered.Sort();
        for(const FString& Name:Ordered)
        {
            TArray<FAssetData> Assets;const bool Found=Registry.GetAssetsByPackageName(FName(*Name),Assets,true,false);
            Classification->SetStringField(TEXT("last_asset_query_package"),Name);Classification->SetBoolField(TEXT("last_asset_query_returned"),Found);if(!Current())return false;
            if(!Found||Assets.IsEmpty())return Reject(TEXT("Missing on-disk System dependency asset metadata: ")+Name);
            if(Assets.Num()>4096-AssetRows.Num())return Reject(TEXT("System dependency asset metadata exceeds the 4096-row bound."));
            for(const auto& Asset:Assets)
            {
                auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("package"),Asset.PackageName.ToString());A->SetStringField(TEXT("object_path"),Asset.GetObjectPathString());A->SetStringField(TEXT("class_path"),Asset.AssetClassPath.ToString());
                A->SetStringField(TEXT("package_flags_string"),LexToString(Asset.PackageFlags));A->SetBoolField(TEXT("top_level_asset"),Asset.IsTopLevelAsset());AssetRows.Add(MakeShared<FJsonValueObject>(A));
                if(!Asset.IsValid()||Asset.PackageName.ToString()!=Name)return Reject(TEXT("On-disk dependency metadata does not match its exact queried package: ")+Name);
            }
            AssetsByPackage.Add(Name,MoveTemp(Assets));
            TArray<FAssetDependency> Dependencies;const bool Queried=Registry.GetDependencies(FAssetIdentifier(FName(*Name)),Dependencies,UE::AssetRegistry::EDependencyCategory::Package);
            Classification->SetStringField(TEXT("last_dependency_query_package"),Name);Classification->SetBoolField(TEXT("last_dependency_query_returned"),Queried);if(!Current())return false;
            if(!Queried)return Reject(TEXT("Package edge property query failed: ")+Name);
            if(Dependencies.Num()>65536-RawEdges.Num())return Reject(TEXT("System dependency classification exceeds the 65536-edge bound."));
            TSet<FString> Observed;
            for(const auto& Dependency:Dependencies)
            {
                const FString To=Dependency.AssetId.PackageName.ToString();const bool Game=EnumHasAnyFlags(Dependency.Properties,UE::AssetRegistry::EDependencyProperty::Game);
                auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("from"),Name);E->SetStringField(TEXT("to"),To);E->SetNumberField(TEXT("properties"),static_cast<uint8>(Dependency.Properties));E->SetBoolField(TEXT("game"),Game);E->SetBoolField(TEXT("editor_only_dependency"),!Game);
                E->SetBoolField(TEXT("hard"),EnumHasAnyFlags(Dependency.Properties,UE::AssetRegistry::EDependencyProperty::Hard));E->SetBoolField(TEXT("build"),EnumHasAnyFlags(Dependency.Properties,UE::AssetRegistry::EDependencyProperty::Build));const auto Row=MakeShared<FJsonValueObject>(E);RawEdges.Add(Row);if(Game)RawGameEdges.Add(Row);else RawEditorEdges.Add(Row);
                if(!Dependency.AssetId.IsPackage()||Dependency.Category!=UE::AssetRegistry::EDependencyCategory::Package||To.IsEmpty()
                    ||(static_cast<uint8>(Dependency.Properties)&~static_cast<uint8>(UE::AssetRegistry::EDependencyProperty::PackageMask)))return Reject(TEXT("Unrecognized Package dependency property/identifier: ")+Name);
                Observed.Add(To);if(Game)GameEdges.FindOrAdd(Name).Add(To);
            }
            const TSet<FString>* Expected=ExpectedEdges.Find(Name);
            if(Observed.Num()!=(Expected?Expected->Num():0))return Reject(TEXT("All-reference and classified dependency edge counts disagree: ")+Name);
            if(Expected)for(const FString& To:*Expected)if(!Observed.Contains(To))return Reject(TEXT("All-reference and classified dependency edge targets disagree: ")+Name);
            Flush();
        }
        const bool StillLoading=Registry.IsLoadingAssets();if(!Current())return false;if(StillLoading)return Reject(TEXT("AssetRegistry resumed discovery during classification."));Classification->SetBoolField(TEXT("edge_classification_complete"),true);
        const TSet<FString> Scripts={TEXT("/Script/CoreUObject"),TEXT("/Script/Engine"),TEXT("/Script/PhysicsCore"),TEXT("/Script/MeshDescription"),TEXT("/Script/StaticMeshDescription"),TEXT("/Script/NavigationSystem"),TEXT("/Script/Niagara"),TEXT("/Script/NiagaraCore")};
        TArray<FString> RuntimeQueue={PackageName};TSet<FString> RuntimeSeen;RuntimeSeen.Add(PackageName);
        for(int32 I=0;I<RuntimeQueue.Num();++I)if(const auto* Edges=GameEdges.Find(RuntimeQueue[I]))for(const FString& To:*Edges)
        {
            if(To.StartsWith(TEXT("/Script/")))
            {if(!Scripts.Contains(To))return Reject(TEXT("Game-reachable System dependency uses an unaudited runtime module: ")+To,true);}
            else if(!RuntimeSeen.Contains(To)){RuntimeSeen.Add(To);RuntimeQueue.Add(To);}
        }
        // Retain loaded package metadata through the final current barrier. Registry reads
        // do not GetAsset/LoadAsset or create class defaults for dependency assets.
        TArray<TStrongObjectPtr<UPackage>> LoadedPackages;TArray<FString> LoadedNames;TArray<uint32> LoadedFlags;
        for(const FString& Name:RuntimeQueue)
        {
            const auto* Assets=AssetsByPackage.Find(Name);if(!Assets)return Reject(TEXT("Game-reachable package was not covered by the complete recursive audit: ")+Name);
            for(const auto& Asset:*Assets)
            {
                const FString Class=Asset.AssetClassPath.ToString(),ClassPackage=Asset.AssetClassPath.GetPackageName().ToString();
                if(!Asset.IsTopLevelAsset()||Asset.IsRedirector()||Asset.HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||!Scripts.Contains(ClassPackage)
                    ||(ClassPackage==TEXT("/Script/Niagara")&&Class!=TEXT("/Script/Niagara.NiagaraSystem")))return Reject(TEXT("Game-reachable asset has an unsupported class or editor/developer/uncooked package role: ")+Name,true);
                if(Name.StartsWith(TEXT("/Niagara/"))&&Class!=TEXT("/Script/Engine.UserDefinedEnum"))return Reject(TEXT("Niagara runtime content class is outside the audited exact UserDefinedEnum subset: ")+Class,true);
            }
            if(!Current())return false;UPackage* Loaded=FindPackage(nullptr,*Name);if(!Current())return false;
            if(Loaded)
            {
                if(!FMaterialVariableScope::Loaded(Loaded)||Loaded->GetName()!=Name||Loaded->IsDirty()||Loaded->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))return Reject(TEXT("Loaded runtime dependency package is dirty or unavailable: ")+Name);
                LoadedPackages.Emplace(Loaded);LoadedNames.Add(Name);LoadedFlags.Add(Loaded->GetPackageFlags());
            }
            auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("package"),Name);P->SetStringField(TEXT("policy"),TEXT("game_reachable_saved_metadata_verified"));RuntimeRows.Add(MakeShared<FJsonValueObject>(P));
        }
        if(!Current())return false;
        for(int32 I=0;I<LoadedPackages.Num();++I)if(!FMaterialVariableScope::Loaded(LoadedPackages[I].Get())||LoadedPackages[I]->GetName()!=LoadedNames[I]||LoadedPackages[I]->GetPackageFlags()!=LoadedFlags[I]||LoadedPackages[I]->IsDirty())return Reject(TEXT("Captured runtime dependency package metadata changed before completion."));
        Flush();Classification->SetBoolField(TEXT("complete"),true);Classification->SetBoolField(TEXT("passed"),true);Classification->SetStringField(TEXT("editor_provenance_policy"),TEXT("non-Game rows remain in the complete audit/raw list; they are not authorized as runtime modules or packaging proof"));
        S.R.Data->SetBoolField(TEXT("niagara_system_reference_runtime_dependency_verified"),true);return true;
    }
    bool Prepare(const FString& Json)
    {
        TSharedPtr<FJsonValue> V;if(!Parse(Json,V,S.R)||!Current())return false;
        if(V->Type!=EJson::Null)
        {
            if(V->Type!=EJson::String||!SafeName(V->AsString())||!V->AsString().StartsWith(TEXT("/")))return Fail(S.R,TEXT("variable_default_invalid"),TEXT("Object defaults require an exact persistent asset path or JSON null; empty/None are not null aliases."));
            S.R.Data->SetBoolField(TEXT("native_resource_reference_load_attempted"),true);UObject* O=LoadObject<UObject>(nullptr,*V->AsString());
            S.R.Data->SetBoolField(TEXT("native_resource_reference_load_returned"),O!=nullptr);
            if(FMaterialVariableScope::Loaded(O))
            {
                Reference.Reset(O);ReferenceClass.Reset(O->GetClass());ReferencePackage.Reset(O->GetOutermost());ReferencePath=O->GetPathName();ReferenceFlags=O->GetFlags();ReferencePackageFlags=ReferencePackage->GetPackageFlags();ReferenceDirty=ReferencePackage->IsDirty();
                S.R.Data->SetStringField(TEXT("native_resource_reference_returned_path"),ReferencePath);
            }
            if(!Current())return false;if(!Reference.IsValid()||Reference->GetPathName()!=V->AsString())return Fail(S.R,TEXT("variable_reference_unsupported"),TEXT("The returned resource must be the exact requested loaded asset; no alias/default-object fallback."));
            if(S.TextureRenderTarget2D&&O->GetClass()!=UTextureRenderTarget2D::StaticClass())return Fail(S.R,TEXT("variable_default_invalid"),TEXT("TextureRenderTarget2D defaults require the exact declared native asset class; no subclass/runtime render-target fallback."));
            const bool Runtime=S.NiagaraSystem?NiagaraSystemReference(O):RuntimeReference(O,S.R);if(!Current())return false;if(!Runtime)return false;
            const bool Compatible=(S.NiagaraSystem||S.TextureRenderTarget2D)?O->GetClass()==S.Native.Get():O->IsA(S.Native.Get());if(!Current())return false;if(S.NiagaraSystem||S.TextureRenderTarget2D)S.R.Data->SetStringField(TEXT("native_resource_reference_actual_class_path"),ReferenceClass->GetPathName());if(!Compatible)return Fail(S.R,TEXT("variable_default_invalid"),TEXT("Resource default is not an instance of the exact declared native constraint."));
        }
        Property->SetObjectPropertyValue(Value,Reference.Get());if(!Current())return false;
        const bool Exported=FBlueprintEditorUtils::PropertyValueToString_Direct(Property,static_cast<uint8*>(Value),Stored,Owner.Get());if(!Current())return false;
        if(!Exported)return Fail(S.R,TEXT("variable_default_unsupported"),TEXT("Resource member-default export failed."));
        CheckValue=FMemory::Malloc(Size,Property->GetMinAlignment());Property->InitializeValue(CheckValue);if(!Current())return false;
        const bool Imported=FBlueprintEditorUtils::PropertyValueFromString_Direct(Property,Stored,static_cast<uint8*>(CheckValue),Owner.Get());if(!Current())return false;
        const bool Same=Imported&&Property->Identical(Value,CheckValue,PPF_None);if(!Current())return false;
        S.R.Data->SetBoolField(TEXT("native_resource_default_roundtrip_verified"),Same);return Same||Fail(S.R,TEXT("variable_default_roundtrip_unsupported"),TEXT("UE typed default text did not preserve this resource exactly; no target writer was dispatched."));
    }
};
FResult AddMaterialVariable(const FString& Path,const FString& Name,const FString& Type,const FString& TypeObject,bool bArray,const FString& DefaultJson,
    const FString& Category,const FString& Tooltip,const FString& Friendly,const FString& Guid,const FGuid& RequestedGuid,
    const TArray<TPair<FName,FString>>& Entries,bool bExposed,bool bEditable,bool bTransient,bool bSkeletalArray=false,bool bMapStringVector=false)
{
    const bool bMapStringDouble=Type==TEXT("map_string_double");bMapStringVector|=bMapStringDouble;
    FResult R;R.Data->SetStringField(TEXT("declaration_scope"),bMapStringDouble?TEXT("own_native_string_double_map"):bMapStringVector?TEXT("own_native_string_vector_map"):bSkeletalArray?TEXT("own_native_skeletal_component_array"):TEXT("own_native_material_value"));
    if(bMapStringVector)
    {
        if(bArray||!bTransient||!TypeObject.IsEmpty()){Fail(R,TEXT("variable_map_declaration_unsupported"),bMapStringDouble?TEXT("map_string_double requires is_array=false, is_transient=true and empty type_object; other Map/Set forms remain unsupported."):TEXT("map_string_vector requires is_array=false, is_transient=true and empty type_object; other Map/Set forms remain unsupported."));return R;}
        if(!DefaultJson.IsEmpty()){TSharedPtr<FJsonValue> Value;if(!Parse(DefaultJson,Value,R))return R;if(Value->Type!=EJson::Object||!Value->AsObject()->Values.IsEmpty()){Fail(R,TEXT("variable_map_default_unsupported"),TEXT("Only omitted or empty JSON object defaults are admitted; no Map default buffer/import or runtime value is created."));return R;}}
    }
    if(bSkeletalArray)
    {
        R.Data->SetBoolField(TEXT("native_skeletal_array_declaration"),true);R.Data->SetBoolField(TEXT("skeletal_array_nonempty_defaults_supported"),false);
        if(!bArray||!bTransient){Fail(R,TEXT("variable_skeletal_array_declaration_unsupported"),TEXT("Only an exact own SkeletalMeshComponent[] declaration with is_transient=true is admitted; scalar/non-transient declarations remain pending."));return R;}
        if(!DefaultJson.IsEmpty()){TSharedPtr<FJsonValue> Value;if(!Parse(DefaultJson,Value,R))return R;if(Value->Type!=EJson::Array||!Value->AsArray().IsEmpty()){Fail(R,TEXT("variable_skeletal_array_default_unsupported"),TEXT("Only omitted or empty JSON array defaults are admitted; object/null/nonempty values are not loaded or imported."));return R;}}
    }
    if(bArray&&!bSkeletalArray){Fail(R,TEXT("variable_material_container_unsupported"),TEXT("The material/texture extension admits only exact scalar native constraints, not object arrays."));return R;}
    if(!FMaterialVariableScope::Globals()){Fail(R,TEXT("variable_material_context_invalid"),TEXT("Idle Editor context required before native class or target lookup."));return R;}
    UBlueprint* BP=LoadBlueprint(Path,R);if(!BP||!MutationScope(BP,R))return R;
    FMaterialVariableScope S(R);if(bMapStringVector?!S.BeginMap(BP,Name,bMapStringDouble):!S.Begin(BP,Name,bSkeletalArray?FindObject<UClass>(nullptr,TEXT("/Script/Engine.SkeletalMeshComponent")):MaterialVariableClass(TypeObject),bSkeletalArray))return R;auto Before=S.Snapshot();R.Data->SetObjectField(TEXT("before"),Before);
    const FName VarName(*Name);if(BP->NewVariables.Num()+BP->GeneratedVariables.Num()>=MaxVariables){Fail(R,TEXT("variable_budget_exceeded"),TEXT("The 4096 descriptor budget would be exceeded."));return R;}
    TSet<FName> Existing;FBlueprintEditorUtils::GetClassVariableList(BP,Existing,true);if(!S.Current())return R;
    for(const auto& V:BP->NewVariables)Existing.Add(V.VarName);for(const auto& V:BP->GeneratedVariables)Existing.Add(V.VarName);
    if(Existing.Contains(VarName)){Fail(R,TEXT("variable_name_collision"),TEXT("A local or inherited member already uses this name; no automatic rename."));return R;}
    for(const auto& V:BP->NewVariables)if(!Guid.IsEmpty()&&V.VarGuid==RequestedGuid){Fail(R,TEXT("variable_guid_collision"),TEXT("The requested GUID is already used."));return R;}
    for(const auto& V:BP->GeneratedVariables)if(!Guid.IsEmpty()&&V.VarGuid==RequestedGuid){Fail(R,TEXT("variable_guid_collision"),TEXT("The requested GUID is already used."));return R;}
    FEdGraphPinType Pin;Pin.PinCategory=UEdGraphSchema_K2::PC_Object;Pin.PinSubCategoryObject=S.Native.Get();if(bSkeletalArray)Pin.ContainerType=EPinContainerType::Array;
    if(bMapStringVector){Pin.PinCategory=UEdGraphSchema_K2::PC_String;Pin.PinSubCategoryObject=nullptr;Pin.ContainerType=EPinContainerType::Map;Pin.PinValueType.TerminalCategory=bMapStringDouble?UEdGraphSchema_K2::PC_Real:UEdGraphSchema_K2::PC_Struct;Pin.PinValueType.TerminalSubCategory=bMapStringDouble?UEdGraphSchema_K2::PC_Double:NAME_None;Pin.PinValueType.TerminalSubCategoryObject=bMapStringDouble?nullptr:S.MapVector.Get();}
    FMaterialVariableDefault Typed(S);
    if(!bSkeletalArray&&!bMapStringVector&&(!Typed.Create(Pin)||(!DefaultJson.IsEmpty()&&!Typed.Prepare(DefaultJson))))return R;
    const bool InputValid=MutationScope(BP,R);if(!S.Current())return R;if(!InputValid)return R;
    Existing.Reset();FBlueprintEditorUtils::GetClassVariableList(BP,Existing,true);if(!S.Current())return R;
    for(const auto& V:BP->NewVariables)Existing.Add(V.VarName);for(const auto& V:BP->GeneratedVariables)Existing.Add(V.VarName);
    if(Existing.Contains(VarName)||BP->NewVariables.Num()+BP->GeneratedVariables.Num()>=MaxVariables){Fail(R,TEXT("variable_preflight_changed_blueprint"),TEXT("Name availability or budget changed before the writer."));return R;}
    R.Stage=TEXT("add");R.bMutationAttempted=true;R.Data->SetBoolField(TEXT("add_member_variable_attempted"),true);
    const bool Added=FBlueprintEditorUtils::AddMemberVariable(BP,VarName,Pin,Typed.Stored);R.Data->SetBoolField(TEXT("add_member_variable_returned"),Added);
    R.Data->SetBoolField(TEXT("creation_identity_observed"),false);R.Data->SetBoolField(TEXT("creation_observed"),false);R.Data->SetStringField(TEXT("creation_requested_name"),Name);
    // Preserve a safe returned creation prefix before the full source/unselected guard. No additional SDK is dispatched.
    if(S.TargetBase())
    {
        const FBPVariableDescription* Created=nullptr;int32 Count=0;for(const auto& Candidate:BP->NewVariables)if(Candidate.VarName==VarName){Created=&Candidate;++Count;}
        if(Count==1&&Created)
        {
            R.Data->SetBoolField(TEXT("creation_identity_observed"),true);R.Data->SetBoolField(TEXT("creation_observed"),true);R.Data->SetStringField(TEXT("created_variable_name"),Created->VarName.ToString());
            R.Data->SetStringField(TEXT("created_variable_guid"),Created->VarGuid.ToString(EGuidFormats::Digits));R.Data->SetBoolField(TEXT("created_type_matches_request"),MaterialPinSame(Created->VarType,Pin));
            R.Data->SetStringField(TEXT("created_native_constraint_requested_path"),S.NativePath);
        }
    }
    if(!Added){S.Stop(TEXT("variable_add_failed"),TEXT("AddMemberVariable returned false; its actual prefix is preserved and endpoint observation is incomplete."));return R;}
    if(!S.AcceptWriter(true,false,true))return R;FBPVariableDescription* V=S.Selected();
    R.Data->SetBoolField(TEXT("creation_observed"),true);R.Data->SetStringField(TEXT("created_variable_name"),V->VarName.ToString());R.Data->SetStringField(TEXT("created_variable_guid"),V->VarGuid.ToString(EGuidFormats::Digits));
    if(!MaterialPinSame(V->VarType,Pin)||!V->VarGuid.IsValid()||((bSkeletalArray||bMapStringVector||!DefaultJson.IsEmpty())&&V->DefaultValue!=Typed.Stored))
    {S.Observed(Before);Fail(R,TEXT("variable_add_postcondition_failed"),bSkeletalArray?TEXT("The created SkeletalMeshComponent array did not retain its exact type/GUID/empty default."):TEXT("The created resource declaration did not retain its exact scalar type/GUID/default."));return R;}
    const bool MetadataWork=!Category.IsEmpty()||!Tooltip.IsEmpty()||!Friendly.IsEmpty()||!Guid.IsEmpty()||!Entries.IsEmpty()||bExposed||bEditable||bTransient;
    int32 MetadataNotifications=0,CategoryReturns=0;
    const auto ExpectSelected=[&]()->FBPVariableDescription*{for(auto& E:S.Expected)if(E.VarName==VarName)return &E;return nullptr;};
    const auto MetadataReturned=[&](){++MetadataNotifications;R.Data->SetNumberField(TEXT("resource_metadata_helpers_returned"),MetadataNotifications);return S.AcceptWriter(true);};
    if(MetadataWork)
    {
        // Preserve public metadata dependency/reconstruction effects. Never keep a descriptor pointer across a helper callback.
        // These selected helpers are not suppressed: a failed callback does not require a follow-up UObject write to restore a suppress flag.
        if(!Category.IsEmpty())
        {
            const FText RequestedCategory=FText::FromString(Category);if(!S.Current())return R;
            const FText ExistingCategory=FBlueprintEditorUtils::GetBlueprintVariableCategory(BP,VarName,nullptr);if(!S.Current())return R;
            const bool CategoryNoOp=ExistingCategory.EqualTo(RequestedCategory);if(!S.Current())return R;
            R.Data->SetBoolField(TEXT("resource_metadata_category_helper_noop_expected"),CategoryNoOp);
            // The public helper retains the original descriptor FText on its EqualTo no-op branch.
            // Otherwise one shared FText is copied into both the expectation and the helper's authored value.
            if(!CategoryNoOp)ExpectSelected()->Category=RequestedCategory;
            FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,VarName,nullptr,RequestedCategory,true);CategoryReturns=1;R.Data->SetNumberField(TEXT("resource_metadata_category_helpers_returned"),CategoryReturns);if(!MetadataReturned())return R;
        }
        if(!Tooltip.IsEmpty())
        {ExpectSelected()->SetMetaData(TEXT("Tooltip"),Tooltip);FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,VarName,nullptr,TEXT("Tooltip"),Tooltip);if(!MetadataReturned())return R;}
        V=S.Selected();if(!Friendly.IsEmpty())V->FriendlyName=Friendly;if(!Guid.IsEmpty())V->VarGuid=RequestedGuid;S.Expected=BP->NewVariables;
        for(const auto& E:Entries)
        {ExpectSelected()->SetMetaData(E.Key,E.Value);FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,VarName,nullptr,E.Key,E.Value);if(!MetadataReturned())return R;}
        if(bExposed||bEditable)
        {ExpectSelected()->PropertyFlags&=~CPF_DisableEditOnInstance;FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,VarName,false);if(!MetadataReturned())return R;}
        if(bTransient)
        {ExpectSelected()->PropertyFlags|=CPF_Transient;FBlueprintEditorUtils::SetVariableTransientFlag(BP,VarName,true);if(!MetadataReturned())return R;}
        R.Data->SetBoolField(TEXT("descriptor_metadata_batch_applied"),true);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);R.Data->SetBoolField(TEXT("metadata_structural_notification_returned"),true);
        if(!S.AcceptWriter(true))return R;V=S.Selected();
    }
    R.Data->SetNumberField(TEXT("unsuppressed_structural_regeneration_requests"),1+MetadataNotifications-CategoryReturns+(MetadataWork?1:0));R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"),true);R.Data->SetBoolField(TEXT("explicit_full_compile_performed"),false);
    bool Valid=MaterialPinSame(V->VarType,Pin)&&V->VarGuid.IsValid()&&((!bSkeletalArray&&!bMapStringVector&&DefaultJson.IsEmpty())||V->DefaultValue==Typed.Stored)&&(Guid.IsEmpty()||V->VarGuid==RequestedGuid)
        &&(Category.IsEmpty()||V->Category.ToString()==Category)&&(Friendly.IsEmpty()||V->FriendlyName==Friendly)&&(!(bExposed||bEditable)||!(V->PropertyFlags&CPF_DisableEditOnInstance))&&!!(V->PropertyFlags&CPF_Transient)==bTransient;
    for(const auto& E:Entries){const int32 I=V->FindMetaDataEntryIndexForKey(E.Key);Valid=Valid&&I!=INDEX_NONE&&V->MetaDataArray[I].DataValue==E.Value;}
    const int32 Index=FBlueprintEditorUtils::FindNewVariableIndex(BP,VarName);R.Data->SetStringField(TEXT("variable_name"),Name);R.Data->SetStringField(TEXT("variable_type"),Type);R.Data->SetStringField(TEXT("default_value"),V->DefaultValue);R.Data->SetStringField(TEXT("stored_default"),V->DefaultValue);
    R.Data->SetObjectField(TEXT("pin_type"),PinJson(V->VarType));R.Data->SetStringField(TEXT("variable_guid"),V->VarGuid.ToString(EGuidFormats::Digits));R.Data->SetObjectField(TEXT("verified_variable"),MaterialDeclarationRow(*V,Index));R.Data->SetBoolField(TEXT("requested_is_transient"),bTransient);
    S.Observed(Before);if(S.Stopped)return R;if(!Valid){Fail(R,TEXT("variable_add_postcondition_failed"),TEXT("Created declaration metadata/type/default/flags did not retain the request; no rollback."));R.Stage=TEXT("verification");return R;}
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
bool MaterialGeneratedDefault(FMaterialVariableScope& S,FMaterialVariableDefault& Typed,bool& Same)
{
    Same=false;if(!Typed.Current())return false;UClass* C=S.Generated.Get();const auto Handle=MaterialVariableCDOHandle(C);
    if(!Handle.IsResolved())return S.Stop(TEXT("variable_material_generated_cdo_unavailable"),TEXT("Generated CDO handle must already be resolved; no hidden reference load or creation."));
    const UObject* CDO=Handle==nullptr?nullptr:FindObject<UObject>(nullptr,*Handle.GetPathName());
    if(!Typed.Current())return false;if(MaterialVariableCDOHandle(C)!=Handle||Handle!=CDO||(Handle!=nullptr&&!CDO))return S.Stop(TEXT("variable_material_generated_cdo_changed"),TEXT("Generated optional CDO lookup differs from the current shallow handle."));if(!FMaterialVariableScope::Loaded(CDO)||CDO->GetClass()!=C||!CDO->HasAnyFlags(RF_ClassDefaultObject))return true;
    TStrongObjectPtr<const UObject> Hold(CDO);FProperty* P=C->FindPropertyByName(S.Name);FObjectProperty* O=CastField<FObjectProperty>(P);
    if(!O||O->GetClass()!=FObjectProperty::StaticClass()||O->GetOwnerClass()!=C||O->ArrayDim!=1||!O->PropertyClass.IsResolved()||O->PropertyClass!=S.Native.Get())return true;
    const FString CDOPath=CDO->GetPathName();const EObjectFlags CDOFlags=CDO->GetFlags();UObject* CDOOuter=CDO->GetOuter();TStrongObjectPtr<UObject> KeepOuter(CDOOuter);
    const EPropertyFlags PropertyFlags=O->GetPropertyFlags();const int32 PropertyOffset=O->GetOffset_ForInternal(),PropertySize=O->GetSize();
    const UObject* Actual=O->GetObjectPropertyValue(O->ContainerPtrToValuePtr<void>(CDO));S.R.Data->SetBoolField(TEXT("native_resource_generated_default_read_returned"),true);
    if(!Typed.Current())return false;
    const auto CurrentCDO=MaterialVariableCDOHandle(C);
    if(!CurrentCDO.IsResolved()||CurrentCDO!=Handle||CurrentCDO!=CDO||!FMaterialVariableScope::Loaded(CDO)||!FMaterialVariableScope::Loaded(CDOOuter)
        ||CDO->GetClass()!=C||CDO->GetOuter()!=CDOOuter||CDO->GetPathName()!=CDOPath||CDO->GetFlags()!=CDOFlags||C->FindPropertyByName(S.Name)!=P)
        return S.Stop(TEXT("variable_material_generated_cdo_changed"),TEXT("Selected held CDO identity/metadata/current handle or property changed during tracked value read; the returned retired value is not accepted."));
    if(O->GetClass()!=FObjectProperty::StaticClass()||O->GetOwnerClass()!=C||O->ArrayDim!=1||!O->PropertyClass.IsResolved()||O->PropertyClass!=S.Native.Get()
        ||O->GetPropertyFlags()!=PropertyFlags||O->GetOffset_ForInternal()!=PropertyOffset||O->GetSize()!=PropertySize)
        return S.Stop(TEXT("variable_material_generated_property_changed"),TEXT("Selected generated default property schema changed during tracked object read."));
    S.R.Data->SetBoolField(TEXT("native_resource_generated_cdo_current_after_read_verified"),true);
    Same=Actual==Typed.Reference.Get();S.R.Data->SetBoolField(TEXT("native_resource_generated_default_observed"),true);S.R.Data->SetBoolField(TEXT("native_resource_generated_default_matches"),Same);return true;
}
FResult SetMaterialVariableDefault(UBlueprint* BP,const FString& Name,const FString& Json,bool bCompile,bool bSave,bool bDryRun,FResult R)
{
    R.Data->SetStringField(TEXT("declaration_scope"),TEXT("own_native_material_value"));int32 Index=INDEX_NONE;FBPVariableDescription* V=FindLocal(BP,Name,Index,R);if(!V)return R;
    const FBPVariableDescription Original=*V;UClass* Native=Cast<UClass>(Original.VarType.PinSubCategoryObject.Get());
    if(Original.VarType.IsContainer()||Original.VarType.PinCategory!=UEdGraphSchema_K2::PC_Object||!Native||Native!=MaterialVariableClass(Native->GetPathName())
        ||Original.VarType.bIsReference||Original.VarType.bIsConst||Original.VarType.bIsWeakPointer||!Original.VarGuid.IsValid())
    {Fail(R,TEXT("variable_material_type_unsupported"),TEXT("Only an exact own scalar native resource member is supported by this default writer."));return R;}
    FMaterialVariableScope S(R);if(!S.Begin(BP,Name,Native))return R;R.Data->SetBoolField(TEXT("niagara_system_reference_only"),S.NiagaraSystem);R.Data->SetBoolField(TEXT("texture_render_target_2d_reference_only"),S.TextureRenderTarget2D);auto Before=S.Snapshot();R.Data->SetObjectField(TEXT("before"),Before);FMaterialVariableDefault Typed(S);
    if(bCompile)
    {
        TArray<TSharedPtr<FJsonValue>> OtherPending;
        for(const auto& D:BP->NewVariables)if(D.VarName!=S.Name&&!D.DefaultValue.IsEmpty())OtherPending.Add(MakeShared<FJsonValueString>(TEXT("new:")+D.VarName.ToString()));
        for(const auto& D:BP->GeneratedVariables)if(!D.DefaultValue.IsEmpty())OtherPending.Add(MakeShared<FJsonValueString>(TEXT("generated:")+D.VarName.ToString()));
        R.Data->SetArrayField(TEXT("other_pending_default_descriptors"),OtherPending);R.Data->SetNumberField(TEXT("other_pending_default_count"),OtherPending.Num());
        if(!OtherPending.IsEmpty())
        {
            R.Data->SetStringField(TEXT("pending_default_resolution"),TEXT("This finite resource writer does not sample or consume other pending defaults. Review and compile those declarations in a separate explicit operation, or use compile=false and one separately validated shared compile. No automatic retry or default clearing is performed."));
            S.Observed(Before);if(S.Stopped)return R;Fail(R,TEXT("variable_material_compile_pending_others"),TEXT("Full compile also consumes other pending own/generated defaults. This combination is unsupported before any Modify/Compile; inspect the listed descriptor names."));return R;
        }
    }
    if(!Typed.Create(Original.VarType)||!Typed.Prepare(Json))return R;const bool InputValid=MutationScope(BP,R);if(!S.Current())return R;if(!InputValid)return R;
    V=S.Selected();const FString Previous=V->DefaultValue;bool Matches=false;
    if(!Previous.IsEmpty())Matches=Previous==Typed.Stored;else if(!MaterialGeneratedDefault(S,Typed,Matches))return R;
    const bool WouldChange=!Matches;R.Data->SetStringField(TEXT("previous_stored_default"),Previous);R.Data->SetStringField(TEXT("requested_stored_default"),Typed.Stored);R.Data->SetBoolField(TEXT("dry_run"),bDryRun);R.Data->SetBoolField(TEXT("would_change"),WouldChange);
    if(WouldChange&&Typed.Stored.IsEmpty()){Fail(R,TEXT("variable_empty_default_unsupported"),TEXT("An empty pending descriptor cannot clear a compiled default."));return R;}
    if(!bCompile&&bSave&&BP->CachedUDSDependencies.Num()>0&&(WouldChange||BP->Status!=BS_UpToDate))
    {Fail(R,TEXT("variable_save_requires_compile"),TEXT("Saving with cached UDS dependencies may compile implicitly; request compile=true before save."));return R;}
    if(bDryRun){S.Observed(Before);if(S.Stopped)return R;R.Data->SetStringField(TEXT("stored_default_after"),Previous);R.Data->SetObjectField(TEXT("verified_variable"),MaterialDeclarationRow(*S.Selected(),Index));R.Data->SetBoolField(TEXT("compiled"),false);R.Data->SetBoolField(TEXT("saved"),false);R.Data->SetStringField(TEXT("compile_status"),TEXT("not_attempted"));R.bSuccess=R.bPostconditionPassed=!R.bChanged;R.Stage=R.bSuccess?TEXT("complete"):TEXT("verification");return R;}
    if(WouldChange)
    {
        R.Stage=TEXT("set_default");R.bMutationAttempted=true;const bool Modified=BP->Modify();R.Data->SetBoolField(TEXT("target_modify_returned"),Modified);if(!S.AcceptWriter())return R;
        V=S.Selected();V->DefaultValue=Typed.Stored;S.Expected=BP->NewVariables;R.Data->SetBoolField(TEXT("stored_descriptor_write_performed"),true);
        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);R.Data->SetBoolField(TEXT("mark_modified_returned"),true);if(!S.AcceptWriter())return R;
        const bool Marked=BP->MarkPackageDirty();R.Data->SetBoolField(TEXT("mark_package_dirty_returned"),Marked);if(!S.AcceptWriter())return R;
    }
    bool CompiledDefaultVerified=false;
    if(bCompile)
    {
        R.Stage=TEXT("compile");R.bCompileAttempted=R.bMutationAttempted=true;auto C=CompileBlueprint(S.Path,false,false);R.Data->SetObjectField(TEXT("compile"),C);bool Compiled=false;C->TryGetBoolField(TEXT("compiled"),Compiled);R.Compiled=Compiled;
        // SDK Full compile consumes a selected pending DefaultValue after applying it to the generated CDO.
        // Adopt only that exact stored-text -> empty transition; GUID/type/metadata/flags and all other declarations remain frozen.
        if(Compiled)
        {
            if(!S.Source())return R;FBPVariableDescription* Actual=S.Selected();
            for(auto& Expected:S.Expected)if(Expected.VarName==S.Name)
            {
                FBPVariableDescription Consumed=Expected;Consumed.DefaultValue.Empty();
                const bool ConsumedExactly=Actual&&!Typed.Stored.IsEmpty()&&Expected.DefaultValue==Typed.Stored&&Actual->DefaultValue.IsEmpty()&&MaterialDescriptionSame(Consumed,*Actual);
                R.Data->SetBoolField(TEXT("native_resource_selected_default_consumed_by_compile"),ConsumedExactly);
                if(ConsumedExactly)Expected.DefaultValue.Empty();break;
            }
        }
        if(!S.AcceptWriter())return R;if(!Compiled){S.Observed(Before);Fail(R,TEXT("variable_compile_failed"),TEXT("Resource default changed but compilation failed; save was not attempted."));return R;}
        bool Exact=false;if(!MaterialGeneratedDefault(S,Typed,Exact))return R;if(!Exact){S.Observed(Before);Fail(R,TEXT("variable_default_postcondition_failed"),TEXT("Selected compiled CDO did not retain the exact requested resource/null default."));return R;}CompiledDefaultVerified=true;
    }
    if(!S.Current())return R;V=S.Selected();if(V->DefaultValue!=Typed.Stored&&!Matches&&!CompiledDefaultVerified){S.Observed(Before);Fail(R,TEXT("variable_default_postcondition_failed"),TEXT("Pending resource default differs after the writer."));return R;}
    if(bSave)
    {
        R.Stage=TEXT("save");R.bSaveAttempted=R.bMutationAttempted=true;R.bSaved=UEditorAssetLibrary::SaveLoadedAsset(BP,false);R.Data->SetBoolField(TEXT("save_returned"),R.bSaved);
        if(!S.AcceptWriter(false,true))return R;if(!R.bSaved){S.Observed(Before);Fail(R,TEXT("variable_save_failed"),TEXT("Readback passed but save failed; inspect actual dirty state before retry."));return R;}
    }
    S.Observed(Before);if(S.Stopped)return R;V=S.Selected();R.Data->SetStringField(TEXT("stored_default_after"),V->DefaultValue);R.Data->SetObjectField(TEXT("verified_variable"),MaterialDeclarationRow(*V,Index));
    R.Data->SetBoolField(TEXT("compiled"),R.Compiled.Get(false));R.Data->SetBoolField(TEXT("saved"),R.bSaved);R.Data->SetStringField(TEXT("compile_status"),!R.bCompileAttempted?TEXT("not_attempted"):R.Compiled.Get(false)?TEXT("passed"):TEXT("failed"));
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}

}

FResult Inspect(const FString& Path, const FString& Name, bool bMetadata)
{
    FResult R; R.Stage = TEXT("read"); UBlueprint* BP = LoadBlueprint(Path, R); if (!BP) { return R; }
    const auto Before = Capture(BP); R.Data->SetObjectField(TEXT("before"), Before);
    if (!bMetadata) { Merge(*R.Data, *Before); }
    else
    {
        if (!SafeName(Name)) { Fail(R, TEXT("variable_name_invalid"), TEXT("Variable selector must be bounded and nonempty.")); return R; }
        const FBPVariableDescription* Found = nullptr; FString Scope; int32 Index = INDEX_NONE;
        auto Search = [&](const TArray<FBPVariableDescription>& Variables, const TCHAR* Label)
        { for (int32 I = 0; I < Variables.Num(); ++I) { if (Variables[I].VarName.ToString().Equals(Name, ESearchCase::IgnoreCase)) { if (Found) { return false; } Found = &Variables[I]; Scope = Label; Index = I; } } return true; };
        if (!Search(BP->NewVariables, TEXT("new")) || !Search(BP->GeneratedVariables, TEXT("generated"))) { Fail(R, TEXT("variable_name_ambiguous"), TEXT("Variable occurs in multiple descriptor scopes.")); return R; }
        if (!Found) { Fail(R, TEXT("variable_not_found"), TEXT("Variable metadata descriptor was not found.")); return R; }
        R.Data->SetStringField(TEXT("scope"), Scope); R.Data->SetNumberField(TEXT("index"), Index); R.Data->SetStringField(TEXT("guid"), Found->VarGuid.ToString(EGuidFormats::Digits));
        R.Data->SetStringField(TEXT("variable_name"), Found->VarName.ToString()); R.Data->SetObjectField(TEXT("metadata"), Metadata(*Found));
        TArray<TSharedPtr<FJsonValue>> Entries;
        for (const auto& E : Found->MetaDataArray) { auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("index"), Entries.Num()); J->SetStringField(TEXT("key"), E.DataKey.ToString()); J->SetStringField(TEXT("value"), E.DataValue); Entries.Add(MakeShared<FJsonValueObject>(J)); }
        R.Data->SetArrayField(TEXT("metadata_entries"), Entries); R.Data->SetNumberField(TEXT("metadata_count"), Entries.Num()); R.Data->SetNumberField(TEXT("metadata_entry_count"), Entries.Num());
        R.Data->SetStringField(TEXT("blueprint_name"), BP->GetName()); R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName());
    }
    R.Data->SetBoolField(TEXT("read_only"), true); ObserveAfter(BP, Before, R);
    R.bSuccess = R.bPostconditionPassed = !R.bChanged; R.Stage = R.bSuccess ? TEXT("complete") : TEXT("verification");
    if (!R.bSuccess) { Fail(R, TEXT("variable_read_changed_state"), TEXT("Local variable/dirty/status observations changed during inspection.")); } return R;
}

FResult Add(const FString& Path, const FString& Name, const FString& Type, bool bExposed, const FString& DefaultJson,
    const FString& Category, const FString& Tooltip, const FString& Friendly, const FString& TypeObject, bool bArray,
    const FString& MetadataJson, const FString& Guid, bool bEditable, bool bTransient)
{
    FResult R;
    if (!Identifier(Name) || !SafeName(Category, true) || !SafeName(Friendly, true) || !SafeText(Tooltip, 4096) || !SafeName(Type))
    { Fail(R, TEXT("variable_name_invalid"), TEXT("New variable names require an ASCII identifier of 1..100 characters; category/friendly name <=1023 and tooltip <=4096.")); return R; }
    FGuid RequestedGuid;
    if (!Guid.IsEmpty() && (!FGuid::Parse(Guid, RequestedGuid) || !RequestedGuid.IsValid())) { Fail(R, TEXT("variable_guid_invalid"), TEXT("variable_guid must be a valid nonzero GUID.")); return R; }
    TSharedPtr<FJsonValue> MetaJson;
    if (!Parse(MetadataJson.IsEmpty() ? TEXT("{}") : MetadataJson, MetaJson, R)) { return R; }
    if (MetaJson->Type != EJson::Object || MetaJson->AsObject()->Values.Num() > 64) { Fail(R, TEXT("variable_metadata_invalid"), TEXT("metadata_json requires an object with at most 64 scalar entries.")); return R; }
    TArray<TPair<FName, FString>> Entries; TSet<FName> MetaNames;
    for (const auto& Pair : MetaJson->AsObject()->Values)
    {
        const FString Key(Pair.Key.ToView()); const FName MetaName(*Key);
        if (MetaName.IsNone() || MetaNames.Contains(MetaName)) { Fail(R, TEXT("variable_metadata_invalid"), TEXT("Metadata keys must be non-None and unique ignoring case.")); return R; }
        FString Value;
        if (Pair.Value->Type == EJson::String) { Value = Pair.Value->AsString(); }
        else if (Pair.Value->Type == EJson::Boolean) { Value = Pair.Value->AsBool() ? TEXT("true") : TEXT("false"); }
        else if (Pair.Value->Type == EJson::Number) { Value = FString::SanitizeFloat(Pair.Value->AsNumber()); }
        else { Fail(R, TEXT("variable_metadata_invalid"), TEXT("Metadata supports only string, boolean or numeric scalar values.")); return R; }
        Entries.Emplace(MetaName, Value); MetaNames.Add(MetaName);
    }
    if(Type==TEXT("map_string_vector")||Type==TEXT("map_string_double"))return AddMaterialVariable(Path,Name,Type,TypeObject,bArray,DefaultJson,Category,Tooltip,Friendly,Guid,RequestedGuid,Entries,bExposed,bEditable,bTransient,false,true);
    if(SkeletalArrayVariableRequest(Type,TypeObject))return AddMaterialVariable(Path,Name,Type,TypeObject,bArray,DefaultJson,Category,Tooltip,Friendly,Guid,RequestedGuid,Entries,bExposed,bEditable,bTransient,true);
    if(MaterialVariableRequest(Type,TypeObject))return AddMaterialVariable(Path,Name,Type,TypeObject,bArray,DefaultJson,Category,Tooltip,Friendly,Guid,RequestedGuid,Entries,bExposed,bEditable,bTransient);
    UBlueprint* BP = LoadBlueprint(Path, R); if (!BP || !MutationScope(BP, R)) { return R; }
    const auto Before = Capture(BP); R.Data->SetObjectField(TEXT("before"), Before);
    if (BP->NewVariables.Num() + BP->GeneratedVariables.Num() >= MaxVariables) { Fail(R, TEXT("variable_budget_exceeded"), TEXT("Adding a variable would exceed the 4096 descriptor budget.")); return R; }
    const FName VarName(*Name); TSet<FName> Existing; FBlueprintEditorUtils::GetClassVariableList(BP, Existing, true);
    for (const auto& V : BP->NewVariables) { Existing.Add(V.VarName); }
    for (const auto& V : BP->GeneratedVariables) { Existing.Add(V.VarName); }
    if (Existing.Contains(VarName)) { Fail(R, TEXT("variable_name_collision"), TEXT("A local or inherited member already uses this variable name; no automatic rename is performed.")); return R; }
    for (const auto& V : BP->NewVariables) { if (!Guid.IsEmpty() && V.VarGuid == RequestedGuid) { Fail(R, TEXT("variable_guid_collision"), TEXT("variable_guid is already in use.")); return R; } }
    for (const auto& V : BP->GeneratedVariables) { if (!Guid.IsEmpty() && V.VarGuid == RequestedGuid) { Fail(R, TEXT("variable_guid_collision"), TEXT("variable_guid is already in use.")); return R; } }
    FEdGraphPinType Pin; if (!PinType(Type, TypeObject, bArray, Pin, R)) { ObserveAfter(BP, Before, R); return R; }
    FTypedDefault Typed; if (!ValidateExistingPin(Pin, R) || !Typed.Create(Pin, BP, R) || (!DefaultJson.IsEmpty() && !PrepareDefault(Typed, DefaultJson, R))) { ObserveAfter(BP, Before, R); return R; }
    if (!MutationScope(BP, R)) { ObserveAfter(BP, Before, R); return R; }
    Existing.Reset(); FBlueprintEditorUtils::GetClassVariableList(BP, Existing, true);
    for (const auto& V : BP->NewVariables) { Existing.Add(V.VarName); }
    for (const auto& V : BP->GeneratedVariables) { Existing.Add(V.VarName); }
    if (Existing.Contains(VarName) || BP->NewVariables.Num() + BP->GeneratedVariables.Num() >= MaxVariables)
    { Fail(R, TEXT("variable_preflight_changed_blueprint"), TEXT("Name availability or descriptor budget changed while resolving input types/defaults.")); ObserveAfter(BP, Before, R); return R; }
    R.Stage = TEXT("add"); R.bMutationAttempted = true;
    if (!FBlueprintEditorUtils::AddMemberVariable(BP, VarName, Pin, Typed.Stored))
    { ObserveAfter(BP, Before, R); Fail(R, TEXT("variable_add_failed"), TEXT("AddMemberVariable returned false; inspect actual before/after state before retry.")); return R; }
    int32 Index = INDEX_NONE; FBPVariableDescription* Variable = FindLocal(BP, Name, Index, R);
    if (!Variable) { ObserveAfter(BP, Before, R); return R; }
    const bool MetadataWork = !Category.IsEmpty() || !Tooltip.IsEmpty() || !Friendly.IsEmpty() || !Guid.IsEmpty() || !Entries.IsEmpty() || bExposed || bEditable || bTransient;
    if (MetadataWork)
    {
        // Blueprint.h explicitly permits this scope only when followed by MarkStructural.
        // No compile, save, asynchronous task or early return occurs within the scope.
        {
        FMetadataSuppressionScope Suppress(BP);
        if (!Category.IsEmpty()) { Variable->Category = FText::FromString(Category); FBlueprintEditorUtils::SetBlueprintVariableCategory(BP, VarName, nullptr, FText::FromString(Category), true); }
        if (!Tooltip.IsEmpty()) { FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, VarName, nullptr, TEXT("Tooltip"), Tooltip); }
        if (!Friendly.IsEmpty()) { Variable->FriendlyName = Friendly; }
        if (!Guid.IsEmpty()) { Variable->VarGuid = RequestedGuid; }
        for (const auto& E : Entries) { FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, VarName, nullptr, E.Key, E.Value); }
        if (bExposed || bEditable) { FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, VarName, false); }
        if (bTransient) { FBlueprintEditorUtils::SetVariableTransientFlag(BP, VarName, true); }
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }
    R.Data->SetNumberField(TEXT("unsuppressed_structural_regeneration_requests"), MetadataWork ? 2 : 1);
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), true); R.Data->SetBoolField(TEXT("explicit_full_compile_performed"), false);
    Variable = FindLocal(BP, Name, Index, R); ObserveAfter(BP, Before, R);
    if (!Variable) { return R; }
    R.Data->SetStringField(TEXT("variable_name"), Name); R.Data->SetStringField(TEXT("variable_type"), Type);
    R.Data->SetStringField(TEXT("default_value"), Variable->DefaultValue); R.Data->SetStringField(TEXT("stored_default"), Variable->DefaultValue);
    R.Data->SetObjectField(TEXT("pin_type"), PinJson(Variable->VarType)); R.Data->SetStringField(TEXT("variable_guid"), Variable->VarGuid.ToString(EGuidFormats::Digits));
    R.Data->SetObjectField(TEXT("verified_variable"), Row(BP, *Variable, TEXT("new"), Index));
    R.Data->SetBoolField(TEXT("requested_is_transient"), bTransient);
    bool Valid = Variable->VarType == Pin && (DefaultJson.IsEmpty() || Variable->DefaultValue == Typed.Stored)
        && (Guid.IsEmpty() || Variable->VarGuid == RequestedGuid) && (Category.IsEmpty() || Variable->Category.ToString() == Category)
        && (Friendly.IsEmpty() || Variable->FriendlyName == Friendly) && (!(bExposed || bEditable) || !(Variable->PropertyFlags & CPF_DisableEditOnInstance))
        && (!!(Variable->PropertyFlags & CPF_Transient) == bTransient);
    for (const auto& E : Entries) { const int32 I = Variable->FindMetaDataEntryIndexForKey(E.Key); Valid &= I != INDEX_NONE && Variable->MetaDataArray[I].DataValue == E.Value; }
    if (!Valid) { Fail(R, TEXT("variable_add_postcondition_failed"), TEXT("Created variable did not retain its requested descriptor metadata/type/default; no rollback is claimed.")); R.Stage = TEXT("verification"); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}

FResult SetDefault(const FString& Path, const FString& Name, const FString& Json, bool bCompile, bool bSave, bool bDryRun)
{
    FResult R; R.bCompileRequested = bCompile; R.bSaveRequested = bSave;
    UBlueprint* BP = LoadBlueprint(Path, R); if (!BP || !MutationScope(BP, R)) { return R; }
    if(SafeName(Name))for(const auto& Own:BP->NewVariables)if(Own.VarName.ToString().Equals(Name,ESearchCase::IgnoreCase)&&Own.VarType.PinCategory==UEdGraphSchema_K2::PC_Object)
    {const UObject* T=Own.VarType.PinSubCategoryObject.Get();if(T&&(T==UMaterialInterface::StaticClass()||T==UMaterialInstanceDynamic::StaticClass()||T==UTexture::StaticClass()||T==UNiagaraSystem::StaticClass()||T==UTextureRenderTarget2D::StaticClass()))return SetMaterialVariableDefault(BP,Name,Json,bCompile,bSave,bDryRun,R);}

    const auto Before = Capture(BP); R.Data->SetObjectField(TEXT("before"), Before); int32 Index = INDEX_NONE;
    FBPVariableDescription* V = FindLocal(BP, Name, Index, R); if (!V) { return R; }
    const FBPVariableDescription OriginalVariable = *V;
    FTypedDefault Typed; if (!ValidateExistingPin(OriginalVariable.VarType, R) || !Typed.Create(OriginalVariable.VarType, BP, R) || !PrepareDefault(Typed, Json, R)) { ObserveAfter(BP, Before, R); return R; }
    if (!MutationScope(BP, R)) { ObserveAfter(BP, Before, R); return R; }
    V = FindLocal(BP, Name, Index, R);
    if (!V || V->VarGuid != OriginalVariable.VarGuid || V->VarType != OriginalVariable.VarType || V->DefaultValue != OriginalVariable.DefaultValue)
    { Fail(R, TEXT("variable_preflight_changed_member"), TEXT("Member identity/type/default changed while resolving input references; no variable write was dispatched.")); ObserveAfter(BP, Before, R); return R; }
    const FString Previous = V->DefaultValue; const bool WouldChange = !CurrentMatches(BP, *V, Typed);
    R.Data->SetStringField(TEXT("previous_stored_default"), Previous); R.Data->SetStringField(TEXT("requested_stored_default"), Typed.Stored);
    R.Data->SetBoolField(TEXT("dry_run"), bDryRun); R.Data->SetBoolField(TEXT("would_change"), WouldChange);
    // KismetCompiler treats an empty descriptor string as "no pending default".
    // Writing it cannot clear a nonempty existing string CDO, including in dry-run.
    if (WouldChange && Typed.Stored.IsEmpty())
    { Fail(R, TEXT("variable_empty_default_unsupported"), TEXT("Clearing an existing nonempty string default needs a separate CDO edit path; the compiler ignores an empty pending descriptor. No variable write was dispatched.")); ObserveAfter(BP, Before, R); return R; }
    // UBlueprint::PreSave may recompile stale UDS dependencies without SkipSave.
    // Reject the known risky compile=false path before marking this asset dirty.
    if (!bCompile && bSave && BP->CachedUDSDependencies.Num() > 0 && (WouldChange || BP->Status != BS_UpToDate))
    { Fail(R, TEXT("variable_save_requires_compile"), TEXT("Saving a dirty Blueprint with cached user-defined-struct dependencies may implicitly compile in PreSave. Request compile=true before save; no variable write was dispatched.")); ObserveAfter(BP, Before, R); return R; }
    if (bDryRun)
    {
        R.Data->SetStringField(TEXT("stored_default_after"), Previous); R.Data->SetObjectField(TEXT("verified_variable"), Row(BP, *V, TEXT("new"), Index));
        R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetStringField(TEXT("compile_status"), TEXT("not_attempted"));
        ObserveAfter(BP, Before, R); R.bSuccess = R.bPostconditionPassed = !R.bChanged; R.Stage = R.bSuccess ? TEXT("complete") : TEXT("verification");
        if (!R.bSuccess) { Fail(R, TEXT("variable_dry_run_observed_change"), TEXT("No explicit mutation was dispatched, but local observations changed during dry-run preflight.")); } return R;
    }
    if (WouldChange)
    { R.Stage = TEXT("set_default"); R.bMutationAttempted = true; BP->Modify(); V->DefaultValue = Typed.Stored; FBlueprintEditorUtils::MarkBlueprintAsModified(BP); BP->MarkPackageDirty(); }
    const bool Finished = FinishRequested(BP, Name, Typed, R);
    ObserveAfter(BP, Before, R); V = FindLocal(BP, Name, Index, R);
    if (V) { R.Data->SetStringField(TEXT("stored_default_after"), V->DefaultValue); R.Data->SetObjectField(TEXT("verified_variable"), Row(BP, *V, TEXT("new"), Index)); }
    R.Data->SetBoolField(TEXT("compiled"), R.Compiled.Get(false)); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
    R.Data->SetStringField(TEXT("compile_status"), !R.bCompileAttempted ? TEXT("not_attempted") : R.Compiled.Get(false) ? TEXT("passed") : TEXT("failed"));
    if (!Finished || !V) { return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}

// BZ: original local-variable metadata/order maintenance. Existing variable bodies above remain byte-exact.
#include "GameFramework/Actor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectGlobals.h"
namespace Cubeless::UE58::Variable
{
namespace
{
FString DeclarationEnumText(const TSharedRef<FJsonObject>& J)
{ FString S;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S));return S; }
bool DeclarationEnumGlobals()
{ return IsEditorGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage(); }
bool DeclarationEnumLoaded(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool DeclarationEnumEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!DeclarationEnumEqual(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& Pair:X->Values) { const auto* V=Y->Values.Find(Pair.Key);if(!V||!DeclarationEnumEqual(Pair.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
struct FDeclarationEnumScope
{
    FResult* Reply=nullptr;bool Active=false,Stopped=false,LoadEntered=false;
    TStrongObjectPtr<UEnum> Source;TStrongObjectPtr<UPackage> SourcePackage;
    // JM: a concrete native Actor/Object descriptor reuses the existing local
    // declaration admission/structural-return boundary. The enum route is unchanged.
    bool NativeClassMode=false;
    bool PresetMapLibrary=false;bool LocalMapMode=false; // MC: exact local String -> double descriptor only.
    // JQ: exact built-in value structures reuse the captured declaration boundary.
    bool NativeStructMode=false;TSharedPtr<FLocalScalarUdsSource> ScalarUds;TStrongObjectPtr<UScriptStruct> NativeStructSource;
    EStructFlags NativeStructFlags=STRUCT_NoFlags;int32 NativeStructSize=0,NativeStructAlignment=0;UStruct* NativeStructSuper=nullptr;
    bool NativeTransformMode=false;
    bool TransformSourceStop(const TCHAR* Why){return Bad(TEXT("local_transform_source_changed"),Why);}
    // OR finite Transform -> Quat/Vector source roles. No values or default buffers.
    struct FTransformSourceField{FProperty* Address=nullptr;FFieldClass* Kind=nullptr;FName FieldName;uint64 PropertyFlags=0;int32 Offset=0,Size=0,Dim=0,Alignment=0;UScriptStruct* Binding=nullptr;FString BindingPath;TMap<FName,FString> Metadata;};
    struct FTransformSourcePart{TStrongObjectPtr<UScriptStruct> Type;FString Path;EObjectFlags ObjectFlags=RF_NoFlags;EStructFlags StructFlags=STRUCT_NoFlags;int32 Size=0,Alignment=0;UStruct* Super=nullptr;TArray<FTransformSourceField> Fields;TMap<FName,FString> Metadata;};
    TArray<FTransformSourcePart> TransformSourceParts;bool TransformSourceReady=false,TransformSourceMetadataReady=false;
    static bool TransformSourceMapSame(const TMap<FName,FString>& A,const TMap<FName,FString>& B)
    {if(A.Num()!=B.Num())return false;for(const auto& Pair:A){const FString* V=B.Find(Pair.Key);if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return false;}return true;}
    bool TransformSourceCurrent()
    {
        if(!TransformSourceReady)return true;
        if(!Context()||TransformSourceParts.Num()!=3)return TransformSourceStop(TEXT("Current finite Transform source context/inventory differs."));
        for(const auto& Part:TransformSourceParts)
        {
            const auto* SourceType=Part.Type.Get();
            if(!DeclarationEnumLoaded(SourceType)||SourceType->GetClass()!=UScriptStruct::StaticClass()||SourceType->GetOuter()!=SourcePackage.Get()||SourceType->GetPackage()!=SourcePackage.Get()||!SourceType->GetPathName().Equals(Part.Path,ESearchCase::CaseSensitive)||SourceType->GetFlags()!=Part.ObjectFlags||SourceType->StructFlags!=Part.StructFlags||SourceType->GetStructureSize()!=Part.Size||SourceType->GetMinAlignment()!=Part.Alignment||SourceType->GetSuperStruct()!=Part.Super)return TransformSourceStop(TEXT("Current native Transform/Quat/Vector source identity/layout differs."));
            int32 FieldIndex=0;for(TFieldIterator<FProperty> It(SourceType,EFieldIterationFlags::None);It;++It,++FieldIndex)
            {
                if(!Part.Fields.IsValidIndex(FieldIndex)||*It!=Part.Fields[FieldIndex].Address)return TransformSourceStop(TEXT("Current native Transform source field address inventory differs; no retained field read."));
                const auto& Field=Part.Fields[FieldIndex];const auto* Physical=Field.Address;
                if(Physical->GetClass()!=Field.Kind||Physical->GetOwnerVariant()!=FFieldVariant(SourceType)||Physical->GetFName()!=Field.FieldName||uint64(Physical->GetPropertyFlags())!=Field.PropertyFlags||Physical->ArrayDim!=Field.Dim||Physical->GetOffset_ForInternal()!=Field.Offset||Physical->GetSize()!=Field.Size||Physical->GetMinAlignment()!=Field.Alignment||Physical->HasSetterOrGetter())return TransformSourceStop(TEXT("Current Transform source direct field owner/layout/flags differ."));
                const auto* Map=Physical->GetMetaDataMap();if((Map?Map->Num():0)!=Field.Metadata.Num()||(Map&&!TransformSourceMapSame(*Map,Field.Metadata)))return TransformSourceStop(TEXT("Current Transform source field metadata differs."));
                if(Field.Binding){const auto* StructField=static_cast<const FStructProperty*>(Physical);if(!StructField->Struct.IsResolved()||StructField->Struct!=Field.Binding)return TransformSourceStop(TEXT("Current Transform source FStructProperty binding differs."));}
            }
            if(FieldIndex!=Part.Fields.Num())return TransformSourceStop(TEXT("Current Transform source field count differs."));
        }
        return true;
    }
    bool CaptureTransformSource()
    {
        const TCHAR* Paths[]={TEXT("/Script/CoreUObject.Transform"),TEXT("/Script/CoreUObject.Quat"),TEXT("/Script/CoreUObject.Vector")};
        for(int32 Index=0;Index<3;++Index)
        {
            UScriptStruct* Returned=FindObject<UScriptStruct>(nullptr,Paths[Index]);TStrongObjectPtr<UScriptStruct> HoldReturned(Returned);if(!Context())return TransformSourceStop(TEXT("Transform source lookup changed Editor context."));
            UScriptStruct* Expected=Index==0?TBaseStructure<FTransform>::Get():Index==1?TBaseStructure<FQuat>::Get():TBaseStructure<FVector>::Get();TStrongObjectPtr<UScriptStruct> HoldExpected(Expected);if(!Context())return TransformSourceStop(TEXT("Native Transform type lookup changed context."));
            if(!DeclarationEnumLoaded(Returned)||Returned!=Expected||Returned->GetClass()!=UScriptStruct::StaticClass()||Returned->GetOuter()!=SourcePackage.Get()||Returned->GetPackage()!=SourcePackage.Get()||!Returned->GetPathName().Equals(Paths[Index],ESearchCase::CaseSensitive)||!(Returned->StructFlags&STRUCT_Native)||Returned->IsEditorOnly()||Returned->GetStructureSize()<=0||Returned->GetMinAlignment()<=0)return TransformSourceStop(TEXT("Exact loaded native Transform/Quat/Vector types required; no loader or source values."));
            FTransformSourcePart Part;Part.Type.Reset(Returned);Part.Path=Paths[Index];Part.ObjectFlags=Returned->GetFlags();Part.StructFlags=Returned->StructFlags;Part.Size=Returned->GetStructureSize();Part.Alignment=Returned->GetMinAlignment();Part.Super=Returned->GetSuperStruct();TransformSourceParts.Add(MoveTemp(Part));
        }
        for(int32 Index=0;Index<3;++Index)
        {
            auto& Part=TransformSourceParts[Index];TSet<FName> Seen;
            for(TFieldIterator<FProperty> It(Part.Type.Get(),EFieldIterationFlags::None);It;++It)
            {
                FProperty* Physical=*It;const FName FieldName=Physical->GetFName();UScriptStruct* Binding=nullptr;FString BindingPath;
                if(Physical->GetOwnerVariant()!=FFieldVariant(Part.Type.Get())||Seen.Contains(FieldName)||Physical->ArrayDim!=1||Physical->HasSetterOrGetter()||Physical->GetOffset_ForInternal()<0||Physical->GetSize()<=0||int64(Physical->GetOffset_ForInternal())+Physical->GetSize()>Part.Size||Physical->GetMinAlignment()<=0)return TransformSourceStop(TEXT("Native Transform source direct field layout is unobservable."));
                if(Index==0)
                {
                    if(FieldName==TEXT("Rotation")){Binding=TransformSourceParts[1].Type.Get();BindingPath=TransformSourceParts[1].Path;}else if(FieldName==TEXT("Translation")||FieldName==TEXT("Scale3D")){Binding=TransformSourceParts[2].Type.Get();BindingPath=TransformSourceParts[2].Path;}else return TransformSourceStop(TEXT("Transform must expose its entire Rotation/Translation/Scale3D reflected inventory."));
                    if(Physical->GetClass()!=FStructProperty::StaticClass())return TransformSourceStop(TEXT("Transform direct fields must be exact FStructProperty."));const auto* StructField=static_cast<const FStructProperty*>(Physical);if(!StructField->Struct.IsResolved()||StructField->Struct!=Binding)return TransformSourceStop(TEXT("Transform direct Quat/Vector binding is unresolved or differs."));
                }
                else if(Physical->GetClass()!=FDoubleProperty::StaticClass()||!(FieldName==TEXT("X")||FieldName==TEXT("Y")||FieldName==TEXT("Z")||(Index==1&&FieldName==TEXT("W"))))return TransformSourceStop(TEXT("Native Transform nested Quat/Vector fields must match complete Double inventory."));
                FTransformSourceField Field;Field.Address=Physical;Field.Kind=Physical->GetClass();Field.FieldName=FieldName;Field.PropertyFlags=uint64(Physical->GetPropertyFlags());Field.Offset=Physical->GetOffset_ForInternal();Field.Size=Physical->GetSize();Field.Dim=Physical->ArrayDim;Field.Alignment=Physical->GetMinAlignment();Field.Binding=Binding;Field.BindingPath=BindingPath;if(const auto* Map=Physical->GetMetaDataMap()){if(Map->Num()>256)return TransformSourceStop(TEXT("Native Transform field metadata bound exceeded."));Field.Metadata=*Map;}Seen.Add(FieldName);Part.Fields.Add(MoveTemp(Field));
            }
            if(Seen.Num()!=(Index==1?4:3))return TransformSourceStop(TEXT("Native Transform source property inventory is incomplete."));
        }
        TransformSourceReady=true;return TransformSourceCurrent();
    }
    bool TransformSourceMetadata(bool Capture)
    {
        for(auto& Part:TransformSourceParts)
        {
            const auto* Returned=FMetaData::GetMapForObject(Part.Type.Get());TMap<FName,FString> Copy;if(Returned){if(Returned->Num()>2048)return TransformSourceStop(TEXT("Transform source metadata bound exceeded."));Copy=*Returned;}
            if(!(NativeStructIdentity()&&Target()))return false;if(Capture)Part.Metadata=MoveTemp(Copy);else if(!TransformSourceMapSame(Copy,Part.Metadata))return TransformSourceStop(TEXT("Native Transform/Quat/Vector source metadata changed."));
        }
        if(Capture)TransformSourceMetadataReady=true;return true;
    }
    TSharedRef<FJsonObject> TransformSourcePartJson(int32 Index)const
    {
        const auto& Part=TransformSourceParts[Index];auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("struct_path"),Part.Path);J->SetStringField(TEXT("type_path"),Part.Path);J->SetStringField(TEXT("type_flags_string"),LexToString(uint32(Part.StructFlags)));J->SetNumberField(TEXT("type_structure_size"),Part.Size);J->SetNumberField(TEXT("type_alignment"),Part.Alignment);auto Metadata=MakeShared<FJsonObject>();for(const auto& Pair:Part.Metadata)Metadata->SetStringField(Pair.Key.ToString(),Pair.Value);J->SetObjectField(TEXT("metadata"),Metadata);
        TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& Field:Part.Fields){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),Field.FieldName.ToString());V->SetStringField(TEXT("class"),Field.Kind->GetName());V->SetStringField(TEXT("owner"),Part.Path);V->SetStringField(TEXT("path"),Field.Address->GetPathName());V->SetNumberField(TEXT("offset"),Field.Offset);V->SetNumberField(TEXT("size"),Field.Size);V->SetNumberField(TEXT("element_size"),Field.Size);V->SetNumberField(TEXT("array_dim"),Field.Dim);V->SetNumberField(TEXT("alignment"),Field.Alignment);V->SetStringField(TEXT("flags"),LexToString(Field.PropertyFlags));V->SetStringField(TEXT("struct_path"),Field.BindingPath);Rows.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("properties"),Rows);return J;
    }
    TStrongObjectPtr<UClass> NativeSource;
    struct FNativeClassRole
    {
        TStrongObjectPtr<UClass> Hold;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UObject> Default;
        UClass* Meta=nullptr;UClass* Super=nullptr;UClass* Within=nullptr;UObject* Outer=nullptr;
        FString Path,DefaultPath;UObject* DefaultOuter=nullptr;UClass* DefaultClass=nullptr;
        EObjectFlags Flags=RF_NoFlags,PackageFlags=RF_NoFlags,DefaultFlags=RF_NoFlags;uint32 ClassFlags=0,PackageBits=0;bool Dirty=false;
    };
    TArray<FNativeClassRole> NativeClasses;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    UObject* SourceOuter=nullptr;UClass* SourceClass=nullptr;FString SourcePath;EObjectFlags SourceFlags=RF_NoFlags;bool SourceDirty=false;
    uint32 SourcePackageFlags=0;EObjectFlags SourcePackageObjectFlags=RF_NoFlags;TSharedPtr<FJsonObject> Before;
    TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UEdGraph> Graph;TStrongObjectPtr<UPackage> TargetPackage;
    TStrongObjectPtr<UClass> Parent,Generated,Skeleton;TStrongObjectPtr<UObject> Default;
    UObject* BlueprintOuter=nullptr;UClass* BlueprintClass=nullptr;FString BlueprintPath,GraphPath;FGuid GraphGuid;
    UClass* GeneratedMetaClass=nullptr;UClass* SkeletonMetaClass=nullptr;UObject* GeneratedOuter=nullptr;UObject* SkeletonOuter=nullptr;UObject* DefaultOuter=nullptr;
    FString GeneratedPath,SkeletonPath,DefaultPath;EObjectFlags GeneratedFlags=RF_NoFlags,SkeletonFlags=RF_NoFlags,DefaultFlags=RF_NoFlags;uint32 GeneratedClassFlags=0,SkeletonClassFlags=0;
    struct FEditableRole { TStrongObjectPtr<UEdGraphNode> Hold;FGuid Guid;UClass* Class=nullptr;FString Path; };
    TArray<FEditableRole> Editable;
bool Bad(const TCHAR* Code,const TCHAR* Why)
    {
        if(Stopped) { return false; }Stopped=true;auto& R=*Reply;R.bSuccess=R.bPostconditionPassed=false;R.bChangeStateKnown=!LoadEntered&&!R.bMutationAttempted;
        if(!R.ErrorCode.IsEmpty()&&R.ErrorCode!=Code) { R.Data->SetStringField(NativeStructMode?TEXT("struct_preceding_error_code"):NativeClassMode?TEXT("class_preceding_error_code"):TEXT("enum_preceding_error_code"),R.ErrorCode); }
        R.ErrorCode=Code;R.Error=Why;R.Data->SetBoolField(NativeStructMode?TEXT("struct_source_observed_unchanged"):NativeClassMode?TEXT("class_source_observed_unchanged"):TEXT("enum_source_observed_unchanged"),false);
        R.Data->SetBoolField(TEXT("automatic_retry_safe"),false);return false;
    }
bool Context() const
    { return DeclarationEnumGlobals()&&(!LocalMapMode||!GIsImportingT3D)&&GEditor==Editor.Get()&&!IsPlaySessionActive()&&World.IsValid()&&(LocalMapMode?static_cast<UWorld*>(GWorld)==World.Get():GEditor->GetEditorWorldContext().World()==World.Get())&&!World->bIsTearingDown; }
bool Identity()
    {
        if(Stopped) { return false; }if(!Active) { return true; }
        if(!Context()) { return Bad(TEXT("declaration_enum_context_changed"),TEXT("Captured Editor/world or GT/PIE/GC/exit/save admission changed; no next SDK or cleanup.")); }
        if(LocalMapMode){return DeclarationEnumLoaded(SourcePackage.Get())&&SourcePackage->GetName().Equals(TEXT("/Script/CoreUObject"),ESearchCase::CaseSensitive)&&uint32(SourcePackage->GetPackageFlags())==SourcePackageFlags&&SourcePackage->GetFlags()==SourcePackageObjectFlags&&!SourcePackage->IsDirty()?true:Bad(TEXT("local_map_source_changed"),TEXT("The exact loaded native scalar package changed."));}if(NativeStructMode){return NativeStructIdentity();}if(NativeClassMode) { return NativeIdentity(); }auto* E=Source.Get();
        if(!DeclarationEnumLoaded(E)||!DeclarationEnumLoaded(SourcePackage.Get())||!DeclarationEnumLoaded(SourceOuter)||!DeclarationEnumLoaded(SourceClass)
            ||E->GetClass()!=SourceClass||E->GetOuter()!=SourceOuter||E->GetPackage()!=SourcePackage.Get()||E->GetPathName()!=SourcePath||E->GetFlags()!=SourceFlags||SourcePackage->IsDirty()!=SourceDirty||uint32(SourcePackage->GetPackageFlags())!=SourcePackageFlags||SourcePackage->GetFlags()!=SourcePackageObjectFlags)
        { return Bad(TEXT("declaration_enum_identity_changed"),TEXT("Read-only enum source identity/package/flags/dirty changed; no replacement source is adopted.")); }
        return true;
    }
bool Snapshot(TSharedRef<FJsonObject>& J)
    {
        if(!Identity()) { return false; }if(LocalMapMode){J->SetStringField(TEXT("value_kind"),TEXT("string_double_map"));J->SetStringField(TEXT("package_path"),TEXT("/Script/CoreUObject"));J->SetStringField(TEXT("package_flags_string"),LexToString(SourcePackageFlags));J->SetBoolField(TEXT("observations_complete"),true);J->SetBoolField(TEXT("native_identity_verified"),true);J->SetStringField(TEXT("scalar_property_class"),TEXT("DoubleProperty"));J->SetNumberField(TEXT("size"),sizeof(double));J->SetNumberField(TEXT("alignment"),alignof(double));return true;}if(NativeStructMode){return NativeStructSnapshot(J);}if(NativeClassMode) { return NativeSnapshot(J); }auto* E=Source.Get();
        J->SetStringField(TEXT("path"),SourcePath);J->SetStringField(TEXT("class"),SourceClass->GetPathName());J->SetBoolField(TEXT("dirty"),SourceDirty);J->SetNumberField(TEXT("flags"),uint32(SourceFlags));J->SetNumberField(TEXT("package_flags"),SourcePackageFlags);J->SetNumberField(TEXT("package_object_flags"),uint32(SourcePackageObjectFlags));
        if(E->NumEnums()<2||E->NumEnums()>257) { return Bad(TEXT("declaration_enum_entries_invalid"),TEXT("Bounded byte UserDefinedEnum entries required.")); }
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(int32 I=0;I<E->NumEnums();++I)
        {
            auto Row=MakeShared<FJsonObject>();const FString Name=E->GetNameStringByIndex(I);const int64 Value=E->GetValueByIndex(I);const FString Display=E->GetDisplayNameTextByIndex(I).ToString();
            if(!Identity()) { return false; }
            if(Name.Len()>512||Display.Len()>4096||(I<E->NumEnums()-1&&(Value<0||Value>255))) { return Bad(TEXT("declaration_enum_entries_invalid"),TEXT("Enum byte entries/text exceed the finite source scope.")); }
            Row->SetStringField(TEXT("name"),Name);Row->SetStringField(TEXT("full_name"),E->GetNameByIndex(I).ToString());Row->SetStringField(TEXT("value"),LexToString(Value));Row->SetStringField(TEXT("display"),Display);
            Row->SetBoolField(TEXT("hidden"),E->HasMetaData(TEXT("Hidden"),I));Row->SetBoolField(TEXT("spacer"),E->HasMetaData(TEXT("Spacer"),I));if(!Identity()) { return false; }Entries.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetArrayField(TEXT("entries"),Entries);J->SetNumberField(TEXT("cpp_form"),int32(E->GetCppForm()));J->SetNumberField(TEXT("underlying"),int32(E->GetUnderlyingType()));
        for(const FName Field:{FName(TEXT("UniqueNameIndex")),FName(TEXT("EnumDescription")),FName(TEXT("DisplayNameMap"))})
        {
            auto* P=FindFProperty<FProperty>(E->GetClass(),Field);FString Value;
            if(!P||P->ArrayDim!=1||!P->ExportText_Direct(Value,P->ContainerPtrToValuePtr<void>(E),P->ContainerPtrToValuePtr<void>(E),E,PPF_None)||!Identity()) { return Bad(TEXT("declaration_enum_stored_fields_incomplete"),TEXT("Complete stored UserDefinedEnum fields unavailable.")); }
            if(Value.Len()>262144) { return Bad(TEXT("declaration_enum_source_bound"),TEXT("Stored enum source field exceeds finite observation bound.")); }J->SetStringField(Field.ToString(),Value);
        }
        const TMap<FName,FString>* Meta=FMetaData::GetMapForObject(E);if(!Identity()) { return false; }auto M=MakeShared<FJsonObject>();
        if(Meta)
        {
            if(Meta->Num()>2048) { return Bad(TEXT("declaration_enum_source_bound"),TEXT("Enum source metadata exceeds finite observation bound.")); }
            for(const auto& Pair:*Meta) { if(Pair.Key.ToString().Len()>512||Pair.Value.Len()>16384) { return Bad(TEXT("declaration_enum_source_bound"),TEXT("Enum metadata text exceeds bound.")); }M->SetStringField(Pair.Key.ToString(),Pair.Value); }
        }
        J->SetObjectField(TEXT("metadata"),M);FString File;
        if(!FPackageName::TryConvertLongPackageNameToFilename(SourcePackage->GetName(),File,FPackageName::GetAssetPackageExtension())) { return Bad(TEXT("declaration_enum_disk_incomplete"),TEXT("Enum source package filename unavailable.")); }
        const int64 Size=IFileManager::Get().FileSize(*File);J->SetBoolField(TEXT("disk_exists"),Size>=0);J->SetStringField(TEXT("disk_path"),File);
        if(Size>=0)
        {
            if(Size>16*1024*1024) { return Bad(TEXT("declaration_enum_disk_bound"),TEXT("Enum source disk package exceeds16MiB bound.")); }
            TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size) { return Bad(TEXT("declaration_enum_disk_incomplete"),TEXT("Enum source disk read incomplete.")); }
            uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);J->SetNumberField(TEXT("disk_bytes"),Size);J->SetStringField(TEXT("disk_sha1"),BytesToHex(Digest,FSHA1::DigestSize));
        }
        if(!Identity()||DeclarationEnumText(J).Len()>1048576) { return Bad(TEXT("declaration_enum_source_bound"),TEXT("Complete enum source snapshot exceeds1Mi UTF16 bound or context changed.")); }return true;
    }
    bool NativeStructIdentity()
    {
        if(ScalarUds.IsValid())return ScalarUds->PlainCurrent();
        UScriptStruct* S=NativeStructSource.Get();UPackage* P=SourcePackage.Get();
        if(!DeclarationEnumLoaded(S)||!DeclarationEnumLoaded(P)||S->GetClass()!=SourceClass||SourceClass!=UScriptStruct::StaticClass()||S->GetOuter()!=SourceOuter||SourceOuter!=P||S->GetPackage()!=P
            ||S->GetPathName()!=SourcePath||FindObject<UScriptStruct>(nullptr,*SourcePath)!=S||S->GetFlags()!=SourceFlags||S->StructFlags!=NativeStructFlags||S->GetStructureSize()!=NativeStructSize||S->GetMinAlignment()!=NativeStructAlignment||S->GetSuperStruct()!=NativeStructSuper
            ||(S!=TBaseStructure<FVector>::Get()&&S!=TBaseStructure<FRotator>::Get()&&(!NativeTransformMode||S!=TBaseStructure<FTransform>::Get()))||!(S->StructFlags&STRUCT_Native)||S->IsEditorOnly()
            ||P->GetName()!=TEXT("/Script/CoreUObject")||P->IsDirty()!=SourceDirty||uint32(P->GetPackageFlags())!=SourcePackageFlags||P->GetFlags()!=SourcePackageObjectFlags)
        {return Bad(TEXT("local_struct_source_changed"),TEXT("Exact native Vector/Rotator source identity/fields role/package changed; no next SDK or local writer."));}return !NativeTransformMode||TransformSourceCurrent();
    }
    bool NativeStructSnapshot(TSharedRef<FJsonObject>& J)
    {
        if(!NativeStructIdentity()){return false;}if(ScalarUds.IsValid()){if(!ScalarUds->Query())return false;J=ScalarUds->Snapshot();return true;}UScriptStruct* S=NativeStructSource.Get();
        if(NativeTransformMode)
        {
            if(!TransformSourceMetadata(!TransformSourceMetadataReady))return false;auto Root=TransformSourcePartJson(0);J->SetStringField(TEXT("path"),SourcePath);J->SetStringField(TEXT("class"),SourceClass->GetPathName());J->SetStringField(TEXT("outer"),SourceOuter->GetPathName());J->SetNumberField(TEXT("flags"),uint32(SourceFlags));J->SetNumberField(TEXT("struct_flags"),uint32(NativeStructFlags));J->SetNumberField(TEXT("size"),NativeStructSize);J->SetNumberField(TEXT("alignment"),NativeStructAlignment);J->SetStringField(TEXT("super"),GetPathNameSafe(NativeStructSuper));J->SetBoolField(TEXT("dirty"),SourceDirty);J->SetNumberField(TEXT("package_flags"),SourcePackageFlags);J->SetNumberField(TEXT("package_object_flags"),uint32(SourcePackageObjectFlags));J->SetArrayField(TEXT("fields"),Root->GetArrayField(TEXT("properties")));J->SetObjectField(TEXT("metadata"),Root->GetObjectField(TEXT("metadata")));TArray<TSharedPtr<FJsonValue>> Nested;for(int32 Index=1;Index<3;++Index)Nested.Add(MakeShared<FJsonValueObject>(TransformSourcePartJson(Index)));J->SetArrayField(TEXT("nested_structs"),Nested);J->SetStringField(TEXT("readonly_scope"),TEXT("Exact native Transform local declaration source with direct Quat/Vector bindings and complete nested Double field/metadata guards; no values, CDO or default buffer sampled."));return NativeStructIdentity()&&Target();
        }
        J->SetStringField(TEXT("path"),SourcePath);J->SetStringField(TEXT("class"),SourceClass->GetPathName());J->SetStringField(TEXT("outer"),SourceOuter->GetPathName());J->SetNumberField(TEXT("flags"),uint32(SourceFlags));J->SetNumberField(TEXT("struct_flags"),uint32(NativeStructFlags));
        J->SetNumberField(TEXT("size"),NativeStructSize);J->SetNumberField(TEXT("alignment"),NativeStructAlignment);J->SetStringField(TEXT("super"),GetPathNameSafe(NativeStructSuper));J->SetBoolField(TEXT("dirty"),SourceDirty);J->SetNumberField(TEXT("package_flags"),SourcePackageFlags);J->SetNumberField(TEXT("package_object_flags"),uint32(SourcePackageObjectFlags));
        TArray<TSharedPtr<FJsonValue>> Fields;int32 Count=0;
        for(TFieldIterator<FProperty> It(S,EFieldIteratorFlags::IncludeSuper);It;++It)
        {
            FProperty* Field=*It;if(++Count>32||Field->ArrayDim!=1||Field->GetOwnerStruct()!=S){return Bad(TEXT("local_struct_source_bound"),TEXT("Exact bounded built-in structure member fields required."));}
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Field->GetName());Row->SetStringField(TEXT("class"),Field->GetClass()->GetName());Row->SetStringField(TEXT("owner"),Field->GetOwnerStruct()->GetPathName());Row->SetNumberField(TEXT("offset"),Field->GetOffset_ForInternal());Row->SetNumberField(TEXT("element_size"),Field->GetElementSize());Row->SetStringField(TEXT("flags"),LexToString(uint64(Field->GetPropertyFlags())));Fields.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetArrayField(TEXT("fields"),Fields);const auto* Map=FMetaData::GetMapForObject(S);if(!Identity()||!Target()){return false;}auto Metadata=MakeShared<FJsonObject>();int32 Chars=0;
        if(Map){if(Map->Num()>2048){return Bad(TEXT("local_struct_source_bound"),TEXT("Built-in structure metadata count exceeds finite scope."));}for(const auto& Pair:*Map){Chars+=Pair.Key.ToString().Len()+Pair.Value.Len();if(Pair.Key.ToString().Len()>512||Pair.Value.Len()>65536||Chars>1048576){return Bad(TEXT("local_struct_source_bound"),TEXT("Built-in structure metadata text exceeds finite scope."));}Metadata->SetStringField(Pair.Key.ToString(),Pair.Value);}}
        J->SetObjectField(TEXT("metadata"),Metadata);J->SetStringField(TEXT("readonly_scope"),TEXT("Exact compiled-in CoreUObject Vector/Rotator identity/size/alignment/flags/member signature/metadata; no struct loader, CDO warming, global cache or runtime value evaluation claim."));
        return (Identity()&&Target()&&DeclarationEnumText(J).Len()<=1048576)||Bad(TEXT("local_struct_source_changed"),TEXT("Built-in source snapshot lost admission or exceeds1Mi UTF16."));
    }
    bool LoadScalarUds(const FString& Requested,FResult& R)
    {
        Reply=&R;Active=NativeStructMode=true;if(!DeclarationEnumGlobals()||IsPlaySessionActive()||GIsImportingT3D)return Bad(TEXT("local_uds_editor_required"),TEXT("Safe Editor context required before loaded UDS observation."));
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!World.IsValid()||World->bIsTearingDown)return Bad(TEXT("local_uds_editor_required"),TEXT("Current Editor world required."));
        ScalarUds=MakeShared<FLocalScalarUdsSource>();if(!ScalarUds->Load(Requested,[this]{return !Stopped&&Context()&&Target();},[this](const TCHAR* Code,const TCHAR* Why){const bool Unknown=ScalarUds->OutcomeUnknown();const bool Result=Bad(Code,Why);if(Unknown){Reply->bChangeStateKnown=false;Reply->Data->SetBoolField(TEXT("scalar_uds_observation_outcome_unknown"),true);}return Result;}))return false;
        NativeStructSource.Reset(ScalarUds->Type());SourcePackage.Reset(ScalarUds->Package());SourceOuter=NativeStructSource->GetOuter();SourceClass=NativeStructSource->GetClass();SourcePath=NativeStructSource->GetPathName();SourceFlags=NativeStructSource->GetFlags();SourceDirty=false;SourcePackageFlags=uint32(SourcePackage->GetPackageFlags());SourcePackageObjectFlags=SourcePackage->GetFlags();
        Before=ScalarUds->Snapshot();R.Data->SetObjectField(TEXT("struct_source_before"),Before.ToSharedRef());R.Data->SetStringField(TEXT("struct_type_object"),SourcePath);R.Data->SetBoolField(TEXT("struct_source_read_only"),true);return Check();
    }
    bool LoadNativeStruct(const FString& Type,FResult& R,bool AllowTransform=false)
    {
        Reply=&R;Active=NativeStructMode=true;
        if(!DeclarationEnumGlobals()||IsPlaySessionActive()){return Bad(TEXT("local_struct_editor_required"),TEXT("Safe Editor GT context required before built-in descriptor observation."));}
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!World.IsValid()||World->bIsTearingDown){return Bad(TEXT("local_struct_editor_required"),TEXT("Current Editor world required."));}
        NativeTransformMode=AllowTransform&&Type==TEXT("transform");const bool Vector=Type==TEXT("vector");if(!Vector&&Type!=TEXT("rotator")&&!NativeTransformMode){return Bad(TEXT("local_struct_descriptor_unsupported"),TEXT("Only exact builtin Vector/Rotator local descriptors are admitted."));}
        UScriptStruct* S=NativeTransformMode?TBaseStructure<FTransform>::Get():Vector?TBaseStructure<FVector>::Get():TBaseStructure<FRotator>::Get();const TCHAR* Path=NativeTransformMode?TEXT("/Script/CoreUObject.Transform"):Vector?TEXT("/Script/CoreUObject.Vector"):TEXT("/Script/CoreUObject.Rotator");
        if(!Context()||!DeclarationEnumLoaded(S)||S->GetClass()!=UScriptStruct::StaticClass()||S->GetPathName()!=Path||S->GetOuter()!=S->GetPackage()||S->GetStructureSize()!=int32(NativeTransformMode?sizeof(FTransform):Vector?sizeof(FVector):sizeof(FRotator)))
        {return Bad(TEXT("local_struct_descriptor_invalid"),TEXT("Exact already registered native value structure required; no generic struct loader or substitution."));}
        NativeStructSource.Reset(S);SourcePackage.Reset(S->GetPackage());SourceOuter=S->GetOuter();SourceClass=S->GetClass();SourcePath=Path;SourceFlags=S->GetFlags();SourceDirty=SourcePackage->IsDirty();SourcePackageFlags=uint32(SourcePackage->GetPackageFlags());SourcePackageObjectFlags=SourcePackage->GetFlags();NativeStructFlags=S->StructFlags;NativeStructSize=S->GetStructureSize();NativeStructAlignment=S->GetMinAlignment();NativeStructSuper=S->GetSuperStruct();
        if(SourceDirty||!SourcePackage->HasAnyPackageFlags(PKG_CompiledIn)||SourcePackage->HasAnyPackageFlags(PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)){return Bad(TEXT("local_struct_descriptor_invalid"),TEXT("Clean compiled-in runtime CoreUObject source package required."));}
        if(NativeTransformMode&&!CaptureTransformSource())return false;
        auto J=MakeShared<FJsonObject>();if(!Snapshot(J)){return false;}Before=J;R.Data->SetObjectField(TEXT("struct_source_before"),J);R.Data->SetStringField(TEXT("struct_type_object"),SourcePath);R.Data->SetBoolField(TEXT("struct_source_read_only"),true);return Check();
    }
    bool NativeIdentity()
    {
        if(!DeclarationEnumLoaded(NativeSource.Get())||NativeClasses.IsEmpty()||NativeClasses[0].Hold.Get()!=NativeSource.Get())
        { return Bad(TEXT("local_class_source_changed"),TEXT("Exact native descriptor source is unavailable.")); }
        for(const auto& Role:NativeClasses)
        {
            UClass* C=Role.Hold.Get();UPackage* P=Role.Package.Get();UObject* D=Role.Default.Get();
            if(!DeclarationEnumLoaded(C)||!DeclarationEnumLoaded(P)||C->GetClass()!=Role.Meta||C->GetOuter()!=Role.Outer||C->GetPackage()!=P||C->GetPathName()!=Role.Path||C->GetFlags()!=Role.Flags
                ||uint32(C->GetClassFlags())!=Role.ClassFlags||C->GetSuperClass()!=Role.Super||C->ClassWithin!=Role.Within||C->ClassGeneratedBy!=nullptr||!C->HasAnyClassFlags(CLASS_Native)
                ||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||P->GetFlags()!=Role.PackageFlags||uint32(P->GetPackageFlags())!=Role.PackageBits||P->IsDirty()!=Role.Dirty||C->GetDefaultObject(false)!=D)
            { return Bad(TEXT("local_class_source_changed"),TEXT("Native class hierarchy identity/flags/current existing CDO/package changed; no next SDK or declaration writer.")); }
            if(D&&(!DeclarationEnumLoaded(D)||D->GetClass()!=Role.DefaultClass||D->GetOuter()!=Role.DefaultOuter||D->GetPackage()!=P||D->GetPathName()!=Role.DefaultPath||D->GetFlags()!=Role.DefaultFlags))
            { return Bad(TEXT("local_class_source_changed"),TEXT("Captured existing native CDO identity changed; no creation or replacement adoption.")); }
        }
        return true;
    }
    bool NativeSnapshot(TSharedRef<FJsonObject>& J)
    {
        if(!NativeIdentity()) { return false; }TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& Role:NativeClasses)
        {
            UClass* C=Role.Hold.Get();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),Role.Path);Row->SetStringField(TEXT("class"),Role.Meta->GetPathName());Row->SetStringField(TEXT("outer"),Role.Outer->GetPathName());
            Row->SetStringField(TEXT("super"),GetPathNameSafe(Role.Super));Row->SetStringField(TEXT("within"),GetPathNameSafe(Role.Within));Row->SetStringField(TEXT("flags"),LexToString(uint32(Role.Flags)));Row->SetStringField(TEXT("class_flags"),LexToString(Role.ClassFlags));
            Row->SetStringField(TEXT("package"),Role.Package->GetName());Row->SetStringField(TEXT("package_flags"),LexToString(Role.PackageBits));Row->SetBoolField(TEXT("package_dirty"),Role.Dirty);Row->SetStringField(TEXT("existing_cdo"),Role.DefaultPath);
            const TMap<FName,FString>* Metadata=FMetaData::GetMapForObject(C);if(!Identity()||!Target()) { return false; }auto Meta=MakeShared<FJsonObject>();
            if(Metadata)
            {
                if(Metadata->Num()>2048) { return Bad(TEXT("local_class_source_bound"),TEXT("Native class metadata exceeds the finite descriptor observation bound.")); }
                for(const auto& Pair:*Metadata) { if(Pair.Key.ToString().Len()>512||Pair.Value.Len()>16384) { return Bad(TEXT("local_class_source_bound"),TEXT("Native class metadata text exceeds bound.")); }Meta->SetStringField(Pair.Key.ToString(),Pair.Value); }
            }
            Row->SetObjectField(TEXT("metadata"),Meta);Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        J->SetArrayField(TEXT("native_classes"),Rows);J->SetStringField(TEXT("source_path"),SourcePath);J->SetStringField(TEXT("disk_scope"),TEXT("Native compiled-in Engine/CoreUObject sources have no selected asset file; target Blueprint disk remains independently captured."));
        J->SetStringField(TEXT("readonly_scope"),TEXT("Exact native class hierarchy/metadata/current existing CDO identity only; no class/CDO creation, source Modify/save or total global cache purity claim."));
        return (Identity()&&Target()&&DeclarationEnumText(J).Len()<=1048576)||Bad(TEXT("local_class_source_changed"),TEXT("Native class source snapshot lost admission or exceeded1Mi UTF16."));
    }
    bool LoadNativeClass(const FString& Input,FResult& R)
    {
        Reply=&R;Active=NativeClassMode=true;
        if(!DeclarationEnumGlobals()||IsPlaySessionActive()) { return Bad(TEXT("local_class_editor_required"),TEXT("Safe Editor GT context required before native descriptor observation.")); }
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!World.IsValid()||World->bIsTearingDown) { return Bad(TEXT("local_class_editor_required"),TEXT("Current Editor world required.")); }
        FString P=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();P.TrimQuotesInline();
        if(P.IsEmpty()||P==TEXT("Object")) { P=TEXT("/Script/CoreUObject.Object"); }else if(P==TEXT("Actor")) { P=TEXT("/Script/Engine.Actor"); }
        if(!SafeName(Input,true)||(P!=TEXT("/Script/Engine.Actor")&&P!=TEXT("/Script/CoreUObject.Object")))
        { return Bad(TEXT("local_class_descriptor_unsupported"),TEXT("This local slice admits exact runtime native Actor and the original omitted Object constraint only; other native/generated/plugin/editor classes remain pending.")); }
        UClass* Expected=P==TEXT("/Script/Engine.Actor")?AActor::StaticClass():UObject::StaticClass();UClass* C=FindObject<UClass>(nullptr,*P);
        if(!Context()||C!=Expected||!DeclarationEnumLoaded(C)||C->GetClass()!=UClass::StaticClass()||C->GetPathName()!=P||C->ClassGeneratedBy||!C->HasAnyClassFlags(CLASS_Native))
        { return Bad(TEXT("local_class_descriptor_invalid"),TEXT("Exact already loaded runtime native UClass required; no module load or CDO warming.")); }
        NativeSource.Reset(C);SourcePath=P;SourcePackage.Reset(C->GetPackage());
        for(UClass* K=C;K;K=K->GetSuperClass())
        {
            const FString Path=K->GetPathName();if(NativeClasses.Num()>=2||!DeclarationEnumLoaded(K)||K->GetClass()!=UClass::StaticClass()||(K!=AActor::StaticClass()&&K!=UObject::StaticClass())||(Path!=TEXT("/Script/Engine.Actor")&&Path!=TEXT("/Script/CoreUObject.Object"))||K->ClassGeneratedBy||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists))
            { return Bad(TEXT("local_class_descriptor_invalid"),TEXT("Descriptor hierarchy must remain the exact native Actor/Object chain.")); }
            FNativeClassRole Role;Role.Hold.Reset(K);Role.Package.Reset(K->GetPackage());Role.Meta=K->GetClass();Role.Outer=K->GetOuter();Role.Path=Path;Role.Super=K->GetSuperClass();Role.Within=K->ClassWithin;Role.Flags=K->GetFlags();Role.ClassFlags=uint32(K->GetClassFlags());
            if(!DeclarationEnumLoaded(Role.Package.Get())||Role.Outer!=Role.Package.Get()||!Role.Package->HasAnyPackageFlags(PKG_CompiledIn)||Role.Package->HasAnyPackageFlags(PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly)||Role.Package->IsDirty())
            { return Bad(TEXT("local_class_descriptor_invalid"),TEXT("Clean runtime compiled-in native source package required.")); }
            Role.PackageFlags=Role.Package->GetFlags();Role.PackageBits=uint32(Role.Package->GetPackageFlags());Role.Dirty=Role.Package->IsDirty();UObject* D=K->GetDefaultObject(false);
            if(D)
            {
                if(!DeclarationEnumLoaded(D)||D->GetClass()!=K||D->GetPackage()!=Role.Package.Get()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)) { return Bad(TEXT("local_class_descriptor_invalid"),TEXT("Existing native CDO is incomplete; it is not created or repaired.")); }
                Role.Default.Reset(D);Role.DefaultPath=D->GetPathName();Role.DefaultClass=D->GetClass();Role.DefaultOuter=D->GetOuter();Role.DefaultFlags=D->GetFlags();
            }
            NativeClasses.Add(MoveTemp(Role));
        }
        auto J=MakeShared<FJsonObject>();if(!Snapshot(J)) { return false; }Before=J;R.Data->SetObjectField(TEXT("class_source_before"),J);R.Data->SetStringField(TEXT("class_type_object"),P);R.Data->SetBoolField(TEXT("class_source_read_only"),true);return Check();
    }
    bool NativeDefault(const TSharedPtr<FJsonValue>& V,bool ClassPin,FString& Stored)
    {
        Stored.Reset();if(!Check()) { return false; }if(!V.IsValid()||V->Type==EJson::Null) { Reply->Data->SetBoolField(TEXT("default_omitted_original_None"),true);return true; }
        Reply->Data->SetBoolField(TEXT("default_omitted_original_None"),false);
        if(V->Type!=EJson::String) { return Bad(TEXT("local_class_default_unsupported"),TEXT("Object/Class defaults in this slice require the original literal string or omitted/null; loose nonstring conversions remain pending.")); }
        Stored=V->AsString();if(Stored.IsEmpty()||Stored==TEXT("None")) { return Check(); }
        // Preserve the caller's literal spelling; no class or object default is loaded.
        if(!ClassPin||Stored!=SourcePath)
        { return Bad(TEXT("local_class_default_unsupported"),TEXT("Only the exact native constraint class path is accepted as a nonempty Class default; object instances, other classes and load/coercion variants remain pending.")); }
        Reply->Data->SetBoolField(TEXT("class_default_reference_observed"),true);return Check();
    }
bool BeginLocalMap(FResult& R)
    {
        Reply=&R;Active=LocalMapMode=true;
        if(!DeclarationEnumGlobals()||GIsImportingT3D||IsPlaySessionActive())return Bad(TEXT("local_map_editor_required"),TEXT("Idle editor before any local Map lookup is required."));
        Editor=GEditor;World=static_cast<UWorld*>(GWorld);if(!World.IsValid()||World->bIsTearingDown)return Bad(TEXT("local_map_editor_required"),TEXT("Current editor world required."));
        SourcePackage.Reset(FindPackage(nullptr,TEXT("/Script/CoreUObject")));
        if(!DeclarationEnumLoaded(SourcePackage.Get())||!SourcePackage->HasAnyPackageFlags(PKG_CompiledIn)||SourcePackage->IsDirty())return Bad(TEXT("local_map_source_unavailable"),TEXT("The native scalar package must already be loaded."));
        SourcePackageFlags=uint32(SourcePackage->GetPackageFlags());SourcePackageObjectFlags=SourcePackage->GetFlags();auto J=MakeShared<FJsonObject>();if(!Snapshot(J))return false;Before=J;
        R.Data->SetObjectField(TEXT("local_map_source_before"),J);R.Data->SetBoolField(TEXT("native_string_double_map_local"),true);return Check();
    }
    bool MapTarget(bool Classes,bool Nodes)
    {
        UBlueprint* B=Blueprint.Get();if(!B)return true;
        if(!DeclarationEnumLoaded(B)||!DeclarationEnumLoaded(TargetPackage.Get())||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPackage()!=TargetPackage.Get()||!B->GetPathName().Equals(BlueprintPath,ESearchCase::CaseSensitive)
            ||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=Parent.Get()||Parent.Get()!=(PresetMapLibrary?UBlueprintFunctionLibrary::StaticClass():AActor::StaticClass())||B->BlueprintType!=(PresetMapLibrary?BPTYPE_FunctionLibrary:BPTYPE_Normal)||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified)
            return Bad(TEXT("local_map_target_changed"),TEXT("The current normal own native Actor target changed."));
        if(PresetMapLibrary&&(!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=nullptr||!B->NewVariables.IsEmpty()||!B->GeneratedVariables.IsEmpty()||!B->ImplementedInterfaces.IsEmpty()||!B->CachedUDSDependencies.IsEmpty()))return Bad(TEXT("local_map_library_scope_changed"),TEXT("PresetLoader BFL has no SCS, instance/member descriptors, interfaces or UDS dependencies."));
        if(Classes)
        {
            if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr()!=Generated.Get()||B->SkeletonGeneratedClass.GetGCPtr()!=Skeleton.Get())return Bad(TEXT("local_map_classes_changed"),TEXT("Class replacement outside the explicit structural return boundary."));
            for(UClass* C:{Generated.Get(),Skeleton.Get()})if(!DeclarationEnumLoaded(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=B||C->GetPackage()!=TargetPackage.Get())return Bad(TEXT("local_map_classes_changed"),TEXT("Owned class identity changed."));
            if(!Generated->GetPathName().Equals(GeneratedPath,ESearchCase::CaseSensitive)||!Skeleton->GetPathName().Equals(SkeletonPath,ESearchCase::CaseSensitive)||Generated->GetOuter()!=GeneratedOuter||Skeleton->GetOuter()!=SkeletonOuter||Generated->GetFlags()!=GeneratedFlags||Skeleton->GetFlags()!=SkeletonFlags||uint32(Generated->GetClassFlags())!=GeneratedClassFlags||uint32(Skeleton->GetClassFlags())!=SkeletonClassFlags||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject!=Default.Get())return Bad(TEXT("local_map_classes_changed"),TEXT("Captured class flags or existing CDO identity changed."));
            if(Default.Get()&&(!DeclarationEnumLoaded(Default.Get())||Default->GetClass()!=Generated.Get()||Default->GetOuter()!=DefaultOuter||Default->GetFlags()!=DefaultFlags))return Bad(TEXT("local_map_classes_changed"),TEXT("Captured existing CDO role changed; no value sampled."));
        }
        if(Graph.Get())
        {
            UEdGraph* G=Graph.Get();if(!DeclarationEnumLoaded(G)||G->GetOuter()!=B||!B->FunctionGraphs.Contains(G)||G->GraphGuid!=GraphGuid||!G->GetPathName().Equals(GraphPath,ESearchCase::CaseSensitive)||!G->Schema.GetGCPtr().IsResolved()||G->Schema.GetGCPtr()!=UEdGraphSchema_K2::StaticClass()||!G->bEditable)return Bad(TEXT("local_map_graph_changed"),TEXT("Selected function graph/schema changed."));
            if(PresetMapLibrary&&G->GetFName()!=TEXT("PresetLoader"))return Bad(TEXT("local_map_library_function_unsupported"),TEXT("Only the exact own PresetLoader local Map function is admitted in a function library."));
            if(Nodes)for(const auto& Role:Editable){int32 Count=0;for(const auto& H:G->Nodes){if(!H.IsResolved())return Bad(TEXT("local_map_node_changed"),TEXT("Unresolved current node role."));if(H==Role.Hold.Get())++Count;}auto* N=Role.Hold.Get();if(Count!=1||!DeclarationEnumLoaded(N)||N->GetClass()!=Role.Class||N->GetOuter()!=G||N->NodeGuid!=Role.Guid||!N->GetPathName().Equals(Role.Path,ESearchCase::CaseSensitive))return Bad(TEXT("local_map_node_changed"),TEXT("Current Entry/Result role changed."));}
        }
        return true;
    }
    bool PresetEntry(UK2Node_FunctionEntry* E)
    {
        if(!PresetMapLibrary)return true;
        if(!MapTarget(true,false)||!DeclarationEnumLoaded(E)||E->GetOuter()!=Graph.Get()||E->GetClass()!=UK2Node_FunctionEntry::StaticClass()||!(E->GetExtraFlags()&FUNC_Static))return Bad(TEXT("local_map_library_entry_changed"),TEXT("PresetLoader requires its current static own Entry."));
        FMemberReference Expected;Expected.SetExternalMember(Graph->GetFName(),nullptr,E->FunctionReference.GetMemberGuid());
        if(!E->FunctionReference.IsSameReference(Expected)||E->FunctionReference.IsSelfContext()||E->FunctionReference.IsDeprecated()||!E->CustomGeneratedFunctionName.IsNone())return Bad(TEXT("local_map_library_entry_changed"),TEXT("PresetLoader Entry must retain its own external-null reference."));
        int32 Count=0;for(UEdGraphPin* P:E->Pins)if(P&&P->PinName==TEXT("__WorldContext")){FEdGraphPinType ExpectedType;ExpectedType.PinCategory=UEdGraphSchema_K2::PC_Object;ExpectedType.PinSubCategoryObject=UObject::StaticClass();if(P->WasTrashed()||P->GetOwningNodeUnchecked()!=E||!P->PinId.IsValid()||P->Direction!=EGPD_Output||P->PinType!=ExpectedType||P->PinType.bIsUObjectWrapper||P->PinType.bSerializeAsSinglePrecisionFloat||P->bOrphanedPin||P->bDeprecated||!P->bHidden||P->ParentPin||!P->SubPins.IsEmpty()||!P->DefaultObject.IsResolved()||P->DefaultObject!=nullptr||!P->DefaultTextValue.IsEmpty())return Bad(TEXT("local_map_library_world_changed"),TEXT("Current whole hidden automatic Entry WorldContext output required; no authored world edge is assumed."));++Count;}
        return Count==1?true:Bad(TEXT("local_map_library_world_changed"),TEXT("One automatic Entry WorldContext is required."));
    }
    bool RefreshMapClasses()
    {
        if(!Identity()||!MapTarget(false,false))return false;auto* B=Blueprint.Get();
        const auto GetClass=[&](const TObjectPtr<UClass>& H,TStrongObjectPtr<UClass>& Out,const FString& Path){if(!H.IsResolved()||H==nullptr)return false;UClass* C=static_cast<UClass*>(StaticFindObject(nullptr,nullptr,*Path));if(!DeclarationEnumLoaded(C)||H!=C||C->GetClass()!=UBlueprintGeneratedClass::StaticClass())return false;Out.Reset(C);return Identity()&&MapTarget(false,false);};
        const FString Asset=B->GetPathName(),Package=B->GetPackage()->GetName(),Name=B->GetName();
        if(!GetClass(B->GeneratedClass.GetGCPtr(),Generated,Asset+TEXT("_C"))||!GetClass(B->SkeletonGeneratedClass.GetGCPtr(),Skeleton,Package+TEXT(".SKEL_")+Name+TEXT("_C")))return Bad(TEXT("local_map_classes_unavailable"),TEXT("Current exact generated/skeleton classes unavailable after structural return."));
        if(!Generated->ClassGeneratedBy.IsResolved()||Generated->ClassGeneratedBy!=B||!Skeleton->ClassGeneratedBy.IsResolved()||Skeleton->ClassGeneratedBy!=B||!Generated->ClassDefaultObject.IsResolved())return Bad(TEXT("local_map_classes_unavailable"),TEXT("Current class ownership/CDO handle unavailable."));
        Default.Reset();const auto CurrentDefaultHandle=Generated->ClassDefaultObject;
        if(CurrentDefaultHandle!=nullptr){UObject* D=StaticFindObject(nullptr,nullptr,*(Package+TEXT(".Default__")+Name+TEXT("_C")));if(!DeclarationEnumLoaded(D)||CurrentDefaultHandle!=D||D->GetClass()!=Generated.Get()||D->GetOuter()!=TargetPackage.Get()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Bad(TEXT("local_map_cdo_changed"),TEXT("Existing CDO role is unavailable; no creation or value read."));Default.Reset(D);DefaultOuter=D->GetOuter();DefaultFlags=D->GetFlags();}
        for(auto& Role:Editable){UEdGraphNode* Next=nullptr;const auto Inventory=Graph->Nodes;for(const auto& H:Inventory){if(!H.IsResolved())return Bad(TEXT("local_map_node_changed"),TEXT("Resolved refreshed node inventory required."));UEdGraphNode* N=H.Get();if(!Identity()||!MapTarget(false,false)||Graph->Nodes!=Inventory)return false;if(DeclarationEnumLoaded(N)&&N->NodeGuid==Role.Guid){if(Next)return Bad(TEXT("local_map_node_changed"),TEXT("Duplicate refreshed entry GUID."));Next=N;}}if(!Next||Next->GetClass()!=Role.Class||Next->GetOuter()!=Graph.Get()||!Next->GetPathName().Equals(Role.Path,ESearchCase::CaseSensitive))return Bad(TEXT("local_map_node_changed"),TEXT("Current Entry identity cannot be reacquired after structural return."));Role.Hold.Reset(Next);}GeneratedPath=Generated->GetPathName();SkeletonPath=Skeleton->GetPathName();GeneratedOuter=Generated->GetOuter();SkeletonOuter=Skeleton->GetOuter();GeneratedFlags=Generated->GetFlags();SkeletonFlags=Skeleton->GetFlags();GeneratedClassFlags=uint32(Generated->GetClassFlags());SkeletonClassFlags=uint32(Skeleton->GetClassFlags());
        return Check();
    }
bool Load(const FString& Input,FResult& R)
    {
        Reply=&R;Active=true;
        if(!DeclarationEnumGlobals()||IsPlaySessionActive()) { return Bad(TEXT("declaration_enum_editor_required"),TEXT("Safe Editor GT context required before enum asset load.")); }
        Editor=GEditor;World=GEditor->GetEditorWorldContext().World();if(!World.IsValid()||World->bIsTearingDown) { return Bad(TEXT("declaration_enum_editor_required"),TEXT("Current Editor world required.")); }
        FString P=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();P.TrimQuotesInline();P.ReplaceInline(TEXT("\\"),TEXT("/"));
        if(!SafeName(Input)||(!P.StartsWith(TEXT("/Game/"))&&!P.StartsWith(TEXT("/Engine/")))) { return Bad(TEXT("declaration_enum_descriptor_pending"),TEXT("This enum extension consumes exact Game/Engine UserDefinedEnum assets; native/custom-module/slow-find variants remain pending.")); }
        const FString Low=P.ToLower();for(const TCHAR* Token:{TEXT("cubelessmcp"),TEXT("cubelesscontrolrigtools"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal")}) { if(Low.Contains(Token)) { return Bad(TEXT("declaration_enum_dependency_forbidden"),TEXT("Enum asset dependency points to authoring bridge content.")); } }
        if(!P.Contains(TEXT("."))) { P+=TEXT(".")+FPackageName::GetShortName(P); }
        if(P.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(P)) { return Bad(TEXT("declaration_enum_path_invalid"),TEXT("Raw and expanded exact enum asset object path must fit NAME_SIZE.")); }
        LoadEntered=true;R.Data->SetNumberField(TEXT("enum_load_calls"),1);UEnum* E=LoadObject<UEnum>(nullptr,*P);R.Data->SetNumberField(TEXT("enum_load_returns"),1);R.Data->SetBoolField(TEXT("enum_load_global_effects_unverified"),true);
        if(!Context()) { return Bad(TEXT("declaration_enum_context_changed"),TEXT("Enum load returned after context loss; return UObject was not observed/adopted.")); }
        if(!DeclarationEnumLoaded(E)||E->GetClass()!=UUserDefinedEnum::StaticClass()||E->GetPathName()!=P||E->GetOuter()!=E->GetPackage()||!E->HasAnyFlags(RF_Public)
            ||E->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||E->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly))
        { return Bad(TEXT("declaration_enum_source_unsupported"),TEXT("Exact runtime public UserDefinedEnum asset required; redirected/custom/transient source is not substituted.")); }
        Source.Reset(E);SourcePackage.Reset(E->GetPackage());SourceOuter=E->GetOuter();SourceClass=E->GetClass();SourcePath=P;SourceFlags=E->GetFlags();SourceDirty=SourcePackage->IsDirty();SourcePackageFlags=uint32(SourcePackage->GetPackageFlags());SourcePackageObjectFlags=SourcePackage->GetFlags();
        auto J=MakeShared<FJsonObject>();if(!Snapshot(J)) { return false; }Before=J;R.Data->SetStringField(TEXT("enum_asset_path"),P);R.Data->SetObjectField(TEXT("enum_source_before"),J);
        R.Data->SetBoolField(TEXT("enum_source_read_only"),true);R.Data->SetBoolField(TEXT("enum_source_initial_dirty"),SourceDirty);return Check();
    }
bool Value(const TSharedPtr<FJsonValue>& V,FString& Out)
    {
        if(!Check()) { return false; }int32 Index=INDEX_NONE;UEnum* E=Source.Get();
        if(V.IsValid()&&V->Type==EJson::String)
        {
            FString Name=V->AsString().TrimStartAndEnd();Name.TrimQuotesInline();if(Name.IsEmpty()||Name.Len()>512) { return Bad(TEXT("declaration_enum_value_invalid"),TEXT("Enum value must be a bounded nonempty entry name or integral numeric value.")); }
            Index=E->GetIndexByNameString(Name);if(Index==INDEX_NONE&&!Name.Contains(TEXT("::"))) { Index=E->GetIndexByNameString(E->GetName()+TEXT("::")+Name); }
            if(Index==INDEX_NONE) { const FString Full=E->GenerateFullEnumName(*Name);if(!Check()) { return false; }Index=E->GetIndexByNameString(Full); }
        }
        else if(V.IsValid()&&V->Type==EJson::Number)
        {
            const double D=V->AsNumber();if(!FMath::IsFinite(D)||D<0||D>255||FMath::TruncToDouble(D)!=D) { return Bad(TEXT("declaration_enum_value_invalid"),TEXT("Enum numeric default must be an exact byte-valued entry; no truncation.")); }Index=E->GetIndexByValue(int64(D));
        }
        if(!Check()) { return false; }
        if(Index==INDEX_NONE||Index>=E->NumEnums()-1||E->HasMetaData(TEXT("Hidden"),Index)||E->HasMetaData(TEXT("Spacer"),Index)) { return Bad(TEXT("declaration_enum_value_invalid"),TEXT("Enum entry is missing, hidden, spacer, MAX or is not a string/integer default.")); }
        Out=E->GetNameStringByIndex(Index);return Check();
    }

    UEdGraphNode* CurrentEditable(const FGuid& Guid)
    { if(!Admit()) { return nullptr; }for(const auto& Role:Editable) { if(Role.Guid==Guid) { return Role.Hold.Get(); } }Bad(TEXT("declaration_enum_editable_changed"),TEXT("Expected editable role was not captured."));return nullptr; }
    bool PinRole(UEdGraphNode* N,const FName Name,EEdGraphPinDirection Direction,const FEdGraphPinType& Type,FGuid& PinId,bool CaptureId=false)
    {
        if(!Admit()||!DeclarationEnumLoaded(N)||N->Pins.Num()>512) { return Bad(TEXT("declaration_enum_pin_changed"),TEXT("Current bounded editable pin array unavailable.")); }
        UEdGraphPin* Selected=nullptr;TSet<FGuid> Ids;
        for(UEdGraphPin* P:N->Pins)
        {
            if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||Ids.Contains(P->PinId)) { return Bad(TEXT("declaration_enum_pin_changed"),TEXT("Current editable pin IDs/ownership are invalid.")); }Ids.Add(P->PinId);
            if(P->PinName==Name&&P->Direction==Direction) { if(Selected) { return Bad(TEXT("declaration_enum_pin_changed"),TEXT("Requested parameter pin is ambiguous.")); }Selected=P; }
        }
        if(!Selected||Selected->PinType!=Type||Selected->PinType.PinSubCategoryObject.Get()!=Source.Get()||Selected->bOrphanedPin||Selected->ParentPin||!Selected->SubPins.IsEmpty()||!Selected->LinkedTo.IsEmpty()||Selected->DefaultObject||!Selected->DefaultTextValue.IsEmpty()
            ||(!CaptureId&&Selected->PinId!=PinId)) { return Bad(TEXT("declaration_enum_pin_changed"),TEXT("Added enum pin no longer has the exact name/direction/type/ID/default role.")); }
        if(CaptureId) { PinId=Selected->PinId;Reply->Data->SetStringField(TEXT("selected_parameter_pin_id"),PinId.ToString(EGuidFormats::Digits)); }
        return true;
    }

    bool Target(bool CheckClasses=true,bool CheckNodes=true)
    {
        if(LocalMapMode)return MapTarget(CheckClasses,CheckNodes);
        if(!Blueprint.IsValid()) { return true; }UBlueprint* B=Blueprint.Get();
        if(!DeclarationEnumLoaded(B)||!DeclarationEnumLoaded(TargetPackage.Get())||B->GetClass()!=BlueprintClass||B->GetOuter()!=BlueprintOuter||B->GetPackage()!=TargetPackage.Get()||!B->GetPathName().Equals(BlueprintPath,ESearchCase::CaseSensitive)
            ||B->ParentClass.Get()!=Parent.Get()||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||TargetPackage->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_CompiledIn|PKG_PlayInEditor))
        { return Bad(TEXT("declaration_enum_target_changed"),TEXT("Exact selected Blueprint/parent/package role changed; no next SDK or cleanup.")); }
        if(CheckClasses&&(B->GeneratedClass.Get()!=Generated.Get()||B->SkeletonGeneratedClass.Get()!=Skeleton.Get()
            ||!DeclarationEnumLoaded(Generated.Get())||!DeclarationEnumLoaded(Skeleton.Get())||!DeclarationEnumLoaded(Default.Get())
            ||Generated->ClassGeneratedBy!=B||Skeleton->ClassGeneratedBy!=B||Generated->GetPackage()!=TargetPackage.Get()||Skeleton->GetPackage()!=TargetPackage.Get()
            ||Generated->GetClass()!=GeneratedMetaClass||Skeleton->GetClass()!=SkeletonMetaClass||Generated->GetOuter()!=GeneratedOuter||Skeleton->GetOuter()!=SkeletonOuter
            ||Generated->GetPathName()!=GeneratedPath||Skeleton->GetPathName()!=SkeletonPath||Generated->GetFlags()!=GeneratedFlags||Skeleton->GetFlags()!=SkeletonFlags
            ||uint32(Generated->GetClassFlags())!=GeneratedClassFlags||uint32(Skeleton->GetClassFlags())!=SkeletonClassFlags||Default->GetOuter()!=DefaultOuter||Default->GetPathName()!=DefaultPath||Default->GetFlags()!=DefaultFlags
            ||Generated->GetDefaultObject(false)!=Default.Get()||Default->GetClass()!=Generated.Get()||Default->GetOuter()!=Generated->GetOuter()||Default->GetPackage()!=TargetPackage.Get()
            ||!Default->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)))
        { return Bad(TEXT("declaration_enum_generated_state_changed"),TEXT("Current class/CDO identity changed outside an explicit structural compiler return boundary.")); }
        if(Graph.IsValid())
        {
            UEdGraph* G=Graph.Get();if(!DeclarationEnumLoaded(G)||G->GetOuter()!=B||G->GetPackage()!=TargetPackage.Get()||G->GetPathName()!=GraphPath||G->GraphGuid!=GraphGuid
                ||!B->FunctionGraphs.Contains(G)||!G->bEditable||G->Schema!=UEdGraphSchema_K2::StaticClass())
            { return Bad(TEXT("declaration_enum_graph_changed"),TEXT("Selected exact directly owned function graph/schema role changed.")); }
            if(CheckNodes)
            {
                for(const auto& Role:Editable)
                {
                    UEdGraphNode* N=Role.Hold.Get();int32 Count=0;for(const auto& It:G->Nodes) { if(It.Get()==N) { ++Count; } }
                    if(!DeclarationEnumLoaded(N)||Count!=1||N->GetOuter()!=G||N->GetPackage()!=TargetPackage.Get()||N->GetClass()!=Role.Class||N->NodeGuid!=Role.Guid||N->GetPathName()!=Role.Path)
                    { return Bad(TEXT("declaration_enum_editable_changed"),TEXT("Captured Entry/Result node identity or function-graph membership changed.")); }
                }
            }
        }
        return true;
    }
    bool Admit() { if(!Identity()||!Target())return false;if(LocalMapMode&&PresetMapLibrary)for(const auto& Role:Editable)if(Role.Class==UK2Node_FunctionEntry::StaticClass()&&!PresetEntry(static_cast<UK2Node_FunctionEntry*>(Role.Hold.Get())))return false;return true; }
    bool Check()
    {
        if(!Admit()) { return false; }auto Now=MakeShared<FJsonObject>();if(!Snapshot(Now)) { return false; }
        if(!DeclarationEnumEqual(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(Now)))
        { return Bad(NativeStructMode?TEXT("local_struct_source_changed"):NativeClassMode?TEXT("local_class_source_changed"):TEXT("declaration_enum_source_changed"),TEXT("Read-only descriptor source snapshot differs; no next SDK, cleanup or retry.")); }
        if(LocalMapMode){Reply->Data->SetObjectField(TEXT("local_map_source_after"),Now);Reply->Data->SetBoolField(TEXT("local_map_source_observed_unchanged"),true);return Admit();}Reply->Data->SetBoolField(NativeStructMode?TEXT("struct_source_observed_unchanged"):NativeClassMode?TEXT("class_source_observed_unchanged"):TEXT("enum_source_observed_unchanged"),true);return Admit();
    }
    bool BindBlueprint(UBlueprint* B)
    {
        // Caller has just received a public load return: no enum-source SDK before strong adoption.
        if(!Identity()) { return false; }
        if(!DeclarationEnumLoaded(B)||B->GetClass()!=UBlueprint::StaticClass()||B->GetOuter()!=B->GetPackage()||B->GetPackage()==SourcePackage.Get())
        { return Bad(TEXT("declaration_enum_target_unsupported"),TEXT("Current runtime Blueprint asset in a separate package required.")); }
        Blueprint.Reset(B);TargetPackage.Reset(B->GetPackage());if(LocalMapMode){PresetMapLibrary=B->BlueprintType==BPTYPE_FunctionLibrary;UClass* ExpectedParent=PresetMapLibrary?UBlueprintFunctionLibrary::StaticClass():AActor::StaticClass();if(!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()!=ExpectedParent)return Bad(TEXT("local_map_parent_unsupported"),TEXT("Only direct native Actor or exact PresetLoader function-library local scope is admitted."));Parent.Reset(ExpectedParent);if(PresetMapLibrary)Reply->Data->SetStringField(TEXT("function_library_profile"),TEXT("preset_loader"));}else Parent.Reset(B->ParentClass.Get());BlueprintOuter=B->GetOuter();BlueprintClass=B->GetClass();BlueprintPath=B->GetPathName();
        return RefreshClasses();
    }
    bool BindGraph(UEdGraph* G)
    {
        if(!Admit()||!DeclarationEnumLoaded(G)||G->GetOuter()!=Blueprint.Get()) { return Bad(TEXT("declaration_enum_graph_changed"),TEXT("Selected local function graph unavailable.")); }
        Graph.Reset(G);GraphPath=G->GetPathName();GraphGuid=G->GraphGuid;return Check();
    }
    bool BindEditable(UEdGraphNode* N)
    {
        if(!Admit()||!DeclarationEnumLoaded(N)||N->GetOuter()!=Graph.Get()||N->GetPackage()!=TargetPackage.Get()||!N->NodeGuid.IsValid())
        { return Bad(TEXT("declaration_enum_editable_changed"),TEXT("Current Entry/Result return cannot be strongly adopted in the selected graph.")); }
        for(const auto& Role:Editable) { if(Role.Hold.Get()==N) { return Check(); } }
        FEditableRole Role;Role.Hold.Reset(N);Role.Guid=N->NodeGuid;Role.Class=N->GetClass();Role.Path=N->GetPathName();Editable.Add(MoveTemp(Role));return Check();
    }
    bool RefreshClasses()
    {
        // Only call at initial binding or directly after the explicit structural SDK returned.
        // Past generated/skeleton/CDO objects are held history and are never dereferenced here.
        if(LocalMapMode)return RefreshMapClasses();
        if(!Identity()||!Target(false,false)) { return false; }UBlueprint* B=Blueprint.Get();
        UClass* NextGenerated=B->GeneratedClass.Get();UClass* NextSkeleton=B->SkeletonGeneratedClass.Get();
        if(!DeclarationEnumLoaded(NextGenerated)||!DeclarationEnumLoaded(NextSkeleton)||NextGenerated->ClassGeneratedBy!=B||NextSkeleton->ClassGeneratedBy!=B
            ||NextGenerated->GetPackage()!=TargetPackage.Get()||NextSkeleton->GetPackage()!=TargetPackage.Get())
        { return Bad(TEXT("declaration_enum_generated_state_unavailable"),TEXT("Structural return has no current exact owned generated/skeleton classes.")); }
        UObject* NextDefault=NextGenerated->GetDefaultObject(false);
        if(!DeclarationEnumLoaded(NextDefault)||NextDefault->GetClass()!=NextGenerated||NextDefault->GetOuter()!=NextGenerated->GetOuter()||NextDefault->GetPackage()!=TargetPackage.Get()
            ||!NextDefault->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
        { return Bad(TEXT("declaration_enum_generated_state_unavailable"),TEXT("Current generated class CDO role unavailable after structural return.")); }
        Generated.Reset(NextGenerated);Skeleton.Reset(NextSkeleton);Default.Reset(NextDefault);
        GeneratedMetaClass=NextGenerated->GetClass();SkeletonMetaClass=NextSkeleton->GetClass();GeneratedOuter=NextGenerated->GetOuter();SkeletonOuter=NextSkeleton->GetOuter();DefaultOuter=NextDefault->GetOuter();
        GeneratedPath=NextGenerated->GetPathName();SkeletonPath=NextSkeleton->GetPathName();DefaultPath=NextDefault->GetPathName();GeneratedFlags=NextGenerated->GetFlags();SkeletonFlags=NextSkeleton->GetFlags();DefaultFlags=NextDefault->GetFlags();
        GeneratedClassFlags=uint32(NextGenerated->GetClassFlags());SkeletonClassFlags=uint32(NextSkeleton->GetClassFlags());
        for(auto& Role:Editable)
        {
            UEdGraphNode* Current=nullptr;for(const auto& It:Graph->Nodes)
            { UEdGraphNode* Candidate=It.Get();if(DeclarationEnumLoaded(Candidate)&&Candidate->NodeGuid==Role.Guid) { if(Current) { return Bad(TEXT("declaration_enum_editable_changed"),TEXT("Duplicate current editable node GUID after structural return.")); }Current=Candidate; } }
            if(!Current||Current->GetClass()!=Role.Class||Current->GetOuter()!=Graph.Get()||Current->GetPathName()!=Role.Path)
            { return Bad(TEXT("declaration_enum_editable_changed"),TEXT("Original Entry/Result role cannot be reacquired after structural return.")); }Role.Hold.Reset(Current);
        }
        return Check();
    }
};


struct FLifecycleObservation
{
    bool Complete = false;
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), Source = MakeShared<FJsonObject>(), Other = MakeShared<FJsonObject>(), Disk = MakeShared<FJsonObject>(), References = MakeShared<FJsonObject>();
};
void LifecycleCount(FResult& R, const TCHAR* Key) { R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
void LifecycleInit(FResult& R)
{
    for (const TCHAR* K : { TEXT("outer_transaction_calls"), TEXT("explicit_modify_calls"), TEXT("metadata_descriptor_set_calls"), TEXT("metadata_editor_set_calls"), TEXT("metadata_editor_remove_calls"), TEXT("descriptor_flag_edit_calls"), TEXT("metadata_readback_get_calls"), TEXT("explicit_mark_modified_calls"), TEXT("explicit_mark_structural_calls"), TEXT("explicit_mark_package_dirty_calls"), TEXT("full_compile_attempts"), TEXT("save_attempt_count"), TEXT("lifecycle_observation_capture_calls") }) { R.Data->SetNumberField(K, 0); }
    for (const TCHAR* K : { TEXT("observations_complete"), TEXT("nonselected_state_preserved"), TEXT("source_preserved"), TEXT("references_preserved"), TEXT("other_package_dirty_preserved"), TEXT("disk_preserved_before_save"), TEXT("save_state_preserved") }) { R.Data->SetBoolField(K, false); }
    R.Data->SetStringField(TEXT("counter_scope"), TEXT("Explicit adapter call/application counts; helper-internal Modify/storage/skeleton/caches/delegates are not counted as zero. Flag edit counts are logical requested applications, not physical stores.")); R.Data->SetArrayField(TEXT("metadata_calls"), {}); R.Data->SetArrayField(TEXT("save_calls"), {});
    R.Data->SetBoolField(TEXT("implicit_skeleton_compile_possible"), true); R.Data->SetField(TEXT("actual_implicit_skeleton_compile_count"), MakeShared<FJsonValueNull>());
    R.Data->SetBoolField(TEXT("implicit_cache_child_notification_counts_instrumented"), false); R.Data->SetBoolField(TEXT("legacy_mutation_result_v1_emitted"), false);
    R.Data->SetStringField(TEXT("admission_scope"), TEXT("Exact native Actor parent; current locally declared simple scalar variables; no loaded external instances/derived Blueprints. Generated/inherited/local-function/display-RPC variants remain incomplete."));
    R.Data->SetStringField(TEXT("transaction_scope"), TEXT("One original Editor transaction after finite admission. Earlier admission refusals do not open the original pre-handler transaction; Python journal is separate."));
    R.Data->SetStringField(TEXT("observation_scope"), TEXT("Local descriptors/current values, non-transient reflected Actor CDO fields, graph nodes/pins, native Actor source/CDO, target disk and other loaded package dirtiness. Unreflected caches, unloaded assets, global delegates and initial load effects are not fully measured."));
}
TSharedRef<FJsonObject> LifecycleIdentity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), IsValid(O)); J->SetStringField(TEXT("path"), IsValid(O) ? O->GetPathName() : FString()); J->SetStringField(TEXT("class"), IsValid(O) ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), IsValid(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString()); J->SetStringField(TEXT("object_flags"), IsValid(O) ? LexToString(static_cast<uint64>(O->GetFlags())) : FString()); return J;
}
bool LifecycleEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, int32 Depth = 0)
{
    if (!A || !B || A->Type != B->Type || Depth > 32) { return false; }
    if (A->Type == EJson::Object) { const auto X = A->AsObject(), Y = B->AsObject(); if (!X || !Y || X->Values.Num() != Y->Values.Num()) { return false; } for (const auto& P : X->Values) { if (!LifecycleEqual(P.Value, Y->TryGetField(P.Key.ToView()), Depth + 1)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!LifecycleEqual(X[I], Y[I], Depth + 1)) { return false; } } return true; }
    return A->Type == EJson::String ? A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive) : FJsonValue::CompareEqual(*A, *B);
}
bool LifecycleEqual(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B) { return LifecycleEqual(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
bool LifecycleFields(UObject* O, const TSharedRef<FJsonObject>& Out, FDeclarationEnumScope* EnumScope = nullptr, TFunction<bool()> Fence = {})
{
    if ((EnumScope && !EnumScope->Admit()) || !IsValid(O)) { return false; }
    TStrongObjectPtr<UObject> EnumHold(EnumScope ? O : nullptr);const FString EnumPath=EnumScope ? O->GetPathName() : FString();UClass* EnumClass=EnumScope ? O->GetClass() : nullptr;UObject* EnumOuter=EnumScope ? O->GetOuter() : nullptr; int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(O->GetClass()); It; ++It)
    {
        FProperty* P = *It; if (P->HasAnyPropertyFlags(CPF_Transient)) { continue; }
        if (++Count > 512 || P->ArrayDim != 1) { return false; }
        auto J = MakeShared<FJsonObject>(); Out->SetObjectField(P->GetName(), J); J->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); J->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        FString Value; const void* Address = P->ContainerPtrToValuePtr<void>(O); const bool Exported=P->ExportText_Direct(Value, Address, Address, O, PPF_None);
        if(Fence&&!Fence())return false;
        if(EnumScope && (!EnumScope->Admit()||!DeclarationEnumLoaded(O)||O->GetClass()!=EnumClass||O->GetOuter()!=EnumOuter||O->GetPathName()!=EnumPath)) { return false; }
        if (!Exported || !SafeText(Value, 65536) || (Chars += Value.Len()) > 1048576) { return false; } J->SetStringField(TEXT("export_text"), Value);
    }
    return true;
}
bool LifecycleGraphs(UBlueprint* BP, const TSharedRef<FJsonObject>& Out, TFunction<bool()> Fence = {})
{
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); if (Fence && !Fence()) { return false; } if (Graphs.Num() > 128) { return false; } int32 Nodes = 0, Pins = 0, Links = 0;
    for (UEdGraph* G : Graphs)
    {
        if (!IsValid(G) || G->GetOutermost() != BP->GetOutermost()) { return false; } auto J = MakeShared<FJsonObject>(); Out->SetObjectField(G->GetPathName(), J); J->SetObjectField(TEXT("identity"), LifecycleIdentity(G)); J->SetStringField(TEXT("guid"), G->GraphGuid.ToString(EGuidFormats::Digits));
        auto Rows = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("nodes"), Rows);
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!IsValid(N) || N->GetGraph() != G || ++Nodes > 8192) { return false; } auto Row = MakeShared<FJsonObject>(); Rows->SetObjectField(N->GetPathName(), Row); Row->SetObjectField(TEXT("identity"), LifecycleIdentity(N)); Row->SetStringField(TEXT("guid"), N->NodeGuid.ToString(EGuidFormats::Digits)); Row->SetNumberField(TEXT("x"), N->NodePosX); Row->SetNumberField(TEXT("y"), N->NodePosY);
            TArray<TSharedPtr<FJsonValue>> PinRows; Row->SetArrayField(TEXT("pins"), PinRows);
            for (UEdGraphPin* P : N->Pins)
            {
                if (!P || P->WasTrashed() || P->GetOwningNodeUnchecked() != N || ++Pins > 32768) { return false; }
                auto Q = MakeShared<FJsonObject>(); PinRows.Add(MakeShared<FJsonValueObject>(Q)); Row->SetArrayField(TEXT("pins"), PinRows); Q->SetStringField(TEXT("id"), P->PinId.ToString(EGuidFormats::Digits)); Q->SetStringField(TEXT("name"), P->PinName.ToString()); Q->SetNumberField(TEXT("direction"), static_cast<int32>(P->Direction)); Q->SetObjectField(TEXT("type"), PinJson(P->PinType)); Q->SetStringField(TEXT("default_value"), P->DefaultValue); Q->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString()); Q->SetStringField(TEXT("default_object"), P->DefaultObject ? P->DefaultObject->GetPathName() : FString());
                TArray<TSharedPtr<FJsonValue>> LinkRows; Q->SetArrayField(TEXT("links"), LinkRows);
                for (UEdGraphPin* L : P->LinkedTo) { if (!L || L->WasTrashed() || !IsValid(L->GetOwningNodeUnchecked()) || ++Links > 131072) { return false; } auto A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("node"), L->GetOwningNode()->GetPathName()); A->SetStringField(TEXT("pin"), L->PinName.ToString()); A->SetStringField(TEXT("id"), L->PinId.ToString(EGuidFormats::Digits)); LinkRows.Add(MakeShared<FJsonValueObject>(A)); Q->SetArrayField(TEXT("links"), LinkRows); }
            }
        }
    }
    return true;
}
bool LifecycleReferences(UBlueprint* BP, const TSharedRef<FJsonObject>& Out, FDeclarationEnumScope* EnumScope = nullptr, TFunction<bool()> Fence = {})
{
    if((Fence&&!Fence())||(EnumScope&&!EnumScope->Admit())) { return false; }
    TArray<UObject*> Objects; GetObjectsWithOuter(BP->GetOutermost(), Objects, EGetObjectsFlags::IncludeNestedObjects); if (Objects.Num() > 4096) { return false; } int32 Total = 0;
    for (UObject* Object : Objects)
    {
        if (!IsValid(Object) || Object->HasAnyFlags(RF_Transient) || Object->GetOutermost() != BP->GetOutermost()) { continue; }
        TStrongObjectPtr<UObject> EnumHold(EnumScope || Fence ? Object : nullptr);const FString EnumPath=EnumScope || Fence ? Object->GetPathName() : FString();UClass* EnumClass=EnumScope || Fence ? Object->GetClass() : nullptr;UObject* EnumOuter=EnumScope || Fence ? Object->GetOuter() : nullptr;
        TArray<UObject*> Refs; FReferenceFinder Finder(Refs, nullptr, false, true, false, true); Finder.FindReferences(Object);
        if(Fence&&(!Fence()||!DeclarationEnumLoaded(Object)||Object->GetClass()!=EnumClass||Object->GetOuter()!=EnumOuter||Object->GetPathName()!=EnumPath))return false;
        if(EnumScope&&(!EnumScope->Admit()||!DeclarationEnumLoaded(Object)||Object->GetClass()!=EnumClass||Object->GetOuter()!=EnumOuter||Object->GetPathName()!=EnumPath)) { return false; }
        TArray<FString> Paths; Out->SetArrayField(Object->GetPathName(), {});
        for (UObject* Ref : Refs)
        {
            if (!IsValid(Ref) || ++Total > 16384) { return false; } const FString P = Ref->GetPathName(); Paths.AddUnique(P); Out->SetArrayField(Object->GetPathName(), StringValues(Paths));
            for (const TCHAR* Token : { TEXT("/Script/CubelessMCP"), TEXT("/Script/UnrealMCP"), TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/") }) { if (P.Contains(Token, ESearchCase::IgnoreCase)) { return false; } }
        }
        Paths.Sort(); Out->SetArrayField(Object->GetPathName(), StringValues(Paths));
    }
    return true;
}
bool LifecycleCapture(UBlueprint* BP, FLifecycleObservation& O, FResult& R, const TCHAR* Phase, FDeclarationEnumScope* EnumScope = nullptr,
    TFunction<bool()> Fence = {}, TFunction<bool(const TSharedRef<FJsonObject>&)> GraphCapture = {}, TFunction<bool(const TSharedRef<FJsonObject>&)> ReferenceCapture = {}, UClass* NativeSourceParent = nullptr)
{
    if(EnumScope&&!EnumScope->Admit()) { return false; }
    O = FLifecycleObservation{}; LifecycleCount(R, TEXT("lifecycle_observation_capture_calls")); auto J = MakeShared<FJsonObject>(); R.Data->SetObjectField(Phase, J); J->SetBoolField(TEXT("complete"), false); J->SetStringField(TEXT("failed_stage"), TEXT("current_class")); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("source"), O.Source); J->SetObjectField(TEXT("other_package_dirty"), O.Other); J->SetObjectField(TEXT("disk"), O.Disk); J->SetObjectField(TEXT("references"), O.References);
    const bool PresetCapture=EnumScope&&EnumScope->LocalMapMode&&EnumScope->PresetMapLibrary;
    if(PresetCapture&&(!EnumScope->Check()||!EnumScope->Default.IsValid()))return false;
    if (!IsValid(BP) || (!PresetCapture&&(!IsValid(BP->GeneratedClass) || !BP->GeneratedClass->GetDefaultObject(false)))) { return false; }
    O.State->SetObjectField(TEXT("variables"), PresetCapture?Capture(BP,EnumScope->Default.Get(),true):Fence?Capture(BP,BP->GeneratedClass->ClassDefaultObject.Get(),true,Fence):Capture(BP)); if(Fence&&!Fence())return false; O.State->SetObjectField(TEXT("blueprint_identity"), LifecycleIdentity(BP)); UObject* CDO = PresetCapture?EnumScope->Default.Get():BP->GeneratedClass->GetDefaultObject(false); O.State->SetObjectField(TEXT("cdo_identity"), LifecycleIdentity(CDO)); auto Fields = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("cdo_fields"), Fields); auto Graphs = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("graphs"), Graphs);
    J->SetStringField(TEXT("failed_stage"), TEXT("local_current_values")); for (const auto& V : O.State->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("new_variables"))) { if (!V->AsObject()->GetBoolField(TEXT("default_value_available")) || !V->AsObject()->GetBoolField(TEXT("generated_property_available"))) { return false; } }
    J->SetStringField(TEXT("failed_stage"), TEXT("cdo_fields")); if (CDO->GetOutermost() != BP->GetOutermost() || !LifecycleFields(CDO, Fields, EnumScope, Fence)) { return false; } J->SetStringField(TEXT("failed_stage"), TEXT("graphs")); if (!(GraphCapture?GraphCapture(Graphs):LifecycleGraphs(BP, Graphs))) { return false; } J->SetStringField(TEXT("failed_stage"), TEXT("selected_serialized_references")); if (!(ReferenceCapture?ReferenceCapture(O.References):LifecycleReferences(BP, O.References, EnumScope))) { return false; }
    J->SetStringField(TEXT("failed_stage"), TEXT("native_source")); UClass* Parent = NativeSourceParent?NativeSourceParent:EnumScope&&EnumScope->LocalMapMode&&EnumScope->PresetMapLibrary?UBlueprintFunctionLibrary::StaticClass():AActor::StaticClass(); UObject* ParentCDO = Parent->GetDefaultObject(false);TStrongObjectPtr<UObject> PresetParentCDOHold;if(PresetCapture){if(!EnumScope->Check()||!DeclarationEnumLoaded(ParentCDO)||!Parent->ClassDefaultObject.IsResolved()||Parent->ClassDefaultObject!=ParentCDO||ParentCDO->GetClass()!=Parent)return false;PresetParentCDOHold.Reset(ParentCDO);} O.Source->SetObjectField(TEXT("class"), LifecycleIdentity(Parent)); O.Source->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint32>(Parent->GetClassFlags()))); O.Source->SetBoolField(TEXT("package_dirty"), Parent->GetOutermost()->IsDirty()); O.Source->SetObjectField(TEXT("cdo"), LifecycleIdentity(ParentCDO)); auto SourceFields = MakeShared<FJsonObject>(); O.Source->SetObjectField(TEXT("cdo_fields"), SourceFields); if (!LifecycleFields(ParentCDO, SourceFields, EnumScope, Fence)) { return false; }
    J->SetStringField(TEXT("failed_stage"), TEXT("other_package_dirty")); const FString Package = BP->GetOutermost()->GetName(); int32 Count = 0;
    for (TObjectIterator<UPackage> It; It; ++It) { if (!IsValid(*It) || It->GetName() == Package) { continue; } if (++Count > 8192) { return false; } O.Other->SetBoolField(It->GetName(), It->IsDirty()); }
    J->SetStringField(TEXT("failed_stage"), TEXT("disk")); FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(Package, Filename, FPackageName::GetAssetPackageExtension())) { return false; } Filename = FPaths::ConvertRelativePathToFull(Filename); O.Disk->SetStringField(TEXT("path"), Filename); const int64 Size = IFileManager::Get().FileSize(*Filename); O.Disk->SetBoolField(TEXT("exists"), Size >= 0); O.Disk->SetNumberField(TEXT("bytes"), Size < 0 ? 0 : Size);
    if (Size > 67108864) { return false; } if (Size >= 0) { TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; } FSHAHash Hash; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash.Hash); O.Disk->SetStringField(TEXT("sha1"), Hash.ToString()); }
    O.Complete = true; J->SetBoolField(TEXT("complete"), true); J->SetStringField(TEXT("failed_stage"), TEXT("")); return true;
}
bool LifecycleSimple(UBlueprint* BP, FResult& R)
{
    if (!MutationScope(BP, R)) { return false; }
    if (BP->ParentClass != AActor::StaticClass() || !IsValid(BP->GeneratedClass) || !BP->GeneratedClass->GetDefaultObject(false) || !BP->GeneratedVariables.IsEmpty() || BP->CachedUDSDependencies.Num() > 0 || BP->NewVariables.Num() > 256)
    { return Fail(R, TEXT("variable_lifecycle_scope_unsupported"), TEXT("Exact native Actor/current local simple members without generated variables or UDS dependencies required; no descriptor edit.")); }
    for (const auto& V : BP->NewVariables)
    {
        const FName K = V.VarType.PinCategory; if (V.VarType.IsContainer() || V.VarType.bIsReference || V.VarType.bIsConst || !(K == UEdGraphSchema_K2::PC_Boolean || K == UEdGraphSchema_K2::PC_Int || K == UEdGraphSchema_K2::PC_Int64 || K == UEdGraphSchema_K2::PC_Real || K == UEdGraphSchema_K2::PC_String || K == UEdGraphSchema_K2::PC_Name || K == UEdGraphSchema_K2::PC_Text)) { return Fail(R, TEXT("variable_lifecycle_type_unsupported"), TEXT("First lifecycle slice preserves simple local scalar fields only; other original variants remain incomplete.")); }
        const auto J = Row(BP, V, TEXT("new"), 0); if (!J->GetBoolField(TEXT("default_value_available")) || !J->GetBoolField(TEXT("generated_property_available"))) { return Fail(R, TEXT("variable_lifecycle_current_class_required"), TEXT("Every local member must already have an observable current generated CDO field; compile through existing tools first.")); }
    }
    return true;
}
bool LifecycleFlag(const FString& Json, TOptional<bool>& Out, FResult& R)
{ TSharedPtr<FJsonValue> Value; if (!Parse(Json, Value, R)) { return false; } if (Value->Type == EJson::Null) { return true; } if (Value->Type != EJson::Boolean) { return Fail(R, TEXT("variable_flag_invalid"), TEXT("Optional flag requires JSON null or exact boolean.")); } Out = Value->AsBool(); return true; }
bool LifecycleIdentitySame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, bool Saved, bool CompiledCDO)
{
    for (const auto& P : A->Values)
    {
        const auto Q = B->TryGetField(P.Key.ToView()); if (!Q) { return false; }
        if (P.Key.ToView() != TEXT("object_flags")) { if (!LifecycleEqual(P.Value, Q)) { return false; } continue; }
        if (P.Value->Type != EJson::String || Q->Type != EJson::String) { return false; } const FString Old = P.Value->AsString(), Now = Q->AsString(); const uint64 F = FCString::Strtoui64(*Old, nullptr, 10), G = FCString::Strtoui64(*Now, nullptr, 10), Load = static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted);
        if (LexToString(F) != Old || LexToString(G) != Now) { return false; }
        if (Saved) { if (G != (F | Load)) { return false; } }
        else if (CompiledCDO) { const uint64 CDOFlags = static_cast<uint64>(RF_ClassDefaultObject | RF_ArchetypeObject); if ((F & CDOFlags) != CDOFlags || (G & CDOFlags) != CDOFlags || (F & ~Load) != (G & ~Load) || !((F & Load) == (G & Load) || (F & Load) == Load && (G & Load) == 0)) { return false; } }
        else if (F != G) { return false; }
    }
    return A->Values.Num() == B->Values.Num();
}
bool LifecycleGraphSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, bool Saved, int32 Depth = 0)
{
    if (Depth > 8 || A->Values.Num() != B->Values.Num()) { return false; }
    for (const auto& P : A->Values)
    {
        const auto Q = B->TryGetField(P.Key.ToView()); if (!Q || P.Value->Type != Q->Type) { return false; }
        if (P.Key.ToView() == TEXT("identity")) { if (!LifecycleIdentitySame(P.Value->AsObject().ToSharedRef(), Q->AsObject().ToSharedRef(), Saved, false)) { return false; } }
        else if (P.Value->Type == EJson::Object) { if (!LifecycleGraphSame(P.Value->AsObject().ToSharedRef(), Q->AsObject().ToSharedRef(), Saved, Depth + 1)) { return false; } }
        else if (!LifecycleEqual(P.Value, Q)) { return false; }
    }
    return true;
}
bool LifecycleRowsSame(const TArray<TSharedPtr<FJsonValue>>& Before, const TArray<TSharedPtr<FJsonValue>>& After, const TArray<FName>& Order, const FBPVariableDescription* Desired, bool FullCompiled)
{
    if (Before.Num() != After.Num() || After.Num() != Order.Num()) { return false; }
    constexpr uint64 AllowedFlags = CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance | CPF_ExposeOnSpawn;
    for (int32 I = 0; I < After.Num(); ++I)
    {
        const auto B = After[I]->AsObject(); if (!B || FName(*B->GetStringField(TEXT("name"))) != Order[I]) { return false; } TSharedPtr<FJsonObject> A;
        for (const auto& V : Before) { if (FName(*V->AsObject()->GetStringField(TEXT("name"))) == Order[I]) { if (A) { return false; } A = V->AsObject(); } } if (!A) { return false; }
        const bool Selected = Desired && Desired->VarName == Order[I];
        for (const auto& P : A->Values)
        {
            const auto Q = B->TryGetField(P.Key.ToView()); if (!Q) { return false; }
            if (P.Key.ToView() == TEXT("index")) { if (Q->Type != EJson::Number || Q->AsNumber() != I) { return false; } continue; }
            if (P.Key.ToView() == TEXT("default_value_current_compile") || P.Key.ToView() == TEXT("generated_property_current_compile")) { if (Q->Type != EJson::Boolean) { return false; } continue; }
            if (Selected && (P.Key.ToView() == TEXT("metadata") || P.Key.ToView() == TEXT("property_flags") || P.Key.ToView() == TEXT("property_flags_string") || P.Key.ToView() == TEXT("instance_editable") || P.Key.ToView() == TEXT("blueprint_visible") || P.Key.ToView() == TEXT("expose_on_spawn"))) { continue; }
            if (Selected && P.Key.ToView() == TEXT("generated_property_flags_string")) { const uint64 X = FCString::Strtoui64(*P.Value->AsString(), nullptr, 10), Y = FCString::Strtoui64(*Q->AsString(), nullptr, 10); const uint64 Wanted = (X & ~AllowedFlags) | (Desired->PropertyFlags & AllowedFlags); if ((X & ~AllowedFlags) != (Y & ~AllowedFlags) || !(Y == Wanted || !FullCompiled && Y == X)) { return false; } continue; }
            if (!LifecycleEqual(P.Value, Q)) { return false; }
        }
        if (Selected && (!LifecycleEqual(Metadata(*Desired), B->GetObjectField(TEXT("metadata")).ToSharedRef()) || B->GetStringField(TEXT("property_flags_string")) != LexToString(Desired->PropertyFlags) || B->GetNumberField(TEXT("property_flags")) != static_cast<double>(Desired->PropertyFlags) || B->GetBoolField(TEXT("instance_editable")) != (!!(Desired->PropertyFlags & CPF_Edit) && !(Desired->PropertyFlags & CPF_DisableEditOnInstance)) || B->GetBoolField(TEXT("blueprint_visible")) != !!(Desired->PropertyFlags & CPF_BlueprintVisible) || B->GetBoolField(TEXT("expose_on_spawn")) != !!(Desired->PropertyFlags & CPF_ExposeOnSpawn))) { return false; }
    }
    return true;
}
bool LifecycleCdoFieldsSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, const FBPVariableDescription* Desired)
{
    if (A->Values.Num() != B->Values.Num()) { return false; }
    constexpr uint64 Mask = CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance | CPF_ExposeOnSpawn;
    for (const auto& P : A->Values)
    {
        const auto Q = B->TryGetField(P.Key.ToView()); if (!Q || P.Value->Type != EJson::Object || Q->Type != EJson::Object) { return false; } const auto X = P.Value->AsObject(), Y = Q->AsObject();
        if (!Desired || FName(*FString(P.Key.ToView())) != Desired->VarName) { if (!LifecycleEqual(P.Value, Q)) { return false; } continue; }
        if (X->Values.Num() != Y->Values.Num() || X->GetStringField(TEXT("owner")) != Y->GetStringField(TEXT("owner")) || X->GetStringField(TEXT("export_text")) != Y->GetStringField(TEXT("export_text"))) { return false; }
        const uint64 F = FCString::Strtoui64(*X->GetStringField(TEXT("flags")), nullptr, 10), G = FCString::Strtoui64(*Y->GetStringField(TEXT("flags")), nullptr, 10); if ((F & ~Mask) != (G & ~Mask) || !(G == F || G == ((F & ~Mask) | (Desired->PropertyFlags & Mask)))) { return false; }
    }
    return true;
}
FString LifecycleStatus(EBlueprintStatus Status)
{ switch (Status) { case BS_Dirty: return TEXT("dirty"); case BS_Error: return TEXT("error"); case BS_UpToDate: return TEXT("up_to_date"); case BS_BeingCreated: return TEXT("being_created"); case BS_UpToDateWithWarnings: return TEXT("up_to_date_with_warnings"); default: return TEXT("unknown"); } }
bool LifecyclePreserved(const FLifecycleObservation& Before, const FLifecycleObservation& After, const TArray<FName>& Order, const FBPVariableDescription* Desired, bool FullCompiled, bool Saved, bool DiskRequired, FResult& R)
{
    const auto A = Before.State->GetObjectField(TEXT("variables")), B = After.State->GetObjectField(TEXT("variables"));
    bool State = Before.Complete && After.Complete && LifecycleRowsSame(A->GetArrayField(TEXT("new_variables")), B->GetArrayField(TEXT("new_variables")), Order, Desired, FullCompiled)
        && LifecycleEqual(A->TryGetField(TEXT("generated_variables")), B->TryGetField(TEXT("generated_variables"))) && LifecycleEqual(A->TryGetField(TEXT("category_sorting")), B->TryGetField(TEXT("category_sorting")))
        && A->GetStringField(TEXT("generated_cdo_path")) == B->GetStringField(TEXT("generated_cdo_path")) && LifecycleCdoFieldsSame(Before.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), Desired)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), Saved, false)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), Saved, FullCompiled)
        && LifecycleGraphSame(Before.State->GetObjectField(TEXT("graphs")).ToSharedRef(), After.State->GetObjectField(TEXT("graphs")).ToSharedRef(), Saved);
    const bool Sources = Before.Complete && After.Complete && LifecycleEqual(Before.Source, After.Source), Refs = Before.Complete && After.Complete && LifecycleEqual(Before.References, After.References), Other = Before.Complete && After.Complete && LifecycleEqual(Before.Other, After.Other), Disk = Before.Complete && After.Complete && (!DiskRequired || LifecycleEqual(Before.Disk, After.Disk));
    if (Saved) { State &= !B->GetBoolField(TEXT("package_dirty")) && After.Disk->GetBoolField(TEXT("exists")) && After.Disk->GetNumberField(TEXT("bytes")) > 0 && Before.Disk->GetStringField(TEXT("path")) == After.Disk->GetStringField(TEXT("path")); }
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), State); R.Data->SetBoolField(TEXT("source_preserved"), Sources); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(Saved ? TEXT("save_state_preserved") : TEXT("disk_preserved_before_save"), Saved ? State && Sources && Refs && Other : Disk);
    return State && Sources && Refs && Other && Disk;
}
void LifecycleAggregateClear(FResult& R)
{ for (const TCHAR* K : { TEXT("observations_complete"), TEXT("nonselected_state_preserved"), TEXT("source_preserved"), TEXT("references_preserved"), TEXT("other_package_dirty_preserved"), TEXT("save_state_preserved") }) { R.Data->SetBoolField(K, false); } }
bool LifecycleObserved(UBlueprint* BP, FLifecycleObservation& O, const FLifecycleObservation& Before, FResult& R, const TCHAR* Phase)
{
    if (!LifecycleCapture(BP, O, R, Phase)) { R.bChangeStateKnown = false; return Fail(R, TEXT("variable_lifecycle_observation_incomplete"), TEXT("Bounded actual phase prefix is incomplete; inspect it without replaying the writer.")); }
    R.bChangeStateKnown = true; R.bChanged = !LifecycleEqual(Before.State, O.State) || !LifecycleEqual(Before.Source, O.Source) || !LifecycleEqual(Before.References, O.References) || !LifecycleEqual(Before.Other, O.Other) || !LifecycleEqual(Before.Disk, O.Disk); return true;
}
// ABD: API-owned metadata operations on direct native Actor/UObject owners.
// ABK reuses this API-local typed lease for original order/category writers.
// Metadata keeps its original mode; only SetOrder may adopt the exact planned
// declaration permutation. Names/GUIDs/types/values and source roles stay fixed;
// returned structural/full-compile boundaries may regenerate current field roles.
struct FMetadataTypedScope
{
    enum class EUse { Metadata, VariableOrder, CategoryOrder };
    EUse Use=EUse::Metadata;TArray<FName> AuthoredOrder;
    struct FField
    {
        FProperty* P=nullptr;FFieldClass* Kind=nullptr;FName Name;uint64 Flags=0;int32 Dim=0,Size=0,Offset=0;
        UScriptStruct* Struct=nullptr;FProperty* Inner=nullptr;FFieldClass* InnerKind=nullptr;FName InnerName;
        uint64 InnerFlags=0;int32 InnerSize=0,InnerOffset=0;bool Authored=false;
    };
    FResult& R;bool Stopped=false;TStrongObjectPtr<UBlueprint> BP;TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UClass> Parent,Generated,Skeleton;TStrongObjectPtr<UObject> CDO,ParentCDO;
    TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;
    TArray<TStrongObjectPtr<UObject>> Holds;TArray<FBPVariableDescription> Variables;
    TArray<FField> Fields,SkeletonFields;TMap<UScriptStruct*,TSharedPtr<FJsonObject>> StructSources;
    TArray<TUniquePtr<FDeclarationEnumScope>> NativeSources;
    TSharedPtr<FJsonObject> ValuesBefore;FString Path;EObjectFlags BPFlags=RF_NoFlags;bool NativeObject=false;
    explicit FMetadataTypedScope(FResult& In,EUse InUse=EUse::Metadata):Use(InUse),R(In),Editor(GEditor),World(GWorld){}
    bool Bad(const TCHAR* Why)
    {
        if(!Stopped){R.Data->SetStringField(TEXT("metadata_typed_first_failure"),Why);R.Data->SetStringField(TEXT("metadata_typed_failure_stage"),R.Stage);}
        Stopped=true;if(R.bMutationAttempted)R.bChangeStateKnown=false;return Fail(R,Use==EUse::VariableOrder?TEXT("variable_order_typed_scope_changed"):Use==EUse::CategoryOrder?TEXT("category_sorting_typed_scope_changed"):TEXT("variable_metadata_typed_scope_changed"),Why);
    }
    static bool Same(const FEdGraphPinType& A,const FEdGraphPinType& B)
    {return A==B&&A.bIsUObjectWrapper==B.bIsUObjectWrapper&&A.bSerializeAsSinglePrecisionFloat==B.bSerializeAsSinglePrecisionFloat&&A.PinValueType.bTerminalIsUObjectWrapper==B.PinValueType.bTerminalIsUObjectWrapper;}
    static bool PrimitiveInner(FProperty* P)
    {return P&&(P->GetClass()==FBoolProperty::StaticClass()||P->GetClass()==FIntProperty::StaticClass()||P->GetClass()==FInt64Property::StaticClass()||P->GetClass()==FFloatProperty::StaticClass()||P->GetClass()==FDoubleProperty::StaticClass()||P->GetClass()==FStrProperty::StaticClass()||P->GetClass()==FNameProperty::StaticClass()||P->GetClass()==FTextProperty::StaticClass());}
    static bool AdmittedType(const FEdGraphPinType& In)
    {auto T=In;if(T.bIsConst||T.bIsReference||T.bIsWeakPointer||T.bIsUObjectWrapper)return false;if(T.IsArray()){T.ContainerType=EPinContainerType::None;return Function::Pin::IsPrimitive(T);}return Function::Pin::IsPrimitive(T)||Function::Pin::IsScalarValueStruct(T);}
    bool Base()
    {
        if(Stopped)return false;
        if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||GExitPurge||IsEngineExitRequested()||UE::IsSavingPackage()||GEditor!=Editor.Get()||GWorld!=World.Get()||!DeclarationEnumLoaded(Editor.Get()))return Bad(TEXT("Idle metadata Editor context changed."));
        auto* B=BP.Get();auto* P=Parent.Get();
        if(!DeclarationEnumLoaded(B)||!DeclarationEnumLoaded(Package.Get())||!DeclarationEnumLoaded(P)||B->GetClass()!=UBlueprint::StaticClass()||B->GetPathName()!=Path||B->GetOuter()!=Package.Get()||B->GetPackage()!=Package.Get()||B->GetFlags()!=BPFlags
            ||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr().Get()!=P||B->BlueprintType!=BPTYPE_Normal||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified
            ||!B->GeneratedVariables.IsEmpty()||B->CachedUDSDependencies.Num()>0||B->NewVariables.Num()!=Variables.Num())return Bad(TEXT("Selected Blueprint/parent/current descriptor scope changed."));
        if(P!=(NativeObject?UObject::StaticClass():AActor::StaticClass())||!P->ClassDefaultObject.IsResolved()||P->ClassDefaultObject.Get()!=ParentCDO.Get()||!DeclarationEnumLoaded(ParentCDO.Get())||ParentCDO->GetClass()!=P)return Bad(TEXT("Existing native parent CDO role changed; no default-object creation."));
        if(Use==EUse::VariableOrder)
        {
            if(AuthoredOrder.Num()!=Variables.Num())return Bad(TEXT("Planned authored order inventory changed."));
            for(int32 I=0;I<AuthoredOrder.Num();++I){const auto* V=Variables.FindByPredicate([&](const FBPVariableDescription& X){return X.VarName==AuthoredOrder[I];});if(!V||B->NewVariables[I].VarName!=V->VarName||B->NewVariables[I].VarGuid!=V->VarGuid||!Same(B->NewVariables[I].VarType,V->VarType))return Bad(TEXT("Actual authored order/name/GUID/type differs from the admitted permutation."));}
        }
        else for(int32 I=0;I<Variables.Num();++I)if(B->NewVariables[I].VarName!=Variables[I].VarName||B->NewVariables[I].VarGuid!=Variables[I].VarGuid||!Same(B->NewVariables[I].VarType,Variables[I].VarType))return Bad(TEXT("Authored variable order/name/GUID/type changed outside metadata/category contract."));
        for(const auto& S:NativeSources)if(!S->Check())return Bad(TEXT("Existing native value source lease failed; no next SDK."));
        for(const auto& E:StructSources){FEdGraphPinType T;T.PinCategory=UEdGraphSchema_K2::PC_Struct;T.PinSubCategoryObject=E.Key;if(!DeclarationEnumLoaded(E.Key)||!Function::Pin::IsScalarValueStruct(T)||!LifecycleEqual(E.Value.ToSharedRef(),Function::Pin::ScalarValueStructSource(T)))return Bad(TEXT("Native Vector/Rotator/Transform source identity changed."));}
        return true;
    }
    // Called immediately after the local descriptor-array assignment, before the
    // structural SDK. It does not adopt fields/CDO/source values or callback edits.
    bool AdoptOrder(const TArray<FName>& Desired)
    {
        if(Use!=EUse::VariableOrder||Desired.Num()!=Variables.Num())return Bad(TEXT("Only the order writer can adopt its complete planned permutation."));
        TSet<FName> Seen;for(int32 I=0;I<Desired.Num();++I){const auto* V=Variables.FindByPredicate([&](const FBPVariableDescription& X){return X.VarName==Desired[I];});if(!V||Seen.Contains(Desired[I])||BP->NewVariables[I].VarName!=V->VarName||BP->NewVariables[I].VarGuid!=V->VarGuid||!Same(BP->NewVariables[I].VarType,V->VarType))return Bad(TEXT("Assigned descriptor order is not the original member permutation."));Seen.Add(Desired[I]);}
        AuthoredOrder=Desired;return Current();
    }
    bool FieldCurrent(UClass* C,const TArray<FField>& Expected)
    {
        TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){if(Now.Num()>256)return Bad(TEXT("Own reflected field budget changed."));Now.Add(*It);}
        if(Now.Num()!=Expected.Num())return Bad(TEXT("Own reflected root inventory count changed."));
        for(int32 I=0;I<Now.Num();++I)
        {
            const auto& E=Expected[I];if(Now[I]!=E.P)return Bad(TEXT("Own reflected root inventory address changed before next borrow."));auto* P=Now[I];
            if(P->GetOwnerStruct()!=C||P->GetOwnerProperty()!=P||P->GetClass()!=E.Kind||P->GetFName()!=E.Name||uint64(P->GetPropertyFlags())!=E.Flags||P->ArrayDim!=E.Dim||P->GetSize()!=E.Size||P->GetOffset_ForInternal()!=E.Offset||P->HasSetterOrGetter())return Bad(TEXT("Current reflected root role/layout changed."));
            if(E.Struct){auto* S=CastField<FStructProperty>(P);if(!S||!S->Struct.IsResolved()||S->Struct.Get()!=E.Struct)return Bad(TEXT("Current native struct binding changed."));}
            if(E.Inner){auto* A=CastField<FArrayProperty>(P);if(!A||A->Inner!=E.Inner)return Bad(TEXT("Current Array.Inner slot changed before next borrow."));auto* V=A->Inner;if(V->GetClass()!=E.InnerKind||V->GetOwner<FProperty>()!=P||V->GetOwnerProperty()!=P||V->GetOwnerStruct()!=C||V->GetFName()!=E.InnerName||uint64(V->GetPropertyFlags())!=E.InnerFlags||V->ArrayDim!=1||V->GetSize()!=E.InnerSize||V->GetOffset_ForInternal()!=E.InnerOffset||V->HasSetterOrGetter())return Bad(TEXT("Current primitive Array.Inner owner/layout changed."));}
        }return true;
    }
    bool Current()
    {
        if(!Base())return false;auto* B=BP.Get();
        if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=Generated.Get()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=Skeleton.Get())return Bad(TEXT("Generated/skeleton role changed outside returned metadata SDK boundary."));
        for(auto* C:{Generated.Get(),Skeleton.Get()})if(!DeclarationEnumLoaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=B||C->GetOuter()!=Package.Get()||C->GetSuperClass()!=Parent.Get())return Bad(TEXT("Current owned class role changed."));
        if(!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject.Get()!=CDO.Get()||!DeclarationEnumLoaded(CDO.Get())||CDO->GetClass()!=Generated.Get()||CDO->GetOuter()!=Package.Get()||!CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Bad(TEXT("Existing generated CDO role changed."));
        if(!FieldCurrent(Generated.Get(),Fields)||!FieldCurrent(Skeleton.Get(),SkeletonFields))return false;
        for(const auto& E:Fields)if(E.Inner){FScriptArrayHelper A(static_cast<FArrayProperty*>(E.P),E.P->ContainerPtrToValuePtr<void>(CDO.Get()));if(A.Num()<0||A.Num()>256)return Bad(TEXT("Current primitive array value exceeds256 elements."));}
        return true;
    }
    bool BindFields(UClass* C,TArray<FField>& Out)
    {
        Out.Empty();TArray<FProperty*> List;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){if(List.Num()>256)return Bad(TEXT("At most256 authored roots and one SDK compiler frame admitted."));List.Add(*It);}
        int32 Authored=0;for(auto* P:List)
        {
            FField E;E.P=P;E.Kind=P->GetClass();E.Name=P->GetFName();E.Flags=uint64(P->GetPropertyFlags());E.Dim=P->ArrayDim;E.Size=P->GetSize();E.Offset=P->GetOffset_ForInternal();
            if(P->GetOwnerStruct()!=C||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->HasSetterOrGetter())return Bad(TEXT("Direct own reflected scalar storage required."));
            const auto* V=Variables.FindByPredicate([&](const FBPVariableDescription& X){return X.VarName==E.Name;});E.Authored=V!=nullptr;
            if(auto* S=CastField<FStructProperty>(P)){if(!S->Struct.IsResolved()||!DeclarationEnumLoaded(S->Struct.Get()))return Bad(TEXT("Resolved native struct role required."));E.Struct=S->Struct.Get();Holds.Emplace(E.Struct);}
            if(!V)
            {
                auto* BGC=static_cast<UBlueprintGeneratedClass*>(C);if(E.Name!=UBlueprintGeneratedClass::GetUberGraphFrameName()||!E.Struct||E.Struct->GetPathName()!=TEXT("/Script/Engine.PointerToUberGraphFrame")||(E.Struct->StructFlags&STRUCT_Native)==0||(E.Flags&uint64(CPF_Transient|CPF_DuplicateTransient))!=uint64(CPF_Transient|CPF_DuplicateTransient)||!BGC->UberGraphFunction.IsResolved()||!(P==BGC->UberGraphFramePointerProperty||(!BGC->UberGraphFunction.Get()&&!BGC->UberGraphFramePointerProperty)))return Bad(TEXT("Only own authored fields plus exact public SDK compiler-frame role admitted."));
                Out.Add(E);continue;
            }
            ++Authored;if(auto* A=CastField<FArrayProperty>(P)){auto* I=A->Inner;if(!PrimitiveInner(I)||I->GetOwner<FProperty>()!=P||I->GetOwnerProperty()!=P||I->GetOwnerStruct()!=C||I->ArrayDim!=1||I->HasSetterOrGetter())return Bad(TEXT("One-level directly owned primitive Array.Inner required."));E.Inner=I;E.InnerKind=I->GetClass();E.InnerName=I->GetFName();E.InnerFlags=uint64(I->GetPropertyFlags());E.InnerSize=I->GetSize();E.InnerOffset=I->GetOffset_ForInternal();}
            FEdGraphPinType T;const bool Converted=GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(P,T);if(!Base())return false;
            if(!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr().Get()!=Generated.Get()||BP->SkeletonGeneratedClass.GetGCPtr().Get()!=Skeleton.Get()
                ||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject.Get()!=CDO.Get())return Bad(TEXT("Current class/CDO slots changed during schema type observation; no following SDK."));
            TArray<FProperty*> Check;for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It)Check.Add(*It);if(Check!=List)return Bad(TEXT("Reflected inventory changed during schema type observation."));
            if(!Converted||!AdmittedType(T)||!Same(T,V->VarType)){R.Data->SetStringField(TEXT("metadata_typed_mismatch_member"),E.Name.ToString());R.Data->SetObjectField(TEXT("metadata_typed_expected_type"),PinJson(V->VarType));R.Data->SetObjectField(TEXT("metadata_typed_actual_type"),PinJson(T));return Bad(TEXT("Generated field type does not match admitted authored type."));}
            if(Function::Pin::IsScalarValueStruct(T)){
                if(!StructSources.Contains(E.Struct)){auto Guard=MakeUnique<FDeclarationEnumScope>();const FString Kind=E.Struct==TBaseStructure<FVector>::Get()?TEXT("vector"):E.Struct==TBaseStructure<FRotator>::Get()?TEXT("rotator"):TEXT("transform");if(!Guard->LoadNativeStruct(Kind,R,true))return Bad(TEXT("Existing native value descriptor lease could not bind."));NativeSources.Add(MoveTemp(Guard));}
                auto J=Function::Pin::ScalarValueStructSource(T);if(!J->GetBoolField(TEXT("observations_complete"))||!J->GetBoolField(TEXT("native_identity_verified")))return Bad(TEXT("Complete native value source required."));const auto* Old=StructSources.Find(E.Struct);if(Old&&!LifecycleEqual(Old->ToSharedRef(),J))return Bad(TEXT("Native value source changed at rebind."));StructSources.Add(E.Struct,J);}
            Out.Add(E);
        }return Authored==Variables.Num()||Bad(TEXT("Every authored member needs an exact current generated field."));
    }
    bool Values(TSharedRef<FJsonObject>& Out)
    {
        Out=MakeShared<FJsonObject>();if(!Current())return false;int32 Chars=0;
        for(const auto& E:Fields)if(E.Authored)
        {FString V;const void* At=E.P->ContainerPtrToValuePtr<void>(CDO.Get());const bool OK=E.P->ExportText_Direct(V,At,At,CDO.Get(),PPF_None);if(!Current())return false;if(!OK||!SafeText(V,65536)||(Chars+=V.Len())>1048576)return Bad(TEXT("Bounded authored CDO values could not be exported."));Out->SetStringField(E.Name.ToString(),V);}return true;
    }
    bool Rebind()
    {
        if(!Base())return false;auto* B=BP.Get();if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("Returned metadata SDK leaves unresolved current class roles."));
        auto* G=B->GeneratedClass.GetGCPtr().Get();auto* S=B->SkeletonGeneratedClass.GetGCPtr().Get();TStrongObjectPtr<UClass> HG(G),HS(S);
        for(auto* C:{G,S})if(!DeclarationEnumLoaded(C)||C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy.Get()!=B||C->GetOuter()!=Package.Get()||C->GetSuperClass()!=Parent.Get())return Bad(TEXT("Returned current classes have no admitted owned native-parent role."));
        if(!G->ClassDefaultObject.IsResolved())return Bad(TEXT("Returned existing generated CDO slot unresolved."));auto* D=G->ClassDefaultObject.Get();TStrongObjectPtr<UObject> HD(D);if(!DeclarationEnumLoaded(D)||D->GetClass()!=G||D->GetOuter()!=Package.Get()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return Bad(TEXT("Returned generated CDO ownership unavailable."));
        Generated.Reset(G);Skeleton.Reset(S);CDO.Reset(D);Holds.Emplace(G);Holds.Emplace(S);Holds.Emplace(D);
        if(!BindFields(G,Fields)||!BindFields(S,SkeletonFields)||!Current()||!MutationScope(B,R))return false;
        TSharedRef<FJsonObject> Now=MakeShared<FJsonObject>();if(!Values(Now))return false;if(ValuesBefore.IsValid()&&!LifecycleEqual(ValuesBefore.ToSharedRef(),Now)){R.Data->SetObjectField(TEXT("metadata_typed_values_actual"),Now);return Bad(TEXT("Metadata SDK changed authored CDO values; no following SDK or retry."));}if(!ValuesBefore.IsValid()){ValuesBefore=Now;R.Data->SetObjectField(TEXT("metadata_typed_values_before"),Now);}return Current();
    }
    bool SavedReturned(bool Saved)
    {
        // SavePackage2 marks serialized exports as loaded. Allow only that
        // source-confirmed transition after this writer's successful save.
        if(Use==EUse::Metadata)return Bad(TEXT("Metadata does not own an explicit save boundary."));
        if(!DeclarationEnumLoaded(BP.Get()))return Bad(TEXT("Saved target Blueprint is no longer loaded."));
        const auto Expected=EObjectFlags(BPFlags|RF_WasLoaded|RF_LoadCompleted);
        if(Saved){if(BP->GetFlags()!=Expected)return Bad(TEXT("Save returned an unexpected Blueprint object-flag transition."));BPFlags=Expected;}
        return Rebind();
    }
    bool Bind(UBlueprint* B)
    {
        if(!MutationScope(B,R))return false;auto Slot=B->ParentClass;auto* P=Slot.GetGCPtr().IsResolved()?Slot.GetGCPtr().Get():nullptr;
        if(B->GetClass()!=UBlueprint::StaticClass()||B->GetOuter()!=B->GetPackage()||(P!=AActor::StaticClass()&&P!=UObject::StaticClass())||!B->GeneratedVariables.IsEmpty()||B->CachedUDSDependencies.Num()>0||B->NewVariables.Num()>256)return Bad(TEXT("Typed metadata admits direct native Actor/UObject and own fields without UDS/generated members."));
        if(!P->ClassDefaultObject.IsResolved()||!DeclarationEnumLoaded(P->ClassDefaultObject.Get()))return Bad(TEXT("Existing resolved native parent CDO required."));
        BP.Reset(B);Package.Reset(B->GetPackage());Parent.Reset(P);ParentCDO.Reset(P->ClassDefaultObject.Get());NativeObject=P==UObject::StaticClass();Path=B->GetPathName();BPFlags=B->GetFlags();Variables=B->NewVariables;for(const auto& V:Variables)AuthoredOrder.Add(V.VarName);
        for(const auto& V:Variables)if(!V.VarGuid.IsValid()||!AdmittedType(V.VarType))return Bad(TEXT("Own primitive scalar/primitive Array/native Vector-Rotator-Transform scalar members only; wider types remain incomplete."));
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("SetMetadata only: direct native Actor/UObject; current own primitive scalar/primitive Array<=256/native Vector-Rotator-Transform scalar fields; no external instances/derived Blueprints, UDS or generated variables. Original metadata/flag applications and no explicit compile/save retained; wider inherited/object-reference/enum/container variants remain incomplete; arrays over256 remain unobservable through the shared ReadTyped contract and incomplete; usage validation pending."));
        if(Use!=EUse::Metadata){R.Data->SetStringField(TEXT("typed_lifecycle_operation"),Use==EUse::VariableOrder?TEXT("set_blueprint_variable_order"):TEXT("set_blueprint_category_sorting"));R.Data->SetStringField(TEXT("admission_scope"),TEXT("Original declaration/category ordering on direct native Actor/UObject with current own primitive scalar/primitive Array<=256/native Vector-Rotator-Transform scalar fields. Existing strict/permutation or category-root/append semantics and actual optional compile/save prefixes are retained. Only exact planned variable-order permutation may change authored order; values/types/GUIDs/native sources stay protected. Wider inheritance/reference/enum/UDS/container/large-array support and runtime/cold behavior remain incomplete; usage validation pending."));}
        R.Data->SetStringField(TEXT("observation_scope"),TEXT("Existing lifecycle descriptor/CDO-value/graph/reference/native-parent/disk/other-package observations plus typed own field/Inner/native3 source leases. SDK-return class roles may be reacquired; compiler frame is not treated as authored data. Initial load effects, arbitrary native source internals and global delegates are not fully measured."));if(!Rebind())return false;TArray<TSharedPtr<FJsonValue>> Sources;for(const auto& S:NativeSources)Sources.Add(MakeShared<FJsonValueObject>(S->Before));R.Data->SetArrayField(TEXT("metadata_native_value_sources"),Sources);return true;
    }
    bool CapturePhase(FLifecycleObservation& O,const TCHAR* Phase)
    {auto Fence=[this](){return Current();};return Current()&&LifecycleCapture(BP.Get(),O,R,Phase,nullptr,Fence,[&](const TSharedRef<FJsonObject>& G){return LifecycleGraphs(BP.Get(),G,Fence)&&Current();},[&](const TSharedRef<FJsonObject>& Q){return LifecycleReferences(BP.Get(),Q,nullptr,Fence)&&Current();},Parent.Get())&&Current();}
};
bool UsesTypedOrdering(UBlueprint* BP)
{bool Typed=BP->ParentClass!=AActor::StaticClass();for(const auto& V:BP->NewVariables)Typed|=!Function::Pin::IsPrimitive(V.VarType);return Typed;}
bool TypedOrderingObserved(FMetadataTypedScope& Typed,FLifecycleObservation& O,const FLifecycleObservation& Before,FResult& R,const TCHAR* Phase)
{
    if(!Typed.CapturePhase(O,Phase)){R.bChangeStateKnown=false;if(!Typed.Stopped)Fail(R,TEXT("variable_order_observation_incomplete"),TEXT("Current typed ordering prefix could not be observed; no automatic replay."));return false;}
    R.bChangeStateKnown=true;R.bChanged=!LifecycleEqual(Before.State,O.State)||!LifecycleEqual(Before.Source,O.Source)||!LifecycleEqual(Before.References,O.References)||!LifecycleEqual(Before.Other,O.Other)||!LifecycleEqual(Before.Disk,O.Disk);return true;
}
}
FResult SetMetadata(const FString& Path, const FString& Name, const FString& MetadataJson, const FString& EditableJson, const FString& InstanceJson, const FString& SpawnJson)
{
    FResult R; LifecycleInit(R); TSharedPtr<FJsonValue> Meta; TOptional<bool> Editable, Instance, Spawn;
    if (!Parse(MetadataJson, Meta, R) || !(Meta->Type == EJson::Null || Meta->Type == EJson::Object) || !LifecycleFlag(EditableJson, Editable, R) || !LifecycleFlag(InstanceJson, Instance, R) || !LifecycleFlag(SpawnJson, Spawn, R)) { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("variable_metadata_invalid"), TEXT("Metadata requires JSON object or null; optional flags require null/bool.")); } return R; }
    TArray<TPair<FString, FString>> Entries; TArray<TSharedPtr<FJsonValue>> Skipped;
    if (Meta->Type == EJson::Object)
    {
        if (Meta->AsObject()->Values.Num() > 32) { Fail(R, TEXT("variable_metadata_budget_exceeded"), TEXT("At most32 metadata entries admitted.")); return R; }
        for (const auto& P : Meta->AsObject()->Values)
        {
            FString Value; if (P.Value->Type == EJson::String) { Value = P.Value->AsString(); } else if (P.Value->Type == EJson::Boolean) { Value = P.Value->AsBool() ? TEXT("true") : TEXT("false"); } else if (P.Value->Type == EJson::Number) { Value = FString::SanitizeFloat(P.Value->AsNumber()); } else { auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("key"), FString(P.Key.ToView())); J->SetField(TEXT("value"), P.Value); Skipped.Add(MakeShared<FJsonValueObject>(J)); continue; }
            if (FName(*FString(P.Key.ToView())).IsNone()) { Fail(R, TEXT("variable_metadata_key_unsupported"), TEXT("Non-None bounded metadata keys required.")); return R; } Entries.Emplace(FString(P.Key.ToView()), Value);
        }
    }
    R.Data->SetArrayField(TEXT("skipped_metadata"), Skipped); if (Entries.IsEmpty() && !Editable.IsSet() && !Instance.IsSet() && !Spawn.IsSet()) { Fail(R, TEXT("variable_metadata_inputs_missing"), TEXT("Original named metadata/flag request contains no supported edit.")); return R; }
    TStrongObjectPtr<UBlueprint> Blueprint(LoadBlueprint(Path, R)); UBlueprint* BP = Blueprint.Get();if(!BP)return R;
    bool TypedProfile=BP->ParentClass!=AActor::StaticClass();for(const auto& V:BP->NewVariables)TypedProfile|=!Function::Pin::IsPrimitive(V.VarType);
    TUniquePtr<FMetadataTypedScope> Typed;if(TypedProfile){Typed=MakeUnique<FMetadataTypedScope>(R);if(!Typed->Bind(BP))return R;}else if(!LifecycleSimple(BP,R))return R;
    auto Current=[&](){return !Typed||Typed->Current();};auto SDKReturned=[&](){return !Typed||Typed->Rebind();};
    int32 Index = INDEX_NONE; FBPVariableDescription* V = FindLocal(BP, Name, Index, R); if (!V) { return R; }
    const FName Target = V->VarName; FBPVariableDescription Desired = *V; for (const auto& E : Entries) { Desired.SetMetaData(FName(*E.Key), E.Value); }
    auto DesiredEditable = [&](bool X) { if (X) { Desired.PropertyFlags |= static_cast<uint64>(CPF_Edit | CPF_BlueprintVisible); Desired.PropertyFlags &= ~static_cast<uint64>(CPF_DisableEditOnInstance); } else { Desired.PropertyFlags |= static_cast<uint64>(CPF_DisableEditOnInstance); } };
    if (Editable.IsSet()) { DesiredEditable(*Editable); } if (Instance.IsSet()) { DesiredEditable(*Instance); } if (Spawn.IsSet()) { if (*Spawn) { Desired.PropertyFlags |= CPF_ExposeOnSpawn; Desired.SetMetaData(TEXT("ExposeOnSpawn"), TEXT("true")); } else { Desired.PropertyFlags &= ~static_cast<uint64>(CPF_ExposeOnSpawn); Desired.RemoveMetaData(TEXT("ExposeOnSpawn")); } }
    FLifecycleObservation Before, After; if (!(Typed?Typed->CapturePhase(Before,TEXT("before_observation")):LifecycleCapture(BP, Before, R, TEXT("before_observation")))) { R.bChangeStateKnown = false; Fail(R, TEXT("variable_lifecycle_before_unobservable"), TEXT("Before actual prefix is incomplete; no edit.")); return R; }
    R.Data->SetObjectField(TEXT("desired_variable"), Row(BP, Desired, TEXT("new"), Index)); R.Data->SetStringField(TEXT("blueprint_name"), Path); R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName()); R.Data->SetStringField(TEXT("variable_name"), Name);
    auto Applied = MakeShared<FJsonObject>(), Flags = MakeShared<FJsonObject>(), Verified = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("metadata"), Applied); R.Data->SetObjectField(TEXT("display"), MakeShared<FJsonObject>()); R.Data->SetObjectField(TEXT("flags"), Flags); R.Data->SetObjectField(TEXT("verified_metadata"), Verified);
    if(!Current())return R;LifecycleCount(R, TEXT("outer_transaction_calls")); const FScopedTransaction Transaction(NSLOCTEXT("CubelessVariable", "Metadata", "Set Blueprint variable metadata")); R.Stage = TEXT("set_metadata"); R.bMutationAttempted = true; R.bChangeStateKnown = false; TArray<TSharedPtr<FJsonValue>> Calls;if(!Current())return R;
    for (const auto& E : Entries)
    {
        if(!Current())return R;V = FindLocal(BP, Name, Index, R); if (!V || V->VarGuid != Desired.VarGuid || V->VarType != Desired.VarType) { Fail(R, TEXT("variable_metadata_member_changed"), TEXT("Actual callback moved the selected member; prefix retained.")); return R; }
        auto J = MakeShared<FJsonObject>(); Calls.Add(MakeShared<FJsonValueObject>(J)); R.Data->SetArrayField(TEXT("metadata_calls"), Calls); J->SetStringField(TEXT("key"), E.Key); J->SetStringField(TEXT("value"), E.Value); J->SetBoolField(TEXT("returned"), false);
        LifecycleCount(R, TEXT("metadata_descriptor_set_calls")); V->SetMetaData(FName(*E.Key), E.Value); LifecycleCount(R, TEXT("metadata_editor_set_calls")); FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Target, nullptr, FName(*E.Key), E.Value); J->SetBoolField(TEXT("returned"), true); Applied->SetStringField(E.Key, E.Value);if(!SDKReturned())return R;
    }
    auto ApplyEditable = [&](bool X, const TCHAR* Alias) { if(!Current())return false;V = FindLocal(BP, Name, Index, R); if (!V || V->VarGuid != Desired.VarGuid || V->VarType != Desired.VarType) { return false; } uint64* F = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(BP, Target); if (!F) { return false; } LifecycleCount(R, TEXT("descriptor_flag_edit_calls")); if (X) { *F |= static_cast<uint64>(CPF_Edit | CPF_BlueprintVisible); *F &= ~static_cast<uint64>(CPF_DisableEditOnInstance); } else { *F |= static_cast<uint64>(CPF_DisableEditOnInstance); } V->PropertyFlags = *F; Flags->SetBoolField(TEXT("instance_editable"), X); if (Alias) { Flags->SetBoolField(Alias, X); } return true; };
    if (Editable.IsSet() && !ApplyEditable(*Editable, TEXT("is_editable")) || Instance.IsSet() && !ApplyEditable(*Instance, nullptr)) { Fail(R, TEXT("variable_metadata_flags_unobservable"), TEXT("Editable callback prefix retained; selected storage unavailable.")); return R; }
    if (Spawn.IsSet()) { if(!Current())return R;V = FindLocal(BP, Name, Index, R); if (!V || V->VarGuid != Desired.VarGuid || V->VarType != Desired.VarType) { Fail(R, TEXT("variable_metadata_member_changed"), TEXT("Selected spawn-flag member identity changed; prefix retained.")); return R; } LifecycleCount(R, TEXT("descriptor_flag_edit_calls")); if (*Spawn) { V->PropertyFlags |= CPF_ExposeOnSpawn; LifecycleCount(R, TEXT("metadata_editor_set_calls")); FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP, Target, nullptr, TEXT("ExposeOnSpawn"), TEXT("true")); } else { V->PropertyFlags &= ~static_cast<uint64>(CPF_ExposeOnSpawn); LifecycleCount(R, TEXT("metadata_editor_remove_calls")); FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(BP, Target, nullptr, TEXT("ExposeOnSpawn")); } Flags->SetBoolField(TEXT("expose_on_spawn"), *Spawn);if(!SDKReturned())return R; }
    if(!Current())return R;LifecycleCount(R, TEXT("explicit_mark_modified_calls")); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!Current())return R; R.Data->SetNumberField(TEXT("source_expected_implicit_structural_requests"), R.Data->GetNumberField(TEXT("metadata_editor_set_calls")) + R.Data->GetNumberField(TEXT("metadata_editor_remove_calls")));
    if(Typed){if(!Typed->CapturePhase(After,TEXT("after_observation"))){R.bChangeStateKnown=false;return R;}R.bChangeStateKnown=true;R.bChanged=!LifecycleEqual(Before.State,After.State)||!LifecycleEqual(Before.Source,After.Source)||!LifecycleEqual(Before.References,After.References)||!LifecycleEqual(Before.Other,After.Other)||!LifecycleEqual(Before.Disk,After.Disk);}else if(!LifecycleObserved(BP,After,Before,R,TEXT("after_observation")))return R; V = FindLocal(BP, Name, Index, R); if (!V) { return R; }
    for (const auto& E : Entries) { FString Value; LifecycleCount(R, TEXT("metadata_readback_get_calls")); const bool Got=FBlueprintEditorUtils::GetBlueprintVariableMetaData(BP, Target, nullptr, FName(*E.Key), Value);if(!Current())return R;if(Got) { Verified->SetStringField(E.Key, Value); } }
    R.Data->SetNumberField(TEXT("property_flags"), static_cast<double>(V->PropertyFlags)); R.Data->SetStringField(TEXT("property_flags_string"), LexToString(V->PropertyFlags)); R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetBoolField(TEXT("saved"), false);
    TArray<FName> Order; for (const auto& Old : Before.State->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("new_variables"))) { Order.Add(FName(*Old->AsObject()->GetStringField(TEXT("name")))); }
    if (!LifecyclePreserved(Before, After, Order, &Desired, false, false, true, R)) { Fail(R, TEXT("variable_metadata_preservation_failed"), TEXT("Actual metadata/flags or other declared scope differs; no compile/save/retry.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
FResult SetOrder(const FString& Path, const FString& OrderJson, bool Strict, bool Compile, bool Save)
{
    FResult R; LifecycleInit(R); R.bCompileRequested = Compile; R.bSaveRequested = Save; R.Data->SetBoolField(TEXT("caller_compile_requested"), Compile); R.Data->SetBoolField(TEXT("caller_save_requested"), Save); TSharedPtr<FJsonValue> Json;
    if (!Parse(OrderJson, Json, R) || Json->Type != EJson::Array) { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("variable_order_invalid"), TEXT("Original order array required.")); } return R; }
    TArray<FString> Requested; TArray<FName> Names; for (const auto& V : Json->AsArray()) { if (V->Type != EJson::String) { continue; } const FString N = V->AsString().TrimStartAndEnd(); if (!N.IsEmpty()) { if (!SafeName(N)) { Fail(R, TEXT("variable_order_name_invalid"), TEXT("Bounded order names required.")); return R; } Requested.Add(N); Names.Add(FName(*N)); } }
    if (Names.IsEmpty()) { Fail(R, TEXT("variable_order_empty"), TEXT("Original order contains no nonempty variable names.")); return R; }
    TStrongObjectPtr<UBlueprint> Blueprint(LoadBlueprint(Path,R));UBlueprint* BP=Blueprint.Get();if(!BP)return R;
    TUniquePtr<FMetadataTypedScope> Typed;if(UsesTypedOrdering(BP)){Typed=MakeUnique<FMetadataTypedScope>(R,FMetadataTypedScope::EUse::VariableOrder);if(!Typed->Bind(BP))return R;}else if(!LifecycleSimple(BP,R))return R;
    auto Current=[&](){return !Typed||Typed->Current();};auto SDKReturned=[&](){return !Typed||Typed->Rebind();};
    auto Observed=[&](FLifecycleObservation& O,const FLifecycleObservation& Before,const TCHAR* Phase){return Typed?TypedOrderingObserved(*Typed,O,Before,R,Phase):LifecycleObserved(BP,O,Before,R,Phase);};
    TArray<FBPVariableDescription> Reordered; TArray<FName> TargetOrder; TArray<FString> Previous, Missing; TSet<FName> Added;
    for (const auto& V : BP->NewVariables) { Previous.Add(V.VarName.ToString()); }
    for (FName N : Names) { const FBPVariableDescription* Found = nullptr; for (const auto& V : BP->NewVariables) { if (V.VarName == N) { if (Found) { Fail(R, TEXT("variable_order_ambiguous"), TEXT("Ambiguous local order descriptor.")); return R; } Found = &V; } } if (!Found) { Missing.Add(N.ToString()); } else if (!Added.Contains(N)) { Added.Add(N); Reordered.Add(*Found); } }
    R.Data->SetStringField(TEXT("blueprint_name"), Path); R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName()); R.Data->SetArrayField(TEXT("requested_order"), StringValues(Requested)); R.Data->SetArrayField(TEXT("previous_order"), StringValues(Previous)); R.Data->SetArrayField(TEXT("missing_variables"), StringValues(Missing));
    if (Strict && !Missing.IsEmpty()) { Fail(R, TEXT("variable_order_missing"), TEXT("Strict original order contains missing local members; no edit.")); return R; }
    for (const auto& V : BP->NewVariables) { if (!Added.Contains(V.VarName)) { Reordered.Add(V); } }
    bool Changed = Reordered.Num() != BP->NewVariables.Num(); for (int32 I = 0; I < Reordered.Num(); ++I) { TargetOrder.Add(Reordered[I].VarName); if (!Changed && Reordered[I].VarName != BP->NewVariables[I].VarName) { Changed = true; } }
    R.Data->SetBoolField(TEXT("changed"), Changed); R.Data->SetBoolField(TEXT("effective_compile_requested"), Compile && Changed); R.Data->SetBoolField(TEXT("effective_save_requested"), Save && Changed);
    FLifecycleObservation Before, Configured, Compiled, Saved; if (!(Typed?Typed->CapturePhase(Before,TEXT("before_observation")):LifecycleCapture(BP,Before,R,TEXT("before_observation")))) { R.bChangeStateKnown = false; Fail(R, TEXT("variable_lifecycle_before_unobservable"), TEXT("Before actual prefix incomplete; no order write.")); return R; }
    R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetStringField(TEXT("compile_status"), TEXT("not_requested")); R.Data->SetBoolField(TEXT("legacy_compiled_means_dispatch_only"), true);
    if(!Current())return R;LifecycleCount(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(NSLOCTEXT("CubelessVariable","Order","Set Blueprint variable order"));if(!Current())return R;
    if(Changed){R.Stage=TEXT("set_order");R.bMutationAttempted=true;R.bChangeStateKnown=false;LifecycleCount(R,TEXT("explicit_modify_calls"));BP->Modify();if(!Current())return R;BP->NewVariables=Reordered;if(Typed&&!Typed->AdoptOrder(TargetOrder))return R;if(!Current())return R;LifecycleCount(R,TEXT("explicit_mark_structural_calls"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);if(!SDKReturned())return R;LifecycleCount(R,TEXT("explicit_mark_package_dirty_calls"));BP->MarkPackageDirty();if(!Current())return R;}
    TArray<FString> ConfiguredOrder; for (const auto& V : BP->NewVariables) { ConfiguredOrder.Add(V.VarName.ToString()); } R.Data->SetArrayField(TEXT("new_order"), StringValues(ConfiguredOrder)); if (!Observed(Configured,Before,TEXT("after_configuration"))) { return R; }
    const bool ConfigOK = LifecyclePreserved(Before, Configured, TargetOrder, nullptr, false, false, true, R); R.Data->SetBoolField(TEXT("after_configuration_preservation_passed"), ConfigOK); if (!ConfigOK) { Fail(R, TEXT("variable_order_preservation_failed"), TEXT("Actual configured order or other observed scope differs; compile/save suppressed.")); return R; }
    FLifecycleObservation* Final = &Configured;
    if (Changed && Compile)
    {
        LifecycleAggregateClear(R);R.Stage=TEXT("compile");if(!Current())return R;R.bCompileAttempted=true; R.bChangeStateKnown = false; LifecycleCount(R,TEXT("full_compile_attempts"));const auto C=Cubeless::UE58::CompileBlueprint(BP->GetPathName(),false,false);R.Data->SetObjectField(TEXT("compile_result"),C);R.Data->SetBoolField(TEXT("compiled"),true);bool OK=false;C->TryGetBoolField(TEXT("compiled"),OK);R.Compiled=OK;if(!SDKReturned())return R;R.Data->SetStringField(TEXT("compile_status"),LifecycleStatus(BP->Status));
        if (!Observed(Compiled,Before,TEXT("after_compile"))) { return R; } Final = &Compiled; R.Data->SetStringField(TEXT("compile_status"), LifecycleStatus(BP->Status)); const bool Preserved = LifecyclePreserved(Before, Compiled, TargetOrder, nullptr, OK, false, true, R); R.Data->SetBoolField(TEXT("after_compile_preservation_passed"), Preserved);
        if (!OK || !Preserved) { Fail(R, OK ? TEXT("variable_order_compile_preservation_failed") : TEXT("variable_order_compile_failed"), TEXT("Actual compiler/order prefix retained; target save suppressed.")); return R; }
    }
    if (Changed && Save)
    {
        const auto& SaveBefore = *Final; R.Data->SetObjectField(TEXT("before_save_observation"), R.Data->GetObjectField(Compile ? TEXT("after_compile") : TEXT("after_configuration"))); R.Data->SetBoolField(TEXT("before_save_preservation_passed"), true); LifecycleAggregateClear(R);R.Stage=TEXT("save");if(!Current())return R;R.bSaveAttempted=true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("save_attempt_count")); auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("asset_path"), BP->GetPathName()); Receipt->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::SaveLoadedAsset")); Receipt->SetBoolField(TEXT("only_if_dirty"), false); Receipt->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) }); R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(BP, false); Receipt->SetBoolField(TEXT("returned"), true); Receipt->SetBoolField(TEXT("saved"), R.bSaved);if(Typed&&!Typed->SavedReturned(R.bSaved))return R;
        R.Data->SetBoolField(TEXT("saved"), R.bSaved); if (!Observed(Saved,Before,TEXT("after_save"))) { return R; } Final = &Saved; const bool Preserved = LifecyclePreserved(SaveBefore, Saved, TargetOrder, nullptr, false, R.bSaved, !R.bSaved, R);
        if (!R.bSaved || !Preserved) { Fail(R, R.bSaved ? TEXT("variable_order_save_preservation_failed") : TEXT("variable_order_save_failed"), TEXT("Actual target save prefix retained; no retry or rollback.")); return R; }
    }
    TArray<FString> NewOrder; for (const auto& V : BP->NewVariables) { NewOrder.Add(V.VarName.ToString()); } R.Data->SetArrayField(TEXT("new_order"), StringValues(NewOrder)); R.Data->SetBoolField(TEXT("compiled"), R.bCompileAttempted); R.Data->SetBoolField(TEXT("saved"), R.bSaved); if (!R.bCompileAttempted) { R.Data->SetStringField(TEXT("compile_status"), TEXT("not_requested")); } R.Data->SetBoolField(TEXT("no_op"), !Changed); R.Data->SetBoolField(TEXT("final_observation_complete"), Final->Complete);
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}

// CA: single unused local scalar declaration removal; BZ and earlier bodies remain unchanged.
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Internationalization/Text.h"
namespace Cubeless::UE58::Variable
{
namespace
{
bool RemovalDependencyReference(const FString& Value)
{
    FString Candidate = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd(); Candidate.TrimQuotesInline();
    for (const TCHAR* Token : { TEXT("/Script/UnrealMCP"), TEXT("/UnrealMCP"), TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("/Script/CubelessControlRigTools"), TEXT("CubelessControlRigTools"), TEXT("CubelessControlRigToolsEditor") })
    { if (Candidate.Contains(Token, ESearchCase::IgnoreCase)) { return true; } }
    return false;
}

// PN: the selected own Get/Set nodes and their incident links are a copied deletion
// plan. Removed node/pin addresses are compared only, never dereferenced after SDK return.
struct FRemovalLinkedPlan
{
    struct FPin
    {
        UEdGraphPin* Address=nullptr; FGuid Id; FName Name; EEdGraphPinDirection Direction=EGPD_Input;
        FEdGraphPinType Type; FString Raw,Auto; FText Text; UObject* Default=nullptr;
        TArray<UEdGraphPin*> Links; UEdGraphPin* Parent=nullptr; TArray<UEdGraphPin*> Children;
        bool Orphan=false,Hidden=false,NotConnectable=false; TStrongObjectPtr<UObject> DefaultHold;
    };
    struct FNode
    {
        TStrongObjectPtr<UEdGraphNode> Hold; UEdGraph* Graph=nullptr; UClass* Class=nullptr;
        FGuid Guid; FString Path; EObjectFlags Flags=RF_NoFlags; int32 X=0,Y=0;
        bool Selected=false,VariableNode=false; FMemberReference Member; TArray<FPin> Pins;
    };
    struct FGraph
    {
        TStrongObjectPtr<UEdGraph> Hold; FString Path; FGuid Guid; UClass* Class=nullptr;
        EObjectFlags Flags=RF_NoFlags; TArray<UEdGraphNode*> Order;
    };
    FResult& R; TStrongObjectPtr<UBlueprint> Blueprint; TStrongObjectPtr<UPackage> Package;
    TStrongObjectPtr<UClass> Generated,Skeleton; TStrongObjectPtr<UObject> Default;
    FString BlueprintPath,SkeletonPath;UObject* SkeletonOuter=nullptr;UClass* SkeletonMeta=nullptr;EObjectFlags SkeletonFlags=RF_NoFlags;uint32 SkeletonClassFlags=0; FName Variable; TArray<FGraph> Graphs; TArray<FNode> Nodes;
    struct FClassFields{TStrongObjectPtr<UClass> Hold;TArray<FProperty*> Fields;TArray<FFieldClass*> Classes;TArray<UStruct*> Owners;TArray<uint64> Flags;TArray<int32> Offsets,Sizes,Dimensions;};
    TArray<FClassFields> FieldOwners;TStrongObjectPtr<UObject> NativeCDO;
    struct FScsRole{TStrongObjectPtr<USCS_Node> Node;TStrongObjectPtr<UActorComponent> Template;TStrongObjectPtr<UClass> Class;FString Path,TemplatePath;UObject* Outer=nullptr;EObjectFlags Flags=RF_NoFlags,TemplateFlags=RF_NoFlags;FGuid Guid;FName Name;TArray<USCS_Node*> Children;};
    TStrongObjectPtr<USimpleConstructionScript> Scs;TArray<FScsRole> ScsRoles;TArray<USCS_Node*> ScsAll,ScsRoots;
    TSet<UObject*> Removed; TSet<UEdGraphPin*> RemovedPins; TSet<FString> RemovedPaths;
    bool Active=false,After=false,Stopped=false,ReacquiringText=false; int32 IncidentDirected=0;
    TSharedPtr<FJsonObject> GraphBaseline;
    // ADA: descriptor storage may reallocate at removal; retain value copies only.
    // Every callback fence compares the complete authored inventory before another
    // SDK writer. Only the selected row may disappear at StructuralReturn.
    TArray<FBPVariableDescription> OriginalDeclarations;
    struct FInterfaceRole
    {
        TStrongObjectPtr<UClass> Class; FString Path; UObject* Outer=nullptr;
        EObjectFlags Flags=RF_NoFlags; uint32 ClassFlags=0; TArray<UEdGraph*> Graphs;
    };
    TArray<FInterfaceRole> OwnInterfaces;
    TArray<FImplementedInterface> NativeActorInterfaces;
    bool AuthoredReady=false;
    explicit FRemovalLinkedPlan(FResult& Result):R(Result){}
    bool Bad(const TCHAR* Why)
    {
        if(Stopped)return false;Stopped=true;R.bChangeStateKnown=false;
        if(!R.Data->HasField(TEXT("first_current_failure")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("reason"),Why);
            J->SetStringField(TEXT("object"),BlueprintPath);J->SetStringField(TEXT("property"),TEXT("linked_removal_current"));
            J->SetBoolField(TEXT("operands_observed"),false);J->SetField(TEXT("expected"),MakeShared<FJsonValueNull>());J->SetField(TEXT("actual"),MakeShared<FJsonValueNull>());
            R.Data->SetObjectField(TEXT("first_current_failure"),J);
        }
        if(R.ErrorCode.IsEmpty())Fail(R,TEXT("variable_remove_linked_current_lost"),Why);return false;
    }
    bool Difference(const FString& Object,const TCHAR* Property,const FString& Expected,const FString& Actual,const TCHAR* Why)
    {
        if(!R.Data->HasField(TEXT("first_current_failure")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("reason"),Why);J->SetStringField(TEXT("object"),Object);
            J->SetStringField(TEXT("property"),Property);J->SetBoolField(TEXT("operands_observed"),true);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);
            R.Data->SetObjectField(TEXT("first_current_failure"),J);
        }
        return Bad(Why);
    }
    bool DescriptorCurrent(const FBPVariableDescription& Expected,const FBPVariableDescription& Actual)
    {
        // UE FString equality is IgnoreCase. Authored strings require explicit
        // case-sensitive checks here; changing the shared helper would widen this fix.
        const FString RowPath=BlueprintPath+TEXT(".NewVariables[")+Expected.VarName.ToString()+TEXT("]");
        auto Changed=[&](const TCHAR* Property,const FString& E,const FString& A){return Difference(RowPath,Property,E,A,TEXT("An authored descriptor changed at a callback fence; no next SDK writer."));};
        if(Expected.VarName!=Actual.VarName)return Changed(TEXT("VarName"),Expected.VarName.ToString(),Actual.VarName.ToString());
        if(Expected.VarGuid!=Actual.VarGuid)return Changed(TEXT("VarGuid"),Expected.VarGuid.ToString(EGuidFormats::Digits),Actual.VarGuid.ToString(EGuidFormats::Digits));
        if(!MaterialPinSame(Expected.VarType,Actual.VarType))
        {
            auto TypeOperand=[](const FEdGraphPinType& P)
            {
                return FString::Printf(TEXT("category=%s;subcategory=%s;object=%llu;container=%d;ref=%d;const=%d;weak=%d;wrapper=%d;single=%d;terminal_category=%s;terminal_subcategory=%s;terminal_object=%llu;terminal_const=%d;terminal_weak=%d;terminal_wrapper=%d"),
                    *P.PinCategory.ToString(),*P.PinSubCategory.ToString(),uint64(reinterpret_cast<UPTRINT>(P.PinSubCategoryObject.Get())),int32(P.ContainerType),int32(P.bIsReference),int32(P.bIsConst),int32(P.bIsWeakPointer),int32(P.bIsUObjectWrapper),int32(P.bSerializeAsSinglePrecisionFloat),
                    *P.PinValueType.TerminalCategory.ToString(),*P.PinValueType.TerminalSubCategory.ToString(),uint64(reinterpret_cast<UPTRINT>(P.PinValueType.TerminalSubCategoryObject.Get())),int32(P.PinValueType.bTerminalIsConst),int32(P.PinValueType.bTerminalIsWeakPointer),int32(P.PinValueType.bTerminalIsUObjectWrapper));
            };
            return Changed(TEXT("VarType"),TypeOperand(Expected.VarType),TypeOperand(Actual.VarType));
        }
        if(!Expected.FriendlyName.Equals(Actual.FriendlyName,ESearchCase::CaseSensitive))return Changed(TEXT("FriendlyName"),Expected.FriendlyName,Actual.FriendlyName);
        if(FTextInspector::GetSharedDataId(Expected.Category)!=FTextInspector::GetSharedDataId(Actual.Category)||FTextInspector::GetFlags(Expected.Category)!=FTextInspector::GetFlags(Actual.Category))
        {
            // This authored descriptor is not rewritten by the structural-only SDK.
            // Do not call IdenticalTo/ToString: either can rebuild localized text
            // and invalidate the current borrowed NewVariables storage.
            auto TextOperand=[](const FText& Text){return FString::Printf(TEXT("shared_data_id=%llu;flags=%u"),uint64(reinterpret_cast<UPTRINT>(FTextInspector::GetSharedDataId(Text))),uint32(FTextInspector::GetFlags(Text)));};
            return Changed(TEXT("Category.identity"),TextOperand(Expected.Category),TextOperand(Actual.Category));
        }
        if(Expected.PropertyFlags!=Actual.PropertyFlags)return Changed(TEXT("PropertyFlags"),LexToString(Expected.PropertyFlags),LexToString(Actual.PropertyFlags));
        if(Expected.RepNotifyFunc!=Actual.RepNotifyFunc)return Changed(TEXT("RepNotifyFunc"),Expected.RepNotifyFunc.ToString(),Actual.RepNotifyFunc.ToString());
        if(Expected.ReplicationCondition!=Actual.ReplicationCondition)return Changed(TEXT("ReplicationCondition"),LexToString(int32(Expected.ReplicationCondition.GetValue())),LexToString(int32(Actual.ReplicationCondition.GetValue())));
        if(!Expected.DefaultValue.Equals(Actual.DefaultValue,ESearchCase::CaseSensitive))return Changed(TEXT("DefaultValue"),Expected.DefaultValue,Actual.DefaultValue);
        if(Expected.MetaDataArray.Num()!=Actual.MetaDataArray.Num())return Changed(TEXT("MetaDataArray.Num"),LexToString(Expected.MetaDataArray.Num()),LexToString(Actual.MetaDataArray.Num()));
        for(int32 I=0;I<Expected.MetaDataArray.Num();++I)
        {
            const FBPVariableMetaDataEntry& E=Expected.MetaDataArray[I];const FBPVariableMetaDataEntry& A=Actual.MetaDataArray[I];
            const FString Prefix=TEXT("MetaDataArray[")+LexToString(I)+TEXT("]");
            if(E.DataKey!=A.DataKey)return Changed(*(Prefix+TEXT(".DataKey")),E.DataKey.ToString(),A.DataKey.ToString());
            if(!E.DataValue.Equals(A.DataValue,ESearchCase::CaseSensitive))return Changed(*(Prefix+TEXT(".DataValue")),E.DataValue,A.DataValue);
        }
        return true;
    }
    bool AuthoredCurrent()
    {
        UBlueprint* B=Blueprint.Get();if(!AuthoredReady)return Bad(TEXT("Authored declaration/interface plan was not captured."));
        const int32 ExpectedCount=OriginalDeclarations.Num()-(After?1:0);
        if(B->NewVariables.Num()!=ExpectedCount)return Difference(BlueprintPath,TEXT("NewVariables.Num"),LexToString(ExpectedCount),LexToString(B->NewVariables.Num()),TEXT("Current descriptor inventory differs from the exact selected-row subtraction."));
        int32 ActualIndex=0;
        for(const FBPVariableDescription& Expected:OriginalDeclarations)
        {
            if(After&&Expected.VarName==Variable)continue;
            const FBPVariableDescription& Actual=B->NewVariables[ActualIndex++];
            if(!DescriptorCurrent(Expected,Actual))return false;
        }
        if(B->ImplementedInterfaces.Num()!=OwnInterfaces.Num())return Difference(BlueprintPath,TEXT("ImplementedInterfaces.Num"),LexToString(OwnInterfaces.Num()),LexToString(B->ImplementedInterfaces.Num()),TEXT("Own interface membership changed before the next writer."));
        for(int32 I=0;I<OwnInterfaces.Num();++I)
        {
            const FInterfaceRole& Expected=OwnInterfaces[I];FBPInterfaceDescription& Actual=B->ImplementedInterfaces[I];
            if(!Actual.Interface.GetGCPtr().IsResolved()||Actual.Interface.Get()!=Expected.Class.Get()||!DeclarationEnumLoaded(Expected.Class.Get())
                ||Expected.Class->GetOuter()!=Expected.Outer||Expected.Class->GetFlags()!=Expected.Flags||uint32(Expected.Class->GetClassFlags())!=Expected.ClassFlags
                ||!Expected.Class->GetPathName().Equals(Expected.Path,ESearchCase::CaseSensitive)||Actual.Graphs.Num()!=Expected.Graphs.Num())return Bad(TEXT("Current own interface class role changed before the next writer."));
            for(int32 K=0;K<Expected.Graphs.Num();++K)if(!Actual.Graphs[K].IsResolved()||Actual.Graphs[K].Get()!=Expected.Graphs[K])return Bad(TEXT("Current interface graph membership/order changed before the next writer."));
        }
        const TArray<FImplementedInterface>& ActualNative=AActor::StaticClass()->Interfaces;
        if(ActualNative.Num()!=NativeActorInterfaces.Num())return Bad(TEXT("Native Actor interface membership changed."));
        for(int32 I=0;I<ActualNative.Num();++I)
        {
            const FImplementedInterface& A=ActualNative[I];const FImplementedInterface& E=NativeActorInterfaces[I];
            if(!A.Class.IsResolved()||A.Class.Get()!=E.Class.Get()||A.PointerOffset!=E.PointerOffset||A.bImplementedByK2!=E.bImplementedByK2)return Bad(TEXT("Native Actor interface descriptor changed."));
        }
        return true;
    }
    bool Plain(bool ReacquirePins=false)
    {
        if(Stopped)return false;
        UBlueprint* B=Blueprint.Get();
        if(!IsEditorGameThread()||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()
            ||!DeclarationEnumLoaded(B)||!DeclarationEnumLoaded(Package.Get())||B->GetPackage()!=Package.Get()
            ||!B->GetPathName().Equals(BlueprintPath,ESearchCase::CaseSensitive)||B->ParentClass.Get()!=AActor::StaticClass()
            ||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad||B->bSuppressStructurallyModified)
            return Bad(TEXT("Captured linked-removal target/context changed; no next SDK."));
        if(!AuthoredCurrent())return false;
        if(!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass!=Generated.Get()||!DeclarationEnumLoaded(Generated.Get())
            ||!Generated->ClassDefaultObject.IsResolved()||Generated->ClassDefaultObject!=Default.Get()
            ||!DeclarationEnumLoaded(Default.Get())||Default->GetClass()!=Generated.Get()
            ||Generated->ClassGeneratedBy!=B||Generated->GetPackage()!=Package.Get()
            ||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!DeclarationEnumLoaded(B->SkeletonGeneratedClass))
            return Bad(TEXT("Captured generated class/CDO or current skeleton role changed."));
        if(B->SkeletonGeneratedClass!=Skeleton.Get()||Skeleton->ClassGeneratedBy!=B||Skeleton->GetClass()!=SkeletonMeta
            ||Skeleton->GetOuter()!=SkeletonOuter||Skeleton->GetPackage()!=Package.Get()
            ||!Skeleton->GetPathName().Equals(SkeletonPath,ESearchCase::CaseSensitive)||Skeleton->GetFlags()!=SkeletonFlags
            ||uint32(Skeleton->GetClassFlags())!=SkeletonClassFlags)
            return Bad(TEXT("Current skeleton does not retain the validated structural-return role."));
        if(!AActor::StaticClass()->ClassDefaultObject.IsResolved()||AActor::StaticClass()->ClassDefaultObject!=NativeCDO.Get()||!DeclarationEnumLoaded(NativeCDO.Get()))
            return Bad(TEXT("Captured native Actor source CDO role changed."));
        for(const FClassFields& Role:FieldOwners)
        {
            if(!DeclarationEnumLoaded(Role.Hold.Get()))return Bad(TEXT("Retained observation class is no longer live."));
            TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(Role.Hold.Get());It;++It)Fields.Add(*It);
            if(Fields!=Role.Fields)return Bad(TEXT("Current reflected field membership changed."));
            for(int32 I=0;I<Fields.Num();++I)
                if(Fields[I]->GetClass()!=Role.Classes[I]||Fields[I]->GetOwnerStruct()!=Role.Owners[I]||Fields[I]->ArrayDim!=Role.Dimensions[I]||uint64(Fields[I]->GetPropertyFlags())!=Role.Flags[I]||Fields[I]->GetOffset_ForInternal()!=Role.Offsets[I]||Fields[I]->GetSize()!=Role.Sizes[I])
                    return Bad(TEXT("Retained reflected field layout changed."));
        }
        if(!B->SimpleConstructionScript.IsResolved()||B->SimpleConstructionScript!=Scs.Get()||!DeclarationEnumLoaded(Scs.Get())
            ||Scs->GetAllNodes()!=ScsAll||Scs->GetRootNodes()!=ScsRoots)return Bad(TEXT("Captured SCS membership changed."));
        for(const FScsRole& S:ScsRoles)
        {
            if(!DeclarationEnumLoaded(S.Node.Get())||!S.Node->GetPathName().Equals(S.Path,ESearchCase::CaseSensitive)||S.Node->GetOuter()!=S.Outer||S.Node->GetFlags()!=S.Flags||S.Node->VariableGuid!=S.Guid||S.Node->GetVariableName()!=S.Name
                ||S.Node->GetChildNodes()!=S.Children||!S.Node->ComponentTemplate.IsResolved()||S.Node->ComponentTemplate!=S.Template.Get()
                ||!S.Node->ComponentClass.IsResolved()||S.Node->ComponentClass!=S.Class.Get()||!DeclarationEnumLoaded(S.Template.Get())
                ||S.Template->GetClass()!=S.Class.Get()||S.Template->GetPackage()!=Package.Get()||S.Template->GetFlags()!=S.TemplateFlags||!S.Template->GetPathName().Equals(S.TemplatePath,ESearchCase::CaseSensitive))
                return Bad(TEXT("Retained SCS node/template binding changed before the next observer."));
        }
        TArray<UEdGraph*> Actual;B->GetAllGraphs(Actual);
        if(Actual.Num()!=Graphs.Num())return Difference(BlueprintPath,TEXT("owned_graph_count"),LexToString(Graphs.Num()),LexToString(Actual.Num()),TEXT("Owned graph inventory changed."));
        for(int32 I=0;I<Graphs.Num();++I)
        {
            const FGraph& G=Graphs[I];UEdGraph* A=Actual[I];
            if(A!=G.Hold.Get()||!DeclarationEnumLoaded(A)||A->GetOuter()!=B||A->GetPackage()!=Package.Get()
                ||!A->GetPathName().Equals(G.Path,ESearchCase::CaseSensitive)||A->GraphGuid!=G.Guid||A->GetClass()!=G.Class||A->Schema!=UEdGraphSchema_K2::StaticClass())
                return Bad(TEXT("Owned graph identity/order changed."));
            if(A->GetFlags()!=G.Flags)return Difference(G.Path,TEXT("object_flags"),LexToString(uint32(G.Flags)),LexToString(uint32(A->GetFlags())),TEXT("Graph object flags changed."));
            TArray<UEdGraphNode*> Expected;for(UEdGraphNode* N:G.Order)if(!After||!Removed.Contains(N))Expected.Add(N);
            if(A->Nodes.Num()!=Expected.Num())return Difference(G.Path,TEXT("Nodes.Num"),LexToString(Expected.Num()),LexToString(A->Nodes.Num()),TEXT("Unexpected graph node inventory after planned deletion."));
            for(int32 K=0;K<Expected.Num();++K)if(A->Nodes[K].Get()!=Expected[K])return Difference(G.Path,TEXT("Nodes.slot_address"),LexToString(reinterpret_cast<UPTRINT>(Expected[K])),LexToString(reinterpret_cast<UPTRINT>(A->Nodes[K].Get())),TEXT("Nonselected node order/identity changed."));
        }
        // All current memberships are closed before retained node/pin addresses are read.
        TMap<UEdGraphPin*,UEdGraphPin*> PinsNow;
        for(FNode& N:Nodes)
        {
            if(After&&N.Selected)continue;
            UEdGraphNode* A=N.Hold.Get();
            if(DeclarationEnumLoaded(A)&&A->GetFlags()!=N.Flags)return Difference(N.Path,TEXT("object_flags"),LexToString(uint32(N.Flags)),LexToString(uint32(A->GetFlags())),TEXT("Surviving node object flags changed."));
            if(!DeclarationEnumLoaded(A)||A->GetOuter()!=N.Graph||A->GetPackage()!=Package.Get()
                ||A->GetClass()!=N.Class||A->NodeGuid!=N.Guid||!A->GetPathName().Equals(N.Path,ESearchCase::CaseSensitive)||A->GetFlags()!=N.Flags
                ||A->NodePosX!=N.X||A->NodePosY!=N.Y||A->Pins.Num()!=N.Pins.Num())
                return Bad(TEXT("Current surviving node identity or pin inventory changed."));
            if(N.VariableNode&&!static_cast<UK2Node_Variable*>(A)->VariableReference.IsSameReference(N.Member))
                return Bad(TEXT("Captured variable reference changed."));
            for(int32 I=0;I<N.Pins.Num();++I)
            {
                const FPin& P=N.Pins[I];UEdGraphPin* PinActual=N.Hold->Pins[I];
                if(PinActual&&PinActual==P.Address&&!PinActual->WasTrashed()&&PinActual->GetOwningNodeUnchecked()==N.Hold.Get())
                {
                    if(!PinActual->DefaultValue.Equals(P.Raw,ESearchCase::CaseSensitive))return Difference(N.Path,TEXT("pin.DefaultValue"),P.Raw,PinActual->DefaultValue,TEXT("Surviving raw default changed."));
                    if(!PinActual->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive))return Difference(N.Path,TEXT("pin.AutogeneratedDefaultValue"),P.Auto,PinActual->AutogeneratedDefaultValue,TEXT("Surviving Auto default changed."));
                }
                if(!PinActual||(!ReacquirePins&&PinActual!=P.Address)||PinActual->WasTrashed()||PinActual->GetOwningNodeUnchecked()!=N.Hold.Get()
                    ||PinActual->PinId!=P.Id||PinActual->PinName!=P.Name||PinActual->Direction!=P.Direction||PinActual->PinType!=P.Type||PinActual->PinType.bIsUObjectWrapper!=P.Type.bIsUObjectWrapper
                    ||PinActual->PinType.bSerializeAsSinglePrecisionFloat!=P.Type.bSerializeAsSinglePrecisionFloat
                    ||PinActual->PinType.PinValueType.bTerminalIsUObjectWrapper!=P.Type.PinValueType.bTerminalIsUObjectWrapper
                    ||!PinActual->DefaultValue.Equals(P.Raw,ESearchCase::CaseSensitive)||!PinActual->AutogeneratedDefaultValue.Equals(P.Auto,ESearchCase::CaseSensitive)||((!ReacquiringText&&FTextInspector::GetSharedDataId(PinActual->DefaultTextValue)!=FTextInspector::GetSharedDataId(P.Text))||FTextInspector::GetFlags(PinActual->DefaultTextValue)!=FTextInspector::GetFlags(P.Text))
                    ||!PinActual->DefaultObject.IsResolved()||PinActual->DefaultObject!=P.Default
                    ||PinActual->bOrphanedPin!=P.Orphan||PinActual->bHidden!=P.Hidden||PinActual->bNotConnectable!=P.NotConnectable)
                    return Bad(TEXT("Surviving pin type/default/role changed outside the incident-link plan."));
                PinsNow.Add(P.Address,PinActual);
            }
        }
        for(FNode& N:Nodes)
        {
            if(After&&N.Selected)continue;
            for(const FPin& P:N.Pins)
            {
                UEdGraphPin* A=PinsNow.FindChecked(P.Address);TArray<UEdGraphPin*> Expected;
                for(UEdGraphPin* L:P.Links)if(!After||!RemovedPins.Contains(L))
                {UEdGraphPin** Current=PinsNow.Find(L);if(!Current)return Bad(TEXT("Unowned/external incident pin."));Expected.Add(*Current);}
                if(A->LinkedTo!=Expected)return Difference(N.Path,TEXT("pin.LinkedTo"),LexToString(Expected.Num()),LexToString(A->LinkedTo.Num()),TEXT("Actual links differ from exactly the planned incident-link subtraction; counts may be equal while identities/order differ."));
                UEdGraphPin* ExpectedParent=nullptr;if(P.Parent){auto* V=PinsNow.Find(P.Parent);if(!V)return Bad(TEXT("Split pin parent role lost."));ExpectedParent=*V;}
                TArray<UEdGraphPin*> ExpectedChildren;for(auto* C:P.Children){auto* V=PinsNow.Find(C);if(!V)return Bad(TEXT("Split pin child role lost."));ExpectedChildren.Add(*V);}
                if(A->ParentPin!=ExpectedParent||A->SubPins!=ExpectedChildren)return Bad(TEXT("Nonselected split pin roles changed."));
            }
        }
        if(ReacquirePins)
        {
            // Explicit structural-return boundary only: commit addresses after logical
            // IDs/type/defaults/links match the original copied plan, never seed values.
            for(FNode& N:Nodes)if(!N.Selected)for(FPin& P:N.Pins)
            {
                TArray<UEdGraphPin*> Links;for(auto* L:P.Links)if(!RemovedPins.Contains(L))Links.Add(PinsNow.FindChecked(L));
                P.Links=MoveTemp(Links);if(P.Parent)P.Parent=PinsNow.FindChecked(P.Parent);
                for(auto*& C:P.Children)C=PinsNow.FindChecked(C);P.Address=PinsNow.FindChecked(P.Address);
            }
            // StructuralReturn has already validated and held the new skeleton role.
        }
        return true;
    }
    bool Build(UBlueprint* B,FName Name,bool DeclarationOnly=false)
    {
        Blueprint.Reset(B);Package.Reset(B->GetPackage());BlueprintPath=B->GetPathName();Variable=Name;
        OriginalDeclarations=B->NewVariables;int32 SelectedRows=0;for(const auto& D:OriginalDeclarations)if(D.VarName==Name)++SelectedRows;
        if(SelectedRows!=1)return Bad(TEXT("Selected authored descriptor must occur exactly once."));
        for(FBPInterfaceDescription& Description:B->ImplementedInterfaces)
        {
            if(!Description.Interface.GetGCPtr().IsResolved()||!DeclarationEnumLoaded(Description.Interface.Get()))return Bad(TEXT("Own interface must already be resolved/live before capture."));
            FInterfaceRole Role;Role.Class.Reset(Description.Interface.Get());Role.Path=Role.Class->GetPathName();Role.Outer=Role.Class->GetOuter();Role.Flags=Role.Class->GetFlags();Role.ClassFlags=uint32(Role.Class->GetClassFlags());
            for(const auto& G:Description.Graphs){if(!G.IsResolved()||!DeclarationEnumLoaded(G.Get()))return Bad(TEXT("Own interface graph must already be resolved/live before capture."));Role.Graphs.Add(G.Get());}
            OwnInterfaces.Add(MoveTemp(Role));
        }
        NativeActorInterfaces=AActor::StaticClass()->Interfaces;for(const auto& I:NativeActorInterfaces)if(!I.Class.IsResolved())return Bad(TEXT("Native Actor interface class must already be resolved before capture."));
        AuthoredReady=true;
        if(!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("Current class handles must already be resolved."));
        Generated.Reset(B->GeneratedClass.Get());Skeleton.Reset(B->SkeletonGeneratedClass.Get());
        if(!DeclarationEnumLoaded(Skeleton.Get())||Skeleton->ClassGeneratedBy!=B||Skeleton->GetPackage()!=Package.Get())return Bad(TEXT("Initial owned skeleton role unavailable."));
        SkeletonPath=Skeleton->GetPathName();SkeletonOuter=Skeleton->GetOuter();SkeletonMeta=Skeleton->GetClass();SkeletonFlags=Skeleton->GetFlags();SkeletonClassFlags=uint32(Skeleton->GetClassFlags());
        if(!DeclarationEnumLoaded(Generated.Get())||!Generated->ClassDefaultObject.IsResolved())return Bad(TEXT("Existing generated CDO must already be resolved."));
        Default.Reset(Generated->ClassDefaultObject.Get());
        if(!AActor::StaticClass()->ClassDefaultObject.IsResolved())return Bad(TEXT("Native Actor source CDO must already be resolved."));
        NativeCDO.Reset(AActor::StaticClass()->ClassDefaultObject.Get());
        for(UClass* Class:{Generated.Get(),AActor::StaticClass()})
        {
            FClassFields Role;Role.Hold.Reset(Class);
            for(TFieldIterator<FProperty> It(Class);It;++It){Role.Fields.Add(*It);Role.Classes.Add(It->GetClass());Role.Owners.Add(It->GetOwnerStruct());Role.Dimensions.Add(It->ArrayDim);Role.Flags.Add(uint64(It->GetPropertyFlags()));Role.Offsets.Add(It->GetOffset_ForInternal());Role.Sizes.Add(It->GetSize());}
            FieldOwners.Add(MoveTemp(Role));
        }
        if(!B->SimpleConstructionScript.IsResolved())return Bad(TEXT("SCS handle is unresolved."));
        Scs.Reset(B->SimpleConstructionScript.Get());if(!DeclarationEnumLoaded(Scs.Get()))return Bad(TEXT("Current SCS unavailable."));
        ScsAll=Scs->GetAllNodes();ScsRoots=Scs->GetRootNodes();
        for(USCS_Node* N:ScsAll)
        {
            if(!DeclarationEnumLoaded(N)||!N->ComponentTemplate.IsResolved()||!N->ComponentClass.IsResolved())return Bad(TEXT("Current SCS role unresolved."));
            FScsRole Role;Role.Node.Reset(N);Role.Template.Reset(N->ComponentTemplate.Get());Role.Class.Reset(N->ComponentClass.Get());
            Role.Path=N->GetPathName();Role.Outer=N->GetOuter();Role.Flags=N->GetFlags();Role.TemplatePath=Role.Template->GetPathName();Role.TemplateFlags=Role.Template->GetFlags();
            Role.Guid=N->VariableGuid;Role.Name=N->GetVariableName();Role.Children=N->GetChildNodes();
            bool HasFields=false;for(const FClassFields& Existing:FieldOwners)HasFields|=Existing.Hold.Get()==Role.Class.Get();
            if(!HasFields)
            {
                FClassFields Fields;Fields.Hold.Reset(Role.Class.Get());
                for(TFieldIterator<FProperty> It(Role.Class.Get());It;++It){Fields.Fields.Add(*It);Fields.Classes.Add(It->GetClass());Fields.Owners.Add(It->GetOwnerStruct());Fields.Dimensions.Add(It->ArrayDim);Fields.Flags.Add(uint64(It->GetPropertyFlags()));Fields.Offsets.Add(It->GetOffset_ForInternal());Fields.Sizes.Add(It->GetSize());}
                FieldOwners.Add(MoveTemp(Fields));
            }
            ScsRoles.Add(MoveTemp(Role));
        }
        TArray<UEdGraph*> All;B->GetAllGraphs(All);
        for(UEdGraph* G:All)
        {
            if(!DeclarationEnumLoaded(G)||G->GetOuter()!=B||G->Schema!=UEdGraphSchema_K2::StaticClass())
                return Fail(R,TEXT("variable_remove_linked_graph_unsupported"),TEXT("Linked deletion requires existing directly owned standard K2 graphs; other valid variants remain incomplete."));
            FGraph GR;GR.Hold.Reset(G);GR.Path=G->GetPathName();GR.Guid=G->GraphGuid;GR.Class=G->GetClass();GR.Flags=G->GetFlags();
            for(UEdGraphNode* N:G->Nodes)
            {
                if(!DeclarationEnumLoaded(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid())return Bad(TEXT("Unreadable current node membership."));
                FNode NR;NR.Hold.Reset(N);NR.Graph=G;NR.Class=N->GetClass();NR.Guid=N->NodeGuid;NR.Path=N->GetPathName();NR.Flags=N->GetFlags();NR.X=N->NodePosX;NR.Y=N->NodePosY;
                auto* V=Cast<UK2Node_Variable>(N);if(V){NR.VariableNode=true;NR.Member=V->VariableReference;}
                if(V&&V->GetVarName()==Name&&V->VariableReference.GetMemberParentClass(Generated.Get())==Generated.Get())
                {
                    if((N->GetClass()!=UK2Node_VariableGet::StaticClass()&&N->GetClass()!=UK2Node_VariableSet::StaticClass())
                        ||!V->VariableReference.IsSelfContext()||V->VariableReference.IsLocalScope())
                        return Fail(R,TEXT("variable_remove_linked_node_unsupported"),TEXT("Only exact own self-context member Get/Set nodes are admitted; custom/local/external node variants remain incomplete."));
                    TArray<UObject*> Children;GetObjectsWithOuter(N,Children,EGetObjectsFlags::IncludeNestedObjects);
                    if(!Children.IsEmpty())return Fail(R,TEXT("variable_remove_linked_node_subobjects_unsupported"),TEXT("Owned node subobject retirement remains incomplete."));
                    NR.Selected=true;Removed.Add(N);RemovedPaths.Add(NR.Path);
                }
                TSet<FGuid> IDs;
                for(UEdGraphPin* P:N->Pins)
                {
                    if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||!P->PinId.IsValid()||IDs.Contains(P->PinId)||!P->DefaultObject.IsResolved())
                        return Bad(TEXT("Pin inventory is unreadable/ambiguous."));
                    IDs.Add(P->PinId);FPin PR;PR.Address=P;PR.Id=P->PinId;PR.Name=P->PinName;PR.Direction=P->Direction;PR.Type=P->PinType;
                    PR.Raw=P->DefaultValue;PR.Auto=P->AutogeneratedDefaultValue;PR.Text=P->DefaultTextValue;PR.Default=P->DefaultObject.Get();PR.DefaultHold.Reset(PR.Default);
                    PR.Links=P->LinkedTo;PR.Parent=P->ParentPin;PR.Children=P->SubPins;PR.Orphan=P->bOrphanedPin;PR.Hidden=P->bHidden;PR.NotConnectable=P->bNotConnectable;
                    NR.Pins.Add(MoveTemp(PR));if(NR.Selected)RemovedPins.Add(P);
                }
                GR.Order.Add(N);Nodes.Add(MoveTemp(NR));
            }
            Graphs.Add(MoveTemp(GR));
        }
        Active=!Removed.IsEmpty()||DeclarationOnly;if(!Active)return true;
        TSet<UEdGraphPin*> Owned;for(const FNode& N:Nodes)for(const FPin& P:N.Pins)Owned.Add(P.Address);
        for(const FNode& N:Nodes)for(const FPin& P:N.Pins)
        {
            TSet<UEdGraphPin*> Unique;
            for(UEdGraphPin* L:P.Links)
            {
                if(!Owned.Contains(L)||L==P.Address||Unique.Contains(L)||L->GetOwningNodeUnchecked()->GetGraph()!=N.Graph
                    ||(L->LinkedTo.FilterByPredicate([&P](UEdGraphPin* Link){return Link==P.Address;}).Num()!=1)||L->Direction==P.Direction)
                    return Fail(R,TEXT("variable_remove_linked_links_unsupported"),TEXT("Only unique directly owned reciprocal same-graph links are admitted."));
                Unique.Add(L);if(N.Selected||RemovedPins.Contains(L))++IncidentDirected;
                // BreakLinkTo also clears snapping bookkeeping for paired snapped pins.
                // That uncommon editor layout variant is not silently treated as exact.
                if((N.Selected||RemovedPins.Contains(L))&&(P.Address->IsSnapped()||L->IsSnapped()))
                    return Fail(R,TEXT("variable_remove_linked_snapping_unsupported"),TEXT("Snapped pin bookkeeping remains incomplete; no writer dispatched."));
            }
        }
        R.Data->SetBoolField(TEXT("native_linked_variable_removal"),!Removed.IsEmpty());
        R.Data->SetArrayField(TEXT("selected_variable_node_paths"),StringValues(RemovedPaths.Array()));
        R.Data->SetNumberField(TEXT("planned_removed_node_count"),Removed.Num());R.Data->SetNumberField(TEXT("planned_incident_link_count"),IncidentDirected/2);
        R.Data->SetBoolField(TEXT("retired_node_pin_values_reread"),false);
        R.Data->SetStringField(TEXT("linked_removal_scope"),TEXT("Exact own simple primitive member and optional Get/Set retirement; includes replication/RepNotify/condition associations and direct nativeActor FieldNotify metadata without an implementing interface. Selected membership and incident reciprocal links removed; full surviving authored descriptors, own/nativeActor interface membership, graph state/order and SCS/source remain guarded. Actual interface-owned FieldNotify cascade remains incomplete. Retained objects do not prove GC destruction. No explicit full compile/save."));
        return Plain();
    }
    bool StructuralReturn()
    {
        if(Stopped)return false;UBlueprint* B=Blueprint.Get();
        if(!DeclarationEnumLoaded(B)||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Bad(TEXT("Structural return lost the target/skeleton handle."));
        TStrongObjectPtr<UClass> ReturnedSkeleton(B->SkeletonGeneratedClass.Get());
        auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("stage"),R.Stage);Receipt->SetStringField(TEXT("expected_path"),SkeletonPath);Receipt->SetStringField(TEXT("expected_flags"),LexToString(uint32(SkeletonFlags)));
        const bool LiveReturned=DeclarationEnumLoaded(ReturnedSkeleton.Get());Receipt->SetBoolField(TEXT("returned_live"),LiveReturned);
        if(LiveReturned){Receipt->SetStringField(TEXT("actual_path"),ReturnedSkeleton->GetPathName());Receipt->SetStringField(TEXT("actual_flags"),LexToString(uint32(ReturnedSkeleton->GetFlags())));Receipt->SetStringField(TEXT("actual_class_flags"),LexToString(uint32(ReturnedSkeleton->GetClassFlags())));}
        R.Data->SetObjectField(TEXT("last_structural_return_skeleton"),Receipt);
        if(!DeclarationEnumLoaded(ReturnedSkeleton.Get())||ReturnedSkeleton->ClassGeneratedBy!=B||ReturnedSkeleton->GetPackage()!=Package.Get()
            ||ReturnedSkeleton->GetClass()!=SkeletonMeta||ReturnedSkeleton->GetOuter()!=SkeletonOuter
            ||!ReturnedSkeleton->GetPathName().Equals(SkeletonPath,ESearchCase::CaseSensitive)||ReturnedSkeleton->GetFlags()!=SkeletonFlags
            ||uint32(ReturnedSkeleton->GetClassFlags())!=SkeletonClassFlags)
            return Bad(TEXT("Returned skeleton identity differs from the planned owned class role."));
        Skeleton=MoveTemp(ReturnedSkeleton);After=true;
        // The two original SDK calls can regenerate the skeleton and physical pins.
        // Reacquisition is logical-ID based; retired nodes/pins are never inspected.
        ReacquiringText=true;if(!Plain(true))return false;
        auto Observed=MakeShared<FJsonObject>();if(!Snapshot(Observed))return false;
        if(!GraphBaseline.IsValid()||!LifecycleGraphSame(ExpectedGraphs(GraphBaseline.ToSharedRef()),Observed,false))
            return Bad(TEXT("Surviving logical graph/default state differs after structural return."));
        for(FNode& N:Nodes)if(!N.Selected)for(FPin& P:N.Pins)P.Text=P.Address->DefaultTextValue;
        ReacquiringText=false;return Plain();
    }
    bool Snapshot(const TSharedRef<FJsonObject>& Out)
    {
        if(!Plain())return false;TMap<UEdGraphPin*,const void*> TextIdentities;
        for(const FGraph& G:Graphs)
        {
            auto Row=MakeShared<FJsonObject>();Out->SetObjectField(G.Path,Row);Row->SetObjectField(TEXT("identity"),LifecycleIdentity(G.Hold.Get()));Row->SetStringField(TEXT("guid"),G.Guid.ToString(EGuidFormats::Digits));
            auto NodeRows=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("nodes"),NodeRows);
            for(const FNode& N:Nodes)
            {
                if(N.Graph!=G.Hold.Get()||(After&&N.Selected))continue;
                auto J=MakeShared<FJsonObject>();NodeRows->SetObjectField(N.Path,J);J->SetObjectField(TEXT("identity"),LifecycleIdentity(N.Hold.Get()));J->SetStringField(TEXT("guid"),N.Guid.ToString(EGuidFormats::Digits));J->SetNumberField(TEXT("x"),N.X);J->SetNumberField(TEXT("y"),N.Y);
                TArray<TSharedPtr<FJsonValue>> Pins;
                for(const FPin& P:N.Pins)
                {
                    // No node/pin data is touched across a text localization callback.
                    const FText CopiedText=P.Address->DefaultTextValue;TextIdentities.Add(P.Address,FTextInspector::GetSharedDataId(CopiedText));const FString Text=CopiedText.ToString();if(!Plain())return false;
                    auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("id"),P.Id.ToString(EGuidFormats::Digits));Q->SetStringField(TEXT("name"),P.Name.ToString());Q->SetNumberField(TEXT("direction"),P.Direction);
                    Q->SetObjectField(TEXT("type"),PinJson(P.Type));if(!Plain())return false;Q->SetStringField(TEXT("default_value"),P.Raw);Q->SetStringField(TEXT("default_text"),Text);Q->SetStringField(TEXT("default_object"),P.Default?P.Default->GetPathName():FString());
                    TArray<TSharedPtr<FJsonValue>> Links;
                    for(auto* L:P.Address->LinkedTo)
                    {
                        auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("node"),L->GetOwningNodeUnchecked()->GetPathName());A->SetStringField(TEXT("pin"),L->PinName.ToString());A->SetStringField(TEXT("id"),L->PinId.ToString(EGuidFormats::Digits));Links.Add(MakeShared<FJsonValueObject>(A));
                    }
                    Q->SetArrayField(TEXT("links"),Links);Pins.Add(MakeShared<FJsonValueObject>(Q));
                }
                J->SetArrayField(TEXT("pins"),Pins);
            }
        }
        if(!Plain())return false;
        for(const auto& Pair:TextIdentities)if(FTextInspector::GetSharedDataId(Pair.Key->DefaultTextValue)!=Pair.Value)return Bad(TEXT("A copied pin text role changed during graph observation."));
        if(!After)GraphBaseline=Out;
        return true;
    }
    TSharedRef<FJsonObject> ExpectedGraphs(const TSharedRef<FJsonObject>& Before)
    {
        TSharedPtr<FJsonObject> Copy;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText(Before)),Copy);
        for(auto& Pair:Copy->Values)
        {
            auto Rows=Pair.Value->AsObject()->GetObjectField(TEXT("nodes"));for(const FString& Path:RemovedPaths)Rows->RemoveField(Path);
            for(auto& Node:Rows->Values)for(const auto& Pin:Node.Value->AsObject()->GetArrayField(TEXT("pins")))
            {
                TArray<TSharedPtr<FJsonValue>> Keep;for(const auto& L:Pin->AsObject()->GetArrayField(TEXT("links")))if(!RemovedPaths.Contains(L->AsObject()->GetStringField(TEXT("node"))))Keep.Add(L);
                Pin->AsObject()->SetArrayField(TEXT("links"),Keep);
            }
        }
        return Copy.ToSharedRef();
    }
    bool References(const TSharedRef<FJsonObject>& Out)
    {
        if(!Plain())return false;TArray<UObject*> Objects;GetObjectsWithOuter(Package.Get(),Objects,EGetObjectsFlags::IncludeNestedObjects);if(Objects.Num()>4096)return Bad(TEXT("Reference inventory exceeds the existing bound."));
        int32 Total=0;
        for(UObject* O:Objects)
        {
            if(After&&Removed.Contains(O))continue; // address test before any retired-object read
            if(!IsValid(O)||O->HasAnyFlags(RF_Transient)||O->GetPackage()!=Package.Get())continue;
            TStrongObjectPtr<UObject> Held(O);const FString Path=O->GetPathName();const UClass* Class=O->GetClass();UObject* Outer=O->GetOuter();
            TArray<UObject*> Refs;FReferenceFinder Finder(Refs,nullptr,false,true,false,true);Finder.FindReferences(O);
            TArray<TStrongObjectPtr<UObject>> Returned;Returned.Reserve(Refs.Num());
            for(UObject* Ref:Refs)
            {
                if(After&&Removed.Contains(Ref))return Bad(TEXT("Returned serialization includes a retired selected address; no retired read."));
                Returned.Emplace(Ref);
            }
            if(!Plain()||!DeclarationEnumLoaded(Held.Get())||Held->GetClass()!=Class||Held->GetOuter()!=Outer||!Held->GetPathName().Equals(Path,ESearchCase::CaseSensitive))return Bad(TEXT("Reference callback changed an observed object."));
            TArray<FString> Paths;
            for(UObject* Ref:Refs)
            {
                if(After&&Removed.Contains(Ref))return Bad(TEXT("Unexpected surviving serialized reference to a retired selected node."));
                if(!IsValid(Ref)||++Total>16384)return Bad(TEXT("Unreadable serialized reference."));
                const FString P=Ref->GetPathName();for(const TCHAR* Token:{TEXT("/Script/CubelessMCP"),TEXT("/Script/UnrealMCP"),TEXT("/Script/ModelContextProtocol"),TEXT("/Script/ToolsetRegistry"),TEXT("/CubelessMCP_UE58/"),TEXT("/UnrealMCP/")})if(P.Contains(Token,ESearchCase::IgnoreCase))return Bad(TEXT("Disallowed observed dependency."));
                Paths.AddUnique(P);
            }
            Paths.Sort();Out->SetArrayField(Path,StringValues(Paths));
        }
        return Plain();
    }
    TSharedRef<FJsonObject> ExpectedReferences(const TSharedRef<FJsonObject>& Before)
    {
        auto Expected=MakeShared<FJsonObject>();
        for(const auto& Pair:Before->Values)
        {
            const FString Key(Pair.Key.ToView());if(RemovedPaths.Contains(Key))continue;
            TArray<TSharedPtr<FJsonValue>> Keep;for(const auto& V:Pair.Value->AsArray())if(!RemovedPaths.Contains(V->AsString()))Keep.Add(V);
            Expected->SetArrayField(Key,Keep);
        }
        return Expected;
    }
};


// ADA: original RemoveMemberVariable does not delete RepNotify functions. Capture
// authored replication associations and generated executable history separately
// from compiler-owned RepIndex/ClassReps caches. No runtime Net driver is called.
bool RemovalReplicationState(UBlueprint* BP,const FBPVariableDescription& Selected,FRemovalLinkedPlan& Plan,const TSharedRef<FJsonObject>& Out)
{
 if(!Plan.Plain())return false;
 Out->SetBoolField(TEXT("complete"),false);Out->SetStringField(TEXT("selected_notify"),Selected.RepNotifyFunc.ToString());
 if(!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Plan.Bad(TEXT("Replication observation requires current already-resolved generated/skeleton classes."));
 TStrongObjectPtr<UClass> Classes[2]={TStrongObjectPtr<UClass>(BP->GeneratedClass.Get()),TStrongObjectPtr<UClass>(BP->SkeletonGeneratedClass.Get())};
 for(int32 I=0;I<2;++I)
 {
  UClass* C=Classes[I].Get();if(!DeclarationEnumLoaded(C)||C->ClassGeneratedBy!=BP||C->GetPackage()!=BP->GetPackage())return Plan.Bad(TEXT("Replication class ownership changed before observation."));
  auto Row=MakeShared<FJsonObject>();Out->SetObjectField(I?TEXT("skeleton"):TEXT("generated"),Row);auto Properties=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("own_replication_properties"),Properties);int32 Count=0;
  for(TFieldIterator<FProperty> It(C,EFieldIteratorFlags::ExcludeSuper);It;++It)
  {
   FProperty* P=*It;if(++Count>512||P->ArrayDim!=1)return Plan.Bad(TEXT("Replication field inventory exceeds the existing simple-member profile."));
   if(!P->HasAnyPropertyFlags(CPF_Net|CPF_RepNotify)&&P->RepNotifyFunc.IsNone())continue;
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("kind"),P->GetClass()->GetName());J->SetStringField(TEXT("owner"),P->GetOwnerStruct()->GetPathName());J->SetStringField(TEXT("flags"),LexToString(uint64(P->GetPropertyFlags())));J->SetStringField(TEXT("notify"),P->RepNotifyFunc.ToString());Properties->SetObjectField(P->GetName(),J);
  }
  // A structural-only request may retain the old generated property/executable.
  // Cache counters are observations, not promises of a runtime net-layout rebuild.
  Row->SetNumberField(TEXT("observed_ClassReps_count"),C->ClassReps.Num());Row->SetNumberField(TEXT("observed_NetFields_count"),C->NetFields.Num());
  auto Notify=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("selected_notify_function"),Notify);UFunction* Function=Selected.RepNotifyFunc.IsNone()?nullptr:C->FindFunctionByName(Selected.RepNotifyFunc);
  TStrongObjectPtr<UFunction> Hold(Function);Notify->SetBoolField(TEXT("exists"),Function!=nullptr);
  if(Function)
  {
   if(!DeclarationEnumLoaded(Function)||Function->GetOuter()!=C||Function->GetPackage()!=BP->GetPackage())return Plan.Bad(TEXT("Selected RepNotify function is not an exact current own class member."));
   Notify->SetStringField(TEXT("path"),Function->GetPathName());Notify->SetStringField(TEXT("flags"),LexToString(uint32(Function->FunctionFlags)));Notify->SetNumberField(TEXT("num_parms"),Function->NumParms);Notify->SetNumberField(TEXT("parms_size"),Function->ParmsSize);
   TArray<TSharedPtr<FJsonValue>> Params;int32 N=0;for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm))continue;if(++N>32)return Plan.Bad(TEXT("RepNotify signature exceeds finite observation scope."));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),It->GetName());J->SetStringField(TEXT("kind"),It->GetClass()->GetName());J->SetStringField(TEXT("flags"),LexToString(uint64(It->GetPropertyFlags())));J->SetNumberField(TEXT("array_dim"),It->ArrayDim);J->SetNumberField(TEXT("size"),It->GetSize());Params.Add(MakeShared<FJsonValueObject>(J));}Notify->SetArrayField(TEXT("params"),Params);
   if(!I){if(Function->Script.Num()>1048576)return Plan.Bad(TEXT("Retained generated RepNotify executable exceeds finite evidence budget."));FSHAHash Hash;FSHA1::HashBuffer(Function->Script.GetData(),Function->Script.Num(),Hash.Hash);Notify->SetStringField(TEXT("existing_generated_script_sha1"),Hash.ToString());}
  }
  if(!Plan.Plain())return false;
 }
 TArray<TSharedPtr<FJsonValue>> Interfaces;for(auto& Interface:BP->ImplementedInterfaces){if(!Interface.Interface.GetGCPtr().IsResolved()||!DeclarationEnumLoaded(Interface.Interface.Get()))return Plan.Bad(TEXT("Own interface reference is not currently resolved."));Interfaces.Add(MakeShared<FJsonValueString>(Interface.Interface->GetPathName()));}Out->SetArrayField(TEXT("own_interfaces"),Interfaces);
 Out->SetBoolField(TEXT("runtime_replication_rebuild_proven"),false);Out->SetBoolField(TEXT("complete"),true);return Plan.Plain();
}
bool RemovalReplicationPreserved(const TSharedRef<FJsonObject>& Before,const TSharedRef<FJsonObject>& After,FName Name,FResult& R)
{
 bool Okay=Before->GetBoolField(TEXT("complete"))&&After->GetBoolField(TEXT("complete"))&&LifecycleEqual(Before->TryGetField(TEXT("own_interfaces")),After->TryGetField(TEXT("own_interfaces")));
 for(const TCHAR* Key:{TEXT("generated"),TEXT("skeleton")})
 {
  const auto A=Before->GetObjectField(Key),B=After->GetObjectField(Key);auto X=MakeShared<FJsonObject>(),Y=MakeShared<FJsonObject>();
  for(const auto& E:A->GetObjectField(TEXT("own_replication_properties"))->Values)if(FName(*FString(E.Key.ToView()))!=Name)X->SetField(E.Key,E.Value);
  for(const auto& E:B->GetObjectField(TEXT("own_replication_properties"))->Values)if(FName(*FString(E.Key.ToView()))!=Name)Y->SetField(E.Key,E.Value);
  Okay&=LifecycleEqual(X,Y)&&LifecycleEqual(A->TryGetField(TEXT("selected_notify_function")),B->TryGetField(TEXT("selected_notify_function")));
  const auto Was=A->GetObjectField(TEXT("own_replication_properties"))->TryGetField(Name.ToString()),Now=B->GetObjectField(TEXT("own_replication_properties"))->TryGetField(Name.ToString());
  Okay&=!Now||(Was&&LifecycleEqual(Was,Now));
 }
 R.Data->SetBoolField(TEXT("nonselected_replication_associations_preserved"),Okay);R.Data->SetBoolField(TEXT("RepNotify_functions_preserved"),Okay);return Okay;
}

bool RemovalAdmission(UBlueprint* BP, const FBPVariableDescription& Selected, FResult& R)
{
    auto Proof = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("removal_admission"), Proof);
    Proof->SetBoolField(TEXT("complete"), false); Proof->SetBoolField(TEXT("unused_local_scalar"), false);
    Proof->SetBoolField(TEXT("selected_field_notify"), Selected.HasMetaData(TEXT("FieldNotify")));
    Proof->SetArrayField(TEXT("selected_variable_nodes"), {}); Proof->SetArrayField(TEXT("selected_scs_collisions"), {});
    // ADA: on the current exact native Actor parent, an empty own interface list
    // cannot implement native NotBlueprintable NotifyFieldValueChanged. The SDK
    // therefore does not run its FieldNotify cascade; arbitrary interface owners
    // remain pending, rather than treating that cascade as ordinary deletion.
    if (Selected.HasMetaData(TEXT("FieldNotify")) && !BP->ImplementedInterfaces.IsEmpty())
    { return Fail(R, TEXT("variable_remove_field_notify_unsupported"), TEXT("Actual interface-owned FieldNotify metadata cascade remains incomplete; no removal dispatched.")); }
    Proof->SetBoolField(TEXT("selected_replication"), !Selected.RepNotifyFunc.IsNone() || (Selected.PropertyFlags & (CPF_Net | CPF_RepNotify)) || static_cast<int32>(Selected.ReplicationCondition) != 0);
    Proof->SetBoolField(TEXT("field_notify_cascade_expected"), false);
    FProperty* Property = BP->GeneratedClass ? FindFProperty<FProperty>(BP->GeneratedClass, Selected.VarName) : nullptr;
    if (!Property || Property->GetOwnerStruct() != BP->GeneratedClass)
    { return Fail(R, TEXT("variable_remove_current_local_field_required"), TEXT("Selected current generated field must belong to this exact local generated class.")); }
    TArray<FString> Nodes, Collisions; TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); int32 Count = 0;
    if (Graphs.Num() > 128) { return Fail(R, TEXT("variable_remove_reference_scan_incomplete"), TEXT("Owned graph admission exceeds the existing bounded scope.")); }
    for (UEdGraph* Graph : Graphs)
    {
        if (!IsValid(Graph) || Graph->GetOutermost() != BP->GetOutermost()) { return Fail(R, TEXT("variable_remove_reference_scan_incomplete"), TEXT("Owned graph admission is unreadable.")); }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!IsValid(Node) || ++Count > 8192) { return Fail(R, TEXT("variable_remove_reference_scan_incomplete"), TEXT("Owned node admission is unreadable or exceeds its bound.")); }
            const auto Variable = Cast<UK2Node_Variable>(Node);
            if (Variable && Variable->GetVarName() == Selected.VarName && Variable->VariableReference.GetMemberParentClass(BP->GeneratedClass) == BP->GeneratedClass)
            { Nodes.Add(Variable->GetPathName()); Proof->SetArrayField(TEXT("selected_variable_nodes"), StringValues(Nodes)); }
        }
    }
    const USimpleConstructionScript* SCS = BP->SimpleConstructionScript.Get();
    if (!IsValid(SCS) || SCS->GetAllNodes().Num() > 128) { return Fail(R, TEXT("variable_remove_scs_unobservable"), TEXT("A bounded existing SCS is required before removing a local member.")); }
    for (const USCS_Node* Node : SCS->GetAllNodes())
    {
        if (!IsValid(Node)) { return Fail(R, TEXT("variable_remove_scs_unobservable"), TEXT("Existing SCS contains an unreadable node.")); }
        if (Node->GetVariableName() == Selected.VarName) { Collisions.Add(Node->GetPathName()); Proof->SetArrayField(TEXT("selected_scs_collisions"), StringValues(Collisions)); }
    }
    Proof->SetNumberField(TEXT("graphs_scanned"), Graphs.Num()); Proof->SetNumberField(TEXT("nodes_scanned"), Count);
    Proof->SetBoolField(TEXT("complete"), true);
    if (!Collisions.IsEmpty()) { return Fail(R, TEXT("variable_remove_scs_collision_unsupported"), TEXT("Selected component/member collision requires separate SCS lifecycle support.")); }
    Proof->SetBoolField(TEXT("unused_local_scalar"), true); return true;
}
bool RemovalSCS(UBlueprint* BP, const TSharedRef<FJsonObject>& Out, FDeclarationEnumScope* EnumScope = nullptr, TFunction<bool()> Fence = {})
{
    if(EnumScope&&!EnumScope->Admit()) { return false; }
    // ME: a function library has no construction script. This is an exact
    // current observation, not a waiver for Actor or other lifecycle callers.
    if(EnumScope&&EnumScope->LocalMapMode&&EnumScope->PresetMapLibrary)
    {
        Out->SetBoolField(TEXT("complete"),false);
        const bool SameBlueprint=EnumScope->Blueprint.Get()==BP;
        const bool HandleResolved=SameBlueprint&&BP->SimpleConstructionScript.IsResolved();
        const bool CurrentAbsent=HandleResolved&&BP->SimpleConstructionScript==nullptr;
        auto ExpectedPresence=MakeShared<FJsonObject>();ExpectedPresence->SetBoolField(TEXT("same_blueprint"),true);ExpectedPresence->SetBoolField(TEXT("handle_resolved"),true);ExpectedPresence->SetBoolField(TEXT("scs_absent"),true);
        auto ActualPresence=MakeShared<FJsonObject>();ActualPresence->SetBoolField(TEXT("same_blueprint"),SameBlueprint);ActualPresence->SetBoolField(TEXT("handle_resolved"),HandleResolved);ActualPresence->SetBoolField(TEXT("scs_absent"),CurrentAbsent);
        Out->SetObjectField(TEXT("expected_presence"),ExpectedPresence);Out->SetObjectField(TEXT("actual_presence"),ActualPresence);
        if(!SameBlueprint||!CurrentAbsent)
        {
            if(!EnumScope->Reply->Data->HasField(TEXT("local_scs_first_mismatch")))
            {
                auto Mismatch=MakeShared<FJsonObject>();Mismatch->SetStringField(TEXT("stage"),TEXT("local_scs_observation"));Mismatch->SetStringField(TEXT("object"),EnumScope->BlueprintPath);Mismatch->SetStringField(TEXT("property"),TEXT("SimpleConstructionScript"));Mismatch->SetObjectField(TEXT("expected"),ExpectedPresence);Mismatch->SetObjectField(TEXT("actual"),ActualPresence);EnumScope->Reply->Data->SetObjectField(TEXT("local_scs_first_mismatch"),Mismatch);
            }
            return EnumScope->Bad(TEXT("local_map_library_scs_changed"),TEXT("Exact PresetLoader local scope requires its current resolved-null construction script; no SCS lookup or declaration followed."));
        }
        Out->SetObjectField(TEXT("identity"),LifecycleIdentity(nullptr));Out->SetArrayField(TEXT("roots"),{});Out->SetArrayField(TEXT("all_nodes"),{});Out->SetObjectField(TEXT("nodes"),MakeShared<FJsonObject>());Out->SetStringField(TEXT("default_scene_root"),TEXT(""));
        Out->SetStringField(TEXT("presence"),TEXT("absent"));Out->SetStringField(TEXT("scope"),TEXT("exact member0 PresetLoader function-library local observation"));Out->SetBoolField(TEXT("scs_get_attempted"),false);
        if(!EnumScope->Admit())return false;
        Out->SetBoolField(TEXT("absent_scs_current_verified"),true);Out->SetBoolField(TEXT("complete"),true);return true;
    }
    Out->SetBoolField(TEXT("complete"), false); const USimpleConstructionScript* SCS = BP->SimpleConstructionScript.Get();
    Out->SetObjectField(TEXT("identity"), LifecycleIdentity(SCS)); Out->SetArrayField(TEXT("roots"), {}); Out->SetArrayField(TEXT("all_nodes"), {}); auto Rows = MakeShared<FJsonObject>(); Out->SetObjectField(TEXT("nodes"), Rows);
    if (!IsValid(SCS) || SCS->GetOutermost() != BP->GetOutermost() || SCS->GetAllNodes().Num() > 128) { return false; }
    TArray<FString> Roots; for (const USCS_Node* Node : SCS->GetRootNodes()) { if (!IsValid(Node) || !SCS->GetAllNodes().Contains(Node)) { return false; } Roots.Add(Node->GetPathName()); Out->SetArrayField(TEXT("roots"), StringValues(Roots)); }
    const USCS_Node* Default = SCS->GetDefaultSceneRootNode(); Out->SetStringField(TEXT("default_scene_root"), IsValid(Default) ? Default->GetPathName() : FString());
    if (Default && !IsValid(Default)) { return false; }
    TArray<const USCS_Node*> Observed; TArray<FString> All;
    for (const USCS_Node* Node : SCS->GetAllNodes()) { if (!IsValid(Node)) { return false; } Observed.Add(Node); All.Add(Node->GetPathName()); Out->SetArrayField(TEXT("all_nodes"), StringValues(All)); }
    if (Default) { Observed.AddUnique(Default); }
    int32 Fields = 0, Chars = 0;
    for (const USCS_Node* Node : Observed)
    {
        if (!IsValid(Node) || Node->GetOutermost() != BP->GetOutermost()) { return false; }
        auto J = MakeShared<FJsonObject>(); Rows->SetObjectField(Node->GetPathName(), J); J->SetObjectField(TEXT("identity"), LifecycleIdentity(Node));
        J->SetStringField(TEXT("variable_name"), Node->GetVariableName().ToString()); J->SetStringField(TEXT("variable_guid"), Node->VariableGuid.ToString(EGuidFormats::Digits));
        J->SetStringField(TEXT("parent_name"), Node->ParentComponentOrVariableName.ToString()); J->SetStringField(TEXT("parent_owner"), Node->ParentComponentOwnerClassName.ToString()); J->SetBoolField(TEXT("parent_native"), Node->bIsParentComponentNative);
        J->SetArrayField(TEXT("children"), {}); TArray<FString> Children;
        for (const USCS_Node* Child : Node->GetChildNodes()) { if (!IsValid(Child) || !SCS->GetAllNodes().Contains(Child)) { return false; } Children.Add(Child->GetPathName()); J->SetArrayField(TEXT("children"), StringValues(Children)); }
        UActorComponent* Template = Node->ComponentTemplate.Get(); UClass* Class = Node->ComponentClass.Get();
        J->SetObjectField(TEXT("component_class"), LifecycleIdentity(Class)); J->SetObjectField(TEXT("component_template"), LifecycleIdentity(Template)); auto Values = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("template_fields"), Values);
        if (!IsValid(Class) || !Class->HasAnyClassFlags(CLASS_Native) || Class->GetOutermost()->GetName() != TEXT("/Script/Engine") || !IsValid(Template) || Template->GetClass() != Class || Template->GetOutermost() != BP->GetOutermost() || !Template->IsTemplate() || Template->IsRegistered() || Template->IsPreRegistering()) { return false; }
        if (!LifecycleFields(Template, Values, EnumScope, Fence)) { return false; }
        for (const auto& Value : Values->Values) { if (++Fields > 8192 || (Chars += Value.Value->AsObject()->GetStringField(TEXT("export_text")).Len()) > 1048576) { return false; } }
    }
    Out->SetBoolField(TEXT("complete"), true); return true;
}
bool RemovalGenerated(UBlueprint* BP, FName Name, const TSharedRef<FJsonObject>& Out, TFunction<bool()> Fence = {})
{
    Out->SetBoolField(TEXT("complete"), false); Out->SetBoolField(TEXT("property_exists"), false); Out->SetBoolField(TEXT("value_available"), false);
    Out->SetStringField(TEXT("selected_name"), Name.ToString()); Out->SetBoolField(TEXT("current_compile"), BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings);
    Out->SetStringField(TEXT("scope"), TEXT("Actual existing current generated class/CDO only, without creation. Descriptor removal can leave a stale compiled property; no runtime/cold layout absence is inferred."));
    UClass* Class = BP->GeneratedClass.Get(); UObject* CDO = IsValid(Class) ? Class->GetDefaultObject(false) : nullptr;
    Out->SetObjectField(TEXT("class_identity"), LifecycleIdentity(Class)); Out->SetObjectField(TEXT("cdo_identity"), LifecycleIdentity(CDO));
    if (!IsValid(Class) || !IsValid(CDO) || CDO->GetClass() != Class) { return false; }
    FProperty* Property = FindFProperty<FProperty>(Class, Name); Out->SetBoolField(TEXT("property_exists"), Property != nullptr);
    if (Property)
    {
        auto J = MakeShared<FJsonObject>(); Out->SetObjectField(TEXT("property"), J); J->SetStringField(TEXT("name"), Property->GetName()); J->SetStringField(TEXT("owner"), Property->GetOwnerStruct()->GetPathName());
        J->SetStringField(TEXT("property_class"), Property->GetClass()->GetName()); J->SetStringField(TEXT("cpp_type"), Property->GetCPPType()); J->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
        if (Property->GetOwnerStruct() != Class || Property->ArrayDim != 1) { return false; }
        const void* Address = Property->ContainerPtrToValuePtr<void>(CDO); const auto Value = ReadTyped(Property, Address); FString Text;
        if(Fence&&!Fence())return false;
        if(!Value.IsValid())return false;const bool Exported=Property->ExportText_Direct(Text,Address,Address,CDO,PPF_None);
        if(Fence&&!Fence())return false;
        if(!Exported||!SafeText(Text,65536))return false;
        J->SetField(TEXT("value"), Value); J->SetStringField(TEXT("export_text"), Text); Out->SetBoolField(TEXT("value_available"), true);
    }
    Out->SetBoolField(TEXT("complete"), true); return true;
}
bool RemovalCapture(UBlueprint* BP, FName Name, FLifecycleObservation& O, FResult& R, const TCHAR* Phase, FRemovalLinkedPlan* Linked=nullptr)
{
    if(Linked&&(!Linked->Plain()))return false;
    const bool Base = LifecycleCapture(BP, O, R, Phase, nullptr, Linked?[Linked](){return Linked->Plain();}:TFunction<bool()>(), Linked?[Linked](const TSharedRef<FJsonObject>& Graphs){return Linked->Snapshot(Graphs);}:TFunction<bool(const TSharedRef<FJsonObject>&)>(), Linked?[Linked](const TSharedRef<FJsonObject>& Refs){return Linked->References(Refs);}:TFunction<bool(const TSharedRef<FJsonObject>&)>()); const auto J = R.Data->GetObjectField(Phase); J->SetBoolField(TEXT("complete"), false); O.Complete = false;
    auto SCS = MakeShared<FJsonObject>(), Generated = MakeShared<FJsonObject>(), Replication = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("scs"), SCS); O.State->SetObjectField(TEXT("selected_generated_property"), Generated); O.State->SetObjectField(TEXT("descriptor_replication"), Replication);
    if (!Base) { return false; }
    J->SetStringField(TEXT("failed_stage"), TEXT("descriptor_replication"));
    for (const FBPVariableDescription& Variable : BP->NewVariables)
    {
        auto Row = MakeShared<FJsonObject>(); Replication->SetObjectField(Variable.VarName.ToString(), Row); Row->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("rep_notify"), Variable.RepNotifyFunc.ToString()); Row->SetNumberField(TEXT("replication_condition"), static_cast<int32>(Variable.ReplicationCondition));
    }
    J->SetStringField(TEXT("failed_stage"), TEXT("scs")); LifecycleCount(R, TEXT("removal_scs_observation_calls")); if (!RemovalSCS(BP, SCS, nullptr, Linked?[Linked](){return Linked->Plain();}:TFunction<bool()>())) { return false; }
    J->SetStringField(TEXT("failed_stage"), TEXT("selected_generated_property")); LifecycleCount(R, TEXT("selected_generated_observation_calls")); if (!RemovalGenerated(BP, Name, Generated, Linked?[Linked](){return Linked->Plain();}:TFunction<bool()>())) { return false; }
    if(Linked&&!Linked->Plain())return false;
    O.Complete = true; J->SetBoolField(TEXT("complete"), true); J->SetStringField(TEXT("failed_stage"), TEXT("")); return true;
}
bool RemovalFieldsSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, FName Name)
{
    auto X = MakeShared<FJsonObject>(), Y = MakeShared<FJsonObject>();
    for (const auto& P : A->Values) { if (FName(*FString(P.Key.ToView())) != Name) { X->SetField(FString(P.Key.ToView()), P.Value); } }
    for (const auto& P : B->Values) { if (FName(*FString(P.Key.ToView())) != Name) { Y->SetField(FString(P.Key.ToView()), P.Value); } }
    return LifecycleEqual(X, Y);
}
bool RemovalGeneratedSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{
    if (!A->GetBoolField(TEXT("complete")) || !B->GetBoolField(TEXT("complete")) || !A->GetBoolField(TEXT("property_exists")) || !A->GetBoolField(TEXT("value_available"))
        || !LifecycleEqual(A->GetObjectField(TEXT("class_identity")).ToSharedRef(), B->GetObjectField(TEXT("class_identity")).ToSharedRef())
        || !LifecycleEqual(A->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), B->GetObjectField(TEXT("cdo_identity")).ToSharedRef())) { return false; }
    return !B->GetBoolField(TEXT("property_exists")) ? !B->GetBoolField(TEXT("value_available")) : B->GetBoolField(TEXT("value_available")) && LifecycleEqual(A->GetObjectField(TEXT("property")).ToSharedRef(), B->GetObjectField(TEXT("property")).ToSharedRef());
}
bool RemovalPreserved(const FLifecycleObservation& Before, const FLifecycleObservation& After, FName Name, FResult& R, FRemovalLinkedPlan* Linked=nullptr)
{
    const auto A = Before.State->GetObjectField(TEXT("variables")), B = After.State->GetObjectField(TEXT("variables")); TArray<TSharedPtr<FJsonValue>> Survivors; TArray<FName> Order;
    int32 Selected = 0; for (const auto& Row : A->GetArrayField(TEXT("new_variables"))) { const FName N(*Row->AsObject()->GetStringField(TEXT("name"))); if (N == Name) { ++Selected; } else { Survivors.Add(Row); Order.Add(N); } }
    bool Absent = true; for (const auto& Row : B->GetArrayField(TEXT("new_variables"))) { Absent &= FName(*Row->AsObject()->GetStringField(TEXT("name"))) != Name; }
    const bool Generated = RemovalGeneratedSame(Before.State->GetObjectField(TEXT("selected_generated_property")).ToSharedRef(), After.State->GetObjectField(TEXT("selected_generated_property")).ToSharedRef());
    const bool SCS = LifecycleEqual(Before.State->GetObjectField(TEXT("scs")).ToSharedRef(), After.State->GetObjectField(TEXT("scs")).ToSharedRef());
    const bool State = Before.Complete && After.Complete && Selected == 1 && Absent && B->GetNumberField(TEXT("new_variable_count")) == A->GetNumberField(TEXT("new_variable_count")) - 1
        && LifecycleRowsSame(Survivors, B->GetArrayField(TEXT("new_variables")), Order, nullptr, false)
        && LifecycleEqual(A->TryGetField(TEXT("generated_variables")), B->TryGetField(TEXT("generated_variables"))) && LifecycleEqual(A->TryGetField(TEXT("category_sorting")), B->TryGetField(TEXT("category_sorting")))
        && A->GetStringField(TEXT("generated_cdo_path")) == B->GetStringField(TEXT("generated_cdo_path")) && Generated && SCS
        && RemovalFieldsSame(Before.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), Name)
        && RemovalFieldsSame(Before.State->GetObjectField(TEXT("descriptor_replication")).ToSharedRef(), After.State->GetObjectField(TEXT("descriptor_replication")).ToSharedRef(), Name)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), false, false)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), false, false)
        && LifecycleGraphSame(Linked?Linked->ExpectedGraphs(Before.State->GetObjectField(TEXT("graphs")).ToSharedRef()):Before.State->GetObjectField(TEXT("graphs")).ToSharedRef(), After.State->GetObjectField(TEXT("graphs")).ToSharedRef(), false);
    const bool Source = LifecycleEqual(Before.Source, After.Source), Refs = LifecycleEqual(Linked?Linked->ExpectedReferences(Before.References):Before.References, After.References), Other = LifecycleEqual(Before.Other, After.Other), Disk = LifecycleEqual(Before.Disk, After.Disk);
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("declaration_removed"), Selected == 1 && Absent);
    R.Data->SetBoolField(TEXT("selected_current_generated_property_preserved_or_absent"), Generated); R.Data->SetBoolField(TEXT("scs_preserved"), SCS); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), State);
    R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), Disk);
    return State && Source && Refs && Other && Disk;
}
}
FResult Remove(const FString& Path, const FString& Name)
{
    FResult R; LifecycleInit(R); for (const TCHAR* K : { TEXT("remove_member_variable_calls"), TEXT("remove_member_variable_return_count"), TEXT("caller_mark_structural_return_count"), TEXT("removal_scs_observation_calls"), TEXT("selected_generated_observation_calls"), TEXT("source_expected_implicit_structural_requests") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetStringField(TEXT("blueprint_name"), Path); R.Data->SetStringField(TEXT("variable_name"), Name); R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetBoolField(TEXT("saved"), false);
    R.Data->SetBoolField(TEXT("declaration_removed"), false); R.Data->SetBoolField(TEXT("scs_preserved"), false); R.Data->SetBoolField(TEXT("selected_current_generated_property_preserved_or_absent"), false);
    R.Data->SetStringField(TEXT("removal_scope"), TEXT("Single simple local nativeActor member, including original descriptor/Get-Set retirement, replication/RepNotify/condition associations and FieldNotify metadata without an implementing interface. Actual interface-owned FieldNotify cascade, inherited/generated/function-local/complex/delegate variants and external consumers remain incomplete. No full compile/save or runtime/cold layout absence is inferred."));
    if (!SafeName(Name)) { Fail(R, TEXT("variable_name_invalid"), TEXT("Exact bounded safe variable name required.")); return R; }
    if (RemovalDependencyReference(Name)) { Fail(R, TEXT("variable_reference_unsupported"), TEXT("Original variable_name dependency-reference guard rejected this selector; no Blueprint load or removal.")); return R; }
    TStrongObjectPtr<UBlueprint> Blueprint(LoadBlueprint(Path, R)); UBlueprint* BP = Blueprint.Get(); if (!BP || !LifecycleSimple(BP, R)) { return R; }
    int32 Index = INDEX_NONE; FBPVariableDescription* Variable = FindLocal(BP, Name, Index, R); if (!Variable) { if (R.ErrorCode == TEXT("variable_not_found")) { R.Error = TEXT("Blueprint variable not found: ") + Name; } return R; }
    const FBPVariableDescription Selected = *Variable; R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName()); R.Data->SetStringField(TEXT("stored_variable_name"), Selected.VarName.ToString()); R.Data->SetStringField(TEXT("selected_variable_guid"), Selected.VarGuid.ToString(EGuidFormats::Digits));
    if (!RemovalAdmission(BP, Selected, R)) { return R; }
    const bool ReplicationRemoval=R.Data->GetObjectField(TEXT("removal_admission"))->GetBoolField(TEXT("selected_replication"));
    FRemovalLinkedPlan LinkPlan(R);if((ReplicationRemoval||Selected.HasMetaData(TEXT("FieldNotify"))||!R.Data->GetObjectField(TEXT("removal_admission"))->GetArrayField(TEXT("selected_variable_nodes")).IsEmpty())&&!LinkPlan.Build(BP,Selected.VarName,ReplicationRemoval||Selected.HasMetaData(TEXT("FieldNotify"))))return R;
    FRemovalLinkedPlan* Linked=LinkPlan.Active?&LinkPlan:nullptr;
    if(Linked){R.Data->SetStringField(TEXT("removal_scope"),R.Data->GetStringField(TEXT("linked_removal_scope")));R.Data->GetObjectField(TEXT("removal_admission"))->SetBoolField(TEXT("unused_local_scalar"),false);}
    FLifecycleObservation Before, After; R.Stage = TEXT("before_observation"); if (!RemovalCapture(BP, Selected.VarName, Before, R, TEXT("before_observation"), Linked)) { R.bChangeStateKnown = false; if(R.ErrorCode.IsEmpty())Fail(R, TEXT("variable_remove_before_unobservable"), TEXT("Before actual prefix is incomplete; no removal.")); return R; }
    auto ReplicationBefore=MakeShared<FJsonObject>(),ReplicationAfter=MakeShared<FJsonObject>();
    if(ReplicationRemoval){R.Data->SetObjectField(TEXT("replication_before"),ReplicationBefore);R.Data->SetStringField(TEXT("replication_removal_scope"),TEXT("Original descriptor/Get-Set retirement plus preservation of surviving replication associations and existing RepNotify functions; structural-only generated/skeleton observations. ClassReps/RepIndex cache regeneration, full compile/runtime/cold net layout absence are not inferred. Interface-owned FieldNotify cascade remains incomplete."));if(!RemovalReplicationState(BP,Selected,LinkPlan,ReplicationBefore))return R;}
    Variable = FindLocal(BP, Name, Index, R);
    if(Linked)
    {
        if(!Linked->Plain())return R;if(!Variable){Linked->Bad(TEXT("Selected descriptor was lost before deletion."));return R;}
        const FBPVariableDescription Current=*Variable;
        const bool Same=Current.VarGuid==Selected.VarGuid&&Current.VarType==Selected.VarType&&Current.DefaultValue.Equals(Selected.DefaultValue,ESearchCase::CaseSensitive)
            &&Current.PropertyFlags==Selected.PropertyFlags&&Current.FriendlyName.Equals(Selected.FriendlyName,ESearchCase::CaseSensitive)
            &&Current.Category.IdenticalTo(Selected.Category)&&Current.RepNotifyFunc==Selected.RepNotifyFunc&&Current.ReplicationCondition==Selected.ReplicationCondition
            &&LifecycleEqual(Metadata(Current),Metadata(Selected));
        if(!Linked->Plain())return R;
        if(!Same){if(R.ErrorCode.IsEmpty())Fail(R,TEXT("variable_remove_member_changed"),TEXT("Selected copied descriptor changed before deletion; no writer."));return R;}
    }
    else
    {
    if (!Variable || Variable->VarGuid != Selected.VarGuid || Variable->VarType != Selected.VarType || Variable->DefaultValue != Selected.DefaultValue || Variable->PropertyFlags != Selected.PropertyFlags || Variable->FriendlyName != Selected.FriendlyName || !Variable->Category.IdenticalTo(Selected.Category) || Variable->RepNotifyFunc != Selected.RepNotifyFunc || Variable->ReplicationCondition != Selected.ReplicationCondition || !LifecycleEqual(Metadata(*Variable), Metadata(Selected)))
    { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("variable_remove_member_changed"), TEXT("Selected member changed during read admission; no removal.")); } return R; }
    }
    R.Data->SetObjectField(TEXT("removed_descriptor_before"),Row(BP,Selected,TEXT("new"),Index,Linked?[Linked](){return Linked->Plain();}:TFunction<bool()>()));
    if(Linked&&!Linked->Plain())return R;
    LifecycleCount(R, TEXT("outer_transaction_calls")); const FScopedTransaction Transaction(FText::FromString(TEXT("Remove Blueprint variable")));
    R.Stage = TEXT("remove_member_variable"); R.bMutationAttempted = true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("remove_member_variable_calls"));
    if(Linked&&!Linked->Plain())return R;
    FBlueprintEditorUtils::RemoveMemberVariable(BP, Selected.VarName); LifecycleCount(R, TEXT("remove_member_variable_return_count")); R.Data->SetNumberField(TEXT("source_expected_implicit_structural_requests"), 1);
    if(Linked&&!Linked->StructuralReturn())return R;
    R.Stage = TEXT("mark_structural"); LifecycleCount(R, TEXT("explicit_mark_structural_calls")); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); LifecycleCount(R, TEXT("caller_mark_structural_return_count")); R.Data->SetNumberField(TEXT("source_expected_implicit_structural_requests"), 2);
    if(Linked&&!Linked->StructuralReturn())return R;
    R.Stage = TEXT("after_observation"); if (!RemovalCapture(BP, Selected.VarName, After, R, TEXT("after_observation"), Linked)) { R.bChangeStateKnown = false; if(R.ErrorCode.IsEmpty())Fail(R, TEXT("variable_remove_after_unobservable"), TEXT("Returned removal/structural prefix retained; bounded final read incomplete, no compile/save/retry/rollback.")); return R; }
    R.bChangeStateKnown = true; R.bChanged = !LifecycleEqual(Before.State, After.State) || !LifecycleEqual(Before.Source, After.Source) || !LifecycleEqual(Before.References, After.References) || !LifecycleEqual(Before.Other, After.Other) || !LifecycleEqual(Before.Disk, After.Disk);
    const auto Generated = After.State->GetObjectField(TEXT("selected_generated_property")); R.Data->SetBoolField(TEXT("selected_generated_property_retained"), Generated->GetBoolField(TEXT("property_exists")));
    R.Data->SetBoolField(TEXT("current_compiled_layout_absence_observed"), !Generated->GetBoolField(TEXT("property_exists"))); R.Data->SetBoolField(TEXT("runtime_or_cold_layout_verified"), false);
    if(ReplicationRemoval){R.Data->SetObjectField(TEXT("replication_after"),ReplicationAfter);if(!RemovalReplicationState(BP,Selected,LinkPlan,ReplicationAfter))return R;if(!RemovalReplicationPreserved(ReplicationBefore,ReplicationAfter,Selected.VarName,R)){Fail(R,TEXT("variable_remove_replication_preservation_failed"),TEXT("Actual surviving replication associations or existing RepNotify function changed; removal prefix retained, no compile/save/retry."));return R;}}
    if (!RemovalPreserved(Before, After, Selected.VarName, R, Linked)) { Fail(R, TEXT("variable_remove_preservation_failed"), TEXT("Actual declaration/remaining current fields or declared source/graph/SCS/disk/other-dirty scope differs; writer prefix retained, no compile/save/rollback.")); return R; }
    if(Linked){if(!Linked->Plain())return R;R.Data->SetBoolField(TEXT("selected_get_set_membership_removed"),true);R.Data->SetBoolField(TEXT("incident_links_removed"),true);R.Data->SetBoolField(TEXT("nonselected_graph_state_preserved"),true);R.Data->SetBoolField(TEXT("planned_serialized_references_preserved"),true);}
    R.Data->SetStringField(TEXT("message"), TEXT("Removed Blueprint variable: ") + Name); R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}

// CB: original category display ordering; existing variable/removal bodies remain unchanged.
namespace Cubeless::UE58::Variable
{
namespace
{
TArray<TSharedPtr<FJsonValue>> CategoryNames(const TArray<FName>& Names)
{ TArray<TSharedPtr<FJsonValue>> Rows; for (FName Name : Names) { Rows.Add(MakeShared<FJsonValueString>(Name.ToString())); } return Rows; }
bool CategoryMatches(const TArray<TSharedPtr<FJsonValue>>& Actual, const TArray<FName>& Desired)
{
    if (Actual.Num() != Desired.Num()) { return false; }
    for (int32 I = 0; I < Actual.Num(); ++I) { if (!Actual[I].IsValid() || Actual[I]->Type != EJson::String || FName(*Actual[I]->AsString()) != Desired[I]) { return false; } }
    return true;
}
bool CategoryCapture(UBlueprint* BP, FLifecycleObservation& O, FResult& R, const TCHAR* Phase,FMetadataTypedScope* Typed=nullptr)
{
    const bool Base=Typed?Typed->CapturePhase(O,Phase):LifecycleCapture(BP,O,R,Phase);const TSharedPtr<FJsonObject>* PhaseObject=nullptr;
    if(!R.Data->TryGetObjectField(Phase,PhaseObject)||!PhaseObject||!PhaseObject->IsValid()){O.Complete=false;return false;}
    const auto J=*PhaseObject;J->SetBoolField(TEXT("complete"),false);O.Complete=false;
    auto SCS = MakeShared<FJsonObject>(), Replication = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("scs"), SCS); O.State->SetObjectField(TEXT("descriptor_replication"), Replication);
    if (!Base) { return false; }
    J->SetStringField(TEXT("failed_stage"), TEXT("descriptor_replication"));
    for (const FBPVariableDescription& Variable : BP->NewVariables)
    {
        auto Row = MakeShared<FJsonObject>(); Replication->SetObjectField(Variable.VarName.ToString(), Row); Row->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::Digits));
        Row->SetStringField(TEXT("rep_notify"), Variable.RepNotifyFunc.ToString()); Row->SetNumberField(TEXT("replication_condition"), static_cast<int32>(Variable.ReplicationCondition));
    }
    J->SetStringField(TEXT("failed_stage"),TEXT("scs"));LifecycleCount(R,TEXT("category_scs_observation_calls"));
    if(Typed&&Typed->NativeObject)
    {
        if(!Typed->Current()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=nullptr)return false;
        SCS->SetObjectField(TEXT("identity"),LifecycleIdentity(nullptr));SCS->SetArrayField(TEXT("roots"),{});SCS->SetArrayField(TEXT("all_nodes"),{});SCS->SetObjectField(TEXT("nodes"),MakeShared<FJsonObject>());SCS->SetStringField(TEXT("default_scene_root"),TEXT(""));SCS->SetBoolField(TEXT("complete"),true);SCS->SetStringField(TEXT("presence"),TEXT("exact_direct_UObject_absent"));
        if(!Typed->Current()||!BP->SimpleConstructionScript.IsResolved()||BP->SimpleConstructionScript.Get()!=nullptr)return false;
    }
    else if(!RemovalSCS(BP,SCS,nullptr,Typed?TFunction<bool()>([&](){return Typed->Current();}):TFunction<bool()>{}))return false;
    if(Typed&&!Typed->Current())return false;
    O.Complete = true; J->SetBoolField(TEXT("complete"), true); J->SetStringField(TEXT("failed_stage"), TEXT("")); return true;
}
bool CategorySCSSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, bool Saved)
{
    // ABO: CategoryCapture proved resolved-null SCS for an exact direct UObject.
    // Its null identity has empty flags, not a decimal uint64 or saved export.
    // Require the complete canonical absence DTO on both sides; valid Actor SCS
    // identities still use the existing object-flag/save transition verifier.
    FString BeforePresence, AfterPresence;
    if (A->TryGetStringField(TEXT("presence"), BeforePresence) && BeforePresence == TEXT("exact_direct_UObject_absent")
        && B->TryGetStringField(TEXT("presence"), AfterPresence) && AfterPresence == BeforePresence)
    {
        auto Absent = MakeShared<FJsonObject>(); Absent->SetObjectField(TEXT("identity"), LifecycleIdentity(nullptr));
        Absent->SetArrayField(TEXT("roots"), {}); Absent->SetArrayField(TEXT("all_nodes"), {}); Absent->SetObjectField(TEXT("nodes"), MakeShared<FJsonObject>());
        Absent->SetStringField(TEXT("default_scene_root"), TEXT("")); Absent->SetBoolField(TEXT("complete"), true); Absent->SetStringField(TEXT("presence"), BeforePresence);
        return LifecycleEqual(A, Absent) && LifecycleEqual(B, Absent);
    }
    if (A->Values.Num() != B->Values.Num() || !LifecycleIdentitySame(A->GetObjectField(TEXT("identity")).ToSharedRef(), B->GetObjectField(TEXT("identity")).ToSharedRef(), Saved, false)) { return false; }
    for (const auto& P : A->Values) { if (P.Key.ToView() != TEXT("identity") && P.Key.ToView() != TEXT("nodes") && !LifecycleEqual(P.Value, B->TryGetField(P.Key.ToView()))) { return false; } }
    const auto X = A->GetObjectField(TEXT("nodes")), Y = B->GetObjectField(TEXT("nodes")); if (X->Values.Num() != Y->Values.Num()) { return false; }
    for (const auto& P : X->Values)
    {
        const auto Q = Y->TryGetField(P.Key.ToView()); if (!Q || Q->Type != EJson::Object || P.Value->Type != EJson::Object) { return false; }
        const auto M = P.Value->AsObject(), N = Q->AsObject(); if (M->Values.Num() != N->Values.Num()) { return false; }
        for (const auto& Field : M->Values)
        {
            const auto Other = N->TryGetField(Field.Key.ToView()); if (!Other || Field.Value->Type != Other->Type) { return false; }
            if (Field.Key.ToView() == TEXT("identity") || Field.Key.ToView() == TEXT("component_template"))
            { if (!LifecycleIdentitySame(Field.Value->AsObject().ToSharedRef(), Other->AsObject().ToSharedRef(), Saved, false)) { return false; } }
            else if (!LifecycleEqual(Field.Value, Other)) { return false; }
        }
    }
    return true;
}
bool CategoryPreserved(const FLifecycleObservation& Before, const FLifecycleObservation& After, const TArray<FName>& Desired, bool FullCompiled, bool Saved, bool DiskRequired, FResult& R)
{
    const auto A = Before.State->GetObjectField(TEXT("variables")), B = After.State->GetObjectField(TEXT("variables")); TArray<FName> Order;
    for (const auto& Row : A->GetArrayField(TEXT("new_variables"))) { Order.Add(FName(*Row->AsObject()->GetStringField(TEXT("name")))); }
    const bool Categories = CategoryMatches(B->GetArrayField(TEXT("category_sorting")), Desired);
    const bool SCS = CategorySCSSame(Before.State->GetObjectField(TEXT("scs")).ToSharedRef(), After.State->GetObjectField(TEXT("scs")).ToSharedRef(), Saved);
    bool State = Before.Complete && After.Complete && Categories && SCS
        && LifecycleRowsSame(A->GetArrayField(TEXT("new_variables")), B->GetArrayField(TEXT("new_variables")), Order, nullptr, FullCompiled)
        && LifecycleEqual(A->TryGetField(TEXT("generated_variables")), B->TryGetField(TEXT("generated_variables")))
        && A->GetNumberField(TEXT("new_variable_count")) == B->GetNumberField(TEXT("new_variable_count")) && A->GetNumberField(TEXT("generated_variable_count")) == B->GetNumberField(TEXT("generated_variable_count"))
        && LifecycleEqual(Before.State->GetObjectField(TEXT("descriptor_replication")).ToSharedRef(), After.State->GetObjectField(TEXT("descriptor_replication")).ToSharedRef())
        && A->GetStringField(TEXT("generated_cdo_path")) == B->GetStringField(TEXT("generated_cdo_path"))
        && LifecycleCdoFieldsSame(Before.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), nullptr)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), Saved, false)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), Saved, FullCompiled)
        && LifecycleGraphSame(Before.State->GetObjectField(TEXT("graphs")).ToSharedRef(), After.State->GetObjectField(TEXT("graphs")).ToSharedRef(), Saved);
    const bool Source = Before.Complete && After.Complete && LifecycleEqual(Before.Source, After.Source), Refs = Before.Complete && After.Complete && LifecycleEqual(Before.References, After.References);
    const bool Other = Before.Complete && After.Complete && LifecycleEqual(Before.Other, After.Other), Disk = Before.Complete && After.Complete && (!DiskRequired || LifecycleEqual(Before.Disk, After.Disk));
    if (Saved) { State &= !B->GetBoolField(TEXT("package_dirty")) && After.Disk->GetBoolField(TEXT("exists")) && After.Disk->GetNumberField(TEXT("bytes")) > 0 && Before.Disk->GetStringField(TEXT("path")) == After.Disk->GetStringField(TEXT("path")); }
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("category_order_matches_current_by_fname"), Categories); R.Data->SetBoolField(TEXT("scs_preserved"), SCS);
    R.Data->SetBoolField(TEXT("nonselected_state_preserved"), State); R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other);
    R.Data->SetBoolField(Saved ? TEXT("save_state_preserved") : TEXT("disk_preserved_before_save"), Saved ? State && Source && Refs && Other : Disk);
    return State && Source && Refs && Other && Disk;
}
void CategoryAggregateClear(FResult& R)
{
    LifecycleAggregateClear(R); R.Data->SetBoolField(TEXT("category_order_matches_current_by_fname"), false); R.Data->SetBoolField(TEXT("scs_preserved"), false);
}
bool CategoryObserved(UBlueprint* BP, FLifecycleObservation& O, const FLifecycleObservation& Before, FResult& R, const TCHAR* Phase,FMetadataTypedScope* Typed=nullptr)
{
    if (!CategoryCapture(BP,O,R,Phase,Typed)) { R.bChangeStateKnown = false; return Fail(R, TEXT("category_sorting_observation_incomplete"), TEXT("Bounded current phase prefix retained; no replay or rollback after an unreadable observation.")); }
    R.bChangeStateKnown = true; R.bChanged = !LifecycleEqual(Before.State, O.State) || !LifecycleEqual(Before.Source, O.Source) || !LifecycleEqual(Before.References, O.References) || !LifecycleEqual(Before.Other, O.Other) || !LifecycleEqual(Before.Disk, O.Disk);
    R.Data->SetArrayField(TEXT("actual_category_order"), O.State->GetObjectField(TEXT("variables"))->GetArrayField(TEXT("category_sorting"))); R.Data->SetStringField(TEXT("actual_category_order_scope"), Phase);
    R.Data->SetNumberField(TEXT("current_blueprint_status"), O.State->GetObjectField(TEXT("variables"))->GetNumberField(TEXT("blueprint_status"))); R.Data->SetStringField(TEXT("current_blueprint_status_scope"), Phase); return true;
}
}
FResult SetCategorySorting(const FString& Path, const FString& OrderJson, const FString& Front, bool Append, bool Save, bool Compile)
{
    FResult R; LifecycleInit(R); R.bCompileRequested = Compile; R.bSaveRequested = Save; R.Data->SetNumberField(TEXT("category_scs_observation_calls"), 0);
    R.Data->SetStringField(TEXT("blueprint_name"), Path); R.Data->SetStringField(TEXT("requested_category_order_json"), OrderJson); R.Data->SetStringField(TEXT("category_to_front"), Front);
    R.Data->SetBoolField(TEXT("append_existing"), Append); R.Data->SetBoolField(TEXT("caller_compile_requested"), Compile); R.Data->SetBoolField(TEXT("caller_save_requested"), Save);
    R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetStringField(TEXT("compile_status"), TEXT("not_requested")); R.Data->SetStringField(TEXT("compile_status_scope"), TEXT("no_explicit_compile_returned"));
    R.Data->SetBoolField(TEXT("legacy_compiled_means_dispatch_only"), true); R.Data->SetBoolField(TEXT("category_order_matches_current_by_fname"), false); R.Data->SetBoolField(TEXT("scs_preserved"), false);
    R.Data->SetStringField(TEXT("category_sorting_scope"), TEXT("Exact nativeActor/current simple local member observation scope. Editor-only category display order is independent of declaration order. Broader original generated/inherited/complex/Blueprint owners and internal RPC category alias remain incomplete. No runtime/cooked effect is inferred."));
    R.Data->SetStringField(TEXT("typed_compile_scope"), TEXT("Actual returned explicit synchronous full-compile receipt, distinct from legacy dispatch bool; current final Blueprint status is separately retained. No exhaustive implicit PreSave/helper compile accounting or cold persistence proof."));
    TSharedPtr<FJsonValue> Requested; if (!Parse(OrderJson, Requested, R) || !(Requested->Type == EJson::Null || Requested->Type == EJson::Array) || !SafeName(Front, true))
    { if (R.ErrorCode.IsEmpty()) { Fail(R, TEXT("category_sorting_input_invalid"), TEXT("Bounded JSON null/string-array and optional bounded category_to_front string required.")); } return R; }
    TStrongObjectPtr<UBlueprint> Blueprint(LoadBlueprint(Path,R));UBlueprint* BP=Blueprint.Get();if(!BP)return R;
    TUniquePtr<FMetadataTypedScope> Typed;if(UsesTypedOrdering(BP)){Typed=MakeUnique<FMetadataTypedScope>(R,FMetadataTypedScope::EUse::CategoryOrder);if(!Typed->Bind(BP))return R;R.Data->SetStringField(TEXT("category_sorting_scope"),TEXT("Direct native Actor/UObject and current own primitive scalar/primitive Array<=256/native3 scalar values; same original category normalization/append/no-op dirty-save behavior. Actual optional compile/save uses guarded returned class roles; no declaration order/value/type change or broad source/runtime claim; usage validation pending."));}else if(!LifecycleSimple(BP,R))return R;
    auto Current=[&](){return !Typed||Typed->Current();};auto SDKReturned=[&](){return !Typed||Typed->Rebind();};
    R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName()); const TArray<FName> Previous = BP->CategorySorting; TArray<FName> Desired;
    auto AddCategory = [&](const FString& Input)
    {
        FString Value = Input.TrimStartAndEnd(); int32 Delimiter = INDEX_NONE; if (Value.FindChar(TEXT('|'), Delimiter)) { Value = Value.Left(Delimiter).TrimStartAndEnd(); }
        if (!Value.IsEmpty()) { Desired.AddUnique(FName(*Value)); }
    };
    if (Requested->Type == EJson::Array) { for (const auto& Value : Requested->AsArray()) { if (Value->Type == EJson::String) { if (!SafeName(Value->AsString(), true)) { Fail(R, TEXT("category_sorting_input_invalid"), TEXT("Requested category string exceeds its finite bound.")); return R; } AddCategory(Value->AsString()); } } }
    AddCategory(Front);
    if (Desired.IsEmpty()) { Fail(R, TEXT("category_sorting_inputs_missing"), TEXT("Missing 'category_order', 'category_to_front', or 'category' parameter")); return R; }
    if (Append)
    {
        for (FName Category : Previous) { Desired.AddUnique(Category); }
        for (const FBPVariableDescription& Variable : BP->NewVariables) { AddCategory(Variable.Category.ToString()); }
        for (const FBPVariableDescription& Variable : BP->GeneratedVariables) { AddCategory(Variable.Category.ToString()); }
    }
    if (Previous.Num() > 256 || Desired.Num() > 256) { Fail(R, TEXT("category_sorting_budget_exceeded"), TEXT("First category scope retains at most256 prior/final category names.")); return R; }
    const bool Changed = Desired != Previous; R.Data->SetBoolField(TEXT("changed"), Changed); R.Data->SetBoolField(TEXT("category_order_no_op"), !Changed);
    R.Data->SetArrayField(TEXT("previous_category_order"), CategoryNames(Previous)); R.Data->SetArrayField(TEXT("category_order"), CategoryNames(Desired)); R.Data->SetBoolField(TEXT("effective_compile_requested"), Changed && Compile);
    FLifecycleObservation Before, Configured, Compiled, Saved; R.Stage = TEXT("before_observation"); if (!CategoryCapture(BP,Before,R,TEXT("before_observation"),Typed.Get())) { R.bChangeStateKnown = false; Fail(R, TEXT("category_sorting_before_unobservable"), TEXT("Bounded before prefix incomplete; no category edit/compile/save.")); return R; }
    if (BP->CategorySorting != Previous) { Fail(R, TEXT("category_sorting_state_changed"), TEXT("Category storage changed during admission; no editor writer dispatched.")); return R; }
    if(!Current())return R;LifecycleCount(R,TEXT("outer_transaction_calls"));const FScopedTransaction Transaction(FText::FromString(TEXT("Set Blueprint category sorting")));if(!Current())return R;
    if (Changed)
    {
        R.Stage = TEXT("set_category_sorting"); R.bMutationAttempted = true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("explicit_modify_calls")); BP->Modify();if(!Current())return R;BP->CategorySorting=Desired;if(!Current())return R;
        LifecycleCount(R, TEXT("explicit_mark_modified_calls")); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);if(!SDKReturned())return R;LifecycleCount(R, TEXT("explicit_mark_package_dirty_calls")); BP->MarkPackageDirty();if(!Current())return R;
    }
    R.Stage = TEXT("after_configuration"); if (!CategoryObserved(BP,Configured,Before,R,TEXT("after_configuration"),Typed.Get())) { return R; }
    const bool ConfigurationOK = CategoryPreserved(Before, Configured, Desired, false, false, true, R); R.Data->SetBoolField(TEXT("after_configuration_preservation_passed"), ConfigurationOK);
    if (!ConfigurationOK) { Fail(R, TEXT("category_sorting_preservation_failed"), TEXT("Current categories or other declared scope differs; compile/save suppressed, actual prefix retained.")); return R; }
    const FLifecycleObservation* Final = &Configured; bool FullCompiled = false;
    if (Changed && Compile)
    {
        CategoryAggregateClear(R);R.Stage=TEXT("compile");if(!Current())return R;R.bCompileAttempted=true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("full_compile_attempts"));
        const auto Result=Cubeless::UE58::CompileBlueprint(BP->GetPathName(),false,false);R.Data->SetObjectField(TEXT("compile_result"),Result);R.Data->SetBoolField(TEXT("compiled"),true);R.Data->SetStringField(TEXT("compile_status_scope"),TEXT("returned_explicit_compile"));bool OK=false;Result->TryGetBoolField(TEXT("compiled"),OK);R.Compiled=OK;if(!SDKReturned())return R;R.Data->SetStringField(TEXT("compile_status"),LifecycleStatus(BP->Status));
        if (!CategoryObserved(BP,Compiled,Before,R,TEXT("after_compile"),Typed.Get())) { return R; } Final = &Compiled; FullCompiled = OK;
        const bool CompileOK = CategoryPreserved(Before, Compiled, Desired, OK, false, true, R); R.Data->SetBoolField(TEXT("after_compile_preservation_passed"), CompileOK);
        if (!OK || !CompileOK) { Fail(R, OK ? TEXT("category_sorting_compile_preservation_failed") : TEXT("category_sorting_compile_failed"), TEXT("Returned compiler/category prefix retained; first finite slice suppresses save after compiler/preservation failure, unlike old fallback.")); return R; }
    }
    const bool SaveEffective = Save && (Changed || BP->GetOutermost()->IsDirty()); R.Data->SetBoolField(TEXT("effective_save_requested"), SaveEffective);
    R.Data->SetBoolField(TEXT("save_due_to_existing_dirty_matching_order"), SaveEffective && !Changed); R.Data->SetBoolField(TEXT("package_dirty_before_save_decision"), BP->GetOutermost()->IsDirty());
    if (SaveEffective)
    {
        R.Data->SetObjectField(TEXT("before_save_observation"), R.Data->GetObjectField(Final == &Compiled ? TEXT("after_compile") : TEXT("after_configuration"))); R.Data->SetBoolField(TEXT("before_save_preservation_passed"), true);
        CategoryAggregateClear(R);R.Stage=TEXT("save");if(!Current())return R;R.bMutationAttempted=true; R.bSaveAttempted = true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("save_attempt_count"));
        auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("asset_path"), BP->GetPathName()); Receipt->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::SaveLoadedAsset")); Receipt->SetBoolField(TEXT("only_if_dirty"), false); Receipt->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) });
        R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(BP, false); Receipt->SetBoolField(TEXT("returned"), true); Receipt->SetBoolField(TEXT("saved"), R.bSaved); R.Data->SetBoolField(TEXT("saved"), R.bSaved);if(Typed&&!Typed->SavedReturned(R.bSaved))return R;
        if (!CategoryObserved(BP,Saved,Before,R,TEXT("after_save"),Typed.Get())) { return R; } Final = &Saved; R.Data->SetStringField(TEXT("compile_status"), R.bCompileAttempted ? LifecycleStatus(BP->Status) : TEXT("not_requested")); R.Data->SetStringField(TEXT("compile_status_scope"), TEXT("after_save"));
        const bool SaveOK = CategoryPreserved(Before, Saved, Desired, FullCompiled, R.bSaved, !R.bSaved, R);
        if (!R.bSaved || !SaveOK) { Fail(R, R.bSaved ? TEXT("category_sorting_save_preservation_failed") : TEXT("category_sorting_save_failed"), TEXT("Actual save receipt and phase retained; no retry/rollback.")); return R; }
    }
    R.Data->SetBoolField(TEXT("final_observation_complete"), Final->Complete); R.Data->SetBoolField(TEXT("runtime_or_cold_layout_verified"), false);
    R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}

// CE: exact original custom-event metadata setter; existing variable contracts stay unchanged.
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "UObject/MetaData.h"
#include "Internationalization/Text.h"
namespace Cubeless::UE58::Variable
{
namespace
{
void CustomMetadataInit(FResult& R)
{
    LifecycleInit(R);
    for (const TCHAR* K : { TEXT("graph_modify_calls"), TEXT("node_modify_calls"), TEXT("node_tooltip_assignments"), TEXT("node_metadata_set_calls"),
        TEXT("node_comment_assignments"), TEXT("node_comment_bubble_assignments"), TEXT("node_call_in_editor_assignments"), TEXT("metadata_call_in_editor_assignments"),
        TEXT("reconstruct_node_attempts"), TEXT("reconstruct_node_returns"), TEXT("notify_graph_changed_attempts"), TEXT("notify_graph_changed_returns"),
        TEXT("structural_return_count"), TEXT("generated_function_modify_calls"), TEXT("generated_function_metadata_set_attempts"),
        TEXT("generated_function_metadata_set_returns"), TEXT("generated_function_dirty_calls"), TEXT("custom_metadata_observation_calls"),
        TEXT("node_field_observation_calls"), TEXT("generated_function_metadata_observation_calls"), TEXT("metadata_expected_export_calls") }) { R.Data->SetNumberField(K, 0); }
    R.Data->SetArrayField(TEXT("node_metadata_calls"), {}); R.Data->SetArrayField(TEXT("generated_function_metadata_calls"), {});
    R.Data->SetBoolField(TEXT("generated_function_tooltip_updated"), false); R.Data->SetBoolField(TEXT("generated_metadata_verified"), false);
    R.Data->SetBoolField(TEXT("configured_state_preserved"), false); R.Data->SetBoolField(TEXT("compile_state_preserved"), false);
    R.Data->SetBoolField(TEXT("runtime_or_cold_metadata_behavior_verified"), false); R.Data->SetBoolField(TEXT("UI_visibility_verified"), false);
    R.Data->SetStringField(TEXT("admission_scope"), TEXT("One current own exact native CustomEvent without user pins/net override in an existing own event graph of a direct native Actor Blueprint; mature current own generated function required when compiling. Complex events/overrides/ambiguous selectors and arbitrary callback/runtime/UI variants remain incomplete."));
    R.Data->SetStringField(TEXT("observation_scope"), TEXT("Bounded member descriptors/CDO, graph topology/pins and nontransient reflected node/graph fields, serialized custom-event metadata fields, owned generated-function public metadata, SCS, native source/CDO, target disk and other loaded package dirtiness. Raw transient latent metadata cache retained separately; private bytecode/cache/runtime execution is unverified."));
    R.Data->SetStringField(TEXT("counter_scope"), TEXT("Explicit adapter call/application/return counts. Reconstruct/Notify/structural/compiler/save internal callbacks, temporary compiler objects and physical packed-bit writes are not counted as zero; implicit skeleton count is null. Core CompileBlueprint additionally requests one same-asset LoadObject, source-derived and separate from wrapper calls."));
    R.Data->SetStringField(TEXT("transaction_scope"), TEXT("One original outer transaction after finite preflight; original empty pre-handler transactions on earlier refusal are not reproduced."));
    R.Data->SetBoolField(TEXT("compiled"), false); R.Data->SetStringField(TEXT("compile_status"), TEXT("not_requested"));
}
TSharedRef<FJsonObject> CustomMetadataMap(const TMap<FName,FString>& Map)
{
    auto J = MakeShared<FJsonObject>(); for (const auto& P : Map) { J->SetStringField(P.Key.ToString(), P.Value); } return J;
}
// CE r4: source/display/flags and raw localization identity via public text inspection.
// GetDisplayString may rebuild the display cache; no identity setter or asset write is used.
TSharedRef<FJsonObject> CustomMetadataText(const FText& Text, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); const FString* Source = FTextInspector::GetSourceString(Text);
    const FString SourceValue = Source ? *Source : FString(), Display = FTextInspector::GetDisplayString(Text);
    const auto Namespace = FTextInspector::GetNamespace(Text), Key = FTextInspector::GetKey(Text);
    Complete = (!Source || SafeText(SourceValue, 65536)) && SafeText(Display, 65536)
        && (!Namespace.IsSet() || SafeText(Namespace.GetValue(), 65536)) && (!Key.IsSet() || SafeText(Key.GetValue(), 65536));
    J->SetBoolField(TEXT("complete"), Complete); J->SetBoolField(TEXT("source_present"), Source != nullptr);
    if (Source) { J->SetStringField(TEXT("source"), SourceValue); } else { J->SetField(TEXT("source"), MakeShared<FJsonValueNull>()); }
    J->SetStringField(TEXT("display"), Display); J->SetStringField(TEXT("flags"), LexToString(FTextInspector::GetFlags(Text)));
    J->SetBoolField(TEXT("culture_invariant"), Text.IsCultureInvariant()); J->SetBoolField(TEXT("transient"), Text.IsTransient());
    J->SetBoolField(TEXT("from_string_table"), Text.IsFromStringTable());
    if (Namespace.IsSet()) { J->SetStringField(TEXT("namespace"), Namespace.GetValue()); } else { J->SetField(TEXT("namespace"), MakeShared<FJsonValueNull>()); }
    if (Key.IsSet()) { J->SetStringField(TEXT("key"), Key.GetValue()); } else { J->SetField(TEXT("key"), MakeShared<FJsonValueNull>()); }
    return J;
}
TSharedRef<FJsonObject> CustomMetadataFields(const FKismetUserDeclaredFunctionMetadata& Meta, bool& Complete)
{
    auto J = MakeShared<FJsonObject>(); Complete = false; int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(FKismetUserDeclaredFunctionMetadata::StaticStruct()); It; ++It)
    {
        const FProperty* P = *It; if (P->HasAnyPropertyFlags(CPF_Transient)) { continue; }
        if (++Count > 64 || P->ArrayDim != 1) { return J; } FString Value; const void* A = P->ContainerPtrToValuePtr<void>(&Meta);
        if (!P->ExportText_Direct(Value, A, A, nullptr, PPF_None) || !SafeText(Value, 65536) || (Chars += Value.Len()) > 131072) { return J; }
        J->SetStringField(P->GetName(), Value);
    }
    Complete = true; return J;
}
TSharedRef<FJsonObject> CustomMetadataSnapshot(UBlueprint* BP, UK2Node_CustomEvent* N)
{
    auto J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("identity"), LifecycleIdentity(N)); J->SetStringField(TEXT("node_id"), N->NodeGuid.ToString());
    J->SetStringField(TEXT("custom_event_name"), N->CustomFunctionName.ToString()); J->SetStringField(TEXT("description"), N->GetUserDefinedMetaData().ToolTip.ToString());
    J->SetBoolField(TEXT("call_in_editor"), N->bCallInEditor); J->SetBoolField(TEXT("metadata_call_in_editor"), N->GetUserDefinedMetaData().bCallInEditor);
    J->SetStringField(TEXT("node_comment"), N->NodeComment); J->SetBoolField(TEXT("bubble_visible"), N->bCommentBubbleVisible);
    J->SetBoolField(TEXT("bubble_make_visible"), N->bCommentBubbleMakeVisible); J->SetBoolField(TEXT("bubble_pinned"), N->bCommentBubblePinned);
    J->SetObjectField(TEXT("user_metadata"), CustomMetadataMap(N->GetUserDefinedMetaData().GetMetaDataMap()));
    J->SetNumberField(TEXT("transient_latent_cache"), N->GetUserDefinedMetaData().HasLatentFunctions);
    bool Complete = false, TextComplete = false;
    J->SetObjectField(TEXT("tooltip_text"), CustomMetadataText(N->GetUserDefinedMetaData().ToolTip, TextComplete));
    J->SetObjectField(TEXT("serialized_metadata_fields"), CustomMetadataFields(N->GetUserDefinedMetaData(), Complete)); J->SetBoolField(TEXT("complete"), Complete && TextComplete);
    J->SetStringField(TEXT("generated_tooltip_scope"), TEXT("Current loaded own generated UFunction metadata only; absent/current-compile status is observed, not inferred from node descriptors."));
    J->SetBoolField(TEXT("current_compile"), BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings); return J;
}
bool CustomMetadataFunctionMaps(UBlueprint* BP, const TSharedRef<FJsonObject>& Out, FResult& R)
{
    if (!IsValid(BP->GeneratedClass)) { return false; } int32 Count = 0, Keys = 0, Chars = 0;
    for (TFieldIterator<UFunction> It(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        UFunction* F = *It; if (!IsValid(F) || F->GetOuter() != BP->GeneratedClass || F->GetOutermost() != BP->GetOutermost() || ++Count > 256) { return false; }
        LifecycleCount(R, TEXT("generated_function_metadata_observation_calls")); auto Row = MakeShared<FJsonObject>(); Out->SetObjectField(F->GetName(), Row);
        Row->SetObjectField(TEXT("identity"), LifecycleIdentity(F)); Row->SetStringField(TEXT("function_flags"), LexToString(static_cast<uint32>(F->FunctionFlags)));
        auto Values = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("metadata"), Values);
        const TMap<FName,FString>* Map = FMetaData::GetMapForObject(F); Row->SetBoolField(TEXT("metadata_map_present"), Map != nullptr);
        if (Map) { for (const auto& P : *Map) { if (++Keys > 4096 || !SafeName(P.Key.ToString(), true) || !SafeText(P.Value, 65536) || (Chars += P.Value.Len()) > 1048576) { return false; } Values->SetStringField(P.Key.ToString(), P.Value); } }
    }
    return true;
}
bool CustomMetadataCapture(UBlueprint* BP, FLifecycleObservation& O, FResult& R, const TCHAR* Phase)
{
    const bool Base = LifecycleCapture(BP, O, R, Phase); const auto J = R.Data->GetObjectField(Phase); J->SetBoolField(TEXT("complete"), false); O.Complete = false;
    auto Fields = MakeShared<FJsonObject>(), Metadata = MakeShared<FJsonObject>(), Functions = MakeShared<FJsonObject>(), SCS = MakeShared<FJsonObject>();
    O.State->SetObjectField(TEXT("graph_node_fields"), Fields); O.State->SetObjectField(TEXT("custom_event_metadata"), Metadata);
    O.State->SetObjectField(TEXT("generated_function_metadata"), Functions); O.State->SetObjectField(TEXT("scs"), SCS);
    if (!Base) { return false; } LifecycleCount(R, TEXT("custom_metadata_observation_calls")); J->SetStringField(TEXT("failed_stage"), TEXT("node_fields"));
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); int32 Count = 0;
    for (UEdGraph* G : Graphs)
    {
        auto GFields = MakeShared<FJsonObject>(); Fields->SetObjectField(G->GetPathName(), GFields); LifecycleCount(R, TEXT("node_field_observation_calls")); if (!LifecycleFields(G, GFields)) { return false; }
        for (UEdGraphNode* N : G->Nodes)
        {
            if (++Count > 8192) { return false; } auto NFields = MakeShared<FJsonObject>(); Fields->SetObjectField(N->GetPathName(), NFields);
            LifecycleCount(R, TEXT("node_field_observation_calls")); if (!LifecycleFields(N, NFields)) { return false; }
            if (auto Custom = Cast<UK2Node_CustomEvent>(N)) { const auto Row = CustomMetadataSnapshot(BP, Custom); Metadata->SetObjectField(N->GetPathName(), Row); if (!Row->GetBoolField(TEXT("complete"))) { return false; } }
        }
    }
    J->SetStringField(TEXT("failed_stage"), TEXT("generated_function_metadata")); if (!CustomMetadataFunctionMaps(BP, Functions, R)) { return false; }
    J->SetStringField(TEXT("failed_stage"), TEXT("scs")); if (!RemovalSCS(BP, SCS)) { return false; }
    O.Complete = true; J->SetBoolField(TEXT("complete"), true); J->SetStringField(TEXT("failed_stage"), TEXT("")); return true;
}
bool CustomMetadataTickCompatible(UBlueprint* BP, FResult& R)
{
    auto P = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("compiler_tick_policy"), P); P->SetBoolField(TEXT("complete"), false); P->SetBoolField(TEXT("compatible"), false);
    AActor* CDO = Cast<AActor>(BP->GeneratedClass->GetDefaultObject(false)); AActor* Parent = Cast<AActor>(BP->ParentClass->GetDefaultObject(false));
    if (!IsValid(CDO) || !IsValid(Parent) || Parent->GetClass() != AActor::StaticClass() || CDO->GetClass() != BP->GeneratedClass)
    { return Fail(R, TEXT("custom_event_metadata_tick_policy_unobservable"), TEXT("Existing own/native-parent Actor CDOs required; no creation.")); }
    P->SetBoolField(TEXT("current_can_ever_tick"), CDO->PrimaryActorTick.bCanEverTick); P->SetBoolField(TEXT("parent_can_ever_tick"), Parent->PrimaryActorTick.bCanEverTick);
    const FName Tick(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveTick)); bool Ambiguous = BP->GeneratedClass->FindFunctionByName(Tick, EIncludeSuperFlag::ExcludeSuper) != nullptr;
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); if (Graphs.Num() > 128) { return Fail(R, TEXT("custom_event_metadata_tick_policy_unobservable"), TEXT("Source graph budget exceeded.")); }
    for (UEdGraph* G : Graphs)
    {
        Ambiguous |= G->GetFName() == Tick;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (auto E = Cast<UK2Node_FunctionEntry>(N)) { Ambiguous |= E->CustomGeneratedFunctionName == Tick || E->FunctionReference.GetMemberName() == Tick; }
            if (auto E = Cast<UK2Node_Event>(N); E && (E->EventReference.GetMemberName() == Tick || E->CustomFunctionName == Tick))
            {
                int32 Links = 0; for (UEdGraphPin* Pin : E->Pins) { if (!Pin || Pin->GetOwningNodeUnchecked() != E) { return Fail(R, TEXT("custom_event_metadata_tick_policy_unobservable"), TEXT("Source Tick pin ownership incomplete.")); } Links += Pin->LinkedTo.Num(); }
                const bool Inert = E->GetClass() == UK2Node_Event::StaticClass() && E->GetOuter() == G && E->bOverrideFunction && E->CustomFunctionName.IsNone()
                    && E->GetDesiredEnabledState() == ENodeEnabledState::Disabled && Links == 0; Ambiguous |= !Inert;
            }
        }
    }
    const bool OK = CDO->PrimaryActorTick.bCanEverTick == Parent->PrimaryActorTick.bCanEverTick && (Parent->PrimaryActorTick.bCanEverTick || !Ambiguous);
    P->SetBoolField(TEXT("source_ambiguous"), Ambiguous); P->SetBoolField(TEXT("complete"), true); P->SetBoolField(TEXT("compatible"), OK);
    return OK || Fail(R, TEXT("custom_event_metadata_tick_policy_unsupported"), TEXT("Current actor Tick eligibility must be native-parent/compiler compatible before compile/save; original CDO-only/active override variants remain incomplete."));
}
TSharedRef<FJsonObject> CustomMetadataCopy(const TSharedRef<FJsonObject>& Old)
{ auto J = MakeShared<FJsonObject>(); for (const auto& P : Old->Values) { J->SetField(FString(P.Key.ToView()), P.Value); } return J; }
bool CustomMetadataSetExpectedField(UClass* Class, const TSharedRef<FJsonObject>& Fields, const TCHAR* Name, const FString* String, const bool* Bool)
{
    FProperty* P = FindFProperty<FProperty>(Class, Name); const auto Old = Fields->TryGetField(Name); if (!P || !Old || Old->Type != EJson::Object) { return false; }
    FDefaultConstructedPropertyElement Scratch(P);
    if (String) { auto S = CastField<FStrProperty>(P); if (!S) { return false; } S->SetPropertyValue(Scratch.GetObjAddress(), *String); }
    else { auto B = CastField<FBoolProperty>(P); if (!B || !Bool) { return false; } B->SetPropertyValue(Scratch.GetObjAddress(), *Bool); }
    FString Text; if (!P->ExportText_Direct(Text, Scratch.GetObjAddress(), Scratch.GetObjAddress(), nullptr, PPF_None)) { return false; }
    auto Row = CustomMetadataCopy(Old->AsObject().ToSharedRef()); Row->SetStringField(TEXT("export_text"), Text); Fields->SetObjectField(Name, Row); return true;
}
bool CustomMetadataSetExpectedMetadataField(UK2Node_CustomEvent* N, const FKismetUserDeclaredFunctionMetadata& Meta, const TSharedRef<FJsonObject>& Fields)
{
    const FStructProperty* P = FindFProperty<FStructProperty>(N->GetClass(), TEXT("MetaData")); const auto Old = Fields->TryGetField(TEXT("MetaData"));
    if (!P || P->ArrayDim != 1 || P->Struct != FKismetUserDeclaredFunctionMetadata::StaticStruct() || !Old || Old->Type != EJson::Object) { return false; }
    FString Text; if (!P->ExportText_Direct(Text, &Meta, &Meta, N, PPF_None) || !SafeText(Text, 65536)) { return false; }
    auto Row = CustomMetadataCopy(Old->AsObject().ToSharedRef()); Row->SetStringField(TEXT("export_text"), Text); Fields->SetObjectField(TEXT("MetaData"), Row); return true;
}
void CustomMetadataFirstFieldMismatch(FResult& R, const FString& Object, const TCHAR* Property, const TCHAR* Phase,
    const TCHAR* Reason, const TSharedPtr<FJsonValue>& Expected, const TSharedPtr<FJsonValue>& Actual)
{
    if (R.Data->HasField(TEXT("first_metadata_field_mismatch"))) { return; }
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("object"), Object); J->SetStringField(TEXT("property"), Property);
    J->SetStringField(TEXT("phase"), Phase); J->SetStringField(TEXT("reason"), Reason);
    J->SetField(TEXT("expected"), Expected.IsValid() ? Expected : MakeShared<FJsonValueNull>());
    J->SetField(TEXT("actual"), Actual.IsValid() ? Actual : MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("actual_missing"), !Actual.IsValid()); R.Data->SetObjectField(TEXT("first_metadata_field_mismatch"), J);
}
bool CustomMetadataExpectedNodeFields(const FLifecycleObservation& Before, UK2Node_CustomEvent* N, bool Description, const FString& Text,
    const TOptional<bool>& Flag, bool Comment, const FKismetUserDeclaredFunctionMetadata& Metadata, TSharedRef<FJsonObject>& Desired, FResult& R)
{
    Desired = CustomMetadataCopy(Before.State->GetObjectField(TEXT("graph_node_fields"))->GetObjectField(N->GetPathName()).ToSharedRef()); const bool False = false;
    auto Serialized = [&](const TCHAR* Name, const FString* String, const bool* Bool)
    {
        if (CustomMetadataSetExpectedField(N->GetClass(), Desired, Name, String, Bool)) { return true; }
        TSharedPtr<FJsonValue> Expected;
        if (String) { Expected = MakeShared<FJsonValueString>(*String); }
        else { Expected = MakeShared<FJsonValueBoolean>(*Bool); }
        CustomMetadataFirstFieldMismatch(R, N->GetPathName(), Name, TEXT("prepare_expected_node_fields"),
            TEXT("Required authored serialized field/type/export is missing or invalid"), Expected, Desired->TryGetField(Name)); return false;
    };
    if (Description && Comment)
    {
        if (!Serialized(TEXT("NodeComment"), &Text, nullptr)
            || !Serialized(TEXT("bCommentBubbleVisible"), nullptr, &False)
            || !Serialized(TEXT("bCommentBubblePinned"), nullptr, &False)) { return false; }
        // EdGraphNode.h: only MakeVisible is Transient. LifecycleFields excludes
        // that UI bit; its existing direct snapshot, not a fabricated serialized
        // row, proves the requested false value after the real writer/callbacks.
        const auto* UI = FindFProperty<FBoolProperty>(N->GetClass(), TEXT("bCommentBubbleMakeVisible"));
        const auto Snapshot = Before.State->GetObjectField(TEXT("custom_event_metadata"))->TryGetField(N->GetPathName());
        const auto Observed = Snapshot.IsValid() && Snapshot->Type == EJson::Object
            ? Snapshot->AsObject()->TryGetField(TEXT("bubble_make_visible")) : TSharedPtr<FJsonValue>();
        if (!UI || UI->ArrayDim != 1 || UI->GetOwnerStruct() != UEdGraphNode::StaticClass() || !UI->HasAnyPropertyFlags(CPF_Transient)
            || Desired->HasField(TEXT("bCommentBubbleMakeVisible")) || !Observed.IsValid() || Observed->Type != EJson::Boolean
            || Observed->AsBool() != N->bCommentBubbleMakeVisible)
        {
            CustomMetadataFirstFieldMismatch(R, N->GetPathName(), TEXT("bCommentBubbleMakeVisible"), TEXT("prepare_expected_node_fields"),
                TEXT("Expected exact transient UI bool omitted from serialized capture with a matching direct before observation"),
                MakeShared<FJsonValueBoolean>(False), Observed); return false;
        }
        auto UIExpected = MakeShared<FJsonObject>(); UIExpected->SetStringField(TEXT("object"), N->GetPathName());
        UIExpected->SetStringField(TEXT("property"), TEXT("bCommentBubbleMakeVisible")); UIExpected->SetBoolField(TEXT("before"), Observed->AsBool());
        UIExpected->SetBoolField(TEXT("expected"), False); UIExpected->SetBoolField(TEXT("serialized_capture_excludes_transient"), true);
        UIExpected->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(UI->GetPropertyFlags())));
        R.Data->SetObjectField(TEXT("expected_transient_comment_bubble"), UIExpected);
    }
    if (Flag.IsSet()) { const bool Value = Flag.GetValue(); if (!Serialized(TEXT("bCallInEditor"), nullptr, &Value)) { return false; } }
    if (!CustomMetadataSetExpectedMetadataField(N, Metadata, Desired))
    {
        CustomMetadataFirstFieldMismatch(R, N->GetPathName(), TEXT("MetaData"), TEXT("prepare_expected_node_fields"),
            TEXT("Required authored metadata struct/type/export is missing or invalid"), MakeShared<FJsonValueString>(TEXT("complete FKismetUserDeclaredFunctionMetadata scratch export")),
            Desired->TryGetField(TEXT("MetaData"))); return false;
    }
    LifecycleCount(R, TEXT("metadata_expected_export_calls")); return true;
}
bool CustomMetadataNodesSame(const TSharedRef<FJsonObject>& Before, const TSharedRef<FJsonObject>& After, const FString& Path, const TSharedRef<FJsonObject>& Desired)
{
    if (Before->Values.Num() != After->Values.Num()) { return false; }
    for (const auto& P : Before->Values)
    {
        const auto Q = After->TryGetField(P.Key.ToView()); if (!Q || Q->Type != EJson::Object) { return false; }
        if (FString(P.Key.ToView()) != Path) { if (!LifecycleEqual(P.Value, Q)) { return false; } continue; }
        if (!LifecycleEqual(Desired, Q->AsObject().ToSharedRef())) { return false; }
    }
    return true;
}
TSharedRef<FJsonObject> CustomMetadataFunctionDesired(const TSharedRef<FJsonObject>& Before, const FString& Description, bool HasDescription,
    const TOptional<bool>& Flag, bool Compiled, bool GeneratedWrite)
{
    auto Desired = CustomMetadataCopy(Before);
    auto EraseName = [&](FName Name) { TArray<FString> Remove; for (const auto& P : Desired->Values) { if (FName(*FString(P.Key.ToView())) == Name) { Remove.Add(FString(P.Key.ToView())); } } for (const auto& K : Remove) { Desired->RemoveField(K); } };
    auto SetName = [&](FName Name, const FString& Value) { EraseName(Name); Desired->SetStringField(Name.ToString(), Value); };
    if (Compiled) { EraseName(TEXT("ShortTooltip")); EraseName(FBlueprintMetadata::MD_Tooltip); // Custom-event stub entry does not copy ToolTip; original explicit writes follow.
        if (Flag.IsSet()) { if (Flag.GetValue()) { SetName(FBlueprintMetadata::MD_CallInEditor, TEXT("true")); } else { EraseName(FBlueprintMetadata::MD_CallInEditor); } } }
    if (GeneratedWrite && HasDescription) { SetName(FBlueprintMetadata::MD_Tooltip, Description); SetName(TEXT("ShortTooltip"), Description); }
    return Desired;
}
bool CustomMetadataFunctionsSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, const FString& Selected,
    const TSharedRef<FJsonObject>& Desired, bool Compiled, bool Saved)
{
    if (A->Values.Num() != B->Values.Num()) { return false; }
    for (const auto& P : A->Values)
    {
        const auto Q = B->TryGetField(P.Key.ToView()); if (!Q || P.Value->Type != EJson::Object || Q->Type != EJson::Object) { return false; }
        const auto Old = P.Value->AsObject(), Now = Q->AsObject();
        const auto OldIdentity = Old->GetObjectField(TEXT("identity")), NewIdentity = Now->GetObjectField(TEXT("identity"));
        if (OldIdentity->Values.Num() != NewIdentity->Values.Num()) { return false; }
        for (const auto& Field : OldIdentity->Values)
        {
            const auto Value = NewIdentity->TryGetField(Field.Key.ToView()); if (!Value) { return false; }
            if (Field.Key.ToView() != TEXT("object_flags")) { if (!LifecycleEqual(Field.Value, Value)) { return false; } continue; }
            if (Field.Value->Type != EJson::String || Value->Type != EJson::String) { return false; }
            const FString X = Field.Value->AsString(), Y = Value->AsString(); const uint64 F = FCString::Strtoui64(*X, nullptr, 10), G = FCString::Strtoui64(*Y, nullptr, 10);
            const uint64 Load = static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted);
            if (LexToString(F) != X || LexToString(G) != Y) { return false; }
            // Only current owned generated UFunctions after confirmed compile; KismetCompiler creates them RF_Public.
            if (Compiled) { if ((F & ~Load) != RF_Public || (G & ~Load) != RF_Public) { return false; } } // Only these two load-history bits may vary; no zero/union assumption.
            else if (Saved ? (F & ~Load) != (G & ~Load) : F != G) { return false; } // Saved current owned functions retain every nonload bit.
        }
        if (Old->GetStringField(TEXT("function_flags")) != Now->GetStringField(TEXT("function_flags"))) { return false; }
        if (!LifecycleEqual(FString(P.Key.ToView()) == Selected ? Desired : Old->GetObjectField(TEXT("metadata")).ToSharedRef(), Now->GetObjectField(TEXT("metadata")).ToSharedRef())) { return false; }
    }
    return true;
}
// A successful save may vary only the selected request-assigned literal's namespace/key.
// Its retained pre-save identity and raw current source/display/flags/base-text export qualify
// that scope BEFORE actual ToolTip is copied into an expected metadata STRUCT COPY.
bool CustomMetadataSavedTooltip(UK2Node_CustomEvent* N, const FKismetUserDeclaredFunctionMetadata& DesiredMetadata,
    const TSharedRef<FJsonObject>& AfterRow, const TSharedRef<FJsonObject>& NodeFields, TSharedRef<FJsonObject>& ExpectedNodeFields,
    TSharedRef<FJsonObject>& ExpectedMetadataFields, FResult& R)
{
    auto Proof = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("save_tooltip_localization"), Proof);
    for (const TCHAR* K : {TEXT("qualification_passed"), TEXT("pre_save_identity_verified"), TEXT("live_snapshot_verified"),
        TEXT("source_display_flags_preserved"), TEXT("base_text_export_verified"), TEXT("identity_changed"), TEXT("scratch_exports_complete")}) { Proof->SetBoolField(K, false); }
    const auto Expected = R.Data->GetObjectField(TEXT("expected_tooltip_text")).ToSharedRef();
    const auto PreSave = R.Data->GetObjectField(TEXT("before_save_observation"))->GetObjectField(TEXT("state"))
        ->GetObjectField(TEXT("custom_event_metadata"))->GetObjectField(N->GetPathName())->GetObjectField(TEXT("tooltip_text")).ToSharedRef();
    const auto Actual = AfterRow->GetObjectField(TEXT("tooltip_text")).ToSharedRef(); bool LiveComplete = false;
    const auto Live = CustomMetadataText(N->GetUserDefinedMetaData().ToolTip, LiveComplete);
    Proof->SetObjectField(TEXT("request_assigned_text"), Expected); Proof->SetObjectField(TEXT("before_save_text"), PreSave);
    Proof->SetObjectField(TEXT("after_save_text"), Actual); Proof->SetObjectField(TEXT("live_text"), Live);
    const bool PreSaveSame = Expected->GetBoolField(TEXT("complete")) && PreSave->GetBoolField(TEXT("complete")) && LifecycleEqual(Expected, PreSave);
    const bool LiveSame = LiveComplete && Actual->GetBoolField(TEXT("complete")) && LifecycleEqual(Actual, Live);
    auto IdentityProjection = CustomMetadataCopy(Actual); IdentityProjection->SetField(TEXT("namespace"), Expected->TryGetField(TEXT("namespace")));
    IdentityProjection->SetField(TEXT("key"), Expected->TryGetField(TEXT("key"))); Proof->SetObjectField(TEXT("identity_only_comparison_projection"), IdentityProjection);
    const bool TextSame = Expected->GetBoolField(TEXT("source_present")) && Expected->GetStringField(TEXT("source")) == Expected->GetStringField(TEXT("display"))
        && !Expected->GetBoolField(TEXT("culture_invariant")) && !Expected->GetBoolField(TEXT("transient")) && !Expected->GetBoolField(TEXT("from_string_table"))
        && LifecycleEqual(Expected, IdentityProjection);
    const bool IdentityChanged = !LifecycleEqual(Expected, Actual); const FString ActualExport = AfterRow->GetObjectField(TEXT("serialized_metadata_fields"))->GetStringField(TEXT("ToolTip"));
    bool BaseExport = !IdentityChanged; FString AllowedExport;
    if (IdentityChanged)
    {
        const auto Namespace = Actual->TryGetField(TEXT("namespace")), Key = Actual->TryGetField(TEXT("key")); FGuid Guid;
        // FromString assigned a keyless literal. Persistent UE package save assigns its GUID key.
        BaseExport = Expected->TryGetField(TEXT("namespace"))->Type == EJson::Null && Expected->TryGetField(TEXT("key"))->Type == EJson::Null
            && Namespace && Namespace->Type == EJson::String && Key && Key->Type == EJson::String && FGuid::ParseExact(Key->AsString(), EGuidFormats::Digits, Guid) && Guid.IsValid();
        if (BaseExport)
        {
            AllowedExport = TEXT("NSLOCTEXT(\"") + Namespace->AsString().ReplaceCharWithEscapedChar() + TEXT("\", \"")
                + Key->AsString().ReplaceCharWithEscapedChar() + TEXT("\", \"") + Expected->GetStringField(TEXT("source")).ReplaceCharWithEscapedChar() + TEXT("\")");
            BaseExport = ActualExport == AllowedExport;
        }
    }
    Proof->SetBoolField(TEXT("pre_save_identity_verified"), PreSaveSame); Proof->SetBoolField(TEXT("live_snapshot_verified"), LiveSame);
    Proof->SetBoolField(TEXT("source_display_flags_preserved"), TextSame); Proof->SetBoolField(TEXT("base_text_export_verified"), BaseExport);
    Proof->SetBoolField(TEXT("identity_changed"), IdentityChanged); Proof->SetStringField(TEXT("actual_tooltip_export"), ActualExport);
    Proof->SetStringField(TEXT("allowed_base_tooltip_export"), AllowedExport);
    if (!PreSaveSame || !LiveSame || !TextSame || !BaseExport) { return false; }
    FKismetUserDeclaredFunctionMetadata Scratch = DesiredMetadata; Scratch.ToolTip = N->GetUserDefinedMetaData().ToolTip;
    bool FieldsComplete = false; ExpectedMetadataFields = CustomMetadataFields(Scratch, FieldsComplete); ExpectedNodeFields = CustomMetadataCopy(NodeFields);
    const bool NodeComplete = FieldsComplete && CustomMetadataSetExpectedMetadataField(N, Scratch, ExpectedNodeFields);
    Proof->SetBoolField(TEXT("scratch_exports_complete"), NodeComplete);
    Proof->SetObjectField(TEXT("expected_serialized_metadata_fields_with_saved_tooltip_identity"), ExpectedMetadataFields);
    Proof->SetObjectField(TEXT("expected_selected_node_fields_with_saved_tooltip_identity"), ExpectedNodeFields);
    Proof->SetBoolField(TEXT("qualification_passed"), NodeComplete); return NodeComplete;
}
bool CustomMetadataPreserved(const FLifecycleObservation& Before, const FLifecycleObservation& After, const FString& Path,
    const FString& FunctionName, const TSharedRef<FJsonObject>& NodeFields, const TSharedRef<FJsonObject>& MetadataFields,
    const TSharedRef<FJsonObject>& FunctionMetadata, bool Compiled, bool Saved, UK2Node_CustomEvent* SelectedNode,
    const FKismetUserDeclaredFunctionMetadata& DesiredMetadata, bool HasDescription, FResult& R)
{
    TSharedRef<FJsonObject> ExpectedNodeFields = NodeFields, ExpectedMetadataFields = MetadataFields; bool TooltipQualified = true;
    const auto Selected = After.State->GetObjectField(TEXT("custom_event_metadata"))->TryGetField(Path);
    if (Saved && HasDescription) { TooltipQualified = Selected && Selected->Type == EJson::Object && IsValid(SelectedNode) && SelectedNode->GetPathName() == Path
        && CustomMetadataSavedTooltip(SelectedNode, DesiredMetadata, Selected->AsObject().ToSharedRef(), NodeFields, ExpectedNodeFields, ExpectedMetadataFields, R); }
    const auto A = Before.State->GetObjectField(TEXT("variables")), B = After.State->GetObjectField(TEXT("variables")); TArray<FName> Order;
    for (const auto& V : A->GetArrayField(TEXT("new_variables"))) { Order.Add(FName(*V->AsObject()->GetStringField(TEXT("name")))); }
    const auto X = Before.State->GetObjectField(TEXT("custom_event_metadata")), Y = After.State->GetObjectField(TEXT("custom_event_metadata")); bool Meta = X->Values.Num() == Y->Values.Num();
    for (const auto& P : X->Values)
    {
        const auto Q = Y->TryGetField(P.Key.ToView()); if (!Q || Q->Type != EJson::Object) { Meta = false; continue; }
        const auto Old = P.Value->AsObject(), Now = Q->AsObject();
        if (!LifecycleEqual(FString(P.Key.ToView()) == Path ? ExpectedMetadataFields : Old->GetObjectField(TEXT("serialized_metadata_fields")).ToSharedRef(), Now->GetObjectField(TEXT("serialized_metadata_fields")).ToSharedRef())) { Meta = false; }
    }
    bool TransientComment = true;
    if (R.Data->HasField(TEXT("expected_transient_comment_bubble")))
    {
        const auto ExpectedUI = R.Data->GetObjectField(TEXT("expected_transient_comment_bubble"));
        const auto Row = Y->TryGetField(Path);
        const auto ActualUI = Row.IsValid() && Row->Type == EJson::Object
            ? Row->AsObject()->TryGetField(TEXT("bubble_make_visible")) : TSharedPtr<FJsonValue>();
        TransientComment = ExpectedUI->GetStringField(TEXT("object")) == Path && ActualUI.IsValid() && ActualUI->Type == EJson::Boolean
            && ActualUI->AsBool() == ExpectedUI->GetBoolField(TEXT("expected")) && IsValid(SelectedNode)
            && SelectedNode->GetPathName() == Path && SelectedNode->bCommentBubbleMakeVisible == ActualUI->AsBool();
        R.Data->SetBoolField(TEXT("requested_transient_comment_bubble_verified"), TransientComment);
        if (!TransientComment)
        {
            CustomMetadataFirstFieldMismatch(R, Path, TEXT("bCommentBubbleMakeVisible"), *R.Stage,
                TEXT("Requested transient UI state differs from current direct snapshot or live node"),
                MakeShared<FJsonValueBoolean>(ExpectedUI->GetBoolField(TEXT("expected"))), ActualUI);
        }
    }
    const bool Functions = CustomMetadataFunctionsSame(Before.State->GetObjectField(TEXT("generated_function_metadata")).ToSharedRef(), After.State->GetObjectField(TEXT("generated_function_metadata")).ToSharedRef(), FunctionName, FunctionMetadata, Compiled, Saved);
    const bool State = Before.Complete && After.Complete && TooltipQualified && Meta && Functions && TransientComment
        && LifecycleRowsSame(A->GetArrayField(TEXT("new_variables")), B->GetArrayField(TEXT("new_variables")), Order, nullptr, Compiled)
        && LifecycleEqual(A->TryGetField(TEXT("generated_variables")), B->TryGetField(TEXT("generated_variables"))) && LifecycleEqual(A->TryGetField(TEXT("category_sorting")), B->TryGetField(TEXT("category_sorting")))
        && A->GetStringField(TEXT("generated_cdo_path")) == B->GetStringField(TEXT("generated_cdo_path"))
        && LifecycleEqual(Before.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef())
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), Saved, false)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), Saved, Compiled)
        && LifecycleGraphSame(Before.State->GetObjectField(TEXT("graphs")).ToSharedRef(), After.State->GetObjectField(TEXT("graphs")).ToSharedRef(), Saved)
        && CustomMetadataNodesSame(Before.State->GetObjectField(TEXT("graph_node_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("graph_node_fields")).ToSharedRef(), Path, ExpectedNodeFields)
        && CategorySCSSame(Before.State->GetObjectField(TEXT("scs")).ToSharedRef(), After.State->GetObjectField(TEXT("scs")).ToSharedRef(), Saved);
    const bool Source = LifecycleEqual(Before.Source, After.Source), Refs = LifecycleEqual(Before.References, After.References), Other = LifecycleEqual(Before.Other, After.Other);
    const bool Disk = Saved ? After.Disk->GetBoolField(TEXT("exists")) && After.Disk->GetNumberField(TEXT("bytes")) > 0 && !B->GetBoolField(TEXT("package_dirty")) && Before.Disk->GetStringField(TEXT("path")) == After.Disk->GetStringField(TEXT("path")) : LifecycleEqual(Before.Disk, After.Disk);
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("nonselected_state_preserved"), State);
    R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other);
    R.Data->SetBoolField(Saved ? TEXT("save_state_preserved") : TEXT("disk_preserved_before_save"), Disk && State && Source && Refs && Other);
    if (Saved && HasDescription && R.Data->HasField(TEXT("save_tooltip_localization"))) { R.Data->GetObjectField(TEXT("save_tooltip_localization"))->SetBoolField(TEXT("full_metadata_and_node_fields_preserved"), TooltipQualified && Meta && CustomMetadataNodesSame(Before.State->GetObjectField(TEXT("graph_node_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("graph_node_fields")).ToSharedRef(), Path, ExpectedNodeFields)); }
    return State && Source && Refs && Other && Disk;
}
bool CustomMetadataObserved(UBlueprint* BP, FLifecycleObservation& O, const FLifecycleObservation& Before, FResult& R, const TCHAR* Phase)
{
    if (!CustomMetadataCapture(BP, O, R, Phase)) { R.bChangeStateKnown = false; return Fail(R, TEXT("custom_event_metadata_observation_incomplete"), TEXT("Bounded current phase incomplete; actual writer/compile/save prefix retained without retry.")); }
    R.bChangeStateKnown = true; R.bChanged = !LifecycleEqual(Before.State, O.State) || !LifecycleEqual(Before.Source, O.Source)
        || !LifecycleEqual(Before.References, O.References) || !LifecycleEqual(Before.Other, O.Other) || !LifecycleEqual(Before.Disk, O.Disk); return true;
}
}
FResult SetCustomEventMetadata(const FString& Path, const FString& PrimaryName, const FString& Description,
    const FString& NodeId, const FString& AliasName, const FString& Tooltip, const FString& FlagJson,
    bool Comment, bool Compile, bool Save)
{
    FResult R; CustomMetadataInit(R); R.bCompileRequested = Compile; R.bSaveRequested = Save;
    R.Data->SetStringField(TEXT("blueprint_name"), Path); R.Data->SetStringField(TEXT("requested_custom_event_name"), PrimaryName);
    R.Data->SetStringField(TEXT("requested_node_id"), NodeId); R.Data->SetStringField(TEXT("event_name_alias"), AliasName);
    R.Data->SetStringField(TEXT("description_primary"), Description); R.Data->SetStringField(TEXT("tooltip_alias"), Tooltip);
    FString Name = PrimaryName.IsEmpty() ? AliasName : PrimaryName; Name.TrimStartAndEndInline(); FString Id = NodeId; Id.TrimStartAndEndInline();
    const FString Text = Description.IsEmpty() ? Tooltip : Description; bool HasDescription = !Text.IsEmpty(); TOptional<bool> Flag;
    if (!SafeName(Path) || !Path.StartsWith(TEXT("/Game/")) || !SafeName(PrimaryName, true) || !SafeName(AliasName, true) || !SafeName(NodeId, true)
        || !SafeText(Description, 65536) || !SafeText(Tooltip, 65536))
    { Fail(R, TEXT("custom_event_metadata_input_invalid"), TEXT("Exact bounded safe Blueprint/event/GUID and description strings required.")); return R; }
    if (Name.IsEmpty() && Id.IsEmpty()) { Fail(R, TEXT("custom_event_metadata_missing_identifier"), TEXT("custom_event_name or node_id is required.")); return R; }
    // AAW: original Batch presence uses this existing private JSON argument.
    // Public Named bool/null semantics and every downstream writer/proof stay unchanged.
    if (FlagJson.TrimStartAndEnd().StartsWith(TEXT("{")))
    {
        TSharedPtr<FJsonValue> Carrier;
        if (!Parse(FlagJson, Carrier, R)) { return R; }
        if (!Carrier.IsValid() || Carrier->Type != EJson::Object)
        { Fail(R, TEXT("custom_event_metadata_carrier_invalid"), TEXT("Exact private metadata presence object required; no asset load or edit.")); return R; }
        const auto Object = Carrier->AsObject();
        if (!Object.IsValid()) { Fail(R, TEXT("custom_event_metadata_carrier_invalid"), TEXT("Private metadata presence object is invalid.")); return R; }
        const auto Tag = Object->TryGetField(TEXT("cubeless_raw_metadata_presence_v1"));
        const auto Present = Object->TryGetField(TEXT("description_present"));
        const auto Call = Object->TryGetField(TEXT("call_in_editor"));
        if (Object->Values.Num() != 3 || !Tag.IsValid() || Tag->Type != EJson::Boolean || !Tag->AsBool()
            || !Present.IsValid() || Present->Type != EJson::Boolean || !Call.IsValid()
            || (Call->Type != EJson::Null && Call->Type != EJson::Boolean)
            || !Tooltip.IsEmpty() || (!Present->AsBool() && !Description.IsEmpty()))
        { Fail(R, TEXT("custom_event_metadata_carrier_invalid"), TEXT("Exact tag/keys/typed description presence and nullable Boolean flag must match the selected description; no asset load or edit.")); return R; }
        HasDescription = Present->AsBool();
        if (Call->Type == EJson::Boolean) { Flag = Call->AsBool(); }
        R.Data->SetBoolField(TEXT("raw_metadata_presence_carrier"), true);
        R.Data->SetBoolField(TEXT("raw_description_present"), HasDescription);
        R.Data->SetBoolField(TEXT("raw_call_in_editor_present"), Flag.IsSet());
    }
    else if (!LifecycleFlag(FlagJson, Flag, R)) { return R; }
    if (!HasDescription && !Flag.IsSet()) { Fail(R, TEXT("custom_event_metadata_missing_value"), TEXT("Missing description/tooltip or call_in_editor value; default None is omission.")); return R; }
    FString Ref = Text.ToLower().Replace(TEXT("\\"), TEXT("/"));
    for (const TCHAR* Token : {TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("cubelesscontrolrigtools"),TEXT("/script/cubelessmcp"),TEXT("/cubelessmcp_ue58/"),TEXT("/script/modelcontextprotocol"),TEXT("/script/toolsetregistry")})
    { if (Ref.Contains(Token)) { Fail(R, TEXT("custom_event_metadata_reference_unsupported"), TEXT("Description cannot persist MCP-only dependency references.")); return R; } }
    TStrongObjectPtr<UBlueprint> Hold(LoadBlueprint(Path, R)); UBlueprint* BP = Hold.Get(); if (!BP || !LifecycleSimple(BP, R)) { return R; }
    R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName()); TArray<UK2Node_CustomEvent*> Events;
    FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(BP, Events); UK2Node_CustomEvent* N = nullptr;
    for (UK2Node_CustomEvent* E : Events)
    {
        if (!IsValid(E)) { Fail(R, TEXT("custom_event_metadata_target_unobservable"), TEXT("Custom-event inventory contains an invalid node.")); return R; }
        const bool Matches = (!Id.IsEmpty() && E->NodeGuid.ToString() == Id) || (!Name.IsEmpty() && E->CustomFunctionName == FName(*Name));
        if (!Matches) { continue; } if (N) { Fail(R, TEXT("custom_event_metadata_target_ambiguous"), TEXT("Original OR name/GUID selectors match multiple events; first-match ambiguity is outside finite scope.")); return R; } N = E;
    }
    if (!N) { Fail(R, TEXT("custom_event_metadata_not_found"), FString::Printf(TEXT("Custom event not found in Blueprint '%s': %s"), *Path, Name.IsEmpty() ? *Id : *Name)); return R; }
    UEdGraph* G = N->GetGraph();
    if (Events.Num() != 1 || N->GetClass() != UK2Node_CustomEvent::StaticClass() || !N->IsEditable() || !N->NodeGuid.IsValid() || !N->UserDefinedPins.IsEmpty()
        || N->bOverrideFunction || N->GetNetFlags() != 0 || !IsValid(G) || G->GetOuter() != BP || N->GetOuter() != G || G->GetClass() != UEdGraph::StaticClass()
        || (!BP->UbergraphPages.Contains(G) && !BP->EventGraphs.Contains(G)) || !G->GetSchema() || G->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass()
        || N->CustomFunctionName.IsNone() || BP->ParentClass->FindFunctionByName(N->CustomFunctionName) != nullptr)
    { Fail(R, TEXT("custom_event_metadata_scope_unsupported"), TEXT("First slice requires one exact editable ordinary own parameterless custom event in an own K2 event graph; complex/net/inherited/multiple/override cases remain incomplete.")); return R; }
    TStrongObjectPtr<UK2Node_CustomEvent> NodeHold(N); TStrongObjectPtr<UEdGraph> GraphHold(G);
    UFunction* InitialFunction = BP->GeneratedClass->FindFunctionByName(N->CustomFunctionName, EIncludeSuperFlag::ExcludeSuper);
    if (Compile && (!(BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings) || !IsValid(InitialFunction) || InitialFunction->GetOuter() != BP->GeneratedClass))
    { Fail(R, TEXT("custom_event_metadata_mature_function_required"), TEXT("First compile slice requires an already compiled own generated custom-event function; create/compile it through existing tools first.")); return R; }
    if ((Compile || Save) && !CustomMetadataTickCompatible(BP, R)) { return R; }
    FLifecycleObservation Before, Configured, Compiled, Generated, Saved; R.Stage = TEXT("before_observation");
    if (!CustomMetadataCapture(BP, Before, R, TEXT("before_observation"))) { R.bChangeStateKnown = false; Fail(R, TEXT("custom_event_metadata_before_unobservable"), TEXT("Bounded before state incomplete; no node edit.")); return R; }
    const FString NodePath = N->GetPathName(), FunctionName = N->CustomFunctionName.ToString(); const FGuid Guid = N->NodeGuid;
    const auto BeforeFunctions = Before.State->GetObjectField(TEXT("generated_function_metadata"));
    if (!BeforeFunctions->HasField(FunctionName)) { Fail(R, TEXT("custom_event_metadata_current_function_required"), TEXT("Current own generated function metadata required for exact finite phase proof; fresh uncompiled event variants remain incomplete.")); return R; }
    const auto BeforeFunctionMetadata = BeforeFunctions->GetObjectField(FunctionName)->GetObjectField(TEXT("metadata")).ToSharedRef();
    FKismetUserDeclaredFunctionMetadata DesiredMetadata = N->GetUserDefinedMetaData();
    if (HasDescription) { DesiredMetadata.ToolTip = FText::FromString(Text); DesiredMetadata.SetMetaData(FBlueprintMetadata::MD_Tooltip, Text); }
    if (Flag.IsSet()) { DesiredMetadata.bCallInEditor = Flag.GetValue(); }
    bool MetadataComplete = false, ExpectedTextComplete = false; const auto DesiredMetadataFields = CustomMetadataFields(DesiredMetadata, MetadataComplete); TSharedRef<FJsonObject> DesiredNodeFields = MakeShared<FJsonObject>();
    const auto ExpectedTooltipText = CustomMetadataText(DesiredMetadata.ToolTip, ExpectedTextComplete);
    R.Data->SetObjectField(TEXT("expected_tooltip_text"), ExpectedTooltipText); R.Data->SetBoolField(TEXT("expected_tooltip_assigned_from_request"), HasDescription);
    R.Data->SetStringField(TEXT("saved_tooltip_localization_scope"), TEXT("Only the selected request-assigned ToolTip after a returned successful save may gain a localization namespace/GUID key. Retained before-save identity, exact current source/display/flags/base-text export and complete other metadata/node fields must agree. Raw ID/export differences remain; scratch copies only, no text setters/ChangeKey or extra writer/compile/save. Public display inspection may rebuild the text display cache; helper getter/export internals are not counted as additional asset writes."));
    if (!MetadataComplete || !ExpectedTextComplete || !CustomMetadataExpectedNodeFields(Before, N, HasDescription, Text, Flag, Comment, DesiredMetadata, DesiredNodeFields, R))
    { Fail(R, TEXT("custom_event_metadata_expected_state_unobservable"), TEXT("Public serialized metadata/scratch field expectations incomplete; no node edit.")); return R; }
    R.Data->SetObjectField(TEXT("expected_serialized_metadata_fields"), DesiredMetadataFields); R.Data->SetObjectField(TEXT("expected_selected_node_fields"), DesiredNodeFields);
    R.Data->SetStringField(TEXT("selected_node_path"), NodePath); R.Data->SetStringField(TEXT("selected_node_guid"), Guid.ToString()); R.Data->SetStringField(TEXT("selected_function_name"), FunctionName);
    const FScopedTransaction Outer(NSLOCTEXT("CubelessMCP", "CustomEventMetadata", "MCP set_blueprint_custom_event_metadata")); LifecycleCount(R, TEXT("outer_transaction_calls"));
    R.Stage = TEXT("configure_node"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
    LifecycleCount(R, TEXT("explicit_modify_calls")); LifecycleCount(R, TEXT("graph_modify_calls")); G->Modify();
    LifecycleCount(R, TEXT("explicit_modify_calls")); LifecycleCount(R, TEXT("node_modify_calls")); N->Modify();
    TArray<TSharedPtr<FJsonValue>> NodeCalls;
    if (HasDescription)
    {
        auto Call = MakeShared<FJsonObject>(); Call->SetStringField(TEXT("route"), TEXT("GetUserDefinedMetaData.ToolTip/SetMetaData")); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false);
        NodeCalls.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("node_metadata_calls"), NodeCalls);
        LifecycleCount(R, TEXT("node_tooltip_assignments")); N->GetUserDefinedMetaData().ToolTip = FText::FromString(Text);
        LifecycleCount(R, TEXT("node_metadata_set_calls")); N->GetUserDefinedMetaData().SetMetaData(FBlueprintMetadata::MD_Tooltip, Text); Call->SetBoolField(TEXT("returned"), true);
        if (Comment)
        {
            LifecycleCount(R, TEXT("node_comment_assignments")); N->NodeComment = Text;
            LifecycleCount(R, TEXT("node_comment_bubble_assignments")); N->bCommentBubbleVisible = false;
            LifecycleCount(R, TEXT("node_comment_bubble_assignments")); N->bCommentBubbleMakeVisible = false;
            LifecycleCount(R, TEXT("node_comment_bubble_assignments")); N->bCommentBubblePinned = false;
        }
    }
    if (Flag.IsSet())
    {
        LifecycleCount(R, TEXT("node_call_in_editor_assignments")); N->bCallInEditor = Flag.GetValue();
        LifecycleCount(R, TEXT("metadata_call_in_editor_assignments")); N->GetUserDefinedMetaData().bCallInEditor = Flag.GetValue();
    }
    R.Stage = TEXT("reconstruct_node"); LifecycleCount(R, TEXT("reconstruct_node_attempts")); N->ReconstructNode(); LifecycleCount(R, TEXT("reconstruct_node_returns"));
    R.Stage = TEXT("notify_graph"); LifecycleCount(R, TEXT("notify_graph_changed_attempts")); G->NotifyGraphChanged(); LifecycleCount(R, TEXT("notify_graph_changed_returns"));
    R.Stage = TEXT("mark_structural"); LifecycleCount(R, TEXT("explicit_mark_structural_calls")); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); LifecycleCount(R, TEXT("structural_return_count"));
    R.Stage = TEXT("after_configuration"); LifecycleAggregateClear(R);
    if (!CustomMetadataObserved(BP, Configured, Before, R, TEXT("after_configuration"))) { return R; }
    if (!CustomMetadataPreserved(Before, Configured, NodePath, FunctionName, DesiredNodeFields, DesiredMetadataFields, BeforeFunctionMetadata, false, false, N, DesiredMetadata, HasDescription, R))
    { Fail(R, TEXT("custom_event_metadata_configuration_preservation_failed"), TEXT("Known node configuration prefix violated finite nonselected/source/disk proof; later compile/metadata/save suppressed.")); return R; }
    R.Data->SetBoolField(TEXT("configured_state_preserved"), true); const FLifecycleObservation* Final = &Configured;
    bool CompilePassed = true;
    if (Compile)
    {
        LifecycleAggregateClear(R); R.Stage = TEXT("compile"); R.bCompileAttempted = true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("full_compile_attempts"));
        R.Data->SetNumberField(TEXT("source_expected_core_compile_same_asset_load_requests"), 1);
        const auto Receipt = Cubeless::UE58::CompileBlueprint(BP->GetPathName(), false, false); R.Data->SetObjectField(TEXT("compile_result"), Receipt);
        R.Data->SetBoolField(TEXT("compiled"), true); R.Data->SetStringField(TEXT("compile_status"), LifecycleStatus(BP->Status));
        if (!Receipt->TryGetBoolField(TEXT("compiled"), CompilePassed)) { R.Compiled.Reset(); Fail(R, TEXT("custom_event_metadata_compile_outcome_unobservable"), TEXT("Returned compiler result lacks actual compiled outcome; no later writer/retry.")); return R; }
        R.Compiled = CompilePassed; R.Stage = TEXT("after_compile");
        if (!CustomMetadataObserved(BP, Compiled, Before, R, TEXT("after_compile"))) { return R; }
        const auto DesiredFunction = CustomMetadataFunctionDesired(BeforeFunctionMetadata, Text, HasDescription, Flag, CompilePassed, false);
        R.Data->SetObjectField(TEXT("expected_generated_metadata_after_compile"), DesiredFunction);
        if (!CustomMetadataPreserved(Before, Compiled, NodePath, FunctionName, DesiredNodeFields, DesiredMetadataFields, DesiredFunction, CompilePassed, false, N, DesiredMetadata, HasDescription, R))
        { Fail(R, TEXT("custom_event_metadata_compile_preservation_failed"), TEXT("Actual compiler prefix retained; finite preservation failed and later generated metadata/save were suppressed. No ephemeral restore or retry.")); return R; }
        R.Data->SetBoolField(TEXT("compile_state_preserved"), true); Final = &Compiled;
    }
    bool Current = IsValid(N) && IsValid(G) && N->GetGraph() == G && N->GetOuter() == G && G->Nodes.Contains(N) && N->NodeGuid == Guid
        && N->GetPathName() == NodePath && N->CustomFunctionName.ToString() == FunctionName && (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G));
    if (!Current) { Fail(R, TEXT("custom_event_metadata_current_node_changed"), TEXT("Current selected node membership/identity changed after structural/compile work; no later metadata/save.")); return R; }
    UFunction* Function = BP->GeneratedClass->FindFunctionByName(N->CustomFunctionName, EIncludeSuperFlag::ExcludeSuper);
    if (!IsValid(Function) || Function->GetOuter() != BP->GeneratedClass || Function->GetOutermost() != BP->GetOutermost())
    { Fail(R, TEXT("custom_event_metadata_generated_function_unobservable"), TEXT("Current own generated function unavailable; real prefix retained, no function metadata/save.")); return R; }
    TStrongObjectPtr<UFunction> FunctionHold(Function); TArray<TSharedPtr<FJsonValue>> FunctionCalls;
    if (HasDescription)
    {
        R.Stage = TEXT("generated_function_metadata"); R.bChangeStateKnown = false; LifecycleAggregateClear(R);
        LifecycleCount(R, TEXT("explicit_modify_calls")); LifecycleCount(R, TEXT("generated_function_modify_calls")); Function->Modify();
        for (const FName Key : {FBlueprintMetadata::MD_Tooltip, FName(TEXT("ShortTooltip"))})
        {
            auto Call = MakeShared<FJsonObject>(); Call->SetStringField(TEXT("function_path"), Function->GetPathName()); Call->SetStringField(TEXT("key"), Key.ToString());
            Call->SetStringField(TEXT("requested"), Text); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false);
            FunctionCalls.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("generated_function_metadata_calls"), FunctionCalls);
            LifecycleCount(R, TEXT("generated_function_metadata_set_attempts")); Function->SetMetaData(Key, *Text);
            LifecycleCount(R, TEXT("generated_function_metadata_set_returns")); Call->SetBoolField(TEXT("returned"), true); Call->SetStringField(TEXT("observed"), Function->GetMetaData(Key));
        }
        R.Data->SetBoolField(TEXT("generated_function_tooltip_updated"), true); LifecycleCount(R, TEXT("explicit_mark_package_dirty_calls"));
        LifecycleCount(R, TEXT("generated_function_dirty_calls")); BP->MarkPackageDirty();
    }
    R.Data->SetStringField(TEXT("generated_function_tooltip"), Function->GetMetaData(FBlueprintMetadata::MD_Tooltip));
    const auto DesiredFinalFunction = CustomMetadataFunctionDesired(BeforeFunctionMetadata, Text, HasDescription, Flag, Compile && CompilePassed, HasDescription);
    R.Data->SetObjectField(TEXT("expected_generated_metadata_final"), DesiredFinalFunction); R.Stage = TEXT("after_generated_metadata");
    if (!CustomMetadataObserved(BP, Generated, Before, R, TEXT("after_generated_metadata"))) { return R; }
    if (!CustomMetadataPreserved(Before, Generated, NodePath, FunctionName, DesiredNodeFields, DesiredMetadataFields, DesiredFinalFunction, Compile && CompilePassed, false, N, DesiredMetadata, HasDescription, R))
    { Fail(R, TEXT("custom_event_metadata_generated_preservation_failed"), TEXT("Known generated metadata prefix retained; finite final-before-save proof failed, save suppressed.")); return R; }
    R.Data->SetBoolField(TEXT("generated_metadata_verified"), true); Final = &Generated;
    if (Save)
    {
        R.Data->SetObjectField(TEXT("before_save_observation"), R.Data->GetObjectField(TEXT("after_generated_metadata"))); R.Data->SetBoolField(TEXT("before_save_preservation_passed"), true);
        LifecycleAggregateClear(R); R.Stage = TEXT("save"); R.bSaveAttempted = true; R.bChangeStateKnown = false; LifecycleCount(R, TEXT("save_attempt_count"));
        auto Call = MakeShared<FJsonObject>(); Call->SetStringField(TEXT("asset_path"), BP->GetPathName()); Call->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::SaveLoadedAsset"));
        Call->SetBoolField(TEXT("only_if_dirty"), false); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false); Call->SetField(TEXT("saved"), MakeShared<FJsonValueNull>());
        R.Data->SetArrayField(TEXT("save_calls"), {MakeShared<FJsonValueObject>(Call)}); R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(BP, false);
        Call->SetBoolField(TEXT("returned"), true); Call->SetBoolField(TEXT("saved"), R.bSaved); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
        R.Stage = TEXT("after_save"); if (!CustomMetadataObserved(BP, Saved, Before, R, TEXT("after_save"))) { return R; }
        if (!R.bSaved || !CustomMetadataPreserved(Before, Saved, NodePath, FunctionName, DesiredNodeFields, DesiredMetadataFields, DesiredFinalFunction, Compile && CompilePassed, true, N, DesiredMetadata, HasDescription, R))
        { Fail(R, TEXT("custom_event_metadata_save_failed"), TEXT("Actual save bool/file/final-state prefix retained; no second save or writer replay.")); return R; } Final = &Saved;
    }
    R.Data->SetStringField(TEXT("node_id"), N->NodeGuid.ToString()); R.Data->SetStringField(TEXT("node_guid"), N->NodeGuid.ToString());
    R.Data->SetStringField(TEXT("node_name"), N->GetName()); R.Data->SetStringField(TEXT("node_type"), N->GetClass()->GetName());
    R.Data->SetStringField(TEXT("custom_event_name"), N->CustomFunctionName.ToString()); R.Data->SetStringField(TEXT("description"), Text);
    R.Data->SetBoolField(TEXT("call_in_editor"), N->bCallInEditor); R.Data->SetStringField(TEXT("generated_function_tooltip"), Function->GetMetaData(FBlueprintMetadata::MD_Tooltip));
    auto Graph = MakeShared<FJsonObject>(); Graph->SetStringField(TEXT("graph_id"), G->GraphGuid.ToString()); Graph->SetStringField(TEXT("graph_name"), G->GetName());
    Graph->SetStringField(TEXT("graph_type"), TEXT("event")); Graph->SetStringField(TEXT("schema"), G->GetSchema()->GetClass()->GetName()); Graph->SetNumberField(TEXT("node_count"), G->Nodes.Num()); R.Data->SetObjectField(TEXT("graph"), Graph);
    const bool Desired = (!HasDescription || N->GetUserDefinedMetaData().ToolTip.ToString() == Text && N->GetUserDefinedMetaData().GetMetaData(FBlueprintMetadata::MD_Tooltip) == Text
        && Function->GetMetaData(FBlueprintMetadata::MD_Tooltip) == Text && Function->GetMetaData(TEXT("ShortTooltip")) == Text)
        && (!Flag.IsSet() || N->bCallInEditor == Flag.GetValue() && N->GetUserDefinedMetaData().bCallInEditor == Flag.GetValue())
        && (!HasDescription || !Comment || N->NodeComment == Text && !N->bCommentBubbleVisible && !N->bCommentBubbleMakeVisible && !N->bCommentBubblePinned);
    R.Data->SetBoolField(TEXT("desired_metadata_verified"), Desired); R.Data->SetStringField(TEXT("transient_metadata_scope"), TEXT("Declared CPF_Transient HasLatentFunctions is retained raw but excluded from serialized metadata equality; no whole metadata or callback mask."));
    const bool CurrentFinal = G->Nodes.Contains(N) && N->GetGraph() == G && N->NodeGuid == Guid
        && (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G))
        && BP->GeneratedClass->FindFunctionByName(N->CustomFunctionName, EIncludeSuperFlag::ExcludeSuper) == Function;
    R.Data->SetBoolField(TEXT("current_selected_node_and_function_verified"), CurrentFinal);
    R.bPostconditionPassed = Desired && CompilePassed && Final->Complete && CurrentFinal;
    if (!R.bPostconditionPassed) { Fail(R, TEXT("custom_event_metadata_postcondition_failed"), TEXT("Actual ordered metadata/compile/save prefix retained; requested compiler failure or desired state failure is not hidden.")); return R; }
    R.bSuccess = true; R.Stage = TEXT("complete"); return R;
}
}

// CD: original Part2 local declaration; current member declaration APIs stay byte-exact.
#include "K2Node_FunctionEntry.h"
namespace Cubeless::UE58::Variable
{
namespace
{

// IL: only the new scalar UserDefinedEnum declaration routes use this local scope.

void LocalInit(FResult& R)
{
    LifecycleInit(R);
    for (const TCHAR* Key : { TEXT("local_variable_add_attempts"), TEXT("local_variable_add_returns"), TEXT("local_variable_add_successes"),
        TEXT("local_category_descriptor_set_calls"), TEXT("local_tooltip_descriptor_set_calls"), TEXT("local_structural_return_count"),
        TEXT("local_declaration_observation_calls"), TEXT("local_node_field_observation_calls"), TEXT("local_expected_export_calls"),
        TEXT("explicit_graph_create_calls"), TEXT("explicit_asset_load_calls") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("local_variable_calls"), {}); R.Data->SetArrayField(TEXT("local_descriptor_calls"), {});
    R.Data->SetBoolField(TEXT("local_variable_declared"), false); R.Data->SetBoolField(TEXT("local_declarations_preserved"), false);
    R.Data->SetBoolField(TEXT("local_node_fields_preserved"), false); R.Data->SetBoolField(TEXT("scs_preserved"), false);
    R.Data->SetBoolField(TEXT("runtime_or_cold_local_layout_verified"), false); R.Data->SetBoolField(TEXT("original_public_Any_wire_verified"), false);
    R.Data->SetStringField(TEXT("admission_scope"), TEXT("One new ordinary scalar declaration in an existing exact own editable user function graph of a direct native Actor Blueprint. Existing members/local descriptors remain intact. Arrays/reference/complex types, graph creation, ambiguous/override/signature/custom entries and legacy loose default coercions remain incomplete."));
    R.Data->SetStringField(TEXT("counter_scope"), TEXT("Explicit adapter dispatch/application/return counts only. Public AddLocalVariable internally requests one transaction, Blueprint/Entry Modify and structural refresh; caller requests one further structural refresh. Those source-derived requests are separate from actual uninstrumented skeleton compilation/notification/cache counts."));
    R.Data->SetStringField(TEXT("transaction_scope"), TEXT("One original outer transaction after finite admission, plus public helper's own internal transaction. Original pre-handler transaction on earlier refusals is not reproduced. No Python journal is created by Native."));
    R.Data->SetStringField(TEXT("observation_scope"), TEXT("Bounded current member descriptors/CDO, all graph nodes/pins and nontransient graph/node fields, own function-local descriptors, selected SCS, native Actor source, serialized references, target disk and other loaded package dirtiness after initial load. Current generated local layout/runtime and implicit load/global callback effects are not inferred."));
    R.Data->SetNumberField(TEXT("source_expected_helper_transaction_requests"), 1);
    R.Data->SetNumberField(TEXT("source_expected_helper_blueprint_modify_requests"), 1);
    R.Data->SetNumberField(TEXT("source_expected_helper_entry_modify_requests"), 1);
    R.Data->SetNumberField(TEXT("source_expected_helper_structural_requests"), 1);
    R.Data->SetNumberField(TEXT("source_expected_total_structural_requests_on_success"), 2);
}
FString LocalGraphKind(UBlueprint* BP, UEdGraph* G)
{
    if (BP->FunctionGraphs.Contains(G)) { return TEXT("function"); }
    if (BP->MacroGraphs.Contains(G)) { return TEXT("macro"); }
    if (BP->UbergraphPages.Contains(G) || BP->EventGraphs.Contains(G)) { return TEXT("event"); }
    if (BP->DelegateSignatureGraphs.Contains(G)) { return TEXT("delegate"); }
    return TEXT("unknown");
}
FString LocalGraphType(const FString& Type)
{
    FString T = Type.ToLower(); T.ReplaceInline(TEXT(" "), TEXT("")); T.ReplaceInline(TEXT("-"), TEXT("_"));
    if (T == TEXT("eventgraph") || T == TEXT("event_graph") || T == TEXT("ubergraph") || T == TEXT("uber_graph")) { return TEXT("event"); }
    if (T == TEXT("functiongraph") || T == TEXT("function_graph")) { return TEXT("function"); }
    if (T == TEXT("macrograph") || T == TEXT("macro_graph")) { return TEXT("macro"); }
    return T;
}
TSharedRef<FJsonObject> LocalGraphJson(UBlueprint* BP, UEdGraph* G)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("graph_id"), G->GraphGuid.ToString());
    J->SetStringField(TEXT("graph_name"), G->GetName()); J->SetStringField(TEXT("graph_type"), LocalGraphKind(BP, G));
    J->SetStringField(TEXT("schema"), G->GetSchema() ? G->GetSchema()->GetClass()->GetName() : FString());
    J->SetNumberField(TEXT("node_count"), G->Nodes.Num()); return J;
}
TSharedRef<FJsonObject> LocalPinJson(const FEdGraphPinType& Type)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("category"), Type.PinCategory.ToString());
    J->SetStringField(TEXT("subcategory"), Type.PinSubCategory.ToString()); J->SetBoolField(TEXT("is_array"), Type.IsArray());
    if (Type.PinSubCategoryObject.IsValid()) { J->SetStringField(TEXT("subcategory_object"), Type.PinSubCategoryObject->GetPathName()); }
    return J;
}
TSharedRef<FJsonObject> LocalDescriptor(const FBPVariableDescription& V, int32 Index)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("index"), Index); J->SetStringField(TEXT("name"), V.VarName.ToString());
    J->SetStringField(TEXT("guid"), V.VarGuid.ToString(EGuidFormats::Digits)); J->SetObjectField(TEXT("pin_type"), LocalPinJson(V.VarType));
    J->SetBoolField(TEXT("is_reference"), V.VarType.bIsReference); J->SetBoolField(TEXT("is_const"), V.VarType.bIsConst);
    J->SetStringField(TEXT("friendly_name"), V.FriendlyName); J->SetStringField(TEXT("category"), V.Category.ToString());
    J->SetStringField(TEXT("property_flags_string"), LexToString(V.PropertyFlags)); J->SetStringField(TEXT("rep_notify"), V.RepNotifyFunc.ToString());
    J->SetNumberField(TEXT("replication_condition"), static_cast<int32>(V.ReplicationCondition));
    J->SetObjectField(TEXT("metadata"), Metadata(V)); J->SetStringField(TEXT("stored_default"), V.DefaultValue); return J;
}
bool LocalDefault(const FString& Json, const FEdGraphPinType& Type, FString& Stored, FResult& R)
{
    TSharedPtr<FJsonValue> V; if (!Parse(Json, V, R)) { return false; } Stored.Reset();
    if (V->Type == EJson::Null) { R.Data->SetBoolField(TEXT("default_omitted_original_None"), true); return true; }
    R.Data->SetBoolField(TEXT("default_omitted_original_None"), false);
    const FName C = Type.PinCategory;
    if(C==UEdGraphSchema_K2::PC_Struct&&(Type.PinSubCategoryObject.Get()==TBaseStructure<FVector>::Get()||Type.PinSubCategoryObject.Get()==TBaseStructure<FRotator>::Get()))
    {
        const bool Vector=Type.PinSubCategoryObject.Get()==TBaseStructure<FVector>::Get();
        if(V->Type==EJson::Array||V->Type==EJson::Object)
        {
            double Axes[3]={0,0,0};int32 Fallbacks=0;
            for(int32 Axis=0;Axis<3;++Axis)
            {
                if(V->Type==EJson::Array){const auto& Values=V->AsArray();if(Values.IsValidIndex(Axis)&&!Values[Axis]->TryGetNumber(Axes[Axis])){Axes[Axis]=0;++Fallbacks;}}
                else{const TCHAR* Key=Vector?(Axis==0?TEXT("x"):Axis==1?TEXT("y"):TEXT("z")):(Axis==0?TEXT("pitch"):Axis==1?TEXT("yaw"):TEXT("roll"));const auto Value=V->AsObject()->TryGetField(Key);if(Value.IsValid()&&!Value->TryGetNumber(Axes[Axis])){Axes[Axis]=0;++Fallbacks;}}
                if(!FMath::IsFinite(Axes[Axis])||FMath::Abs(Axes[Axis])>9007199254740991.0){return Fail(R,TEXT("local_variable_struct_axis_unsupported"),TEXT("Converted axes must remain finite within the existing JSON exact numeric bound; unsafe loose conversions are not passed to the compiler."));}
            }
            Stored=Vector?FString::Printf(TEXT("(X=%s,Y=%s,Z=%s)"),*FString::SanitizeFloat(Axes[0]),*FString::SanitizeFloat(Axes[1]),*FString::SanitizeFloat(Axes[2])):FString::Printf(TEXT("(Pitch=%s,Yaw=%s,Roll=%s)"),*FString::SanitizeFloat(Axes[0]),*FString::SanitizeFloat(Axes[1]),*FString::SanitizeFloat(Axes[2]));
            R.Data->SetNumberField(TEXT("struct_nonnumeric_axes_defaulted_zero"),Fallbacks);R.Data->SetStringField(TEXT("struct_default_representation"),V->Type==EJson::Array?TEXT("first_three_array_axes"):TEXT("lowercase_axis_object"));return true;
        }
        if(V->Type==EJson::String){Stored=V->AsString();}
        else if(V->Type==EJson::Boolean){Stored=V->AsBool()?TEXT("true"):TEXT("false");}
        else if(V->Type==EJson::Number){Stored=FString::SanitizeFloat(V->AsNumber());}
        else{return Fail(R,TEXT("local_variable_default_unsupported"),TEXT("This built-in local supports original bounded array/object/string or scalar fallback default storage."));}
        R.Data->SetStringField(TEXT("struct_default_representation"),TEXT("original_literal_storage_no_parse_or_evaluation"));return true;
    }
    if (C == UEdGraphSchema_K2::PC_Boolean)
    {
        if (V->Type != EJson::Boolean) { return Fail(R, TEXT("local_variable_default_invalid"), TEXT("Boolean default values must be JSON booleans.")); }
        Stored = V->AsBool() ? TEXT("true") : TEXT("false"); return true;
    }
    if (C == UEdGraphSchema_K2::PC_Int || C == UEdGraphSchema_K2::PC_Int64)
    {
        if (V->Type != EJson::Number || !FMath::IsFinite(V->AsNumber()) || FMath::Abs(V->AsNumber()) > 9007199254740991.0
            || V->AsNumber() != FMath::FloorToDouble(V->AsNumber())
            || (C == UEdGraphSchema_K2::PC_Int && (V->AsNumber() < MIN_int32 || V->AsNumber() > MAX_int32)))
        { return Fail(R, TEXT("local_variable_default_unsupported"), TEXT("First integer slice requires finite integral JSON numbers within +/-9007199254740991 (int32 range for int); larger original int64 and loose/string/fractional variants remain incomplete.")); }
        Stored = FString::Printf(TEXT("%lld"), static_cast<int64>(V->AsNumber())); return true;
    }
    if (C == UEdGraphSchema_K2::PC_Real)
    {
        if (V->Type != EJson::Number) { return Fail(R, TEXT("local_variable_default_unsupported"), TEXT("First real slice requires a finite JSON number; original string/coercion variants remain incomplete.")); }
        Stored = FString::SanitizeFloat(V->AsNumber()); return true;
    }
    if (C == UEdGraphSchema_K2::PC_String || C == UEdGraphSchema_K2::PC_Name || C == UEdGraphSchema_K2::PC_Text)
    {
        if (V->Type != EJson::String) { return Fail(R, TEXT("local_variable_default_unsupported"), TEXT("First string/name/text slice requires a JSON string; original numeric/object conversion variants remain incomplete.")); }
        Stored = V->AsString(); return true;
    }
    return Fail(R, TEXT("local_variable_type_unsupported"), TEXT("First local-variable defaults preserve ordinary scalars only."));
}
UBlueprint* LocalLoad(const FString& Path, FResult& R, FDeclarationEnumScope* EnumScope = nullptr)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("local_variable_editor_state_unsupported"), TEXT("Editor game thread outside PIE/SIE required.")); return nullptr; }
    if (!SafeName(Path) || !Path.StartsWith(TEXT("/Game/"))) { Fail(R, TEXT("local_variable_blueprint_path_invalid"), TEXT("Provide an exact bounded /Game Blueprint package/object path.")); return nullptr; }
    FString AssetPath = Path; if (!Path.Contains(TEXT("."))) { AssetPath += TEXT(".") + FPackageName::GetShortName(Path); }
    if (!SafeName(AssetPath)) { Fail(R, TEXT("local_variable_blueprint_path_invalid"), TEXT("Completed object path exceeds safe name bounds.")); return nullptr; }
    const bool Loaded = FindObject<UBlueprint>(nullptr, *AssetPath) != nullptr;
    R.Data->SetBoolField(TEXT("loaded_during_operation"), !Loaded); R.Data->SetBoolField(TEXT("implicit_postload_work_possible"), !Loaded);
    R.Data->SetStringField(TEXT("load_scope"), TEXT("Observational baseline begins after this one explicit load; implicit PostLoad/CDO/cache effects are not measured as zero."));
    LifecycleCount(R, TEXT("explicit_asset_load_calls")); if(EnumScope&&(EnumScope->NativeClassMode||EnumScope->NativeStructMode||EnumScope->LocalMapMode)) { EnumScope->LoadEntered=true; } UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if(EnumScope&&!EnumScope->BindBlueprint(BP)) { return nullptr; }
    if (!IsValid(BP)) { R.bChangeStateKnown = Loaded; Fail(R, TEXT("local_variable_blueprint_not_found"), TEXT("Blueprint load failed; possible load effects remain separately unverified.")); return nullptr; }
    if (BP->NewVariables.Num() + BP->GeneratedVariables.Num() > MaxVariables) { Fail(R, TEXT("local_variable_member_budget"), TEXT("Existing member descriptor budget exceeded.")); return nullptr; }
    return BP;
}
UEdGraph* LocalSelect(UBlueprint* BP, const FString& Name, const FString& Id, const FString& Type, bool Create, FResult& R, bool AllowMissingFunction=false)
{
    const FString Kind = LocalGraphType(Type); FGuid Guid;
    if (!Id.IsEmpty() && (!FGuid::Parse(Id, Guid) || !Guid.IsValid())) { Fail(R, TEXT("local_variable_graph_id_invalid"), TEXT("graph_id must select one existing nonzero GUID.")); return nullptr; }
    TArray<UEdGraph*> All; BP->GetAllGraphs(All); if (All.Num() > 128) { Fail(R, TEXT("local_variable_graph_budget"), TEXT("Graph selection exceeds 128 own graphs.")); return nullptr; }
    UEdGraph* Found = nullptr;
    for (UEdGraph* G : All)
    {
        if (!IsValid(G) || (!Name.IsEmpty() && !G->GetName().Equals(Name, ESearchCase::IgnoreCase))
            || (!Id.IsEmpty() && G->GraphGuid != Guid) || (!Kind.IsEmpty() && Kind != TEXT("any") && LocalGraphKind(BP, G) != Kind)) { continue; }
        if (Found) { Fail(R, TEXT("local_variable_graph_ambiguous"), TEXT("Supply a graph name/GUID; ambiguous first-match selection remains incomplete.")); return nullptr; }
        Found = G;
    }
    if (!Found)
    {
        if(Create&&AllowMissingFunction)return nullptr; // Expected selection result, not an engine failure/retry.
        Fail(R, Create ? TEXT("local_variable_graph_creation_unsupported") : TEXT("local_variable_graph_not_found"),
            Create ? TEXT("Missing-function graph creation is an original valid feature outside the first finite slice; no graph writer dispatched.")
                : FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_id='%s', graph_type='%s'"), *Name, *Id, *Type));
        return nullptr;
    }
    if (LocalGraphKind(BP, Found) != TEXT("function")) { Fail(R, TEXT("local_variable_graph_type_invalid"), TEXT("Local variables are only supported in function graphs.")); return nullptr; }
    return Found;
}
bool LocalAdmission(UBlueprint* BP, UEdGraph* Graph, UK2Node_FunctionEntry*& Entry, FResult& R)
{
    TArray<UK2Node_FunctionEntry*> Entries; Graph->GetNodesOfClass(Entries);
    if (!BP->FunctionGraphs.Contains(Graph) || Graph->GetOuter() != BP || !Graph->GraphGuid.IsValid()
        || Graph->GetClass() != UEdGraph::StaticClass() || !Graph->GetSchema() || Graph->GetSchema()->GetClass() != UEdGraphSchema_K2::StaticClass()
        || FBlueprintEditorUtils::GetTopLevelGraph(Graph) != Graph || FBlueprintEditorUtils::FindBlueprintForGraph(Graph) != BP
        || !FBlueprintEditorUtils::DoesSupportLocalVariables(Graph) || Entries.Num() != 1 || !IsValid(Entries[0])
        || Entries[0]->GetClass() != UK2Node_FunctionEntry::StaticClass() || Entries[0]->GetGraph() != Graph || !Entries[0]->IsEditable()
        || !Entries[0]->CustomGeneratedFunctionName.IsNone() || Entries[0]->FunctionReference.GetMemberName() != Graph->GetFName()
        || Entries[0]->FunctionReference.GetMemberParentClass() != nullptr || Entries[0]->LocalVariables.Num() > 255)
    { return Fail(R, TEXT("local_variable_function_scope_unsupported"), TEXT("Requires one exact editable own top-level user function Entry, matching graph name and no inherited/signature/custom generated function; local declaration budget is 256.")); }
    Entry = Entries[0];
    FArrayProperty* Local = FindFProperty<FArrayProperty>(Entry->GetClass(), TEXT("LocalVariables"));
    FStructProperty* Inner = Local ? CastField<FStructProperty>(Local->Inner) : nullptr;
    if (!Local || Local->ArrayDim != 1 || !Inner || Inner->Struct != FBPVariableDescription::StaticStruct()
        || Local->ContainerPtrToValuePtr<void>(Entry) != &Entry->LocalVariables)
    { return Fail(R, TEXT("local_variable_storage_unobservable"), TEXT("Current public LocalVariables property must describe the exact native descriptor array storage.")); }
    TSet<FName> Names; TSet<FGuid> Guids;
    for (const auto& V : Entry->LocalVariables)
    {
        if (V.VarName.IsNone() || !V.VarGuid.IsValid() || Names.Contains(V.VarName) || Guids.Contains(V.VarGuid)
            || !SafeText(V.DefaultValue, 65536) || !SafeText(V.Category.ToString(), 4096) || V.MetaDataArray.Num() > 64)
        { return Fail(R, TEXT("local_variable_existing_declaration_invalid"), TEXT("Existing local descriptors need unique names/GUIDs and bounded public fields; no declaration write.")); }
        Names.Add(V.VarName); Guids.Add(V.VarGuid);
    }
    return true;
}
bool LocalCapture(UBlueprint* BP, FLifecycleObservation& O, FResult& R, const TCHAR* Phase, FDeclarationEnumScope* EnumScope = nullptr)
{
    const bool Base = LifecycleCapture(BP, O, R, Phase, EnumScope); const auto Receipt = R.Data->GetObjectField(Phase); Receipt->SetBoolField(TEXT("complete"), false); O.Complete = false;
    auto Fields = MakeShared<FJsonObject>(), Locals = MakeShared<FJsonObject>(), SCS = MakeShared<FJsonObject>();
    O.State->SetObjectField(TEXT("graph_node_fields"), Fields); O.State->SetObjectField(TEXT("function_local_declarations"), Locals); O.State->SetObjectField(TEXT("scs"), SCS);
    if (!Base || (EnumScope&&!EnumScope->Admit())) { return false; }
    Receipt->SetStringField(TEXT("failed_stage"), TEXT("graph_node_fields")); TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); int32 Count = 0;
    for (UEdGraph* G : Graphs)
    {
        auto GFields = MakeShared<FJsonObject>(); Fields->SetObjectField(G->GetPathName(), GFields);
        LifecycleCount(R, TEXT("local_node_field_observation_calls")); if (!LifecycleFields(G, GFields, EnumScope)) { return false; }
        for (UEdGraphNode* N : G->Nodes)
        {
            if (++Count > 8192) { return false; } auto NFields = MakeShared<FJsonObject>(); Fields->SetObjectField(N->GetPathName(), NFields);
            LifecycleCount(R, TEXT("local_node_field_observation_calls")); if (!LifecycleFields(N, NFields, EnumScope)) { return false; }
            if (const auto Function = Cast<UK2Node_FunctionEntry>(N))
            {
                if (Function->LocalVariables.Num() > 256) { return false; } TArray<TSharedPtr<FJsonValue>> Rows;
                Locals->SetArrayField(N->GetPathName(), Rows); LifecycleCount(R, TEXT("local_declaration_observation_calls"));
                for (int32 I = 0; I < Function->LocalVariables.Num(); ++I) { Rows.Add(MakeShared<FJsonValueObject>(LocalDescriptor(Function->LocalVariables[I], I))); Locals->SetArrayField(N->GetPathName(), Rows); }
            }
        }
    }
    Receipt->SetStringField(TEXT("failed_stage"), TEXT("scs")); if (!RemovalSCS(BP, SCS, EnumScope)) { return false; }
    O.Complete = true; Receipt->SetBoolField(TEXT("complete"), true); Receipt->SetStringField(TEXT("failed_stage"), TEXT("")); return true;
}
bool LocalNodeFieldsSame(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B, const FString& EntryPath, const FString& ExpectedExport)
{
    if (A->Values.Num() != B->Values.Num()) { return false; }
    for (const auto& Pair : A->Values)
    {
        const auto Found = B->TryGetField(Pair.Key.ToView()); if (!Found || Pair.Value->Type != EJson::Object || Found->Type != EJson::Object) { return false; }
        const auto X = Pair.Value->AsObject(), Y = Found->AsObject();
        if (FString(Pair.Key.ToView()) != EntryPath) { if (!LifecycleEqual(Pair.Value, Found)) { return false; } continue; }
        if (X->Values.Num() != Y->Values.Num()) { return false; }
        for (const auto& Field : X->Values)
        {
            const auto Now = Y->TryGetField(Field.Key.ToView()); if (!Now) { return false; }
            if (Field.Key.ToView() != TEXT("LocalVariables")) { if (!LifecycleEqual(Field.Value, Now)) { return false; } continue; }
            if (Field.Value->Type != EJson::Object || Now->Type != EJson::Object) { return false; }
            const auto BeforeRow = Field.Value->AsObject(), AfterRow = Now->AsObject();
            if (BeforeRow->Values.Num() != AfterRow->Values.Num()) { return false; }
            for (const auto& P : BeforeRow->Values)
            {
                const auto Q = AfterRow->TryGetField(P.Key.ToView()); if (!Q) { return false; }
                if (P.Key.ToView() == TEXT("export_text")) { if (Q->Type != EJson::String || Q->AsString() != ExpectedExport) { return false; } }
                else if (!LifecycleEqual(P.Value, Q)) { return false; }
            }
        }
    }
    return true;
}
bool LocalEnumReferencesSame(const FLifecycleObservation& Before,const FLifecycleObservation& After,const FString& EntryPath,const FName LocalName,FDeclarationEnumScope& EnumScope)
{
    // Report only the first cached reference mismatch; no failure-path engine probe.
    const auto Mismatch=[&](const TCHAR* Why,const FString& Owner,const TSharedPtr<FJsonValue>& Expected,const TSharedPtr<FJsonValue>& Actual)
    {
        if(!EnumScope.Reply->Data->HasField(TEXT("local_reference_first_mismatch")))
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),TEXT("local_reference_postcondition"));J->SetStringField(TEXT("reason"),Why);J->SetStringField(TEXT("owner"),Owner);
            J->SetStringField(TEXT("source_path"),EnumScope.SourcePath);J->SetNumberField(TEXT("captured_source_flags"),uint32(EnumScope.SourceFlags));
            if(Expected.IsValid()){J->SetField(TEXT("expected"),Expected);}else{J->SetField(TEXT("expected"),MakeShared<FJsonValueNull>());}
            if(Actual.IsValid()){J->SetField(TEXT("actual"),Actual);}else{J->SetField(TEXT("actual"),MakeShared<FJsonValueNull>());}
            if(Expected.IsValid()&&Actual.IsValid()&&Expected->Type==EJson::Array&&Actual->Type==EJson::Array)
            {
                const auto& E=Expected->AsArray();const auto& A=Actual->AsArray();
                for(int32 I=0;I<FMath::Max(E.Num(),A.Num());++I)
                {
                    if(I<E.Num()&&I<A.Num()&&LifecycleEqual(E[I],A[I])){continue;}J->SetNumberField(TEXT("first_index"),I);
                    if(I<E.Num()){J->SetField(TEXT("first_expected"),E[I]);}else{J->SetField(TEXT("first_expected"),MakeShared<FJsonValueNull>());}
                    if(I<A.Num()){J->SetField(TEXT("first_actual"),A[I]);}else{J->SetField(TEXT("first_actual"),MakeShared<FJsonValueNull>());}break;
                }
            }
            EnumScope.Reply->Data->SetObjectField(TEXT("local_reference_first_mismatch"),J);
        }
        return false;
    };
    if(!EnumScope.Admit()){return false;}
    if(Before.References->Values.Num()!=After.References->Values.Num())
    {return Mismatch(TEXT("reference_owner_count_changed"),TEXT(""),MakeShared<FJsonValueNumber>(Before.References->Values.Num()),MakeShared<FJsonValueNumber>(After.References->Values.Num()));}
    bool NativeStructReferenceFiltered=false;
    if(EnumScope.NativeStructMode)
    {
        UScriptStruct* S=EnumScope.NativeStructSource.Get();
        const bool ExactSource=(EnumScope.ScalarUds.IsValid()&&S==EnumScope.ScalarUds->Type()&&EnumScope.ScalarUds->PlainCurrent())||(S&&((S==TBaseStructure<FVector>::Get()&&EnumScope.SourcePath==TEXT("/Script/CoreUObject.Vector"))||(S==TBaseStructure<FRotator>::Get()&&EnumScope.SourcePath==TEXT("/Script/CoreUObject.Rotator"))||(EnumScope.NativeTransformMode&&S==TBaseStructure<FTransform>::Get()&&EnumScope.SourcePath==TEXT("/Script/CoreUObject.Transform")))
            &&S->GetFlags()==EnumScope.SourceFlags&&S->HasAllFlags(RF_Public|RF_Transient)&&S->GetClass()==UScriptStruct::StaticClass()&&S->GetOuter()==EnumScope.SourcePackage.Get()
            &&EnumScope.SourcePackage->GetName()==TEXT("/Script/CoreUObject")&&EnumScope.SourcePackage->HasAnyPackageFlags(PKG_CompiledIn)&&(S->StructFlags&STRUCT_Native));
        UK2Node_FunctionEntry* Entry=nullptr;
        for(const auto& Role:EnumScope.Editable)
        {
            if(Role.Path!=EntryPath){continue;}
            if(Entry||Role.Class!=UK2Node_FunctionEntry::StaticClass()){return Mismatch(TEXT("selected_struct_entry_role_invalid"),EntryPath,nullptr,nullptr);}
            Entry=Cast<UK2Node_FunctionEntry>(Role.Hold.Get());
        }
        const FBPVariableDescription* Selected=nullptr;FString SelectedGuid;
        if(Entry)
        {
            for(const auto& V:Entry->LocalVariables)
            {if(V.VarName==LocalName){if(Selected){return Mismatch(TEXT("selected_struct_local_ambiguous"),EntryPath,nullptr,nullptr);}Selected=&V;}}
        }
        FEdGraphPinType ExpectedType;ExpectedType.PinCategory=UEdGraphSchema_K2::PC_Struct;ExpectedType.PinSubCategoryObject=S;bool StructArray=false;EnumScope.Reply->Data->TryGetBoolField(TEXT("native_value_struct_array_local_declaration"),StructArray);if(StructArray)ExpectedType.ContainerType=EPinContainerType::Array;
        if(!ExactSource||!Entry||!Selected||!Selected->VarGuid.IsValid()||!EnumScope.Reply->Data->TryGetStringField(TEXT("selected_variable_guid"),SelectedGuid)
            ||Selected->VarGuid.ToString(EGuidFormats::Digits)!=SelectedGuid||Selected->VarType!=ExpectedType)
        {return Mismatch(TEXT("selected_native_struct_reference_proof_incomplete"),EntryPath,nullptr,nullptr);}
        // SDK: Entry subtype is weak; compiled FProperty references exclude native/
        // disregard-for-GC sources. RF_Transient is captured identity, not that filter.
        NativeStructReferenceFiltered=S->HasAnyInternalFlags(EInternalObjectFlags::Native)||GUObjectArray.IsDisregardForGC(S);
        EnumScope.Reply->Data->SetStringField(TEXT("local_struct_reference_expectation"),NativeStructReferenceFiltered?TEXT("exact_before_map_native_property_reference_filter"):TEXT("selected_source_reference_required"));
    }
    // LocalVariables.VarType subtype is weak; preserve every existing Entry reference.
    // Only verified compiled FProperty owners below may introduce a strong source.
    TSet<FString> Allowed,ClassFunctions;
    // Only the selected function's exact typed local field can explain a new source reference.
    for(UClass* Class:{EnumScope.Generated.Get(),EnumScope.Skeleton.Get()})
    {
        UFunction* Function=Class->FindFunctionByName(EnumScope.Graph->GetFName(),EIncludeSuperFlag::ExcludeSuper);
        if(!Function){continue;}
        if(EnumScope.NativeStructMode)
        {
            const FProperty* OwnField=FindFProperty<FProperty>(Function,LocalName);bool StructArray=false;EnumScope.Reply->Data->TryGetBoolField(TEXT("native_value_struct_array_local_declaration"),StructArray);
            const FArrayProperty* ArrayField=StructArray?CastField<FArrayProperty>(OwnField):nullptr;const FStructProperty* Field=StructArray?(ArrayField?CastField<FStructProperty>(ArrayField->Inner):nullptr):CastField<FStructProperty>(OwnField);
            if(OwnField&&OwnField->GetOwnerVariant()==FFieldVariant(Function)&&Field&&Field->GetOwnerStruct()==Function&&(!StructArray||(Field->GetOwnerVariant()==FFieldVariant(ArrayField)&&Field->ArrayDim==1))&&Field->Struct.IsResolved()&&Field->Struct==EnumScope.NativeStructSource.Get()){Allowed.Add(Function->GetPathName());}
        }
        else if(EnumScope.NativeClassMode)
        {
            const FProperty* OwnField=FindFProperty<FProperty>(Function,LocalName);bool ReferencedArray=false;EnumScope.Reply->Data->TryGetBoolField(TEXT("native_referenced_array_local_declaration"),ReferencedArray);const FArrayProperty* ArrayField=ReferencedArray?CastField<FArrayProperty>(OwnField):nullptr;const FProperty* Field=ReferencedArray?(ArrayField?ArrayField->Inner:nullptr):OwnField;
            if(ReferencedArray&&(!OwnField||OwnField->GetOwnerVariant()!=FFieldVariant(Function)||!Field||Field->GetOwnerVariant()!=FFieldVariant(ArrayField)||Field->GetOwnerStruct()!=Function||Field->ArrayDim!=1))continue;
            if(const FClassProperty* ClassField=CastField<FClassProperty>(Field))
            {
                if(ClassField->GetOwnerStruct()==Function&&ClassField->MetaClass==EnumScope.NativeSource.Get()&&ClassField->PropertyClass==UClass::StaticClass()){Allowed.Add(Function->GetPathName());ClassFunctions.Add(Function->GetPathName());}
            }
            else if(const FObjectProperty* ObjectField=CastField<FObjectProperty>(Field))
            {if(ObjectField->GetOwnerStruct()==Function&&ObjectField->PropertyClass==EnumScope.NativeSource.Get()){Allowed.Add(Function->GetPathName());}}
        }
        else
        {
            const FProperty* OwnField=FindFProperty<FProperty>(Function,LocalName);bool ReferencedArray=false;EnumScope.Reply->Data->TryGetBoolField(TEXT("native_referenced_array_local_declaration"),ReferencedArray);const FArrayProperty* ArrayField=ReferencedArray?CastField<FArrayProperty>(OwnField):nullptr;const FByteProperty* Field=ReferencedArray?(ArrayField?CastField<FByteProperty>(ArrayField->Inner):nullptr):CastField<FByteProperty>(OwnField);
            if(ReferencedArray&&(!OwnField||OwnField->GetOwnerVariant()!=FFieldVariant(Function)||!Field||Field->GetOwnerVariant()!=FFieldVariant(ArrayField)||Field->GetOwnerStruct()!=Function||Field->ArrayDim!=1))continue;
            if(Field&&Field->GetOwnerStruct()==Function&&Field->Enum==EnumScope.Source.Get()){Allowed.Add(Function->GetPathName());}
        }
    }
    for(const auto& Pair:Before.References->Values)
    {
        const FString Owner(Pair.Key.ToView());const auto Now=After.References->TryGetField(Pair.Key.ToView());
        if(!Now||Pair.Value->Type!=EJson::Array||Now->Type!=EJson::Array){return Mismatch(TEXT("reference_owner_missing_or_not_array"),Owner,Pair.Value,Now);}
        if(!Allowed.Contains(Owner)||NativeStructReferenceFiltered)
        {if(!LifecycleEqual(Pair.Value,Now)){return Mismatch(TEXT("unselected_or_filtered_reference_changed"),Owner,Pair.Value,Now);}continue;}
        TArray<TSharedPtr<FJsonValue>> Expected=Pair.Value->AsArray();TArray<FString> AddedPaths{EnumScope.SourcePath};if(ClassFunctions.Contains(Owner)){AddedPaths.Add(UClass::StaticClass()->GetPathName());}
        for(const auto& Path:AddedPaths)
        {
            bool Had=false;for(const auto& Ref:Expected){if(Ref->Type!=EJson::String){return Mismatch(TEXT("reference_path_not_string"),Owner,Pair.Value,Now);}Had|=Ref->AsString()==Path;}
            if(!Had){Expected.Add(MakeShared<FJsonValueString>(Path));}
        }
        Expected.Sort([](const auto& A,const auto& B){return A->AsString()<B->AsString();});
        // Retain every pre-existing path. Only a collector-visible exact source is added.
        const TSharedPtr<FJsonValue> ExpectedValue=MakeShared<FJsonValueArray>(Expected);
        if(!LifecycleEqual(ExpectedValue,Now)){return Mismatch(TEXT("selected_reference_paths_changed"),Owner,ExpectedValue,Now);}
    }
    return EnumScope.Admit();
}

bool LocalPreserved(const FLifecycleObservation& Before, const FLifecycleObservation& After, const FString& EntryPath,
    const TArray<TSharedPtr<FJsonValue>>& ExpectedRows, const FString& ExpectedExport, FResult& R, FDeclarationEnumScope* EnumScope = nullptr, FName LocalName = NAME_None)
{
    const auto A = Before.State->GetObjectField(TEXT("variables")), B = After.State->GetObjectField(TEXT("variables")); TArray<FName> Order;
    for (const auto& V : A->GetArrayField(TEXT("new_variables"))) { Order.Add(FName(*V->AsObject()->GetStringField(TEXT("name")))); }
    const auto OldLocals = Before.State->GetObjectField(TEXT("function_local_declarations")), NewLocals = After.State->GetObjectField(TEXT("function_local_declarations"));
    bool Locals = OldLocals->Values.Num() == NewLocals->Values.Num() && OldLocals->HasField(EntryPath) && NewLocals->HasField(EntryPath);
    for (const auto& Pair : OldLocals->Values)
    {
        const auto Now = NewLocals->TryGetField(Pair.Key.ToView());
        Locals &= FString(Pair.Key.ToView()) == EntryPath ? LifecycleEqual(MakeShared<FJsonValueArray>(ExpectedRows), Now) : LifecycleEqual(Pair.Value, Now);
    }
    const bool NodeFields = LocalNodeFieldsSame(Before.State->GetObjectField(TEXT("graph_node_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("graph_node_fields")).ToSharedRef(), EntryPath, ExpectedExport);
    const bool SCS = LifecycleEqual(Before.State->GetObjectField(TEXT("scs")).ToSharedRef(), After.State->GetObjectField(TEXT("scs")).ToSharedRef());
    const bool State = Before.Complete && After.Complete && Locals && NodeFields && SCS
        && LifecycleRowsSame(A->GetArrayField(TEXT("new_variables")), B->GetArrayField(TEXT("new_variables")), Order, nullptr, false)
        && LifecycleEqual(A->TryGetField(TEXT("generated_variables")), B->TryGetField(TEXT("generated_variables")))
        && LifecycleEqual(A->TryGetField(TEXT("category_sorting")), B->TryGetField(TEXT("category_sorting")))
        && A->GetStringField(TEXT("generated_cdo_path")) == B->GetStringField(TEXT("generated_cdo_path"))
        && LifecycleEqual(Before.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_fields")).ToSharedRef())
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("blueprint_identity")).ToSharedRef(), false, false)
        && LifecycleIdentitySame(Before.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), After.State->GetObjectField(TEXT("cdo_identity")).ToSharedRef(), false, false)
        && LifecycleGraphSame(Before.State->GetObjectField(TEXT("graphs")).ToSharedRef(), After.State->GetObjectField(TEXT("graphs")).ToSharedRef(), false);
    const bool Source = LifecycleEqual(Before.Source, After.Source), Refs = EnumScope && !EnumScope->LocalMapMode ? LocalEnumReferencesSame(Before,After,EntryPath,LocalName,*EnumScope) : LifecycleEqual(Before.References, After.References), Other = LifecycleEqual(Before.Other, After.Other), Disk = LifecycleEqual(Before.Disk, After.Disk);
    R.Data->SetBoolField(TEXT("observations_complete"), Before.Complete && After.Complete); R.Data->SetBoolField(TEXT("local_declarations_preserved"), Locals);
    R.Data->SetBoolField(TEXT("local_node_fields_preserved"), NodeFields); R.Data->SetBoolField(TEXT("scs_preserved"), SCS);
    R.Data->SetBoolField(TEXT("nonselected_state_preserved"), State); R.Data->SetBoolField(TEXT("source_preserved"), Source);
    R.Data->SetBoolField(TEXT("references_preserved"), Refs); R.Data->SetBoolField(TEXT("other_package_dirty_preserved"), Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), Disk);
    return State && Source && Refs && Other && Disk;
}
}
FResult AddLocal(const FString& Path, const FString& Name, const FString& Type, const FString& DefaultJson,
    const FString& Category, const FString& Tooltip, const FString& TypeObject, bool Array,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool CreateGraph)
{
    FResult R; LocalInit(R); TSharedPtr<FDeclarationEnumScope> EnumScope; R.Data->SetStringField(TEXT("blueprint_name"), Path); R.Data->SetStringField(TEXT("variable_name"), Name);
    R.Data->SetStringField(TEXT("variable_type"), Type); R.Data->SetStringField(TEXT("requested_graph_name"), GraphName); R.Data->SetStringField(TEXT("requested_graph_id"), GraphId);
    R.Data->SetStringField(TEXT("requested_graph_type"), GraphType); R.Data->SetBoolField(TEXT("create_graph_if_missing"), CreateGraph);
    if (!Identifier(Name) || !SafeText(Type, 128, false) || !SafeText(Category, 4096) || !SafeText(Tooltip, 65536)
        || !SafeName(TypeObject, true) || !SafeName(GraphName, true) || !SafeName(GraphId, true) || !SafeText(GraphType, 128))
    { Fail(R, TEXT("local_variable_input_invalid"), TEXT("Bounded safe inputs and an ordinary ASCII local identifier are required; extended legacy names remain incomplete.")); return R; }
    const FString EnumType=Type.ToLower().Replace(TEXT("_"),TEXT("")).Replace(TEXT("-"),TEXT("")).Replace(TEXT(" "),TEXT(""));const bool IsEnum=EnumType==TEXT("enum")||EnumType==TEXT("byteenum");const bool IsNativeClass=EnumType==TEXT("object")||EnumType==TEXT("class");
    const bool IsScalarUds=!Array&&(EnumType==TEXT("struct")||EnumType==TEXT("scriptstruct"));const bool IsUdsArray=Array&&(EnumType==TEXT("struct")||EnumType==TEXT("scriptstruct"));
    if (!TypeObject.IsEmpty()&&!IsEnum&&!IsNativeClass&&!IsScalarUds&&!IsUdsArray&&EnumType!=TEXT("transform")) { Fail(R, TEXT("local_variable_type_unsupported"), TEXT("Other complex type_object variants remain outside the finite local slice.")); return R; }
    const FString T = Type.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).Replace(TEXT(" "), TEXT(""));
    const TSet<FString> Types = {TEXT("bool"),TEXT("boolean"),TEXT("int"),TEXT("integer"),TEXT("int64"),TEXT("integer64"),TEXT("float"),TEXT("single"),TEXT("double"),TEXT("real"),TEXT("string"),TEXT("name"),TEXT("text")};
    const bool IsNativeStruct=T==TEXT("vector")||T==TEXT("rotator")||T==TEXT("transform");const bool IsLocalMap=T==TEXT("mapstringdouble");
    if (!IsLocalMap&&!IsEnum&&!IsNativeClass&&!IsNativeStruct&&!IsScalarUds&&!IsUdsArray&&!Types.Contains(T)) { Fail(R, TEXT("local_variable_type_unsupported"), TEXT("Local slice supports ordinary scalar, exact Vector/Rotator, UserDefinedEnum and native Actor/Object constraints only; other original types remain incomplete.")); return R; }
    FEdGraphPinType Pin; FString Stored;
    if(IsScalarUds||IsUdsArray)
    {
        EnumScope=MakeShared<FDeclarationEnumScope>();if(!EnumScope->LoadScalarUds(TypeObject,R))return R;Pin.PinCategory=UEdGraphSchema_K2::PC_Struct;Pin.PinSubCategoryObject=EnumScope->NativeStructSource.Get();if(IsUdsArray){Pin.ContainerType=EPinContainerType::Array;R.Data->SetBoolField(TEXT("native_primitive_array_local_declaration"),true);R.Data->SetBoolField(TEXT("native_value_struct_array_local_declaration"),true);R.Data->SetBoolField(TEXT("primitive_array_runtime_initialization_verified"),false);R.Data->SetBoolField(TEXT("primitive_array_literal_buffer_import_performed"),false);R.Data->SetBoolField(TEXT("default_omitted_original_None"),DefaultJson.IsEmpty());}
        if(!DefaultJson.IsEmpty()){
            Function::FResult Converted;TSharedPtr<FJsonValue> Default;if(!Function::Pin::ParseJson(DefaultJson,Default,Converted,true)){Fail(R,TEXT("local_uds_default_invalid"),Converted.Error);return R;}
            if(Default->Type==EJson::Null){Stored.Reset();if(IsUdsArray)R.Data->SetBoolField(TEXT("default_omitted_original_None"),true);}else if(Default->Type==EJson::String)Stored=Default->AsString();else if(Default->Type==EJson::Boolean)Stored=Default->AsBool()?TEXT("true"):TEXT("false");else if(Default->Type==EJson::Number)Stored=FString::SanitizeFloat(Default->AsNumber());else if(!FJsonSerializer::Serialize(Default,TEXT(""),TJsonWriterFactory<>::Create(&Stored))){Fail(R,TEXT("local_uds_default_invalid"),TEXT("Original JSON fallback declaration text could not be serialized."));return R;}
        }
        if(!SafeText(Stored,1048576)||RemovalDependencyReference(Stored)||Stored.Contains(TEXT("CubelessMCP"),ESearchCase::IgnoreCase)){Fail(R,TEXT("local_uds_default_invalid"),TEXT("Bounded dependency-safe one declaration string required; no field import or source mutation."));return R;}
        R.Data->SetBoolField(IsUdsArray?TEXT("native_scalar_uds_array_local_declaration"):TEXT("native_scalar_uds_local_declaration"),true);R.Data->SetStringField(TEXT("scalar_uds_default_text"),Stored);R.Data->SetBoolField(TEXT("scalar_uds_runtime_initialization_verified"),false);R.Data->SetBoolField(TEXT("scalar_uds_default_buffer_import_performed"),false);R.Data->SetStringField(TEXT("admission_scope"),TEXT("Loaded saved-clean primary Game UDS with 1..32 own scalar7 fields in an own Actor function. Original Any becomes one declaration string; no source load, nested/container fields, default-buffer import or runtime proof."));
        if(IsUdsArray){R.Data->SetStringField(TEXT("scalar_uds_array_default_text"),Stored);R.Data->SetStringField(TEXT("primitive_array_default_contract"),TEXT("Original UDS Any-to-one declaration string; no per-element parsing, runtime initialization or source default-buffer import."));R.Data->SetStringField(TEXT("admission_scope"),TEXT("One-dimensional own Actor function local Array of an already-loaded saved-clean primary Game UDS with 1..32 own scalar7 fields. Current actual Array/Inner binding, source GUID/EditorData/metadata and bind/final file remain guarded; generic sources/loaders/nested fields/runtime values remain incomplete."));}
    }
    else if(Array)
    {
        if(IsEnum||IsNativeClass)
        {
            EnumScope=MakeShared<FDeclarationEnumScope>();
            if(IsEnum){if(!EnumScope->Load(TypeObject,R))return R;Pin.PinCategory=UEdGraphSchema_K2::PC_Byte;Pin.PinSubCategoryObject=EnumScope->Source.Get();}
            else{if(!EnumScope->LoadNativeClass(TypeObject,R))return R;Pin.PinCategory=T==TEXT("class")?UEdGraphSchema_K2::PC_Class:UEdGraphSchema_K2::PC_Object;Pin.PinSubCategoryObject=EnumScope->NativeSource.Get();}
            Pin.ContainerType=EPinContainerType::Array;R.Data->SetBoolField(TEXT("native_primitive_array_local_declaration"),true);R.Data->SetBoolField(TEXT("native_referenced_array_local_declaration"),true);R.Data->SetBoolField(TEXT("primitive_array_runtime_initialization_verified"),false);R.Data->SetBoolField(TEXT("primitive_array_literal_buffer_import_performed"),false);R.Data->SetBoolField(TEXT("default_omitted_original_None"),DefaultJson.IsEmpty());
            if(!DefaultJson.IsEmpty())
            {
                Function::FResult Converted;TSharedPtr<FJsonValue> Default;
                if(!Function::Pin::ParseJson(DefaultJson,Default,Converted,true)){EnumScope->Bad(TEXT("local_referenced_array_default_invalid"),*Converted.Error);return R;}
                if(Default->Type==EJson::Null){Stored.Reset();R.Data->SetBoolField(TEXT("default_omitted_original_None"),true);}
                else if(IsEnum){EnumScope->Bad(TEXT("local_enum_array_default_unsupported"),TEXT("Original checked enum Array declarations reject explicit defaults. Public None omission is permitted; no element buffer import."));return R;}
                else if(Default->Type==EJson::String){Stored=Default->AsString();if(RemovalDependencyReference(Stored)||Stored.Contains(TEXT("CubelessMCP"),ESearchCase::IgnoreCase)){EnumScope->Bad(TEXT("local_referenced_array_default_dependency"),TEXT("Original Object/Class dependency-reference guard rejected this declaration string."));return R;}}
                else if(Default->Type==EJson::Boolean)Stored=Default->AsBool()?TEXT("true"):TEXT("false");
                else if(Default->Type==EJson::Number)Stored=FString::SanitizeFloat(Default->AsNumber());
                else if(!FJsonSerializer::Serialize(Default,TEXT(""),TJsonWriterFactory<>::Create(&Stored))){EnumScope->Bad(TEXT("local_referenced_array_default_invalid"),TEXT("Original JSON fallback declaration text could not be serialized."));return R;}
            }
            if(!EnumScope->Check())return R;
            R.Data->SetStringField(TEXT("referenced_array_default_text"),Stored);R.Data->SetStringField(TEXT("primitive_array_default_contract"),TEXT("Enum Array explicit defaults retain the original checked rejection; public None is omitted. Object/Class Array Any values remain one declaration string using original scalar/JSON fallback, without default-object loading, per-element initialization or runtime buffer import."));R.Data->SetStringField(TEXT("admission_scope"),TEXT("One-dimensional referenced local Array: original Game/Engine UserDefinedEnum or exact loaded native Actor/Object Class/Object constraint. Existing held source/current, GUID/structural refresh and whole Array.Inner plan are reused; other enum/class sources, loaders and runtime values remain incomplete."));
        }
        else if(IsNativeStruct)
        {
            EnumScope=MakeShared<FDeclarationEnumScope>();if(!EnumScope->LoadNativeStruct(T,R,T==TEXT("transform")))return R;
            if(!PinType(Type,TEXT(""),true,Pin,R)||Pin.PinSubCategoryObject.Get()!=EnumScope->NativeStructSource.Get())return R;
            R.Data->SetBoolField(TEXT("native_primitive_array_local_declaration"),true);R.Data->SetBoolField(TEXT("native_value_struct_array_local_declaration"),true);R.Data->SetBoolField(TEXT("primitive_array_runtime_initialization_verified"),false);R.Data->SetBoolField(TEXT("primitive_array_literal_buffer_import_performed"),false);R.Data->SetBoolField(TEXT("default_omitted_original_None"),DefaultJson.IsEmpty());
            if(T==TEXT("transform"))
            {
                R.Data->SetBoolField(TEXT("native_transform_array_local_declaration"),true);
                if(!DefaultJson.IsEmpty())
                {
                    Function::FResult Converted;TSharedPtr<FJsonValue> ValueJson;
                    if(!Function::Pin::ParseJson(DefaultJson,ValueJson,Converted,true)){Fail(R,TEXT("local_transform_array_default_invalid"),Converted.Error);return R;}
                    if(ValueJson->Type==EJson::Null){Stored.Reset();R.Data->SetBoolField(TEXT("default_omitted_original_None"),true);}
                    else{if(!EnumScope->Check())return R;if(!Function::Pin::PrimitiveArrayDefaultText(Pin,ValueJson,Stored,Converted)){Fail(R,TEXT("local_transform_array_default_invalid"),Converted.Error);return R;}if(!EnumScope->Check())return R;}
                }
                R.Data->SetStringField(TEXT("transform_array_default_text"),Stored);R.Data->SetStringField(TEXT("primitive_array_default_contract"),TEXT("Original Transform checked Any/group defaults to one compact declaration string; public ParseVector/ParseRotator return/finite checks preserve nonzero rotation without silent fallback. No per-element array initialization or runtime buffer import."));
            }
            else if(!DefaultJson.IsEmpty()&&!LocalDefault(DefaultJson,Pin,Stored,R))return R;
            if(T!=TEXT("transform"))R.Data->SetStringField(TEXT("primitive_array_default_contract"),TEXT("Original Vector/Rotator checked Any-to-three-axis declaration text storage, including None omission. The declaration text is not per-element array initialization or a runtime buffer import."));
            R.Data->SetStringField(TEXT("admission_scope"),T==TEXT("transform")?TEXT("Exact native Transform one-dimensional local array using existing own function declaration/current boundary and held Transform->Quat/Vector type/field/metadata guards. Original implicit selectors ignored; no scalar Transform admission, loader, runtime/default-buffer evaluation or arbitrary structs."):TEXT("Exact native CoreUObject Vector/Rotator one-dimensional local array in the existing own Actor function. Existing structural refresh, Array/Inner direct-owner/current and readonly native struct three-field/source metadata guards are verified. Runtime/default evaluation, arbitrary structs and other containers remain incomplete."));
        }
        else
        {
        if(!Types.Contains(T)||!TypeObject.IsEmpty()){Fail(R,TEXT("local_primitive_array_type_unsupported"),TEXT("One-dimensional primitive8 local arrays require an empty type_object; nested/enum/object/struct containers remain incomplete."));return R;}
        R.Data->SetBoolField(TEXT("native_primitive_array_local_declaration"),true);R.Data->SetBoolField(TEXT("primitive_array_runtime_initialization_verified"),false);R.Data->SetBoolField(TEXT("primitive_array_literal_buffer_import_performed"),false);
        if(!PinType(Type,TEXT(""),true,Pin,R))return R;
        R.Data->SetBoolField(TEXT("default_omitted_original_None"),DefaultJson.IsEmpty());
        if(!DefaultJson.IsEmpty())
        {
            Function::FResult ConvertedDefault;TSharedPtr<FJsonValue> ArrayDefault;
            if(!Function::Pin::ParseJson(DefaultJson,ArrayDefault,ConvertedDefault))
            {R.Data->SetStringField(TEXT("primitive_array_default_error_code"),ConvertedDefault.ErrorCode);Fail(R,TEXT("local_primitive_array_default_invalid"),ConvertedDefault.Error);return R;}
            if(ArrayDefault->Type==EJson::Null){Stored.Reset();R.Data->SetBoolField(TEXT("default_omitted_original_None"),true);}
            else if(!Function::Pin::PrimitiveArrayDefaultText(Pin,ArrayDefault,Stored,ConvertedDefault))
            {R.Data->SetStringField(TEXT("primitive_array_default_error_code"),ConvertedDefault.ErrorCode);Fail(R,TEXT("local_primitive_array_default_invalid"),ConvertedDefault.Error);return R;}
        }
        R.Data->SetStringField(TEXT("primitive_array_default_contract"),TEXT("Original checked primitive declaration literal storage; boolean arrays require JSON boolean, other primitive arrays reuse the original scalar/JSON serialization. No runtime buffer import or pin literal validity claim."));
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("One-dimensional primitive8 local array declaration in the existing own Actor function admission. Existing structural refresh, actual descriptor/default and selected Array/Inner current plan are verified; no full compile/save or runtime initialization."));
        }
    }
    else if(IsLocalMap)
    {
        if(!DefaultJson.IsEmpty()){TSharedPtr<FJsonValue> D;if(!Parse(DefaultJson,D,R))return R;if(D->Type!=EJson::Object||!D->AsObject()->Values.IsEmpty()){Fail(R,TEXT("local_map_default_unsupported"),TEXT("Exact String/double local Map permits omitted or empty object default only; no buffer import."));return R;}}
        EnumScope=MakeShared<FDeclarationEnumScope>();if(!EnumScope->BeginLocalMap(R))return R;
        Pin.PinCategory=UEdGraphSchema_K2::PC_String;Pin.ContainerType=EPinContainerType::Map;Pin.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Real;Pin.PinValueType.TerminalSubCategory=UEdGraphSchema_K2::PC_Double;
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("Exact own normal Actor or member0 PresetLoader function-library local String/double Map declaration, empty stored default; no member transient flag, runtime value or implicit full compile/save."));
    }
    else if(IsEnum)
    {
        EnumScope=MakeShared<FDeclarationEnumScope>();if(!EnumScope->Load(TypeObject,R)) { return R; }Pin.PinCategory=UEdGraphSchema_K2::PC_Byte;Pin.PinSubCategoryObject=EnumScope->Source.Get();
        TSharedPtr<FJsonValue> Default;if(!DefaultJson.IsEmpty()&&!Parse(DefaultJson,Default,R)) { return R; }
        if(Default.IsValid()&&Default->Type!=EJson::Null&&!EnumScope->Value(Default,Stored)) { return R; }
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("One exact read-only UserDefinedEnum local in an existing own top-level function of a direct native Actor Blueprint; existing scalar/member/function admission remains in force. Arrays/native enums/reference and inherited/function-signature variants remain pending."));
    }
    else if(IsNativeClass)
    {
        EnumScope=MakeShared<FDeclarationEnumScope>();if(!EnumScope->LoadNativeClass(TypeObject,R)) { return R; }
        Pin.PinCategory=T==TEXT("class")?UEdGraphSchema_K2::PC_Class:UEdGraphSchema_K2::PC_Object;Pin.PinSubCategoryObject=EnumScope->NativeSource.Get();
        TSharedPtr<FJsonValue> Default;if(!DefaultJson.IsEmpty()&&!Parse(DefaultJson,Default,R)) { return R; }
        if(!EnumScope->NativeDefault(Default,T==TEXT("class"),Stored)) { return R; }
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("One scalar Object/Class local with exact runtime native Actor or original omitted Object constraint in an existing own function. Native source hierarchy/metadata/existing CDO identity remains read-only; arrays/generated/plugin/editor sources and nonempty object defaults remain pending."));
    }
    else if(IsNativeStruct)
    {
        EnumScope=MakeShared<FDeclarationEnumScope>();if(!EnumScope->LoadNativeStruct(T,R,T==TEXT("transform"))){return R;}
        if(!PinType(Type,TEXT(""),false,Pin,R)||Pin.PinSubCategoryObject.Get()!=EnumScope->NativeStructSource.Get()){return R;}
        if(T==TEXT("transform"))
        {
            R.Data->SetBoolField(TEXT("native_transform_local_declaration"),true);R.Data->SetBoolField(TEXT("transform_runtime_initialization_verified"),false);R.Data->SetBoolField(TEXT("transform_default_buffer_import_performed"),false);R.Data->SetBoolField(TEXT("default_omitted_original_None"),DefaultJson.IsEmpty());
            if(!DefaultJson.IsEmpty())
            {
                Function::FResult Converted;TSharedPtr<FJsonValue> ValueJson;
                if(!Function::Pin::ParseJson(DefaultJson,ValueJson,Converted,true)){Fail(R,TEXT("local_transform_default_invalid"),Converted.Error);return R;}
                if(ValueJson->Type==EJson::Null){Stored.Reset();R.Data->SetBoolField(TEXT("default_omitted_original_None"),true);}
                else
                {
                    // A conversion-only Array view reuses the checked original Any-to-one-text
                    // contract. The declaration Pin and compiled local remain scalar.
                    FEdGraphPinType ConversionType=Pin;ConversionType.ContainerType=EPinContainerType::Array;
                    if(!EnumScope->Check())return R;if(!Function::Pin::PrimitiveArrayDefaultText(ConversionType,ValueJson,Stored,Converted)){Fail(R,TEXT("local_transform_default_invalid"),Converted.Error);return R;}if(!EnumScope->Check())return R;
                }
            }
            R.Data->SetStringField(TEXT("transform_default_text"),Stored);R.Data->SetStringField(TEXT("transform_default_contract"),TEXT("Original implicit Transform Any/group defaults become one declaration string. Checked public ParseVector/ParseRotator preserves finite nonzero components without silent fallback; no default buffer import or runtime local initialization/evaluation proof."));
            R.Data->SetStringField(TEXT("admission_scope"),TEXT("One exact native CoreUObject Transform scalar local in the existing own Actor function. Original implicit type selector is ignored; held Transform/Quat/Vector source identity, direct field bindings and metadata remain guarded. Other structs, loaders, containers and runtime values remain incomplete."));
        }
        else
        {
            if(!LocalDefault(DefaultJson,Pin,Stored,R))return R;
            R.Data->SetStringField(TEXT("admission_scope"),TEXT("One exact native CoreUObject Vector/Rotator scalar local in an existing own function. Source member signature/metadata/identity stays readonly; custom structures/type loaders and runtime local evaluation remain incomplete."));
        }
    }
    else if (!PinType(Type, TEXT(""), false, Pin, R) || !LocalDefault(DefaultJson, Pin, Stored, R)) { return R; }
    TStrongObjectPtr<UBlueprint> Hold(LocalLoad(Path, R, EnumScope.Get())); UBlueprint* BP = Hold.Get();
    if (!BP || (!(IsLocalMap&&EnumScope->PresetMapLibrary)&&!LifecycleSimple(BP, R))) { return R; } if(EnumScope&&!EnumScope->Check()) { return R; } R.Data->SetStringField(TEXT("asset_path"), BP->GetPathName());
    // AAK creates the missing function once, before the existing local writer.
    // All already-supported local descriptors may join an own Actor function.
    // Source-backed scopes retain their original source snapshot. Only after a
    // successful verified structural helper may their existing RefreshClasses
    // adopt the target's current class/CDO roles; failed creation never rebases.
    const bool MayCreate=CreateGraph&&BP->BlueprintType==BPTYPE_Normal&&GraphId.IsEmpty()
        &&LocalGraphType(GraphType)==TEXT("function")&&!GraphName.IsEmpty();
    Function::FResult CreatedFunction;TFunction<bool(UEdGraphNode*)> CreatedFunctionCurrent;
    TUniquePtr<FScopedTransaction> CreationTransaction;bool FunctionCreated=false,CreationChanged=false;
    UEdGraph* G = LocalSelect(BP, GraphName, GraphId, GraphType, CreateGraph, R,MayCreate);
    if(!G)
    {
        if(!MayCreate||!R.ErrorCode.IsEmpty())return R;
        // Keep one outer transaction for the composite API. The original local
        // transaction is opened below only when this earlier scope is absent.
        if(EnumScope&&!EnumScope->Check())return R;
        CreationTransaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","AddLocalVariable","MCP add_blueprint_local_variable"));
        LifecycleCount(R,TEXT("outer_transaction_calls"));
        if(EnumScope&&!EnumScope->Check())return R;
        const bool Created=Function::CreateFunctionGraphForNode(BP,GraphName,CreatedFunction,G,CreatedFunctionCurrent);
        auto Prefix=MakeShared<FJsonObject>();Prefix->SetBoolField(TEXT("helper_returned"),Created);
        Prefix->SetBoolField(TEXT("mutation_attempted"),CreatedFunction.bMutationAttempted);Prefix->SetBoolField(TEXT("change_state_known"),CreatedFunction.bChangeStateKnown);
        Prefix->SetField(TEXT("changed"),CreatedFunction.bChangeStateKnown?TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(CreatedFunction.bChanged)):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
        Prefix->SetStringField(TEXT("stage"),CreatedFunction.Stage);Prefix->SetStringField(TEXT("error_code"),CreatedFunction.ErrorCode);Prefix->SetStringField(TEXT("error"),CreatedFunction.Error);Prefix->SetObjectField(TEXT("data"),CreatedFunction.Data);
        R.Data->SetObjectField(TEXT("local_function_creation_prefix"),Prefix);
        R.bMutationAttempted|=CreatedFunction.bMutationAttempted;R.bChanged|=CreatedFunction.bChanged;
        R.bChangeStateKnown=CreatedFunction.bChangeStateKnown;R.Stage=CreatedFunction.Stage;CreationChanged=CreatedFunction.bChanged;
        R.Data->SetBoolField(TEXT("local_function_creation_attempted"),true);R.Data->SetBoolField(TEXT("local_function_created"),Created);
        R.Data->SetBoolField(TEXT("target_blueprint_load_repeated_for_local_creation"),false);
        R.Data->SetStringField(TEXT("local_function_creation_scope"),TEXT("One missing explicitly named own Actor function followed by one already-supported local declaration, with one outer transaction and no target reload or public operation replay. Existing function-creation and local lifecycle writers own their separate verified stages; failures retain the created function and actual declaration prefix without rollback. Existing enum/native class/native value-struct/UDS source guards, primitive/referenced/value-struct array declarations and String-double Map are reused at the normal target structural boundary. New descriptor types, BFL/new-owner/selector creation variants and actual runtime initialization remain incomplete; usage validation pending."));
        if(!Created||!CreatedFunction.bChangeStateKnown||!CreatedFunction.ErrorCode.IsEmpty())
        {R.ErrorCode=CreatedFunction.ErrorCode;R.Error=CreatedFunction.Error;if(R.ErrorCode.IsEmpty())Fail(R,TEXT("local_function_creation_failed"),TEXT("Function helper did not establish a known created endpoint; complete actual prefix retained without local write or retry."));return R;}
        if(EnumScope)
        {
            // The helper has completed its AddFunctionGraph structural return
            // and verified old graph preservation. Source identity is checked
            // before existing RefreshClasses adopts only current target roles;
            // its final Check still compares the original complete source.
            const bool Refreshed=EnumScope->RefreshClasses();
            Prefix->SetBoolField(TEXT("source_scope_target_refreshed_after_creation"),Refreshed);
            R.Data->SetBoolField(TEXT("local_creation_source_snapshot_replaced"),false);
            if(!Refreshed)return R;
        }
        FunctionCreated=true;
        R.Data->SetStringField(TEXT("admission_scope_before_function_creation"),R.Data->GetStringField(TEXT("admission_scope")));
        R.Data->SetStringField(TEXT("admission_scope"),TEXT("Already-supported local descriptor in a newly created explicitly named own native Actor function; complete original type/source/default/partial-declaration checks remain in force. The existing function helper owns graph creation and its structural prefix; explicit_graph_create_calls belongs only to the following local writer and is not a whole-operation zero count. Wider descriptors/owners/selectors and runtime evaluation remain incomplete; usage validation pending."));
        R.Data->SetStringField(TEXT("graph_creation_count_authority"),TEXT("local_function_creation_prefix.data.graph_resolution for helper graph creation and actual returned structural prefix; explicit_graph_create_calls counts direct local-writer calls only."));
    }
    if(!G)return R;
    auto CreationCurrent=[&]() -> bool
    {
        if(!FunctionCreated)return true;
        if(CreatedFunctionCurrent&&CreatedFunctionCurrent(nullptr))return true;
        R.bChangeStateKnown=false;R.Data->SetObjectField(TEXT("local_function_creation_latest_data"),CreatedFunction.Data);
        return Fail(R,TEXT("local_function_creation_endpoint_changed"),TEXT("Created function/current target changed before the local writer; original creation prefix retained, no declaration or automatic replay."));
    };
    if(!CreationCurrent())return R;
    if(EnumScope&&!EnumScope->BindGraph(G)) { return R; }
    UK2Node_FunctionEntry* Entry = nullptr; if (IsLocalMap) { const auto Inventory=G->Nodes; for(const auto& H:Inventory){if(!H.IsResolved()){EnumScope->Bad(TEXT("local_map_entry_unavailable"),TEXT("Resolved entry inventory required."));return R;}UEdGraphNode* N=H.Get();if(!EnumScope->Check()||G->Nodes!=Inventory)return R;if(N&&N->GetClass()==UK2Node_FunctionEntry::StaticClass()){if(Entry){EnumScope->Bad(TEXT("local_map_entry_unavailable"),TEXT("One exact Entry required."));return R;}Entry=static_cast<UK2Node_FunctionEntry*>(N);}}FMemberReference OwnLocalReference;if(Entry)OwnLocalReference.SetExternalMember(G->GetFName(),nullptr,Entry->FunctionReference.GetMemberGuid());if(!Entry||!Entry->FunctionReference.IsSameReference(OwnLocalReference)||Entry->FunctionReference.IsSelfContext()||Entry->GetOuter()!=G||!Entry->IsEditable()||!Entry->CustomGeneratedFunctionName.IsNone()||Entry->FunctionReference.GetMemberName()!=G->GetFName()||Entry->FunctionReference.IsLocalScope()||Entry->FunctionReference.IsDeprecated()||Entry->LocalVariables.Num()>255||((Entry->GetExtraFlags()&FUNC_Static)!=0)!=EnumScope->PresetMapLibrary){EnumScope->Bad(TEXT("local_variable_function_scope_unsupported"),TEXT("One editable own nonstatic Actor function Entry required."));return R;}}else if (!LocalAdmission(BP, G, Entry, R)) { return R; } if(EnumScope&&!EnumScope->BindEditable(Entry)) { return R; }if(IsLocalMap&&!EnumScope->PresetEntry(Entry))return R;const FGuid EntryGuid=Entry->NodeGuid;
    if(!CreationCurrent())return R;
    TStrongObjectPtr<UEdGraph> GraphHold(G); TStrongObjectPtr<UK2Node_FunctionEntry> EntryHold(Entry);
    const FName Requested(*Name);
    const auto FindSelectedLocal=[&](UK2Node_FunctionEntry** Out=nullptr)->FBPVariableDescription*{if(!IsLocalMap)return FBlueprintEditorUtils::FindLocalVariable(BP,G,Requested,Out);if(!EnumScope->Check())return nullptr;auto* D=Entry->LocalVariables.FindByPredicate([&](const auto& X){return X.VarName==Requested;});if(D&&Out)*Out=Entry;return D;};
    if (FindSelectedLocal()) { Fail(R, TEXT("local_variable_already_exists"), FString::Printf(TEXT("Local variable already exists: %s"), *Name)); return R; }
    bool ExistingMapClassField=false;if(IsLocalMap){for(auto I=EnumScope->Generated->GetSuperStructIterator();I;++I){for(TFieldIterator<FProperty> F(*I,EFieldIterationFlags::None);F;++F){if(F->GetFName()==Requested){ExistingMapClassField=true;break;}}}if(!EnumScope->Check())return R;}
    if ((IsLocalMap?ExistingMapClassField:FindFProperty<FProperty>(BP->GeneratedClass, Requested)!=nullptr) || BP->NewVariables.ContainsByPredicate([&](const FBPVariableDescription& V){return V.VarName == Requested;}))
    { Fail(R, TEXT("local_variable_member_collision_unsupported"), TEXT("Member/inherited name shadowing is outside the first local declaration slice.")); return R; }
    R.Data->SetObjectField(TEXT("graph"), LocalGraphJson(BP, G)); R.Data->SetStringField(TEXT("function_entry_path"), Entry->GetPathName());
    R.Data->SetStringField(TEXT("default_value"), Stored); R.Data->SetObjectField(TEXT("pin_type"), LocalPinJson(Pin));
    FLifecycleObservation Before, After; R.Stage = TEXT("before_observation");
    if (!LocalCapture(BP, Before, R, TEXT("before_observation"), EnumScope.Get())) { if(EnumScope&&EnumScope->Stopped) { return R; } R.bChangeStateKnown = false; Fail(R, TEXT("local_variable_before_unobservable"), TEXT("Bounded initial local/member/graph/source prefix incomplete; no declaration dispatched.")); return R; }
    if(EnumScope&&!EnumScope->Check()) { return R; }
    const TArray<FBPVariableDescription> Old = Entry->LocalVariables; const FString EntryPath = Entry->GetPathName();
    if(!CreationCurrent())return R;
    // The function-creation lease freezes its new Entry. Retire that completed
    // stage only after the existing complete local baseline has been captured;
    // AddLocalVariable/MarkStructural legitimately rebuilds the target classes.
    if(FunctionCreated){CreatedFunctionCurrent=nullptr;R.Data->SetBoolField(TEXT("function_creation_scope_retired_before_local_declaration"),true);}
    TArray<TSharedPtr<FJsonValue>> Calls, DescriptorCalls; TUniquePtr<FScopedTransaction> LocalTransaction;
    if(!CreationTransaction){LocalTransaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","AddLocalVariable","MCP add_blueprint_local_variable"));LifecycleCount(R,TEXT("outer_transaction_calls"));}
    if(EnumScope&&!EnumScope->Check()) { return R; } R.Stage = TEXT("add_local_variable"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
    auto Call = MakeShared<FJsonObject>(); Call->SetStringField(TEXT("route"), TEXT("FBlueprintEditorUtils::AddLocalVariable")); Call->SetStringField(TEXT("graph_path"), G->GetPathName());
    Call->SetStringField(TEXT("variable_name"), Name); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false); Call->SetField(TEXT("result"), MakeShared<FJsonValueNull>());
    Calls.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("local_variable_calls"), Calls); LifecycleCount(R, TEXT("local_variable_add_attempts"));
    const bool Added = FBlueprintEditorUtils::AddLocalVariable(BP, G, Requested, Pin, Stored);
    LifecycleCount(R, TEXT("local_variable_add_returns")); Call->SetBoolField(TEXT("returned"), true); Call->SetBoolField(TEXT("result"), Added);
    if (Added) { LifecycleCount(R, TEXT("local_variable_add_successes")); }
    if(EnumScope)
    {
        if(!EnumScope->RefreshClasses()) { return R; }Entry=Cast<UK2Node_FunctionEntry>(EnumScope->CurrentEditable(EntryGuid));if(!Entry) { return R; }EntryHold.Reset(Entry);
    }
    UK2Node_FunctionEntry* CurrentEntry = nullptr; FBPVariableDescription* New = FindSelectedLocal(&CurrentEntry);
    bool Ready = Added && New && CurrentEntry == Entry && Entry->LocalVariables.Num() == Old.Num() + 1
        && New == &Entry->LocalVariables.Last() && New->VarGuid.IsValid();
    FGuid AddedGuid;
    if (Ready) { for (const auto& V : Old) { Ready &= V.VarGuid != New->VarGuid; } }
    if (Ready)
    {
        AddedGuid = New->VarGuid; R.Data->SetStringField(TEXT("selected_variable_guid"), AddedGuid.ToString(EGuidFormats::Digits));
        for (const auto& Change : { TPair<FString,FString>(TEXT("category"), Category), TPair<FString,FString>(TEXT("tooltip"), Tooltip) })
        {
            if (Change.Value.IsEmpty()) { continue; } auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("property"), Change.Key);
            Receipt->SetStringField(TEXT("requested_value"), Change.Value); Receipt->SetBoolField(TEXT("attempted"), true); Receipt->SetBoolField(TEXT("returned"), false);
            DescriptorCalls.Add(MakeShared<FJsonValueObject>(Receipt)); R.Data->SetArrayField(TEXT("local_descriptor_calls"), DescriptorCalls);
            if (Change.Key == TEXT("category")) { LifecycleCount(R, TEXT("local_category_descriptor_set_calls")); New->Category = FText::FromString(Change.Value); }
            else { LifecycleCount(R, TEXT("local_tooltip_descriptor_set_calls")); New->SetMetaData(TEXT("Tooltip"), Change.Value); }
            Receipt->SetBoolField(TEXT("returned"), true);
        }
        R.Stage = TEXT("mark_structural"); LifecycleCount(R, TEXT("explicit_mark_structural_calls"));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); LifecycleCount(R, TEXT("local_structural_return_count"));
        if(EnumScope)
        {
            if(!EnumScope->RefreshClasses()) { return R; }Entry=Cast<UK2Node_FunctionEntry>(EnumScope->CurrentEditable(EntryGuid));if(!Entry) { return R; }EntryHold.Reset(Entry);New=nullptr;CurrentEntry=nullptr;
        }
    }
    R.Stage = TEXT("after_observation"); LifecycleAggregateClear(R);
    if (!LocalCapture(BP, After, R, TEXT("after_observation"), EnumScope.Get())) { if(EnumScope&&EnumScope->Stopped) { return R; } R.bChangeStateKnown = false; Fail(R, TEXT("local_variable_after_unobservable"), TEXT("Returned add/descriptor/structural prefix retained; final actual observation incomplete. No compile/save/retry.")); return R; }
    if(EnumScope&&!EnumScope->Check()) { return R; }
    R.bChangeStateKnown = true; R.bChanged = CreationChanged || !LifecycleEqual(Before.State, After.State) || !LifecycleEqual(Before.Source, After.Source)
        || !LifecycleEqual(Before.References, After.References) || !LifecycleEqual(Before.Other, After.Other) || !LifecycleEqual(Before.Disk, After.Disk);
    R.Data->SetBoolField(TEXT("observations_complete"), true); New = FindSelectedLocal(&CurrentEntry);
    if (!Ready || !New || CurrentEntry != Entry || Entry->LocalVariables.Num() != Old.Num() + 1 || New != &Entry->LocalVariables.Last() || New->VarGuid != AddedGuid)
    { Fail(R, TEXT("local_variable_add_failed"), TEXT("Public helper did not leave one current appended selected local declaration; complete actual prefix retained.")); return R; }
    FBPVariableDescription Expected; Expected.VarName = Requested; Expected.VarGuid = New->VarGuid; Expected.VarType = Pin; Expected.PropertyFlags |= CPF_BlueprintVisible;
    // Installed PostSetupObjectPinType(10069+) adds this descriptor bit for
    // Actor object locals; Class constraints are not Actor instance variables.
    if(IsNativeClass&&Pin.PinCategory==UEdGraphSchema_K2::PC_Object&&EnumScope->NativeSource.Get()==AActor::StaticClass()) { Expected.PropertyFlags|=CPF_DisableEditOnTemplate; }
    Expected.FriendlyName = FName::NameToDisplayString(Requested.ToString(), Pin.PinCategory == UEdGraphSchema_K2::PC_Boolean);
    Expected.Category = UEdGraphSchema_K2::VR_DefaultCategory; Expected.DefaultValue = Stored;
    if (!Category.IsEmpty()) { Expected.Category = FText::FromString(Category); } if (!Tooltip.IsEmpty()) { Expected.SetMetaData(TEXT("Tooltip"), Tooltip); }
    TArray<FBPVariableDescription> ExpectedLocals = Old; ExpectedLocals.Add(Expected); TArray<TSharedPtr<FJsonValue>> ExpectedRows;
    for (int32 I = 0; I < ExpectedLocals.Num(); ++I) { ExpectedRows.Add(MakeShared<FJsonValueObject>(LocalDescriptor(ExpectedLocals[I], I))); }
    FArrayProperty* Local = FindFProperty<FArrayProperty>(Entry->GetClass(), TEXT("LocalVariables")); FString Export;
    LifecycleCount(R, TEXT("local_expected_export_calls")); const bool Exported = Local && Local->ExportText_Direct(Export, &ExpectedLocals, &ExpectedLocals, Entry, PPF_None) && SafeText(Export, 65536);
    if(EnumScope)
    {
        if(!EnumScope->Check()) { return R; }CurrentEntry=nullptr;New=FindSelectedLocal(&CurrentEntry);
        if(!New||CurrentEntry!=Entry||New->VarGuid!=AddedGuid) { EnumScope->Bad(TEXT("declaration_enum_local_changed"),TEXT("Selected local declaration changed during reflected readback."));return R; }
    }
    R.Data->SetArrayField(TEXT("expected_local_declarations"), ExpectedRows); R.Data->SetStringField(TEXT("expected_local_variables_export"), Export);
    const bool Declared = New->VarName == Requested && New->VarGuid.IsValid() && LifecycleEqual(LocalDescriptor(*New, Old.Num()), LocalDescriptor(Expected, Old.Num()));
    R.Data->SetBoolField(TEXT("local_variable_declared"), Declared); R.Data->SetStringField(TEXT("stored_variable_name"), New->VarName.ToString());
    if (!Exported || !Declared || !LocalPreserved(Before, After, EntryPath, ExpectedRows, Export, R, EnumScope.Get(), Requested))
    { Fail(R, TEXT("local_variable_postcondition_failed"), TEXT("Local declaration or finite nonselected/source/reference/disk proof failed after known effects; raw prefix retained, no compile/save/retry.")); return R; }
    R.Data->SetObjectField(TEXT("graph"), LocalGraphJson(BP, G)); R.Data->SetStringField(TEXT("default_value"), New->DefaultValue); R.Data->SetObjectField(TEXT("pin_type"), LocalPinJson(New->VarType));
    if(EnumScope&&!EnumScope->Check()) { return R; }
    if(Array)
    {
        // Reuse the finite current own-local reader instead of a second Array
        // ownership verifier. Preserve the known declaration before a read can fail.
        auto Prefix=MakeShared<FJsonObject>();Prefix->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);Prefix->SetBoolField(TEXT("changed"),R.bChanged);Prefix->SetBoolField(TEXT("local_variable_declared"),Declared);Prefix->SetStringField(TEXT("stage"),R.Stage);R.Data->SetObjectField(TEXT("primitive_array_declaration_prefix_before_read"),Prefix);
        const FString ArrayGraphPath=G->GetPathName(),ArrayVariableGuid=AddedGuid.ToString(EGuidFormats::Digits),ArrayVariableName=R.Data->GetStringField(TEXT("stored_variable_name"));
        R.Stage=TEXT("array_current_readback");R.Data->SetBoolField(TEXT("primitive_array_plan_read_attempted"),true);
        const auto Plan=Graph::ReadMemberVariablePlan(ArrayGraphPath,ArrayVariableName,false);R.Data->SetBoolField(TEXT("primitive_array_plan_read_returned"),true);R.Data->SetObjectField(TEXT("primitive_array_declaration_plan"),Plan);
        bool PlanSuccess=false;FString Scope,CurrentGuid,CurrentName,CurrentGraph;
        if(!Plan->TryGetBoolField(TEXT("success"),PlanSuccess)||!PlanSuccess||!Plan->TryGetStringField(TEXT("member_scope"),Scope)||Scope!=TEXT("local_primitive_array")||!Plan->TryGetStringField(TEXT("variable_guid"),CurrentGuid)||CurrentGuid!=ArrayVariableGuid||!Plan->TryGetStringField(TEXT("variable_name"),CurrentName)||CurrentName!=ArrayVariableName||!Plan->TryGetStringField(TEXT("graph_path"),CurrentGraph)||CurrentGraph!=ArrayGraphPath)
        {R.bChangeStateKnown=false;Fail(R,TEXT("local_primitive_array_readback_incomplete"),TEXT("Known local declaration/structural prefix retained; current Array/Inner readback failed or became uncertain. No next SDK, compile/save, cleanup or automatic retry."));return R;}
        // New struct-array reader success does not replace the declaration's original source baseline.
        if(EnumScope&&!EnumScope->Check()){R.bChangeStateKnown=false;return R;}
    }
    if(IsScalarUds||IsUdsArray){if(!EnumScope->ScalarUds->FinalFile()||!EnumScope->Check()){R.bChangeStateKnown=false;return R;}R.Data->SetObjectField(TEXT("struct_source_after"),EnumScope->ScalarUds->Snapshot());}
    R.bPostconditionPassed = true; R.bSuccess = true; R.Stage = TEXT("complete"); return R;
}
}
