#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Misc/EngineVersionComparison.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Compatibility/UE58/DomainExpansion/CubelessDomainDependencyAudit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/OutputDeviceHelper.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"

namespace Cubeless::UE58::DomainExpansion
{
using FObject = TSharedPtr<FJsonObject>;
inline bool Live(const UObject* O)
{return IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);}
inline bool SafeContext()
{return IsInGameThread()&&Live(GEditor)&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage();}
inline bool BoundedText(const FString& S,int32 Limit=4096)
{if(S.Len()>Limit)return false;for(TCHAR C:S)if(C==0)return false;return true;}
inline bool BoundedObjectPath(const FString& Path)
{return BoundedText(Path,NAME_SIZE-1)&&(Path.Contains(TEXT("."))||Path.Len()+1+FPackageName::GetLongPackageAssetName(Path).Len()<NAME_SIZE)&&Path.StartsWith(TEXT("/"))&&!Path.Contains(TEXT("//"))&&!Path.Contains(TEXT(":"))&&FPackageName::IsValidObjectPath(Path);}
inline FObject Result()
{
	auto R=MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"),true); R->SetBoolField(TEXT("saved"),false);
	R->SetBoolField(TEXT("mutation_performed"),false); R->SetBoolField(TEXT("read_only"),true);
    R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("automatic_retry"),false);R->SetBoolField(TEXT("atomic_rollback_available"),false);
	return R;
}
inline FObject Error(const FString& Message)
{
	auto R=Result(); R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("error"),Message);
	R->SetStringField(TEXT("error_code"),TEXT("ue58_authoring_validation")); return R;
}
inline FString String(const FObject& P,const TCHAR* Key)
{
	FString S; if(P)P->TryGetStringField(Key,S); return S;
}
inline bool DryRun(const FObject& P) { return !P || !P->HasTypedField<EJson::Boolean>(TEXT("dry_run")) || P->GetBoolField(TEXT("dry_run")); }
inline bool CanonicalTempPath(const FString& Path)
{
	return BoundedText(Path,NAME_SIZE-1) && (Path.Len()+1+FPackageName::GetLongPackageAssetName(Path).Len()<NAME_SIZE) && Path.StartsWith(TEXT("/Game/_MCP_Temp/")) && !Path.Contains(TEXT("//")) &&
		!Path.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(Path);
}
inline bool UnusedAssetPath(const FString& Path,FString& Why)
{
	if(!SafeContext() || !CanonicalTempPath(Path)) { Why=TEXT("Canonical unused temp asset path required"); return false; }
	const FString ObjectPath=Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path);
	if(FindPackage(nullptr,*Path) || FPackageName::DoesPackageExist(Path) ||
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid())
	{ Why=TEXT("Target already exists in memory, registry or disk; overwrite is refused"); return false; }
	return true;
}
inline FObject CandidateFailure(UObject* Object,const FString& Why,bool Pending=false)
{
	auto R=Error(Why); R->SetBoolField(TEXT("read_only"),false); R->SetBoolField(TEXT("mutation_performed"),true);
	R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("do_not_retry"),true); R->SetBoolField(TEXT("candidate_retained"),true);
	R->SetBoolField(TEXT("compilation_pending"),Pending); R->SetStringField(TEXT("target_path"),SafeContext()&&Live(Object) ? Object->GetPathName() : TEXT(""));
	return R;
}
inline void PublishCandidate(UObject* Object,const FObject& R)
{
    if(!SafeContext()||!Live(Object)){R->Values.Append(CandidateFailure(nullptr,TEXT("Candidate/context unavailable before publication"))->Values);return;}
    TStrongObjectPtr<UObject> Hold(Object);TStrongObjectPtr<UPackage> Package(Object->GetPackage());const FString Path=Object->GetPathName();
    auto Current=[&]{return SafeContext()&&Live(Hold.Get())&&Live(Package.Get())&&Object->GetPackage()==Package.Get()&&Object->GetPathName()==Path;};
	FString Why;
	if(!FDomainDependencyAudit::ValidateMCPDependencyIndependence(TEXT("ue58_authoring_candidate"),Object,EDomainAuditMode::InMemoryOnly,Why))
	{ R->Values.Append(CandidateFailure(Object,Why)->Values); return; }
	if(!Current()){R->Values.Append(CandidateFailure(nullptr,TEXT("Candidate changed during dependency audit"))->Values);return;}
    R->SetBoolField(TEXT("dependency_audit_passed"),true);
    FAssetRegistryModule::AssetCreated(Object);
    if(!Current()){R->Values.Append(CandidateFailure(nullptr,TEXT("Candidate changed during AssetCreated callback"))->Values);return;}
    Object->MarkPackageDirty();
    if(!Current()){R->Values.Append(CandidateFailure(nullptr,TEXT("Candidate changed during dirty notification"))->Values);return;}
	R->SetStringField(TEXT("target_path"),Object->GetPathName()); R->SetBoolField(TEXT("read_only"),false);
	R->SetBoolField(TEXT("mutation_performed"),true); R->SetBoolField(TEXT("postcondition_verified"),true);
}
inline bool HasMCPReference(const FString& Value)
{
	return FDomainDependencyAudit::IsMCPDependencyReference(Value);
}
inline bool IndependentSavedSource(UObject* Object,FString& Why)
{
	if(!SafeContext() || !Live(Object) || Object->GetOutermost()->IsDirty() || !FPackageName::DoesPackageExist(Object->GetOutermost()->GetName())) { Why=TEXT("Source must be a saved clean asset"); return false; }
	TStrongObjectPtr<UObject> Hold(Object);TStrongObjectPtr<UPackage> Package(Object->GetPackage());const FString Path=Object->GetPathName();
    const bool Passed=FDomainDependencyAudit::ValidateMCPDependencyIndependence(TEXT("ue58_authoring_source"),Object,EDomainAuditMode::RequirePersisted,Why);
    if(!Passed)return false;
    if(!SafeContext()||!Live(Hold.Get())||Object->GetPackage()!=Package.Get()||Object->GetPathName()!=Path||Package->IsDirty()){Why=TEXT("Source changed during dependency audit");return false;}return true;
}
inline FString StructText(const UScriptStruct* Type,const void* Data)
{
	FString S; Type->ExportText(S,Data,nullptr,nullptr,PPF_None,nullptr); return S;
}
inline FString Hash(const FString& S) { return FMD5::HashAnsiString(*S); }
inline FString StructBinaryHash(UScriptStruct* Type,void* Data)
{
	TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer,false);
	Type->SerializeItem(Archive,Data,nullptr);
	if(Archive.IsError()) { return FString(); }
	FMD5 MD5; MD5.Update(Bytes.GetData(),Bytes.Num()); uint8 Digest[16]; MD5.Final(Digest); return BytesToHex(Digest,16);
}
inline bool Writable(UObject* Object,FString& Why)
{
	if(!Live(Object) || !CanonicalTempPath(Object->GetOutermost()->GetName())) { Why=TEXT("Only an exact temp-owned target is writable"); return false; }
	if(!SafeContext()) { Why=TEXT("An idle editor outside PIE is required"); return false; }
	return true;
}
template<typename T> T* Component(const FObject& P)
{
	if(!SafeContext())return nullptr;
    UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if(!World || String(P,TEXT("actor_path")).IsEmpty() || String(P,TEXT("component_name")).IsEmpty()) { return nullptr; }
	T* Found=nullptr;
	for(TActorIterator<AActor> It(World);It;++It)
	{
		if(It->GetPathName()!=String(P,TEXT("actor_path"))) { continue; }
		TInlineComponentArray<T*> Components; It->GetComponents(Components);
		for(T* C:Components) { if(C && C->GetName()==String(P,TEXT("component_name"))) { if(Found) { return nullptr; } Found=C; } }
	}
	return Found;
}
inline bool SimpleLeaf(const FProperty* P)
{
	if(!P || P->ArrayDim!=1 || P->HasSetterOrGetter()) { return false; }
    if(CastField<FBoolProperty>(P) || CastField<FNumericProperty>(P) || CastField<FEnumProperty>(P) || CastField<FNameProperty>(P) || CastField<FStrProperty>(P)) { return true; }
	const auto* S=CastField<FStructProperty>(P);
	return S && (S->Struct==TBaseStructure<FVector>::Get() || S->Struct==TBaseStructure<FRotator>::Get() ||
		S->Struct==TBaseStructure<FTransform>::Get() || S->Struct==TBaseStructure<FLinearColor>::Get());
}
inline bool ResolveLeaf(UStruct* Type,void* Data,const FString& Path,FProperty*& OutProperty,void*& OutValue)
{
	TArray<FString> Parts; Path.ParseIntoArray(Parts,TEXT("."),false);
	if(!Type||!Data||!BoundedText(Path,1024)||Parts.IsEmpty() || Parts.Num()>8) { return false; }
	for(int32 I=0;I<Parts.Num();++I)
	{
		if(!BoundedText(Parts[I],NAME_SIZE-1)||Parts[I].IsEmpty())return false;
        FProperty* P=FindFProperty<FProperty>(Type,*Parts[I]); if(!P) { return false; }
		if(!P->HasAnyPropertyFlags(CPF_Edit) || P->HasAnyPropertyFlags(CPF_Transient|CPF_EditConst)) { return false; }
		Data=P->ContainerPtrToValuePtr<void>(Data);
		if(I==Parts.Num()-1) { OutProperty=P; OutValue=Data; return SimpleLeaf(P); }
		auto* S=CastField<FStructProperty>(P); if(!S || P->ArrayDim!=1) { return false; } Type=S->Struct;
	}
	return false;
}
inline bool FiniteProperties(UStruct* Type,const void* Data)
{
	for(TFieldIterator<FProperty> It(Type);It;++It)
	{
		const FProperty* P=*It; const void* V=P->ContainerPtrToValuePtr<void>(Data);
		if(const auto* N=CastField<FNumericProperty>(P))
		{
			if(N->IsFloatingPoint() && (!FMath::IsFinite(N->GetFloatingPointPropertyValue(V)) || FMath::Abs(N->GetFloatingPointPropertyValue(V))>1.e12)) { return false; }
		}
		else if(const auto* S=CastField<FStructProperty>(P)) { if(!FiniteProperties(S->Struct,V)) { return false; } }
	}
	return true;
}
inline bool Patch(UStruct* Type,void* Data,const FObject& Changes,FString& Why)
{
	if(!Changes || Changes->Values.IsEmpty() || Changes->Values.Num()>64) { Why=TEXT("1..64 property edits required"); return false; }
	for(const auto& Pair:Changes->Values)
	{
		const FString Path(*Pair.Key); FString Text;
		if(!Pair.Value||!Pair.Value->TryGetString(Text)||!BoundedText(Text,4096)||HasMCPReference(Text)) { Why=TEXT("Property values require bounded engine export-text strings without MCP references"); return false; }
		FProperty* P=nullptr; void* V=nullptr;
		if(!ResolveLeaf(Type,Data,Path,P,V)) { Why=FString::Printf(TEXT("Unsupported property: %s"),*Path); return false; }
		if(const auto* N=CastField<FNumericProperty>(P); N && N->IsInteger() && !N->IsEnum())
		{
			int64 Integer=0; if(!LexTryParseString(Integer,*Text) || !N->CanHoldValue(Integer)) { Why=TEXT("Integer overflow or invalid integer text"); return false; }
		}
		if((CastField<FNameProperty>(P)||CastField<FEnumProperty>(P)||(CastField<FByteProperty>(P)&&CastField<FByteProperty>(P)->Enum)||CastField<FStructProperty>(P))&&Text.Len()>=NAME_SIZE){Why=TEXT("Native name/enum/struct import text exceeds safe token bound");return false;}
        FStringOutputDevice Errors; const TCHAR* End=P->ImportText_Direct(*Text,V,nullptr,PPF_None,&Errors);
		if(!End || !FString(End).TrimStartAndEnd().IsEmpty() || !Errors.IsEmpty()) { Why=FString::Printf(TEXT("Invalid export text for %s"),*Path); return false; }
		if(const auto* N=CastField<FNumericProperty>(P))
		{
			const double Value=N->IsFloatingPoint() ? N->GetFloatingPointPropertyValue(V) : static_cast<double>(N->GetSignedIntPropertyValue(V));
			if(!FMath::IsFinite(Value) || FMath::Abs(Value)>1.e12 ||
				(P->HasMetaData(TEXT("ClampMin")) && Value<FCString::Atod(*P->GetMetaData(TEXT("ClampMin")))) ||
				(P->HasMetaData(TEXT("ClampMax")) && Value>FCString::Atod(*P->GetMetaData(TEXT("ClampMax"))))) { Why=TEXT("Numeric value outside property bounds"); return false; }
		}
		if(const auto* E=CastField<FEnumProperty>(P)) { const int64 Value=E->GetUnderlyingProperty()->GetSignedIntPropertyValue(V); if(!E->GetEnum()->IsValidEnumValue(Value) || E->GetEnum()->GetNameStringByValue(Value).EndsWith(TEXT("_MAX"))) { Why=TEXT("Invalid enum value"); return false; } }
		if(const auto* B=CastField<FByteProperty>(P)) { if(B->Enum && !B->Enum->IsValidEnumValue(B->GetPropertyValue(V))) { Why=TEXT("Invalid enum value"); return false; } }
	}
	if(!FiniteProperties(Type,Data)) { Why=TEXT("Nonfinite or oversized numeric value"); return false; }
	return true;
}
inline void Describe(UStruct* Type,const void* Data,FObject& Rows,const FString& Prefix=TEXT(""),int32 Depth=0)
{
	if(Depth>6 || Rows->Values.Num()>=256) { return; }
	for(TFieldIterator<FProperty> It(Type);It && Rows->Values.Num()<256;++It)
	{
		FProperty* P=*It; const FString Key=Prefix+P->GetName(); const void* V=P->ContainerPtrToValuePtr<void>(Data);
		if(SimpleLeaf(P)) { FString Text; P->ExportTextItem_Direct(Text,V,nullptr,nullptr,PPF_None); Rows->SetStringField(Key,Text); }
		else if(auto* S=CastField<FStructProperty>(P)) { Describe(S->Struct,V,Rows,Key+TEXT("."),Depth+1); }
	}
}
inline bool Expected(const FObject& P,const FString& Current) { return String(P,TEXT("expected_state_hash"))==Current; }
inline void MutationResult(const FObject& R,UObject* Owner,const FString& Before,const FString& After,bool DirtyBefore,bool Verified,bool RolledBack)
{
	R->SetBoolField(TEXT("read_only"),false); R->SetBoolField(TEXT("mutation_performed"),true);
	if(!SafeContext()||!Live(Owner)||!Live(Owner->GetOutermost())){R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("postcondition_verified"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("do_not_retry"),true);R->SetStringField(TEXT("error"),TEXT("Owner/context retired; no later UObject observation dispatched"));return;}
	R->SetBoolField(TEXT("postcondition_verified"),Verified); R->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);
	R->SetBoolField(TEXT("package_dirty_after"),Owner->GetOutermost()->IsDirty()); R->SetStringField(TEXT("state_hash"),Hash(After));
	if(!Verified) { R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("error"),TEXT("Readback failed; inspect rollback evidence before further writes")); R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("do_not_retry"),true); R->SetBoolField(TEXT("rollback_verified"),RolledBack && After==Before && Owner->GetOutermost()->IsDirty()==DirtyBefore); }
}

FObject ExecuteRig(const FObject& P);
FObject ClothGraph(const FObject& P);
FObject Execute(const FObject& P);
FObject Physics(const FObject& P);
FObject PCG(const FObject& P);
FObject Niagara(const FObject& P);
FObject Animation(const FObject& P);
FObject Cloth(const FObject& P);
void Shutdown();
void ShutdownNiagara();
}
