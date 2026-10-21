// CR: original texture-filtered TextureSample sampler6; no compile-completion save gate.
#include "Compatibility/UE58/CubelessMaterialSamplerApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"

namespace Cubeless::UE58::MaterialSampler
{
namespace
{
using FResult=Variable::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=false;R.ErrorCode=Code;R.Error=Error;return false; }
TSharedPtr<FJsonObject> LegacyError(FResult& R,const FString& Error)
{
    if(R.ErrorCode.IsEmpty()) { Fail(R,TEXT("material_sampler_error"),Error); }
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J;
}
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Text(const FString& S,int32 Max) { if(S.Len()>Max) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==TCHAR('\0')) { return false; } }return true; }

bool SamplerSafe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
bool Readable(const UObject* O)
{
 TSet<const UObject*> Seen;for(int32 I=0;O&&I<64;++I){if(!Loaded(O)||Seen.Contains(O))return false;Seen.Add(O);O=O->GetOuter();}return O==nullptr;
}
struct FSamplerIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;
 TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;
 explicit FSamplerIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}
 bool Exact()const
 {
  const UObject* O=Object.Get();return Readable(O)&&Readable(Class.Get())&&Readable(Package.Get())&&Readable(Effective.Get())&&
   O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);
 }
};
struct FContext
{
    TStrongObjectPtr<UMaterial> Asset{nullptr};
    TStrongObjectPtr<UTexture> Texture{nullptr};
    UMaterial* MaterialAsset=nullptr;
    TArray<TStrongObjectPtr<UMaterialExpression>> Expressions;
    TSharedPtr<FJsonObject> Before;
    EMaterialSamplerType Desired=SAMPLERTYPE_LinearColor;
    bool Dry=false;
    TArray<FString> ChangedNames;
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    TArray<TUniquePtr<FSamplerIdentity>> Identities;
    TMap<UMaterialExpressionTextureBase*,UTexture*> TextureRoles;bool RolesCaptured=false;
    EShaderPlatform ShaderPlatform=GMaxRHIShaderPlatform;
};

bool Check(FResult& R,FContext& C)
{
 auto Stop=[&](){R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_context_or_role_changed"),TEXT("Exact Editor/world/owned expression/Texture role changed after callback; actual prefix retained, no further SDK/retry."));};
 if(!SamplerSafe()||C.Editor.Get()!=GEditor||C.ShaderPlatform!=GMaxRHIShaderPlatform)return Stop();
 UWorld* World=GEditor->GetEditorWorldContext().World();
 if(!Readable(World)||World->bIsTearingDown||C.World.Get()!=World||GEditor->PlayWorld||GEditor->GetPIEWorldContext())return Stop();
 if(!C.RolesCaptured)return true;
 for(const auto& I:C.Identities)if(!I->Exact())return Stop();
 const auto Current=C.Asset->GetExpressions();if(Current.Num()!=C.Expressions.Num())return Stop();
 for(int32 I=0;I<Current.Num();++I)if(Current[I].Get()!=C.Expressions[I].Get())return Stop();
 for(const auto& P:C.TextureRoles)if(P.Key->Texture.Get()!=P.Value)return Stop();
 return true;
}
bool DerivedFields(FResult& R,FContext& C,UMaterialExpression* X,TSharedRef<FJsonObject> Out,int32& TotalChars)
{
 // Preserve reflected fields declared by TextureSample and its derived native
 // Engine class. TextureBase.Texture/SamplerType are projected separately.
 int32 FieldCount=0;
 for(TFieldIterator<FProperty> It(X->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)
 {
  FProperty* P=*It;UStruct* Owner=P->GetOwnerStruct();
  if(!Owner||!Owner->IsChildOf(UMaterialExpressionTextureSample::StaticClass())||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;
  if(++FieldCount>128||P->ArrayDim<1||P->ArrayDim>64)return Fail(R,TEXT("material_sampler_derived_fields_budget"),TEXT("Native derived authored-field inventory exceeds finite bound."));
  TArray<TSharedPtr<FJsonValue>> Values;
  for(int32 I=0;I<P->ArrayDim;++I)
  {
   FString V;const void* Address=P->ContainerPtrToValuePtr<void>(X,I);
   const bool Exported=P->ExportText_Direct(V,Address,Address,X,PPF_None);
   if(!Check(R,C))return false;
   if(!Exported||V.Len()>131072||TotalChars>1048576-V.Len())return Fail(R,TEXT("material_sampler_derived_fields_budget"),TEXT("Native derived authored-field export is incomplete or exceeds finite1Mi character bound."));
   TotalChars+=V.Len();Values.Add(MakeShared<FJsonValueString>(V));
  }
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("owner"),Owner->GetPathName());Row->SetArrayField(TEXT("elements"),Values);
  Out->SetObjectField(P->GetName(),Row);
 }
 return true;
}
void Init(FResult& R,bool Dry,bool Save,bool Compile)
{
    R.bSaveRequested=Save;R.bCompileRequested=Compile;
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("observation_capture_calls"),
        TEXT("expression_scan_visits"),TEXT("texture_sample_match_visits"),TEXT("expression_modify_calls"),TEXT("sampler_assignment_calls"),TEXT("asset_modify_calls"),
        TEXT("pre_edit_change_calls"),TEXT("post_edit_change_calls"),TEXT("force_recompile_calls"),TEXT("mark_package_dirty_calls"),
        TEXT("save_helper_calls"),TEXT("save_helper_returns"),TEXT("save_attempt_count"),TEXT("save_return_count"),TEXT("set_dirty_flag_calls"),
        TEXT("material_resource_queries"),TEXT("material_compile_status_queries"),TEXT("finish_compilation_calls"),TEXT("shader_manager_pump_calls"),TEXT("explicit_GC_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetArrayField(TEXT("save_calls"),{});
    R.Data->SetBoolField(TEXT("dry_run_requested"),Dry);R.Data->SetBoolField(TEXT("sampler_operation_success_requires_shader_completion"),false);
    R.Data->SetBoolField(TEXT("source_texture_assignment_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
    R.Data->SetBoolField(TEXT("full_original_variant_runtime_or_cold_verified"),false);R.Data->SetBoolField(TEXT("all_implicit_work_measured"),false);
    R.Data->SetStringField(TEXT("success_scope"),TEXT("Original sampler operation plus exact current selected sampler/identity observations; legacy success=True may accompany saved=False. Nullable shader completion is separate and never gates original requested save."));
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Captured selected Material identity/expression order/path/class/GUID/TextureBase texture+sampler fields, dirty/disk and selected source texture identity/dirty/disk after initial loads. Engine shader/cache/editor/child/world work and loading before first observation remain unmeasured."));
    R.Data->SetStringField(TEXT("counter_scope"),TEXT("Explicit adapter callsites only; logical SamplerType assignment counts, not every Engine setter internals. Original compound outer transaction and two asset loads preserved for admitted inputs; finite string guards precede transaction."));
    R.Data->SetStringField(TEXT("compile_scope"),TEXT("Original optional ForceRecompileForRendering(Default=Background) submission only. PostEditChange may compile even compile=False. One added public strict current-resource observer after original actions may finalize ready cache, no blocking Finish/pump/retry. Pending/error does not suppress the original direct save or fabricate rendering success."));
}
bool Disk(const UObject* O,const TSharedRef<FJsonObject>& Out)
{
    FString Path;if(!Loaded(O)||!FPackageName::TryConvertLongPackageNameToFilename(O->GetOutermost()->GetName(),Path,FPackageName::GetAssetPackageExtension())) { return false; }
    Path=FPaths::ConvertRelativePathToFull(Path);const int64 N=IFileManager::Get().FileSize(*Path);
    Out->SetStringField(TEXT("path"),Path);Out->SetBoolField(TEXT("exists"),N>=0);Out->SetNumberField(TEXT("bytes"),FMath::Max<int64>(0,N));Out->SetStringField(TEXT("sha1"),TEXT(""));
    if(N<0) { return true; }if(N>134217728) { return false; }TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Path)||Bytes.Num()!=N) { return false; }
    FSHAHash Hash;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash.Hash);Out->SetStringField(TEXT("sha1"),Hash.ToString());return true;
}
bool Admit(FResult& R,FContext& C)
{
    if(!Check(R,C))return false;
    auto* A=C.Asset.Get();auto* T=C.Texture.Get();
    if(!Loaded(A)||A->GetClass()!=UMaterial::StaticClass()||!A->IsAsset()||!A->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))
        ||A->GetOutermost()->ContainsMap()||A->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||(R.bSaveRequested&&!A->GetOutermost()->IsFullyLoaded()))
    { return Fail(R,TEXT("material_sampler_target_not_ready"),TEXT("Finite ordinary loaded /Game native Material required; loaded package for requested save. Broader original targets remain incomplete.")); }
    if(!Loaded(T)||!T->IsAsset()||!T->GetClass()->HasAnyClassFlags(CLASS_Native)||T->GetClass()->GetOutermost()->GetName()!=TEXT("/Script/Engine")||T->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient))
    { return Fail(R,TEXT("material_sampler_texture_not_ready"),TEXT("Ordinary loaded native Engine Texture asset required; custom/transient variants remain incomplete.")); }
    if(T->GetOutermost()==A->GetOutermost()) { return Fail(R,TEXT("material_sampler_shared_package_incomplete"),TEXT("First slice requires source Texture and target Material distinct packages; shared-package original variants remain incomplete.")); }
    const auto E=A->GetExpressions();if(E.Num()>512) { return Fail(R,TEXT("material_sampler_budget_exceeded"),TEXT("Bounded expression array512 exceeded.")); }
    for(const auto& P:E)
    {
        auto* X=P.Get();if(!Loaded(X)||X->GetOuter()!=A||X->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)) { return Fail(R,TEXT("material_sampler_expression_not_ready"),TEXT("Direct-owned loaded ordinary expressions required.")); }
        if(auto* Sample=Cast<UMaterialExpressionTextureSample>(X);Sample&&Sample->Texture.Get()==T&&
           (!Sample->GetClass()->HasAnyClassFlags(CLASS_Native)||Sample->GetClass()->GetOutermost()->GetName()!=TEXT("/Script/Engine")))
        { return Fail(R,TEXT("material_sampler_sample_variant_incomplete"),TEXT("Matching native Engine TextureSample hierarchy is supported; custom class callback parity remains incomplete.")); }
        C.Expressions.Emplace(X);
        C.Identities.Add(MakeUnique<FSamplerIdentity>(X));
        if(auto* Base=Cast<UMaterialExpressionTextureBase>(X)){auto* Texture=Base->Texture.Get();C.TextureRoles.Add(Base,Texture);if(Texture){if(!Readable(Texture))return Fail(R,TEXT("material_sampler_expression_texture_unreadable"),TEXT("Captured expression Texture role is not readable."));C.Identities.Add(MakeUnique<FSamplerIdentity>(Texture));}}
    }
    C.Identities.Add(MakeUnique<FSamplerIdentity>(A));C.Identities.Add(MakeUnique<FSamplerIdentity>(T));C.RolesCaptured=true;
    R.Data->SetBoolField(TEXT("native_derived_texture_samples_admitted"),true);
    R.Data->SetStringField(TEXT("derived_authored_preservation_scope"),TEXT("All non-transient reflected TextureSample-and-derived fields plus existing TextureBase texture/sampler projection, exact owned physical expression roles. SamplerType alone is planned to change. Global Editor/compiler callback preservation is not claimed."));
    return Check(R,C);
}
bool Capture(FResult& R,FContext& C,const TCHAR* Key,TSharedPtr<FJsonObject>& Out)
{
    if(!Check(R,C))return false;int32 DerivedTextChars=0;
    Count(R,TEXT("observation_capture_calls"));Out=MakeShared<FJsonObject>();R.Data->SetObjectField(Key,Out);Out->SetBoolField(TEXT("complete"),false);
    if(!Loaded(C.Asset.Get())||!Loaded(C.Texture.Get())) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_observation_incomplete"),TEXT("Selected object lifetime/readback incomplete; raw prefix retained.")); }
    auto* A=C.Asset.Get();Out->SetStringField(TEXT("asset_path"),A->GetPathName());Out->SetStringField(TEXT("asset_class"),A->GetClass()->GetPathName());Out->SetStringField(TEXT("package"),A->GetOutermost()->GetName());Out->SetBoolField(TEXT("dirty"),A->GetOutermost()->IsDirty());
    auto File=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("disk"),File);if(!Disk(A,File)) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_disk_incomplete"),TEXT("Bounded selected disk prefix incomplete.")); }
    const auto E=A->GetExpressions();TArray<TSharedPtr<FJsonValue>> Rows;Out->SetArrayField(TEXT("expressions"),Rows);Out->SetNumberField(TEXT("expression_count"),E.Num());
    if(E.Num()!=C.Expressions.Num()) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_expression_set_changed"),TEXT("Current expression count differs after callbacks.")); }
    for(int32 I=0;I<E.Num();++I)
    {
        auto Row=MakeShared<FJsonObject>();Rows.Add(MakeShared<FJsonValueObject>(Row));Out->SetArrayField(TEXT("expressions"),Rows);auto* X=E[I].Get();
        if(!Loaded(X)||X!=C.Expressions[I].Get()||X->GetOuter()!=A) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_expression_set_changed"),TEXT("Current expression order/identity differs.")); }
        Row->SetNumberField(TEXT("index"),I);Row->SetStringField(TEXT("object_path"),X->GetPathName());Row->SetStringField(TEXT("class"),X->GetClass()->GetPathName());Row->SetStringField(TEXT("name"),X->GetName());Row->SetStringField(TEXT("guid"),X->MaterialExpressionGuid.ToString());
        auto* B=Cast<UMaterialExpressionTextureBase>(X);Row->SetBoolField(TEXT("texture_base"),B!=nullptr);Row->SetBoolField(TEXT("matched_texture_sample"),Cast<UMaterialExpressionTextureSample>(X)!=nullptr&&B&&B->Texture.Get()==C.Texture.Get());
        if(B) { Row->SetStringField(TEXT("texture"),B->Texture?B->Texture->GetPathName():FString());Row->SetNumberField(TEXT("sampler_type"),static_cast<int32>(B->SamplerType.GetValue())); }
        if(auto* Sample=Cast<UMaterialExpressionTextureSample>(X);Sample&&Sample->GetClass()!=UMaterialExpressionTextureSample::StaticClass())
        {auto Authored=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("derived_authored_fields"),Authored);if(!DerivedFields(R,C,X,Authored,DerivedTextChars)){R.bChangeStateKnown=false;return false;}}
    }
    auto Source=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("source_texture"),Source);auto* T=C.Texture.Get();Source->SetStringField(TEXT("object_path"),T->GetPathName());Source->SetStringField(TEXT("class"),T->GetClass()->GetPathName());Source->SetBoolField(TEXT("dirty"),T->GetOutermost()->IsDirty());
    auto SourceFile=MakeShared<FJsonObject>();Source->SetObjectField(TEXT("disk"),SourceFile);if(!Disk(T,SourceFile)) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_source_disk_incomplete"),TEXT("Source texture disk prefix incomplete.")); }
    Out->SetBoolField(TEXT("complete"),true);return true;
}
bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();
    case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
bool Verify(FResult& R,FContext& C,const TSharedPtr<FJsonObject>& After)
{
    auto Expected=MakeShared<FJsonObject>();for(const auto& P:C.Before->Values) { Expected->SetField(P.Key,P.Value); }
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& V:C.Before->GetArrayField(TEXT("expressions")))
    {
        auto Row=MakeShared<FJsonObject>();for(const auto& P:V->AsObject()->Values) { Row->SetField(P.Key,P.Value); }
        if(!C.Dry&&Row->GetBoolField(TEXT("matched_texture_sample"))) { Row->SetNumberField(TEXT("sampler_type"),static_cast<int32>(C.Desired)); }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Expected->SetArrayField(TEXT("expressions"),Rows);R.Data->SetObjectField(TEXT("expected_state_projection"),Expected);
    auto Projected=MakeShared<FJsonObject>();for(const auto& P:After->Values) { Projected->SetField(P.Key,P.Value); }
    Expected->RemoveField(TEXT("dirty"));Expected->RemoveField(TEXT("disk"));Projected->RemoveField(TEXT("dirty"));Projected->RemoveField(TEXT("disk"));
    const bool OK=Equal(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Projected));R.Data->SetBoolField(TEXT("current_sampler_and_selected_identity_verified"),OK);
    if(!OK) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_postcondition_failed"),TEXT("Selected sampler/other captured expression/source fields differed after actual callbacks; no repair/retry.")); }
    return true;
}
bool SaveLoadedAsset(FResult& R,FContext& C,UObject* Asset)
{
    auto Call=MakeShared<FJsonObject>();R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});Call->SetBoolField(TEXT("helper_attempted"),true);Call->SetBoolField(TEXT("helper_returned"),false);Call->SetBoolField(TEXT("attempted"),false);Call->SetStringField(TEXT("route"),TEXT("UPackage.SavePackage"));Call->SetBoolField(TEXT("dirty_filter_applied"),false);
    auto Refused=[&](){Call->SetBoolField(TEXT("helper_returned"),true);Call->SetBoolField(TEXT("saved"),false);return false;};
    if(!Check(R,C))return false;
    if(!IsValid(Asset)) { return Refused(); }UPackage* Package=Asset->GetOutermost();
    if(!Package||Package==GetTransientPackage()||!FPackageName::IsValidLongPackageName(Package->GetName(),false)) { return Refused(); }
    FString Filename;if(!FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(),Filename,FPackageName::GetAssetPackageExtension())) { return Refused(); }
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;
    Call->SetStringField(TEXT("asset_path"),Asset->GetPathName());Call->SetStringField(TEXT("package"),Package->GetName());Call->SetStringField(TEXT("filename"),FPaths::ConvertRelativePathToFull(Filename));Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);
    R.bSaveAttempted=true;Count(R,TEXT("save_attempt_count"));const bool Saved=UPackage::SavePackage(Package,Asset,*Filename,Args);Count(R,TEXT("save_return_count"));R.bSaved|=Saved;
    Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("helper_returned"),true);Call->SetBoolField(TEXT("saved"),Saved);
    if(!Check(R,C))return Saved;
    // Observe Engine save bookkeeping; never clear a dirty flag produced by a callback.
    R.Data->SetBoolField(TEXT("explicit_dirty_flag_clear_performed"),false);return Saved;
}
FString NormalizeContentRoot(FString Path)
{
    Path.TrimStartAndEndInline();
    while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
    {
        Path.LeftChopInline(1);
    }
    return Path;
}

FString NormalizePackagePathParam(FString Path)
{
    Path.TrimStartAndEndInline();
    Path.TrimQuotesInline();

    FString LongPackageName;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, LongPackageName))
    {
        return NormalizeContentRoot(LongPackageName);
    }

    if (Path.Contains(TEXT(".")))
    {
        Path = FPackageName::ObjectPathToPackageName(Path);
    }
    return NormalizeContentRoot(Path);
}

FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FString& DefaultValue)
{
    FString Value;
    if (Params.IsValid() && Params->TryGetStringField(FieldName, Value) && !Value.IsEmpty())
    {
        return Value;
    }
    return DefaultValue;
}

bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, bool bDefaultValue)
{
    bool bValue = bDefaultValue;
    if (Params.IsValid() && Params->HasTypedField<EJson::Boolean>(FieldName))
    {
        bValue = Params->GetBoolField(FieldName);
    }
    return bValue;
}

bool TryGetRequiredStringParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutValue,
    FString& OutError)
{
    FString Value;
    if (!Params.IsValid() || !Params->TryGetStringField(FieldName, Value))
    {
        OutError = FString::Printf(TEXT("Missing required parameter: %s"), FieldName);
        return false;
    }

    Value.TrimStartAndEndInline();
    if (Value.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Missing required parameter: %s"), FieldName);
        return false;
    }

    OutValue = Value;
    return true;
}

bool ObserveCompile(FContext& C, FResult& R, const TCHAR* Key)
{
    if(!Check(R,C))return false;
    auto Row = MakeShared<FJsonObject>(); R.Data->SetObjectField(Key, Row); Row->SetBoolField(TEXT("complete"), false);
    R.Compiled.Reset(); Row->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("compilation_finished"), MakeShared<FJsonValueNull>());
    Row->SetNumberField(TEXT("shader_platform"), static_cast<int32>(GMaxRHIShaderPlatform)); Row->SetBoolField(TEXT("null_rhi"), GUsingNullRHI);
    Count(R, TEXT("material_resource_queries")); FMaterialResource* Resource = C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);
    TArray<TSharedPtr<FJsonValue>> Errors; Row->SetArrayField(TEXT("compile_errors"), Errors);
    if (!Resource)
    { R.Compiled = false; Row->SetBoolField(TEXT("resource_available"), false); Row->SetStringField(TEXT("status"), TEXT("resource_unavailable")); Errors.Add(MakeShared<FJsonValueString>(TEXT("No material resource for current shader platform."))); Row->SetArrayField(TEXT("compile_errors"), Errors); }
    else
    {
        Row->SetBoolField(TEXT("resource_available"), true); Count(R, TEXT("material_compile_status_queries"));
        const bool Finished=Resource->IsCompilationFinished();
        if(!Check(R,C))return false;
        Count(R,TEXT("material_resource_queries"));
        if(C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform)!=Resource){R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_resource_changed"),TEXT("Current resource changed during ready-cache application; no stale resource access."));}
        const auto CompileErrors=Resource->GetCompileErrors();int32 Chars=0;
        Row->SetBoolField(TEXT("compilation_finished"), Finished);
        for (const FString& Error : CompileErrors)
        { if (Errors.Num() >= 256 || Error.Len() > 1048576 || Chars > 1048576 - Error.Len()) { return Fail(R, TEXT("material_sampler_compile_status_incomplete"), TEXT("Compile error observation bound exceeded.")); } Chars += Error.Len(); Errors.Add(MakeShared<FJsonValueString>(Error)); Row->SetArrayField(TEXT("compile_errors"), Errors); }
        const FMaterialShaderMap* Map = Resource->GetGameThreadShaderMap(); const bool Complete = Resource->IsGameThreadShaderMapComplete(), Valid = Resource->HasValidGameThreadShaderMap();
        Row->SetBoolField(TEXT("game_thread_shader_map_complete"), Complete); Row->SetBoolField(TEXT("has_valid_game_thread_shader_map"), Valid); Row->SetBoolField(TEXT("game_thread_shader_map_present"), Map != nullptr);
        const bool MapFinalized=Map&&Map->IsCompilationFinalized(),MapSuccess=Map&&Map->CompiledSuccessfully();
        if (Map) { Row->SetBoolField(TEXT("shader_map_compilation_finalized"),MapFinalized); Row->SetBoolField(TEXT("shader_map_compiled_successfully"),MapSuccess); }
        else { Row->SetField(TEXT("shader_map_compilation_finalized"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("shader_map_compiled_successfully"), MakeShared<FJsonValueNull>()); }
        const bool HadErrorOrCompiling=C.MaterialAsset->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform);
        if(!Check(R,C))return false;
        Count(R,TEXT("material_resource_queries"));
        if(C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform)!=Resource||Resource->GetGameThreadShaderMap()!=Map){R.bChangeStateKnown=false;return Fail(R,TEXT("material_sampler_resource_changed"),TEXT("Current resource/map changed during original Editor status predicate; saved prefix retained."));}
        Row->SetBoolField(TEXT("original_compiling_or_had_error"),HadErrorOrCompiling);
        if (!CompileErrors.IsEmpty()) { R.Compiled = false; Row->SetStringField(TEXT("status"), TEXT("compile_errors")); }
        else if (Finished && !HadErrorOrCompiling && Complete && Valid && Map && MapFinalized && MapSuccess) { R.Compiled = true; Row->SetStringField(TEXT("status"), TEXT("current_resource_complete")); }
        else { Row->SetStringField(TEXT("status"), Finished ? TEXT("resource_incomplete") : TEXT("pending")); }
    }
    if (R.Compiled.IsSet()) { Row->SetBoolField(TEXT("compiled"), R.Compiled.GetValue()); }
    Row->SetNumberField(TEXT("compile_error_count"), Errors.Num()); Row->SetBoolField(TEXT("complete"), true);
    for (const TCHAR* Field : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_errors"), TEXT("compile_error_count") }) { R.Data->SetField(Field, Row->TryGetField(Field)); }
    return true;
}

TSharedPtr<FJsonObject> LegacyRun(const TSharedPtr<FJsonObject>& Params, FResult& R, FContext& C)
{
    if (!Params.IsValid())
    {
        return LegacyError(R,TEXT("Missing parameters"));
    }

    FString MaterialPathRaw;
    FString TexturePathRaw;
    FString ParamError;
    if (!TryGetRequiredStringParamForMCP(Params, TEXT("material_path"), MaterialPathRaw, ParamError) ||
        !TryGetRequiredStringParamForMCP(Params, TEXT("texture_path"), TexturePathRaw, ParamError))
    {
        return LegacyError(R,ParamError);
    }

    const FString MaterialPath = NormalizePackagePathParam(MaterialPathRaw);
    const FString TexturePath = NormalizePackagePathParam(TexturePathRaw);
    R.Stage=TEXT("load_material"); Count(R,TEXT("asset_load_calls"));
    UMaterial* Material = Cast<UMaterial>(UEditorAssetLibrary::LoadAsset(MaterialPath));
    Count(R,TEXT("asset_load_returns"));if(!Check(R,C))return LegacyError(R,R.Error);C.Asset.Reset(Material);C.MaterialAsset=Material;
    if (!Material)
    {
        return LegacyError(R,FString::Printf(TEXT("material_path is not a Material asset: %s"), *MaterialPath));
    }

    R.Stage=TEXT("load_texture"); Count(R,TEXT("asset_load_calls"));
    UTexture* Texture = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(TexturePath));
    Count(R,TEXT("asset_load_returns"));if(!Check(R,C))return LegacyError(R,R.Error);C.Texture.Reset(Texture);
    if (!Texture)
    {
        return LegacyError(R,FString::Printf(TEXT("texture_path is not a Texture asset: %s"), *TexturePath));
    }

    if(!Admit(R,C)) { return LegacyError(R,R.Error); }
    const FString SamplerTypeRaw = GetStringParam(Params, TEXT("sampler_type"), TEXT("SAMPLERTYPE_LinearColor"));
    EMaterialSamplerType SamplerType = SAMPLERTYPE_LinearColor;
    if (SamplerTypeRaw.Equals(TEXT("SAMPLERTYPE_Color"), ESearchCase::IgnoreCase) ||
        SamplerTypeRaw.Equals(TEXT("Color"), ESearchCase::IgnoreCase))
    {
        SamplerType = SAMPLERTYPE_Color;
    }
    else if (SamplerTypeRaw.Equals(TEXT("SAMPLERTYPE_LinearColor"), ESearchCase::IgnoreCase) ||
        SamplerTypeRaw.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase) ||
        SamplerTypeRaw.Equals(TEXT("Linear Color"), ESearchCase::IgnoreCase))
    {
        SamplerType = SAMPLERTYPE_LinearColor;
    }
    else if (SamplerTypeRaw.Equals(TEXT("SAMPLERTYPE_Masks"), ESearchCase::IgnoreCase) ||
        SamplerTypeRaw.Equals(TEXT("Masks"), ESearchCase::IgnoreCase))
    {
        SamplerType = SAMPLERTYPE_Masks;
    }
    else
    {
        return LegacyError(R,FString::Printf(TEXT("Unsupported sampler_type: %s"), *SamplerTypeRaw));
    }

    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false);
    const bool bSave = GetBoolParam(Params, TEXT("save"), true);
    const bool bCompile = GetBoolParam(Params, TEXT("compile"), true);

    C.Desired=SamplerType; R.Stage=TEXT("before_observation");
    if(!Capture(R,C,TEXT("before_observation"),C.Before)) { return LegacyError(R,R.Error); }
    TArray<FString> ChangedNodes;
    int32 MatchedNodeCount = 0;
    int32 AlreadyCorrectCount = 0;
    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
    {
        Count(R,TEXT("expression_scan_visits"));
        UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(ExpressionPtr.Get());
        if (!TextureSample || TextureSample->Texture != Texture)
        {
            continue;
        }

        ++MatchedNodeCount; Count(R,TEXT("texture_sample_match_visits"));
        if (TextureSample->SamplerType.GetValue() == SamplerType)
        {
            ++AlreadyCorrectCount;
            continue;
        }

        ChangedNodes.Add(TextureSample->GetName());
        if (!bDryRun)
        {
            R.Stage=TEXT("set_sampler"); R.bMutationAttempted=true; R.bChangeStateKnown=false;
            Count(R,TEXT("expression_modify_calls")); TextureSample->Modify();if(!Check(R,C))return LegacyError(R,R.Error);
            Count(R,TEXT("sampler_assignment_calls")); TextureSample->SamplerType = SamplerType;
            R.bChanged=true;
        }
    }

    bool bSaved = false;
    if (!bDryRun && ChangedNodes.Num() > 0)
    {
        R.Stage=TEXT("material_callbacks");
        Count(R,TEXT("asset_modify_calls")); Material->Modify();if(!Check(R,C))return LegacyError(R,R.Error);
        Count(R,TEXT("pre_edit_change_calls")); Material->PreEditChange(nullptr);if(!Check(R,C))return LegacyError(R,R.Error);
        Count(R,TEXT("post_edit_change_calls")); Material->PostEditChange();if(!Check(R,C))return LegacyError(R,R.Error);
        if (bCompile)
        {
            R.Stage=TEXT("compile_dispatch"); R.bCompileAttempted=true;
            Count(R,TEXT("force_recompile_calls")); Material->ForceRecompileForRendering();if(!Check(R,C))return LegacyError(R,R.Error);
        }
        Count(R,TEXT("mark_package_dirty_calls")); Material->MarkPackageDirty();if(!Check(R,C))return LegacyError(R,R.Error);
        if (bSave)
        {
            R.Stage=TEXT("save_dispatch"); Count(R,TEXT("save_helper_calls"));
            bSaved = SaveLoadedAsset(R,C,Material); Count(R,TEXT("save_helper_returns")); R.bSaved|=bSaved;if(!R.ErrorCode.IsEmpty())return LegacyError(R,R.Error);
        }
    }

    C.Dry=bDryRun; C.ChangedNames=ChangedNodes;
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetStringField(TEXT("texture_path"), TexturePath);
    ResultObj->SetStringField(TEXT("sampler_type"), SamplerTypeRaw);
    ResultObj->SetNumberField(TEXT("matched_node_count"), MatchedNodeCount);
    ResultObj->SetNumberField(TEXT("already_correct_count"), AlreadyCorrectCount);
    ResultObj->SetNumberField(TEXT("changed_node_count"), ChangedNodes.Num());
    ResultObj->SetBoolField(TEXT("saved"), bSaved);
    TArray<TSharedPtr<FJsonValue>> ChangedNodeValues;
    for (const FString& NodeName : ChangedNodes)
    {
        ChangedNodeValues.Add(MakeShared<FJsonValueString>(NodeName));
    }
    ResultObj->SetArrayField(TEXT("changed_nodes"), ChangedNodeValues);
    return ResultObj;
}

}
Variable::FResult SetSamplerType(const FString& MaterialPath,const FString& TexturePath,const FString& SamplerType,bool Dry,bool Save,bool Compile)
{
    FResult R;Init(R,Dry,Save,Compile);
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(R,TEXT("material_sampler_editor_state_invalid"),TEXT("Editor game thread outside PIE/SIE required."));return R; }
    if(!Text(MaterialPath,1023)||!Text(TexturePath,1023)||!Text(SamplerType,128)) { Fail(R,TEXT("material_sampler_input_invalid"),TEXT("Bounded path/enum literal with no interior NUL required; larger original inputs remain incomplete."));return R; }
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("material_path"),MaterialPath);Params->SetStringField(TEXT("texture_path"),TexturePath);Params->SetStringField(TEXT("sampler_type"),SamplerType);Params->SetBoolField(TEXT("dry_run"),Dry);Params->SetBoolField(TEXT("save"),Save);Params->SetBoolField(TEXT("compile"),Compile);
    FContext C;C.Editor=GEditor;C.World=GEditor->GetEditorWorldContext().World();if(!Check(R,C))return R;
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","CRSampler","Set texture sample sampler type"));
    auto Legacy=LegacyRun(Params,R,C);R.Data->SetObjectField(TEXT("original_result"),Legacy);
    for(const auto& P:Legacy->Values) { if(!FString(P.Key.ToView()).Equals(TEXT("success"),ESearchCase::CaseSensitive)) { R.Data->SetField(P.Key,P.Value); } }
    if(!R.ErrorCode.IsEmpty()) { return R; }
    R.Stage=TEXT("after_actions_observation");TSharedPtr<FJsonObject> After;
    if(!Capture(R,C,TEXT("after_actions_observation"),After)||!Verify(R,C,After)) { return R; }
    R.Data->SetBoolField(TEXT("actual_sampler_values_changed"),!Dry&&!C.ChangedNames.IsEmpty());
    R.Data->SetBoolField(TEXT("would_change_count_is_not_actual_dry_write_count"),Dry);
    if(Dry||C.ChangedNames.IsEmpty())
    {
        const bool Stable=Equal(MakeShared<FJsonValueObject>(C.Before),MakeShared<FJsonValueObject>(After));R.Data->SetBoolField(TEXT("no_op_selected_state_preserved"),Stable);
        if(!Stable) { R.bChangeStateKnown=false;Fail(R,TEXT("material_sampler_noop_scope_changed"),TEXT("No sampler writer dispatched, but captured selected point state differed."));return R; }
    }
    if(R.bCompileAttempted)
    {
        R.Stage=TEXT("after_dispatch_compile_observation");
        if(!ObserveCompile(C,R,TEXT("after_dispatch_compile_status"))) { R.bChangeStateKnown=false;return R; }
        TSharedPtr<FJsonObject> Final;R.Stage=TEXT("after_compile_observer");
        if(!Capture(R,C,TEXT("after_compile_observation"),Final)||!Verify(R,C,Final)) { return R; }
    }
    R.Data->SetBoolField(TEXT("saved_disk_observed"),R.bSaved&&After->GetObjectField(TEXT("disk"))->GetBoolField(TEXT("exists"))&&After->GetObjectField(TEXT("disk"))->GetNumberField(TEXT("bytes"))>0);
    if(R.bSaved&&!R.Data->GetBoolField(TEXT("saved_disk_observed"))) { R.bChangeStateKnown=true;Fail(R,TEXT("material_sampler_saved_disk_unverified"),TEXT("Actual save=True prefix retained, but current bounded file proof is missing; no fallback save/retry."));return R; }
    R.Data->SetBoolField(TEXT("legacy_save_false_success_preserved"),R.Data->GetNumberField(TEXT("save_helper_calls"))>0&&!R.bSaved);
    R.Data->SetBoolField(TEXT("compiled_true_not_required_for_sampler_success"),true);
    R.bChangeStateKnown=true;R.bChanged=!Dry&&!C.ChangedNames.IsEmpty();R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
